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
 * @file   task_therm.c
 * @brief  THERM task
 *
 * This code is the main THERM task, which handles servicing commmands for both
 * itself and various sub-tasks (e.g. FANCTRL and SMBPBI) as well as maintaining
 * its own 1Hz callback.
 *
 */

/* ------------------------- System Includes -------------------------------- */

/* ------------------------- Application Includes --------------------------- */
#include "task_therm.h"
#include "pmu_objpmu.h"
#include "therm/objtherm.h"
#include "therm/therm_detach.h"
#include "therm/thrmchannel_pmumon.h"
#include "therm/therm_pmumon.h"
#include "fan/objfan.h"
#include "fan/fancooler_pmumon.h"
#include "os/pmu_lwos_task.h"
#include "pmu_fanctrl.h"
#include "pmu_objpmgr.h"
#include "pmu_objtimer.h"
#include "pmu_objclk.h"
#include "pmu_selwrity.h"
#include "main.h"
#include "main_init_api.h"
#include "pmu_i2capi.h"
#include "cmdmgmt/cmdmgmt.h"
#include "rmpmusupersurfif.h"

#include "g_pmurpc.h"
#include "g_pmurmifrpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static void             s_thermInit(void);

static FLCN_STATUS      s_thermDispatch(DISPATCH_THERM *pDispTherm);

static FLCN_STATUS      s_pmuRpcThermLoadDevice(LwBool bLoad);

static FLCN_STATUS      s_pmuRpcThermObjectLoadMonitor(LwBool bLoad);

static FLCN_STATUS      s_thermEventHandle(DISP2THERM_CMD *pRequest);

lwrtosTASK_FUNCTION(task_therm, pvParameters);

/* ------------------------- Static Variables ------------------------------- */
/*!
 * @brief Union of all possible RPC subsurface data that can be handled by the THERM task.
 */
typedef union
{
    RM_PMU_THERM_THERM_CHANNEL_BOARDOBJ_GRP_SET            thermChannelBoardobjGrpSet;
    RM_PMU_THERM_THERM_CHANNEL_BOARDOBJ_GRP_GET_STATUS     thermChannelBoardobjGrpGetStatus;

    RM_PMU_THERM_THERM_DEVICE_BOARDOBJ_GRP_SET             thermDeviceBoardobjGrpSet;

    RM_PMU_THERM_THERM_MONITOR_BOARDOBJ_GRP_SET            thermMonitorBoardobjGrpSet;
    RM_PMU_THERM_THERM_MONITOR_BOARDOBJ_GRP_GET_STATUS     thermMonitorBoardobjGrpGetStatus;

    RM_PMU_THERM_THERM_POLICY_BOARDOBJ_GRP_SET             thermPolicyBoardobjGrpSet;
    RM_PMU_THERM_THERM_POLICY_BOARDOBJ_GRP_GET_STATUS      thermPolicyBoardobjGrpGetStatus;

    RM_PMU_FAN_FAN_COOLER_BOARDOBJ_GRP_SET                 fanCoolerBoardobjGrpSet;
    RM_PMU_FAN_FAN_COOLER_BOARDOBJ_GRP_GET_STATUS          fanCoolerBoardobjGrpGetStatus;

    RM_PMU_FAN_FAN_POLICY_BOARDOBJ_GRP_SET                 fanPolicyBoardobjGrpSet;
    RM_PMU_FAN_FAN_POLICY_BOARDOBJ_GRP_GET_STATUS          fanPolicyBoardobjGrpGetStatus;

    RM_PMU_FAN_FAN_ARBITER_BOARDOBJ_GRP_SET                fanArbiterBoardobjGrpSet;
    RM_PMU_FAN_FAN_ARBITER_BOARDOBJ_GRP_GET_STATUS         fanArbiterBoardobjGrpGetStatus;

    /*!
     * @brief Default size, as defined by configuration
     */
    LwU8 defaultSize[OSTASK_ATTRIBUTE_THERM_CMD_SCRATCH_SIZE];
} TASK_SURFACE_UNION_THERM;

/*!
 * @brief   Defines the command buffer for the THERM task.
 */
PMU_TASK_CMD_BUFFER_DEFINE_SCRATCH_SIZE(THERM, "dmem_thermCmdBuffer");

/* ------------------------- Global Variables ------------------------------- */
LwrtosQueueHandle ThermI2cQueue;
#if (!PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
OS_TIMER     ThermOsTimer;
#endif

/*!
 * @brief Array of THERM RM_PMU_BOARDOBJ_CMD_GRP_*** handler entries.
 */
BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_ENTRY _thermBoardObjGrpHandlers[]
    GCC_ATTRIB_SECTION(LW_ARCH_VAL("dmem_therm", "dmem_globalRodata"), "_thermBoardObjGrpHandlers") =
{
#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_DEVICE))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA_SET_ONLY(THERM, THERM_DEVICE,
        thermDeviceBoardObjGrpIfaceModel10Set, all.therm.thermDeviceGrpSet),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_DEVICE))

#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_CHANNEL))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA(THERM, THERM_CHANNEL,
        thermChannelBoardObjGrpIfaceModel10Set, all.therm.thermChannelGrpSet,
        thermChannelBoardObjGrpIfaceModel10GetStatus, all.therm.thermChannelGrpGetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_CHANNEL))

#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA(THERM, THERM_POLICY,
        thermPolicyBoardObjGrpIfaceModel10Set, all.therm.thermPolicyGrpSet,
        thermPolicyBoardObjGrpIfaceModel10GetStatus, all.therm.thermPolicyGrpGetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY))

#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_MONITOR))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA(THERM, THERM_MONITOR,
        thrmMonitorBoardObjGrpIfaceModel10Set, all.therm.thermMonitorGrpSet,
        thrmMonitorBoardObjGrpIfaceModel10GetStatus, all.therm.thermMonitorGrpGetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_MONITOR))
};

/*!
 * @brief  Thermal sensor configuration (initialized by RM).
 */
RM_PMU_THERM_CONFIG ThermConfig;

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Main task entry point and event processing loop.
 */
lwrtosTASK_FUNCTION(task_therm, pvParameters)
{
    DISP2THERM_CMD  disp2Therm = {{ 0 }};

    // Init the necessary SW and HW state
    s_thermInit();

#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    LWOS_TASK_LOOP(LWOS_QUEUE(PMU, THERM), &disp2Therm, status, PMU_TASK_CMD_BUFFER_PTRS_GET(THERM))
    {
        status = s_thermEventHandle(&disp2Therm);
    }
    LWOS_TASK_LOOP_END(status);
#else
    lwosVarNOP(PMU_TASK_CMD_BUFFER_PTRS_GET(THERM));

    for (;;)
    {
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_START(&ThermOsTimer, LWOS_QUEUE(PMU, THERM),
                                                  &disp2Therm, lwrtosMAX_DELAY);
        {
            if (FLCN_OK != s_thermEventHandle(&disp2Therm))
            {
                PMU_HALT();
            }
        }
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_STOP(&ThermOsTimer, lwrtosMAX_DELAY);
    }
#endif
}

/*!
 * @brief Pre-STATE_INIT for THERM from init overlay.
 */
void
thermPreInit_IMPL(void)
{
#if (!PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    extern LwrtosTaskHandle OsTaskTherm;

    osTimerInitTracked(OSTASK_TCB(THERM), &ThermOsTimer,
                       THERM_OS_TIMER_ENTRY_NUM_ENTRIES);
#endif

    // Structure is cleared by default.

    // Initialize the sensor A and B values so we don't need to re-read.
    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_SENSOR))
    {
        thermInitializeIntSensorAB_HAL(&Therm);
    }

    // Initialize THERM event violation monitoring.
    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_INTR))
    {
        thermEventViolationPreInit();
    }

    // Initialize RM_PMU_SW_SLOW feature.
    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_SW_SLOW))
    {
        thermSlowCtrlSwSlowInitialize();
    }

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_MEMORY_TEMPERATURE_READ_DELAY)
    // ONKARB-TODO: Add thermMemTempInit() and do the book-keeping there.
    FLCN_TIMESTAMP now;
    osPTimerTimeNsLwrrentGet(&now);

    //
    // Record timestamp and disallow temperature reads from memory devices
    // until temperature read delay expires.
    //
    Therm.bMemTempAvailable = LW_FALSE;
    Therm.memTempInitTimestamp1024ns = TIMER_TO_1024_NS(now);
#endif

    // Initialize the sub-tasks.
    if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI))
    {
        smbpbiInit();
    }
    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_2X_AND_LATER))
    {
        fanConstruct();
    }
    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY))
    {
        thermPoliciesPreInit();
    }
    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_BA_WAR_200433679))
    {
        (void)thermBaControlOverrideWarBug200433679_HAL();
    }
    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_BA_DEBUG_INIT))
    {
        (void)thermBaInitDebug_HAL();
    }
    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_CHANNEL_PMUMON))
    {
        (void)thermChannelPmumonConstruct();
    }
    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_COOLER_PMUMON))
    {
        (void)fanCoolerPmumonConstruct();
    }
}

/*!
 * @brief      Initialize the THERM Task.
 *
 * @return     FLCN_OK on success,
 * @return     descriptive error code otherwise.
 */
FLCN_STATUS
thermPreInitTask_IMPL(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 queueSize = 8;

    //
    // 16 bins of 2^15[ns] == ~32.8[us] ending at ~524[us]
    // 16 bins of 2^18[ns] == ~262[us] last bin starting at ~4.2[ms]
    //
    OSTASK_CREATE(status, PMU, THERM);
    if (status != FLCN_OK)
    {
        goto thermPreInitTask_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    {
        // Assure that we have queue slot for each callback.
        queueSize++;    // OsCBFan
        queueSize++;    // OsCBThermMonitor
        queueSize++;    // OsCBThermPolicy

        if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_CHANNEL_PMUMON))
        {
            queueSize++;    // OsCBThermChannelPmuMon
        }
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_COOLER_PMUMON))
    {
        queueSize++;    // OsCBFanCoolerPmuMon
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_PMUMON_WAR_3076546))
    {
            queueSize++;    // OsCBThermPmumon
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_PERF_VF_ILWALIDATION_NOTIFY))
    {
        queueSize += 2; // Reserve two slots for VF ilwalidation notification.
    }

    status = lwrtosQueueCreateOvlRes(&LWOS_QUEUE(PMU, THERM), queueSize,
                                     sizeof(DISP2THERM_CMD));
    if (status != FLCN_OK)
    {
        goto thermPreInitTask_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_I2C_INT))
    {
        status = lwrtosQueueCreateOvlRes(&ThermI2cQueue, 1, sizeof(I2C_INT_MESSAGE));
        if (status != FLCN_OK)
        {
            goto thermPreInitTask_exit;
        }
    }

thermPreInitTask_exit:
    return status;
}

/*!
 * @brief   Exelwtes generic BOARD_OBJ_GRP_CMD RPC.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_THERM_BOARD_OBJ_GRP_CMD
 * @param[in]       pBuffer Scratch buffer address and size structure
 */
FLCN_STATUS
pmuRpcThermBoardObjGrpCmd
(
    RM_PMU_RPC_STRUCT_THERM_BOARD_OBJ_GRP_CMD *pParams,
    PMU_DMEM_BUFFER                           *pBuffer
)
{
    FLCN_STATUS status  = FLCN_ERR_FEATURE_NOT_ENABLED;

    // Note: pBuffer->size is validated in boardObjGrpIfaceModel10GetStatus_SUPER

    switch (pParams->commandId)
    {
        case RM_PMU_BOARDOBJGRP_CMD_SET:
        {
            OSTASK_OVL_DESC ovlDescList[] = {
#if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI))
                OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, smbpbi)
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_I2C_INT))
                OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libi2c)
#endif
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBoardObj)
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, thermLibConstruct)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                status = boardObjGrpIfaceModel10CmdHandlerDispatch(
                    pParams->classId,                               // classId
                    pBuffer,                                        // pBuffer
                    LW_ARRAY_ELEMENTS(_thermBoardObjGrpHandlers),   // numEntries
                    _thermBoardObjGrpHandlers,                      // pEntries
                    pParams->commandId);                            // commandId
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            break;
        }

        case RM_PMU_BOARDOBJGRP_CMD_GET_STATUS:
        {
            OSTASK_OVL_DESC ovlDescList[] = {
#if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI))
                OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, smbpbi)
#endif
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBoardObj)
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, thermLibSensor2X)
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY)
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, thermLibPolicy)
#endif
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_MONITOR)
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, thermLibMonitor)
#endif
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                status = boardObjGrpIfaceModel10CmdHandlerDispatch(
                    pParams->classId,                               // classId
                    pBuffer,                                        // pBuffer
                    LW_ARRAY_ELEMENTS(_thermBoardObjGrpHandlers),   // numEntries
                    _thermBoardObjGrpHandlers,                      // pEntries
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
 * @brief   Exelwtes generic PMU_TASK_LOAD RPC.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_THERM_PMU_TASK_LOAD
 */
FLCN_STATUS
pmuRpcThermPmuTaskLoad
(
    RM_PMU_RPC_STRUCT_THERM_PMU_TASK_LOAD *pParams
)
{
    // Ensure that load and unload actions are symmetrical.
    if (pParams->bLoad)
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_THERM_SW_SLOW))
        {
            thermSlowCtrlSwSlowActivateArbiter(LW_TRUE);
        }

        // Add new load actions here...
    }
    else
    {
        // Add new unload actions here...

        if (PMUCFG_FEATURE_ENABLED(PMU_THERM_SW_SLOW))
        {
            thermSlowCtrlSwSlowActivateArbiter(LW_FALSE);
        }
    }

    return FLCN_OK;
}

/*!
 * @brief   Exelwtes generic INIT_CFG RPC.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_THERM_INIT_CFG
 */
FLCN_STATUS
pmuRpcThermInitCfg
(
    RM_PMU_RPC_STRUCT_THERM_INIT_CFG *pParams
)
{
    // Copy in config data.
    ThermConfig = pParams->cfgData;

    // Automatically configure power supply hot unplug shutdown event.
    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_PWR_SUPPLY_HOT_UNPLUG_SHUTDOWN_AUTO_CONFIG) &&
        (FLD_TEST_DRF(_RM_PMU_THERM, _FEATURES, _PWR_SUPPLY_HOT_UNPLUG_SHUTDOWN,
                        _ENABLED, ThermConfig.thermFeatures)))
    {
        thermPwrSupplyHotUnplugShutdownAutoConfig_HAL(&Therm);
    }

    // Apply MXM override.
    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_MXM_OVERRIDE))
    {
        thermHwPorMxmOverride_HAL(&Therm,
            ThermConfig.mxmOverrideInfo.mxmMaxTemp,
            ThermConfig.mxmOverrideInfo.mxmAssertTemp);
    }

    return FLCN_OK;
}

/*!
 * @brief   Exelwtes generic OBJECT_LOAD RPC.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_THERM_OBJECT_LOAD
 */
FLCN_STATUS
pmuRpcThermObjectLoad
(
    RM_PMU_RPC_STRUCT_THERM_OBJECT_LOAD *pParams
)
{
    FLCN_STATUS status = FLCN_OK;

    if (pParams->bLoad)
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_MONITOR))
        {
            status = s_pmuRpcThermObjectLoadMonitor(pParams->bLoad);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto pmuRpcThermObjectLoad_exit;
            }
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_CHANNEL_PMUMON))
        {
            status = thermChannelPmumonLoad();
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto pmuRpcThermObjectLoad_exit;
            }
        }

        // Load Functionality for PMU_THERM_THERM_PMUMON_WAR_3076546
        if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_PMUMON_WAR_3076546))
        {
            status = thermPmumon(pParams->bLoad);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto pmuRpcThermObjectLoad_exit;
            }
        }

        // Keep _load_ and _unload_ sections symmetrical.
    }
    else
    {
        // Keep _load_ and _unload_ sections symmetrical.

        // Unload Functionality for PMU_THERM_THERM_PMUMON_WAR_3076546
        if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_PMUMON_WAR_3076546))
        {
            status = thermPmumon(pParams->bLoad);
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_CHANNEL_PMUMON))
        {
            thermChannelPmumonUnload();
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_MONITOR))
        {
            status = s_pmuRpcThermObjectLoadMonitor(pParams->bLoad);
        }
    }

pmuRpcThermObjectLoad_exit:
    return status;
}

/*!
 * @brief   Exelwtes generic THERM_DEVICE_LOAD RPC.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_THERM_THERM_DEVICE_LOAD
 */
FLCN_STATUS
pmuRpcThermThermDeviceLoad
(
    RM_PMU_RPC_STRUCT_THERM_THERM_DEVICE_LOAD *pParams
)
{
    FLCN_STATUS status;

    if (pParams->bLoad)
    {
        status = s_pmuRpcThermLoadDevice(pParams->bLoad);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto pmuRpcThermThermDeviceLoad_exit;
        }

        // Keep _load_ and _unload_ sections symmetrical.
    }
    else
    {
        // Keep _load_ and _unload_ sections symmetrical.

        status = s_pmuRpcThermLoadDevice(pParams->bLoad);
    }

pmuRpcThermThermDeviceLoad_exit:
    return status;
}

/*!
 * @brief   Exelwtes generic CLK_CNTR_SAMPLE RPC.
 */
FLCN_STATUS
pmuRpcThermClkCntrSample
(
    RM_PMU_RPC_STRUCT_THERM_CLK_CNTR_SAMPLE *pParams
)
{
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_CLK_COUNTER
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Sample the clock counter.
        clkCntrSamplePart(pParams->clkDomain,
                          LW2080_CTRL_CLK_DOMAIN_PART_IDX_UNDEFINED,
                          &pParams->sample);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Initializes THERM task and all sub-tasks (FANCTRL, SMBPBI, etc.).
 */
static void
s_thermInit(void)
{
    //
    // a) Permanently attach the I2C IMEM library and (if applicable) DMEM
    // object overlays to this task so that it can freely call into the
    // i2c-device interfaces.
    // b) Old design was keeping entire SMBPBI functionality within THERM task.
    // It's time to ripp that off to assure that mem footprint of THERM task
    // is least possible, and to allow other THERM code to grow.
    //
    OSTASK_OVL_DESC ovlDescList[] = {
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_I2C_DEVICE))
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, i2cDevice)
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_I2C_INT))
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libi2c)
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI))
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, smbpbi)
#endif
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList); // NJ??

    // Initialize RM_PMU_SW_SLOW feature.
    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_SW_SLOW))
    {
        //
        // This particular feature is activated automatically without waiting
        // for the PMU_TASK_LOAD callback.
        //
        thermSlowCtrlSwSlowActivateArbiter(LW_TRUE);
    }

    // Initialize the sub-tasks.
    if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI))
    {
        sysMonInit();
    }
}

/*!
 * @brief   Helper call handling events sent to THERM task.
 */
static FLCN_STATUS
s_thermEventHandle
(
    DISP2THERM_CMD *pRequest
)
{
    FLCN_STATUS status = FLCN_ERR_QUEUE_TASK_ILWALID_UNIT_ID;

    // We got a event/command, so process that.
    switch (pRequest->hdr.unitId)
    {
        // Send FANCTRL events/commands to FANCTRL sub-task.
        case RM_PMU_UNIT_FAN:
        {
            //
            // The else condition needn't be handled right now as the
            // default case below ignores unknown commands and proceed
            // normally. In case default case changes, then add the
            // else condition here. Or optionally remove the if() and
            // compile out the entire case using #if
            //
            if (PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_2X_AND_LATER))
            {
                status = fanCtrlDispatch(&pRequest->fanctrl);
            }
            break;
        }

        // Send SMBPBI events/commands to SMBPBI sub-task.
        case RM_PMU_UNIT_SMBPBI:
        {
            //
            // The else condition needn't be handled right now as the
            // default case below ignores unknown commands and proceed
            // normally. In case default case changes, then add the
            // else condition here. Or optionally remove the if() and
            // compile out the entire case using #if.
            //
            if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI))
            {
                OSTASK_OVL_DESC ovlDescList[] = {
                    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, smbpbi)
                };

                OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
                {
                    status = smbpbiDispatch(&pRequest->smbpbi);
                }
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            }
            break;
        }

        // Handle THERM events/commands here.
        case RM_PMU_UNIT_THERM:
        {
            status = s_thermDispatch(&pRequest->therm);
            break;
        }
    }

    return status;
}

/*!
 * @brief Processes and acknowledges all THERM-related commands sent to the PMU (either
 * externally or internally).
 *
 * @param[in] pDispTherm  Pointer to the DISPATCH_THERM message to process
 */
static FLCN_STATUS
s_thermDispatch
(
    DISPATCH_THERM *pDispTherm
)
{
    FLCN_STATUS status = FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE;

    switch (pDispTherm->hdr.eventType)
    {
        case DISP2UNIT_EVT_RM_RPC:
        {
            status = pmuRpcProcessUnitTherm(&(pDispTherm->rpc));
            break;
        }
        // Signal from therm ISR for bottom half processing.
        case THERM_SIGNAL_IRQ:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_INTR);
            thermEventProcess();
            status = FLCN_OK;
            break;
        }

        case THERM_EVENT_ID_PERF_VF_ILWALIDATION_NOTIFY:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_PERF_VF_ILWALIDATION_NOTIFY);
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, thermLibPolicy)
                OSTASK_OVL_DESC_DEFINE_PFF_ILWALIDATE
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                status = thermPoliciesVfIlwalidate();
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            break;
        }
#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_DETACH_REQUEST))
        case THERM_DETACH_REQUEST:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_DETACH_REQUEST);
            // Handle the DETACH (allow the fan controll post driver-unload).
            thermDetachProcess();
            status = FLCN_OK;
            break;
        }
#endif // (PMUCFG_FEATURE_ENABLED(PMU_THERM_DETACH_REQUEST))
    }

    return status;
}

/*!
 * @brief   THERM_DEVICE_LOAD RPC helper for THERM_DEVICEs.
 */
static FLCN_STATUS
s_pmuRpcThermLoadDevice
(
    LwBool  bLoad
)
{
    FLCN_STATUS status = FLCN_OK;

    if (bLoad)
    {
        OSTASK_OVL_DESC ovlDescList[] = {
#if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI))
            OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, smbpbi)
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_I2C_INT))
            OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libi2c)
#endif
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBoardObj)
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, thermLibSensor2X)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = thermDevicesLoad();
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }
    else
    {
        // Nothing to unload as of now.
        lwosNOP();
    }

    return status;
}

/*!
 * @brief   OBJECT_LOAD RPC helper for THERM_MONITORs.
 */
static FLCN_STATUS
s_pmuRpcThermObjectLoadMonitor
(
    LwBool  bLoad
)
{
    FLCN_STATUS     status        = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
#if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI))
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, smbpbi)
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_I2C_INT))
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libi2c)
#endif
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBoardObj)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, thermLibMonitor)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        if (bLoad)
        {
            status = thermMonitorsLoad();
        }
        else
        {
            (void)thermMonitorsUnload();
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}
