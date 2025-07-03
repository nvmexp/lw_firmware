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
 * @file    perf_cf_controller_2x.c
 * @copydoc perf_cf_controller_2x.h
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
static BoardObjGrpIfaceModel10SetEntry    (s_perfCfControllerIfaceModel10SetEntry_2X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfControllerIfaceModel10SetEntry_2X");
static FLCN_STATUS s_perfCfControllersConstructReducedControllersMask_2X(PERF_CF_CONTROLLERS_2X *pControllers2x)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfControllersConstructReducedControllersMask_2X");
static BoardObjIfaceModel10GetStatus                (s_perfCfControllerReducedIfaceModel10GetStatus_2X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfControllerReducedIfaceModel10GetStatus_2X");

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
perfCfControllerBoardObjGrpIfaceModel10Set_2X
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    PERF_CF_CONTROLLERS    *pControllers   = PERF_CF_GET_CONTROLLERS();
    PERF_CF_CONTROLLERS_2X *pControllers2x =
        (PERF_CF_CONTROLLERS_2X *)pControllers;
    LwBool                  bFirstConstruct;
    FLCN_STATUS             status;

    bFirstConstruct = !pControllers->super.super.bConstructed;

    status = BOARDOBJGRP_IFACE_MODEL_10_SET(E32,             // _grpType
        PERF_CF_CONTROLLER,                         // _class
        pBuffer,                                    // _pBuffer
        perfCfControllerIfaceModel10SetHeader_2X,         // _hdrFunc
        s_perfCfControllerIfaceModel10SetEntry_2X,        // _entryFunc
        ga10xAndLater.perfCf.controllers2xGrpSet,   // _ssElement
        OVL_INDEX_DMEM(perfCfController),           // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
    if (status != FLCN_OK)
    {
        goto perfCfControllerBoardObjGrpIfaceModel10Set_2X_exit;
    }

    boardObjGrpMaskInit_E32(&pControllers2x->controllersReducedMask);

    status = perfCfControllerGrpConstruct_SUPER(pControllers, bFirstConstruct);
    if (status != FLCN_OK)
    {
        goto perfCfControllerBoardObjGrpIfaceModel10Set_2X_exit;
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllersConstructReducedControllersMask_2X(pControllers2x),
        perfCfControllerBoardObjGrpIfaceModel10Set_2X_exit);

perfCfControllerBoardObjGrpIfaceModel10Set_2X_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
perfCfControllerBoardObjGrpIfaceModel10GetStatus_2X
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status = LW_OK;

    status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS(E32,                    // _grpType
        PERF_CF_CONTROLLER,                                 // _class
        pBuffer,                                            // _pBuffer
        perfCfControllerIfaceModel10GetStatusHeader_2X,                 // _hdrFunc
        perfCfControllerIfaceModel10GetStatus_2X,                       // _entryFunc
        ga10xAndLater.perfCf.controllers2xGrpGetStatus);    // _ssElement

    return status;
}

/*!
 * @copydoc PerfCfControllersPostInit
 */
FLCN_STATUS
perfCfControllersPostInit_2X
(
    PERF_CF_CONTROLLERS **ppControllers
)
{
    FLCN_STATUS status;
    PERF_CF_CONTROLLERS_2X *const pControllers2x =
        lwosCallocType(OVL_INDEX_DMEM(perfCf), 1U, PERF_CF_CONTROLLERS_2X);

    PMU_ASSERT_OK_OR_GOTO(status,
        pControllers2x != NULL ? FLCN_OK : FLCN_ERR_NO_FREE_MEM,
        perfCfControllersPostInit_2X);

    *ppControllers = &pControllers2x->super;

    if (perfCfControllerMemTuneIsLhrEnhancementFuseEnabled())
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfControllerMemTuneParseVbiosPorMclk_HAL(),
            perfCfControllersPostInit_2X);    
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfControllersPostInit_SUPER(ppControllers),
        perfCfControllersPostInit_2X);

perfCfControllersPostInit_2X:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
perfCfControllerIfaceModel10GetStatus_2X
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
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_UTIL_2X:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_UTIL_2X))
            {
                return perfCfControllerIfaceModel10GetStatus_UTIL_2X(pModel10, pBuf);
            }
            break;
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
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_MEM_TUNE_1X:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MEM_TUNE))
            {
                return perfCfControllerIfaceModel10GetStatus_MEM_TUNE(pModel10, pBuf);
            }
            break;
        }
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_MEM_TUNE_2X:
        {
            if (perfCfControllerMemTune2xIsEnabled())
            {
                return perfCfControllerIfaceModel10GetStatus_MEM_TUNE_2X(pModel10, pBuf);
            }
            break;
        }
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}


/*!
 * @brief Similar to PerfCfControllersStatusGet, but supports reduced set
 *        of controllers since it is called internal to PMU
 */
FLCN_STATUS
perfCfControllersGetReducedStatus_2X
(
    PERF_CF_CONTROLLERS_2X              *pControllers2x,
    PERF_CF_CONTROLLERS_REDUCED_STATUS  *pStatus
)
{
    PERF_CF_CONTROLLER  *pController;
    LwBoardObjIdx        i;
    FLCN_STATUS          status = FLCN_OK;

    PMU_ASSERT_OK_OR_GOTO(status,
        (boardObjGrpMaskIsSubset(&(pStatus->mask),
        &(pControllers2x->controllersReducedMask)) ?
        FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT),
    perfCfControllersGetReducedStatus_2X_exit);

    BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_CONTROLLER, pController, i, &(pStatus->mask.super))
    {
        BOARDOBJ_IFACE_MODEL_10 *pObjModel10 =
            boardObjIfaceModel10FromBoardObjGet(&pController->super);

        status = s_perfCfControllerReducedIfaceModel10GetStatus_2X(
            pObjModel10,
            &pStatus->controllers[i].boardObj);

        if (status != FLCN_OK)
        {
            goto perfCfControllersGetReducedStatus_2X_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

perfCfControllersGetReducedStatus_2X_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_perfCfControllerIfaceModel10SetEntry_2X
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
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_UTIL_2X:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_UTIL_2X))
            {
                return perfCfControllerGrpIfaceModel10ObjSetImpl_UTIL_2X(pModel10, ppBoardObj,
                            LW_SIZEOF32(PERF_CF_CONTROLLER_UTIL_2X), pBuf);
            }
            break;
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
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_MEM_TUNE_1X:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MEM_TUNE))
            {
                return perfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE(pModel10, ppBoardObj,
                            LW_SIZEOF32(PERF_CF_CONTROLLER_MEM_TUNE), pBuf);
            }
            break;
        }
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_MEM_TUNE_2X:
        {
            if (perfCfControllerMemTune2xIsEnabled())
            {
                return perfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE_2X(pModel10, ppBoardObj,
                            LW_SIZEOF32(PERF_CF_CONTROLLER_MEM_TUNE_2X), pBuf);
            }
            break;
        }
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @brief Construct mask of supported controllers by
 *        @ref PERF_CF_CONTROLLERS_REDUCED_STATUS
 *
 * @param[in/out] pControlllers pointer to PERF_CF_CONTROLLERS
 *
 * @return FLCN_OK mask contstructed successfully
 */
static FLCN_STATUS
s_perfCfControllersConstructReducedControllersMask_2X
(
    PERF_CF_CONTROLLERS_2X *pControllers2x
)
{
    PERF_CF_CONTROLLER  *pController;
    LwBoardObjIdx       i;    

    BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_CONTROLLER, pController, i, NULL)
    {
        switch (BOARDOBJ_GET_TYPE(&pController->super))
        {
            case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_UTIL:
            {
                boardObjGrpMaskBitSet(&pControllers2x->controllersReducedMask, i);
                break;
            }
            case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_UTIL_2X:
            {
                boardObjGrpMaskBitSet(&pControllers2x->controllersReducedMask, i);
                break;
            }
            case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_OPTP_2X:
            case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_DLPPC_1X:
            case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_MEM_TUNE_1X:
            case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_MEM_TUNE_2X:
            {
                break;
            }
            default:
            {
                PMU_BREAKPOINT();
                break;
            }
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    return FLCN_OK;
}

/*!
 * @Copydoc BoardObjIfaceModel10GetStatus
 */
static FLCN_STATUS
s_perfCfControllerReducedIfaceModel10GetStatus_2X
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
            return perfCfControllerGetReducedStatus_UTIL(pModel10, pBuf);
        }
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_UTIL_2X:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_UTIL_2X))
            {
                return perfCfControllerGetReducedStatus_UTIL_2X(pModel10, pBuf);
            }
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

    return FLCN_ERR_NOT_SUPPORTED;
}
