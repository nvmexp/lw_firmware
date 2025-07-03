/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

#ifndef SOE_OBJGIN_H
#define SOE_OBJGIN_H

/*!
 * @file    soe_objgin.h
 * @copydoc soe_objgin.c
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "config/g_gin_hal.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
extern GIN_HAL_IFACES GinHal;
LwU32                 ginBase;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
void constructGin(void) GCC_ATTRIB_SECTION("imem_init", "constructGin");

//
// Standard macros supporting the falcon bus access functions.


#define GIN_REG_RD32(addr)                     BAR0_REG_RD32(ginBase + addr)
#define GIN_REG_RD32_ERRCHK(addr, pReadVal)    BAR0_REG_RD32_ERRCHK(ginBase + addr, pReadVal)
#define GIN_REG_WR32(addr, data)               BAR0_REG_WR32(ginBase + addr, data)
#define GIN_REG_WR32_ERRCHK(addr, data)        BAR0_REG_WR32_ERRCHK(ginBase + addr, data)

#define ISR_REG_RD32(addr)                     _soeBar0RegRd32_LR10(addr)
#define ISR_REG_WR32(addr, data)               _soeBar0RegWr32NonPosted_LR10(addr, data)

#define GIN_ISR_REG_RD32(addr)                 ISR_REG_RD32(ginBase + addr)
#define GIN_ISR_REG_WR32(addr, data)           ISR_REG_WR32(ginBase + addr, data)

#endif // SOE_OBJGIN_H
