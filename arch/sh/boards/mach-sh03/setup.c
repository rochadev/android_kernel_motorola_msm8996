/*
 * linux/arch/sh/boards/sh03/setup.c
 *
 * Copyright (C) 2004  Interface Co.,Ltd. Saito.K
 *
 */

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/ata_platform.h>
#include <asm/io.h>
#include <asm/rtc.h>
#include <mach-sh03/mach/io.h>
#include <mach-sh03/mach/sh03.h>
#include <asm/addrspace.h>

static void __init init_sh03_IRQ(void)
{
	plat_irq_setup_pins(IRQ_MODE_IRQ);
}

/* arch/sh/boards/sh03/rtc.c */
void sh03_time_init(void);

static void __init sh03_setup(char **cmdline_p)
{
	board_time_init = sh03_time_init;
}

static struct resource cf_ide_resources[3];

static struct platform_device cf_ide_device = {
	.name		= "pata_platform",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(cf_ide_resources),
	.resource	= cf_ide_resources,
};

static struct resource heartbeat_resources[] = {
	[0] = {
		.start	= 0xa0800000,
		.end	= 0xa0800000,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device heartbeat_device = {
	.name		= "heartbeat",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(heartbeat_resources),
	.resource	= heartbeat_resources,
};

static struct platform_device *sh03_devices[] __initdata = {
	&heartbeat_device,
	&cf_ide_device,
};

static int __init sh03_devices_setup(void)
{
	pgprot_t prot;
	unsigned long paddrbase;
	void *cf_ide_base;

	/* open I/O area window */
	paddrbase = virt_to_phys((void *)PA_AREA5_IO);
	prot = PAGE_KERNEL_PCC(1, _PAGE_PCC_IO16);
	cf_ide_base = p3_ioremap(paddrbase, PAGE_SIZE, prot.pgprot);
	if (!cf_ide_base) {
		printk("allocate_cf_area : can't open CF I/O window!\n");
		return -ENOMEM;
	}

	/* IDE cmd address : 0x1f0-0x1f7 and 0x3f6 */
	cf_ide_resources[0].start = (unsigned long)cf_ide_base + 0x40;
	cf_ide_resources[0].end   = (unsigned long)cf_ide_base + 0x40 + 0x0f;
	cf_ide_resources[0].flags = IORESOURCE_IO;
	cf_ide_resources[1].start = (unsigned long)cf_ide_base + 0x2c;
	cf_ide_resources[1].end   = (unsigned long)cf_ide_base + 0x2c + 0x03;
	cf_ide_resources[1].flags = IORESOURCE_IO;
	cf_ide_resources[2].start = IRQ_FATA;
	cf_ide_resources[2].flags = IORESOURCE_IRQ;

	return platform_add_devices(sh03_devices, ARRAY_SIZE(sh03_devices));
}
__initcall(sh03_devices_setup);

static struct sh_machine_vector mv_sh03 __initmv = {
	.mv_name		= "Interface (CTP/PCI-SH03)",
	.mv_setup		= sh03_setup,
	.mv_nr_irqs		= 48,
	.mv_init_irq		= init_sh03_IRQ,
};
