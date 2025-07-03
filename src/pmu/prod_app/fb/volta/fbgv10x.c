/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   fbgv10x.c
 * @brief  FB Hal Functions for GV100+
 *
 * FB Hal Functions implement FB related functionalities for GV100
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_fuse.h"
#include "dev_top.h"
#include "dev_fbpa_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objfb.h"
#include "pmu_objfuse.h"
#include "pmu_objpmu.h"
#include "pmu_objtimer.h"
#include "therm/objtherm.h"
#include "config/g_fb_private.h"

/* ------------------------- Static function Prototypes ---------------------*/
/* ------------------------- Macros -----------------------------------------*/

/*!
 * @brief Gets the active FBP mask corresponding to the non floorswept FBPs
 */
LwU32
fbGetActiveFBPMask_GV10X
(
)
{
    LwU32 fbpCount;
    LwU32 activeFBPMask;

    // Retrieve the total number of FBPs.
    fbpCount = REG_RD32(BAR0, LW_PTOP_SCAL_NUM_FBPS);
    fbpCount = DRF_VAL(_PTOP, _SCAL_NUM_FBPAS, _VALUE, fbpCount);

    // Retrieve the active FBPA bitmask.
    activeFBPMask = fuseRead(LW_FUSE_STATUS_OPT_FBP);
    activeFBPMask = DRF_VAL(_FUSE, _STATUS_OPT_FBP, _DATA, activeFBPMask);

    // Value is flipped since the value of an enabled FBP is 0 in HW.
    activeFBPMask = ~activeFBPMask;

    // Mask away disabled FBPs.
    activeFBPMask &= (BIT(fbpCount) - 1);

    return activeFBPMask;
}

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_MEMORY_TEMPERATURE_READ_DELAY)
/*!
 * @brief Checks if memory temperature is available to be read.
 *
 * @return LW_TRUE  Memory temperature reads enabled. 
 *         LW_FALSE Memory temperature reads bypassed.
 */
LwBool
fbIsTempReadAvailable_GV10X(void)
{
    FLCN_TIMESTAMP  now;
    FLCN_TIMESTAMP  maxTimeout;
    LwU32           now1024ns;
    LwU32           maxTimeout1024ns;
    LwU32           timeSinceReset1024ns;

    if (!Therm.bMemTempAvailable)
    {
        osPTimerTimeNsLwrrentGet(&now);
        now1024ns = TIMER_TO_1024_NS(now);

        maxTimeout.data = LW_PFB_TEMP_READ_TIMEOUT_MAX_NS;
        maxTimeout1024ns = TIMER_TO_1024_NS(maxTimeout);

        // Check if the memory temperature read delay has completed.
        timeSinceReset1024ns = now1024ns - Therm.memTempInitTimestamp1024ns;
        Therm.bMemTempAvailable = (timeSinceReset1024ns >= maxTimeout1024ns);
    }

    return Therm.bMemTempAvailable;
}
#endif
