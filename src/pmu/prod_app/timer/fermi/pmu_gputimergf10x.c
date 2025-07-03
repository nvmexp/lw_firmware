/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_gputimergf10x.c
 * @brief  HAL-interface for the GF10x GPU Timer Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_timer.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "pmu_objtimer.h"
#include "config/g_timer_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */


/*!
 * @brief Get the GPU's current time in nanoseconds.
 *
 * Gets GPU current time based on it PTIMER time registers. See dev_timer.ref
 * for details.
 *
 * @param[out]  pTime   Current timestamp.
 */
void
timerGetGpuLwrrentNs_GMXXX
(
    FLCN_TIMESTAMP *pTime
)
{
    LwU32 hi;
    LwU32 lo;

    do
    {
        hi = REG_RD32(BAR0, LW_PTIMER_TIME_1);
        lo = REG_RD32(BAR0, LW_PTIMER_TIME_0);
    }
    while (hi != REG_RD32(BAR0, LW_PTIMER_TIME_1));

    // Stick the values into the timestamp.
    pTime->parts.lo = lo;
    pTime->parts.hi = hi;
}

