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
 * @file    perf_cf_controller_mem_tune_2x.c
 * @copydoc perf_cf_controller_mem_tune_2x.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "g_lwconfig.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf.h"

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE_2X
(
    BOARDOBJGRP_IFACE_MODEL_10     *pModel10,
    BOARDOBJ       **ppBoardObj,
    LwLength         size,
    RM_PMU_BOARDOBJ *pBoardObjDesc
)
{
    RM_PMU_PERF_CF_CONTROLLER_MEM_TUNE_2X *pDescController =
        (RM_PMU_PERF_CF_CONTROLLER_MEM_TUNE_2X *)pBoardObjDesc;
    PERF_CF_CONTROLLER_MEM_TUNE_2X        *pControllerMemTune2x;
    FLCN_STATUS                            status          = FLCN_OK;

    // Construct and initialize parent-class object.
    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfControllerGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc),
        perfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE_2X_exit);

    pControllerMemTune2x = (PERF_CF_CONTROLLER_MEM_TUNE_2X *)*ppBoardObj;

    // TODO-Chandrashis: Populate remainder of the function.

    (void)pControllerMemTune2x;
    (void)pDescController;

perfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE_2X_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfControllerIfaceModel10GetStatus_MEM_TUNE_2X
(
    BOARDOBJ_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJ *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_CF_CONTROLLER_MEM_TUNE_2X_GET_STATUS *pStatus            =
        (RM_PMU_PERF_CF_CONTROLLER_MEM_TUNE_2X_GET_STATUS *)pPayload;
    PERF_CF_CONTROLLER_MEM_TUNE_2X                   *pControllerMemTune2x =
        (PERF_CF_CONTROLLER_MEM_TUNE_2X *)pBoardObj;
    FLCN_STATUS                                       status             =
        FLCN_OK;

    // Call into the superclass implementation first.
    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfControllerIfaceModel10GetStatus_SUPER(pModel10, pPayload),
        perfCfControllerIfaceModel10GetStatus_MEM_TUNE_2X_exit);

    // TODO-Chandrashis: Populate remainder of the function.

    (void)pControllerMemTune2x;
    (void)pStatus;

perfCfControllerIfaceModel10GetStatus_MEM_TUNE_2X_exit:
    return status;
}

/*!
 * @copydoc PerfCfControllerExelwte
 */
FLCN_STATUS
perfCfControllerExelwte_MEM_TUNE_2X
(
    PERF_CF_CONTROLLER *pController
)
{
    PERF_CF_CONTROLLER_MEM_TUNE_2X         *pControllerMemTune2x =
        (PERF_CF_CONTROLLER_MEM_TUNE_2X *)pController;
    FLCN_STATUS                             status               = FLCN_OK;

    status = perfCfControllerExelwte_SUPER(pController);

    // TODO-Chandrashis: Populate remainder of the function.

    (void)pControllerMemTune2x;

    return status;
}

/*!
 * @copydoc PerfCfControllerSetMultData
 */
FLCN_STATUS
perfCfControllerSetMultData_MEM_TUNE_2X
(
    PERF_CF_CONTROLLER                 *pController,
    PERF_CF_CONTROLLER_MULTIPLIER_DATA *pMultData
)
{
    PERF_CF_CONTROLLER_MEM_TUNE_2X *pControllerMemTune2x =
        (PERF_CF_CONTROLLER_MEM_TUNE_2X *)pController;
    FLCN_STATUS                     status               = FLCN_OK;

    // TODO-Chandrashis: Populate remainder of the function.

    (void)pControllerMemTune2x;

    return status;
}

/*!
 * @copydoc PerfCfControllerLoad
 */
FLCN_STATUS
perfCfControllerLoad_MEM_TUNE_2X
(
    PERF_CF_CONTROLLER *pController
)
{
    PERF_CF_CONTROLLER_MEM_TUNE_2X *pControllerMemTune2x =
        (PERF_CF_CONTROLLER_MEM_TUNE_2X *)pController;
    FLCN_STATUS                     status               = FLCN_OK;

    // TODO-Chandrashis: Populate remainder of the function.

    (void)pControllerMemTune2x;

    return status;
}

/* ------------------------ Private Functions ------------------------------- */
