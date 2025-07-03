/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   perfrmcmd.c
 * @brief  OBJPERF Interfaces for RM/PMU Communication
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "cmdmgmt/cmdmgmt.h"
#include "dbgprintf.h"
#include "lib/lib_perf.h"
#include "perf/2x/perfps20.h"
#include "perf/3x/vfe.h"
#include "pmu_objperf.h"
#include "pmu_objpmgr.h"
#include "pmu_objpmu.h"
#include "pmu_selwrity.h"
#include "rmpmusupersurfif.h"

#include "g_pmurpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * Array of all PERF BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_ENTRY handler entries. To be used
 * with @ref boardObjGrpIfaceModel10CmdHandlerDispatch.
 */
static BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_ENTRY perfBoardObjGrpHandlers[]
    GCC_ATTRIB_SECTION(LW_ARCH_VAL("dmem_perf", "dmem_globalRodata"), "perfBoardObjGrpHandlers") =
{
#ifdef UTF_FUNCTION_MOCKING
    {},
#else // UTF_FUNCTION_MOCKING
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PERF_LIMIT))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE(PERF, PERF_LIMIT,
        perfLimitBoardObjGrpIfaceModel10Set,
        perfLimitBoardObjGrpIfaceModel10GetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_PERF_PERF_LIMIT))

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_POLICY))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE(PERF, POLICY,
        perfPolicyBoardObjGrpIfaceModel10Set,
        perfPolicyBoardObjGrpIfaceModel10GetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_PERF_POLICY))

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_VAR))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE(PERF, VFE_VAR,
        vfeVarBoardObjGrpIfaceModel10Set,
        vfeVarBoardObjGrpIfaceModel10GetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_VAR))

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_EQU))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA_SET_ONLY(PERF, VFE_EQU,
        vfeEquBoardObjGrpIfaceModel10Set, pascalAndLater.perf.vfeEquGrpSet),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_EQU))

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VPSTATES))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_SET_ONLY(PERF, VPSTATE,
        vpstateBoardObjGrpIfaceModel10Set),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_PERF_VPSTATES))

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATE_BOARDOBJ_INTERFACES))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE(PERF, PSTATE,
        pstateBoardObjGrpIfaceModel10Set,
        pstateBoardObjGrpIfaceModel10GetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATE_BOARDOBJ_INTERFACES))
#endif // UTF_FUNCTION_MOCKING
};

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Exelwtes BOARD_OBJ_GRP_CMD RPC for the Perf CMDMGMT Unit.
 *
 * @details Depending on which CMD is specified, SET or GET_STATUS, a different
 *          set of overlays may be attached. See @ref
 *          boardObjGrpIfaceModel10CmdHandlerDispatch for more details.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_PERF_BOARD_OBJ_GRP_CMD
 */
FLCN_STATUS
pmuRpcPerfBoardObjGrpCmd
(
    RM_PMU_RPC_STRUCT_PERF_BOARD_OBJ_GRP_CMD *pParams,
    PMU_DMEM_BUFFER                          *pBuffer
)
{
    FLCN_STATUS status  = FLCN_ERR_FEATURE_NOT_ENABLED;

    // Note: pBuffer->size is validated in boardObjGrpIfaceModel10GetStatus_SUPER

    switch (pParams->commandId)
    {
        case RM_PMU_BOARDOBJGRP_CMD_SET:
        {
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPerfBoardObj)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                status = boardObjGrpIfaceModel10CmdHandlerDispatch(
                    pParams->classId,                               // classId
                    pBuffer,                                        // pBuffer
                    LW_ARRAY_ELEMENTS(perfBoardObjGrpHandlers),     // numEntries
                    perfBoardObjGrpHandlers,                        // pEntries
                    pParams->commandId);                            // commandId
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

            break;
        }

        case RM_PMU_BOARDOBJGRP_CMD_GET_STATUS:
        {
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPerfBoardObj)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                status = boardObjGrpIfaceModel10CmdHandlerDispatch(
                    pParams->classId,                               // classId
                    pBuffer,                                        // pBuffer
                    LW_ARRAY_ELEMENTS(perfBoardObjGrpHandlers),     // numEntries
                    perfBoardObjGrpHandlers,                        // pEntries
                    pParams->commandId);                            // commandId
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            break;
        }

        default:
        {
            status = FLCN_ERR_ILWALID_INDEX;
            break;
        }
    }

    return status;
}

/* ------------------------- Private Functions ------------------------------ */
