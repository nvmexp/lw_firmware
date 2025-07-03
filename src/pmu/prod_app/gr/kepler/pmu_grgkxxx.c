/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_grgkxxx.c
 * @brief  HAL-interface for the GKXXX Graphics Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objlpwr.h"
#include "pmu_objgr.h"
#include "pmu_objseq.h"
#include "pmu_objfifo.h"
#include "pmu_objpmu.h"

#include "dev_top.h"
#include "dev_graphics_nobundle.h"
#include "dev_graphics_nobundle_addendum.h"
#include "dev_pwr_pri.h"

#include "dbgprintf.h"
#include "config/g_gr_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static void        s_grPgAbort_GMXXX(LwU32 abortReason, LwU32 abortStage);

/* ------------------------- Public Functions ------------------------------- */

/*!
 * Save GR's state
 *
 * @returns FLCN_OK    when saved successfully.
 * @returns FLCN_ERROR otherwise
 */
FLCN_STATUS
grPgSave_GMXXX(void)
{
    GR_SEQ_CACHE *pGrSeqCache = GR_SEQ_GET_CACHE();
    OBJPGSTATE   *pGrState    = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);
    LwU32         abortReason = 0;

    pGrSeqCache->fifoMutexToken   = LW_U8_MAX;
    pGrSeqCache->grMutexToken     = LW_U8_MAX;
    pGrSeqCache->bSkipSchedEnable = LW_FALSE;

    // Checkpoint1: Check GR Idleness
    if (!grIsIdle_HAL(&Gr, &abortReason, RM_PMU_LPWR_CTRL_ID_GR_PG))
    {
        s_grPgAbort_GMXXX(abortReason,
                         GR_ABORT_CHKPT_AFTER_PG_ON);

        return FLCN_ERROR;
    }

    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libFifo));

    //
    // While disabling engine activity (scheduler, preemption), we need to make
    // sure RM does not touch these registers.
    //
    if (fifoMutexAcquire(&pGrSeqCache->fifoMutexToken) != FLCN_OK)
    {
        s_grPgAbort_GMXXX(GR_ABORT_REASON_MUTEX_ACQUIRE_ERROR,
                         GR_ABORT_CHKPT_AFTER_OVERLAY_LOAD);

        return FLCN_ERROR;
    }

    //
    // Acquire GR mutex to avoid queueing any new bind/ilwalidate requests
    // from RM.
    //
    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, UNBIND))
    {
        if (grMutexAcquire(&pGrSeqCache->grMutexToken) != FLCN_OK)
        {
            s_grPgAbort_GMXXX(GR_ABORT_REASON_MUTEX_ACQUIRE_ERROR,
                             GR_ABORT_CHKPT_AFTER_FIFO_MUTEX_ACQUIRE);

            return FLCN_ERROR;
        }
    }

    //
    // Disable scheduling for GR runlist. Runlist will not get schedule until
    // we enable it. If runlist is already scheduled then channel within runlist
    // can still send the workload to engine.
    //
    Gr.pSeqCache->bSkipSchedEnable = fifoSchedulerDisable_HAL(&Fifo, PMU_ENGINE_GR);

    // Checkpoint 2: Check GR Idleness
    if (!grIsIdle_HAL(&Gr, &abortReason, RM_PMU_LPWR_CTRL_ID_GR_PG))
    {
        s_grPgAbort_GMXXX(abortReason,
                         GR_ABORT_CHKPT_AFTER_SCHEDULER_DISABLE);

        return FLCN_ERROR;
    }

   // Enable engine holdoffs before exelwting preempt sequence
    if (FLCN_OK != lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_GR_PG,
                                                 pGrState->holdoffMask))
    {
        s_grPgAbort_GMXXX(GR_ABORT_REASON_HOLDOFF_ENABLE_TIMEOUT,
                         GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_0);

        return FLCN_ERROR;
    }

    // Checkpoint 3: Check GR Idleness
    if (!grIsIdle_HAL(&Gr, &abortReason, RM_PMU_LPWR_CTRL_ID_GR_PG))
    {
        s_grPgAbort_GMXXX(abortReason,
                         GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_0);

        return FLCN_ERROR;
    }

    //
    // Execute preemption sequence. As part of preemption sequence host saves
    // channel and engine context.
    //
    if (fifoPreemptSequence_HAL(&Fifo, PMU_ENGINE_GR, RM_PMU_LPWR_CTRL_ID_GR_PG) != FLCN_OK)
    {
        s_grPgAbort_GMXXX(GR_ABORT_REASON_FIFO_PREEMPT_ERROR,
                         GR_ABORT_CHKPT_AFTER_PREEMPT_SEQUENCE);

        return FLCN_ERROR;
    }

    // Re-enable holdoff: Holdoff was disengaged by preemption sequence
    if (FLCN_OK != lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_GR_PG,
                                                 pGrState->holdoffMask))
    {
        s_grPgAbort_GMXXX(GR_ABORT_REASON_HOLDOFF_ENABLE_TIMEOUT,
                         GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_1);

        return FLCN_ERROR;
    }

    // Re-enable scheduling for GR runlist
    if (!pGrSeqCache->bSkipSchedEnable)
    {
        fifoSchedulerEnable_HAL(&Fifo, PMU_ENGINE_GR);
        pGrSeqCache->bSkipSchedEnable = LW_TRUE;
    }

    // Checkpoint 4: Check GR Idleness
    if (!grIsIdle_HAL(&Gr, &abortReason, RM_PMU_LPWR_CTRL_ID_GR_PG))
    {
        s_grPgAbort_GMXXX(abortReason,
                         GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_1);

        return FLCN_ERROR;
    }

    // Save global state via FECS
    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, SAVE_GLOBAL_STATE))
    {
        if (FLCN_OK != grGlobalStateSave_HAL(&Gr, RM_PMU_LPWR_CTRL_ID_GR_PG))
        {
            s_grPgAbort_GMXXX(GR_ABORT_REASON_REGLIST_SAVE_TIMEOUT,
                             GR_ABORT_CHKPT_AFTER_SAVE_REGLIST_REQUEST);
        }
    }

    //
    // Issue unbind operation for GR prevents HUB MMU from sending cache
    // invalid or cache eviction requests to GPC MMU.
    //
    // - Power gating shut downs GPC MMU on Kepler. Thus issue unbind to
    //   avoid accessing GPC-MMU on Kepler.
    // - We don't shut down GPC-MMU on Turing. But cache eviction request can
    //   cause the graphics traffic on Turing. Thus avoid sending cache
    //   eviction by ilwalidating all the requests.
    //
    // Reference bug 555852 and bug 2270284.
    //
    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, UNBIND))
    {
        if (FLCN_OK != grUnbind_HAL(&Gr, RM_PMU_LPWR_CTRL_ID_GR_PG, LW_TRUE))
        {
            s_grPgAbort_GMXXX(GR_ABORT_REASON_UNBIND,
                             GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_1);

            return FLCN_ERROR;
        }
    }

    //
    // Wait for the GR subunit to become idle. GR Entry
    // sequence can cause the GR signals to reprot busy
    // momentarily.
    //
    if (!grWaitForSubunitsIdle_HAL(&Gr))
    {
        s_grPgAbort_GMXXX(GR_ABORT_REASON_SUBUNIT_BUSY,
                          GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_1);
        return FLCN_ERROR;
    }

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, PRIV_RING))
    {
        grPgPrivErrHandlingActivate_HAL(&Gr, LW_TRUE);
    }

    // Release GR mutex
    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, UNBIND))
    {
        PMU_HALT_COND(pGrSeqCache->grMutexToken != LW_U8_MAX);
        grMutexRelease(pGrSeqCache->grMutexToken);
    }

    //
    // All FIFO register accesses should be done by this time. Release the
    // fifo mutex that was acquired at the start of this function.
    //
    PMU_HALT_COND(pGrSeqCache->fifoMutexToken != LW_U8_MAX);

    fifoMutexRelease(pGrSeqCache->fifoMutexToken);

    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libFifo));

    //
    // ELCG needs to be forced off before clearing the PG_ON interrupt. It will
    // be turned back on when POWERED_DOWN event is received.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PG_GR_WAR1696192_ELCG_FORCE_OFF))
    {
        lpwrCgElcgDisable(PMU_ENGINE_GR, LPWR_CG_ELCG_CTRL_REASON_GR_PG);
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PG_GR_UNIFIED_POWER_SEQ) &&
        LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, HW_SEQUENCE))
    {
        grHwPgEngage_HAL(&Gr);
    }

    return FLCN_OK;
}

/*!
 *  Resets graphics engine
 *
 *  @returns FLCN_OK
 */
FLCN_STATUS
grPgReset_GMXXX(void)
{
    OBJPGSTATE *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);

    //
    // Enable priv access temporarily as grAssertEngineReset
    // may access gr registers.
    //
    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, RESET))
    {
        if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, PRIV_RING))
        {
            grPgPrivErrHandlingActivate_HAL(&Gr, LW_FALSE);
        }

        grPgAssertEngineReset(LW_TRUE);

        if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, PRIV_RING))
        {
            grPgPrivErrHandlingActivate_HAL(&Gr, LW_TRUE);
        }
    }

    return FLCN_OK;
}

/*!
 * Restores the state of GR engine
 *
 * @returns FLCN_OK
 */
FLCN_STATUS
grPgRestore_GMXXX(void)
{
    OBJPGSTATE *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);

    if (PMUCFG_FEATURE_ENABLED(PMU_PG_GR_UNIFIED_POWER_SEQ) &&
        LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, HW_SEQUENCE))
    {
        grHwPgDisengage_HAL(&Gr);
    }

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, PRIV_RING))
    {
        grPgPrivErrHandlingActivate_HAL(&Gr, LW_FALSE);
    }

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, RESET))
    {
        //
        // @bug 754593 : WAR - BLxG controller needs to be reset here to avoid
        // GBL_WRITE_ERROR_GPC.
        //
        grResetBlxgWar_HAL(&Gr);

        grPgAssertEngineReset(LW_FALSE);
    }

    // force context reload by forcing FECS context to invalid
    REG_FLD_WR_DRF_DEF(BAR0, _PGRAPH, _PRI_FECS_LWRRENT_CTX, _VALID, _FALSE);

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, SAVE_GLOBAL_STATE))
    {
        grGlobalStateRestore_HAL(&Gr, RM_PMU_LPWR_CTRL_ID_GR_PG);
    }

    // Disable holdoffs
    lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_GR_PG,
                                  LPWR_HOLDOFF_MASK_NONE);

    return FLCN_OK;
}

/*
 * @brief Restore the global non-ctxsw state
 *
 * 1. Playback the RM GR init sequence
 * 2. Playback the ZBC reload sequence
 * 3. Send RESTORE_REGLIST method to FECS to restore the global state
 */
void
grGlobalStateRestore_GMXXX(LwU8 ctrlId)
{
    FLCN_STATUS status = FLCN_OK;
    
    //
    // Restore global state via FECS
    //
    // Method data = 0x1 (ELPG register list)
    // Poll value  = 0x1
    //
    status = grSubmitMethod_HAL(&Gr, LW_PGRAPH_PRI_FECS_METHOD_PUSH_ADR_RESTORE_REGLIST,
                                0x1, 0x1);

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    grPgL1Scrub_HAL();
}

/*!
 * @brief Initializes holdoff mask for GR.
 */
void
grInitSetHoldoffMask_GMXXX(void)
{
    OBJPGSTATE *pGrState    = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);
    LwU32       holdoffMask = 0;

    if (FIFO_IS_ENGINE_PRESENT(GR))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_GR));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE2))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE2));
    }

    pGrState->holdoffMask = holdoffMask;
}

/*!
 * @brief Enable/disable WAR for bug 1793923 by toggling bIgnoreHoldoffIntr
 *
 * @param[in] bActivate Boolean value to activate/deactivate WAR
 */
void
grActivateHoldoffIntrWar_GMXXX
(
    LwBool bActivate
)
{
    appTaskCriticalEnter();
    {
        Gr.bIgnoreHoldoffIntr = bActivate;
    }
    appTaskCriticalExit();
}

/*!
 * @brief Save Global state via FECS
 * 
 * @return FLCN_STATUS  FLCN_OK    If save reglist is sucessful in 4ms.
 *                      FLCN_ERROR If save reglist is unsucessful in 4ms.
 */
FLCN_STATUS
grGlobalStateSave_GMXXX(LwU8 ctrlId)
{
    FLCN_STATUS status = FLCN_OK;
    //
    // Save global state via FECS
    //
    // Method data = 0x1 (ELPG register list)
    // Poll value  = 0x1
    //
    status = grSubmitMethod_HAL(&Gr, LW_PGRAPH_PRI_FECS_METHOD_PUSH_ADR_SAVE_REGLIST,
                                0x1, 0x1);

    if (status == FLCN_OK)
    {
        //
        // State save introduces GR traffic. This asserts idle flip. Clear
        // idle flip to avoid this false alarm.
        //
        grClearIdleFlip_HAL(&Gr, ctrlId);
    }

    return status;
}

/*!
 * @brief Check Idleness of GR engine
 *
 * We basically check 2 things -
 *      1. If any GR interrupt is pending
 *      2. If GR Idleness has been flipped
 *      3. If message is pending in the LWOS_QUEUE(PMU, PG)
 *
 * @param[in]  pAbortReason    Pointer to send back Abort Reason
 *
 * @return     LW_TRUE         if GR engine is idle
 *             LW_FALSE        otherwise
 */
LwBool
grIsIdle_GMXXX
(
    LwU32 *pAbortReason,
    LwU8   ctrlId
)
{
    // Report non-idle if there are pending interrupts on GR
    if (grIsIntrPending_HAL(&Gr))
    {
        *pAbortReason = GR_ABORT_REASON_GR_INTR_PENDING;
        return LW_FALSE;
    }

    // Abort if GR IDLE_FLIP asserted
    if (pgCtrlIdleFlipped_HAL(&Pg, ctrlId))
    {
        *pAbortReason = GR_ABORT_REASON_IDLE_FLIPPED;
        return LW_FALSE;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_GR_WAKEUP_PENDING) &&
        grWakeupPending())
    {
        *pAbortReason = GR_ABORT_REASON_WAKEUP;
        return LW_FALSE;
    }

    return LW_TRUE;
}

/*!
 * @brief Wait till graphics submits are idle
 *
 * Function polls the idle status to make sure GR subsystem is idle.
 *
 * @return      LW_TURE   GR sub-system is idle
 *              LW_FALSE  GR sub-system is not idle
 */
LwBool
grWaitForSubunitsIdle_GMXXX(void)
{
    FLCN_TIMESTAMP idleStatusPollTimeNs;
    LwBool         bIdle = LW_FALSE;

    // Start time for idle status polling
    osPTimerTimeNsLwrrentGet(&idleStatusPollTimeNs);

    // Poll until signals do not report Idle or we timeout
    do
    {
        bIdle = pgCtrlIsSubunitsIdle_HAL(&Pg,
                                         RM_PMU_LPWR_CTRL_ID_GR_PG);

        // Check for the timeout if units are not idle
        if ((!bIdle) &&
            (osPTimerTimeNsElapsedGet(&idleStatusPollTimeNs) >=
            GR_IDLE_STATUS_CHECK_TIMEOUT_NS))
        {
            break;
        }
    } while (!bIdle);

    return bIdle;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Abort GR sequence, perform backout sequence as needed
 *
 * @param[in]  abortReason     Reason for aborting gr during entry sequence
 * @param[in]  abortStage      Stage of entry sequence where gr is aborted,
 *                             this determines the steps to be performed
 *                             in abort sequence
 */
static void
s_grPgAbort_GMXXX
(
   LwU32  abortReason,
   LwU32  abortStage
)
{
    GR_SEQ_CACHE *pGrSeqCache  = GR_SEQ_GET_CACHE();
    OBJPGSTATE   *pGrState     = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);

    pGrState->stats.abortReason = (abortStage | abortReason);

    switch (abortStage)
    {
        //
        // Do not change the order of case sections.
        // No break in each case except the last one.
        // Do not compile out "case" using #if(PMUCFG_FEATURE_ENABLED()).
        //
        case GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_1:
        case GR_ABORT_REASON_REGLIST_SAVE_TIMEOUT:
        case GR_ABORT_CHKPT_AFTER_PREEMPT_SEQUENCE:
        case GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_0:
        {
            // Disable holdoffs
            lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_GR_PG,
                                   LPWR_HOLDOFF_MASK_NONE);

            // coverity[fallthrough]
        }
        case GR_ABORT_CHKPT_AFTER_SCHEDULER_DISABLE:
        {
            // Enable scheduling
            if (!pGrSeqCache->bSkipSchedEnable)
            {
                fifoSchedulerEnable_HAL(&Fifo, PMU_ENGINE_GR);
            }

            // coverity[fallthrough]
        }
        case GR_ABORT_CHKPT_AFTER_GR_MUTEX_ACQUIRE:
        {
            // Relase the GR mutex
            if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, UNBIND))
            {
                PMU_HALT_COND(pGrSeqCache->grMutexToken != LW_U8_MAX);
                grMutexRelease(pGrSeqCache->grMutexToken);
            }

            // coverity[fallthrough]
        }
        case GR_ABORT_CHKPT_AFTER_FIFO_MUTEX_ACQUIRE:
        {
            //
            // All FIFO register accesses should be done by this time. Release
            // the fifo mutex that was acquired at the start of GR entry
            //
            PMU_HALT_COND(pGrSeqCache->fifoMutexToken != LW_U8_MAX);

            fifoMutexRelease(pGrSeqCache->fifoMutexToken);

            // coverity[fallthrough]
        }
        case GR_ABORT_CHKPT_AFTER_OVERLAY_LOAD:
        {
            OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libFifo));

            // coverity[fallthrough]
        }
        case GR_ABORT_CHKPT_AFTER_PG_ON:
        {
            // No-op
            break;
        }
    }
}
