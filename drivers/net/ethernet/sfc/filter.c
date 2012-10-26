/****************************************************************************
 * Driver for Solarflare Solarstorm network controllers and boards
 * Copyright 2005-2010 Solarflare Communications Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, incorporated herein by reference.
 */

#include <linux/in.h>
#include <net/ip.h>
#include "efx.h"
#include "filter.h"
#include "io.h"
#include "nic.h"
#include "farch_regs.h"

/* "Fudge factors" - difference between programmed value and actual depth.
 * Due to pipelined implementation we need to program H/W with a value that
 * is larger than the hop limit we want.
 */
#define EFX_FARCH_FILTER_CTL_SRCH_FUDGE_WILD 3
#define EFX_FARCH_FILTER_CTL_SRCH_FUDGE_FULL 1

/* Hard maximum search limit.  Hardware will time-out beyond 200-something.
 * We also need to avoid infinite loops in efx_farch_filter_search() when the
 * table is full.
 */
#define EFX_FARCH_FILTER_CTL_SRCH_MAX 200

/* Don't try very hard to find space for performance hints, as this is
 * counter-productive. */
#define EFX_FARCH_FILTER_CTL_SRCH_HINT_MAX 5

enum efx_farch_filter_type {
	EFX_FARCH_FILTER_TCP_FULL = 0,
	EFX_FARCH_FILTER_TCP_WILD,
	EFX_FARCH_FILTER_UDP_FULL,
	EFX_FARCH_FILTER_UDP_WILD,
	EFX_FARCH_FILTER_MAC_FULL = 4,
	EFX_FARCH_FILTER_MAC_WILD,
	EFX_FARCH_FILTER_UC_DEF = 8,
	EFX_FARCH_FILTER_MC_DEF,
	EFX_FARCH_FILTER_TYPE_COUNT,		/* number of specific types */
};

enum efx_farch_filter_table_id {
	EFX_FARCH_FILTER_TABLE_RX_IP = 0,
	EFX_FARCH_FILTER_TABLE_RX_MAC,
	EFX_FARCH_FILTER_TABLE_RX_DEF,
	EFX_FARCH_FILTER_TABLE_TX_MAC,
	EFX_FARCH_FILTER_TABLE_COUNT,
};

enum efx_farch_filter_index {
	EFX_FARCH_FILTER_INDEX_UC_DEF,
	EFX_FARCH_FILTER_INDEX_MC_DEF,
	EFX_FARCH_FILTER_SIZE_RX_DEF,
};

struct efx_farch_filter_spec {
	u8	type:4;
	u8	priority:4;
	u8	flags;
	u16	dmaq_id;
	u32	data[3];
};

struct efx_farch_filter_table {
	enum efx_farch_filter_table_id id;
	u32		offset;		/* address of table relative to BAR */
	unsigned	size;		/* number of entries */
	unsigned	step;		/* step between entries */
	unsigned	used;		/* number currently used */
	unsigned long	*used_bitmap;
	struct efx_farch_filter_spec *spec;
	unsigned	search_limit[EFX_FARCH_FILTER_TYPE_COUNT];
};

struct efx_farch_filter_state {
	struct efx_farch_filter_table table[EFX_FARCH_FILTER_TABLE_COUNT];
};

static void
efx_farch_filter_table_clear_entry(struct efx_nic *efx,
				   struct efx_farch_filter_table *table,
				   unsigned int filter_idx);

/* The filter hash function is LFSR polynomial x^16 + x^3 + 1 of a 32-bit
 * key derived from the n-tuple.  The initial LFSR state is 0xffff. */
static u16 efx_farch_filter_hash(u32 key)
{
	u16 tmp;

	/* First 16 rounds */
	tmp = 0x1fff ^ key >> 16;
	tmp = tmp ^ tmp >> 3 ^ tmp >> 6;
	tmp = tmp ^ tmp >> 9;
	/* Last 16 rounds */
	tmp = tmp ^ tmp << 13 ^ key;
	tmp = tmp ^ tmp >> 3 ^ tmp >> 6;
	return tmp ^ tmp >> 9;
}

/* To allow for hash collisions, filter search continues at these
 * increments from the first possible entry selected by the hash. */
static u16 efx_farch_filter_increment(u32 key)
{
	return key * 2 - 1;
}

static enum efx_farch_filter_table_id
efx_farch_filter_spec_table_id(const struct efx_farch_filter_spec *spec)
{
	BUILD_BUG_ON(EFX_FARCH_FILTER_TABLE_RX_IP !=
		     (EFX_FARCH_FILTER_TCP_FULL >> 2));
	BUILD_BUG_ON(EFX_FARCH_FILTER_TABLE_RX_IP !=
		     (EFX_FARCH_FILTER_TCP_WILD >> 2));
	BUILD_BUG_ON(EFX_FARCH_FILTER_TABLE_RX_IP !=
		     (EFX_FARCH_FILTER_UDP_FULL >> 2));
	BUILD_BUG_ON(EFX_FARCH_FILTER_TABLE_RX_IP !=
		     (EFX_FARCH_FILTER_UDP_WILD >> 2));
	BUILD_BUG_ON(EFX_FARCH_FILTER_TABLE_RX_MAC !=
		     (EFX_FARCH_FILTER_MAC_FULL >> 2));
	BUILD_BUG_ON(EFX_FARCH_FILTER_TABLE_RX_MAC !=
		     (EFX_FARCH_FILTER_MAC_WILD >> 2));
	BUILD_BUG_ON(EFX_FARCH_FILTER_TABLE_TX_MAC !=
		     EFX_FARCH_FILTER_TABLE_RX_MAC + 2);
	return (spec->type >> 2) + ((spec->flags & EFX_FILTER_FLAG_TX) ? 2 : 0);
}

static void efx_farch_filter_push_rx_config(struct efx_nic *efx)
{
	struct efx_farch_filter_state *state = efx->filter_state;
	struct efx_farch_filter_table *table;
	efx_oword_t filter_ctl;

	efx_reado(efx, &filter_ctl, FR_BZ_RX_FILTER_CTL);

	table = &state->table[EFX_FARCH_FILTER_TABLE_RX_IP];
	EFX_SET_OWORD_FIELD(filter_ctl, FRF_BZ_TCP_FULL_SRCH_LIMIT,
			    table->search_limit[EFX_FARCH_FILTER_TCP_FULL] +
			    EFX_FARCH_FILTER_CTL_SRCH_FUDGE_FULL);
	EFX_SET_OWORD_FIELD(filter_ctl, FRF_BZ_TCP_WILD_SRCH_LIMIT,
			    table->search_limit[EFX_FARCH_FILTER_TCP_WILD] +
			    EFX_FARCH_FILTER_CTL_SRCH_FUDGE_WILD);
	EFX_SET_OWORD_FIELD(filter_ctl, FRF_BZ_UDP_FULL_SRCH_LIMIT,
			    table->search_limit[EFX_FARCH_FILTER_UDP_FULL] +
			    EFX_FARCH_FILTER_CTL_SRCH_FUDGE_FULL);
	EFX_SET_OWORD_FIELD(filter_ctl, FRF_BZ_UDP_WILD_SRCH_LIMIT,
			    table->search_limit[EFX_FARCH_FILTER_UDP_WILD] +
			    EFX_FARCH_FILTER_CTL_SRCH_FUDGE_WILD);

	table = &state->table[EFX_FARCH_FILTER_TABLE_RX_MAC];
	if (table->size) {
		EFX_SET_OWORD_FIELD(
			filter_ctl, FRF_CZ_ETHERNET_FULL_SEARCH_LIMIT,
			table->search_limit[EFX_FARCH_FILTER_MAC_FULL] +
			EFX_FARCH_FILTER_CTL_SRCH_FUDGE_FULL);
		EFX_SET_OWORD_FIELD(
			filter_ctl, FRF_CZ_ETHERNET_WILDCARD_SEARCH_LIMIT,
			table->search_limit[EFX_FARCH_FILTER_MAC_WILD] +
			EFX_FARCH_FILTER_CTL_SRCH_FUDGE_WILD);
	}

	table = &state->table[EFX_FARCH_FILTER_TABLE_RX_DEF];
	if (table->size) {
		EFX_SET_OWORD_FIELD(
			filter_ctl, FRF_CZ_UNICAST_NOMATCH_Q_ID,
			table->spec[EFX_FARCH_FILTER_INDEX_UC_DEF].dmaq_id);
		EFX_SET_OWORD_FIELD(
			filter_ctl, FRF_CZ_UNICAST_NOMATCH_RSS_ENABLED,
			!!(table->spec[EFX_FARCH_FILTER_INDEX_UC_DEF].flags &
			   EFX_FILTER_FLAG_RX_RSS));
		EFX_SET_OWORD_FIELD(
			filter_ctl, FRF_CZ_MULTICAST_NOMATCH_Q_ID,
			table->spec[EFX_FARCH_FILTER_INDEX_MC_DEF].dmaq_id);
		EFX_SET_OWORD_FIELD(
			filter_ctl, FRF_CZ_MULTICAST_NOMATCH_RSS_ENABLED,
			!!(table->spec[EFX_FARCH_FILTER_INDEX_MC_DEF].flags &
			   EFX_FILTER_FLAG_RX_RSS));

		/* There is a single bit to enable RX scatter for all
		 * unmatched packets.  Only set it if scatter is
		 * enabled in both filter specs.
		 */
		EFX_SET_OWORD_FIELD(
			filter_ctl, FRF_BZ_SCATTER_ENBL_NO_MATCH_Q,
			!!(table->spec[EFX_FARCH_FILTER_INDEX_UC_DEF].flags &
			   table->spec[EFX_FARCH_FILTER_INDEX_MC_DEF].flags &
			   EFX_FILTER_FLAG_RX_SCATTER));
	} else if (efx_nic_rev(efx) >= EFX_REV_FALCON_B0) {
		/* We don't expose 'default' filters because unmatched
		 * packets always go to the queue number found in the
		 * RSS table.  But we still need to set the RX scatter
		 * bit here.
		 */
		EFX_SET_OWORD_FIELD(
			filter_ctl, FRF_BZ_SCATTER_ENBL_NO_MATCH_Q,
			efx->rx_scatter);
	}

	efx_writeo(efx, &filter_ctl, FR_BZ_RX_FILTER_CTL);
}

static void efx_farch_filter_push_tx_limits(struct efx_nic *efx)
{
	struct efx_farch_filter_state *state = efx->filter_state;
	struct efx_farch_filter_table *table;
	efx_oword_t tx_cfg;

	efx_reado(efx, &tx_cfg, FR_AZ_TX_CFG);

	table = &state->table[EFX_FARCH_FILTER_TABLE_TX_MAC];
	if (table->size) {
		EFX_SET_OWORD_FIELD(
			tx_cfg, FRF_CZ_TX_ETH_FILTER_FULL_SEARCH_RANGE,
			table->search_limit[EFX_FARCH_FILTER_MAC_FULL] +
			EFX_FARCH_FILTER_CTL_SRCH_FUDGE_FULL);
		EFX_SET_OWORD_FIELD(
			tx_cfg, FRF_CZ_TX_ETH_FILTER_WILD_SEARCH_RANGE,
			table->search_limit[EFX_FARCH_FILTER_MAC_WILD] +
			EFX_FARCH_FILTER_CTL_SRCH_FUDGE_WILD);
	}

	efx_writeo(efx, &tx_cfg, FR_AZ_TX_CFG);
}

static int
efx_farch_filter_from_gen_spec(struct efx_farch_filter_spec *spec,
			       const struct efx_filter_spec *gen_spec)
{
	bool is_full = false;

	if ((gen_spec->flags & EFX_FILTER_FLAG_RX_RSS) &&
	    gen_spec->rss_context != EFX_FILTER_RSS_CONTEXT_DEFAULT)
		return -EINVAL;

	spec->priority = gen_spec->priority;
	spec->flags = gen_spec->flags;
	spec->dmaq_id = gen_spec->dmaq_id;

	switch (gen_spec->match_flags) {
	case (EFX_FILTER_MATCH_ETHER_TYPE | EFX_FILTER_MATCH_IP_PROTO |
	      EFX_FILTER_MATCH_LOC_HOST | EFX_FILTER_MATCH_LOC_PORT |
	      EFX_FILTER_MATCH_REM_HOST | EFX_FILTER_MATCH_REM_PORT):
		is_full = true;
		/* fall through */
	case (EFX_FILTER_MATCH_ETHER_TYPE | EFX_FILTER_MATCH_IP_PROTO |
	      EFX_FILTER_MATCH_LOC_HOST | EFX_FILTER_MATCH_LOC_PORT): {
		__be32 rhost, host1, host2;
		__be16 rport, port1, port2;

		EFX_BUG_ON_PARANOID(!(gen_spec->flags & EFX_FILTER_FLAG_RX));

		if (gen_spec->ether_type != htons(ETH_P_IP))
			return -EPROTONOSUPPORT;
		if (gen_spec->loc_port == 0 ||
		    (is_full && gen_spec->rem_port == 0))
			return -EADDRNOTAVAIL;
		switch (gen_spec->ip_proto) {
		case IPPROTO_TCP:
			spec->type = (is_full ? EFX_FARCH_FILTER_TCP_FULL :
				      EFX_FARCH_FILTER_TCP_WILD);
			break;
		case IPPROTO_UDP:
			spec->type = (is_full ? EFX_FARCH_FILTER_UDP_FULL :
				      EFX_FARCH_FILTER_UDP_WILD);
			break;
		default:
			return -EPROTONOSUPPORT;
		}

		/* Filter is constructed in terms of source and destination,
		 * with the odd wrinkle that the ports are swapped in a UDP
		 * wildcard filter.  We need to convert from local and remote
		 * (= zero for wildcard) addresses.
		 */
		rhost = is_full ? gen_spec->rem_host[0] : 0;
		rport = is_full ? gen_spec->rem_port : 0;
		host1 = rhost;
		host2 = gen_spec->loc_host[0];
		if (!is_full && gen_spec->ip_proto == IPPROTO_UDP) {
			port1 = gen_spec->loc_port;
			port2 = rport;
		} else {
			port1 = rport;
			port2 = gen_spec->loc_port;
		}
		spec->data[0] = ntohl(host1) << 16 | ntohs(port1);
		spec->data[1] = ntohs(port2) << 16 | ntohl(host1) >> 16;
		spec->data[2] = ntohl(host2);

		break;
	}

	case EFX_FILTER_MATCH_LOC_MAC | EFX_FILTER_MATCH_OUTER_VID:
		is_full = true;
		/* fall through */
	case EFX_FILTER_MATCH_LOC_MAC:
		spec->type = (is_full ? EFX_FARCH_FILTER_MAC_FULL :
			      EFX_FARCH_FILTER_MAC_WILD);
		spec->data[0] = is_full ? ntohs(gen_spec->outer_vid) : 0;
		spec->data[1] = (gen_spec->loc_mac[2] << 24 |
				 gen_spec->loc_mac[3] << 16 |
				 gen_spec->loc_mac[4] << 8 |
				 gen_spec->loc_mac[5]);
		spec->data[2] = (gen_spec->loc_mac[0] << 8 |
				 gen_spec->loc_mac[1]);
		break;

	case EFX_FILTER_MATCH_LOC_MAC_IG:
		spec->type = (is_multicast_ether_addr(gen_spec->loc_mac) ?
			      EFX_FARCH_FILTER_MC_DEF :
			      EFX_FARCH_FILTER_UC_DEF);
		memset(spec->data, 0, sizeof(spec->data)); /* ensure equality */
		break;

	default:
		return -EPROTONOSUPPORT;
	}

	return 0;
}

static void
efx_farch_filter_to_gen_spec(struct efx_filter_spec *gen_spec,
			     const struct efx_farch_filter_spec *spec)
{
	bool is_full = false;

	/* *gen_spec should be completely initialised, to be consistent
	 * with efx_filter_init_{rx,tx}() and in case we want to copy
	 * it back to userland.
	 */
	memset(gen_spec, 0, sizeof(*gen_spec));

	gen_spec->priority = spec->priority;
	gen_spec->flags = spec->flags;
	gen_spec->dmaq_id = spec->dmaq_id;

	switch (spec->type) {
	case EFX_FARCH_FILTER_TCP_FULL:
	case EFX_FARCH_FILTER_UDP_FULL:
		is_full = true;
		/* fall through */
	case EFX_FARCH_FILTER_TCP_WILD:
	case EFX_FARCH_FILTER_UDP_WILD: {
		__be32 host1, host2;
		__be16 port1, port2;

		gen_spec->match_flags =
			EFX_FILTER_MATCH_ETHER_TYPE |
			EFX_FILTER_MATCH_IP_PROTO |
			EFX_FILTER_MATCH_LOC_HOST | EFX_FILTER_MATCH_LOC_PORT;
		if (is_full)
			gen_spec->match_flags |= (EFX_FILTER_MATCH_REM_HOST |
						  EFX_FILTER_MATCH_REM_PORT);
		gen_spec->ether_type = htons(ETH_P_IP);
		gen_spec->ip_proto =
			(spec->type == EFX_FARCH_FILTER_TCP_FULL ||
			 spec->type == EFX_FARCH_FILTER_TCP_WILD) ?
			IPPROTO_TCP : IPPROTO_UDP;

		host1 = htonl(spec->data[0] >> 16 | spec->data[1] << 16);
		port1 = htons(spec->data[0]);
		host2 = htonl(spec->data[2]);
		port2 = htons(spec->data[1] >> 16);
		if (spec->flags & EFX_FILTER_FLAG_TX) {
			gen_spec->loc_host[0] = host1;
			gen_spec->rem_host[0] = host2;
		} else {
			gen_spec->loc_host[0] = host2;
			gen_spec->rem_host[0] = host1;
		}
		if (!!(gen_spec->flags & EFX_FILTER_FLAG_TX) ^
		    (!is_full && gen_spec->ip_proto == IPPROTO_UDP)) {
			gen_spec->loc_port = port1;
			gen_spec->rem_port = port2;
		} else {
			gen_spec->loc_port = port2;
			gen_spec->rem_port = port1;
		}

		break;
	}

	case EFX_FARCH_FILTER_MAC_FULL:
		is_full = true;
		/* fall through */
	case EFX_FARCH_FILTER_MAC_WILD:
		gen_spec->match_flags = EFX_FILTER_MATCH_LOC_MAC;
		if (is_full)
			gen_spec->match_flags |= EFX_FILTER_MATCH_OUTER_VID;
		gen_spec->loc_mac[0] = spec->data[2] >> 8;
		gen_spec->loc_mac[1] = spec->data[2];
		gen_spec->loc_mac[2] = spec->data[1] >> 24;
		gen_spec->loc_mac[3] = spec->data[1] >> 16;
		gen_spec->loc_mac[4] = spec->data[1] >> 8;
		gen_spec->loc_mac[5] = spec->data[1];
		gen_spec->outer_vid = htons(spec->data[0]);
		break;

	case EFX_FARCH_FILTER_UC_DEF:
	case EFX_FARCH_FILTER_MC_DEF:
		gen_spec->match_flags = EFX_FILTER_MATCH_LOC_MAC_IG;
		gen_spec->loc_mac[0] = spec->type == EFX_FARCH_FILTER_MC_DEF;
		break;

	default:
		WARN_ON(1);
		break;
	}
}

static void
efx_farch_filter_reset_rx_def(struct efx_nic *efx, unsigned filter_idx)
{
	struct efx_farch_filter_state *state = efx->filter_state;
	struct efx_farch_filter_table *table =
		&state->table[EFX_FARCH_FILTER_TABLE_RX_DEF];
	struct efx_farch_filter_spec *spec = &table->spec[filter_idx];

	/* If there's only one channel then disable RSS for non VF
	 * traffic, thereby allowing VFs to use RSS when the PF can't.
	 */
	spec->type = EFX_FARCH_FILTER_UC_DEF + filter_idx;
	spec->priority = EFX_FILTER_PRI_MANUAL;
	spec->flags = (EFX_FILTER_FLAG_RX |
		       (efx->n_rx_channels > 1 ? EFX_FILTER_FLAG_RX_RSS : 0) |
		       (efx->rx_scatter ? EFX_FILTER_FLAG_RX_SCATTER : 0));
	spec->dmaq_id = 0;
	table->used_bitmap[0] |= 1 << filter_idx;
}

/* Build a filter entry and return its n-tuple key. */
static u32 efx_farch_filter_build(efx_oword_t *filter,
				  struct efx_farch_filter_spec *spec)
{
	u32 data3;

	switch (efx_farch_filter_spec_table_id(spec)) {
	case EFX_FARCH_FILTER_TABLE_RX_IP: {
		bool is_udp = (spec->type == EFX_FARCH_FILTER_UDP_FULL ||
			       spec->type == EFX_FARCH_FILTER_UDP_WILD);
		EFX_POPULATE_OWORD_7(
			*filter,
			FRF_BZ_RSS_EN,
			!!(spec->flags & EFX_FILTER_FLAG_RX_RSS),
			FRF_BZ_SCATTER_EN,
			!!(spec->flags & EFX_FILTER_FLAG_RX_SCATTER),
			FRF_BZ_TCP_UDP, is_udp,
			FRF_BZ_RXQ_ID, spec->dmaq_id,
			EFX_DWORD_2, spec->data[2],
			EFX_DWORD_1, spec->data[1],
			EFX_DWORD_0, spec->data[0]);
		data3 = is_udp;
		break;
	}

	case EFX_FARCH_FILTER_TABLE_RX_MAC: {
		bool is_wild = spec->type == EFX_FARCH_FILTER_MAC_WILD;
		EFX_POPULATE_OWORD_7(
			*filter,
			FRF_CZ_RMFT_RSS_EN,
			!!(spec->flags & EFX_FILTER_FLAG_RX_RSS),
			FRF_CZ_RMFT_SCATTER_EN,
			!!(spec->flags & EFX_FILTER_FLAG_RX_SCATTER),
			FRF_CZ_RMFT_RXQ_ID, spec->dmaq_id,
			FRF_CZ_RMFT_WILDCARD_MATCH, is_wild,
			FRF_CZ_RMFT_DEST_MAC_HI, spec->data[2],
			FRF_CZ_RMFT_DEST_MAC_LO, spec->data[1],
			FRF_CZ_RMFT_VLAN_ID, spec->data[0]);
		data3 = is_wild;
		break;
	}

	case EFX_FARCH_FILTER_TABLE_TX_MAC: {
		bool is_wild = spec->type == EFX_FARCH_FILTER_MAC_WILD;
		EFX_POPULATE_OWORD_5(*filter,
				     FRF_CZ_TMFT_TXQ_ID, spec->dmaq_id,
				     FRF_CZ_TMFT_WILDCARD_MATCH, is_wild,
				     FRF_CZ_TMFT_SRC_MAC_HI, spec->data[2],
				     FRF_CZ_TMFT_SRC_MAC_LO, spec->data[1],
				     FRF_CZ_TMFT_VLAN_ID, spec->data[0]);
		data3 = is_wild | spec->dmaq_id << 1;
		break;
	}

	default:
		BUG();
	}

	return spec->data[0] ^ spec->data[1] ^ spec->data[2] ^ data3;
}

static bool efx_farch_filter_equal(const struct efx_farch_filter_spec *left,
				   const struct efx_farch_filter_spec *right)
{
	if (left->type != right->type ||
	    memcmp(left->data, right->data, sizeof(left->data)))
		return false;

	if (left->flags & EFX_FILTER_FLAG_TX &&
	    left->dmaq_id != right->dmaq_id)
		return false;

	return true;
}

/*
 * Construct/deconstruct external filter IDs.  At least the RX filter
 * IDs must be ordered by matching priority, for RX NFC semantics.
 *
 * Deconstruction needs to be robust against invalid IDs so that
 * efx_filter_remove_id_safe() and efx_filter_get_filter_safe() can
 * accept user-provided IDs.
 */

#define EFX_FARCH_FILTER_MATCH_PRI_COUNT	5

static const u8 efx_farch_filter_type_match_pri[EFX_FARCH_FILTER_TYPE_COUNT] = {
	[EFX_FARCH_FILTER_TCP_FULL]	= 0,
	[EFX_FARCH_FILTER_UDP_FULL]	= 0,
	[EFX_FARCH_FILTER_TCP_WILD]	= 1,
	[EFX_FARCH_FILTER_UDP_WILD]	= 1,
	[EFX_FARCH_FILTER_MAC_FULL]	= 2,
	[EFX_FARCH_FILTER_MAC_WILD]	= 3,
	[EFX_FARCH_FILTER_UC_DEF]	= 4,
	[EFX_FARCH_FILTER_MC_DEF]	= 4,
};

static const enum efx_farch_filter_table_id efx_farch_filter_range_table[] = {
	EFX_FARCH_FILTER_TABLE_RX_IP,	/* RX match pri 0 */
	EFX_FARCH_FILTER_TABLE_RX_IP,
	EFX_FARCH_FILTER_TABLE_RX_MAC,
	EFX_FARCH_FILTER_TABLE_RX_MAC,
	EFX_FARCH_FILTER_TABLE_RX_DEF,	/* RX match pri 4 */
	EFX_FARCH_FILTER_TABLE_TX_MAC,	/* TX match pri 0 */
	EFX_FARCH_FILTER_TABLE_TX_MAC,	/* TX match pri 1 */
};

#define EFX_FARCH_FILTER_INDEX_WIDTH 13
#define EFX_FARCH_FILTER_INDEX_MASK ((1 << EFX_FARCH_FILTER_INDEX_WIDTH) - 1)

static inline u32
efx_farch_filter_make_id(const struct efx_farch_filter_spec *spec,
			 unsigned int index)
{
	unsigned int range;

	range = efx_farch_filter_type_match_pri[spec->type];
	if (!(spec->flags & EFX_FILTER_FLAG_RX))
		range += EFX_FARCH_FILTER_MATCH_PRI_COUNT;

	return range << EFX_FARCH_FILTER_INDEX_WIDTH | index;
}

static inline enum efx_farch_filter_table_id
efx_farch_filter_id_table_id(u32 id)
{
	unsigned int range = id >> EFX_FARCH_FILTER_INDEX_WIDTH;

	if (range < ARRAY_SIZE(efx_farch_filter_range_table))
		return efx_farch_filter_range_table[range];
	else
		return EFX_FARCH_FILTER_TABLE_COUNT; /* invalid */
}

static inline unsigned int efx_farch_filter_id_index(u32 id)
{
	return id & EFX_FARCH_FILTER_INDEX_MASK;
}

u32 efx_filter_get_rx_id_limit(struct efx_nic *efx)
{
	struct efx_farch_filter_state *state = efx->filter_state;
	unsigned int range = EFX_FARCH_FILTER_MATCH_PRI_COUNT - 1;
	enum efx_farch_filter_table_id table_id;

	do {
		table_id = efx_farch_filter_range_table[range];
		if (state->table[table_id].size != 0)
			return range << EFX_FARCH_FILTER_INDEX_WIDTH |
				state->table[table_id].size;
	} while (range--);

	return 0;
}

/**
 * efx_filter_insert_filter - add or replace a filter
 * @efx: NIC in which to insert the filter
 * @spec: Specification for the filter
 * @replace_equal: Flag for whether the specified filter may replace an
 *	existing filter with equal priority
 *
 * On success, return the filter ID.
 * On failure, return a negative error code.
 *
 * If an existing filter has equal match values to the new filter
 * spec, then the new filter might replace it, depending on the
 * relative priorities.  If the existing filter has lower priority, or
 * if @replace_equal is set and it has equal priority, then it is
 * replaced.  Otherwise the function fails, returning -%EPERM if
 * the existing filter has higher priority or -%EEXIST if it has
 * equal priority.
 */
s32 efx_filter_insert_filter(struct efx_nic *efx,
			     struct efx_filter_spec *gen_spec,
			     bool replace_equal)
{
	struct efx_farch_filter_state *state = efx->filter_state;
	struct efx_farch_filter_table *table;
	struct efx_farch_filter_spec spec;
	efx_oword_t filter;
	int rep_index, ins_index;
	unsigned int depth = 0;
	int rc;

	rc = efx_farch_filter_from_gen_spec(&spec, gen_spec);
	if (rc)
		return rc;

	table = &state->table[efx_farch_filter_spec_table_id(&spec)];
	if (table->size == 0)
		return -EINVAL;

	netif_vdbg(efx, hw, efx->net_dev,
		   "%s: type %d search_limit=%d", __func__, spec.type,
		   table->search_limit[spec.type]);

	if (table->id == EFX_FARCH_FILTER_TABLE_RX_DEF) {
		/* One filter spec per type */
		BUILD_BUG_ON(EFX_FARCH_FILTER_INDEX_UC_DEF != 0);
		BUILD_BUG_ON(EFX_FARCH_FILTER_INDEX_MC_DEF !=
			     EFX_FARCH_FILTER_MC_DEF - EFX_FARCH_FILTER_UC_DEF);
		rep_index = spec.type - EFX_FARCH_FILTER_UC_DEF;
		ins_index = rep_index;

		spin_lock_bh(&efx->filter_lock);
	} else {
		/* Search concurrently for
		 * (1) a filter to be replaced (rep_index): any filter
		 *     with the same match values, up to the current
		 *     search depth for this type, and
		 * (2) the insertion point (ins_index): (1) or any
		 *     free slot before it or up to the maximum search
		 *     depth for this priority
		 * We fail if we cannot find (2).
		 *
		 * We can stop once either
		 * (a) we find (1), in which case we have definitely
		 *     found (2) as well; or
		 * (b) we have searched exhaustively for (1), and have
		 *     either found (2) or searched exhaustively for it
		 */
		u32 key = efx_farch_filter_build(&filter, &spec);
		unsigned int hash = efx_farch_filter_hash(key);
		unsigned int incr = efx_farch_filter_increment(key);
		unsigned int max_rep_depth = table->search_limit[spec.type];
		unsigned int max_ins_depth =
			spec.priority <= EFX_FILTER_PRI_HINT ?
			EFX_FARCH_FILTER_CTL_SRCH_HINT_MAX :
			EFX_FARCH_FILTER_CTL_SRCH_MAX;
		unsigned int i = hash & (table->size - 1);

		ins_index = -1;
		depth = 1;

		spin_lock_bh(&efx->filter_lock);

		for (;;) {
			if (!test_bit(i, table->used_bitmap)) {
				if (ins_index < 0)
					ins_index = i;
			} else if (efx_farch_filter_equal(&spec,
							  &table->spec[i])) {
				/* Case (a) */
				if (ins_index < 0)
					ins_index = i;
				rep_index = i;
				break;
			}

			if (depth >= max_rep_depth &&
			    (ins_index >= 0 || depth >= max_ins_depth)) {
				/* Case (b) */
				if (ins_index < 0) {
					rc = -EBUSY;
					goto out;
				}
				rep_index = -1;
				break;
			}

			i = (i + incr) & (table->size - 1);
			++depth;
		}
	}

	/* If we found a filter to be replaced, check whether we
	 * should do so
	 */
	if (rep_index >= 0) {
		struct efx_farch_filter_spec *saved_spec =
			&table->spec[rep_index];

		if (spec.priority == saved_spec->priority && !replace_equal) {
			rc = -EEXIST;
			goto out;
		}
		if (spec.priority < saved_spec->priority) {
			rc = -EPERM;
			goto out;
		}
	}

	/* Insert the filter */
	if (ins_index != rep_index) {
		__set_bit(ins_index, table->used_bitmap);
		++table->used;
	}
	table->spec[ins_index] = spec;

	if (table->id == EFX_FARCH_FILTER_TABLE_RX_DEF) {
		efx_farch_filter_push_rx_config(efx);
	} else {
		if (table->search_limit[spec.type] < depth) {
			table->search_limit[spec.type] = depth;
			if (spec.flags & EFX_FILTER_FLAG_TX)
				efx_farch_filter_push_tx_limits(efx);
			else
				efx_farch_filter_push_rx_config(efx);
		}

		efx_writeo(efx, &filter,
			   table->offset + table->step * ins_index);

		/* If we were able to replace a filter by inserting
		 * at a lower depth, clear the replaced filter
		 */
		if (ins_index != rep_index && rep_index >= 0)
			efx_farch_filter_table_clear_entry(efx, table,
							   rep_index);
	}

	netif_vdbg(efx, hw, efx->net_dev,
		   "%s: filter type %d index %d rxq %u set",
		   __func__, spec.type, ins_index, spec.dmaq_id);
	rc = efx_farch_filter_make_id(&spec, ins_index);

out:
	spin_unlock_bh(&efx->filter_lock);
	return rc;
}

static void
efx_farch_filter_table_clear_entry(struct efx_nic *efx,
				   struct efx_farch_filter_table *table,
				   unsigned int filter_idx)
{
	static efx_oword_t filter;

	if (table->id == EFX_FARCH_FILTER_TABLE_RX_DEF) {
		/* RX default filters must always exist */
		efx_farch_filter_reset_rx_def(efx, filter_idx);
		efx_farch_filter_push_rx_config(efx);
	} else if (test_bit(filter_idx, table->used_bitmap)) {
		__clear_bit(filter_idx, table->used_bitmap);
		--table->used;
		memset(&table->spec[filter_idx], 0, sizeof(table->spec[0]));

		efx_writeo(efx, &filter,
			   table->offset + table->step * filter_idx);

		/* If this filter required a greater search depth than
		 * any other, the search limit for its type can now be
		 * decreased.  However, it is hard to determine that
		 * unless the table has become completely empty - in
		 * which case, all its search limits can be set to 0.
		 */
		if (unlikely(table->used == 0)) {
			memset(table->search_limit, 0,
			       sizeof(table->search_limit));
			if (table->id == EFX_FARCH_FILTER_TABLE_TX_MAC)
				efx_farch_filter_push_tx_limits(efx);
			else
				efx_farch_filter_push_rx_config(efx);
		}
	}
}

/**
 * efx_filter_remove_id_safe - remove a filter by ID, carefully
 * @efx: NIC from which to remove the filter
 * @priority: Priority of filter, as passed to @efx_filter_insert_filter
 * @filter_id: ID of filter, as returned by @efx_filter_insert_filter
 *
 * This function will range-check @filter_id, so it is safe to call
 * with a value passed from userland.
 */
int efx_filter_remove_id_safe(struct efx_nic *efx,
			      enum efx_filter_priority priority,
			      u32 filter_id)
{
	struct efx_farch_filter_state *state = efx->filter_state;
	enum efx_farch_filter_table_id table_id;
	struct efx_farch_filter_table *table;
	unsigned int filter_idx;
	struct efx_farch_filter_spec *spec;
	int rc;

	table_id = efx_farch_filter_id_table_id(filter_id);
	if ((unsigned int)table_id >= EFX_FARCH_FILTER_TABLE_COUNT)
		return -ENOENT;
	table = &state->table[table_id];

	filter_idx = efx_farch_filter_id_index(filter_id);
	if (filter_idx >= table->size)
		return -ENOENT;
	spec = &table->spec[filter_idx];

	spin_lock_bh(&efx->filter_lock);

	if (test_bit(filter_idx, table->used_bitmap) &&
	    spec->priority == priority) {
		efx_farch_filter_table_clear_entry(efx, table, filter_idx);
		rc = 0;
	} else {
		rc = -ENOENT;
	}

	spin_unlock_bh(&efx->filter_lock);

	return rc;
}

/**
 * efx_filter_get_filter_safe - retrieve a filter by ID, carefully
 * @efx: NIC from which to remove the filter
 * @priority: Priority of filter, as passed to @efx_filter_insert_filter
 * @filter_id: ID of filter, as returned by @efx_filter_insert_filter
 * @spec: Buffer in which to store filter specification
 *
 * This function will range-check @filter_id, so it is safe to call
 * with a value passed from userland.
 */
int efx_filter_get_filter_safe(struct efx_nic *efx,
			       enum efx_filter_priority priority,
			       u32 filter_id, struct efx_filter_spec *spec_buf)
{
	struct efx_farch_filter_state *state = efx->filter_state;
	enum efx_farch_filter_table_id table_id;
	struct efx_farch_filter_table *table;
	struct efx_farch_filter_spec *spec;
	unsigned int filter_idx;
	int rc;

	table_id = efx_farch_filter_id_table_id(filter_id);
	if ((unsigned int)table_id >= EFX_FARCH_FILTER_TABLE_COUNT)
		return -ENOENT;
	table = &state->table[table_id];

	filter_idx = efx_farch_filter_id_index(filter_id);
	if (filter_idx >= table->size)
		return -ENOENT;
	spec = &table->spec[filter_idx];

	spin_lock_bh(&efx->filter_lock);

	if (test_bit(filter_idx, table->used_bitmap) &&
	    spec->priority == priority) {
		efx_farch_filter_to_gen_spec(spec_buf, spec);
		rc = 0;
	} else {
		rc = -ENOENT;
	}

	spin_unlock_bh(&efx->filter_lock);

	return rc;
}

static void
efx_farch_filter_table_clear(struct efx_nic *efx,
			     enum efx_farch_filter_table_id table_id,
			     enum efx_filter_priority priority)
{
	struct efx_farch_filter_state *state = efx->filter_state;
	struct efx_farch_filter_table *table = &state->table[table_id];
	unsigned int filter_idx;

	spin_lock_bh(&efx->filter_lock);
	for (filter_idx = 0; filter_idx < table->size; ++filter_idx)
		if (table->spec[filter_idx].priority <= priority)
			efx_farch_filter_table_clear_entry(efx, table,
							   filter_idx);
	spin_unlock_bh(&efx->filter_lock);
}

/**
 * efx_filter_clear_rx - remove RX filters by priority
 * @efx: NIC from which to remove the filters
 * @priority: Maximum priority to remove
 */
void efx_filter_clear_rx(struct efx_nic *efx, enum efx_filter_priority priority)
{
	efx_farch_filter_table_clear(efx, EFX_FARCH_FILTER_TABLE_RX_IP,
				     priority);
	efx_farch_filter_table_clear(efx, EFX_FARCH_FILTER_TABLE_RX_MAC,
				     priority);
}

u32 efx_filter_count_rx_used(struct efx_nic *efx,
			     enum efx_filter_priority priority)
{
	struct efx_farch_filter_state *state = efx->filter_state;
	enum efx_farch_filter_table_id table_id;
	struct efx_farch_filter_table *table;
	unsigned int filter_idx;
	u32 count = 0;

	spin_lock_bh(&efx->filter_lock);

	for (table_id = EFX_FARCH_FILTER_TABLE_RX_IP;
	     table_id <= EFX_FARCH_FILTER_TABLE_RX_DEF;
	     table_id++) {
		table = &state->table[table_id];
		for (filter_idx = 0; filter_idx < table->size; filter_idx++) {
			if (test_bit(filter_idx, table->used_bitmap) &&
			    table->spec[filter_idx].priority == priority)
				++count;
		}
	}

	spin_unlock_bh(&efx->filter_lock);

	return count;
}

s32 efx_filter_get_rx_ids(struct efx_nic *efx,
			  enum efx_filter_priority priority,
			  u32 *buf, u32 size)
{
	struct efx_farch_filter_state *state = efx->filter_state;
	enum efx_farch_filter_table_id table_id;
	struct efx_farch_filter_table *table;
	unsigned int filter_idx;
	s32 count = 0;

	spin_lock_bh(&efx->filter_lock);

	for (table_id = EFX_FARCH_FILTER_TABLE_RX_IP;
	     table_id <= EFX_FARCH_FILTER_TABLE_RX_DEF;
	     table_id++) {
		table = &state->table[table_id];
		for (filter_idx = 0; filter_idx < table->size; filter_idx++) {
			if (test_bit(filter_idx, table->used_bitmap) &&
			    table->spec[filter_idx].priority == priority) {
				if (count == size) {
					count = -EMSGSIZE;
					goto out;
				}
				buf[count++] = efx_farch_filter_make_id(
					&table->spec[filter_idx], filter_idx);
			}
		}
	}
out:
	spin_unlock_bh(&efx->filter_lock);

	return count;
}

/* Restore filter stater after reset */
void efx_restore_filters(struct efx_nic *efx)
{
	struct efx_farch_filter_state *state = efx->filter_state;
	enum efx_farch_filter_table_id table_id;
	struct efx_farch_filter_table *table;
	efx_oword_t filter;
	unsigned int filter_idx;

	spin_lock_bh(&efx->filter_lock);

	for (table_id = 0; table_id < EFX_FARCH_FILTER_TABLE_COUNT; table_id++) {
		table = &state->table[table_id];

		/* Check whether this is a regular register table */
		if (table->step == 0)
			continue;

		for (filter_idx = 0; filter_idx < table->size; filter_idx++) {
			if (!test_bit(filter_idx, table->used_bitmap))
				continue;
			efx_farch_filter_build(&filter, &table->spec[filter_idx]);
			efx_writeo(efx, &filter,
				   table->offset + table->step * filter_idx);
		}
	}

	efx_farch_filter_push_rx_config(efx);
	efx_farch_filter_push_tx_limits(efx);

	spin_unlock_bh(&efx->filter_lock);
}

int efx_probe_filters(struct efx_nic *efx)
{
	struct efx_farch_filter_state *state;
	struct efx_farch_filter_table *table;
	unsigned table_id;

	state = kzalloc(sizeof(struct efx_farch_filter_state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;
	efx->filter_state = state;

	spin_lock_init(&efx->filter_lock);

	if (efx_nic_rev(efx) >= EFX_REV_FALCON_B0) {
#ifdef CONFIG_RFS_ACCEL
		efx->rps_flow_id = kcalloc(FR_BZ_RX_FILTER_TBL0_ROWS,
					   sizeof(*efx->rps_flow_id),
					   GFP_KERNEL);
		if (!efx->rps_flow_id)
			goto fail;
#endif
		table = &state->table[EFX_FARCH_FILTER_TABLE_RX_IP];
		table->id = EFX_FARCH_FILTER_TABLE_RX_IP;
		table->offset = FR_BZ_RX_FILTER_TBL0;
		table->size = FR_BZ_RX_FILTER_TBL0_ROWS;
		table->step = FR_BZ_RX_FILTER_TBL0_STEP;
	}

	if (efx_nic_rev(efx) >= EFX_REV_SIENA_A0) {
		table = &state->table[EFX_FARCH_FILTER_TABLE_RX_MAC];
		table->id = EFX_FARCH_FILTER_TABLE_RX_MAC;
		table->offset = FR_CZ_RX_MAC_FILTER_TBL0;
		table->size = FR_CZ_RX_MAC_FILTER_TBL0_ROWS;
		table->step = FR_CZ_RX_MAC_FILTER_TBL0_STEP;

		table = &state->table[EFX_FARCH_FILTER_TABLE_RX_DEF];
		table->id = EFX_FARCH_FILTER_TABLE_RX_DEF;
		table->size = EFX_FARCH_FILTER_SIZE_RX_DEF;

		table = &state->table[EFX_FARCH_FILTER_TABLE_TX_MAC];
		table->id = EFX_FARCH_FILTER_TABLE_TX_MAC;
		table->offset = FR_CZ_TX_MAC_FILTER_TBL0;
		table->size = FR_CZ_TX_MAC_FILTER_TBL0_ROWS;
		table->step = FR_CZ_TX_MAC_FILTER_TBL0_STEP;
	}

	for (table_id = 0; table_id < EFX_FARCH_FILTER_TABLE_COUNT; table_id++) {
		table = &state->table[table_id];
		if (table->size == 0)
			continue;
		table->used_bitmap = kcalloc(BITS_TO_LONGS(table->size),
					     sizeof(unsigned long),
					     GFP_KERNEL);
		if (!table->used_bitmap)
			goto fail;
		table->spec = vzalloc(table->size * sizeof(*table->spec));
		if (!table->spec)
			goto fail;
	}

	if (state->table[EFX_FARCH_FILTER_TABLE_RX_DEF].size) {
		/* RX default filters must always exist */
		unsigned i;
		for (i = 0; i < EFX_FARCH_FILTER_SIZE_RX_DEF; i++)
			efx_farch_filter_reset_rx_def(efx, i);
	}

	efx_farch_filter_push_rx_config(efx);

	return 0;

fail:
	efx_remove_filters(efx);
	return -ENOMEM;
}

void efx_remove_filters(struct efx_nic *efx)
{
	struct efx_farch_filter_state *state = efx->filter_state;
	enum efx_farch_filter_table_id table_id;

	for (table_id = 0; table_id < EFX_FARCH_FILTER_TABLE_COUNT; table_id++) {
		kfree(state->table[table_id].used_bitmap);
		vfree(state->table[table_id].spec);
	}
#ifdef CONFIG_RFS_ACCEL
	kfree(efx->rps_flow_id);
#endif
	kfree(state);
}

/* Update scatter enable flags for filters pointing to our own RX queues */
void efx_filter_update_rx_scatter(struct efx_nic *efx)
{
	struct efx_farch_filter_state *state = efx->filter_state;
	enum efx_farch_filter_table_id table_id;
	struct efx_farch_filter_table *table;
	efx_oword_t filter;
	unsigned int filter_idx;

	spin_lock_bh(&efx->filter_lock);

	for (table_id = EFX_FARCH_FILTER_TABLE_RX_IP;
	     table_id <= EFX_FARCH_FILTER_TABLE_RX_DEF;
	     table_id++) {
		table = &state->table[table_id];

		for (filter_idx = 0; filter_idx < table->size; filter_idx++) {
			if (!test_bit(filter_idx, table->used_bitmap) ||
			    table->spec[filter_idx].dmaq_id >=
			    efx->n_rx_channels)
				continue;

			if (efx->rx_scatter)
				table->spec[filter_idx].flags |=
					EFX_FILTER_FLAG_RX_SCATTER;
			else
				table->spec[filter_idx].flags &=
					~EFX_FILTER_FLAG_RX_SCATTER;

			if (table_id == EFX_FARCH_FILTER_TABLE_RX_DEF)
				/* Pushed by efx_farch_filter_push_rx_config() */
				continue;

			efx_farch_filter_build(&filter, &table->spec[filter_idx]);
			efx_writeo(efx, &filter,
				   table->offset + table->step * filter_idx);
		}
	}

	efx_farch_filter_push_rx_config(efx);

	spin_unlock_bh(&efx->filter_lock);
}

#ifdef CONFIG_RFS_ACCEL

int efx_filter_rfs(struct net_device *net_dev, const struct sk_buff *skb,
		   u16 rxq_index, u32 flow_id)
{
	struct efx_nic *efx = netdev_priv(net_dev);
	struct efx_channel *channel;
	struct efx_filter_spec spec;
	const struct iphdr *ip;
	const __be16 *ports;
	int nhoff;
	int rc;

	nhoff = skb_network_offset(skb);

	if (skb->protocol == htons(ETH_P_8021Q)) {
		EFX_BUG_ON_PARANOID(skb_headlen(skb) <
				    nhoff + sizeof(struct vlan_hdr));
		if (((const struct vlan_hdr *)skb->data + nhoff)->
		    h_vlan_encapsulated_proto != htons(ETH_P_IP))
			return -EPROTONOSUPPORT;

		/* This is IP over 802.1q VLAN.  We can't filter on the
		 * IP 5-tuple and the vlan together, so just strip the
		 * vlan header and filter on the IP part.
		 */
		nhoff += sizeof(struct vlan_hdr);
	} else if (skb->protocol != htons(ETH_P_IP)) {
		return -EPROTONOSUPPORT;
	}

	/* RFS must validate the IP header length before calling us */
	EFX_BUG_ON_PARANOID(skb_headlen(skb) < nhoff + sizeof(*ip));
	ip = (const struct iphdr *)(skb->data + nhoff);
	if (ip_is_fragment(ip))
		return -EPROTONOSUPPORT;
	EFX_BUG_ON_PARANOID(skb_headlen(skb) < nhoff + 4 * ip->ihl + 4);
	ports = (const __be16 *)(skb->data + nhoff + 4 * ip->ihl);

	efx_filter_init_rx(&spec, EFX_FILTER_PRI_HINT,
			   efx->rx_scatter ? EFX_FILTER_FLAG_RX_SCATTER : 0,
			   rxq_index);
	rc = efx_filter_set_ipv4_full(&spec, ip->protocol,
				      ip->daddr, ports[1], ip->saddr, ports[0]);
	if (rc)
		return rc;

	rc = efx_filter_insert_filter(efx, &spec, true);
	if (rc < 0)
		return rc;

	/* Remember this so we can check whether to expire the filter later */
	efx->rps_flow_id[rc] = flow_id;
	channel = efx_get_channel(efx, skb_get_rx_queue(skb));
	++channel->rfs_filters_added;

	netif_info(efx, rx_status, efx->net_dev,
		   "steering %s %pI4:%u:%pI4:%u to queue %u [flow %u filter %d]\n",
		   (ip->protocol == IPPROTO_TCP) ? "TCP" : "UDP",
		   &ip->saddr, ntohs(ports[0]), &ip->daddr, ntohs(ports[1]),
		   rxq_index, flow_id, rc);

	return rc;
}

bool __efx_filter_rfs_expire(struct efx_nic *efx, unsigned quota)
{
	struct efx_farch_filter_state *state = efx->filter_state;
	struct efx_farch_filter_table *table =
		&state->table[EFX_FARCH_FILTER_TABLE_RX_IP];
	unsigned mask = table->size - 1;
	unsigned index;
	unsigned stop;

	if (!spin_trylock_bh(&efx->filter_lock))
		return false;

	index = efx->rps_expire_index;
	stop = (index + quota) & mask;

	while (index != stop) {
		if (test_bit(index, table->used_bitmap) &&
		    table->spec[index].priority == EFX_FILTER_PRI_HINT &&
		    rps_may_expire_flow(efx->net_dev,
					table->spec[index].dmaq_id,
					efx->rps_flow_id[index], index)) {
			netif_info(efx, rx_status, efx->net_dev,
				   "expiring filter %d [flow %u]\n",
				   index, efx->rps_flow_id[index]);
			efx_farch_filter_table_clear_entry(efx, table, index);
		}
		index = (index + 1) & mask;
	}

	efx->rps_expire_index = stop;

	spin_unlock_bh(&efx->filter_lock);
	return true;
}

#endif /* CONFIG_RFS_ACCEL */
