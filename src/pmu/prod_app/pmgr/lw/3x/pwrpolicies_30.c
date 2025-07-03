/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrpolicies_30.c
 * @brief  Interface specification for PWR_POLICIES_30.
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
#include "pmgr/3x/pwrpolicies_30.h"
#include "logger.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static LwBool s_pwrPolicies3XExpiredMaskGet(LwU32 ticksNow, LwU32 *pMask)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyCallback", "s_pwrPolicies3XExpiredMaskGet");
static FLCN_STATUS s_pwrPolicies30Filter(PWR_POLICIES *pPolicies, LwU32 *pPolicyMask)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyCallback", "s_pwrPolicies30Filter");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * PWR_POLICIES_30 main evaluation function.
 *
 * @copydoc PwrPoliciesEvaluate
 */
void
pwrPoliciesEvaluate_30
(
    PWR_POLICIES    *pPolicies,
    LwU32            expiredPolicyMask,
    BOARDOBJGRPMASK *pExpiredViolationMask
)
{
    //
    // Update all PWR_POLICYs to cache and filter their latest readings from
    // PWR_CHANNELs.
    //
    if (FLCN_OK != s_pwrPolicies30Filter(pPolicies, &expiredPolicyMask))
    {
        return;
    }

    pwrPoliciesEvaluate_3X(pPolicies, expiredPolicyMask, pExpiredViolationMask);
}

/*!
 * PWR_POLICIES_30 implementation of PwrPolicies3xCallbackBody virtual interface.
 *
 * @copydoc PwrPolicies3xCallbackBody
 */
void
pwrPolicies3xCallbackBody_30
(
    LwU32   ticksExpired
)
{
    BOARDOBJGRPMASK_E32 expiredMaskViolation;
    LwU32               expiredMaskPolicy  = 0;
    LwBool              bViolationsExpired = LW_FALSE;
    LwBool              bPoliciesExpired   = LW_FALSE;

    // Handle PWR_VIOLATION objects before PWR_POLICY ones.
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_VIOLATION))
    {
        bViolationsExpired =
            pwrViolationsExpiredMaskGet(ticksExpired, &expiredMaskViolation);
    }

    bPoliciesExpired =
        s_pwrPolicies3XExpiredMaskGet(ticksExpired, &expiredMaskPolicy);

    //
    // This code path was modified to fix bug due to compiler optimization where
    // If the first boolean is true, logical OR operation will never check for
    // the rest of the booleans as the output will always be TRUE.
    //
    if (bViolationsExpired || bPoliciesExpired)
    {
        PWR_POLICIES *pPolicies = PWR_POLICIES_GET();
        PWR_POLICIES_EVALUATE_PROLOGUE(pPolicies);
        {
            pwrPoliciesEvaluate_30(pPolicies, expiredMaskPolicy,
                                &(expiredMaskViolation.super));
        }
        PWR_POLICIES_EVALUATE_EPILOGUE(pPolicies);
    }
}

/* ------------------------- Static Functions ------------------------------- */
/*!
 * Builds a bit-mask of expired PWR_POLICY objects while updating their new
 * expiration information.
 *
 * @param[in]   ticksNow   OS ticks time at which we evaluate policy expiration.
 * @param[in]   pMask      Pointer of mask to be updated with expiration info.
 *
 * @return  Set if at least one PWR_POLICY object has expired.
 *          Used by the caller to avoid checking if mask is zero.
 */
static LwBool
s_pwrPolicies3XExpiredMaskGet
(
    LwU32   ticksNow,
    LwU32  *pMask
)
{
    LwU32   mask = 0;
    LwU8    i;

    FOR_EACH_INDEX_IN_MASK(32, i, Pmgr.pwr.policies.objMask.super.pData[0])
    {
        PWR_POLICY *pPolicy;

        pPolicy = PWR_POLICY_GET(i);
        if (pPolicy == NULL)
        {
            PMU_BREAKPOINT();
            return LW_FALSE;
        }

        if (pwrPolicy30CheckExpiredAndUpdate(pPolicy, ticksNow))
        {
            mask |= BIT(i);
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    *pMask = mask;

    return (mask != 0);
}

/*!
 * Filter all PWR_POLICYs in PWR_POLICIES_30 infrastructure.
 *
 * @params[in]  pPwrPolicies
 *      Pointer to PWR_POLICIES * pointer at which to populate PWR_POLICIES
 *      structure.
 * @params[in]  policyMask
 *      Requested policy mask.
 *
 * @return      FLCN_OK
 *      This function completes successfully.
 * @return      other
 *      Error propagated from inner functions.
 */
static FLCN_STATUS
s_pwrPolicies30Filter
(
    PWR_POLICIES  * pPolicies,
    LwU32          *pPolicyMask
)
{
    LwU32           i;
    LwU32           channelMask = 0;
    FLCN_STATUS     status = FLCN_OK;
    PWR_POLICY      *pPolicy;
    OSTASK_OVL_DESC ovlDescList[] = {
        PWR_POLICIES_3X_FILTER_OVERLAYS
    };

    //
    // Hack to decrease local variable's size to reduce stack usage on
    // GM10x. Cut this array's size to half.
    //
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_2X_REDUCED_CHANNEL_STATUS))
    LW2080_CTRL_PMGR_PWR_TUPLE
            channels[LW2080_CTRL_PMGR_PWR_CHANNEL_MAX_CHANNELS / 2];
#else
    LW2080_CTRL_PMGR_PWR_TUPLE
            channels[LW2080_CTRL_PMGR_PWR_CHANNEL_MAX_CHANNELS];
#endif
    RM_PMU_PMGR_PWR_CHANNELS_TUPLE_QUERY_PAYLOAD    *pPayload =
        (RM_PMU_PMGR_PWR_CHANNELS_TUPLE_QUERY_PAYLOAD *)&channels;

    if (Pmgr.pPwrMonitor == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_pwrPolicies30Filter_exit;
    }

    FOR_EACH_INDEX_IN_MASK(32, i, *pPolicyMask)
    {
        pPolicy = PWR_POLICY_GET(i);
        if (pPolicy == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto s_pwrPolicies30Filter_exit;
        }
        channelMask |= pwrPolicy3XChannelMaskGet(pPolicy);
    }
    FOR_EACH_INDEX_IN_MASK_END;

    status = pwrMonitorChannelsTupleQuery(Pmgr.pPwrMonitor, channelMask,
                 NULL, pPayload, LW_FALSE);
    if (status != FLCN_OK)
    {
        goto s_pwrPolicies30Filter_exit;
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        FOR_EACH_INDEX_IN_MASK(32, i, *pPolicyMask)
        {
            pPolicy = PWR_POLICY_GET(i);
            if (pPolicy == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_INDEX;
                goto s_pwrPolicies30Filter_detach;
            }
            status = pwrPolicy3XFilter(pPolicy, Pmgr.pPwrMonitor, pPayload);
            if (status == FLCN_ERR_MORE_PROCESSING_REQUIRED)
            {
                // Re-adjust the mask on which evaluation needs to be skipped.
                *pPolicyMask &= ~BIT(i);
                status = FLCN_OK;
            }
            else if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto s_pwrPolicies30Filter_detach;
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;

s_pwrPolicies30Filter_detach:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

s_pwrPolicies30Filter_exit:
    return status;
}
