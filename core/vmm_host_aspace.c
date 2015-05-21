/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file vmm_host_aspace.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup patel (anup@brainfault.org)
 * @brief Source file for host virtual address space management.
 */

#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_host_ram.h>
#include <vmm_host_vapool.h>
#include <vmm_host_aspace.h>
#include <arch_config.h>
#include <arch_sections.h>
#include <arch_cpu_aspace.h>
#include <arch_devtree.h>
#include <libs/stringlib.h>

static virtual_addr_t host_mem_rw_va[CONFIG_CPU_COUNT];

virtual_addr_t vmm_host_memmap(physical_addr_t pa,
			       virtual_size_t sz,
			       u32 mem_flags)
{
	int rc, ite;
	virtual_addr_t va;
	physical_addr_t tpa;

	sz = VMM_ROUNDUP2_PAGE_SIZE(sz);

	if ((rc = vmm_host_vapool_alloc(&va, sz))) {
		/* Don't have space */
		BUG();
	}

	tpa = pa & ~VMM_PAGE_MASK;

	for (ite = 0; ite < (sz >> VMM_PAGE_SHIFT); ite++) {
		rc = arch_cpu_aspace_map(va + ite * VMM_PAGE_SIZE,
					tpa + ite * VMM_PAGE_SIZE,
					mem_flags);
		if (rc) {
			/* We were not able to map physical address */
			BUG();
		}
	}

	return va + (pa & VMM_PAGE_MASK);
}

static int host_memunmap(virtual_addr_t va, virtual_size_t sz)
{
	int rc, ite;

	sz = VMM_ROUNDUP2_PAGE_SIZE(sz);
	va &= ~VMM_PAGE_MASK;

	for (ite = 0; ite < (sz >> VMM_PAGE_SHIFT); ite++) {
		rc = arch_cpu_aspace_unmap(va + ite * VMM_PAGE_SIZE);
		if (rc) {
			return rc;
		}
	}

	if ((rc = vmm_host_vapool_free(va, sz))) {
		BUG();
		return rc;
	}

	return VMM_OK;
}

int vmm_host_memunmap(virtual_addr_t va)
{
	int rc;
	virtual_addr_t alloc_va;
	virtual_size_t alloc_sz;

	rc = vmm_host_vapool_find(va, &alloc_va, &alloc_sz);
	if (rc) {
		return rc;
	}

	return host_memunmap(alloc_va, alloc_sz);
}

virtual_addr_t vmm_host_alloc_pages(u32 page_count, u32 mem_flags)
{
	physical_addr_t pa = 0x0;

	if (!vmm_host_ram_alloc(&pa,
				page_count * VMM_PAGE_SIZE,
				VMM_PAGE_SHIFT)) {
		return 0x0;
	}

	return vmm_host_memmap(pa, page_count * VMM_PAGE_SIZE, mem_flags);
}

int vmm_host_free_pages(virtual_addr_t page_va, u32 page_count)
{
	int rc = VMM_OK;
	physical_addr_t pa = 0x0;

	page_va &= ~VMM_PAGE_MASK;

	if ((rc = arch_cpu_aspace_va2pa(page_va, &pa))) {
		return rc;
	}

	if ((rc = host_memunmap(page_va, page_count * VMM_PAGE_SIZE))) {
		return rc;
	}

	return vmm_host_ram_free(pa, page_count * VMM_PAGE_SIZE);
}

int vmm_host_va2pa(virtual_addr_t va, physical_addr_t *pa)
{
	int rc = VMM_OK;
	physical_addr_t _pa = 0x0;

	if ((rc = arch_cpu_aspace_va2pa(va & ~VMM_PAGE_MASK, &_pa))) {
		return rc;
	}

	if (pa) {
		*pa = _pa | (va & VMM_PAGE_MASK);
	}

	return VMM_OK;
}

int vmm_host_pa2va(physical_addr_t pa,
		   virtual_size_t sz,
		   u32 mem_flags,
		   virtual_addr_t *va)
{
	int rc = VMM_OK;
	virtual_addr_t _va = 0x0;

	sz = VMM_ROUNDUP2_PAGE_SIZE(sz);
	if ((rc = arch_cpu_aspace_pa2va(pa & ~VMM_PAGE_MASK,
					sz, mem_flags, &_va))) {
		return rc;
	}

	if (va) {
		*va = _va | (pa & VMM_PAGE_MASK);
	}

	return VMM_OK;
}

u32 vmm_host_memory_read(physical_addr_t hpa,
			 void *dst, u32 len, bool cacheable)
{
	int rc;
	irq_flags_t flags;
	u32 bytes_read = 0, page_offset, page_read;
	virtual_addr_t tmp_va = host_mem_rw_va[vmm_smp_processor_id()];

	/* Read one page at time with irqs disabled since, we use
	 * one virtual address per-host CPU to do read/write.
	 */
	while (bytes_read < len) {
		page_offset = hpa & VMM_PAGE_MASK;

		page_read = VMM_PAGE_SIZE - page_offset;
		page_read = (page_read < (len - bytes_read)) ?
			     page_read : (len - bytes_read);

		arch_cpu_irq_save(flags);

#if !defined(ARCH_HAS_MEMORY_READWRITE)
		rc = arch_cpu_aspace_map(tmp_va, hpa & ~VMM_PAGE_MASK,
					 (cacheable) ?
					 VMM_MEMORY_FLAGS_NORMAL :
					 VMM_MEMORY_FLAGS_NORMAL_NOCACHE);
		if (rc) {
			break;
		}

		memcpy(dst, (void *)(tmp_va + page_offset), page_read);

		rc = arch_cpu_aspace_unmap(tmp_va);
		if (rc) {
			break;
		}
#else
		rc = arch_cpu_aspace_memory_read(tmp_va, hpa,
						 dst, page_read, cacheable);
		if (rc) {
			break;
		}
#endif

		arch_cpu_irq_restore(flags);

		hpa += page_read;
		bytes_read += page_read;
		dst += page_read;
	}

	return bytes_read;
}

u32 vmm_host_memory_write(physical_addr_t hpa,
			  void *src, u32 len, bool cacheable)
{
	int rc;
	irq_flags_t flags;
	u32 bytes_written = 0, page_offset, page_write;
	virtual_addr_t tmp_va = host_mem_rw_va[vmm_smp_processor_id()];

	/* Write one page at time with irqs disabled since, we use
	 * one virtual address per-host CPU to do read/write.
	 */
	while (bytes_written < len) {
		page_offset = hpa & VMM_PAGE_MASK;

		page_write = VMM_PAGE_SIZE - page_offset;
		page_write = (page_write < (len - bytes_written)) ?
			      page_write : (len - bytes_written);

		arch_cpu_irq_save(flags);

#if !defined(ARCH_HAS_MEMORY_READWRITE)
		rc = arch_cpu_aspace_map(tmp_va, hpa & ~VMM_PAGE_MASK,
					 (cacheable) ?
					 VMM_MEMORY_FLAGS_NORMAL :
					 VMM_MEMORY_FLAGS_NORMAL_NOCACHE);
		if (rc) {
			break;
		}

		memcpy((void *)(tmp_va + page_offset), src, page_write);

		rc = arch_cpu_aspace_unmap(tmp_va);
		if (rc) {
			break;
		}
#else
		rc = arch_cpu_aspace_memory_write(tmp_va, hpa,
						  src, page_write, cacheable);
		if (rc) {
			break;
		}
#endif

		arch_cpu_irq_restore(flags);

		hpa += page_write;
		bytes_written += page_write;
		src += page_write;
	}

	return bytes_written;
}

u32 vmm_host_memory_set(physical_addr_t hpa,
			  u8 byte, u32 len, bool cacheable)
{
	u8 buf[256];
	u32 to_wr, wr, total_written = 0;
	physical_addr_t pos, end;

	memset(buf, byte, sizeof(buf));

	pos = hpa;
	end = hpa + len;
	while (pos < end) {
		to_wr = (sizeof(buf) < (end - pos)) ?
					sizeof(buf) : (end - pos);

		wr = vmm_host_memory_write(pos, buf, to_wr, cacheable);

		pos += to_wr;
		total_written += to_wr;

		if (wr < to_wr) {
			break;
		}
	}

	return total_written;
}

u32 vmm_host_free_initmem(void)
{
	int rc;
	virtual_addr_t init_start;
	virtual_size_t init_size;

	init_start = arch_init_vaddr();
	init_size = arch_init_size();
	init_size = VMM_ROUNDUP2_PAGE_SIZE(init_size);

	if ((rc = vmm_host_free_pages(init_start, init_size >> VMM_PAGE_SHIFT))) {
		BUG();
	}

	return (init_size >> VMM_PAGE_SHIFT) * VMM_PAGE_SIZE / 1024;
}

int __cpuinit vmm_host_aspace_init(void)
{
	int rc, cpu, bank_found = 0;
	u32 resv, resv_count, bank, bank_count = 0x0;
	physical_addr_t ram_start, core_resv_pa = 0x0, arch_resv_pa = 0x0;
	physical_size_t ram_size;
	virtual_addr_t vapool_start, vapool_hkstart, ram_hkstart;
	virtual_size_t vapool_size, vapool_hksize, ram_hksize;
	virtual_size_t hk_total_size = 0x0;
	virtual_addr_t core_resv_va = 0x0, arch_resv_va = 0x0;
	virtual_size_t core_resv_sz = 0x0, arch_resv_sz = 0x0;

	/* For Non-Boot CPU just call arch code and return */
	if (!vmm_smp_is_bootcpu()) {
		rc = arch_cpu_aspace_secondary_init();
		if (rc) {
			return rc;
		}

#if defined(ARCH_HAS_MEMORY_READWRITE)
		/* Initialize memory read/write for Non-Boot CPU */
		rc = arch_cpu_aspace_memory_rwinit(
				host_mem_rw_va[vmm_smp_processor_id()]);
		if (rc) {
			return rc;
		}
#endif

		return VMM_OK;
	}

	/* Determine VAPOOL start and size */
	vapool_start = arch_code_vaddr_start();
	vapool_size = (CONFIG_VAPOOL_SIZE_MB << 20);

	/* Determine VAPOOL house-keeping size based on VAPOOL size */
	vapool_hksize = vmm_host_vapool_estimate_hksize(vapool_size);

	/* Determine RAM bank count, start and size */
	if ((rc = arch_devtree_ram_bank_setup())) {
		return rc;
	}
	if ((rc = arch_devtree_ram_bank_count(&bank_count))) {
		return rc;
	}
	if (bank_count == 0) {
		return VMM_ENOMEM;
	}
	if (bank_count > CONFIG_MAX_RAM_BANK_COUNT) {
		return VMM_EINVALID;
	}
	bank_found = 0;
	for (bank = 0; bank < bank_count; bank++) {
		if ((rc = arch_devtree_ram_bank_start(bank, &ram_start))) {
			return rc;
		}
		if (ram_start & VMM_PAGE_MASK) {
			return VMM_EINVALID;
		}
		if ((rc = arch_devtree_ram_bank_size(bank, &ram_size))) {
			return rc;
		}
		if (ram_size & VMM_PAGE_MASK) {
			return VMM_EINVALID;
		}
		if ((ram_start <= arch_code_paddr_start()) &&
		    (arch_code_paddr_start() < (ram_start + ram_size))) {
			bank_found = 1;
			break;
		}
	}
	if (!bank_found) {
		return VMM_ENODEV;
	}

	/* Determine RAM house-keeping size */
	ram_hksize = vmm_host_ram_estimate_hksize();

	/* Calculate physical address, virtual address, and size of
	 * core reserved space for VAPOOL and RAM house-keeping
	 */
	hk_total_size = vapool_hksize + ram_hksize;
	hk_total_size = VMM_ROUNDUP2_PAGE_SIZE(hk_total_size);
	core_resv_pa = ram_start;
	core_resv_va = vapool_start + arch_code_size();
	core_resv_sz = hk_total_size;

	/* We cannot estimate the physical address, virtual address,
	 * and size of arch reserved space so we set all of them to
	 * zero and expect that arch_primary_cpu_aspace_init() will
	 * update them if arch code requires arch reserved space.
	 */
	arch_resv_pa = 0x0;
	arch_resv_va = 0x0;
	arch_resv_sz = 0x0;

	/* Call arch_primary_cpu_aspace_init() with the estimated
	 * parameters for core reserved space and arch reserved space.
	 * The arch_primary_cpu_aspace_init() can change these parameter
	 * as needed.
	 */
	if ((rc = arch_cpu_aspace_primary_init(&core_resv_pa,
						&core_resv_va,
						&core_resv_sz,
						&arch_resv_pa,
						&arch_resv_va,
						&arch_resv_sz))) {
		return rc;
	}
	if (core_resv_sz < hk_total_size) {
		return VMM_EFAIL;
	}
	if ((vapool_size <= core_resv_sz) ||
	    (ram_size <= core_resv_sz)) {
		return VMM_EFAIL;
	}
	vapool_hkstart = core_resv_va;
	ram_hkstart = core_resv_va + vapool_hksize;

	/* Initialize VAPOOL managment */
	if ((rc = vmm_host_vapool_init(vapool_start,
					vapool_size,
					vapool_hkstart))) {
		return rc;
	}

	/* Initialize RAM managment */
	if ((rc = vmm_host_ram_init(ram_hkstart))) {
		return rc;
	}

	/* Reserve all pages covering code space, core reserved space,
	 * and arch reserved space in VAPOOL & RAM.
	 */
	if (arch_code_vaddr_start() < core_resv_va) {
		core_resv_va = arch_code_vaddr_start();
	}
	if ((arch_resv_sz > 0) && (arch_resv_va < core_resv_va)) {
		core_resv_va = arch_resv_va;
	}
	if (arch_code_paddr_start() < core_resv_pa) {
		core_resv_pa = arch_code_paddr_start();
	}
	if ((arch_resv_sz > 0) &&
	    (arch_resv_pa < core_resv_pa)) {
		core_resv_pa = arch_resv_pa;
	}
	if ((core_resv_va + core_resv_sz) <
			(arch_code_vaddr_start() + arch_code_size())) {
		core_resv_sz =
		(arch_code_vaddr_start() + arch_code_size()) - core_resv_va;
	}
	if ((arch_resv_sz > 0) &&
	    ((core_resv_va + core_resv_sz) < (arch_resv_va + arch_resv_sz))) {
		core_resv_sz = (arch_resv_va + arch_resv_sz) - core_resv_va;
	}
	if ((rc = vmm_host_vapool_reserve(core_resv_va,
					  core_resv_sz))) {
		return rc;
	}
	if ((rc = vmm_host_ram_reserve(core_resv_pa,
				       core_resv_sz))) {
		return rc;
	}

	/* Reserve portion of RAM as specified by
	 * arch device tree functions.
	 */
	if ((rc = arch_devtree_reserve_count(&resv_count))) {
		return rc;
	}
	for (resv = 0; resv < resv_count; resv++) {
		if ((rc = arch_devtree_reserve_addr(resv, &ram_start))) {
			return rc;
		}
		if ((rc = arch_devtree_reserve_size(resv, &ram_size))) {
			return rc;
		}
		if (ram_start & VMM_PAGE_MASK) {
			ram_size += ram_start & VMM_PAGE_MASK;
			ram_start -= ram_start & VMM_PAGE_MASK;
		}
		ram_size &= ~VMM_PAGE_MASK;
		if ((rc = vmm_host_ram_reserve(ram_start, ram_size))) {
			return rc;
		}
	}

	/* Setup temporary virtual address for physical read/write */
	for (cpu = 0; cpu < CONFIG_CPU_COUNT; cpu++) {
		rc = vmm_host_vapool_alloc(&host_mem_rw_va[cpu],
					   VMM_PAGE_SIZE);
		if (rc) {
			return rc;
		}
	}

#if defined(ARCH_HAS_MEMORY_READWRITE)
	/* Initialize memory read/write for Boot CPU */
	rc = arch_cpu_aspace_memory_rwinit(
				host_mem_rw_va[vmm_smp_bootcpu_id()]);
	if (rc) {
		return rc;
	}
#endif

	return VMM_OK;
}
