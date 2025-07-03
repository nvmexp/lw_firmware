/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBOS_PAGING_H
#define LIBOS_PAGING_H

#include "memory.h"

#if LIBOS_CONFIG_TCM_PAGING
    #include "tcm_resident_page.h"

    LwU64 kernel_paging_page_in(libos_pagetable_entry_t *memory, LwU64 bad_addr, LwU64 xcause, LwBool skip_read);
    void kernel_ilwerted_pagetable_load(LwS64 phdr_boot_virtual_to_physical_delta);
    void kernel_paging_load(LwS64 phdr_boot_virtual_to_physical_delta);
    void LIBOS_SECTION_IMEM_PINNED kernel_paging_writeback(resident_page *page);
    LIBOS_SECTION_IMEM_PINNED void kernel_paging_writeback_and_ilwalidate(resident_page *page);
#endif

static inline LwU64 mpu_page_addr(LwU64 addr)
{
    return addr & LIBOS_MPU_PAGEMASK;
}

extern LwU64 code_paged_begin[], code_paged_end[];
extern LwU64 data_paged_begin[], data_paged_end[];

#define LIBOS_KERNEL_PINNED_ITCM_SIZE \
    ((((LwU64)code_paged_end - (LwU64)code_paged_begin) + (LIBOS_MPU_PAGESIZE - 1)) & LIBOS_MPU_PAGEMASK)
#define LIBOS_KERNEL_PINNED_DTCM_SIZE \
    ((((LwU64)data_paged_end - (LwU64)data_paged_begin) + (LIBOS_MPU_PAGESIZE - 1)) & LIBOS_MPU_PAGEMASK)

#define LIBOS_KERNEL_PINNED_ITCM_PAGE_COUNT (LIBOS_KERNEL_PINNED_ITCM_SIZE >> LIBOS_MPU_PAGESHIFT)
#define LIBOS_KERNEL_PINNED_DTCM_PAGE_COUNT (LIBOS_KERNEL_PINNED_DTCM_SIZE >> LIBOS_MPU_PAGESHIFT)

#define LIBOS_MPU_INDEX_KERNEL_PINNED_ITCM 0u  // Initialized in kernel_mmu_load_kernel
#define LIBOS_MPU_INDEX_KERNEL_PINNED_DTCM 1u  // Initialized in kernel_mmu_load_kernel
#define LIBOS_MPU_INDEX_CACHE_CODE         2u  // Initialized in bootstrap.c
#define LIBOS_MPU_INDEX_CACHE_DATA         3u  // Initialized in bootstrap.c
#define LIBOS_MPU_INDEX_PAGETABLE          4u  // Initialized by kernel_mmu_map_pagetable
#define LIBOS_MPU_INDEX_EXTIO_PRI          5u  // Maps EXTIO peregrine registers
#define LIBOS_MPU_INDEX_INTIO_PRI          6u  // Maps INTIO peregrine registers

#if LIBOS_CONFIG_TCM_PAGING
    // Reserve an extra page at the start of ITCM/DTCM for the separation kernel, if configured
    #define LIBOS_TCM_INDEX_PAGED_ITCM_FIRST  (LIBOS_CONFIG_LWRISCV_SEPKERN + LIBOS_KERNEL_PINNED_ITCM_PAGE_COUNT)
    #define LIBOS_TCM_INDEX_PAGED_DTCM_FIRST  (LIBOS_CONFIG_LWRISCV_SEPKERN + LIBOS_KERNEL_PINNED_DTCM_PAGE_COUNT)

    // tcm_page indices are directly offset from mpu indices
    #define LIBOS_MPU_INDEX_PAGED_TCM_FIRST   7u
    #define LIBOS_MPU_INDEX_SCRUB_START LIBOS_MPU_INDEX_PAGED_TCM_FIRST

    // Dedicated MPU entry for each DTCM page
    #define LIBOS_MPU_INDEX_PAGED_DTCM_FIRST (LIBOS_MPU_INDEX_PAGED_TCM_FIRST)
    #define LIBOS_MPU_INDEX_PAGED_DTCM_COUNT \
        ((LIBOS_CONFIG_DTCM_SIZE >> LIBOS_MPU_PAGESHIFT) - LIBOS_TCM_INDEX_PAGED_DTCM_FIRST)

    // Dedicated MPU entry for each ITCM page
    #define LIBOS_MPU_INDEX_PAGED_ITCM_FIRST (LIBOS_MPU_INDEX_PAGED_DTCM_FIRST + LIBOS_MPU_INDEX_PAGED_DTCM_COUNT)
    #define LIBOS_MPU_INDEX_PAGED_ITCM_COUNT \
        ((LIBOS_CONFIG_ITCM_SIZE >> LIBOS_MPU_PAGESHIFT) - LIBOS_TCM_INDEX_PAGED_ITCM_FIRST)

    // Remaining MPU entries are general purpose for cached mappings
    // Count depends on the architecture
    #define LIBOS_MPU_INDEX_CACHED_FIRST (LIBOS_MPU_INDEX_PAGED_ITCM_FIRST + LIBOS_MPU_INDEX_PAGED_ITCM_COUNT)

    // Statically callwlated, so this doesn't exclude pinned kernel pages (just a few)
    #define MAX_PAGEABLE_TCM_PAGES \
            ((LIBOS_CONFIG_DTCM_SIZE >> LIBOS_MPU_PAGESHIFT) + \
             (LIBOS_CONFIG_ITCM_SIZE >> LIBOS_MPU_PAGESHIFT))

    extern resident_page LIBOS_SECTION_DMEM_PINNED(tcm_pages)[MAX_PAGEABLE_TCM_PAGES];

    #define DMEM_PAGE_BASE_INDEX 0u
    #define IMEM_PAGE_BASE_INDEX LIBOS_MPU_INDEX_PAGED_DTCM_COUNT
#else // LIBOS_CONFIG_TCM_PAGING
    // Remaining MPU entries are general purpose for cached mappings
    // Count depends on the architecture
    #define LIBOS_MPU_INDEX_CACHED_FIRST 7u
    #define LIBOS_MPU_INDEX_SCRUB_START LIBOS_MPU_INDEX_CACHED_FIRST
#endif

// MPU index for temporary scratch mappings during init
#define LIBOS_MPU_INDEX_SCRATCH LIBOS_MPU_INDEX_SCRUB_START

#endif
