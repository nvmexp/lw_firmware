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
#include "mm.h"
#include "libriscv.h"

#include "../include/libos.h"
#include "cache.h"
#include "task.h"
#include "scheduler.h"

void LIBOS_SECTION_TEXT_COLD LIBOS_NORETURN
KernelSyscallCacheIlwalidate(LwU64 start, LwU64 end)
{
#define RISCV_CACHE_LINE_SIZE 64ULL
    LwU64 startMasked = (start & ~(RISCV_CACHE_LINE_SIZE - 1ULL));

    for (; startMasked < end; startMasked += RISCV_CACHE_LINE_SIZE)
    {
        KernelCsrWrite(
            LW_RISCV_CSR_XDCACHEOP, startMasked |
                                        REF_DEF64(LW_RISCV_CSR_XDCACHEOP_ADDR_MODE, _VA) |
                                        REF_DEF64(LW_RISCV_CSR_XDCACHEOP_MODE, _ILW_LINE));
    }

    KernelSyscallReturn(LibosOk);
}
