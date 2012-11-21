/*
 * SAMSUNG EXYNOS5250 Flattened Device Tree enabled machine
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/of_platform.h>
#include <linux/serial_core.h>
#include <linux/io.h>

#include <asm/mach/arch.h>
#include <asm/hardware/gic.h>
#include <mach/map.h>
#include <mach/regs-pmu.h>

#include <plat/cpu.h>
#include <plat/regs-serial.h>

#include "common.h"

/*
 * The following lookup table is used to override device names when devices
 * are registered from device tree. This is temporarily added to enable
 * device tree support addition for the EXYNOS5 architecture.
 *
 * For drivers that require platform data to be provided from the machine
 * file, a platform data pointer can also be supplied along with the
 * devices names. Usually, the platform data elements that cannot be parsed
 * from the device tree by the drivers (example: function pointers) are
 * supplied. But it should be noted that this is a temporary mechanism and
 * at some point, the drivers should be capable of parsing all the platform
 * data from the device tree.
 */
static const struct of_dev_auxdata exynos5250_auxdata_lookup[] __initconst = {
	OF_DEV_AUXDATA("samsung,exynos4210-uart", EXYNOS5_PA_UART0,
				"exynos4210-uart.0", NULL),
	OF_DEV_AUXDATA("samsung,exynos4210-uart", EXYNOS5_PA_UART1,
				"exynos4210-uart.1", NULL),
	OF_DEV_AUXDATA("samsung,exynos4210-uart", EXYNOS5_PA_UART2,
				"exynos4210-uart.2", NULL),
	OF_DEV_AUXDATA("samsung,exynos4210-uart", EXYNOS5_PA_UART3,
				"exynos4210-uart.3", NULL),
	OF_DEV_AUXDATA("samsung,s3c2440-i2c", EXYNOS5_PA_IIC(0),
				"s3c2440-i2c.0", NULL),
	OF_DEV_AUXDATA("samsung,s3c2440-i2c", EXYNOS5_PA_IIC(1),
				"s3c2440-i2c.1", NULL),
	OF_DEV_AUXDATA("samsung,s3c2440-i2c", EXYNOS5_PA_IIC(2),
				"s3c2440-i2c.2", NULL),
	OF_DEV_AUXDATA("samsung,s3c2440-hdmiphy-i2c", EXYNOS5_PA_IIC(8),
				"s3c2440-hdmiphy-i2c", NULL),
	OF_DEV_AUXDATA("samsung,exynos5250-dw-mshc", EXYNOS5_PA_DWMCI0,
				"dw_mmc.0", NULL),
	OF_DEV_AUXDATA("samsung,exynos5250-dw-mshc", EXYNOS5_PA_DWMCI1,
				"dw_mmc.1", NULL),
	OF_DEV_AUXDATA("samsung,exynos5250-dw-mshc", EXYNOS5_PA_DWMCI2,
				"dw_mmc.2", NULL),
	OF_DEV_AUXDATA("samsung,exynos5250-dw-mshc", EXYNOS5_PA_DWMCI3,
				"dw_mmc.3", NULL),
	OF_DEV_AUXDATA("samsung,exynos4210-spi", EXYNOS5_PA_SPI0,
				"exynos4210-spi.0", NULL),
	OF_DEV_AUXDATA("samsung,exynos4210-spi", EXYNOS5_PA_SPI1,
				"exynos4210-spi.1", NULL),
	OF_DEV_AUXDATA("samsung,exynos4210-spi", EXYNOS5_PA_SPI2,
				"exynos4210-spi.2", NULL),
	OF_DEV_AUXDATA("arm,pl330", EXYNOS5_PA_PDMA0, "dma-pl330.0", NULL),
	OF_DEV_AUXDATA("arm,pl330", EXYNOS5_PA_PDMA1, "dma-pl330.1", NULL),
	OF_DEV_AUXDATA("arm,pl330", EXYNOS5_PA_MDMA1, "dma-pl330.2", NULL),
	OF_DEV_AUXDATA("samsung,exynos5-gsc", EXYNOS5_PA_GSC0,
				"exynos-gsc.0", NULL),
	OF_DEV_AUXDATA("samsung,exynos5-gsc", EXYNOS5_PA_GSC1,
				"exynos-gsc.1", NULL),
	OF_DEV_AUXDATA("samsung,exynos5-gsc", EXYNOS5_PA_GSC2,
				"exynos-gsc.2", NULL),
	OF_DEV_AUXDATA("samsung,exynos5-gsc", EXYNOS5_PA_GSC3,
				"exynos-gsc.3", NULL),
	OF_DEV_AUXDATA("samsung,exynos5-hdmi", 0x14530000,
				"exynos5-hdmi", NULL),
	OF_DEV_AUXDATA("samsung,exynos5-mixer", 0x14450000,
				"exynos5-mixer", NULL),
	{},
};

static void __init exynos5250_dt_map_io(void)
{
	exynos_init_io(NULL, 0);
	s3c24xx_init_clocks(24000000);
}

static void __init exynos5250_dt_machine_init(void)
{
	struct device_node *i2c_np;
	const char *i2c_compat = "samsung,s3c2440-i2c";
	unsigned int tmp;

	/*
	 * Exynos5's legacy i2c controller and new high speed i2c
	 * controller have muxed interrupt sources. By default the
	 * interrupts for 4-channel HS-I2C controller are enabled.
	 * If node for first four channels of legacy i2c controller
	 * are available then re-configure the interrupts via the
	 * system register.
	 */
	for_each_compatible_node(i2c_np, NULL, i2c_compat) {
		if (of_device_is_available(i2c_np)) {
			if (of_alias_get_id(i2c_np, "i2c") < 4) {
				tmp = readl(EXYNOS5_SYS_I2C_CFG);
				writel(tmp & ~(0x1 << of_alias_get_id(i2c_np, "i2c")),
						EXYNOS5_SYS_I2C_CFG);
			}
		}
	}

	of_platform_populate(NULL, of_default_bus_match_table,
				exynos5250_auxdata_lookup, NULL);
}

static char const *exynos5250_dt_compat[] __initdata = {
	"samsung,exynos5250",
	NULL
};

DT_MACHINE_START(EXYNOS5_DT, "SAMSUNG EXYNOS5 (Flattened Device Tree)")
	/* Maintainer: Kukjin Kim <kgene.kim@samsung.com> */
	.init_irq	= exynos5_init_irq,
	.smp		= smp_ops(exynos_smp_ops),
	.map_io		= exynos5250_dt_map_io,
	.handle_irq	= gic_handle_irq,
	.init_machine	= exynos5250_dt_machine_init,
	.init_late	= exynos_init_late,
	.timer		= &exynos4_timer,
	.dt_compat	= exynos5250_dt_compat,
	.restart        = exynos5_restart,
MACHINE_END
