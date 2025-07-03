/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

#ifndef SOE_OBJPMGR_H
#define SOE_OBJPMGR_H

/*!
 * @file    soe_objpmgr.h 
 * @copydoc soe_objpmgr.c
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "config/g_pmgr_hal.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
extern PMGR_HAL_IFACES PmgrHal;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
void constructPmgr(void) GCC_ATTRIB_SECTION("imem_init", "constructPmgr");
void pmgrPreInit(void)   GCC_ATTRIB_SECTION("imem_init", "pmgrPreInit");

//
// TODO: Hard code the PMGR base until we have discovery in place
// https://confluence.lwpu.com/display/LWSVERIF/LR+Manual+Locations
//
#define PMGR_BAR0_BASE 0x0000D000

//
// Standard macros supporting the falcon bus access functions.
// TODO: These need to be updated once discovery is in place
//
#define PMGR_REG_RD32(addr)                     BAR0_REG_RD32(PMGR_BAR0_BASE+addr)
#define PMGR_REG_RD32_ERRCHK(addr, pReadVal)    BAR0_REG_RD32_ERRCHK(PMGR_BAR0_BASE+addr, pReadVal)
#define PMGR_REG_WR32(addr, data)               BAR0_REG_WR32(PMGR_BAR0_BASE+addr, data)
#define PMGR_REG_WR32_ERRCHK(addr, data)        BAR0_REG_WR32_ERRCHK(PMGR_BAR0_BASE+addr, data)

#define ISR_REG_RD32(addr)                     _soeBar0RegRd32_LR10(addr)
#define ISR_REG_WR32(addr, data)               _soeBar0RegWr32NonPosted_LR10(addr, data)

#define PMGR_ISR_REG_RD32(addr)                 ISR_REG_RD32(PMGR_BAR0_BASE+addr)
#define PMGR_ISR_REG_WR32(addr, data)           ISR_REG_WR32(PMGR_BAR0_BASE+addr, data)

#endif // SOE_OBJPMGR_H
