/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2020 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

#ifndef SOE_OBJINTR_H
#define SOE_OBJINTR_H

/*!
 * @file    soe_objsaw.h 
 * @copydoc soe_objsaw.c
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "config/g_intr_hal.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
extern INTR_HAL_IFACES IntrHal;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
void constructIntr(void) GCC_ATTRIB_SECTION("imem_init", "constructIntr");
#endif // SOE_OBJSAW_H

