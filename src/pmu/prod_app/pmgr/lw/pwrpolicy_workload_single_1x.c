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
 * @file   pwrpolicy_workload_single_1x.c
 * @brief  Specification for a PWR_POLICY_WORKLOAD_SINGLE_1X.
 */
/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrpolicy_workload_single_1x.h"
#include "dbgprintf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Functions  ------------------------------ */
static BoardObjVirtualTableGetInterfaceFromBoardObj (s_pwrPolicyGetInterfaceFromBoardObj_WORKLOAD_SINGLE_1X)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyPerfCfPwrModel", "s_pwrPolicyGetInterfaceFromBoardObj_WORKLOAD_SINGLE_1X");

static FLCN_STATUS s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X(PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1x, LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_SINGLE_1X *pObs)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyPerfCfPwrModel", "s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X");

static FLCN_STATUS s_pwrPolicyComputeWorkload_WORKLOAD_SINGLE_1X(PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1x, LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_SINGLE_1X *pObs)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyPerfCfPwrModel", "s_pwrPolicyComputeWorkload_WORKLOAD_SINGLE_1X");

static FLCN_STATUS s_pwrPolicyGetEstMetricsAtFreq_WORKLOAD_SINGLE_1X(PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1x, LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_SINGLE_1X *pObs, LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_ESTIMATED_METRICS_WORKLOAD_SINGLE_1X *pEst)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyPerfCfPwrModelScale", "s_pwrPolicyGetEstMetricsAtFreq_WORKLOAD_SINGLE_1X");

static FLCN_STATUS s_pwrPolicyGetVoltageFloor_WORKLOAD_SINGLE_1X(PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1x, LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_SINGLE_1X *pObs)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyPerfCfPwrModel", "s_pwrPolicyGetVoltageFloor_WORKLOAD_SINGLE_1X");

static void s_pwrPolicyLpwrResidenciesCopyIn_WORKLOAD_SINGLE_1X(PMGR_LPWR_RESIDENCIES *pPgRes, LwUFXP20_12 *pLpwrResidency, LwU8 size)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyPerfCfPwrModel", "s_pwrPolicyLpwrResidenciesCopyIn_WORKLOAD_SINGLE_1X");

static FLCN_STATUS s_pwrPolicyUpdateVoltageFloor_WORKLOAD_SINGLE_1X(PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1x, LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_WORKLOAD_SINGLE_1X_INDEPENDENT_DOMAINS_VMIN *pIndependentDomainsVmin, PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT *pMetricsInput)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyPerfCfPwrModelScale", "s_pwrPolicyUpdateVoltageFloor_WORKLOAD_SINGLE_1X");

static FLCN_STATUS s_pwrPolicySoftFloorFreqGet_WORKLOAD_SINGLE_1X(PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1x, PERF_CF_CONTROLLERS_REDUCED_STATUS *pPerfCfControllerStatus, LwU32 *pFreqMHz)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyPerfCfPwrModel", "s_pwrPolicySoftFloorFreqGet_WORKLOAD_SINGLE_1X");

/* ------------------------- Global Variables ------------------------------- */
/*!
 * Main structure for all PWR_POLICY_WORKLOAD_SINGLE_1X VIRTUAL_TABLE data.
 */
BOARDOBJ_VIRTUAL_TABLE pwrPolicyWorkloadSingle1xVirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_GET_INTERFACE_FROM_BOARDOBJ(
        s_pwrPolicyGetInterfaceFromBoardObj_WORKLOAD_SINGLE_1X)
};

/*!
 * Main structure for all PWR_POLICY_WORKLOAD_SINGLE_1X VIRTUAL_TABLE data.
 */
INTERFACE_VIRTUAL_TABLE pwrPolicyVirtualTable_WORKLOAD_SINGLE_1X =
{
    LW_OFFSETOF(PWR_POLICY_WORKLOAD_SINGLE_1X, pwrModel)   // offset
};

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Construct a _WORKLOAD_SINGLE_1X PWR_POLICY object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_SINGLE_1X
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PMGR_PWR_POLICY_WORKLOAD_SINGLE_1X_BOARDOBJ_SET  *pSingle1xSet =
        (RM_PMU_PMGR_PWR_POLICY_WORKLOAD_SINGLE_1X_BOARDOBJ_SET *)pBoardObjDesc;
    PWR_POLICY_WORKLOAD_SINGLE_1X                           *pSingle1x;
    BOARDOBJ_VTABLE                                         *pBoardObjVtable;
    FLCN_STATUS status;

    // Construct and initialize parent-class object.
    PMU_ASSERT_OK_OR_GOTO(status,
        pwrPolicyGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc),
        pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_SINGLE_1X_exit);

    pSingle1x       = (PWR_POLICY_WORKLOAD_SINGLE_1X *)*ppBoardObj;
    pBoardObjVtable = &pSingle1x->super.super;

    // Construct PWR_POLICY_PERF_CF_PWR_MODEL interface class
    PMU_ASSERT_OK_OR_GOTO(status,
        pwrPolicyConstructImpl_PWR_POLICY_PERF_CF_PWR_MODEL(pBObjGrp,
                &pSingle1x->pwrModel.super,
                &pSingle1xSet->pwrModel.super,
                &pwrPolicyVirtualTable_WORKLOAD_SINGLE_1X),
        pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_SINGLE_1X_exit);

    // Override the Vtable pointer.
    pBoardObjVtable->pVirtualTable = &pwrPolicyWorkloadSingle1xVirtualTable;

    pSingle1x->voltRailIdx        = pSingle1xSet->voltRailIdx;
    pSingle1x->voltMode           = pSingle1xSet->voltMode;
    pSingle1x->clkDomainIdx       = pSingle1xSet->clkDomainIdx;
    pSingle1x->bClkGatingAware    = pSingle1xSet->bClkGatingAware;
    pSingle1x->bDummy             = pSingle1xSet->bDummy;
    pSingle1x->ignoreClkDomainIdx = pSingle1xSet->ignoreClkDomainIdx;
    pSingle1x->softFloor.bSoftFloor             =
        pSingle1xSet->softFloor.bSoftFloor;
    pSingle1x->softFloor.clkPropTopIdx          =
        pSingle1xSet->softFloor.clkPropTopIdx;
    pSingle1x->softFloor.perfCfControllerClkIdx =
        pSingle1xSet->softFloor.perfCfControllerClkIdx;
    pSingle1x->softFloor.perfCfControllerIdx    =
        pSingle1xSet->softFloor.perfCfControllerIdx;

    if (pwrPolicySingle1xIsSoftFloorEnabled(pSingle1x) &&
       (pSingle1x->softFloor.clkPropTopIdx == LW2080_CTRL_CLK_CLK_PROP_TOP_IDX_ILWALID))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_STATE;
        goto pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_SINGLE_1X_exit;
    }

    // Allocate memory for the CLK_ADC_ACC_SAMPLEs only if not allocated
    if (pSingle1x->voltData.pClkAdcAccSample == NULL)
    {
        voltRailSensedVoltageDataConstruct(&pSingle1x->voltData,
            VOLT_RAIL_GET(pSingle1x->voltRailIdx),
            pSingle1xSet->voltMode, pBObjGrp->ovlIdxDmem);
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pSingle1x->voltData.pClkAdcAccSample != NULL),
            FLCN_ERR_NO_FREE_MEM,
            pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_SINGLE_1X_exit);
    }

pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_SINGLE_1X_exit:
    return status;
}

/*!
 * Load _WORKLOAD_SINGLE_1X PWR_POLICY object.
 *
 * @copydoc PwrPolicyLoad
 */
FLCN_STATUS
pwrPolicyLoad_WORKLOAD_SINGLE_1X
(
    PWR_POLICY *pPolicy,
    LwU32       ticksNow
)
{
    PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1x =
        (PWR_POLICY_WORKLOAD_SINGLE_1X *)pPolicy;
    FLCN_STATUS                    status;

    // Call super class implementation
    PMU_ASSERT_OK_OR_GOTO(status,
        pwrPolicyLoad_SUPER(pPolicy, ticksNow),
        pwrPolicyLoad_WORKLOAD_SINGLE_1X_exit);

    (void)pSingle1x;

pwrPolicyLoad_WORKLOAD_SINGLE_1X_exit:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
pwrPolicyIfaceModel10GetStatus_WORKLOAD_SINGLE_1X
(
    BOARDOBJ_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJ *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PMGR_PWR_POLICY_WORKLOAD_SINGLE_1X_BOARDOBJ_GET_STATUS *pGetStatus;
    PWR_POLICY_WORKLOAD_SINGLE_1X      *pSingle1x =
        (PWR_POLICY_WORKLOAD_SINGLE_1X *)pBoardObj;

    (void)pGetStatus;
    (void)pSingle1x;

    return FLCN_OK;
}

/*!
 * @copydoc PwrPolicy3XChannelMaskGet
 */
LwU32
pwrPolicy3XChannelMaskGet_WORKLOAD_SINGLE_1X
(
    PWR_POLICY *pPolicy
)
{
    // Update the channelMask with super class channel.
    LwU32 channelMask = LWBIT32(pPolicy->chIdx);

    return channelMask;
}

/*!
 * @copydoc PwrPolicyPerfCfPwrModelObserveMetrics
 */
FLCN_STATUS
pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_SINGLE_1X
(
    PWR_POLICY_PERF_CF_PWR_MODEL               *pPwrModel,
    PERF_CF_PWR_MODEL_SAMPLE_DATA              *pSampleData,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS *pObsrvdMetrics
)
{
    PWR_POLICY                    *pPolicy =
        (PWR_POLICY *)INTERFACE_TO_BOARDOBJ_CAST(pPwrModel);
    PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1x =
        (PWR_POLICY_WORKLOAD_SINGLE_1X *)pPolicy;
    LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_SINGLE_1X *pObserved =
        (LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_SINGLE_1X *)pObsrvdMetrics;
    FLCN_STATUS                    status;
    LW2080_CTRL_PMGR_PWR_TUPLE    *pTuple = NULL;

    if (pwrPolicySingle1xIsSoftFloorEnabled(pSingle1x) &&
       (pSampleData->pPerfCfControllersStatus != NULL))
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrPolicySoftFloorFreqGet_WORKLOAD_SINGLE_1X(pSingle1x,
            pSampleData->pPerfCfControllersStatus,
            &pObserved->freqSoftFloorMHz),
            pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_SINGLE_1X_exit);
    }

    if (pSampleData->pPwrChannelsStatus == NULL)
    {
        pObserved->observedVal = 0;
    }
    else
    {
        pTuple = pwrChannelsStatusChannelTupleGet(pSampleData->pPwrChannelsStatus,
                    pPolicy->chIdx);
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pTuple != NULL),
            FLCN_ERR_ILWALID_INDEX,
            pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_SINGLE_1X_exit);

        if (pwrPolicyLimitUnitGet(pPolicy) ==
            LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_POWER_MW)
        {
            pObserved->observedVal = pTuple->pwrmW;
        }
        else if (pwrPolicyLimitUnitGet(pPolicy) ==
                 LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_LWRRENT_MA)
        {
            pObserved->observedVal = pTuple->lwrrmA;
        }
        else
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                FLCN_ERR_NOT_SUPPORTED,
                pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_SINGLE_1X_exit);
        }
    }
    //
    // Read current state of the system.  Bail on unexpected errors if PERF
    // subsystem not initialized.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X(pSingle1x, pObserved),
        pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_SINGLE_1X_exit);


    // Get the voltageFlooruV
    PMU_ASSERT_OK_OR_GOTO(status,
        s_pwrPolicyGetVoltageFloor_WORKLOAD_SINGLE_1X(pSingle1x, pObserved),
        pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_SINGLE_1X_exit);

    // Ignore workload computations for dummy policy
    if (pwrPolicyWorkloadSingle1xIsDummy(pSingle1x))
    {
        pObserved->observedVal = 0;
    }
    else
    {
        // Callwlate the workload from the current power.
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrPolicyComputeWorkload_WORKLOAD_SINGLE_1X(pSingle1x, pObserved),
            pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_SINGLE_1X_exit);
    }

pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_SINGLE_1X_exit:
    return status;
}

/*!
 * @copydoc     PwrPolicyPerfCfPwrModelScaleMetrics
 *
 * @deprecated  Use pwrPolicyPerfCfPwrModelScale_WORKLOAD_SINGLE_1X going
 *              forward, it better conforms to the PERF_CF_PWR_MODEL interface
 *              of the same name.
 *
 * @todo        Move this function body into
 *              pwrPolicyPerfCfPwrModelScale_WORKLOAD_SINGLE_1X once all
 *              references are removed. Could simply be made static also.
 */
FLCN_STATUS
pwrPolicyPerfCfPwrModelScaleMetrics_WORKLOAD_SINGLE_1X
(
    PWR_POLICY_PERF_CF_PWR_MODEL               *pPwrModel,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS *pObservedMetrics,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS *pEstimatedMetrics
)
{
    PWR_POLICY                      *pPolicy =
        (PWR_POLICY *)INTERFACE_TO_BOARDOBJ_CAST(pPwrModel);
    PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1x =
        (PWR_POLICY_WORKLOAD_SINGLE_1X *)pPolicy;
    LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_SINGLE_1X *pObs  =
        (LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_SINGLE_1X *)pObservedMetrics;
    LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_ESTIMATED_METRICS_WORKLOAD_SINGLE_1X *pEst =
        (LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_ESTIMATED_METRICS_WORKLOAD_SINGLE_1X *)pEstimatedMetrics;
    FLCN_STATUS                       status = FLCN_OK;

    //
    // Populate workload factor
    // TO-DO: Implement a median filter to store desired workload term.
    //
    pEst->workload = pObs->workload;

    //
    // Copy in the lpwr residency from the observed metrics
    // structure for now.
    // TO DO - Come up with a scaled low power residency estimation model
    //
    memcpy(&(pEst->lpwrResidency), &(pObs->lpwrResidency),
        sizeof(pObs->lpwrResidency));

    // Get the EstimatedMetrics
    PMU_ASSERT_OK_OR_GOTO(status,
        s_pwrPolicyGetEstMetricsAtFreq_WORKLOAD_SINGLE_1X(pSingle1x,
            pObs, pEst),
        pwrPolicyPerfCfPwrModelScaleMetrics_WORKLOAD_SINGLE_1X_exit);

pwrPolicyPerfCfPwrModelScaleMetrics_WORKLOAD_SINGLE_1X_exit:
    return status;
}

/*!
 * @copydoc PwrPolicyPerfCfPwrModelScale
 *
 * @note    Prior to calling, the observed metrics must be populated with
 *          appropriate ::workload, ::lpwrResidency, and ::voltFlooruV
 */
FLCN_STATUS
pwrPolicyPerfCfPwrModelScale_WORKLOAD_SINGLE_1X
(
    PWR_POLICY_PERF_CF_PWR_MODEL                   *pPwrModel,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS     *pObservedMetrics,
    LwLength                                        numMetricsInputs,
    PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT          *pMetricsInputs,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS    **ppEstimatedMetrics,
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS            *pTypeParams
)
{
    PWR_POLICY_WORKLOAD_SINGLE_1X      *pSingle1x =
        (PWR_POLICY_WORKLOAD_SINGLE_1X *)INTERFACE_TO_BOARDOBJ_CAST(pPwrModel);
    LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_SINGLE_1X
                                       *pObserved  =
        (LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_SINGLE_1X *)pObservedMetrics;
    LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_ESTIMATED_METRICS_WORKLOAD_SINGLE_1X
                                       *pEstimated;
    LwLength                            loopIdx;
    FLCN_STATUS                         status   = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pPwrModel          == NULL) ||
         (pObservedMetrics   == NULL) ||
         (pMetricsInputs     == NULL) ||
         (ppEstimatedMetrics == NULL) ||
         (pTypeParams == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        pwrPolicyPerfCfPwrModelScale_WORKLOAD_SINGLE_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        (numMetricsInputs == 0U) ||
        (numMetricsInputs > LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_ESTIMATED_METRICS_MAX) ?
            FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        pwrPolicyPerfCfPwrModelScale_WORKLOAD_SINGLE_1X_exit);

    //
    // Ensure:
    //  1.) metrics type is correct
    //  2.) at least the single's clock domain index is specified
    //
    for (loopIdx = 0; loopIdx < numMetricsInputs; loopIdx++)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            ppEstimatedMetrics[loopIdx]->type == LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_WORKLOAD_SINGLE_1X ?
                FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
            pwrPolicyPerfCfPwrModelScale_WORKLOAD_SINGLE_1X_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfPwrModelScaleMeticsInputCheckIndex(&pMetricsInputs[loopIdx],
                pSingle1x->clkDomainIdx) ? FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
            pwrPolicyPerfCfPwrModelScale_WORKLOAD_SINGLE_1X_exit);
    }

    // Populate the estimated metrics
    for (loopIdx = 0; loopIdx < numMetricsInputs; loopIdx++)
    {
        LwU32       freqkHz = 0;
        CLK_DOMAIN *pDomain = CLK_DOMAIN_GET(pSingle1x->clkDomainIdx);

        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pDomain != NULL),
            FLCN_ERR_ILWALID_STATE,
            pwrPolicyPerfCfPwrModelScale_WORKLOAD_SINGLE_1X_exit);

        pEstimated = (LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_ESTIMATED_METRICS_WORKLOAD_SINGLE_1X *)ppEstimatedMetrics[loopIdx];

        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfPwrModelScaleMeticsInputGetFreqkHz(
                &pMetricsInputs[loopIdx],
                pSingle1x->clkDomainIdx,
                &freqkHz),
            pwrPolicyPerfCfPwrModelScale_WORKLOAD_SINGLE_1X_exit);

        // Pass in the metric inputs
        pEstimated->freqMHz = freqkHz / clkDomainFreqkHzScaleGet(pDomain);

        // Copy in vmin information
        pEstimated->independentDomainsVmin = pObserved->independentDomainsVmin;

        // Re-compute voltage floor based on metrics input
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrPolicyUpdateVoltageFloor_WORKLOAD_SINGLE_1X(
                pSingle1x,
                &pEstimated->independentDomainsVmin,
                &pMetricsInputs[loopIdx]),
            pwrPolicyPerfCfPwrModelScale_WORKLOAD_SINGLE_1X_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
            pwrPolicyPerfCfPwrModelScaleMetrics_WORKLOAD_SINGLE_1X(
                pPwrModel,
                pObservedMetrics,
                &pEstimated->super),
            pwrPolicyPerfCfPwrModelScale_WORKLOAD_SINGLE_1X_exit);
    }

pwrPolicyPerfCfPwrModelScale_WORKLOAD_SINGLE_1X_exit:
    return status;
}

/*!
 * Initialises the necessary metrics
 *
 * @param[in]  pSingle1x   PWR_POLICY_WORKLOAD_SINGLE_1X pointer.
 * @param[out] pMetrics
 *     LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS *pointer
 *
 * @return FLCN_OK
 *     All state successfully retrieved.
 * @return Others
 *     Invalid GPU state returned.
 */
FLCN_STATUS
pwrPolicyMetricsInit_WORKLOAD_SINGLE_1X
(
    PWR_POLICY_WORKLOAD_SINGLE_1X              *pSingle1x,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS *pMetrics
)
{
    LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_SINGLE_1X *pObs  =
        (LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_SINGLE_1X *)pMetrics;
    FLCN_STATUS   status = FLCN_OK;

    //
    // The state of sensed voltage is stored in the PMU in @ref VOLT_RAIL_SENSED_VOLTAGE_DATA.
    // To query the voltage, we need to send it as an input. Hence copy this state into
    // the @ref LW2080_CTRL_VOLT_RAIL_SENSED_VOLTAGE_DATA structure in the
    // @ref LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_SINGLE_1X
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        voltRailSensedVoltageDataCopyOut(&(pObs->sensed), &(pSingle1x->voltData)),
        pwrPolicyMetricsInit_WORKLOAD_SINGLE_1X_exit);

pwrPolicyMetricsInit_WORKLOAD_SINGLE_1X_exit:
    return status;
}

/*!
 * @brief      Copy in workload-specific parameters into observed metrics.
 *
 * @param      pSingle1x             CWCS1X pointer
 * @param      pObsMetrics           CWCS1X observed metrics
 * @param      pTgp1xWorkloadParams  Tgp1x workload parameters
 *
 * @return     FLCN_ERR_ILWALID_ARGUMENT in case of NULL arguments, FLCN_OK otherwise.
 */
FLCN_STATUS
pwrPolicyWorkloadSingle1xImportTgp1xWorkloadParameters
(
    PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1x,
    LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_SINGLE_1X *pObsMetrics,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TGP_1X_WORKLOAD_PARAMETERS *pTgp1xWorkloadParams
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pSingle1x != NULL) &&
         (pObsMetrics != NULL) &&
         (pTgp1xWorkloadParams != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrPolicyWorkloadSingle1xImportTgp1xWorkloadParameters_exit);

    // Pull workload parameter for single's volt rail
    pObsMetrics->workload = pTgp1xWorkloadParams->workload[pSingle1x->voltRailIdx];

    // No low power residency, this could be added to TGP_1X workload parameters in the future
    memset(pObsMetrics->lpwrResidency, 0, sizeof(pObsMetrics->lpwrResidency));

pwrPolicyWorkloadSingle1xImportTgp1xWorkloadParameters_exit:
    return status;
}

/*!
 * @copydoc PwrPolicy35PerfCfControllerMaskGet
 */
FLCN_STATUS
pwrPolicy35PerfCfControllerMaskGet_WORKLOAD_SINGLE_1X
(
    PWR_POLICY          *pPolicy,
    BOARDOBJGRPMASK_E32 *pPerfCfControllerMask
)
{
    PWR_POLICY_WORKLOAD_SINGLE_1X      *pSingle1x =
        (PWR_POLICY_WORKLOAD_SINGLE_1X *)pPolicy;
    FLCN_STATUS status = FLCN_OK;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pPerfCfControllerMask != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrPolicy35PerfCfControllerMaskGet_WORKLOAD_SINGLE_1X_exit);
 
    if (pwrPolicySingle1xIsSoftFloorEnabled(pSingle1x) &&
       (pSingle1x->softFloor.perfCfControllerIdx !=
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_INDEX_ILWALID))
    {
        boardObjGrpMaskBitSet(pPerfCfControllerMask,
            pSingle1x->softFloor.perfCfControllerIdx);
    }
pwrPolicy35PerfCfControllerMaskGet_WORKLOAD_SINGLE_1X_exit:
   return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @copydoc BoardObjVirtualTableGetInterfaceFromBoardObj
 */
static BOARDOBJ_INTERFACE *
s_pwrPolicyGetInterfaceFromBoardObj_WORKLOAD_SINGLE_1X
(
    BOARDOBJ   *pBoardObj,
    LwU8        interfaceType
)
{
    PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1x =
        (PWR_POLICY_WORKLOAD_SINGLE_1X *)pBoardObj;

    switch (interfaceType)
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_INTERFACE_TYPE_PERF_CF_PWR_MODEL:
        {
            return &pSingle1x->pwrModel.super;
        }
    }

    PMU_TRUE_BP();
    return NULL;
}

/*!
 * Retrieves the current GPU state (clocks, voltage, leakage power) which
 * will be used to callwlate the current workload/active capacitance term (w).
 *
 * @param[in]  pSingle1x   PWR_POLICY_WORKLOAD_SINGLE_1X pointer.
 * @param[out] pObserved
 *     LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_SINGLE_1X *pointer to
 *     populate the with the current GPU state.
 *
 * @return FLCN_OK
 *     All state successfully retrieved.
 * @return Others
 *     Invalid GPU state returned.
 */
static FLCN_STATUS
s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X
(
    PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1x,
    LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_SINGLE_1X *pObserved
)
{
    VOLT_RAIL  *pRail;
    FLCN_STATUS status = FLCN_OK;
    RM_PMU_PMGR_PWR_POLICY_DOMGRP_MAP_ENTRY domData;
    CLK_PROP_REGIME                        *pPropRegime;
    CLK_DOMAIN                             *pDomain;

    // Get the voltage rail
    pRail = VOLT_RAIL_GET(pSingle1x->voltRailIdx);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pRail != NULL),
        FLCN_ERR_ILWALID_INDEX,
        s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X_exit);

    boardObjGrpMaskInit_E32(&pSingle1x->independentClkDomainMask);
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROP_REGIME))
    {
        // Get the domain group data
        PMU_ASSERT_OK_OR_GOTO(status,
            pwrPolicies35DomGrpDataGet(PWR_POLICIES_35_GET(), &domData,
                pSingle1x->clkDomainIdx),
            s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X_exit);

        pPropRegime = clkPropRegimesGetByRegimeId(domData.regimeId);
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pPropRegime != NULL),
            FLCN_ERR_ILWALID_STATE,
            s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X_exit);
        PMU_ASSERT_OK_OR_GOTO(status,
            boardObjGrpMaskCopy(&pSingle1x->independentClkDomainMask,
                clkPropRegimeClkDomainMaskGet(pPropRegime)),
            s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X_exit);

        if (pSingle1x->ignoreClkDomainIdx !=
            LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                pwrPolicies35DomGrpDataGet(PWR_POLICIES_35_GET(), &domData,
                     pSingle1x->ignoreClkDomainIdx),
                s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X_exit);

            pPropRegime = clkPropRegimesGetByRegimeId(domData.regimeId);
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pPropRegime != NULL),
                FLCN_ERR_ILWALID_STATE,
                s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X_exit);
            PMU_ASSERT_OK_OR_GOTO(status,
                boardObjGrpMaskOr(&pSingle1x->independentClkDomainMask, &pSingle1x->independentClkDomainMask,
                    clkPropRegimeClkDomainMaskGet(pPropRegime)),
                s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X_exit);
        }
    }
    else
    {
        LwU32 clkDomainIdx = 0;

        PMU_ASSERT_OK_OR_GOTO(status,
            clkDomainsGetIndexByApiDomain(LW2080_CTRL_CLK_DOMAIN_GPCCLK,
                &clkDomainIdx),
            s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X_exit);
        boardObjGrpMaskBitSet(&pSingle1x->independentClkDomainMask, clkDomainIdx);

        PMU_ASSERT_OK_OR_GOTO(status,
            clkDomainsGetIndexByApiDomain(LW2080_CTRL_CLK_DOMAIN_XBARCLK,
                &clkDomainIdx),
            s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X_exit);
        boardObjGrpMaskBitSet(&pSingle1x->independentClkDomainMask, clkDomainIdx);

        PMU_ASSERT_OK_OR_GOTO(status,
            clkDomainsGetIndexByApiDomain(LW2080_CTRL_CLK_DOMAIN_SYSCLK,
                &clkDomainIdx),
            s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X_exit);
        boardObjGrpMaskBitSet(&pSingle1x->independentClkDomainMask, clkDomainIdx);

        PMU_ASSERT_OK_OR_GOTO(status,
            clkDomainsGetIndexByApiDomain(LW2080_CTRL_CLK_DOMAIN_LWDCLK,
                &clkDomainIdx),
            s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X_exit);
        boardObjGrpMaskBitSet(&pSingle1x->independentClkDomainMask, clkDomainIdx);

        PMU_ASSERT_OK_OR_GOTO(status,
            clkDomainsGetIndexByApiDomain(LW2080_CTRL_CLK_DOMAIN_HOSTCLK,
                &clkDomainIdx),
            s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X_exit);
        boardObjGrpMaskBitSet(&pSingle1x->independentClkDomainMask, clkDomainIdx);
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskIlw(&(pSingle1x->independentClkDomainMask)),
        s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskAnd(&pSingle1x->independentClkDomainMask,
                 voltRailGetClkDomainsProgMask(pRail),
                 &pSingle1x->independentClkDomainMask),
        s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X_exit);

    // Get frequency in MHz
    pDomain = CLK_DOMAIN_GET(pSingle1x->clkDomainIdx);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDomain != NULL),
        FLCN_ERR_ILWALID_STATE,
        s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X_exit);

    pSingle1x->clkCntrStart = pObserved->clkCntrStart;

    pObserved->freqMHz = pmgrFreqMHzGet(clkDomainApiDomainGet(pDomain),
                             &pSingle1x->clkCntrStart);

    pObserved->clkCntrStart = pSingle1x->clkCntrStart;

    //
    // Copy in VOLT_RAIL_SENSED_VOLTAGE_DATA from
    // LW2080_CTRL_VOLT_RAIL_SENSED_VOLTAGE_DATA.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        voltRailSensedVoltageDataCopyIn(&(pSingle1x->voltData), &(pObserved->sensed)),
        s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X_exit);

    // Get the sensed voltage
    PMU_ASSERT_OK_OR_GOTO(status,
        voltRailGetVoltageSensed(pRail, &pSingle1x->voltData),
        s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X_exit);

    //
    // Copy out from VOLT_RAIL_SENSED_VOLTAGE_DATA to
    // LW2080_CTRL_VOLT_RAIL_SENSED_VOLTAGE_DATA
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        voltRailSensedVoltageDataCopyOut(&(pObserved->sensed), &(pSingle1x->voltData)),
        s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X_exit);

    // Get voltage in mV.
    pObserved->voltmV = PWR_POLICY_WORKLOAD_UV_TO_MV(pSingle1x->voltData.voltageuV);

    if (pwrPolicyWorkloadSingle1xIsDummy(pSingle1x))
    {
        pObserved->leakagemX = 0;
    }
    else
    {
        PMGR_LPWR_RESIDENCIES pgRes;
        s_pwrPolicyLpwrResidenciesCopyIn_WORKLOAD_SINGLE_1X(&pgRes,
            pObserved->lpwrResidency, LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MAX);

        // Callwlate leakage power for given voltage.
        PMU_ASSERT_OK_OR_GOTO(status,
            voltRailGetLeakage(pRail, pSingle1x->voltData.voltageuV,
                pwrPolicyLimitUnitGet(&(pSingle1x->super)),
                &pgRes,
                &(pObserved->leakagemX)),
            s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X_exit);
    }

s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X_exit:
    return status;
}

/*!
 * Computes the current workload/active capacitance term (w) given the observed
 * power and GPU state.
 *
 * @param[in] pSingle1x   PWR_POLICY_WORKLOAD_SINGLE_1X pointer.
 * @param[in/out] pObserved
 *     LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_SINGLE_1X
 *     pointer to containing the current GPU state.
 *
 * @return FLCN_OK workload computed successfully
 *         Others  Unexpected errors propogated from other functions
 */
static FLCN_STATUS
s_pwrPolicyComputeWorkload_WORKLOAD_SINGLE_1X
(
    PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1x,
    LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_SINGLE_1X *pObserved
)
{
    FLCN_STATUS           status = FLCN_OK;
    VOLT_RAIL            *pRail;
    LwU64                 factor = 1000;
    LwUFXP40_24           workloadFXP40_24;
    LwUFXP52_12           denomFXP52_12;
    LwUFXP52_12           quotientFXP52_12;
    PWR_EQUATION_DYNAMIC *pDyn;
    LwU8                  dynamicEquIdx =
                              LW2080_CTRL_PMGR_PWR_EQUATION_INDEX_ILWALID;

    // Fetch the dynamic equation index
    pRail = VOLT_RAIL_GET(pSingle1x->voltRailIdx);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pRail != NULL),
        FLCN_ERR_ILWALID_INDEX,
        s_pwrPolicyComputeWorkload_WORKLOAD_SINGLE_1X_exit);
    dynamicEquIdx = voltRailVoltScaleExpPwrEquIdxGet(pRail);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (dynamicEquIdx != LW2080_CTRL_PMGR_PWR_EQUATION_INDEX_ILWALID),
        FLCN_ERR_NOT_SUPPORTED,
        s_pwrPolicyComputeWorkload_WORKLOAD_SINGLE_1X_exit);

    //
    // Assume 0 workload for the following tricky numerical cases (i.e. avoiding
    // negative numbers or dividing by zero):
    // 1. Power is less than leakage callwlation.
    // 2. Measured frequency is zero.
    // 3. Measured voltage is zero (eg - with GPC-RG)
    //
    if ((pObserved->observedVal < pObserved->leakagemX) ||
        (pObserved->freqMHz == 0) ||
        (pObserved->voltmV == 0))
    {
        pObserved->workload = 0;
        goto s_pwrPolicyComputeWorkload_WORKLOAD_SINGLE_1X_exit;
    }

    pDyn =  (PWR_EQUATION_DYNAMIC *)PWR_EQUATION_GET(dynamicEquIdx);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDyn != NULL),
        FLCN_ERR_ILWALID_STATE,
        s_pwrPolicyComputeWorkload_WORKLOAD_SINGLE_1X_exit);

    workloadFXP40_24 = LW_TYPES_U64_TO_UFXP_X_Y_SCALED(40, 24,
        (pObserved->observedVal - pObserved->leakagemX),
        pObserved->freqMHz);

    if (pwrPolicyLimitUnitGet(&(pSingle1x->super)) ==
        LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_POWER_MW)
    {
        //
        //     w   = ((Ptotal - Pleakage) / f  ) / V^(dynamicExp)
        // Units:
        //         = (         mW         / MHz) / V^(dynamicExp)
        //         = (         mW         / MHz) / mV^(dynamicExp) * 1000
        // Numerical Analysis:
        //   52.12 = (        40.24            ) / 52.12
        //
        // Integer overflow:
        // Assuming Vmin = 600mV and dynamicExp = 2.4, for 20 integer bits
        // to overflow, the numerator in above callwlation should be greater
        // than 0.6^2.4 * 1000 * 2^20 = 307725000. This value of numerator
        // is not expected but is not impossible, so we should detect for
        // integer overflow.
        //
        // Fractional underflow:
        // Assuming Vmax = 1.2V and dynamicExp = 2.4, for 12 fractional bits
        // to underflow, numerator has to be less than 1.2^2.4 * 1000 * 2^-12
        // = 0.378. This means the numerator has to be 1 for underflow.
        // We can assume workload to be zero for such low dynamic power.
        //
        denomFXP52_12 = (LwUFXP52_12)(pwrEquationDynamicScaleVoltagePower(pDyn,
            LW_TYPES_U32_TO_UFXP_X_Y_SCALED(20, 12,
            pObserved->voltmV, 1000)));
        lwU64Mul(&denomFXP52_12, &denomFXP52_12, &factor);
    }
    else
    {
        //
        //     w   = ((Itotal - Ileakage) / f  ) / V^(dynamicExp)
        // Units:
        //         = (         mA         / MHz) / V^(dynamicExp)
        //         = (         mA         / MHz) / mV^(dynamicExp) * 1000
        // Numerical Analysis:
        //   52.12 = (        40.24            ) / 52.12
        //
        // Overflow callwlation similar to power case above
        //
        denomFXP52_12 = (LwUFXP52_12)(pwrEquationDynamicScaleVoltageLwrrent(pDyn,
           LW_TYPES_U32_TO_UFXP_X_Y_SCALED(20, 12,
                pObserved->voltmV, 1000)));
        lwU64Mul(&denomFXP52_12, &denomFXP52_12, &factor);
    }

    // Check for division by zero and return 0 workload as this is undefined.
    if (denomFXP52_12 == LW_TYPES_FXP_ZERO)
    {
        pObserved->workload = 0;
        goto s_pwrPolicyComputeWorkload_WORKLOAD_SINGLE_1X_exit;
    }
    lwU64DivRounded(&quotientFXP52_12, &workloadFXP40_24, &denomFXP52_12);
    if (LwU64_HI32(quotientFXP52_12) != LW_TYPES_FXP_ZERO)
    {
        // Overflow detected, cap workload to LW_U32_MAX
        pObserved->workload = LW_U32_MAX;
    }
    else
    {
        pObserved->workload = LwU64_LO32(quotientFXP52_12);
    }

s_pwrPolicyComputeWorkload_WORKLOAD_SINGLE_1X_exit:
    return status;
}

/*!
 * Retrieves the voltage floor limit for the given voltage rail based on the
 * lastest perf status received from RM.
 *
 * @param[in]     pSingle1x   PWR_POLICY_WORKLOAD_SINGLE_1X pointer.
 * @param[in]     mclkkHz     memory clock in kHz.
 * @param[out]    voltFlooruV Callwlated voltage floor
 *
 * @return FLCN_OK
 *     All state successfully retrieved.
 * @return Others
 *     Invalid GPU state returned.
 *
 * @note:
 * If mclkkHz is zero, latest perf status received from RM is used
 *     to retrieve the voltage floor limit.
 * Else mclkkHz value is passed as input to the arbiter to retrieve the
 *     voltage floor limit.
 */
static FLCN_STATUS
s_pwrPolicyGetVoltageFloor_WORKLOAD_SINGLE_1X
(
    PWR_POLICY_WORKLOAD_SINGLE_1X  *pSingle1x,
    LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_SINGLE_1X *pObs
)
{
    VOLT_RAIL                  *pRail;
    CLK_DOMAIN                 *pDomain = NULL;
    PERF_LIMITS_VF              vfRail;
    LwBoardObjIdx               clkIdx;
    LwU32                       railVminuV;
    FLCN_STATUS                 status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pSingle1x != NULL) &&
         (pObs != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_pwrPolicyGetVoltageFloor_WORKLOAD_SINGLE_1X_exit);

    //
    // Initialize the voltFlooruV to minimum.
    // Directly query the VOLTAGE_RAIL Vmin, which is separate
    // from any CLK_DOMAIN Vmin (i.e. a loose VOLTAGE min which
    // won't re-target the frequencies)
    //
    pRail  = VOLT_RAIL_GET(pSingle1x->voltRailIdx);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pRail != NULL),
        FLCN_ERR_ILWALID_INDEX,
        s_pwrPolicyGetVoltageFloor_WORKLOAD_SINGLE_1X_exit);

    status = voltRailGetVoltageMin(pRail, &railVminuV);
    //
    // FLCN_ERR_NOT_SUPPORTED is an expected error - i.e. no
    // VOLTAGE_RAIL Vmin specified for this GPU.
    //
    if (status == FLCN_ERR_NOT_SUPPORTED)
    {
        status = FLCN_OK;
        pObs->independentDomainsVmin.railVminuV = 0;
    }
    // Unexpected errors
    else if (status != FLCN_OK)
    {
        PMU_TRUE_BP();
        goto s_pwrPolicyGetVoltageFloor_WORKLOAD_SINGLE_1X_exit;
    }
    // Include the VOLTAGE_RAIL Vmin in the voltage floor.
    else
    {
        pObs->independentDomainsVmin.railVminuV = railVminuV;
    }

    pObs->independentDomainsVmin.voltFlooruV = pObs->independentDomainsVmin.railVminuV;

    vfRail.idx = BOARDOBJ_GRP_IDX_TO_8BIT(pSingle1x->voltRailIdx);

    memset(&pObs->independentDomainsVmin.domains, 0,
        sizeof(pObs->independentDomainsVmin.domains));

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskExport_E32(
            &pSingle1x->independentClkDomainMask,
            &pObs->independentDomainsVmin.independentClkDomainMask),
        s_pwrPolicyGetVoltageFloor_WORKLOAD_SINGLE_1X_exit);

    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, clkIdx,
        &(pSingle1x->independentClkDomainMask.super))
    {
        PERF_LIMITS_VF vfDomain;

        vfDomain.idx = BOARDOBJ_GRP_IDX_TO_8BIT(clkIdx);

        // Optimization to save IMEM (see overlay combinations Tasks.pm)
        {
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfLimitVf)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                // Get the voltage required to support given freq
                vfDomain.value = perfChangeSeqChangeLastClkFreqkHzGet(clkIdx);
                status = perfLimitsFreqkHzToVoltageuV(&vfDomain, &vfRail, LW_TRUE);
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

            PMU_ASSERT_OK_OR_GOTO(
                status,
                status,
                s_pwrPolicyGetVoltageFloor_WORKLOAD_SINGLE_1X_exit);
        }

        // Cache off the individual domain vmin and frequency
        pObs->independentDomainsVmin.domains[clkIdx].freqkHz = vfDomain.value;
        pObs->independentDomainsVmin.domains[clkIdx].voltuV  = vfRail.value;

        // Store the floor value in voltageFlooruV
        pObs->independentDomainsVmin.voltFlooruV = LW_MAX(pObs->independentDomainsVmin.voltFlooruV, vfRail.value);
    }
    BOARDOBJGRP_ITERATOR_END;

    // Callwlate Fmax @ indepenent domains Vmin
    {
        PERF_LIMITS_PSTATE_RANGE pstateRange;
        PERF_LIMITS_VF           vfDomain;
        PSTATES                 *pPstates = PSTATES_GET();
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfLimitVf)
        };

        pDomain = CLK_DOMAIN_GET(pSingle1x->clkDomainIdx);
        PMU_ASSERT_TRUE_OR_GOTO(status,
            ((pDomain != NULL) &&
             (pPstates != NULL)),
            FLCN_ERR_ILWALID_STATE,
            s_pwrPolicyGetVoltageFloor_WORKLOAD_SINGLE_1X_exit);

        vfDomain.idx = BOARDOBJ_GRP_IDX_TO_8BIT(pSingle1x->clkDomainIdx);
        vfRail.value = pObs->independentDomainsVmin.voltFlooruV;

        PERF_LIMITS_RANGE_SET_VALUE(&pstateRange,
            boardObjGrpMaskBitIdxHighest(&(pPstates->super.objMask)));

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = perfLimitsVoltageuVToFreqkHz(&vfRail, &pstateRange, &vfDomain, LW_TRUE, LW_TRUE);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

        PMU_ASSERT_OK_OR_GOTO(
            status,
            status,
            s_pwrPolicyGetVoltageFloor_WORKLOAD_SINGLE_1X_exit);

        pObs->freqFmaxVminMHz = vfDomain.value / clkDomainFreqkHzScaleGet(pDomain);
    }

s_pwrPolicyGetVoltageFloor_WORKLOAD_SINGLE_1X_exit:
    return status;
}

/*!
 * Evaluate the power, volt and leakagemX at the given frequency
 * based on the power equation:
 *
 *      Given frequency f:
 *          P = v ^ 2 * f * workload + P_leakage
 *
 *      Here,
 *      v = voltage value needed at rail to support f
 *
 * @param[in]   pSingle1x     PWR_POLICY_WORKLOAD_SINGLE_1X pointer.
 * @param[in]   pEst
 *   LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_ESTIMATED_METRICS_WORKLOAD_SINGLE_1X pointer
 *
 * @return FLCN_OK
 *     All state successfully retrieved.
 * @return Others
 *     Invalid GPU state returned.
 */
static FLCN_STATUS
s_pwrPolicyGetEstMetricsAtFreq_WORKLOAD_SINGLE_1X
(
    PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1x,
    LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_SINGLE_1X *pObs,
    LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_ESTIMATED_METRICS_WORKLOAD_SINGLE_1X *pEst
)
{
    VOLT_RAIL            *pRail;
    CLK_DOMAIN           *pDomain;
    PERF_LIMITS_VF        vfRail;
    PERF_LIMITS_VF        vfDomain;
    LwU32                 voltageuV;
    LwU8                  dynamicEquIdx;
    LwS32                 voltDeltauV;
    LwU64                 factor;
    LwU64                 estimatedValU64;
    LwUFXP52_12           tempFXP52_12;
    LwUFXP40_24           productFXP40_24;
    PWR_EQUATION_DYNAMIC *pDyn;
    LwUFXP52_12           workloadFXP52_12 = (LwUFXP52_12)pEst->workload;
    FLCN_STATUS           status = FLCN_OK;
    PMGR_LPWR_RESIDENCIES pgRes;

    // Initialize the required parameters.
    pDomain = CLK_DOMAIN_GET(pSingle1x->clkDomainIdx);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDomain != NULL),
        FLCN_ERR_ILWALID_STATE,
        s_pwrPolicyGetEstMetricsAtFreq_WORKLOAD_SINGLE_1X_exit);

    vfRail.idx     = BOARDOBJ_GRP_IDX_TO_8BIT(pSingle1x->voltRailIdx);
    vfDomain.idx   = BOARDOBJ_GRP_IDX_TO_8BIT(pSingle1x->clkDomainIdx);
    vfDomain.value = pEst->freqMHz * clkDomainFreqkHzScaleGet(pDomain);

    // Get the voltage required to support given freq
    PMU_ASSERT_OK_OR_GOTO(status,
        perfLimitsFreqkHzToVoltageuV(&vfDomain, &vfRail, LW_TRUE),
        s_pwrPolicyGetEstMetricsAtFreq_WORKLOAD_SINGLE_1X_exit);

    voltageuV = LW_MAX(pEst->independentDomainsVmin.voltFlooruV, vfRail.value);

    //
    // Update the voltage value with the voltage delta value
    // TO-DO - Replace this with volt API
    //
    voltDeltauV =
        perfChangeSeqChangeLastVoltOffsetuVGet(pSingle1x->voltRailIdx,
        LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_CLFC);

    voltageuV += voltDeltauV;
    pEst->voltmV = PWR_POLICY_WORKLOAD_UV_TO_MV(voltageuV);

    if (pwrPolicyWorkloadSingle1xIsDummy(pSingle1x))
    {
        pEst->leakagemX = 0;
        pEst->estimatedVal = 0;
        pEst->workload = 0;
        goto s_pwrPolicyGetEstMetricsAtFreq_WORKLOAD_SINGLE_1X_exit;
    }

    // Callwlate the new leakage using the new required voltage
    pRail  = VOLT_RAIL_GET(pSingle1x->voltRailIdx);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pRail != NULL),
        FLCN_ERR_ILWALID_INDEX,
        s_pwrPolicyGetEstMetricsAtFreq_WORKLOAD_SINGLE_1X_exit);

    s_pwrPolicyLpwrResidenciesCopyIn_WORKLOAD_SINGLE_1X(&pgRes,
            pObs->lpwrResidency, LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MAX);

    // Optimization to save IMEM (see overlay combinations Tasks.pm)
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_DEFINE_LIB_LW64_DETACH
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = voltRailGetLeakage(pRail, voltageuV,
                        pwrPolicyLimitUnitGet(pSingle1x),
                        &pgRes, /* PGres = 0.0 */
                        &(pEst->leakagemX));
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

        PMU_ASSERT_OK_OR_GOTO(
            status,
            status,
            s_pwrPolicyGetEstMetricsAtFreq_WORKLOAD_SINGLE_1X_exit);
    }

    // Fetch the dynamic equation index
    dynamicEquIdx = voltRailVoltScaleExpPwrEquIdxGet(pRail);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (dynamicEquIdx != LW2080_CTRL_PMGR_PWR_EQUATION_INDEX_ILWALID),
        FLCN_ERROR,
        s_pwrPolicyGetEstMetricsAtFreq_WORKLOAD_SINGLE_1X_exit);

    pDyn = (PWR_EQUATION_DYNAMIC *)PWR_EQUATION_GET(dynamicEquIdx);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDyn != NULL),
        FLCN_ERROR,
        s_pwrPolicyGetEstMetricsAtFreq_WORKLOAD_SINGLE_1X_exit);

    if (pSingle1x->bClkGatingAware)
    {
        //
        // Even when mscg residency is 1 (almost impossible), we'll be
        // fine because we should be assuming continued 100% residency
        // and thus the effective frequency (and dynamic power) will
        // fall to 0.
        //
        // TO-DO - Remove MSCG residency, and add clock residency instead
        //
        pEst->effectiveFreqMHz = LW_TYPES_UFXP_X_Y_TO_U32(20, 12,
            (pEst->freqMHz * (LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1) -
            pEst->lpwrResidency[LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS])));
    }
    else
    {
        pEst->effectiveFreqMHz = pEst->freqMHz;
    }

    if (pwrPolicyLimitUnitGet(pSingle1x) ==
        LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_POWER_MW)
    {
        //
        // Compute the power:
        // From previous callwlation of workload:
        //  w =          mW
        //      --------------------
        //      V^(dynamicExp) * MHz
        //
        // Pwr = V^(dynamicExp) * f * w + Pleakage
        // i.e.
        //  mV^(dynamicExp) * MHz *        ( mW )        + mW
        //                         --------------------       = mW
        //                         mV^(dynamicExp) * MHz
        //
        // 52.12 * 32.0 * 52.12 = 40.24 => 20.12
        // Will overflow when power/current is > 1048575 mW/mA, this
        // should be a safe assumption.
        //
        tempFXP52_12 =
            (LwUFXP52_12)pwrEquationDynamicScaleVoltagePower(pDyn,
            LW_TYPES_U32_TO_UFXP_X_Y_SCALED(20, 12,
            pEst->voltmV, 1000));
    }
    else
    {
        tempFXP52_12 =
            (LwUFXP52_12)pwrEquationDynamicScaleVoltageLwrrent(pDyn,
            LW_TYPES_U32_TO_UFXP_X_Y_SCALED(20, 12,
            pEst->voltmV, 1000));
    }

    factor = 1000;
    lwU64Mul(&tempFXP52_12, &tempFXP52_12, &factor);
    factor = pEst->effectiveFreqMHz;
    lwU64Mul(&tempFXP52_12, &tempFXP52_12, &factor);

    lwU64Mul(&productFXP40_24, &tempFXP52_12, &workloadFXP52_12);
    lw64Lsr(&estimatedValU64, &productFXP40_24, 24);

    if (LwU64_HI32(estimatedValU64) != LW_TYPES_FXP_ZERO)
    {
        // Overflow detected, cap estimatedVal to LW_U32_MAX
        pEst->estimatedVal = LW_U32_MAX;
    }
    else
    {
        pEst->estimatedVal = LwU64_LO32(estimatedValU64) + pEst->leakagemX;
    }

s_pwrPolicyGetEstMetricsAtFreq_WORKLOAD_SINGLE_1X_exit:
    return status;
}

/*!
 * Copy data from LwUFXP20_12 lpwrResidency array to
 * PMGR_LPWR_RESIDENCIES
 *
 * @param[out] pPgRes
 *                PMGR_LPWR_RESIDENCIES pointer.
 * @param[out] pLpwrResidency
 *                LwUFXP20_12 pointer to array of lpwr residencies
 *
 */
static void
s_pwrPolicyLpwrResidenciesCopyIn_WORKLOAD_SINGLE_1X
(
    PMGR_LPWR_RESIDENCIES  *pPgRes,
    LwUFXP20_12            *pLpwrResidency,
    LwU8                    size
)
{
    LwU8 i;
    PMU_HALT_COND(pPgRes != NULL);
    PMU_HALT_COND(pLpwrResidency != NULL);
    PMU_HALT_COND(size <= LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MAX);

    for (i = 0; i < size; i++)
    {
        pPgRes->lpwrResidency[i] = pLpwrResidency[i];
    }
}

/*!
 * @brief      Updates the independent domains vmin if any of the clock domain
 *             frequencies in the metrics input differs from what was originally
 *             observed for the power model's voltage floor.
 *
 * @todo       Consider case of clock propogation when incomplete sets of inputs
 *             are provided for secondary domains.
 *
 * @param      pSingle1x                The single 1 x
 * @param      pIndependentDomainsVmin  The independent domains vmin
 * @param      pMetricsInput            The metrics input
 *
 * @return     The flcn status.
 */
static FLCN_STATUS
s_pwrPolicyUpdateVoltageFloor_WORKLOAD_SINGLE_1X
(
    PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1x,
    LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_WORKLOAD_SINGLE_1X_INDEPENDENT_DOMAINS_VMIN *pIndependentDomainsVmin,
    PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT *pMetricsInput
)
{
    BOARDOBJGRPMASK_E32 metricsDomainsMask;
    BOARDOBJGRPMASK_E32 intersectMask;
    BOARDOBJGRPMASK_E32 independentClkDomainMask;
    LwBoardObjIdx       clkIdx;
    FLCN_STATUS         status;
    PERF_LIMITS_VF      vfRail;
    LwBool              bUpdateRequired = LW_FALSE;

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pSingle1x == NULL) ||
         (pIndependentDomainsVmin == NULL) ||
         (pMetricsInput == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_pwrPolicyUpdateVoltageFloor_WORKLOAD_SINGLE_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfPwrModelScaleMeticsInputGetDomainsMask(
            pMetricsInput,
            &metricsDomainsMask),
        s_pwrPolicyUpdateVoltageFloor_WORKLOAD_SINGLE_1X_exit);

    boardObjGrpMaskInit_E32(&intersectMask);
    boardObjGrpMaskInit_E32(&independentClkDomainMask);

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskImport_E32(
            &independentClkDomainMask,
            &pIndependentDomainsVmin->independentClkDomainMask),
        s_pwrPolicyUpdateVoltageFloor_WORKLOAD_SINGLE_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskAnd(
            &intersectMask,
            &metricsDomainsMask,
            &independentClkDomainMask),
        s_pwrPolicyUpdateVoltageFloor_WORKLOAD_SINGLE_1X_exit);

    vfRail.idx = BOARDOBJ_GRP_IDX_TO_8BIT(pSingle1x->voltRailIdx);

    // Update vmins for specified clock domains
    BOARDOBJGRPMASK_FOR_EACH_BEGIN(&intersectMask, clkIdx)
    {
        LwU32          freqkHz;
        PERF_LIMITS_VF vfDomain;

        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfPwrModelScaleMeticsInputGetFreqkHz(
                pMetricsInput,
                clkIdx,
                &freqkHz),
            s_pwrPolicyUpdateVoltageFloor_WORKLOAD_SINGLE_1X_exit);

        // Update vmin for domain if needed
        if (freqkHz != pIndependentDomainsVmin->domains[clkIdx].freqkHz)
        {
            bUpdateRequired = LW_TRUE;
            vfDomain.idx = BOARDOBJ_GRP_IDX_TO_8BIT(clkIdx);

            // Get the voltage required to support given freq
            vfDomain.value = freqkHz;
            PMU_HALT_OK_OR_GOTO(status,
                perfLimitsFreqkHzToVoltageuV(&vfDomain, &vfRail, LW_TRUE),
                s_pwrPolicyUpdateVoltageFloor_WORKLOAD_SINGLE_1X_exit);

            // Cache off the individual domain vmin and frequency
            pIndependentDomainsVmin->domains[clkIdx].freqkHz = vfDomain.value;
            pIndependentDomainsVmin->domains[clkIdx].voltuV  = vfRail.value;
        }
    }
    BOARDOBJGRPMASK_FOR_EACH_END;

    // Sanity check that estimated metrics independent clk domain's mask is correct
    PMU_ASSERT_TRUE_OR_GOTO(status,
        boardObjGrpMaskIsEqual(
            &independentClkDomainMask,
            &pSingle1x->independentClkDomainMask),
        FLCN_ERR_ILWALID_STATE,
        s_pwrPolicyUpdateVoltageFloor_WORKLOAD_SINGLE_1X_exit);

    if (!bUpdateRequired)
    {
        goto s_pwrPolicyUpdateVoltageFloor_WORKLOAD_SINGLE_1X_exit;
    }

    // Update vmin considering all independent domains
    pIndependentDomainsVmin->voltFlooruV = pIndependentDomainsVmin->railVminuV;
    BOARDOBJGRPMASK_FOR_EACH_BEGIN(&independentClkDomainMask, clkIdx)
    {
        // Store the floor value in voltageFlooruV
        pIndependentDomainsVmin->voltFlooruV = LW_MAX(pIndependentDomainsVmin->voltFlooruV,
                                                    pIndependentDomainsVmin->domains[clkIdx].voltuV);
    }
    BOARDOBJGRPMASK_FOR_EACH_END;

s_pwrPolicyUpdateVoltageFloor_WORKLOAD_SINGLE_1X_exit:
    return status;
}

/*!
 * Helper function to get the soft floor frequency value
 *
 * @param[in] pSingle1x  Pointer to PWR_POLICY_WORKLOAD_SINGLE_1X
 * @param[in] pPerfCfControllerStatus
 *                       Pointer to PERF_CF_CONTROLLERS_REDUCED_STATUS
 * @param[out] pFreqMHz  pointer to the soft floor frequency
 *
 * @return FLCN_OK
 *     Frequency is successfully retrieved.
 * @return Others
 *     Invalid GPU state returned.
 */
static FLCN_STATUS
s_pwrPolicySoftFloorFreqGet_WORKLOAD_SINGLE_1X
(
    PWR_POLICY_WORKLOAD_SINGLE_1X       *pSingle1x,
    PERF_CF_CONTROLLERS_REDUCED_STATUS  *pPerfCfControllersStatus,
    LwU32                               *pFreqMHz
)
{
    LwU32                     freqKHz;
    CLK_DOMAIN               *pDomain;
    PSTATE                   *pPstate;
    PERF_LIMITS_PSTATE_RANGE  pstateRange;
    PERF_LIMITS_VF_ARRAY      vfArrayIn;
    PERF_LIMITS_VF_ARRAY      vfArrayOut;
    BOARDOBJGRPMASK_E32       propFromClkDomainMask;
    BOARDOBJGRPMASK_E32       propToClkDomainMask;
    FLCN_STATUS               status;
    PERF_CF_CONTROLLERS_REDUCED_BOARDOBJ_GET_STATUS_UNION
                             *pControllerStatus;
    PERF_LIMITS_VF            vfDomain;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pFreqMHz != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_pwrPolicySoftFloorFreqGet_WORKLOAD_SINGLE_1X_exit);

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pPerfCfControllersStatus != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_pwrPolicySoftFloorFreqGet_WORKLOAD_SINGLE_1X_exit);

    pControllerStatus =
        &pPerfCfControllersStatus->controllers[pSingle1x->softFloor.perfCfControllerIdx];

    freqKHz = pControllerStatus->controllerUtil.avgTargetkHz;
    if (freqKHz == 0U)
    {
        *pFreqMHz = freqKHz;
        status    = FLCN_OK;
        goto s_pwrPolicySoftFloorFreqGet_WORKLOAD_SINGLE_1X_exit;
    }

    // Init the "Prop From" and "prop to" clock domain masks
    boardObjGrpMaskInit_E32(&propFromClkDomainMask);
    boardObjGrpMaskInit_E32(&propToClkDomainMask);

    // Init the input/output struct.
    PERF_LIMITS_VF_ARRAY_INIT(&vfArrayIn);
    PERF_LIMITS_VF_ARRAY_INIT(&vfArrayOut);

    pPstate = PSTATE_GET_BY_LEVEL(perfGetPstateCount() - 1);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pPstate != NULL), FLCN_ERR_ILWALID_STATE,
        s_pwrPolicySoftFloorFreqGet_WORKLOAD_SINGLE_1X_exit);

    PERF_LIMITS_RANGE_SET_VALUE(&pstateRange,
        BOARDOBJ_GET_GRP_IDX(&pPstate->super));

    vfDomain.idx = BOARDOBJ_GRP_IDX_TO_8BIT(pSingle1x->softFloor.perfCfControllerClkIdx);
    vfDomain.value = freqKHz;
    PMU_ASSERT_OK_OR_GOTO(status,
        perfLimitsFreqkHzQuantize(&vfDomain,
                                  &pstateRange,
                                  LW_FALSE),
        s_pwrPolicySoftFloorFreqGet_WORKLOAD_SINGLE_1X_exit);

    boardObjGrpMaskBitSet(&propFromClkDomainMask, 
        pSingle1x->softFloor.perfCfControllerClkIdx);
    vfArrayIn.pMask = &propFromClkDomainMask;
    vfArrayIn.values[pSingle1x->softFloor.perfCfControllerClkIdx] = vfDomain.value;

    // populate vfarraymask.out
    boardObjGrpMaskBitSet(&propToClkDomainMask, pSingle1x->clkDomainIdx);
    vfArrayOut.pMask = &propToClkDomainMask;

    PMU_ASSERT_OK_OR_GOTO(status,
        perfLimitsFreqPropagate(&pstateRange,
            &vfArrayIn, &vfArrayOut, LW2080_CTRL_CLK_CLK_PROP_TOP_IDX_ILWALID),
        s_pwrPolicySoftFloorFreqGet_WORKLOAD_SINGLE_1X_exit);

    pDomain = CLK_DOMAIN_GET(pSingle1x->clkDomainIdx);

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDomain != NULL),
        FLCN_ERR_ILWALID_STATE,
        s_pwrPolicySoftFloorFreqGet_WORKLOAD_SINGLE_1X_exit);

    *pFreqMHz = vfArrayOut.values[pSingle1x->clkDomainIdx] /
                    clkDomainFreqkHzScaleGet(pDomain);

s_pwrPolicySoftFloorFreqGet_WORKLOAD_SINGLE_1X_exit:
    return status;
}
