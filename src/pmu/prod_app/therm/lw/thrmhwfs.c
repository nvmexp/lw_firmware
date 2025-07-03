/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_thrmhwfs.c
 * @brief  Control over HW failsafe slowdown settings
 *
 * The following is a thermal/power control module that aims to control the
 * settings of HW failsafe slowdowns.
 */

/* ------------------------ System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes --------------------------- */
#include "therm/objtherm.h"

#include "g_pmurpc.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Static Function Prototypes  -------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Local Functions  ------------------------------- */
/* ------------------------ Function Prototypes  --------------------------- */
/* ------------------------ Public Functions  ------------------------------ */

/*!
 * @brief   Exelwtes HWFS_CFG RPC.
 *
 * Exelwtes HWFS_CFG RPC by accepting new RM settings and then reevaluating and
 * applying new HW state.
 */
FLCN_STATUS
pmuRpcThermHwfsCfg
(
    RM_PMU_RPC_STRUCT_THERM_HWFS_CFG *pParams
)
{
    FLCN_STATUS status     = FLCN_ERR_NOT_SUPPORTED;
    LwBool      bSupported = LW_FALSE;

    // We can accept RM settings for pre GM20X and Pascal and onwards
    if (!PMUCFG_FEATURE_ENABLED(PMU_PRIV_SEC) ||
        PMUCFG_FEATURE_ENABLED(PMU_THERM_SUPPORT_DEDICATED_OVERT))
    {
        bSupported = LW_TRUE;
    }

    //
    // Configuring incorrect settings for OVERT may damage the chip and so
    // for GM20X, we're simply rejecting OVERT configuration.
    //
    else if (!PMUCFG_FEATURE_ENABLED(PMU_THERM_SUPPORT_DEDICATED_OVERT) && 
             (pParams->eventId != RM_PMU_THERM_EVENT_OVERT))
    {
        bSupported = LW_TRUE;
    }

    if (bSupported)
    {
        status = thermSlctSetEventSlowdownFactor_HAL(&Therm, pParams->eventId,
                                                             pParams->factor);
    }

    return status;
}
