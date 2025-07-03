/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   soe_objcore.c
 * @brief  Container-object for SOE Core routines.
 */

/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"

/* ------------------------- Application Includes --------------------------- */
#include "soe_objhal.h"
#include "soe_objcore.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

/*!
* Main structure for all SOE Core Functions.
*/
OBJCORE Core;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Construct the CORE object.  This includes the HAL interface as well as all
 * software objects used by the Core module.
 *
 * @return  FLCN_OK     On success
 */
FLCN_STATUS
constructCore(void)
{
    memset(&Core, 0, sizeof(OBJCORE));
    IfaceSetup->coreHalIfacesSetupFn(&Core.hal);
    return FLCN_OK;
}

/*!
 * Pre-init the CORE task
 */
void
corePreInit(void)
{
    osTimerInitTracked(OSTASK_TCB(CORE), &CoreOsTimer,
                       CORE_OS_TIMER_ENTRY_NUM_ENTRIES);

    // Do platform-specific pre-init
    corePreInit_HAL();
}

