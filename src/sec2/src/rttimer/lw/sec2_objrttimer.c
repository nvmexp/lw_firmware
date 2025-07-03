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
 * @file   sec2_objrttimer.c
 * @brief  Top-level timer object for rt timer functionality and HAL infrastructure.
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_objhal.h"
#include "sec2_objrttimer.h"

#include "config/g_rttimer_hal.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

/*!
 * A mutex to prevent multiple tasks using the RT timer at the same time
 */
LwrtosSemaphoreHandle RttimerMutex;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Initialization for the RT timer engine.
 */
void
rttimerPreInit(void)
{
    lwrtosSemaphoreCreateBinaryGiven(&RttimerMutex, OVL_INDEX_OS_HEAP);
    if (RttimerMutex == NULL)
    {
        SEC2_HALT();
    }
}
