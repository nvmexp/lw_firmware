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
 * @file    perf_cf_topology_sensed.c
 * @copydoc perf_cf_topology_sensed.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf.h"
#include "perf/cf/perf_cf_topology_sensed.h"

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
perfCfTopologyGrpIfaceModel10ObjSetImpl_SENSED
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PERF_CF_TOPOLOGY_SENSED *pDescTopology =
        (RM_PMU_PERF_CF_TOPOLOGY_SENSED *)pBoardObjDesc;
    PERF_CF_TOPOLOGY_SENSED *pTopologySensed;
    LwBool bFirstConstruct = (*ppBoardObj == NULL);
    FLCN_STATUS status = FLCN_OK;

    PMU_HALT_OK_OR_GOTO(status,
        (pBObjGrp != NULL) &&
        (ppBoardObj != NULL) &&
        (pBoardObjDesc != NULL) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        perfCfTopologyGrpIfaceModel10ObjSetImpl_SENSED_exit);

    PMU_HALT_OK_OR_GOTO(status,
        perfCfTopologyGrpIfaceModel10ObjSetImpl_SUPER(
            pModel10,
            ppBoardObj,
            size,
            pBoardObjDesc),
        perfCfTopologyGrpIfaceModel10ObjSetImpl_SENSED_exit);

    pTopologySensed = (PERF_CF_TOPOLOGY_SENSED *)*ppBoardObj;

    // We cannot allow subsequent SET calls to change these parameters.
    PMU_HALT_OK_OR_GOTO(status,
        bFirstConstruct ||
        (pTopologySensed->sensorIdx == pDescTopology->sensorIdx) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        perfCfTopologyGrpIfaceModel10ObjSetImpl_SENSED_exit);

    // Set member variables.
    pTopologySensed->sensorIdx = pDescTopology->sensorIdx;

    boardObjGrpMaskBitSet(&(((PERF_CF_TOPOLOGYS *)pBObjGrp)->senseTypeMask),
        BOARDOBJ_GET_GRP_IDX(*ppBoardObj));

    PMU_HALT_OK_OR_GOTO(status,
        BOARDOBJGRP_IS_VALID(PERF_CF_SENSOR, pTopologySensed->sensorIdx) ?
            FLCN_OK : FLCN_ERR_ILWALID_INDEX,
        perfCfTopologyGrpIfaceModel10ObjSetImpl_SENSED_exit);

perfCfTopologyGrpIfaceModel10ObjSetImpl_SENSED_exit:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
perfCfTopologyIfaceModel10GetStatus_SENSED
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    const PERF_CF_TOPOLOGYS *const pTopologys = PERF_CF_GET_TOPOLOGYS();
    RM_PMU_PERF_CF_TOPOLOGY_SENSED_GET_STATUS *pStatus =
        (RM_PMU_PERF_CF_TOPOLOGY_SENSED_GET_STATUS *)pPayload;
    PERF_CF_TOPOLOGY_SENSED *pTopologySensed =
        (PERF_CF_TOPOLOGY_SENSED *)pBoardObj;
    const RM_PMU_PERF_CF_SENSOR_BOARDOBJ_GET_STATUS_UNION *pSensorStatus;
    LwU64       last;
    LwU64       lwrr;
    LwU64       diff;
    LwU64       upper24;
    FLCN_STATUS status = FLCN_OK;

    PMU_HALT_OK_OR_GOTO(status,
        (pBoardObj != NULL) &&
        (pPayload != NULL) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        perfCfTopologyIfaceModel10GetStatus_SENSED_exit);

    pSensorStatus =
        &pTopologys->pTmpSensorsStatus->sensors[pTopologySensed->sensorIdx];

    // Callwlate difference since last sensor reading.
    LwU64_ALIGN32_UNPACK(&last, &pStatus->lastSensorReading);
    LwU64_ALIGN32_UNPACK(&lwrr, &pSensorStatus->sensor.reading);
    lw64Sub(&diff, &lwrr, &last);
    pStatus->lastSensorReading = pSensorStatus->sensor.reading;

    lw64Lsr(&upper24, &diff, 40);
    if (upper24 == 0U)
    {
        // If the upper 24 bits are 0, then it's safe to shift left 24 bits.
        lw64Lsl(&diff, &diff, 24);
        pTopologySensed->super.tmpReading = diff;
    }
    else
    {
        //
        // It would take ~18 hours for a PTIMER based sensor to overflow 40
        // bits, so this "shouldn't" happen. If it does, then return the max
        // integer value. (This may also happen on the first call, but this
        // value is invalid in that case.)
        //
        pTopologySensed->super.tmpReading = LW_TYPES_UFXP_INTEGER_MAX(40, 24);
    }

    // Call super class function to pack reading into status.
    PMU_HALT_OK_OR_GOTO(status,
        perfCfTopologyIfaceModel10GetStatus_SUPER(pModel10, pPayload),
        perfCfTopologyIfaceModel10GetStatus_SENSED_exit);

perfCfTopologyIfaceModel10GetStatus_SENSED_exit:
    return status;
}

/*!
 * @copydoc PerfCfTopologyUpdateSensorsMask
 */
FLCN_STATUS
perfCfTopologyUpdateSensorsMask_SENSED
(
    PERF_CF_TOPOLOGY       *pTopology,
    BOARDOBJGRPMASK_E32    *pMask
)
{
    FLCN_STATUS status;
    PERF_CF_TOPOLOGY_SENSED *pTopologySensed =
        (PERF_CF_TOPOLOGY_SENSED *)pTopology;

    PMU_HALT_OK_OR_GOTO(status,
        (pTopology != NULL) &&
        (pMask != NULL) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        perfCfTopologyUpdateSensorsMask_SENSED_exit);

    boardObjGrpMaskBitSet(pMask, pTopologySensed->sensorIdx);

perfCfTopologyUpdateSensorsMask_SENSED_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
