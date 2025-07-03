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
 * @file    perf_cf_topology_min_max.c
 * @copydoc perf_cf_topology_min_max.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf.h"
#include "perf/cf/perf_cf_topology_min_max.h"

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
perfCfTopologyGrpIfaceModel10ObjSetImpl_MIN_MAX
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PERF_CF_TOPOLOGY_MIN_MAX *pDescTopology =
        (RM_PMU_PERF_CF_TOPOLOGY_MIN_MAX *)pBoardObjDesc;
    PERF_CF_TOPOLOGY_MIN_MAX   *pTopologyMinMax;
    LwBool                      bFirstConstruct = (*ppBoardObj == NULL);
    FLCN_STATUS                 status          = FLCN_OK;

    status = perfCfTopologyGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto perfCfTopologyGrpIfaceModel10ObjSetImpl_MIN_MAX_exit;
    }
    pTopologyMinMax = (PERF_CF_TOPOLOGY_MIN_MAX *)*ppBoardObj;

    if (!bFirstConstruct)
    {
        // We cannot allow subsequent SET calls to change these parameters.
        if ((pTopologyMinMax->topologyIdx1  != pDescTopology->topologyIdx1) ||
            (pTopologyMinMax->topologyIdx2  != pDescTopology->topologyIdx2) ||
            (pTopologyMinMax->bMax          != pDescTopology->bMax))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfCfTopologyGrpIfaceModel10ObjSetImpl_MIN_MAX_exit;
        }
    }

    // Set member variables.
    pTopologyMinMax->topologyIdx1   = pDescTopology->topologyIdx1;
    pTopologyMinMax->topologyIdx2   = pDescTopology->topologyIdx2;
    pTopologyMinMax->bMax           = pDescTopology->bMax;

    boardObjGrpMaskBitSet(&(((PERF_CF_TOPOLOGYS *)pBObjGrp)->refTypeMask),
        BOARDOBJ_GET_GRP_IDX(*ppBoardObj));

perfCfTopologyGrpIfaceModel10ObjSetImpl_MIN_MAX_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfTopologyIfaceModel10GetStatus_MIN_MAX
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_CF_TOPOLOGY_MIN_MAX_GET_STATUS *pStatus =
        (RM_PMU_PERF_CF_TOPOLOGY_MIN_MAX_GET_STATUS *)pPayload;
    PERF_CF_TOPOLOGY_MIN_MAX *pTopologyMinMax =
        (PERF_CF_TOPOLOGY_MIN_MAX *)pBoardObj;
    FLCN_STATUS         status          = FLCN_OK;

    (void)pStatus;

    // Update temporary reading using relwrsion.
    status = perfCfTopologyUpdateTmpReading(&(pTopologyMinMax->super));
    if (status != FLCN_OK)
    {
        goto perfCfTopologyIfaceModel10GetStatus_MIN_MAX_exit;
    }

    // Call super class function to pack reading into status.
    status = perfCfTopologyIfaceModel10GetStatus_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto perfCfTopologyIfaceModel10GetStatus_MIN_MAX_exit;
    }

perfCfTopologyIfaceModel10GetStatus_MIN_MAX_exit:
    return status;
}

/*!
 * @copydoc PerfCfTopologyUpdateTmpReading
 */
FLCN_STATUS
perfCfTopologyUpdateTmpReading_MIN_MAX
(
    PERF_CF_TOPOLOGY   *pTopology
)
{
    PERF_CF_TOPOLOGY_MIN_MAX *pTopologyMinMax =
        (PERF_CF_TOPOLOGY_MIN_MAX *)pTopology;
    PERF_CF_TOPOLOGY   *pTopology1;
    PERF_CF_TOPOLOGY   *pTopology2;
    FLCN_STATUS         status  = FLCN_OK;

    // Relwrsively update topology 1
    pTopology1 = BOARDOBJGRP_OBJ_GET(PERF_CF_TOPOLOGY, pTopologyMinMax->topologyIdx1);
    if (pTopology1 == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto perfCfTopologyUpdateTmpReading_MIN_MAX_exit;
    }
    status = perfCfTopologyUpdateTmpReading(pTopology1);
    if (status != FLCN_OK)
    {
        goto perfCfTopologyUpdateTmpReading_MIN_MAX_exit;
    }

    // Relwrsively update topology 2
    pTopology2 = BOARDOBJGRP_OBJ_GET(PERF_CF_TOPOLOGY, pTopologyMinMax->topologyIdx2);
    if (pTopology2 == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto perfCfTopologyUpdateTmpReading_MIN_MAX_exit;
    }
    status = perfCfTopologyUpdateTmpReading(pTopology2);
    if (status != FLCN_OK)
    {
        goto perfCfTopologyUpdateTmpReading_MIN_MAX_exit;
    }

    // Select the right topology to copy reading.
    if ((pTopologyMinMax->bMax &&
         lwU64CmpLT(&pTopology1->tmpReading, &pTopology2->tmpReading)) ||
        (!pTopologyMinMax->bMax &&
         lwU64CmpGT(&pTopology1->tmpReading, &pTopology2->tmpReading)))
    {
        pTopology1 = pTopology2;
    }
    pTopologyMinMax->super.tmpReading = pTopology1->tmpReading;

perfCfTopologyUpdateTmpReading_MIN_MAX_exit:
    return status;
}

/*!
 * @copydoc PerfCfTopologyUpdateDependencyMask
 */
FLCN_STATUS
perfCfTopologyUpdateDependencyMask_MIN_MAX
(
    PERF_CF_TOPOLOGY       *pTopology,
    BOARDOBJGRPMASK_E32    *pMask,
    BOARDOBJGRPMASK_E32    *pCheckMask
)
{
    PERF_CF_TOPOLOGY_MIN_MAX *pTopologyMinMax =
        (PERF_CF_TOPOLOGY_MIN_MAX *)pTopology;
    PERF_CF_TOPOLOGY   *pTmp;
    FLCN_STATUS         status  = FLCN_OK;

    // Check topology index 1 and merge in dependency relwrsively.
    pTmp = BOARDOBJGRP_OBJ_GET(PERF_CF_TOPOLOGY, pTopologyMinMax->topologyIdx1);
    if (pTmp == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto perfCfTopologyUpdateDependencyMask_MIN_MAX_exit;
    }
    status = perfCfTopologyUpdateDependencyMask(pTmp, pMask, pCheckMask);
    if (status != FLCN_OK)
    {
        goto perfCfTopologyUpdateDependencyMask_MIN_MAX_exit;
    }

    // Check topology index 2 and merge in dependency relwrsively.
    pTmp = BOARDOBJGRP_OBJ_GET(PERF_CF_TOPOLOGY, pTopologyMinMax->topologyIdx2);
    if (pTmp == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto perfCfTopologyUpdateDependencyMask_MIN_MAX_exit;
    }
    status = perfCfTopologyUpdateDependencyMask(pTmp, pMask, pCheckMask);
    if (status != FLCN_OK)
    {
        goto perfCfTopologyUpdateDependencyMask_MIN_MAX_exit;
    }

perfCfTopologyUpdateDependencyMask_MIN_MAX_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
