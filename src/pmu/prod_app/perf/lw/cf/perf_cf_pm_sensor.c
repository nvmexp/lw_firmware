/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_cf_pm_sensor.c
 * @copydoc perf_cf_pm_sensor.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

#include "dev_pwr_csb.h"
#include "dev_therm.h"
#include "dev_chiplet_pwr.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_cg.h"
#include "pmu_objlpwr.h"
#include "pmu_objfifo.h"
#include "perf/cf/perf_cf.h"
#include "perf/cf/perf_cf_pm_sensor.h"
#include "config/g_perf_cf_private.h"
#include "perf/cf/perf_cf_pm_sensors_pmumon.h"
#include "perf/cf/perf_cf_pm_sensor_clw_v10.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjGrpIfaceModel10SetHeader       (s_perfCfPmSensorIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfPmSensorIfaceModel10SetHeader");
static BoardObjGrpIfaceModel10SetEntry        (s_perfCfPmSensorIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfPmSensorIfaceModel10SetEntry");
static BoardObjGrpIfaceModel10GetStatusHeader       (s_perfCfPmSensorIfaceModel10GetStatusHeader)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorIfaceModel10GetStatusHeader");
static BoardObjIfaceModel10GetStatus                    (s_perfCfPmSensorIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorIfaceModel10GetStatus");
static PerfCfPmSensorLoad               (s_perfCfPmSensorLoad)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfPmSensorLoad");
static FLCN_STATUS                      s_perfCfPmSensorsSnap(PERF_CF_PM_SENSORS *pPmSensors)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "perfCfPmSensorsSnap");
static FLCN_STATUS                           s_perfCfPmSensorsSnapElcgWarEngage(PERF_CF_PM_SENSORS *pPmSensors, LwBool *pbElcgArmed)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "perfCfPmSensorsSnap");
static FLCN_STATUS                           s_perfCfPmSensorsSnapElcgWarDisengage(PERF_CF_PM_SENSORS *pPmSensors, LwBool bElcgArmed)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "perfCfPmSensorsSnap");
static PerfCfPmSensorSnap               (s_perfCfPmSensorSnap)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorSnap");
static PerfCfPmSensorSignalsUpdate      (s_perfCfPmSensorSignalsUpdate)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorSignalsUpdate");

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
perfCfPmSensorBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status = LW_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
         OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfCfPmSensors)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        PMU_HALT_OK_OR_GOTO(status,
            BOARDOBJGRP_IFACE_MODEL_10_SET(E32,                      // _grpType
                PERF_CF_PM_SENSOR,                          // _class
                pBuffer,                                    // _pBuffer
                s_perfCfPmSensorIfaceModel10SetHeader,            // _hdrFunc
                s_perfCfPmSensorIfaceModel10SetEntry,             // _entryFunc
                ga10xAndLater.perfCf.pmSensorGrpSet,        // _ssElement
                OVL_INDEX_DMEM(perfCfPmSensors),            // _ovlIdxDmem
                BoardObjGrpVirtualTablesNotImplemented),    // _ppObjectVtables
            perfCfPmSensorBoardObjGrpIfaceModel10Set_exit);

    // Trigger Perf CF PmSensors sanity check.
    status = perfCfPmSensorsSanityCheck_HAL(&PerfCf);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfCfPmSensorBoardObjGrpIfaceModel10Set_exit;
    }

perfCfPmSensorBoardObjGrpIfaceModel10Set_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
perfCfPmSensorBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status = LW_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_PERF_CF_PM_SENSORS
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS(E32,            // _grpType
            PERF_CF_PM_SENSOR,                          // _class
            pBuffer,                                    // _pBuffer
            s_perfCfPmSensorIfaceModel10GetStatusHeader,            // _hdrFunc
            s_perfCfPmSensorIfaceModel10GetStatus,                  // _entryFunc
            ga10xAndLater.perfCf.pmSensorGrpGetStatus); // _ssElement
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfCfPmSensorGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PERF_CF_PM_SENSOR *pDescSensor =
        (RM_PMU_PERF_CF_PM_SENSOR *)pBoardObjDesc;
    PERF_CF_PM_SENSOR   *pPmSensor;
    LwBool               bFirstConstruct  = (*ppBoardObj == NULL);
    FLCN_STATUS          status;
    LwU32                tmpNumSignals;
    BOARDOBJGRPMASK_E1024 tmpMask;

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto perfCfPmSensorGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }

    pPmSensor = (PERF_CF_PM_SENSOR *)*ppBoardObj;

    if (bFirstConstruct)
    {
        // Init and copy in the mask.
        boardObjGrpMaskInit_E1024(&(pPmSensor->signalsSupportedMask));
        status = boardObjGrpMaskImport_E1024(
                 &(pPmSensor->signalsSupportedMask),
                 &(pDescSensor->signalsSupportedMask));
        if (status != FLCN_OK)
        {
            goto perfCfPmSensorGrpIfaceModel10ObjSetImpl_SUPER_exit;
        }

        // Allocate signal memory based on the number of signals supported
        tmpNumSignals = boardObjGrpMaskBitIdxHighest(
                            &(pPmSensor->signalsSupportedMask)) + 1U;
        pPmSensor->pSignals = lwosCallocType(OVL_INDEX_DMEM(perfCfPmSensors),
                                  tmpNumSignals, LwU64);
    }
    else
    {
        boardObjGrpMaskInit_E1024(&tmpMask);
        status = boardObjGrpMaskImport_E1024(
                    &tmpMask,
                    &(pDescSensor->signalsSupportedMask));
        if (!(boardObjGrpMaskIsEqual(&(tmpMask),
                &(pPmSensor->signalsSupportedMask))))
        {
            PMU_BREAKPOINT();
            goto perfCfPmSensorGrpIfaceModel10ObjSetImpl_SUPER_exit;
        }
    }

perfCfPmSensorGrpIfaceModel10ObjSetImpl_SUPER_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfPmSensorIfaceModel10GetStatus_SUPER
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_CF_PM_SENSOR_GET_STATUS *pStatus =
        (RM_PMU_PERF_CF_PM_SENSOR_GET_STATUS *)pPayload;
    PERF_CF_PM_SENSOR *pPmSensor =
        (PERF_CF_PM_SENSOR *)pBoardObj;
    FLCN_STATUS status = FLCN_OK;
    BOARDOBJGRPMASK_E1024 tmpMask;
    LwBoardObjIdx        idx;
    LwU64                cntDiff;
    LwU64                cntLast;

    // Call super-class impementation;
    status = boardObjIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto perfCfPmSensorIfaceModel10GetStatus_SUPER_exit;
    }

    //
    // SUPER class specific implementation.
    // Get the requested signal mask and ensure it is a subset of
    // the signalsSupportedMask.
    //
    boardObjGrpMaskInit_E1024(&tmpMask);
    status = boardObjGrpMaskImport_E1024(
                &tmpMask,
                &(pStatus->signalsMask));

    if (!boardObjGrpMaskIsSubset(&(tmpMask), &(pPmSensor->signalsSupportedMask)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfPmSensorIfaceModel10GetStatus_SUPER_exit;
    }

    BOARDOBJGRPMASK_FOR_EACH_BEGIN(&tmpMask, idx)
    {
        LwU64_ALIGN32_UNPACK(&cntLast,
            &pStatus->signals[idx].cntLast);

        lw64Sub(&cntDiff, &pPmSensor->pSignals[idx], &cntLast);

        LwU64_ALIGN32_PACK(&pStatus->signals[idx].cntLast,
                               &pPmSensor->pSignals[idx]);
        LwU64_ALIGN32_PACK(&pStatus->signals[idx].cntDiff, &cntDiff);
    }
    BOARDOBJGRPMASK_FOR_EACH_END;

perfCfPmSensorIfaceModel10GetStatus_SUPER_exit:
    return status;
}

/*!
 * @copydoc PerfCfPmSensorLoad
 */
FLCN_STATUS
perfCfPmSensorLoad_SUPER
(
    PERF_CF_PM_SENSOR *pPmSensor
)
{
    FLCN_STATUS status = FLCN_OK;

    // Base class implementation.

    return status;
}

/*!
 * @copydoc PerfCfPmSensorLoad
 */
FLCN_STATUS
perfCfPmSensorsLoad
(
    PERF_CF_PM_SENSORS *pPmSensors,
    LwBool              bLoad
)
{
    PERF_CF_PM_SENSOR *pPmSensor;
    FLCN_STATUS        status = FLCN_OK;
    LwBoardObjIdx      i;
    OSTASK_OVL_DESC    ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_PERF_CF_PM_SENSORS
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Nothing to do when there are no PM SENSOR objects.
        if (boardObjGrpMaskIsZero(&(pPmSensors->super.objMask)))
        {
            goto perfCfPmSensorsLoad_exit;
        }

        if (bLoad)
        {
            BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_PM_SENSOR, pPmSensor, i, NULL)
            {
                status = s_perfCfPmSensorLoad(pPmSensor);
                if (status != FLCN_OK)
                {
                    break;
                }
            }
            BOARDOBJGRP_ITERATOR_END;
            
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSORS_PMUMON))
            {
                PMU_ASSERT_OK_OR_GOTO(status,
                    perfCfPmSensorsPmumonLoad(),
                    perfCfPmSensorsLoad_exit);
            }

            // CLW/OOBRC/PMA init
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR_CLW_V10))
            {
                perfCfPmSensorsClwV10Load();
            }
        }
        else
        {
            // Add unload() actions here.
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSORS_PMUMON))
            {
                perfCfPmSensorsPmumonUnload();
            }
        }
    }

perfCfPmSensorsLoad_exit:
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    return status;
}

/*!
 * @copydoc PerfCfPmSensorsStatusGet
 */
FLCN_STATUS
perfCfPmSensorsStatusGet
(
    PERF_CF_PM_SENSORS        *pPmSensors,
    PERF_CF_PM_SENSORS_STATUS *pStatus
)
{
    FLCN_STATUS        status;
    PERF_CF_PM_SENSOR *pPmSensor;
    LwBoardObjIdx      i;

    if (!boardObjGrpMaskIsSubset(&(pStatus->mask), &(pPmSensors->super.objMask)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfPmSensorsStatusGet_exit;
    }

    // Trigger BA PM count
    PMU_HALT_OK_OR_GOTO(status,
        s_perfCfPmSensorsSnap(pPmSensors),
        perfCfPmSensorsStatusGet_exit);

    BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_PM_SENSOR, pPmSensor, i, &(pStatus->mask.super))
    {
        BOARDOBJ_IFACE_MODEL_10 *pObjModel10 =
            boardObjIfaceModel10FromBoardObjGet(&pPmSensor->super);

        status = s_perfCfPmSensorIfaceModel10GetStatus(pObjModel10,
                                             &(pStatus->pmSensors[i].boardObj));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            break; // NJ??
        }
    }
    BOARDOBJGRP_ITERATOR_END;

perfCfPmSensorsStatusGet_exit:
    return status;
}

/*!
 * PERF_CF_PM_SENSORS post-init function.
 */
FLCN_STATUS
perfCfPmSensorsPostInit
(
    PERF_CF_PM_SENSORS *pPmSensors
)
{
    PERF_CF_PM_SENSORS_SNAP_ELCG_WAR *pSnapElcgWar;
    FLCN_STATUS status;

    //
    // Init SNAP ELCG WAR to not enabled.  Individual
    // PERF_CF_PM_SENSORs will opt-in, if required.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfPmSensorsSnapElcgWarGet(pPmSensors, &pSnapElcgWar),
        perfCfPmSensorsPostInit_exit);
    if (pSnapElcgWar != NULL)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfPmSensorsSnapElcgWarInit(pSnapElcgWar),
            perfCfPmSensorsPostInit_exit);
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSORS_PMUMON))
    {
        status = perfCfPmSensorsPmumonConstruct();
        if (status != FLCN_OK)
        {
            goto perfCfPmSensorsPostInit_exit;
        }
    }
    
perfCfPmSensorsPostInit_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @coypdoc BoardObjGrpIfaceModel10SetHeader
 */
static FLCN_STATUS
s_perfCfPmSensorIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    PERF_CF_PM_SENSORS_SNAP_ELCG_WAR *pSnapElcgWar;
    PERF_CF_PM_SENSORS *pPmSensors = (PERF_CF_PM_SENSORS *)pBObjGrp;
    FLCN_STATUS status;

    // Call SUPER implementation.
    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc),
        s_perfCfPmSensorIfaceModel10SetHeader_exit);

    //
    // Init SNAP ELCG WAR to not enabled.  Individual
    // PERF_CF_PM_SENSORs will opt-in, if required.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfPmSensorsSnapElcgWarGet(pPmSensors, &pSnapElcgWar),
        s_perfCfPmSensorIfaceModel10SetHeader_exit);
    if (pSnapElcgWar != NULL)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfPmSensorsSnapElcgWarInit(pSnapElcgWar),
            s_perfCfPmSensorIfaceModel10SetHeader_exit);
    }

s_perfCfPmSensorIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_perfCfPmSensorIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    switch (pBuf->type)
    {
        case LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_V00:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR_V00))
            {
                return perfCfPmSensorGrpIfaceModel10ObjSetImpl_V00(pModel10, ppBoardObj,
                            sizeof(PERF_CF_PM_SENSOR_V00), pBuf);
            }
        case LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_V10:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR_V10))
            {
                return perfCfPmSensorGrpIfaceModel10ObjSetImpl_V10(pModel10, ppBoardObj,
                            sizeof(PERF_CF_PM_SENSOR_V10), pBuf);
            }
        case LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_DEV_V10:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR_CLW_DEV_V10))
            {
                return perfCfPmSensorGrpIfaceModel10ObjSetImpl_CLW_DEV_V10(pModel10, ppBoardObj,
                            sizeof(PERF_CF_PM_SENSOR_CLW_DEV_V10), pBuf);
            }
        case LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_MIG_V10:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR_CLW_MIG_V10))
            {
                return perfCfPmSensorGrpIfaceModel10ObjSetImpl_CLW_MIG_V10(pModel10, ppBoardObj,
                            sizeof(PERF_CF_PM_SENSOR_CLW_MIG_V10), pBuf);
            }
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10GetStatusHeader
 */
static FLCN_STATUS
s_perfCfPmSensorIfaceModel10GetStatusHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pBuf,
    BOARDOBJGRPMASK    *pMask
)
{
    BOARDOBJGRP *pBoardObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PERF_CF_PM_SENSOR_BOARDOBJGRP_GET_STATUS_HEADER *pHdr =
        (RM_PMU_PERF_CF_PM_SENSOR_BOARDOBJGRP_GET_STATUS_HEADER *)pBuf;
    PERF_CF_PM_SENSORS *pPmSensors =
        (PERF_CF_PM_SENSORS *)pBoardObjGrp;
    FLCN_STATUS         status;

    (void)pHdr;

    // Trigger BA PM count
    PMU_HALT_OK_OR_GOTO(status,
        s_perfCfPmSensorsSnap(pPmSensors),
        s_perfCfPmSensorIfaceModel10GetStatusHeader_exit);

s_perfCfPmSensorIfaceModel10GetStatusHeader_exit:
    return status;
}

/*!
 * @Copydoc BoardObjIfaceModel10GetStatus
 */
static FLCN_STATUS
s_perfCfPmSensorIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pBuf
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_V00:
             if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR_V00))
             {
                 return perfCfPmSensorIfaceModel10GetStatus_V00(pModel10, pBuf);
             }
        case LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_V10:
             if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR_V10))
             {
                 return perfCfPmSensorIfaceModel10GetStatus_V10(pModel10, pBuf);
             }
        case LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_DEV_V10:
             if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR_CLW_DEV_V10))
             {
                 return perfCfPmSensorIfaceModel10GetStatus_CLW_DEV_V10(pModel10, pBuf);
             }
        case LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_MIG_V10:
             if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR_CLW_MIG_V10))
             {
                 return perfCfPmSensorIfaceModel10GetStatus_CLW_MIG_V10(pModel10, pBuf);
             }
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc PerfCfPmSensorLoad
 */
static FLCN_STATUS
s_perfCfPmSensorLoad
(
    PERF_CF_PM_SENSOR *pPmSensor
)
{
    switch (BOARDOBJ_GET_TYPE(pPmSensor))
    {
        case LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_V00:
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR_V00))
        {
            return perfCfPmSensorLoad_V00(pPmSensor);
        }
        case LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_V10:
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR_V10))
        {
            return perfCfPmSensorLoad_V10(pPmSensor);
        }
        case LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_DEV_V10:
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR_CLW_DEV_V10))
        {
            return perfCfPmSensorLoad_CLW_DEV_V10(pPmSensor);
        }
        case LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_MIG_V10:
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR_CLW_MIG_V10))
        {
            return perfCfPmSensorLoad_CLW_MIG_V10(pPmSensor);
        }
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @brief Trigger BA PM data collection of all PM sensors
 *
 * @param[in] pPmSensors    PM sensors object
 *
 * @return FLCN_OK
 *            If the function is supported on GPU
 *         FLCN_ERR_NOT_SUPPORTED
 *            If the function is not supported on GPU
 */
static FLCN_STATUS
s_perfCfPmSensorsSnap
(
    PERF_CF_PM_SENSORS *pPmSensors
)
{
    PERF_CF_PM_SENSORS_SNAP_ELCG_WAR *pSnapElcgWar = NULL;
    PERF_CF_PM_SENSOR                *pPmSensor;
    FLCN_STATUS        status = FLCN_OK;
    LwBoardObjIdx      i;

    // Fetch Snap ELCG WAR state if applicable.
    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfPmSensorsSnapElcgWarGet(pPmSensors, &pSnapElcgWar),
        s_perfCfPmSensorsSnap_exit);

    //Clear CLW snap mask
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR_CLW_V10))
    {
        perfCfPmSensorClwV10SnapMaskClear();
    }

    appTaskCriticalEnter();
    {
        LwBool bElcgArmedRestore = LW_FALSE;

        //
        // If SNAP ELCG WAR is supported and enabled, engage it before
        // issuing SNAP to all PERF_CF_PM_SENSORs.
        //
        if ((pSnapElcgWar != NULL) &&
            pSnapElcgWar->bEnabled)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                s_perfCfPmSensorsSnapElcgWarEngage(pPmSensors, &bElcgArmedRestore),
                s_perfCfPmSensorsSnap_exit_critical);
        }

        BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_PM_SENSOR, pPmSensor, i, NULL)
        {
            //
            // Cannot "goto _exit;" here.  Must ensure that will
            // always restore ELCG state, even on error.
            //
            PMU_ASSERT_OK_OR_GOTO(status,
                s_perfCfPmSensorSnap(pPmSensor),
                s_perfCfPmSensorsSnap_elcg_restore);
        }
        BOARDOBJGRP_ITERATOR_END;

        //snap CLW signals here if mask is set
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR_CLW_V10) &&
            perfCfPmSensorClwV10SnapMaskIsSet())
        {
            perfCfPmSensorClwV10Snap();
        }

s_perfCfPmSensorsSnap_elcg_restore:
        //
        // If SNAP ELCG WAR is supported and enabled, disengage it after
        // issuing SNAP to all PERF_CF_PM_SENSORs.
        //
       if ((pSnapElcgWar != NULL) &&
            pSnapElcgWar->bEnabled)
        {
            PMU_ASSERT_OK_OR_CAPTURE_FIRST_ERROR(status,
                s_perfCfPmSensorsSnapElcgWarDisengage(pPmSensors, bElcgArmedRestore));
        }

s_perfCfPmSensorsSnap_exit_critical:
       lwosNOP();
    }
    appTaskCriticalExit();
    // If error oclwrred during critical section above, must bail out now.
    if (status != FLCN_OK)
    {
        goto s_perfCfPmSensorsSnap_exit;
    }

    //wait for CLW SNAP to complete if mask is set
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR_CLW_V10) &&
        perfCfPmSensorClwV10SnapMaskIsSet())
    {
        PMU_ASSERT_OK_OR_GOTO(status, perfCfPmSensorClwV10WaitSnapComplete(),
                s_perfCfPmSensorsSnap_exit);
    }

    // Update sw signal count since the signals have been snapped.
    BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_PM_SENSOR, pPmSensor, i, NULL)
    {
        status = s_perfCfPmSensorSignalsUpdate(pPmSensor);
        if (status != FLCN_OK)
        {
            //
            // TODO - Switch this to PMU_ASSERT_OK_OR_GOTO() to bail
            // out.
            //
            PMU_BREAKPOINT();
            break;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

s_perfCfPmSensorsSnap_exit:
    return status;
}

/*!
 * @brief Helper function to engage the ELCG WAR.  Will save current
 * GR-ELCG enablement state, disable GR-ELCG, and then issue a dummy
 * read to GPC to ensure that GPCCLK is compeltely ungated.
 *
 * @param[in]  pPmSensors    Pointer to PERF_CF_PM_SENSORS object
 * @param[out] pbElcgArmed
 *     Pointer in which to return current/previous GR-ECLG enablement
 *     state to be restored when WAR is disabled.
 *
 * @return FLCN_OK
 *     WAR successfully engaged
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Incorrect pointer parameters (i.e. NULL) provided.
 * @return FLCN_ERR_ILWALID_STATE
 *     GR-ELCG is in _STOP state, which WAR does not support.
 */
static FLCN_STATUS
s_perfCfPmSensorsSnapElcgWarEngage
(
    PERF_CF_PM_SENSORS *pPmSensors,
    LwBool             *pbElcgArmed
)
{
    FLCN_STATUS status;

    // NULL-check parameters.
    PMU_ASSERT_OK_OR_GOTO(status,
       (((pPmSensors != NULL) &&
         (pbElcgArmed != NULL)) ? FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT),
        s_perfCfPmSensorsSnapElcgWarEngage_exit);

    //
    // Read the GR-ELCG control register.
    // Todo aman: need to fix once hardware mannual are updated
    //
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_AD10B_PLUS))
    LwU32       gateCtrl;

    gateCtrl = THERM_GATE_CTRL_REG_RD32(GET_FIFO_ENG(PMU_ENGINE_GR));
    //
    // Sanity check that ELCG state is not _STOP mode.  This
    // implementation does not support that.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        (!FLD_TEST_DRF(_CPWR_THERM, _GATE_CTRL, _ENG_CLK,
            _STOP, gateCtrl) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        s_perfCfPmSensorsSnapElcgWarEngage_exit);
    //
    // Return whether GR-ELCG is lwrrently armed back to caller for
    // @ref s_perfCfPmSensorsSnapElcgWarDisengage().
    //
    *pbElcgArmed = FLD_TEST_DRF(_CPWR_THERM, _GATE_CTRL, _ENG_CLK,
                                _AUTO, gateCtrl);
    // Explicitly turn off GR-ELCG,
    gateCtrl = FLD_SET_DRF(_CPWR_THERM, _GATE_CTRL, _ENG_CLK,
                           _RUN, gateCtrl);
    THERM_GATE_CTRL_REG_WR32(GET_FIFO_ENG(PMU_ENGINE_GR), gateCtrl);

    // Insert a dummy PRIV read to GR to ensure that GPCCLK is toggling.
    (void)REG_RD32(BAR0, LW_PCHIPLET_PWR_GPCS_CONFIG_1);

#endif // (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_AD10B_PLUS))

s_perfCfPmSensorsSnapElcgWarEngage_exit:
    return status;
}

/*!
 * @brief Helper function to disengage the ELCG WAR.  Will restore the
 * previous current GR-ELCG enablement state, which should have been
 * sampled via @ref s_perfCfPmSensorsSnapElcgWarEngage().
 *
 * @param[in]  pPmSensors    Pointer to PERF_CF_PM_SENSORS object
 * @param[out] bElcgArmed
 *     Previous GR-ECLG enablement state to restore.
 *
 * @return FLCN_OK
 *     WAR successfully disengaged
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Incorrect pointer parameters (i.e. NULL) provided.
 */
static FLCN_STATUS
s_perfCfPmSensorsSnapElcgWarDisengage
(
    PERF_CF_PM_SENSORS *pPmSensors,
    LwBool              bElcgArmed
)
{
    FLCN_STATUS status;

    // NULL-check parameters.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pPmSensors != NULL) ? FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT),
        s_perfCfPmSensorsSnapElcgWarDisengage_exit);

    //
    // Read the GR-ELCG control register.
    // Todo aman: need to fix once hardware mannual are updated
    //
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_AD10B_PLUS))
    LwU32       gateCtrl;
    
    gateCtrl = THERM_GATE_CTRL_REG_RD32(GET_FIFO_ENG(PMU_ENGINE_GR));
    if (bElcgArmed)
    {
        gateCtrl = FLD_SET_DRF(_CPWR_THERM, _GATE_CTRL, _ENG_CLK,
                               _AUTO, gateCtrl);
    }
    else
    {
        gateCtrl = FLD_SET_DRF(_CPWR_THERM, _GATE_CTRL, _ENG_CLK,
                               _RUN, gateCtrl);
    }
    THERM_GATE_CTRL_REG_WR32(GET_FIFO_ENG(PMU_ENGINE_GR), gateCtrl);

#endif // (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_AD10B_PLUS))

s_perfCfPmSensorsSnapElcgWarDisengage_exit:
    return status;
}

/*!
 * @copydoc PerfCfPmSensorSnap
 */
static FLCN_STATUS
s_perfCfPmSensorSnap
(
    PERF_CF_PM_SENSOR *pPmSensor
)
{
    switch (BOARDOBJ_GET_TYPE(pPmSensor))
    {
        case LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_V00:
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR_V00))
        {
            return perfCfPmSensorSnap_V00(pPmSensor);
        }
        case LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_V10:
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR_V10))
        {
            return perfCfPmSensorSnap_V10(pPmSensor);
        }
        case LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_DEV_V10:
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR_CLW_DEV_V10))
        {
            return perfCfPmSensorSnap_CLW_DEV_V10(pPmSensor);
        }
        case LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_MIG_V10:
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR_CLW_MIG_V10))
        {
            return perfCfPmSensorSnap_CLW_MIG_V10(pPmSensor);
        }
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc PerfCfPmSensorSignalsUpdate
 */
static FLCN_STATUS
s_perfCfPmSensorSignalsUpdate
(
    PERF_CF_PM_SENSOR *pPmSensor
)
{
    switch (BOARDOBJ_GET_TYPE(pPmSensor))
    {
        case LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_V00:
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR_V00))
        {
            return perfCfPmSensorSignalsUpdate_V00(pPmSensor);
        }
        case LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_V10:
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR_V10))
        {
            return perfCfPmSensorSignalsUpdate_V10(pPmSensor);
        }
        case LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_DEV_V10:
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR_CLW_DEV_V10))
        {
            return perfCfPmSensorSignalsUpdate_CLW_DEV_V10(pPmSensor);
        }
        case LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_MIG_V10:
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR_CLW_MIG_V10))
        {
            return perfCfPmSensorSignalsUpdate_CLW_MIG_V10(pPmSensor);
        }
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}
