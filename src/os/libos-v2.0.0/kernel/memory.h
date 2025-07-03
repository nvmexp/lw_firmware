/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBOS_MEMORY_H
#define LIBOS_MEMORY_H

#include "kernel.h"
#include "libos_defines.h"
#include "libos_ilwerted_pagetable.h"
#include "libos_syscalls.h"
#include "task.h"

#define LIBOS_MEMORY_IS_EXELWTE(m) (((m)->attributes_and_size & LIBOS_MEMORY_ATTRIBUTE_EXELWTE_MASK) != 0ULL)
#define LIBOS_MEMORY_IS_CACHED(m)                                                                            \
    (((m)->attributes_and_size & LIBOS_MEMORY_ATTRIBUTE_KIND) == LIBOS_MEMORY_KIND_CACHED)
#define LIBOS_MEMORY_IS_PAGED_TCM(m)                                                                         \
    (((m)->attributes_and_size & LIBOS_MEMORY_ATTRIBUTE_KIND) == LIBOS_MEMORY_KIND_PAGED_TCM)
#define LIBOS_MEMORY_IS_MMIO(m)                                                                              \
    (((m)->attributes_and_size & LIBOS_MEMORY_ATTRIBUTE_KIND) == LIBOS_MEMORY_KIND_MMIO)
#define LIBOS_MEMORY_IS_DMA(m)                                                                               \
    (((m)->attributes_and_size & LIBOS_MEMORY_ATTRIBUTE_KIND) == LIBOS_MEMORY_KIND_DMA)

typedef struct
{
    LwU64 index;
    LwU64 va, size;
} libos_phdr_patch_t;

LwU32 LIBOS_NORETURN kernel_syscall_memory_query(task_index_t remote_task, LwU64 pointer);

void LIBOS_NORETURN kernel_syscall_ilwalidate_cache(LwU64 start, LwU64 size);
void LIBOS_NORETURN kernel_syscall_ilwalidate_cache_all(void);

LIBOS_SECTION_IMEM_PINNED libos_pagetable_entry_t *
validate_memory_access_or_return(LwU64 buffer, LwU64 buffer_size, LwBool write);
LIBOS_SECTION_IMEM_PINNED libos_pagetable_entry_t *
validate_memory_pasid_access_or_return(LwU64 buffer, LwU64 buffer_size, LwBool write, libos_pasid_t pasid);

#endif
