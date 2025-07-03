/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    client_perf_cf_pwr_model_profile.c
 * @copydoc client_perf_cf_pwr_model_profile.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "dmemovl.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf.h"
#include "perf/cf/client_perf_cf_pwr_model_profile.h"
#include "pmu_objperf.h"

#include "g_pmurpc.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjGrpIfaceModel10SetEntry              (s_clientPerfCfPwrModelProfileIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_clientPerfCfPwrModelProfileIfaceModel10SetEntry");


/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Schema for scratch buffer used for clientPerfCfPwrModelProfileScale.
 *
 * Add more types here behind feature checks as additional client profile types
 * are supported.
 */
typedef struct
{
    PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT metricsInput;

    union
    {
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS          super;
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TGP_1X   tgp1x;
    } metricsEstimated;

    union
    {
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS                  super;
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TGP_1X_OBSERVED  tgp1x;
    } metricsObserved;

    union
    {
        PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS         super;
        PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_TGP_1X  tgp1x;
    } metricsTypeParams;
} CLIENT_PROFILE_SCALING_SCRATCH_DATA;

/* ------------------------ Global Variables -------------------------------- */
/*!
 * Scratch buffer used in clientPerfCfPwrModelProfileScale
 */
static CLIENT_PROFILE_SCALING_SCRATCH_DATA clientProfileScalingScratchData
    GCC_ATTRIB_SECTION("dmem_clientPerfCfPwrModelProfileScaleRpc", "clientProfileScalingScratchData");

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clientPerfCfPwrModelProfileBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS     status = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_CLIENT_PERF_CF_PWR_MODEL_PROFILE_CONSTRUCT
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Construct using header and entries.
        status = BOARDOBJGRP_IFACE_MODEL_10_SET(E32,                         // _grpType
                CLIENT_PERF_CF_PWR_MODEL_PROFILE,                   // _class
                pBuffer,                                            // _pBuffer
                boardObjGrpIfaceModel10SetHeader,                         // _hdrFunc
                s_clientPerfCfPwrModelProfileIfaceModel10SetEntry,        // _entryFunc
                ga100AndLater.perfCf.clientPwrModelProfileGrpSet,   // _ssElement
                OVL_INDEX_DMEM(perfCfModel),                        // _ovlIdxDmem
                BoardObjGrpVirtualTablesNotImplemented);            // _ppObjectVtables
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clientPerfCfPwrModelProfileBoardObjGrpIfaceModel10Set_exit; // NJ??
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

clientPerfCfPwrModelProfileBoardObjGrpIfaceModel10Set_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clientPerfCfPwrModelProfileGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);

    RM_PMU_PERF_CF_CLIENT_PWR_MODEL_PROFILE    *pTempClientPwrModelProfile =
        (RM_PMU_PERF_CF_CLIENT_PWR_MODEL_PROFILE *)pBoardObjDesc;
    CLIENT_PERF_CF_PWR_MODEL_PROFILE           *pClientPwrModelProfile;
    FLCN_STATUS                                 status    = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pBObjGrp      == NULL) ||
         (ppBoardObj    == NULL) ||
         (pBoardObjDesc == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
         clientPerfCfPwrModelProfileGrpIfaceModel10ObjSetImpl_SUPER_exit);

    // Ilwoke the super-class constructor.
    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc),
        clientPerfCfPwrModelProfileGrpIfaceModel10ObjSetImpl_SUPER_exit);
    pClientPwrModelProfile = (CLIENT_PERF_CF_PWR_MODEL_PROFILE *)(*ppBoardObj);

    // Set member variables.
    pClientPwrModelProfile->profileId             = pTempClientPwrModelProfile->profileId;
    pClientPwrModelProfile->pwrModelSemanticIndex = pTempClientPwrModelProfile->pwrModelSemanticIndex;

clientPerfCfPwrModelProfileGrpIfaceModel10ObjSetImpl_SUPER_exit:
    return status;
}

/*!
 * @copydoc ClientPerfCfPwrModelProfileScale
 */
FLCN_STATUS
clientPerfCfPwrModelProfileScale
(
    CLIENT_PERF_CF_PWR_MODEL_PROFILE                               *pClientPwrModelProfile,
    CLIENT_PERF_CF_PWR_MODEL_PROFILE_SCALE_INPUT                   *pInput,
    LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_SCALE_OUTPUT *pOutput
)
{
    PERF_CF_PWR_MODEL  *pPwrModel;
    FLCN_STATUS         status;
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS *pEstimatedMetrics;

    // Sanity checking.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pClientPwrModelProfile != NULL) &&
         (pInput                 != NULL) &&
         (pOutput                != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        clientPerfCfPwrModelProfileScale_exit);

    // 1. Get the internal pwr model for this profile
    pPwrModel = PERF_CF_PWR_MODEL_GET_BY_SEMANTIC_IDX(pClientPwrModelProfile->pwrModelSemanticIndex);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pPwrModel != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        clientPerfCfPwrModelProfileScale_exit);

    // 2. Colwert pInput to PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT
    perfCfPwrModelScaleMeticsInputInit(&clientProfileScalingScratchData.metricsInput);

    PMU_ASSERT_OK_OR_GOTO(status,
        clientPerfCfPwrModelProfileScaleInputColwertToInternal(
            pInput, &clientProfileScalingScratchData.metricsInput),
        clientPerfCfPwrModelProfileScale_exit);

    // 3. Populate internal metrics and bounds
    PMU_ASSERT_OK_OR_GOTO(status,
        clientPerfCfPwrModelProfileScaleInternalMetricsInit(
            pClientPwrModelProfile,
            &clientProfileScalingScratchData.metricsObserved.super,
            &clientProfileScalingScratchData.metricsTypeParams.super),
        clientPerfCfPwrModelProfileScale_exit);

    // 4. Ilwoke perfCfPwrModelScale with internal parameters and pwr model
    clientProfileScalingScratchData.metricsEstimated.super.type =
        clientProfileScalingScratchData.metricsObserved.super.type;

    pEstimatedMetrics = &clientProfileScalingScratchData.metricsEstimated.super;

    status = perfCfPwrModelScale(
                pPwrModel,
                &clientProfileScalingScratchData.metricsObserved.super,
                1U,
                &clientProfileScalingScratchData.metricsInput,
                &pEstimatedMetrics,
                &clientProfileScalingScratchData.metricsTypeParams.super);
    if (status == FLCN_ERR_FREQ_NOT_SUPPORTED)
    {
        // This is an RPC failure scenario we want to handle without a breakpoint.
        // TODO: once  PMU_TRUE_BP() actually works, revert this to PMU_ASSERT_OK_OR_GOTO()
        goto clientPerfCfPwrModelProfileScale_exit;
    }
    PMU_ASSERT_OK_OR_GOTO(status,
        status,
        clientPerfCfPwrModelProfileScale_exit);

    // 5. Extract estimated metrics into pOutput
    PMU_ASSERT_OK_OR_GOTO(status,
        clientPerfCfPwrModelProfileScaleInternalMetricsExtract(
            pClientPwrModelProfile,
            pEstimatedMetrics,
            pOutput),
        clientPerfCfPwrModelProfileScale_exit);

clientPerfCfPwrModelProfileScale_exit:
    return status;
}

/*!
 * @copydoc ClientPerfCfPwrModelProfileScaleInternalMetricsInit
 */
FLCN_STATUS
clientPerfCfPwrModelProfileScaleInternalMetricsInit
(
    CLIENT_PERF_CF_PWR_MODEL_PROFILE           *pClientPwrModelProfile,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS *pObservedMetrics,
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS        *pTypeParams
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pClientPwrModelProfile))
    {
        case LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_TYPE_TGP_1X:
            if (PMUCFG_FEATURE_ENABLED(PMU_CLIENT_PERF_CF_PWR_MODEL_PROFILE_TGP_1X))
            {
                PMU_ASSERT_OK_OR_GOTO(status,
                    clientPerfCfPwrModelProfileScaleInternalMetricsInit_TGP_1X(
                        pClientPwrModelProfile, pObservedMetrics, pTypeParams),
                    clientPerfCfPwrModelProfileScaleInternalMetricsInit_exit);
            }
            else
            {
                PMU_BREAKPOINT();
                goto clientPerfCfPwrModelProfileScaleInternalMetricsInit_exit;
            }
            break;

        default:
            PMU_BREAKPOINT();
            goto clientPerfCfPwrModelProfileScaleInternalMetricsInit_exit;
            break;
    }

clientPerfCfPwrModelProfileScaleInternalMetricsInit_exit:
    return status;
}

/*!
 * @copydoc ClientPerfCfPwrModelProfileScaleInternalMetricsInit
 */
FLCN_STATUS
clientPerfCfPwrModelProfileScaleInternalMetricsInit_SUPER
(
    CLIENT_PERF_CF_PWR_MODEL_PROFILE           *pClientPwrModelProfile,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS *pObservedMetrics,
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS        *pTypeParams
)
{
    // Nothing to do.
    return FLCN_OK;
}

/*!
 * @copydoc CclientPerfCfPwrModelProfileScaleInternalMetricsExtract
 */
FLCN_STATUS
clientPerfCfPwrModelProfileScaleInternalMetricsExtract
(
    CLIENT_PERF_CF_PWR_MODEL_PROFILE                               *pClientPwrModelProfile,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS                     *pEstimatedMetrics,
    LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_SCALE_OUTPUT *pOutputs
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pClientPwrModelProfile))
    {
        case LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_TYPE_TGP_1X:
            if (PMUCFG_FEATURE_ENABLED(PMU_CLIENT_PERF_CF_PWR_MODEL_PROFILE_TGP_1X))
            {
                PMU_ASSERT_OK_OR_GOTO(status,
                    clientPerfCfPwrModelProfileScaleInternalMetricsExtract_TGP_1X(
                        pClientPwrModelProfile, pEstimatedMetrics, pOutputs),
                    clientPerfCfPwrModelProfileScaleInternalMetricsExtract_exit);
            }
            else
            {
                PMU_BREAKPOINT();
                goto clientPerfCfPwrModelProfileScaleInternalMetricsExtract_exit;
            }
            break;

        default:
            PMU_BREAKPOINT();
            goto clientPerfCfPwrModelProfileScaleInternalMetricsExtract_exit;
            break;
    }

clientPerfCfPwrModelProfileScaleInternalMetricsExtract_exit:
    return status;
}

/*!
 * @copydoc CclientPerfCfPwrModelProfileScaleInternalMetricsExtract
 */
FLCN_STATUS
clientPerfCfPwrModelProfileScaleInternalMetricsExtract_SUPER
(
    CLIENT_PERF_CF_PWR_MODEL_PROFILE                               *pClientPwrModelProfile,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS                     *pEstimatedMetrics,
    LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_SCALE_OUTPUT *pOutputs
)
{
    // Nothing to do.
    return FLCN_OK;
}

/*!
 * @brief      Handler for PERF_CF_CLIENT_PWR_MODEL_PROFILE_SCALE RPC
 *
 * @param      pParams  The parameters
 *
 * @return     The flcn status.
 */
FLCN_STATUS
pmuRpcPerfCfClientPwrModelProfileScale
(
    RM_PMU_RPC_STRUCT_PERF_CF_CLIENT_PWR_MODEL_PROFILE_SCALE *pParams
)
{
    FLCN_STATUS                                     status;
    CLIENT_PERF_CF_PWR_MODEL_PROFILE               *pProfile;
    CLIENT_PERF_CF_PWR_MODEL_PROFILE_SCALE_INPUT    input;
    LwBoardObjIdx                                   i;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_CLIENT_PERF_CF_PWR_MODEL_PROFILE_SCALE_RPC
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        clientPerfCfPwrModelProfileScaleInputInit(&input);

        PMU_ASSERT_OK_OR_GOTO(status,
            clientPerfCfPwrModelProfileScaleInputImport(&pParams->input, &input),
            pmuRpcPerfCfClientPwrModelProfileScale_detach);

        // Scale using the profile matching the input profileId
        status = FLCN_ERR_OBJECT_NOT_FOUND;
        BOARDOBJGRP_ITERATOR_BEGIN(CLIENT_PERF_CF_PWR_MODEL_PROFILE, pProfile, i, NULL)
        {
            if (pProfile->profileId == pParams->profileId)
            {
                perfReadSemaphoreTake();
                status = clientPerfCfPwrModelProfileScale(pProfile,
                            &input, &pParams->output);
                perfReadSemaphoreGive();
                break;
            }
        }
        BOARDOBJGRP_ITERATOR_END;

        if (status == FLCN_ERR_FREQ_NOT_SUPPORTED)
        {
            // This is an RPC failure scenario we want to handle without a breakpoint.
            // TODO: once  PMU_TRUE_BP() actually works, revert this to PMU_ASSERT_OK_OR_GOTO()
            goto pmuRpcPerfCfClientPwrModelProfileScale_detach;
        }
        PMU_ASSERT_OK_OR_GOTO(
            status,
            status,
            pmuRpcPerfCfClientPwrModelProfileScale_detach);

pmuRpcPerfCfClientPwrModelProfileScale_detach:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_clientPerfCfPwrModelProfileIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    FLCN_STATUS   status = FLCN_ERR_NOT_SUPPORTED;

    switch (pBuf->type)
    {
        case LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_TYPE_TGP_1X:
            if (PMUCFG_FEATURE_ENABLED(PMU_CLIENT_PERF_CF_PWR_MODEL_PROFILE_TGP_1X))
            {
                PMU_HALT_OK_OR_GOTO(status,
                    clientPerfCfPwrModelProfileGrpIfaceModel10ObjSetImpl_TGP_1X(pModel10, ppBoardObj,
                        LW_SIZEOF32(CLIENT_PERF_CF_PWR_MODEL_PROFILE_TGP_1X), pBuf),
                    s_clientPerfCfPwrModelProfileIfaceModel10SetEntry_exit);
            }
            else
            {
                PMU_BREAKPOINT();
                goto s_clientPerfCfPwrModelProfileIfaceModel10SetEntry_exit;
            }
            break;

        default:
            PMU_BREAKPOINT();
            goto s_clientPerfCfPwrModelProfileIfaceModel10SetEntry_exit;
            break;
    }

s_clientPerfCfPwrModelProfileIfaceModel10SetEntry_exit:
    return status;
}
