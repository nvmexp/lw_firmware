/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2020 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

/*!
 * @file   soe_objsaw.c
 * @brief  Container-object for SOE SAW routines. Contains generic non-HAL
 *         interrupt-routines plus logic required to hook-up chip-specific
 *         interrupt HAL-routines.
 */

/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"

/* ------------------------- Application Includes --------------------------- */
#include "soe_objhal.h"
#include "soe_objintr.h"

#include "config/g_intr_hal.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
INTR_HAL_IFACES IntrHal;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */

void
constructIntr(void)
{
    IfaceSetup->intrHalIfacesSetupFn(&IntrHal);
}

