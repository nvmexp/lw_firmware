/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrpolicies_3x.c
 * @brief  Interface specification for PWR_POLICIES.
 *
 * The PWR_POLICIES is a group of PWR_POLICY and some global data fields.
 * Functions inside this file applies to the whole PWR_POLICIES instead of
 * a single PWR_POLICY. for PWR_POLICY functions, please refer to
 * pwrpolicy_3x.c.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmu_objperf.h"
#include "pmgr/3x/pwrpolicies_3x.h"
#include "logger.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_CALLBACKS_CENTRALIZED))
OS_TMR_CALLBACK OsCBPwrPolicy;
#endif

/* ------------------------- Macros and Defines ----------------------------- */
static OsTimerCallback      (s_pwrPoliciesEvaluateCallback_3X)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyCallback", "s_pwrPoliciesEvaluateCallback_3X")
    GCC_ATTRIB_USED();
static OsTmrCallbackFunc    (s_pwrPoliciesEvaluateOsCallback_3X)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyCallback", "s_pwrPoliciesEvaluateOsCallback_3X")
    GCC_ATTRIB_USED();

#if (!PMUCFG_FEATURE_ENABLED(PMU_PMGR_CALLBACKS_CENTRALIZED))
static void   _pwrPolicies3XCheckAndScheduleLowSamplingCallback(LwBool bLowSampling);
#endif

static FLCN_STATUS s_pwrPoliciesGpumonPwrSampleGet(LW2080_CTRL_PMGR_GPUMON_PWR_SAMPLE *pSample);
static FLCN_STATUS s_pwrPoliciesGpumonPwrSampleLog(void);

/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * PWR_POLICIES_3X specific implementation
 *
 * @copydoc PwrPoliciesConstructHeader
 */
FLCN_STATUS
pwrPoliciesConstructHeader_3X
(
    PWR_POLICIES                                 **ppPolicies,
    PWR_MONITOR                                   *pMon,
    RM_PMU_PMGR_PWR_POLICY_BOARDOBJGRP_SET_HEADER *pHdr
)
{
    PWR_POLICIES_3X *pPolicies = (PWR_POLICIES_3X *)*ppPolicies;

    (void)pwrPoliciesConstructHeader(ppPolicies, pMon, pHdr);
    pPolicies->baseSamplePeriodms      = pHdr->baseSamplePeriod;
    pPolicies->minClientSamplePeriodms = pHdr->minClientSamplePeriod;
    pPolicies->lowSamplingMult         = pHdr->lowSamplingMult;
    pPolicies->graphicsClkDom          = pHdr->graphicsClkDom;

    //
    // If the lowSamplingMult value is set to 0, which is the case in earlier
    // GM204 VBIOS, change it to 1 i.e. no low sampling functionality will
    // be introduced.
    //
    if (pPolicies->lowSamplingMult == 0)
    {
        pPolicies->lowSamplingMult = 1;
    }

    return FLCN_OK;
}

/*!
 * Schedule policy evaluation callback for Power Policy 3.x
 *
 * @param[in]   ticksNow        OS ticks timestamp to synchronize all load() code
 * @param[in]   bLowSampling
 *      Boolean indicating if this is scheduling Low Sampling callback
 */
void
pwrPolicies3XScheduleEvaluationCallback
(
    LwU32   ticksNow,
    LwBool  bLowSampling
)
{
    LwU32 samplePeriodus =
        ((PWR_POLICIES_3X *)Pmgr.pPwrPolicies)->baseSamplePeriodms * 1000;

    // Sanity check
    PMU_HALT_COND(((PWR_POLICIES_3X *)Pmgr.pPwrPolicies)->lowSamplingMult != 0);
    PMU_HALT_COND((((PWR_POLICIES_3X *)Pmgr.pPwrPolicies)->lowSamplingMult *
                samplePeriodus) <= LW_U32_MAX);

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_CALLBACKS_CENTRALIZED))
    {
        if (!OS_TMR_CALLBACK_WAS_CREATED(&OsCBPwrPolicy))
        {
            osTmrCallbackCreate(&OsCBPwrPolicy,                          // pCallback
                OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED,  // type
                OVL_INDEX_IMEM(pmgrPwrPolicyCallback),                   // pOverlays
                s_pwrPoliciesEvaluateOsCallback_3X,                      // pTmrCallbackFunc
                LWOS_QUEUE(PMU, PMGR),                                   // queueHandle
                samplePeriodus,                                          // periodNormalus
                (samplePeriodus *                                        // periodSleepus
                    ((PWR_POLICIES_3X *)Pmgr.pPwrPolicies)->lowSamplingMult),
                OS_TIMER_RELAXED_MODE_USE_NORMAL,                        // bRelaxedUseSleep
                RM_PMU_TASK_ID_PMGR);                                    // taskId

            osTmrCallbackSchedule(&OsCBPwrPolicy);
        }
        else
        {
            osTmrCallbackSchedule(&OsCBPwrPolicy);

            // Update callback periods
            osTmrCallbackUpdate(&OsCBPwrPolicy, samplePeriodus, (samplePeriodus *
                    ((PWR_POLICIES_3X *)Pmgr.pPwrPolicies)->lowSamplingMult),
                    OS_TIMER_RELAXED_MODE_USE_NORMAL,
                    OS_TIMER_UPDATE_USE_BASE_LWRRENT);
        }

    }
#else
    {
        if (bLowSampling)
        {
            samplePeriodus *=
                ((PWR_POLICIES_3X *)Pmgr.pPwrPolicies)->lowSamplingMult;
        }

        osTimerScheduleCallback(
                &PmgrOsTimer,
                PMGR_OS_TIMER_ENTRY_PWR_POLICY_3X_EVAL_CALLBACK,
                ticksNow,
                samplePeriodus,
                DRF_DEF(_OS_TIMER, _FLAGS, _RELWRRING, _YES_MISSED_SKIP),
                s_pwrPoliciesEvaluateCallback_3X,
                NULL,
                OVL_INDEX_IMEM(pmgrPwrPolicyCallback));
    }
#endif
}

/*!
 * Cancel policy evaluation callback for Power Policy 3.x
 * This function cancels callback for each power policy.
 */
void pwrPolicies3XCancelEvaluationCallback(void)
{
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_CALLBACKS_CENTRALIZED))
    {
        osTmrCallbackCancel(&OsCBPwrPolicy);
    }
#else
    {
        osTimerDeScheduleCallback(&PmgrOsTimer,
                PMGR_OS_TIMER_ENTRY_PWR_POLICY_3X_EVAL_CALLBACK);
    }
#endif
}

/*!
 * @copydoc PwrPoliciesProcessPerfStatus
 */
void
pwrPoliciesProcessPerfStatus_3X(RM_PMU_PERF_STATUS *pStatus)
{
#if (!PMUCFG_FEATURE_ENABLED(PMU_PMGR_CALLBACKS_CENTRALIZED))
    PERF_DOMAIN_GROUP_LIMITS domGrpLimits;
    LwBool                   bLowSampling = LW_FALSE;

    // Handle Policy 3.X evaluation callback scheduling
    if ((Pmgr.pPwrPolicies->version >= RM_PMU_PMGR_PWR_POLICY_DESC_TABLE_VERSION_3X) &&
        (perfVoltageuVGet() != 0) &&
        ((PWR_POLICIES_3X *)Pmgr.pPwrPolicies)->lowSamplingMult != 1)
    {
        //
        // Switch to Low Sampling callback, if in the new PerfStatus we are in
        // lowest Pstate, and lwrrently PMU is not requesting to cap at lowest.
        // For all other cases, continue to use normal sampling.
        //
        pwrPolicyDomGrpLimitsGetMin(&domGrpLimits);

        if ((domGrpLimits.values[RM_PMU_PERF_DOMAIN_GROUP_PSTATE] != 0) &&
            (pStatus->domGrps[RM_PMU_DOMAIN_GROUP_PSTATE].value == 0))
        {
            bLowSampling = LW_TRUE;
        }

        _pwrPolicies3XCheckAndScheduleLowSamplingCallback(bLowSampling);
    }
#endif
}

/*!
 * Power Policies 3.X main evaluation function.
 *
 * @copydoc PwrPoliciesEvaluate
 */
void
pwrPoliciesEvaluate_3X
(
    PWR_POLICIES    *pPolicies,
    LwU32            expiredPolicyMask,
    BOARDOBJGRPMASK *pExpiredViolationMask
)
{
    pwrPoliciesEvaluate_SUPER(pPolicies, expiredPolicyMask, pExpiredViolationMask);

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_GPUMON_PWR_3X))
    {
        if (PMGR_PWR_POLICY_IDX_IS_ENABLED(LW2080_CTRL_PMGR_PWR_POLICY_IDX_TGP) &&
            ((PMGR_GET_PWR_POLICY_IDX_MASK(LW2080_CTRL_PMGR_PWR_POLICY_IDX_TGP) &
              expiredPolicyMask) != 0))
        {
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, loggerWrite)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                s_pwrPoliciesGpumonPwrSampleLog();
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        }
    }
}

/*!
 * @copydoc PwrPolicies3xLoad
 */
FLCN_STATUS
pwrPolicies3xLoad
(
    PWR_POLICIES_3X *pPolicies3x,
    LwU32            ticksNow
)
{
    FLCN_STATUS status = FLCN_OK;

    // Load PWR_POLICIES_35-specific SW state.
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_35))
    {
        status = pwrPolicies3xLoad_35(pPolicies3x, ticksNow);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto pwrPolicies3xLoad_exit;
        }
    }

    // Schedule the callback to run.
    pwrPolicies3XScheduleEvaluationCallback(ticksNow, LW_FALSE);

pwrPolicies3xLoad_exit:
    return status;
}

/* ------------------------- Static Functions ------------------------------- */
/*!
 * Policy 3.X evaluation routine.
 */
static void
s_pwrPoliciesEvaluateCallback_3X
(
    void   *pParam,
    LwU32   ticksExpired,
    LwU32   skipped
)
{
    pwrPolicies3xCallbackBody(ticksExpired);
}

/*!
 * @ref     OsTmrCallbackFunc
 *
 * Policy 3.X evaluation routine.
 */
static FLCN_STATUS
s_pwrPoliciesEvaluateOsCallback_3X
(
    OS_TMR_CALLBACK *pCallback
)
{
    pwrPolicies3xCallbackBody(
        OS_TMR_CALLBACK_TICKS_EXPIRED_GET(pCallback));

    return FLCN_OK; // NJ-TODO-CB
}

#if (!PMUCFG_FEATURE_ENABLED(PMU_PMGR_CALLBACKS_CENTRALIZED))
/*!
 * Check the request from @ref pwrPoliciesProcessPerfStatus_3X and schedule new
 * evaluation callback accordingly.
 *
 * @param[in] bLowSampling  Boolean indicating request type
 */
static void
_pwrPolicies3XCheckAndScheduleLowSamplingCallback
(
    LwBool bLowSampling
)
{
    OS_TIMER_ENTRY *pEntry =
        &PmgrOsTimer.pEntries[PMGR_OS_TIMER_ENTRY_PWR_POLICY_3X_EVAL_CALLBACK];
    LwU32   ticksDelay = pEntry->ticksDelay;
    LwU32   ticksDelayFromBase =
        ((PWR_POLICIES_3X *)Pmgr.pPwrPolicies)->baseSamplePeriodms * 1000 / LWRTOS_TICK_PERIOD_US;

    //
    // If requesting to use low sampling and current sampling is normal, switch
    // to use low sampling frequency.
    //
    if (bLowSampling && (ticksDelay == ticksDelayFromBase))
    {
        pwrPolicies3XScheduleEvaluationCallback(pEntry->ticksPrev, LW_TRUE);
    }

    //
    // On the other side, if we are requesting to use normal sampling and
    // current sampling is low, switch to normal sampling.
    //
    if (!bLowSampling && (ticksDelay > ticksDelayFromBase))
    {
        pwrPolicies3XScheduleEvaluationCallback(pEntry->ticksPrev, LW_FALSE);
    }
}
#endif

/*!
 * Helper function to get a GPU monitoring PWR_POLICIES status sample.
 * Lwrrently, it gets only TGP policy status.
 *
 * @param[out] pSample
 *      Logging sample with instantaneous TGP power value
 *
 * @return FLCN_OK
 *     Sample successfully retrieved
 *
 * @return FLCN_ERR_ILWALID_STATE
 *     Error accessing the TGP power policy state
 */
static FLCN_STATUS
s_pwrPoliciesGpumonPwrSampleGet
(
    LW2080_CTRL_PMGR_GPUMON_PWR_SAMPLE *pSample
)
{
    PWR_POLICY *pPolicy;

    pPolicy = PMGR_GET_PWR_POLICY_IDX(LW2080_CTRL_PMGR_PWR_POLICY_IDX_TGP);
    if (pPolicy == NULL)
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    pSample->type = LW2080_CTRL_PMGR_GPUMON_PWR_SAMPLE_TYPE_POLICY_STATUS;
    pSample->data.polStatus.valueLwrr = pPolicy->valueLwrr;
    timerGetGpuLwrrentNs_HAL(&Timer, (FLCN_TIMESTAMP *)&pSample->base.timeStamp);
    return FLCN_OK;
}

/*!
 * Helper function to log PWR_POLICIES's status based GPU monitoring sample
 * with data from the last iteration.
 *
 * @return FLCN_OK
 *     Sample successfully logged
 *
 * @return FLCN_ERROR
 *     Unexpected internal error
 */
static FLCN_STATUS
s_pwrPoliciesGpumonPwrSampleLog(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8 sampleBuf[RM_PMU_LOGGER_SAMPLE_SIZE_POWER*2];
    LW2080_CTRL_PMGR_GPUMON_PWR_SAMPLE *pSample;

    // Keep size a power of 2 to reduce DMA transfers between PMU and sysmem
    ct_assert(sizeof(LW2080_CTRL_PMGR_GPUMON_PWR_SAMPLE) == RM_PMU_LOGGER_SAMPLE_SIZE_POWER);

    // Ensure alignment of sample structure
    pSample = (LW2080_CTRL_PMGR_GPUMON_PWR_SAMPLE*)
        LW_ALIGN_UP((LwUPtr)&sampleBuf[0], RM_PMU_LOGGER_SAMPLE_SIZE_POWER);

    status = s_pwrPoliciesGpumonPwrSampleGet(pSample);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        return status;
    }

    loggerWrite(RM_PMU_LOGGER_SEGMENT_ID_POWER,
        sizeof(LW2080_CTRL_PMGR_GPUMON_PWR_SAMPLE),
        pSample);
    return status;
}

