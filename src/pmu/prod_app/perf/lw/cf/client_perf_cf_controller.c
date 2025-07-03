/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    client_perf_cf_controller.c
 * @copydoc client_perf_cf_controller.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "dmemovl.h"
#include "lwostimer.h"
#include "lwostmrcallback.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf.h"
#include "perf/cf/client_perf_cf_controller.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjGrpIfaceModel10SetEntry    (s_perfCfClientPerfCfControllerIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfClientPerfCfControllerIfaceModel10SetEntry");
static BoardObjIfaceModel10GetStatus                (s_perfCfClientPerfCfControllerIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfClientPerfCfControllerIfaceModel10GetStatus");
static BoardObjGrpIfaceModel10GetStatusHeader   (s_perfCfClientPerfCfControllerIfaceModel10GetStatusHeader)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfClientPerfCfControllerIfaceModel10GetStatusHeader");

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
perfCfClientPerfCfControllerBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status;

    status = BOARDOBJGRP_IFACE_MODEL_10_SET(E32,                     // _grpType
        CLIENT_PERF_CF_CONTROLLER,                                 // _class
        pBuffer,                                            // _pBuffer
        boardObjGrpIfaceModel10SetHeader,                         // _hdrFunc
        s_perfCfClientPerfCfControllerIfaceModel10SetEntry,       // _entryFunc
        ga10xAndLater.perfCf.client.controllerGrpSet,       // _ssElement
        OVL_INDEX_DMEM(perfCfController),                   // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);            // _ppObjectVtables
    if (status != FLCN_OK)
    {
        goto perfCfClientPerfCfControllerBoardObjGrpIfaceModel10Set_exit;
    }

perfCfClientPerfCfControllerBoardObjGrpIfaceModel10Set_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
perfCfClientPerfCfControllerBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status = LW_OK;

    status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS(E32,                        // _grpType
        CLIENT_PERF_CF_CONTROLLER,                              // _class
        pBuffer,                                                // _pBuffer
        s_perfCfClientPerfCfControllerIfaceModel10GetStatusHeader,          // _hdrFunc
        s_perfCfClientPerfCfControllerIfaceModel10GetStatus,                // _entryFunc
        ga10xAndLater.perfCf.client.controllerGrpGetStatus);    // _ssElement

    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfCfClientPerfCfControllerGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PERF_CF_CLIENT_PERF_CF_CONTROLLER *pDescController =
        (RM_PMU_PERF_CF_CLIENT_PERF_CF_CONTROLLER *)pBoardObjDesc;
    CLIENT_PERF_CF_CONTROLLER *pController;
    LwBool              bFirstConstruct = (*ppBoardObj == NULL);
    FLCN_STATUS         status;

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto perfCfClientPerfCfControllerGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }
    pController = (CLIENT_PERF_CF_CONTROLLER *)*ppBoardObj;

    if (!bFirstConstruct)
    {
        // We cannot allow subsequent SET calls to change these parameters.
        if (pController->controllerIdx != pDescController->controllerIdx)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfCfClientPerfCfControllerGrpIfaceModel10ObjSetImpl_SUPER_exit;
        }
    }

    // Set member variables.
    pController->controllerIdx = pDescController->controllerIdx;

    // Validate member variables.
    if (!BOARDOBJGRP_IS_VALID(PERF_CF_CONTROLLER, pController->controllerIdx))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto perfCfClientPerfCfControllerGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }

perfCfClientPerfCfControllerGrpIfaceModel10ObjSetImpl_SUPER_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfClientPerfCfControllerIfaceModel10GetStatus_SUPER
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_CF_CLIENT_PERF_CF_CONTROLLER_GET_STATUS *pStatus =
        (RM_PMU_PERF_CF_CLIENT_PERF_CF_CONTROLLER_GET_STATUS *)pPayload;
    CLIENT_PERF_CF_CONTROLLER *pController =
        (CLIENT_PERF_CF_CONTROLLER *)pBoardObj;
    FLCN_STATUS         status      = FLCN_OK;

    // Call super-class implementation
    status = boardObjIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto perfCfClientPerfCfControllerIfaceModel10GetStatus_SUPER_exit;
    }
    (void)pStatus;
    (void)pController;

perfCfClientPerfCfControllerIfaceModel10GetStatus_SUPER_exit:
    return status;
}
/* ------------------------ Private Functions ------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_perfCfClientPerfCfControllerIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    switch (pBuf->type)
    {
        case LW2080_CTRL_PERF_CLIENT_PERF_CF_CONTROLLER_TYPE_MEM_TUNE_1X:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLIENT_PERF_CF_CONTROLLER_MEM_TUNE))
            {
                return perfCfClientPerfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE(pModel10, ppBoardObj,
                            LW_SIZEOF32(CLIENT_PERF_CF_CONTROLLER_MEM_TUNE), pBuf);
            }
            break;
        }

    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10GetStatusHeader
 */
static FLCN_STATUS
s_perfCfClientPerfCfControllerIfaceModel10GetStatusHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pBuf,
    BOARDOBJGRPMASK    *pMask
)
{
    RM_PMU_PERF_CF_CLIENT_PERF_CF_CONTROLLER_BOARDOBJGRP_GET_STATUS_HEADER *pHdr =
        (RM_PMU_PERF_CF_CLIENT_PERF_CF_CONTROLLER_BOARDOBJGRP_GET_STATUS_HEADER *)pBuf;

    BOARDOBJGRPMASK_E32         *pMaskActive = perfCfControllersMaskActiveGet();
    CLIENT_PERF_CF_CONTROLLER   *pController;
    LwBoardObjIdx                i;

    // Init mask.
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_INIT(&(pHdr->maskActive.super));

    // Pull the active policy mask from internal policy.
    BOARDOBJGRP_ITERATOR_BEGIN(CLIENT_PERF_CF_CONTROLLER, pController, i, NULL)
    {
        if (boardObjGrpMaskBitGet(pMaskActive, pController->controllerIdx))
        {
            LW2080_CTRL_BOARDOBJGRP_MASK_BIT_SET(&(pHdr->maskActive.super), i);
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    return FLCN_OK;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
static FLCN_STATUS
s_perfCfClientPerfCfControllerIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pBuf
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_PERF_CLIENT_PERF_CF_CONTROLLER_TYPE_MEM_TUNE_1X:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLIENT_PERF_CF_CONTROLLER_MEM_TUNE))
            {
                return perfCfClientPerfCfControllerIfaceModel10GetStatus_MEM_TUNE(pModel10, pBuf);
            }
            break;
        }
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}
