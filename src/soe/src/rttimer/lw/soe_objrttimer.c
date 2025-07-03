/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   soe_objrttimer.c
 * @brief  Top-level timer object for rt timer functionality and HAL infrastructure.
 */

/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"

/* ------------------------- Application Includes --------------------------- */
#include "soe_objhal.h"
#include "soe_objrttimer.h"

#include "config/g_rttimer_hal.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
OBJRTTIMER Rttimer;

/*!
 * A mutex to prevent multiple tasks using the RT timer at the same time
 */
LwrtosSemaphoreHandle RttimerMutex;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */


/*!
 * Construct the RT TIMER object.  This sets up the HAL interface used by the
 * RT TIMER module.
 */
void
constructRTTimer(void)
{
    //
    // Setup the HAL interfaces.
    //
    if (RTTIMER_INDIRECT_HAL_CALLS)
    {
        IfaceSetup->rttimerHalIfacesSetupFn(&Rttimer.hal);
    }
}

/*!
 * @brief Initialization for the RT timer engine.
 */
void
rttimerPreInit(void)
{
    lwrtosSemaphoreCreateBinaryGiven(&RttimerMutex, OVL_INDEX_OS_HEAP);
    if (RttimerMutex == NULL)
    {
        SOE_HALT();
    }
}
