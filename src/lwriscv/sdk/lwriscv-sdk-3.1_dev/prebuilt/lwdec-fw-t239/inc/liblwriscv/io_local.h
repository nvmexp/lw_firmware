/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBLWRISCV_IO_LOCAL_H
#define LIBLWRISCV_IO_LOCAL_H

#include <stdint.h>
#include <lwriscv/gcc_attrs.h>
#if (__riscv_xlen == 32)
#include LWRISCV32_MANUAL_ADDRESS_MAP
#include LWRISCV32_MANUAL_LOCAL_IO
#else
#include LWRISCV64_MANUAL_ADDRESS_MAP
#include LWRISCV64_MANUAL_LOCAL_IO
#endif // (__riscv_xlen == 32)

GCC_ATTR_ALWAYSINLINE
static inline uint32_t localRead(uint32_t addr)
{
    volatile uint32_t *pReg = (volatile uint32_t*)((uintptr_t)addr);

    return *pReg;
}

GCC_ATTR_ALWAYSINLINE
static inline void localWrite(uint32_t addr, uint32_t val)
{
    volatile uint32_t *pReg = (volatile uint32_t*)((uintptr_t)addr);

    *pReg = val;
}

#endif // LIBLWRISCV_IO_LOCAL_H
