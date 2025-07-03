/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "kernel.h"
#include "libriscv.h"
#include "../include/libos.h"
#include "cache.h"
#include "task.h"
#include "scheduler.h"
#include "mm/pagetable.h"

LibosStatus KernelSyscallCacheIlwalidate(LwU64 start, LwU64 end)
{
#if !LIBOS_TINY
    Task * self = KernelSchedulerGetTask();
    PageTableGlobal * pagetable = self->asid->pagetable;
#endif

#define RISCV_CACHE_LINE_SIZE 64ULL
    LwU64 startMasked = (start & ~(RISCV_CACHE_LINE_SIZE - 1ULL));

#if !LIBOS_TINY
    // Heuristic: If we're being asked to flush more than 128 lines
    //            Shoot down the entire cache. 
    //  @note: Do not increase this value as it directly effects the
    //         maximum time spent in the kernel.
    if ((end - start) > (128*RISCV_CACHE_LINE_SIZE))
    {
        KernelCsrWrite(LW_RISCV_CSR_XDCACHEOP,
            REF_DEF64(LW_RISCV_CSR_XDCACHEOP_MODE, _ILW_ALL));
        return LibosOk;
    }
#endif    

    for (; startMasked < end; startMasked += RISCV_CACHE_LINE_SIZE)
    {        
#if !LIBOS_TINY
        PageMapping mapping;
        if (KernelPageMappingPin(pagetable, startMasked, &mapping) != LibosOk)
            return LibosErrorArgument;
        LwU64 pa = mapping.physicalAddress + (startMasked - mapping.virtualAddress);
#else
        LwU64 pa = startMasked;         // PA=VA for LIBOS_TINY
#endif

        KernelCsrWrite(
            LW_RISCV_CSR_XDCACHEOP, pa |
                                        REF_DEF64(LW_RISCV_CSR_XDCACHEOP_ADDR_MODE, _PA) |
                                        REF_DEF64(LW_RISCV_CSR_XDCACHEOP_MODE, _ILW_LINE));
#if !LIBOS_TINY
        KernelPageMappingRelease(&mapping);
#endif
    }

    return LibosOk;
}
