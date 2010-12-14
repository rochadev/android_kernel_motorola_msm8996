/*
 * linux/fs/nfs/nfs2xdr.c
 *
 * XDR functions to encode/decode NFS RPC arguments and results.
 *
 * Copyright (C) 1992, 1993, 1994  Rick Sladkey
 * Copyright (C) 1996 Olaf Kirch
 * 04 Aug 1998  Ion Badulescu <ionut@cs.columbia.edu>
 * 		FIFO's need special handling in NFSv2
 */

#include <linux/param.h>
#include <linux/time.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/in.h>
#include <linux/pagemap.h>
#include <linux/proc_fs.h>
#include <linux/sunrpc/clnt.h>
#include <linux/nfs.h>
#include <linux/nfs2.h>
#include <linux/nfs_fs.h>
#include "internal.h"

#define NFSDBG_FACILITY		NFSDBG_XDR

/* Mapping from NFS error code to "errno" error code. */
#define errno_NFSERR_IO		EIO

/*
 * Declare the space requirements for NFS arguments and replies as
 * number of 32bit-words
 */
#define NFS_fhandle_sz		(8)
#define NFS_sattr_sz		(8)
#define NFS_filename_sz		(1+(NFS2_MAXNAMLEN>>2))
#define NFS_path_sz		(1+(NFS2_MAXPATHLEN>>2))
#define NFS_fattr_sz		(17)
#define NFS_info_sz		(5)
#define NFS_entry_sz		(NFS_filename_sz+3)

#define NFS_diropargs_sz	(NFS_fhandle_sz+NFS_filename_sz)
#define NFS_removeargs_sz	(NFS_fhandle_sz+NFS_filename_sz)
#define NFS_sattrargs_sz	(NFS_fhandle_sz+NFS_sattr_sz)
#define NFS_readlinkargs_sz	(NFS_fhandle_sz)
#define NFS_readargs_sz		(NFS_fhandle_sz+3)
#define NFS_writeargs_sz	(NFS_fhandle_sz+4)
#define NFS_createargs_sz	(NFS_diropargs_sz+NFS_sattr_sz)
#define NFS_renameargs_sz	(NFS_diropargs_sz+NFS_diropargs_sz)
#define NFS_linkargs_sz		(NFS_fhandle_sz+NFS_diropargs_sz)
#define NFS_symlinkargs_sz	(NFS_diropargs_sz+1+NFS_sattr_sz)
#define NFS_readdirargs_sz	(NFS_fhandle_sz+2)

#define NFS_attrstat_sz		(1+NFS_fattr_sz)
#define NFS_diropres_sz		(1+NFS_fhandle_sz+NFS_fattr_sz)
#define NFS_readlinkres_sz	(2)
#define NFS_readres_sz		(1+NFS_fattr_sz+1)
#define NFS_writeres_sz         (NFS_attrstat_sz)
#define NFS_stat_sz		(1)
#define NFS_readdirres_sz	(1)
#define NFS_statfsres_sz	(1+NFS_info_sz)


/*
 * While encoding arguments, set up the reply buffer in advance to
 * receive reply data directly into the page cache.
 */
static void prepare_reply_buffer(struct rpc_rqst *req, struct page **pages,
				 unsigned int base, unsigned int len,
				 unsigned int bufsize)
{
	struct rpc_auth	*auth = req->rq_cred->cr_auth;
	unsigned int replen;

	replen = RPC_REPHDRSIZE + auth->au_rslack + bufsize;
	xdr_inline_pages(&req->rq_rcv_buf, replen << 2, pages, base, len);
}


/*
 * Common NFS XDR functions as inlines
 */
static inline __be32 *
xdr_decode_fhandle(__be32 *p, struct nfs_fh *fhandle)
{
	/* NFSv2 handles have a fixed length */
	fhandle->size = NFS2_FHSIZE;
	memcpy(fhandle->data, p, NFS2_FHSIZE);
	return p + XDR_QUADLEN(NFS2_FHSIZE);
}

static inline __be32*
xdr_decode_time(__be32 *p, struct timespec *timep)
{
	timep->tv_sec = ntohl(*p++);
	/* Convert microseconds into nanoseconds */
	timep->tv_nsec = ntohl(*p++) * 1000;
	return p;
}

static __be32 *
xdr_decode_fattr(__be32 *p, struct nfs_fattr *fattr)
{
	u32 rdev, type;
	type = ntohl(*p++);
	fattr->mode = ntohl(*p++);
	fattr->nlink = ntohl(*p++);
	fattr->uid = ntohl(*p++);
	fattr->gid = ntohl(*p++);
	fattr->size = ntohl(*p++);
	fattr->du.nfs2.blocksize = ntohl(*p++);
	rdev = ntohl(*p++);
	fattr->du.nfs2.blocks = ntohl(*p++);
	fattr->fsid.major = ntohl(*p++);
	fattr->fsid.minor = 0;
	fattr->fileid = ntohl(*p++);
	p = xdr_decode_time(p, &fattr->atime);
	p = xdr_decode_time(p, &fattr->mtime);
	p = xdr_decode_time(p, &fattr->ctime);
	fattr->valid |= NFS_ATTR_FATTR_V2;
	fattr->rdev = new_decode_dev(rdev);
	if (type == NFCHR && rdev == NFS2_FIFO_DEV) {
		fattr->mode = (fattr->mode & ~S_IFMT) | S_IFIFO;
		fattr->rdev = 0;
	}
	return p;
}

/*
 * Encode/decode NFSv2 basic data types
 *
 * Basic NFSv2 data types are defined in section 2.3 of RFC 1094:
 * "NFS: Network File System Protocol Specification".
 *
 * Not all basic data types have their own encoding and decoding
 * functions.  For run-time efficiency, some data types are encoded
 * or decoded inline.
 */

/*
 * 2.3.3.  fhandle
 *
 *	typedef opaque fhandle[FHSIZE];
 */
static void encode_fhandle(struct xdr_stream *xdr, const struct nfs_fh *fh)
{
	__be32 *p;

	BUG_ON(fh->size != NFS2_FHSIZE);
	p = xdr_reserve_space(xdr, NFS2_FHSIZE);
	memcpy(p, fh->data, NFS2_FHSIZE);
}

/*
 * 2.3.4.  timeval
 *
 *	struct timeval {
 *		unsigned int seconds;
 *		unsigned int useconds;
 *	};
 */
static __be32 *xdr_encode_time(__be32 *p, const struct timespec *timep)
{
	*p++ = cpu_to_be32(timep->tv_sec);
	if (timep->tv_nsec != 0)
		*p++ = cpu_to_be32(timep->tv_nsec / NSEC_PER_USEC);
	else
		*p++ = cpu_to_be32(0);
	return p;
}

/*
 * Passing the invalid value useconds=1000000 is a Sun convention for
 * "set to current server time".  It's needed to make permissions checks
 * for the "touch" program across v2 mounts to Solaris and Irix servers
 * work correctly.  See description of sattr in section 6.1 of "NFS
 * Illustrated" by Brent Callaghan, Addison-Wesley, ISBN 0-201-32750-5.
 */
static __be32 *xdr_encode_current_server_time(__be32 *p,
					      const struct timespec *timep)
{
	*p++ = cpu_to_be32(timep->tv_sec);
	*p++ = cpu_to_be32(1000000);
	return p;
}

/*
 * 2.3.6.  sattr
 *
 *	struct sattr {
 *		unsigned int	mode;
 *		unsigned int	uid;
 *		unsigned int	gid;
 *		unsigned int	size;
 *		timeval		atime;
 *		timeval		mtime;
 *	};
 */

#define NFS2_SATTR_NOT_SET	(0xffffffff)

static __be32 *xdr_time_not_set(__be32 *p)
{
	*p++ = cpu_to_be32(NFS2_SATTR_NOT_SET);
	*p++ = cpu_to_be32(NFS2_SATTR_NOT_SET);
	return p;
}

static void encode_sattr(struct xdr_stream *xdr, const struct iattr *attr)
{
	__be32 *p;

	p = xdr_reserve_space(xdr, NFS_sattr_sz << 2);

	if (attr->ia_valid & ATTR_MODE)
		*p++ = cpu_to_be32(attr->ia_mode);
	else
		*p++ = cpu_to_be32(NFS2_SATTR_NOT_SET);
	if (attr->ia_valid & ATTR_UID)
		*p++ = cpu_to_be32(attr->ia_uid);
	else
		*p++ = cpu_to_be32(NFS2_SATTR_NOT_SET);
	if (attr->ia_valid & ATTR_GID)
		*p++ = cpu_to_be32(attr->ia_gid);
	else
		*p++ = cpu_to_be32(NFS2_SATTR_NOT_SET);
	if (attr->ia_valid & ATTR_SIZE)
		*p++ = cpu_to_be32((u32)attr->ia_size);
	else
		*p++ = cpu_to_be32(NFS2_SATTR_NOT_SET);

	if (attr->ia_valid & ATTR_ATIME_SET)
		p = xdr_encode_time(p, &attr->ia_atime);
	else if (attr->ia_valid & ATTR_ATIME)
		p = xdr_encode_current_server_time(p, &attr->ia_atime);
	else
		p = xdr_time_not_set(p);
	if (attr->ia_valid & ATTR_MTIME_SET)
		xdr_encode_time(p, &attr->ia_mtime);
	else if (attr->ia_valid & ATTR_MTIME)
		xdr_encode_current_server_time(p, &attr->ia_mtime);
	else
		xdr_time_not_set(p);
}

/*
 * 2.3.7.  filename
 *
 *	typedef string filename<MAXNAMLEN>;
 */
static void encode_filename(struct xdr_stream *xdr,
			    const char *name, u32 length)
{
	__be32 *p;

	BUG_ON(length > NFS2_MAXNAMLEN);
	p = xdr_reserve_space(xdr, 4 + length);
	xdr_encode_opaque(p, name, length);
}

/*
 * 2.3.8.  path
 *
 *	typedef string path<MAXPATHLEN>;
 */
static void encode_path(struct xdr_stream *xdr, struct page **pages, u32 length)
{
	__be32 *p;

	BUG_ON(length > NFS2_MAXPATHLEN);
	p = xdr_reserve_space(xdr, 4);
	*p = cpu_to_be32(length);
	xdr_write_pages(xdr, pages, 0, length);
}

/*
 * 2.3.10.  diropargs
 *
 *	struct diropargs {
 *		fhandle  dir;
 *		filename name;
 *	};
 */
static void encode_diropargs(struct xdr_stream *xdr, const struct nfs_fh *fh,
			     const char *name, u32 length)
{
	encode_fhandle(xdr, fh);
	encode_filename(xdr, name, length);
}


/*
 * NFSv2 XDR encode functions
 *
 * NFSv2 argument types are defined in section 2.2 of RFC 1094:
 * "NFS: Network File System Protocol Specification".
 */

static int nfs2_xdr_enc_fhandle(struct rpc_rqst *req, __be32 *p,
				const struct nfs_fh *fh)
{
	struct xdr_stream xdr;

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_fhandle(&xdr, fh);
	return 0;
}

/*
 * 2.2.3.  sattrargs
 *
 *	struct sattrargs {
 *		fhandle file;
 *		sattr attributes;
 *	};
 */
static int nfs2_xdr_enc_sattrargs(struct rpc_rqst *req, __be32 *p,
				  const struct nfs_sattrargs *args)
{
	struct xdr_stream xdr;

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_fhandle(&xdr, args->fh);
	encode_sattr(&xdr, args->sattr);
	return 0;
}

static int nfs2_xdr_enc_diropargs(struct rpc_rqst *req, __be32 *p,
				  const struct nfs_diropargs *args)
{
	struct xdr_stream xdr;

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_diropargs(&xdr, args->fh, args->name, args->len);
	return 0;
}

static int nfs2_xdr_enc_readlinkargs(struct rpc_rqst *req, __be32 *p,
				     const struct nfs_readlinkargs *args)
{
	struct xdr_stream xdr;

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_fhandle(&xdr, args->fh);
	prepare_reply_buffer(req, args->pages, args->pgbase,
					args->pglen, NFS_readlinkres_sz);
	return 0;
}

/*
 * 2.2.7.  readargs
 *
 *	struct readargs {
 *		fhandle file;
 *		unsigned offset;
 *		unsigned count;
 *		unsigned totalcount;
 *	};
 */
static void encode_readargs(struct xdr_stream *xdr,
			    const struct nfs_readargs *args)
{
	u32 offset = args->offset;
	u32 count = args->count;
	__be32 *p;

	encode_fhandle(xdr, args->fh);

	p = xdr_reserve_space(xdr, 4 + 4 + 4);
	*p++ = cpu_to_be32(offset);
	*p++ = cpu_to_be32(count);
	*p = cpu_to_be32(count);
}

static int nfs2_xdr_enc_readargs(struct rpc_rqst *req, __be32 *p,
				 const struct nfs_readargs *args)
{
	struct xdr_stream xdr;

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_readargs(&xdr, args);
	prepare_reply_buffer(req, args->pages, args->pgbase,
					args->count, NFS_readres_sz);
	req->rq_rcv_buf.flags |= XDRBUF_READ;
	return 0;
}

/*
 * Decode READ reply
 */
static int
nfs_xdr_readres(struct rpc_rqst *req, __be32 *p, struct nfs_readres *res)
{
	struct kvec *iov = req->rq_rcv_buf.head;
	size_t hdrlen;
	u32 count, recvd;
	int status;

	if ((status = ntohl(*p++)))
		return nfs_stat_to_errno(status);
	p = xdr_decode_fattr(p, res->fattr);

	count = ntohl(*p++);
	res->eof = 0;
	hdrlen = (u8 *) p - (u8 *) iov->iov_base;
	if (iov->iov_len < hdrlen) {
		dprintk("NFS: READ reply header overflowed:"
				"length %Zu > %Zu\n", hdrlen, iov->iov_len);
		return -errno_NFSERR_IO;
	} else if (iov->iov_len != hdrlen) {
		dprintk("NFS: READ header is short. iovec will be shifted.\n");
		xdr_shift_buf(&req->rq_rcv_buf, iov->iov_len - hdrlen);
	}

	recvd = req->rq_rcv_buf.len - hdrlen;
	if (count > recvd) {
		dprintk("NFS: server cheating in read reply: "
			"count %u > recvd %u\n", count, recvd);
		count = recvd;
	}

	dprintk("RPC:      readres OK count %u\n", count);
	if (count < res->count)
		res->count = count;

	return count;
}


/*
 * 2.2.9.  writeargs
 *
 *	struct writeargs {
 *		fhandle file;
 *		unsigned beginoffset;
 *		unsigned offset;
 *		unsigned totalcount;
 *		nfsdata data;
 *	};
 */
static void encode_writeargs(struct xdr_stream *xdr,
			     const struct nfs_writeargs *args)
{
	u32 offset = args->offset;
	u32 count = args->count;
	__be32 *p;

	encode_fhandle(xdr, args->fh);

	p = xdr_reserve_space(xdr, 4 + 4 + 4 + 4);
	*p++ = cpu_to_be32(offset);
	*p++ = cpu_to_be32(offset);
	*p++ = cpu_to_be32(count);

	/* nfsdata */
	*p = cpu_to_be32(count);
	xdr_write_pages(xdr, args->pages, args->pgbase, count);
}

static int nfs2_xdr_enc_writeargs(struct rpc_rqst *req, __be32 *p,
				  const struct nfs_writeargs *args)
{
	struct xdr_stream xdr;

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_writeargs(&xdr, args);
	xdr.buf->flags |= XDRBUF_WRITE;
	return 0;
}

/*
 * 2.2.10.  createargs
 *
 *	struct createargs {
 *		diropargs where;
 *		sattr attributes;
 *	};
 */
static int nfs2_xdr_enc_createargs(struct rpc_rqst *req, __be32 *p,
				   const struct nfs_createargs *args)
{
	struct xdr_stream xdr;

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_diropargs(&xdr, args->fh, args->name, args->len);
	encode_sattr(&xdr, args->sattr);
	return 0;
}

static int nfs2_xdr_enc_removeargs(struct rpc_rqst *req, __be32 *p,
				   const struct nfs_removeargs *args)
{
	struct xdr_stream xdr;

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_diropargs(&xdr, args->fh, args->name.name, args->name.len);
	return 0;
}

/*
 * 2.2.12.  renameargs
 *
 *	struct renameargs {
 *		diropargs from;
 *		diropargs to;
 *	};
 */
static int nfs2_xdr_enc_renameargs(struct rpc_rqst *req, __be32 *p,
				   const struct nfs_renameargs *args)
{
	const struct qstr *old = args->old_name;
	const struct qstr *new = args->new_name;
	struct xdr_stream xdr;

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_diropargs(&xdr, args->old_dir, old->name, old->len);
	encode_diropargs(&xdr, args->new_dir, new->name, new->len);
	return 0;
}

/*
 * 2.2.13.  linkargs
 *
 *	struct linkargs {
 *		fhandle from;
 *		diropargs to;
 *	};
 */
static int nfs2_xdr_enc_linkargs(struct rpc_rqst *req, __be32 *p,
				 const struct nfs_linkargs *args)
{
	struct xdr_stream xdr;

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_fhandle(&xdr, args->fromfh);
	encode_diropargs(&xdr, args->tofh, args->toname, args->tolen);
	return 0;
}

/*
 * 2.2.14.  symlinkargs
 *
 *	struct symlinkargs {
 *		diropargs from;
 *		path to;
 *		sattr attributes;
 *	};
 */
static int nfs2_xdr_enc_symlinkargs(struct rpc_rqst *req, __be32 *p,
				    const struct nfs_symlinkargs *args)
{
	struct xdr_stream xdr;

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_diropargs(&xdr, args->fromfh, args->fromname, args->fromlen);
	encode_path(&xdr, args->pages, args->pathlen);
	encode_sattr(&xdr, args->sattr);
	return 0;
}

/*
 * 2.2.17.  readdirargs
 *
 *	struct readdirargs {
 *		fhandle dir;
 *		nfscookie cookie;
 *		unsigned count;
 *	};
 */
static void encode_readdirargs(struct xdr_stream *xdr,
			       const struct nfs_readdirargs *args)
{
	__be32 *p;

	encode_fhandle(xdr, args->fh);

	p = xdr_reserve_space(xdr, 4 + 4);
	*p++ = cpu_to_be32(args->cookie);
	*p = cpu_to_be32(args->count);
}

static int nfs2_xdr_enc_readdirargs(struct rpc_rqst *req, __be32 *p,
				    const struct nfs_readdirargs *args)
{
	struct xdr_stream xdr;

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_readdirargs(&xdr, args);
	prepare_reply_buffer(req, args->pages, 0,
					args->count, NFS_readdirres_sz);
	return 0;
}

/*
 * Decode the result of a readdir call.
 * We're not really decoding anymore, we just leave the buffer untouched
 * and only check that it is syntactically correct.
 * The real decoding happens in nfs_decode_entry below, called directly
 * from nfs_readdir for each entry.
 */
static int
nfs_xdr_readdirres(struct rpc_rqst *req, __be32 *p, void *dummy)
{
	struct xdr_buf *rcvbuf = &req->rq_rcv_buf;
	struct kvec *iov = rcvbuf->head;
	struct page **page;
	size_t hdrlen;
	unsigned int pglen, recvd;
	int status;

	if ((status = ntohl(*p++)))
		return nfs_stat_to_errno(status);

	hdrlen = (u8 *) p - (u8 *) iov->iov_base;
	if (iov->iov_len < hdrlen) {
		dprintk("NFS: READDIR reply header overflowed:"
				"length %Zu > %Zu\n", hdrlen, iov->iov_len);
		return -errno_NFSERR_IO;
	} else if (iov->iov_len != hdrlen) {
		dprintk("NFS: READDIR header is short. iovec will be shifted.\n");
		xdr_shift_buf(rcvbuf, iov->iov_len - hdrlen);
	}

	pglen = rcvbuf->page_len;
	recvd = rcvbuf->len - hdrlen;
	if (pglen > recvd)
		pglen = recvd;
	page = rcvbuf->pages;
	return pglen;
}

static void print_overflow_msg(const char *func, const struct xdr_stream *xdr)
{
	dprintk("nfs: %s: prematurely hit end of receive buffer. "
		"Remaining buffer length is %tu words.\n",
		func, xdr->end - xdr->p);
}

__be32 *
nfs_decode_dirent(struct xdr_stream *xdr, struct nfs_entry *entry, struct nfs_server *server, int plus)
{
	__be32 *p;
	p = xdr_inline_decode(xdr, 4);
	if (unlikely(!p))
		goto out_overflow;
	if (!ntohl(*p++)) {
		p = xdr_inline_decode(xdr, 4);
		if (unlikely(!p))
			goto out_overflow;
		if (!ntohl(*p++))
			return ERR_PTR(-EAGAIN);
		entry->eof = 1;
		return ERR_PTR(-EBADCOOKIE);
	}

	p = xdr_inline_decode(xdr, 8);
	if (unlikely(!p))
		goto out_overflow;

	entry->ino	  = ntohl(*p++);
	entry->len	  = ntohl(*p++);

	p = xdr_inline_decode(xdr, entry->len + 4);
	if (unlikely(!p))
		goto out_overflow;
	entry->name	  = (const char *) p;
	p		 += XDR_QUADLEN(entry->len);
	entry->prev_cookie	  = entry->cookie;
	entry->cookie	  = ntohl(*p++);

	entry->d_type = DT_UNKNOWN;

	p = xdr_inline_peek(xdr, 8);
	if (p != NULL)
		entry->eof = !p[0] && p[1];
	else
		entry->eof = 0;

	return p;

out_overflow:
	print_overflow_msg(__func__, xdr);
	return ERR_PTR(-EAGAIN);
}

/*
 * NFS XDR decode functions
 */
/*
 * Decode simple status reply
 */
static int
nfs_xdr_stat(struct rpc_rqst *req, __be32 *p, void *dummy)
{
	int	status;

	if ((status = ntohl(*p++)) != 0)
		status = nfs_stat_to_errno(status);
	return status;
}

/*
 * Decode attrstat reply
 * GETATTR, SETATTR, WRITE
 */
static int
nfs_xdr_attrstat(struct rpc_rqst *req, __be32 *p, struct nfs_fattr *fattr)
{
	int	status;

	if ((status = ntohl(*p++)))
		return nfs_stat_to_errno(status);
	xdr_decode_fattr(p, fattr);
	return 0;
}

/*
 * Decode diropres reply
 * LOOKUP, CREATE, MKDIR
 */
static int
nfs_xdr_diropres(struct rpc_rqst *req, __be32 *p, struct nfs_diropok *res)
{
	int	status;

	if ((status = ntohl(*p++)))
		return nfs_stat_to_errno(status);
	p = xdr_decode_fhandle(p, res->fh);
	xdr_decode_fattr(p, res->fattr);
	return 0;
}

/*
 * Decode READLINK reply
 */
static int
nfs_xdr_readlinkres(struct rpc_rqst *req, __be32 *p, void *dummy)
{
	struct xdr_buf *rcvbuf = &req->rq_rcv_buf;
	struct kvec *iov = rcvbuf->head;
	size_t hdrlen;
	u32 len, recvd;
	int	status;

	if ((status = ntohl(*p++)))
		return nfs_stat_to_errno(status);
	/* Convert length of symlink */
	len = ntohl(*p++);
	if (len >= rcvbuf->page_len) {
		dprintk("nfs: server returned giant symlink!\n");
		return -ENAMETOOLONG;
	}
	hdrlen = (u8 *) p - (u8 *) iov->iov_base;
	if (iov->iov_len < hdrlen) {
		dprintk("NFS: READLINK reply header overflowed:"
				"length %Zu > %Zu\n", hdrlen, iov->iov_len);
		return -errno_NFSERR_IO;
	} else if (iov->iov_len != hdrlen) {
		dprintk("NFS: READLINK header is short. iovec will be shifted.\n");
		xdr_shift_buf(rcvbuf, iov->iov_len - hdrlen);
	}
	recvd = req->rq_rcv_buf.len - hdrlen;
	if (recvd < len) {
		dprintk("NFS: server cheating in readlink reply: "
				"count %u > recvd %u\n", len, recvd);
		return -EIO;
	}

	xdr_terminate_string(rcvbuf, len);
	return 0;
}

/*
 * Decode WRITE reply
 */
static int
nfs_xdr_writeres(struct rpc_rqst *req, __be32 *p, struct nfs_writeres *res)
{
	res->verf->committed = NFS_FILE_SYNC;
	return nfs_xdr_attrstat(req, p, res->fattr);
}

/*
 * Decode STATFS reply
 */
static int
nfs_xdr_statfsres(struct rpc_rqst *req, __be32 *p, struct nfs2_fsstat *res)
{
	int	status;

	if ((status = ntohl(*p++)))
		return nfs_stat_to_errno(status);

	res->tsize  = ntohl(*p++);
	res->bsize  = ntohl(*p++);
	res->blocks = ntohl(*p++);
	res->bfree  = ntohl(*p++);
	res->bavail = ntohl(*p++);
	return 0;
}

/*
 * We need to translate between nfs status return values and
 * the local errno values which may not be the same.
 */
static struct {
	int stat;
	int errno;
} nfs_errtbl[] = {
	{ NFS_OK,		0		},
	{ NFSERR_PERM,		-EPERM		},
	{ NFSERR_NOENT,		-ENOENT		},
	{ NFSERR_IO,		-errno_NFSERR_IO},
	{ NFSERR_NXIO,		-ENXIO		},
/*	{ NFSERR_EAGAIN,	-EAGAIN		}, */
	{ NFSERR_ACCES,		-EACCES		},
	{ NFSERR_EXIST,		-EEXIST		},
	{ NFSERR_XDEV,		-EXDEV		},
	{ NFSERR_NODEV,		-ENODEV		},
	{ NFSERR_NOTDIR,	-ENOTDIR	},
	{ NFSERR_ISDIR,		-EISDIR		},
	{ NFSERR_INVAL,		-EINVAL		},
	{ NFSERR_FBIG,		-EFBIG		},
	{ NFSERR_NOSPC,		-ENOSPC		},
	{ NFSERR_ROFS,		-EROFS		},
	{ NFSERR_MLINK,		-EMLINK		},
	{ NFSERR_NAMETOOLONG,	-ENAMETOOLONG	},
	{ NFSERR_NOTEMPTY,	-ENOTEMPTY	},
	{ NFSERR_DQUOT,		-EDQUOT		},
	{ NFSERR_STALE,		-ESTALE		},
	{ NFSERR_REMOTE,	-EREMOTE	},
#ifdef EWFLUSH
	{ NFSERR_WFLUSH,	-EWFLUSH	},
#endif
	{ NFSERR_BADHANDLE,	-EBADHANDLE	},
	{ NFSERR_NOT_SYNC,	-ENOTSYNC	},
	{ NFSERR_BAD_COOKIE,	-EBADCOOKIE	},
	{ NFSERR_NOTSUPP,	-ENOTSUPP	},
	{ NFSERR_TOOSMALL,	-ETOOSMALL	},
	{ NFSERR_SERVERFAULT,	-EREMOTEIO	},
	{ NFSERR_BADTYPE,	-EBADTYPE	},
	{ NFSERR_JUKEBOX,	-EJUKEBOX	},
	{ -1,			-EIO		}
};

/*
 * Convert an NFS error code to a local one.
 * This one is used jointly by NFSv2 and NFSv3.
 */
int
nfs_stat_to_errno(int stat)
{
	int i;

	for (i = 0; nfs_errtbl[i].stat != -1; i++) {
		if (nfs_errtbl[i].stat == stat)
			return nfs_errtbl[i].errno;
	}
	dprintk("nfs_stat_to_errno: bad nfs status return value: %d\n", stat);
	return nfs_errtbl[i].errno;
}

#define PROC(proc, argtype, restype, timer)				\
[NFSPROC_##proc] = {							\
	.p_proc	    =  NFSPROC_##proc,					\
	.p_encode   =  (kxdrproc_t)nfs2_xdr_enc_##argtype,		\
	.p_decode   =  (kxdrproc_t) nfs_xdr_##restype,			\
	.p_arglen   =  NFS_##argtype##_sz,				\
	.p_replen   =  NFS_##restype##_sz,				\
	.p_timer    =  timer,						\
	.p_statidx  =  NFSPROC_##proc,					\
	.p_name     =  #proc,						\
	}
struct rpc_procinfo	nfs_procedures[] = {
    PROC(GETATTR,	fhandle,	attrstat, 1),
    PROC(SETATTR,	sattrargs,	attrstat, 0),
    PROC(LOOKUP,	diropargs,	diropres, 2),
    PROC(READLINK,	readlinkargs,	readlinkres, 3),
    PROC(READ,		readargs,	readres, 3),
    PROC(WRITE,		writeargs,	writeres, 4),
    PROC(CREATE,	createargs,	diropres, 0),
    PROC(REMOVE,	removeargs,	stat, 0),
    PROC(RENAME,	renameargs,	stat, 0),
    PROC(LINK,		linkargs,	stat, 0),
    PROC(SYMLINK,	symlinkargs,	stat, 0),
    PROC(MKDIR,		createargs,	diropres, 0),
    PROC(RMDIR,		diropargs,	stat, 0),
    PROC(READDIR,	readdirargs,	readdirres, 3),
    PROC(STATFS,	fhandle,	statfsres, 0),
};

struct rpc_version		nfs_version2 = {
	.number			= 2,
	.nrprocs		= ARRAY_SIZE(nfs_procedures),
	.procs			= nfs_procedures
};
