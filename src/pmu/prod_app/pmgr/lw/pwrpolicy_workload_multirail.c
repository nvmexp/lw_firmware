/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrpolicy_workload_multirail.c
 * @brief  Interface specification for a PWR_POLICY_WORKLOAD_MULTIRAIL.
 *
 * The Core Workload Controller Multi Rail policy extends current Core Workload
 * Controller policy with support of multiple voltage rails. The CWC-MultiRail
 * uses same algorithm as CWC is using, but with support of two rails.
 *
 * CWC inherently operates in 1x clock domain for graphics clock (GPC). It reads
 * in 2x (GPC2) clock or in 1x (GPC) clock (when it is supported) and translate them
 * to 1x (GPC) for its internal algorithm.
 */
/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objperf.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrpolicy_workload_multirail.h"
#include "dbgprintf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static FLCN_STATUS s_pwrPolicyWorkloadMultiRailInputStateGet(PWR_POLICY_WORKLOAD_MULTIRAIL *pWorkload, LW2080_CTRL_PMGR_PWR_POLICY_STATUS_DATA_WORKLOAD_MULTIRAIL_WORK_INPUT *pInput)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadMultiRailInputStateGet");
static FLCN_STATUS s_pwrPolicyWorkloadMultiRailComputeClkMHz(PWR_POLICY_WORKLOAD_MULTIRAIL *pWorkload, PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadMultiRailComputeClkMHz");
static FLCN_STATUS s_pwrPolicyWorkloadMultiRailScaleClocks(PWR_POLICY_WORKLOAD_MULTIRAIL *pWorkload, PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadMultiRailScaleClocks");
static FLCN_STATUS s_pwrPolicyWorkloadMultiRailResidencyGet(PWR_POLICY_WORKLOAD_MULTIRAIL *pWorkload)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadMultiRailResidencyGet");
static BoardObjVirtualTableGetInterfaceFromBoardObj (s_pwrPolicyGetInterfaceFromBoardObj_WorkloadMultiRail)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyGetInterfaceFromBoardObj_WorkloadMultiRail");

/* ------------------------- Global Variables ------------------------------- */
/*!
 * Main structure for all PWR_POLICY_WORKLOAD_MULTIRAIL_WORKLOAD_MULTIRAIL interface data.
 */
BOARDOBJ_VIRTUAL_TABLE pwrPolicyWorkloadMultiRailVirtualTable_MultiRail =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_GET_INTERFACE_FROM_BOARDOBJ(
        s_pwrPolicyGetInterfaceFromBoardObj_WorkloadMultiRail)
};

/*!
 * Main structure for all PWR_POLICY_WORKLOAD_MULTIRAIL VIRTUAL_TABLE data.
 */
INTERFACE_VIRTUAL_TABLE pwrPolicyMultiRailVirtualTable =
{
    LW_OFFSETOF(PWR_POLICY_WORKLOAD_MULTIRAIL, workIface)   // offset
};

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Construct a _WORKLOAD_MULTIRAIL PWR_POLICY object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_MULTIRAIL
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PMGR_PWR_POLICY_WORKLOAD_MULTIRAIL_BOARDOBJ_SET  *pWorkloadSet =
        (RM_PMU_PMGR_PWR_POLICY_WORKLOAD_MULTIRAIL_BOARDOBJ_SET *)pBoardObjDesc;
    PWR_POLICY_WORKLOAD_MULTIRAIL                           *pWorkload;
    BOARDOBJ_VTABLE                                         *pBoardObjVtable;
    FLCN_STATUS status;
    LwU8        railIdx;
    LwBool      bFirstConstruct = (*ppBoardObj == NULL);

    // Construct and initialize parent-class object.
    status = pwrPolicyGrpIfaceModel10ObjSetImpl_DOMGRP(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_MULTIRAIL_exit;
    }
    pWorkload       = (PWR_POLICY_WORKLOAD_MULTIRAIL *)*ppBoardObj;
    pBoardObjVtable = (BOARDOBJ_VTABLE *)*ppBoardObj;

    // Construct MULTIRAIL interface class
    status = pwrPolicyConstructImpl_WORKLOAD_MULTIRAIL_INTERFACE(pBObjGrp,
                &pWorkload->workIface.super,
                &pWorkloadSet->workIface.super,
                &pwrPolicyMultiRailVirtualTable);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_MULTIRAIL_exit;
    }

    // Copy in the _WORKLOAD type-specific data.
    pWorkload->bBidirectionalSearch  = pWorkloadSet->bBidirectionalSearch;
    pWorkload->clkUpScale            = pWorkloadSet->clkUpScale;
    pWorkload->clkDownScale          = pWorkloadSet->clkDownScale;
    // Copy in per rail specific parameters.
    FOR_EACH_VALID_VOLT_RAIL_IFACE((&pWorkload->workIface), railIdx)
    {

        // Allocate buffers for median filter, if not already allocated.
        if (pWorkload->rail[railIdx].median.pEntries == NULL)
        {
            //
            // Need two (2) buffers of same size, so instead of two allocations
            // (with duplicate error checking and the second one could possibly
            // fail), allocate in one double-sized buffer and then split the buffer
            // appropriately.
            //
            pWorkload->rail[railIdx].median.pEntries =
                lwosCallocType(
                    PWR_POLICY_ALLOCATIONS_DMEM_OVL(pBObjGrp->ovlIdxDmem),
                    2 * pWorkloadSet->medianFilterSize,
                    PWR_POLICY_WORKLOAD_MEDIAN_FILTER_ENTRY);
            if (pWorkload->rail[railIdx].median.pEntries == NULL)
            {
                status = FLCN_ERR_NO_FREE_MEM;
                goto pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_MULTIRAIL_exit;
            }

            // Assign this buffer from the common allocation above.
            pWorkload->rail[railIdx].median.pEntriesSort =
                &pWorkload->rail[railIdx].median.pEntries[pWorkloadSet->medianFilterSize];
        }
        // Otherwise, assert that filter size is the same length!
        else
        {
            PMU_HALT_COND(pWorkload->rail[railIdx].median.sizeTotal ==
                pWorkloadSet->medianFilterSize);
        }

        // Initialize the state of the median filter.
        pWorkload->rail[railIdx].median.idx       = 0;
        pWorkload->rail[railIdx].median.sizeTotal = pWorkloadSet->medianFilterSize;
        pWorkload->rail[railIdx].median.sizeLwrr  = 0;
        memset(&pWorkload->rail[railIdx].median.entryLwrrOutput, 0,
            sizeof(PWR_POLICY_WORKLOAD_MEDIAN_FILTER_ENTRY));
    }
    FOR_EACH_VALID_VOLT_RAIL_IFACE_END

    if (bFirstConstruct)
    {
        //
        // Cache initial state of the counter values to be used for average
        // frequency.
        //
        (void)pmgrFreqMHzGet(pwrPoliciesGetGraphicsClkDomain(Pmgr.pPwrPolicies),
                            &pWorkload->clkCntrStart);

        // Initialize the frequency from where to start the search
        pWorkload->workIface.freq.freqMaxMHz = LW_U32_MAX;
    }

    // Override the Vtable pointer.
   pBoardObjVtable->pVirtualTable = &pwrPolicyWorkloadMultiRailVirtualTable_MultiRail;

pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_MULTIRAIL_exit:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
pwrPolicyIfaceModel10GetStatus_WORKLOAD_MULTIRAIL
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PMGR_PWR_POLICY_WORKLOAD_MULTIRAIL_BOARDOBJ_GET_STATUS *pGetStatus;
    PWR_POLICY_WORKLOAD_MULTIRAIL      *pWorkload;
    FLCN_STATUS                         status;

    // Call _DOMGRP super-class implementation.
    status = pwrPolicyIfaceModel10GetStatus_DOMGRP(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        return status;
    }
    pWorkload  = (PWR_POLICY_WORKLOAD_MULTIRAIL *)pBoardObj;
    pGetStatus =
        (RM_PMU_PMGR_PWR_POLICY_WORKLOAD_MULTIRAIL_BOARDOBJ_GET_STATUS *)pPayload;
    // Copy out the state to the RM.

    // Query MULTI_RAIL_INTERFACE interface
    status = pwrPolicyGetStatus_WORKLOAD_MULTIRAIL_INTERFACE(&pWorkload->workIface,
                &pGetStatus->workIface);
    if (status != FLCN_OK)
    {
        return status;
    }

    return status;
}

/*!
 * WORKLOAD_MULTIRAIL-implementation of filtering.  Applies the 3 filter
 * on the workload term and stores the information related to the filtered out.
 *
 * @copydoc PwrPolicyFilter
 */
FLCN_STATUS
pwrPolicyFilter_WORKLOAD_MULTIRAIL
(
    PWR_POLICY  *pPolicy,
    PWR_MONITOR *pMon
)
{
    PWR_POLICY_WORKLOAD_MULTIRAIL              *pWorkload =
        (PWR_POLICY_WORKLOAD_MULTIRAIL *)pPolicy;
    PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE   *pWorkIface =
        PWR_POLICY_WORKLOAD_MULTIRAIL_IFACE_GET(pWorkload);
    PWR_POLICY_WORKLOAD_MEDIAN_FILTER_ENTRY     filterInput;
    LwU8                                        railIdx;

    //
    // 1. Read current state of the system.  Bail on unexpected errors if PERF
    // subsystem not initialized.
    //
    pWorkload->status = s_pwrPolicyWorkloadMultiRailInputStateGet(pWorkload,
                            &pWorkIface->work);
    if (pWorkload->status != FLCN_OK)
    {
        return pWorkload->status;
    }

    // 2. Callwlate the per rail workload paramter.
    FOR_EACH_VALID_VOLT_RAIL_IFACE(pWorkIface, railIdx)
    {
        // a. Callwlate the workload from the current power.
        pWorkIface->rail[railIdx].workload =
            pwrPolicyWorkloadMultiRailComputeWorkload(pWorkIface,
                &pWorkIface->work, railIdx, pwrPolicyLimitUnitGet(pWorkload));

        filterInput.workload = pWorkIface->rail[railIdx].workload;
        filterInput.value    = pWorkIface->rail[railIdx].valueLwrr;

        // b. Insert latest sample into the Workload Median Filter.
        pwrPolicyWorkloadMedianFilterInsert_SHARED(
            &pWorkload->rail[railIdx].median, &filterInput);
    }
    FOR_EACH_VALID_VOLT_RAIL_IFACE_END

    return FLCN_OK;
}

/*!
 * @copydoc PwrPolicy3XFilter
 */
FLCN_STATUS
pwrPolicy3XFilter_WORKLOAD_MULTIRAIL
(
    PWR_POLICY     *pPolicy,
    PWR_MONITOR    *pMon,
    PWR_POLICY_3X_FILTER_PAYLOAD_TYPE *pPayload
)
{
    PWR_POLICY_WORKLOAD_MULTIRAIL               *pWorkload =
        (PWR_POLICY_WORKLOAD_MULTIRAIL *)pPolicy;
    PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE    *pWorkIface =
        PWR_POLICY_WORKLOAD_MULTIRAIL_IFACE_GET(pWorkload);
    FLCN_STATUS    status;

    OSTASK_OVL_DESC   ovlDesc[] = {
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pmgrPwrPolicyPerfCfPwrModel)
#else
        OSTASK_OVL_DESC_ILWALID
#endif
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDesc);
    {
        // Callwlate MSCG and PG residency
        status = s_pwrPolicyWorkloadMultiRailResidencyGet(pWorkload);
        if (status != FLCN_OK)
        {
            goto pwrPolicy3XFilter_WORKLOAD_MULTIRAIL_detach;
        }

        // Call the interface for common filtering
        status = pwrPolicy3XFilter_WORKLOAD_MULTIRAIL_INTERFACE(pWorkIface, pPayload);
        if (status != FLCN_OK)
        {
            goto pwrPolicy3XFilter_WORKLOAD_MULTIRAIL_detach;
        }

        // Apply filtering
        status = pwrPolicyFilter_WORKLOAD_MULTIRAIL(pPolicy, pMon);
        if (status != FLCN_OK)
        {
            goto pwrPolicy3XFilter_WORKLOAD_MULTIRAIL_detach;
        }

pwrPolicy3XFilter_WORKLOAD_MULTIRAIL_detach:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDesc);

    return status;
}

/*!
 * WORKLOAD_MULTIRAIL-implementation to evaluate the power limit and callwlate
 * the corresponding domain group limits which will provide power <= the limit.
 *
 * @pre @ref pwrPolicyFilter_WORKLOAD_MULTIRAIL() must be called every iteration
 * to callwlate and filter the workload term before calling this function.
 *
 * @copydoc PwrPolicyDomGrpEvaluate
 */
FLCN_STATUS
pwrPolicyDomGrpEvaluate_WORKLOAD_MULTIRAIL
(
    PWR_POLICY_DOMGRP *pDomGrp
)
{
    PWR_POLICY_WORKLOAD_MULTIRAIL  *pWorkload    =
        (PWR_POLICY_WORKLOAD_MULTIRAIL *)pDomGrp;
    PERF_DOMAIN_GROUP_LIMITS domGrpLimits = {{ 0 }};
    FLCN_STATUS status;

    // Bail out if the system isn't ready for power policy actions yet.
    if (pWorkload->status != FLCN_OK)
    {
        status = pWorkload->status;
        goto pwrPolicyDomGrpEvaluate_WORKLOAD_MULTIRAIL_exit;
    }

    //
    // 1. Callwlate clocks for workload and power limit.
    //
    // Use stack variable domGrpLimits because we depend on current values in
    // PWR_POLICY_DOMGRP::domGrpLimits for ramp-rate control in
    // _pwrPolicyWorkloadScaleClocks() below.
    //
    status = s_pwrPolicyWorkloadMultiRailComputeClkMHz(pWorkload, &domGrpLimits);
    if (status != FLCN_OK)
    {
        goto pwrPolicyDomGrpEvaluate_WORKLOAD_MULTIRAIL_exit;
    }

    //
    // 2. Scale clocks for ramp-rate control and scale back to domain-group
    // units.
    //
    status = s_pwrPolicyWorkloadMultiRailScaleClocks(pWorkload, &domGrpLimits);
    if (status != FLCN_OK)
    {
        goto pwrPolicyDomGrpEvaluate_WORKLOAD_MULTIRAIL_exit;
    }

    // 3. Update/cache the domain group limits for this policy.
    pWorkload->super.domGrpLimits = domGrpLimits;

pwrPolicyDomGrpEvaluate_WORKLOAD_MULTIRAIL_exit:
    return status;
}

/*!
 * Load _WORKLOAD_MULTIRAIL PWR_POLICY object.
 *
 * @copydoc PwrPolicyLoad
 */
FLCN_STATUS
pwrPolicyLoad_WORKLOAD_MULTIRAIL
(
    PWR_POLICY *pPolicy,
    LwU32       ticksNow
)
{
    PWR_POLICY_WORKLOAD_MULTIRAIL           *pWorkload =
        (PWR_POLICY_WORKLOAD_MULTIRAIL *)pPolicy;
    PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE *pWorkIface =
        PWR_POLICY_WORKLOAD_MULTIRAIL_IFACE_GET(pWorkload);
    FLCN_STATUS                              status;

    // Call super class implementation
    status = pwrPolicyLoad_SUPER(pPolicy, ticksNow);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrPolicyLoad_WORKLOAD_MULTIRAIL_exit;
    }

    // Call MULTIRAIL_INTERFACE implementation
    status = pwrPolicyLoad_WORKLOAD_MULTIRAIL_INTERFACE(pWorkIface);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrPolicyLoad_WORKLOAD_MULTIRAIL_exit;
    }

pwrPolicyLoad_WORKLOAD_MULTIRAIL_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * Retrieves the current GPU state (clocks, voltage, leakage power, etc.) which
 * will be used to callwlate the current workload/active capacitance term (w).
 *
 * @param[in]  pWorkload   PWR_POLICY_WORKLOAD_MULTIRAIL pointer.
 * @param[out] pInput
 *     RM_PMU_PMGR_PWR_POLICY_QUERY_POLICY_WORKLOAD_MULTIRAIL_WORK_INPUT pointer to
 *     populate the with the current GPU state.
 *
 * @return FLCN_OK
 *     All state successfully retrieved.
 * @return FLCN_ERR_ILWALID_STATE
 *     Invalid GPU state returned from the PERF engine.
 */
static FLCN_STATUS
s_pwrPolicyWorkloadMultiRailInputStateGet
(
    PWR_POLICY_WORKLOAD_MULTIRAIL *pWorkload,
    LW2080_CTRL_PMGR_PWR_POLICY_STATUS_DATA_WORKLOAD_MULTIRAIL_WORK_INPUT *pInput
)
{
    FLCN_STATUS     status        = FLCN_OK;

    status = pwrPolicyWorkloadMultiRailInputStateGetCommonParams(&pWorkload->workIface,
        pInput, pwrPolicyLimitUnitGet(pWorkload));
    if (status != FLCN_OK)
    {
        goto s_pwrPolicyWorkloadMultiRailInputStateGet_done;
    }

    // Get GPCCLK frequency in MHz
    pInput->freqMHz = pmgrFreqMHzGet(pwrPoliciesGetGraphicsClkDomain(Pmgr.pPwrPolicies),
                            &pWorkload->clkCntrStart);

    // Translate the graphics clock reading to target 1x clock (GPC)
    pInput->freqMHz =s_pwrPolicyDomGrpFreqTranslate(pInput->freqMHz,
        pwrPoliciesGetGraphicsClkDomain(Pmgr.pPwrPolicies),
        LW2080_CTRL_CLK_DOMAIN_GPCCLK);

s_pwrPolicyWorkloadMultiRailInputStateGet_done:
    return status;
}

/*!
 * Computes the output domain group limit values this power policy wishes to
 * apply.  This is determined as the maximum frequency and voltage values which
 * would yield power at the budget/limit value given the filtered workload
 * value, based on the power equation:
 *
 *      Solve for f using the equations:
 *          P_Limit >= P_TotalPwr(f)
 *      where,
 *          P_TotalPwr(f) is the sum of power required for all rails at f
 *
 * @param[in]   pWorkload   PWR_POLICY_WORKLOAD_MULTIRAIL pointer.
 * @param[out]  pDomGrpLimits
 *     PERF_DOMAIN_GROUP_LIMITS structure to populate with the limits the
 *     PWR_POLICY object wants to apply.
 *
 * @return FLCN_OK
 *     Domain group limit values computed successfully.
 * @return FLCN_ERR_ILWALID_STATE
 *     Encountered an invalid state during computation.
 * @return Other errors
 *     Propagates return values from various calls.
 */
static FLCN_STATUS
s_pwrPolicyWorkloadMultiRailComputeClkMHz
(
    PWR_POLICY_WORKLOAD_MULTIRAIL  *pWorkload,
    PERF_DOMAIN_GROUP_LIMITS       *pDomGrpLimits
)
{
    PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE    *pWorkIface =
        PWR_POLICY_WORKLOAD_MULTIRAIL_IFACE_GET(pWorkload);
    LW2080_CTRL_PMGR_PWR_POLICY_STATUS_DATA_WORKLOAD_MULTIRAIL_FREQ_INPUT *pInput =
        &pWorkIface->freq;
    CLK_DOMAIN                 *pDomain;
    CLK_DOMAIN_PROG            *pDomainProg;
    LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY
                                pstateClkEntry;
    PSTATE         *pPstate;
    LwU8            railIdx;
    LwU16           tempFreqMHz;
    LwU32           limitLwrr;
    LwU32           totalPwr;
    LwU32           freqMHz;
    FLCN_STATUS     status = FLCN_OK;

    // Initialize the required parameters.
    pDomain =
        clkDomainsGetByApiDomain(pwrPoliciesGetGraphicsClkDomain(Pmgr.pPwrPolicies));
    if (pDomain == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto s_pwrPolicyWorkloadMultiRailComputeClkMHz_exit;
    }

    pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
    if (pDomainProg == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto s_pwrPolicyWorkloadMultiRailComputeClkMHz_exit;
    }

    pDomGrpLimits->values[RM_PMU_DOMAIN_GROUP_GPC2CLK] = 0;

    pInput->pgResidency =
        pwrPolicyWorkloadMultiRailMovingAvgFilterQuery(pWorkIface, &pWorkIface->pgResFilter);

    FOR_EACH_VALID_VOLT_RAIL_IFACE(pWorkIface, railIdx)
    {
        pInput->rail[railIdx].workload =
            PWR_POLICY_WORKLOAD_MEDIAN_FILTER_OUTPUT_WORKLOAD_GET(
                &pWorkload->rail[railIdx].median);
    }
    FOR_EACH_VALID_VOLT_RAIL_IFACE_END

    // Get the highest allowed frequency for the graphics clock.
    pPstate = PSTATE_GET_BY_LEVEL(perfGetPstateCount() - 1);
    if (pPstate == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto s_pwrPolicyWorkloadMultiRailComputeClkMHz_exit;
    }

    status = perfPstateClkFreqGet(pPstate,
        clkDomainsGetIdxByDomainGrpIdx(RM_PMU_DOMAIN_GROUP_GPC2CLK),
        &pstateClkEntry);
    if (status != FLCN_OK)
    {
        goto s_pwrPolicyWorkloadMultiRailComputeClkMHz_exit;
    }

    status = clkDomainProgVfMaxFreqMHzGet(
                pDomainProg, &freqMHz);
    if (status != FLCN_OK)
    {
        goto s_pwrPolicyWorkloadMultiRailComputeClkMHz_exit;
    }

    // Bound MAX clock value to PSTATE MAX limit.
    freqMHz = LW_MIN(freqMHz, PWR_POLICY_WORKLOAD_KHZ_TO_MHZ(pstateClkEntry.max.freqkHz));

    // Run-time Optimization - callwlate the per rail voltage floor once.
    status = pwrPolicyWorkloadMultiRailGetVoltageFloor(pWorkIface, 0);
    if (status != FLCN_OK)
    {
        goto s_pwrPolicyWorkloadMultiRailComputeClkMHz_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD_BIDIRECTIONAL_SEARCH) &&
        pWorkload->bBidirectionalSearch)
    {
        LW2080_CTRL_PMGR_PWR_POLICY_STATUS_DATA_WORKLOAD_MULTIRAIL_FREQ_INPUT lastInput;
        LwU32 lastFreqMHz;
        LwU16 freqIdx;
        LwU16 startIdx = clkDomainProgGetIndexByFreqMHz(
                            pDomainProg,
                            PWR_POLICY_WORKLOAD_KHZ_TO_MHZ(pstateClkEntry.min.freqkHz), LW_FALSE);
        LwU16 endIdx   = clkDomainProgGetIndexByFreqMHz(
                            pDomainProg,
                            freqMHz, LW_TRUE);

        if ((startIdx == LW_U16_MAX) || (endIdx == LW_U16_MAX))
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto s_pwrPolicyWorkloadMultiRailComputeClkMHz_exit;
        }

        // Colwert last searched freq from 1x clocks to supported domain (1x/2x)
        lastFreqMHz = s_pwrPolicyDomGrpFreqTranslate(pInput->freqMaxMHz,
            LW2080_CTRL_CLK_DOMAIN_GPCCLK,
            pwrPoliciesGetGraphicsClkDomain(Pmgr.pPwrPolicies));

        // Start from last searched freq if it is valid else start from highest
        if ((lastFreqMHz >= PWR_POLICY_WORKLOAD_KHZ_TO_MHZ(pstateClkEntry.min.freqkHz)) &&
            (lastFreqMHz <= freqMHz))
        {
            freqMHz = lastFreqMHz;
            freqIdx = clkDomainProgGetIndexByFreqMHz(
                        pDomainProg,
                        lastFreqMHz, LW_TRUE);
        }
        else
        {
            freqIdx = endIdx;
            freqMHz = clkDomainProgGetFreqMHzByIndex(
                        pDomainProg, freqIdx);
            if (freqMHz == LW_U16_MAX)
            {
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
                goto s_pwrPolicyWorkloadMultiRailComputeClkMHz_exit;
            }
        }

        pwrPolicyWorkloadMultiRailEvaluateTotalPowerAtFreq(pWorkIface,
            pDomain, freqMHz, &totalPwr, pwrPolicyLimitUnitGet(pWorkload));

        limitLwrr = pwrPolicyLimitLwrrGet(&pWorkload->super.super);

        //
        // If the condition "P_Limit = P_TotalPwr" is satisfied
        // then we have found the match and no further action required.
        //

        // Determine direction of search
        if (limitLwrr > totalPwr)
        {
            while ((limitLwrr > totalPwr) && (endIdx > freqIdx))
            {
                freqIdx++;
                freqMHz = clkDomainProgGetFreqMHzByIndex(
                            pDomainProg, freqIdx);
                if (freqMHz == LW_U16_MAX)
                {
                    status = FLCN_ERR_ILWALID_STATE;
                    PMU_BREAKPOINT();
                    goto s_pwrPolicyWorkloadMultiRailComputeClkMHz_exit;
                }

                lastInput = pWorkIface->freq;
                pwrPolicyWorkloadMultiRailEvaluateTotalPowerAtFreq(pWorkIface,
                    pDomain, freqMHz, &totalPwr, pwrPolicyLimitUnitGet(pWorkload));
            }

            if (limitLwrr < totalPwr)
            {
                // Select the freq and pInput of last iteration.
                freqIdx--;
                freqMHz = clkDomainProgGetFreqMHzByIndex(
                            pDomainProg, freqIdx);
                pWorkIface->freq = lastInput;
            }
        }
        else
        {
            while ((limitLwrr < totalPwr) && (startIdx < freqIdx))
            {
                freqIdx--;
                freqMHz = clkDomainProgGetFreqMHzByIndex(
                            pDomainProg, freqIdx);
                if (freqMHz == LW_U16_MAX)
                {
                    status = FLCN_ERR_ILWALID_STATE;
                    PMU_BREAKPOINT();
                    goto s_pwrPolicyWorkloadMultiRailComputeClkMHz_exit;
                }

                pwrPolicyWorkloadMultiRailEvaluateTotalPowerAtFreq(pWorkIface,
                    pDomain, freqMHz, &totalPwr, pwrPolicyLimitUnitGet(pWorkload));
            }
        }
    }
    else
    {
        //
        // Bound MAX clock value to PSTATE MAX limit.
        // Increment by one to get the correct first iteration value
        //
        tempFreqMHz = freqMHz + 1;

        // Iterate over set of graphics clock freq, starting from the highest freq
        while (clkDomainProgFreqEnumIterate(
                pDomainProg, &tempFreqMHz) == FLCN_OK)
        {
            //
            // As graphics clock freq value in MHz will always be less than 2^16,
            // the assignment from LwU16 -> LwU32 is always safe.
            //
            freqMHz = tempFreqMHz;

            pwrPolicyWorkloadMultiRailEvaluateTotalPowerAtFreq(pWorkIface,
                pDomain, freqMHz, &totalPwr, pwrPolicyLimitUnitGet(pWorkload));

            //
            // If the condition "P_Limit >= P_Rail0 + P_Rail1 + ..." is satisfied,
            // we have found the applicable cap value for given input combo.
            //
            if (pwrPolicyLimitLwrrGet(&pWorkload->super.super) >= totalPwr)
            {
                break;
            }
        }
    }

    // Translate the graphics clock reading to target 1x clock (GPC)
    pInput->freqMaxMHz = s_pwrPolicyDomGrpFreqTranslate(freqMHz,
        pwrPoliciesGetGraphicsClkDomain(Pmgr.pPwrPolicies),
        LW2080_CTRL_CLK_DOMAIN_GPCCLK);

    // Set the new frequency value
    pDomGrpLimits->values[RM_PMU_DOMAIN_GROUP_GPC2CLK] = pInput->freqMaxMHz;

    // As PSTATE 0 contains the entire freq range, we will always be in P0.
    pDomGrpLimits->values[RM_PMU_DOMAIN_GROUP_PSTATE] =
        (perfGetPstateCount() - 1);

s_pwrPolicyWorkloadMultiRailComputeClkMHz_exit:
    return status;
}

/*!
 * Scales the final output clock value to account for ramp rate control and to
 * colwert back 1CLK MHz -> 2CLK kHz.
 *
 * @param[in]     pWorkload   PWR_POLICY_WORKLOAD_MULTIRAIL pointer.
 * @param[in/out] pDomGrpLimits
 *     PERF_DOMAIN_GROUP_LIMITS structure which holds the desired maximum
 *     output clocks as callwlated by the WORKLOAD power policy algorithm. This
 *     is the value which should be scaled/colwerted.
 *
 * @return FLCN_OK
 *     Successfully scaled clocks or no scaling required.
 * @return FLCN_ERR_ILWALID_STATE
 *     Could not get a valid pstate.
 * @return Other errors
 *     Propagates return values from various calls.
 */
static FLCN_STATUS
s_pwrPolicyWorkloadMultiRailScaleClocks
(
    PWR_POLICY_WORKLOAD_MULTIRAIL *pWorkload,
    PERF_DOMAIN_GROUP_LIMITS      *pDomGrpLimits
)
{
    LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY
                pstateClkEntry;
    PSTATE     *pPstate;
    LwUFXP20_12 scaledDiff;
    LwU32       oldLimitValue = pWorkload->super.domGrpLimits.values[RM_PMU_DOMAIN_GROUP_GPC2CLK];
    LwU32       newLimitValue = pDomGrpLimits->values[RM_PMU_DOMAIN_GROUP_GPC2CLK];
    FLCN_STATUS status = FLCN_OK;

    //
    // Ramp rate requires that both old and new limit values are != _DISABLE.
    //
    // 1. If newLimitValue == _DISABLE, done.
    //
    if (RM_PMU_PREF_DOMAIN_GROUP_LIMIT_VALUE_DISABLE == newLimitValue)
    {
        goto s_pwrPolicyWorkloadMultiRailScaleClocks_exit;
    }
    // 2. If oldLimitValue == _DISABLE, must still scale newLimitvalue from CWC
    // clocks -> POR clocks.
    if (RM_PMU_PREF_DOMAIN_GROUP_LIMIT_VALUE_DISABLE == oldLimitValue)
    {
        goto s_pwrPolicyWorkloadMultiRailScaleClocks_ScaleNewLimitValue;
    }

    //
    // Scale the pWorkload clock value to MHz for doing the maths in MHz.
    // Reason: If we do the maths in KHz, scaledDiff (representing GPCCLK) will
    // overflow at 2 ^ 20 KHz = 1024MHz, incresing the chances of hitting the
    // limit if clock change is significant.
    //
    oldLimitValue = PWR_POLICY_WORKLOAD_KHZ_TO_MHZ(oldLimitValue);

    // Translate to 1x clock
    oldLimitValue = s_pwrPolicyDomGrpFreqTranslate(oldLimitValue,
        pwrPoliciesGetGraphicsClkDomain(Pmgr.pPwrPolicies),
        LW2080_CTRL_CLK_DOMAIN_GPCCLK);

    // 1. Scale for clock up
    if ((newLimitValue > oldLimitValue) &&
        (pWorkload->clkUpScale != LW_TYPES_FXP_ZERO))
    {
        //
        // 4.12 * 32.0 = 20.12
        //
        // Will overflow at 1048575 MHz.  This should be a safe assumption.
        //
        scaledDiff    = pWorkload->clkUpScale * (newLimitValue - oldLimitValue);
        newLimitValue = oldLimitValue +
            LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(20, 12, scaledDiff);
    }
    // 2. Scale for clock down.
    else if ((newLimitValue < oldLimitValue) &&
             (pWorkload->clkDownScale != LW_TYPES_FXP_ZERO))
    {
        //
        // 4.12 * 32.0 = 20.12
        //
        // Will overflow at 1048575 MHz.  This should be a safe assumption.
        //
        scaledDiff    = pWorkload->clkDownScale * (oldLimitValue - newLimitValue);
        newLimitValue = oldLimitValue -
            LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(20, 12, scaledDiff);
    }

s_pwrPolicyWorkloadMultiRailScaleClocks_ScaleNewLimitValue:
    // 3. Scale clock value back MHz -> KHz
    newLimitValue = PWR_POLICY_WORKLOAD_MHZ_TO_KHZ(newLimitValue);

    // Translate back from 1x to supported domain (1x/2x) before boundary check and storing.
    newLimitValue = s_pwrPolicyDomGrpFreqTranslate(newLimitValue,
        LW2080_CTRL_CLK_DOMAIN_GPCCLK,
        pwrPoliciesGetGraphicsClkDomain(Pmgr.pPwrPolicies));

    // 4. Bound to MIN clock value in Perf Table.
    pPstate = PSTATE_GET_BY_LEVEL(0);
    if (pPstate == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto s_pwrPolicyWorkloadMultiRailScaleClocks_DetachOverlay;
    }
    status = perfPstateClkFreqGet(pPstate,
        clkDomainsGetIdxByDomainGrpIdx(RM_PMU_DOMAIN_GROUP_GPC2CLK),
        &pstateClkEntry);
    if (status != FLCN_OK)
    {
        goto s_pwrPolicyWorkloadMultiRailScaleClocks_DetachOverlay;
    }
    newLimitValue = LW_MAX(newLimitValue, pstateClkEntry.min.freqkHz);

    // 5. Bound to MAX clock value in Perf Table.
    pPstate = PSTATE_GET_BY_LEVEL(perfGetPstateCount() - 1);
    if (pPstate == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto s_pwrPolicyWorkloadMultiRailScaleClocks_DetachOverlay;
    }
    status = perfPstateClkFreqGet(pPstate,
        clkDomainsGetIdxByDomainGrpIdx(RM_PMU_DOMAIN_GROUP_GPC2CLK),
        &pstateClkEntry);
    if (status != FLCN_OK)
    {
        goto s_pwrPolicyWorkloadMultiRailScaleClocks_DetachOverlay;
    }
    newLimitValue = LW_MIN(newLimitValue, pstateClkEntry.max.freqkHz);

s_pwrPolicyWorkloadMultiRailScaleClocks_DetachOverlay:

    // Store in kHz.
    pDomGrpLimits->values[RM_PMU_DOMAIN_GROUP_GPC2CLK] = newLimitValue;
s_pwrPolicyWorkloadMultiRailScaleClocks_exit:
    return status;
}

/*!
 * Callwlates the MSCG and PG residency in the period from last policy evaluation
 * to current policy evaluation.
 *
 * @param[in]     pWorkload   PWR_POLICY_WORKLOAD_MULTIRAIL pointer.
 */
static FLCN_STATUS
s_pwrPolicyWorkloadMultiRailResidencyGet
(
    PWR_POLICY_WORKLOAD_MULTIRAIL  *pWorkload
)
{
    PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE *pWorkIface =
        PWR_POLICY_WORKLOAD_MULTIRAIL_IFACE_GET(pWorkload);
    FLCN_STATUS status = FLCN_OK;

    // Get MSCG residency if feature is enabled.
    if (pWorkIface->bMscgResidencyEnabled)
    {
        pWorkIface->work.mscgResidency =
            pwrPolicyWorkloadResidencyCompute(
                &pWorkload->mscgLpwrTime,
                RM_PMU_LPWR_CTRL_ID_MS_MSCG);
    }
    else
    {
        pWorkIface->work.mscgResidency = LW_TYPES_FXP_ZERO;
    }

    // Get PG residency if feature is enabled.
    if (pWorkIface->bPgResidencyEnabled)
    {
        pWorkIface->work.pgResidency =
            pwrPolicyWorkloadResidencyCompute(
                &pWorkload->pgLpwrTime,
                RM_PMU_LPWR_CTRL_ID_GR_PG);

        // Insert into moving average filter
        status = pwrPolicyWorkloadMultiRailMovingAvgFilterInsert(pWorkIface,
            &pWorkIface->pgResFilter, pWorkIface->work.pgResidency);
    }
    else
    {
        pWorkIface->work.pgResidency = LW_TYPES_FXP_ZERO;
    }

    return status;
}

/*!
 * @copydoc BoardObjVirtualTableGetInterfaceFromBoardObj
 */
static BOARDOBJ_INTERFACE *
s_pwrPolicyGetInterfaceFromBoardObj_WorkloadMultiRail
(
    BOARDOBJ   *pBoardObj,
    LwU8        interfaceType
)
{
    PWR_POLICY_WORKLOAD_MULTIRAIL *pWorkload = (PWR_POLICY_WORKLOAD_MULTIRAIL *)pBoardObj;

    switch (interfaceType)
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_INTERFACE_TYPE_WORKLOAD_MULTIRAIL_INTERFACE:
        {
            return &pWorkload->workIface.super;
        }
    }

    PMU_BREAKPOINT();
    return NULL;
}
