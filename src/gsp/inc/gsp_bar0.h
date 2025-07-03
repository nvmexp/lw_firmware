/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef GSP_BAR0_H
#define GSP_BAR0_H

/*!
 * @file gsp_bar0.h
 */

#include "regmacros.h"

#if UPROC_RISCV

#include "engine.h"

//
// GPU BAR0/Priv Access Macros {BAR0_*}
//
#define BAR0_REG_RD32(addr)        bar0Read(addr)               
#define BAR0_REG_WR32(addr, data)  bar0Write(addr, data)

//
// GPU Config-Space Access through BAR0 Macros {BAR0_CFG_*}
//

#define BAR0_CFG_REG_RD32(addr) \
    BAR0_REG_RD32(DEVICE_BASE(LW_PCFG) + (addr))

#define BAR0_CFG_REG_WR32(addr, data) \
    BAR0_REG_WR32(DEVICE_BASE(LW_PCFG) + (addr), (data))

#else // UPROC_RISCV

//
// These two functions are actually not implemented since HS-Falcon is not
// actively using them. But we need below declaration to pass the compilation.
// Once HS-Falcon starts using it, we should see compilation error and will
// need to fill their real implementation.
//
extern LwU32 gspBar0RegRd32_TU10X(LwU32 addr);
extern void  gspBar0RegWr32NonPosted_TU10X(LwU32 addr, LwU32 data);

#define BAR0_REG_RD32(addr)        gspBar0RegRd32_TU10X(addr)
#define BAR0_REG_WR32(addr, data)  gspBar0RegWr32NonPosted_TU10X(addr, data)

#endif // UPROC_RISCV

#endif // GSP_BAR0_H
