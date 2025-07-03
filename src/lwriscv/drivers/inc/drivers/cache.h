/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#if defined(LWRISCV_SDK) && LWRISCV_HAS_DCACHE
#include <liblwriscv/g_lwriscv_config.h>
#include <lwriscv/cache.h>
#else /* defined(LWRISCV_SDK) && LWRISCV_HAS_DCACHE */
#ifndef CACHE_H
#define CACHE_H

#include <riscv_csr.h>
#include <drivers/hardware.h>

#if LWRISCV_HAS_DCACHE
#define dcacheIlwalidate() ({ __asm__ volatile("csrrw x0, %0, %1" : :"i"(LW_RISCV_CSR_DCACHEOP), "r"(0)); })
#define dcacheIlwalidatePaRange(start, end) ({ \
    unsigned long long _start = (start & DRF_SHIFTMASK64(LW_RISCV_CSR_DCACHEOP_ADDR)) | \
                                DRF_DEF64(_RISCV_CSR, _DCACHEOP, _ADDR_MODE, _PA) | \
                                DRF_DEF64(_RISCV_CSR, _DCACHEOP, _MODE, _ILW_LINE); \
    unsigned long long _end = end; \
    for (;_start<_end; _start += RISCV_CACHE_LINE_SIZE) {\
    __asm__ volatile("csrrw x0, %0, %1" : :"i"(LW_RISCV_CSR_DCACHEOP), "r"(_start)); }\
    })
#define dcacheIlwalidateVaRange(start, end) ({ \
    unsigned long long _start = (start & DRF_SHIFTMASK64(LW_RISCV_CSR_DCACHEOP_ADDR)) | \
                                DRF_DEF64(_RISCV_CSR, _DCACHEOP, _ADDR_MODE, _VA) | \
                                DRF_DEF64(_RISCV_CSR, _DCACHEOP, _MODE, _ILW_LINE); \
    unsigned long long _end = end; \
    for (;_start<_end; _start += RISCV_CACHE_LINE_SIZE) {\
    __asm__ volatile("csrrw x0, %0, %1" : :"i"(LW_RISCV_CSR_DCACHEOP), "r"(_start)); }\
    })

#define icacheIlwalidate() ({ __asm__ volatile("fence.i"); })
#else // LWRISCV_HAS_DCACHE

//
// On platforms with no dcache, writing LW_RISCV_CSR_DCACHEOP
// can cause an illegal instruction fault. Stub out these macros.
//
#define dcacheIlwalidate()
#define dcacheIlwalidatePaRange(start, end)
#define dcacheIlwalidateVaRange(start, end)
#define icacheIlwalidate()
#endif // LWRISCV_HAS_DCACHE
#endif // CACHE_H
#endif /* defined(LWRISCV_SDK) && LWRISCV_HAS_DCACHE */
