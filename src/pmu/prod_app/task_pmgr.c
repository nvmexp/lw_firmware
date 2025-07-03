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
 * @file   task_pmgr.c
 * @brief  PMU PMGR Task
 *
 * The PMGR task is responsible for the monitoring and handling of all things
 * board-specific: power-sensing, voltage-control, etc ...
 *
 * <more detailed description to follow>
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "main_init_api.h"
#include "cmdmgmt/cmdmgmt.h"
#include "task_pmgr.h"
#include "unit_api.h"
#include "os/pmu_lwos_task.h"
#include "pmu_i2capi.h"
#include "pmu_objpmu.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrdev.h"
#include "pmu_objperf.h"
#include "pmu_oslayer.h"
#include "dbgprintf.h"
#include "lib/lib_gpio.h"
#include "pmu_objpsi.h"

#include "g_pmurpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * Initializes the delay value to be used by the PMUTASK_PMGR loop
 */
#define LOOP_DELAY_START(first)                                                \
{                                                                              \
    (first) = lwrtosTaskGetTickCount32();                                      \
}

/*!
 * Updates the delay value for the PMUTASK_PMGR loop to account for receiving
 * commands and drift from the actual monitroing/capping algorithms.
 */
#define LOOP_DELAY_UPDATE(delay, first)                                        \
{                                                                              \
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR) &&                        \
        (Pmgr.pPwrMonitor != NULL) &&                                          \
        (Pmgr.pPwrMonitor->type !=                                             \
            RM_PMU_PMGR_PWR_MONITOR_TYPE_NO_POLLING) &&                        \
        Pmgr.bLoaded)                                                          \
    {                                                                          \
        if (lwrtosMAX_DELAY != ((delay) = pwrMonitorDelay(Pmgr.pPwrMonitor)))  \
        {                                                                      \
            LwU32 diff;                                                        \
            diff = (first) + (delay) - lwrtosTaskGetTickCount32();             \
            (delay) = (diff > delay) ? 0 : diff;                               \
        }                                                                      \
    }                                                                          \
    else                                                                       \
    {                                                                          \
        (delay) = lwrtosMAX_DELAY;                                             \
    }                                                                          \
}

/* ------------------------- Prototypes ------------------------------------- */

static FLCN_STATUS s_pmgrEventHandle(DISPATCH_PMGR *pRequest);
#if (!PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_3X_ONLY))
static void _task6PmgrBodyLegacyImplemetation(void);
#endif
static FLCN_STATUS s_pmgrPgCallbackTrigger(LwBool bSchedule)
    GCC_ATTRIB_SECTION("imem_libPsiCallback", "s_pmgrPgCallbackTrigger")
    GCC_ATTRIB_NOINLINE();

lwrtosTASK_FUNCTION(task_pmgr, pvParameters);

/* ------------------------- Static Variables ------------------------------- */
/*!
 * @brief Union of all possible RPC subsurface data that can be handled by the PMGR task.
 */
typedef union
{
    RM_PMU_PMGR_PWR_EQUATION_BOARDOBJ_GRP_SET        pwrEquationGrpSet;

    RM_PMU_PMGR_PWR_DEVICE_BOARDOBJ_GRP_SET          pwrDeviceGrpSet;
    RM_PMU_PMGR_PWR_DEVICE_BOARDOBJ_GRP_GET_STATUS   pwrDeviceGrpGetStatus;

    RM_PMU_PMGR_I2C_DEVICE_BOARDOBJ_GRP_SET          i2cDeviceGrpSet;

    RM_PMU_PMGR_ILLUM_DEVICE_BOARDOBJ_GRP_SET        illumDeviceGrpSet;

    RM_PMU_PMGR_ILLUM_ZONE_BOARDOBJ_GRP_SET          illumZoneGrpSet;

    /*!
     * @brief Default size, as defined by configuration
     */
    LwU8 defaultSize[OSTASK_ATTRIBUTE_PMGR_CMD_SCRATCH_SIZE];
} TASK_SURFACE_UNION_PMGR;

/*!
 * @brief   Defines the command buffer for the PMGR task.
 */
PMU_TASK_CMD_BUFFER_DEFINE_SCRATCH_SIZE(PMGR, "dmem_pmgrCmdBuffer");

/* ------------------------- Global Variables ------------------------------- */
/*!
 * Array of PMGR RM_PMU_BOARDOBJ_CMD_GRP_*** handler entries.
 */
BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_ENTRY _pmgrBoardObjGrpHandlers[]
    GCC_ATTRIB_SECTION(LW_ARCH_VAL("dmem_pmgr", "dmem_globalRodata"), "_pmgrBoardObjGrpHandlers") =
{
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_EQUATION))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA_SET_ONLY(PMGR, PWR_EQUATION,
        pwrEquationBoardObjGrpIfaceModel10Set, all.pmgr.pwrEquationGrpSet),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_EQUATION))

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA(PMGR, PWR_DEVICE,
        pwrDeviceBoardObjGrpIfaceModel10Set, all.pmgr.pwrDeviceGrpSet,
        pwrDeviceBoardObjGrpIfaceModel10GetStatus, all.pmgr.pwrDeviceGrpGetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE))

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE(PMGR, PWR_POLICY,
        pwrPolicyBoardObjGrpIfaceModel10Set,
        pwrPolicyBoardObjGrpIfaceModel10GetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY))

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_I2C_DEVICE))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA_SET_ONLY(PMGR, I2C_DEVICE,
        i2cDeviceBoardObjGrpIfaceModel10Set, all.pmgr.i2cDeviceGrpSet),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_PMGR_I2C_DEVICE))

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_ILLUM_DEVICE))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA_SET_ONLY(PMGR, ILLUM_DEVICE,
        illumDeviceBoardObjGrpIfaceModel10Set, turingAndLater.illum.illumDeviceGrpSet),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_PMGR_ILLUM_DEVICE))

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_ILLUM_ZONE))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA_SET_ONLY(PMGR, ILLUM_ZONE,
        illumZoneBoardObjGrpIfaceModel10Set, turingAndLater.illum.illumZoneGrpSet),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_PMGR_ILLUM_ZONE))

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR))
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_2X))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE(PMGR, PWR_CHANNEL,
        pwrChannelBoardObjGrpIfaceModel10Set,
        pwrChannelBoardObjGrpIfaceModel10GetStatus),
#else
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_SET_ONLY(PMGR, PWR_CHANNEL,
        pwrChannelBoardObjGrpIfaceModel10Set),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_2X))
#endif // (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR))
};

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief      Initialize the PMGR Task.
 *
 * @return     FLCN_OK on success,
 * @return     descriptive error code otherwise.
 */
FLCN_STATUS
pmgrPreInitTask(void)
{
    FLCN_STATUS status = FLCN_OK;
    //
    // PMGR should not need all 4 slots, but temporarily increasing from 1 => 4
    // for bug 940576, where 1 slot caused a sort of priority ilwersion (caused
    // by several other issues as well).
    //
    LwU32 queueSize = 4;

    //
    // 16 bins of 2^15[ns] == ~32.8[us] ending at ~524[us]
    // 16 bins of 2^18[ns] == ~262[us] last bin starting at ~4.2[ms]
    //
    OSTASK_CREATE(status, PMU, PMGR);
    if (status != FLCN_OK)
    {
        goto pmgrPreInitTask_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_TOPOLOGY_2X_THERM_PWR_CHANNEL_QUERY))
    {
        // Accomodate event sent by THERM/FAN task for Fan Stop sub-policy.
        queueSize++;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_CALLBACKS_CENTRALIZED))
    {
        // Assure that we have queue slot for each callback.
        queueSize++;    // OsCBPsi
        queueSize++;    // OsCBPwrPolicy
        queueSize++;    // OsCBPwrDev

        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_CHANNELS_PMUMON))
        {
            queueSize++;    // OsCBPwrChannelsPmuMon
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_CHANNEL_35))
        {
            queueSize++;    // OsCBPwrChannels
        }
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PERF_VF_ILWALIDATION_NOTIFY))
    {
        queueSize += 2;    // Reserve two slots for VF ilwalidation notification.
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BEACON_INTERRUPT))
    {
        // Queue slot for beacon interrupt from GPUADC power device
        queueSize++;
    }

    status = lwrtosQueueCreateOvlRes(&LWOS_QUEUE(PMU, PMGR), queueSize,
                                     sizeof(DISPATCH_PMGR));
    if (status != FLCN_OK)
    {
        goto pmgrPreInitTask_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_I2C_INT))
    {
        status = lwrtosQueueCreateOvlRes(&Pmgr.i2cQueue, 1,
                                         sizeof(I2C_INT_MESSAGE));
        if (status != FLCN_OK)
        {
            goto pmgrPreInitTask_exit;
        }
    }

pmgrPreInitTask_exit:
    return status;
}

/*!
 * Entry point for the PMGR task. This task does not receive any parameters.
 */
lwrtosTASK_FUNCTION(task_pmgr, pvParameters)
{
    FLCN_STATUS status;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPmgrInit)
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_SEMAPHORE)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libInit)
#endif // PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_SEMAPHORE)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = pmgrPostInit();
        if (status != FLCN_OK)
        {
            PMU_HALT();
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_3X_ONLY))
    {
        DISPATCH_PMGR dispatch;

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_CALLBACKS_CENTRALIZED))
        {
            LWOS_TASK_LOOP(LWOS_QUEUE(PMU, PMGR), &dispatch, status, PMU_TASK_CMD_BUFFER_PTRS_GET(PMGR))
            {
                status = s_pmgrEventHandle(&dispatch);
            }
            LWOS_TASK_LOOP_END(status);
        }
#else
        {
            lwosVarNOP(PMU_TASK_CMD_BUFFER_PTRS_GET(PMGR));

            for (;;)
            {
                OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_START(&PmgrOsTimer, LWOS_QUEUE(PMU, PMGR),
                       &dispatch, lwrtosMAX_DELAY)

                    if (FLCN_OK != s_pmgrEventHandle(&dispatch))
                    {
                        PMU_HALT();
                    }

                OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_STOP(&PmgrOsTimer, lwrtosMAX_DELAY)
            }
        }
#endif
    }
#else
    {
        lwosVarNOP(PMU_TASK_CMD_BUFFER_PTRS_GET(PMGR));

        //
        // In GM10X series, few chips uses policy 2X whereas other uses 3X
        // AS the legacy implementation works for both 2X and 3X implementation
        // Please dont change the condition check to power policy _2X or _3X.
        //
        _task6PmgrBodyLegacyImplemetation();
    }
#endif
}

/*!
 * @brief   Exelwtes generic BOARD_OBJ_GRP_CMD RPC.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_PMGR_BOARD_OBJ_GRP_CMD
 * @param[in]       pBuffer Scratch buffer address and size structure
 */
FLCN_STATUS
pmuRpcPmgrBoardObjGrpCmd
(
    RM_PMU_RPC_STRUCT_PMGR_BOARD_OBJ_GRP_CMD *pParams,
    PMU_DMEM_BUFFER                          *pBuffer
)
{
    FLCN_STATUS status = FLCN_ERR_FEATURE_NOT_ENABLED;

    // Note: pBuffer->size is validated in boardObjGrpIfaceModel10GetStatus_SUPER

    switch (pParams->commandId)
    {
        case RM_PMU_BOARDOBJGRP_CMD_SET:
        {
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBoardObj)
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibConstruct)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                status = boardObjGrpIfaceModel10CmdHandlerDispatch(
                    pParams->classId,                               // classId
                    pBuffer,                                        // pBuffer
                    LW_ARRAY_ELEMENTS(_pmgrBoardObjGrpHandlers),    // numEntries
                    _pmgrBoardObjGrpHandlers,                       // pEntries
                    pParams->commandId);                            // commandId
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            break;
        }

        case RM_PMU_BOARDOBJGRP_CMD_GET_STATUS:
        {
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBoardObj)
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibQuery)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                status = boardObjGrpIfaceModel10CmdHandlerDispatch(
                    pParams->classId,                               // classId
                    pBuffer,                                        // pBuffer
                    LW_ARRAY_ELEMENTS(_pmgrBoardObjGrpHandlers),    // numEntries
                    _pmgrBoardObjGrpHandlers,                       // pEntries
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

/*!
 * @brief   Exelwtes generic PWR_DEV_VOLTAGE_CHANGE RPC.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_PMGR_PWR_DEV_VOLTAGE_CHANGE
 */
FLCN_STATUS
pmuRpcPmgrPwrDevVoltageChange
(
    RM_PMU_RPC_STRUCT_PMGR_PWR_DEV_VOLTAGE_CHANGE *pParams
)
{
    FLCN_STATUS status = FLCN_OK;

    // Validate the device index
    if ((BIT(pParams->pwrDevIdx) & Pmgr.pwr.devices.objMask.super.pData[0]) == 0)
    {
        status = FLCN_ERR_OBJECT_NOT_FOUND;
        goto pmuRpcPmgrPwrDevVoltageChange_exit;
    }

    status =
        pwrDevVoltageSetChanged(&(PWR_DEVICE_GET(pParams->pwrDevIdx)->super),
                                pParams->voltageuV);

pmuRpcPmgrPwrDevVoltageChange_exit:
    return status;
}

/*!
 * @brief   Exelwtes generic PWR_CHANNELS_QUERY_LEGACY RPC.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_PMGR_PWR_CHANNELS_QUERY_LEGACY
 */
FLCN_STATUS
pmuRpcPmgrPwrChannelsQueryLegacy
(
    RM_PMU_RPC_STRUCT_PMGR_PWR_CHANNELS_QUERY_LEGACY *pParams
)
{
    FLCN_STATUS status;

    // If PWR_MONITOR not allocated yet, we're obviously not supported
    if (Pmgr.pPwrMonitor != NULL)
    {
        status = pwrMonitorChannelsQuery(Pmgr.pPwrMonitor, pParams->channelMask,
            &(pParams->payload));
    }
    else
    {
        status = FLCN_ERR_NOT_SUPPORTED;
    }

    return status;
}

/*!
 * @brief   Exelwtes generic PERF_TABLES_INFO_SET RPC.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_PMGR_PERF_TABLES_INFO_SET
 */
FLCN_STATUS
pmuRpcPmgrPerfTablesInfoSet
(
    RM_PMU_RPC_STRUCT_PMGR_PERF_TABLES_INFO_SET *pParams
)
{
    return perfProcessTablesInfo(&(pParams->data));
}

/*!
 * @brief   Exelwtes generic PERF_STATUS_UPDATE RPC.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_PMGR_PERF_STATUS_UPDATE
 */
FLCN_STATUS
pmuRpcPmgrPerfStatusUpdate
(
    RM_PMU_RPC_STRUCT_PMGR_PERF_STATUS_UPDATE *pParams
)
{
    return perfProcessPerfStatus(&(pParams->perfStatus));
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * Helper function handling the legacy power policy 2X and 1X dependency code.
 *
 * The new implementation of 3X will be shifting power data sampling from power
 * channel centralized to power policy centralized. In the legacy policy 2X,
 * power monitor keeps on polling each power channels and continuously refreshing
 * latest values polled from each channel. Power policies then go and read
 * channels they need, and then evaluate.
 * In the new policy 3X infrastructure, power monitor doesn't just blindly poll
 * all power channels, but instead will poll a set of channels that are requested
 * by power policies that need them. This gives the benefit that different power
 * policies can refresh and evaluate at different timescale independently.
 *
 * WIKI: https://wiki.lwpu.com/engwiki/index.php/Resman/GPU_Boost/Multiple_Timescale_Support
 */
#if (!PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_3X_ONLY))
static void
_task6PmgrBodyLegacyImplemetation()
{
    DISPATCH_PMGR dispatch;
    LwU32         ticksDelay = lwrtosMAX_DELAY;
    LwU32         ticksFirst = 0;
    LwBool        bCompleted = LW_FALSE;

    for (;;)
    {
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_START(&PmgrOsTimer, LWOS_QUEUE(PMU, PMGR),
               &dispatch, ticksDelay)

            if (FLCN_OK != s_pmgrEventHandle(&dispatch))
            {
                PMU_HALT();
            }

            //
            // This catches the case where PMGR has just loaded and need to
            // switch from lwrtosMAX_DELAY to the delay specified by the
            // PWR_MONITOR
            //
            if ((ticksDelay == lwrtosMAX_DELAY) &&
                PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR) &&
                (Pmgr.pPwrMonitor != NULL) &&
                (Pmgr.pPwrMonitor->type !=
                    RM_PMU_PMGR_PWR_MONITOR_TYPE_NO_POLLING) &&
                Pmgr.bLoaded)
            {
                break;
            }
            LOOP_DELAY_UPDATE(ticksDelay, ticksFirst);
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_STOP(&PmgrOsTimer, ticksDelay)

        LOOP_DELAY_START(ticksFirst);

        //
        // Update per device internal state for policy 2X and beforehand
        // For power policy 3X we had created seperate callback infrastructure
        // in pwrdev.c
        //
        if (PMUCFG_FEATURE_ENABLED_STATUS(PMU_PMGR_PWR_DEVICE_BA12X) == FLCN_OK)
        {
            LwU8    d;
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrPwrDeviceStateSync)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                FOR_EACH_INDEX_IN_MASK(32, d, Pmgr.pwr.devices.objMask.super.pData[0])
                {
                    (void)pwrDevStateSync(PWR_DEVICE_GET(d));
                }
                FOR_EACH_INDEX_IN_MASK_END;
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        }

        // If we have a valid PWR_MONITOR, call into it
        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR) &&
            (Pmgr.pPwrMonitor != NULL) &&
            (Pmgr.pPwrMonitor->type !=
                RM_PMU_PMGR_PWR_MONITOR_TYPE_NO_POLLING) &&
            Pmgr.bLoaded)
        {
            // Call the PwrMonitor (code)
            bCompleted = pwrMonitor(Pmgr.pPwrMonitor);

            //
            // If the PwrMonitor (completed) its latest iteration, pass onto any
            // objects (PWR_CAP, PWR_POLICY) which might need to take action.
            //
            if (bCompleted &&
                PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_2X))
            {
                PWR_POLICIES *pPolicies = PWR_POLICIES_GET();
                // PWR_POLICY - Generic power policies for KEPLER+.
                if ((pPolicies != NULL) &&
                    (RM_PMU_PMGR_PWR_POLICY_DESC_TABLE_VERSION_2X == pPolicies->version))
                {
                    PWR_POLICIES_EVALUATE_PROLOGUE(pPolicies);
                    {
                        // Previous to pwrpolicy_3X we evaluated all policies.
                        pwrPoliciesEvaluate_2X(pPolicies,
                                            Pmgr.pwr.policies.objMask.super.pData[0],
                                            NULL);
                    }
                    PWR_POLICIES_EVALUATE_EPILOGUE(pPolicies);
                }
            }
        }

        LOOP_DELAY_UPDATE(ticksDelay, ticksFirst);
    }
}
#endif

/*!
 * @brief   Helper call handling events sent to PMGR task.
 */
static FLCN_STATUS
s_pmgrEventHandle
(
    DISPATCH_PMGR *pRequest
)
{
    FLCN_STATUS status = FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE;

    switch (pRequest->hdr.eventType)
    {
        case DISP2UNIT_EVT_RM_RPC:
        {
            status = FLCN_ERR_QUEUE_TASK_ILWALID_UNIT_ID;

            switch (pRequest->hdr.unitId)
            {
                case RM_PMU_UNIT_PMGR:
                {
                    status = pmuRpcProcessUnitPmgr(&(pRequest->rpc));
                    break;
                }
            }
            break;
        }
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_TOPOLOGY_2X_THERM_PWR_CHANNEL_QUERY)
        case PMGR_EVENT_ID_THERM_PWR_CHANNEL_QUERY:
        {
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibPwrMonitor)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                status = pmgrPwrMonitorChannelQuery(&pRequest->query);
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            break;
        }
#endif //PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_TOPOLOGY_2X_THERM_PWR_CHANNEL_QUERY)

        case PMGR_EVENT_ID_PG_CALLBACK_TRIGGER:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PSI_PMGR_SLEEP_CALLBACK);
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPsiCallback)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                // Schedule callback for sleep current callwlation
                status = s_pmgrPgCallbackTrigger(pRequest->trigger.bSchedule);
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            break;
        }
        case PMGR_EVENT_ID_PERF_VOLT_CHANGE_NOTIFY:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_VOLT_STATE_SYNC);
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrPwrDeviceStateSync)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                pwrDevVoltStateSync();
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            status = FLCN_OK;
            break;
        }
        case PMGR_EVENT_ID_PERF_VF_ILWALIDATION_NOTIFY:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PERF_VF_ILWALIDATION_NOTIFY);
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_DEFINE_PWR_POLICIES_VF_ILWALIDATE
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                status = pwrPoliciesVfIlwalidate(PWR_POLICIES_GET());
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            break;
        }
        case PMGR_EVENT_ID_PWR_DEV_BEACON_INTERRUPT:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BEACON_INTERRUPT))
            {
                if ((PMGR_PWR_DEV_IDX_BEACON_GET(&Pmgr) !=
                        LW2080_CTRL_BOARDOBJ_IDX_ILWALID) &&
                    !PWR_DEVICE_IS_VALID(PMGR_PWR_DEV_IDX_BEACON_GET(&Pmgr)))
                {
                    status = FLCN_ERR_ILWALID_STATE;
                    PMU_BREAKPOINT();
                    break;
                }

                OSTASK_OVL_DESC ovlDescList[] = {
                    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibPwrMonitor)
                };

                OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
                {
                    if (PMGR_PWR_DEV_IDX_BEACON_GET(&Pmgr) !=
                            LW2080_CTRL_BOARDOBJ_IDX_ILWALID)
                    {
                        pwrDevGpuAdc1xReset((PWR_DEVICE_GPUADC1X *)
                            PWR_DEVICE_GET(PMGR_PWR_DEV_IDX_BEACON_GET(&Pmgr)));
                    }
                    pmgrPwrDevBeaconInterruptClearAndReenable_HAL(void);
                    status = FLCN_OK;
                }
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            }
            break;
        }
    }

    return status;
}

/*!
 *  @brief  Schedules relwrring callback for PSI to callwlate sleep current.
 *
 *  @param[in]  bSchedule   Indicates if callback is to be scheduled or
 *      de-scheduled.
 *
 *  @return  FLCN_OK if the transaction was successful, error otherwise.
 */
static FLCN_STATUS
s_pmgrPgCallbackTrigger
(
    LwBool  bSchedule
)
{
    FLCN_STATUS        status;
    PMGR_PSI_CALLBACK *pPsiCallback;

    // Get the PMGR_PSI_CALLBACK pointer
    PMU_ASSERT_OK_OR_GOTO(status,
        pmgrPsiCallbackGet(&pPsiCallback),
        s_pmgrPgCallbackTrigger_exit);

    // Sanity check
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pPsiCallback != NULL) &&
         (pPsiCallback->bRequested != bSchedule)),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_pmgrPgCallbackTrigger_exit);

    pPsiCallback->bRequested = bSchedule;

    if (Pmgr.bLoaded)
    {
        //
        // The first callback will be serviced after a finite X ms. We
        // need sleep current to have a valid value as soon as possible.
        // Hence explicitly calling this function here
        //
        if (bSchedule)
        {
            psiSleepLwrrentCalcMultiRail();
        }

        psiCallbackTriggerMultiRail(bSchedule);
        pPsiCallback->bDeferred = LW_FALSE;
    }
    else
    {
        if (bSchedule)
        {
            // PMGR is not yet loaded hence defer the schedule request
            pPsiCallback->bDeferred = LW_TRUE;
        }
        else
        {
            //
            // TODO-Tariq: Remove this else case and also remove bSchedule as a
            // parameter to this function because bSchedule is always LW_TRUE
            // when PG_CALLBACK_TRIGGER event is sent to PMGR.
            //
            // PMGR is not loaded but we can still de-schedule callbacks
            //
            psiCallbackTriggerMultiRail(bSchedule);
        }
    }

s_pmgrPgCallbackTrigger_exit:
    return status;
}
