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
 * @file   pwrpolicy_workload.c
 * @brief  Interface specification for a PWR_POLICY_WORKLOAD.
 *
 * Workload Algorithm Theory:
 *
 * The Workload Power Policy is a power/current controller object which regulates power/current
 * to a power/current limit/budget.  This controller uses the equations for dynamic and
 * leakage power.
 *
 * When it operates as a power based controller
 * Pdynamic = V^2 * f * w
 *     V = voltage
 *     f = frequency
 *     w = workload (@note: This term absorbs the common constant coefficient
 *             "k")
 *
 * Pleakage = Leakage power value callwlated by @ref PWR_EQUATION_LEAKAGE.
 *
 * Ptotal = Pdynamic + Pleakage
 *
 * When it operates as a current based controller
 * The above power based equation can be adjusted by diving by voltage
 * Idynamic = V * f * w = Pdynamic / V
 * Ileakage = Pleakage / V
 * Itotal = Idynamic + Ileakage;
 *
 * 1. Measure/observe core power (Ptotal) and etimate leakage power (Pleakage).
 * From those values, compute workload/active capacitance (w).
 *
 *     w = (Ptotal - Pleakage) / (V^2 * f)
 *
 * 2. Given computed workload factor, find the maximum clocks (and corresponding
 * voltages) which would generate a power within the given limit.
 *
 *     Solve for f: Plimit = V(f)^2 * f * w + Pleakage
 *
 * 3. Apply that value as a maximum limit on clocks (and corresponding voltages)
 * via an OBJPERF PERF_LIMIT.
 *
 *
 * Workload Algorithm Tuning:
 *
 * The workload algorithm uses the following parameters to tune its response for
 * better behavior, response to transients.
 *
 * 1. Workload Median Filter - A median filter of size N is applied to the
 * observed workload values, when solving for the maximum clocks. For more
 * information about median filters see:
 * http://en.wikipedia.org/wiki/Median_filter.
 *
 * 2. Clock Ramp Rate Control - Changes to the maximum clock limit are ramped,
 * with separate ramp rates for up down.
 *
 *    Ramping Up:
 *       clkTarget' = clkLwrr + (clkTarget - clkLwrr) * clkUpSlope
 *
 *    Ramping Down:
 *       clkTarget' = clkLwrr - (clkLwrr - clkTarget) * clkDownSlope
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objperf.h"
#include "pmu_objpmgr.h"
#include "therm/thrmintr.h"
#include "pmgr/pwrpolicy_workload.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static FLCN_STATUS s_pwrPolicyWorkloadInputStateGet(PWR_POLICY_WORKLOAD *pWorkload, RM_PMU_PMGR_PWR_POLICY_QUERY_POLICY_WORKLOAD_WORK_INPUT *pInput)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadInputStateGet");
static LwUFXP20_12 s_pwrPolicyWorkloadComputeWorkload(PWR_POLICY_WORKLOAD *pWorkload, RM_PMU_PMGR_PWR_POLICY_QUERY_POLICY_WORKLOAD_WORK_INPUT *pInput)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadComputeWorkload");
static FLCN_STATUS s_pwrPolicyWorkloadComputeClkMHz(PWR_POLICY_WORKLOAD *pWorkload, PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadComputeClkMHz");
static LwU32       s_pwrPolicyWorkloadVfEntryComputeClkMHz(PWR_POLICY_WORKLOAD *pWorkload, RM_PMU_PMGR_PWR_POLICY_QUERY_POLICY_WORKLOAD_FREQ_INPUT *pInput)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadVfEntryComputeClkMHz");
static void        s_pwrPolicyWorkloadScaleClocks(PWR_POLICY_WORKLOAD *pWorkload, PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits, LwBool *pBCopyLimits)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadScaleClocks");
static FLCN_STATUS s_pwrPolicyWorkloadLeakageCompute(PWR_POLICY_WORKLOAD *pWorkload, LwU32 voltageuV, LwU32 *pLeakageVal)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadLeakageCompute");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Construct a _WORKLOAD PWR_POLICY object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PMGR_PWR_POLICY_WORKLOAD_BOARDOBJ_SET *pWorkloadSet    =
        (RM_PMU_PMGR_PWR_POLICY_WORKLOAD_BOARDOBJ_SET *)pBoardObjSet;
    PWR_POLICY_WORKLOAD                          *pWorkload;
    FLCN_STATUS                                   status;

    //
    // TODO-aherring: To avoid issues with conditional compilation that makes
    // this variable unused. Can be removed if conditional compilation changed
    //
    (void)pBObjGrp;

    // Sanity check input.
    if ((Pmgr.pwr.equations.objMask.super.pData[0] & BIT(pWorkloadSet->leakageIdx)) == 0)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NOT_SUPPORTED;
        goto pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_exit;
    }

    // Construct and initialize parent-class object.
    status = pwrPolicyGrpIfaceModel10ObjSetImpl_DOMGRP(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_exit;
    }
    pWorkload = (PWR_POLICY_WORKLOAD *)*ppBoardObj;

    // Copy in the _WORKLOAD type-specific data.
    pWorkload->clkUpScale   = pWorkloadSet->clkUpScale;
    pWorkload->clkDownScale = pWorkloadSet->clkDownScale;
    pWorkload->pLeakage     = PWR_EQUATION_GET(pWorkloadSet->leakageIdx);
    if (pWorkload->pLeakage == NULL)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        goto pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_exit;
    }

    // Allocate buffers for median filter, if not already allocated.
    if (pWorkload->median.pEntries == NULL)
    {
        //
        // Need two (2) buffers of same size, so instead of two allocations
        // (with duplicate error checking and the second one could possibly
        // fail), allocate in one double-sized buffer and then split the buffer
        // appropriately.
        //
        pWorkload->median.pEntries =
            lwosCallocType(
                PWR_POLICY_ALLOCATIONS_DMEM_OVL(pBObjGrp->ovlIdxDmem),
                2 * pWorkloadSet->medianFilterSize,
                PWR_POLICY_WORKLOAD_MEDIAN_FILTER_ENTRY);
        if (pWorkload->median.pEntries == NULL)
        {
            status = FLCN_ERR_NO_FREE_MEM;
            goto pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_exit;
        }

        // Assign this buffer from the common allocation above.
        pWorkload->median.pEntriesSort =
            &pWorkload->median.pEntries[pWorkloadSet->medianFilterSize];
    }
    // Otherwise, assert that filter size is the same length!
    else
    {
        PMU_HALT_COND(pWorkload->median.sizeTotal ==
            pWorkloadSet->medianFilterSize);
    }

    // Initialize the state of the median filter.
    pWorkload->median.idx        = 0;
    pWorkload->median.sizeTotal  = pWorkloadSet->medianFilterSize;
    pWorkload->median.sizeLwrr   = 0;
    memset(&pWorkload->median.entryLwrrOutput, 0,
        sizeof(PWR_POLICY_WORKLOAD_MEDIAN_FILTER_ENTRY));
    pWorkload->bViolThresEnabled = pWorkloadSet->bViolThresEnabled;
    pWorkload->violThreshold     = pWorkloadSet->violThreshold;
    pWorkload->lastViolTime      = 0;
    pWorkload->violPct           = 0;

pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_exit:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
pwrPolicyIfaceModel10GetStatus_WORKLOAD
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PMGR_PWR_POLICY_WORKLOAD_BOARDOBJ_GET_STATUS *pGetStatus;
    PWR_POLICY_WORKLOAD                                 *pWorkload;
    FLCN_STATUS                                          status;

    // Call _DOMGRP super-class implementation.
    status = pwrPolicyIfaceModel10GetStatus_DOMGRP(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        return status;
    }
    pWorkload  = (PWR_POLICY_WORKLOAD *)pBoardObj;
    pGetStatus = (RM_PMU_PMGR_PWR_POLICY_WORKLOAD_BOARDOBJ_GET_STATUS *)pPayload;

    // Copy out the state to the RM.
    pGetStatus->work     = pWorkload->work;
    pGetStatus->freq     = pWorkload->freq;
    pGetStatus->workload = pWorkload->workload;

    return status;
}

/*!
 * WORKLOAD-implementation of filtering.  Applies the median filter on the
 * workload term and stores the information related to the filtered out.
 *
 * @copydoc PwrPolicyFilter
 */
FLCN_STATUS
pwrPolicyFilter_WORKLOAD
(
    PWR_POLICY  *pPolicy,
    PWR_MONITOR *pMon
)
{
    PWR_POLICY_WORKLOAD                    *pWorkload   =
        (PWR_POLICY_WORKLOAD *)pPolicy;
    PWR_POLICY_WORKLOAD_MEDIAN_FILTER_ENTRY filterInput;

    //
    // 1. Read current state of the system.  Bail on unexpected errors if PERF
    // subsystem not initialized.
    //
    pWorkload->status = s_pwrPolicyWorkloadInputStateGet(pWorkload, &pWorkload->work);
    if (pWorkload->status != FLCN_OK)
    {
        return pWorkload->status;
    }

    // 2. Callwlate the workload from the current power.
    pWorkload->workload =
        s_pwrPolicyWorkloadComputeWorkload(pWorkload, &pWorkload->work);

    // 3. Insert latest sample into the Workload Median Filter.
    filterInput.workload = pWorkload->workload;
    filterInput.value = pWorkload->super.super.valueLwrr;
    pwrPolicyWorkloadMedianFilterInsert_SHARED(&pWorkload->median, &filterInput);

    return FLCN_OK;
}

/*!
 * @copydoc PwrPolicy3XFilter
 */
FLCN_STATUS
pwrPolicy3XFilter_WORKLOAD
(
    PWR_POLICY  *pPolicy,
    PWR_MONITOR *pMon,
    PWR_POLICY_3X_FILTER_PAYLOAD_TYPE *pPayload
)
{
    FLCN_STATUS status;

    // Call the super class implementation.
    status = pwrPolicy3XFilter_SUPER(pPolicy, pMon, pPayload);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Apply filtering
    return pwrPolicyFilter_WORKLOAD(pPolicy, pMon);
}

/*!
 * WORKLOAD-implementation to return the input value corresponding to the output
 * of the median filter.
 *
 * @copydoc PwrPolicyValueLwrrGet
 */
LwU32
pwrPolicyValueLwrrGet_WORKLOAD
(
    PWR_POLICY *pPolicy
)
{
    PWR_POLICY_WORKLOAD *pWorkload = (PWR_POLICY_WORKLOAD *)pPolicy;

    return PWR_POLICY_WORKLOAD_MEDIAN_FILTER_OUTPUT_VALUE_GET(
            &pWorkload->median);
}

/*!
 * WORKLOAD-implementation to evaluate the power limit and callwlate the
 * corresponding domain group limits which will provide power <= the limit.
 *
 * @pre @ref pwrPolicyFilter_WORKLOAD() must be called every iteration to
 * callwlate and filter the workload term before calling this function.
 *
 * @copydoc PwrPolicyDomGrpEvaluate
 */
FLCN_STATUS
pwrPolicyDomGrpEvaluate_WORKLOAD
(
    PWR_POLICY_DOMGRP *pDomGrp
)
{
    FLCN_STATUS              status       = FLCN_OK;
    PWR_POLICY_WORKLOAD     *pWorkload    = (PWR_POLICY_WORKLOAD *)pDomGrp;
    PERF_DOMAIN_GROUP_LIMITS domGrpLimits = {{ 0 }};
    LwBool                   bCopyLimits;

    // Bail out if the system isn't ready for power policy actions yet.
    if (pWorkload->status != FLCN_OK)
    {
        return pWorkload->status;
    }

    //
    // 1. Callwlate clocks for workload and power limit.
    //
    // Use stack variable domGrpLimits because we depend on current values in
    // PWR_POLICY_DOMGRP::domGrpLimits for ramp-rate control in
    // s_pwrPolicyWorkloadScaleClocks() below.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        s_pwrPolicyWorkloadComputeClkMHz(pWorkload, &domGrpLimits),
        pwrPolicyDomGrpEvaluate_WORKLOAD_exit);

    //
    // 2. Scale clocks for ramp-rate control and scale back to domain-group
    // units.
    //
    s_pwrPolicyWorkloadScaleClocks(pWorkload, &domGrpLimits, &bCopyLimits);

    // 3. Update/cache the domain group limits for this policy (if required).
    if (bCopyLimits)
    {
        pWorkload->super.domGrpLimits = domGrpLimits;
    }

pwrPolicyDomGrpEvaluate_WORKLOAD_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * Retrieves the current GPU state (clocks, voltage, leakage power, etc.) which
 * will be used to callwlate the current workload/active capacitance term (w).
 *
 * @param[in]  pWorkload   PWR_POLICY_WORKLOAD pointer.
 * @param[out] pInput
 *     RM_PMU_PMGR_PWR_POLICY_QUERY_POLICY_WORKLOAD_WORK_INPUT pointer to
 *     populate the with the current GPU state.
 *
 * @return FLCN_OK
 *     All state successfully retrieved.
 * @return FLCN_ERR_ILWALID_STATE
 *     Invalid GPU state returned from the PERF engine.  This is most likely due
 *     to the fact that the PMU PERF engine has not yet received all its
 *     necessary data from the RM.  This error is only expected to be
 *     encountered in the early phases of PMU initialization.
 */
static FLCN_STATUS
s_pwrPolicyWorkloadInputStateGet
(
    PWR_POLICY_WORKLOAD *pWorkload,
    RM_PMU_PMGR_PWR_POLICY_QUERY_POLICY_WORKLOAD_WORK_INPUT *pInput
)
{
    FLCN_STATUS status = FLCN_OK;
    // Read the current voltage.
    LwU32 voltageuV = perfVoltageuVGet();
    if (voltageuV == 0)
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto s_pwrPolicyWorkloadInputStateGet_exit;
    }

    // Callwlate leakage power for voltage.
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_LWRRENT_AWARE))
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrPolicyWorkloadLeakageCompute(pWorkload, voltageuV, &pInput->leakagemX),
            s_pwrPolicyWorkloadInputStateGet_exit);
    }
    else
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            pwrEquationLeakageEvaluatemW(
                (PWR_EQUATION_LEAKAGE *)pWorkload->pLeakage,
                voltageuV,
                NULL /* PGres = 0.0 */, &pInput->leakagemX),
            s_pwrPolicyWorkloadInputStateGet_exit);
    }

    // Get GPCCLK frequency in MHz
    pInput->freqMHz = pmgrFreqMHzGet(LW2080_CTRL_CLK_DOMAIN_GPC2CLK,
                            &pWorkload->clkCntrStart);
    pInput->freqMHz /= 2;

    // Get voltage in mV.
    pInput->voltmV = PWR_POLICY_WORKLOAD_UV_TO_MV(voltageuV);

s_pwrPolicyWorkloadInputStateGet_exit:
    return status;
}

/*!
 * Computes the current workload/active capacitance term (w) given the observed
 * power and GPU state.
 *
 * @param[in] pWorkload   PWR_POLICY_WORKLOAD pointer.
 * @param[in] pInput
 *     RM_PMU_PMGR_PWR_POLICY_QUERY_POLICY_WORKLOAD_WORK_INPUT pointer to
 *     containing the current GPU state.
 *
 * @return Workoad/active capacitance term (w) in unsigned FXP 20.12 format,
 *     units mW / Mhz / mV^2 or mA / MHz / mV
 */
static LwUFXP20_12
s_pwrPolicyWorkloadComputeWorkload
(
    PWR_POLICY_WORKLOAD *pWorkload,
    RM_PMU_PMGR_PWR_POLICY_QUERY_POLICY_WORKLOAD_WORK_INPUT *pInput
)
{
    LwUFXP20_12 workload;

    //
    // Assume 0 workload for the following tricky numerical cases (i.e. avoiding
    // negative numbers or dividing by zero):
    // 1. Power is less than leakage callwlation.
    // 2. Measured frequency is zero.
    //
    if ((pWorkload->super.super.valueLwrr < pInput->leakagemX) ||
        (pInput->freqMHz == 0))
    {
        return LW_TYPES_FXP_ZERO;
    }

    //
    // POWER:
    // Ptotal = Pdynamic + Pleakage
    //        = V^2 * f * w + Pleakage
    //
    // w = (Ptotal - Pleakage) / (V^2 * f)
    //
    // mW * mW
    // -----------
    //      mV^2 MHz

    //
    // CURRENT:
    // Itotal = Idynamic + Ileakage
    //        = V * f * w + Ileakage
    //
    // w = (Itotal - Ileakage) / (V * f)
    //
    // Numerical Analysis:
    //
    //     32.0 << 12    => 20.12
    //  / 32.0           => 32.0
    //  / 32.0           => 32.0
    // ----------------------------
    //                      20.12
    //
    // Will overflow when power/current difference is > 1048575 mW/mA, this
    // should a safe assumption.
    //
    // Note: 32BIT_OVERFLOW - Possible FXP20.12 overflow, not going to fix
    // because this code segment is tied to the @ref PMU_PMGR_PWR_POLICY_WORKLOAD
    // feature and is not used on AD10X and later chips.
    //
    workload =
        LW_TYPES_U32_TO_UFXP_X_Y_SCALED(20, 12,
            (pWorkload->super.super.valueLwrr - pInput->leakagemX),
             pInput->freqMHz);

    if (pwrPolicyLimitUnitGet(pWorkload) ==
            LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_POWER_MW)
    {
        workload = LW_UNSIGNED_ROUNDED_DIV(workload,
            LW_UNSIGNED_ROUNDED_DIV((pInput->voltmV * pInput->voltmV), 1000));
    }
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_LWRRENT_AWARE)
    else
    {
        workload = LW_UNSIGNED_ROUNDED_DIV(workload,
                            pInput->voltmV);
    }
#endif

    return workload;
}

/*!
 * Computes the output domain group limit values this power policy wishes to
 * apply.  This is determined as the maximum frequency and voltage values which
 * would yield power at the budget/limit value given the filtered workload
 * value, based on the power equation:
 *
 *     Solve for f: Plimit = V(f)^2 * f * w + Pleakage
 *
 * @param[in]   pWorkload   PWR_POLICY_WORKLOAD pointer.
 * @param[out]  pDomGrpLimits
 *     PERF_DOMAIN_GROUP_LIMITS structure to populate with the limits the
 *     PWR_POLICY object wants to apply.
 */
static FLCN_STATUS
s_pwrPolicyWorkloadComputeClkMHz
(
    PWR_POLICY_WORKLOAD      *pWorkload,
    PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits
)
{
    FLCN_STATUS status                                              =
        FLCN_OK;
    RM_PMU_PERF_PSTATE_DOMAIN_GROUP_INFO *pPstateDomGrp;
    RM_PMU_PERF_VF_ENTRY_INFO            *pVfEntry;
    RM_PMU_PMGR_PWR_POLICY_QUERY_POLICY_WORKLOAD_FREQ_INPUT *pInput =
        &pWorkload->freq;
    LwU32  voltageuV;
    LwU32  freqMaxMHz;
    LwU32  freqCeilingMHz;
    LwU8   p;
    LwU8   v;

    // Run-time Optimization - callwlate the frequency ceiling (in GPCCLK MHz) once.
    freqCeilingMHz = PWR_POLICY_WORKLOAD_2KHZ_TO_MHZ(
                        pWorkload->super.domGrpCeiling.values[RM_PMU_DOMAIN_GROUP_GPC2CLK]);

    // Set the frequency cap to 0, to pass the checks against maxFreqMHz below.
    pDomGrpLimits->values[RM_PMU_DOMAIN_GROUP_GPC2CLK] = 0;

    // Initialize the workload input only once, this won't change below.
    pInput->workload =
        PWR_POLICY_WORKLOAD_MEDIAN_FILTER_OUTPUT_WORKLOAD_GET(
            &pWorkload->median);

    //
    // Iterate over set of pstates, starting from the highest pstate allowed via
    // the object's Domain Group Ceiling;
    //
    p = pWorkload->super.domGrpCeiling.values[RM_PMU_PERF_DOMAIN_GROUP_PSTATE];
    do
    {
        pPstateDomGrp = perfPstateDomGrpGet(p, RM_PMU_DOMAIN_GROUP_GPC2CLK);

        // Iterate over set of VF entries for this pstate.
        v = pPstateDomGrp->vfIdxLast;
        do
        {
            pVfEntry = perfVfEntryGet(v);

            //
            // If the max frequency of this VF entry is lower than the
            // previously callwlated limit clock (at higher VF entry), have
            // found the applicable cap value within this pstate.
            //
            freqMaxMHz = PWR_POLICY_WORKLOAD_2KHZ_TO_MHZ(
                            perfVfEntryGetMaxFreqKHz(pPstateDomGrp, pVfEntry));
            if (freqMaxMHz < pDomGrpLimits->values[RM_PMU_DOMAIN_GROUP_GPC2CLK])
            {
                break;
            }

            // Set voltage will be the max of the pstate and VF entry.
            voltageuV = LW_MAX(perfPstateMilwoltageuVGet(p),
                            pVfEntry->voltageuV);

            //
            // Input parameters for @ref
            // s_pwrPolicyWorkloadVfEntryComputeClkMHz().
            //
            if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_LWRRENT_AWARE))
            {
                PMU_ASSERT_OK_OR_GOTO(status,
                    s_pwrPolicyWorkloadLeakageCompute(pWorkload, voltageuV, &pInput->leakagemX),
                    s_pwrPolicyWorkloadComputeClkMHz_exit);
            }
            else
            {
                PMU_ASSERT_OK_OR_GOTO(status,
                    pwrEquationLeakageEvaluatemW(
                           (PWR_EQUATION_LEAKAGE *)pWorkload->pLeakage,
                           voltageuV,
                           NULL /* PGres = 0.0 */, &pInput->leakagemX),
                    s_pwrPolicyWorkloadComputeClkMHz_exit);
            }

            pInput->freqMaxMHz = freqMaxMHz;
            pInput->voltmV = PWR_POLICY_WORKLOAD_UV_TO_MV(voltageuV);
            pInput->vfEntryIdx = v; // Just for debugging!

            //
            // Callwlate the corresponding frequency limit for this {pstate, VF
            // entry} combo.
            //
            pDomGrpLimits->values[RM_PMU_DOMAIN_GROUP_GPC2CLK] = LW_MIN(
                s_pwrPolicyWorkloadVfEntryComputeClkMHz(pWorkload, pInput),
                freqCeilingMHz);
            // Store the pstate index in the output structure as well.
            pDomGrpLimits->values[RM_PMU_DOMAIN_GROUP_PSTATE] = p;
        } while (v-- > pPstateDomGrp->vfIdxFirst);

        //
        // If the frequency cap is greater than the minimum frequncy for the
        // domain group, then this is the final cap.
        //
        if (pDomGrpLimits->values[RM_PMU_DOMAIN_GROUP_GPC2CLK] >=
            PWR_POLICY_WORKLOAD_2KHZ_TO_MHZ(pPstateDomGrp->minFreqKHz))
        {
            break;
        }

    } while (p-- > 0);

s_pwrPolicyWorkloadComputeClkMHz_exit:
     return status;
}

/*!
 * Computes the maximum possible clock frequency which would provide power <=
 * the limit value for the given voltage and workload value.  The provided
 * voltage is determined based on the pstate and VF values.  The maximum clock
 * frequency is determined based on the following equation:
 *
 *     Solve for f:
 *         Plimit = V(f)^2 * f * w + Pleakage
 *         OR
 *         Ilimit = V(f) * f * w + Ileakage
 *
 * @param[in]   pWorkload   PWR_POLICY_WORKLOAD pointer.
 * @param[in]   pInput
 *     RM_PMU_PMGR_PWR_POLICY_QUERY_POLICY_WORKLOAD_FREQ_INPUT pointer to
 *     structure which holds all the input values for the frequency callwlation.
 *     The calling function populates this strulwtre as it iterates over the set
 *     of possible pstate/VF entry combinations.
 *
 * @return Maximum possible clock frequency in 1CLK MHz.
 */
static LwU32
s_pwrPolicyWorkloadVfEntryComputeClkMHz
(
    PWR_POLICY_WORKLOAD *pWorkload,
    RM_PMU_PMGR_PWR_POLICY_QUERY_POLICY_WORKLOAD_FREQ_INPUT *pInput
)
{
    LwU32 clkMHz;

    // If no workload, assume the maximum clock.
    if (pInput->workload == LW_TYPES_FXP_ZERO)
    {
        return pInput->freqMaxMHz;
    }

    //
    // If leakage would be greater than the budget, assume 0 clock and that a
    // lower pstate/VF entry will work.
    //
    if (pInput->leakagemX > pwrPolicyLimitLwrrGet(&pWorkload->super.super))
    {
        return 0;
    }

    //
    // POWER:
    // Plimit = Pdynamic + Pleakage
    //        = V^2 * f * w + Pleakage
    //
    // f = (Plimit - Pleakage) / (V^2 * w)
    //
    //        1         mV^2 * MHz
    // mW  * ------- * ------------ = MHz
    //        mV^2        mW
    //
    // CURRENT:
    // Ilimit = Idynamic + Ileakage
    //        = V * f * w + Ileakage
    //
    // f = (Ilimit - Ileakage) / (V * w)
    //
    //        1      mV * MHz
    // mA  * ---- * ---------- = MHz
    //        mV        mA
    //
    // Numerical Analysis:
    //
    //   32.0 <<  12  => 20.12
    // / 32.0         => 32.0
    // / 20.12        => 20.12
    // ------------------------
    //                   20.0
    //
    // Will overflow when power/lwrrenet difference is > 1048575 mW/mA, this
    // should a safe assumption.
    //
    // Note: 32BIT_OVERFLOW - Possible FXP20.12 overflow, not going to fix
    // because this code segment is tied to the @ref PMU_PMGR_PWR_POLICY_WORKLOAD
    // feature and is not used on AD10X and later chips.
    //
    clkMHz = LW_TYPES_U32_TO_UFXP_X_Y_SCALED(20, 12,
                (pwrPolicyLimitLwrrGet(&pWorkload->super.super)
                    - pInput->leakagemX),
                pInput->workload);

    if (pwrPolicyLimitUnitGet(pWorkload) ==
            LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_POWER_MW)
    {
        clkMHz = LW_UNSIGNED_ROUNDED_DIV(clkMHz,
            LW_UNSIGNED_ROUNDED_DIV((pInput->voltmV * pInput->voltmV) , 1000));
    }
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_LWRRENT_AWARE)
    else
    {
        clkMHz = LW_UNSIGNED_ROUNDED_DIV(clkMHz, pInput->voltmV);
    }
#endif
    // Cap to maximum frequency.
    return LW_MIN(clkMHz, pInput->freqMaxMHz);
}

/*!
 * Scales the final output clock value to account for ramp rate control and to
 * colwert back 1CLK MHz -> 2CLK kHz.
 *
 * @param[in]     pWorkload   PWR_POLICY_WORKLOAD pointer.
 * @param[in/out] pDomGrpLimits
 *     PERF_DOMAIN_GROUP_LIMITS structure which holds the desired maximum
 *     output clocks as callwlated by the WORKLOAD power policy algorithm.  This
 *     is the value which should be scaled/colwerted.
 * @param[out]    pBCopyLimits
 *     Returns LW_FALSE if new limits should be dropped, LW_TRUE otherwise.
 */
static void
s_pwrPolicyWorkloadScaleClocks
(
    PWR_POLICY_WORKLOAD         *pWorkload,
    PERF_DOMAIN_GROUP_LIMITS    *pDomGrpLimits,
    LwBool                      *pBCopyLimits
)
{
    LwU32 oldLimitValue;
    LwU32 newLimitValue;

    *pBCopyLimits = LW_TRUE;

    oldLimitValue =
        pWorkload->super.domGrpLimits.values[RM_PMU_DOMAIN_GROUP_GPC2CLK];
    newLimitValue = pDomGrpLimits->values[RM_PMU_DOMAIN_GROUP_GPC2CLK];

    //
    // BUG: 1685356 - We are not using the clock scale functionality in the
    // process of updating the clocks based on the observed workload. So here
    // these "if - elseIf" block are basically dead code. We tryed to fix this
    // BUG, but we were running into MODS sanity failure. We will FIX this ASAP.
    // NOTE: Even without the clock scale code, the CWC is working as expected.
    //
    if (newLimitValue != RM_PMU_PREF_DOMAIN_GROUP_LIMIT_VALUE_DISABLE)
    {
        RM_PMU_PERF_PSTATE_DOMAIN_GROUP_INFO *pPstateDomGrp;
        LwUFXP20_12 scaledDiff;

        // 1. Scale for clock up
        if ((newLimitValue > oldLimitValue) &&
            (pWorkload->clkUpScale != LW_TYPES_FXP_ZERO))
        {
            //
            // 4.12 * 32.0 = 20.12
            //
            // Will overflow at 1048575 MHz.  This should be a safe assumption.
            //
            scaledDiff = pWorkload->clkUpScale * (newLimitValue - oldLimitValue);
            newLimitValue = oldLimitValue +
                LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(20, 12, scaledDiff);
        }
        // 2. Scale for clock down.
        else if ((newLimitValue > oldLimitValue) &&
                 (pWorkload->clkDownScale != LW_TYPES_FXP_ZERO))
        {
            //
            // 4.12 * 32.0 = 20.12
            //
            // Will overflow at 1048575 MHz.  This should be a safe assumption.
            //
            scaledDiff = pWorkload->clkDownScale * (oldLimitValue - newLimitValue);
            newLimitValue = oldLimitValue -
                LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(20, 12, scaledDiff);
        }

        // 3. Scale clock value back MHz -> 2KHz
        newLimitValue = PWR_POLICY_WORKLOAD_MHZ_TO_2KHZ(newLimitValue);

        // 4. Bound to MIN clock value in Perf Table.
        pPstateDomGrp = perfPstateDomGrpGet(0, RM_PMU_DOMAIN_GROUP_GPC2CLK);
        newLimitValue = LW_MAX(newLimitValue, pPstateDomGrp->minFreqKHz);

        // 5. Bound to MAX clock value in Perf Table.
        pPstateDomGrp = perfPstateDomGrpGet(perfGetPstateCount() - 1,
                                            RM_PMU_DOMAIN_GROUP_GPC2CLK);
        newLimitValue = LW_MIN(newLimitValue, pPstateDomGrp->maxFreqKHz);

        pDomGrpLimits->values[RM_PMU_DOMAIN_GROUP_GPC2CLK] = newLimitValue;
    }

    //
    // 6. Check if HWFS violation time percentage is above threshold.
    // If yes, do not raise limit above previous capping limit.
    //
    if (pWorkload->bViolThresEnabled)
    {
        LwU32 evalTime = pWorkload->lastEvalTime.parts.lo;
        LwU32 violTime = pWorkload->lastViolTime;

        // Code should not get interrupted/preempted when sampling timers.
        appTaskCriticalEnter();
        {
            osPTimerTimeNsLwrrentGet(&pWorkload->lastEvalTime);

            (void)thermEventViolationGetTimer32(RM_PMU_THERM_EVENT_META_ANY_HW_FS,
                                                &pWorkload->lastViolTime);
        }
        appTaskCriticalExit();

        //
        // Time in [ns] expired since 'lastEvalTime' (limited to 4.29 sec.).
        // Avoiding use of osPTimerTimeNsElapsedGet() to save on reg. accesses.
        //
        evalTime = pWorkload->lastEvalTime.parts.lo - evalTime;
        //
        // Time in [ns] that this event has spent in asserted state (elapsed
        // since 'lastEvalTime').
        //
        violTime = pWorkload->lastViolTime - violTime;

        // Compute violation time in percents.
        evalTime           = LW_UNSIGNED_ROUNDED_DIV(evalTime, 100);
        pWorkload->violPct = (LwU8)LW_UNSIGNED_ROUNDED_DIV(violTime, evalTime);

        //
        // If we exceed threshold, and the new limit is higher than current
        // limit, leave lwrent limits untouched.
        //
        if ((newLimitValue != RM_PMU_PREF_DOMAIN_GROUP_LIMIT_VALUE_DISABLE) &&
            (pWorkload->violPct > pWorkload->violThreshold) &&
            (newLimitValue > oldLimitValue))
        {
            *pBCopyLimits = LW_FALSE;
        }
    }
}

/*!
 * Helper function to estimate the leakage value for a given voltage.
 *
 * @param[in]        pWorkload       PWR_POLICY_WORKLOAD pointer.
 * @param[in]        voltageuV       Voltage for which to estimate leakage.
 * @@param[in/out]   pLeakageVal     Leakage power in mW/ current in mA
 *
 * @return FLCN_OK
 *      successfully got the Leakage value in appropriate units depending on 
 *      @ref PWR_POLICY::limitUnit: _POWER_MW -> mW, _LWRRENT_MA -> mA.
 * @return Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
FLCN_STATUS
s_pwrPolicyWorkloadLeakageCompute
(
    PWR_POLICY_WORKLOAD *pWorkload,
    LwU32                voltageuV,
    LwU32               *pLeakageVal
)
{
    FLCN_STATUS status = FLCN_OK;

    if (LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_POWER_MW ==
            pwrPolicyLimitUnitGet(pWorkload))
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            pwrEquationLeakageEvaluatemW(
                (PWR_EQUATION_LEAKAGE *)pWorkload->pLeakage,
                voltageuV,
                NULL /* PGres = 0.0 */, pLeakageVal),
            s_pwrPolicyWorkloadLeakageCompute_exit);
    }
    else if (LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_LWRRENT_MA ==
                pwrPolicyLimitUnitGet(pWorkload))
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            pwrEquationLeakageEvaluatemA(
                (PWR_EQUATION_LEAKAGE *)pWorkload->pLeakage,
                voltageuV,
                NULL /* PGres = 0.0 */, pLeakageVal),
            s_pwrPolicyWorkloadLeakageCompute_exit);
    }

s_pwrPolicyWorkloadLeakageCompute_exit:
    return status;
}
