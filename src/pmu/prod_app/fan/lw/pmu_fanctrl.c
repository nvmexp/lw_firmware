/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_fanctrl.c
 * @brief   PMU fan control
 *
 * The following is a fan-control module that aims to control both the GPU
 * fanspeed and ramp-rate.
 *
 * This code lives as a sub-task of the THERM task.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "dbgprintf.h"
#include "fan/objfan.h"
#include "pmu_fanctrl.h"
#include "pmu_objpmgr.h"
#include "task_therm.h"
#include "therm/objtherm.h"
#include "rmpmusupersurfif.h"

#include "g_pmurpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * Array of FAN RM_PMU_BOARDOBJ_CMD_GRP_*** handler entries.
 */
BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_ENTRY _fanBoardObjGrpHandlers[]
    GCC_ATTRIB_SECTION(LW_ARCH_VAL("dmem_therm", "dmem_globalRodata"), "_fanBoardObjGrpHandlers") =
{
#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_COOLER))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA(FAN, FAN_COOLER,
        fanCoolersBoardObjGrpIfaceModel10Set, all.fan.fanCoolerGrpSet,
        fanCoolersBoardObjGrpIfaceModel10GetStatus, all.fan.fanCoolerGrpGetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_COOLER))

#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA(FAN, FAN_POLICY,
        fanPoliciesBoardObjGrpIfaceModel10Set, all.fan.fanPolicyGrpSet,
        fanPoliciesBoardObjGrpIfaceModel10GetStatus, all.fan.fanPolicyGrpGetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY))

#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_ARBITER))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA(FAN, FAN_ARBITER,
        fanArbitersBoardObjGrpIfaceModel10Set, all.fan.fanArbiterGrpSet,
        fanArbitersBoardObjGrpIfaceModel10GetStatus, all.fan.fanArbiterGrpGetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_ARBITER))
};

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Wrapper call assuring that all necessary overlays are swapped in (attached)
 * before we pass control to FAN code to processes and acknowledges all
 * fan-related commands sent to the PMU (either externally or internally).
 *
 * @param[in]  pDispFanctrl  Pointer to the DISPATCH_FANCTRL message to process
 */
FLCN_STATUS
fanCtrlDispatch
(
    DISPATCH_FANCTRL *pDispFanctrl
)
{
    FLCN_STATUS status = FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE;
    switch (pDispFanctrl->hdr.eventType)
    {
        case DISP2UNIT_EVT_RM_RPC:
        {
            status = pmuRpcProcessUnitFan(&(pDispFanctrl->rpc));
            break;
        }
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_FAN_STOP)
        case FANCTRL_EVENT_ID_PMGR_PWR_CHANNEL_QUERY_RESPONSE:
        {
            status = FLCN_OK;
            if (Fan.bFan2xAndLaterEnabled)
            {
                OSTASK_OVL_DESC ovlDescList[] = {
                    OSTASK_OVL_DESC_DEFINE_FAN_CTRL_COMMON_BASE
                    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, thermLibFanCommon)
                    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libFxpBasic)
                };

                OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
                {
                    fanPolicyPwrChannelQueryResponse(&pDispFanctrl->queryResponse);
                }
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            }
            break;
        }
#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_FAN_STOP)
    }

    return status;
}

/*!
 * @brief   Exelwtes FAN_LOAD RPC.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_THERM_FAN_LOAD
 */
FLCN_STATUS
pmuRpcFanLoad
(
    RM_PMU_RPC_STRUCT_FAN_LOAD *pParams
)
{
    FLCN_STATUS     status        = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_FAN_CTRL_COMMON_BASE
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, thermLibFanCommon)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPwm)
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_COOLER_HW_MAX_FAN)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libFxpBasic)
#endif
    };

    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_2X_AND_LATER) &&
        Fan.bFan2xAndLaterEnabled)
    {
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = fanLoad();
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }

    return status;
}

/*!
 * @brief   Exelwtes generic BOARD_OBJ_GRP_CMD RPC.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_FAN_BOARD_OBJ_GRP_CMD
 * @param[in]       pBuffer Scratch buffer address and size structure
 */
FLCN_STATUS
pmuRpcFanBoardObjGrpCmd
(
    RM_PMU_RPC_STRUCT_FAN_BOARD_OBJ_GRP_CMD *pParams,
    PMU_DMEM_BUFFER                         *pBuffer
)
{
    FLCN_STATUS status = FLCN_ERR_FEATURE_NOT_ENABLED;

    // Note: pBuffer->size is validated in boardObjGrpIfaceModel10GetStatus_SUPER

    switch (pParams->commandId)
    {
        case RM_PMU_BOARDOBJGRP_CMD_SET:
        {
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, smbpbi)
                OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libi2c)
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBoardObj)
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPwmConstruct)
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, thermLibFanCommonConstruct)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                status = boardObjGrpIfaceModel10CmdHandlerDispatch(
                    pParams->classId,                           // classId
                    pBuffer,                                    // pBuffer
                    LW_ARRAY_ELEMENTS(_fanBoardObjGrpHandlers), // numEntries
                    _fanBoardObjGrpHandlers,                    // pEntries
                    pParams->commandId);                        // commandId
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

            // RM requested enablement of Fan Control 2.X+.
            Fan.bFan2xAndLaterEnabled = LW_TRUE;
            break;
        }

        case RM_PMU_BOARDOBJGRP_CMD_GET_STATUS:
        {
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_DEFINE_FAN_CTRL_COMMON_BASE
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, thermLibFanCommon)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                status = boardObjGrpIfaceModel10CmdHandlerDispatch(
                    pParams->classId,                           // classId
                    pBuffer,                                    // pBuffer
                    LW_ARRAY_ELEMENTS(_fanBoardObjGrpHandlers), // numEntries
                    _fanBoardObjGrpHandlers,                    // pEntries
                    pParams->commandId);                        // commandId
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
