/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBOS_PAGING_H
#define LIBOS_PAGING_H

#include "libos.h"
#include "memory.h"
#include "lwriscv.h"

// PagingPageIn returns this on failure
#define LIBOS_ILWALID_TCM_OFFSET (0xFFFFFFFF)

void kernel_paging_init(LwS64 PHDRBootVirtualToPhysicalDelta);
LwU64 PagingPageIn(Pasid pasid, LwU64 bad_addr, LwU64 xcause, LwBool skip_read);
void LIBOS_NORETURN KernelSyscallCacheIlwalidate(LwU64 start, LwU64 end);
void KernelPagingPrepareForProcessorSuspend(void);
void kernel_ilwerted_pagetable_load(LwS64 PHDRBootVirtualToPhysicalDelta);
void KernelPagingLoad(LwS64 PHDRBootVirtualToPhysicalDelta);
void KernelPagingLoad(LwS64 PHDRBootVirtualToPhysicalDelta);

LwU32 LIBOS_NORETURN kernel_syscall_memory_query(LibosTaskId remote_task, LwU64 pointer);

LIBOS_SECTION_IMEM_PINNED libos_pagetable_entry_t *
KernelValidateMemoryAccessOrReturn(LwU64 buffer, LwU64 buffer_size, LwBool write);
LIBOS_SECTION_IMEM_PINNED libos_pagetable_entry_t *
validate_memory_pasid_access_or_return(LwU64 buffer, LwU64 buffer_size, LwBool write, Pasid pasid);

extern LwU64 code_paged_begin[], code_paged_end[];
extern LwU64 data_paged_begin[], data_paged_end[];

// indices into mpu_attributes_base[] table
#define MPU_ATTRIBUTES_COHERENT   0
#define MPU_ATTRIBUTES_INCOHERENT 1

// @todo Should be indexed on kind.
extern LwU64 LIBOS_SECTION_DMEM_PINNED(mpu_attributes_base)[2];

LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN void kernel_datafault();
LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN void kernel_codefault();

// Initialized by bootstrap.c
#define LIBOS_MPU_INDEX_CACHE_CODE 0u
#define LIBOS_MPU_INDEX_CACHE_DATA 2u

// Initialized by KernelPagingLoad
#define LIBOS_MPU_INDEX_KERNEL_DMEM 1u
#define LIBOS_MPU_INDEX_KERNEL_IMEM 3u

#define LIBOS_MPU_MMIO_KERNEL 4u /* only maps peregrine registers */

// tcm_page indices are directly offset from mpu indices
#define LIBOS_MPU_INDEX_TCM_FIRST 5u

// Dedicated MPU entry for each DTCM page
#define LIBOS_MPU_INDEX_DTCM_FIRST (LIBOS_MPU_INDEX_TCM_FIRST)
#if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0
#define LIBOS_MPU_INDEX_DTCM_COUNT 14u
#define LIBOS_TCM_INDEX_DTCM_FIRST 2u
#else
#define LIBOS_MPU_INDEX_DTCM_COUNT 15u
#define LIBOS_TCM_INDEX_DTCM_FIRST 1u
#endif

// Dedicated MPU entry for each ITCM page
#define LIBOS_MPU_INDEX_ITCM_FIRST (LIBOS_MPU_INDEX_DTCM_FIRST + LIBOS_MPU_INDEX_DTCM_COUNT)
#define LIBOS_MPU_INDEX_ITCM_COUNT 14u
#if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0
#define LIBOS_TCM_INDEX_ITCM_FIRST 3u
#else
#define LIBOS_TCM_INDEX_ITCM_FIRST 2u
#endif

// General purpose MPU entries for cached mappings
#define LIBOS_MPU_INDEX_CACHED_FIRST (LIBOS_MPU_INDEX_ITCM_FIRST + LIBOS_MPU_INDEX_ITCM_COUNT)
#define LIBOS_MPU_INDEX_CACHED_COUNT 16u

#define LIBOS_MPU_INDEX_SCRUB_START LIBOS_MPU_INDEX_DTCM_FIRST
#define LIBOS_MPU_INDEX_SCRUB_COUNT                                                                          \
    (LIBOS_MPU_INDEX_DTCM_COUNT + LIBOS_MPU_INDEX_ITCM_COUNT + LIBOS_MPU_INDEX_CACHED_COUNT)

#define HASH_BUCKETS 32u

#define PAGEABLE_TCM_PAGES (LIBOS_MPU_INDEX_DTCM_COUNT + LIBOS_MPU_INDEX_ITCM_COUNT)

#define DMEM_PAGE_BASE_INDEX 0u
#define IMEM_PAGE_BASE_INDEX LIBOS_MPU_INDEX_DTCM_COUNT

#endif
