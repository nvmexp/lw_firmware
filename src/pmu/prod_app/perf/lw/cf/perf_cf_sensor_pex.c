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
 * @file    perf_cf_sensor_pex.c
 * @copydoc perf_cf_sensor_pex.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_sensor_pex.h"
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
perfCfSensorGrpIfaceModel10ObjSetImpl_PEX
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PERF_CF_SENSOR_PEX *pDescSensor =
        (RM_PMU_PERF_CF_SENSOR_PEX *)pBoardObjDesc;
    PERF_CF_SENSOR_PEX *pSensorPex;
    LwBool              bFirstConstruct = (*ppBoardObj == NULL);
    FLCN_STATUS         status          = FLCN_OK;

    status = perfCfSensorGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto perfCfSensorGrpIfaceModel10ObjSetImpl_PEX_exit;
    }
    pSensorPex = (PERF_CF_SENSOR_PEX *)*ppBoardObj;

    if (!bFirstConstruct)
    {
        // We cannot allow subsequent SET calls to change these parameters.
        if (pSensorPex->bRx != pDescSensor->bRx)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfCfSensorGrpIfaceModel10ObjSetImpl_PEX_exit;
        }
    }

    // Set member variables.
    pSensorPex->bRx = pDescSensor->bRx;

perfCfSensorGrpIfaceModel10ObjSetImpl_PEX_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfSensorIfaceModel10GetStatus_PEX
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_CF_SENSOR_PEX_GET_STATUS *pStatus =
        (RM_PMU_PERF_CF_SENSOR_PEX_GET_STATUS *)pPayload;
    PERF_CF_SENSOR_PEX *pSensorPex  = (PERF_CF_SENSOR_PEX *)pBoardObj;
    FLCN_STATUS         status      = FLCN_OK;
    LwU32               pexDiff;

    status = perfCfSensorIfaceModel10GetStatus_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto perfCfSensorIfaceModel10GetStatus_PEX_exit;
    }

    status = pmuPexCntGetDiff_HAL(&Pmu,
        pSensorPex->bRx, &pexDiff, &pSensorPex->last);
    if (status != FLCN_OK)
    {
        goto perfCfSensorIfaceModel10GetStatus_PEX_exit;
    }

    lw64Add32(&pSensorPex->reading, pexDiff);

    pStatus->last = pSensorPex->last;
    LwU64_ALIGN32_PACK(&pStatus->super.reading, &pSensorPex->reading);

perfCfSensorIfaceModel10GetStatus_PEX_exit:
    return status;
}

/*!
 * @copydoc PerfCfSensorLoad
 */
FLCN_STATUS
perfCfSensorLoad_PEX
(
    PERF_CF_SENSOR *pSensor
)
{
    PERF_CF_SENSOR_PEX *pSensorPex = (PERF_CF_SENSOR_PEX *)pSensor;
    FLCN_STATUS status = FLCN_OK;

    status = perfCfSensorLoad_SUPER(pSensor);
    if (status != FLCN_OK)
    {
        goto perfCfSensorLoad_PEX_exit;
    }

    // Apply HW settings.
    pmuPEXCntInit_HAL(&Pmu);

    // Get latest HW reading.
    status = pmuPexCntGetDiff_HAL(&Pmu,
        pSensorPex->bRx, NULL, &pSensorPex->last);
    if (status != FLCN_OK)
    {
        goto perfCfSensorLoad_PEX_exit;
    }

perfCfSensorLoad_PEX_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
