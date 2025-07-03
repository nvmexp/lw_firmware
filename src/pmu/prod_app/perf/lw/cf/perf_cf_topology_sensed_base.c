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
 * @file    perf_cf_topology_sensed_base.c
 * @copydoc perf_cf_topology_sensed_base.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf.h"
#include "perf/cf/perf_cf_topology_sensed_base.h"

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
perfCfTopologyGrpIfaceModel10ObjSetImpl_SENSED_BASE
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PERF_CF_TOPOLOGY_SENSED_BASE *pDescTopology =
        (RM_PMU_PERF_CF_TOPOLOGY_SENSED_BASE *)pBoardObjDesc;
    PERF_CF_TOPOLOGY_SENSED_BASE   *pTopologySensedBase;
    LwBool                          bFirstConstruct = (*ppBoardObj == NULL);
    FLCN_STATUS                     status          = FLCN_OK;

    status = perfCfTopologyGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto perfCfTopologyGrpIfaceModel10ObjSetImpl_SENSED_BASE_exit;
    }
    pTopologySensedBase = (PERF_CF_TOPOLOGY_SENSED_BASE *)*ppBoardObj;

    if (!bFirstConstruct)
    {
        // We cannot allow subsequent SET calls to change these parameters.
        if ((pTopologySensedBase->sensorIdx     != pDescTopology->sensorIdx)  ||
            (pTopologySensedBase->baseSensorIdx != pDescTopology->baseSensorIdx))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfCfTopologyGrpIfaceModel10ObjSetImpl_SENSED_BASE_exit;
        }
    }

    // Set member variables.
    pTopologySensedBase->sensorIdx      = pDescTopology->sensorIdx;
    pTopologySensedBase->baseSensorIdx  = pDescTopology->baseSensorIdx;

    boardObjGrpMaskBitSet(&(((PERF_CF_TOPOLOGYS *)pBObjGrp)->senseTypeMask),
        BOARDOBJ_GET_GRP_IDX(*ppBoardObj));

    if (!BOARDOBJGRP_IS_VALID(PERF_CF_SENSOR, pTopologySensedBase->sensorIdx) ||
        !BOARDOBJGRP_IS_VALID(PERF_CF_SENSOR, pTopologySensedBase->baseSensorIdx))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto perfCfTopologyGrpIfaceModel10ObjSetImpl_SENSED_BASE_exit;
    }

perfCfTopologyGrpIfaceModel10ObjSetImpl_SENSED_BASE_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfTopologyIfaceModel10GetStatus_SENSED_BASE
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_CF_TOPOLOGY_SENSED_BASE_GET_STATUS *pStatus =
        (RM_PMU_PERF_CF_TOPOLOGY_SENSED_BASE_GET_STATUS *)pPayload;
    PERF_CF_TOPOLOGY_SENSED_BASE *pTopologySensedBase =
        (PERF_CF_TOPOLOGY_SENSED_BASE *)pBoardObj;
    RM_PMU_PERF_CF_SENSOR_BOARDOBJ_GET_STATUS_UNION  *pSensorStatus;
    RM_PMU_PERF_CF_SENSOR_BOARDOBJ_GET_STATUS_UNION  *pBaseSensorStatus;
    LwU64       last;
    LwU64       lwrr;
    LwU64       diff;
    LwU64       base;
    FLCN_STATUS status = FLCN_OK;

    pSensorStatus =
        &((PERF_CF_TOPOLOGYS *)BOARDOBJGRP_GRP_GET(PERF_CF_TOPOLOGY))->pTmpSensorsStatus->sensors[pTopologySensedBase->sensorIdx];
    pBaseSensorStatus =
        &((PERF_CF_TOPOLOGYS *)BOARDOBJGRP_GRP_GET(PERF_CF_TOPOLOGY))->pTmpSensorsStatus->sensors[pTopologySensedBase->baseSensorIdx];

    // Callwlate difference since last sensor reading.
    LwU64_ALIGN32_UNPACK(&last, &pStatus->lastSensorReading);
    LwU64_ALIGN32_UNPACK(&lwrr, &pSensorStatus->sensor.reading);
    lw64Sub(&diff, &lwrr, &last);
    pStatus->lastSensorReading = pSensorStatus->sensor.reading;

    // Callwlate difference since last base sensor reading.
    LwU64_ALIGN32_UNPACK(&last, &pStatus->lastBaseSensorReading);
    LwU64_ALIGN32_UNPACK(&lwrr, &pBaseSensorStatus->sensor.reading);
    lw64Sub(&base, &lwrr, &last);
    pStatus->lastBaseSensorReading = pBaseSensorStatus->sensor.reading;

    // If base sensor has not incremented at all, set it to 1 to avoid divide by 0.
    if (base == 0)
    {
        base = 1;
    }

    lw64Lsr(&lwrr, &diff, 40);
    if (lwrr == 0)
    {
        // Confirmed above that shifting 24 bits would not overflow 64 bits.
        lw64Lsl(&diff, &diff, 24);
        lwU64Div(&pTopologySensedBase->super.tmpReading, &diff, &base);
    }
    else
    {
        //
        // It would take 33+ minutes of PMU counter (at 540MHz) or 19+ hours of PEX
        // counter (at 16GB/s) to overflow 40 bits, so this should never happen,
        // except on first call, where last sensor reading is invalid.
        //
        pTopologySensedBase->super.tmpReading = 0;
    }

    // Call super class function to pack reading into status.
    status = perfCfTopologyIfaceModel10GetStatus_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto perfCfTopologyIfaceModel10GetStatus_SENSED_BASE_exit;
    }

perfCfTopologyIfaceModel10GetStatus_SENSED_BASE_exit:
    return status;
}

/*!
 * @copydoc PerfCfTopologyUpdateSensorsMask
 */
FLCN_STATUS
perfCfTopologyUpdateSensorsMask_SENSED_BASE
(
    PERF_CF_TOPOLOGY       *pTopology,
    BOARDOBJGRPMASK_E32    *pMask
)
{
    PERF_CF_TOPOLOGY_SENSED_BASE *pTopologySensedBase =
        (PERF_CF_TOPOLOGY_SENSED_BASE *)pTopology;

    boardObjGrpMaskBitSet(pMask, pTopologySensedBase->sensorIdx);
    boardObjGrpMaskBitSet(pMask, pTopologySensedBase->baseSensorIdx);

    return FLCN_OK;
}

/* ------------------------ Private Functions ------------------------------- */
