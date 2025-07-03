/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   objlpwr.c
 * @brief  PMU LowPower Object
 *
 * Manages all LowPower features
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dmemovl.h"
#include "lwrtosHooks.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objdisp.h"
#include "pmu_objhal.h"
#include "pmu_objlpwr.h"
#include "pmu_objpmu.h"
#include "pmu_objgr.h"
#include "pmu_objms.h"
#include "pmu_objdi.h"
#include "pmu_objei.h"
#include "pmu_objap.h"
#include "pmu_objdfpr.h"
#include "task_perf.h"
#include "cmdmgmt/cmdmgmt.h"
#include "pmu_oslayer.h"
#include "lwostimer.h"
#include "main.h"
#include "main_init_api.h"
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE))
#include "perf/3x/vfe.h"
#endif

#include "dbgprintf.h"

#include "g_pmurpc.h"
#include "g_pmurmifrpc.h"

#include "config/g_pg_private.h"
#if(PG_MOCK_FUNCTIONS_GENERATED)
#include "config/g_pg_mock.c"
#endif

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
OBJLPWR Lpwr;

/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS s_lpwrPmuExtEventHandler(DISPATCH_LPWR_EXT_EVENT *);
static void        s_lpwrEiNotificationGrRgHandler(LwU8 notificationType);
static FLCN_STATUS s_lpwrEiNotificationMsHandler(LwU8 notificationType);
static void        s_lpwrEiNotificationSacHandler(LwU8 notificationType);
static FLCN_STATUS s_lpwrEiNotificationHandler(RM_PMU_LPWR_EI_NOTIFICATION_DATA eiNotificationData);
static FLCN_STATUS s_lpwrPerfScriptGrRgCompletionEvtHandler(PG_LOGIC_STATE *pPgLogicState);
static FLCN_STATUS s_lpwrLpRmPrefetchRequestSend(void)
                   GCC_ATTRIB_SECTION("imem_lpwrLp", "s_lpwrLpRmPrefetchRequestSend");
static FLCN_STATUS s_lpwrLpRmIdleSnapNotificationSend(LwU8 lpwrCtrlId)
                   GCC_ATTRIB_SECTION("imem_lpwrLp", "s_lpwrLpRmIdleSnapNotificationSend");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Construct the LPWR object.
 *
 * @return  FLCN_OK     On success
 */
FLCN_STATUS
constructLpwr(void)
{
    memset(&Lpwr, 0, sizeof(Lpwr));
    return FLCN_OK;
}

/*!
 * @brief Initialize LPWR
 *
 * This function does the initialization of LPWR before scheduler schedules
 * LPWR task for the exelwtion.
 */
void
lpwrPreInit(void)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_WITH_PS35))
    {
        // Initialize LPWR VBIOS
        lpwrVbiosPreInit();
    }

    lpwrCgPreInit();
}

/*!
 * @brief Initialize LPWR post task schedule
 *
 * This function does the initialization of LPWR features immediately after scheduler
 * schedules LPWR task for the exelwtion.
 */
FLCN_STATUS
lpwrPostInit(void)
{
    FLCN_STATUS status = FLCN_OK;

    // Permanently attach the overlays
    pgOverlayAttachAndLoad(PG_OVL_ID_MASK_FOREVER);

    // Initialize LPWR group related structures
    status = lpwrGrpPostInit();
    if (status != FLCN_OK)
    {
        goto lpwrPostInit_exit;
    }

    // Initialize LPWR perf related structure
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_WITH_PS35))
    {
        status = lpwrPerfPostInit();
        if (status != FLCN_OK)
        {
            goto lpwrPostInit_exit;
        }
    }

    // Initialize GR related structures
    if (PMUCFG_ENGINE_ENABLED(GR))
    {
        status = grPostInit();
        if (status != FLCN_OK)
        {
            goto lpwrPostInit_exit;
        }
    }

    // Initialize MS related structures
    if (PMUCFG_ENGINE_ENABLED(MS))
    {
        status = msPostInit();
        if (status != FLCN_OK)
        {
            goto lpwrPostInit_exit;
        }
    }

    // Initialize EI related structures
    if (PMUCFG_ENGINE_ENABLED(EI))
    {
        status = eiPostInit();
        if (status != FLCN_OK)
        {
            goto lpwrPostInit_exit;
        }
    }

    // Initialize PSI related structures
    if (PMUCFG_ENGINE_ENABLED(PSI))
    {
        status = psiPostInit();
        if (status != FLCN_OK)
        {
            goto lpwrPostInit_exit;
        }
    }

    // Initialize Adaptive Power related structures
    if (PMUCFG_ENGINE_ENABLED(AP))
    {
        status = apPostInit();
        if (status != FLCN_OK)
        {
            goto lpwrPostInit_exit;
        }
    }

    // Initialize the PRIV Blocker Related structure
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_UNIFIED_PRIV_BLOCKER_CTRL))
    {
        LPWR_PRIV_BLOCKER *pLpwrPrivBlocker = LPWR_GET_PRIV_BLOCKER();

        //
        // We are allocating the memory in resident section, as we want to
        // use this structure in ISR and also the memory footprint is very
        // small.
        //
        if (pLpwrPrivBlocker == NULL)
        {
            pLpwrPrivBlocker = lwosCallocResidentType(1, LPWR_PRIV_BLOCKER);
            if (pLpwrPrivBlocker == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_NO_FREE_MEM;

                goto lpwrPostInit_exit;
            }

            Lpwr.pPrivBlocker = pLpwrPrivBlocker;
        }
    }

    // Initialize the LPWR Sequencer related structure and state
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_SEQ))
    {
        status = lpwrSeqInit();
        if (status != FLCN_OK)
        {
            goto lpwrPostInit_exit;
        }
    }

    // Initialize dfpr related structures
    if (PMUCFG_ENGINE_ENABLED(DFPR))
    {
        status = dfprPostInit();
        if (status != FLCN_OK)
        {
            goto lpwrPostInit_exit;
        }
    }

    // Initialize LPWR STAT Infrastructure
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_STATS))
    {
        status = lpwrStatInit();
        if (status != FLCN_OK)
        {
            goto lpwrPostInit_exit;
        }
    }

    // Initialize the Clock Gating related structures
    status = lpwrCgPostInit();
    if (status != FLCN_OK)
    {
        goto lpwrPostInit_exit;
    }

lpwrPostInit_exit:

    return  status;
}

/*!
 * Top level LPWR Event Handler.
 *
 * lpwrEventHandler is the top level event handler responsible for handling
 * event for all LPWR features - ELCG, GR-ELPG, TPC-PG, MSCG, DI, RPPG,
 * PSI etc.
 *
 * Event handler is doing special handling PG feature group to optimize run
 * time. It colwerts LPWR event to PG event by updating pPgLogicState. PG SM
 * does the further processing of this this event.
 *
 * @param[in]  pLpwrEvt
 *     The source of all LPWR events. This can come from RM or HW.
 *
 * @param[out] pPgLogicState   Pointer to PG Logic state
 *
 */
void
lpwrEventHandler
(
    DISPATCH_LPWR   *pLpwrEvt,
    PG_LOGIC_STATE  *pPgLogicState
)
{
    FLCN_STATUS status = FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE;

    switch (pLpwrEvt->hdr.eventType)
    {
        case LPWR_EVENT_ID_PG_INTR:
        case LPWR_EVENT_ID_PG_HOLDOFF_INTR:
        {
             // Process HW Interrupts
            if (PMUCFG_FEATURE_ENABLED(PMU_PG_PMU_ONLY))
            {
                status = pgProcessInterrupts_HAL(&Pg, pLpwrEvt, pPgLogicState);
            }
            break;
        }

        case LPWR_EVENT_ID_MS_WAKEUP_INTR:
        case LPWR_EVENT_ID_MS_DISP_INTR:
        case LPWR_EVENT_ID_MS_ABORT_DMA_SUSP:
        case LPWR_EVENT_ID_MS_OSM_NISO_START:
        case LPWR_EVENT_ID_MS_OSM_NISO_END:
        {
            // Process MS group specific Events/Interrupts
            if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MS))
            {
                if (lpwrGrpIsCtrlSupported(RM_PMU_LPWR_GRP_CTRL_ID_MS))
                {
                    status = msProcessInterrupts(pLpwrEvt, pPgLogicState);
                }
                else
                {
                    status = FLCN_OK;
                }
            }
            break;
        }

        case DISP2UNIT_EVT_RM_RPC:
        {
            // Process RM Commands
            status = FLCN_ERR_QUEUE_TASK_ILWALID_UNIT_ID;

            if (pLpwrEvt->hdr.unitId == RM_PMU_UNIT_LPWR)
            {
                status = pmuRpcProcessUnitLpwr(&(pLpwrEvt->rpc), pPgLogicState);
            }
            else if (pLpwrEvt->hdr.unitId == RM_PMU_UNIT_LPWR_LOADIN)
            {
                pgOverlayAttachAndLoad(PG_OVL_ID_MASK_LPWR_LOADIN);
                status = pmuRpcProcessUnitLpwrLoadin(&(pLpwrEvt->rpc));
                pgOverlayDetach(PG_OVL_ID_MASK_LPWR_LOADIN);
            }
            break;
        }

        case LPWR_EVENT_ID_PMU:
        {
            // Update PG Logic state based on event
            if (PMUCFG_FEATURE_ENABLED(PMU_PG_PMU_ONLY))
            {
                pPgLogicState->ctrlId  = pLpwrEvt->fsmEvent.ctrlId;
                pPgLogicState->eventId = pLpwrEvt->fsmEvent.eventId;
                pPgLogicState->data    = pLpwrEvt->fsmEvent.data;

                status = FLCN_OK;
            }
            break;
        }

        case LPWR_EVENT_ID_PMU_EXT:
        {
            // Process External Events(Events from other tasks within PMU)
            if (PMUCFG_FEATURE_ENABLED(PMU_PG_PMU_ONLY))
            {
                status = s_lpwrPmuExtEventHandler(&pLpwrEvt->extEvent);
            }
            break;
        }

       case LPWR_EVENT_ID_PRIV_BLOCKER_WAKEUP_INTR:
       {
            // Process priv blocker interrupt
            if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_UNIFIED_PRIV_BLOCKER_CTRL))
            {
                status = lpwrPrivBlockerIntrProcess_HAL(&Lpwr, pLpwrEvt);
            }
            break;
        }

        case LPWR_EVENT_ID_PERF_CHANGE_NOTIFICATION:
        {
            // Process perf change event
            if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_WITH_PS35))
            {
                status = lpwrPerfChangeEvtHandler(pLpwrEvt->perfChange.data);
            }
            break;
        }

        case LPWR_EVENT_ID_EI_NOTIFICATION:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_EI))
            {
                status = s_lpwrEiNotificationHandler(pLpwrEvt->eiNotification.data);
            }
            break;
        }

        case LPWR_EVENT_ID_GR_RG_PERF_SCRIPT_COMPLETION:
        {
            // Process perf script completion event
            if (PMUCFG_FEATURE_ENABLED(PMU_PG_GR_RG))
            {
                status = s_lpwrPerfScriptGrRgCompletionEvtHandler(pPgLogicState);
            }
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_HALT();
    }
}

/*!
 * Primary LPWR_LP Event Handler.
 *
 * lpwrLpEventHandler is the Primary event handler responsible for handling
 * event for all LPWR feature handled by LPWR_LP task. e.g EI - Engine Idle
 *
 * @param[in]  pLpwrLpEvt
 *     The source of all LPWR_LP events. This can come from RM or HW or
 *      other PMU Task.
 */
FLCN_STATUS
lpwrLpEventHandler
(
    DISPATCH_LPWR_LP *pRequest
)
{
    FLCN_STATUS status = FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE;

    switch (pRequest->hdr.eventType)
    {
        case DISP2UNIT_EVT_RM_RPC:
        {
            status = FLCN_ERR_QUEUE_TASK_ILWALID_UNIT_ID;

            if (pRequest->hdr.unitId == RM_PMU_UNIT_LPWR_LP)
            {
                status = pmuRpcProcessUnitLpwrLp(&(pRequest->rpc));
            }
            break;
        }

        case LPWR_LP_EVENT_EI_FSM:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_EI))
            {
                // EI FSM Events
                status = eiProcessFsmEvents(&pRequest->eiFsmEvent);
            }
            break;
        }

        case LPWR_LP_EVENT_EI_CLIENT_REQUEST:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_EI))
            {
                // PMU based EI Client Requests
                status = eiProcessClientRequest((&pRequest->eiClientRequest));
            }
            break;
        }

        case LPWR_LP_EVENT_DIFR_PREFETCH_REQUEST:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_DIFR))
            {
                // Send the DIFR Prefetch request to RM
                status = s_lpwrLpRmPrefetchRequestSend();
            }
            break;
        }

        case LPWR_LP_LPWR_FSM_IDLE_SNAP:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_LP_IDLE_SNAP_NOTIFICATION))
            {
                // Send the IDLE Sanp Notification to RM
                status = s_lpwrLpRmIdleSnapNotificationSend(pRequest->idleSnapEvent.ctrlId);
            }
            break;
        }

        case LPWR_LP_PSTATE_CHANGE_NOTIFICATION:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_WITH_PS35))
            {
                // Perform pstate change event
                status = lpwrLpPstateChangeEvtHandler(pRequest->pstateChangeNotification.data);
            }
            break;
        }

    }

    return status;
}

/*!
 * @brief Sends RTOS idle notification to LPWR task
 *
 * This function uses lwrtosQueueSendFromAtomic() because we don't want to
 * trigger task yield when we are in critical section.
 *
 * @return      LW_TRUE     lwrtosYIELD() is required
 * @return      LW_FALSE    lwrtosYIELD() is not required
 */
LwBool
lpwrNotifyRtosIdle(void)
{
    DISPATCH_LPWR lpwrEvt;
    LwBool        bYieldRequired = LW_FALSE;

    appTaskCriticalEnter();
    {
        // Send IDLE notification to LPWR task only if its enabled
        if (Pg.rtosIdleState == PG_RTOS_IDLE_EVENT_STATE_ENABLED)
        {
            FLCN_STATUS status;

            lpwrEvt.hdr.eventType    = LPWR_EVENT_ID_PMU_EXT;
            lpwrEvt.extEvent.eventId = PMU_PG_EXT_EVENT_RTOS_IDLE;

            //
            // ctrlId dont have any valid meaning for RTOS_IDLE event.
            // Initialize it to Zero.
            //
            lpwrEvt.extEvent.ctrlId = RM_PMU_LPWR_CTRL_ID_ILWALID_ENGINE;

            status = lwrtosQueueIdSendFromAtomic(LWOS_QUEUE_ID(PMU, LPWR),
                                                 &lpwrEvt, sizeof(lpwrEvt));

            if (status == FLCN_OK)
            {
                //
                // Notification successfully queued:
                // Change RTOS notification state to _QUEUED. LPWR task will
                // process this notification on next lwrtosYIELD().
                //
                Pg.rtosIdleState = PG_RTOS_IDLE_EVENT_STATE_QUEUED;
                bYieldRequired   = LW_TRUE;
            }
            else if (status == FLCN_ERR_TIMEOUT)
            {
                //
                // Timeout while sending the notification:
                // Queue was full when trying to send the idle notification.
                // This is not expected from LowPower task. Thus, assert PMU
                // breakpoint to notify the timeout.
                //
                PMU_BREAKPOINT();
            }
            else
            {
                //
                // Fatal error:
                // Call the error handling routine lwrtosHookError().
                //
                lwrtosHookError(OsTaskLpwr, status);
            }
        }
    }
    appTaskCriticalExit();

    return bYieldRequired;
}

/*!
 * @brief Check if Non-LPWR tasks are idle or not
 *
 * Non-LPWR tasks will be in suspended or delayed queue if they dont have
 * any pending work. At max we can have LPWR (i.e. PG) and IDLE tasks in
 * ready list when non-LPWR tasks dont have any work. Means total ready
 * task count will be less than or equal to two if all non-LPWR tasks
 * are idle.
 *
 * i.e. Non-LPWR tasks has some pending workload => We have more than two
 *                                                  tasks in ready list.
 *
 * This API is valid only for one task model of LPWR. One task model
 * is introduced on GP10X and onward GPUs where we have only one task
 * to handle all LPWR features. preGP10X has two tasks to manage LPWR.
 *
 * This API will not cause any fatal errors even if we enabled it on Two
 * task model. It's just that we will not get any benefits from it.
 *
 * @return LW_TRUE      If non-LPWR task dont have any work
 *         LW_FALSE     Otherwise
 */
LwBool
lpwrIsNonLpwrTaskIdle(void)
{
    LwU32 taskReadyCount = lwrtosTaskReadyTaskCountGet();

    return (taskReadyCount <= 2);
}

/*!
 * @brief Helper to handle Gr Rg perf script completion event.
 *
 * LPWR task will queue request to perf daemon task for exelwtion of
 * post processing of GPC-RG script. Upon completion of the script,
 * perf daemon task will queue this event to LPWR task.
 */
static FLCN_STATUS
s_lpwrPerfScriptGrRgCompletionEvtHandler
(
    PG_LOGIC_STATE  *pPgLogicState
)
{
    OBJPGSTATE *pGrRgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_RG);
    FLCN_STATUS status     = FLCN_OK;

    //
    // Re-enable the access to GPC clock if the corresponding clock feature is
    // enabled.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_LPWR))
    {
        if (LPWR_CTRL_IS_SF_ENABLED(pGrRgState, GR, PRIV_RING) ||
            LPWR_CTRL_IS_SF_ENABLED(pGrRgState, GR, RESET))
        {
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, clkLibClk3)
            };

            // Attach required overlays
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                status = clkFreqDomainsDisable(LW2080_CTRL_CLK_DOMAIN_GPCCLK,      // clkDomainMask
                                               LW2080_CTRL_CLK_CLIENT_ID_GR_RG,    // clientId
                                               LW_FALSE,                           // bDisable
                                               LW_FALSE);                          // bConditional
            }
            // Detach overlays
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

            // Check for the returned status now
            if (status != FLCN_OK)
            {
                PMU_HALT();
                goto s_lpwrPerfScriptGrRgCompletionEvtHandler_exit;
            }
        }
    }

    // Allow back GPC-RG with reason SELF
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_LPWR) &&
        LPWR_CTRL_IS_SF_ENABLED(pGrRgState, GR, RG_PERF_CHANGE_SEQ))
    {
        // Update debugging info.
        g_CHSEQ_LPWR_DEBUG[14]++;

        // Allow all PgCtrls except EI
        grRgPerfSeqDisallowAll(LW_FALSE);
    }

s_lpwrPerfScriptGrRgCompletionEvtHandler_exit:
    return status;
}

/*!
 * @brief Handles PState change command
 *
 * @param[in]  pParam  Param for Pstate change sent from RPC struct
 */
void
lpwrProcessPstateChange
(
    RM_PMU_RPC_STRUCT_LPWR_PSTATE_CHANGE *pParam
)
{
    //
    // These coefficients are used for Multirail PSI
    // only.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_MULTIRAIL)             &&
        PMUCFG_FEATURE_ENABLED(PMU_PSI_FLAVOUR_LWRRENT_AWARE) &&
        PMUCFG_FEATURE_ENABLED(PMU_PG_MS)                     &&
        pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_MSCG))
    {
        // Set MS sepecific dynamic current coefficient
        Ms.dynamicLwrrentCoeff[RM_PMU_PSI_RAIL_ID_LWVDD]      =
                                             pParam->msDynamicLwrrentCoeffLogic;
        Ms.dynamicLwrrentCoeff[RM_PMU_PSI_RAIL_ID_LWVDD_SRAM] =
                                             pParam->msDynamicLwrrentCoeffSram;
    }
}

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE))
/*!
 * @brief : PG Handler for VFE Dynamic Updates
 *
 * Re-evaluate DI voltage for logic and sram rails and cache
 * them in Di object
 */
void
lpwrProcessVfeDynamilwpdate(void)
{
    RM_PMU_PERF_VFE_EQU_RESULT  result;
    FLCN_STATUS                 status;
    LwU32                       railIdx;

    // Evaluation is done only if DI voltage feature is supported
    if (PMUCFG_FEATURE_ENABLED(PMU_DI_SEQUENCE_COMMON) &&
        Di.bVoltSupport)
    {
        for (railIdx = 0; railIdx < PG_VOLT_RAIL_IDX_MAX; railIdx++)
        {
            if (PG_VOLT_IS_RAIL_SUPPORTED(railIdx))
            {
                status = vfeEquEvaluate(PG_VOLT_GET_SLEEP_VFE_IDX(railIdx), NULL, 0,
                                        LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VOLT_UV, &result);
                if ((status != FLCN_OK) ||
                    (result.voltuV == 0))
                {
                    PMU_HALT();
                }

                PG_VOLT_SET_SLEEP_VOLTAGE_UV(railIdx, result.voltuV);

                pgIslandSleepVoltageUpdate_HAL(&Pg, railIdx);

                Di.bSleepVoltValid = LW_TRUE;
            }
        }
    }
}
#endif

/*!
 * @brief Send Oneshot state change response message
 */
void
lpwrOsmAckSend(void)
{
    PMU_RM_RPC_STRUCT_LPWR_PG_ASYNC_CMD_RESP rpc;
    FLCN_STATUS                              status;

    if (Pg.bOsmAckPending)
    {
        // Send the asynchronous response message
        rpc.ctrlId  = RM_PMU_LPWR_CTRL_ID_MS_MSCG;
        rpc.msgId   = RM_PMU_PG_MSG_ASYNC_CMD_ONESHOT;

        // Send the Event as an RPC.  RC-TODO, properly handle status.
        PMU_RM_RPC_EXELWTE_BLOCKING(status, LPWR, PG_ASYNC_CMD_RESP, &rpc);

        Pg.bOsmAckPending = LW_FALSE;
    }
}

/*!
 * @brief Generic function to enable/disable EI notification
 * for the client which are managed by LPWR task e.g GR-RG
 *
 * Note:
 *      Please always call this function under the check of
 *      PMU CONFIG FEATURE - PMU_LPWR_CTRL_EI i.e caller
 *      of this function has to implement this check.
 *
 * @param[in]   clientId    ID of the client as per EI Framework
 * @Param[in]   bEnable     LW_TRUE -> Request the notification enablement
 *                          LW_FALSE -> Request the notification disablement
 *
 * @return      FLCN_OK                   On success
 *              FLCN_ERR_ILWALID_ARGUMENT If client is invalid and not supported
 *                                        by the EI
 */
FLCN_STATUS
lpwrRequestEiNotificationEnable
(
    LwU8    clientId,
    LwBool  bEnable
)
{
    FLCN_STATUS status = FLCN_OK;

    // Attach the LPWR_LP DMEM Overlay to access EI object
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, lpwrLp)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        if (EI_IS_CLIENT_SUPPORTED(clientId))
        {
            DISPATCH_LPWR_LP disp2LpwrLp = {{ 0 }};

            disp2LpwrLp.eiClientRequest.hdr.eventType = LPWR_LP_EVENT_EI_CLIENT_REQUEST;
            disp2LpwrLp.eiClientRequest.clientId      = clientId;

            if (bEnable)
            {
                 disp2LpwrLp.eiClientRequest.eventId =
                                     LPWR_LP_EVENT_ID_EI_CLIENT_NOTIFICATION_ENABLE;
            }
            else
            {
                disp2LpwrLp.eiClientRequest.eventId =
                                    LPWR_LP_EVENT_ID_EI_CLIENT_NOTIFICATION_DISABLE;
            }

            status = lwrtosQueueIdSendBlocking(LWOS_QUEUE_ID(PMU, LPWR_LP),
                                               &disp2LpwrLp, sizeof(disp2LpwrLp));
        }
        else
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * Wrapper around pmuPreInitGc6Exit_HAL. To be called at init time as part of
 * the PMU Engine construct sequence.
 *
 * @return  FLCN_OK     On success
 * @return  other       Error propagated from inner functions.
 */
FLCN_STATUS
pmuPreInitGc6Exit(void)
{
    if (!DMA_ADDR_IS_ZERO(&PmuInitArgs.gc6Ctx))
    {
        return pmuPreInitGc6Exit_HAL(&Pmu);
    }
    else
    {
        return FLCN_OK;
    }
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Process external PG events
 *
 * Called by the Primary event processor, pgProcessEvents. This processes
 * PG related events came from other task in PMU.
 *
 * @param[in]   pLpwrEvt        LPWR event from PMU
 * @param[out]  pPgLogicState   Pointer to PG Logic state
 *
 * @return      FLCN_OK                                 On success
 * @return      FLCN_ERR_ILWALID_STATE                  Invalid state
 * @return      FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE  Otherwise
 */
static FLCN_STATUS
s_lpwrPmuExtEventHandler
(
    DISPATCH_LPWR_EXT_EVENT *pLpwrExtEvent
)
{
    FLCN_STATUS status = FLCN_OK;

    if ((PMUCFG_FEATURE_ENABLED(PMU_PG_TASK_MGMT)) &&
        (pLpwrExtEvent->eventId == PMU_PG_EXT_EVENT_RTOS_IDLE))
    {
        LwU32       ctrlId;
        OBJPGSTATE *pPgState;

        // Something went wrong. Keep the LowPower feature disabled.
        if (Pg.rtosIdleState != PG_RTOS_IDLE_EVENT_STATE_QUEUED)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;

            goto s_lpwrPmuExtEventHandler_exit;
        }

        //
        // Re-enable all PgCtrls that got disabled by thrashing
        // detection policy.
        //
        FOR_EACH_INDEX_IN_MASK(32, ctrlId, Pg.supportedMask)
        {
            pPgState = GET_OBJPGSTATE(ctrlId);
            if (pPgState->taskMgmt.bDisallow)
            {
                pgCtrlAllow(ctrlId, PG_CTRL_ALLOW_REASON_MASK(THRASHING));

                pPgState->taskMgmt.bDisallow = LW_FALSE;
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;

        //
        // RTOS_IDLE event is processed successfully. Disable
        // further notifications.
        //
        Pg.rtosIdleState = PG_RTOS_IDLE_EVENT_STATE_DISABLED;
    }
    else if ((PMUCFG_FEATURE_ENABLED(PMU_LPWR_ENG)) &&
             (pLpwrExtEvent->eventId == PMU_PG_EXT_EVENT_SELF_ALLOW))
    {
        //
        // If the FSM goes into Self Disallow mode and external Task input is
        // required before we can enable the FSM, then external Task need to send
        // this event and based on this event, we will allow the FSM again.
        //
        if (pgIsCtrlSupported(pLpwrExtEvent->ctrlId) &&
            GET_OBJPGSTATE(pLpwrExtEvent->ctrlId)->bSelfDisallow)
        {
            pgCtrlAllow(pLpwrExtEvent->ctrlId, PG_CTRL_ALLOW_REASON_MASK(SELF));
        }
    }
    else
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE;
    }

s_lpwrPmuExtEventHandler_exit:

    return status;
}

/*!
 * @brief Handles EI notification for GR-RG
 *
 * @param[in]  notificationType  EI Framework Entry/Exit Notification
 */
void
s_lpwrEiNotificationGrRgHandler
(
    LwU8 eiNotificationType
)
{
    if (eiNotificationType == LPWR_EI_NOTIFICATION_ENTRY_DONE)
    {
        // Allow GPC-RG to engage
        lpwrGrpCtrlMutualExclusion(RM_PMU_LPWR_GRP_CTRL_ID_GR,
                                   LPWR_GRP_CTRL_CLIENT_ID_EI, 0x0);
    }
    else
    {
        //
        // Disable GPC-RG through mutual exclusion policy. This will
        // automatically enable GR-RPG or GR-PASSIVE.
        //
        lpwrGrpCtrlMutualExclusion(RM_PMU_LPWR_GRP_CTRL_ID_GR,
                                   LPWR_GRP_CTRL_CLIENT_ID_EI,
                                   BIT(RM_PMU_LPWR_CTRL_ID_GR_RG));
    }
}

/*!
 * @brief Handles EI notification for MSCG realted sub features
 *
 * @param[in]  notificationType  EI Framework Entry/Exit Notification
 *
 * Entry Sequence for notification type LPWR_EI_NOTIFICATION_ENTRY_DONE
 *
 * 1. Enable the MS-RPG
 * 2. Enable L2_CACHE_OPS
 * 3. Enable SYS and LWD Clk SlowDown
 *
 * There is no need to explicity enable the SFM Mask as
 * by default value they are already enabled
 *
 * Exit Sequence for Notification type LPWR_EI_NOTIFICATION_EXIT_DONE
 * 1. Disable MS-RPG
 * 2. Disable L2-Flush
 * 3. Disable SYS and LWD Clk Slowdown
 */
FLCN_STATUS
s_lpwrEiNotificationMsHandler
(
    LwU8 eiNotificationType
)
{
    FLCN_STATUS status      = FLCN_OK;
    LwU32       msEiSfmMask = RM_PMU_LPWR_CTRL_SFM_VAL_DEFAULT;
    LwU8        lpwrCtrlId;

    if (eiNotificationType == LPWR_EI_NOTIFICATION_EXIT_DONE)
    {
        // Disable RPG
        msEiSfmMask = LPWR_SF_MASK_CLEAR(MS, RPG, msEiSfmMask);

        if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CLK_SLOWDOWN))
        {
            // Disable the SYS and LWD Clock Slowdown
            msEiSfmMask = LPWR_SF_MASK_CLEAR(MS, CLK_SLOWDOWN_SYS, msEiSfmMask);
            msEiSfmMask = LPWR_SF_MASK_CLEAR(MS, CLK_SLOWDOWN_LWD, msEiSfmMask);
        }
    }

    // If MS-RPG Pre Entry steps are not exelwted, keep MS-RPG disabled
    if (PMUCFG_FEATURE_ENABLED(PMU_MS_RPG) &&
        (!Ms.pMsRpg->bRpgPreEntryStepDone))
    {
        // Disable RPG
        msEiSfmMask = LPWR_SF_MASK_CLEAR(MS, RPG, msEiSfmMask);
    }

    // Loop over all the MS group lpwr ctrlIds
    FOR_EACH_INDEX_IN_MASK(32, lpwrCtrlId, lpwrGrpCtrlMaskGet(RM_PMU_LPWR_GRP_CTRL_ID_MS))
    {
        // Send the request to update the Sub feature mask
        pgCtrlSubfeatureMaskRequest(lpwrCtrlId,
                                    LPWR_CTRL_SFM_CLIENT_ID_EI_NOTIFICATION, msEiSfmMask);
    }
    FOR_EACH_INDEX_IN_MASK_END;

    return status;
}

/*!
 * @brief Handles EI notification for Sleep Aware Callback
 *
 * @param[in]  notificationType  EI Framework Entry/Exit Notification
 */
void
s_lpwrEiNotificationSacHandler
(
    LwU8 eiNotificationType
)
{
    if (eiNotificationType == LPWR_EI_NOTIFICATION_ENTRY_DONE)
    {
        lpwrCallbackSacArbiter(LPWR_SAC_CLIENT_ID_EI, LW_TRUE);
    }
    else
    {
        lpwrCallbackSacArbiter(LPWR_SAC_CLIENT_ID_EI, LW_FALSE);
    }
}

/*!
 * @brief API to handle EI Notification for LPWR based Clients
 *
 * @param[in]   eiNotificationData    Info recieved from EI Framework
 */
static FLCN_STATUS
s_lpwrEiNotificationHandler
(
    RM_PMU_LPWR_EI_NOTIFICATION_DATA eiNotificationData
)
{
    FLCN_STATUS status = FLCN_OK;

    switch (eiNotificationData.clientId)
    {
        case RM_PMU_LPWR_EI_CLIENT_ID_PMU_LPWR:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MUTUAL_EXCLUSION) &&
                PMUCFG_FEATURE_ENABLED(PMU_PG_GR_RG)                  &&
                pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_GR_RG))
            {
                s_lpwrEiNotificationGrRgHandler(eiNotificationData.notificationType);
            }

            if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MS) &&
                lpwrGrpIsCtrlSupported(RM_PMU_LPWR_GRP_CTRL_ID_MS))
            {
                status = s_lpwrEiNotificationMsHandler(eiNotificationData.notificationType);
            }

            if (PMUCFG_FEATURE_ENABLED(PMU_PG_TASK_MGMT))
            {
                s_lpwrEiNotificationSacHandler(eiNotificationData.notificationType);
            }

            break;
        }

        default:
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @brief Function to send request to RM
 *
 * @param[in]   clientId       EI Client ID based on RM_PMU_LPWR_EI_CLIENT_xvy
 * @param[in]   notification   Entry/Exit Notificatio
 *
 * @return      FLCN_OK        On success
 * @return      FLCN_ERROR     On failure to send notification
 */
FLCN_STATUS
s_lpwrLpRmPrefetchRequestSend(void)
{
    FLCN_STATUS status = FLCN_OK;

    PMU_RM_RPC_STRUCT_LPWR_DIFR_PREFETCH_REQUEST rpc = { { 0 } };

    PMU_RM_RPC_EXELWTE_BLOCKING(status, LPWR, DIFR_PREFETCH_REQUEST, &rpc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }
    return status;
}

/*!
 * @brief Function to send notification to RM for Idle Snap
 *
 * @param[in]   ctrlId      LPWR_CTRL ID for which notification is sent
 *
 * @return      FLCN_OK        On success
 * @return      FLCN_ERROR     On failure to send notification
 */
FLCN_STATUS
s_lpwrLpRmIdleSnapNotificationSend
(
    LwU8 lpwrCtrlId
)
{
    OBJPGSTATE *pPgState = NULL;
    FLCN_STATUS status   = FLCN_OK;

    PMU_RM_RPC_STRUCT_LPWR_PG_IDLE_SNAP idleSnapRpc;

    memset(&idleSnapRpc, 0x00, sizeof(idleSnapRpc));

    if (pgIsCtrlSupported(lpwrCtrlId))
    {
        pPgState = GET_OBJPGSTATE(lpwrCtrlId);

        idleSnapRpc.ctrlId      = pPgState->id;
        idleSnapRpc.reason      = pPgState->idleSnapReasonMask;
        idleSnapRpc.idleStatus  = pPgState->idleStatusCache[0];
        idleSnapRpc.idleStatus1 = pPgState->idleStatusCache[1];
        idleSnapRpc.idleStatus2 = pPgState->idleStatusCache[2];

        PMU_RM_RPC_EXELWTE_BLOCKING(status, LPWR, PG_IDLE_SNAP, &idleSnapRpc);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
        }
    }
    return status;
}
