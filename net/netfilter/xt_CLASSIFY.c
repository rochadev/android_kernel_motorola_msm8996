/*
 * This is a module which is used for setting the skb->priority field
 * of an skb for qdisc classification.
 */

/* (C) 2001-2002 Patrick McHardy <kaber@trash.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <net/checksum.h>

#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_CLASSIFY.h>

MODULE_AUTHOR("Patrick McHardy <kaber@trash.net>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("iptables qdisc classification target module");
MODULE_ALIAS("ipt_CLASSIFY");
MODULE_ALIAS("ip6t_CLASSIFY");

static unsigned int
classify_tg(struct sk_buff *skb, const struct net_device *in,
            const struct net_device *out, unsigned int hooknum,
            const struct xt_target *target, const void *targinfo)
{
	const struct xt_classify_target_info *clinfo = targinfo;

	skb->priority = clinfo->priority;
	return XT_CONTINUE;
}

static struct xt_target classify_tg_reg[] __read_mostly = {
	{
		.family		= AF_INET,
		.name 		= "CLASSIFY",
		.target 	= classify_tg,
		.targetsize	= sizeof(struct xt_classify_target_info),
		.table		= "mangle",
		.hooks		= (1 << NF_INET_LOCAL_OUT) |
				  (1 << NF_INET_FORWARD) |
				  (1 << NF_INET_POST_ROUTING),
		.me 		= THIS_MODULE,
	},
	{
		.name 		= "CLASSIFY",
		.family		= AF_INET6,
		.target 	= classify_tg,
		.targetsize	= sizeof(struct xt_classify_target_info),
		.table		= "mangle",
		.hooks		= (1 << NF_INET_LOCAL_OUT) |
				  (1 << NF_INET_FORWARD) |
				  (1 << NF_INET_POST_ROUTING),
		.me 		= THIS_MODULE,
	},
};

static int __init classify_tg_init(void)
{
	return xt_register_targets(classify_tg_reg,
	       ARRAY_SIZE(classify_tg_reg));
}

static void __exit classify_tg_exit(void)
{
	xt_unregister_targets(classify_tg_reg, ARRAY_SIZE(classify_tg_reg));
}

module_init(classify_tg_init);
module_exit(classify_tg_exit);
