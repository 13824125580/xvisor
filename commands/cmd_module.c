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
 * @file cmd_module.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of module command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_devtree.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>

#define MODULE_NAME			"Command module"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_module_init
#define	MODULE_EXIT			cmd_module_exit

void cmd_module_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   module help\n");
	vmm_cprintf(cdev, "   module list\n");
	vmm_cprintf(cdev, "   module info <index>\n");
	vmm_cprintf(cdev, "   module unload <index>\n");
}

void cmd_module_list(struct vmm_chardev *cdev)
{
	int num, count;
	struct vmm_module *mod;
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_cprintf(cdev, " %-5s %-40s %-20s %-12s\n", 
			  "Num", "Name", "Author", "Type");
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	count = vmm_modules_count();
	for (num = 0; num < count; num++) {
		mod = vmm_modules_getmodule(num);
		vmm_cprintf(cdev, " %-5d %-40s %-20s %-12s\n", 
				  num, mod->name, mod->author, 
				  vmm_modules_isbuiltin(mod) ? 
				  "built-in" : "loadable");
	}
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_cprintf(cdev, "Total %d modules\n", count);
}

int cmd_module_info(struct vmm_chardev *cdev, u32 index)
{
	struct vmm_module *mod;

	mod = vmm_modules_getmodule(index);
	if (!mod) {
		return VMM_EFAIL;
	}

	vmm_cprintf(cdev, "Name:      %s\n", mod->name);
	vmm_cprintf(cdev, "Author:    %s\n", mod->author);
	vmm_cprintf(cdev, "iPriority: %d\n", mod->ipriority);
	vmm_cprintf(cdev, "iStatus:   %d\n", mod->istatus);
	vmm_cprintf(cdev, "Type:      %s\n", vmm_modules_isbuiltin(mod) ? 
					 "built-in" : "loadable");

	return VMM_OK;
}

int cmd_module_unload(struct vmm_chardev *cdev, u32 index)
{
	int rc = VMM_OK;
	struct vmm_module *mod;

	mod = vmm_modules_getmodule(index);
	if (!mod) {
		return VMM_EFAIL;
	}

	if (vmm_modules_isbuiltin(mod)) {
		vmm_cprintf(cdev, "Can't unload built-in module\n");
		return VMM_EFAIL;
	}

	if ((rc = vmm_modules_unload(mod))) {
		vmm_cprintf(cdev, "Failed to unload module (error %d)\n", rc);
	} else {
		vmm_cprintf(cdev, "Unloaded module succesfully\n");
	}

	return rc;
}

int cmd_module_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	int index;
	if (argc == 2) {
		if (vmm_strcmp(argv[1], "help") == 0) {
			cmd_module_usage(cdev);
			return VMM_OK;
		} else if (vmm_strcmp(argv[1], "list") == 0) {
			cmd_module_list(cdev);
			return VMM_OK;
		}
	}
	if (argc < 3) {
		cmd_module_usage(cdev);
		return VMM_EFAIL;
	}
	index = vmm_str2int(argv[2], 10);
	if (vmm_strcmp(argv[1], "info") == 0) {
		return cmd_module_info(cdev, index);
	} else if (vmm_strcmp(argv[1], "unload") == 0) {
		return cmd_module_unload(cdev, index);
	} else {
		cmd_module_usage(cdev);
		return VMM_EFAIL;
	}
	return VMM_OK;
}

static struct vmm_cmd cmd_module = {
	.name = "module",
	.desc = "module related commands",
	.usage = cmd_module_usage,
	.exec = cmd_module_exec,
};

static int __init cmd_module_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_module);
}

static void __exit cmd_module_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_module);
}

VMM_DECLARE_MODULE(MODULE_NAME, 
			MODULE_AUTHOR, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
