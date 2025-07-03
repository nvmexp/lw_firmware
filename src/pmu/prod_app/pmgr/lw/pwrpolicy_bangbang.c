/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrpolicy_bangbang.c
 * @brief  Interface specification for a PWR_POLICY_BANG_BANG_VF.
 *
 * Bang-Bang VF Algorithm Theory:
 *
 * The Bang-Bang VF Power Policy is a controller object which regulates the
 * controlled value (@ref PWR_POLICY::valueLwrr - units do not matter) via a
 * bang-bang algorithm on the VF lwrve.
 *
 * http://en.wikipedia.org/wiki/Bang%E2%80%93bang_control
 *
 * The hysteresis value is specified as ratio/percentage of the limit (@ref
 * PWR_POLICY::limitLwrr) - @ref PWR_POLICY_BANG_BANG_VF::uncapLimitRatio.
 *
 * The actual algorithm works as follows:
 *
 * if (valueLwrr > limitLwrr)
 *     cap()
 * else if (valueLwrr <= limitLwrr * uncapLimitRatio)
 *     uncap()
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "pmu_objperf.h"
#include "pmu_objpmgr.h"
#include "therm/thrmintr.h"
#include "pmgr/pwrpolicy_bangbang.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static void s_pwrPolicyBangBangCap(PWR_POLICY_BANG_BANG_VF *pBangBang, PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimitsLwrr)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyBangBangCap");
static void s_pwrPolicyBangBangUncap(PWR_POLICY_BANG_BANG_VF *pBangBang, PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimitsLwrr)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyBangBangUncap");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Construct a _BANG_BANG_VF PWR_POLICY object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrPolicyGrpIfaceModel10ObjSetImpl_BANG_BANG_VF
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    RM_PMU_PMGR_PWR_POLICY_BANG_BANG_VF_BOARDOBJ_SET *pBangBangSet   =
        (RM_PMU_PMGR_PWR_POLICY_BANG_BANG_VF_BOARDOBJ_SET *)pBoardObjSet;
    PWR_POLICY_BANG_BANG_VF                          *pBangBang;
    FLCN_STATUS                                       status;
    LwBool                                            bFirstConstruct =
                                                         (*ppBoardObj == NULL);

    // Construct and initialize parent-class object.
    status = pwrPolicyGrpIfaceModel10ObjSetImpl_DOMGRP(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        return status;
    }
    pBangBang = (PWR_POLICY_BANG_BANG_VF *)*ppBoardObj;

    // Copy in the _BANG_BANG_VF type-specific data.
    pBangBang->uncapLimitRatio = pBangBangSet->uncapLimitRatio;

    //
    // If first construction, init HW_FAILSAFE violation timer to the current
    // reading.  All subsequent updates will be handled by @ref
    // pwrPolicyDomGrpEvaluate_BANG_BANG_VF().
    //
    if (bFirstConstruct)
    {
        if (thermEventViolationGetTimer32(RM_PMU_THERM_EVENT_META_ANY_HW_FS,
                        &pBangBang->thermHwFsTimernsPrev) != FLCN_OK)
        {
            PMU_HALT();
            return FLCN_ERR_ILWALID_STATE;
        }
    }

    return status;
}


/*!
 * _BANG_BANG_VF implemenation.
 *
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
pwrPolicyIfaceModel10GetStatus_BANG_BANG_VF
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PMGR_PWR_POLICY_BANG_BANG_VF_BOARDOBJ_GET_STATUS *pGetStatus;
    PWR_POLICY_BANG_BANG_VF                                 *pBangBang;
    FLCN_STATUS                                              status;

    // Call _DOMGRP super-class implementation.
    status = pwrPolicyIfaceModel10GetStatus_DOMGRP(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        return status;
    }
    pBangBang  = (PWR_POLICY_BANG_BANG_VF *)pBoardObj;
    pGetStatus =
        (RM_PMU_PMGR_PWR_POLICY_BANG_BANG_VF_BOARDOBJ_GET_STATUS *)pPayload;

    // Copy out type-specific state to the RM.
    pGetStatus->action = pBangBang->action;
    pGetStatus->input  = pBangBang->input;
    pGetStatus->output = pBangBang->output;

    return status;
}

/*!
 * _BANG_BANG_VF implementation.
 *
 * @copydoc PwrPolicyDomGrpEvaluate
 */
FLCN_STATUS
pwrPolicyDomGrpEvaluate_BANG_BANG_VF
(
    PWR_POLICY_DOMGRP *pDomGrp
)
{
    PWR_POLICY_BANG_BANG_VF              *pBangBang =
        (PWR_POLICY_BANG_BANG_VF *)pDomGrp;
    PERF_DOMAIN_GROUP_LIMITS              domGrpLimitsLwrr;
    RM_PMU_PERF_PSTATE_DOMAIN_GROUP_INFO *pPstateDomGrp;
    RM_PMU_PERF_VF_ENTRY_INFO            *pVfEntry;
    FLCN_STATUS                           status = FLCN_ERR_ILWALID_STATE;
    LwU32                                 thermHwFsTimerns =
        pBangBang->thermHwFsTimernsPrev;
    LwU32                                 idx;

    //
    // If current voltage is zero, then assume haven't gotten PERF_STATUS,
    // and all PERF interfaces will be slightly broken.  Bail here.
    //
    if (perfVoltageuVGet() == 0)
    {
        goto pwrPolicyDomGrpEvaluate_BANG_BANG_VF_done;
    }

    //
    // Query the THERM HW_FAILSAFE violation time.  This counter tracks the
    // amount of time any HW_FAILSAFEs are engaged.
    //
    if (thermEventViolationGetTimer32(RM_PMU_THERM_EVENT_META_ANY_HW_FS,
                                      &thermHwFsTimerns) != FLCN_OK)
    {
        PMU_HALT();
        goto pwrPolicyDomGrpEvaluate_BANG_BANG_VF_done;
    }

    //
    // Retrieve current domain group limits from OBJPERF.  Will be used for
    // capping and uncapping actions below.
    //
    idx = BOARDOBJ_GET_GRP_IDX(&pBangBang->super.super.super.super);
    if ((BIT(idx) & Pmgr.pPwrPolicies->domGrpPolicyPwrMask) != 0)
    {
        pwrPolicyDomGrpLimitsGetById(&domGrpLimitsLwrr,
                            RM_PMU_PERF_DOMAIN_GROUP_LIMITS_ID_PWR);
    }
    else
    {
        pwrPolicyDomGrpLimitsGetById(&domGrpLimitsLwrr,
                            RM_PMU_PERF_DOMAIN_GROUP_LIMITS_ID_THERM);
    }

    // If the value is over the limit, cap.
    if (pBangBang->super.super.valueLwrr >
            pwrPolicyLimitLwrrGet(&pBangBang->super.super))
    {
        pBangBang->action = LW2080_CTRL_PMGR_PWR_POLICY_BANG_BANG_VF_ACTION_CAP;
        s_pwrPolicyBangBangCap(pBangBang, &domGrpLimitsLwrr);
    }
    //
    // If the value is under the limit * ratio and no HW_FAILSAFE timer
    // increase, uncap.
    //
    //
    // Note: 32BIT_OVERFLOW - Possible FXP20.12 overflow, not going to fix
    // because this code segment is tied to the @ref PMU_PMGR_PWR_POLICY_BANG_BANG_VF
    // feature and is not used on AD10X and later chips.
    //
    else if ((pBangBang->super.super.valueLwrr <=
                LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(20, 12,
                    pwrPolicyLimitLwrrGet(&pBangBang->super.super) *
                        pBangBang->uncapLimitRatio)) &&
             (thermHwFsTimerns == pBangBang->thermHwFsTimernsPrev))
    {
        pBangBang->action =
            LW2080_CTRL_PMGR_PWR_POLICY_BANG_BANG_VF_ACTION_UNCAP;
        s_pwrPolicyBangBangUncap(pBangBang, &domGrpLimitsLwrr);
    }
    // No action, can bail out.
    else
    {
        pBangBang->action =
            LW2080_CTRL_PMGR_PWR_POLICY_BANG_BANG_VF_ACTION_NONE;
        status = FLCN_OK;
        goto pwrPolicyDomGrpEvaluate_BANG_BANG_VF_done;
    }

    //
    // Apply the new limits computed above:
    // 1. Set the new pstate limit.
    //
    pBangBang->super.domGrpLimits.values[RM_PMU_PERF_DOMAIN_GROUP_PSTATE] =
        pBangBang->output.pstateIdx;

    // 2. Set the new GPC2CLK limit.
    pPstateDomGrp = perfPstateDomGrpGet(pBangBang->output.pstateIdx,
                        RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK);
    if (pPstateDomGrp == NULL)
    {
        PMU_BREAKPOINT();
        goto pwrPolicyDomGrpEvaluate_BANG_BANG_VF_done;
    }

    pVfEntry = perfVfEntryGet(pBangBang->output.vfIdx);
    if (pVfEntry == NULL)
    {
        PMU_BREAKPOINT();
        goto pwrPolicyDomGrpEvaluate_BANG_BANG_VF_done;
    }

    pBangBang->super.domGrpLimits.values[RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK] =
        perfVfEntryGetMaxFreqKHz(pPstateDomGrp, pVfEntry);

    status = FLCN_OK;
pwrPolicyDomGrpEvaluate_BANG_BANG_VF_done:
    pBangBang->thermHwFsTimernsPrev = thermHwFsTimerns;
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * Helper function to apply the @ref
 * LW2080_CTRL_PMGR_PWR_POLICY_BANG_BANG_VF_ACTION_CAP action.  This function
 * will cap down one VF entry from current VF point (if available).
 *
 * @param[in/out] pBangBang  PWR_POLICY_BANG_BANG_VF pointer
 * @param[in]     pDomGrpLimitsLwrr
 *      PERF_DOMAIN_GROUP_LIMITS pointer containing the current domain
 *      group limits being applied by the PMU.
 */
static void
s_pwrPolicyBangBangCap
(
    PWR_POLICY_BANG_BANG_VF  *pBangBang,
    PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimitsLwrr
)
{
    RM_PMU_PERF_PSTATE_DOMAIN_GROUP_INFO *pPstateDomGrp;

    // Find the current pstate limit set for all PWR_POLICYs.
    pBangBang->input.pstateIdx =
        LW_MIN(perfGetPstateCount() - 1,
            pDomGrpLimitsLwrr->values[RM_PMU_PERF_DOMAIN_GROUP_PSTATE]);

    //
    // Find the current VF entry corresponding to the GPC2CLK limit set for all
    // PWR_POLICYs.
    //
    pBangBang->input.vfIdx = perfVfEntryIdxGetForFreq(
                pBangBang->input.pstateIdx,
                RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK,
                pDomGrpLimitsLwrr->values[RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK]);

    //
    // Copy the input state to the output state, as those are values which will
    // be manipulated.
    //
    pBangBang->output = pBangBang->input;

    // If lower VF indexes available in VBIOS, drop to those.
    pPstateDomGrp = perfPstateDomGrpGet(pBangBang->output.pstateIdx,
                        RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK);
    if (pBangBang->output.vfIdx > pPstateDomGrp->vfIdxFirst)
    {
        pBangBang->output.vfIdx--;
    }
    // Otherwise, drop the pstate down and find the closest GPC2CLK value.
    else if (pBangBang->output.pstateIdx > 0)
    {
        pBangBang->output.pstateIdx--;
        //
        // Find the VF entry corresponding to the current GPC2CLK limit in the
        // new pstate.
        //
        pBangBang->output.vfIdx = perfVfEntryIdxGetForFreq(
                    pBangBang->output.pstateIdx,
                    RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK,
                    pDomGrpLimitsLwrr->values[RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK]);

        // If have another VF entry to drop in the lower pstate, do that too.
        pPstateDomGrp = perfPstateDomGrpGet(pBangBang->output.pstateIdx,
                            RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK);
        if (pBangBang->output.vfIdx > pPstateDomGrp->vfIdxFirst)
        {
            pBangBang->output.vfIdx--;
        }
    }
}

/*!
 * Helper function to apply the @ref
 * LW2080_CTRL_PMGR_PWR_POLICY_BANG_BANG_VF_ACTION_UNCAP action.  This function
 * will uncap up one VF entry from current VF point (if available) only when the
 * this PWR_POLICY_BANG_BANG_VF is enforcing domain group limits which are
 * lwrrently the lowest enforced limits.
 *
 * @param[in/out] pBangBang  PWR_POLICY_BANG_BANG_VF pointer
 * @param[in]     pDomGrpLimitsLwrr
 *      PERF_DOMAIN_GROUP_LIMITS pointer containing the current domain
 *      group limits being applied by the PMU.
 */
static void
s_pwrPolicyBangBangUncap
(
    PWR_POLICY_BANG_BANG_VF  *pBangBang,
    PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimitsLwrr
)
{
    RM_PMU_PERF_PSTATE_DOMAIN_GROUP_INFO *pPstateDomGrp;

    // Find this PWR_POLICY_DOMGRP's current pstate limit.
    pBangBang->input.pstateIdx =
        LW_MIN(perfGetPstateCount() - 1,
            pBangBang->super.domGrpLimits.values[RM_PMU_PERF_DOMAIN_GROUP_PSTATE]);

    //
    // Find this PWR_POLICY_DOMGRP's current VF entry index.
    //
    pBangBang->input.vfIdx = perfVfEntryIdxGetForFreq(
                pBangBang->input.pstateIdx,
                RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK,
                pBangBang->super.domGrpLimits.values[RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK]);

    //
    // Copy the input state to the output state, as those are values which will
    // be manipulated below, if applicable.
    //
    pBangBang->output = pBangBang->input;

    // If higher VF indexes available in VBIOS, raise to those.
    pPstateDomGrp = perfPstateDomGrpGet(pBangBang->output.pstateIdx,
                        RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK);
    if (pBangBang->output.vfIdx < pPstateDomGrp->vfIdxLast)
    {
        pBangBang->output.vfIdx++;
    }
    // Otherwise, raise the pstate up and find the closest GPC2CLK value.
    else if (pBangBang->output.pstateIdx < perfGetPstateCount() - 1)
    {
        pBangBang->output.pstateIdx++;
        //
        // Find the VF entry corresponding to the current GPC2CLK limit in the
        // new pstate.
        //
        pBangBang->output.vfIdx = perfVfEntryIdxGetForFreq(
                    pBangBang->output.pstateIdx,
                    RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK,
                    pBangBang->super.domGrpLimits.values[RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK]);

        // If have another VF entry to raise in the higher pstate, do that too.
        pPstateDomGrp = perfPstateDomGrpGet(pBangBang->output.pstateIdx,
                            RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK);
        if (pBangBang->output.vfIdx < pPstateDomGrp->vfIdxLast)
        {
            pBangBang->output.vfIdx++;
        }
    }
}
