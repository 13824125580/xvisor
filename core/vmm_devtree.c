/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_devtree.c
 * @version 0.1
 * @author Anup Patel (anup@brainfault.org)
 * @brief Device Tree Implementation.
 */

#include <arch_board.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_heap.h>
#include <vmm_devtree.h>

struct vmm_devtree_ctrl {
        struct vmm_devtree_node *root;
};

static struct vmm_devtree_ctrl dtree_ctrl;

const char *vmm_devtree_attrval(struct vmm_devtree_node * node, 
				const char *attrib)
{
	struct vmm_devtree_attr *attr;
	struct dlist *l;

	if (!node) {
		return NULL;
	}

	list_for_each(l, &node->attr_list) {
		attr = list_entry(l, struct vmm_devtree_attr, head);
		if (vmm_strcmp(attr->name, attrib) == 0) {
			return attr->value;
		}
	}

	return NULL;
}

u32 vmm_devtree_attrlen(struct vmm_devtree_node * node, const char *attrib)
{
	struct vmm_devtree_attr *attr;
	struct dlist *l;

	if (!node) {
		return 0;
	}

	list_for_each(l, &node->attr_list) {
		attr = list_entry(l, struct vmm_devtree_attr, head);
		if (vmm_strcmp(attr->name, attrib) == 0) {
			return attr->len;
		}
	}

	return 0;
}

struct vmm_devtree_attr * vmm_devtree_addattr(struct vmm_devtree_node * node,
					      const char *name,
					      char * value,
					      u32 len)
{
	struct dlist *l;
	struct vmm_devtree_attr * attr;

	if (!node || !name || !value) {
		return NULL;
	}

	list_for_each(l, &node->attr_list) {
		attr = list_entry(l, struct vmm_devtree_attr, head);
		if (vmm_strcmp(attr->name, name) == 0) {
			return NULL;
		}
	}

	attr = vmm_malloc(sizeof(struct vmm_devtree_attr));
	INIT_LIST_HEAD(&attr->head);
	attr->len = len;
	len = vmm_strlen(name) + 1;
	attr->name = vmm_malloc(len);
	vmm_strncpy(attr->name, name, len);
	attr->value = vmm_malloc(attr->len);
	vmm_memcpy(attr->value, value, attr->len);
	list_add_tail(&node->attr_list, &attr->head);

	return attr;
}

int vmm_devtree_setattr(struct vmm_devtree_node * node,
			const char *name,
			char * new_value,
			u32 new_len)
{
	bool found;
	struct dlist *l;
	struct vmm_devtree_attr * attr;

	if (!node || !name || !new_value) {
		return VMM_EFAIL;
	}

	found = FALSE;
	list_for_each(l, &node->attr_list) {
		attr = list_entry(l, struct vmm_devtree_attr, head);
		if (vmm_strcmp(attr->name, name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return VMM_EFAIL;
	}


	if (attr->len != new_len) {
		attr->len = new_len;
		vmm_free(attr->value);
		attr->value = vmm_malloc(attr->len);
	}

	vmm_memcpy(attr->value, new_value, attr->len);

	return VMM_OK;
}

int vmm_devtree_delattr(struct vmm_devtree_node * node, const char *name)
{
	bool found;
	struct dlist *l;
	struct vmm_devtree_attr * attr;

	if (!node || !name) {
		return VMM_EFAIL;
	}

	found = FALSE;
	list_for_each(l, &node->attr_list) {
		attr = list_entry(l, struct vmm_devtree_attr, head);
		if (vmm_strcmp(attr->name, name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return VMM_EFAIL;
	}

	vmm_free(attr->name);
	vmm_free(attr->value);
	list_del(&attr->head);
	vmm_free(attr);

	return VMM_OK;
}

void recursive_getpath(char **out, struct vmm_devtree_node * node)
{
	if (!node)
		return;

	if (node->parent) {
		recursive_getpath(out, node->parent);
		**out = VMM_DEVTREE_PATH_SEPARATOR;
		(*out) += 1;
		**out = '\0';
	}

	vmm_strcat(*out, node->name);
	(*out) += vmm_strlen(node->name);
}

int vmm_devtree_getpath(char *out, struct vmm_devtree_node * node)
{
	char *out_ptr = out;

	if (!node)
		return VMM_EFAIL;

	vmm_strcpy(out, "");

	recursive_getpath(&out_ptr, node);

	if (vmm_strcmp(out, "") == 0) {
		out[0] = VMM_DEVTREE_PATH_SEPARATOR;
		out[1] = '\0';
	}

	return VMM_OK;
}

struct vmm_devtree_node *vmm_devtree_getchild(struct vmm_devtree_node * node,
					      const char *path)
{
	bool found;
	struct dlist *lentry;
	struct vmm_devtree_node *child;

	if (!path || !node)
		return NULL;

	while (*path) {
		found = FALSE;
		list_for_each(lentry, &node->child_list) {
			child = list_entry(lentry, struct vmm_devtree_node, head);
			if (vmm_strncmp(child->name, path,
					vmm_strlen(child->name)) == 0) {
				found = TRUE;
				path += vmm_strlen(child->name);
				if (*path) {
					if (*path != VMM_DEVTREE_PATH_SEPARATOR
					    && *(path + 1) != '\0')
						return NULL;
					if (*path == VMM_DEVTREE_PATH_SEPARATOR)
						path++;
				}
				break;
			}
		}
		if (!found)
			return NULL;
		node = child;
	};

	return node;
}

struct vmm_devtree_node *vmm_devtree_getnode(const char *path)
{
	struct vmm_devtree_node *node = dtree_ctrl.root;

	if (!path || !node)
		return NULL;

	if (vmm_strncmp(node->name, path, vmm_strlen(node->name)) != 0)
		return NULL;

	path += vmm_strlen(node->name);

	if (*path) {
		if (*path != VMM_DEVTREE_PATH_SEPARATOR && *(path + 1) != '\0')
			return NULL;
		if (*path == VMM_DEVTREE_PATH_SEPARATOR)
			path++;
	}

	return vmm_devtree_getchild(node, path);
}

struct vmm_devtree_node *vmm_devtree_addnode(struct vmm_devtree_node * parent,
					     const char * name,
					     u32 type,
					     void * priv)
{
	u32 len;
	struct dlist *l;
	struct vmm_devtree_node * node = NULL;

	if (!name) {
		return NULL;
	}
	if (parent) {
		list_for_each(l, &parent->child_list) {
			node = list_entry(l, struct vmm_devtree_node, head);
			if (vmm_strcmp(node->name, name) == 0) {
				return NULL;
			}
		}
	}

	node = vmm_malloc(sizeof(struct vmm_devtree_node));
	INIT_LIST_HEAD(&node->head);
	INIT_LIST_HEAD(&node->attr_list);
	INIT_LIST_HEAD(&node->child_list);
	if (name) {
		len = vmm_strlen(name) + 1;
		node->name = vmm_malloc(len);
		vmm_strncpy(node->name, name, len);
	} else {
		node->name = NULL;
	}
	node->type = type;
	node->priv = priv;
	node->parent = parent;
	if (parent) {
		list_add_tail(&parent->child_list, &node->head);
	}

	return node;
}

int vmm_devtree_delnode(struct vmm_devtree_node * node)
{
	int rc;
	struct dlist *l;
	struct vmm_devtree_attr * attr;
	struct vmm_devtree_node * child;

	if (!node) {
		return VMM_EFAIL;
	}

	while(!list_empty(&node->attr_list)) {
		l = list_first(&node->attr_list);
		attr = list_entry(l, struct vmm_devtree_attr, head);
		if ((rc = vmm_devtree_delattr(node, attr->name))) {
			return rc;
		}
	}

	while(!list_empty(&node->child_list)) {
		l = list_first(&node->child_list);
		child = list_entry(l, struct vmm_devtree_node, head);
		if ((rc = vmm_devtree_delnode(child))) {
			return rc;
		}
	}

	list_del(&node->head);

	vmm_free(node);

	return VMM_OK;
}

struct vmm_devtree_node *vmm_devtree_rootnode(void)
{
	return dtree_ctrl.root;
}

int __init vmm_devtree_init(void)
{
	/* Reset the control structure */
	vmm_memset(&dtree_ctrl, 0, sizeof(dtree_ctrl));

	/* Populate Device Tree */
	return arch_devtree_populate(&dtree_ctrl.root);
}
