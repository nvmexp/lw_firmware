/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

#ifndef SOE_OBJSAW_H
#define SOE_OBJSAW_H

/*!
 * @file    soe_objsaw.h 
 * @copydoc soe_objsaw.c
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "config/g_saw_hal.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
extern SAW_HAL_IFACES SawHal;
LwU32                 sawBase;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
void constructSaw(void) GCC_ATTRIB_SECTION("imem_init", "constructSaw");

//
// Standard macros supporting the falcon bus access functions.
// TODO: These need to be updated once discovery is in place
//


#define SAW_REG_RD32(addr)                     BAR0_REG_RD32(sawBase+addr)
#define SAW_REG_RD32_ERRCHK(addr, pReadVal)    BAR0_REG_RD32_ERRCHK(sawBase+addr, pReadVal)
#define SAW_REG_WR32(addr, data)               BAR0_REG_WR32(sawBase+addr, data)
#define SAW_REG_WR32_ERRCHK(addr, data)        BAR0_REG_WR32_ERRCHK(sawBase+addr, data)

#define ISR_REG_RD32(addr)                     _soeBar0RegRd32_LR10(addr)
#define ISR_REG_WR32(addr, data)               _soeBar0RegWr32NonPosted_LR10(addr, data)

#define SAW_ISR_REG_RD32(addr)                 ISR_REG_RD32(sawBase+addr)
#define SAW_ISR_REG_WR32(addr, data)           ISR_REG_WR32(sawBase+addr, data)

#endif // SOE_OBJSAW_H
