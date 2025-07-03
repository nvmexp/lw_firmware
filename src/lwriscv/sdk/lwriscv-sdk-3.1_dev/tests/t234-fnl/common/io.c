/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include "engine.h"
#include "io.h"

uint32_t localRead(uint32_t addr)
{
    volatile uint32_t *pReg = (volatile uint32_t*)((uint64_t)addr);

    return *pReg;
}

void localWrite(uint32_t addr, uint32_t val)
{
    volatile uint32_t *pReg = (volatile uint32_t*)((uint64_t)addr);

    *pReg = val;
}
