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
 * @file    perf_cf_sensor_pmu.c
 * @copydoc perf_cf_sensor_pmu.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_sensor_pmu.h"
#include "pmu_objpmu.h"

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
perfCfSensorGrpIfaceModel10ObjSetImpl_PMU
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PERF_CF_SENSOR_PMU *pDescSensor =
        (RM_PMU_PERF_CF_SENSOR_PMU *)pBoardObjDesc;
    PERF_CF_SENSOR_PMU *pSensorPmu;
    LwBool              bFirstConstruct = (*ppBoardObj == NULL);
    FLCN_STATUS         status          = FLCN_OK;

    status = perfCfSensorGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto perfCfSensorGrpIfaceModel10ObjSetImpl_PMU_exit;
    }
    pSensorPmu = (PERF_CF_SENSOR_PMU *)*ppBoardObj;

    if (!bFirstConstruct)
    {
        // We cannot allow subsequent SET calls to change these parameters.
        if ((pSensorPmu->idleMask0  != pDescSensor->idleMask0)  ||
            (pSensorPmu->idleMask1  != pDescSensor->idleMask1)  ||
            (pSensorPmu->idleMask2  != pDescSensor->idleMask2)  ||
            (pSensorPmu->counterIdx != pDescSensor->counterIdx))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfCfSensorGrpIfaceModel10ObjSetImpl_PMU_exit;
        }
    }

    // Set member variables.
    pSensorPmu->idleMask0   = pDescSensor->idleMask0;
    pSensorPmu->idleMask1   = pDescSensor->idleMask1;
    pSensorPmu->idleMask2   = pDescSensor->idleMask2;
    pSensorPmu->counterIdx  = pDescSensor->counterIdx;

    if (pSensorPmu->counterIdx >= pmuIdleCounterSize_HAL(&Pmu))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto perfCfSensorGrpIfaceModel10ObjSetImpl_PMU_exit;
    }

perfCfSensorGrpIfaceModel10ObjSetImpl_PMU_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfSensorIfaceModel10GetStatus_PMU
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_CF_SENSOR_PMU_GET_STATUS *pStatus =
        (RM_PMU_PERF_CF_SENSOR_PMU_GET_STATUS *)pPayload;
    PERF_CF_SENSOR_PMU *pSensorPmu = (PERF_CF_SENSOR_PMU *)pBoardObj;
    FLCN_STATUS status      = FLCN_OK;
    LwU32       idleDiff;

    status = perfCfSensorIfaceModel10GetStatus_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto perfCfSensorIfaceModel10GetStatus_PMU_exit;
    }

    status = pmuIdleCounterDiff_HAL(&Pmu,
        pSensorPmu->counterIdx, &idleDiff, &pSensorPmu->last);
    if (status != FLCN_OK)
    {
        goto perfCfSensorIfaceModel10GetStatus_PMU_exit;
    }

    lw64Add32(&pSensorPmu->reading, idleDiff);

    pStatus->last = pSensorPmu->last;
    LwU64_ALIGN32_PACK(&pStatus->super.reading, &pSensorPmu->reading);

perfCfSensorIfaceModel10GetStatus_PMU_exit:
    return status;
}

/*!
 * @copydoc PerfCfSensorLoad
 */
FLCN_STATUS
perfCfSensorLoad_PMU
(
    PERF_CF_SENSOR *pSensor
)
{
    PERF_CF_SENSOR_PMU *pSensorPmu = (PERF_CF_SENSOR_PMU *)pSensor;
    FLCN_STATUS status = FLCN_OK;

    status = perfCfSensorLoad_SUPER(pSensor);
    if (status != FLCN_OK)
    {
        goto perfCfSensorLoad_PMU_exit;
    }

    // Apply HW settings.
    status = pmuIdleCounterInit_HAL(&Pmu,
        pSensorPmu->counterIdx,
        pSensorPmu->idleMask0,
        pSensorPmu->idleMask1,
        pSensorPmu->idleMask2);
    if (status != FLCN_OK)
    {
        goto perfCfSensorLoad_PMU_exit;
    }

    // Get latest HW reading.
    status = pmuIdleCounterDiff_HAL(&Pmu,
        pSensorPmu->counterIdx, NULL, &pSensorPmu->last);
    if (status != FLCN_OK)
    {
        goto perfCfSensorLoad_PMU_exit;
    }

perfCfSensorLoad_PMU_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
