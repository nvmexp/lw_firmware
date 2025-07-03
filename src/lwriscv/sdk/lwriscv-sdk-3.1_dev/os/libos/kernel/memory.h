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

void kernel_paging_clear_mpu(void);

#endif
