/*
 * Copyright 2013 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Ben Skeggs
 */

#include <core/client.h>
#include <core/option.h>
#include <nvif/unpack.h>
#include <nvif/class.h>
#include <nvif/ioctl.h>

#include <subdev/clock.h>

#include "priv.h"

#define QUAD_MASK 0x0f
#define QUAD_FREE 0x01

static struct nouveau_perfsig *
nouveau_perfsig_find_(struct nouveau_perfdom *dom, const char *name, u32 size)
{
	char path[64];
	int i;

	if (name[0] != '/') {
		for (i = 0; i < dom->signal_nr; i++) {
			if ( dom->signal[i].name &&
			    !strncmp(name, dom->signal[i].name, size))
				return &dom->signal[i];
		}
	} else {
		for (i = 0; i < dom->signal_nr; i++) {
			snprintf(path, sizeof(path), "/%s/%02x", dom->name, i);
			if (!strncmp(name, path, size))
				return &dom->signal[i];
		}
	}

	return NULL;
}

struct nouveau_perfsig *
nouveau_perfsig_find(struct nouveau_perfmon *ppm, const char *name, u32 size,
		     struct nouveau_perfdom **pdom)
{
	struct nouveau_perfdom *dom = *pdom;
	struct nouveau_perfsig *sig;

	if (dom == NULL) {
		list_for_each_entry(dom, &ppm->domains, head) {
			sig = nouveau_perfsig_find_(dom, name, size);
			if (sig) {
				*pdom = dom;
				return sig;
			}
		}

		return NULL;
	}

	return nouveau_perfsig_find_(dom, name, size);
}

struct nouveau_perfctr *
nouveau_perfsig_wrap(struct nouveau_perfmon *ppm, const char *name,
		     struct nouveau_perfdom **pdom)
{
	struct nouveau_perfsig *sig;
	struct nouveau_perfctr *ctr;

	sig = nouveau_perfsig_find(ppm, name, strlen(name), pdom);
	if (!sig)
		return NULL;

	ctr = kzalloc(sizeof(*ctr), GFP_KERNEL);
	if (ctr) {
		ctr->signal[0] = sig;
		ctr->logic_op = 0xaaaa;
	}

	return ctr;
}

/*******************************************************************************
 * Perfmon object classes
 ******************************************************************************/
static int
nouveau_perfctr_query(struct nouveau_object *object, void *data, u32 size)
{
	union {
		struct nvif_perfctr_query_v0 v0;
	} *args = data;
	struct nouveau_device *device = nv_device(object);
	struct nouveau_perfmon *ppm = (void *)object->engine;
	struct nouveau_perfdom *dom = NULL, *chk;
	const bool all = nouveau_boolopt(device->cfgopt, "NvPmShowAll", false);
	const bool raw = nouveau_boolopt(device->cfgopt, "NvPmUnnamed", all);
	const char *name;
	int tmp = 0, di, si;
	int ret;

	nv_ioctl(object, "perfctr query size %d\n", size);
	if (nvif_unpack(args->v0, 0, 0, false)) {
		nv_ioctl(object, "perfctr query vers %d iter %08x\n",
			 args->v0.version, args->v0.iter);
		di = (args->v0.iter & 0xff000000) >> 24;
		si = (args->v0.iter & 0x00ffffff) - 1;
	} else
		return ret;

	list_for_each_entry(chk, &ppm->domains, head) {
		if (tmp++ == di) {
			dom = chk;
			break;
		}
	}

	if (dom == NULL || si >= (int)dom->signal_nr)
		return -EINVAL;

	if (si >= 0) {
		if (raw || !(name = dom->signal[si].name)) {
			snprintf(args->v0.name, sizeof(args->v0.name),
				 "/%s/%02x", dom->name, si);
		} else {
			strncpy(args->v0.name, name, sizeof(args->v0.name));
		}
	}

	do {
		while (++si < dom->signal_nr) {
			if (all || dom->signal[si].name) {
				args->v0.iter = (di << 24) | ++si;
				return 0;
			}
		}
		si = -1;
		di = di + 1;
		dom = list_entry(dom->head.next, typeof(*dom), head);
	} while (&dom->head != &ppm->domains);

	args->v0.iter = 0xffffffff;
	return 0;
}

static int
nouveau_perfctr_sample(struct nouveau_object *object, void *data, u32 size)
{
	union {
		struct nvif_perfctr_sample none;
	} *args = data;
	struct nouveau_perfmon *ppm = (void *)object->engine;
	struct nouveau_perfctr *ctr, *tmp;
	struct nouveau_perfdom *dom;
	int ret;

	nv_ioctl(object, "perfctr sample size %d\n", size);
	if (nvif_unvers(args->none)) {
		nv_ioctl(object, "perfctr sample\n");
	} else
		return ret;
	ppm->sequence++;

	list_for_each_entry(dom, &ppm->domains, head) {
		/* sample previous batch of counters */
		if (dom->quad != QUAD_MASK) {
			dom->func->next(ppm, dom);
			tmp = NULL;
			while (!list_empty(&dom->list)) {
				ctr = list_first_entry(&dom->list,
							typeof(*ctr), head);
				if (ctr->slot < 0) break;
				if ( tmp && tmp == ctr) break;
				if (!tmp) tmp = ctr;
				dom->func->read(ppm, dom, ctr);
				ctr->slot  = -1;
				list_move_tail(&ctr->head, &dom->list);
			}
		}

		dom->quad = QUAD_MASK;

		/* setup next batch of counters for sampling */
		list_for_each_entry(ctr, &dom->list, head) {
			ctr->slot = ffs(dom->quad) - 1;
			if (ctr->slot < 0)
				break;
			dom->quad &= ~(QUAD_FREE << ctr->slot);
			dom->func->init(ppm, dom, ctr);
		}

		if (dom->quad != QUAD_MASK)
			dom->func->next(ppm, dom);
	}

	return 0;
}

static int
nouveau_perfctr_read(struct nouveau_object *object, void *data, u32 size)
{
	union {
		struct nvif_perfctr_read_v0 v0;
	} *args = data;
	struct nouveau_perfctr *ctr = (void *)object;
	int ret;

	nv_ioctl(object, "perfctr read size %d\n", size);
	if (nvif_unpack(args->v0, 0, 0, false)) {
		nv_ioctl(object, "perfctr read vers %d\n", args->v0.version);
	} else
		return ret;

	if (!ctr->clk)
		return -EAGAIN;

	args->v0.clk = ctr->clk;
	args->v0.ctr = ctr->ctr;
	return 0;
}

static int
nouveau_perfctr_mthd(struct nouveau_object *object, u32 mthd,
		     void *data, u32 size)
{
	switch (mthd) {
	case NVIF_PERFCTR_V0_QUERY:
		return nouveau_perfctr_query(object, data, size);
	case NVIF_PERFCTR_V0_SAMPLE:
		return nouveau_perfctr_sample(object, data, size);
	case NVIF_PERFCTR_V0_READ:
		return nouveau_perfctr_read(object, data, size);
	default:
		break;
	}
	return -EINVAL;
}

static void
nouveau_perfctr_dtor(struct nouveau_object *object)
{
	struct nouveau_perfctr *ctr = (void *)object;
	if (ctr->head.next)
		list_del(&ctr->head);
	nouveau_object_destroy(&ctr->base);
}

static int
nouveau_perfctr_ctor(struct nouveau_object *parent,
		     struct nouveau_object *engine,
		     struct nouveau_oclass *oclass, void *data, u32 size,
		     struct nouveau_object **pobject)
{
	union {
		struct nvif_perfctr_v0 v0;
	} *args = data;
	struct nouveau_perfmon *ppm = (void *)engine;
	struct nouveau_perfdom *dom = NULL;
	struct nouveau_perfsig *sig[4] = {};
	struct nouveau_perfctr *ctr;
	int ret, i;

	nv_ioctl(parent, "create perfctr size %d\n", size);
	if (nvif_unpack(args->v0, 0, 0, false)) {
		nv_ioctl(parent, "create perfctr vers %d logic_op %04x\n",
			 args->v0.version, args->v0.logic_op);
	} else
		return ret;

	for (i = 0; i < ARRAY_SIZE(args->v0.name) && args->v0.name[i][0]; i++) {
		sig[i] = nouveau_perfsig_find(ppm, args->v0.name[i],
					      strnlen(args->v0.name[i],
					      sizeof(args->v0.name[i])),
					      &dom);
		if (!sig[i])
			return -EINVAL;
	}

	ret = nouveau_object_create(parent, engine, oclass, 0, &ctr);
	*pobject = nv_object(ctr);
	if (ret)
		return ret;

	ctr->slot = -1;
	ctr->logic_op = args->v0.logic_op;
	ctr->signal[0] = sig[0];
	ctr->signal[1] = sig[1];
	ctr->signal[2] = sig[2];
	ctr->signal[3] = sig[3];
	if (dom)
		list_add_tail(&ctr->head, &dom->list);
	return 0;
}

static struct nouveau_ofuncs
nouveau_perfctr_ofuncs = {
	.ctor = nouveau_perfctr_ctor,
	.dtor = nouveau_perfctr_dtor,
	.init = nouveau_object_init,
	.fini = nouveau_object_fini,
	.mthd = nouveau_perfctr_mthd,
};

struct nouveau_oclass
nouveau_perfmon_sclass[] = {
	{ .handle = NVIF_IOCTL_NEW_V0_PERFCTR,
	  .ofuncs = &nouveau_perfctr_ofuncs,
	},
	{},
};

/*******************************************************************************
 * PPM context
 ******************************************************************************/
static void
nouveau_perfctx_dtor(struct nouveau_object *object)
{
	struct nouveau_perfmon *ppm = (void *)object->engine;
	mutex_lock(&nv_subdev(ppm)->mutex);
	nouveau_engctx_destroy(&ppm->context->base);
	ppm->context = NULL;
	mutex_unlock(&nv_subdev(ppm)->mutex);
}

static int
nouveau_perfctx_ctor(struct nouveau_object *parent,
		     struct nouveau_object *engine,
		     struct nouveau_oclass *oclass, void *data, u32 size,
		     struct nouveau_object **pobject)
{
	struct nouveau_perfmon *ppm = (void *)engine;
	struct nouveau_perfctx *ctx;
	int ret;

	ret = nouveau_engctx_create(parent, engine, oclass, NULL,
				    0, 0, 0, &ctx);
	*pobject = nv_object(ctx);
	if (ret)
		return ret;

	mutex_lock(&nv_subdev(ppm)->mutex);
	if (ppm->context == NULL)
		ppm->context = ctx;
	mutex_unlock(&nv_subdev(ppm)->mutex);

	if (ctx != ppm->context)
		return -EBUSY;

	return 0;
}

struct nouveau_oclass
nouveau_perfmon_cclass = {
	.handle = NV_ENGCTX(PERFMON, 0x00),
	.ofuncs = &(struct nouveau_ofuncs) {
		.ctor = nouveau_perfctx_ctor,
		.dtor = nouveau_perfctx_dtor,
		.init = _nouveau_engctx_init,
		.fini = _nouveau_engctx_fini,
	},
};

/*******************************************************************************
 * PPM engine/subdev functions
 ******************************************************************************/
int
nouveau_perfdom_new(struct nouveau_perfmon *ppm, const char *name, u32 mask,
		    u32 base, u32 size_unit, u32 size_domain,
		    const struct nouveau_specdom *spec)
{
	const struct nouveau_specdom *sdom;
	const struct nouveau_specsig *ssig;
	struct nouveau_perfdom *dom;
	int i;

	for (i = 0; i == 0 || mask; i++) {
		u32 addr = base + (i * size_unit);
		if (i && !(mask & (1 << i)))
			continue;

		sdom = spec;
		while (sdom->signal_nr) {
			dom = kzalloc(sizeof(*dom) + sdom->signal_nr *
				      sizeof(*dom->signal), GFP_KERNEL);
			if (!dom)
				return -ENOMEM;

			if (mask) {
				snprintf(dom->name, sizeof(dom->name),
					 "%s/%02x/%02x", name, i,
					 (int)(sdom - spec));
			} else {
				snprintf(dom->name, sizeof(dom->name),
					 "%s/%02x", name, (int)(sdom - spec));
			}

			list_add_tail(&dom->head, &ppm->domains);
			INIT_LIST_HEAD(&dom->list);
			dom->func = sdom->func;
			dom->addr = addr;
			dom->quad = QUAD_MASK;
			dom->signal_nr = sdom->signal_nr;

			ssig = (sdom++)->signal;
			while (ssig->name) {
				dom->signal[ssig->signal].name = ssig->name;
				ssig++;
			}

			addr += size_domain;
		}

		mask &= ~(1 << i);
	}

	return 0;
}

int
_nouveau_perfmon_fini(struct nouveau_object *object, bool suspend)
{
	struct nouveau_perfmon *ppm = (void *)object;
	return nouveau_engine_fini(&ppm->base, suspend);
}

int
_nouveau_perfmon_init(struct nouveau_object *object)
{
	struct nouveau_perfmon *ppm = (void *)object;
	return nouveau_engine_init(&ppm->base);
}

void
_nouveau_perfmon_dtor(struct nouveau_object *object)
{
	struct nouveau_perfmon *ppm = (void *)object;
	struct nouveau_perfdom *dom, *tmp;

	list_for_each_entry_safe(dom, tmp, &ppm->domains, head) {
		list_del(&dom->head);
		kfree(dom);
	}

	nouveau_engine_destroy(&ppm->base);
}

int
nouveau_perfmon_create_(struct nouveau_object *parent,
			struct nouveau_object *engine,
			struct nouveau_oclass *oclass,
			int length, void **pobject)
{
	struct nouveau_perfmon *ppm;
	int ret;

	ret = nouveau_engine_create_(parent, engine, oclass, true, "PPM",
				     "perfmon", length, pobject);
	ppm = *pobject;
	if (ret)
		return ret;

	INIT_LIST_HEAD(&ppm->domains);
	return 0;
}
