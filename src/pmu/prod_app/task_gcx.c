/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2008-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_gcx.c
 * @brief  Represents an overlay task that is responsible for handling
 *         requests and signals related to entering and exiting deepidle.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "main_init_api.h"
#include "cmdmgmt/cmdmgmt.h"
#include "pmu_didle.h"
#include "dbgprintf.h"

#include "pmu_objpmu.h"
#include "pmu_objtimer.h"
#include "pmu_objpcm.h"
#include "pmu_objgcx.h"
#include "pmu_objpg.h"
#include "pmu_objdi.h"
#include "pmu_objdisp.h"
#include "pmu_objfifo.h"
#include "pmu_oslayer.h"
#include "pmu_disp.h"
#include "g_pmurmifrpc.h"

#include "g_pmurpc.h"

/* ------------------------- Global Variables ------------------------------- */
/*!
 * This is the lwrrently active deepidle state.  The possible values are
 * defined in the DIDLE_STATE enum.
 */
LwU8 DidleLwrState = DIDLE_STATE_INACTIVE;

/*!
 * This is the lwrrently active deepidle sub state when in the process
 * of entering or exiting deeepidle.  The possible values are defined in
 * the GC4_SUBSTATE enum.
 */
LwU8 DidleSubstate = GC4_SUBSTATE_START;

/*!
 * Following tracks various data points for:
 * deepidle NH
 */
RM_PMU_DIDLE_STAT DidleStatisticsNH;

/* ------------------------- Prototypes ------------------------------------- */
static LwU8 s_gcxDiosEnter(LwU8 nextState);
static void s_gcxDmaResume(void);

lwrtosTASK_FUNCTION(task_gcx, pvParameters);

/* ------------------------- Functions  ------------------------------------- */
/*!
 * Resets the statistics data for the specified statistics structure.
 *
 * @param[out]  pStats  Pointer to the statistics structure to clear.
 */
void didleResetStatistics
(
    RM_PMU_DIDLE_STAT *pStats
)
{
    memset(pStats, 0x00, sizeof(RM_PMU_DIDLE_STAT));
}

/*!
 * Sends response to RM for DISARM command
 *
 * @param[in] pCommand      Holds requested command
 * @param[in] cmdStatus     Holds requested command's status
 * @param[in] eventType     Holds type of the event
 */
void
didleMessageDisarmOs
(
    LwU8 cmdStatus
)
{
    PMU_RM_RPC_STRUCT_GCX_DIOS_DISARM_STATUS  rpc;
    FLCN_STATUS                               status;

    rpc.status = cmdStatus;

    // Send the Event as an RPC.  RC-TODO Propery handle status here.
    PMU_RM_RPC_EXELWTE_BLOCKING(status, GCX, DIOS_DISARM_STATUS, &rpc);

    Gcx.pGc4->bDisarmResponsePending = LW_FALSE;
}

/*!
 * Performs any processing based on the signal received and figures out the
 * next valid state to transition to for Deepidle GPIO signals. If the signal
 * should be ignored in the current state, the current state is returned.
 *
 * @param[in] signalType    Which signal was triggered
 *
 * @return the next valid end state resulting from the signal. Internal states
 *         of *_ENTERING_* are not returned as they are intermediate states
 *         and not end states.
 */
static LwU8
didleNextStateGetBySignal(void)
{
    LwU8            nextState   = DidleLwrState;
    OBJGC4         *pGc4        = GCX_GET_GC4(Gcx);
    LwU8            signalType  = pGc4->dispatch.sigGen.signalType;
    LwU32           pexSleepState;

    switch (signalType)
    {
        //
        // An L1 Entry signal indicates that we should go into IN_OS
        // if we're in ARMED_OS .
        //
        case DIDLE_SIGNAL_DEEP_L1_ENTRY:
        {
            Gcx.pGc4->bInDeepL1 = LW_TRUE;
            pexSleepState       = pGc4->dispatch.sigDeepL1.pexSleepState;

            DBG_PRINTF_DIDLE(("DI: L1 entry, lwrrstate %u\n", DidleLwrState, 0, 0, 0));

            nextState = gcxGC4NextStateGet_HAL(&Gcx);

            Di.pexSleepState = pexSleepState;

            // now re-enable the rising-edge Deep-L1 interrupts
            gcxDidleDeepL1SetInterrupt_HAL(&Gcx, LW_TRUE, LW_TRUE, pexSleepState);
            break;
        }

        //
        // An L1 Exit signal indicates that we should go into ARMED_OS
        // if we're in either IN_OS or ENTERING_OS.
        //
        case DIDLE_SIGNAL_DEEP_L1_EXIT:
        {
            Gcx.pGc4->bInDeepL1 = LW_FALSE;
            pexSleepState       = pGc4->dispatch.sigDeepL1.pexSleepState;

            DBG_PRINTF_DIDLE(("DI: L1 Exit,lwrrstate %u\n", DidleLwrState, 0, 0, 0));
            //
            // The L1 exit signal does not cause a state change during the
            // following states:
            // INACTIVE, ARMED_OS
            //
            //
            // If we're lwrrently in DIDLE_STATE_IN_OS or
            // DIDLE_STATE_ENTERING_OS, the next state is
            // DIDLE_STATE_ARMED_OS.
            //
            if ((DidleLwrState == DIDLE_STATE_IN_OS) ||
                (DidleLwrState == DIDLE_STATE_ENTERING_OS))
            {
                nextState = DIDLE_STATE_ARMED_OS;
            }

            // now re-enable the falling-edge Deep-L1 interrupts
            gcxDidleDeepL1SetInterrupt_HAL(&Gcx, LW_FALSE, LW_TRUE, pexSleepState);
            break;
        }

        case DIDLE_SIGNAL_ELPG_EXIT:
        case DIDLE_SIGNAL_ABORT:
        {
            DBG_PRINTF_DIDLE(("DI: Abort Event,lwrrstate %u\n", DidleLwrState, 0, 0, 0));
            //
            // The abort signal does not cause a state change during the
            // following states:
            // INACTIVE, ARMED_OS
            //
            //
            // If we're lwrrently in DIDLE_STATE_IN_OS or
            // DIDLE_STATE_ENTERING_OS, the next state is
            // DIDLE_STATE_ARMED_OS.
            //
            if ((DidleLwrState == DIDLE_STATE_IN_OS) ||
                (DidleLwrState == DIDLE_STATE_ENTERING_OS))
            {
                nextState = DIDLE_STATE_ARMED_OS;
            }
            break;
        }

        case DIDLE_SIGNAL_ELPG_ENTRY:
        {
            break;
        }

        case DIDLE_SIGNAL_MSCG_ENGAGED:
        {
            //
            // This event is anticipated to come from PG task
            // after MSCG engaged and OK_TO_SWITCH Interrupt
            //(configured in Display to signify DWCF)
            //
            if (PMUCFG_FEATURE_ENABLED(PMU_DIDLE_OS))
            {
                pGc4->bMscgEngaged = LW_TRUE;

                nextState = gcxGC4NextStateGet_HAL(&Gcx);
            }
            break;
        }

        case DIDLE_SIGNAL_MSCG_ABORTED:
        {
            //
            // This event comes to didle task when any MSCG wakeup
            // event happens.We should abort/exit SSC sequence here
            //
            if (PMUCFG_FEATURE_ENABLED(PMU_DIDLE_OS))
            {
                pGc4->bMscgEngaged  =  LW_FALSE;
                if ((DidleLwrState == DIDLE_STATE_IN_OS) ||
                    (DidleLwrState == DIDLE_STATE_ENTERING_OS))
                {
                    nextState = DIDLE_STATE_ARMED_OS;
                }
            }
            break;
        }

        case DIDLE_SIGNAL_MSCG_DISENGAGED:
        {
            // SSC-TODO
            break;
        }

#if PMUCFG_FEATURE_ENABLED(PMU_DIDLE_OS)
        case DIDLE_SIGNAL_DIOS_ATTEMPT_TO_ENGAGE:
        {
            nextState = gcxGC4NextStateGet_HAL(&Gcx);
            break;
        }
#endif // PMUCFG_FEATURE_ENABLED(PMU_DIDLE_OS)

        default:
        {
           // Illegal signal type
           DBG_PRINTF_DIDLE(("DI: ERROR: Illegal signal type\n", 0, 0, 0, 0));
           PMU_HALT();
           break;
        }
    }
    return nextState;
}

/*!
 * Gets the next request from the queue and determines what, if any, should
 * be the next state transition.
 *
 * @param[in]       timeout     Amount of time to block and wait to receive a
 *                              request from the deepidle queue. A value of
 *                              zero does not block and returns immediately.
 *
 * @return the next state to transition to after validating the command/signal.
 *         If no transition is required, the current state is returned.
 */
LwU8
didleNextStateGet
(
    LwU32 timeout
)
{
    LwU8 nextState      = DidleLwrState;
    FLCN_STATUS status  = FLCN_OK;

    //
    // Check for incoming commands or signals from the Deepidle queue
    //
    if (lwrtosQueueReceive(LWOS_QUEUE(PMU, GCX), &Gcx.pGc4->dispatch,
                           sizeof(Gcx.pGc4->dispatch), timeout) == FLCN_OK)
    {
        switch (Gcx.pGc4->dispatch.hdr.eventType)
        {
            case DISP2UNIT_EVT_RM_RPC:
            {
                status = pmuRpcProcessUnitGcx(&(Gcx.pGc4->dispatch.rpc), &nextState);
                break;
            }

            case DIDLE_SIGNAL_EVTID:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_GCX_DIDLE))
                {
                    nextState = didleNextStateGetBySignal();
                }
                else
                {
                    //
                    // We should not get any signal if DIDLE features are
                    // not supported.
                    //
                    PMU_HALT();
                }

                break;
            }

            default:
            {
               // Illegal event type
               DBG_PRINTF_DIDLE(("DI: ERROR: Illegal event type\n", 0, 0, 0, 0));
               PMU_HALT();
               break;
            }
        }
    }

    return nextState;
}

/*!
 * Takes actions necessary to change to the next state
 *
 * @param[in]     nextState     State to which to transition
 *
 * @return nextState if the transition was completed successfuly, otherwise
 *         the next required state is returned.
 */
static LwU8
didleStateChange
(
    LwU8 nextState
)
{
    LwU8 retValue      = nextState;
    LwU8 startSubState;
    LwU8 previousState = DIDLE_STATE_INACTIVE;

    switch (nextState)
    {
        case DIDLE_STATE_ARMED_OS:
        case DIDLE_STATE_INACTIVE:
            if (DidleLwrState == DIDLE_STATE_ENTERING_OS)
            {
                //
                // We're re-exiting deepidle in the middle of entering;
                // the substate needs to be backed up to undo the last
                // step we took while entering. For exiting, backing up a step
                // means adding 1 because the exiting states go backward
                // from GC4_SUBSTATE_END to GC4_SUBSTATE_START.
                //
                startSubState = DidleSubstate + 1;
                retValue = gcxGC4Exit_HAL(&Gcx, nextState, startSubState);
                PMU_HALT_COND(retValue == nextState);
                s_gcxDmaResume();
            }
            else if (DidleLwrState == DIDLE_STATE_IN_OS)
            {
                //
                // For exiting deepidle, we start from the last state
                // and work backwards to mirror the entry steps.
                //
                startSubState = GC4_SUBSTATE_END;
                retValue      = gcxGC4Exit_HAL(&Gcx, nextState, startSubState);
                PMU_HALT_COND(retValue == nextState);
                s_gcxDmaResume();
            }
            else
            {
                //
                // We're changing between ARMED_OS and INACTIVE.
                // No action is required beyond changing the state.
                // In case of SSC enabled, state can only change from
                // ACTIVATED_SSC to INACTIVE here.  Again, no action is
                // required beyond changing the state.
                //
                previousState = DidleLwrState;
                DidleLwrState = nextState;
            }

            break;

        case DIDLE_STATE_IN_OS:

            // Enter DIOS
            retValue = s_gcxDiosEnter(nextState);

            //
            // Restore the priority and resume DMA if we didn't start entering
            // DIDLE state.
            //
            if ((retValue != nextState) &&
                (DidleSubstate == GC4_SUBSTATE_START))
            {
                s_gcxDmaResume();
            }
            break;

        default:
            retValue = DidleLwrState;
            DBG_PRINTF_DIDLE(("DI: ERROR: Cannot change to illegal state\n", 0, 0, 0, 0));

            break;
    }
    return retValue;
}

/*!
 * @brief Helper function to put GPU into DI-OS
 *
 * @param[in]     targetState   Desired end state on successful completion
 *                              of the entry steps
 *
 * @return targetState if the transition into DIOS was completed,
 *         otherwise the next desired state of the next request in the queue
 *         is returned.
 */
static LwU8
s_gcxDiosEnter
(
    LwU8 nextState
)
{
    //
    // Suspend DMA if it's not locked. It will prevent code from swapping in
    // during DIOS. Also bump our priority to guarantee that the GCX task gets
    // rescheduled when the scheduler code loops back around after overlay
    // load failure. Suspending DMA and raising priority should be an atomic
    // operation to guarantee that the GCX task will service DMA_EXCEPTION on
    // priority.
    //
    appTaskCriticalEnter();
    {
        PMU_HALT_COND(!Gcx.bDmaSuspended);
        if (!lwosDmaSuspend(LW_FALSE))
        {
            appTaskCriticalExit();
            return DIDLE_STATE_ARMED_OS;
        }

        //
        // Bump priority to MAX_POSSIBLE if MSCG is pre-condition to DIOS
        // otherwise bump priority to DMA_SUSPENDED.
        //
        // ISR redirect all MSCG abort/wake-up interrupts to GCX and PG task.
        // If PG has engaged MSCG then it's already running at DMA_SUSPENDED
        // level. We would like to exit DI-OS before MSCG. Thus, GCX should
        // have higher priority than PG.
        //
        lwrtosLwrrentTaskPrioritySet(PMUCFG_FEATURE_ENABLED(PMU_DIDLE_OS) ?
                                     OSTASK_PRIORITY_MAX_POSSIBLE         :
                                     OSTASK_PRIORITY_DMA_SUSPENDED);

        Gcx.bDmaSuspended = LW_TRUE;

    }
    appTaskCriticalExit();

    return gcxGC4Enter_HAL(&Gcx, nextState);
}

/*!
 * @brief Resume DMA if its suspended by GCX task
 *
 * Helper function to resume DMA and restore priority of GCX task to default.
 */
static void
s_gcxDmaResume(void)
{
    if (Gcx.bDmaSuspended)
    {
        appTaskCriticalEnter();
        {
            lwosDmaResume();
            lwrtosLwrrentTaskPrioritySet(OSTASK_PRIORITY(GCX));

            Gcx.bDmaSuspended = LW_FALSE;
        }
        appTaskCriticalExit();
    }
}

/*!
 * @brief      Initialize the GCX Task.
 *
 * @return     FLCN_OK on success,
 * @return     descriptive error code otherwise.
 */
FLCN_STATUS
gcxPreInitTask(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 queueSize = 0;

    OSTASK_CREATE(status, PMU, GCX);
    if (status != FLCN_OK)
    {
        goto gcxPreInitTask_exit;
    }

    //
    // If SSC/OS is enabled, GCX Task task Queue Size needs to be increased as
    // there could be multiple events(L1 entry/exit, MSCG engaged/disengaged
    // /aborted, Burst Mode On/off, RM commands, Abort events etc) coming to
    // GCX Task conlwrrently.
    //
    queueSize = PMU_GCX_QUEUE_SIZE_GC5;

    status = lwrtosQueueCreateOvlRes(&LWOS_QUEUE(PMU, GCX), queueSize,
                                     sizeof(DISPATCH_DIDLE));
    if (status != FLCN_OK)
    {
        goto gcxPreInitTask_exit;
    }

gcxPreInitTask_exit:
    return status;
}

/*!
 * Defines the entry-point for this task.  This function will execute forever
 * processing each Deepidle unit request that arrives from the main dispatcher,
 * dispatching to the appropriate signal or command handler.
 */
lwrtosTASK_FUNCTION(task_gcx, pvParameters)
{
    LwU8  nextState;
    LwU32 waitTicks;

    DBG_PRINTF_DIDLE(("DI: Alive\n", 0, 0, 0, 0));

    for (;;)
    {
        Gcx.pGc4->bDisarmResponsePending    = LW_FALSE;
        waitTicks                           = lwrtosMAX_DELAY;

        nextState = didleNextStateGet(waitTicks);

        //
        // If a state change is valid, switch to the next state. It is possible
        // that while entering or exiting deepidle, we will need to abort the
        // change in the middle and revert to the previous state. In that case,
        // changeState will return the new desired state. Otherwise,
        // didleChangeState will return the state passed in.
        //
        while ((PMUCFG_FEATURE_ENABLED(PMU_GCX_DIDLE)) &&
               (nextState != DidleLwrState))
        {
            nextState = didleStateChange(nextState);
        }
        if (Gcx.pGc4->bDisarmResponsePending)
        {
            didleMessageDisarmOs(Gcx.pGc4->cmdStatus);
        }
    }
}
