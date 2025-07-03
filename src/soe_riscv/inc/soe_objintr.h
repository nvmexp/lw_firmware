/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

#ifndef SOE_OBJINTR_H
#define SOE_OBJINTR_H

/*!
 * @file    soe_objintr.h 
 * @copydoc soe_objintr.c
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "config/g_intr_hal.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
LwU32                 ginBase;
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
#define GIN_REG_RD32(addr)                     BAR0_REG_RD32(ginBase + addr)
#define GIN_REG_WR32(addr, data)               BAR0_REG_WR32(ginBase + addr, data)
/* ------------------------ Public Functions ------------------------------- */
#endif // SOE_OBJSAW_H

