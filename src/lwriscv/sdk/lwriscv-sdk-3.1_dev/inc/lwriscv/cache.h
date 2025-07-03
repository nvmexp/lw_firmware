/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LWRISCV_CACHE_H
#define LWRISCV_CACHE_H

#include <stdint.h>
#include <lwmisc.h>
#include <lwriscv/csr.h>

#define RISCV_CACHE_LINE_SIZE    64

static LW_FORCEINLINE void
dcacheIlwalidate(void)
{
    __asm__ volatile("csrrw x0, %0, %1" : :"i"(LW_RISCV_CSR_XDCACHEOP), "r"(0));
}

static LW_FORCEINLINE void
dcacheIlwalidatePaRange(uint64_t start, uint64_t end)
{
    start = (start & DRF_SHIFTMASK64(LW_RISCV_CSR_XDCACHEOP_ADDR)) |
                    DRF_DEF64(_RISCV_CSR, _XDCACHEOP, _ADDR_MODE, _PA) |
                    DRF_DEF64(_RISCV_CSR, _XDCACHEOP, _MODE, _ILW_LINE);

    for (;start<end; start += RISCV_CACHE_LINE_SIZE)
    {
        __asm__ volatile("csrrw x0, %0, %1" : :"i"(LW_RISCV_CSR_XDCACHEOP), "r"(start));
    }
}

static LW_FORCEINLINE void
dcacheIlwalidateVaRange(uint64_t start, uint64_t end)
{
    start = (start & DRF_SHIFTMASK64(LW_RISCV_CSR_XDCACHEOP_ADDR)) |
                    DRF_DEF64(_RISCV_CSR, _XDCACHEOP, _ADDR_MODE, _VA) |
                    DRF_DEF64(_RISCV_CSR, _XDCACHEOP, _MODE, _ILW_LINE);
    for (;start<end; start += RISCV_CACHE_LINE_SIZE)
    {
        __asm__ volatile("csrrw x0, %0, %1" : :"i"(LW_RISCV_CSR_XDCACHEOP), "r"(start));
    }
}

static LW_FORCEINLINE void
icacheIlwalidate(void)
{
    __asm__ volatile("fence.i");
}

#endif // LWRISCV_CACHE_H
