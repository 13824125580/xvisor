/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @file net_bridge.c
 * @author Sukanto Ghosh <sukantoghosh@gmail.com>
 * @brief Default packet switch implementation.
 */

#include <vmm_heap.h>
#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_devdrv.h>
#include <vmm_modules.h>
#include <vmm_spinlocks.h>
#include <vmm_threads.h>
#include <vmm_completion.h>
#include <list.h>
#include <net/vmm_protocol.h>
#include <net/vmm_mbuf.h>
#include <net/vmm_netswitch.h>
#include <net/vmm_netport.h>

#undef DEBUG_NETBRIDGE
#undef DUMP_NETBRIDGE_PKT

#ifdef DEBUG_NETBRIDGE
#define DPRINTF(fmt, ...) \
	do { vmm_printf(fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) do {} while(0)
#endif

#define VMM_NETBRIDGE_RX_BUFLEN		20

struct netbridge_xfer {
	struct dlist head;
	struct vmm_netport *src_port;
	struct vmm_mbuf *mbuf;
};

struct vmm_netbridge_ctrl {
	struct vmm_thread *thread;
	struct vmm_completion rx_not_empty;
	struct dlist free_list;
	vmm_spinlock_t free_list_lock;
	struct dlist rx_list;
	vmm_spinlock_t rx_list_lock;
	/* Pool of xfer elements used in the rx_buffer
	 * Having all these blocks contiguous eases alloc
	 * and free operations */
	struct netbridge_xfer *xfer_pool;
};

/**
 *  Thread body responsible for sending the RX buffer packets
 *  to the destination port(s)
 */
static int vmm_netbridge_tx(void *param)
{
	struct dlist *l;
	struct netbridge_xfer *xfer;
	bool broadcast;
	struct vmm_mbuf *mbuf;
	struct vmm_netswitch *nsw;
	const u8 *srcmac, *dstmac;
	unsigned long flags;
	struct vmm_netport *dst_port, *src_port;
	struct vmm_netbridge_ctrl *netbridge_ctrl;

	netbridge_ctrl = param;

	while(1) {
		vmm_spin_lock_irqsave(&netbridge_ctrl->rx_list_lock, flags);
		while(list_empty(&netbridge_ctrl->rx_list)) {
			vmm_spin_unlock_irqrestore(&netbridge_ctrl->rx_list_lock, flags);
			vmm_completion_wait(&netbridge_ctrl->rx_not_empty);
			vmm_spin_lock_irqsave(&netbridge_ctrl->rx_list_lock, flags);
		}
		l = list_pop(&netbridge_ctrl->rx_list);
		vmm_spin_unlock_irqrestore(&netbridge_ctrl->rx_list_lock, flags);

		xfer = list_entry(l, struct netbridge_xfer, head);

		mbuf = xfer->mbuf;
		src_port = xfer->src_port;

		/* Return the node back to free list */
		vmm_spin_lock_irqsave(&netbridge_ctrl->free_list_lock, flags);
		list_add_tail(&netbridge_ctrl->free_list, &xfer->head);
		vmm_spin_unlock_irqrestore(&netbridge_ctrl->free_list_lock, flags);

		broadcast = TRUE;

		nsw = src_port->nsw;
		srcmac = ether_srcmac(mtod(mbuf, u8 *));
		dstmac = ether_dstmac(mtod(mbuf, u8 *));

#ifdef DEBUG_NETBRIDGE
		char tname[30];

		DPRINTF("netbridge: got pkt with srcaddr[%s]", ethaddr_to_str(tname, srcmac));
		DPRINTF(", dstaddr[%s]", ethaddr_to_str(tname, dstmac));
		DPRINTF(", ethertype: 0x%04X\n", ether_type(mtod(mbuf, u8 *)));
#ifdef DUMP_NETBRIDGE_PKT
		if(ether_type(mtod(mbuf, u8 *)) == 0x0806	/* ARP */) {
			DPRINTF("\tARP-HType: 0x%04X\n", arp_htype(ether_payload(mtod(mbuf, u8 *))));
			DPRINTF("\tARP-PType: 0x%04X\n", arp_ptype(ether_payload(mtod(mbuf, u8 *))));
			DPRINTF("\tARP-Hlen: 0x%02X\n",  arp_hlen(ether_payload(mtod(mbuf, u8 *))));
			DPRINTF("\tARP-Plen: 0x%02X\n",  arp_plen(ether_payload(mtod(mbuf, u8 *))));
			DPRINTF("\tARP-Oper: 0x%04X\n",  arp_oper(ether_payload(mtod(mbuf, u8 *))));
			DPRINTF("\tARP-SHA: %s\n", ethaddr_to_str(tname, arp_sha(ether_payload((mtod(mbuf, u8 *))))));
			DPRINTF("\tARP-SPA: %s\n", ip4addr_to_str(tname, arp_spa(ether_payload((mtod(mbuf, u8 *))))));
			DPRINTF("\tARP-THA: %s\n", ethaddr_to_str(tname, arp_tha(ether_payload((mtod(mbuf, u8 *))))));
			DPRINTF("\tARP-TPA: %s\n", ip4addr_to_str(tname, arp_tpa(ether_payload((mtod(mbuf, u8 *))))));
		} else if(ether_type(mtod(mbuf, u8 *)) == 0x0800	/* IPv4 */) {
			DPRINTF("\tIP-SRC: %s\n", ip4addr_to_str(tname, ip_srcaddr(ether_payload((mtod(mbuf, u8 *))))));
			DPRINTF("\tIP-DST: %s\n", ip4addr_to_str(tname, ip_dstaddr(ether_payload((mtod(mbuf, u8 *))))));
			DPRINTF("\tIP-LEN: %d\n", ip_len(ether_payload((mtod(mbuf, u8 *)))));
			DPRINTF("\tIP-TTL: %d\n", ip_ttl(ether_payload((mtod(mbuf, u8 *)))));
			DPRINTF("\tIP-CHKSUM: 0x%04X\n",ip_chksum(ether_payload((mtod(mbuf, u8 *)))));
			DPRINTF("\tIP-PROTOCOL: %d\n",	ip_protocol(ether_payload((mtod(mbuf, u8 *)))));
		}
#endif
#endif
		/* TODO: Learning not required as it is assumed that every port notifies
		 * its macaddr change, multiple macs will still work because of
		 * broadcast (but need to optimize such case)  */

		/* Find if the frame should be unicast because it satisfies both of these:
		 * Case 1: It is not a broadcast MAC address, and
		 * Case 2: We do have the MAC->port mapping
		 */
		if(!is_broadcast_ether_addr(dstmac)) {
			list_for_each(l, &nsw->port_list) {
				   if(!compare_ether_addr(list_port(l)->macaddr, dstmac)) {
					DPRINTF("%s: port->macaddr[%s]\n", __func__,
						ethaddr_to_str(tname, list_port(l)->macaddr));
					dst_port = list_port(l);
					broadcast = FALSE;
					break;
				   }
			}
		}		

		if(broadcast) {
			DPRINTF("%s: broadcasting\n", __func__);
			list_for_each(l, &nsw->port_list) {
				   if(compare_ether_addr(list_port(l)->macaddr, srcmac)) {
					if(list_port(l)->can_receive &&
					   !list_port(l)->can_receive(list_port(l))) {
					   continue;
					}
					MADDREFERENCE(mbuf);
					MCLADDREFERENCE(mbuf);
					list_port(l)->switch2port_xfer(list_port(l), mbuf);
				   }
			}
			m_freem(mbuf);
		} else {
			DPRINTF("%s: unicasting to \"%s\"\n", __func__, dst_port->name);
			if(!dst_port->can_receive || dst_port->can_receive(dst_port)) {
				dst_port->switch2port_xfer(dst_port, mbuf);
			} else {
				/* Free the mbuf if destination cannot do rx */
				m_freem(mbuf);
			}
		}

	}
	return VMM_OK;
}

/**
 *  Handler for filling up the received packets at the RX buffer
 */
static int vmm_netbridge_rx_handler(struct vmm_netport *src_port,
				    struct vmm_mbuf *mbuf)
{
	struct dlist *l;
	struct netbridge_xfer *xfer;
	unsigned long flags;
	struct vmm_netbridge_ctrl *netbridge_ctrl;

	if(!src_port || !src_port->nsw) {
		return VMM_EFAIL;
	}
	if(!(netbridge_ctrl = src_port->nsw->priv)) {
		return VMM_EFAIL;
	}
	/* Get a free netbridge_xfer instance from free_list */
	vmm_spin_lock_irqsave(&netbridge_ctrl->free_list_lock, flags);
	if(list_empty(&netbridge_ctrl->free_list)) {
		vmm_spin_unlock_irqrestore(&netbridge_ctrl->free_list_lock, 
					   flags);
		m_freem(mbuf);
		return VMM_EFAIL;
	}
	l = list_pop(&netbridge_ctrl->free_list);
	vmm_spin_unlock_irqrestore(&netbridge_ctrl->free_list_lock, flags);

	xfer = list_entry(l, struct netbridge_xfer, head);

	/* Fillup the netbridge_xfer */
	xfer->src_port = src_port;
	xfer->mbuf = mbuf;

	/* Add this new xfer to the rx_list */
	vmm_spin_lock_irqsave(&netbridge_ctrl->rx_list_lock, flags);
	list_add_tail(&netbridge_ctrl->rx_list, &xfer->head);
	vmm_spin_unlock_irqrestore(&netbridge_ctrl->rx_list_lock, flags);

	vmm_completion_complete(&netbridge_ctrl->rx_not_empty);

	return VMM_OK;
}

static int vmm_netbridge_enable_port(struct vmm_netport *port)
{
	/* Notify the port about the link-status change */
	port->flags |= VMM_NETPORT_LINK_UP;
	port->link_changed(port);

	return VMM_OK;
}

static int vmm_netbridge_disable_port(struct vmm_netport *port)
{
	/* Notify the port about the link-status change */
	port->flags &= ~VMM_NETPORT_LINK_UP;
	port->link_changed(port);

	return VMM_OK;
}

static int vmm_netbridge_probe(struct vmm_device *dev,
			       const struct vmm_devid *devid)
{
	struct vmm_netswitch *nsw;
	struct dlist *tmp_node;
	struct vmm_netbridge_ctrl *netbridge_ctrl;
	int i, rc = VMM_OK;

	nsw = vmm_netswitch_alloc(dev->node->name);
	if(!nsw) {
		rc = VMM_EFAIL;
		goto vmm_netbridge_probe_failed;
	}
	nsw->port2switch_xfer = vmm_netbridge_rx_handler;
	nsw->enable_port = vmm_netbridge_enable_port;
	nsw->disable_port = vmm_netbridge_disable_port;

	netbridge_ctrl = vmm_malloc(sizeof(struct vmm_netbridge_ctrl));
	if(!netbridge_ctrl) {
		rc = VMM_EFAIL;
		goto vmm_netbridge_probe_failed;
	}
	vmm_memset(netbridge_ctrl, 0, sizeof(struct vmm_netbridge_ctrl));

	INIT_COMPLETION(&netbridge_ctrl->rx_not_empty);
	INIT_SPIN_LOCK(&netbridge_ctrl->free_list_lock);
	INIT_LIST_HEAD(&netbridge_ctrl->free_list);
	INIT_SPIN_LOCK(&netbridge_ctrl->rx_list_lock);
	INIT_LIST_HEAD(&netbridge_ctrl->rx_list);

	netbridge_ctrl->xfer_pool = vmm_malloc(sizeof(struct netbridge_xfer)
						* VMM_NETBRIDGE_RX_BUFLEN);
	netbridge_ctrl->thread = vmm_threads_create(nsw->name,
						   vmm_netbridge_tx,
						   netbridge_ctrl,
						   VMM_THREAD_DEF_PRIORITY,
						   VMM_THREAD_DEF_TIME_SLICE);
	if(!netbridge_ctrl->xfer_pool || !netbridge_ctrl->thread) {
		rc = VMM_EFAIL;
		goto vmm_netbridge_alloc_failed;
	}

	/* Fill the free_list of netbridge_xfer */
	for(i=0; i<VMM_NETBRIDGE_RX_BUFLEN; i++) {
		tmp_node = &((netbridge_ctrl->xfer_pool + i)->head);
		list_add_tail(&netbridge_ctrl->free_list, tmp_node);
	}

	nsw->dev = dev;
	nsw->priv = netbridge_ctrl;
	dev->priv = nsw;

	vmm_netswitch_register(nsw);

	vmm_threads_start(netbridge_ctrl->thread);

	goto vmm_netbridge_probe_done;

vmm_netbridge_alloc_failed:
	if(netbridge_ctrl) {
		if(netbridge_ctrl->xfer_pool) {
			vmm_free(netbridge_ctrl->xfer_pool);
		}
		if(netbridge_ctrl->thread) {
			vmm_threads_destroy(netbridge_ctrl->thread);
		}
		vmm_free(netbridge_ctrl);
	}
vmm_netbridge_probe_failed:
vmm_netbridge_probe_done:
	return rc;
}

static int vmm_netbridge_remove(struct vmm_device *dev)
{
	struct vmm_netswitch *nsw;
	struct vmm_netbridge_ctrl *netbridge_ctrl;
        nsw = dev->priv;
	if(nsw) {
		vmm_netswitch_unregister(nsw);
		netbridge_ctrl = nsw->priv;
		if(netbridge_ctrl) {
			if(netbridge_ctrl->xfer_pool) {
				vmm_free(netbridge_ctrl->xfer_pool);
			}
			if(netbridge_ctrl->thread) {
				vmm_threads_destroy(netbridge_ctrl->thread);
			}
			vmm_free(netbridge_ctrl);
		}
		vmm_free(nsw);
	}
	return VMM_OK;
}

static struct vmm_devid def_netswitch_devid_table[] = {
	{.type = "netswitch",.compatible = "bridge"},
	{ /* end of list */ },
};

static struct vmm_driver net_bridge = {
	.name = "netbridge",
	.match_table = def_netswitch_devid_table,
	.probe = vmm_netbridge_probe,
	.remove = vmm_netbridge_remove,
};

int __init vmm_netbridge_init(void)
{
	return vmm_devdrv_register_driver(&net_bridge);
}

void vmm_netbridge_exit(void)
{
	vmm_devdrv_unregister_driver(&net_bridge);
}

