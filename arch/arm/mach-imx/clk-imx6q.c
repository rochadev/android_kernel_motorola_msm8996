/*
 * Copyright 2011 Freescale Semiconductor, Inc.
 * Copyright 2011 Linaro Ltd.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/init.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <mach/common.h>
#include "clk.h"

#define CCGR0				0x68
#define CCGR1				0x6c
#define CCGR2				0x70
#define CCGR3				0x74
#define CCGR4				0x78
#define CCGR5				0x7c
#define CCGR6				0x80
#define CCGR7				0x84

#define CLPCR				0x54
#define BP_CLPCR_LPM			0
#define BM_CLPCR_LPM			(0x3 << 0)
#define BM_CLPCR_BYPASS_PMIC_READY	(0x1 << 2)
#define BM_CLPCR_ARM_CLK_DIS_ON_LPM	(0x1 << 5)
#define BM_CLPCR_SBYOS			(0x1 << 6)
#define BM_CLPCR_DIS_REF_OSC		(0x1 << 7)
#define BM_CLPCR_VSTBY			(0x1 << 8)
#define BP_CLPCR_STBY_COUNT		9
#define BM_CLPCR_STBY_COUNT		(0x3 << 9)
#define BM_CLPCR_COSC_PWRDOWN		(0x1 << 11)
#define BM_CLPCR_WB_PER_AT_LPM		(0x1 << 16)
#define BM_CLPCR_WB_CORE_AT_LPM		(0x1 << 17)
#define BM_CLPCR_BYP_MMDC_CH0_LPM_HS	(0x1 << 19)
#define BM_CLPCR_BYP_MMDC_CH1_LPM_HS	(0x1 << 21)
#define BM_CLPCR_MASK_CORE0_WFI		(0x1 << 22)
#define BM_CLPCR_MASK_CORE1_WFI		(0x1 << 23)
#define BM_CLPCR_MASK_CORE2_WFI		(0x1 << 24)
#define BM_CLPCR_MASK_CORE3_WFI		(0x1 << 25)
#define BM_CLPCR_MASK_SCU_IDLE		(0x1 << 26)
#define BM_CLPCR_MASK_L2CC_IDLE		(0x1 << 27)

static void __iomem *ccm_base;

void __init imx6q_clock_map_io(void) { }

int imx6q_set_lpm(enum mxc_cpu_pwr_mode mode)
{
	u32 val = readl_relaxed(ccm_base + CLPCR);

	val &= ~BM_CLPCR_LPM;
	switch (mode) {
	case WAIT_CLOCKED:
		break;
	case WAIT_UNCLOCKED:
		val |= 0x1 << BP_CLPCR_LPM;
		break;
	case STOP_POWER_ON:
		val |= 0x2 << BP_CLPCR_LPM;
		break;
	case WAIT_UNCLOCKED_POWER_OFF:
		val |= 0x1 << BP_CLPCR_LPM;
		val &= ~BM_CLPCR_VSTBY;
		val &= ~BM_CLPCR_SBYOS;
		break;
	case STOP_POWER_OFF:
		val |= 0x2 << BP_CLPCR_LPM;
		val |= 0x3 << BP_CLPCR_STBY_COUNT;
		val |= BM_CLPCR_VSTBY;
		val |= BM_CLPCR_SBYOS;
		break;
	default:
		return -EINVAL;
	}

	writel_relaxed(val, ccm_base + CLPCR);

	return 0;
}

static const char *step_sels[]	= { "osc", "pll2_pfd2_396m", };
static const char *pll1_sw_sels[]	= { "pll1_sys", "step", };
static const char *periph_pre_sels[]	= { "pll2_bus", "pll2_pfd2_396m", "pll2_pfd0_352m", "pll2_198m", };
static const char *periph_clk2_sels[]	= { "pll3_usb_otg", "osc", };
static const char *periph_sels[]	= { "periph_pre", "periph_clk2", };
static const char *periph2_sels[]	= { "periph2_pre", "periph2_clk2", };
static const char *axi_sels[]		= { "periph", "pll2_pfd2_396m", "pll3_pfd1_540m", };
static const char *audio_sels[]	= { "pll4_audio", "pll3_pfd2_508m", "pll3_pfd3_454m", "pll3_usb_otg", };
static const char *gpu_axi_sels[]	= { "axi", "ahb", };
static const char *gpu2d_core_sels[]	= { "axi", "pll3_usb_otg", "pll2_pfd0_352m", "pll2_pfd2_396m", };
static const char *gpu3d_core_sels[]	= { "mmdc_ch0_axi", "pll3_usb_otg", "pll2_pfd1_594m", "pll2_pfd2_396m", };
static const char *gpu3d_shader_sels[] = { "mmdc_ch0_axi", "pll3_usb_otg", "pll2_pfd1_594m", "pll2_pfd9_720m", };
static const char *ipu_sels[]		= { "mmdc_ch0_axi", "pll2_pfd2_396m", "pll3_120m", "pll3_pfd1_540m", };
static const char *ldb_di_sels[]	= { "pll5_video", "pll2_pfd0_352m", "pll2_pfd2_396m", "pll3_pfd1_540m", };
static const char *ipu_di_pre_sels[]	= { "mmdc_ch0_axi", "pll3_usb_otg", "pll5_video", "pll2_pfd0_352m", "pll2_pfd2_396m", "pll3_pfd1_540m", };
static const char *ipu1_di0_sels[]	= { "ipu1_di0_pre", "dummy", "dummy", "ldb_di0", "ldb_di1", };
static const char *ipu1_di1_sels[]	= { "ipu1_di1_pre", "dummy", "dummy", "ldb_di0", "ldb_di1", };
static const char *ipu2_di0_sels[]	= { "ipu2_di0_pre", "dummy", "dummy", "ldb_di0", "ldb_di1", };
static const char *ipu2_di1_sels[]	= { "ipu2_di1_pre", "dummy", "dummy", "ldb_di0", "ldb_di1", };
static const char *hsi_tx_sels[]	= { "pll3_120m", "pll2_pfd2_396m", };
static const char *pcie_axi_sels[]	= { "axi", "ahb", };
static const char *ssi_sels[]		= { "pll3_pfd2_508m", "pll3_pfd3_454m", "pll4_audio", };
static const char *usdhc_sels[]	= { "pll2_pfd2_396m", "pll2_pfd0_352m", };
static const char *enfc_sels[]	= { "pll2_pfd0_352m", "pll2_bus", "pll3_usb_otg", "pll2_pfd2_396m", };
static const char *emi_sels[]		= { "axi", "pll3_usb_otg", "pll2_pfd2_396m", "pll2_pfd0_352m", };
static const char *vdo_axi_sels[]	= { "axi", "ahb", };
static const char *vpu_axi_sels[]	= { "axi", "pll2_pfd2_396m", "pll2_pfd0_352m", };
static const char *cko1_sels[]	= { "pll3_usb_otg", "pll2_bus", "pll1_sys", "pll5_video",
				    "dummy", "axi", "enfc", "ipu1_di0", "ipu1_di1", "ipu2_di0",
				    "ipu2_di1", "ahb", "ipg", "ipg_per", "ckil", "pll4_audio", };

static const char * const clks_init_on[] __initconst = {
	"mmdc_ch0_axi", "mmdc_ch1_axi", "usboh3",
};

enum mx6q_clks {
	dummy, ckil, ckih, osc, pll2_pfd0_352m, pll2_pfd1_594m, pll2_pfd2_396m,
	pll3_pfd0_720m, pll3_pfd1_540m, pll3_pfd2_508m, pll3_pfd3_454m,
	pll2_198m, pll3_120m, pll3_80m, pll3_60m, twd, step, pll1_sw,
	periph_pre, periph2_pre, periph_clk2_sel, periph2_clk2_sel, axi_sel,
	esai_sel, asrc_sel, spdif_sel, gpu2d_axi, gpu3d_axi, gpu2d_core_sel,
	gpu3d_core_sel, gpu3d_shader_sel, ipu1_sel, ipu2_sel, ldb_di0_sel,
	ldb_di1_sel, ipu1_di0_pre_sel, ipu1_di1_pre_sel, ipu2_di0_pre_sel,
	ipu2_di1_pre_sel, ipu1_di0_sel, ipu1_di1_sel, ipu2_di0_sel,
	ipu2_di1_sel, hsi_tx_sel, pcie_axi_sel, ssi1_sel, ssi2_sel, ssi3_sel,
	usdhc1_sel, usdhc2_sel, usdhc3_sel, usdhc4_sel, enfc_sel, emi_sel,
	emi_slow_sel, vdo_axi_sel, vpu_axi_sel, cko1_sel, periph, periph2,
	periph_clk2, periph2_clk2, ipg, ipg_per, esai_pred, esai_podf,
	asrc_pred, asrc_podf, spdif_pred, spdif_podf, can_root, ecspi_root,
	gpu2d_core_podf, gpu3d_core_podf, gpu3d_shader, ipu1_podf, ipu2_podf,
	ldb_di0_podf, ldb_di1_podf, ipu1_di0_pre, ipu1_di1_pre, ipu2_di0_pre,
	ipu2_di1_pre, hsi_tx_podf, ssi1_pred, ssi1_podf, ssi2_pred, ssi2_podf,
	ssi3_pred, ssi3_podf, uart_serial_podf, usdhc1_podf, usdhc2_podf,
	usdhc3_podf, usdhc4_podf, enfc_pred, enfc_podf, emi_podf,
	emi_slow_podf, vpu_axi_podf, cko1_podf, axi, mmdc_ch0_axi_podf,
	mmdc_ch1_axi_podf, arm, ahb, apbh_dma, asrc, can1_ipg, can1_serial,
	can2_ipg, can2_serial, ecspi1, ecspi2, ecspi3, ecspi4, ecspi5, enet,
	esai, gpt_ipg, gpt_ipg_per, gpu2d_core, gpu3d_core, hdmi_iahb,
	hdmi_isfr, i2c1, i2c2, i2c3, iim, enfc, ipu1, ipu1_di0, ipu1_di1, ipu2,
	ipu2_di0, ldb_di0, ldb_di1, ipu2_di1, hsi_tx, mlb, mmdc_ch0_axi,
	mmdc_ch1_axi, ocram, openvg_axi, pcie_axi, pwm1, pwm2, pwm3, pwm4,
	gpmi_bch_apb, gpmi_bch, gpmi_io, gpmi_apb, sata, sdma, spba, ssi1,
	ssi2, ssi3, uart_ipg, uart_serial, usboh3, usdhc1, usdhc2, usdhc3,
	usdhc4, vdo_axi, vpu_axi, cko1, pll1_sys, pll2_bus, pll3_usb_otg,
	pll4_audio, pll5_video, pll6_mlb, pll7_usb_host, pll8_enet, clk_max
};

static struct clk *clk[clk_max];

int __init mx6q_clocks_init(void)
{
	struct device_node *np;
	void __iomem *base;
	struct clk *c;
	int i, irq;

	clk[dummy] = imx_clk_fixed("dummy", 0);

	/* retrieve the freqency of fixed clocks from device tree */
	for_each_compatible_node(np, NULL, "fixed-clock") {
		u32 rate;
		if (of_property_read_u32(np, "clock-frequency", &rate))
			continue;

		if (of_device_is_compatible(np, "fsl,imx-ckil"))
			clk[ckil] = imx_clk_fixed("ckil", rate);
		else if (of_device_is_compatible(np, "fsl,imx-ckih1"))
			clk[ckih] = imx_clk_fixed("ckih", rate);
		else if (of_device_is_compatible(np, "fsl,imx-osc"))
			clk[osc] = imx_clk_fixed("osc", rate);
	}

	np = of_find_compatible_node(NULL, NULL, "fsl,imx6q-anatop");
	base = of_iomap(np, 0);
	WARN_ON(!base);

	/*                   type                               name         parent_name  base     gate_mask div_mask */
	clk[pll1_sys]      = imx_clk_pllv3(IMX_PLLV3_SYS,	"pll1_sys",	"osc", base,        0x2000,   0x7f);
	clk[pll2_bus]      = imx_clk_pllv3(IMX_PLLV3_GENERIC,	"pll2_bus",	"osc", base + 0x30, 0x2000,   0x1);
	clk[pll3_usb_otg]  = imx_clk_pllv3(IMX_PLLV3_USB,	"pll3_usb_otg",	"osc", base + 0x10, 0x2000,   0x3);
	clk[pll4_audio]    = imx_clk_pllv3(IMX_PLLV3_AV,	"pll4_audio",	"osc", base + 0x70, 0x2000,   0x7f);
	clk[pll5_video]    = imx_clk_pllv3(IMX_PLLV3_AV,	"pll5_video",	"osc", base + 0xa0, 0x2000,   0x7f);
	clk[pll6_mlb]      = imx_clk_pllv3(IMX_PLLV3_MLB,	"pll6_mlb",	"osc", base + 0xd0, 0x2000,   0x0);
	clk[pll7_usb_host] = imx_clk_pllv3(IMX_PLLV3_USB,	"pll7_usb_host","osc", base + 0x20, 0x2000,   0x3);
	clk[pll8_enet]     = imx_clk_pllv3(IMX_PLLV3_ENET,	"pll8_enet",	"osc", base + 0xe0, 0x182000, 0x3);

	/*                                name              parent_name        reg       idx */
	clk[pll2_pfd0_352m] = imx_clk_pfd("pll2_pfd0_352m", "pll2_bus",     base + 0x100, 0);
	clk[pll2_pfd1_594m] = imx_clk_pfd("pll2_pfd1_594m", "pll2_bus",     base + 0x100, 1);
	clk[pll2_pfd2_396m] = imx_clk_pfd("pll2_pfd2_396m", "pll2_bus",     base + 0x100, 2);
	clk[pll3_pfd0_720m] = imx_clk_pfd("pll3_pfd0_720m", "pll3_usb_otg", base + 0xf0,  0);
	clk[pll3_pfd1_540m] = imx_clk_pfd("pll3_pfd1_540m", "pll3_usb_otg", base + 0xf0,  1);
	clk[pll3_pfd2_508m] = imx_clk_pfd("pll3_pfd2_508m", "pll3_usb_otg", base + 0xf0,  2);
	clk[pll3_pfd3_454m] = imx_clk_pfd("pll3_pfd3_454m", "pll3_usb_otg", base + 0xf0,  3);

	/*                                    name         parent_name     mult div */
	clk[pll2_198m] = imx_clk_fixed_factor("pll2_198m", "pll2_pfd2_396m", 1, 2);
	clk[pll3_120m] = imx_clk_fixed_factor("pll3_120m", "pll3_usb_otg",   1, 4);
	clk[pll3_80m]  = imx_clk_fixed_factor("pll3_80m",  "pll3_usb_otg",   1, 6);
	clk[pll3_60m]  = imx_clk_fixed_factor("pll3_60m",  "pll3_usb_otg",   1, 8);
	clk[twd]       = imx_clk_fixed_factor("twd",       "arm",            1, 2);

	np = of_find_compatible_node(NULL, NULL, "fsl,imx6q-ccm");
	base = of_iomap(np, 0);
	WARN_ON(!base);
	ccm_base = base;

	/*                                  name                reg       shift width parent_names     num_parents */
	clk[step]             = imx_clk_mux("step",	        base + 0xc,  8,  1, step_sels,	       ARRAY_SIZE(step_sels));
	clk[pll1_sw]          = imx_clk_mux("pll1_sw",	        base + 0xc,  2,  1, pll1_sw_sels,      ARRAY_SIZE(pll1_sw_sels));
	clk[periph_pre]       = imx_clk_mux("periph_pre",       base + 0x18, 18, 2, periph_pre_sels,   ARRAY_SIZE(periph_pre_sels));
	clk[periph2_pre]      = imx_clk_mux("periph2_pre",      base + 0x18, 21, 2, periph_pre_sels,   ARRAY_SIZE(periph_pre_sels));
	clk[periph_clk2_sel]  = imx_clk_mux("periph_clk2_sel",  base + 0x18, 12, 1, periph_clk2_sels,  ARRAY_SIZE(periph_clk2_sels));
	clk[periph2_clk2_sel] = imx_clk_mux("periph2_clk2_sel", base + 0x18, 20, 1, periph_clk2_sels,  ARRAY_SIZE(periph_clk2_sels));
	clk[axi_sel]          = imx_clk_mux("axi_sel",          base + 0x14, 6,  2, axi_sels,          ARRAY_SIZE(axi_sels));
	clk[esai_sel]         = imx_clk_mux("esai_sel",         base + 0x20, 19, 2, audio_sels,        ARRAY_SIZE(audio_sels));
	clk[asrc_sel]         = imx_clk_mux("asrc_sel",         base + 0x30, 7,  2, audio_sels,        ARRAY_SIZE(audio_sels));
	clk[spdif_sel]        = imx_clk_mux("spdif_sel",        base + 0x30, 20, 2, audio_sels,        ARRAY_SIZE(audio_sels));
	clk[gpu2d_axi]        = imx_clk_mux("gpu2d_axi",        base + 0x18, 0,  1, gpu_axi_sels,      ARRAY_SIZE(gpu_axi_sels));
	clk[gpu3d_axi]        = imx_clk_mux("gpu3d_axi",        base + 0x18, 1,  1, gpu_axi_sels,      ARRAY_SIZE(gpu_axi_sels));
	clk[gpu2d_core_sel]   = imx_clk_mux("gpu2d_core_sel",   base + 0x18, 16, 2, gpu2d_core_sels,   ARRAY_SIZE(gpu2d_core_sels));
	clk[gpu3d_core_sel]   = imx_clk_mux("gpu3d_core_sel",   base + 0x18, 4,  2, gpu3d_core_sels,   ARRAY_SIZE(gpu3d_core_sels));
	clk[gpu3d_shader_sel] = imx_clk_mux("gpu3d_shader_sel", base + 0x18, 8,  2, gpu3d_shader_sels, ARRAY_SIZE(gpu3d_shader_sels));
	clk[ipu1_sel]         = imx_clk_mux("ipu1_sel",         base + 0x3c, 9,  2, ipu_sels,          ARRAY_SIZE(ipu_sels));
	clk[ipu2_sel]         = imx_clk_mux("ipu2_sel",         base + 0x3c, 14, 2, ipu_sels,          ARRAY_SIZE(ipu_sels));
	clk[ldb_di0_sel]      = imx_clk_mux("ldb_di0_sel",      base + 0x2c, 9,  3, ldb_di_sels,       ARRAY_SIZE(ldb_di_sels));
	clk[ldb_di1_sel]      = imx_clk_mux("ldb_di1_sel",      base + 0x2c, 12, 3, ldb_di_sels,       ARRAY_SIZE(ldb_di_sels));
	clk[ipu1_di0_pre_sel] = imx_clk_mux("ipu1_di0_pre_sel", base + 0x34, 6,  3, ipu_di_pre_sels,   ARRAY_SIZE(ipu_di_pre_sels));
	clk[ipu1_di1_pre_sel] = imx_clk_mux("ipu1_di1_pre_sel", base + 0x34, 15, 3, ipu_di_pre_sels,   ARRAY_SIZE(ipu_di_pre_sels));
	clk[ipu2_di0_pre_sel] = imx_clk_mux("ipu2_di0_pre_sel", base + 0x38, 6,  3, ipu_di_pre_sels,   ARRAY_SIZE(ipu_di_pre_sels));
	clk[ipu2_di1_pre_sel] = imx_clk_mux("ipu2_di1_pre_sel", base + 0x38, 15, 3, ipu_di_pre_sels,   ARRAY_SIZE(ipu_di_pre_sels));
	clk[ipu1_di0_sel]     = imx_clk_mux("ipu1_di0_sel",     base + 0x34, 0,  3, ipu1_di0_sels,     ARRAY_SIZE(ipu1_di0_sels));
	clk[ipu1_di1_sel]     = imx_clk_mux("ipu1_di1_sel",     base + 0x34, 9,  3, ipu1_di1_sels,     ARRAY_SIZE(ipu1_di1_sels));
	clk[ipu2_di0_sel]     = imx_clk_mux("ipu2_di0_sel",     base + 0x38, 0,  3, ipu2_di0_sels,     ARRAY_SIZE(ipu2_di0_sels));
	clk[ipu2_di1_sel]     = imx_clk_mux("ipu2_di1_sel",     base + 0x38, 9,  3, ipu2_di1_sels,     ARRAY_SIZE(ipu2_di1_sels));
	clk[hsi_tx_sel]       = imx_clk_mux("hsi_tx_sel",       base + 0x30, 28, 1, hsi_tx_sels,       ARRAY_SIZE(hsi_tx_sels));
	clk[pcie_axi_sel]     = imx_clk_mux("pcie_axi_sel",     base + 0x18, 10, 1, pcie_axi_sels,     ARRAY_SIZE(pcie_axi_sels));
	clk[ssi1_sel]         = imx_clk_mux("ssi1_sel",         base + 0x1c, 10, 2, ssi_sels,          ARRAY_SIZE(ssi_sels));
	clk[ssi2_sel]         = imx_clk_mux("ssi2_sel",         base + 0x1c, 12, 2, ssi_sels,          ARRAY_SIZE(ssi_sels));
	clk[ssi3_sel]         = imx_clk_mux("ssi3_sel",         base + 0x1c, 14, 2, ssi_sels,          ARRAY_SIZE(ssi_sels));
	clk[usdhc1_sel]       = imx_clk_mux("usdhc1_sel",       base + 0x1c, 16, 1, usdhc_sels,        ARRAY_SIZE(usdhc_sels));
	clk[usdhc2_sel]       = imx_clk_mux("usdhc2_sel",       base + 0x1c, 17, 1, usdhc_sels,        ARRAY_SIZE(usdhc_sels));
	clk[usdhc3_sel]       = imx_clk_mux("usdhc3_sel",       base + 0x1c, 18, 1, usdhc_sels,        ARRAY_SIZE(usdhc_sels));
	clk[usdhc4_sel]       = imx_clk_mux("usdhc4_sel",       base + 0x1c, 19, 1, usdhc_sels,        ARRAY_SIZE(usdhc_sels));
	clk[enfc_sel]         = imx_clk_mux("enfc_sel",         base + 0x2c, 16, 2, enfc_sels,         ARRAY_SIZE(enfc_sels));
	clk[emi_sel]          = imx_clk_mux("emi_sel",          base + 0x1c, 27, 2, emi_sels,          ARRAY_SIZE(emi_sels));
	clk[emi_slow_sel]     = imx_clk_mux("emi_slow_sel",     base + 0x1c, 29, 2, emi_sels,          ARRAY_SIZE(emi_sels));
	clk[vdo_axi_sel]      = imx_clk_mux("vdo_axi_sel",      base + 0x18, 11, 1, vdo_axi_sels,      ARRAY_SIZE(vdo_axi_sels));
	clk[vpu_axi_sel]      = imx_clk_mux("vpu_axi_sel",      base + 0x18, 14, 2, vpu_axi_sels,      ARRAY_SIZE(vpu_axi_sels));
	clk[cko1_sel]         = imx_clk_mux("cko1_sel",         base + 0x60, 0,  4, cko1_sels,         ARRAY_SIZE(cko1_sels));

	/*                              name         reg      shift width busy: reg, shift parent_names  num_parents */
	clk[periph]  = imx_clk_busy_mux("periph",  base + 0x14, 25,  1,   base + 0x48, 5,  periph_sels,  ARRAY_SIZE(periph_sels));
	clk[periph2] = imx_clk_busy_mux("periph2", base + 0x14, 26,  1,   base + 0x48, 3,  periph2_sels, ARRAY_SIZE(periph2_sels));

	/*                                      name                parent_name          reg       shift width */
	clk[periph_clk2]      = imx_clk_divider("periph_clk2",      "periph_clk2_sel",   base + 0x14, 27, 3);
	clk[periph2_clk2]     = imx_clk_divider("periph2_clk2",     "periph2_clk2_sel",  base + 0x14, 0,  3);
	clk[ipg]              = imx_clk_divider("ipg",              "ahb",               base + 0x14, 8,  2);
	clk[ipg_per]          = imx_clk_divider("ipg_per",          "ipg",               base + 0x1c, 0,  6);
	clk[esai_pred]        = imx_clk_divider("esai_pred",        "esai_sel",          base + 0x28, 9,  3);
	clk[esai_podf]        = imx_clk_divider("esai_podf",        "esai_pred",         base + 0x28, 25, 3);
	clk[asrc_pred]        = imx_clk_divider("asrc_pred",        "asrc_sel",          base + 0x30, 12, 3);
	clk[asrc_podf]        = imx_clk_divider("asrc_podf",        "asrc_pred",         base + 0x30, 9,  3);
	clk[spdif_pred]       = imx_clk_divider("spdif_pred",       "spdif_sel",         base + 0x30, 25, 3);
	clk[spdif_podf]       = imx_clk_divider("spdif_podf",       "spdif_pred",        base + 0x30, 22, 3);
	clk[can_root]         = imx_clk_divider("can_root",         "pll3_usb_otg",      base + 0x20, 2,  6);
	clk[ecspi_root]       = imx_clk_divider("ecspi_root",       "pll3_60m",          base + 0x38, 19, 6);
	clk[gpu2d_core_podf]  = imx_clk_divider("gpu2d_core_podf",  "gpu2d_core_sel",    base + 0x18, 23, 3);
	clk[gpu3d_core_podf]  = imx_clk_divider("gpu3d_core_podf",  "gpu3d_core_sel",    base + 0x18, 26, 3);
	clk[gpu3d_shader]     = imx_clk_divider("gpu3d_shader",     "gpu3d_shader_sel",  base + 0x18, 29, 3);
	clk[ipu1_podf]        = imx_clk_divider("ipu1_podf",        "ipu1_sel",          base + 0x3c, 11, 3);
	clk[ipu2_podf]        = imx_clk_divider("ipu2_podf",        "ipu2_sel",          base + 0x3c, 16, 3);
	clk[ldb_di0_podf]     = imx_clk_divider("ldb_di0_podf",     "ldb_di0_sel",       base + 0x20, 10, 1);
	clk[ldb_di1_podf]     = imx_clk_divider("ldb_di1_podf",     "ldb_di1_sel",       base + 0x20, 11, 1);
	clk[ipu1_di0_pre]     = imx_clk_divider("ipu1_di0_pre",     "ipu1_di0_pre_sel",  base + 0x34, 3,  3);
	clk[ipu1_di1_pre]     = imx_clk_divider("ipu1_di1_pre",     "ipu1_di1_pre_sel",  base + 0x34, 12, 3);
	clk[ipu2_di0_pre]     = imx_clk_divider("ipu2_di0_pre",     "ipu2_di0_pre_sel",  base + 0x38, 3,  3);
	clk[ipu2_di1_pre]     = imx_clk_divider("ipu2_di1_pre",     "ipu2_di1_pre_sel",  base + 0x38, 12, 3);
	clk[hsi_tx_podf]      = imx_clk_divider("hsi_tx_podf",      "hsi_tx_sel",        base + 0x30, 29, 3);
	clk[ssi1_pred]        = imx_clk_divider("ssi1_pred",        "ssi1_sel",          base + 0x28, 6,  3);
	clk[ssi1_podf]        = imx_clk_divider("ssi1_podf",        "ssi1_pred",         base + 0x28, 0,  6);
	clk[ssi2_pred]        = imx_clk_divider("ssi2_pred",        "ssi2_sel",          base + 0x2c, 6,  3);
	clk[ssi2_podf]        = imx_clk_divider("ssi2_podf",        "ssi2_pred",         base + 0x2c, 0,  6);
	clk[ssi3_pred]        = imx_clk_divider("ssi3_pred",        "ssi3_sel",          base + 0x28, 22, 3);
	clk[ssi3_podf]        = imx_clk_divider("ssi3_podf",        "ssi3_pred",         base + 0x28, 16, 6);
	clk[uart_serial_podf] = imx_clk_divider("uart_serial_podf", "pll3_80m",          base + 0x24, 0,  6);
	clk[usdhc1_podf]      = imx_clk_divider("usdhc1_podf",      "usdhc1_sel",        base + 0x24, 11, 3);
	clk[usdhc2_podf]      = imx_clk_divider("usdhc2_podf",      "usdhc2_sel",        base + 0x24, 16, 3);
	clk[usdhc3_podf]      = imx_clk_divider("usdhc3_podf",      "usdhc3_sel",        base + 0x24, 19, 3);
	clk[usdhc4_podf]      = imx_clk_divider("usdhc4_podf",      "usdhc4_sel",        base + 0x24, 22, 3);
	clk[enfc_pred]        = imx_clk_divider("enfc_pred",        "enfc_sel",          base + 0x2c, 18, 3);
	clk[enfc_podf]        = imx_clk_divider("enfc_podf",        "enfc_pred",         base + 0x2c, 21, 6);
	clk[emi_podf]         = imx_clk_divider("emi_podf",         "emi_sel",           base + 0x1c, 20, 3);
	clk[emi_slow_podf]    = imx_clk_divider("emi_slow_podf",    "emi_slow_sel",      base + 0x1c, 23, 3);
	clk[vpu_axi_podf]     = imx_clk_divider("vpu_axi_podf",     "vpu_axi_sel",       base + 0x24, 25, 3);
	clk[cko1_podf]        = imx_clk_divider("cko1_podf",        "cko1_sel",          base + 0x60, 4,  3);

	/*                                            name                 parent_name    reg        shift width busy: reg, shift */
	clk[axi]               = imx_clk_busy_divider("axi",               "axi_sel",     base + 0x14, 16,  3,   base + 0x48, 0);
	clk[mmdc_ch0_axi_podf] = imx_clk_busy_divider("mmdc_ch0_axi_podf", "periph",      base + 0x14, 19,  3,   base + 0x48, 4);
	clk[mmdc_ch1_axi_podf] = imx_clk_busy_divider("mmdc_ch1_axi_podf", "periph2",     base + 0x14, 3,   3,   base + 0x48, 2);
	clk[arm]               = imx_clk_busy_divider("arm",               "pll1_sw",     base + 0x10, 0,   3,   base + 0x48, 16);
	clk[ahb]               = imx_clk_busy_divider("ahb",               "periph",      base + 0x14, 10,  3,   base + 0x48, 1);

	/*                                name             parent_name          reg         shift */
	clk[apbh_dma]     = imx_clk_gate2("apbh_dma",      "ahb",               base + 0x68, 4);
	clk[asrc]         = imx_clk_gate2("asrc",          "asrc_podf",         base + 0x68, 6);
	clk[can1_ipg]     = imx_clk_gate2("can1_ipg",      "ipg",               base + 0x68, 14);
	clk[can1_serial]  = imx_clk_gate2("can1_serial",   "can_root",          base + 0x68, 16);
	clk[can2_ipg]     = imx_clk_gate2("can2_ipg",      "ipg",               base + 0x68, 18);
	clk[can2_serial]  = imx_clk_gate2("can2_serial",   "can_root",          base + 0x68, 20);
	clk[ecspi1]       = imx_clk_gate2("ecspi1",        "ecspi_root",        base + 0x6c, 0);
	clk[ecspi2]       = imx_clk_gate2("ecspi2",        "ecspi_root",        base + 0x6c, 2);
	clk[ecspi3]       = imx_clk_gate2("ecspi3",        "ecspi_root",        base + 0x6c, 4);
	clk[ecspi4]       = imx_clk_gate2("ecspi4",        "ecspi_root",        base + 0x6c, 6);
	clk[ecspi5]       = imx_clk_gate2("ecspi5",        "ecspi_root",        base + 0x6c, 8);
	clk[enet]         = imx_clk_gate2("enet",          "ipg",               base + 0x6c, 10);
	clk[esai]         = imx_clk_gate2("esai",          "esai_podf",         base + 0x6c, 16);
	clk[gpt_ipg]      = imx_clk_gate2("gpt_ipg",       "ipg",               base + 0x6c, 20);
	clk[gpt_ipg_per]  = imx_clk_gate2("gpt_ipg_per",   "ipg_per",           base + 0x6c, 22);
	clk[gpu2d_core]   = imx_clk_gate2("gpu2d_core",    "gpu2d_core_podf",   base + 0x6c, 24);
	clk[gpu3d_core]   = imx_clk_gate2("gpu3d_core",    "gpu3d_core_podf",   base + 0x6c, 26);
	clk[hdmi_iahb]    = imx_clk_gate2("hdmi_iahb",     "ahb",               base + 0x70, 0);
	clk[hdmi_isfr]    = imx_clk_gate2("hdmi_isfr",     "pll3_pfd1_540m",    base + 0x70, 4);
	clk[i2c1]         = imx_clk_gate2("i2c1",          "ipg_per",           base + 0x70, 6);
	clk[i2c2]         = imx_clk_gate2("i2c2",          "ipg_per",           base + 0x70, 8);
	clk[i2c3]         = imx_clk_gate2("i2c3",          "ipg_per",           base + 0x70, 10);
	clk[iim]          = imx_clk_gate2("iim",           "ipg",               base + 0x70, 12);
	clk[enfc]         = imx_clk_gate2("enfc",          "enfc_podf",         base + 0x70, 14);
	clk[ipu1]         = imx_clk_gate2("ipu1",          "ipu1_podf",         base + 0x74, 0);
	clk[ipu1_di0]     = imx_clk_gate2("ipu1_di0",      "ipu1_di0_sel",      base + 0x74, 2);
	clk[ipu1_di1]     = imx_clk_gate2("ipu1_di1",      "ipu1_di1_sel",      base + 0x74, 4);
	clk[ipu2]         = imx_clk_gate2("ipu2",          "ipu2_podf",         base + 0x74, 6);
	clk[ipu2_di0]     = imx_clk_gate2("ipu2_di0",      "ipu2_di0_sel",      base + 0x74, 8);
	clk[ldb_di0]      = imx_clk_gate2("ldb_di0",       "ldb_di0_podf",      base + 0x74, 12);
	clk[ldb_di1]      = imx_clk_gate2("ldb_di1",       "ldb_di1_podf",      base + 0x74, 14);
	clk[ipu2_di1]     = imx_clk_gate2("ipu2_di1",      "ipu2_di1_sel",      base + 0x74, 10);
	clk[hsi_tx]       = imx_clk_gate2("hsi_tx",        "hsi_tx_podf",       base + 0x74, 16);
	clk[mlb]          = imx_clk_gate2("mlb",           "pll6_mlb",          base + 0x74, 18);
	clk[mmdc_ch0_axi] = imx_clk_gate2("mmdc_ch0_axi",  "mmdc_ch0_axi_podf", base + 0x74, 20);
	clk[mmdc_ch1_axi] = imx_clk_gate2("mmdc_ch1_axi",  "mmdc_ch1_axi_podf", base + 0x74, 22);
	clk[ocram]        = imx_clk_gate2("ocram",         "ahb",               base + 0x74, 28);
	clk[openvg_axi]   = imx_clk_gate2("openvg_axi",    "axi",               base + 0x74, 30);
	clk[pcie_axi]     = imx_clk_gate2("pcie_axi",      "pcie_axi_sel",      base + 0x78, 0);
	clk[pwm1]         = imx_clk_gate2("pwm1",          "ipg_per",           base + 0x78, 16);
	clk[pwm2]         = imx_clk_gate2("pwm2",          "ipg_per",           base + 0x78, 18);
	clk[pwm3]         = imx_clk_gate2("pwm3",          "ipg_per",           base + 0x78, 20);
	clk[pwm4]         = imx_clk_gate2("pwm4",          "ipg_per",           base + 0x78, 22);
	clk[gpmi_bch_apb] = imx_clk_gate2("gpmi_bch_apb",  "usdhc3",            base + 0x78, 24);
	clk[gpmi_bch]     = imx_clk_gate2("gpmi_bch",      "usdhc4",            base + 0x78, 26);
	clk[gpmi_io]      = imx_clk_gate2("gpmi_io",       "enfc",              base + 0x78, 28);
	clk[gpmi_apb]     = imx_clk_gate2("gpmi_apb",      "usdhc3",            base + 0x78, 30);
	clk[sata]         = imx_clk_gate2("sata",          "ipg",               base + 0x7c, 4);
	clk[sdma]         = imx_clk_gate2("sdma",          "ahb",               base + 0x7c, 6);
	clk[spba]         = imx_clk_gate2("spba",          "ipg",               base + 0x7c, 12);
	clk[ssi1]         = imx_clk_gate2("ssi1",          "ssi1_podf",         base + 0x7c, 18);
	clk[ssi2]         = imx_clk_gate2("ssi2",          "ssi2_podf",         base + 0x7c, 20);
	clk[ssi3]         = imx_clk_gate2("ssi3",          "ssi3_podf",         base + 0x7c, 22);
	clk[uart_ipg]     = imx_clk_gate2("uart_ipg",      "ipg",               base + 0x7c, 24);
	clk[uart_serial]  = imx_clk_gate2("uart_serial",   "uart_serial_podf",  base + 0x7c, 26);
	clk[usboh3]       = imx_clk_gate2("usboh3",        "ipg",               base + 0x80, 0);
	clk[usdhc1]       = imx_clk_gate2("usdhc1",        "usdhc1_podf",       base + 0x80, 2);
	clk[usdhc2]       = imx_clk_gate2("usdhc2",        "usdhc2_podf",       base + 0x80, 4);
	clk[usdhc3]       = imx_clk_gate2("usdhc3",        "usdhc3_podf",       base + 0x80, 6);
	clk[usdhc4]       = imx_clk_gate2("usdhc4",        "usdhc4_podf",       base + 0x80, 8);
	clk[vdo_axi]      = imx_clk_gate2("vdo_axi",       "vdo_axi_sel",       base + 0x80, 12);
	clk[vpu_axi]      = imx_clk_gate2("vpu_axi",       "vpu_axi_podf",      base + 0x80, 14);
	clk[cko1]         = imx_clk_gate("cko1",           "cko1_podf",         base + 0x60, 7);

	for (i = 0; i < ARRAY_SIZE(clk); i++)
		if (IS_ERR(clk[i]))
			pr_err("i.MX6q clk %d: register failed with %ld\n",
				i, PTR_ERR(clk[i]));

	clk_register_clkdev(clk[mmdc_ch0_axi], NULL, "mmdc_ch0_axi");
	clk_register_clkdev(clk[mmdc_ch1_axi], NULL, "mmdc_ch1_axi");
	clk_register_clkdev(clk[gpt_ipg], "ipg", "imx-gpt.0");
	clk_register_clkdev(clk[gpt_ipg_per], "per", "imx-gpt.0");
	clk_register_clkdev(clk[twd], NULL, "smp_twd");
	clk_register_clkdev(clk[usboh3], NULL, "usboh3");
	clk_register_clkdev(clk[uart_serial], "per", "2020000.serial");
	clk_register_clkdev(clk[uart_ipg], "ipg", "2020000.serial");
	clk_register_clkdev(clk[uart_serial], "per", "21e8000.serial");
	clk_register_clkdev(clk[uart_ipg], "ipg", "21e8000.serial");
	clk_register_clkdev(clk[uart_serial], "per", "21ec000.serial");
	clk_register_clkdev(clk[uart_ipg], "ipg", "21ec000.serial");
	clk_register_clkdev(clk[uart_serial], "per", "21f0000.serial");
	clk_register_clkdev(clk[uart_ipg], "ipg", "21f0000.serial");
	clk_register_clkdev(clk[uart_serial], "per", "21f4000.serial");
	clk_register_clkdev(clk[uart_ipg], "ipg", "21f4000.serial");
	clk_register_clkdev(clk[enet], NULL, "2188000.ethernet");
	clk_register_clkdev(clk[usdhc1], NULL, "2190000.usdhc");
	clk_register_clkdev(clk[usdhc2], NULL, "2194000.usdhc");
	clk_register_clkdev(clk[usdhc3], NULL, "2198000.usdhc");
	clk_register_clkdev(clk[usdhc4], NULL, "219c000.usdhc");
	clk_register_clkdev(clk[i2c1], NULL, "21a0000.i2c");
	clk_register_clkdev(clk[i2c2], NULL, "21a4000.i2c");
	clk_register_clkdev(clk[i2c3], NULL, "21a8000.i2c");
	clk_register_clkdev(clk[ecspi1], NULL, "2008000.ecspi");
	clk_register_clkdev(clk[ecspi2], NULL, "200c000.ecspi");
	clk_register_clkdev(clk[ecspi3], NULL, "2010000.ecspi");
	clk_register_clkdev(clk[ecspi4], NULL, "2014000.ecspi");
	clk_register_clkdev(clk[ecspi5], NULL, "2018000.ecspi");
	clk_register_clkdev(clk[sdma], NULL, "20ec000.sdma");
	clk_register_clkdev(clk[dummy], NULL, "20bc000.wdog");
	clk_register_clkdev(clk[dummy], NULL, "20c0000.wdog");

	for (i = 0; i < ARRAY_SIZE(clks_init_on); i++) {
		c = clk_get_sys(clks_init_on[i], NULL);
		if (IS_ERR(c)) {
			pr_err("%s: failed to get clk %s", __func__,
			       clks_init_on[i]);
			return PTR_ERR(c);
		}
		clk_prepare_enable(c);
	}

	np = of_find_compatible_node(NULL, NULL, "fsl,imx6q-gpt");
	base = of_iomap(np, 0);
	WARN_ON(!base);
	irq = irq_of_parse_and_map(np, 0);
	mxc_timer_init(NULL, base, irq);

	return 0;
}
