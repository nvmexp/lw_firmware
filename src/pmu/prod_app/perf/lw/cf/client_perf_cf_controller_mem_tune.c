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
 * @file    client_perf_cf_controller_util.c
 * @copydoc client_perf_cf_controller_util.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf.h"
#include "perf/cf/client_perf_cf_controller_mem_tune.h"
#include "perf/cf/perf_cf_controller_mem_tune.h"

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfCfClientPerfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    FLCN_STATUS status = FLCN_OK;


    status = perfCfClientPerfCfControllerGrpIfaceModel10ObjSetImpl_SUPER(
                pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfCfClientPerfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE_exit;
    }

perfCfClientPerfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfClientPerfCfControllerIfaceModel10GetStatus_MEM_TUNE
(
    BOARDOBJ_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJ *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_CF_CLIENT_PERF_CF_CONTROLLER_MEM_TUNE_GET_STATUS *pStatus =
        (RM_PMU_PERF_CF_CLIENT_PERF_CF_CONTROLLER_MEM_TUNE_GET_STATUS *)pPayload;
    CLIENT_PERF_CF_CONTROLLER_MEM_TUNE *pCliControllerMemTune =
        (CLIENT_PERF_CF_CONTROLLER_MEM_TUNE *)pBoardObj;
    PERF_CF_CONTROLLER_MEM_TUNE        *pControllerMemTune;
    FLCN_STATUS                         status                = FLCN_OK;

    status = perfCfClientPerfCfControllerIfaceModel10GetStatus_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfCfClientPerfCfControllerIfaceModel10GetStatus_MEM_TUNE_exit;
    }
    (void)pCliControllerMemTune;

    pControllerMemTune = (PERF_CF_CONTROLLER_MEM_TUNE *)
        PERF_CF_CONTROLLER_GET(pCliControllerMemTune->super.controllerIdx);

    // Set member variables.
    pStatus->bEngaged           =
        perfCfControllerMemTuneIsEngaged(pControllerMemTune);
    pStatus->engageCounter      =
        perfCfControllerMemTuneEngageCounterGet(pControllerMemTune);

perfCfClientPerfCfControllerIfaceModel10GetStatus_MEM_TUNE_exit:
    return status;
}
