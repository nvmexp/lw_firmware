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
 * @file    perf_cf_controller_1x.c
 * @copydoc perf_cf_controller_1x.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "boardobj/boardobjgrp_e32.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"
#include "pmu/pmuifperf_cf.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf.h"
#include "perf/cf/perf_cf_controller.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjGrpIfaceModel10SetEntry    (s_perfCfControllerIfaceModel10SetEntry_1X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfControllerIfaceModel10SetEntry_1X");
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
perfCfControllerBoardObjGrpIfaceModel10Set_1X
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    PERF_CF_CONTROLLERS    *pControllers = PERF_CF_GET_CONTROLLERS();
    LwBool                  bFirstConstruct;
    FLCN_STATUS             status;

    bFirstConstruct = !pControllers->super.super.bConstructed;

    status = BOARDOBJGRP_IFACE_MODEL_10_SET(E32,             // _grpType
        PERF_CF_CONTROLLER,                         // _class
        pBuffer,                                    // _pBuffer
        perfCfControllerIfaceModel10SetHeader_1X,         // _hdrFunc
        s_perfCfControllerIfaceModel10SetEntry_1X,        // _entryFunc
        turingAndLater.perfCf.controllers1xGrpSet,  // _ssElement
        OVL_INDEX_DMEM(perfCfController),           // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
    if (status != FLCN_OK)
    {
        goto perfCfControllerBoardObjGrpIfaceModel10Set_1X_exit;
    }

    status = perfCfControllerGrpConstruct_SUPER(pControllers, bFirstConstruct);
    if (status != FLCN_OK)
    {
        goto perfCfControllerBoardObjGrpIfaceModel10Set_1X_exit;
    }

perfCfControllerBoardObjGrpIfaceModel10Set_1X_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
perfCfControllerBoardObjGrpIfaceModel10GetStatus_1X
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status = LW_OK;

    status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS(E32,                    // _grpType
        PERF_CF_CONTROLLER,                                 // _class
        pBuffer,                                            // _pBuffer
        perfCfControllerIfaceModel10GetStatusHeader_1X,                 // _hdrFunc
        perfCfControllerIfaceModel10GetStatus_1X,                       // _entryFunc
        turingAndLater.perfCf.controllers1xGrpGetStatus);   // _ssElement

    return status;
}

/*!
 * @copydoc PerfCfControllersPostInit
 */
FLCN_STATUS
perfCfControllersPostInit_1X
(
    PERF_CF_CONTROLLERS **ppControllers
)
{
    FLCN_STATUS status;
    PERF_CF_CONTROLLERS_1X *const pControllers1x =
        lwosCallocType(OVL_INDEX_DMEM(perfCf), 1U, PERF_CF_CONTROLLERS_1X);

    PMU_ASSERT_OK_OR_GOTO(status,
        pControllers1x != NULL ? FLCN_OK : FLCN_ERR_NO_FREE_MEM,
        perfCfControllersPostInit_1X);

    *ppControllers = &pControllers1x->super;

    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfControllersPostInit_SUPER(ppControllers),
        perfCfControllersPostInit_1X);

perfCfControllersPostInit_1X:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
perfCfControllerIfaceModel10GetStatus_1X
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pBuf
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_UTIL:
        {
            return perfCfControllerIfaceModel10GetStatus_UTIL(pModel10, pBuf);
        }
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_OPTP_2X:
        {
            return perfCfControllerIfaceModel10GetStatus_OPTP_2X(pModel10, pBuf);
        }
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_DLPPC_1X:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_DLPPC_1X))
            {
                return perfCfControllerIfaceModel10GetStatus_DLPPC_1X(pModel10, pBuf);
            }
            break;
        }
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_perfCfControllerIfaceModel10SetEntry_1X
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    switch (pBuf->type)
    {
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_UTIL:
        {
            return perfCfControllerGrpIfaceModel10ObjSetImpl_UTIL(pModel10, ppBoardObj,
                        LW_SIZEOF32(PERF_CF_CONTROLLER_UTIL), pBuf);
        }
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_OPTP_2X:
        {
            return perfCfControllerGrpIfaceModel10ObjSetImpl_OPTP_2X(pModel10, ppBoardObj,
                        LW_SIZEOF32(PERF_CF_CONTROLLER_OPTP_2X), pBuf);
        }
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_DLPPC_1X:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_DLPPC_1X))
            {
                return perfCfControllerGrpIfaceModel10ObjSetImpl_DLPPC_1X(pModel10, ppBoardObj,
                            LW_SIZEOF32(PERF_CF_CONTROLLER_DLPPC_1X), pBuf);
            }
            break;
        }

    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}
