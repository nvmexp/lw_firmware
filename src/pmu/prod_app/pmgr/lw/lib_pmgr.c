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
 * @file   lib_pmgr.c
 * @brief  Shared functions or interfaces for PMGR engine
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"
#include "therm/objtherm.h"
#include "therm/thrmintr.h"
#include "pmgr/lib_pmgr.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Function Prototypes--------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc LibPmgrViolationLwrrGet
 */
LwUFXP4_12
libPmgrViolationLwrrGetEvent
(
    LIB_PMGR_VIOLATION *pViolation,
    LwU8                thrmIdx
)
{
    LwU32 timeEval = pViolation->lastTimeEval.parts.lo;
    LwU32 timeViol = pViolation->lastTimeViol.parts.lo;
    LwU32 lastTimeViol;
    LwU32 bitCount = 0;

    // Code should not get interrupted/preempted when sampling timers.
    appTaskCriticalEnter();
    {
        osPTimerTimeNsLwrrentGet(&pViolation->lastTimeEval);
        (void)thermEventViolationGetTimer32(thrmIdx, &lastTimeViol);
    }
    appTaskCriticalExit();

    // Copy out the lastTimeViol from LwU32 to FLCN_TIMESTAMP.
    pViolation->lastTimeViol.parts.lo = lastTimeViol;

    //
    // Time in [ns] expired since 'lastTimeEval' (limited to 4.29 sec.).
    // Avoiding use of osPTimerTimeNsElapsedGet() to save on reg. accesses.
    //
    timeEval = pViolation->lastTimeEval.parts.lo - timeEval;
    //
    // Time in [ns] event has spent in asserted state (since 'lastTimeEval').
    //
    timeViol = pViolation->lastTimeViol.parts.lo - timeViol;

    // Final violation rate should never exceed 1.0 (timeViol <= timeEval).
    timeViol = LW_MIN(timeViol, timeEval);

    //
    // Violation rate is stored as LwUFXP4_12 so before the division we need
    // to pre-multiply violation time with 2^12, while assuring that it will
    // not overflow LwU32.
    //
    while (timeEval >= BIT(32 - 12))
    {
        timeEval >>= 1;
        bitCount++;
    }
    timeViol <<= (12 - bitCount);

    // Avoiding use of LW_UNSIGNED_ROUNDED_DIV() due to possible overflow.
    return (LwUFXP4_12)(timeViol / timeEval);
}

/*!
 * @copydoc LibPmgrViolationLwrrGet
 */
LwUFXP4_12
libPmgrViolationLwrrGetMon
(
    LIB_PMGR_VIOLATION *pViolation,
    LwU8                thrmIdx
)
{
    THRM_MON      *pThrmMon;
    LwUFXP52_12    num_UFXP52_12;
    LwUFXP52_12    quotient_UFXP52_12;
    FLCN_STATUS    status;
    FLCN_TIMESTAMP timeEval = pViolation->lastTimeEval;
    FLCN_TIMESTAMP timeViol = pViolation->lastTimeViol;

    // Get Thermal Monitor object corresponding to index
    pThrmMon = THRM_MON_GET(thrmIdx);
    if (pThrmMon == NULL)
    {
        PMU_BREAKPOINT();
        return 0;
    }

    // Get current timer values
    status = thrmMonTimer64Get(pThrmMon, &pViolation->lastTimeEval, &pViolation->lastTimeViol);
    if (status != FLCN_OK)
    {
        //
        // Either the therm monitor type is not configured as LEVEL or HW counter
        // overflowed
        //
        PMU_BREAKPOINT();
        return 0;
    }

    // Time in [ns] expired since 'lastTimeEval'.
    lw64Sub(&timeEval.data, &pViolation->lastTimeEval.data, &timeEval.data);

    // Time in [ns] event has spent in asserted state (since 'lastTimeEval').
    lw64Sub(&timeViol.data, &pViolation->lastTimeViol.data, &timeViol.data);

    //
    // Time expired since 'lastTimeEval' cannot be zero. Returning early
    // to avoid division by zero when callwlating current violation rate.
    //
    if (timeEval.data == LW_TYPES_FXP_ZERO)
    {
        PMU_BREAKPOINT();
        return 0;
    }
    // Final violation rate should never exceed 1.0 (timeViol <= timeEval).
    else if (lwU64CmpGT(&timeViol.data, &timeEval.data))
    {
        // Returning 1.0 in LwUFXP4_12
        return LW_TYPES_U32_TO_UFXP_X_Y(4, 12, 1);
    }

    // Compute the current violation rate (as a ratio in LwUFXP4_12)
    num_UFXP52_12 = LW_TYPES_U64_TO_UFXP_X_Y(52, 12, timeViol.data);

    //
    // Avoiding use of lwU64DivRounded due to possible overflow
    // FXP arithmetic: 52.12 / 64.0 = 52.12
    //
    lwU64Div(&quotient_UFXP52_12, &num_UFXP52_12, &timeEval.data);

    // Colwert LwUFXP52_12 to LwUFXP4_12 by taking lower 16 bits
    return LwU32_LO16(LwU64_LO32(quotient_UFXP52_12));
}
