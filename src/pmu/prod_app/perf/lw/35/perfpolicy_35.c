/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perfpolicy_35.c
 * @copydoc perfpolicy_35.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "dmemovl.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objperf.h"

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
static FLCN_STATUS s_perfPolicy35UpdateViolationTime(RM_PMU_PERF_POLICY_35_LIMIT_INPUT *pLimitInput, LW2080_CTRL_PERF_POINT_VIOLATION_STATUS *pViolationStatus, const FLCN_TIMESTAMP *pTime, LwU32 pointMask)
    GCC_ATTRIB_SECTION("imem_libPerfPolicy", "s_perfPolicy35UpdateViolationTime");
static FLCN_STATUS s_perfPolicies35CachePerfPoints(PERF_POLICIES_35 *pPolicies35, PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE *pArbOutputTuple)
    GCC_ATTRIB_SECTION("imem_libPerfPolicy", "s_perfPolicies35CachePerfPoints");
static FLCN_STATUS s_perfPolicies35GetMaxPerfPoint(RM_PMU_PERF_POLICY_35_LIMIT_INPUT *pRefValues)
    GCC_ATTRIB_SECTION("imem_libPerfPolicy", "s_perfPolicies35GetMaxPerfPoint");
static FLCN_STATUS s_perfPolicies35GetVpstatePerfPoint(RM_PMU_PERF_POLICY_35_LIMIT_INPUT *pRefValues, RM_PMU_PERF_VPSTATE_IDX vpstateIdx)
    GCC_ATTRIB_SECTION("imem_libPerfPolicy", "s_perfPolicies35GetVpstatePerfPoint");
static FLCN_STATUS s_perfPolicies35CalcDispClkIntersectPerfPoint(RM_PMU_PERF_POLICY_35_LIMIT_INPUT *pDispClkValues, PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE *pArbOutputTuple)
    GCC_ATTRIB_SECTION("imem_libPerfPolicy", "s_perfPolicies35CalcDispClkIntersectPerfPoint");
static FLCN_STATUS s_perfPolicies35GetPerfPoint(PERF_POLICIES_35 *pPolicies35, LW2080_CTRL_PERF_POINT_ID id, RM_PMU_PERF_POLICY_35_LIMIT_INPUT **ppRefValue)
    GCC_ATTRIB_SECTION("imem_libPerfPolicy", "s_perfPolicies35GetPerfPoint");

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * PERF_POLICY_35 implementation
 *
 * @copy PerfPoliciesConstruct
 */
FLCN_STATUS
perfPoliciesConstruct_35
(
    PERF_POLICIES **ppPolicies
)
{
    PERF_POLICIES_35 *pPolicies35;
    FLCN_STATUS       status;

    pPolicies35 = lwosCallocType(OVL_INDEX_DMEM(perfPolicy), 1U, PERF_POLICIES_35);
    if (pPolicies35 == NULL)
    {
        PMU_HALT();
        status = FLCN_ERR_NO_FREE_MEM;
        goto perfPoliciesConstruct_35_exit;
    }
    *ppPolicies = &pPolicies35->super;

    // Set PERF_POLICIES variables
    pPolicies35->super.version = LW2080_CTRL_PERF_POLICIES_VERSION_35;

    // Construct the PERF_POLICIES super object
    status = perfPoliciesConstruct_SUPER(ppPolicies);
    if (status != FLCN_OK)
    {
        PMU_HALT();
        goto perfPoliciesConstruct_35_exit;
    }

    // Initialize class variables

perfPoliciesConstruct_35_exit:
    return status;
}

/*!
 * PERF_POLICY_35 implementation
 *
 * @copydoc BoardObjGrpIfaceModel10GetStatusHeader
 */
FLCN_STATUS
perfPoliciesIfaceModel10GetStatusHeader_35
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pBuf,
    BOARDOBJGRPMASK    *pMask
)
{
    BOARDOBJGRP *pBoardObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PERF_POLICY_BOARDOBJGRP_GET_STATUS_HEADER *pPoliciesStatus   =
        (RM_PMU_PERF_POLICY_BOARDOBJGRP_GET_STATUS_HEADER *)pBuf;
    RM_PMU_PERF_POLICY_BOARDOBJGRP_GET_STATUS_35     *pPolicies35Status =
        &pPoliciesStatus->data.v35;
    PERF_POLICIES_35 *pPolicies35 = (PERF_POLICIES_35 *)pBoardObjGrp;
    LwU8              i;

    // PERF_POLICIES_35 status variables.
    for (i = 0; i < LW2080_CTRL_PERF_POINT_ID_NUM; ++i)
    {
        pPolicies35Status->pointValues[i] = pPolicies35->pointValues[i];
    }

    return FLCN_OK;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfPolicyGrpIfaceModel10ObjSetImpl_35
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PERF_POLICY_35 *pPolicy35Desc;
    PERF_POLICY_35        *pPolicy35;
    FLCN_STATUS            status;
    LwBool                 bFirstConstruct = (*ppBoardObj == NULL);

    // Call super-class constructor.
    status = perfPolicyGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto perfPolicyGrpIfaceModel10ObjSetImpl_35_exit;
    }
    pPolicy35     = (PERF_POLICY_35 *)*ppBoardObj;
    pPolicy35Desc = (RM_PMU_PERF_POLICY_35 *)pBoardObjDesc;

    // Set PERF_POLICY_35 parameters.
    if (bFirstConstruct)
    {
        pPolicy35->limits.pstateIdx = LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID;
        pPolicy35->limits.gpcclkkHz = LW_U32_MAX;
    }
    (void)pPolicy35Desc;

perfPolicyGrpIfaceModel10ObjSetImpl_35_exit:
    return status;
}

/*!
 * PERF_POLICY_35 implementation
 */
FLCN_STATUS
perfPolicyIfaceModel10GetStatus_35
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_POLICY_35_GET_STATUS *pPolicy35Status =
        (RM_PMU_PERF_POLICY_35_GET_STATUS *)pPayload;
    PERF_POLICY_35 *pPolicy35 = (PERF_POLICY_35 *)pBoardObj;
    FLCN_STATUS     status;

    // Call super-class implementation
    status = perfPolicyIfaceModel10GetStatus_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfPolicyIfaceModel10GetStatus_35_exit;
    }

    // Get PERF_POLICY_35 specific data
    pPolicy35Status->limits = pPolicy35->limits;

perfPolicyIfaceModel10GetStatus_35_exit:
    return status;
}

/*!
 * @copydoc PerfPoliciesResetLimitInput
 */
FLCN_STATUS
perfPolicies35ResetLimitInput
(
    PERF_POLICIES_35 *pPolicies35
)
{
    PERF_POLICY  *pPolicy;
    LwBoardObjIdx idx;

    BOARDOBJGRP_ITERATOR_BEGIN(PERF_POLICY, pPolicy, idx, NULL)
    {
        PERF_POLICY_35 *pPolicy35 = (PERF_POLICY_35 *)pPolicy;

        pPolicy35->limits.pstateIdx = LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID;
        pPolicy35->limits.gpcclkkHz = LW_U32_MAX;
    }
    BOARDOBJGRP_ITERATOR_END;

    return FLCN_OK;
}

/*!
 * @copydoc PerfPolicies35UpdateViolationTime
 */
FLCN_STATUS
perfPolicies35UpdateViolationTime
(
    PERF_POLICIES_35                     *pPolicies35,
    PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE *pArbOutputTuple
)
{
    PERF_POLICY                      *pPolicy;
    FLCN_TIMESTAMP                    lwrrentTimeNs;
    RM_PMU_PERF_POLICY_35_LIMIT_INPUT limitInput;
    LwU32                             gpcclkIdx;
    LwU32                             maxGpcClkkHz;
    LwU32                             maxPstateIdx;
    LwBoardObjIdx                     idx;
    FLCN_STATUS                       status;

    osPTimerTimeNsLwrrentGet(&lwrrentTimeNs);

    // 1. Get perf. point values.
    status = s_perfPolicies35CachePerfPoints(pPolicies35, pArbOutputTuple);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfPolicies35UpdateViolationTime_exit;
    }

    // 2. Update global violation times.
    status = clkDomainsGetIndexByApiDomain(CLK_DOMAIN_MASK_GPC, &gpcclkIdx);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfPolicies35UpdateViolationTime_exit;
    }
    limitInput.pstateIdx = pArbOutputTuple->pstateIdx.value;
    limitInput.gpcclkkHz = pArbOutputTuple->clkDomains[gpcclkIdx].freqkHz.value;

    status = s_perfPolicy35UpdateViolationTime(&limitInput,
                                              &pPolicies35->super.globalViolationStatus,
                                              &lwrrentTimeNs,
                                              pPolicies35->super.pointMask);

    // 3. Update individual perf. policies.
    pPolicies35->super.limitingPolicyMask = 0;
    BOARDOBJGRP_ITERATOR_BEGIN(PERF_POLICY, pPolicy, idx, NULL)
    {
        PERF_POLICY_35 *pPolicy35 = (PERF_POLICY_35 *)pPolicy;

        // Update violation times.
        status = s_perfPolicy35UpdateViolationTime(&pPolicy35->limits,
                                                  &pPolicy35->super.violationStatus,
                                                  &lwrrentTimeNs,
                                                  pPolicies35->super.pointMask);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfPolicies35UpdateViolationTime_exit;
        }

        // Determine if the policy contributes to limiting mask.
        maxPstateIdx = pPolicies35->pointValues[LW2080_CTRL_PERF_POINT_ID_MAX_CLOCK].pstateIdx;
        maxGpcClkkHz = pPolicies35->pointValues[LW2080_CTRL_PERF_POINT_ID_MAX_CLOCK].gpcclkkHz;
        if (pPolicy35->super.violationStatus.perfPointMask != 0U)
        {
            if ((pPolicy35->limits.pstateIdx == pArbOutputTuple->pstateIdx.value) &&
                (pPolicy35->limits.pstateIdx < maxPstateIdx))
            {
                pPolicies35->super.limitingPolicyMask |= BIT(idx);
            }
            if ((pPolicy35->limits.gpcclkkHz == pArbOutputTuple->clkDomains[gpcclkIdx].freqkHz.value) &&
                (pPolicy35->limits.gpcclkkHz < maxGpcClkkHz))
            {
                pPolicies35->super.limitingPolicyMask |= BIT(idx);
            }
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    //
    // If there is a limiting policy other than Utilization, mark Utilization
    // as non-limiting.
    //
    if ((pPolicies35->super.limitingPolicyMask &
        ~BIT(LW2080_CTRL_PERF_POLICY_SW_ID_UTILIZATION)) != 0U)
    {
        pPolicies35->super.limitingPolicyMask &=
            ~BIT(LW2080_CTRL_PERF_POLICY_SW_ID_UTILIZATION);
    }
    else if (pPolicies35->super.limitingPolicyMask == 0U)
    {
        pPolicies35->super.limitingPolicyMask |=
            BIT(LW2080_CTRL_PERF_POLICY_SW_ID_UTILIZATION);
    }

perfPolicies35UpdateViolationTime_exit:
    return status;
}

/*!
 * PERF_POLICY_35 implementation
 *
 * @copydoc PerfPolicyUpdate
 */
FLCN_STATUS
perfPolicy35UpdateLimitInput
(
    PERF_POLICY_35                        *pPolicy35,
    PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE *pArbInput35Tuple,
    LwBool                                 bMin
)
{
    LwU32       clkDomIdx;
    FLCN_STATUS status;

    status = clkDomainsGetIndexByApiDomain(CLK_DOMAIN_MASK_GPC, &clkDomIdx);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfPolicyUpdateLimitInput_35_exit;
    }

    if ((pPolicy35->limits.pstateIdx == LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID) &&
        (pPolicy35->limits.gpcclkkHz == LW_U32_MAX))
    {
        // Values are default; use the input
        pPolicy35->limits.pstateIdx = pArbInput35Tuple->pstateIdx;
        pPolicy35->limits.gpcclkkHz = pArbInput35Tuple->clkDomains[clkDomIdx];
    }
    else
    {
        if (bMin)
        {
            pPolicy35->limits.pstateIdx = LW_MAX(
                pPolicy35->limits.pstateIdx,
                pArbInput35Tuple->pstateIdx);
            pPolicy35->limits.gpcclkkHz = LW_MAX(
                pPolicy35->limits.gpcclkkHz,
                pArbInput35Tuple->clkDomains[clkDomIdx]);
        }
        else
        {
            pPolicy35->limits.pstateIdx = LW_MIN(
                pPolicy35->limits.pstateIdx,
                pArbInput35Tuple->pstateIdx);
            pPolicy35->limits.gpcclkkHz = LW_MIN(
                pPolicy35->limits.gpcclkkHz,
                pArbInput35Tuple->clkDomains[clkDomIdx]);
        }
    }

perfPolicyUpdateLimitInput_35_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * PERF_POLICY_35 implementation
 *
 * @copydoc perfPolicyUpdateViolationTime
 */
static FLCN_STATUS
s_perfPolicy35UpdateViolationTime
(
    RM_PMU_PERF_POLICY_35_LIMIT_INPUT       *pLimitInput,
    LW2080_CTRL_PERF_POINT_VIOLATION_STATUS *pViolationStatus,
    const FLCN_TIMESTAMP                    *pTime,
    LwU32                                    pointMask
)
{
    PERF_POLICIES_35                  *pPolicies35 =
        (PERF_POLICIES_35 *)PERF_POLICIES_GET();
    RM_PMU_PERF_POLICY_35_LIMIT_INPUT *pRefPoint;
    FLCN_STATUS                        status = FLCN_OK;
    LwU8                               i;

    // 2. Iterate through all perf points.
    FOR_EACH_INDEX_IN_MASK(32, i, pointMask)
    {
        // a. Get the reference pstate/gpcclk values for this point.
        status = s_perfPolicies35GetPerfPoint(pPolicies35, i, &pRefPoint);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfPolicyUpdateViolationTime_35_exit;
        }

        //
        // b. Determine if there's any violation. If so, update the violation
        // mask and edge-triggered timer.
        //
        if ((pLimitInput->pstateIdx < pRefPoint->pstateIdx) ||
            (pLimitInput->gpcclkkHz < pRefPoint->gpcclkkHz))
        {
            //
            // A violation has oclwrred. Capture the rising edge of violation
            // to start timer. We only need to update when we previously have
            // not violated a point.
            //
            if ((pViolationStatus->perfPointMask & BIT(i)) == 0U)
            {
                pViolationStatus->perfPointMask |= BIT(i);
                LwU64_ALIGN32_SUB(
                    &pViolationStatus->perfPointTimeNs[i],
                    &pViolationStatus->perfPointTimeNs[i],
                    &pTime->parts);
            }
        }
        else
        {
            //
            // Capture the falling edge of violation to stop timer. We only
            // need to update when the we previously have violated a point.
            //
            if ((pViolationStatus->perfPointMask & BIT(i)) != 0U)
            {
                pViolationStatus->perfPointMask &= ~BIT(i);
                LwU64_ALIGN32_ADD(
                    &pViolationStatus->perfPointTimeNs[i],
                    &pViolationStatus->perfPointTimeNs[i],
                    &pTime->parts);
            }
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

perfPolicyUpdateViolationTime_35_exit:
    return status;
}

/*!
 * Caches the p-state/clock values of the specified perf points.
 *
 * @param[in]  pPolicies  PERF_POLICIES_35 pointer
 * 
 * @return  FLCN_OK if the perf. point p-state/clock values were cached;
 *          detailed error code otherwise
 */
static FLCN_STATUS
s_perfPolicies35CachePerfPoints
(
    PERF_POLICIES_35                     *pPolicies35,
    PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE *pArbOutputTuple
)
{
    LwU32       idx;
    FLCN_STATUS status = FLCN_OK;

    FOR_EACH_INDEX_IN_MASK(32, idx, pPolicies35->super.pointMask)
    {
        switch (idx)
        {
            case LW2080_CTRL_PERF_POINT_ID_MAX_CLOCK:
            {
                status = s_perfPolicies35GetMaxPerfPoint(
                    &pPolicies35->pointValues[idx]);
                break;
            }
            case LW2080_CTRL_PERF_POINT_ID_TURBO_BOOST:
            {
                status = s_perfPolicies35GetVpstatePerfPoint(
                    &pPolicies35->pointValues[idx],
                    RM_PMU_PERF_VPSTATE_IDX_TURBO_BOOST);
                break;
            }
            case LW2080_CTRL_PERF_POINT_ID_3D_BOOST:
            {
                status = s_perfPolicies35GetVpstatePerfPoint(
                    &pPolicies35->pointValues[idx],
                    RM_PMU_PERF_VPSTATE_IDX_3D_BOOST);
                break;
            }
            case LW2080_CTRL_PERF_POINT_ID_RATED_TDP:
            {
                status = s_perfPolicies35GetVpstatePerfPoint(
                    &pPolicies35->pointValues[idx],
                    RM_PMU_PERF_VPSTATE_IDX_RATED_TDP);
                break;
            }
            case LW2080_CTRL_PERF_POINT_ID_MAX_LWSTOMER_BOOST:
            {
                status = s_perfPolicies35GetVpstatePerfPoint(
                    &pPolicies35->pointValues[idx],
                    RM_PMU_PERF_VPSTATE_IDX_MAX_LWSTOMER_BOOST);
                break;
            }
            case LW2080_CTRL_PERF_POINT_ID_DISPLAY_CLOCK_INTERSECT:
            {
                status = s_perfPolicies35CalcDispClkIntersectPerfPoint(
                    &pPolicies35->pointValues[idx], pArbOutputTuple);
                break;
            }
            default:
            {
                status = FLCN_ERR_ILWALID_STATE;
                break;
            }
        }

        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_perfPolicies35CachePerfPoints_exit;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

s_perfPolicies35CachePerfPoints_exit:
    return status;
}

/*!
 * Gets the perf. point of the highest P-state.
 *
 * @param[out]  pRefValues  the highest possible perf. point
 *
 * @return FLCN_OK if pRefValues contains valid data; a detailed error code
 *         otherwise
 */
static FLCN_STATUS
s_perfPolicies35GetMaxPerfPoint
(
    RM_PMU_PERF_POLICY_35_LIMIT_INPUT *pRefValues
)
{
    PSTATE                             *pPstate;
    LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY clkEntry;
    LwU32                               gpcclkIdx;
    FLCN_STATUS                         status;
    LwU8                                pstateIdx;

    pstateIdx = boardObjGrpMaskBitIdxHighest(&(PSTATES_GET()->super.objMask));
    pPstate = PSTATE_GET(pstateIdx);
    if (pPstate == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto s_perfPolicies35GetMaxPerfPoint_exit;
    }

    status = clkDomainsGetIndexByApiDomain(CLK_DOMAIN_MASK_GPC, &gpcclkIdx);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfPolicies35GetMaxPerfPoint_exit;
    }

    status = perfPstateClkFreqGet(pPstate, gpcclkIdx, &clkEntry);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfPolicies35GetMaxPerfPoint_exit;
    }

    pRefValues->pstateIdx = pstateIdx;
    pRefValues->gpcclkkHz = clkEntry.max.freqVFMaxkHz;

s_perfPolicies35GetMaxPerfPoint_exit:
    return status;
}

/*!
 * Gets the P-State index and GPCCLK value for the vP-state specified.
 *
 * @param[in]  pPolicies  PERF_POLICIES_35 pointer
 * 
 * @return  FLCN_OK if the perf. point p-state/clock values were cached;
 *          detailed error code otherwise
 */
static FLCN_STATUS
s_perfPolicies35GetVpstatePerfPoint
(
    RM_PMU_PERF_POLICY_35_LIMIT_INPUT *pRefValues,
    RM_PMU_PERF_VPSTATE_IDX            vpstateIdx
)
{
    VPSTATE_3X_CLOCK_ENTRY  clkEntry;
    VPSTATE_3X             *pVpstate3x;
    CLK_DOMAIN             *pDomain;
    LwU32                   pstateIdx;
    LwU32                   idx;
    FLCN_STATUS             status;

    pVpstate3x = (VPSTATE_3X *)vpstateGetBySemanticIdx(vpstateIdx);
    if (pVpstate3x == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto s_perfPolicies35GetVpstatePerfPoint_exit;
    }

    // Extract the P-state index from the vPstate's P-state level.
    pstateIdx = PSTATE_GET_INDEX_BY_LEVEL(vpstate3xPstateIdxGet(pVpstate3x));
    if (pstateIdx == LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto s_perfPolicies35GetVpstatePerfPoint_exit;
    }
    pRefValues->pstateIdx = (LwU8)pstateIdx;

    // Extract the GPCCLK value from the vPstate.
    status = clkDomainsGetIndexByApiDomain(CLK_DOMAIN_MASK_GPC, &idx);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfPolicies35GetVpstatePerfPoint_exit;
    }
    pDomain = CLK_DOMAIN_GET(idx);
    if (pDomain == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto s_perfPolicies35GetVpstatePerfPoint_exit;
    }
    status = vpstate3xClkDomainGet(pVpstate3x, idx, &clkEntry);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfPolicies35GetVpstatePerfPoint_exit;
    }
    pRefValues->gpcclkkHz = clkEntry.targetFreqMHz *
        clkDomainFreqkHzScaleGet(pDomain);

s_perfPolicies35GetVpstatePerfPoint_exit:
    return status;
}

/*!
 * Callwlates the DISPLAY_CLOCK_INTERSECT perf point.
 *
 * @param[out]  pDispClkValues   the perf point for the dispclk intersect
 * @param[in]   pArbOutputTuple  the arbitration output values
 *
 * @return FLCN_OK if the perf point value was callwlated; detailed error
 *         code otherwise
 */
static FLCN_STATUS
s_perfPolicies35CalcDispClkIntersectPerfPoint
(
    RM_PMU_PERF_POLICY_35_LIMIT_INPUT    *pDispClkValues,
    PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE *pArbOutputTuple
)
{
    CLK_DOMAIN             *pDomainGpc;
    CLK_DOMAIN             *pDomainDisp;
    LwU32                   dispIdx;
    LwU32                   gpcIdx;
    LwBoardObjIdx           idx;
    BOARDOBJGRPMASK_E32     mask;
    FLCN_STATUS             status;

    // Get the clock domain pointers for GPC and DISPCLK.
    pDomainDisp = clkDomainsGetByApiDomain(CLK_DOMAIN_MASK_DISP);
    if (pDomainDisp == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto s_perfPolicies35CalcDispClkIntersectPerfPoint_exit;
    }
    dispIdx = BOARDOBJ_GET_GRP_IDX(&pDomainDisp->super.super);

    pDomainGpc = clkDomainsGetByApiDomain(CLK_DOMAIN_MASK_GPC);
    if (pDomainGpc == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto s_perfPolicies35CalcDispClkIntersectPerfPoint_exit;
    }
    gpcIdx = BOARDOBJ_GET_GRP_IDX(&pDomainGpc->super.super);

    //
    // If GPC and DISPCLK shares Vmin on more or more rail, pick
    // GPC clock based on MAX of voltage intersect on all rails
    // on which they share the Vmin dependencies. If they do not
    // share Vmin dependency, set the GPC clock to POR MIN GPC.
    //
    boardObjGrpMaskInit_E32(&mask);

    status = boardObjGrpMaskAnd(&mask,
                (CLK_DOMAIN_PROG_VOLT_RAIL_VMIN_MASK_GET(CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomainGpc, PROG))),
                (CLK_DOMAIN_PROG_VOLT_RAIL_VMIN_MASK_GET(CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomainDisp, PROG))));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfPolicies35CalcDispClkIntersectPerfPoint_exit;
    }

    // Init GPC to Pstate min.
    pDispClkValues->gpcclkkHz = LW_U32_MAX;

    BOARDOBJGRPMASK_FOR_EACH_BEGIN(&mask, idx)
    {
        PERF_LIMITS_VF  vfDomain;
        PERF_LIMITS_VF  vfRail;

        vfDomain.idx   = (LwU8)dispIdx;
        vfDomain.value = pArbOutputTuple->clkDomains[dispIdx].freqkHz.value;

        vfRail.idx   = (LwU8)idx;
        vfRail.value = 0U;

        // DISPCLK -> Voltage
        status = perfLimitsFreqkHzToVoltageuV(&vfDomain, &vfRail, LW_TRUE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_perfPolicies35CalcDispClkIntersectPerfPoint_exit;
        }

        // Voltage -> GPC
        vfDomain.idx   = (LwU8)gpcIdx;
        vfDomain.value = 0;

        status = perfLimitsVoltageuVToFreqkHz(&vfRail, NULL, &vfDomain, LW_TRUE, LW_TRUE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_perfPolicies35CalcDispClkIntersectPerfPoint_exit;
        }

        //
        // We want to know the GPC we can get for free due to voltage
        // being high to suppport DISPLAY. So pick MIN of all rails.
        //
        pDispClkValues->gpcclkkHz = LW_MIN(pDispClkValues->gpcclkkHz, vfDomain.value);
    }
    BOARDOBJGRPMASK_FOR_EACH_END;

    pDispClkValues->pstateIdx =
        boardObjGrpMaskBitIdxHighest(&(PSTATES_GET()->super.objMask));

    // If there is no shared rails between GPC and DISPCLK, init to pstate min.
    if (pDispClkValues->gpcclkkHz == LW_U32_MAX)
    {
        PSTATE                             *pPstate;
        LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY pstateClkEntry;

        pPstate = PSTATE_GET(pDispClkValues->pstateIdx);
        if (pPstate == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto s_perfPolicies35CalcDispClkIntersectPerfPoint_exit;
        }

        // Get the PSTATE GPC Frequency tuple.
        status = perfPstateClkFreqGet(pPstate, gpcIdx, &pstateClkEntry);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_perfPolicies35CalcDispClkIntersectPerfPoint_exit;
        }

        // Init GPC to Pstate min.
        pDispClkValues->gpcclkkHz = pstateClkEntry.min.freqkHz;
    }

s_perfPolicies35CalcDispClkIntersectPerfPoint_exit:
    return status;
}

/*!
 * Obtains the reference values of a perf point.
 * 
 * @param[in]   pPolicies35  PERF_POLICIES_35 pointer
 * @param[in]   id           ID of the perf point
 * @param[out]  pRefValue    Perf point values
 */
static FLCN_STATUS
s_perfPolicies35GetPerfPoint
(
    PERF_POLICIES_35                   *pPolicies35,
    LW2080_CTRL_PERF_POINT_ID           id,
    RM_PMU_PERF_POLICY_35_LIMIT_INPUT **ppRefValue
)
{
    if (id >= LW2080_CTRL_PERF_POINT_ID_NUM)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_INDEX;
    }

    *ppRefValue = &pPolicies35->pointValues[id];
    return FLCN_OK;
}
