/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrpolicy_march_vf.c
 * @brief  Interface specification for a PWR_POLICY_MARCH_VF.
 *
 * VF Marching Algorithm Theory:
 *
 * The VF Marching policy extends/utilizes the PWR_POLICY_MARCH interface to
 * implement a marching controller.  Based on the cap/uncap actions returned
 * from the PWR_POLICY_MARCH interface, the VF Marching algorithm cap/uncap
 * its Domain Group Limit request by a given step size.
 *
 * The pseudocode for the algorithm is as follows:
 *
 * if (action == _CAP)
 *    reduceVf(&domGrpLimit)
 * else if (action == _UNCAP)
 *    increaseVf(&domGrpLimit)
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objperf.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrpolicy_bangbang.h"
#include "dbgprintf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static void s_pwrPolicyMarchVfCap(PWR_POLICY_MARCH_VF *pMarchVf, LwU8 stepSize)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyMarchVfCap");
static void s_pwrPolicyMarchVfUncap(PWR_POLICY_MARCH_VF *pMarchVf, LwU8 stepSize)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyMarchVfCap");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Construct a _MARCH_N PWR_POLICY object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrPolicyGrpIfaceModel10ObjSetImpl_MARCH_VF
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    RM_PMU_PMGR_PWR_POLICY_MARCH_VF_BOARDOBJ_SET *pMarchVFSet    =
        (RM_PMU_PMGR_PWR_POLICY_MARCH_VF_BOARDOBJ_SET *)pBoardObjSet;
    PWR_POLICY_MARCH_VF                          *pMarchVF;
    FLCN_STATUS                                   status;
    PERF_DOMAIN_GROUP_LIMITS                      domGrpLimits;
    LwBool                                        bFirstConstruct =
                                                     (*ppBoardObj == NULL);

    // Initialize domain group limit for comparison later.
    pwrPolicyDomGrpLimitsInit(&domGrpLimits);

    if (!bFirstConstruct)
    {
        //
        // Save current domain group limits to restore - don't want to
        // completely uncap when re-creating object to adjust for new limit
        // requests.
        //
        domGrpLimits = ((PWR_POLICY_MARCH_VF *)*ppBoardObj)->super.domGrpLimits;
    }

    // Construct and initialize parent-class object.
    status = pwrPolicyGrpIfaceModel10ObjSetImpl_DOMGRP(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        return status;
    }
    pMarchVF = (PWR_POLICY_MARCH_VF *)*ppBoardObj;

    // Call constructor for PWR_POLICY_MARCH interface.
    status = pwrPolicyMarchConstruct(&pMarchVF->march, &pMarchVFSet->march,
                                     bFirstConstruct);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        return status;
    }

    // Restore the current Domain Group Limits if applicable.
    if (RM_PMU_PREF_DOMAIN_GROUP_LIMIT_VALUE_DISABLE !=
            domGrpLimits.values[RM_PMU_PERF_DOMAIN_GROUP_PSTATE])
    {
        pMarchVF->super.domGrpLimits = domGrpLimits;
    }

    return status;
}

/*!
 * _MARCH_VF implemenation.
 *
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
pwrPolicyIfaceModel10GetStatus_MARCH_VF
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PMGR_PWR_POLICY_MARCH_VF_BOARDOBJ_GET_STATUS *pGetStatus;
    PWR_POLICY_MARCH_VF                                 *pMarchVF;
    FLCN_STATUS                                          status;

    // Call _DOMGRP super-class implementation.
    status = pwrPolicyIfaceModel10GetStatus_DOMGRP(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        return status;
    }
    pMarchVF   = (PWR_POLICY_MARCH_VF *)pBoardObj;
    pGetStatus = (RM_PMU_PMGR_PWR_POLICY_MARCH_VF_BOARDOBJ_GET_STATUS *)pPayload;

    // Call the _MARCH interface.
    status = pwrPolicyMarchQuery(&pMarchVF->march, &pGetStatus->march);
    if (status != FLCN_OK)
    {
        return status;
    }

    return status;
}

/*!
 * _MARCH_VF implementation.
 *
 * @copydoc PwrPolicyDomGrpEvaluate
 */
FLCN_STATUS
pwrPolicyDomGrpEvaluate_MARCH_VF
(
    PWR_POLICY_DOMGRP *pDomGrp
)
{
    PWR_POLICY_MARCH_VF *pMarchVF = (PWR_POLICY_MARCH_VF *)pDomGrp;
    LwU8                 action;

    // Call MARCH interface to determine which action to take.
    action = pwrPolicyMarchEvaluateAction(&pMarchVF->super.super,
        &pMarchVF->march);

    // Capping - set lower VF, if possible.
    if (action == LW2080_CTRL_PMGR_PWR_POLICY_MARCH_ACTION_CAP)
    {
        s_pwrPolicyMarchVfCap(pMarchVF,
            pwrPolicyMarchStepSizeGet(&pMarchVF->march));
    }
    // Uncapping - raise VF, if possible.
    else if (action == LW2080_CTRL_PMGR_PWR_POLICY_MARCH_ACTION_UNCAP)
    {
        s_pwrPolicyMarchVfUncap(pMarchVF,
            pwrPolicyMarchStepSizeGet(&pMarchVF->march));
    }

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * Helper function to apply the @ref LW2080_CTRL_PMGR_PWR_POLICY_MARCH_ACTION_CAP
 * action.  This function will cap by @ref stepSize NDIV coefficients from
 * current requested NDIV coefficient (if available).
 *
 * @param[in/out] pMarchVF  PWR_POLICY_MARCH_VF pointer
 * @param[in]     stepSize Step size for number of coefficients by which to cap
 */
static void
s_pwrPolicyMarchVfCap
(
    PWR_POLICY_MARCH_VF *pMarchVF,
    LwU8                 stepSize
)
{
    LwU8                                  p0Idx     = perfGetPstateCount() - 1;
    RM_PMU_PERF_PSTATE_DOMAIN_GROUP_INFO *pP0DomGrp =
        perfPstateDomGrpGet(p0Idx, RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK);
    LwU8                                  vfIdx;

    // 1. If VF steps still left in the P0 VF lwrve, drop to those steps.
    if ((pMarchVF->super.domGrpLimits.values[RM_PMU_PERF_DOMAIN_GROUP_PSTATE]
            >= p0Idx) &&
        ((vfIdx = perfVfEntryIdxGetForFreq(p0Idx, RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK,
            pMarchVF->super.domGrpLimits.values[RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK]))
                >= pP0DomGrp->vfIdxFirst + stepSize))
    {
        pMarchVF->super.domGrpLimits.values[RM_PMU_PERF_DOMAIN_GROUP_PSTATE] = p0Idx;
        pMarchVF->super.domGrpLimits.values[RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK] =
            perfVfEntryGetMaxFreqKHz(pP0DomGrp, perfVfEntryGet(vfIdx - stepSize));
    }
    // 2. Otherwise, drop pstate.
    else if (pMarchVF->super.domGrpLimits.values[RM_PMU_PERF_DOMAIN_GROUP_PSTATE] > 0)
    {
        pMarchVF->super.domGrpLimits.values[RM_PMU_PERF_DOMAIN_GROUP_PSTATE]--;
    }
}

/*!
 * Helper function to apply the @ref LW2080_CTRL_PMGR_PWR_POLICY_MARCH_ACTION_UNCAP
 * action.  This function will uncap by @ref stepSize NDIV coefficients from
 * current requested NDIV coefficient (if available).
 *
 * @param[in/out] pMarchVF  PWR_POLICY_MARCH_VF pointer
 * @param[in]     stepSize
 *     Step size for number of coefficients by which to uncap
 */
static void
s_pwrPolicyMarchVfUncap
(
    PWR_POLICY_MARCH_VF *pMarchVF,
    LwU8                 stepSize
)
{
    LwU8                                  p0Idx     = perfGetPstateCount() - 1;
    RM_PMU_PERF_PSTATE_DOMAIN_GROUP_INFO *pP0DomGrp =
        perfPstateDomGrpGet(p0Idx, RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK);
    LwU8                                  vfIdx;

    // 1. If pstate is below P0, boost pstate.
    if (pMarchVF->super.domGrpLimits.values[RM_PMU_PERF_DOMAIN_GROUP_PSTATE] < p0Idx)
    {
        pMarchVF->super.domGrpLimits.values[RM_PMU_PERF_DOMAIN_GROUP_PSTATE]++;
    }
    // 2. Otherwise, raise VF steps.
    else if ((vfIdx = perfVfEntryIdxGetForFreq(p0Idx, RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK,
                pMarchVF->super.domGrpLimits.values[RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK]))
                    < pP0DomGrp->vfIdxLast)
    {
        pMarchVF->super.domGrpLimits.values[RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK] =
            perfVfEntryGetMaxFreqKHz(pP0DomGrp, perfVfEntryGet(vfIdx + stepSize));
    }
}
