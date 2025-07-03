/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_objdfpr.c
 * @brief  Display Idle Frame Refersh Prefetch Code
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objdfpr.h"
#include "pmu_objlpwr.h"
#include "pmu_objpg.h"
#include "pmu_objfifo.h"
#include "pmu_objms.h"
#include "main.h"
#include "main_init_api.h"
#include "dmemovl.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
OBJDFPR Dfpr;

/* ------------------------- Prototypes --------------------------------------*/
static FLCN_STATUS s_dfprEntry(void)
                   GCC_ATTRIB_NOINLINE();
static FLCN_STATUS s_dfprExit(void)
                   GCC_ATTRIB_NOINLINE();

static FLCN_STATUS s_dfprEntrySeq(LwU8 *pStepId);
static FLCN_STATUS s_dfprExitSeq(LwU8 stepId);

static FLCN_STATUS s_dfprFsmEventSend(void);

static LwBool      s_dfprWakeupPending(void);

static void        s_dfprHoldoffMaskInit(void)
                   GCC_ATTRIB_SECTION("imem_lpwrLoadin", "s_dfprHoldoffMaskInit");
/* ------------------------- Private Variables ------------------------------ */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Construct the dfpr object.
 *
 * @return  FLCN_OK     On success
 */
FLCN_STATUS
constructDfpr(void)
{
   // Add actions here to be performed prior to lpwr task is scheduled
   return FLCN_OK;
}

/*!
 * @brief dfpr initialization post init
 *
 * Initialization of dfpr related structures immediately after scheduling
 * the lpwr task in scheduler.
 */
FLCN_STATUS
dfprPostInit(void)
{
    FLCN_STATUS status = FLCN_OK;

    // All members are by default initialize to Zero.
    memset(&Dfpr, 0, sizeof(Dfpr));

    return status;
}

/*!
 * @brief Initialize MSCG parameters
 */
void
dfprInit(void)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_DFPR);

    // Initialize PG interfaces for MS
    pPgState->pgIf.lpwrEntry = s_dfprEntry;
    pPgState->pgIf.lpwrExit  = s_dfprExit;

    // Initialize the Holdoff Mask
    s_dfprHoldoffMaskInit();

    // Initialize the Idle Mask
    dfprIdleMaskInit_HAL(&Dfpr);

    // Override entry conditions by updating entry equation of MSCG.
    dfprEntryConditionConfig_HAL(&Dfpr);

    // Enable holdoff interrupts
    pgEnableHoldoffIntr_HAL(&Pg, pPgState->holdoffMask, LW_TRUE);

    // Initialize the L2 Cache setting in SW.
    dfprLtcInit_HAL(&Dfpr);
}

/*!
 * @brief Check for various idleness conditions
 *
 * @param[in]      ctrlId - LPWR_CTRL Id
 * @param[in/out] pAbortReason pointer to rerurn the abort reason
 *
 * return LW_TRUE  -> Idleness is still there
 *        LW_FALSE -> Idleness is flipped
 */
LwBool
dfprIsIdle
(
    LwU8   ctrlId,
    LwU16 *pAbortReason
)
{
    LwBool bIsIdle = LW_FALSE;

    // Abort if any wakeup event is pending
    if (s_dfprWakeupPending())
    {
        *pAbortReason = DFPR_ABORT_REASON_WAKEUP_PENDING;
    }
    // Abort if idle status is flipped
    else if (pgCtrlIdleFlipped_HAL(&Pg, ctrlId))
    {
        *pAbortReason = DFPR_ABORT_REASON_IDLE_FLIPPED;
    }
    else
    {
        bIsIdle = LW_TRUE;
    }

    return bIsIdle;
}

/*!
 * @brief Handle the DIFR Prefetch Response From RM
 *
 * @return      FLCN_OK    On success
 *              FLCN_ERROR otherwise
 */
FLCN_STATUS
dfprPrefetchResponseHandler
(
    LwBool bIsPrefetchResponseSuccess
)
{
    //
    // We must never receive any response if Prefetch is not pending.
    // Its a bug in SW and needs to be fixed
    //
    if (!Dfpr.bIsPrefetchResponsePending)
    {
        PMU_BREAKPOINT();
    }

    // Received the prefetch Response, so clear the variable
    Dfpr.bIsPrefetchResponsePending = LW_FALSE;

    // If DFPR FSM was disabled using SW Client, re enable it.
    if (Dfpr.bSelfDisallow)
    {
        pgCtrlHwAllow(RM_PMU_LPWR_CTRL_ID_DFPR, PG_CTRL_ALLOW_REASON_MASK(SELF));
        Dfpr.bSelfDisallow = LW_FALSE;
    }

    //
    // If DFPR is in PWR_OFF Mode
    //  AND
    // Prefetch is successfull, then:
    //
    // Trigger the arbiter to allow DIFR Layer2 and Layer 3
    // Entry
    //
    if (PG_IS_ENGAGED(RM_PMU_LPWR_CTRL_ID_DFPR) &&
        bIsPrefetchResponseSuccess)
    {
        // Call the arbiter to Allow DIFR_SW_ASR to engage
        lpwrGrpCtrlMutualExclusion(RM_PMU_LPWR_GRP_CTRL_ID_MS,
                                   LPWR_GRP_CTRL_CLIENT_ID_DFPR, 0x0);
    }
    else
    {
        //  Queue the wakeup event
        pgCtrlEventSend(RM_PMU_LPWR_CTRL_ID_DFPR, PMU_PG_EVENT_WAKEUP,
                        PG_WAKEUP_EVENT_DFPR_PREFETCH);
    }

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Initializes the Holdoff mask for dfpr PREFETCH FSM
 */
void
s_dfprHoldoffMaskInit(void)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_DFPR);
    LwU32       holdoffMask       = 0;

    if (FIFO_IS_ENGINE_PRESENT(GR))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_GR));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE0) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE0))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE0));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE1) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE1))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE1));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE2) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE2))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE2));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE3) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE3))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE3));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE4) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE4))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE4));
    }
    //  LWDEC = BSP  (reusing MSVLD code for LWDEC)
    if (FIFO_IS_ENGINE_PRESENT(BSP))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_BSP));
    }
    if (FIFO_IS_ENGINE_PRESENT(DEC1))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_DEC1));
    }
    if (FIFO_IS_ENGINE_PRESENT(DEC2))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_DEC2));
    }
#if PMUCFG_FEATURE_ENABLED(PMU_HOST_ENGINE_EXPANSION)
    if (FIFO_IS_ENGINE_PRESENT(DEC3))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_DEC3));
    }
#endif
    if (FIFO_IS_ENGINE_PRESENT(ENC0))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_ENC0));
    }
    if (FIFO_IS_ENGINE_PRESENT(ENC1))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_ENC1));
    }
    if (FIFO_IS_ENGINE_PRESENT(ENC2))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_ENC2));
    }
#if PMUCFG_FEATURE_ENABLED(PMU_HOST_ENGINE_EXPANSION)
    if (FIFO_IS_ENGINE_PRESENT(OFA0))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_OFA0));
    }
    if (FIFO_IS_ENGINE_PRESENT(JPG0))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_JPG0));
    }
#endif
    pPgState->holdoffMask = holdoffMask;
}

/*!
 * @brief Context save interface for LPWR_ENG(dfpr_SW_ASR)
 *
 * Entry Sequence steps:
 * Assumption: MSCG is already in Disabled State
 *
 * 1. Engage the Method holdoff - GR/GR-CE/Video
 * 2. L2 Cache Entry Sequence
 * 3. Send the Prefetch request to LPWR_LP Task.
 *
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
 */
static FLCN_STATUS
s_dfprEntry(void)
{
    LwU8        stepId = DFPR_SEQ_STEP_ID__START;
    FLCN_STATUS status = FLCN_OK;

    // Start DFPR Entry Sequence
    status = s_dfprEntrySeq(&stepId);
    if ((status != FLCN_OK))
    {
        // Abort the DFPR Sequence
        s_dfprExitSeq(stepId);
    }
    return status;
}

/*!
 * @brief Entry Sequence for DIFR SW_ASR
 *
 * @param[out]  pStepId Send back the last execture step of MSCG Entry
 *
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
 */
FLCN_STATUS
s_dfprEntrySeq
(
    LwU8    *pStepId
)
{
    FLCN_STATUS    status   = FLCN_OK;
    LwU8           ctrlId   = RM_PMU_LPWR_CTRL_ID_DFPR;
    OBJPGSTATE    *pPgState = GET_OBJPGSTATE(ctrlId);
    LwU16          abortReason;
    LwBool         bIdleCheck;

    // Reset the Demote Flag on entry
    Dfpr.bDemoteOnExit = LW_FALSE;

    //
    // execute the entry sequence untill
    // 1. Sequence is completed or
    // 2. We have an abort
    //
    while (*pStepId <= DFPR_SEQ_STEP_ID__END)
    {
        //
        // Each step which needs idle check must set this
        // variable
        //
        bIdleCheck = LW_FALSE;

        switch (*pStepId)
        {
            case DFPR_SEQ_STEP_ID_HOLDOFF:
            {
                status = lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_DFPR,
                                                pPgState->holdoffMask);
                if (status != FLCN_OK)
                {
                    abortReason = DFPR_ABORT_REASON_HOLDOFF_TIMEOUT;
                    break;
                }

                bIdleCheck = LW_TRUE;
                break;
            }

            case DFPR_SEQ_STEP_ID_L2_CACHE_DEMOTE:
            {
                // Demote the L2 Cache
                if (LPWR_CTRL_IS_SF_ENABLED(pPgState, DFPR, L2_CACHE_DEMOTE))
                {
                    status = dfprL2CacheDemote_HAL(&Dfpr, &abortReason);
                }

                bIdleCheck = LW_TRUE;
                break;
            }

            case DFPR_SEQ_STEP_ID_L2_CACHE_SETTING:
            {
                // Perform the L2 Cache Settings for Prefetch
                if (LPWR_CTRL_IS_SF_ENABLED(pPgState, DFPR, L2_CACHE_SETTING))
                {
                    status = dfprLtcEntrySeq_HAL(&Dfpr, ctrlId);
                }

                bIdleCheck = LW_TRUE;
                break;
            }

            case DFPR_SEQ_STEP_ID_PREFETCH:
            {
                // Send the event to LPWR_LP Task for Prefetch
                if (LPWR_CTRL_IS_SF_ENABLED(pPgState, DFPR, PREFETCH))
                {
                    status = s_dfprFsmEventSend();
                    if (status == FLCN_OK)
                    {
                        //
                        // Since Prfetch request has been sent, Therefor on FSM Exit we need
                        // to issue the L2 Cache Demote operation
                        //
                        Dfpr.bDemoteOnExit = LW_TRUE;

                        //
                        // Update the flag that Prefetch response is pending.
                        // Prefetch request will be sent by LPWR_LP Task once it receives this
                        // request.
                        //
                        // Reason for setting the flag here:
                        // LPWR task sends async/posted prefetch request to KMD/LwKMS. DFPR FSM
                        // goes in PWR_OFF state once request is posted/queued. Now in PWR-OFF
                        // state, there is possibility that DFPR FSM receives wake-up request
                        // (say from holdoff) before LPWR task receives prefetch response from
                        // KMD/LwKMS. In such case, we would like to keep DFRP FSM disabled until
                        // LPWR task receives the prefetch response. Hence we set the flag here and
                        // keep FSM in self disallow state until the flag is cleared
                        //
                        Dfpr.bIsPrefetchResponsePending = LW_TRUE;
                    }
                }

                break;
            }

            case DFPF_SEQ_STEP_ID_MS_GROUP_MUTUAL_EXCLUSION:
            {
                //
                // Trigger the Sequencer to enable the DIFR Layer (SW_ASR) and
                // disable the MSCG.
                //
                // However, we have to wait for Prefetch response, hence we can not actually
                // trigger the sequencer here. We will do it as a part of Prefetch
                // response handler.
                //

                break;
            }
        }
                // Check if step returned error or Memory subsystem is idle or not
        if ((status != FLCN_OK) ||
            (bIdleCheck         &&
             !dfprIsIdle(ctrlId, &abortReason)))
        {
            // Set the Abort Reason and StepId
            pPgState->stats.abortReason = (*pStepId << 16) | abortReason;

            // Set the status explicity
            status = FLCN_ERROR;

            // Exit out of while loop
            break;
        }

        // Move to next step
        *pStepId = *pStepId + 1;
    }

    return status;
}

/*!
 * @brief Context restore interface for LPWR_ENG(dfpr_SW_ASR)
 *
 * Exit Sequence steps:
 *
 * Assumption: MSCG is already in Disabled State
 *
 * 1. Restore the L2 Cache Settings
 * 2. Demote the L2 Cache
 * 3. Disengage the Holdoff
 *
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
 */
static FLCN_STATUS
s_dfprExit(void)
{
    return s_dfprExitSeq(DFPR_SEQ_STEP_ID__END);
}

/*!
 * @brief Exit Sequence for DFPR
 *
 * @param[in]  stepId  Start of DFPR Exit Sequence
 *
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
 */
FLCN_STATUS
s_dfprExitSeq
(
    LwU8 stepId
)
{
    LwU8           ctrlId   = RM_PMU_LPWR_CTRL_ID_DFPR;
    OBJPGSTATE    *pPgState = GET_OBJPGSTATE(ctrlId);

    while (stepId >= DFPR_SEQ_STEP_ID__START)
    {
        switch (stepId)
        {
            case DFPF_SEQ_STEP_ID_MS_GROUP_MUTUAL_EXCLUSION:
            {
                //
                // Trigger the Sequencer to disable the DIFR Layer (SW_ASR) and
                // enable the MSCG
                //
                // At this point DIFR_SW_ASR FSM will be PWR_ON Mode because of
                // Child-parent dependencies
                //
                lpwrGrpCtrlMutualExclusion(RM_PMU_LPWR_GRP_CTRL_ID_MS,
                                           LPWR_GRP_CTRL_CLIENT_ID_DFPR,
                                           MS_DIFR_FSM_MASK);
                break;
            }

            case DFPR_SEQ_STEP_ID_PREFETCH:
            {
                //
                // If Prefetch request has been sent and we have not received
                // the response, then we need to lock the FSM in Self disallow
                // mode using the SW Client.
                if (Dfpr.bIsPrefetchResponsePending)
                {
                    pgCtrlHwDisallow(ctrlId, PG_CTRL_DISALLOW_REASON_MASK(SELF));
                    Dfpr.bSelfDisallow = LW_TRUE;
                }
                break;

            }

            case DFPR_SEQ_STEP_ID_L2_CACHE_SETTING:
            {
                if (LPWR_CTRL_IS_SF_ENABLED(pPgState, DFPR, L2_CACHE_SETTING))
                {
                    dfprLtcExitSeq_HAL(&Dfpr, ctrlId);
                }
                break;
            }

            case DFPR_SEQ_STEP_ID_L2_CACHE_DEMOTE:
            {
                // Demote the L2 Cache
                if ((LPWR_CTRL_IS_SF_ENABLED(pPgState, DFPR, L2_CACHE_DEMOTE)) &&
                    (Dfpr.bDemoteOnExit))
                {
                    dfprL2CacheDemote_HAL(&Dfpr, NULL);
                }
                break;
            }

            case DFPR_SEQ_STEP_ID_HOLDOFF:
            {
                lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_DFPR,
                                       LPWR_HOLDOFF_MASK_NONE);
                break;
            }
        }

        // Move to next step
        stepId--;
    }

    return FLCN_OK;
}

/*!
 * @brief Check if a message is pending in the LWOS_QUEUE(PMU, LPWR)
 */
LwBool
s_dfprWakeupPending(void)
{
    FLCN_STATUS   status;
    LwU32         numItems;

    status = lwrtosQueueGetLwrrentNumItems(LWOS_QUEUE(PMU, LPWR), &numItems);

    if (status != FLCN_OK)
    {
        PMU_HALT();
    }

    return numItems > 0;
}

/*!
 * @brief Helper function to send the DIFR Prefetch Request
 * events to LPWR_LP task
 *
 * @param[in] eventId   FSM Event ID i.e PREFTCH REQUEST
 *                      (Defined as LPWR_LP_EVENT_ID_DFPR_PREFETCH_REQUEST)
 *
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
*/
static FLCN_STATUS
s_dfprFsmEventSend(void)
{
    FLCN_STATUS      status      = FLCN_OK;
    DISPATCH_LPWR_LP disp2LpwrLp = {{ 0 }};

    // Fill up the structure to send event to LPWR_LP Task
    disp2LpwrLp.hdr.eventType = LPWR_LP_EVENT_DIFR_PREFETCH_REQUEST;

    // Send the event to LPWR_LP task
    status = lwrtosQueueIdSendBlocking(LWOS_QUEUE_ID(PMU, LPWR_LP),
                                       &disp2LpwrLp, sizeof(disp2LpwrLp));
    return status;
}
