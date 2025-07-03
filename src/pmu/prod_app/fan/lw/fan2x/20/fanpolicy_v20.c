/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  fanpolicy_v20.c
 * @brief FAN Fan Policy Model V20
 *
 * This module is a collection of functions managing and manipulating state
 * related to fan policies in the Fan Policy Table V20.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "fan/objfan.h"
#include "therm/objtherm.h"
#include "main.h"
#include "lwostimer.h"
#include "lwoslayer.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static OsTimerCallback   (s_fanPolicyCallbackGeneric)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommon", "s_fanPolicyCallbackGeneric")
    GCC_ATTRIB_USED();
static OsTmrCallbackFunc (s_fanPolicyOsCallbackGeneric)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommon", "s_fanPolicyOsCallbackGeneric")
    GCC_ATTRIB_USED();

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_MULTI_FAN)
static LwU32             s_fanPolicyCallbackGetTime(FAN_POLICY *pPol)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommon", "s_fanPolicyCallbackGetTime");
static LwU32             s_fanPolicyCallbackUpdateTime(FAN_POLICY *pPol, LwU32 timeus)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommon", "s_fanPolicyCallbackUpdateTime");
static FLCN_STATUS       s_fanPolicyCallbackMultiFan(void)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommon", "s_fanPolicyCallbackMultiFan");
#endif

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
OS_TMR_CALLBACK OsCBFan;
#endif

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Constructor for the FAN_POLICY_V20 super-class.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
fanPolicyGrpIfaceModel10ObjSetImpl_V20
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_FAN_FAN_POLICY_BOARDOBJ_SET_V20 *pSet =
        (RM_PMU_FAN_FAN_POLICY_BOARDOBJ_SET_V20 *)pBoardObjDesc;
    FAN_POLICY_V20         *pPolicy;
    FLCN_STATUS             status;

    if (!FAN_COOLER_INDEX_IS_VALID(pSet->coolerIdx))
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto fanPolicyGrpIfaceModel10ObjSetImpl_V20_exit;
    }

    // Call super-class implementation.
    status = fanPolicyGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto fanPolicyGrpIfaceModel10ObjSetImpl_V20_exit;
    }
    pPolicy  = (FAN_POLICY_V20 *)*ppBoardObj;

    // Set member variables.
    pPolicy->coolerIdx          = pSet->coolerIdx;
    pPolicy->samplingPeriodms   = pSet->samplingPeriodms;

fanPolicyGrpIfaceModel10ObjSetImpl_V20_exit:
    return status;
}

/*!
 * FAN_POLICY_V20 implementation.
 *
 * @copydoc FanPolicyLoad
 */
FLCN_STATUS
fanPolicyLoad_V20
(
    FAN_POLICY *pPol
)
{
    FLCN_STATUS status;

    // Call super-class implementation.
    status = fanPolicyLoad_SUPER(pPol);
    if (status != FLCN_OK)
    {
        goto fanPolicyLoad_V20_exit;
    }

#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_MULTI_FAN))
    FAN_POLICY_V20 *pPolicy         = (FAN_POLICY_V20 *)pPol;
    LwU32           lwrrentTimeus   = thermGetLwrrentTimeInUs();

    // Initialize callbackTimeStampus.
    pPolicy->callbackTimestampus = lwrrentTimeus + pPolicy->samplingPeriodms * 1000;
#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_MULTI_FAN)

fanPolicyLoad_V20_exit:
    return status;
}

/*!
 * Load all V20 FAN_POLICIES.
 */
FLCN_STATUS
fanPoliciesLoad_V20(void)
{
#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_MULTI_FAN))
    s_fanPolicyCallbackGeneric(NULL, 0, 0);
#else
    FAN_POLICY     *pPol = NULL;
    LwBoardObjIdx   i;

    BOARDOBJGRP_ITERATOR_BEGIN(FAN_POLICY, pPol, i, NULL)
    {
        // Cache the supported FAN_POLICY object pointer.
        Fan.policies.pPolicySupported = pPol;
    }
    BOARDOBJGRP_ITERATOR_END;

    FAN_POLICY_V20 *pPolicy = (FAN_POLICY_V20 *)Fan.policies.pPolicySupported;

#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    osTmrCallbackCreate(&OsCBFan,                                // pCallback
        OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED,  // type
        OVL_INDEX_IMEM(thermLibFanCommon),                       // ovlImem
        s_fanPolicyOsCallbackGeneric,                            // pTmrCallbackFunc
        LWOS_QUEUE(PMU, THERM),                                  // queueHandle
        pPolicy->samplingPeriodms * 1000,                        // periodNormalus
        pPolicy->samplingPeriodms * 1000,                        // periodSleepus
        OS_TIMER_RELAXED_MODE_USE_NORMAL,                        // bRelaxedUseSleep
        RM_PMU_TASK_ID_THERM);                                   // taskId
    osTmrCallbackSchedule(&OsCBFan);
#else
    osTimerScheduleCallback(
        &ThermOsTimer,                                            // pOsTimer
        THERM_OS_TIMER_ENTRY_FAN_POLICY_CALLBACKS,                // index
        lwrtosTaskGetTickCount32(),                               // ticksPrev
        pPolicy->samplingPeriodms * 1000,                         // usDelay
        DRF_DEF(_OS_TIMER, _FLAGS, _RELWRRING, _YES_MISSED_EXEC), // flags
        s_fanPolicyCallbackGeneric,                               // pCallback
        NULL,                                                     // pParam
        OVL_INDEX_IMEM(thermLibFanCommon));                       // ovlIdx
#endif  // PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED)
#endif  // PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_MULTI_FAN)

    return FLCN_OK;
}

/*!
 * FAN_POLICY_V20 implementation.
 *
 * @copydoc FanPolicyFanPwmSet
 */
FLCN_STATUS
fanPolicyFanPwmSet_V20
(
    FAN_POLICY *pPol,
    LwBool      bBound,
    LwSFXP16_16 pwm
)
{
    FAN_POLICY_V20 *pPolicy = (FAN_POLICY_V20 *)pPol;
    FAN_COOLER     *pCooler = FAN_COOLER_GET(pPolicy->coolerIdx);
    FLCN_STATUS     status;

    // Sanity check.
    if (pCooler == NULL)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        PMU_BREAKPOINT();
        goto fanPolicyFanPwmSet_V20_exit;
    }

    PMU_HALT_OK_OR_GOTO(status,
        fanCoolerPwmSet(pCooler, bBound, pwm),
        fanPolicyFanPwmSet_V20_exit);

fanPolicyFanPwmSet_V20_exit:
    return status;
}

/*!
 * FAN_POLICY_V20 implementation.
 *
 * @copydoc FanPolicyFanRpmSet
 */
FLCN_STATUS
fanPolicyFanRpmSet_V20
(
    FAN_POLICY *pPol,
    LwS32       rpm
)
{
    FAN_POLICY_V20 *pPolicy = (FAN_POLICY_V20 *)pPol;
    FAN_COOLER     *pCooler = FAN_COOLER_GET(pPolicy->coolerIdx);
    FLCN_STATUS     status;

    // Sanity check.
    if (pCooler == NULL)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        PMU_BREAKPOINT();
        goto fanPolicyFanRpmSet_V20_exit;
    }

    PMU_HALT_OK_OR_GOTO(status,
        fanCoolerRpmSet(pCooler, rpm),
        fanPolicyFanRpmSet_V20_exit);

fanPolicyFanRpmSet_V20_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @ref     OsTmrCallbackFunc
 *
 * Generic fan callback. In future, this will handle multiple FAN_POLICIES.
 * For now, only one FAN_POLICY is supported to quickly unblock GTX7XX.
 */
static FLCN_STATUS
s_fanPolicyOsCallbackGeneric
(
    OS_TMR_CALLBACK *pCallback
)
{
    if (Fan.bFan2xAndLaterEnabled)
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_DEFINE_FAN_CTRL_COMMON_BASE
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPwm)
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libFxpBasic)
        };

        if (!(Fan.bTaskDetached))
        {
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        }

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_MULTI_FAN)
        (void)s_fanPolicyCallbackMultiFan();
#else
        fanPolicyCallback(Fan.policies.pPolicySupported);
#endif  // PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_MULTI_FAN)

        if (!(Fan.bTaskDetached))
        {
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        }
    }

    return FLCN_OK; // NJ-TODO-CB
}

/*!
 * @ref     OsTimerCallback
 *
 * Generic fan callback. In future, this will handle multiple FAN_POLICIES.
 * For now, only one FAN_POLICY is supported to quickly unblock GTX7XX.
 */
static void
s_fanPolicyCallbackGeneric
(
    void   *pParam,
    LwU32   ticksExpired,
    LwU32   skipped
)
{
    s_fanPolicyOsCallbackGeneric(NULL);
}

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_MULTI_FAN)
/*!
 * @brief Returns the time, in microseconds the next callback should
 *        occur for the FAN_POLICY specified by @ref pPolicy.
 *
 * @param[in]  pPol Pointer to the FAN_POLICY object
 *
 * @return LwU32 The time, in microseconds the next callback should occur
 */
static LwU32
s_fanPolicyCallbackGetTime
(
    FAN_POLICY *pPol
)
{
    FAN_POLICY_V20 *pPolicy = (FAN_POLICY_V20 *)pPol;

    return pPolicy->callbackTimestampus;
}

/*!
 * @brief Updates the callback time for the FAN_POLICY specified by @ref pPolicy.
 *
 * @param[in]  pPol     Pointer to the FAN_POLICY object
 * @param[in]  timeus   The current time in microseconds
 *
 * @return LwU32 The updated time, in microseconds the next callback should occur.
 */
static LwU32
s_fanPolicyCallbackUpdateTime
(
    FAN_POLICY   *pPol,
    LwU32         timeus
)
{
    FAN_POLICY_V20 *pPolicy             = (FAN_POLICY_V20 *)pPol;
    LwU32           samplingPeriodus    = (LwU32)pPolicy->samplingPeriodms * 1000;
    LwU32           numElapsedPeriods;

    //
    // The following callwlation is done for a couple of reasons:
    // 1. The RM went into a STATE_UNLOAD that may have caused us to miss one
    //    or more callbacks. We are now handling the callback during the
    //    STATE_LOAD.
    // 2. The overall callback handling took long enough that this policy may
    //    have multiple periods expire.
    //

    numElapsedPeriods = (LwU32)(((timeus - pPolicy->callbackTimestampus) /
                                  samplingPeriodus) + 1);
    pPolicy->callbackTimestampus +=
        (numElapsedPeriods * samplingPeriodus);

    return pPolicy->callbackTimestampus;
}

/*!
 * Fan policy callback helper for multifan.
 */
static FLCN_STATUS
s_fanPolicyCallbackMultiFan(void)
{
    FLCN_STATUS     status = FLCN_OK;
    FAN_POLICY     *pPolicy;
    LwU32           lwrrentTimeus;
    LwU32           policyTimeus;
    LwU32           callbackTimeus;
    LwU32           callbackDelayus;
    LwBoardObjIdx   i;

    do
    {
        LwU32 timeTillCallbackus = FAN_POLICY_TIME_INFINITY;
        LwU32 timeTillPolicyus;

        lwrrentTimeus = thermGetLwrrentTimeInUs();

        BOARDOBJGRP_ITERATOR_BEGIN(FAN_POLICY, pPolicy, i, NULL)
        {
            policyTimeus = s_fanPolicyCallbackGetTime(pPolicy);

            //
            // Determine if the policy's callback timer has expired; if so,
            // execute the policy's callback.
            //
            if (osTmrGetTicksDiff(policyTimeus, lwrrentTimeus) >= 0)
            {
                // Execute timer callback
                PMU_HALT_OK_OR_GOTO(status,
                    fanPolicyCallback(pPolicy),
                    s_fanPolicyCallbackMultiFan_exit);

                // Update policy with a new callback time.
                policyTimeus = s_fanPolicyCallbackUpdateTime(pPolicy, lwrrentTimeus);
            }

            //
            // In order to find the the earliest policy expiration, we must use
            // the difference between the policy time and the current time.
            // The above check 'osTmrGetTicksDiff(policyTimeus, lwrrentTimeus)'
            // ensures that this difference is always positive so no need to
            // check for signed overflow.
            //
            timeTillPolicyus = policyTimeus - lwrrentTimeus;

            // Save the earliest policy expiration to timeTillCallback.
            timeTillCallbackus = LW_MIN(timeTillCallbackus, timeTillPolicyus);
        }
        BOARDOBJGRP_ITERATOR_END;

        callbackTimeus = lwrrentTimeus + timeTillCallbackus;

        //
        // Need to determine if a lower index policy's timer expired while
        // processing higher index policies (i.e. policy B's timer expired
        // while processing policy D's timer). Repeat the process if a timer
        // expired while processing.
        //
        lwrrentTimeus = thermGetLwrrentTimeInUs();
    } while (osTmrGetTicksDiff(callbackTimeus, lwrrentTimeus) >= 0);

    callbackDelayus = callbackTimeus - lwrrentTimeus;

    if (!OS_TMR_CALLBACK_WAS_CREATED(&OsCBFan))
    {
        osTmrCallbackCreate(&OsCBFan,                       // pCallback
            OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_ACTUAL_END,   // type
            OVL_INDEX_IMEM(thermLibFanCommon),              // ovlImem
            s_fanPolicyOsCallbackGeneric,                   // pTmrCallbackFunc
            LWOS_QUEUE(PMU, THERM),                         // queueHandle
            callbackDelayus,                                // periodNormalus
            callbackDelayus,                                // periodSleepus
            OS_TIMER_RELAXED_MODE_USE_NORMAL,               // bRelaxedUseSleep
            RM_PMU_TASK_ID_THERM);                          // taskId
        osTmrCallbackSchedule(&OsCBFan);
    }
    else
    {
        // Update callback period
        osTmrCallbackUpdate(&OsCBFan, callbackDelayus, callbackDelayus,
        OS_TIMER_RELAXED_MODE_USE_NORMAL, OS_TIMER_UPDATE_USE_BASE_LWRRENT);
    }

s_fanPolicyCallbackMultiFan_exit:
    return status;
}
#endif
