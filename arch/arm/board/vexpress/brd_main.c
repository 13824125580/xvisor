/**
 * Copyright (c) 2012 Anup Patel.
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
#include <vmm_spinlocks.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_delay.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <arch_board.h>
#include <arch_timer.h>
#include <libs/vtemu.h>
#include <drv/clk-provider.h>
#include <drv/vexpress.h>

#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>

#include <gic.h>
#include <sp810.h>
#include <sp804_timer.h>
#include <smp_twd.h>
#include <versatile/clcd.h>

/*
 * Global board context
 */

static struct vexpress_config_func *reboot_func;
static struct vexpress_config_func *shutdown_func;
static struct vexpress_config_func *muxfpga_func;
static struct vexpress_config_func *dvimode_func;
static virtual_addr_t v2m_sp804_base;
static u32 v2m_sp804_irq;

#if defined(CONFIG_VTEMU)
struct vtemu *v2m_vt;
#endif

/*
 * Reset & Shutdown
 */

static int v2m_reset(void)
{
	int err = VMM_EFAIL;
	if (reboot_func) {
		err = vexpress_config_write(reboot_func, 0, 0);
		vmm_mdelay(1000);
	}
	return err;
}

static int v2m_shutdown(void)
{
	int err = VMM_EFAIL;
	if (shutdown_func) {
		err = vexpress_config_write(shutdown_func, 0, 0);
		vmm_mdelay(1000);
	}
	return err;
}

/*
 * CLCD support.
 */

static void vexpress_clcd_enable(struct clcd_fb *fb)
{
	vexpress_config_write(muxfpga_func, 0, 0);
	vexpress_config_write(dvimode_func, 0, 2);
}

static int vexpress_clcd_setup(struct clcd_fb *fb)
{
	unsigned long framesize = 1024 * 768 * 2;

	fb->panel = versatile_clcd_get_panel("XVGA");
	if (!fb->panel)
		return VMM_EINVALID;

	return versatile_clcd_setup(fb, framesize);
}

static struct clcd_board clcd_system_data = {
	.name		= "VExpress",
	.caps		= CLCD_CAP_5551 | CLCD_CAP_565,
	.check		= clcdfb_check,
	.decode		= clcdfb_decode,
	.enable		= vexpress_clcd_enable,
	.setup		= vexpress_clcd_setup,
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

int __cpuinit arch_host_irq_init(void)
{
	int rc;
	u32 cpu = vmm_smp_processor_id();
	struct vmm_devtree_node *node;

	if (!cpu) {
		node = vmm_devtree_find_compatible(NULL, NULL, 
						   "arm,cortex-a9-gic");
		if (!node) {
			return VMM_ENODEV;
		}

		rc = gic_devtree_init(node, NULL);
	} else {
		gic_secondary_init(0);
		rc = VMM_OK;
	}

	return rc;
}

int __init arch_board_early_init(void)
{
	int rc;
	u32 val;
	virtual_addr_t v2m_sctl_base;
	struct vmm_devtree_node *node;

	/* Host aspace, Heap, Device tree, and Host IRQ available.
	 *
	 * Do necessary early stuff like:
	 * iomapping devices, 
	 * SOC clocking init, 
	 * Setting-up system data in device tree nodes,
	 * ....
	 */

	/* Sysreg early init */
	vexpress_sysreg_of_early_init();

	/* Initialize clocking framework */
	of_clk_init(NULL);

	/* Determine reboot function */
	node = vmm_devtree_find_compatible(NULL, NULL, "arm,vexpress-reboot");
	if (!node) {
		return VMM_ENODEV;
	}
	reboot_func = vexpress_config_func_get_by_node(node);
	if (!reboot_func) {
		return VMM_ENODEV;
	}

	/* Determine shutdown function */
	node = vmm_devtree_find_compatible(NULL, NULL, "arm,vexpress-shutdown");
	if (!node) {
		return VMM_ENODEV;
	}
	shutdown_func = vexpress_config_func_get_by_node(node);
	if (!shutdown_func) {
		return VMM_ENODEV;
	}

	/* Determine muxfpga function */
	node = vmm_devtree_find_compatible(NULL, NULL, "arm,vexpress-muxfpga");
	if (!node) {
		return VMM_ENODEV;
	}
	muxfpga_func = vexpress_config_func_get_by_node(node);
	if (!muxfpga_func) {
		return VMM_ENODEV;
	}

	/* Determine dvimode function */
	node = vmm_devtree_find_compatible(NULL, NULL, "arm,vexpress-dvimode");
	if (!node) {
		return VMM_ENODEV;
	}
	dvimode_func = vexpress_config_func_get_by_node(node);
	if (!dvimode_func) {
		return VMM_ENODEV;
	}

	/* Register reset & shutdown callbacks */
	vmm_register_system_reset(v2m_reset);
	vmm_register_system_shutdown(v2m_shutdown);

	/* Map sysctl */
	node = vmm_devtree_find_compatible(NULL, NULL, "arm,sp810");
	if (!node) {
		return VMM_ENODEV;
	}
	rc = vmm_devtree_regmap(node, &v2m_sctl_base, 0);
	if (rc) {
		return rc;
	}

	/* Select reference clock for sp804 timers: 
	 *      REFCLK is 32KHz
	 *      TIMCLK is 1MHz
	 */
	val = vmm_readl((void *)v2m_sctl_base) | 
				SCCTRL_TIMEREN0SEL_TIMCLK |
				SCCTRL_TIMEREN1SEL_TIMCLK;
	vmm_writel(val, (void *)v2m_sctl_base);

	/* Unmap sysctl */
	rc = vmm_devtree_regunmap(node, v2m_sctl_base, 0);
	if (rc) {
		return rc;
	}

	/* Map sp804 registers */
	node = vmm_devtree_find_compatible(NULL, NULL, "arm,sp804");
	if (!node) {
		return VMM_ENODEV;
	}
	rc = vmm_devtree_regmap(node, &v2m_sp804_base, 0);
	if (rc) {
		return rc;
	}

	/* Get sp804 irq */
	rc = vmm_devtree_irq_get(node, &v2m_sp804_irq, 0);
	if (rc) {
		return rc;
	}

	/* Setup CLCD (before probing) */
	node = vmm_devtree_find_compatible(NULL, NULL, "arm,pl111");
	if (node) {
		node->system_data = &clcd_system_data;
	}

	return 0;
}

int __init arch_clocksource_init(void)
{
	int rc;

	/* Initialize sp804 timer0 as clocksource */
	rc = sp804_clocksource_init(v2m_sp804_base, 
				    "sp804_timer0", 1000000);
	if (rc) {
		vmm_printf("%s: sp804 clocksource init failed (error %d)\n", 
			   __func__, rc);
	}

	return VMM_OK;
}

int __cpuinit arch_clockchip_init(void)
{
	int rc;
	u32 cpu = vmm_smp_processor_id();

	if (!cpu) {
		/* Initialize sp804 timer1 as clockchip */
		rc = sp804_clockchip_init(v2m_sp804_base + 0x20, 
					  "sp804_timer1", v2m_sp804_irq, 
					  1000000, 0);
		if (rc) {
			vmm_printf("%s: sp804 clockchip init failed "
				   "(error %d)\n", __func__, rc);
		}
	}

#if defined(CONFIG_ARM_TWD)
	/* Initialize SMP twd local timer as clockchip */
	rc = twd_clockchip_init((virtual_addr_t)vexpress_get_24mhz_clock_base(),
				24000000);
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
		v2m_vt = vtemu_create(node->name, info, NULL);
	}
#endif

	return VMM_OK;
}
