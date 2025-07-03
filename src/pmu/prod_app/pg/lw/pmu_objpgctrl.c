/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_objpgctrl.c
 * @brief  PG Control object file
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"

/* ------------------------ Application includes --------------------------- */
#include "main.h"
#include "pmu_objhal.h"
#include "pmu_objlpwr.h"
#include "pmu_objgr.h"
#include "pmu_objms.h"
#include "pmu_objdi.h"
#include "pmu_objei.h"
#include "pmu_objdfpr.h"
#include "pmu_objlpwr.h"
#include "dbgprintf.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Prototypes ------------------------------------- */
static FLCN_STATUS s_pgCtrlConfigIdleSuppCntrIdx(LwU32 ctrlId)
       GCC_ATTRIB_SECTION("imem_lpwrLoadin", "s_pgCtrlConfigIdleSuppCntrIdx");

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions  ------------------------------ */

/*!
 * @brief Initialize PgState object (AKA PgCtrl object)
 *
 * Construct and intialize PG state object per powergatable by wiring up
 * appropriate powergatable interfaces. We construct PgCtrl only if its
 * supported on current chip.
 *
 * @param[in] ctrlId    Controller ID
 *
 * @return      FLCN_OK                     On success
 * @return      FLCN_ERR_ILWALID_STATE      Duplicate initialization request
 * @return      FLCN_ERR_NO_FREE_MEM        No free space in DMEM or in DMEM overlay
 * @return      FLCN_ERR_NOT_SUPPORTED      HW PG_ENG not supported
 */
FLCN_STATUS
pgCtrlInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_PG_CTRL_INIT *pParams
)
{
    LwU32       ctrlId    = pParams->ctrlId;
    OBJPGSTATE *pPgState  = GET_OBJPGSTATE(ctrlId);
    FLCN_STATUS status    = FLCN_OK;
    LwU32       idx;

    //
    // Skip the initialization if PgCtrl is already supported. This might
    // be duplicate call.
    //
    if (pgIsCtrlSupported(ctrlId))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;

        goto pgCtrlInit_exit;
    }

    //
    // Allocate space for PgCtrl. Return if there is no free space.
    // Avoid the duplicate allocations.
    //
    if (pPgState == NULL)
    {
        pPgState = lwosCallocResidentType(1, OBJPGSTATE);
        if (pPgState == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NO_FREE_MEM;

            goto pgCtrlInit_exit;
        }

        Pg.pPgStates[ctrlId] = pPgState;
    }

    pPgState->id                 = ctrlId;
    pPgState->supportMask        = pParams->supportMask;
    pPgState->state              = PMU_PG_STATE_DISALLOW;
    pPgState->disallowReasonMask = PG_CTRL_ALLOW_DISALLOW_REASON_MASK_RM;
    pPgState->bRmDisallowed      = LW_TRUE;

    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_WITH_PS35) &&
        !Pg.bNoPstateVbios)
    {
        pPgState->disallowReasonMask     |= PG_CTRL_ALLOW_DISALLOW_REASON_MASK_PERF;
        pPgState->disallowReentrancyMask |= PG_CTRL_ALLOW_DISALLOW_REASON_MASK_PERF;
    }

    // Keep all group memeber by default disabled
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MUTUAL_EXCLUSION)        &&
        (lpwrGrpIsLpwrCtrlMember(RM_PMU_LPWR_GRP_CTRL_ID_GR, ctrlId) ||
         lpwrGrpIsLpwrCtrlMember(RM_PMU_LPWR_GRP_CTRL_ID_MS, ctrlId)))
    {
        pPgState->disallowReasonMask     |= PG_CTRL_ALLOW_DISALLOW_REASON_MASK_LPWR_GRP;
        pPgState->disallowReentrancyMask |= PG_CTRL_ALLOW_DISALLOW_REASON_MASK_LPWR_GRP;
    }

    PG_SET_PENDING_STATE(pPgState, DISALLOW);

    //
    // SFM 2.0: Update the cache for client enabledMask.
    //
    // For RM client we need to initialize the cache with 0 i.e. keep
    // all the subfeature disabled at boot. This cache will be updated
    // once we will have init command from the RM side. Before that
    // we need to make sure that all subfeature are disabled otherwise, PMU
    // may start engaging the feature even though RM init command is yet
    // to come.
    //
    // For other clients will initialize the cache with all the subfeature
    // enabled by default.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PG_SFM_2))
    {
        for (idx = 0; idx < LPWR_CTRL_SFM_CLIENT_ID__COUNT; idx++)
        {
            pPgState->enabledMaskClient[idx] = RM_PMU_LPWR_CTRL_SFM_VAL_DEFAULT;
        }

        // Special setting for RM Client as updated in the above comments.
        pPgState->enabledMaskClient[LPWR_CTRL_SFM_CLIENT_ID_RM] = RM_PMU_LPWR_CTRL_SFM_VAL_RM_INIT;
    }

    // Init the default values for Min/Max Idle thresholds
    if (PMUCFG_FEATURE_ENABLED(PMU_PG_THRESHOLD_UPDATE))
    {
        pgCtrlMinMaxIdleThresholdInit_HAL(&Pg, ctrlId);
    }

    // Controller specific initialization
    if ((PMUCFG_FEATURE_ENABLED(PMU_PG_GR)) &&
        (ctrlId == RM_PMU_LPWR_CTRL_ID_GR_PG))
    {
        pPgState->hwEngIdx = PG_ENG_IDX_ENG_GR;
        Pg.hwEngIdxMap[PG_ENG_IDX_ENG_GR] = ctrlId;

        grInit(pParams);
    }
    else if ((PMUCFG_FEATURE_ENABLED(PMU_PG_GR_RG)) &&
             (ctrlId == RM_PMU_LPWR_CTRL_ID_GR_RG))
    {
        pPgState->hwEngIdx = PG_ENG_IDX_ENG_GR_RG;
        Pg.hwEngIdxMap[PG_ENG_IDX_ENG_GR_RG] = ctrlId;

        // Update the Auto-wakeup interval for GPC-RG from VBIOS
        pPgState->autoWakeupIntervalMs = Lpwr.pVbiosData->grRg.autoWakeupIntervalMs;

        grRgInit(pParams);
    }
    else if ((PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_GR_PASSIVE)) &&
             (ctrlId == RM_PMU_LPWR_CTRL_ID_GR_PASSIVE))
    {
        pPgState->hwEngIdx = PG_ENG_IDX_ENG_GR_PASSIVE;
        Pg.hwEngIdxMap[PG_ENG_IDX_ENG_GR_PASSIVE] = ctrlId;

        grPassiveInit();
    }
    else if ((PMUCFG_FEATURE_ENABLED(PMU_PG_MS)) &&
             (ctrlId == RM_PMU_LPWR_CTRL_ID_MS_MSCG))
    {
        pPgState->hwEngIdx = PG_ENG_IDX_ENG_MS;
        Pg.hwEngIdxMap[PG_ENG_IDX_ENG_MS] = ctrlId;

        //
        // If Pstate3.5 is enabled, we need to set specific init value for
        // mclk client. Details are present in pmuifpg.h file. We need this
        // special value only for MSCG
        //
        if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_WITH_PS35))
        {
            pPgState->enabledMaskClient[LPWR_CTRL_SFM_CLIENT_ID_MCLK] =
                                        RM_PMU_LPWR_CTRL_MS_SFM_VAL_MCLK_INIT;
        }

        msMscgInit(pParams);
    }
    else if ((PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_MS_LTC)) &&
             (ctrlId == RM_PMU_LPWR_CTRL_ID_MS_LTC))
    {
        pPgState->hwEngIdx = PG_ENG_IDX_ENG_MS_LTC;
        Pg.hwEngIdxMap[PG_ENG_IDX_ENG_MS_LTC] = ctrlId;

        msLtcInit(pParams);
    }
    else if ((PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_MS_PASSIVE)) &&
             (ctrlId == RM_PMU_LPWR_CTRL_ID_MS_PASSIVE))
    {
        pPgState->hwEngIdx = PG_ENG_IDX_ENG_MS_PASSIVE;
        Pg.hwEngIdxMap[PG_ENG_IDX_ENG_MS_PASSIVE] = ctrlId;

        msPassiveInit();
    }
    else if ((PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_EI)) &&
             (ctrlId == RM_PMU_LPWR_CTRL_ID_EI))
    {
        pPgState->hwEngIdx = PG_ENG_IDX_ENG_EI;
        Pg.hwEngIdxMap[PG_ENG_IDX_ENG_EI] = ctrlId;

        //
        // Enable SELF_DISALLOW feature for EI
        //
        // EI exit is managed by LPWR and LPWR_LP tasks. Here is high level
        // sequence -
        //
        // LPWR task:
        // Collects wake-up events, triggers HW FSM exit and notifies LPWR_LP
        // task to complete rest of the exit. In LPWR task HW FSM completes
        // the  exit. LPWR_ENG(EI) should not start another entry until
        // LPWR_LP completes its work thus self-disallow LPWR_ENG(EI).
        //
        // LPWR_LP task :
        // Task to complete rest of the exit and re-enables EI FSM.
        //
        pPgState->bSelfDisallow = LW_TRUE;

        eiInit(pParams);
    }
    else if ((PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_EI_PASSIVE)) &&
             (ctrlId == RM_PMU_LPWR_CTRL_ID_EI_PASSIVE))
    {
        pPgState->hwEngIdx = PG_ENG_IDX_ENG_EI_PASSIVE;
        Pg.hwEngIdxMap[PG_ENG_IDX_ENG_EI_PASSIVE] = ctrlId;

        //
        // EI Passive is Pstate agnostic features, hence we prevent disabling with
		// reason perf by clearing the default condition.
		//
        if (pPgState->disallowReasonMask & PG_CTRL_ALLOW_DISALLOW_REASON_MASK_PERF)
        {
            pPgState->disallowReasonMask &= ~PG_CTRL_ALLOW_DISALLOW_REASON_MASK_PERF;
		}
        if (pPgState->disallowReentrancyMask & PG_CTRL_ALLOW_DISALLOW_REASON_MASK_PERF)
		{
            pPgState->disallowReentrancyMask &= ~PG_CTRL_ALLOW_DISALLOW_REASON_MASK_PERF;
        }

        eiPassiveInit();
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_DFPR) &&
            (ctrlId == RM_PMU_LPWR_CTRL_ID_DFPR))
    {
        pPgState->hwEngIdx = PG_ENG_IDX_ENG_DIFR_PREFETCH;
        Pg.hwEngIdxMap[PG_ENG_IDX_ENG_DIFR_PREFETCH] = ctrlId;

        // Initialize the FSM Settings
        dfprInit();
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_MS_DIFR_SW_ASR) &&
            (ctrlId == RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR))
    {
        pPgState->hwEngIdx = PG_ENG_IDX_ENG_DIFR_SW_ASR;
        Pg.hwEngIdxMap[PG_ENG_IDX_ENG_DIFR_SW_ASR] = ctrlId;

        //
        // If Pstate3.5 is enabled, we need to set specific init value for
        // mclk client. Details are present in pmuifpg.h file. We need this
        // special value only for feature which has to do SW_ASR Sequence
        //
        if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_WITH_PS35))
        {
            pPgState->enabledMaskClient[LPWR_CTRL_SFM_CLIENT_ID_MCLK] =
                                        RM_PMU_LPWR_CTRL_MS_SFM_VAL_MCLK_INIT;
        }

        // Initialize FSM settings
        msDifrSwAsrInit();
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_MS_DIFR_CG) &&
            (ctrlId == RM_PMU_LPWR_CTRL_ID_MS_DIFR_CG))
    {
        pPgState->hwEngIdx = PG_ENG_IDX_ENG_DIFR_CG;
        Pg.hwEngIdxMap[PG_ENG_IDX_ENG_DIFR_CG] = ctrlId;

        // Initialize FSM settings
        msDifrCgInit();
    }
    else
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NOT_SUPPORTED;

        goto pgCtrlInit_exit;
    }

    // Initialize the HW FSM
    pgCtrlInit_HAL(&Pg, ctrlId);
    pgCtrlInterruptsInit_HAL(&Pg, ctrlId);

    status = s_pgCtrlConfigIdleSuppCntrIdx(ctrlId);
    if (status != FLCN_OK)
    {
        goto pgCtrlInit_exit;
    }

    pgCtrlIdleMaskSet_HAL(&Pg, ctrlId, pPgState->idleMask);
    pgCtrlConfigCntrMode_HAL(&Pg, ctrlId, PG_SUPP_IDLE_COUNTER_MODE_AUTO_IDLE);

    // Configure Auto-wakeup functionality
    pgCtrlAutoWakeupConfig_HAL(&Pg, ctrlId);

    // Update support mask corresponding to PG Ctrl
    Pg.supportedMask |= BIT(ctrlId);

    // Update the Stats cache with SW disable reason mask
    pPgState->stats.swDisallowReasonMask = pPgState->disallowReasonMask;

pgCtrlInit_exit:

    return status;
}

/*!
 * @brief Send FSM events to LPWR task
 *
 * @param[in]  ctrlId     PG Ctrl ID - RM_PMU_LPWR_CTRL_ID_xyz
 * @param[in]  eventId    LPWR FSM event - PMU_PG_EVENT_xyz
 * @param[in]  data       Additional data for the event
 *
 * @return      FLCN_OK     if API is successfully queued the event
 */
FLCN_STATUS
pgCtrlEventSend
(
    LwU32 ctrlId,
    LwU32 eventId,
    LwU32 data
)
{
    DISPATCH_LPWR disp2Lpwr = {{ 0 }};
    FLCN_STATUS   status;

    disp2Lpwr.hdr.eventType    = LPWR_EVENT_ID_PMU;
    disp2Lpwr.fsmEvent.ctrlId  = (LwU8)ctrlId;
    disp2Lpwr.fsmEvent.eventId = (LwU8)eventId;
    disp2Lpwr.fsmEvent.data    = data;

    //
    // Checking for free slot in PG queue and using that slot should be an
    // atomic operation. Use critical section to ensure the atomicity.
    //
    appTaskCriticalEnter();
    {
        status = lwrtosQueueIdSendFromAtomic(LWOS_QUEUE_ID(PMU, LPWR),
                                             &disp2Lpwr,
                                             sizeof(disp2Lpwr));
    }
    appTaskCriticalExit();

    PMU_HALT_COND(status == FLCN_OK);

    return status;
}

/*!
 * @brief Colwert or queue FSM event
 *
 * @param[in/out]  pPgLogicState  Pointer to PG Logic state
 * @param[in]      ctrlId         PG Ctrl ID - RM_PMU_LPWR_CTRL_ID_xyz
 * @param[in]      eventId        LPWR FSM event - PMU_PG_EVENT_xyz
 * @param[in]      data           Additional data for the event
 *
 * @return      FLCN_OK     if API is colwerted or send the event
 */
FLCN_STATUS
pgCtrlEventColwertOrSend
(
    PG_LOGIC_STATE *pPgLogicState,
    LwU32           ctrlId,
    LwU32           eventId,
    LwU32           data
)
{
    FLCN_STATUS status = FLCN_OK;

    //
    // Inline colwersion is possible when PG logic state not maintaining
    // a valid event
    //
    if (pPgLogicState->eventId == PMU_PG_EVENT_NONE)
    {
        pPgLogicState->ctrlId  = ctrlId;
        pPgLogicState->eventId = eventId;
        pPgLogicState->data    = data;
    }
    else
    {
        // Queue the event if PgLogicState has some valid event
        status = pgCtrlEventSend(ctrlId, eventId, data);
    }

    return status;
}

/*!
 * @brief Disable/Disallow PgCtrl to avoid task thrashing
 *
 * This function implements LPWR thrashing prevention policy. We ilwoke this
 * policy when we detect the LPWR task thrashing or task starvation. This
 * function disables PgCtrl if non-LPWR tasks has some pending workload.
 *
 * @param[in]   ctrlId  Controller ID
 */
void
pgCtrlThrashingDisallow
(
    LwU32 ctrlId
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);

    // Return early if PgCtrl is already disallowed because of task thrashing
    if (pPgState->taskMgmt.bDisallow)
    {
        return;
    }

    // Disable/disallow PgCtrl only when non-LPWR tasks has some pending workload.
    if (!lpwrIsNonLpwrTaskIdle())
    {
        // Update thrashing detection count
        pPgState->stats.detectionCount++;

        pgCtrlDisallow(ctrlId, PG_CTRL_DISALLOW_REASON_MASK(THRASHING));
        pPgState->taskMgmt.bDisallow = LW_TRUE;

        // Enable idle notification only if its disabled
        if (Pg.rtosIdleState == PG_RTOS_IDLE_EVENT_STATE_DISABLED)
        {
            Pg.rtosIdleState = PG_RTOS_IDLE_EVENT_STATE_ENABLED;
        }
    }
}

/*!
 * @brief Update Idle and PPU thresholds
 *
 * @param[in]   ctrlId                 PG controller ID
 * @param[in]   pReqThresholdCycles    Pointer to PG_CTRL_THRESHOLD
 */
void
pgCtrlThresholdsUpdate
(
    LwU32               ctrlId,
    PG_CTRL_THRESHOLD  *pReqThresholdCycles
)
{
    OBJPGSTATE        *pPgState            = GET_OBJPGSTATE(ctrlId);
    LwBool             bUpdate             = LW_FALSE;

    // Check if requested thresholds are same as current thresholds
    if ((pReqThresholdCycles->idle > 0) &&
        (pReqThresholdCycles->idle != pPgState->thresholdCycles.idle))
    {
        //
        // check if requested idle threshold are under min/max limit.
        // If this are not in limit, then cap the values to min/max
        // limit.
        //
        if (LPWR_ARCH_IS_SF_SUPPORTED(IDLE_THRESHOLD_CHECK))
        {
            if (pReqThresholdCycles->idle < pPgState->thresholdCycles.minIdle)
            {
                pReqThresholdCycles->idle = pPgState->thresholdCycles.minIdle;
            }
            else if (pReqThresholdCycles->idle > pPgState->thresholdCycles.maxIdle)
            {
                pReqThresholdCycles->idle = pPgState->thresholdCycles.maxIdle;
            }
        }

        pPgState->thresholdCycles.idle = pReqThresholdCycles->idle;
        bUpdate = LW_TRUE;
    }

    if ((pReqThresholdCycles->ppu > 0) &&
        (pReqThresholdCycles->ppu != pPgState->thresholdCycles.ppu))
    {
        pPgState->thresholdCycles.ppu = pReqThresholdCycles->ppu;
        bUpdate = LW_TRUE;
    }

    if (bUpdate)
    {
        if (pgCtrlDisallow(ctrlId, PG_CTRL_DISALLOW_REASON_MASK(THRESHOLD)))
        {
            pgCtrlIdleThresholdsSet_HAL(&Pg, ctrlId);
            pgCtrlAllow(ctrlId, PG_CTRL_ALLOW_REASON_MASK(THRESHOLD));
        }
    }
}

/*!
 * @brief Disallow PgCtrl
 *
 * This API is used to disable/disallow PgCtrl. API is designed to optimize the
 * LPWR task queue size. It will queue the DIALLOW event only when there is no
 * DISALLOW request in flight.
 *
 * @param[in]   ctrlId      LPWR controller ID
 * @param[in]   reasonMask  Disallow reason mask
 *
 * @return      LW_TRUE     Disallow processing completed and PgCtrl is in
 *                          DISALLOWed state
 *              LW_FALSE    Otherwise
 */
LwBool
pgCtrlDisallow
(
    LwU32 ctrlId,
    LwU32 reasonMask
)
{
    OBJPGSTATE *pPgState    = GET_OBJPGSTATE(ctrlId);
    LwU32       disallowReasonMask;
    LwBool      bDisallowed = LW_FALSE;

    appTaskCriticalEnter();

    // Re-entrancy check
    if ((pPgState->disallowReentrancyMask & reasonMask) != 0)
    {
        // Return LW_TRUE if previous disallow request has been completed
        if ((pPgState->state == PMU_PG_STATE_DISALLOW) &&
            ((pPgState->disallowReasonMask & reasonMask) != 0))
        {
            bDisallowed = LW_TRUE;
        }

        goto pgCtrlDisallow_exit;
    }

    disallowReasonMask = pPgState->disallowReasonMask & (~reasonMask);

    //
    // Apply optimization if -
    //
    // 1) API optimization is supported                                 AND
    // 2) There are other clients which has disabled PgCtrl             AND
    // 3) Previous ALLOW request (if any) for this client has been
    //    processed by FSM
    //
    // i.e Queue the DISALLOW event if this is the first client OR
    //     Last allow havnt completed
    //
    if ((LPWR_ARCH_IS_SF_SUPPORTED(API_OPTIMIZATION)) &&
        (disallowReasonMask != 0)                     &&
        ((pPgState->disallowReasonMask & reasonMask) == 0))
    {
        //
        // PgCtrl is already in DISALLOW state. Means DISALLOW request is
        // completed. Just update disallowReasonMask.
        //
        if (pPgState->state == PMU_PG_STATE_DISALLOW)
        {
            pPgState->disallowReasonMask |= reasonMask;
            bDisallowed = LW_TRUE;
        }
        //
        // DISALLOW request is process. Mark DISALLOW_ACK pending for current
        // DISALLOW event. FSM will send ACK for this event.
        //
        else
        {
            PMU_HALT_COND(pPgState->disallowAckPendingMask);

            pPgState->disallowReasonMask     |= reasonMask;
            pPgState->disallowAckPendingMask |= reasonMask;
        }
    }
    //
    // FSM havnt started processing of any DISALLOW request. It likely possible
    // that this is first DISALLOW request. Queue an event to initiate new
    // DISALLOW request.
    //
    else
    {
        pgCtrlEventSend(ctrlId, PMU_PG_EVENT_DISALLOW, reasonMask);
    }

    // Update re-entrancy mask to avoid duplicate DISALLOW events.
    pPgState->disallowReentrancyMask |= reasonMask;

    // Update the Stats cache with SW disable reason mask
    pPgState->stats.swDisallowReasonMask = pPgState->disallowReasonMask;

pgCtrlDisallow_exit:

    appTaskCriticalExit();

    return bDisallowed;
}

/*!
 * @brief Allow PgCtrl
 *
 * This is the counter part of pgCtrlDisallow(). It is used to enable/allow
 * PgCtrl. API is designed to optimize the LPWR task queue size. It will queue
 * the ALLOW event only when it's the last client that has disallowed the
 * PgCtrl.
 *
 * @param[in]   ctrlId      LPWR controller ID
 * @param[in]   reasonMask  Allow reason mask
 */
void
pgCtrlAllow
(
    LwU32 ctrlId,
    LwU32 reasonMask
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);
    LwU32       disallowReasonMask;

    appTaskCriticalEnter();

    // Return early if its duplicate request
    if ((pPgState->disallowReentrancyMask & reasonMask) == 0)
    {
        goto pgCtrlAllow_exit;
    }

    disallowReasonMask = pPgState->disallowReasonMask & (~reasonMask);

    //
    // Remove the reason from disallow reason mask, if
    //
    // 1) API optimization is supported                                 AND
    // 2) There are other clients which has disabled PgCtrl             AND
    // 3) DISALLOW request for this client has been processed by FSM
    //
    // i.e Queue the ALLOW event if this is the last client OR
    //     Last disallow havnt completed
    //
    if ((LPWR_ARCH_IS_SF_SUPPORTED(API_OPTIMIZATION))      &&
        (disallowReasonMask != 0)                          &&
        ((pPgState->disallowReasonMask & reasonMask) != 0))
    {
        //
        // Clear disallow reason mask as well as pending disallow ack. It might
        // be possible that this allow request came before completion of
        // previous DISALLOW request. Thus, we need to clear pending DISALLOW_ACK.
        //
        pPgState->disallowReasonMask      = disallowReasonMask;
        pPgState->disallowAckPendingMask &= (~reasonMask);
    }
    else
    {
        pgCtrlEventSend(ctrlId, PMU_PG_EVENT_ALLOW, reasonMask);
    }

    //
    // We are no longer in DISALLOW pending state. It's safe to clear the
    // re-entrancy bit.
    //
    pPgState->disallowReentrancyMask &= (~reasonMask);

    // Update the Stats cache with SW disable reason mask
    pPgState->stats.swDisallowReasonMask = pPgState->disallowReasonMask;

pgCtrlAllow_exit:

    appTaskCriticalExit();
}

/*!
 * @brief API to allow PgCtrl from any non-LPWR task
 *
 * This API re-enables PgCtrl that was disabled by pgCtrlDisallow().
 *
 * @param[in]   ctrlId  Controller ID
 */
void
pgCtrlAllowExt
(
    LwU32 ctrlId
)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_PG_PMU_ONLY) &&
        pgIsCtrlSupported(ctrlId))
    {
        OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);

        appTaskCriticalEnter();
        {
            PMU_HALT_COND(pPgState->extDisallowCnt != 0);

            pPgState->extDisallowCnt--;

            if (pPgState->extDisallowCnt == 0)
            {
                if (LPWR_ARCH_IS_SF_SUPPORTED(EXT_API_OPTIMIZATION))
                {
                    pgCtrlAllow(ctrlId, PG_CTRL_ALLOW_REASON_MASK(PMU_API));
                }
                else
                {
                    pgCtrlEventSend(ctrlId, PMU_PG_EVENT_ALLOW,
                                    PG_CTRL_ALLOW_REASON_MASK(PMU_API));
                }
            }
        }
        appTaskCriticalExit();
    }
}

/*!
 * @brief API to disallow PgCtrl from any non-LPWR task
 *
 * This API disables PgCtrl from any non-LPWR task within PMU. This API will
 * work only when caller task's priority is less than PG Task.
 *
 * @param[in]   ctrlId  Controller ID
 */
void
pgCtrlDisallowExt
(
    LwU32 ctrlId
)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_PG_PMU_ONLY) &&
        pgIsCtrlSupported(ctrlId))
    {
        OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);

        appTaskCriticalEnter();
        {
            LwU32 priority;
            //
            // Caller's priority should be less than PG Task. Priority check is
            // required because the pgCtrlEventSend() is non-blocking call. Check
            // ensure that PG Task will get chance to disable PgCtrl as soon as
            // we add event in PG queue.
            //
            lwrtosLwrrentTaskPriorityGet(&priority);

            PMU_HALT_COND(priority < OSTASK_PRIORITY(LPWR));

            //
            // There is very coerner case scenario in which we might end up queuing too much
            // event to LowPower Task such that queue is full. Consider the following:
            // 1.  Client A need to do multiple disable/enable call for LowPower feature.
            // 2.  There are no other disallow request for the feature.
            // 3.  Client A queues the disable request.
            // 4.  Now client A is blocked until feature is disabled.
            // 5.  Now client A queues the allow request.
            // 6.  This will cause the reentrancyMask to become clear, but feature will be still disabled.
            // 7.  This will also queue an event to LPWR Task, but client will not be blocked.
            // 8.  Now for some reason, LowPower Task is not scheudled or suspended.
            // 9.  And Client A again comes with disable request.
            // 10. At this moment, allow request is still pending, as FSM has not serviced it.
            // 11. So this request again push a new requst to disable.
            // 12. But now client will not blocked because feature is already in disabled state.
            // 13. This can cause the client again queue a enable request and then this process
            // can keep continue, which ultimately will lead to queue full scenario.
            //
            // Hence solution is if external allow request is pending, we should wait for allow
            // to get complete before we process any other disable request. This will help us
            // to avoid this scenario for external clients.
            //
            // Wait until completion of Previous allow reqeust if there was any has been completed.
            // This based on the fact if extDisallowCnt is zero, it means allow has been queued.
            // So Wait until allow is complete before queuing up the new Disable request.
            //
            while (((pPgState->disallowReasonMask & PG_CTRL_DISALLOW_REASON_MASK(PMU_API)) != 0) &&
                    (pPgState->extDisallowCnt == 0))
            {
                appTaskCriticalExit();

                lwrtosYIELD();

                appTaskCriticalEnter();
            }

            if (pPgState->extDisallowCnt == 0)
            {
                if (LPWR_ARCH_IS_SF_SUPPORTED(EXT_API_OPTIMIZATION))
                {
                    pgCtrlDisallow(ctrlId, PG_CTRL_DISALLOW_REASON_MASK(PMU_API));
                }
                else
                {
                    pgCtrlEventSend(ctrlId, PMU_PG_EVENT_DISALLOW,
                                    PG_CTRL_DISALLOW_REASON_MASK(PMU_API));
                }
            }

            pPgState->extDisallowCnt++;
        }
        appTaskCriticalExit();

        //
        // Wait until completion of DISALLOW request. Following condition will
        // be true when DISALLOW processing completed -
        // 1) PgCtrl goes to DISALLOW state AND
        // 2) Disallow reason is PMU_API
        //
        while ((pPgState->state != PMU_PG_STATE_DISALLOW) ||
               ((pPgState->disallowReasonMask & PG_CTRL_DISALLOW_REASON_MASK(PMU_API)) == 0))
        {
            lwrtosYIELD();
        }
    }
}

/*!
 * @brief External API to Wake PgCtrl
 *
 * Other tasks will use this API to wake particular PgCtrl.
 *
 * Ideally, we should keep this API into Resident Section of IMEM so that the
 * API will be available to all tasks in PMU. But approach was un-necessarily
 * increasing IMEM foot-print of the tasks which were not using this API. Thus
 * we decided to make it STATIC INLINE function.
 *
 * @param[in]   ctrlId  Controller ID
 */
void
pgCtrlWakeExt
(
    LwU32 ctrlId
)
{
    appTaskCriticalEnter();
    {
        // Wake PgCtrl only if its not in Full Power Mode.
        if (pgIsCtrlSupported(ctrlId) &&
            !PG_IS_FULL_POWER(ctrlId))
        {
            // Send WAKEUP event and update wake-up reason mask
            pgCtrlEventSend(ctrlId, PMU_PG_EVENT_WAKEUP,
                                    PG_WAKEUP_EVENT_PMU_WAKEUP_EXT);
        }
    }
    appTaskCriticalExit();
}

/*!
 * @brief API to request modification in Subfeature mask of PGCTRL
 *
 * @param[in]   ctrlId            Id of Pg Eng for which enabled Mask needs
 *                                to be updated.
 * @param[in]   clientIndex       Id of the client which is requesting the
 *                                change in SFM.
 * @param[in]   clientEnabledMask New value of enabledMask requested by
 *                                client
 *
 * @return      bValid            LW_TRUE if there is valid change in final SFM
 *                                LW_FALSE if there is no change in final SFM
 *
 * Logic for updating the subfeature enabledMask for Multiple clients:
 *
 * To update the Subfeature enabledMask, PG should be in full power mode.
 * If its not in full power mode then updating the enabled mask
 * will mess up with Engage/Disengage sequnce of PG Eng which will
 * result in H/w Failure.
 *
 * To handle this case, we will first disallow the PG Eng and set the
 * state of Pg State MC as pending. Once PG Eng is in disallow state
 * then during the LogicAck handler of PG state machine, we will
 * update the sub feature enabled mask and then clear the pending
 * state of PG State MC.
 *
 * Below are steps which are ilwoled in the sequence :
 *
 * Any client which want to update the SFM will call the API
 * pgCtrlSubfeatureMaskRequest() to request new subfeature mask
 * as per its need. Then we will have the following steps to update
 * the SFM.
 *
 * Step 1: Update the client cache with client's new enabledMask.
 *
 * Step 2: Callwlate the new enabledMask by taking into account the
 * cache of all the clients.
 *
 * Step 3: Check if the new mask is different from the current mask.
 * If both mask are same, simply return. There will be no further steps.
 *
 * Step 4: If both mask are different then check If PG Eng is in disallow
 * state. If PG is in disallow state update the enabled mask using API
 * pgCtrlSubfeatureMaskUpdate() with new requested
 * value and return.
 *
 * Setp 5: If PG Eng is not in disallow state, then we can not directly
 * update the enabled Mask belwase it will mess up with the PG Eng
 * sequence. So we need to disallow the PG ENG.
 *
 * setp 6: Send a disallow event to the PG Eng and Mark the PG Eng
 * state as pending for SFM_UPDATE. We will check this state pending
 * bit while sending further disallow event. It will help us to avoid
 * sending the duplicate request
 *
 * Step 7 and 8 will be exelwted during Pg StateMachine LogicAck
 * handler.
 *
 * Step 7: Update the EnabledMask from the cached Value.
 *
 * Setp 8: Send allow event to PG and clear the Pending state of PG Eng
 * for SFM_UPDATE.
 */
LwBool pgCtrlSubfeatureMaskRequest
(
    LwU8    ctrlId,
    LwU8    clientIndex,
    LwU32   clientEnabledMask
)
{
    LwBool bValid = LW_FALSE;

    if (PMUCFG_FEATURE_ENABLED(PMU_PG_SFM_2))
    {
        LwU8        idx;
        OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);

        // update the client cache.
        pPgState->enabledMaskClient[clientIndex] = clientEnabledMask;

        // Initialize the new requested mask with supportMask
        pPgState->requestedMask = pPgState->supportMask;

        //
        // Callwlate final requested subfeature mask from all client.
        //
        for (idx = 0; idx < LPWR_CTRL_SFM_CLIENT_ID__COUNT; idx++)
        {
            pPgState->requestedMask &= pPgState->enabledMaskClient[idx];
        }

        // If the new requested mask is different from last requested mask update it.
        if (pPgState->requestedMask != pPgState->enabledMask)
        {
            if (pgCtrlDisallow(ctrlId, PG_CTRL_DISALLOW_REASON_MASK(SFM)))
            {
                pgCtrlSubfeatureMaskUpdate(ctrlId);
                pgCtrlAllow(ctrlId, PG_CTRL_ALLOW_REASON_MASK(SFM));
            }

            bValid = LW_TRUE;
        }
    }
    return bValid;
}

/*!
 * @brief API to update H/W state and S/W state for PGCTRL based on
 * new subfeature enabled mask. This API is internal to SFM infrastructure.
 *
 * This API must be called from critical section only.
 *
 * @param[in]   ctrlId      Id of Pg Eng for which enabled Mask needs
 *                          to be updated.
 */
void pgCtrlSubfeatureMaskUpdate
(
    LwU8 ctrlId
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);

    //
    // If there is a delta between requested and the current
    // enabledMask, then only update the H/W and S/W states.
    //
    // We need to check this delta again since it possible that
    // in between a new request might have been queued to update
    // the requestedMask.
    //
    if (pPgState->requestedMask != pPgState->enabledMask)
    {
        //
        // These functions will be used to update the H/W
        // SM of the PG ENG based on the new subfeature mask.
        //
        if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_GR) &&
            lpwrGrpIsLpwrCtrlMember(RM_PMU_LPWR_GRP_CTRL_ID_GR, ctrlId))
        {
            grSfmHandler(ctrlId);
        }
        else if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MS) &&
                 lpwrGrpIsLpwrCtrlMember(RM_PMU_LPWR_GRP_CTRL_ID_MS, ctrlId))
        {
            // Call MS SFM specific functions
            msSfmHandler(ctrlId);
        }
        else
        {
            // nothing to do.
        }

        // Update the enabledMask with new value.
        pPgState->enabledMask = pPgState->requestedMask;
    }
}

/*!
 * @brief Check if a sub-feature of PgCtrl is supported
 *
 * @param[in]   ctrlId      PgCtrl id
 * @param[in]   sfId        Sub-feature id
 *
 * @return  LW_TRUE     If sub feature is supported
 *          LW_FALSE    Otherwise
 */
LwBool
pgCtrlIsSubFeatureSupported
(
    LwU32      ctrlId,
    LwU32      sfId
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);

    // Check if PgCtrl is supported
    if (!pgIsCtrlSupported(ctrlId))
    {
        return LW_FALSE;
    }

    // Check if sub-feature is set in the support mask
    if ((pPgState->supportMask & (BIT(sfId))) == 0)
    {
        return LW_FALSE;
    }

    return LW_TRUE;
}

/*!
 * @brief: Check if current cycles needs PPU threshold
 *
 * It is possible that there is a large delay between power feature exit because
 * of holdoff interrupt and the host sending methods to the target engine. In
 * this time, engine is idle and if the idle counter matches to the idle filter
 * then HW SM can start another entry. This will result in thrashing. To prevent
 * the thrashing the post power up idle filter threshold is only used between
 * the time from the engine is being powered up and until engine goes into
 * non-idle state.
 *
 * During the post power up period if the idle counter matches the threshold in
 * PPU threshold, it kicks off LPWR sequence ON process.
 *
 * By default, activate PPU threshold only for exits caused by holdoff interrupts.
 *
 * PPU_THRESHOLD_ALWAYS and PPU_THRESHOLD_NEVER are debug features used to
 * modify the default behavior.
 *
 * @param[in]  ctrlId       PgCtrl ID
 *
 * @return      LW_TRUE     PPU is required
 *              LW_FALSE    PPU is not required
 */
LwBool
pgCtrlIsPpuThresholdRequired
(
    LwU32 ctrlId
)
{
    OBJPGSTATE *pPgState  = GET_OBJPGSTATE(ctrlId);
    LwBool      bRequired = LW_FALSE;

    if (LPWR_ARCH_IS_SF_SUPPORTED(PPU_THRESHOLD_ALWAYS))
    {
        // Alway activate the PPU threshold
        bRequired = LW_TRUE;
    }
    else if (LPWR_ARCH_IS_SF_SUPPORTED(PPU_THRESHOLD_NEVER))
    {
        // PPU threshold is not required
    }
    else if ((pPgState->stats.wakeUpEvents & PG_WAKEUP_EVENT_HOLDOFF) != 0)
    {
        bRequired = LW_TRUE;
    }

    return bRequired;
}

/*!
 * @brief Disallow PgCtrl through HW mechanism (SW_CLIENT)
 *
 * This interface does the following -
 *  - Disallow the PgCtrl through HW mechanism
 *  - Update the reason mask
 *
 * @param[in]   pgCtrlId        PG controller id
 *
 */
void
pgCtrlHwDisallow
(
    LwU32 ctrlId,
    LwU32 reasonId
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);

    if (pPgState->hwDisallowReasonMask == 0)
    {
        // Disallow PgCtrl
        pgCtrlHwDisallowHal_HAL(&Pg, ctrlId);
    }

    // Update the disable reason mask
    pPgState->hwDisallowReasonMask |= BIT(reasonId);

    // Update the Stats cache with disable reason mask
    pPgState->stats.hwDisallowReasonMask = pPgState->hwDisallowReasonMask;
}

/*!
 * @brief Allow PgCtrl through HW mechanism (SW_CLIENT)
 *
 * This interface does the following -
 *  - Update the reason mask
 *  - Allow the PgCtrl through HW mechanism
 *
 * @param[in]   pgCtrlId        PG controller id
 *
 */
void
pgCtrlHwAllow
(
    LwU32 ctrlId,
    LwU32 reasonId
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);

    // Update the disable reason mask
    pPgState->hwDisallowReasonMask &= (~BIT(reasonId));

    if (pPgState->hwDisallowReasonMask == 0)
    {
        // Allow PgCtrl
        pgCtrlHwAllowHal_HAL(&Pg, ctrlId);
    }

    // Update the Stats cache with disable reason mask
    pPgState->stats.hwDisallowReasonMask = pPgState->hwDisallowReasonMask;
}

/* ------------------------- Private Functions ----------------------------- */
/*!
 * @brief Initializes idle supplementary counter IDX for PgCtrl
 *
 * HW Manual has given following mapping between idle counters and
 * HW PG_ENG -
 * IDLE_MASK_i_SUPP(0) : PG_ENG/LPWR_ENG 0
 * IDLE_MASK_i_SUPP(1) : PG_ENG/LPWR_ENG 1 (Not productized)
 * IDLE_MASK_i_SUPP(2) : Workload histogram 0 -> Histogram(0)
 * IDLE_MASK_i_SUPP(3) : Workload histogram 1 -> Histogram(1)
 * IDLE_MASK_i_SUPP(4) : PG_ENG/LPWR_ENG 4
 * IDLE_MASK_i_SUPP(5) : Workload histogram 2 -> Histogram(2)
 * IDLE_MASK_i_SUPP(6) : Workload histogram 3 -> Histogram(3)
 * IDLE_MASK_i_SUPP(7) : PG_ENG/LPWR_ENG 3
 *
 * NOTE: Supplementary counters are used on Pre-Turing chips. We have
 * introduced LPWR_ENG on Turing. PG_ENG and LPWR_ENG are using new
 * counters on Turing and onward chips.
 *
 * @param[in]   ctrlId      Pg Controller ID
 *
 * @return      FLCN_OK                 On success
 * @return      FLCN_ERR_NOT_SUPPORTED  If HW engine is not supported
 */
static FLCN_STATUS
s_pgCtrlConfigIdleSuppCntrIdx
(
    LwU32 ctrlId
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);
    FLCN_STATUS status   = FLCN_OK;

    //
    // Supplementary counters are not used by LPWR_ENG.
    // They are used by legacy PG_ENG
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_ENG) &&
        LPWR_ARCH_IS_SF_SUPPORTED(LPWR_ENG))
    {
        pPgState->idleSuppIdx = PG_IDLE_SUPP_CNTR_IDX__ILWALID;
    }
    else
    {
        switch (pPgState->hwEngIdx)
        {
            case PG_ENG_IDX_ENG_0:
            {
                pPgState->idleSuppIdx = PG_IDLE_SUPP_CNTR_IDX_0;
                break;
            }

            case PG_ENG_IDX_ENG_3:
            {
                pPgState->idleSuppIdx = PG_IDLE_SUPP_CNTR_IDX_7;
                break;
            }

            case PG_ENG_IDX_ENG_4:
            {
                pPgState->idleSuppIdx = PG_IDLE_SUPP_CNTR_IDX_4;
                break;
            }

            default:
            {
                PMU_BREAKPOINT();
                pPgState->idleSuppIdx = PG_IDLE_SUPP_CNTR_IDX__ILWALID;
                status                = FLCN_ERR_NOT_SUPPORTED;
            }
        }
    }

    return status;
}
