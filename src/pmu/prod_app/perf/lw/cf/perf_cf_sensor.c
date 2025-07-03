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
 * @file    perf_cf_sensor.c
 * @copydoc perf_cf_sensor.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf.h"
#include "perf/cf/perf_cf_sensor.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjGrpIfaceModel10SetEntry    (s_perfCfSensorIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfSensorIfaceModel10SetEntry");
static BoardObjGrpIfaceModel10GetStatusHeader   (s_perfCfSensorIfaceModel10GetStatusHeader)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "s_perfCfSensorIfaceModel10GetStatusHeader");
static BoardObjIfaceModel10GetStatus                (s_perfCfSensorIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "s_perfCfSensorIfaceModel10GetStatus");
static PerfCfSensorLoad             (s_perfCfSensorLoad)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfSensorLoad");

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
perfCfSensorBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status = LW_OK;

    status = BOARDOBJGRP_IFACE_MODEL_10_SET(E32,             // _grpType
        PERF_CF_SENSOR,                             // _class
        pBuffer,                                    // _pBuffer
        boardObjGrpIfaceModel10SetHeader,                 // _hdrFunc
        s_perfCfSensorIfaceModel10SetEntry,               // _entryFunc
        turingAndLater.perfCf.sensorGrpSet,         // _ssElement
        OVL_INDEX_DMEM(perfCfTopology),             // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables

    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
perfCfSensorBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status = LW_OK;

    status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS(E32,            // _grpType
        PERF_CF_SENSOR,                             // _class
        pBuffer,                                    // _pBuffer
        s_perfCfSensorIfaceModel10GetStatusHeader,              // _hdrFunc
        s_perfCfSensorIfaceModel10GetStatus,                    // _entryFunc
        turingAndLater.perfCf.sensorGrpGetStatus);  // _ssElement

    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfCfSensorGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    FLCN_STATUS  status;

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto perfCfSensorGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }

    // No member variables.

perfCfSensorGrpIfaceModel10ObjSetImpl_SUPER_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfSensorIfaceModel10GetStatus_SUPER
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_CF_SENSOR_GET_STATUS *pStatus =
        (RM_PMU_PERF_CF_SENSOR_GET_STATUS *)pPayload;
    PERF_CF_SENSOR *pSensor =
        (PERF_CF_SENSOR *)pBoardObj;
    FLCN_STATUS status = FLCN_OK;

    (void)pStatus;
    (void)pSensor;

    // Call super-class impementation;
    status = boardObjIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto perfCfSensorIfaceModel10GetStatus_SUPER_exit;
    }

    // SUPER class specific implementation.

perfCfSensorIfaceModel10GetStatus_SUPER_exit:
    return status;
}

/*!
 * @copydoc PerfCfSensorLoad
 */
FLCN_STATUS
perfCfSensorLoad_SUPER
(
    PERF_CF_SENSOR *pSensor
)
{
    FLCN_STATUS status = FLCN_OK;

    // Base class implementation.

    return status;
}

/*!
 * @copydoc PerfCfSensorLoad
 */
FLCN_STATUS
perfCfSensorsLoad
(
    PERF_CF_SENSORS    *pSensors,
    LwBool              bLoad
)
{
    PERF_CF_SENSOR *pSensor;
    FLCN_STATUS     status = FLCN_OK;
    LwBoardObjIdx   i;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_PERF_CF_TOPOLOGY
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libEngUtil)   // Used by SENSOR_PEX
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf)         // Used by SENSOR_CLKCNTR/PMU_FB (to colwert CLK domain index to API)
        OSTASK_OVL_DESC_DEFINE_PERF_CF_SENSOR_THERM_MONITOR // Used by SENSOR_THERM_MONITOR (to check for valid THRM_MON index)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Nothing to do when there are no SENSOR objects.
        if (boardObjGrpMaskIsZero(&(pSensors->super.objMask)))
        {
            goto perfCfSensorsLoad_exit;
        }

        if (bLoad)
        {
            BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_SENSOR, pSensor, i, NULL)
            {
                status = s_perfCfSensorLoad(pSensor);
                if (status != FLCN_OK)
                {
                    break;
                }
            }
            BOARDOBJGRP_ITERATOR_END;
        }
        else
        {
            // Add unload() actions here.
        }
    }
perfCfSensorsLoad_exit:
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    return status;
}

/*!
 * @copydoc PerfCfSensorsStatusGet
 */
FLCN_STATUS
perfCfSensorsStatusGet
(
    PERF_CF_SENSORS        *pSensors,
    PERF_CF_SENSORS_STATUS *pStatus
)
{
    PERF_CF_SENSOR *pSensor;
    LwBoardObjIdx   i;
    FLCN_STATUS     status = FLCN_OK;

    if (!boardObjGrpMaskIsSubset(&(pStatus->mask), &(pSensors->super.objMask)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfSensorsStatusGet_exit;
    }

    //
    // Sensors should ideally get sampled at the exact same time, but this isn't
    // actually possible, so we would use a critical section to prevent large
    // gaps in time between sensor samplings.
    //
    // The sampling takes a rather long time (measured 35us on fake GP104 with
    // 16 sensors: 2 PEX, 2 PTIMER_CLK, 6 PMU, 1 PMU_FB, 1 PTIMER, 3 CLKCNTR,
    // 1 PGTIME), so to reduce impact to the rest of the system, we are using
    // scheduler suspension instead, allowing IRQ servicing to still occur.
    //
    appTaskSchedulerSuspend();
    {
        BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_SENSOR, pSensor, i, &(pStatus->mask.super))
        {
            BOARDOBJ_IFACE_MODEL_10 *pObjModel10 =
                boardObjIfaceModel10FromBoardObjGet(&pSensor->super);

            status = s_perfCfSensorIfaceModel10GetStatus(pObjModel10,
                                                 &(pStatus->sensors[i].boardObj));
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                break;
            }
        }
        BOARDOBJGRP_ITERATOR_END;
    }
    appTaskSchedulerResume();

perfCfSensorsStatusGet_exit:
    return status;
}

/*!
 * PERF_CF_SENSORS post-init function.
 */
FLCN_STATUS
perfCfSensorsPostInit
(
    PERF_CF_SENSORS *pSensors
)
{
    return FLCN_OK;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_perfCfSensorIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    switch (pBuf->type)
    {
        case LW2080_CTRL_PERF_PERF_CF_SENSOR_TYPE_PMU:
            return perfCfSensorGrpIfaceModel10ObjSetImpl_PMU(pModel10, ppBoardObj,
                        sizeof(PERF_CF_SENSOR_PMU), pBuf);
        case LW2080_CTRL_PERF_PERF_CF_SENSOR_TYPE_PMU_FB:
            return perfCfSensorGrpIfaceModel10ObjSetImpl_PMU_FB(pModel10, ppBoardObj,
                        sizeof(PERF_CF_SENSOR_PMU_FB), pBuf);
        case LW2080_CTRL_PERF_PERF_CF_SENSOR_TYPE_PEX:
            return perfCfSensorGrpIfaceModel10ObjSetImpl_PEX(pModel10, ppBoardObj,
                        sizeof(PERF_CF_SENSOR_PEX), pBuf);
        case LW2080_CTRL_PERF_PERF_CF_SENSOR_TYPE_PTIMER:
            return perfCfSensorGrpIfaceModel10ObjSetImpl_PTIMER(pModel10, ppBoardObj,
                        sizeof(PERF_CF_SENSOR_PTIMER), pBuf);
        case LW2080_CTRL_PERF_PERF_CF_SENSOR_TYPE_PTIMER_CLK:
            return perfCfSensorGrpIfaceModel10ObjSetImpl_PTIMER_CLK(pModel10, ppBoardObj,
                        sizeof(PERF_CF_SENSOR_PTIMER_CLK), pBuf);
        case LW2080_CTRL_PERF_PERF_CF_SENSOR_TYPE_CLKCNTR:
            return perfCfSensorGrpIfaceModel10ObjSetImpl_CLKCNTR(pModel10, ppBoardObj,
                        sizeof(PERF_CF_SENSOR_CLKCNTR), pBuf);
        case LW2080_CTRL_PERF_PERF_CF_SENSOR_TYPE_PGTIME:
            return perfCfSensorGrpIfaceModel10ObjSetImpl_PGTIME(pModel10, ppBoardObj,
                        sizeof(PERF_CF_SENSOR_PGTIME), pBuf);
        case LW2080_CTRL_PERF_PERF_CF_SENSOR_TYPE_THERM_MONITOR:
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_SENSOR_THERM_MONITOR))
        {
            return perfCfSensorGrpIfaceModel10ObjSetImpl_THERM_MONITOR(pModel10, ppBoardObj,
                        sizeof(PERF_CF_SENSOR_THERM_MONITOR), pBuf);
        }
        break;
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10GetStatusHeader
 */
static FLCN_STATUS
s_perfCfSensorIfaceModel10GetStatusHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pBuf,
    BOARDOBJGRPMASK    *pMask
)
{
    BOARDOBJGRP *pBoardObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PERF_CF_SENSOR_BOARDOBJGRP_GET_STATUS_HEADER *pHdr =
        (RM_PMU_PERF_CF_SENSOR_BOARDOBJGRP_GET_STATUS_HEADER *)pBuf;
    PERF_CF_SENSORS *pSensors =
        (PERF_CF_SENSORS *)pBoardObjGrp;

    (void)pHdr;
    (void)pSensors;

    // ...

    return FLCN_OK;
}

/*!
 * @Copydoc BoardObjIfaceModel10GetStatus
 */
static FLCN_STATUS
s_perfCfSensorIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pBuf
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_PERF_PERF_CF_SENSOR_TYPE_PMU:
            return perfCfSensorIfaceModel10GetStatus_PMU(pModel10, pBuf);
        case LW2080_CTRL_PERF_PERF_CF_SENSOR_TYPE_PMU_FB:
            return perfCfSensorIfaceModel10GetStatus_PMU_FB(pModel10, pBuf);
        case LW2080_CTRL_PERF_PERF_CF_SENSOR_TYPE_PEX:
            return perfCfSensorIfaceModel10GetStatus_PEX(pModel10, pBuf);
        case LW2080_CTRL_PERF_PERF_CF_SENSOR_TYPE_PTIMER:
            return perfCfSensorIfaceModel10GetStatus_PTIMER(pModel10, pBuf);
        case LW2080_CTRL_PERF_PERF_CF_SENSOR_TYPE_PTIMER_CLK:
            return perfCfSensorIfaceModel10GetStatus_PTIMER_CLK(pModel10, pBuf);
        case LW2080_CTRL_PERF_PERF_CF_SENSOR_TYPE_CLKCNTR:
            return perfCfSensorIfaceModel10GetStatus_CLKCNTR(pModel10, pBuf);
        case LW2080_CTRL_PERF_PERF_CF_SENSOR_TYPE_PGTIME:
            return perfCfSensorIfaceModel10GetStatus_PGTIME(pModel10, pBuf);
        case LW2080_CTRL_PERF_PERF_CF_SENSOR_TYPE_THERM_MONITOR:
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_SENSOR_THERM_MONITOR))
        {
            return perfCfSensorIfaceModel10GetStatus_THERM_MONITOR(pModel10, pBuf);
        }
        break;
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc PerfCfSensorLoad
 */
static FLCN_STATUS
s_perfCfSensorLoad
(
    PERF_CF_SENSOR *pSensor
)
{
    switch (BOARDOBJ_GET_TYPE(pSensor))
    {
        case LW2080_CTRL_PERF_PERF_CF_SENSOR_TYPE_PMU:
            return perfCfSensorLoad_PMU(pSensor);
        case LW2080_CTRL_PERF_PERF_CF_SENSOR_TYPE_PMU_FB:
            return perfCfSensorLoad_PMU_FB(pSensor);
        case LW2080_CTRL_PERF_PERF_CF_SENSOR_TYPE_PEX:
            return perfCfSensorLoad_PEX(pSensor);
        case LW2080_CTRL_PERF_PERF_CF_SENSOR_TYPE_PTIMER:
            return perfCfSensorLoad_PTIMER(pSensor);
        case LW2080_CTRL_PERF_PERF_CF_SENSOR_TYPE_PTIMER_CLK:
            return perfCfSensorLoad_PTIMER_CLK(pSensor);
        case LW2080_CTRL_PERF_PERF_CF_SENSOR_TYPE_CLKCNTR:
            return perfCfSensorLoad_CLKCNTR(pSensor);
        case LW2080_CTRL_PERF_PERF_CF_SENSOR_TYPE_PGTIME:
            return perfCfSensorLoad_PGTIME(pSensor);
        case LW2080_CTRL_PERF_PERF_CF_SENSOR_TYPE_THERM_MONITOR:
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_SENSOR_THERM_MONITOR))
        {
            return perfCfSensorLoad_THERM_MONITOR(pSensor);
        }
        break;
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}
