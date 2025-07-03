/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "kernel.h"

#if LIBOS_CONFIG_TCM_PAGING
#include "../debug/elf.h"
#include "dev_falcon_v4.h"
#include "dev_gsp_riscv_csr_64.h"
#include "dev_riscv_csr_64_addendum.h"
#include "lw_gsp_riscv_address_map.h"
#include "dma.h"
#include "include/libos_init_daemon.h"
#include "include/libos_status.h"
#include "ilwerted_pagetable.h"
#include "memory.h"
#include "mmu.h"
#include "paging.h"
#include "tcm_resident_page.h"
#include "riscv-isa.h"
#include "task.h"

resident_page LIBOS_SECTION_DMEM_PINNED(tcm_pages)[MAX_PAGEABLE_TCM_PAGES] = {0};

void LIBOS_SECTION_TEXT_COLD kernel_paging_load(LwS64 phdr_boot_virtual_to_physical_delta)
{
    // Initialize physically tagged cache
    for (LwU32 i = 0u; i < HASH_BUCKETS; i++)
    {
        page_by_pa[i].next = &page_by_pa[i];
        page_by_pa[i].prev = &page_by_pa[i];
    }
}

void LIBOS_SECTION_IMEM_PINNED kernel_paging_writeback(resident_page *page)
{
    if ((page->fb_offset & PAGE_DIRTY) == 0ULL)
    {
        return;
    }

    LwU32 page_index = (LwU32)to_uint64(page - &tcm_pages[0u]);
    LwU64 phys_addr  = page->fb_offset & PAGE_MASK;

    // Clear the dirty bit and mark the page as read-only
    page->fb_offset &= ~PAGE_DIRTY;
    kernel_mmu_mark_mpu_entry_ro(widen(LIBOS_MPU_INDEX_PAGED_DTCM_FIRST + page_index));

    // @note We're guaranteed that this is not exelwtable memory
    // libos.py doesn't allow writeable code sections
    LwU64 offset = widen(LIBOS_TCM_INDEX_PAGED_DTCM_FIRST + page_index - DMEM_PAGE_BASE_INDEX) << LIBOS_MPU_PAGESHIFT;
    kernel_dma_start(phys_addr, offset, LIBOS_MPU_PAGESIZE, LIBOS_DMAIDX_PHYS_VID_FN0, LIBOS_DMAFLAG_WRITE);

    kernel_dma_flush();
}

LIBOS_SECTION_IMEM_PINNED void kernel_paging_writeback_and_ilwalidate(resident_page *page)
{
    if ((page->fb_offset & PAGE_RESIDENT) == 0u)
    {
        return;
    }

    kernel_paging_writeback(page);

    // Mark page as invalid
    kernel_mmu_clear_mpu_entry(widen(LIBOS_MPU_INDEX_PAGED_TCM_FIRST + (LwU32)to_uint64(page - &tcm_pages[0u])));

    // De-associate DMEM from page
    resident_page_remove(&page->header);
    page->fb_offset &= ~PAGE_RESIDENT;
}

/*!
 *
 * @brief pages in a cached or TCM paged.
 *
 */
LIBOS_SECTION_IMEM_PINNED LwU64
kernel_paging_page_in(libos_pagetable_entry_t *memory, LwU64 xbadaddr, LwU64 xcause, LwBool skip_read)
{
    // This function is not lwrrently used. Will be filled when 1K paging is implemented.
    return 0;
}

#endif // LIBOS_CONFIG_TCM_PAGING
