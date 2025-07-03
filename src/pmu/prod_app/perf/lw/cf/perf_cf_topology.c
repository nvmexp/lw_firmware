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
 * @file    perf_cf_topology.c
 * @copydoc perf_cf_topology.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "lwostmrcallback.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf.h"
#include "perf/cf/perf_cf_topology.h"
#include "perf/cf/perf_cf_topology_pmumon.h"
#include "logger.h"
#include "pmu_objfifo.h"
#include "pmu_objsmbpbi.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjGrpIfaceModel10SetHeader   (s_perfCfTopologyIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfTopologyIfaceModel10SetHeader");
static BoardObjGrpIfaceModel10SetEntry    (s_perfCfTopologyIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfTopologyIfaceModel10SetEntry");
static BoardObjGrpIfaceModel10GetStatusHeader   (s_perfCfTopologyIfaceModel10GetStatusHeaderSense)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "s_perfCfTopologyIfaceModel10GetStatusHeaderSense");
static BoardObjGrpIfaceModel10GetStatusHeader   (s_perfCfTopologyIfaceModel10GetStatusHeaderRef)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "s_perfCfTopologyIfaceModel10GetStatusHeaderRef");
static FLCN_STATUS                  s_perfCfTopologyGetStatusHeaderCore(PERF_CF_TOPOLOGYS *pTopologys, BOARDOBJGRPMASK_E32 *pMask)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "s_perfCfTopologyGetStatusHeaderCore");
static BoardObjIfaceModel10GetStatus                (s_perfCfTopologyIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "s_perfCfTopologyIfaceModel10GetStatus");
static FLCN_STATUS                  s_perfCfTopologyPostConstruct(void)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfTopologyPostConstruct")
    GCC_ATTRIB_NOINLINE();
static void                         s_perfCfTopologyGetGpumonSample(PERF_CF_TOPOLOGYS *pTopologys, RM_PMU_GPUMON_PERFMON_SAMPLE *pSample)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "s_perfCfTopologyGetGpumonSample");
static FLCN_STATUS                  s_perfCfTopologyLogGpumonSample(PERF_CF_TOPOLOGYS *pTopologys)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "s_perfCfTopologyLogGpumonSample");

static OsTmrCallbackFunc            (s_perfCfTopologysCallback)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "s_perfCfTopologysCallback")
    GCC_ATTRIB_USED();

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @PERF_CF_TMR_CALLBACK_TOPOLOGYS
 * derived callback structure from base class OS_TMR_CALLBACK.
 */
typedef struct
{
    /*!
     * OS_TMR_CALLBACK super class. This should always be the first member!
     */
    OS_TMR_CALLBACK     super;
    /*!
     * PERF_CF group of TOPOLOGY objects.
     */
    PERF_CF_TOPOLOGYS  *pTopologys;
} PERF_CF_TMR_CALLBACK_TOPOLOGYS;

/* ------------------------ Global Variables -------------------------------- */
PERF_CF_TMR_CALLBACK_TOPOLOGYS OsCBPerfCfTopologys;

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
perfCfTopologyBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status = FLCN_OK;

    status = BOARDOBJGRP_IFACE_MODEL_10_SET(E32,             // _grpType
        PERF_CF_TOPOLOGY,                           // _class
        pBuffer,                                    // _pBuffer
        s_perfCfTopologyIfaceModel10SetHeader,            // _hdrFunc
        s_perfCfTopologyIfaceModel10SetEntry,             // _entryFunc
        turingAndLater.perfCf.topologyGrpSet,       // _ssElement
        OVL_INDEX_DMEM(perfCfTopology),             // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
    if (status != FLCN_OK)
    {
        goto perfCfTopologyBoardObjGrpIfaceModel10Set_exit;
    }

    // Trigger Perf CF Topology sanity check.
    status = perfCfTopologysSanityCheck_HAL(&PerfCf);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfCfTopologyBoardObjGrpIfaceModel10Set_exit;
    }

    status = s_perfCfTopologyPostConstruct();
    if (status != FLCN_OK)
    {
        goto perfCfTopologyBoardObjGrpIfaceModel10Set_exit;
    }

perfCfTopologyBoardObjGrpIfaceModel10Set_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
perfCfTopologyBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status = FLCN_OK;

    // First pass takes care of the header and _SENSED/_SENSED_BASE status/tmpReading.
    status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS(E32,                // _grpType
        PERF_CF_TOPOLOGY,                               // _class
        pBuffer,                                        // _pBuffer
        s_perfCfTopologyIfaceModel10GetStatusHeaderSense,           // _hdrFunc
        s_perfCfTopologyIfaceModel10GetStatus,                      // _entryFunc
        turingAndLater.perfCf.topologyGrpGetStatus);    // _ssElement
    if (status != FLCN_OK)
    {
        goto perfCfTopologyBoardObjGrpIfaceModel10GetStatus_exit;
    }

    // Second pass takes care of the _MIN_MAX status.
    status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS(E32,                // _grpType
        PERF_CF_TOPOLOGY,                               // _class
        pBuffer,                                        // _pBuffer
        s_perfCfTopologyIfaceModel10GetStatusHeaderRef,             // _hdrFunc
        s_perfCfTopologyIfaceModel10GetStatus,                      // _entryFunc
        turingAndLater.perfCf.topologyGrpGetStatus);    // _ssElement
    if (status != FLCN_OK)
    {
        goto perfCfTopologyBoardObjGrpIfaceModel10GetStatus_exit;
    }

perfCfTopologyBoardObjGrpIfaceModel10GetStatus_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfCfTopologyGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PERF_CF_TOPOLOGY *pDescTopology =
        (RM_PMU_PERF_CF_TOPOLOGY *)pBoardObjDesc;
    PERF_CF_TOPOLOGY   *pTopology;
    LwBool              bFirstConstruct = (*ppBoardObj == NULL);
    FLCN_STATUS         status          = FLCN_OK;

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto perfCfTopologyGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }
    pTopology = (PERF_CF_TOPOLOGY *)*ppBoardObj;

    if (!bFirstConstruct)
    {
        // We cannot allow subsequent SET calls to change these parameters.
        if (pTopology->gpumonTag != pDescTopology->gpumonTag)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfCfTopologyGrpIfaceModel10ObjSetImpl_SUPER_exit;
        }
    }

    // Set member variables.
    boardObjGrpMaskInit_E32(&(pTopology->dependencyMask));
    pTopology->gpumonTag     = pDescTopology->gpumonTag;
    pTopology->bNotAvailable = pDescTopology->bNotAvailable;

perfCfTopologyGrpIfaceModel10ObjSetImpl_SUPER_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfTopologyIfaceModel10GetStatus_SUPER
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    PERF_CF_TOPOLOGYS *pTopologys = PERF_CF_GET_TOPOLOGYS();
    RM_PMU_PERF_CF_TOPOLOGY_GET_STATUS *pStatus =
        (RM_PMU_PERF_CF_TOPOLOGY_GET_STATUS *)pPayload;
    PERF_CF_TOPOLOGY *pTopology =
        (PERF_CF_TOPOLOGY *)pBoardObj;
    FLCN_STATUS status = FLCN_OK;

    // Call super-class impementation;
    status = boardObjIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto perfCfTopologyIfaceModel10GetStatus_SUPER_exit;
    }

    // SUPER class specific implementation.
    LwU64_ALIGN32_PACK(&pStatus->reading, &pTopology->tmpReading);
    pStatus->lastPolledReading =
        pTopologys->pPollingStatus->topologys[BOARDOBJ_GET_GRP_IDX(pBoardObj)].topology.reading;

perfCfTopologyIfaceModel10GetStatus_SUPER_exit:
    return status;
}

/*!
 * @copydoc PerfCfTopologysLoad
 */
FLCN_STATUS
perfCfTopologysLoad
(
    PERF_CF_TOPOLOGYS  *pTopologys,
    LwBool              bLoad
)
{
    FLCN_STATUS     status          = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[]   = {
        OSTASK_OVL_DESC_DEFINE_PERF_CF_TOPOLOGY
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Nothing to do when there are no TOPOLOGY objects.
        if (boardObjGrpMaskIsZero(&(pTopologys->super.objMask)))
        {
            goto perfCfTopologysLoad_exit;
        }

        if (bLoad)
        {
            status = boardObjGrpMaskCopy(&(pTopologys->pPollingStatus->mask),
                                         &(pTopologys->super.objMask));
            if (status != FLCN_OK)
            {
                goto perfCfTopologysLoad_exit;
            }

            if (!OS_TMR_CALLBACK_WAS_CREATED(&(OsCBPerfCfTopologys.super)))
            {
                OsCBPerfCfTopologys.pTopologys = pTopologys;

                osTmrCallbackCreate(&(OsCBPerfCfTopologys.super),           // pCallback
                    OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED, // type
                    OVL_INDEX_IMEM(perfCfTopology),                         // ovlImem
                    s_perfCfTopologysCallback,                              // pTmrCallbackFunc
                    LWOS_QUEUE(PMU, PERF_CF),                               // queueHandle
                    pTopologys->pollingPeriodus,                            // periodNormalus
                    pTopologys->pollingPeriodus,                            // periodSleepus
                    OS_TIMER_RELAXED_MODE_USE_NORMAL,                       // bRelaxedUseSleep
                    RM_PMU_TASK_ID_PERF_CF);                                // taskId
                osTmrCallbackSchedule(&(OsCBPerfCfTopologys.super));
            }

            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_TOPOLOGIES_PMUMON))
            {
                status = perfCfTopologiesPmumonLoad();
                if (status != FLCN_OK)
                {
                    goto perfCfTopologysLoad_exit;
                }
            }
        }
        else
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_TOPOLOGIES_PMUMON))
            {
                perfCfTopologiesPmumonUnload();
            }

            if (OS_TMR_CALLBACK_WAS_CREATED(&(OsCBPerfCfTopologys.super)))
            {
                if (!osTmrCallbackCancel(&(OsCBPerfCfTopologys.super)))
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_STATE;
                    goto perfCfTopologysLoad_exit;
                }
            }
        }
    }
perfCfTopologysLoad_exit:
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    return status;
}

/*!
 * @copydoc PerfCfTopologysStatusGet
 */
FLCN_STATUS
perfCfTopologysStatusGet
(
    PERF_CF_TOPOLOGYS          *pTopologys,
    PERF_CF_TOPOLOGYS_STATUS   *pStatus
)
{
    PERF_CF_TOPOLOGY   *pTopology;
    BOARDOBJGRPMASK_E32 mask;
    LwBoardObjIdx       i;
    FLCN_STATUS         status      = FLCN_OK;

    if (!boardObjGrpMaskIsSubset(&(pStatus->mask), &(pTopologys->super.objMask)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfTopologysStatusGet_exit;
    }

    // Get sensors status into temporary buffer.
    status = s_perfCfTopologyGetStatusHeaderCore(pTopologys, &(pStatus->mask));
    if (status != FLCN_OK)
    {
        goto perfCfTopologysStatusGet_exit;
    }

    boardObjGrpMaskInit_E32(&mask);

    status = boardObjGrpMaskAnd(&mask, &(pStatus->mask), &(pTopologys->senseTypeMask));
    if (status != FLCN_OK)
    {
        goto perfCfTopologysStatusGet_exit;
    }

    // First pass sensors status into _SENSED/_SENSED_BASED status and tmpReading.
    BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_TOPOLOGY, pTopology, i, &(mask.super))
    {
        BOARDOBJ_IFACE_MODEL_10 *pObjModel10 =
            boardObjIfaceModel10FromBoardObjGet(&pTopology->super);

        status = s_perfCfTopologyIfaceModel10GetStatus(
            pObjModel10,
            &pStatus->topologys[i].boardObj);
        if (status != FLCN_OK)
        {
            goto perfCfTopologysStatusGet_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    status = boardObjGrpMaskAnd(&mask, &(pStatus->mask), &(pTopologys->refTypeMask));
    if (status != FLCN_OK)
    {
        goto perfCfTopologysStatusGet_exit;
    }

    // Second pass _SENSED/_SENSED_BASED tmpReading into _MIN_MAX status via relwrsion.
    BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_TOPOLOGY, pTopology, i, &(mask.super))
    {
        BOARDOBJ_IFACE_MODEL_10 *pObjModel10 =
            boardObjIfaceModel10FromBoardObjGet(&pTopology->super);

        status = s_perfCfTopologyIfaceModel10GetStatus(
            pObjModel10,
            &pStatus->topologys[i].boardObj);
        if (status != FLCN_OK)
        {
            goto perfCfTopologysStatusGet_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

perfCfTopologysStatusGet_exit:
    return status;
}

/*!
 * @copydoc PerfCfTopologyUpdateSensorsMask
 */
FLCN_STATUS
perfCfTopologyUpdateSensorsMask
(
    PERF_CF_TOPOLOGY       *pTopology,
    BOARDOBJGRPMASK_E32    *pMask
)
{
    switch (BOARDOBJ_GET_TYPE(&pTopology->super))
    {
        case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_TYPE_SENSED_BASE:
            return perfCfTopologyUpdateSensorsMask_SENSED_BASE(pTopology, pMask);
        case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_TYPE_SENSED:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_TOPOLOGY_SENSED))
            {
                return perfCfTopologyUpdateSensorsMask_SENSED(pTopology, pMask);
            }
            break;
    }

    return FLCN_OK;
}

/*!
 * PERF_CF_TOPOLOGYS post-init function.
 */
FLCN_STATUS
perfCfTopologysPostInit
(
    PERF_CF_TOPOLOGYS *pTopologys
)
{
    FLCN_STATUS status = FLCN_OK;

    // Allocate larger structures in TOPOLOGY DMEM.

    pTopologys->pPollingStatus =
        lwosCallocType(OVL_INDEX_DMEM(perfCfTopology), 1U,
            PERF_CF_TOPOLOGYS_STATUS);
    if (pTopologys->pPollingStatus == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NO_FREE_MEM;
        goto perfCfTopologysPostInit_exit;
    }

    pTopologys->pTmpSensorsStatus =
        lwosCallocType(OVL_INDEX_DMEM(perfCfTopology), 1U,
            PERF_CF_SENSORS_STATUS);
    if (pTopologys->pTmpSensorsStatus == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NO_FREE_MEM;
        goto perfCfTopologysPostInit_exit;
    }

    // Initialize the masks.
    boardObjGrpMaskInit_E32(&(pTopologys->pPollingStatus->mask));
    boardObjGrpMaskInit_E32(&(pTopologys->pTmpSensorsStatus->mask));
    boardObjGrpMaskInit_E32(&(pTopologys->senseTypeMask));
    boardObjGrpMaskInit_E32(&(pTopologys->refTypeMask));

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_TOPOLOGIES_PMUMON))
    {
        status = perfCfTopologiesPmumonConstruct();
        if (status != FLCN_OK)
        {
            goto perfCfTopologysPostInit_exit;
        }
    }

perfCfTopologysPostInit_exit:
    return status;
}

/*!
 * @copydoc PerfCfTopologyUpdateTmpReading
 */
FLCN_STATUS
perfCfTopologyUpdateTmpReading
(
    PERF_CF_TOPOLOGY   *pTopology
)
{
    switch (BOARDOBJ_GET_TYPE(&pTopology->super))
    {
        case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_TYPE_SENSED_BASE:
            // Tmp reading should already be valid from first pass.
            break;
        case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_TYPE_MIN_MAX:
            return perfCfTopologyUpdateTmpReading_MIN_MAX(pTopology);
            break;
        case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_TYPE_SENSED:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_TOPOLOGY_SENSED))
            {
                // Tmp reading should already be valid from first pass.
            }
            break;
    }

    return FLCN_OK;
}

/*!
 * @copydoc PerfCfTopologyUpdateDependencyMask
 */
FLCN_STATUS
perfCfTopologyUpdateDependencyMask
(
    PERF_CF_TOPOLOGY       *pTopology,
    BOARDOBJGRPMASK_E32    *pMask,
    BOARDOBJGRPMASK_E32    *pCheckMask
)
{
    FLCN_STATUS status = FLCN_OK;

    // Check for relwrsive loop.
    if (boardObjGrpMaskBitGet(pCheckMask, BOARDOBJ_GET_GRP_IDX(&pTopology->super)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto perfCfTopologyUpdateDependencyMask_exit;
    }
    boardObjGrpMaskBitSet(pCheckMask, BOARDOBJ_GET_GRP_IDX(&pTopology->super));

    // Construct current dependency mask first, if needed.
    if (boardObjGrpMaskIsZero(&pTopology->dependencyMask))
    {
        // All topologys depend on self.
        boardObjGrpMaskBitSet(&pTopology->dependencyMask, BOARDOBJ_GET_GRP_IDX(&pTopology->super));

        switch (BOARDOBJ_GET_TYPE(&pTopology->super))
        {
            case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_TYPE_SENSED_BASE:
                // These does not depend on other topologys.
                break;

            case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_TYPE_MIN_MAX:
                status = perfCfTopologyUpdateDependencyMask_MIN_MAX(
                            pTopology, &pTopology->dependencyMask, pCheckMask);
                if (status != FLCN_OK)
                {
                    goto perfCfTopologyUpdateDependencyMask_exit;
                }
                break;

            case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_TYPE_SENSED:
                if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_TOPOLOGY_SENSED))
                {
                    // These does not depend on other topologys.
                }
                break;
        }
    }

    // Bit-wise-OR current dependency mask into output.
    if (pMask != NULL)
    {
        status = boardObjGrpMaskOr(pMask, pMask, &pTopology->dependencyMask);
        if (status != FLCN_OK)
        {
            goto perfCfTopologyUpdateDependencyMask_exit;
        }
    }

perfCfTopologyUpdateDependencyMask_exit:
    boardObjGrpMaskBitClr(pCheckMask, BOARDOBJ_GET_GRP_IDX(&pTopology->super));
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
static FLCN_STATUS
s_perfCfTopologyIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PERF_CF_TOPOLOGY_BOARDOBJGRP_SET_HEADER *pHdr =
        (RM_PMU_PERF_CF_TOPOLOGY_BOARDOBJGRP_SET_HEADER *)pHdrDesc;
    PERF_CF_TOPOLOGYS  *pTopologys  = (PERF_CF_TOPOLOGYS *)pBObjGrp;
    FLCN_STATUS         status      = FLCN_OK;

    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        goto s_perfCfTopologyIfaceModel10SetHeader_exit;
    }

    pTopologys->pollingPeriodus = ((LwU32)pHdr->pollingPeriodms) * 1000U;

    if (pTopologys->pollingPeriodus == 0)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_perfCfTopologyIfaceModel10SetHeader_exit;
    }

s_perfCfTopologyIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_perfCfTopologyIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    switch (pBuf->type)
    {
        case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_TYPE_SENSED_BASE:
            return perfCfTopologyGrpIfaceModel10ObjSetImpl_SENSED_BASE(pModel10, ppBoardObj,
                        sizeof(PERF_CF_TOPOLOGY_SENSED_BASE), pBuf);
        case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_TYPE_MIN_MAX:
            return perfCfTopologyGrpIfaceModel10ObjSetImpl_MIN_MAX(pModel10, ppBoardObj,
                        sizeof(PERF_CF_TOPOLOGY_MIN_MAX), pBuf);
        case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_TYPE_SENSED:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_TOPOLOGY_SENSED))
            {
                return perfCfTopologyGrpIfaceModel10ObjSetImpl_SENSED(pModel10, ppBoardObj,
                            sizeof(PERF_CF_TOPOLOGY_SENSED), pBuf);
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
s_perfCfTopologyIfaceModel10GetStatusHeaderSense
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pBuf,
    BOARDOBJGRPMASK    *pMask
)
{
    BOARDOBJGRP *pBoardObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    PERF_CF_TOPOLOGYS *pTopologys =
        (PERF_CF_TOPOLOGYS *)pBoardObjGrp;
    RM_PMU_PERF_CF_TOPOLOGY_BOARDOBJGRP_GET_STATUS_HEADER *pHdr =
        (RM_PMU_PERF_CF_TOPOLOGY_BOARDOBJGRP_GET_STATUS_HEADER *)pBuf;
    FLCN_STATUS         status  = FLCN_OK;
    BOARDOBJGRPMASK_E32 mask;

    // Init and copy in the header mask.
    boardObjGrpMaskInit_E32(&mask);
    status = boardObjGrpMaskImport_E32(&mask, &(pHdr->super.objMask));
    if (status != FLCN_OK)
    {
        goto s_perfCfTopologyIfaceModel10GetStatusHeaderSense_exit;
    }

    status = s_perfCfTopologyGetStatusHeaderCore(pTopologys, &mask);
    if (status != FLCN_OK)
    {
        goto s_perfCfTopologyIfaceModel10GetStatusHeaderSense_exit;
    }

    // Copy out the updated header mask.
    status = boardObjGrpMaskExport_E32(&mask, &(pHdr->super.objMask));
    if (status != FLCN_OK)
    {
        goto s_perfCfTopologyIfaceModel10GetStatusHeaderSense_exit;
    }

    // Update and filter the to-be-traversed mask to include only "sense type" topologys.
    status = boardObjGrpMaskAnd((BOARDOBJGRPMASK_E32 *)pMask,
        &mask, &(pTopologys->senseTypeMask));
    if (status != FLCN_OK)
    {
        goto s_perfCfTopologyIfaceModel10GetStatusHeaderSense_exit;
    }

s_perfCfTopologyIfaceModel10GetStatusHeaderSense_exit:
    return FLCN_OK;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10GetStatusHeader
 */
static FLCN_STATUS
s_perfCfTopologyIfaceModel10GetStatusHeaderRef
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pBuf,
    BOARDOBJGRPMASK    *pMask
)
{
    BOARDOBJGRP *pBoardObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    PERF_CF_TOPOLOGYS *pTopologys =
        (PERF_CF_TOPOLOGYS *)pBoardObjGrp;
    FLCN_STATUS status = FLCN_OK;

    // Filter the to-be-traversed mask to include only "reference type" topologys.
    status = boardObjGrpMaskAnd((BOARDOBJGRPMASK_E32 *)pMask,
        (BOARDOBJGRPMASK_E32 *)pMask, &(pTopologys->refTypeMask));
    if (status != FLCN_OK)
    {
        goto s_perfCfTopologyIfaceModel10GetStatusHeaderRef_exit;
    }

s_perfCfTopologyIfaceModel10GetStatusHeaderRef_exit:
    return FLCN_OK;
}

/*!
 * Core of the Topology GetStatus function. Update topology mask to include
 * dependency and cache latest sensor status into tmpSensorsStatus.
 *
 * @param[in]   pMask       Pointer to the mask of Topology BOARDOBJs
 * @param[out]  pTopologys  Pointer to Topologys BOARDOBJGRP
 *
 * @return FLCN_OK
 *      Latest sensor status is queried successfully
 */
static FLCN_STATUS
s_perfCfTopologyGetStatusHeaderCore
(
    PERF_CF_TOPOLOGYS      *pTopologys,
    BOARDOBJGRPMASK_E32    *pMask
)
{
    BOARDOBJGRPMASK_E32 fullTopologyMask;
    PERF_CF_TOPOLOGY   *pTopology;
    PERF_CF_SENSORS    *pSensors = PERF_CF_GET_SENSORS();
    LwBoardObjIdx       i;
    FLCN_STATUS         status   = FLCN_OK;

    // Update to full mask (includes all dependency).
    boardObjGrpMaskInit_E32(&fullTopologyMask);
    BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_TOPOLOGY, pTopology, i, &(pMask->super))
    {
        status = boardObjGrpMaskOr(&fullTopologyMask, &fullTopologyMask,
                                   &(pTopology->dependencyMask));
        if (status != FLCN_OK)
        {
            goto s_perfCfTopologyGetStatusHeaderCore_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;
    status = boardObjGrpMaskCopy(pMask, &fullTopologyMask);
    if (status != FLCN_OK)
    {
        goto s_perfCfTopologyGetStatusHeaderCore_exit;
    }

    // Clear and update sensors mask.
    boardObjGrpMaskClr(&(pTopologys->pTmpSensorsStatus->mask));
    BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_TOPOLOGY, pTopology, i, &(pMask->super))
    {
        status = perfCfTopologyUpdateSensorsMask(pTopology,
            &pTopologys->pTmpSensorsStatus->mask);
        if (status != FLCN_OK)
        {
            goto s_perfCfTopologyGetStatusHeaderCore_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    // Get sensors status into temporary buffer.
    status = perfCfSensorsStatusGet(pSensors, pTopologys->pTmpSensorsStatus);
    if (status != FLCN_OK)
    {
        goto s_perfCfTopologyGetStatusHeaderCore_exit;
    }

s_perfCfTopologyGetStatusHeaderCore_exit:
    return FLCN_OK;
}

/*!
 * @Copydoc BoardObjIfaceModel10GetStatus
 */
static FLCN_STATUS
s_perfCfTopologyIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pBuf
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_TYPE_SENSED_BASE:
            return perfCfTopologyIfaceModel10GetStatus_SENSED_BASE(pModel10, pBuf);
        case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_TYPE_MIN_MAX:
            return perfCfTopologyIfaceModel10GetStatus_MIN_MAX(pModel10, pBuf);
        case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_TYPE_SENSED:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_TOPOLOGY_SENSED))
            {
                return perfCfTopologyIfaceModel10GetStatus_SENSED(pModel10, pBuf);
            }
            break;
    }

    return FLCN_OK;
}

/*!
 * @copydoc OsTmrCallbackFunc
 */
static FLCN_STATUS
s_perfCfTopologysCallback
(
    OS_TMR_CALLBACK *pCallback
)
{
    FLCN_STATUS     status        = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_PERF_CF_TOPOLOGY
#if PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_ACLWMULATE_UTIL_COUNTERS)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libEngUtil)
#endif
#if PMUCFG_FEATURE_ENABLED(PMU_LOGGER)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, loggerWrite)
#endif
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        PERF_CF_TOPOLOGYS  *pTopologys;

        pTopologys = ((PERF_CF_TMR_CALLBACK_TOPOLOGYS *)pCallback)->pTopologys;

        status = perfCfTopologysStatusGet(pTopologys, pTopologys->pPollingStatus);
        if (status != FLCN_OK)
        {
            goto s_perfCfTopologysCallback_exit;
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_LOGGER))
        {
            PMU_HALT_OK_OR_GOTO(status,
                s_perfCfTopologyLogGpumonSample(pTopologys),
                s_perfCfTopologysCallback_exit);
        }
    }
s_perfCfTopologysCallback_exit:
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return FLCN_OK; // NJ-TODO-CB
}

static FLCN_STATUS
s_perfCfTopologyPostConstruct(void)
{
    PERF_CF_TOPOLOGY   *pTopology;
    BOARDOBJGRPMASK_E32 checkMask;
    LwBoardObjIdx       i;
    FLCN_STATUS         status = FLCN_OK;

    boardObjGrpMaskInit_E32(&checkMask);

    // Validate and set dependency mask for all topologys.
    BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_TOPOLOGY, pTopology, i, NULL)
    {
        status = perfCfTopologyUpdateDependencyMask(pTopology, NULL, &checkMask);
        if (status != FLCN_OK)
        {
            goto s_perfCfTopologyPostConstruct_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

s_perfCfTopologyPostConstruct_exit:
    return status;
}

/*!
 * Helper function to log a GPU monitoring utilization sample.
 *
 * @param[in]  pTopologys   Pointer to topologys group.
 */
static FLCN_STATUS
s_perfCfTopologyLogGpumonSample
(
    PERF_CF_TOPOLOGYS *pTopologys
)
{
    LwU32 buffer[(RM_PMU_LOGGER_SAMPLE_SIZE_PERFMON_UTIL/sizeof(LwU32)) * 2];
    RM_PMU_GPUMON_PERFMON_SAMPLE *pSample;
    FLCN_TIMESTAMP               samplePrevTimestamp;
    FLCN_TIMESTAMP               sampleTimestamp;
    LwU32                        delayUs;
    FLCN_STATUS                  status = FLCN_OK;

    ct_assert(sizeof(RM_PMU_GPUMON_PERFMON_SAMPLE) == RM_PMU_LOGGER_SAMPLE_SIZE_PERFMON_UTIL);

    pSample = (RM_PMU_GPUMON_PERFMON_SAMPLE *)
        LW_ALIGN_UP((LwUPtr)&buffer[0], RM_PMU_LOGGER_SAMPLE_SIZE_PERFMON_UTIL); 

    samplePrevTimestamp = pTopologys->lastGpumonTimestamp;
    s_perfCfTopologyGetGpumonSample(pTopologys, pSample);

    sampleTimestamp                 = (FLCN_TIMESTAMP)pSample->base.timeStamp;
    pTopologys->lastGpumonTimestamp = sampleTimestamp;

    loggerWrite(RM_PMU_LOGGER_SEGMENT_ID_PERFMON_UTIL,
        sizeof(RM_PMU_GPUMON_PERFMON_SAMPLE),
        pSample);

    if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_ACLWMULATE_UTIL_COUNTERS) &&
        (samplePrevTimestamp.data != 0))
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, smbpbi)
        };
        //
        // The sample rate will always be less than 4.29 secs, as any sample rate
        // greater than 1 sec will be too inaclwrate. Lwrrently, the sample rate
        // is 200 ms. Because of this, only the lower 32 bits of the timestamps
        // need to be used.
        //
        delayUs = (sampleTimestamp.parts.lo - samplePrevTimestamp.parts.lo) / 1000;

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = smbpbiAclwtilCounterHelper(delayUs, pSample);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_perfCfTopologyLogGpumonSample_exit;
        }
    }

s_perfCfTopologyLogGpumonSample_exit:
    return status;
}

/*!
 * Helper function to get a GPU monitoring utilization sample.
 *
 * @param[in]  pTopologys   Pointer to topologys group.
 * @param[out] pSample      @ref LW2080_CTRL_PERF_GPUMON_PERFMON_UTIL_SAMPLE
 */
static void
s_perfCfTopologyGetGpumonSample
(
    PERF_CF_TOPOLOGYS            *pTopologys,
    RM_PMU_GPUMON_PERFMON_SAMPLE *pSample
)
{
    PERF_CF_TOPOLOGY *pTopology;
    LwU64             util64      = 0;
    LwU64             pct100      = 10000;
    LwU32             util        = 0;
    LwU32             lwenlwtil   = 0;
    LwU32             lwencCount  = 0;
    LwU32             lwdelwtil   = 0;
    LwU32             lwdecCount  = 0;
    LwBoardObjIdx     i           = 0;
    LwU8              engId       = 0;

    memset(pSample, 0,
        sizeof(RM_PMU_GPUMON_PERFMON_SAMPLE));

    //
    // Get each engine's context.
    //

    fifoGetEngineStatus_HAL(&Fifo, PMU_ENGINE_GR,
        (LwU32 *)&pSample->gr.context);

    // For FB, returns GR engine's context
    pSample->fb.context = pSample->gr.context;

    if (GET_FIFO_ENG(PMU_ENGINE_ENC0) != PMU_ENGINE_MISSING)
    {
        fifoGetEngineStatus_HAL(&Fifo, PMU_ENGINE_ENC0,
            (LwU32 *)&pSample->lwenc.context);
    }

    // PMU engine ID for LWDEC is repurposed on different chips.
    fifoGetLwdecPmuEngineId_HAL(&Fifo, &engId);
    if (GET_FIFO_ENG(engId) != PMU_ENGINE_MISSING)
    {
        fifoGetEngineStatus_HAL(&Fifo, engId,
            (LwU32 *)&pSample->lwdec.context);
    }

    timerGetGpuLwrrentNs_HAL(&Timer, (FLCN_TIMESTAMP *)&pSample->base.timeStamp);

    //
    // Parse utilization data.
    //

    BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_TOPOLOGY, pTopology, i, NULL)
    {
        if ((LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_GPUMON_TAG_NONE == pTopology->gpumonTag) ||
            (LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_GPUMON_TAG_VID  == pTopology->gpumonTag) ||
            pTopology->bNotAvailable)
        {
            continue;
        }

        LwU64_ALIGN32_UNPACK(&util64,
            &(pTopologys->pPollingStatus->topologys[i].topology.reading));

        // Scale 1 (FXP40.24) to 0.01% (LwU32).
        lwU64Mul(&util64, &util64, &pct100);
        lw64Lsr(&util64, &util64, 24);
        util = (LwU32)util64;

        // Clamp utilization to 100%.
        util = LW_MIN(util, (LwU32)pct100);

        switch (pTopology->gpumonTag)
        {
            case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_GPUMON_TAG_FB:
                pSample->fb.util = util;
                break;
            case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_GPUMON_TAG_GR:
                pSample->gr.util = util;
                break;
            case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_GPUMON_TAG_LWENC:
                lwenlwtil += util;
                lwencCount++;
                break;
            case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_GPUMON_TAG_LWDEC:
                lwdelwtil += util;
                lwdecCount++;
                break;
            default:
                PMU_BREAKPOINT();
                break;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    // Callwlate avarage if LWENC/LWDEC engines are present.
    if (lwencCount > 0)
    {
         pSample->lwenc.util = lwenlwtil / lwencCount;
    }
    if (lwdecCount > 0)
    {
         pSample->lwdec.util = lwdelwtil / lwdecCount;
    }
}
