/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBOS_DMA_H
#define LIBOS_DMA_H

#include "kernel.h"
#include "libos_ilwerted_pagetable.h"

/*!
 * DMA Index
 *
 * The GSP's frame-buffer interface block has several slots/indices which can
 * be bound to support DMA to various surfaces in memory. This is an enumeration
 * that gives a name to each index based on type of memory-aperture the index is
 * used to access.
 */
#define LIBOS_DMAIDX_PHYS_VID_FN0     U(1)
#define LIBOS_DMAIDX_PHYS_SYS_COH_FN0 U(2)

#define LIBOS_DMAFLAG_WRITE 1
#define LIBOS_DMAFLAG_IMEM  2

typedef struct task_dma_state
{
    struct
    {
        LIBOS_PTR32(libos_pagetable_entry_t) memory;
        LwU32 size;
        LwU64 va;
    } tcm;
} task_dma_state_t;

void kernel_dma_init(void);
void kernel_dma_flush(void);
void kernel_dma_start(LwU64 phys_addr, LwU32 tcm_offset, LwU32 length, LwU32 dma_idx, LwU32 dma_flag);

void LIBOS_NORETURN kernel_syscall_dma_register_tcm(LwU64 va, LwU32 size);
void LIBOS_NORETURN kernel_syscall_dma_copy(LwU64 dst_va, LwU64 src_va, LwU32 size);
void LIBOS_NORETURN kernel_syscall_dma_flush(void);

#endif
