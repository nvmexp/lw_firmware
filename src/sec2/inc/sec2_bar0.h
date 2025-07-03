/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SEC2_BAR0_H
#define SEC2_BAR0_H

/*!
 * @file sec2_bar0.h
 */

#include "regmacros.h"

#define BAR0_REG_RD32(addr)                     sec2Bar0RegRd32_HAL(&Sec2, addr)
#define BAR0_REG_RD32_ERRCHK(addr, pReadVal)    sec2Bar0ErrChkRegRd32_HAL(&Sec2, addr, pReadVal)
#define BAR0_REG_WR32(addr, data)               sec2Bar0RegWr32NonPosted_HAL(&Sec2, addr, data)
#define BAR0_REG_WR32_ERRCHK(addr, data)        sec2Bar0ErrChkRegWr32NonPosted_HAL(&Sec2, addr, data)

//
// GPU Config-Space Access through BAR0 Macros {BAR0_CFG_*}
//
#define BAR0_CFG_REG_RD32(addr)                                                \
    BAR0_REG_RD32(DEVICE_BASE(LW_PCFG) + (addr))

#define BAR0_CFG_REG_WR32(addr, data)                                          \
    BAR0_REG_WR32(DEVICE_BASE(LW_PCFG) + (addr), (data))

#endif // SEC2_BAR0_H

