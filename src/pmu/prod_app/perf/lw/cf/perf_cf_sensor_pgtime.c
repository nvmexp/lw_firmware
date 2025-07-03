/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_cf_sensor_pgtime.c
 * @copydoc perf_cf_sensor_pgtime.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_sensor_pgtime.h"
#include "pmu_objpg.h"

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
perfCfSensorGrpIfaceModel10ObjSetImpl_PGTIME
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PERF_CF_SENSOR_PGTIME *pDescSensor =
        (RM_PMU_PERF_CF_SENSOR_PGTIME *)pBoardObjDesc;
    PERF_CF_SENSOR_PGTIME  *pSensorPgtime;
    LwBool                  bFirstConstruct = (*ppBoardObj == NULL);
    FLCN_STATUS             status          = FLCN_OK;

    status = perfCfSensorGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto perfCfSensorGrpIfaceModel10ObjSetImpl_PGTIME_exit;
    }
    pSensorPgtime = (PERF_CF_SENSOR_PGTIME *)*ppBoardObj;

    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING_PERF_CF))
    {
        if (pDescSensor->pgEngineId >= RM_PMU_LPWR_CTRL_ID_ILWALID_ENGINE)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfCfSensorGrpIfaceModel10ObjSetImpl_PGTIME_exit;
        }
    }

    if (!bFirstConstruct)
    {
        // We cannot allow subsequent SET calls to change these parameters.
        if (pSensorPgtime->pgEngineId != pDescSensor->pgEngineId)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfCfSensorGrpIfaceModel10ObjSetImpl_PGTIME_exit;
        }
    }

    // Set member variables.
    pSensorPgtime->pgEngineId = pDescSensor->pgEngineId;

perfCfSensorGrpIfaceModel10ObjSetImpl_PGTIME_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfSensorIfaceModel10GetStatus_PGTIME
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_CF_SENSOR_PGTIME_GET_STATUS *pStatus =
        (RM_PMU_PERF_CF_SENSOR_PGTIME_GET_STATUS *)pPayload;
    PERF_CF_SENSOR_PGTIME *pSensorPgtime =
        (PERF_CF_SENSOR_PGTIME *)pBoardObj;
    FLCN_STATUS status  = FLCN_OK;

    status = perfCfSensorIfaceModel10GetStatus_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto perfCfSensorIfaceModel10GetStatus_PGTIME_exit;
    }

    // pPgState is NULL when MSCG is disabled.
    if (GET_OBJPGSTATE(pSensorPgtime->pgEngineId) == NULL)
    {
        pStatus->super.reading.lo = 0;
        pStatus->super.reading.hi = 0;

        goto perfCfSensorIfaceModel10GetStatus_PGTIME_exit;
    }

    {
        FLCN_TIMESTAMP timeStamp;

        pgStatLwrrentSleepTimeGetNs(pSensorPgtime->pgEngineId, &timeStamp);
        pStatus->super.reading = timeStamp.parts;
    }

perfCfSensorIfaceModel10GetStatus_PGTIME_exit:
    return status;
}

/*!
 * @copydoc PerfCfSensorLoad
 */
FLCN_STATUS
perfCfSensorLoad_PGTIME
(
    PERF_CF_SENSOR *pSensor
)
{
    PERF_CF_SENSOR_PGTIME *pSensorPgtime =
        (PERF_CF_SENSOR_PGTIME *)pSensor;
    FLCN_STATUS status = FLCN_OK;

    status = perfCfSensorLoad_SUPER(pSensor);
    if (status != FLCN_OK)
    {
        goto perfCfSensorLoad_PGTIME_exit;
    }

    // Only MSCG is supported for now.
    if (LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS != pSensorPgtime->pgEngineId)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto perfCfSensorLoad_PGTIME_exit;
    }

perfCfSensorLoad_PGTIME_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
