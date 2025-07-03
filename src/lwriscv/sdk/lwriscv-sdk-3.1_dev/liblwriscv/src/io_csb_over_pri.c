/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include LWRISCV64_MANUAL_ADDRESS_MAP
#include <lwmisc.h>
#if LWRISCV_IS_ENGINE_pmu
#include <dev_pwr_csb.h>
#endif
#include "lwriscv/peregrine.h"

#include "liblwriscv/io.h"

uint32_t csbRead(uint32_t addr)
{
#if LWRISCV_IS_ENGINE_pmu
    if (((uintptr_t)addr <= DEVICE_EXTENT(LW_CPWR_THERM)) &&
            ((uintptr_t)addr >= DEVICE_BASE(LW_CPWR_THERM)))
    {
        return priRead(addr);
    }
    else
#endif
    {
        return priRead((FALCON_BASE +
                        (addr >> LWRISCV_HAS_CSB_OVER_PRI_SHIFT)));
    }
}

void csbWrite(uint32_t addr, uint32_t val)
{
#ifdef LWRISCV_IS_ENGINE_pmu
    if (((uintptr_t)addr <= DEVICE_EXTENT(LW_CPWR_THERM)) &&
        ((uintptr_t)addr >= DEVICE_BASE(LW_CPWR_THERM)))
    {
        priWrite(addr, val);
    }
    else
#endif
    {
        priWrite((FALCON_BASE + (addr >> LWRISCV_HAS_CSB_OVER_PRI_SHIFT)), val);
    }
}
