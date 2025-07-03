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
 * @file    client_perf_cf_pwr_model_profile_tgp_1x.c
 * @copydoc client_perf_cf_pwr_model_profile_tgp_1x.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf.h"
#include "perf/cf/client_perf_cf_pwr_model_profile_tgp_1x.h"

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clientPerfCfPwrModelProfileGrpIfaceModel10ObjSetImpl_TGP_1X
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PERF_CF_CLIENT_PWR_MODEL_PROFILE_TGP_1X *pTmpTgp1x =
        (RM_PMU_PERF_CF_CLIENT_PWR_MODEL_PROFILE_TGP_1X *)pBoardObjDesc;
    CLIENT_PERF_CF_PWR_MODEL_PROFILE_TGP_1X        *pTgp1x;
    FLCN_STATUS                                     status = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pBObjGrp      != NULL) &&
         (ppBoardObj    != NULL) &&
         (pBoardObjDesc != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        clientPerfCfPwrModelProfileGrpIfaceModel10ObjSetImpl_TGP_1X_exit);

    // Ilwoke the super-class constructor.
    PMU_ASSERT_OK_OR_GOTO(status,
        clientPerfCfPwrModelProfileGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc),
        clientPerfCfPwrModelProfileGrpIfaceModel10ObjSetImpl_TGP_1X_exit);
    pTgp1x = (CLIENT_PERF_CF_PWR_MODEL_PROFILE_TGP_1X *)(*ppBoardObj);

    // Set member variables.
    pTgp1x->workloadParameters = pTmpTgp1x->workloadParameters;
    pTgp1x->fbPwrmW            = pTmpTgp1x->fbPwrmW;
    pTgp1x->otherPwrmW         = pTmpTgp1x->otherPwrmW;

clientPerfCfPwrModelProfileGrpIfaceModel10ObjSetImpl_TGP_1X_exit:
    return status;
}

/*!
 * @copydoc ClientPerfCfPwrModelProfileScaleInternalMetricsInit
 */
FLCN_STATUS
clientPerfCfPwrModelProfileScaleInternalMetricsInit_TGP_1X
(
    CLIENT_PERF_CF_PWR_MODEL_PROFILE           *pClientPwrModelProfile,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS *pObservedMetrics,
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS        *pTypeParams
)
{
    CLIENT_PERF_CF_PWR_MODEL_PROFILE_TGP_1X *pProfileTgp1x
        = (CLIENT_PERF_CF_PWR_MODEL_PROFILE_TGP_1X *) pClientPwrModelProfile;
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TGP_1X_OBSERVED *pMetricsTgp1x
        = (LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TGP_1X_OBSERVED *) pObservedMetrics;
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_TGP_1X *pTypeParamsTgp1x
        = (PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_TGP_1X *) pTypeParams;
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pClientPwrModelProfile != NULL) &&
         (pObservedMetrics       != NULL) &&
         (pTypeParams            != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        clientPerfCfPwrModelProfileScaleInternalMetricsInit_TGP_1X_exit);

    // Call super first
    PMU_ASSERT_OK_OR_GOTO(status,
        clientPerfCfPwrModelProfileScaleInternalMetricsInit_SUPER(
           pClientPwrModelProfile, pObservedMetrics, pTypeParams),
        clientPerfCfPwrModelProfileScaleInternalMetricsInit_TGP_1X_exit);

    // Populate metrics
    pMetricsTgp1x->super.type         = LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_TGP_1X;
    pMetricsTgp1x->workloadParameters = pProfileTgp1x->workloadParameters;
    pMetricsTgp1x->fbPwrmW            = pProfileTgp1x->fbPwrmW;
    pMetricsTgp1x->otherPwrmW         = pProfileTgp1x->otherPwrmW;

    // Populate bounds
    pTypeParamsTgp1x->super.type = LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_TGP_1X;

clientPerfCfPwrModelProfileScaleInternalMetricsInit_TGP_1X_exit:
    return status;
}

/*!
 * @copydoc CclientPerfCfPwrModelProfileScaleInternalMetricsExtract
 */
FLCN_STATUS
clientPerfCfPwrModelProfileScaleInternalMetricsExtract_TGP_1X
(
    CLIENT_PERF_CF_PWR_MODEL_PROFILE                               *pClientPwrModelProfile,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS                     *pEstimatedMetrics,
    LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_SCALE_OUTPUT *pOutputs
)
{
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TGP_1X *pMetricsTgp1x
        = (LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TGP_1X *) pEstimatedMetrics;
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pClientPwrModelProfile != NULL) &&
         (pEstimatedMetrics      != NULL) &&
         (pOutputs               != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        clientPerfCfPwrModelProfileScaleInternalMetricsExtract_TGP_1X_exit);

    // Call super first
    PMU_ASSERT_OK_OR_GOTO(status,
        clientPerfCfPwrModelProfileScaleInternalMetricsExtract_SUPER(
           pClientPwrModelProfile, pEstimatedMetrics, pOutputs),
        clientPerfCfPwrModelProfileScaleInternalMetricsExtract_TGP_1X_exit);

    // Populate outputs
    pOutputs->totalGpuPwrmW = pMetricsTgp1x->tgpPwrmW;

clientPerfCfPwrModelProfileScaleInternalMetricsExtract_TGP_1X_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
