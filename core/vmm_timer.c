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
 * @file vmm_timer.c
 * @version 0.1
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of timer subsystem
 */

#include <vmm_cpu.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_heap.h>
#include <vmm_timer.h>

/** Control structure for Timer Subsystem */
struct vmm_timer_ctrl {
	u64 cycles_last;
	u64 cycles_mask;
	u32 cycles_mult;
	u32 cycles_shift;
	u64 timestamp;
	bool cpu_started;
	vmm_timer_event_t *cpu_curr;
	struct dlist cpu_event_list;
	struct dlist event_list;
};

static struct vmm_timer_ctrl tctrl;

u64 vmm_timer_timestamp(void)
{
	u64 cycles_now, cycles_delta;
	u64 ns_offset;

	cycles_now = vmm_cpu_clocksource_cycles();
	cycles_delta = (cycles_now - tctrl.cycles_last) & tctrl.cycles_mask;
	tctrl.cycles_last = cycles_now;

	ns_offset = (cycles_delta * tctrl.cycles_mult) >> tctrl.cycles_shift;
	tctrl.timestamp += ns_offset;

	return tctrl.timestamp;
}

#ifdef CONFIG_PROFILE
u64 __notrace vmm_timer_timestamp_for_profile(void)
{
	u64 cycles_now, cycles_delta;
	u64 ns_offset;

	cycles_now = vmm_cpu_clocksource_cycles();
	cycles_delta = (cycles_now - tctrl.cycles_last) & tctrl.cycles_mask;
	ns_offset = (cycles_delta * tctrl.cycles_mult) >> tctrl.cycles_shift;

	return tctrl.timestamp + ns_offset;
}
#endif

static void vmm_timer_schedule_next_event(void)
{
	vmm_timer_event_t *e;

	/* If not started yet, we give up */
	if (!tctrl.cpu_started) {
		return;
	}

	/* If no events, we give up */
	if (list_empty(&tctrl.cpu_event_list)) {
		return;
	}

	/* retrieve first timer in the list of active timers */
	e = list_entry(list_first(&tctrl.cpu_event_list), vmm_timer_event_t,
		       cpu_head);

	if (tctrl.cpu_curr != e) {
		/* The current event is not the one at the head of the list. */
		u64 tstamp = vmm_timer_timestamp();

		tctrl.cpu_curr = e;

		if (tstamp > e->expiry_tstamp) {
			tstamp = e->expiry_tstamp;
		}

		vmm_cpu_clockevent_start(e->expiry_tstamp - tstamp);
	} else {
		/* FIXME: What if expiry time of current event changed ?? */
		/* Nothing to change as the current event is the one at the */
		/* head of the list and they are ordered by expiration time */
	}
}

/**
 * This is call from interrupt context. So we don't need to protect the list
 * when manipulating it.
 */
void vmm_timer_clockevent_process(vmm_user_regs_t * regs)
{
	vmm_timer_event_t *e;

	/* process expired active events */
	while (!list_empty(&tctrl.cpu_event_list)) {
		e = list_entry(list_first(&tctrl.cpu_event_list),
			       vmm_timer_event_t, cpu_head);
		/* Current timestamp */
		if (e->expiry_tstamp <= vmm_timer_timestamp()) {
			/* Set current CPU event to NULL */
			tctrl.cpu_curr = NULL;
			/* consume expired active events */
			list_del(&e->cpu_head);
			e->expiry_tstamp = 0;
			e->active = FALSE;
			e->cpu_regs = regs;
			e->handler(e);
			e->cpu_regs = NULL;
		} else {
			/* no more expired events */
			break;
		}
	}

	/* Schedule next timer event */
	vmm_timer_schedule_next_event();
}

int vmm_timer_event_start(vmm_timer_event_t * ev, u64 duration_nsecs)
{
	bool added;
	irq_flags_t flags;
	struct dlist *l;
	vmm_timer_event_t *e;
	u64 tstamp;

	if (!ev) {
		return VMM_EFAIL;
	}

	tstamp = vmm_timer_timestamp();

	flags = vmm_cpu_irq_save();

	if (ev->active) {
		/*
		 * if the timer event is already started, we remove it from
		 * the active list because it has changed.
		 */
		list_del(&ev->cpu_head);
	}

	ev->expiry_tstamp = tstamp + duration_nsecs;
	ev->duration_nsecs = duration_nsecs;
	ev->active = TRUE;
	added = FALSE;
	e = NULL;
	list_for_each(l, &tctrl.cpu_event_list) {
		e = list_entry(l, vmm_timer_event_t, cpu_head);
		if (ev->expiry_tstamp < e->expiry_tstamp) {
			list_add_tail(&e->cpu_head, &ev->cpu_head);
			added = TRUE;
			break;
		}
	}

	if (!added) {
		list_add_tail(&tctrl.cpu_event_list, &ev->cpu_head);
	}

	vmm_timer_schedule_next_event();

	vmm_cpu_irq_restore(flags);

	return VMM_OK;
}

int vmm_timer_event_restart(vmm_timer_event_t * ev)
{
	if (!ev) {
		return VMM_EFAIL;
	}

	return vmm_timer_event_start(ev, ev->duration_nsecs);
}

int vmm_timer_event_expire(vmm_timer_event_t * ev)
{
	irq_flags_t flags;

	if (!ev) {
		return VMM_EFAIL;
	}

	/* prevent (timer) interrupt */
	flags = vmm_cpu_irq_save();

	/* if the event is already engaged */
	if (ev->active) {
		/* We remove it from the list */
		list_del(&ev->cpu_head);
	}

	/* set the expiry_tstamp to before now */
	ev->expiry_tstamp = 0;
	ev->active = TRUE;

	/* add the event on list head as it is going to expire now */
	list_add(&tctrl.cpu_event_list, &ev->cpu_head);

	/* trigger a timer interrupt */
	vmm_cpu_clockevent_expire();

	/* allow (timer) interrupts */
	vmm_cpu_irq_restore(flags);

	return VMM_OK;
}

int vmm_timer_event_stop(vmm_timer_event_t * ev)
{
	irq_flags_t flags;

	if (!ev) {
		return VMM_EFAIL;
	}

	flags = vmm_cpu_irq_save();

	ev->expiry_tstamp = 0;

	if (ev->active) {
		list_del(&ev->cpu_head);
		ev->active = FALSE;
	}

	vmm_timer_schedule_next_event();

	vmm_cpu_irq_restore(flags);

	return VMM_OK;
}

vmm_timer_event_t *vmm_timer_event_create(const char *name,
					  vmm_timer_event_handler_t handler,
					  void *priv)
{
	bool found;
	struct dlist *l;
	vmm_timer_event_t *e;

	e = NULL;
	found = FALSE;
	list_for_each(l, &tctrl.event_list) {
		e = list_entry(l, vmm_timer_event_t, head);
		if (vmm_strcmp(name, e->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		return NULL;
	}

	e = vmm_malloc(sizeof(vmm_timer_event_t));
	if (!e) {
		return NULL;
	}

	INIT_LIST_HEAD(&e->head);
	vmm_strcpy(e->name, name);
	e->active = FALSE;
	INIT_LIST_HEAD(&e->cpu_head);
	e->cpu_regs = NULL;
	e->expiry_tstamp = 0;
	e->duration_nsecs = 0;
	e->handler = handler;
	e->priv = priv;

	list_add_tail(&tctrl.event_list, &e->head);

	return e;
}

int vmm_timer_event_destroy(vmm_timer_event_t * ev)
{
	bool found;
	struct dlist *l;
	vmm_timer_event_t *e;

	if (!ev) {
		return VMM_EFAIL;
	}

	if (list_empty(&tctrl.event_list)) {
		return VMM_EFAIL;
	}

	e = NULL;
	found = FALSE;
	list_for_each(l, &tctrl.event_list) {
		e = list_entry(l, vmm_timer_event_t, head);
		if (vmm_strcmp(e->name, ev->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return VMM_ENOTAVAIL;
	}

	list_del(&e->head);

	vmm_free(e);

	return VMM_OK;
}

vmm_timer_event_t *vmm_timer_event_find(const char *name)
{
	bool found;
	struct dlist *l;
	vmm_timer_event_t *e;

	if (!name) {
		return NULL;
	}

	found = FALSE;
	e = NULL;

	list_for_each(l, &tctrl.event_list) {
		e = list_entry(l, vmm_timer_event_t, head);
		if (vmm_strcmp(e->name, name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return NULL;
	}

	return e;
}

vmm_timer_event_t *vmm_timer_event_get(int index)
{
	bool found;
	struct dlist *l;
	vmm_timer_event_t *ret;

	if (index < 0) {
		return NULL;
	}

	ret = NULL;
	found = FALSE;

	list_for_each(l, &tctrl.event_list) {
		ret = list_entry(l, vmm_timer_event_t, head);
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	if (!found) {
		return NULL;
	}

	return ret;
}

u32 vmm_timer_event_count(void)
{
	u32 retval = 0;
	struct dlist *l;

	list_for_each(l, &tctrl.event_list) {
		retval++;
	}

	return retval;
}

void vmm_timer_start(void)
{
	vmm_cpu_clockevent_start(1000000);

	tctrl.cpu_started = TRUE;
}

void vmm_timer_stop(void)
{
	vmm_cpu_clockevent_stop();

	tctrl.cpu_started = FALSE;
}

int __init vmm_timer_init(void)
{
	int rc;

	/* Initialize Per CPU event status */
	tctrl.cpu_started = FALSE;

	/* Initialize Per CPU current event pointer */
	tctrl.cpu_curr = NULL;

	/* Initialize Per CPU event list */
	INIT_LIST_HEAD(&tctrl.cpu_event_list);

	/* Initialize event list */
	INIT_LIST_HEAD(&tctrl.event_list);

	/* Initialize cpu specific timer event */
	if ((rc = vmm_cpu_clockevent_init())) {
		return rc;
	}

	/* Initialize cpu specific timer cycle counter */
	if ((rc = vmm_cpu_clocksource_init())) {
		return rc;
	}

	/* Setup configuration for reading cycle counter */
	tctrl.cycles_mask = vmm_cpu_clocksource_mask();
	tctrl.cycles_mult = vmm_cpu_clocksource_mult();
	tctrl.cycles_shift = vmm_cpu_clocksource_shift();
	tctrl.cycles_last = vmm_cpu_clocksource_cycles();

	/* Starting value of timestamp */
	tctrl.timestamp = 0x0;

	return VMM_OK;
}
