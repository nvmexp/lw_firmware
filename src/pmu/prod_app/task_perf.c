/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_perf.c
 * @brief  PMU PERF Task
 *
 * <more detailed description to follow>
 */
/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "main_init_api.h"
#include "cmdmgmt/cmdmgmt.h"
#include "os/pmu_lwos_task.h"
#include "task_perf.h"
#include "pmu_objperf.h"
#include "pmu_objclk.h"
#include "pmu_objpmu.h"

#include "g_pmurpc.h"

/* ------------------------- Static Function Prototypes  -------------------- */
static FLCN_STATUS s_perfEventHandle(DISPATCH_PERF *pRequest);

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
lwrtosTASK_FUNCTION(task_perf, pvParameters);

/* ------------------------- Static Variables ------------------------------- */
/*!
 * @brief Union of all possible RPC subsurface data that can be handled by the PERF task.
 */
typedef union
{
    RM_PMU_PERF_VFE_EQU_BOARDOBJ_GRP_SET                     perfVfeEquBoardobjGrpSet;

    RM_PMU_VOLT_VOLT_DEVICE_BOARDOBJ_GRP_SET                 voltDeviceGrpSet;

    RM_PMU_VOLT_VOLT_POLICY_BOARDOBJ_GRP_SET                 voltPolicyGrpSet;
    RM_PMU_VOLT_VOLT_POLICY_BOARDOBJ_GRP_GET_STATUS          voltPolicyGrpGetStatus;

    RM_PMU_VOLT_VOLT_RAIL_BOARDOBJ_GRP_SET                   voltRailGrpSet;
    RM_PMU_VOLT_VOLT_RAIL_BOARDOBJ_GRP_GET_STATUS            voltRailGrpGetStatus;

    RM_PMU_CLK_CLK_ADC_DEVICE_BOARDOBJ_GRP_SET               clkAdcDeviceGrpSet;
    RM_PMU_CLK_CLK_ADC_DEVICE_BOARDOBJ_GRP_GET_STATUS        clkAdcDeviceGrpGetStatus;

    RM_PMU_CLK_CLK_DOMAIN_BOARDOBJ_GRP_SET                   clkDomainGrpSet;

    RM_PMU_CLK_CLK_FREQ_CONTROLLER_BOARDOBJ_GRP_SET          clkFreqControllerGrpSet;
    RM_PMU_CLK_CLK_FREQ_CONTROLLER_BOARDOBJ_GRP_GET_STATUS   clkFreqControllerGrpGetStatus;

    RM_PMU_CLK_CLK_NAFLL_DEVICE_BOARDOBJ_GRP_SET             clkNafllDeviceGrpSet;
    RM_PMU_CLK_CLK_NAFLL_DEVICE_BOARDOBJ_GRP_GET_STATUS      clkNafllDeviceGrpGetStatus;

    RM_PMU_CLK_CLK_PROG_BOARDOBJ_GRP_SET                     clkProgGrpSet;
    RM_PMU_CLK_CLK_PROG_BOARDOBJ_GRP_GET_STATUS              clkProgGrpGetStatus;

    RM_PMU_CLK_PLL_DEVICE_BOARDOBJ_GRP_SET                   clkPllDeviceGrpSet;

    RM_PMU_CLK_FREQ_DOMAIN_BOARDOBJ_GRP_SET                  clkFreqDomainGrpSet;
    RM_PMU_CLK_FREQ_DOMAIN_BOARDOBJ_GRP_GET_STATUS           clkFreqDomainGrpGetStatus;

    RM_PMU_CLK_CLK_VOLT_CONTROLLER_BOARDOBJ_GRP_SET          clkVoltControllerGrpSet;
    RM_PMU_CLK_CLK_VOLT_CONTROLLER_BOARDOBJ_GRP_GET_STATUS   clkVoltControllerGrpGetStatus;

    RM_PMU_CLK_CLK_PROP_REGIME_BOARDOBJ_GRP_SET              clkPropRegimeGrpSet;

    RM_PMU_CLK_CLK_PROP_TOP_BOARDOBJ_GRP_SET                 clkPropTopGrpSet;
    RM_PMU_CLK_CLK_PROP_TOP_BOARDOBJ_GRP_GET_STATUS          clkPropTopGrpGetStatus;

    RM_PMU_CLK_CLK_PROP_TOP_REL_BOARDOBJ_GRP_SET             clkPropTopRelGrpSet;

    RM_PMU_CLK_CLIENT_CLK_PROP_TOP_POL_BOARDOBJ_GRP_SET      clkClientClkPropTopPolGrpSet;

    RM_PMU_CLK_CLK_ENUM_BOARDOBJ_GRP_SET                     clkEnumGrpSet;

    RM_PMU_CLK_CLK_VF_REL_BOARDOBJ_GRP_SET                   clkVfRelGrpSet;
    RM_PMU_CLK_CLK_VF_REL_BOARDOBJ_GRP_GET_STATUS            clkVfRelGrpGetStatus;

    RM_PMU_CLK_CLIENT_CLK_VF_POINT_BOARDOBJ_GRP_SET          clientClkVfPointGrpSet;
    RM_PMU_CLK_CLIENT_CLK_VF_POINT_BOARDOBJ_GRP_GET_STATUS   clientClkVfPointGrpGetStatus;


    /*!
     * @brief Default size, as defined by configuration
     */
    LwU8 defaultSize[OSTASK_ATTRIBUTE_PERF_CMD_SCRATCH_SIZE];
} TASK_SURFACE_UNION_PERF;

/*!
 * @brief   Defines the command buffer for the PERF task.
 */
PMU_TASK_CMD_BUFFER_DEFINE_SCRATCH_SIZE(PERF, "dmem_perfCmdBuffer");

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief      Initialize the Perf Task.
 *
 * @return     FLCN_OK on success
 * @return     descriptive error code otherwise
 */
FLCN_STATUS
perfPreInitTask_IMPL(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       queueSize = 4;

    //
    // 16 bins of 2^15[ns] == ~32.8[us] ending at ~524[us]
    // 16 bins of 2^18[ns] == ~262[us] last bin starting at ~4.2[ms]
    //
    OSTASK_CREATE(status, PMU, PERF);
    if (status != FLCN_OK)
    {
        goto perfPreInitTask_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    {
        // Assure that we have queue slot for each callback.
        queueSize++;    // OsCBClkFreqController
        queueSize++;    // OsCBClkFreqEffectiveSample
        queueSize++;    // OsCBAdcVoltageSample
        queueSize++;    // OsCBClkCntrSwCntr
        queueSize++;    // OsCBVfeTimer
    }
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_30))
    {
        queueSize++;    // Asynchronous calls to RM_PMU_PERF_CMD_SET_OBJECT
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PERF_MODE))
    {
        queueSize++;    // Asynchronous calls to PERF_EVENT_ID_PERF_VFE_ILWALIDATE
    }
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PERF_LIMIT))
    {
        // Assure that we have queue slot for each arbiter client.
        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_DOMGRP_PMU_PERF_LIMITS))
        {
            queueSize++;    // power policies
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_BIF_PMU_PERF_LIMIT))
        {
            queueSize++;    // pcie limit for USB-C
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_POLICY_PMUMON))
        {
            queueSize++;    // perf policy sampling
        }
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAINS_PMUMON))
    {
        queueSize++;    // OsCBClkDomainsPmuMon
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAILS_PMUMON))
    {
        queueSize++;    // OsCBVoltRailsPmumon
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_MEM_TUNE_CHANGE))
    {
        queueSize++;    // perfChangeSeqQueueMemTuneChange
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_CONTROLLER))
    {
        queueSize++;    // perfClkControllerIlwalidate
    }

    status = lwrtosQueueCreateOvlRes(&LWOS_QUEUE(PMU, PERF), queueSize, sizeof(DISPATCH_PERF));
    if (status != FLCN_OK)
    {
        goto perfPreInitTask_exit;
    }

    // Permanently attach the perfVf to the PERF task
    if (OVL_INDEX_ILWALID != OVL_INDEX_IMEM(perfVf))
    {
        OSTASK_ATTACH_IMEM_OVERLAY_TO_TASK(OSTASK_TCB(PERF), OVL_INDEX_IMEM(perfVf));
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ELW_PSTATES_3X) &&
        (OVL_INDEX_ILWALID != OVL_INDEX_IMEM(libBoardObj)))
    {
        OSTASK_ATTACH_IMEM_OVERLAY_TO_TASK(OSTASK_TCB(PERF), OVL_INDEX_IMEM(libBoardObj));
    }

#if (!PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_AVFS)
    osTimerInitTracked(OSTASK_TCB(PERF), &PerfOsTimer,
        PERF_OS_TIMER_ENTRY_NUM_ENTRIES);
#else
    osTimerInitTracked(OSTASK_TCB(PERF), &PerfOsTimer, 0);
#endif
#endif

    // Schedule the callback for clock counter feature.
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CNTR_PERF_CALLBACK))
    {
        clkCntrSchedPerfCallback();
    }

perfPreInitTask_exit:
    return status;
}


/*!
 * Entry point for the PERF task. This task does not receive any parameters.
 */
lwrtosTASK_FUNCTION(task_perf, pvParameters)
{
    DISPATCH_PERF dispatch;

    // To do - Update this code with TU10X work for parsing the VBIOS in PMU.
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPerfInit)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libInit)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        (void)perfPostInit();
        (void)voltPostInit_IMPL();
        (void)clkPostInit();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    LWOS_TASK_LOOP(LWOS_QUEUE(PMU, PERF), &dispatch, status, PMU_TASK_CMD_BUFFER_PTRS_GET(PERF))
    {
        status = s_perfEventHandle(&dispatch);
    }
    LWOS_TASK_LOOP_END(status);
#else
    lwosVarNOP(PMU_TASK_CMD_BUFFER_PTRS_GET(PERF));

    for (;;)
    {
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_START(&PerfOsTimer, LWOS_QUEUE(PMU, PERF),
                                                  &dispatch, lwrtosMAX_DELAY)
        {
            if (FLCN_OK != s_perfEventHandle(&dispatch))
            {
                PMU_HALT();
            }
        }
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_STOP(&PerfOsTimer, lwrtosMAX_DELAY)
    }
#endif
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   Helper call handling events sent to PERF task.
 */
static FLCN_STATUS
s_perfEventHandle
(
    DISPATCH_PERF *pRequest
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
                case RM_PMU_UNIT_PERF:
                {
                    status = pmuRpcProcessUnitPerf(&(pRequest->rpc));
                    break;
                }
                case RM_PMU_UNIT_CLK:
                {
                    PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_CLK_IN_PERF);

                    pmuRpcProcessUnitClk(&(pRequest->rpc));
                    status = FLCN_OK; // NJ??
                    break;
                }
                case RM_PMU_UNIT_CLK_FREQ:
                {
                    PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_CLK_IN_PERF_DAEMON_PASS_THROUGH_PERF);

                #if PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER)
                    // Turn off skipping of the sweep(), as the next PMU task (PERF_DAEMON) should call the function.
                    if (PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER))
                    {
                        pRequest->rpc.fbqPtcb.bSweepSkip = LW_FALSE;
                    }
                #endif // PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER)

                    // Pass the command to the PERF_DAEMON task.
                    lwrtosQueueIdSendBlocking(LWOS_QUEUE_ID(PMU, PERF_DAEMON),
                                              &(pRequest->rpc), sizeof(pRequest->rpc));
                    status = FLCN_OK;
                    break;
                }
                case RM_PMU_UNIT_VOLT:
                {
                    PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_VOLT_IN_PERF);
                    status = voltProcessRmCmdFromPerf(&(pRequest->rpc));
                    break;
                }
            }
            break;
        }
        case PERF_EVENT_ID_PERF_CHANGE_SEQ_SCRIPT_COMPLETION:
        PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PERF_CHANGE_SEQ);
        {
            // Attach CHANGE_SEQ overlays
            OSTASK_OVL_DESC ovlDescList[] = {
                CHANGE_SEQ_OVERLAYS_BASE
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                //
                // This interface updates the change seq last completed change
                // which is read by other task such as PMGR and DISPLAY.
                //
                (void)perfChangeSeqScriptCompletion(pRequest->scriptCompletion.
                                                        completionStatus);
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

            status = FLCN_OK;
            break;
        }
        case PERF_EVENT_ID_PERF_CHANGE_SEQ_NOTIFICATION_REQUEST_REGISTER:
        PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PERF_CHANGE_SEQ);
        {
            // Attach CHANGE_SEQ overlays
            OSTASK_OVL_DESC ovlDescList[] = {
                CHANGE_SEQ_OVERLAYS_BASE
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                (void)perfChangeSeqRegisterNotification(
                        pRequest->notificationRequest.notification,
                        pRequest->notificationRequest.pNotification);
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

            status = FLCN_OK;
            break;
        }
        case PERF_EVENT_ID_PERF_CHANGE_SEQ_NOTIFICATION_REQUEST_UNREGISTER:
        PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PERF_CHANGE_SEQ);
        {
            OSTASK_OVL_DESC ovlDescList[] = {
                CHANGE_SEQ_OVERLAYS_BASE
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                (void)perfChangeSeqUnregisterNotification(
                        pRequest->notificationRequest.notification,
                        pRequest->notificationRequest.pNotification);
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

            status = FLCN_OK;
            break;
        }
        case PERF_EVENT_ID_PERF_LIMITS_CLIENT:
        PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PERF_PERF_LIMIT);
        {
            OSTASK_OVL_DESC ovlDescList[] = {
                PERF_LIMIT_OVERLAYS_BASE
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                status = perfLimitsEvtHandlerClient(&pRequest->limitsClient);
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            break;
        }
        case PERF_EVENT_ID_PERF_CLK_CONTROLLERS_ILWALIDATE:
        PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_CLK_CLK_CONTROLLER);
        {
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkController)
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkVoltController)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                // Do not need to override delta on VFE ilwalidation.
                status = perfClkControllersIlwalidate(LW_FALSE);
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            break;
        }
        case PERF_EVENT_ID_PERF_CHANGE_SEQ_QUEUE_MEM_TUNE_CHANGE:
        PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PERF_CHANGE_SEQ_MEM_TUNE_CHANGE);
        {
            // Do not need to override delta on VFE ilwalidation.
            status = perfChangeSeqQueueMemTuneChange(pRequest->memTune.tFAW);
            break;
        }
        case PERF_EVENT_ID_PERF_VFE_ILWALIDATE:
        PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PERF_VFE);
        {
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_DEFINE_VFE(FULL)
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfIlwalidation)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                status = vfeIlwalidate();
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            break;
        }
    }

    return status;
}
