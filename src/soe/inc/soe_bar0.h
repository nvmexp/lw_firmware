/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SOE_BAR0_H
#define SOE_BAR0_H

/*!
 * @file soe_bar0.h
 */

#include "regmacros.h"
#include "dev_soe_csb.h"
#include "lwoslayer.h"
#include "soe_objsoe.h"
#include "soe_bar0_hs_inlines.h"

#define BAR0_REG_RD32(addr)                     soeBar0RegRd32_HAL(&Soe, addr)
#define BAR0_REG_RD32_ERRCHK(addr, pReadVal)    soeBar0ErrChkRegRd32_HAL(&Soe, addr, pReadVal)
#define BAR0_REG_WR32(addr, data)               soeBar0RegWr32NonPosted_HAL(&Soe, addr, data)
#define BAR0_REG_WR32_ERRCHK(addr, data)        soeBar0ErrChkRegWr32NonPosted_HAL(&Soe, addr, data)

//
// GPU Config-Space Access through BAR0 Macros {BAR0_CFG_*}
//
#define BAR0_CFG_REG_RD32(addr)                                                \
    BAR0_REG_RD32(DEVICE_BASE(LW_PCFG) + (addr))

#define BAR0_CFG_REG_WR32(addr, data)                                          \
    BAR0_REG_WR32(DEVICE_BASE(LW_PCFG) + (addr), (data))


static inline void _soeBar0RegWr32NonPosted_LR10(LwU32 addr, LwU32 data);

/*!
 * Writes a 32-bit value to the given BAR0 address.  This is the nonposted form
 * (will wait for transaction to complete) of bar0RegWr32Posted.  It is safe to
 * call it repeatedly and in any combination with other BAR0MASTER functions.
 *
 * @param[in]  addr  The BAR0 address to write
 * @param[in]  data  The data to write to the address
 */
static inline void
_soeBar0RegWr32NonPosted_LR10
(
    LwU32  addr,
    LwU32  data
)
{
    // If we are not doing PEH,  we don't need to check the return value.
    (void)_soeBar0WaitIdle_LR10(LW_FALSE);

    REG_WR32(CSB, LW_CSOE_BAR0_ADDR, addr);
    REG_WR32(CSB, LW_CSOE_BAR0_DATA, data);

    REG_WR32(CSB, LW_CSOE_BAR0_CSR,
        DRF_DEF(_CSOE, _BAR0_CSR, _CMD ,        _WRITE) |
        DRF_NUM(_CSOE, _BAR0_CSR, _BYTE_ENABLE, 0xF   ) |
        DRF_DEF(_CSOE, _BAR0_CSR, _TRIG,        _TRUE ));

    (void)REG_RD32(CSB, LW_CSOE_BAR0_CSR);

    // If we are not doing PEH,  we don't need to check the return value.
    (void)_soeBar0WaitIdle_LR10(LW_FALSE);
}


#endif // SOE_BAR0_H

