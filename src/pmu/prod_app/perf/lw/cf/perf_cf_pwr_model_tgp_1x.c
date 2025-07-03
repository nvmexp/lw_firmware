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
 * @file    perf_cf_pwr_model_tgp_1x.c
 * @copydoc perf_cf_pwr_model_tgp_1x.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf.h"
#include "perf/cf/perf_cf_pwr_model_tgp_1x.h"

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Global Variables -------------------------------- */
static LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_COMBINED_1X workloadCombined1xObservedMetrics
    GCC_ATTRIB_SECTION("dmem_perfCfPwrModelTgp1xRuntimeScratch", "workloadCombined1xObservedMetrics");

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfCfPwrModelGrpIfaceModel10ObjSetImpl_TGP_1X
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);

    RM_PMU_PERF_CF_PWR_MODEL_TGP_1X    *pTmpTgp1x   =
        (RM_PMU_PERF_CF_PWR_MODEL_TGP_1X *)pBoardObjDesc;
    PERF_CF_PWR_MODEL_TGP_1X           *pTgp1x;
    FLCN_STATUS                         status      = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pBObjGrp      == NULL) ||
         (ppBoardObj    == NULL) ||
         (pBoardObjDesc == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
         perfCfPwrModelGrpIfaceModel10ObjSetImpl_TGP_1X_exit);

    // Ilwoke the super-class constructor.
    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfPwrModelGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc),
        perfCfPwrModelGrpIfaceModel10ObjSetImpl_TGP_1X_exit);
    pTgp1x = (PERF_CF_PWR_MODEL_TGP_1X *)(*ppBoardObj);

    // Set member variables.
    pTgp1x->workloadPwrPolicyIdx = pTmpTgp1x->workloadPwrPolicyIdx;

perfCfPwrModelGrpIfaceModel10ObjSetImpl_TGP_1X_exit:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
perfCfPwrModelIfaceModel10GetStatus_TGP_1X
(
    BOARDOBJ_IFACE_MODEL_10          *pModel10,
    RM_PMU_BOARDOBJ   *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    FLCN_STATUS status;
    RM_PMU_PERF_CF_PWR_MODEL_TGP_1X_GET_STATUS *pStatus =
        (RM_PMU_PERF_CF_PWR_MODEL_TGP_1X_GET_STATUS *)pPayload;
    PERF_CF_PWR_MODEL_TGP_1X *pTgp1x =
            (PERF_CF_PWR_MODEL_TGP_1X *)pBoardObj;

    // Sanity checking.
    PMU_HALT_OK_OR_GOTO(status,
        (pBoardObj != NULL &&
         pPayload  != NULL) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        perfCfPwrModelIfaceModel10GetStatus_TGP_1X_exit);

    // Call the super-class implementation.
    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfPwrModelIfaceModel10GetStatus_SUPER(
            pModel10,
            pPayload),
        perfCfPwrModelIfaceModel10GetStatus_TGP_1X_exit);

    // Copy over type specific status data.
    (void)pTgp1x;
    (void)pStatus;

perfCfPwrModelIfaceModel10GetStatus_TGP_1X_exit:
    return status;
}

/*!
 * @copydoc PerfCfPwrModelLoad
 */
FLCN_STATUS
perfCfPwrModelLoad_TGP_1X
(
    PERF_CF_PWR_MODEL *pPwrModel
)
{
    PERF_CF_PWR_MODEL_TGP_1X   *pTgp1x = (PERF_CF_PWR_MODEL_TGP_1X *)pPwrModel;
    LwU32                       apiDomains[] = {
                                                  CLK_DOMAIN_MASK_GPC,
                                                  CLK_DOMAIN_MASK_XBAR,     // Only really needed for ga10x
                                                  LW2080_CTRL_CLK_DOMAIN_MCLK,
                                               };
    LwLength                    i;
    FLCN_STATUS                 status;

    // Sanity checking.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pPwrModel != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfPwrModelLoad_TGP_1X_exit);

    // KO-TODO: Additionally fetch domains required by the CWCC1X
    for (i = 0; i < LW_ARRAY_ELEMENTS(apiDomains); i++)
    {
        LwU32 idx;

        PMU_ASSERT_OK_OR_GOTO(status,
            clkDomainsGetIndexByApiDomain(apiDomains[i], &idx),
            perfCfPwrModelLoad_TGP_1X_exit);

        boardObjGrpMaskBitSet(&pTgp1x->super.requiredDomainsMask, (LwU8)idx);
    }

    // Omit non-programmable domains
    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskAnd(
            &pTgp1x->super.requiredDomainsMask,
            &pTgp1x->super.requiredDomainsMask,
            &(CLK_CLK_DOMAINS_GET()->progDomainsMask)),
        perfCfPwrModelLoad_TGP_1X_exit);

    // TGP_1X requires at least one programmable domain as input
    PMU_ASSERT_TRUE_OR_GOTO(status,
        !boardObjGrpMaskIsZero(&pTgp1x->super.requiredDomainsMask),
        FLCN_ERR_NOT_SUPPORTED,
        perfCfPwrModelLoad_TGP_1X_exit);

perfCfPwrModelLoad_TGP_1X_exit:
    return status;
}

/*!
 * @copydoc PerfCfPwrModelSampleDataInit
 */
FLCN_STATUS
perfCfPwrModelSampleDataInit_TGP_1X
(
    PERF_CF_PWR_MODEL                              *pPwrModel,
    PERF_CF_PWR_MODEL_SAMPLE_DATA                  *pSampleData,
    PERF_CF_TOPOLOGYS_STATUS                       *pPerfTopoStatus,
    PWR_CHANNELS_STATUS                            *pPwrChannelsStatus,
    PERF_CF_PM_SENSORS_STATUS                      *pPmSensorsStatus,
    PERF_CF_CONTROLLERS_REDUCED_STATUS             *pPerfCfControllersStatus,
    PERF_CF_PWR_MODEL_CALLER_SPECIFIED_SAMPLE_DATA *pPerfCfPwrModelCallerSpecifiedData
)
{
    FLCN_STATUS status = FLCN_OK;
    PERF_CF_PWR_MODEL_TGP_1X *const pPwrModelTgp1x =
        (PERF_CF_PWR_MODEL_TGP_1X *)pPwrModel;

    // Sanity checking.
    if (pPwrModelTgp1x == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfPwrModelSampleDataInit_TGP_1X_exit;
    }

    if (pSampleData == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfPwrModelSampleDataInit_TGP_1X_exit;
    }

perfCfPwrModelSampleDataInit_TGP_1X_exit:
    return status;
}

/*!
 * @copydoc PerfCfPwrModelObserveMetrics
 */
FLCN_STATUS
perfCfPwrModelObserveMetrics_TGP_1X
(
    PERF_CF_PWR_MODEL                             *pPwrModel,
    PERF_CF_PWR_MODEL_SAMPLE_DATA                 *pSampleData,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS    *pObservedMetrics
)
{
    PERF_CF_PWR_MODEL_TGP_1X   *pTgp1x = (PERF_CF_PWR_MODEL_TGP_1X *)pPwrModel;
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TGP_1X_OBSERVED
                               *pTgp1xObservedMetrics = (LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TGP_1X_OBSERVED *)pObservedMetrics;
    FLCN_STATUS                 status = FLCN_OK;
    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pPwrModel        == NULL) ||
         (pSampleData      == NULL) ||
         (pObservedMetrics == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        perfCfPwrModelObserveMetrics_TGP_1X_exit);

    (void)pTgp1x;
    (void)pTgp1xObservedMetrics;

perfCfPwrModelObserveMetrics_TGP_1X_exit:
    return status;
}

/*!
 * @copydoc PerfCfPwrModelScale
 */
FLCN_STATUS
perfCfPwrModelScale_TGP_1X
(
    PERF_CF_PWR_MODEL                              *pPwrModel,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS     *pObservedMetrics,
    LwLength                                        numMetricsInputs,
    PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT          *pMetricsInputs,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS    **ppEstimatedMetrics,
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS            *pTypeParams
)
{
    PERF_CF_PWR_MODEL_TGP_1X                                   *pTgp1x = (PERF_CF_PWR_MODEL_TGP_1X *)pPwrModel;
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TGP_1X_OBSERVED *pTgp1xObserved = (LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TGP_1X_OBSERVED *)pObservedMetrics;
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TGP_1X          *pTgp1xEstimated;
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS                 *localEstimatedMetrics[LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_ESTIMATED_METRICS_MAX];
    PWR_POLICY_WORKLOAD_COMBINED_1X                            *pWorkloadPwrPolicy;
    LwLength                                                    loopIdx;
    LwBool                                                      bSemaphoreAcquired = LW_FALSE;
    FLCN_STATUS                                                 status = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pPwrModel          == NULL) ||
         (pObservedMetrics   == NULL) ||
         (pMetricsInputs     == NULL) ||
         (ppEstimatedMetrics == NULL) ||
         (pTypeParams == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        perfCfPwrModelScale_TGP_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        (numMetricsInputs == 0U) ||
        (numMetricsInputs > LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_ESTIMATED_METRICS_MAX) ?
            FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        perfCfPwrModelScale_TGP_1X_exit);

    // Ensure metrics type is correct
    for (loopIdx = 0; loopIdx < numMetricsInputs; loopIdx++)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            ppEstimatedMetrics[loopIdx]->type == LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_TGP_1X ?
                FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
            perfCfPwrModelScale_TGP_1X_exit);
    }

    // Acquire perf and pwr policies semaphores
    //
    // Note that when taking both the perf semaphore and the PWR_POLICIES
    // semaphore, the perf semaphore must be taken first.
    //
    PWR_POLICIES_SEMAPHORE_TAKE;
    bSemaphoreAcquired = LW_TRUE;

    // Fetch workload pwr policy
    pWorkloadPwrPolicy = (PWR_POLICY_WORKLOAD_COMBINED_1X*)PWR_POLICY_GET(pTgp1x->workloadPwrPolicyIdx);
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pWorkloadPwrPolicy == NULL) ||
         (BOARDOBJ_GET_TYPE(pWorkloadPwrPolicy) !=
            LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_COMBINED_1X)) ?
            FLCN_ERR_ILWALID_STATE : FLCN_OK,
        perfCfPwrModelScale_TGP_1X_exit);

    // Fetch CWCC1X observed metrics
    PMU_ASSERT_OK_OR_GOTO(status,
        pwrPolicyWorkloadCombined1xObservedMetricsExport(
            pWorkloadPwrPolicy,
            &workloadCombined1xObservedMetrics),
        perfCfPwrModelScale_TGP_1X_exit);

    // Override workload parameters
    PMU_ASSERT_OK_OR_GOTO(status,
        pwrPolicyWorkloadCombined1xImportTgp1xWorkloadParameters(
            pWorkloadPwrPolicy,
            &workloadCombined1xObservedMetrics,
            &pTgp1xObserved->workloadParameters),
        perfCfPwrModelScale_TGP_1X_exit);

    // Create local array of estimated metrics pointers and set type
    for (loopIdx = 0; loopIdx < numMetricsInputs; loopIdx++)
    {
        pTgp1xEstimated = (LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TGP_1X *)ppEstimatedMetrics[loopIdx];
        localEstimatedMetrics[loopIdx] = &pTgp1xEstimated->workloadCombined1x.super;
        localEstimatedMetrics[loopIdx]->type = LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_WORKLOAD_COMBINED_1X;
    }

    // Call CWCC1X scale
    PMU_ASSERT_OK_OR_GOTO(status,
        pwrPolicyPerfCfPwrModelScale_WORKLOAD_COMBINED_1X(
            &pWorkloadPwrPolicy->pwrModel,
            &workloadCombined1xObservedMetrics.super,
            numMetricsInputs,
            pMetricsInputs,
            localEstimatedMetrics,
            pTypeParams),
        perfCfPwrModelScale_TGP_1X_exit);

    // Populate the estimated metrics
    for (loopIdx = 0; loopIdx < numMetricsInputs; loopIdx++)
    {
        pTgp1xEstimated = (LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TGP_1X *)ppEstimatedMetrics[loopIdx];

        pTgp1xEstimated->fbPwrmW    = pTgp1xObserved->fbPwrmW;
        pTgp1xEstimated->otherPwrmW = pTgp1xObserved->otherPwrmW;
        pTgp1xEstimated->tgpPwrmW   = pTgp1xEstimated->workloadCombined1x.estimatedVal +
                                      pTgp1xEstimated->fbPwrmW +
                                      pTgp1xEstimated->otherPwrmW;
    }

perfCfPwrModelScale_TGP_1X_exit:
    // Release semaphores if acquired
    if (bSemaphoreAcquired)
    {
        PWR_POLICIES_SEMAPHORE_GIVE;
    }
    return status;
}

/*!
 * @copydoc perfCfPwrModelScaleTypeParamsInit_TGP_1X
 */
FLCN_STATUS
perfCfPwrModelScaleTypeParamsInit_TGP_1X
(
    PERF_CF_PWR_MODEL *pPwrModel,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS *pCtrlParams,
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS *pTypeParams
)
{
    FLCN_STATUS status;
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_TGP_1X *pTgp1xParams =
        (PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_TGP_1X *)pTypeParams;

    PMU_ASSERT_OK_OR_GOTO(status,
        (pPwrModel != NULL) &&
        (pCtrlParams != NULL) &&
        (pTypeParams != NULL) &&
        (pCtrlParams->type == LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_TGP_1X) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        perfCfPwrModelScaleTypeParamsInit_TGP_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfPwrModelScaleTypeParamsInit_SUPER(
            pPwrModel,
            pCtrlParams,
            pTypeParams),
        perfCfPwrModelScaleTypeParamsInit_TGP_1X_exit);

    pTgp1xParams->rsvd = pCtrlParams->data.tgp1x.rsvd;

perfCfPwrModelScaleTypeParamsInit_TGP_1X_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
