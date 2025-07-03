/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file pglogic.c
 * Handle PgCtrl AKA LpwrCtrl related operations
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "lwostimer.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "pmu_objlpwr.h"
#include "pmu_objgcx.h"
#include "pmu_objgr.h"
#include "pmu_objms.h"
#include "pmu_objdi.h"
#include "pmu_objap.h"
#include "pmu_objei.h"
#include "pmu_objdfpr.h"
#include "pmu_objfifo.h"
#include "pmu_objpmgr.h"
#include "main.h"
#include "dbgprintf.h"
#include "cmdmgmt/cmdmgmt.h"
#include "g_pmurmifrpc.h"

/* ------------------------- Prototypes ------------------------------------- */
// PG Logics
static FLCN_STATUS s_pgLogicLogging        (PG_LOGIC_STATE *);
static void        s_pgLogicAdaptive       (PG_LOGIC_STATE *);
static FLCN_STATUS s_pgLogicTaskMgmt       (PG_LOGIC_STATE *);
static void        s_pgLogicEiHandler      (PG_LOGIC_STATE *);
static void        s_pgLogicGrGrpHandler   (PG_LOGIC_STATE *);
static void        s_pgLogicMsGrpHandler   (PG_LOGIC_STATE *);
static FLCN_STATUS s_pgLogicDisallowAck    (PG_LOGIC_STATE *);
static void        s_pgLogicDfprMscgHandler(PG_LOGIC_STATE *);
static void        s_pgLogicDfprHandler    (PG_LOGIC_STATE *);
static void        s_lpwrIdleSnapRmNotify  (OBJPGSTATE *pPgState);

// Helper functions for PG logics
static void        s_pgGrPsiEngage(LwBool bEngage)
                    GCC_ATTRIB_SECTION("imem_libGr", "s_pgGrPsiEngage");
static LwU8        s_pgGetApCtrlId(LwU8 pgCtrlId);
static LwBool      s_pgFsmDependencyHandler(PG_LOGIC_STATE *, LwU32 childFsmCtrlId);
static void        s_pgGrPgHandler(LwU32 eventId);
static void        s_pgMsMscgHandler(LwU32 eventId);
static void        s_pgMsDifrSwAsrHandler(PG_LOGIC_STATE *);

/* ------------------------- Macros and Defines ----------------------------- */
// Event filter mask for PG Logic Logging
#define PG_LOGIC_LOGGING_EVENT_MASK                                           \
    (BIT(PMU_PG_EVENT_PG_ON)                                                 |\
     BIT(PMU_PG_EVENT_PG_ON_DONE)                                            |\
     BIT(PMU_PG_EVENT_ENG_RST)                                               |\
     BIT(PMU_PG_EVENT_CTX_RESTORE)                                           |\
     BIT(PMU_PG_EVENT_POWERED_UP)                                            |\
     BIT(PMU_PG_EVENT_POWERED_DOWN)                                          |\
     BIT(PMU_PG_EVENT_POWERING_UP)                                           |\
     BIT(PMU_PG_EVENT_DENY_PG_ON))

// Event filter mask for PG Logic Adaptive
#define PG_LOGIC_AP_EVENT_MASK                                                \
    (BIT(PMU_PG_EVENT_DENY_PG_ON)                                            |\
     BIT(PMU_PG_EVENT_POWERED_UP))

// Event filter mask for PG Logic Task Management
#define PG_LOGIC_TASK_MGMT_EVENT_MASK                                         \
    (BIT(PMU_PG_EVENT_PG_ON)                                                 |\
     BIT(PMU_PG_EVENT_DENY_PG_ON)                                            |\
     BIT(PMU_PG_EVENT_POWERED_DOWN)                                          |\
     BIT(PMU_PG_EVENT_POWERED_UP))

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Handle all PG logics.
 *
 * Broadcasts event to various PG logics.
 *
 * @param[in]   pPgLogicState   Pointer to PG Logic state
 *
 * @returns FLCN_OK
 */
FLCN_STATUS
pgLogicAll
(
    PG_LOGIC_STATE *pPgLogicState
)
{
    s_pgLogicDfprMscgHandler(pPgLogicState);
    s_pgLogicLogging        (pPgLogicState);
      pgLogicOvlPre         (pPgLogicState);
    s_pgLogicAdaptive       (pPgLogicState);
    s_pgLogicTaskMgmt       (pPgLogicState);
    s_pgLogicEiHandler      (pPgLogicState);
    s_pgLogicGrGrpHandler   (pPgLogicState);
    s_pgLogicDfprHandler    (pPgLogicState);
    s_pgLogicMsGrpHandler   (pPgLogicState);
      pgLogicStateMachine   (pPgLogicState);
      pgLogicWar            (pPgLogicState);
    s_pgLogicDisallowAck    (pPgLogicState);
      pgLogicOvlPost        (pPgLogicState);

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * PG Logic - Logger
 *
 * This is a sample log handler. Use this as an example of how events can be
 * captured.
 *
 * @param[in]   pPgLogicState   Pointer to PG Logic state
 *
 * @returns FLCN_OK
 */
static FLCN_STATUS
s_pgLogicLogging
(
    PG_LOGIC_STATE *pPgLogicState
)
{
    OBJPGSTATE *pPgState;

    if (!(BIT(pPgLogicState->eventId) & PG_LOGIC_LOGGING_EVENT_MASK))
    {
        return FLCN_OK;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PG_LOG))
    {
        pPgState = GET_OBJPGSTATE(pPgLogicState->ctrlId);

        switch (pPgLogicState->eventId)
        {
            case PMU_PG_EVENT_POWERING_UP:
            case PMU_PG_EVENT_ENG_RST:
            {
                pgLogEvent(pPgLogicState->ctrlId, pPgLogicState->eventId,
                           pPgState->stats.wakeUpEvents);
                break;
            }
            case PMU_PG_EVENT_DENY_PG_ON:
            {
                pgLogEvent(pPgLogicState->ctrlId, pPgLogicState->eventId,
                           pPgState->stats.abortReason);
                break;
            }
            default:
            {
                pgLogEvent(pPgLogicState->ctrlId, pPgLogicState->eventId, 0);
                break;
            }
        }
    }

    pgStatRecordEvent(pPgLogicState->ctrlId, pPgLogicState->eventId);

    return FLCN_OK;
}

/*!
 * @brief PG Logic to handle Adaptive Power Feature
 *
 * It runs AP State machine.
 *
 * @param[in]   pPgLogicState   Pointer to PG Logic state
 *
 * @return      FLCN_STATUS FLCN_OK if success.
 */
static void
s_pgLogicAdaptive
(
    PG_LOGIC_STATE *pPgLogicState
)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_GR))
    {
        LwU32       apCtrlId;
        OBJPGSTATE *pPgState;

        pPgState = GET_OBJPGSTATE(pPgLogicState->ctrlId);
        apCtrlId = s_pgGetApCtrlId(pPgLogicState->ctrlId);

        if ((AP_IS_CTRL_SUPPORTED(apCtrlId))                              &&
            ((BIT(pPgLogicState->eventId) & PG_LOGIC_AP_EVENT_MASK) != 0) &&
            (PG_IS_STATE_PENDING(pPgState, AP_THRESHOLD_UPDATE)))
        {
            // Issue lazy updates for idle threshold.
            apCtrlIdleThresholdSet(apCtrlId, LW_FALSE);
        }
    }
}

/*!
 * @brief PG Logic to handle task thrashing and starvation issues.
 *
 * param[in]   pPgLogicState   Pointer to PG Logic state
 *
 * @returns FLCN_OK
 */
static FLCN_STATUS
s_pgLogicTaskMgmt
(
    PG_LOGIC_STATE *pPgLogicState
)
{
    OBJPGSTATE *pPgState;

    if (!PMUCFG_FEATURE_ENABLED(PMU_PG_TASK_MGMT))
    {
        return FLCN_OK;
    }

    if ((BIT(pPgLogicState->eventId) & PG_LOGIC_TASK_MGMT_EVENT_MASK) != 0)
    {
        pPgState = GET_OBJPGSTATE(pPgLogicState->ctrlId);

        switch (pPgLogicState->eventId)
        {
            case PMU_PG_EVENT_PG_ON:
            {
                // Get the powered up time from last sample
                pgStatDeltaPoweredUpTimeGetUs(pPgLogicState->ctrlId, &pPgState->taskMgmt.poweredUpInfo);

                //
                // Reset Back-to-back abort and wakeup policy counters if
                // PgCtrl stayed in power off state for sufficient time
                //
                if (pPgState->taskMgmt.poweredUpInfo.deltaTimeUs >
                    PG_CTRL_TASK_MGMT_POWERED_UP_TIME_DEFAULT_US)
                {
                    pPgState->taskMgmt.b2bAbortCount  = 0;
                    pPgState->taskMgmt.b2bWakeupCount = 0;
                }
                break;
            }
            case PMU_PG_EVENT_DENY_PG_ON:
            {
                //
                // Back-to-back abort policy: Increase back-to-back abort
                // count on every abort cycle
                //
                pPgState->taskMgmt.b2bAbortCount++;

                //
                // Disallow PgCtrl if counter is reaching to default threshold
                // for back-to-back abort policy. Reset the threshold, that will
                // reset the policy.
                //
                if (pPgState->taskMgmt.b2bAbortCount >=
                    PG_CTRL_TASK_MGMT_B2B_ABORT_THRESHOLD_DEFAULT)
                {
                    pgCtrlThrashingDisallow(pPgLogicState->ctrlId);
                    pPgState->taskMgmt.b2bAbortCount = 0;
                }
                break;
            }
            case PMU_PG_EVENT_POWERED_DOWN:
            {
                //
                // Back-to-back abort policy:
                // Reset back-to-back abort counter on successful entry
                //
                pPgState->taskMgmt.b2bAbortCount = 0;
                break;
            }
            case PMU_PG_EVENT_POWERED_UP:
            {
                // Get the delta sleep time from last sample
                pgStatDeltaSleepTimeGetUs(pPgLogicState->ctrlId, &pPgState->taskMgmt.sleepInfo);

                //
                // Back-to-back wakeup policy:
                // Increase the back-to-back wakeup counter if we are not
                // meeting residency criteria otherwise reset the counter.
                //
                if (pPgState->taskMgmt.sleepInfo.deltaTimeUs <
                    PG_CTRL_TASK_MGMT_MIN_RESIDENT_TIME_DEFAULT_US)
                {
                    pPgState->taskMgmt.b2bWakeupCount++;
                }
                else
                {
                    pPgState->taskMgmt.b2bWakeupCount = 0;
                }

                //
                // Disallow PgCtrl if counter is reaching to default threshold
                // for back-to-back wakeup policy. Reset the threshold, that
                // will reset the policy.
                //
                if (pPgState->taskMgmt.b2bWakeupCount >=
                    PG_CTRL_TASK_MGMT_B2B_WAKEUP_THRESHOLD_DEFAULT)
                {
                    pgCtrlThrashingDisallow(pPgLogicState->ctrlId);
                    pPgState->taskMgmt.b2bWakeupCount = 0;
                }

                break;
            }
        }
    }

    //
    // Sleep Aware Callbacks:
    //
    // 1) Notify OS to activate sleep callbacks on GR entry.
    // 2) Notify OS to deactivate sleep callbacks on GR abort or exit.
    //
    if (lpwrGrpIsLpwrCtrlMember(RM_PMU_LPWR_GRP_CTRL_ID_GR, pPgLogicState->ctrlId))
    {
        switch (pPgLogicState->eventId)
        {
            case PMU_PG_EVENT_PG_ON:
            {
                lpwrCallbackSacArbiter(LPWR_SAC_CLIENT_ID_GR, LW_TRUE);
                break;
            }

            case PMU_PG_EVENT_DENY_PG_ON:
            case PMU_PG_EVENT_POWERED_UP:
            {
                lpwrCallbackSacArbiter(LPWR_SAC_CLIENT_ID_GR, LW_FALSE);
                break;
            }
        }
    }

    return FLCN_OK;
}

/*!
 * @brief PG logic handler for GR group
 *
 * @param[in]   pPgLogicState   Pointer to PG Logic state
 */
static void
s_pgLogicGrGrpHandler
(
    PG_LOGIC_STATE *pPgLogicState
)
{
    LwU32 lpwrCtrlId = 0;

    if (lpwrGrpIsLpwrCtrlMember(RM_PMU_LPWR_GRP_CTRL_ID_GR, pPgLogicState->ctrlId))
    {
        //
        // Handle dependency between GR<->MS groups. MS features should wake before GR.
        // Return early if Dependency handler changed the PG logic state.
        //
        FOR_EACH_INDEX_IN_MASK(32, lpwrCtrlId, lpwrGrpCtrlMaskGet(RM_PMU_LPWR_GRP_CTRL_ID_MS))
        {
            if (s_pgFsmDependencyHandler(pPgLogicState, lpwrCtrlId))
            {
                return;
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;

        //
        // Handle dependency between DIFR Layer 1 (DFPR) and GR Group.
        // GR Group must wake the Layer 1 first and then only GR exit sequence
        // can be exelwted.
        //
        // GR Feature entry triggers the Preemption and as a part of preemption, FECS
        // triggers the L2 Cache Demote operation and same thing happens on GR context
        // save process as well. Hence, we need to make the GR as precondition for
        // DFPR so that, L2 Cache contents are not demoted after DFPR has done the
        // prefetch of display data.
        //
        // Return early if Dependency handler changed the PG logic state.
        //
        if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_DFPR) &&
            pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_DFPR))
        {
            if (s_pgFsmDependencyHandler(pPgLogicState, RM_PMU_LPWR_CTRL_ID_DFPR))
            {
                return;
            }
        }

        // GR-PG specific handling of event
        if ((PMUCFG_FEATURE_ENABLED(PMU_PG_GR)) &&
            (pPgLogicState->ctrlId == RM_PMU_LPWR_CTRL_ID_GR_PG))
        {
            s_pgGrPgHandler(pPgLogicState->eventId);
        }
    }
}

/*!
 * PG logic handler for MS group
 *
 * The wrapper routine is called before pgLogicRunStateMachine. It intercepts
 * a GR wakeup events, GR disallow/allow command from RM and run the MS SM
 *
 * @param[in]   pPgLogicState   Pointer to PG Logic state
 */
static void
s_pgLogicMsGrpHandler
(
    PG_LOGIC_STATE *pPgLogicState
)
{
    if (lpwrGrpIsLpwrCtrlMember(RM_PMU_LPWR_GRP_CTRL_ID_MS, pPgLogicState->ctrlId))
    {
        // MSCG specific handling of event
        if ((PMUCFG_FEATURE_ENABLED(PMU_PG_MS)) &&
            (pPgLogicState->ctrlId == RM_PMU_LPWR_CTRL_ID_MS_MSCG))
        {
            s_pgMsMscgHandler(pPgLogicState->eventId);
        }

        // MS_DIFR_SWASR specific handling of event
        if ((PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_MS_DIFR_SW_ASR)) &&
            (pPgLogicState->ctrlId == RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR))
        {
            s_pgMsDifrSwAsrHandler(pPgLogicState);
        }

        switch (pPgLogicState->eventId)
        {
            // Case: After abort or exit or Entry
            case PMU_PG_EVENT_DENY_PG_ON:
            case PMU_PG_EVENT_POWERED_DOWN:
            case PMU_PG_EVENT_POWERED_UP:
            {
                // Re-enable MS interrupts
                if (Ms.bWakeEnablePending)
                {
                    msEnableIntr_HAL(&Ms);
                    Ms.bWakeEnablePending = LW_FALSE;
                }

                break;
            }
        }
    }
}

/*!
 * DISALLOW_ACK logic event handler
 *
 * @param[in]   pPgLogicState   Pointer to PG Logic state
 *
 * @returns FLCN_OK
 */
static FLCN_STATUS
s_pgLogicDisallowAck
(
    PG_LOGIC_STATE *pPgLogicState
)
{
    OBJPGSTATE *pPgState;
    FLCN_STATUS status;

    if (pPgLogicState->eventId != PMU_PG_EVENT_DISALLOW_ACK)
    {
        return FLCN_OK;
    }

    pPgState = GET_OBJPGSTATE(pPgLogicState->ctrlId);

    PMU_HALT_COND(pPgState->state == PMU_PG_STATE_DISALLOW);

    // Handle RM DISALLOW_ACK
    if (PG_LOGIC_IS_DISALLOW_ACK(pPgLogicState, RM))
    {
        PMU_RM_RPC_STRUCT_LPWR_PG_ASYNC_CMD_RESP disallowRespRpc;

        disallowRespRpc.ctrlId = pPgLogicState->ctrlId;
        disallowRespRpc.msgId  = RM_PMU_PG_MSG_ASYNC_CMD_DISALLOW;

        // RC-TODO, properly handle return code.
        PMU_RM_RPC_EXELWTE_BLOCKING(status, LPWR, PG_ASYNC_CMD_RESP, &disallowRespRpc);
    }

    // Send idle-snap error
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_IDLE_SNAP) &&
        LPWR_ARCH_IS_SF_SUPPORTED(IDLE_SNAP_DBG)   &&
        PG_LOGIC_IS_DISALLOW_ACK(pPgLogicState, IDLE_SNAP))
    {
        s_lpwrIdleSnapRmNotify(pPgState);
    }

    // SFM_UPDATE: Update sub-feature mask and enable PgCtrl
    if (PMUCFG_FEATURE_ENABLED(PMU_PG_SFM_2) &&
        PG_LOGIC_IS_DISALLOW_ACK(pPgLogicState, SFM))
    {
        pgCtrlSubfeatureMaskUpdate(pPgLogicState->ctrlId);

        pgCtrlAllow(pPgLogicState->ctrlId, PG_CTRL_ALLOW_REASON_MASK(SFM));
    }

    //
    // PERF_CHANGE: Continue Perf change handling if
    //              PERF_CHANGE_DISALLOW is pending
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_WITH_PS35) &&
        PG_LOGIC_IS_DISALLOW_ACK(pPgLogicState, PERF))
    {
        lpwrPerfChangePreSeq();
    }

    // THRESHOLD_UPDATE: Set thresholds and enable PgCtrl
    if (PMUCFG_FEATURE_ENABLED(PMU_PG_THRESHOLD_UPDATE) &&
        PG_LOGIC_IS_DISALLOW_ACK(pPgLogicState, THRESHOLD))
    {
        pgCtrlIdleThresholdsSet_HAL(&Pg, pPgLogicState->ctrlId);
        pgCtrlAllow(pPgLogicState->ctrlId, PG_CTRL_ALLOW_REASON_MASK(THRESHOLD));
    }

    // LPWR group: Re-trigger the owner change sequencer
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MUTUAL_EXCLUSION) &&
        PG_LOGIC_IS_DISALLOW_ACK(pPgLogicState, LPWR_GRP))
    {
        // Ensure that current ctrlId belong to GR group
        if (lpwrGrpIsLpwrCtrlMember(RM_PMU_LPWR_GRP_CTRL_ID_GR,
                                    pPgLogicState->ctrlId))
        {
            lpwrGrpCtrlSeqOwnerChange(RM_PMU_LPWR_GRP_CTRL_ID_GR);
        }
        // Ensure that current ctrlId belong to MS group
        else if (lpwrGrpIsLpwrCtrlMember(RM_PMU_LPWR_GRP_CTRL_ID_MS,
                                         pPgLogicState->ctrlId))
        {
            lpwrGrpCtrlSeqOwnerChange(RM_PMU_LPWR_GRP_CTRL_ID_MS);
        }
        else
        {
            PMU_BREAKPOINT();
        }
    }

    //
    // Reenable the DFPR FSM only if
    // 1. Prfetch response is not pending &&
    // 2. Recieved the SELF Disallow ACK
    //
    if ((PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_DFPR))        &&
        (pPgLogicState->ctrlId == RM_PMU_LPWR_CTRL_ID_DFPR) &&
        (PG_LOGIC_IS_DISALLOW_ACK(pPgLogicState, SELF))     &&
        (!Dfpr.bIsPrefetchResponsePending))
    {
        pgCtrlAllow(pPgLogicState->ctrlId, PG_CTRL_ALLOW_REASON_MASK(SELF));
    }

    return FLCN_OK;
}

/*!
 * @brief Helper to send Idle snap Notification to RM
 *
 * @param[in]   pPgState - OBJPGSTATE pointer
 */
void s_lpwrIdleSnapRmNotify
(
     OBJPGSTATE *pPgState
)
{
    FLCN_STATUS status;

    // If LPWR_LP supports to send the Idle snap nofication, use it.
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_LP_IDLE_SNAP_NOTIFICATION) &&
        LPWR_ARCH_IS_SF_SUPPORTED(LPWR_LP_IDLE_SNAP_NOTIFY))
    {
        DISPATCH_LPWR_LP disp2LpwrLp = {{ 0 }};

        // Fill up the structure to send event to LPWR_LP Task
        disp2LpwrLp.hdr.eventType        = LPWR_LP_LPWR_FSM_IDLE_SNAP;
        disp2LpwrLp.idleSnapEvent.ctrlId = pPgState->id;

        //
        // Send the event to LPWR_LP task LPWR Task will not wait
        // for LPWR_LP Task queue to become available.
        // If queue is not empty, we will simply drop the idle snap
        // notification in such case.
        //
        status = lwrtosQueueIdSend(LWOS_QUEUE_ID(PMU, LPWR_LP),
                                   &disp2LpwrLp, sizeof(disp2LpwrLp), 0);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
        }
    }
    else
    {
        PMU_RM_RPC_STRUCT_LPWR_PG_IDLE_SNAP idleSnapRpc;

        memset(&idleSnapRpc, 0x00, sizeof(idleSnapRpc));
        idleSnapRpc.ctrlId      = pPgState->id;
        idleSnapRpc.reason      = pPgState->idleSnapReasonMask;
        idleSnapRpc.idleStatus  = pPgState->idleStatusCache[0];
        idleSnapRpc.idleStatus1 = pPgState->idleStatusCache[1];
        idleSnapRpc.idleStatus2 = pPgState->idleStatusCache[2];

        // RC-TODO, properly handle return code.
        PMU_RM_RPC_EXELWTE_BLOCKING(status, LPWR, PG_IDLE_SNAP, &idleSnapRpc);
    }
}

/*!
 * @brief Helper to handle GR-PG events
 *
 * @param[in]   eventId     FSM event ID for GR-PG
 */
static void
s_pgGrPgHandler
(
    LwU32 eventId
)
{
    switch (eventId)
    {
        case PMU_PG_EVENT_POWERED_DOWN:
        {
            // WAR 1696192: Re-enable GR-ELCG.
            if (PMUCFG_FEATURE_ENABLED(PMU_PG_GR_WAR1696192_ELCG_FORCE_OFF))
            {
                lpwrCgElcgEnable(PMU_ENGINE_GR, LPWR_CG_ELCG_CTRL_REASON_GR_PG);
            }

            // Engage GR coupled PSI
            s_pgGrPsiEngage(LW_TRUE);

            break;
        }
        case PMU_PG_EVENT_POWERING_UP:
        {
            // Dis-engage GR coupled PSI
            s_pgGrPsiEngage(LW_FALSE);

            // WAR 1696192: Disable GR-ELCG.
            if (PMUCFG_FEATURE_ENABLED(PMU_PG_GR_WAR1696192_ELCG_FORCE_OFF))
            {
                lpwrCgElcgDisable(PMU_ENGINE_GR, LPWR_CG_ELCG_CTRL_REASON_GR_PG);
            }

            break;
        }
        case PMU_PG_EVENT_POWERED_UP:
        {
            // WAR 1696192: Re-enable GR-ELCG.
            if (PMUCFG_FEATURE_ENABLED(PMU_PG_GR_WAR1696192_ELCG_FORCE_OFF))
            {
                lpwrCgElcgEnable(PMU_ENGINE_GR, LPWR_CG_ELCG_CTRL_REASON_GR_PG);
            }
            break;
        }
    }

    //
    // Handle Adaptive GR-ELPG logic.
    //
    // 1) Initialize histogram idle counter mode to AUTO_IDLE.
    // 2) Set histogram idle counter mode to FORCE_IDLE before starting ELPG
    //    entry sequence. Entry sequence disturbs idleness of graphics engine
    //    (i.e. sometime entry sequence makes Graphics engine busy). We would
    //    like to ignore such disturbance in histogram callwlations.
    // 3) Reset histogram idle counter mode to AUTO_IDLE after completion of
    //    entry.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_GR_POWER_GATING))
    {
        switch (eventId)
        {
            case PMU_PG_EVENT_PG_ON:
            {
                //
                // Set supplemental idle counters mode of GR Histogram to
                // FORCE_IDLE.
                //
                pgApConfigCntrMode_HAL(&Pg, RM_PMU_PGCTRL_IDLE_MASK_HIST_IDX_GR,
                                            PG_SUPP_IDLE_COUNTER_MODE_FORCE_IDLE);
                break;
            }

            case PMU_PG_EVENT_POWERED_DOWN:
            case PMU_PG_EVENT_DENY_PG_ON:
            {
                //
                // Set supplemental idle counters mode of GR Histogram to
                // AUTO_IDLE.
                //
                pgApConfigCntrMode_HAL(&Pg, RM_PMU_PGCTRL_IDLE_MASK_HIST_IDX_GR,
                                            PG_SUPP_IDLE_COUNTER_MODE_AUTO_IDLE);
                break;
            }
        }
    }
}

/*!
 * @brief Helper to handle MSCG events
 *
 * @param[in]   eventId     FSM event ID for MSCG
 */
static void
s_pgMsMscgHandler
(
    LwU32 eventId
)
{
    //
    // Logic to handle actions -
    // 1) Before entry              AND
    // 2) After abort or exit
    //
    switch (eventId)
    {
        // Case: Before entry
        case PMU_PG_EVENT_PG_ON:
        {
            //
            // SW WAR 1168927:
            // Notify PMGR to before starting MS entry. PMGR will disable
            // BA sensor.
            //
            pmgrSwAsrNotify(LW_TRUE);
            break;
        }

        // Case: After abort or exit
        case PMU_PG_EVENT_DENY_PG_ON:
        case PMU_PG_EVENT_POWERED_UP:
        {
            //
            // SW WAR 1168927:
            // Notify PMGR to after MS exit or abort. PMGR will re-enable
            // BA sensor.
            //
            pmgrSwAsrNotify(LW_FALSE);

            // Notify Centralised LPWR callback about MS exit and abort
            if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CENTRALISED_CALLBACK))
            {
                lpwrCallbackNotifyMsFullPower();
            }

            break;
        }
    }
}

/*!
 * @brief Helper to handle MS_DIFR_SW_ASR events
 *
 * @param[in]   eventId     FSM event ID for MSCG
 */
static void
s_pgMsDifrSwAsrHandler
(
    PG_LOGIC_STATE *pPgLogicState
)
{
    //
    // Handle dependency between DIFR Layer 2 (SW_ASR) and Layer 3 (CG).
    // Layer Must wake the Layer 3 first and then only Layer 2 exit sequence
    // can be exelwted.
    //
    // Return early if Dependency handler changed the PG logic state.
    //
    if (s_pgFsmDependencyHandler(pPgLogicState, RM_PMU_LPWR_CTRL_ID_MS_DIFR_CG))
    {
        return;
    }
}

/*!
 * @brief   Helper function to engage/dis-engage GR coupled PSI
 */
static void
s_pgGrPsiEngage
(
    LwBool bEngage
)
{
    OBJPGSTATE *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);

    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_GR)      &&
        PSI_IS_CTRL_SUPPORTED(RM_PMU_PSI_CTRL_ID_GR) &&
        LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, PSI))
    {
        if (bEngage)
        {
            // Engage GR coupled PSI
            psiEngage(RM_PMU_PSI_CTRL_ID_GR, BIT(RM_PMU_PSI_RAIL_ID_LWVDD),
                      GR_PSI_ENTRY_EXIT_STEPS_MASK);
        }
        else
        {
            // Dis-engage GR coupled PSI
            psiDisengage(RM_PMU_PSI_CTRL_ID_GR, BIT(RM_PMU_PSI_RAIL_ID_LWVDD),
                         GR_PSI_ENTRY_EXIT_STEPS_MASK);
        }
    }
}

/*!
 * @brief Get apCtrlId from pgCtrlId
 *
 * @param[in]   pgCtrlId    PG Ctrl Id
 *
 * @return      LwU8        Ap Ctrl Id
 */
static LwU8
s_pgGetApCtrlId
(
    LwU8 pgCtrlId
)
{
    LwU8 apCtrlId = RM_PMU_AP_CTRL_ID__COUNT;

    if (PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_GR) &&
        pgCtrlId == RM_PMU_LPWR_CTRL_ID_GR_PG)
    {
        apCtrlId = RM_PMU_AP_CTRL_ID_GR;
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_MSCG) &&
        pgCtrlId == RM_PMU_LPWR_CTRL_ID_MS_MSCG)
    {
        apCtrlId = RM_PMU_AP_CTRL_ID_MSCG;
    }

    return apCtrlId;
}

/*!
 * @brief Manage the dependency between two FSMs
 *
 * Child FSM should complete the exit before starting the exit of parent FSM.
 * This helper ensures this
 *
 * @param[in/out]   pPgLogicState   Pointer to PG Logic state of parent FSM
 * @param[in]       childFsmCtrlId  SW CtrlId for child FSM
 *
 * @return      LW_TRUE     PG Logic State changed
 *              LW_FALSE    PG Logic State did not changed
 */
static LwBool
s_pgFsmDependencyHandler
(
    PG_LOGIC_STATE *pPgLogicState,
    LwU32           childFsmCtrlId
)
{
    OBJPGSTATE *pChildState;
    LwU32       parentFsmCtrlId;
    LwBool      bEventColwerted = LW_FALSE;

    //
    // This helper disables child FSM whenever it sees the WAKE or DISALLOW
    // event. No action required if child controller is not supported.
    //
    if (!pgIsCtrlSupported(childFsmCtrlId))
    {
        return bEventColwerted;
    }

    pChildState     = GET_OBJPGSTATE(childFsmCtrlId);
    parentFsmCtrlId = pPgLogicState->ctrlId;

    switch (pPgLogicState->eventId)
    {
        //
        // Disable child FSM of current PgCtrl based on any wake-up/disallow
        // event provided Parent is not in FULL POWER mode. No need to
        // explicitly disable the child FSM if its already in disallow state.
        //
        // Parent FSM is in full-power mode that means -
        // 1) Zero cycles of Parent FSM:
        //    Parent FSM is just enabled but haven't started its first cycle. Child
        //    FSM can engage only when Parent FSM is in PWR_OFF state. As parent
        //    is in PWR_ON state child FSM can not engage because of this parent
        //    FSM. Thus, no need to disallow child FSM.
        //                              OR
        // 2) Non-zero cycles of Parent FSM:
        //    Parent FSM has been engaged couple of times. But lwrrently parent
        //    FSM is in full-power mode, that means parent FSM has processed the
        //    last wakeup and completed the exit sequence. The last wake-up will
        //    first disallow the child FSM and then process the parent FSM exit.
        //    Child FSM will be not be engage once parent FSM moves out of
        //    PWR_OFF state.
        //
        // Based on above, we have to disallow the child FSM only when parent
        // FSM is not in full-power mode. WAKE/DISALLOW when parent FSM is not
        // in full-power mode, FSM will always generate POWERED_UP event. Its
        // safe to re-enable child FSM on POWERED_UP event.
        //
        case PMU_PG_EVENT_DISALLOW:
        case PMU_PG_EVENT_WAKEUP:
        {
            //
            // Return early if parent is in full power mode.
            // We should be good with processing parent wake/disallow
            //
            if (PG_IS_FULL_POWER(parentFsmCtrlId))
            {
                return bEventColwerted;
            }

            //
            // Return early if child is in disallowed
            // We should be good with processing parent wake/disallow
            //
            if ((pChildState->state == PMU_PG_STATE_DISALLOW) &&
                ((pChildState->disallowReasonMask & PG_CTRL_DISALLOW_REASON_MASK(PARENT)) != 0))
            {
                return bEventColwerted;
            }

            //
            // Child FSM is not disabled. We are not yet ready to process this
            // event. Push back the current WAKE/DISALLOW event to LPWR queue.
            //
            pgCtrlEventSend(pPgLogicState->ctrlId, pPgLogicState->eventId,
                            pPgLogicState->data);

            //
            // Initiate the child FSM disallow process.
            //
            // DisallowMask is zero, means its a first parent FSM that has
            // requested the disablement. Colwert current event to child FSM
            // DISALLOW event.
            //
            if (pChildState->parentDisallowMask == 0)
            {
                // Colwert current event to child DISALLOW
                pPgLogicState->ctrlId  = childFsmCtrlId;
                pPgLogicState->eventId = PMU_PG_EVENT_DISALLOW;
                pPgLogicState->data    = PG_CTRL_DISALLOW_REASON_MASK(PARENT);
            }
            else
            {
                //
                // DisallowMask is non-zero means Disallow for child FSM is in
                // progress. No further processing is required.
                //
                pPgLogicState->eventId = PMU_PG_EVENT_NONE;
            }

            // Add parent FSM CtrlId in DisallowMask
            pChildState->parentDisallowMask |= BIT(parentFsmCtrlId);

            // Current event has been colwerted. Update the state.
            bEventColwerted = LW_TRUE;
            break;
        }

        //
        // Re-enable child FSM once parent FSM reaches to PWR_ON state.
        //
        // HW pre-condition logic will not allow child FSM entry once parent
        // FSM moves to PWR_ON state. So its safe to allow the child FSM on
        // POWERED_UP event.
        //
        // Remove parent controller from DisallowMask once it reaches to PWR_ON
        // state. Enable child FSM when there are no pending parents FSMs in
        // DisallowMask.
        //
        case PMU_PG_EVENT_POWERED_UP:
        {
            //
            // Return early DisallowMask is zero. Fsm dependency policy is NOT
            // active.
            //
            if (pChildState->parentDisallowMask == 0)
            {
                return bEventColwerted;
            }

            // Remove the parent FSM from DisallowMask.
            pChildState->parentDisallowMask &= (~BIT(parentFsmCtrlId));

            // Allow child FSM if there are no disallow requests from parent FSM
            if (pChildState->parentDisallowMask == 0)
            {
                pgCtrlEventSend(childFsmCtrlId, PMU_PG_EVENT_ALLOW,
                                                PG_CTRL_ALLOW_REASON_MASK(PARENT));
            }

            break;
        }
    }

    return bEventColwerted;
}

/*!
 * @brief PG logic handler for DFPR Generic Events
 *
 * @param[in]   pPgLogicState   Pointer to PG Logic state
 */
static void
s_pgLogicDfprHandler
(
    PG_LOGIC_STATE *pPgLogicState
)
{
    //
    // Return early if
    // 1. DFPR is not supported OR
    // 2. current event is not for DFPR OR
    //
    if ((!PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_DFPR))  ||
        (!pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_DFPR)) ||
        (pPgLogicState->ctrlId != RM_PMU_LPWR_CTRL_ID_DFPR))
    {
        return;
    }

    //
    // Handle dependency between DIFR Layer 1 (DFPR) and DIFR Layer 2 (SW_ASR).
    // Layer 1 Must wake the Layer 2 first and then only Layer 1 exit sequence
    // can be exelwted.
    //
    // Return early if Dependency handler changed the PG logic state.
    //
    if (s_pgFsmDependencyHandler(pPgLogicState, RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR))
    {
        return;
    }
}

/*!
 * @brief PG logic handler for DFPR and MSCG Interaction
 *
 * @param[in]   pPgLogicState   Pointer to PG Logic state
 */
static void
s_pgLogicDfprMscgHandler
(
    PG_LOGIC_STATE *pPgLogicState
)
{
    //
    // Return early if
    // 1. DFPR is not supported OR
    // 2. current event is not for DFPR OR
    //
    if ((!PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_DFPR))  ||
        (!pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_DFPR)) ||
        (pPgLogicState->ctrlId != RM_PMU_LPWR_CTRL_ID_DFPR))
    {
        return;
    }

    if (pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_MSCG))
    {
        //
        // On PG_ON/POWERING_UP Event: We need to Disable MSCG.
        // On POWERED_UP/DENY_PG_ON/POWERED_DOWN : Enable MSCG
        //
        switch (pPgLogicState->eventId)
        {
            // Entry/Exit Start Events
            case PMU_PG_EVENT_PG_ON:
            case PMU_PG_EVENT_POWERING_UP:
            {
                // If MSCG is already in disallow state, proceed normally
                if (!pgCtrlDisallow(RM_PMU_LPWR_CTRL_ID_MS_MSCG,
                     PG_CTRL_DISALLOW_REASON_MASK(DFPR)))
                {
                    //
                    // MSCG FSM is not disabled. We are not yet ready to process this
                    // event. Push back the current event to LPWR queue. MSCG Disable
                    // request has been queued in the pgCtrlDisallow function.
                    //
                    pgCtrlEventSend(pPgLogicState->ctrlId, pPgLogicState->eventId,
                                    pPgLogicState->data);

                    //
                    // Mark the current event as None as we do not require
                    // more processing.
                    //
                    pPgLogicState->eventId = PMU_PG_EVENT_NONE;
                }

                break;
            }

            // Entry/Abort/Wakeup Completed events
            case PMU_PG_EVENT_POWERED_DOWN:
            case PMU_PG_EVENT_DENY_PG_ON:
            case PMU_PG_EVENT_POWERED_UP:
            {
                // Enable MSCG
                pgCtrlAllow(RM_PMU_LPWR_CTRL_ID_MS_MSCG,
                            PG_CTRL_ALLOW_REASON_MASK(DFPR));

                break;
            }
        }
    }
}

/*!
 * @brief PG logic handler for EI
 *
 * @param[in]   pPgLogicState   Pointer to PG Logic state
 */
static void
s_pgLogicEiHandler
(
    PG_LOGIC_STATE *pPgLogicState
)
{
    OBJPGSTATE *pPgState = NULL;

    // Return early if EI is not supported.
    if ((!PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_EI)) ||
        (!pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_EI)))
    {
        return;
    }

    //
    // EI Passive will not be supported if EI is enabled. We are skipping the below
    // WAR in this scenario as well, as EI-Passive is mutually exclusive with GR-RPG
    // GR-RG graphics features and those are disabled during boot.
    //
    if (pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_EI_PASSIVE))
    {
        PMU_BREAKPOINT();
        return;
    }

    //
    // if the current event is not for GR-PG OR GR-RG, then we do not need
    // to take any further action.
    //
    if ((pPgLogicState->ctrlId != RM_PMU_LPWR_CTRL_ID_GR_PG) &&
        (pPgLogicState->ctrlId != RM_PMU_LPWR_CTRL_ID_GR_RG))
    {
        return;
    }

    //
    // Handle the logic for EI FSM based on GR-PG and GR-RG Events.
    // Details:
    // As part of the GR-PG/GR-RG entry/exit, the sequence can make
    // the Graphics subsystem busy momentarily. Now at this moment
    // if EI is in power off state or in during entry sequence,
    // this will cause the EI to think that there is traffic on Graphics
    // region and we will wake/abort the EI FSM, however in actually
    // there is no traffic.
    //
    // Therefor to overcome this scenario we are doing the following:
    // Whever we are doing the entry/exit of GR-PG or GR-RG, we will
    // remove the GR and GR-CE idle signals from EI idle equation
    // for that period of time. So that EI will not get wakeup becasue
    // of this momentarily traffic.
    //
    // Once the Entry/exit is complete, we will add back the above idle
    // signals back to EI Idle equation.
    //
    // EI will still keep monitoring other idle signals.
    //
    switch (pPgLogicState->eventId)
    {
        // Entry/Exit Start Events
        case PMU_PG_EVENT_PG_ON:
        case PMU_PG_EVENT_POWERING_UP:
        {
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, lpwrLp)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                // Update the EI Idle mask with non GR idle signals.
                pgCtrlIdleMaskSet_HAL(&Pg, RM_PMU_LPWR_CTRL_ID_EI,
                                      Ei.idleMaskNonGr);
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            break;
        }

        // Entry/Exit Completed events
        case PMU_PG_EVENT_POWERED_DOWN:
        case PMU_PG_EVENT_DENY_PG_ON:
        case PMU_PG_EVENT_POWERED_UP:
        {
            pPgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_EI);

            // Restore the original EI Idle mask
            pgCtrlIdleMaskSet_HAL(&Pg, RM_PMU_LPWR_CTRL_ID_EI, pPgState->idleMask);
            break;
        }
    }
}
