/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrpolicy_perf_cf_pwr_model.c
 * @brief  Interface specification for a PWR_POLICY_PERF_CF_PWR_MODEL.
 *
 * The Interface provides all the capability to scale and estimate
 * perf and power based on observation data set.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objperf.h"
#include "pmu_objpmgr.h"
#include "dbgprintf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc BoardObjInterfaceConstruct
 */
FLCN_STATUS
pwrPolicyConstructImpl_PWR_POLICY_PERF_CF_PWR_MODEL
(
    BOARDOBJGRP                *pBObjGrp,
    BOARDOBJ_INTERFACE         *pInterface,
    RM_PMU_BOARDOBJ_INTERFACE  *pInterfaceDesc,
    INTERFACE_VIRTUAL_TABLE    *pVirtualTable
)
{
    RM_PMU_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_BOARDOBJ_SET *pSet =
        (RM_PMU_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_BOARDOBJ_SET *)pInterfaceDesc;
    PWR_POLICY_PERF_CF_PWR_MODEL    *pPwrModel;
    FLCN_STATUS                      status;

    status = boardObjInterfaceConstruct(pBObjGrp, pInterface, pInterfaceDesc, pVirtualTable);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrPolicyConstructImpl_PWR_POLICY_PERF_CF_PWR_MODEL_exit;
    }
    pPwrModel = (PWR_POLICY_PERF_CF_PWR_MODEL *)pInterface;

    (void)pSet;
    (void)pPwrModel;

pwrPolicyConstructImpl_PWR_POLICY_PERF_CF_PWR_MODEL_exit:
    return status;
}

/*!
 * @copydoc PwrPolicyPerfCfPwrModelObserveMetrics
 */
FLCN_STATUS
pwrPolicyPerfCfPwrModelObserveMetrics
(
    PWR_POLICY_PERF_CF_PWR_MODEL                   *pPwrModel,
    PERF_CF_PWR_MODEL_SAMPLE_DATA                  *pSampleData,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS     *pObsrvdMetrics
)
{
    PWR_POLICY            *pPolicy =
        (PWR_POLICY *)INTERFACE_TO_BOARDOBJ_CAST(pPwrModel);
    FLCN_STATUS            status  = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_SINGLE_1X:
             if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_SINGLE_WORKLOAD_1X))
             {
                 return pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_SINGLE_1X(pPwrModel,
                     pSampleData, pObsrvdMetrics);
             }
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_COMBINED_1X:
             if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X))
             {
                 return pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_COMBINED_1X(pPwrModel,
                     pSampleData, pObsrvdMetrics);
             }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrPolicyPerfCfPwrModelObserveMetrics_exit;
    }

pwrPolicyPerfCfPwrModelObserveMetrics_exit:
    return status;
}

/*!
 * @copydoc PwrPolicyPerfCfPwrModelScaleMetrics
 */
FLCN_STATUS
pwrPolicyPerfCfPwrModelScaleMetrics
(
    PWR_POLICY_PERF_CF_PWR_MODEL                *pPwrModel,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS  *pObservedMetrics,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS  *pEstimatedMetrics
)
{
    PWR_POLICY   *pPolicy =
        (PWR_POLICY *)INTERFACE_TO_BOARDOBJ_CAST(pPwrModel);
    FLCN_STATUS   status  = FLCN_ERR_NOT_SUPPORTED;
    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_SINGLE_1X:
             if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_SINGLE_WORKLOAD_1X))
             {
                 return pwrPolicyPerfCfPwrModelScaleMetrics_WORKLOAD_SINGLE_1X(
                     pPwrModel, pObservedMetrics, pEstimatedMetrics);
             }
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_COMBINED_1X:
             if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X))
             {
                 return pwrPolicyPerfCfPwrModelScaleMetrics_WORKLOAD_COMBINED_1X(
                     pPwrModel, pObservedMetrics, pEstimatedMetrics);
             }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrPolicyPerfCfPwrModelScaleMetrics_exit;
    }

pwrPolicyPerfCfPwrModelScaleMetrics_exit:
    return status;
}

/*!
 * @copydoc PwrPolicyPerfCfPwrModelScale
 */
FLCN_STATUS
pwrPolicyPerfCfPwrModelScale
(
    PWR_POLICY_PERF_CF_PWR_MODEL                   *pPwrModel,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS     *pObservedMetrics,
    LwLength                                        numMetricsInputs,
    PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT          *pMetricsInputs,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS    **ppEstimatedMetrics,
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS            *pTypeParams
)
{
    PWR_POLICY   *pPolicy =
        (PWR_POLICY *)INTERFACE_TO_BOARDOBJ_CAST(pPwrModel);
    FLCN_STATUS   status  = FLCN_ERR_NOT_SUPPORTED;
    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_SINGLE_1X:
             if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_SINGLE_WORKLOAD_1X))
             {
                 return pwrPolicyPerfCfPwrModelScale_WORKLOAD_SINGLE_1X(
                            pPwrModel,
                            pObservedMetrics,
                            numMetricsInputs,
                            pMetricsInputs,
                            ppEstimatedMetrics,
                            pTypeParams);
             }
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_COMBINED_1X:
             if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X))
             {
                 return pwrPolicyPerfCfPwrModelScale_WORKLOAD_COMBINED_1X(
                            pPwrModel,
                            pObservedMetrics,
                            numMetricsInputs,
                            pMetricsInputs,
                            ppEstimatedMetrics,
                            pTypeParams);
             }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrPolicyPerfCfPwrModelScale_exit;
    }

pwrPolicyPerfCfPwrModelScale_exit:
    return status;
}
