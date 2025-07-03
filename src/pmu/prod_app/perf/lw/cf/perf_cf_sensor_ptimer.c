/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_cf_sensor_ptimer.c
 * @copydoc perf_cf_sensor_ptimer.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_sensor_ptimer.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfCfSensorGrpIfaceModel10ObjSetImpl_PTIMER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    FLCN_STATUS  status;

    status = perfCfSensorGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto perfCfSensorGrpIfaceModel10ObjSetImpl_PTIMER_exit;
    }

    // No member variables.

perfCfSensorGrpIfaceModel10ObjSetImpl_PTIMER_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfSensorIfaceModel10GetStatus_PTIMER
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    RM_PMU_PERF_CF_SENSOR_PTIMER_GET_STATUS *pStatus =
        (RM_PMU_PERF_CF_SENSOR_PTIMER_GET_STATUS *)pPayload;
    FLCN_STATUS status = FLCN_OK;

    status = perfCfSensorIfaceModel10GetStatus_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto perfCfSensorIfaceModel10GetStatus_PTIMER_exit;
    }

    osPTimerTimeNsLwrrentGetUnaligned(&pStatus->super.reading);

perfCfSensorIfaceModel10GetStatus_PTIMER_exit:
    return status;
}

/*!
 * @copydoc PerfCfSensorLoad
 */
FLCN_STATUS
perfCfSensorLoad_PTIMER
(
    PERF_CF_SENSOR *pSensor
)
{
    PERF_CF_SENSOR_PTIMER *pSensorPtimer = (PERF_CF_SENSOR_PTIMER *)pSensor;
    FLCN_STATUS status = FLCN_OK;

    (void)pSensorPtimer;

    status = perfCfSensorLoad_SUPER(pSensor);
    if (status != FLCN_OK)
    {
        goto perfCfSensorLoad_PTIMER_exit;
    }

    // PTIMER class implementation.

perfCfSensorLoad_PTIMER_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
