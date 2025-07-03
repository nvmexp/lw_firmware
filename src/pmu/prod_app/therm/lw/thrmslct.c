/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_thrmslct.c
 * @brief  Thermal Slowdown Control Task
 *
 * The following is a thermal/power control module that aims to control the
 * thermal slowdown feature on PMU.
 *
 */

/* ------------------------ System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes --------------------------- */
#include "ctrl/ctrl2080/ctrl2080thermal.h"

#include "pmu_objpmu.h"
#include "pmu_objtimer.h"
#include "therm/objtherm.h"
#include "therm/thrmslct.h"

#include "g_pmurpc.h"
#include "g_pmurmifrpc.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Static Function Prototypes  -------------------- */
/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Local Functions  ------------------------------- */
/* ------------------------ Function Prototypes  --------------------------- */
/* ------------------------ Public Functions  ------------------------------ */
/*!
 * Handles requests from RM to enable/disable HW slowdown notifications.
 * Add arbitration between PMU and RM requests if in the future PMU is
 * interested in these HW failsafe slowdown notifications.
 */
FLCN_STATUS
pmuRpcThermHwSlowdownNotification
(
    RM_PMU_RPC_STRUCT_THERM_HW_SLOWDOWN_NOTIFICATION *pParams
)
{
    Therm.evt.bNotifyRM = pParams->bEnable;

    return FLCN_OK;
}

/*!
 * Handler for processing recently triggered HW failsafe slowdown interrupts.
 * Sends notification to interested parties (lwrrently RM) after applying
 * timestamp based event filtering mechanism.
 *
 * @param[in] maskAsserted
 *                  GPU-HAL independent HW failsafe slowdown interrupt mask.
 */
void
thermSlowCtrlHwFsHandleTriggeredSlowIntr
(
    LwU32   maskAsserted
)
{
    PMU_RM_RPC_STRUCT_THERM_HW_SLOWDOWN_NOTIFICATION rpc;
    FLCN_TIMESTAMP      now;
    FLCN_STATUS         status;
    LwU8                evtIdx;
    LwU8                packed;

    // Capture lwrent time
    osPTimerTimeNsLwrrentGet(&now);

    //
    // Apply timestamp based event filtering to send at the most one
    // notification message per HW failsafe per second.
    //
    FOR_EACH_INDEX_IN_MASK(32, evtIdx, maskAsserted)
    {
        packed = thermEventViolationPackEventIndex_HAL(&Therm, evtIdx);

        if (now.data >= Therm.evt.pNextNotif[packed].data)
        {
            Therm.evt.pNextNotif[packed].data = now.data +
                THERM_PMU_EVENT_RM_NOTIFICATION_FILTER_1S_IN_NS;
        }
        else
        {
            maskAsserted &= ~BIT(evtIdx);
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    //
    // Send slowdown notification to RM with a mask of HW failsafe slowdown
    // interrupts which were recently triggered.
    //
    if (maskAsserted != RM_PMU_THERM_EVENT_MASK_NONE)
    {
        rpc.mask = maskAsserted;

        PMU_RM_RPC_EXELWTE_BLOCKING(status, THERM, HW_SLOWDOWN_NOTIFICATION, &rpc);
    }
}

/*!
 * @brief   Exelwtes SLCT RPC.
 *
 * Exelwtes SLCT RPC (@ref RM_PMU_THERM_RPC_SLCT) by accepting new RM settings
 * and then reevaluating and applying new HW state.
 */
FLCN_STATUS
pmuRpcThermSlct
(
    RM_PMU_RPC_STRUCT_THERM_SLCT *pParams
)
{
    LwBool  bApply = LW_TRUE;

    // PMU should evaluate internal requests here (once required).

    //
    // Configuring incorrect settings for OVERT may damage the chip and so
    // from GM20X+, we're simply rejecting OVERT configuration.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PRIV_SEC) &&
        !(PMUCFG_FEATURE_ENABLED(PMU_THERM_SUPPORT_DEDICATED_OVERT)))
    {
        LwU32   overtUseMask =
            thermEventUseMaskGet_HAL(&Therm) & BIT(RM_PMU_THERM_EVENT_OVERT);
        LwU32   overtReqMask =
            pParams->maskEnabled & BIT(RM_PMU_THERM_EVENT_OVERT);

        // Apply settings only if OVERT isn't configured.
        bApply = ((overtUseMask ^ overtReqMask) == RM_PMU_THERM_EVENT_MASK_NONE);
    }

    return bApply ?
        thermSlctEventsEnablementSet_HAL(&Therm, pParams->maskEnabled) :
        FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @brief   Exelwtes SLCT_TEMP_THRESHOLD_SET RPC.
 *
 * Exelwtes SLCT_TEMP_THRESHOLD_SET RPC by accepting new temperature threshold
 * and applying to the specified eventId.
 */
FLCN_STATUS
pmuRpcThermSlctEventTempThSet
(
    RM_PMU_RPC_STRUCT_THERM_SLCT_EVENT_TEMP_TH_SET *pParams
)
{
    LwBool bSupported = LW_FALSE;

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

    return bSupported ?
        thermSlctSetEventTempThreshold_HAL(&Therm, pParams->eventId,
                                           pParams->tempThreshold) :
        FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @brief  Exelwtes HWFS EVENT STATUS GET RPC
 *
 * Gets the current state of events i.e. their violation counters etc.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_THERM_HWFS_EVENT_STATUS_GET.
 */
FLCN_STATUS
pmuRpcThermHwfsEventStatusGet
(
    RM_PMU_RPC_STRUCT_THERM_HWFS_EVENT_STATUS_GET *pParams
)
{
    LwU8  eventId;

    FOR_EACH_INDEX_IN_MASK(32, eventId, pParams->eventMask)
    {

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_VIOLATION_COUNTERS_EXTENDED)
        if (thermEventViolationGetCounter64(eventId,
                &pParams->status[eventId].violCounter) != FLCN_OK)
        {
            PMU_HALT();
        }
#else
        pParams->status[eventId].violCounter.hi = 0;
        if (thermEventViolationGetCounter32(eventId,
                &pParams->status[eventId].violCounter.lo) != FLCN_OK)
        {
            PMU_HALT();
        }
#endif

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_VIOLATION_TIMERS_EXTENDED)
        {
            FLCN_TIMESTAMP timeStamp;

            if (thermEventViolationGetTimer64(eventId, &timeStamp) != FLCN_OK)
            {
                PMU_HALT();
            }

            pParams->status[eventId].violTimerns = timeStamp.parts;
        }
#else
        pParams->status[eventId].violTimerns.hi = 0;
        if (thermEventViolationGetTimer32(eventId,
                &pParams->status[eventId].violTimerns.lo) != FLCN_OK)
        {
            PMU_HALT();
        }
#endif
    }
    FOR_EACH_INDEX_IN_MASK_END;

    return FLCN_OK;
}

