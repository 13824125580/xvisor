/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file vmm_heap.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup Patel (anup@brainfault.org)
 * @author Ankit Jindal (thatsjindal@gmail.com)
 * @brief heap management using buddy allocator
 */

#include <vmm_error.h>
#include <vmm_cache.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <libs/stringlib.h>
#include <libs/buddy.h>

struct vmm_heap_control {
	struct buddy_allocator ba;
	void *hk_start;
	unsigned long hk_size;
	void *mem_start;
	unsigned long mem_size;
	void *heap_start;
	unsigned long heap_size;
};

static struct vmm_heap_control normal_heap;
static struct vmm_heap_control dma_heap;

#define HEAP_MIN_BIN		(VMM_CACHE_LINE_SHIFT)
#define HEAP_MAX_BIN		(VMM_PAGE_SHIFT)

static void *heap_malloc(struct vmm_heap_control *heap,
			 virtual_size_t size)
{
	int rc;
	unsigned long addr;

	if (!size) {
		return NULL;
	}

	rc = buddy_mem_alloc(&heap->ba, size, &addr);
	if (rc) {
		vmm_printf("%s: Failed to alloc size=%d (error %d)\n",
			   __func__, size, rc);
		return NULL;
	}

	return (void *)addr;
}

static virtual_size_t heap_alloc_size(struct vmm_heap_control *heap,
				      const void *ptr)
{
	int rc;
	unsigned long aaddr, asize;

	BUG_ON(!ptr);
	BUG_ON(ptr < heap->mem_start);
	BUG_ON((heap->mem_start + heap->mem_size) <= ptr);

	rc = buddy_mem_find(&heap->ba, (unsigned long) ptr,
					&aaddr, NULL, &asize);
	if (rc) {
		return 0;
	}

	return asize - ((unsigned long)ptr - aaddr);
}

static void heap_free(struct vmm_heap_control *heap, void *ptr)
{
	int rc;

	BUG_ON(!ptr);
	BUG_ON(ptr < heap->mem_start);
	BUG_ON((heap->mem_start + heap->mem_size) <= ptr);

	rc = buddy_mem_free(&heap->ba, (unsigned long)ptr);
	if (rc) {
		vmm_printf("%s: Failed to free ptr=%p (error %d)\n",
			   __func__, ptr, rc);
	}
}

static int heap_print_state(struct vmm_heap_control *heap,
			    struct vmm_chardev *cdev, const char *name)
{
	unsigned long idx;

	vmm_cprintf(cdev, "%s Heap State\n", name);

	for (idx = HEAP_MIN_BIN; idx <= HEAP_MAX_BIN; idx++) {
		if (idx < 10) {
			vmm_cprintf(cdev, "  [BLOCK %4dB]: ", 1<<idx);
		} else if (idx < 20) {
			vmm_cprintf(cdev, "  [BLOCK %4dK]: ", 1<<(idx-10));
		} else {
			vmm_cprintf(cdev, "  [BLOCK %4dM]: ", 1<<(idx-20));
		}
		vmm_cprintf(cdev, "%5d area(s), %5d free block(s)\n",
			    buddy_bins_area_count(&heap->ba, idx),
			    buddy_bins_block_count(&heap->ba, idx));
	}

	vmm_cprintf(cdev, "%s Heap House-Keeping State\n", name);
	vmm_cprintf(cdev, "  Buddy Areas: %d free out of %d\n",
		    buddy_hk_area_free(&heap->ba),
		    buddy_hk_area_total(&heap->ba));

	return VMM_OK;
}

static int heap_init(struct vmm_heap_control *heap,
		     bool is_normal, const u32 size_kb, u32 mem_flags)
{
	memset(heap, 0, sizeof(*heap));

	heap->heap_size = size_kb * 1024;
	heap->heap_start = (void *)vmm_host_alloc_pages(
					VMM_SIZE_TO_PAGE(heap->heap_size),
					mem_flags);
	if (!heap->heap_start) {
		return VMM_ENOMEM;
	}

	/* 12.5 percent for house-keeping */
	heap->hk_size = (heap->heap_size) / 8;

	/* Always have book keeping area for
	 * non-normal heaps in normal heap
	 */
	if (is_normal) {
		heap->hk_start = heap->heap_start;
		heap->mem_start = heap->heap_start + heap->hk_size;
		heap->mem_size = heap->heap_size - heap->hk_size;
	} else {
		heap->hk_start = vmm_malloc(heap->hk_size);
		if (!heap->hk_start) {
			return VMM_ENOMEM;
		}
		heap->mem_start = heap->heap_start;
		heap->mem_size = heap->heap_size;
	}

	return buddy_allocator_init(&heap->ba,
			  heap->hk_start, heap->hk_size,
			  (unsigned long)heap->mem_start, heap->mem_size,
			  HEAP_MIN_BIN, HEAP_MAX_BIN);
}

void *vmm_malloc(virtual_size_t size)
{
	return heap_malloc(&normal_heap, size);
}

void *vmm_zalloc(virtual_size_t size)
{
	void *ret = vmm_malloc(size);

	if (ret) {
		memset(ret, 0, size);
	}

	return ret;
}

virtual_size_t vmm_alloc_size(const void *ptr)
{
	return heap_alloc_size(&normal_heap, ptr);
}

void vmm_free(void *ptr)
{
	heap_free(&normal_heap, ptr);
}

virtual_addr_t vmm_normal_heap_start_va(void)
{
	return (virtual_addr_t)normal_heap.heap_start;
}

virtual_size_t vmm_normal_heap_size(void)
{
	return (virtual_size_t)normal_heap.heap_size;
}

virtual_size_t vmm_normal_heap_hksize(void)
{
	return normal_heap.hk_size;
}

virtual_size_t vmm_normal_heap_free_size(void)
{
	return buddy_bins_free_space(&normal_heap.ba);
}

int vmm_normal_heap_print_state(struct vmm_chardev *cdev)
{
	return heap_print_state(&normal_heap, cdev, "Normal");
}

void *vmm_dma_malloc(virtual_size_t size)
{
	return heap_malloc(&dma_heap, size);
}

void *vmm_dma_zalloc(virtual_size_t size)
{
	void *ret = vmm_dma_malloc(size);

	if (ret) {
		memset(ret, 0, size);
	}

	return ret;
}

virtual_size_t vmm_dma_alloc_size(const void *ptr)
{
	return heap_alloc_size(&dma_heap, ptr);
}

void vmm_dma_free(void *ptr)
{
	heap_free(&dma_heap, ptr);
}

virtual_addr_t vmm_dma_heap_start_va(void)
{
	return (virtual_addr_t)dma_heap.heap_start;
}

virtual_size_t vmm_dma_heap_size(void)
{
	return (virtual_size_t)dma_heap.heap_size;
}

virtual_size_t vmm_dma_heap_hksize(void)
{
	return dma_heap.hk_size;
}

virtual_size_t vmm_dma_heap_free_size(void)
{
	return buddy_bins_free_space(&dma_heap.ba);
}

int vmm_dma_heap_print_state(struct vmm_chardev *cdev)
{
	return heap_print_state(&dma_heap, cdev, "DMA");
}

int __init vmm_heap_init(void)
{
	int rc;

	/*
	 * Always create normal heap first as book keeping area for other heaps
	 * is allocated from normal heap
	 */

	/* Create Normal heap */
	rc = heap_init(&normal_heap, TRUE,
			CONFIG_HEAP_SIZE_MB * 1024,
			VMM_MEMORY_FLAGS_NORMAL);
	if (rc) {
		return rc;
	}

	/* Create DMA heap */
	rc= heap_init(&dma_heap, FALSE,
			CONFIG_DMA_HEAP_SIZE_KB,
			VMM_MEMORY_FLAGS_DMA);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}
