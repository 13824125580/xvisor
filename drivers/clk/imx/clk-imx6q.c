/*
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * Adapted from Linux Kernel 3.13.6 arch/arm/mach-imx/clk-imx6q.c
 *
 * Copyright 2011-2013 Freescale Semiconductor, Inc.
 * Copyright 2011 Linaro Ltd.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @file clk-imx6q.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Freescale i.MX6Q clock management
 */

#include "clk.h"
#include <linux/device.h>
#include <imx-common.h>
#include <imx-hardware.h>
#include <linux/clkdev.h>
#if 0
#include <dt-bindings/clock/imx6qdl-clock.h>
#else
#include <imx6qdl-clock.h>
#endif /* 0 */

static const char *step_sels[]	= { "osc", "pll2_pfd2_396m", };
static const char *pll1_sw_sels[]	= { "pll1_sys", "step", };
static const char *periph_pre_sels[]	= { "pll2_bus", "pll2_pfd2_396m", "pll2_pfd0_352m", "pll2_198m", };
static const char *periph_clk2_sels[]	= { "pll3_usb_otg", "osc", "osc", "dummy", };
static const char *periph2_clk2_sels[]	= { "pll3_usb_otg", "pll2_bus", };
static const char *periph_sels[]	= { "periph_pre", "periph_clk2", };
static const char *periph2_sels[]	= { "periph2_pre", "periph2_clk2", };
static const char *axi_sels[]		= { "periph", "pll2_pfd2_396m", "periph", "pll3_pfd1_540m", };
static const char *audio_sels[]	= { "pll4_audio_div", "pll3_pfd2_508m", "pll3_pfd3_454m", "pll3_usb_otg", };
static const char *gpu_axi_sels[]	= { "axi", "ahb", };
static const char *gpu2d_core_sels[]	= { "axi", "pll3_usb_otg", "pll2_pfd0_352m", "pll2_pfd2_396m", };
static const char *gpu3d_core_sels[]	= { "mmdc_ch0_axi", "pll3_usb_otg", "pll2_pfd1_594m", "pll2_pfd2_396m", };
static const char *gpu3d_shader_sels[] = { "mmdc_ch0_axi", "pll3_usb_otg", "pll2_pfd1_594m", "pll3_pfd0_720m", };
static const char *ipu_sels[]		= { "mmdc_ch0_axi", "pll2_pfd2_396m", "pll3_120m", "pll3_pfd1_540m", };
static const char *ldb_di_sels[]	= { "pll5_video_div", "pll2_pfd0_352m", "pll2_pfd2_396m", "mmdc_ch1_axi", "pll3_usb_otg", };
static const char *ipu_di_pre_sels[]	= { "mmdc_ch0_axi", "pll3_usb_otg", "pll5_video_div", "pll2_pfd0_352m", "pll2_pfd2_396m", "pll3_pfd1_540m", };
static const char *ipu1_di0_sels[]	= { "ipu1_di0_pre", "dummy", "dummy", "ldb_di0", "ldb_di1", };
static const char *ipu1_di1_sels[]	= { "ipu1_di1_pre", "dummy", "dummy", "ldb_di0", "ldb_di1", };
static const char *ipu2_di0_sels[]	= { "ipu2_di0_pre", "dummy", "dummy", "ldb_di0", "ldb_di1", };
static const char *ipu2_di1_sels[]	= { "ipu2_di1_pre", "dummy", "dummy", "ldb_di0", "ldb_di1", };
static const char *hsi_tx_sels[]	= { "pll3_120m", "pll2_pfd2_396m", };
static const char *pcie_axi_sels[]	= { "axi", "ahb", };
static const char *ssi_sels[]		= { "pll3_pfd2_508m", "pll3_pfd3_454m", "pll4_audio_div", };
static const char *usdhc_sels[]	= { "pll2_pfd2_396m", "pll2_pfd0_352m", };
static const char *enfc_sels[]	= { "pll2_pfd0_352m", "pll2_bus", "pll3_usb_otg", "pll2_pfd2_396m", };
static const char *emi_sels[]		= { "pll2_pfd2_396m", "pll3_usb_otg", "axi", "pll2_pfd0_352m", };
static const char *emi_slow_sels[]      = { "axi", "pll3_usb_otg", "pll2_pfd2_396m", "pll2_pfd0_352m", };
static const char *vdo_axi_sels[]	= { "axi", "ahb", };
static const char *vpu_axi_sels[]	= { "axi", "pll2_pfd2_396m", "pll2_pfd0_352m", };
static const char *cko1_sels[]	= { "pll3_usb_otg", "pll2_bus", "pll1_sys", "pll5_video_div",
				    "dummy", "axi", "enfc", "ipu1_di0", "ipu1_di1", "ipu2_di0",
				    "ipu2_di1", "ahb", "ipg", "ipg_per", "ckil", "pll4_audio_div", };
static const char *cko2_sels[] = {
	"mmdc_ch0_axi", "mmdc_ch1_axi", "usdhc4", "usdhc1",
	"gpu2d_axi", "dummy", "ecspi_root", "gpu3d_axi",
	"usdhc3", "dummy", "arm", "ipu1",
	"ipu2", "vdo_axi", "osc", "gpu2d_core",
	"gpu3d_core", "usdhc2", "ssi1", "ssi2",
	"ssi3", "gpu3d_shader", "vpu_axi", "can_root",
	"ldb_di0", "ldb_di1", "esai_extal", "eim_slow",
	"uart_serial", "spdif", "asrc", "hsi_tx",
};
static const char *cko_sels[] = { "cko1", "cko2", };
static const char *lvds_sels[] = {
	"dummy", "dummy", "dummy", "dummy", "dummy", "dummy",
	"pll4_audio", "pll5_video", "pll8_mlb", "enet_ref",
	"pcie_ref_125m", "sata_ref_100m",
};

static struct clk *clk[IMX6QDL_CLK_END];
static struct clk_onecell_data clk_data;

static unsigned int const clks_init_on[] __initconst = {
	/* FIXME: Add the clocks to initialize */
	IMX6QDL_CLK_MMDC_CH0_AXI,
	IMX6QDL_CLK_ROM,
	IMX6QDL_CLK_ARM,
};

static struct clk_div_table clk_enet_ref_table[] = {
	{ .val = 0, .div = 20, },
	{ .val = 1, .div = 10, },
	{ .val = 2, .div = 5, },
	{ .val = 3, .div = 4, },
	{ /* sentinel */ }
};

static struct clk_div_table post_div_table[] = {
	{ .val = 2, .div = 1, },
	{ .val = 1, .div = 2, },
	{ .val = 0, .div = 4, },
	{ /* sentinel */ }
};

static struct clk_div_table video_div_table[] = {
	{ .val = 0, .div = 1, },
	{ .val = 1, .div = 2, },
	{ .val = 2, .div = 1, },
	{ .val = 3, .div = 4, },
	{ /* sentinel */ }
};

static unsigned int share_count_esai;
static unsigned int share_count_asrc;

static void __init imx6q_clocks_init(struct device_node *ccm_node)
{
	u32 rate = 0;
	struct device_node *np;
	virtual_addr_t vbase = 0;
	void __iomem *base;
	unsigned int i;
	int ret;

	clk[IMX6QDL_CLK_DUMMY] = imx_clk_fixed("dummy", 0);

	rate = 0;
	if (NULL != (np = vmm_devtree_getnode("/clocks/ckil")))
		vmm_devtree_clock_frequency(np, &rate);
	vmm_devtree_dref_node(np);
	clk[IMX6QDL_CLK_CKIL] = imx_obtain_fixed_clock("ckil", rate);

	rate = 0;
	if (NULL != (np = vmm_devtree_getnode("/clocks/ckih1")))
		vmm_devtree_clock_frequency(np, &rate);
	vmm_devtree_dref_node(np);
	clk[IMX6QDL_CLK_CKIH] = imx_obtain_fixed_clock("ckih1", rate);

	rate = 0;
	if (NULL != (np = vmm_devtree_getnode("/clocks/osc")))
		vmm_devtree_clock_frequency(np, &rate);
	vmm_devtree_dref_node(np);
	clk[IMX6QDL_CLK_OSC] = imx_obtain_fixed_clock("osc", rate);

	if (NULL == (np = vmm_devtree_find_compatible(NULL, NULL,
						      "fsl,imx6q-anatop"))) {
		WARN(1, "Failed to find compatible \"fsl,imx6q-anatop\"\n");
		return;
	}

	if (VMM_OK != vmm_devtree_request_regmap(np, &vbase, 0, "i.MX6q ANATOP")) {
		WARN(1, "Failed to map imx6q-anatop registers\n");
		vmm_devtree_dref_node(np);
		return;
	}
	vmm_devtree_dref_node(np);
	base = (void __iomem *)vbase;

	/* Audio/video PLL post dividers do not work on i.MX6q revision 1.0 */
	if (cpu_is_imx6q() && imx_get_soc_revision() == IMX_CHIP_REVISION_1_0) {
		post_div_table[1].div = 1;
		post_div_table[2].div = 1;
		video_div_table[1].div = 1;
		video_div_table[2].div = 1;
	};

	/*                   type                               name         parent_name  base     div_mask */
	clk[IMX6QDL_CLK_PLL1_SYS]      = imx_clk_pllv3(IMX_PLLV3_SYS,	"pll1_sys",	"osc", base,        0x7f);
	clk[IMX6QDL_CLK_PLL2_BUS]      = imx_clk_pllv3(IMX_PLLV3_GENERIC,	"pll2_bus",	"osc", base + 0x30, 0x1);
	clk[IMX6QDL_CLK_PLL3_USB_OTG]  = imx_clk_pllv3(IMX_PLLV3_USB,	"pll3_usb_otg",	"osc", base + 0x10, 0x3);
	clk[IMX6QDL_CLK_PLL4_AUDIO]    = imx_clk_pllv3(IMX_PLLV3_AV,	"pll4_audio",	"osc", base + 0x70, 0x7f);
	clk[IMX6QDL_CLK_PLL5_VIDEO]    = imx_clk_pllv3(IMX_PLLV3_AV,	"pll5_video",	"osc", base + 0xa0, 0x7f);
	clk[IMX6QDL_CLK_PLL6_ENET]     = imx_clk_pllv3(IMX_PLLV3_ENET,	"pll6_enet",	"osc", base + 0xe0, 0x3);
	clk[IMX6QDL_CLK_PLL7_USB_HOST] = imx_clk_pllv3(IMX_PLLV3_USB,	"pll7_usb_host","osc", base + 0x20, 0x3);

	/*
	 * Bit 20 is the reserved and read-only bit, we do this only for:
	 * - Do nothing for usbphy clk_enable/disable
	 * - Keep refcount when do usbphy clk_enable/disable, in that case,
	 * the clk framework may need to enable/disable usbphy's parent
	 */
	clk[IMX6QDL_CLK_USBPHY1] = imx_clk_gate("usbphy1", "pll3_usb_otg", base + 0x10, 20);
	clk[IMX6QDL_CLK_USBPHY2] = imx_clk_gate("usbphy2", "pll7_usb_host", base + 0x20, 20);

	/*
	 * usbphy*_gate needs to be on after system boots up, and software
	 * never needs to control it anymore.
	 */
	clk[IMX6QDL_CLK_USBPHY1_GATE] = imx_clk_gate("usbphy1_gate", "dummy", base + 0x10, 6);
	clk[IMX6QDL_CLK_USBPHY2_GATE] = imx_clk_gate("usbphy2_gate", "dummy", base + 0x20, 6);

	clk[IMX6QDL_CLK_SATA_REF] = imx_clk_fixed_factor("sata_ref", "pll6_enet", 1, 5);
	clk[IMX6QDL_CLK_PCIE_REF] = imx_clk_fixed_factor("pcie_ref", "pll6_enet", 1, 4);

	clk[IMX6QDL_CLK_SATA_REF_100M] = imx_clk_gate("sata_ref_100m", "sata_ref", base + 0xe0, 20);
	clk[IMX6QDL_CLK_PCIE_REF_125M] = imx_clk_gate("pcie_ref_125m", "pcie_ref", base + 0xe0, 19);

	clk[IMX6QDL_CLK_ENET_REF] = clk_register_divider_table(NULL, "enet_ref", "pll6_enet", 0,
			base + 0xe0, 0, 2, 0, clk_enet_ref_table,
			&imx_ccm_lock);

	clk[IMX6QDL_CLK_LVDS1_SEL] = imx_clk_mux("lvds1_sel", base + 0x160, 0, 5, lvds_sels, ARRAY_SIZE(lvds_sels));
	clk[IMX6QDL_CLK_LVDS2_SEL] = imx_clk_mux("lvds2_sel", base + 0x160, 5, 5, lvds_sels, ARRAY_SIZE(lvds_sels));

	/*
	 * lvds1_gate and lvds2_gate are pseudo-gates.  Both can be
	 * independently configured as clock inputs or outputs.  We treat
	 * the "output_enable" bit as a gate, even though it's really just
	 * enabling clock output.
	 */
	clk[IMX6QDL_CLK_LVDS1_GATE] = imx_clk_gate("lvds1_gate", "dummy", base + 0x160, 10);
	clk[IMX6QDL_CLK_LVDS2_GATE] = imx_clk_gate("lvds2_gate", "dummy", base + 0x160, 11);

	/*                                name              parent_name        reg       idx */
	clk[IMX6QDL_CLK_PLL2_PFD0_352M] = imx_clk_pfd("pll2_pfd0_352m", "pll2_bus",     base + 0x100, 0);
	clk[IMX6QDL_CLK_PLL2_PFD1_594M] = imx_clk_pfd("pll2_pfd1_594m", "pll2_bus",     base + 0x100, 1);
	clk[IMX6QDL_CLK_PLL2_PFD2_396M] = imx_clk_pfd("pll2_pfd2_396m", "pll2_bus",     base + 0x100, 2);
	clk[IMX6QDL_CLK_PLL3_PFD0_720M] = imx_clk_pfd("pll3_pfd0_720m", "pll3_usb_otg", base + 0xf0,  0);
	clk[IMX6QDL_CLK_PLL3_PFD1_540M] = imx_clk_pfd("pll3_pfd1_540m", "pll3_usb_otg", base + 0xf0,  1);
	clk[IMX6QDL_CLK_PLL3_PFD2_508M] = imx_clk_pfd("pll3_pfd2_508m", "pll3_usb_otg", base + 0xf0,  2);
	clk[IMX6QDL_CLK_PLL3_PFD3_454M] = imx_clk_pfd("pll3_pfd3_454m", "pll3_usb_otg", base + 0xf0,  3);

	/*                                    name         parent_name     mult div */
	clk[IMX6QDL_CLK_PLL2_198M] = imx_clk_fixed_factor("pll2_198m", "pll2_pfd2_396m", 1, 2);
	clk[IMX6QDL_CLK_PLL3_120M] = imx_clk_fixed_factor("pll3_120m", "pll3_usb_otg",   1, 4);
	clk[IMX6QDL_CLK_PLL3_80M]  = imx_clk_fixed_factor("pll3_80m",  "pll3_usb_otg",   1, 6);
	clk[IMX6QDL_CLK_PLL3_60M]  = imx_clk_fixed_factor("pll3_60m",  "pll3_usb_otg",   1, 8);
	clk[IMX6QDL_CLK_TWD]       = imx_clk_fixed_factor("twd",       "arm",            1, 2);

	clk[IMX6QDL_CLK_PLL4_POST_DIV] = clk_register_divider_table(NULL, "pll4_post_div", "pll4_audio", CLK_SET_RATE_PARENT, base + 0x70, 19, 2, 0, post_div_table, &imx_ccm_lock);
	clk[IMX6QDL_CLK_PLL4_AUDIO_DIV] = clk_register_divider(NULL, "pll4_audio_div", "pll4_post_div", CLK_SET_RATE_PARENT, base + 0x170, 15, 1, 0, &imx_ccm_lock);
	clk[IMX6QDL_CLK_PLL5_POST_DIV] = clk_register_divider_table(NULL, "pll5_post_div", "pll5_video", CLK_SET_RATE_PARENT, base + 0xa0, 19, 2, 0, post_div_table, &imx_ccm_lock);
	clk[IMX6QDL_CLK_PLL5_VIDEO_DIV] = clk_register_divider_table(NULL, "pll5_video_div", "pll5_post_div", CLK_SET_RATE_PARENT, base + 0x170, 30, 2, 0, video_div_table, &imx_ccm_lock);

	np = ccm_node;
	if (VMM_OK != (vmm_devtree_request_regmap(np, &vbase, 0, "i.MX6q CCM")))
	{
		vmm_printf("Failed to map CCM registers\n");
		return;
	}
	base = (void __iomem *)vbase;

	imx6q_pm_set_ccm_base(base);

	/*                                  name                reg       shift width parent_names     num_parents */
	clk[IMX6QDL_CLK_STEP]             = imx_clk_mux("step",	        base + 0xc,  8,  1, step_sels,	       ARRAY_SIZE(step_sels));
	clk[IMX6QDL_CLK_PLL1_SW]          = imx_clk_mux("pll1_sw",	        base + 0xc,  2,  1, pll1_sw_sels,      ARRAY_SIZE(pll1_sw_sels));
	clk[IMX6QDL_CLK_PERIPH_PRE]       = imx_clk_mux("periph_pre",       base + 0x18, 18, 2, periph_pre_sels,   ARRAY_SIZE(periph_pre_sels));
	clk[IMX6QDL_CLK_PERIPH2_PRE]      = imx_clk_mux("periph2_pre",      base + 0x18, 21, 2, periph_pre_sels,   ARRAY_SIZE(periph_pre_sels));
	clk[IMX6QDL_CLK_PERIPH_CLK2_SEL]  = imx_clk_mux("periph_clk2_sel",  base + 0x18, 12, 2, periph_clk2_sels,  ARRAY_SIZE(periph_clk2_sels));
	clk[IMX6QDL_CLK_PERIPH2_CLK2_SEL] = imx_clk_mux("periph2_clk2_sel", base + 0x18, 20, 1, periph2_clk2_sels, ARRAY_SIZE(periph2_clk2_sels));
	clk[IMX6QDL_CLK_AXI_SEL]          = imx_clk_mux("axi_sel",          base + 0x14, 6,  2, axi_sels,          ARRAY_SIZE(axi_sels));
	clk[IMX6QDL_CLK_ESAI_SEL]         = imx_clk_mux("esai_sel",         base + 0x20, 19, 2, audio_sels,        ARRAY_SIZE(audio_sels));
	clk[IMX6QDL_CLK_ASRC_SEL]         = imx_clk_mux("asrc_sel",         base + 0x30, 7,  2, audio_sels,        ARRAY_SIZE(audio_sels));
	clk[IMX6QDL_CLK_SPDIF_SEL]        = imx_clk_mux("spdif_sel",        base + 0x30, 20, 2, audio_sels,        ARRAY_SIZE(audio_sels));
	clk[IMX6QDL_CLK_GPU2D_AXI]        = imx_clk_mux("gpu2d_axi",        base + 0x18, 0,  1, gpu_axi_sels,      ARRAY_SIZE(gpu_axi_sels));
	clk[IMX6QDL_CLK_GPU3D_AXI]        = imx_clk_mux("gpu3d_axi",        base + 0x18, 1,  1, gpu_axi_sels,      ARRAY_SIZE(gpu_axi_sels));
	clk[IMX6QDL_CLK_GPU2D_CORE_SEL]   = imx_clk_mux("gpu2d_core_sel",   base + 0x18, 16, 2, gpu2d_core_sels,   ARRAY_SIZE(gpu2d_core_sels));
	clk[IMX6QDL_CLK_GPU3D_CORE_SEL]   = imx_clk_mux("gpu3d_core_sel",   base + 0x18, 4,  2, gpu3d_core_sels,   ARRAY_SIZE(gpu3d_core_sels));
	clk[IMX6QDL_CLK_GPU3D_SHADER_SEL] = imx_clk_mux("gpu3d_shader_sel", base + 0x18, 8,  2, gpu3d_shader_sels, ARRAY_SIZE(gpu3d_shader_sels));
	clk[IMX6QDL_CLK_IPU1_SEL]         = imx_clk_mux("ipu1_sel",         base + 0x3c, 9,  2, ipu_sels,          ARRAY_SIZE(ipu_sels));
	clk[IMX6QDL_CLK_IPU2_SEL]         = imx_clk_mux("ipu2_sel",         base + 0x3c, 14, 2, ipu_sels,          ARRAY_SIZE(ipu_sels));
	clk[IMX6QDL_CLK_LDB_DI0_SEL]      = imx_clk_mux_flags("ldb_di0_sel", base + 0x2c, 9,  3, ldb_di_sels,      ARRAY_SIZE(ldb_di_sels), CLK_SET_RATE_PARENT);
	clk[IMX6QDL_CLK_LDB_DI1_SEL]      = imx_clk_mux_flags("ldb_di1_sel", base + 0x2c, 12, 3, ldb_di_sels,      ARRAY_SIZE(ldb_di_sels), CLK_SET_RATE_PARENT);
	clk[IMX6QDL_CLK_IPU1_DI0_PRE_SEL] = imx_clk_mux("ipu1_di0_pre_sel", base + 0x34, 6,  3, ipu_di_pre_sels,   ARRAY_SIZE(ipu_di_pre_sels));
	clk[IMX6QDL_CLK_IPU1_DI1_PRE_SEL] = imx_clk_mux("ipu1_di1_pre_sel", base + 0x34, 15, 3, ipu_di_pre_sels,   ARRAY_SIZE(ipu_di_pre_sels));
	clk[IMX6QDL_CLK_IPU2_DI0_PRE_SEL] = imx_clk_mux("ipu2_di0_pre_sel", base + 0x38, 6,  3, ipu_di_pre_sels,   ARRAY_SIZE(ipu_di_pre_sels));
	clk[IMX6QDL_CLK_IPU2_DI1_PRE_SEL] = imx_clk_mux("ipu2_di1_pre_sel", base + 0x38, 15, 3, ipu_di_pre_sels,   ARRAY_SIZE(ipu_di_pre_sels));
	clk[IMX6QDL_CLK_IPU1_DI0_SEL]     = imx_clk_mux("ipu1_di0_sel",     base + 0x34, 0,  3, ipu1_di0_sels,     ARRAY_SIZE(ipu1_di0_sels));
	clk[IMX6QDL_CLK_IPU1_DI1_SEL]     = imx_clk_mux("ipu1_di1_sel",     base + 0x34, 9,  3, ipu1_di1_sels,     ARRAY_SIZE(ipu1_di1_sels));
	clk[IMX6QDL_CLK_IPU2_DI0_SEL]     = imx_clk_mux("ipu2_di0_sel",     base + 0x38, 0,  3, ipu2_di0_sels,     ARRAY_SIZE(ipu2_di0_sels));
	clk[IMX6QDL_CLK_IPU2_DI1_SEL]     = imx_clk_mux("ipu2_di1_sel",     base + 0x38, 9,  3, ipu2_di1_sels,     ARRAY_SIZE(ipu2_di1_sels));
	clk[IMX6QDL_CLK_HSI_TX_SEL]       = imx_clk_mux("hsi_tx_sel",       base + 0x30, 28, 1, hsi_tx_sels,       ARRAY_SIZE(hsi_tx_sels));
	clk[IMX6QDL_CLK_PCIE_AXI_SEL]     = imx_clk_mux("pcie_axi_sel",     base + 0x18, 10, 1, pcie_axi_sels,     ARRAY_SIZE(pcie_axi_sels));
	clk[IMX6QDL_CLK_SSI1_SEL]         = imx_clk_fixup_mux("ssi1_sel",   base + 0x1c, 10, 2, ssi_sels,          ARRAY_SIZE(ssi_sels),          imx_cscmr1_fixup);
	clk[IMX6QDL_CLK_SSI2_SEL]         = imx_clk_fixup_mux("ssi2_sel",   base + 0x1c, 12, 2, ssi_sels,          ARRAY_SIZE(ssi_sels),          imx_cscmr1_fixup);
	clk[IMX6QDL_CLK_SSI3_SEL]         = imx_clk_fixup_mux("ssi3_sel",   base + 0x1c, 14, 2, ssi_sels,          ARRAY_SIZE(ssi_sels),          imx_cscmr1_fixup);
	clk[IMX6QDL_CLK_USDHC1_SEL]       = imx_clk_fixup_mux("usdhc1_sel", base + 0x1c, 16, 1, usdhc_sels,        ARRAY_SIZE(usdhc_sels),        imx_cscmr1_fixup);
	clk[IMX6QDL_CLK_USDHC2_SEL]       = imx_clk_fixup_mux("usdhc2_sel", base + 0x1c, 17, 1, usdhc_sels,        ARRAY_SIZE(usdhc_sels),        imx_cscmr1_fixup);
	clk[IMX6QDL_CLK_USDHC3_SEL]       = imx_clk_fixup_mux("usdhc3_sel", base + 0x1c, 18, 1, usdhc_sels,        ARRAY_SIZE(usdhc_sels),        imx_cscmr1_fixup);
	clk[IMX6QDL_CLK_USDHC4_SEL]       = imx_clk_fixup_mux("usdhc4_sel", base + 0x1c, 19, 1, usdhc_sels,        ARRAY_SIZE(usdhc_sels),        imx_cscmr1_fixup);
	clk[IMX6QDL_CLK_ENFC_SEL]         = imx_clk_mux("enfc_sel",         base + 0x2c, 16, 2, enfc_sels,         ARRAY_SIZE(enfc_sels));
	clk[IMX6QDL_CLK_EMI_SEL]          = imx_clk_fixup_mux("emi_sel",      base + 0x1c, 27, 2, emi_sels,        ARRAY_SIZE(emi_sels),          imx_cscmr1_fixup);
	clk[IMX6QDL_CLK_EMI_SLOW_SEL]     = imx_clk_fixup_mux("emi_slow_sel", base + 0x1c, 29, 2, emi_slow_sels,   ARRAY_SIZE(emi_slow_sels),     imx_cscmr1_fixup);
	clk[IMX6QDL_CLK_VDO_AXI_SEL]      = imx_clk_mux("vdo_axi_sel",      base + 0x18, 11, 1, vdo_axi_sels,      ARRAY_SIZE(vdo_axi_sels));
	clk[IMX6QDL_CLK_VPU_AXI_SEL]      = imx_clk_mux("vpu_axi_sel",      base + 0x18, 14, 2, vpu_axi_sels,      ARRAY_SIZE(vpu_axi_sels));
	clk[IMX6QDL_CLK_CKO1_SEL]         = imx_clk_mux("cko1_sel",         base + 0x60, 0,  4, cko1_sels,         ARRAY_SIZE(cko1_sels));
	clk[IMX6QDL_CLK_CKO2_SEL]         = imx_clk_mux("cko2_sel",         base + 0x60, 16, 5, cko2_sels,         ARRAY_SIZE(cko2_sels));
	clk[IMX6QDL_CLK_CKO]              = imx_clk_mux("cko",              base + 0x60, 8, 1,  cko_sels,          ARRAY_SIZE(cko_sels));

	/*                              name         reg      shift width busy: reg, shift parent_names  num_parents */
	clk[IMX6QDL_CLK_PERIPH]  = imx_clk_busy_mux("periph",  base + 0x14, 25,  1,   base + 0x48, 5,  periph_sels,  ARRAY_SIZE(periph_sels));
	clk[IMX6QDL_CLK_PERIPH2] = imx_clk_busy_mux("periph2", base + 0x14, 26,  1,   base + 0x48, 3,  periph2_sels, ARRAY_SIZE(periph2_sels));

	/*                                      name                parent_name          reg       shift width */
	clk[IMX6QDL_CLK_PERIPH_CLK2]      = imx_clk_divider("periph_clk2",      "periph_clk2_sel",   base + 0x14, 27, 3);
	clk[IMX6QDL_CLK_PERIPH2_CLK2]     = imx_clk_divider("periph2_clk2",     "periph2_clk2_sel",  base + 0x14, 0,  3);
	clk[IMX6QDL_CLK_IPG]              = imx_clk_divider("ipg",              "ahb",               base + 0x14, 8,  2);
	clk[IMX6QDL_CLK_IPG_PER]          = imx_clk_fixup_divider("ipg_per",    "ipg",               base + 0x1c, 0,  6, imx_cscmr1_fixup);
	clk[IMX6QDL_CLK_ESAI_PRED]        = imx_clk_divider("esai_pred",        "esai_sel",          base + 0x28, 9,  3);
	clk[IMX6QDL_CLK_ESAI_PODF]        = imx_clk_divider("esai_podf",        "esai_pred",         base + 0x28, 25, 3);
	clk[IMX6QDL_CLK_ASRC_PRED]        = imx_clk_divider("asrc_pred",        "asrc_sel",          base + 0x30, 12, 3);
	clk[IMX6QDL_CLK_ASRC_PODF]        = imx_clk_divider("asrc_podf",        "asrc_pred",         base + 0x30, 9,  3);
	clk[IMX6QDL_CLK_SPDIF_PRED]       = imx_clk_divider("spdif_pred",       "spdif_sel",         base + 0x30, 25, 3);
	clk[IMX6QDL_CLK_SPDIF_PODF]       = imx_clk_divider("spdif_podf",       "spdif_pred",        base + 0x30, 22, 3);
	clk[IMX6QDL_CLK_CAN_ROOT]         = imx_clk_divider("can_root",         "pll3_60m",          base + 0x20, 2,  6);
	clk[IMX6QDL_CLK_ECSPI_ROOT]       = imx_clk_divider("ecspi_root",       "pll3_60m",          base + 0x38, 19, 6);
	clk[IMX6QDL_CLK_GPU2D_CORE_PODF]  = imx_clk_divider("gpu2d_core_podf",  "gpu2d_core_sel",    base + 0x18, 23, 3);
	clk[IMX6QDL_CLK_GPU3D_CORE_PODF]  = imx_clk_divider("gpu3d_core_podf",  "gpu3d_core_sel",    base + 0x18, 26, 3);
	clk[IMX6QDL_CLK_GPU3D_SHADER]     = imx_clk_divider("gpu3d_shader",     "gpu3d_shader_sel",  base + 0x18, 29, 3);
	clk[IMX6QDL_CLK_IPU1_PODF]        = imx_clk_divider("ipu1_podf",        "ipu1_sel",          base + 0x3c, 11, 3);
	clk[IMX6QDL_CLK_IPU2_PODF]        = imx_clk_divider("ipu2_podf",        "ipu2_sel",          base + 0x3c, 16, 3);
	clk[IMX6QDL_CLK_LDB_DI0_DIV_3_5]  = imx_clk_fixed_factor("ldb_di0_div_3_5", "ldb_di0_sel", 2, 7);
	clk[IMX6QDL_CLK_LDB_DI0_PODF]     = imx_clk_divider_flags("ldb_di0_podf", "ldb_di0_div_3_5", base + 0x20, 10, 1, 0);
	clk[IMX6QDL_CLK_LDB_DI1_DIV_3_5]  = imx_clk_fixed_factor("ldb_di1_div_3_5", "ldb_di1_sel", 2, 7);
	clk[IMX6QDL_CLK_LDB_DI1_PODF]     = imx_clk_divider_flags("ldb_di1_podf", "ldb_di1_div_3_5", base + 0x20, 11, 1, 0);
	clk[IMX6QDL_CLK_IPU1_DI0_PRE]     = imx_clk_divider("ipu1_di0_pre",     "ipu1_di0_pre_sel",  base + 0x34, 3,  3);
	clk[IMX6QDL_CLK_IPU1_DI1_PRE]     = imx_clk_divider("ipu1_di1_pre",     "ipu1_di1_pre_sel",  base + 0x34, 12, 3);
	clk[IMX6QDL_CLK_IPU2_DI0_PRE]     = imx_clk_divider("ipu2_di0_pre",     "ipu2_di0_pre_sel",  base + 0x38, 3,  3);
	clk[IMX6QDL_CLK_IPU2_DI1_PRE]     = imx_clk_divider("ipu2_di1_pre",     "ipu2_di1_pre_sel",  base + 0x38, 12, 3);
	clk[IMX6QDL_CLK_HSI_TX_PODF]      = imx_clk_divider("hsi_tx_podf",      "hsi_tx_sel",        base + 0x30, 29, 3);
	clk[IMX6QDL_CLK_SSI1_PRED]        = imx_clk_divider("ssi1_pred",        "ssi1_sel",          base + 0x28, 6,  3);
	clk[IMX6QDL_CLK_SSI1_PODF]        = imx_clk_divider("ssi1_podf",        "ssi1_pred",         base + 0x28, 0,  6);
	clk[IMX6QDL_CLK_SSI2_PRED]        = imx_clk_divider("ssi2_pred",        "ssi2_sel",          base + 0x2c, 6,  3);
	clk[IMX6QDL_CLK_SSI2_PODF]        = imx_clk_divider("ssi2_podf",        "ssi2_pred",         base + 0x2c, 0,  6);
	clk[IMX6QDL_CLK_SSI3_PRED]        = imx_clk_divider("ssi3_pred",        "ssi3_sel",          base + 0x28, 22, 3);
	clk[IMX6QDL_CLK_SSI3_PODF]        = imx_clk_divider("ssi3_podf",        "ssi3_pred",         base + 0x28, 16, 6);
	clk[IMX6QDL_CLK_UART_SERIAL_PODF] = imx_clk_divider("uart_serial_podf", "pll3_80m",          base + 0x24, 0,  6);
	clk[IMX6QDL_CLK_USDHC1_PODF]      = imx_clk_divider("usdhc1_podf",      "usdhc1_sel",        base + 0x24, 11, 3);
	clk[IMX6QDL_CLK_USDHC2_PODF]      = imx_clk_divider("usdhc2_podf",      "usdhc2_sel",        base + 0x24, 16, 3);
	clk[IMX6QDL_CLK_USDHC3_PODF]      = imx_clk_divider("usdhc3_podf",      "usdhc3_sel",        base + 0x24, 19, 3);
	clk[IMX6QDL_CLK_USDHC4_PODF]      = imx_clk_divider("usdhc4_podf",      "usdhc4_sel",        base + 0x24, 22, 3);
	clk[IMX6QDL_CLK_ENFC_PRED]        = imx_clk_divider("enfc_pred",        "enfc_sel",          base + 0x2c, 18, 3);
	clk[IMX6QDL_CLK_ENFC_PODF]        = imx_clk_divider("enfc_podf",        "enfc_pred",         base + 0x2c, 21, 6);
	clk[IMX6QDL_CLK_EMI_PODF]         = imx_clk_fixup_divider("emi_podf",   "emi_sel",           base + 0x1c, 20, 3, imx_cscmr1_fixup);
	clk[IMX6QDL_CLK_EMI_SLOW_PODF]    = imx_clk_fixup_divider("emi_slow_podf", "emi_slow_sel",   base + 0x1c, 23, 3, imx_cscmr1_fixup);
	clk[IMX6QDL_CLK_VPU_AXI_PODF]     = imx_clk_divider("vpu_axi_podf",     "vpu_axi_sel",       base + 0x24, 25, 3);
	clk[IMX6QDL_CLK_CKO1_PODF]        = imx_clk_divider("cko1_podf",        "cko1_sel",          base + 0x60, 4,  3);
	clk[IMX6QDL_CLK_CKO2_PODF]        = imx_clk_divider("cko2_podf",        "cko2_sel",          base + 0x60, 21, 3);

	/*                                            name                 parent_name    reg        shift width busy: reg, shift */
	clk[IMX6QDL_CLK_AXI]               = imx_clk_busy_divider("axi",               "axi_sel",     base + 0x14, 16,  3,   base + 0x48, 0);
	clk[IMX6QDL_CLK_MMDC_CH0_AXI_PODF] = imx_clk_busy_divider("mmdc_ch0_axi_podf", "periph",      base + 0x14, 19,  3,   base + 0x48, 4);
	clk[IMX6QDL_CLK_MMDC_CH1_AXI_PODF] = imx_clk_busy_divider("mmdc_ch1_axi_podf", "periph2",     base + 0x14, 3,   3,   base + 0x48, 2);
	clk[IMX6QDL_CLK_ARM]               = imx_clk_busy_divider("arm",               "pll1_sw",     base + 0x10, 0,   3,   base + 0x48, 16);
	clk[IMX6QDL_CLK_AHB]               = imx_clk_busy_divider("ahb",               "periph",      base + 0x14, 10,  3,   base + 0x48, 1);

	/*                                name             parent_name          reg         shift */
	clk[IMX6QDL_CLK_APBH_DMA]     = imx_clk_gate2("apbh_dma",      "usdhc3",            base + 0x68, 4);
	clk[IMX6QDL_CLK_ASRC]         = imx_clk_gate2_shared("asrc",         "asrc_podf",   base + 0x68, 6, &share_count_asrc);
	clk[IMX6QDL_CLK_ASRC_IPG]     = imx_clk_gate2_shared("asrc_ipg",     "ahb",         base + 0x68, 6, &share_count_asrc);
	clk[IMX6QDL_CLK_ASRC_MEM]     = imx_clk_gate2_shared("asrc_mem",     "ahb",         base + 0x68, 6, &share_count_asrc);
	clk[IMX6QDL_CLK_CAN1_IPG]     = imx_clk_gate2("can1_ipg",      "ipg",               base + 0x68, 14);
	clk[IMX6QDL_CLK_CAN1_SERIAL]  = imx_clk_gate2("can1_serial",   "can_root",          base + 0x68, 16);
	clk[IMX6QDL_CLK_CAN2_IPG]     = imx_clk_gate2("can2_ipg",      "ipg",               base + 0x68, 18);
	clk[IMX6QDL_CLK_CAN2_SERIAL]  = imx_clk_gate2("can2_serial",   "can_root",          base + 0x68, 20);
	clk[IMX6QDL_CLK_ECSPI1]       = imx_clk_gate2("ecspi1",        "ecspi_root",        base + 0x6c, 0);
	clk[IMX6QDL_CLK_ECSPI2]       = imx_clk_gate2("ecspi2",        "ecspi_root",        base + 0x6c, 2);
	clk[IMX6QDL_CLK_ECSPI3]       = imx_clk_gate2("ecspi3",        "ecspi_root",        base + 0x6c, 4);
	clk[IMX6QDL_CLK_ECSPI4]       = imx_clk_gate2("ecspi4",        "ecspi_root",        base + 0x6c, 6);
	clk[IMX6Q_CLK_ECSPI5]       = imx_clk_gate2("ecspi5",        "ecspi_root",        base + 0x6c, 8);
	clk[IMX6QDL_CLK_ENET]         = imx_clk_gate2("enet",          "ipg",               base + 0x6c, 10);
	clk[IMX6QDL_CLK_ESAI_EXTAL]   = imx_clk_gate2_shared("esai_extal",   "esai_podf",   base + 0x6c, 16, &share_count_esai);
	clk[IMX6QDL_CLK_ESAI_IPG]     = imx_clk_gate2_shared("esai_ipg",   "ipg",           base + 0x6c, 16, &share_count_esai);
	clk[IMX6QDL_CLK_ESAI_MEM]     = imx_clk_gate2_shared("esai_mem", "ahb",             base + 0x6c, 16, &share_count_esai);
	clk[IMX6QDL_CLK_GPT_IPG]      = imx_clk_gate2("gpt_ipg",       "ipg",               base + 0x6c, 20);
	clk[IMX6QDL_CLK_GPT_IPG_PER]  = imx_clk_gate2("gpt_ipg_per",   "ipg_per",           base + 0x6c, 22);
	if (cpu_is_imx6dl())
		/*
		 * The multiplexer and divider of imx6q clock gpu3d_shader get
		 * redefined/reused as gpu2d_core_sel and gpu2d_core_podf on imx6dl.
		 */
		clk[IMX6QDL_CLK_GPU2D_CORE] = imx_clk_gate2("gpu2d_core", "gpu3d_shader", base + 0x6c, 24);
	else
		clk[IMX6QDL_CLK_GPU2D_CORE] = imx_clk_gate2("gpu2d_core", "gpu2d_core_podf", base + 0x6c, 24);
	clk[IMX6QDL_CLK_GPU3D_CORE]   = imx_clk_gate2("gpu3d_core",    "gpu3d_core_podf",   base + 0x6c, 26);
	clk[IMX6QDL_CLK_HDMI_IAHB]    = imx_clk_gate2("hdmi_iahb",     "ahb",               base + 0x70, 0);
	clk[IMX6QDL_CLK_HDMI_ISFR]    = imx_clk_gate2("hdmi_isfr",     "pll3_pfd1_540m",    base + 0x70, 4);
	clk[IMX6QDL_CLK_I2C1]         = imx_clk_gate2("i2c1",          "ipg_per",           base + 0x70, 6);
	clk[IMX6QDL_CLK_I2C2]         = imx_clk_gate2("i2c2",          "ipg_per",           base + 0x70, 8);
	clk[IMX6QDL_CLK_I2C3]         = imx_clk_gate2("i2c3",          "ipg_per",           base + 0x70, 10);
	clk[IMX6QDL_CLK_IIM]          = imx_clk_gate2("iim",           "ipg",               base + 0x70, 12);
	clk[IMX6QDL_CLK_ENFC]         = imx_clk_gate2("enfc",          "enfc_podf",         base + 0x70, 14);
	clk[IMX6QDL_CLK_VDOA]         = imx_clk_gate2("vdoa",          "vdo_axi",           base + 0x70, 26);
	clk[IMX6QDL_CLK_IPU1]         = imx_clk_gate2("ipu1",          "ipu1_podf",         base + 0x74, 0);
	clk[IMX6QDL_CLK_IPU1_DI0]     = imx_clk_gate2("ipu1_di0",      "ipu1_di0_sel",      base + 0x74, 2);
	clk[IMX6QDL_CLK_IPU1_DI1]     = imx_clk_gate2("ipu1_di1",      "ipu1_di1_sel",      base + 0x74, 4);
	clk[IMX6QDL_CLK_IPU2]         = imx_clk_gate2("ipu2",          "ipu2_podf",         base + 0x74, 6);
	clk[IMX6QDL_CLK_IPU2_DI0]     = imx_clk_gate2("ipu2_di0",      "ipu2_di0_sel",      base + 0x74, 8);
	clk[IMX6QDL_CLK_LDB_DI0]      = imx_clk_gate2("ldb_di0",       "ldb_di0_podf",      base + 0x74, 12);
	clk[IMX6QDL_CLK_LDB_DI1]      = imx_clk_gate2("ldb_di1",       "ldb_di1_podf",      base + 0x74, 14);
	clk[IMX6QDL_CLK_IPU2_DI1]     = imx_clk_gate2("ipu2_di1",      "ipu2_di1_sel",      base + 0x74, 10);
	clk[IMX6QDL_CLK_HSI_TX]       = imx_clk_gate2("hsi_tx",        "hsi_tx_podf",       base + 0x74, 16);
	if (cpu_is_imx6dl())
		/*
		 * The multiplexer and divider of the imx6q clock gpu2d get
		 * redefined/reused as mlb_sys_sel and mlb_sys_clk_podf on imx6dl.
		 */
		clk[IMX6QDL_CLK_MLB] = imx_clk_gate2("mlb",            "gpu2d_core_podf",   base + 0x74, 18);
	else
		clk[IMX6QDL_CLK_MLB] = imx_clk_gate2("mlb",            "axi",               base + 0x74, 18);
	clk[IMX6QDL_CLK_MMDC_CH0_AXI] = imx_clk_gate2("mmdc_ch0_axi",  "mmdc_ch0_axi_podf", base + 0x74, 20);
	clk[IMX6QDL_CLK_MMDC_CH1_AXI] = imx_clk_gate2("mmdc_ch1_axi",  "mmdc_ch1_axi_podf", base + 0x74, 22);
	clk[IMX6QDL_CLK_OCRAM]        = imx_clk_gate2("ocram",         "ahb",               base + 0x74, 28);
	clk[IMX6QDL_CLK_OPENVG_AXI]   = imx_clk_gate2("openvg_axi",    "axi",               base + 0x74, 30);
	clk[IMX6QDL_CLK_PCIE_AXI]     = imx_clk_gate2("pcie_axi",      "pcie_axi_sel",      base + 0x78, 0);
	clk[IMX6QDL_CLK_PER1_BCH]     = imx_clk_gate2("per1_bch",      "usdhc3",            base + 0x78, 12);
	clk[IMX6QDL_CLK_PWM1]         = imx_clk_gate2("pwm1",          "ipg_per",           base + 0x78, 16);
	clk[IMX6QDL_CLK_PWM2]         = imx_clk_gate2("pwm2",          "ipg_per",           base + 0x78, 18);
	clk[IMX6QDL_CLK_PWM3]         = imx_clk_gate2("pwm3",          "ipg_per",           base + 0x78, 20);
	clk[IMX6QDL_CLK_PWM4]         = imx_clk_gate2("pwm4",          "ipg_per",           base + 0x78, 22);
	clk[IMX6QDL_CLK_GPMI_BCH_APB] = imx_clk_gate2("gpmi_bch_apb",  "usdhc3",            base + 0x78, 24);
	clk[IMX6QDL_CLK_GPMI_BCH]     = imx_clk_gate2("gpmi_bch",      "usdhc4",            base + 0x78, 26);
	clk[IMX6QDL_CLK_GPMI_IO]      = imx_clk_gate2("gpmi_io",       "enfc",              base + 0x78, 28);
	clk[IMX6QDL_CLK_GPMI_APB]     = imx_clk_gate2("gpmi_apb",      "usdhc3",            base + 0x78, 30);
	clk[IMX6QDL_CLK_ROM]          = imx_clk_gate2("rom",           "ahb",               base + 0x7c, 0);
	clk[IMX6QDL_CLK_SATA]         = imx_clk_gate2("sata",          "ipg",               base + 0x7c, 4);
	clk[IMX6QDL_CLK_SDMA]         = imx_clk_gate2("sdma",          "ahb",               base + 0x7c, 6);
	clk[IMX6QDL_CLK_SPBA]         = imx_clk_gate2("spba",          "ipg",               base + 0x7c, 12);
	clk[IMX6QDL_CLK_SPDIF]        = imx_clk_gate2("spdif",         "spdif_podf",    	base + 0x7c, 14);
	clk[IMX6QDL_CLK_SSI1_IPG]     = imx_clk_gate2("ssi1_ipg",      "ipg",        base + 0x7c, 18);
	clk[IMX6QDL_CLK_SSI2_IPG]     = imx_clk_gate2("ssi2_ipg",      "ipg",        base + 0x7c, 20);
	clk[IMX6QDL_CLK_SSI3_IPG]     = imx_clk_gate2("ssi3_ipg",      "ipg",        base + 0x7c, 22);
	clk[IMX6QDL_CLK_UART_IPG]     = imx_clk_gate2("uart_ipg",      "ipg",               base + 0x7c, 24);
	clk[IMX6QDL_CLK_UART_SERIAL]  = imx_clk_gate2("uart_serial",   "uart_serial_podf",  base + 0x7c, 26);
	clk[IMX6QDL_CLK_USBOH3]       = imx_clk_gate2("usboh3",        "ipg",               base + 0x80, 0);
	clk[IMX6QDL_CLK_USDHC1]       = imx_clk_gate2("usdhc1",        "usdhc1_podf",       base + 0x80, 2);
	clk[IMX6QDL_CLK_USDHC2]       = imx_clk_gate2("usdhc2",        "usdhc2_podf",       base + 0x80, 4);
	clk[IMX6QDL_CLK_USDHC3]       = imx_clk_gate2("usdhc3",        "usdhc3_podf",       base + 0x80, 6);
	clk[IMX6QDL_CLK_USDHC4]       = imx_clk_gate2("usdhc4",        "usdhc4_podf",       base + 0x80, 8);
	clk[IMX6QDL_CLK_EIM_SLOW]     = imx_clk_gate2("eim_slow",      "emi_slow_podf",     base + 0x80, 10);
	clk[IMX6QDL_CLK_VDO_AXI]      = imx_clk_gate2("vdo_axi",       "vdo_axi_sel",       base + 0x80, 12);
	clk[IMX6QDL_CLK_VPU_AXI]      = imx_clk_gate2("vpu_axi",       "vpu_axi_podf",      base + 0x80, 14);
	clk[IMX6QDL_CLK_CKO1]         = imx_clk_gate("cko1",           "cko1_podf",         base + 0x60, 7);
	clk[IMX6QDL_CLK_CKO2]         = imx_clk_gate("cko2",           "cko2_podf",         base + 0x60, 24);

	for (i = 0; i < ARRAY_SIZE(clk); i++)
		if (IS_ERR(clk[i]))
			pr_err("i.MX6q clk %d: register failed with %l\n",
				i, PTR_ERR(clk[i]));

	clk_data.clks = clk;
	clk_data.clk_num = ARRAY_SIZE(clk);
	of_clk_add_provider(np, of_clk_src_onecell_get, &clk_data);

	clk_register_clkdev(clk[IMX6QDL_CLK_GPT_IPG], "ipg", "imx-gpt.0");
	clk_register_clkdev(clk[IMX6QDL_CLK_GPT_IPG_PER], "per", "imx-gpt.0");
	clk_register_clkdev(clk[IMX6QDL_CLK_CKO1_SEL], "cko1_sel", NULL);
	clk_register_clkdev(clk[IMX6QDL_CLK_AHB], "ahb", NULL);
	clk_register_clkdev(clk[IMX6QDL_CLK_CKO1], "cko1", NULL);
	clk_register_clkdev(clk[IMX6QDL_CLK_ARM], NULL, "cpu0");
	clk_register_clkdev(clk[IMX6QDL_CLK_PLL4_POST_DIV], "pll4_post_div", NULL);
	clk_register_clkdev(clk[IMX6QDL_CLK_PLL4_AUDIO], "pll4_audio", NULL);

	if ((imx_get_soc_revision() != IMX_CHIP_REVISION_1_0) ||
	    cpu_is_imx6dl()) {
		clk_set_parent(clk[IMX6QDL_CLK_LDB_DI0_SEL], clk[IMX6QDL_CLK_PLL5_VIDEO_DIV]);
		clk_set_parent(clk[IMX6QDL_CLK_LDB_DI1_SEL], clk[IMX6QDL_CLK_PLL5_VIDEO_DIV]);
	}

	/*
	 * The gpmi needs 100MHz frequency in the EDO/Sync mode,
	 * We can not get the 100MHz from the pll2_pfd0_352m.
	 * So choose pll2_pfd2_396m as enfc_sel's parent.
	 */
	clk_set_parent(clk[IMX6QDL_CLK_ENFC_SEL], clk[IMX6QDL_CLK_PLL2_PFD2_396M]);

	for (i = 0; i < ARRAY_SIZE(clks_init_on); i++)
		clk_prepare_enable(clk[clks_init_on[i]]);

	if (IS_ENABLED(CONFIG_USB_MXS_PHY)) {
		clk_prepare_enable(clk[IMX6QDL_CLK_USBPHY1_GATE]);
		clk_prepare_enable(clk[IMX6QDL_CLK_USBPHY2_GATE]);
	}

	/*
	 * Let's initially set up CLKO with OSC24M, since this configuration
	 * is widely used by imx6q board designs to clock audio codec.
	 */
	ret = clk_set_parent(clk[IMX6QDL_CLK_CKO2_SEL], clk[IMX6QDL_CLK_OSC]);
	if (!ret)
		ret = clk_set_parent(clk[IMX6QDL_CLK_CKO], clk[IMX6QDL_CLK_CKO2]);
	if (ret)
		pr_warn("failed to set up CLKO: %d\n", ret);

	/* All existing boards with PCIe use LVDS1 */
	if (IS_ENABLED(CONFIG_PCI_IMX6))
		clk_set_parent(clk[IMX6QDL_CLK_LVDS1_SEL], clk[IMX6QDL_CLK_SATA_REF_100M]);

	/* Set initial power mode */
	imx6q_set_lpm(WAIT_CLOCKED);
}
CLK_OF_DECLARE(imx6q, "fsl,imx6q-ccm", imx6q_clocks_init);

struct clk* imx_clk_get(unsigned int clkid)
{
	return clk[clkid];
}
