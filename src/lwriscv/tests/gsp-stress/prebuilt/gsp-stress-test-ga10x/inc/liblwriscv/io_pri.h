/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBLWRISCV_IO_PRI_H
#define LIBLWRISCV_IO_PRI_H

#include <stdint.h>
#if LWRISCV_FEATURE_PRI_CHECKED_IO
#include <stdbool.h>
#endif

#include LWRISCV64_MANUAL_ADDRESS_MAP

#include <lwriscv/peregrine.h>

static inline uint32_t priRead(uint32_t addr)
{
    volatile uint32_t *pReg = (volatile uint32_t*)(LW_RISCV_AMAP_PRIV_START + addr);

    return *pReg;
}

static inline void priWrite(uint32_t addr, uint32_t val)
{
    volatile uint32_t *pReg = (volatile uint32_t*)(LW_RISCV_AMAP_PRIV_START + addr);

    *pReg = val;
}

static inline uint32_t falconRead(uint32_t offset)
{
    return priRead(FALCON_BASE + offset);
}

static inline void falconWrite(uint32_t offset, uint32_t val)
{
    priWrite(FALCON_BASE + offset, val);
}

static inline uint32_t riscvRead(uint32_t offset)
{
    return priRead(RISCV_BASE + offset);
}

static inline void riscvWrite(uint32_t offset, uint32_t val)
{
    priWrite(RISCV_BASE + offset, val);
}

#if LWRISCV_FEATURE_PRI_CHECKED_IO
bool priWriteChecked(uint32_t addr, uint32_t val);
bool falconWriteChecked(uint32_t offset, uint32_t val);
bool riscvWriteChecked(uint32_t offset, uint32_t val);
#endif

#endif // LIBLWRISCV_IO_PRI_H
