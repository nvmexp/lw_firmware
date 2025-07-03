/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pglogicsm.c
 * @brief  PMU Logic - PG State machine
 *
 * State machine is one of PG Logic supported by PMU. PG logic receives
 * events and acts upon supported powergatables.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- System Includes -------------------------------- */
#include "dev_pwr_falcon_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpg.h"
#include "pmu_objlpwr.h"
#include "pmu_objpmu.h"
#include "pmu_objtimer.h"
#include "dbgprintf.h"

/* ------------------------- Macros and Defines ----------------------------- */
#define PG_LOGIC_SM_EVENT_MASK                                                \
    (PMU_PG_EVENT_ALL_MASK &                                                  \
     ~(BIT(PMU_PG_EVENT_NONE)                                                |\
       BIT(PMU_PG_EVENT_DENY_PG_ON)                                          |\
       BIT(PMU_PG_EVENT_POWERED_UP)                                          |\
       BIT(PMU_PG_EVENT_POWERED_DOWN)                                        |\
       BIT(PMU_PG_EVENT_DISALLOW_ACK)))

/*!
 * @brief Macros to check ALLOW, DISALLOW and WAKEUP events
 */
#define IS_ALLOW_EVENT(pgEventId)           ((pgEventId) == PMU_PG_EVENT_ALLOW)
#define IS_DISALLOW_EVENT(pgEventId)     ((pgEventId) == PMU_PG_EVENT_DISALLOW)
#define IS_WAKEUP_EVENT(pgEventId)         ((pgEventId) == PMU_PG_EVENT_WAKEUP)

/*!
 * @brief Macros to check DISALLOW pending and WAKEUP pending events
 */
#define IS_DISALLOW_EVENT_PENDING(pPgSt, pgEventId)                           \
        (((pgEventId) == PMU_PG_EVENT_CHECK_STATE)                          &&\
         (PG_IS_STATE_PENDING(pPgSt, DISALLOW)))
#define IS_WAKEUP_EVENT_PENDING(pPgSt, pgEventId)                             \
        (((pgEventId) == PMU_PG_EVENT_CHECK_STATE)                          &&\
         (PG_IS_STATE_PENDING(pPgSt, WAKEUP)))

// PG State Machine Debug
#ifdef PG_DEBUG
#define PG_DEBUG_COUNT 20
LwU32 ElpgLogCnt = 0;
LwU32 ElpgLog[PG_DEBUG_COUNT];
LwU32 ElpgTimeLog[PG_DEBUG_COUNT];
#define ELPG_STATE_START(eng, event)                                          \
do {                                                                          \
    ElpgLog[ElpgLogCnt] = (0xA                 << 28) |                       \
                          ((eng)->id           << 24) |                       \
                          ((eng)->state        << 16) |                       \
                          ((event)             <<  8) |                       \
                          ((eng)->statePending <<  0);                        \
    ElpgTimeLog[ElpgLogCnt] = REG_RD32(CSB, LW_CMSDEC_FALCON_PTIMER0);        \
    ElpgLogCnt = (ElpgLogCnt + 1) % PG_DEBUG_COUNT;                           \
} while (LW_FALSE)
#define ELPG_STATE_END(eng, event)                                            \
do                                                                            \
{                                                                             \
    ElpgLog[ElpgLogCnt] = (0xB                 << 28) |                       \
                          ((eng)->id           << 24) |                       \
                          ((eng)->state        << 16) |                       \
                          ((event)             <<  8) |                       \
                          ((eng)->statePending <<  0);                        \
    ElpgTimeLog[ElpgLogCnt] = REG_RD32(CSB, LW_CMSDEC_FALCON_PTIMER0);        \
    ElpgLogCnt = (ElpgLogCnt + 1) % PG_DEBUG_COUNT;                           \
} while (LW_FALSE)
#else
#define ELPG_STATE_START(eng, event)
#define ELPG_STATE_END(eng, event)
#endif

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

#if PMUCFG_FEATURE_ENABLED(PMU_LPWR_DEBUG)
LwU32 LpwrDebugInfo[LPWR_DEBUG_INFO_IDX_MAX];
#endif // PMUCFG_FEATURE_ENABLED(PMU_LPWR_DEBUG)

/* ------------------------- Prototypes ------------------------------------- */
// PG State Machine
static void        s_pgStateMachinePre (PG_LOGIC_STATE *);
static FLCN_STATUS s_pgStateMachine    (PG_LOGIC_STATE *);
static void        s_pgStateMachinePost(PG_LOGIC_STATE *);

// PG State Machine Helper Functions
static FLCN_STATUS s_pgStateDefault       (OBJPGSTATE *, PG_LOGIC_STATE *, LwBool *);
static FLCN_STATUS s_pgStatePwrOn         (OBJPGSTATE *, PG_LOGIC_STATE *, LwBool *);
static FLCN_STATUS s_pgStateOn2OffPgEng   (OBJPGSTATE *, PG_LOGIC_STATE *, LwBool *);
static FLCN_STATUS s_pgStateOn2OffLpwrEng (OBJPGSTATE *, PG_LOGIC_STATE *, LwBool *);
static FLCN_STATUS s_pgStatePwrOff        (OBJPGSTATE *, PG_LOGIC_STATE *, LwBool *);
static FLCN_STATUS s_pgStateOff2OnPgEng   (OBJPGSTATE *, PG_LOGIC_STATE *, LwBool *);
static FLCN_STATUS s_pgStateOff2OnLpwrEng (OBJPGSTATE *, PG_LOGIC_STATE *, LwBool *);
static FLCN_STATUS s_pgStateDisallow      (OBJPGSTATE *, PG_LOGIC_STATE *, LwBool *);
static void        s_pgStateResetOnEntry  (OBJPGSTATE *);

/* ------------------------- Public Functions ------------------------------- */
/*!
 * PG Logic Event Handler - PG State Machine
 *
 * Implements FSM for PgCtrl. It takes events for given PgCtrl and feed events
 * to State Machine.
 * 1) Pre-SM : Preprocessing of few events before feeding them to main SM
 * 2) SM     : Does actual state transitions
 * 3) Post-SM: Postprocessing on few events
 *
 * @param[in/out]   pPgLogicState   Pointer to PG Logic state
 */
void
pgLogicStateMachine
(
    PG_LOGIC_STATE *pPgLogicState
)
{
    s_pgStateMachinePre (pPgLogicState);
    s_pgStateMachine    (pPgLogicState);
    s_pgStateMachinePost(pPgLogicState);
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Pre FSM event handler
 *
 * Helper function to process ALLOW and DISALLOW events. It forward only one
 * ALLOW and one DISALLOW event to FSM.
 *
 * @param[in]   pPgLogicState   Pointer to PG Logic state
 */
static void
s_pgStateMachinePre
(
    PG_LOGIC_STATE *pPgLogicState
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(pPgLogicState->ctrlId);

    if (pPgLogicState->eventId == PMU_PG_EVENT_WAKEUP)
    {
        PMU_HALT_COND(pPgLogicState->data);

        pPgState->stats.wakeUpEvents |= pPgLogicState->data;
        return;
    }

    if ((pPgLogicState->eventId != PMU_PG_EVENT_ALLOW) &&
        (pPgLogicState->eventId != PMU_PG_EVENT_DISALLOW))
    {
        return;
    }

    // Update the debug info
    LPWR_DEBUG_INFO_UPDATE(LPWR_DEBUG_INFO_IDX_0, pPgLogicState->ctrlId);
    LPWR_DEBUG_INFO_UPDATE(LPWR_DEBUG_INFO_IDX_1, pPgLogicState->data);
    LPWR_DEBUG_INFO_UPDATE(LPWR_DEBUG_INFO_IDX_2, pPgState->state);
    LPWR_DEBUG_INFO_UPDATE(LPWR_DEBUG_INFO_IDX_3, pPgState->disallowReasonMask);
    LPWR_DEBUG_INFO_UPDATE(LPWR_DEBUG_INFO_IDX_4, pPgState->disallowAckPendingMask);

    appTaskCriticalEnter();

    if (pPgLogicState->eventId == PMU_PG_EVENT_DISALLOW)
    {
        PMU_HALT_COND(pPgLogicState->data);
        PMU_HALT_COND((pPgState->disallowReasonMask & pPgLogicState->data) == 0);

        // Immediately ACK disallow event if we are in disallow state
        if (pPgState->state == PMU_PG_STATE_DISALLOW)
        {
            pPgLogicState->eventId = PMU_PG_EVENT_DISALLOW_ACK;
        }
        //
        // PgCtrl is not in DISALLOW state. So we can't send the DISALLOW_ACK.
        // Mark DISALLOW_ACK pending. s_pgStateMachinePost() will send ACK on
        // completion of existing DISALLOW request.
        //
        else
        {
            pPgState->disallowAckPendingMask |= pPgLogicState->data;

            //
            // Skip forwarding this request if previous DISALLOW request is
            // in progress.
            //
            if (pPgState->disallowReasonMask != 0)
            {
                pPgLogicState->eventId = PMU_PG_EVENT_NONE;
            }
        }

        pPgState->disallowReasonMask |= pPgLogicState->data;
    }
    else // if (pPgLogicState->eventId == PMU_PG_EVENT_ALLOW)
    {
        PMU_HALT_COND((pPgState->disallowReasonMask & pPgLogicState->data) != 0);

        pPgState->disallowReasonMask &= (~pPgLogicState->data);

        //
        // It might be possible that this allow request came before
        // completion of previous DISALLOW request. Thus, we need to clear
        // pending DISALLOW_ACK.
        //
        pPgState->disallowAckPendingMask &= (~pPgLogicState->data);

        //
        // Skip forwarding this ALLOW request if other clients have marked
        // for DISALLOW.
        //
        if (pPgState->disallowReasonMask != 0)
        {
            pPgLogicState->eventId = PMU_PG_EVENT_NONE;
        }
    }

    // Update the Stats cache with SW disable reason mask
    pPgState->stats.swDisallowReasonMask = pPgState->disallowReasonMask;

    appTaskCriticalExit();
}

/*!
 * @brief Pre FSM event handler
 *
 * Helper function to process DISALLOW_ACK events. It adds up pending ACKs.
 *
 * @param[in]   pPgLogicState   Pointer to PG Logic state
 */
static void
s_pgStateMachinePost
(
    PG_LOGIC_STATE *pPgLogicState
)
{
    OBJPGSTATE *pPgState;

    // Merge pending DISALLOW_ACK and reset the pending mask.
    if (pPgLogicState->eventId == PMU_PG_EVENT_DISALLOW_ACK)
    {
        pPgState = GET_OBJPGSTATE(pPgLogicState->ctrlId);

        appTaskCriticalEnter();

        pPgLogicState->data |= pPgState->disallowAckPendingMask;
        pPgState->disallowAckPendingMask = 0;

        appTaskCriticalExit();
    }
}

/*!
 *
 * The "real" main PG event handler.
 * This function implements a generic PG logic, and take appropriate actions
 * depending on the current state of the pg controller.
 *
 * [PG State Machine]
 *
 * Full state machine diagram can be found in
 * //sw/pvt/qpark/elpg/docs/PMU ELPG.doc or
 * http://teams.lwpu.com/sites/gpu/sw/PMU/ELPG/ELPG%20Docs/PMU%20ELPG.doc
 *
 *  EVENTS    | PWR_ON |CHK_BUSY|PWR_OFF|OFF2ON|ON2OFF |  FRZ  |DISALLOW|FRZ_RDY
 * -----------|--------|--------|-------|------|-------|-------|--------|--------
 *            |   -    | ON2OFF |   -   |   -  |   -   |   -   |   -    |    -
 * ALLOW      |   -    |    -   |   -   |   -  |   -   |   -   | PWR_ON |    -
 * Busy       |   -    | PWR_ON |   -   |   -  |   -   |   -   |   -    |    -
 * CTX_RESTORE|   -    |    -   |   -   |PWR_ON|   -   |   -   |   -    |    -
 * DISALLOW   |DISALLOW|    -   | OFF2ON|   -  |   -   |   -   |   -    |DISALLOW
 * ENG_RST    |   -    |    -   |   -   |OFF2ON|   -   |   -   |   -    |    -
 * HOLD_OFF   |   -    |    -   | OFF2ON|   -  |   -   |   -   |   -    |    -
 * PG_ON      |CHK_BUSY|    -   |   -   |   -  |   -   |   -   |   -    |CHK_BUSY
 * PG_ON_DONE |    -   |    -   |   -   |   -  |PWR_OFF|   -   |   -    |    -
 *
 * @param[in/out]   pPgLogicState   Pointer to PG Logic state
 *
 * @return 'FLCN_OK' if an elpg event is handled successfully.
 */
static FLCN_STATUS
s_pgStateMachine
(
    PG_LOGIC_STATE *pPgLogicState
)
{
    OBJPGSTATE *pPgState = NULL;
    FLCN_STATUS status  = FLCN_OK;
    LwBool      bDone   = LW_FALSE;

    if ((BIT(pPgLogicState->eventId) & PG_LOGIC_SM_EVENT_MASK) == 0)
    {
        return FLCN_OK;
    }

    pPgState = GET_OBJPGSTATE(pPgLogicState->ctrlId);

    while ((!bDone) &&
           (status == FLCN_OK))
    {
        ELPG_STATE_START(pPgState, pPgLogicState->eventId);

        switch (pPgState->state)
        {
            case PMU_PG_STATE_PWR_ON:
            {
                status = s_pgStatePwrOn(pPgState, pPgLogicState, &bDone);
                break;
            }
            case PMU_PG_STATE_ON2OFF:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_ENG) &&
                    LPWR_ARCH_IS_SF_SUPPORTED(LPWR_ENG))
                {
                    status = s_pgStateOn2OffLpwrEng(pPgState, pPgLogicState, &bDone);
                }
                else
                {
                    status = s_pgStateOn2OffPgEng(pPgState, pPgLogicState, &bDone);
                }
                break;
            }
            case PMU_PG_STATE_PWR_OFF:
            {
                status = s_pgStatePwrOff(pPgState, pPgLogicState, &bDone);
                break;
            }
            case PMU_PG_STATE_OFF2ON:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_ENG) &&
                    LPWR_ARCH_IS_SF_SUPPORTED(LPWR_ENG))
                {
                    status = s_pgStateOff2OnLpwrEng(pPgState, pPgLogicState, &bDone);
                }
                else
                {
                    status = s_pgStateOff2OnPgEng(pPgState, pPgLogicState, &bDone);
                }
                break;
            }
            case PMU_PG_STATE_DISALLOW:
            {
                status = s_pgStateDisallow(pPgState, pPgLogicState, &bDone);
                break;
            }
            default:
            {
                status = FLCN_ERROR;
            }
        }

        ELPG_STATE_END(pPgState, pPgLogicState->eventId);
    }

    PMU_HALT_COND(status == FLCN_OK);

    return status;
}

/*!
 * @brief PG State Machine - Handle Default States
 *
 * @param[in]     pPgState        OBJPGSTATE pointer
 * @param[in/out] pPgLogicState   Pointer to PG Logic state
 * @param[out]    pbDone          Pointer to bDone.
 *                                FALSE if there's more work to do.
 *
 * @returns FLCN_OK
 */
static FLCN_STATUS
s_pgStateDefault
(
    OBJPGSTATE     *pPgState,
    PG_LOGIC_STATE *pPgLogicState,
    LwBool         *pbDone
)
{
    FLCN_STATUS status = FLCN_OK;

    //
    // NO-OP events:
    //
    // CHECK_STATE:
    // Check state event is responsible for handling pending states like
    // pending disallow, pending allow, pending wake. Not all state act on
    // pending events. Ignore it if current state doesn't handle CHECK_STATE
    // event.
    //
    if (pPgLogicState->eventId == PMU_PG_EVENT_CHECK_STATE)
    {
    }
    //
    // WAKEUP event:
    //
    // Only PWROFF state reacts to WAKEUP. Set the pending bit for further
    // processing. Pending bit gets clear while processing next PG_ON
    // interrupt (in ON2OFF state).
    //
    // Note: WAKEUP event is no-op for DISALLOW, PWR_ON and OFF2ON state.
    // Thus, no processing done for these states. We simply clear pending
    // bit on next PG_ON interrupt.
    //
    else if (IS_WAKEUP_EVENT(pPgLogicState->eventId))
    {
        PMU_HALT_COND(pPgState->stats.wakeUpEvents);
        PG_SET_PENDING_STATE(pPgState, WAKEUP);
    }
    //
    // DISALLOW event:
    //
    // PWROFF, PWRON and DISALLOW states react to DISALLOW event. We will be in
    // default handler if FSM is in ON2OFF or OFF2ON state. Set the pending bit
    // for further processing. Pending bit gets clear while transitioning from
    // DISALLOW to ALLOW state.
    //
    else if (IS_DISALLOW_EVENT(pPgLogicState->eventId))
    {
        PG_SET_PENDING_STATE(pPgState, DISALLOW);
    }
    //
    // ALLOW event:
    //
    // ALLOW event is processed only in DISALLOW state. We are in default handler,
    // means current state is not DISALLOW. FSM might be processing previous
    // DISALLOW. Stop DISALLOW processing. Colwert DISALLOW event to WAKEUP event.
    //
    else if (IS_ALLOW_EVENT(pPgLogicState->eventId))
    {
        PMU_HALT_COND(PG_IS_STATE_PENDING(pPgState, DISALLOW));
        PG_CLEAR_PENDING_STATE(pPgState, DISALLOW);

        PG_SET_PENDING_STATE(pPgState, WAKEUP);
        pPgLogicState->eventId        = PMU_PG_EVENT_CHECK_STATE;
        pPgLogicState->data           = 0;
        pPgState->stats.wakeUpEvents |= PG_WAKEUP_EVENT_EARLY_ALLOW;

        *pbDone = LW_FALSE;
    }
    // Given event is not expected in current state. Return error.
    else
    {
        status = FLCN_ERROR;
    }

    return status;
}

/*!
 * @brief PG State Machine - Handle PWR_ON State
 *
 * @param[in]     pPgState        OBJPGSTATE pointer
 * @param[in/out] pPgLogicState   Pointer to PG Logic state
 * @param[out]    pbDone          Pointer to bDone.
 *                                 FALSE if there's more work to do.
 *
 * @returns FLCN_OK   if successful
 * @returns FLCN_ERROR otherwise
 */
static FLCN_STATUS
s_pgStatePwrOn
(
    OBJPGSTATE     *pPgState,
    PG_LOGIC_STATE *pPgLogicState,
    LwBool         *pbDone
)
{
    PMU_HALT_COND(pPgState->state == PMU_PG_STATE_PWR_ON);
    *pbDone = LW_TRUE;

    if (IS_DISALLOW_EVENT(pPgLogicState->eventId) ||
       (IS_DISALLOW_EVENT_PENDING(pPgState, pPgLogicState->eventId)))
    {
        if (IS_DISALLOW_EVENT(pPgLogicState->eventId))
        {
            PG_SET_PENDING_STATE(pPgState, DISALLOW);
        }

        pPgState->state = PMU_PG_STATE_DISALLOW;
        pgCtrlInterruptsEnable_HAL(&Pg, pPgState->id, LW_FALSE);

        //
        // DISALLOW state need to do further processing of DIALLOW and
        // DISALLOW-PENDING events.
        //
        *pbDone = LW_FALSE;
    }
    else if (pPgLogicState->eventId == PMU_PG_EVENT_PG_ON)
    {
        //
        // Change SW state to _ON2OFF. SW is starting entry
        // sequence or saving the cotext pPgIf->save()
        //
        pPgState->state = PMU_PG_STATE_ON2OFF;

        // Continue the processing of PG_ON event in _ON2OFF state.
        *pbDone = LW_FALSE;
    }
    else
    {
        // Handle the rest of events
        return s_pgStateDefault(pPgState, pPgLogicState, pbDone);
    }

    return FLCN_OK;
}

/*!
 * @brief PG State Machine - Handle ON2OFF State for PgEng
 *
 * @param[in]     pPgState        OBJPGSTATE pointer
 * @param[in/out] pPgLogicState   Pointer to PG Logic state
 * @param[out]    pbDone          Pointer to bDone.
 *                                FALSE if there's more work to do.
 *
 * @returns FLCN_OK   if successful
 * @returns FLCN_ERROR otherwise
 */
static FLCN_STATUS
s_pgStateOn2OffPgEng
(
    OBJPGSTATE     *pPgState,
    PG_LOGIC_STATE *pPgLogicState,
    LwBool         *pbDone
)
{
    PMU_HALT_COND(pPgState->state == PMU_PG_STATE_ON2OFF);
    *pbDone = LW_TRUE;

    if (pPgLogicState->eventId == PMU_PG_EVENT_PG_ON)
    {
        //
        // We have started new cycle. It's safe to clear previous pending
        // wake-up event (if any).
        //
        PG_CLEAR_PENDING_STATE(pPgState, WAKEUP);

        // Successful SW entry
        if (pPgState->pgIf.lpwrEntry() == FLCN_OK)
        {
            s_pgStateResetOnEntry(pPgState);

            // Clear PG_ON interrupt
            pgCtrlInterruptClear_HAL(&Pg, pPgState->id, pPgLogicState->eventId);
        }
        // Aborted on entry
        else
        {
            //
            // Context save or SW entry sequence aborted:
            // 1) Send DENY_PG_ON event to notify the abort
            // 2) Reset PG_ENG
            //

            //
            // Send DENY event so that this can be logged. Alternatively, we could
            // have added pgLogicLoggingPost instead of sending a DENY event.
            //
            pgCtrlEventSend(pPgState->id, PMU_PG_EVENT_DENY_PG_ON, 0);

            //
            // set event to NONE as we're forwarding this event. It's important
            // that we do this before pgCtrlSoftReset.
            //
            pPgLogicState->eventId = PMU_PG_EVENT_NONE;

            // engine is busy.
            pgCtrlSoftReset_HAL(&Pg, pPgState->id);

            // Restore state to PWR_ON.
            pPgState->state = PMU_PG_STATE_PWR_ON;
        }
    }
    else if (pPgLogicState->eventId == PMU_PG_EVENT_PG_ON_DONE)
    {
        // Send POWERED_DOWN notification event
        pgCtrlEventSend(pPgState->id, PMU_PG_EVENT_POWERED_DOWN, 0);

        // Update SW state to _PWR_OFF
        pPgState->state = PMU_PG_STATE_PWR_OFF;

        // Clear PG_ON_DONE interrupt
        pgCtrlInterruptClear_HAL(&Pg, pPgState->id, pPgLogicState->eventId);

        // Check for pending actions like pending DISALLOW events
        pPgLogicState->eventId = PMU_PG_EVENT_CHECK_STATE;
        *pbDone = LW_FALSE;
    }
    else
    {
        // Handle the rest of events
        return s_pgStateDefault(pPgState, pPgLogicState, pbDone);
    }

    return FLCN_OK;
}

/*!
 * @brief PG State Machine - Handle ON2OFF State for LpwrEng
 *
 * @param[in]     pPgState        OBJPGSTATE pointer
 * @param[in/out] pPgLogicState   Pointer to PG Logic state
 * @param[out]    pbDone          Pointer to bDone.
 *                                FALSE if there's more work to do.
 *
 * @returns FLCN_OK   if successful
 * @returns FLCN_ERROR otherwise
 */
static FLCN_STATUS
s_pgStateOn2OffLpwrEng
(
    OBJPGSTATE     *pPgState,
    PG_LOGIC_STATE *pPgLogicState,
    LwBool         *pbDone
)
{
    PMU_HALT_COND(pPgState->state == PMU_PG_STATE_ON2OFF);
    *pbDone = LW_TRUE;

    if (pPgLogicState->eventId == PMU_PG_EVENT_PG_ON)
    {
        //
        // We have started new cycle. It's safe to clear previous pending
        // wake-up event (if any).
        //
        PG_CLEAR_PENDING_STATE(pPgState, WAKEUP);

        // Successful SW entry
        if (pPgState->pgIf.lpwrEntry() == FLCN_OK)
        {
            s_pgStateResetOnEntry(pPgState);

            // Send POWERED_DOWN notification event
            pgCtrlEventSend(pPgState->id, PMU_PG_EVENT_POWERED_DOWN, 0);

            // Update SW state to _PWR_OFF
            pPgState->state = PMU_PG_STATE_PWR_OFF;

            // Clear PG_ON interrupt
            pgCtrlInterruptClear_HAL(&Pg, pPgState->id, pPgLogicState->eventId);

            // Check for pending actions like pending DISALLOW events
            pPgLogicState->eventId = PMU_PG_EVENT_CHECK_STATE;
            *pbDone = LW_FALSE;
        }
        // Aborted on entry
        else
        {
            //
            // Context save or SW entry sequence aborted:
            // 1) Send DENY_PG_ON event to notify the abort
            // 2) Reset PG_ENG
            //

            //
            // Send DENY event so that this can be logged. Alternatively, we could
            // have added pgLogicLoggingPost instead of sending a DENY event.
            //
            pgCtrlEventSend(pPgState->id, PMU_PG_EVENT_DENY_PG_ON, 0);

            //
            // set event to NONE as we're forwarding this event. It's important
            // that we do this before pgCtrlSoftReset.
            //
            pPgLogicState->eventId = PMU_PG_EVENT_NONE;

            // engine is busy.
            pgCtrlSoftReset_HAL(&Pg, pPgState->id);

            // Restore state to PWR_ON.
            pPgState->state = PMU_PG_STATE_PWR_ON;
        }
    }
    else
    {
        // Handle the rest of events
        return s_pgStateDefault(pPgState, pPgLogicState, pbDone);
    }

    return FLCN_OK;
}

/*!
 * @brief PG State Machine - Handle PWR_OFF State
 *
 * @param[in]     pPgState        OBJPGSTATE pointer
 * @param[in/out] pPgLogicState   Pointer to PG Logic state
 * @param[out]    pbDone          Pointer to bDone.
 *                                FALSE if there's more work to do.
 *
 * @returns FLCN_OK   if successful
 * @returns FLCN_ERROR otherwise
 */
static FLCN_STATUS
s_pgStatePwrOff
(
    OBJPGSTATE     *pPgState,
    PG_LOGIC_STATE *pPgLogicState,
    LwBool         *pbDone
)
{
    PMU_HALT_COND(pPgState->state == PMU_PG_STATE_PWR_OFF);
    *pbDone = LW_TRUE;

    if (IS_WAKEUP_EVENT(pPgLogicState->eventId)                   ||
             IS_DISALLOW_EVENT(pPgLogicState->eventId)                 ||
             IS_WAKEUP_EVENT_PENDING(pPgState, pPgLogicState->eventId) ||
             IS_DISALLOW_EVENT_PENDING(pPgState, pPgLogicState->eventId))
    {
        //
        // Set pending state bit for DISALLOW event. FSM will look at this
        // bit after completion of exiting sleep state and move PgCtrl to
        // DISALLOW state.
        //
        if (IS_DISALLOW_EVENT(pPgLogicState->eventId))
        {
            PG_SET_PENDING_STATE(pPgState, DISALLOW);
        }

        // Update SW state to _OFF2ON
        pPgState->state = PMU_PG_STATE_OFF2ON;

        // Send POWERING_UP notification event
        pgCtrlEventSend(pPgState->id, PMU_PG_EVENT_POWERING_UP, 0);
    }
    else
    {
        // Handle the rest of events
        return s_pgStateDefault(pPgState, pPgLogicState, pbDone);
    }

    return FLCN_OK;
}

/*!
 * @brief PG State Machine - Handle OFF2ON State for PgEng
 *
 * @param[in]     pPgState        OBJPGSTATE pointer
 * @param[in/out] pPgLogicState   Pointer to PG Logic state
 * @param[out]    pbDone          Pointer to bDone.
 *                                FALSE if there's more work to do.
 *
 * @returns FLCN_OK   if successful
 * @returns FLCN_ERROR otherwise
 */
static FLCN_STATUS
s_pgStateOff2OnPgEng
(
    OBJPGSTATE     *pPgState,
    PG_LOGIC_STATE *pPgLogicState,
    LwBool         *pbDone
)
{
    PMU_HALT_COND(pPgState->state == PMU_PG_STATE_OFF2ON);
    *pbDone = LW_TRUE;

    if (pPgLogicState->eventId == PMU_PG_EVENT_POWERING_UP)
    {
        // Trigger the wake up for HW SM
        pgCtrlSmAction_HAL(&Pg, pPgState->id, PMU_PG_SM_ACTION_PG_OFF_START);

        // Process pending idle-snap interrupt
        if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_IDLE_SNAP))
        {
            pgCtrlIdleSnapProcess_HAL(&Pg, pPgState->id);
        }
    }
    else if (pPgLogicState->eventId == PMU_PG_EVENT_ENG_RST)
    {
        // Reset the targeted unit
        pPgState->pgIf.lpwrReset();

        // Clear ENG_RST interrupt
        pgCtrlInterruptClear_HAL(&Pg, pPgState->id, pPgLogicState->eventId);
    }
    else if (pPgLogicState->eventId == PMU_PG_EVENT_CTX_RESTORE)
    {
        // Execute SW exit sequence
        pPgState->pgIf.lpwrExit();

        // Send POWERED_UP event
        pgCtrlEventSend(pPgState->id, PMU_PG_EVENT_POWERED_UP, 0);

        // Update SW state to _PWR_ON
        pPgState->state = PMU_PG_STATE_PWR_ON;

        // Clear CTX_RESTORE interrupt
        pgCtrlInterruptClear_HAL(&Pg, pPgState->id, pPgLogicState->eventId);

        // Check for pending actions like pending DISALLOW events
        pPgLogicState->eventId = PMU_PG_EVENT_CHECK_STATE;
        *pbDone = LW_FALSE;
    }
    else
    {
        // Handle the rest of events
        return s_pgStateDefault(pPgState, pPgLogicState, pbDone);
    }

    return FLCN_OK;
}

/*!
 * @brief PG State Machine - Handle OFF2ON State for LpwrEng
 *
 * @param[in]     pPgState        OBJPGSTATE pointer
 * @param[in/out] pPgLogicState   Pointer to PG Logic state
 * @param[out]    pbDone          Pointer to bDone.
 *                                FALSE if there's more work to do.
 *
 * @returns FLCN_OK   if successful
 * @returns FLCN_ERROR otherwise
 */
static FLCN_STATUS
s_pgStateOff2OnLpwrEng
(
    OBJPGSTATE     *pPgState,
    PG_LOGIC_STATE *pPgLogicState,
    LwBool         *pbDone
)
{
    PMU_HALT_COND(pPgState->state == PMU_PG_STATE_OFF2ON);
    *pbDone = LW_TRUE;

    if (pPgLogicState->eventId == PMU_PG_EVENT_POWERING_UP)
    {
        // Trigger the wake up for HW SM
        pgCtrlSmAction_HAL(&Pg, pPgState->id, PMU_PG_SM_ACTION_PG_OFF_START);

        // Process pending idle-snap interrupt
        if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_IDLE_SNAP))
        {
            pgCtrlIdleSnapProcess_HAL(&Pg, pPgState->id);
        }

        // Execute SW exit sequence
        pPgState->pgIf.lpwrExit();

        // Send POWERED_UP event
        pgCtrlEventSend(pPgState->id, PMU_PG_EVENT_POWERED_UP, 0);

        // Update SW state to _PWR_ON
        pPgState->state = PMU_PG_STATE_PWR_ON;

        // Notify the completion of exit sequence to HW SM
        pgCtrlSmAction_HAL(&Pg, pPgState->id, PMU_PG_SM_ACTION_PG_OFF_DONE);

        //
        // Handle SELF_DISALLOW request
        //
        // - Set DISALLOW pending state so that FSM will continue exit
        //   sequence until it reaches to DISALLOW state.
        // - Update disallow reason mask - DISALLOW_REASON_MASK_SELF.
        // - Set disallow pending ack. ACK will be send after moving to
        //   DISALLOW state.
        // - To reenable the FSM from Self Disallow State, pgctrlAllow()
        //   API has to be used. Please note: Self Disallow feature is
        //   internal to LPWR Task. Hence pgctrlAllow function has to be
        //   called from LPWR Task only.
        //
        if (pPgState->bSelfDisallow)
        {
            PG_SET_PENDING_STATE(pPgState, DISALLOW);

            //
            // pgCtrlAllow/DisallowExt() APIs access disallowReasonMask and
            // disallowAckPendingMask from other tasks. Thus use critical
            // section to ensure an automic update.
            //
            appTaskCriticalEnter();
            {
                pPgState->disallowReasonMask     |= PG_CTRL_DISALLOW_REASON_MASK(SELF);
                pPgState->disallowAckPendingMask |= PG_CTRL_DISALLOW_REASON_MASK(SELF);
                pPgState->disallowReentrancyMask |= PG_CTRL_DISALLOW_REASON_MASK(SELF);

                // Update the Stats cache with SW disable reason mask
                pPgState->stats.swDisallowReasonMask = pPgState->disallowReasonMask;
            }
            appTaskCriticalExit();
        }

        // Check for pending actions like pending DISALLOW events
        pPgLogicState->eventId = PMU_PG_EVENT_CHECK_STATE;
        *pbDone = LW_FALSE;
    }
    else
    {
        // Handle the rest of events
        return s_pgStateDefault(pPgState, pPgLogicState, pbDone);
    }

    return FLCN_OK;
}

/*!
 * @brief PG State Machine - Handle DISALLOW State
 *
 * @param[in]     pPgState        OBJPGSTATE pointer
 * @param[in/out] pPgLogicState   Pointer to PG Logic state
 * @param[out]    pbDone          Pointer to bDone.
 *                                FALSE if there's more work to do.
 *
 * @returns FLCN_OK   if successful
 * @returns FLCN_ERROR otherwise
 */
static FLCN_STATUS
s_pgStateDisallow
(
    OBJPGSTATE     *pPgState,
    PG_LOGIC_STATE *pPgLogicState,
    LwBool         *pbDone
)
{
    PMU_HALT_COND(pPgState->state == PMU_PG_STATE_DISALLOW);
    *pbDone = LW_TRUE;

    if (IS_ALLOW_EVENT(pPgLogicState->eventId))
    {
        // Clear the pending bit for DISALLOW event
        PG_CLEAR_PENDING_STATE(pPgState, DISALLOW);

        // Enable HW SM interrupts
        pgCtrlInterruptsEnable_HAL(&Pg, pPgState->id, LW_TRUE);

        // Update SW state to PWR_ON
        pPgState->state = PMU_PG_STATE_PWR_ON;
    }
    else if (IS_DISALLOW_EVENT(pPgLogicState->eventId) ||
             IS_DISALLOW_EVENT_PENDING(pPgState, pPgLogicState->eventId))
    {
        pPgLogicState->eventId = PMU_PG_EVENT_DISALLOW_ACK;
    }
    else if (pPgLogicState->eventId == PMU_PG_EVENT_PG_ON)
    {
        //
        // Send DENY event so that this can be logged. Alternatively, we could
        // have added pgLogicLoggingPost instead of sending a DENY event.
        //
        pgCtrlEventSend(pPgState->id, PMU_PG_EVENT_DENY_PG_ON, 0);

        //
        // Set event to NONE as we're forwarding this event. It's important
        // that we do this before pgCtrlSoftReset.
        //
        pPgLogicState->eventId = PMU_PG_EVENT_NONE;

        //
        // It may be possible that PG_ON event may slip in before
        // disabling PG interrupt. In that case, just reset PG controller
        //
        pgCtrlSoftReset_HAL(&Pg, pPgState->id);
    }
    else
    {
        // Handle the rest of events
        return s_pgStateDefault(pPgState, pPgLogicState, pbDone);
    }

    return FLCN_OK;
}

/*!
 * @brief Reset/initialize controller parameter on successful entry
 *
 * @param[in]     pPgState        OBJPGSTATE pointer
 */
static void
s_pgStateResetOnEntry
(
    OBJPGSTATE *pPgState
)
{
    LwU32 i;

    // Clear the wake-up reason mask if cumulative feature is disabled
    if (!pPgState->bLwmulativeWakeupMask)
    {
        pPgState->stats.wakeUpEvents = 0;
    }

    // Clear the abortReason
    pPgState->stats.abortReason = 0;

    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_IDLE_SNAP))
    {
        // Reset idle-snap state
        pPgState->idleSnapReasonMask = PG_IDLE_SNAP_REASON__INIT;

        for (i = 0; i < RM_PMU_PG_IDLE_BANK_SIZE; i++)
        {
            pPgState->idleStatusCache[i] = 0;
        }
    }
}
