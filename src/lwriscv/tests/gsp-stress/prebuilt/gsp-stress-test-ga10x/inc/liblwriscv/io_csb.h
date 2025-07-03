/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBLWRISCV_IO_CSB_H
#define LIBLWRISCV_IO_CSB_H

#include <stdint.h>
#include LWRISCV64_MANUAL_ADDRESS_MAP

static inline uint32_t csbRead(uint32_t addr)
{
    volatile uint32_t *pReg = (volatile uint32_t*)(LW_RISCV_AMAP_INTIO_START + addr);

    return *pReg;
}

static inline void csbWrite(uint32_t addr, uint32_t val)
{
    volatile uint32_t *pReg = (volatile uint32_t*)(LW_RISCV_AMAP_INTIO_START + addr);

    *pReg = val;
}

#endif // LIBLWRISCV_IO_CSB_H
