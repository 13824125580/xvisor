/**
 * Copyright (c) 2011 Anup Patel.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file brd_main.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief main source file for board specific code
 */

#include <vmm_error.h>
#include <vmm_main.h>
#include <vmm_smp.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <arch_barrier.h>
#include <arch_board.h>
#include <arch_timer.h>
#include <libs/stringlib.h>
#include <libs/vtemu.h>
#include <drv/clk-provider.h>
#include <drv/platform_data/clk-realview.h>

#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>

#include <realview_plat.h>

#include <smp_twd.h>
#include <versatile/clcd.h>

/*
 * Global board context
 */

static virtual_addr_t realview_sys_base;
static virtual_addr_t realview_sys_24mhz;

#if defined(CONFIG_VTEMU)
struct vtemu *realview_vt;
#endif

void realview_flags_set(u32 addr)
{
	vmm_writel(~0x0, (void *)(realview_sys_base + 
				  REALVIEW_SYS_FLAGSCLR_OFFSET));
	vmm_writel(addr, (void *)(realview_sys_base + 
				  REALVIEW_SYS_FLAGSSET_OFFSET));

	arch_mb();
}

/*
 * Reset & Shutdown
 */

static int realview_reset(void)
{
	u32 board_id;
	void *sys_id = (void *)realview_sys_base + REALVIEW_SYS_ID_OFFSET;
	void *sys_lock = (void *)realview_sys_base + REALVIEW_SYS_LOCK_OFFSET;
	void *sys_resetctl = (void *)realview_sys_base + 
						REALVIEW_SYS_RESETCTL_OFFSET;

	board_id = (vmm_readl(sys_id) & REALVIEW_SYS_ID_BOARD_MASK) >> 
						REALVIEW_SYS_ID_BOARD_SHIFT;

	vmm_writel(REALVIEW_SYS_LOCKVAL, sys_lock);

	switch (board_id) {
	case REALVIEW_SYS_ID_EB:
		vmm_writel(0x0, sys_resetctl);
		vmm_writel(0x08, sys_resetctl);
		break;
	case REALVIEW_SYS_ID_PBA8:
		vmm_writel(0x0, sys_resetctl);
		vmm_writel(0x04, sys_resetctl);
		break;
	default:
		break;
	};

	vmm_writel(0, sys_lock);

	return VMM_OK;
}

static int realview_shutdown(void)
{
	/* FIXME: TBD */
	return VMM_OK;
}

/*
 * CLCD support.
 */

#define SYS_CLCD_NLCDIOON	(1 << 2)
#define SYS_CLCD_VDDPOSSWITCH	(1 << 3)
#define SYS_CLCD_PWR3V5SWITCH	(1 << 4)
#define SYS_CLCD_ID_MASK	(0x1f << 8)
#define SYS_CLCD_ID_SANYO_3_8	(0x00 << 8)
#define SYS_CLCD_ID_UNKNOWN_8_4	(0x01 << 8)
#define SYS_CLCD_ID_EPSON_2_2	(0x02 << 8)
#define SYS_CLCD_ID_SANYO_2_5	(0x07 << 8)
#define SYS_CLCD_ID_VGA		(0x1f << 8)

/*
 * Disable all display connectors on the interface module.
 */
static void realview_clcd_disable(struct clcd_fb *fb)
{
	void *sys_clcd = (void *)realview_sys_base + REALVIEW_SYS_CLCD_OFFSET;
	u32 val;

	val = vmm_readl(sys_clcd);
	val &= ~SYS_CLCD_NLCDIOON | SYS_CLCD_PWR3V5SWITCH;
	vmm_writel(val, sys_clcd);
}

/*
 * Enable the relevant connector on the interface module.
 */
static void realview_clcd_enable(struct clcd_fb *fb)
{
	void *sys_clcd = (void *)realview_sys_base + REALVIEW_SYS_CLCD_OFFSET;
	u32 val;

	/*
	 * Enable the PSUs
	 */
	val = vmm_readl(sys_clcd);
	val |= SYS_CLCD_NLCDIOON | SYS_CLCD_PWR3V5SWITCH;
	vmm_writel(val, sys_clcd);
}

/*
 * Detect which LCD panel is connected, and return the appropriate
 * clcd_panel structure.  Note: we do not have any information on
 * the required timings for the 8.4in panel, so we presently assume
 * VGA timings.
 */
static int realview_clcd_setup(struct clcd_fb *fb)
{
	void *sys_clcd = (void *)realview_sys_base + REALVIEW_SYS_CLCD_OFFSET;
	const char *panel_name, *vga_panel_name;
	unsigned long framesize;
	u32 val;

	/* XVGA, 16bpp 
	 * (Assuming machine is always realview-pb-a8 and not realview-eb)
	 */
	framesize = 1024 * 768 * 2;
	vga_panel_name = "XVGA";

	val = vmm_readl(sys_clcd) & SYS_CLCD_ID_MASK;
	if (val == SYS_CLCD_ID_SANYO_3_8)
		panel_name = "Sanyo TM38QV67A02A";
	else if (val == SYS_CLCD_ID_SANYO_2_5)
		panel_name = "Sanyo QVGA Portrait";
	else if (val == SYS_CLCD_ID_EPSON_2_2)
		panel_name = "Epson L2F50113T00";
	else if (val == SYS_CLCD_ID_VGA)
		panel_name = vga_panel_name;
	else {
		vmm_printf("CLCD: unknown LCD panel ID 0x%08x, using VGA\n", val);
		panel_name = vga_panel_name;
	}

	fb->panel = versatile_clcd_get_panel(panel_name);
	if (!fb->panel)
		return VMM_EINVALID;

	return versatile_clcd_setup(fb, framesize);
}

struct clcd_board clcd_system_data = {
	.name		= "Realview",
	.caps		= CLCD_CAP_ALL,
	.check		= clcdfb_check,
	.decode		= clcdfb_decode,
	.disable	= realview_clcd_disable,
	.enable		= realview_clcd_enable,
	.setup		= realview_clcd_setup,
	.remove		= versatile_clcd_remove,
};

/*
 * Print board information
 */

void arch_board_print_info(struct vmm_chardev *cdev)
{
	/* FIXME: To be implemented. */
}

/*
 * Initialization functions
 */

int __init arch_board_early_init(void)
{
	int rc;
	struct vmm_devtree_node *node;

	/* Host aspace, Heap, Device tree, and Host IRQ available.
	 *
	 * Do necessary early stuff like:
	 * iomapping devices, 
	 * SOC clocking init, 
	 * Setting-up system data in device tree nodes,
	 * ....
	 */

	/* Map sysreg */
	node = vmm_devtree_find_compatible(NULL, NULL, "arm,realview-sysreg");
	if (!node) {
		return VMM_ENODEV;
	}
	rc = vmm_devtree_regmap(node, &realview_sys_base, 0);
	if (rc) {
		return rc;
	}

	/* Register reset & shutdown callbacks */
	vmm_register_system_reset(realview_reset);
	vmm_register_system_shutdown(realview_shutdown);

	/* Get address of 24mhz counter */
	realview_sys_24mhz = realview_sys_base + REALVIEW_SYS_24MHz_OFFSET;

	/* Intialize realview clocking */
	of_clk_init(NULL);
	realview_clk_init((void *)realview_sys_base, FALSE);

	/* Setup CLCD (before probing) */
	node = vmm_devtree_find_compatible(NULL, NULL, "arm,pl111");
	if (node) {
		node->system_data = &clcd_system_data;
	}

	return VMM_OK;
}

int __cpuinit arch_clockchip_init(void)
{
#if defined(CONFIG_ARM_TWD)
	int rc;

	/* Initialize SMP twd local timer as clockchip */
	rc = twd_clockchip_init(realview_sys_24mhz, 24000000);
	if (rc) {
		vmm_printf("%s: local timer init failed (error %d)\n", 
			   __func__, rc);
	}
#endif

	return VMM_OK;
}

int __init arch_board_final_init(void)
{
	int rc;
	struct vmm_devtree_node *node;
#if defined(CONFIG_VTEMU)
	struct vmm_fb_info *info;
#endif

	/* All VMM API's are available here */
	/* We can register a Board specific resource here */

	/* Find simple-bus node */
	node = vmm_devtree_find_compatible(NULL, NULL, "simple-bus");
	if (!node) {
		return VMM_ENODEV;
	}

	/* Do probing using device driver framework */
	rc = vmm_devdrv_probe(node);
	if (rc) {
		return rc;
	}

	/* Create VTEMU instace if available*/
#if defined(CONFIG_VTEMU)
	node = vmm_devtree_find_compatible(NULL, NULL, "arm,pl111");
	if (!node) {
		return VMM_ENODEV;
	}
	info = vmm_fb_find(node->name);
	if (info) {
		realview_vt = vtemu_create(node->name, info, NULL);
	}
#endif

	return VMM_OK;
}
