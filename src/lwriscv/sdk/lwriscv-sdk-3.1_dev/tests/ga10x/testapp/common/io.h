/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef IO_H
#define IO_H
#include <lwtypes.h>
#include <lw_riscv_address_map.h>

#include "engine.h"

static inline LwU32 bar0Read(LwU32 addr)
{
    volatile LwU32 *pReg = (volatile LwU32*)(LW_RISCV_AMAP_PRIV_START + addr);

    return *pReg;
}

static inline void bar0Write(LwU32 addr, LwU32 val)
{
    volatile LwU32 *pReg = (volatile LwU32*)(LW_RISCV_AMAP_PRIV_START + addr);

    *pReg = val;
}

static inline LwU32 engineRead(LwU32 offset)
{
    return bar0Read(ENGINE_BASE + offset);
}

static inline void engineWrite(LwU32 offset, LwU32 val)
{
    bar0Write(ENGINE_BASE + offset, val);
}

static inline LwU32 riscvRead(LwU32 offset)
{
    return bar0Read(RISCV_BASE + offset);
}

static inline void riscvWrite(LwU32 offset, LwU32 val)
{
    bar0Write(RISCV_BASE + offset, val);
}
#endif // IO_H
