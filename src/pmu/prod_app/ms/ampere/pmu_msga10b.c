/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 - 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_ga10b.c
 * @brief  HAL-interface for the GA10B MS_LTC Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"
#include "dev_perf.h"
#include "dev_gsp.h"
#include "dev_bus.h"
#include "dev_ltc.h"
#include "dev_smcarb.h"
#include "dev_pri_ringstation_sys.h"
#if (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
#include "ram_profile_init_seq.h"
#endif // (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objlpwr.h"
#include "pmu_objms.h"
#include "pmu_objfifo.h"

#include "config/g_ms_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

#ifndef SRAM_PG_PROFILE_POWER_ON
#define SRAM_PG_PROFILE_POWER_ON   LW_CPWR_PMU_RAM_TARGET_SEQ_POWER_ON
#endif

// TODO - Divya to confirm with HW team regarding below timeout values

/*
 * Timeout for MS-LTC Priv blocker engagement
 */
#define MS_LTC_PRIV_BLOCKER_ENGAGE_TIMEOUT_NS     (30)

/*!
 * As per RTL test results keeping the max Poll time 10msec
 */
#define MS_UNIFIED_SEQ_POLL_TIME_MS_GA10B         (10000)

/*
 * Timeout for overall MS-LTC NISO FB blockers engagement
 */
#define MS_LTC_NISO_FB_BLOCKER_ENGAGE_TIMEOUT_NS  (60)

/*
 * Timeout for overall MS-LTC LTC IDLE
 */
#define MS_LTC_IDLE_TIMEOUT_NS  (50) 


/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Static Functions -------------------------------- */
static LwBool         s_msLtcIsIdle_GA10B(LwU16 *pAbortReason);
static FLCN_STATUS    s_msLtcNisoBlockersEngage_GA10B(void);
static FLCN_STATUS    s_msLtcNisoBlockersDisengage_GA10B(void);
static void           s_msLtcAbort_GA10B(LwU16 reason, LwU16 stage);
static void           s_msLtcEngageL2Rppg_GA10B(LwBool bBlocking);
static void           s_msLtcDisengageL2Rppg_GA10B(LwBool bBlocking);
/* ------------------------- Global Variables ------------------------------- */
/* --------------------------- Prototypes ------------------------------------*/
/* ------------------------ Public Functions  ------------------------------- */

/*!
 * @brief Initializes the idle mask for MS engine.
 */
void
msLtcIdleMaskInit_GA10B(void)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_LTC);
    LwU32       im[RM_PMU_PG_IDLE_BANK_SIZE] = { 0 };

    if (FIFO_IS_ENGINE_PRESENT(GR))
    {
        im[0] = (FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR, _ENABLED, im[0])     |
                 FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR_SYS, _ENABLED, im[0]) |
                 FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR_GPC, _ENABLED, im[0]));

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_GR, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE0))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_0, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_CE0, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE1))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_1, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_CE1, im);

    }

    if (FIFO_IS_ENGINE_PRESENT(CE2))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_2, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_CE2, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE3))
    {
        im[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_3, _ENABLED, im[1]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_CE3, im);
    }

    if (LPWR_CTRL_IS_SF_SUPPORTED(pPgState, MS, GSP))
    {
        im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _GSP_FB, _ENABLED, im[2]);
        im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _GSP_PRIV_BLOCKER_MS,
                            _ENABLED, im[2]);
    }

    if (LPWR_CTRL_IS_SF_SUPPORTED(pPgState, MS, HSHUB))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _HSHUB_NISO, _ENABLED, im[0]);
    }

    // From TU10X, FBHUB_ALL only represents the NISO traffic.
    im[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _FBHUB_ALL, _ENABLED, im[1]);

    pPgState->idleMask[0] = im[0];
    pPgState->idleMask[1] = im[1];
    pPgState->idleMask[2] = im[2];
}

/*!
 * @brief Initializes the Holdoff mask for MS engine.
 */
void
msLtcHoldoffMaskInit_GA10B(void)
{
    OBJPGSTATE *pMsState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_LTC);
    LwU32       hm       = 0;

    if (FIFO_IS_ENGINE_PRESENT(GR))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_GR));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE0))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE0));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE1))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE1));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE2))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE2));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE3))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE3));
    }

    pMsState->holdoffMask = hm;
}

/*!
 * @brief Configure entry conditions for MS-LTC
 *
 * If MS_LTC is coupled with GR features, then that GR feature
 * becomes a pre-condition for MS_LTC to engage
 */
void
msLtcEntryConditionConfig_GA10B(void)
{
    OBJPGSTATE *pMsLtcState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_LTC);
    LwU32       val;
    LwU32       newVal;
    LwU32       lpwrCtrlId;
    LwU32       hwEngIdx;

    // Check for GR coupled MS_LTC support
    if (!IS_GR_DECOUPLED_MS_SUPPORTED(pMsLtcState))
    {
        // Read child precondition register
        val = newVal = REG_RD32(CSB, LW_CPWR_PMU_PG_ON_TRIGGER_MASK(PG_ENG_IDX_ENG_MS_LTC));

        // Set parent fields in child pre-condition register
        FOR_EACH_INDEX_IN_MASK(32, lpwrCtrlId, lpwrGrpCtrlMaskGet(RM_PMU_LPWR_GRP_CTRL_ID_GR))
        {
            hwEngIdx = PG_GET_ENG_IDX(lpwrCtrlId);

            newVal = FLD_IDX_SET_DRF(_CPWR_PMU, _PG_ON_TRIGGER_MASK,
                                  _ENG_OFF_OR, hwEngIdx, _ENABLE, newVal);
            newVal = FLD_IDX_SET_DRF(_CPWR_PMU, _PG_ON_TRIGGER_MASK,
                                  _ENG_OFF_GROUP, hwEngIdx, _0, newVal);
        }
        FOR_EACH_INDEX_IN_MASK_END;

        // Update child pre-condition register
        if (newVal != val)
        {
            REG_WR32(CSB, LW_CPWR_PMU_PG_ON_TRIGGER_MASK(PG_ENG_IDX_ENG_MS_LTC), newVal);
        }
    }
}

/*!
 * @brief Enable MISCIO MS-LTC interrupts
 */
void
msLtcInitAndEnableIntr_GA10B(void)
{
    OBJPGSTATE *pMsState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_LTC);
    LwU32       val      = 0;

    if (LPWR_CTRL_IS_SF_SUPPORTED(pMsState, MS, PERF))
    {
        val = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_RISING_EN,
                          _PERF_FB_BLOCKER, _ENABLED, val);
    }

    if (LPWR_CTRL_IS_SF_SUPPORTED(pMsState, MS, GSP))
    {
        val = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_RISING_EN,
                          _GSP_FB_BLOCKER, _ENABLED, val);
    }

    //
    // TODO : Divya to confirm from HW team if more interrupts
    //        should be added here
    //

    Ms.intrRiseEnMask |= val;

    REG_WR32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN, val);
}

/*!
 * @brief Enable MS Wake-up interrupts
 */
void
msEnableIntr_GA10B(void)
{
    LwU32 intrMask;

    appTaskCriticalEnter();
    {
        intrMask = Ms.intrRiseEnMask;

        // Make sure that all current non-MS interrupts will remain untouched
        intrMask |= REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN);

        REG_WR32_STALL(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN, intrMask);
    }
    appTaskCriticalExit();
}

/*!
 * @brief MS_LTC entry sequence
 *
 * @returns FLCN_OK    when entry is successful
 * @returns FLCN_ERROR otherwise
 */
FLCN_STATUS
msLtcEntry_GA10B(void)
{
    OBJPGSTATE    *pMsLtcState      = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_LTC);
    LwU16          abortReason      = 0;
    LwBool         bGspEnabled      = LPWR_CTRL_IS_SF_ENABLED(pMsLtcState, MS, GSP);
    FLCN_TIMESTAMP blockStartTimeNs;
    LwS32          timeoutLeftNs;

    // Check for LTC sub-system idle
    if (!s_msLtcIsIdle_GA10B(&abortReason))
    {
        return FLCN_ERROR;
    }

    // Suspend DMA if it's not locked.
    lwrtosENTER_CRITICAL();
    {
        if (!lwosDmaSuspend(LW_FALSE))
        {
            lwrtosEXIT_CRITICAL();
            s_msLtcAbort_GA10B(MS_ABORT_REASON_DMA_SUSPENSION_FAILED,
                               MS_ABORT_CHKPT_AFTER_PGON);
            return FLCN_ERROR;
        }
        lwrtosLwrrentTaskPrioritySet(OSTASK_PRIORITY_DMA_SUSPENDED);
    }
    lwrtosEXIT_CRITICAL();

    // Send DMA suspend notification to PG Log
    pgLogNotifyMsDmaSuspend();

    //
    // Check for any Pending MEMOPs in HOST
    // Abort if Host mem-op request is pending or outstanding
    //
    if (msIsHostMemOpPending_HAL(&Ms))
    {
        s_msLtcAbort_GA10B(MS_ABORT_REASON_HOST_MEMOP_PENDING,
                           MS_ABORT_CHKPT_AFTER_PGON_AT_SUSPEND);
        return FLCN_ERROR;
    }

    //
    // From GA10X, we need to disable the SMCARB Free running timestamp
    // so that XBAR does not report busy.
    //
    msSmcarbTimestampDisable_HAL(&Ms, LW_TRUE);

    // Suppress the host flush/bind
    REG_FLD_WR_DRF_DEF(BAR0, _PBUS, _FLUSH_BIND_CTL, _ENABLE, _FALSE);

    //
    // Read the host flush back to verify it is disabled. Abort if there are
    // any pending or outstanding flushes which causes the FLUSH to not get
    // disabled
    //
    if (REG_FLD_TEST_DRF_DEF(BAR0, _PBUS, _FLUSH_BIND_CTL, _ENABLE, _TRUE))
    {
       s_msLtcAbort_GA10B(MS_ABORT_REASON_HOST_FLUSH_ENABLED,
                          MS_ABORT_CHKPT_HOST_FLUSH_BIND);
       return FLCN_ERROR;
    }

    // Abort if Host mem-op request is pending or outstanding
    if (msIsHostMemOpPending_HAL(&Ms))
    {
        s_msLtcAbort_GA10B(MS_ABORT_REASON_HOST_MEMOP_PENDING,
                           MS_ABORT_CHKPT_HOST_FLUSH_BIND);
        return FLCN_ERROR;
    }
 
    // Engage GSP priv blocker in BLOCK_ALL mode
    if (FLCN_OK != lpwrCtrlPrivBlockerModeSet(RM_PMU_LPWR_CTRL_ID_MS_LTC,
                                    LPWR_PRIV_BLOCKER_MODE_BLOCK_ALL,
                                    LPWR_PRIV_BLOCKER_PROFILE_MS,
                                    (MS_LTC_PRIV_BLOCKER_ENGAGE_TIMEOUT_NS * 1000)))
    {
        s_msLtcAbort_GA10B(MS_ABORT_REASON_BLK_EVERYTHING_TIMEOUT,
                           MS_ABORT_CHKPT_AFTER_PRIV_BLOCKERS);
        return FLCN_ERROR;
    }

    // Issue the PRIV path Flush
    if (!lpwrPrivPathFlush_HAL(&Lpwr, LW_FALSE, LW_FALSE, bGspEnabled,
                               LPWR_PRIV_FLUSH_DEFAULT_TIMEOUT_NS))
    {
        s_msLtcAbort_GA10B(MS_ABORT_REASON_PRIV_FLUSH_TIMEOUT,
                           MS_ABORT_CHKPT_AFTER_PRIV_BLOCKERS);
        return FLCN_ERROR;
    }

    // Abort if Host mem-op request is pending or outstanding
    if (msIsHostMemOpPending_HAL(&Ms))
    {
        s_msLtcAbort_GA10B(MS_ABORT_REASON_HOST_MEMOP_PENDING,
                           MS_ABORT_CHKPT_AFTER_PRIV_BLOCKERS);
        return FLCN_ERROR;
    }

    // Check for LTC sub-system idle
    if (!s_msLtcIsIdle_GA10B(&abortReason))
    {
        s_msLtcAbort_GA10B(abortReason,
                           MS_ABORT_CHKPT_AFTER_PRIV_BLOCKERS);
        return FLCN_ERROR;
    }

    // Engage method hold-off
    if (FLCN_OK != lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_MS_LTC,
                                                 pMsLtcState->holdoffMask))
    {
        s_msLtcAbort_GA10B(MS_ABORT_REASON_HOLDOFF_TIMEOUT,
                           MS_ABORT_CHKPT_AFTER_PRIV_BLOCKERS);
        return FLCN_ERROR;
    }

    // Engage NISO FB blockers
    if (FLCN_OK != s_msLtcNisoBlockersEngage_GA10B())
    {
        s_msLtcAbort_GA10B(MS_ABORT_REASON_FB_BLOCKER_TIMEOUT,
                           MS_ABORT_CHKPT_AFTER_NISO_BLOCKERS);
        return FLCN_ERROR;
    }

    // Engage GSP priv blocker in BLOCK_EQUATION mode
    if (FLCN_OK != lpwrCtrlPrivBlockerModeSet(RM_PMU_LPWR_CTRL_ID_MS_LTC,
                                    LPWR_PRIV_BLOCKER_MODE_BLOCK_EQUATION,
                                    LPWR_PRIV_BLOCKER_PROFILE_MS,
                                    (MS_LTC_PRIV_BLOCKER_ENGAGE_TIMEOUT_NS * 1000)))
    {
        s_msLtcAbort_GA10B(MS_ABORT_REASON_BLK_EQUATION_TIMEOUT,
                           MS_ABORT_CHKPT_AFTER_ALL_BLOCKERS);
        return FLCN_ERROR;
    }

    // Check for LTC sub-system idle
    if (!s_msLtcIsIdle_GA10B(&abortReason))
    {
        s_msLtcAbort_GA10B(abortReason,
                           MS_ABORT_CHKPT_AFTER_PRIV_BLOCKERS);
    }

    // Disable L2’s AUTO CACHE FLUSH functionality.
    msDisableL2AutoFlush_HAL(&Ms);

    //
    // Monitor the IDLE of the following units. 
    // XBAR, LTC, FBHUB, FBHUB MMU, HSHUB, HSHUB MMU
    //
    if (FLCN_OK != msWaitForSubunitsIdle_HAL(&Ms, RM_PMU_LPWR_CTRL_ID_MS_LTC, &blockStartTimeNs,
                                                 &timeoutLeftNs))
    {
        s_msLtcAbort_GA10B(MS_ABORT_REASON_FB_NOT_IDLE,
                           MS_ABORT_CHKPT_AFTER_L2_CACHE_ACTION);
        return FLCN_ERROR;
    }

    // Disable main PRI ring
    msDisablePrivAccess_HAL(&Ms, LW_TRUE);

    // Enable L2-RPPG for the L2-RAMs
    if (LPWR_CTRL_IS_SF_ENABLED(pMsLtcState, MS, RPPG))
    {
        s_msLtcEngageL2Rppg_GA10B(LW_FALSE);
    }

    return FLCN_OK;
}

/*!
 * @brief MS_LTC exit sequence
 *
 * @returns FLCN_OK
 */
FLCN_STATUS
msLtcExit_GA10B(void)
{
    OBJPGSTATE *pMsLtcState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_LTC);

    // Release L2-RPPG
    if (LPWR_CTRL_IS_SF_ENABLED(pMsLtcState, MS, RPPG))
    {
        s_msLtcDisengageL2Rppg_GA10B(LW_TRUE);
    }
    // Enable PRI ring
    msDisablePrivAccess_HAL(&Ms, LW_FALSE);

    // Restore L2 cache auto flush settings
    msRestoreL2AutoFlush_HAL(&Ms);

    // Disengage NISO FB blockers
    s_msLtcNisoBlockersDisengage_GA10B();

    // Disengage method hold-off
    lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_MS_LTC,
                                  LPWR_HOLDOFF_MASK_NONE);

    // Disengage GSP priv blocker
    lpwrCtrlPrivBlockerModeSet(RM_PMU_LPWR_CTRL_ID_MS_LTC,
                               LPWR_PRIV_BLOCKER_MODE_BLOCK_NONE,
                               LPWR_PRIV_BLOCKER_PROFILE_MS,
                               (MS_LTC_PRIV_BLOCKER_ENGAGE_TIMEOUT_NS * 1000));

    // enable host flush-bind control
    REG_FLD_WR_DRF_DEF(BAR0, _PBUS, _FLUSH_BIND_CTL, _ENABLE, _TRUE);

    // Restore the SMCARB Timestamp
    msSmcarbTimestampDisable_HAL(&Ms, LW_FALSE);

    // Resume DMA and lower task priority
    appTaskCriticalEnter();
    {
        lwosDmaResume();
        lwrtosLwrrentTaskPrioritySet(OSTASK_PRIORITY(LPWR));
    }
    appTaskCriticalExit();

    // Send DMA resume notification to PG Log
    pgLogNotifyMsDmaResume();

    return FLCN_OK;
}

/*!
 * @brief Wait till memory subsystem goes idle
 *
 * Function polls LTC idle status to make sure FB subsystem is idle.
 * Also it ensure that all FB sub-units are idle.
 *
 * @param[in]   ctrlId             LPWR_CTRL Id
 * @param[in]   pBlockStartTimeNs  Start time of the block sequence
 * @param[out]  pTimeoutLeftNs     Time left in the block sequence
 *
 * @return      FLCN_OK     FB sub-system is idle
 *              FLCN_ERROR  FB sub-system is not idle
 */
FLCN_STATUS
msWaitForSubunitsIdle_GA10B
(
    LwU8            ctrlId,
    FLCN_TIMESTAMP *pBlockStartTimeNs,
    LwS32          *pTimeoutLeftNs
)
{
    LwBool         bIdle        = LW_FALSE;
    LwU32          idleStatus;
    LwU32          idleStatus1;

    // Start time for polling
    osPTimerTimeNsLwrrentGet(pBlockStartTimeNs);

    // Poll for LTC idle
    do
    {
        idleStatus = REG_RD32(CSB, LW_CPWR_PMU_IDLE_STATUS);
        bIdle = FLD_TEST_DRF(_CPWR_PMU, _IDLE_STATUS, _LTC_IDLE,
                             _IDLE, idleStatus);

        // Check for timeout
        if (osPTimerTimeNsElapsedGet(pBlockStartTimeNs) >=
                (MS_LTC_IDLE_TIMEOUT_NS * 1000))
        {
            return FLCN_ERROR;
        }

    } while (!bIdle);

    // Ensure that all FB subunits idle
    bIdle = FLD_TEST_DRF(_CPWR_PMU, _IDLE_STATUS, _XBAR_IDLE, _IDLE, idleStatus);
    idleStatus1 = REG_RD32(CSB, LW_CPWR_PMU_IDLE_STATUS_1);

    bIdle = bIdle                                                      &&
            FLD_TEST_DRF(_CPWR_PMU, _IDLE_STATUS_1, _FBHUB_ALL, _IDLE,
                         idleStatus1)                                  &&
            FLD_TEST_DRF(_CPWR_PMU, _IDLE_STATUS_1, _MS_MMU, _IDLE,
                         idleStatus1);

    // Ensure that all HSHUB subunits are idle
    bIdle = bIdle &&
            FLD_TEST_DRF(_CPWR_PMU, _IDLE_STATUS, _HSHUB_ALL, _IDLE,
                         idleStatus);

    if (!bIdle)
    {
       return FLCN_ERROR;
    }
    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Check for LTC sub-system idleness
 *
 * This ilwolves the following -
 *      1. Any wakeup event is pending
 *      2. Idleness has been flipped
 *
 * @param[in/out] pAbortReason pointer to rerurn the abort reason
 *
 * return LW_TRUE  -> LTC sub-system is idle
 *        LW_FALSE -> LTC sub-system is busy
 */
static LwBool
s_msLtcIsIdle_GA10B
(
    LwU16 *pAbortReason
)
{
    LwBool bIsIdle = LW_FALSE;

    // Abort if any wakeup event is pending
    if (msWakeupPending())
    {
        *pAbortReason = MS_ABORT_REASON_WAKEUP_PENDING;
    }
    // Abort if idle status is flipped
    else if (pgCtrlIdleFlipped_HAL(&Pg, RM_PMU_LPWR_CTRL_ID_MS_LTC))
    {
        *pAbortReason = MS_ABORT_REASON_FB_NOT_IDLE;
    }
    else
    {
        bIsIdle = LW_TRUE;
    }

    return bIsIdle;
}

/*!
 * @brief Engage Unit2FB NISO blockers
 *
 * It includes the following blockers -
 *      - PERF
 *      - GSP
 *
 * @return  FLCN_OK     NISO blockers got engaged
 *          FLCN_ERROR  Failed to engage NISO blockers
 */
static FLCN_STATUS
s_msLtcNisoBlockersEngage_GA10B(void)
{
    OBJPGSTATE    *pMsLtcState  = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_LTC);
    LwBool         bStopPolling = LW_TRUE;
    LwU32          val          = 0;
    FLCN_TIMESTAMP nisoBlockerStartTimeNs;

    // Start time for Scrub polling
    osPTimerTimeNsLwrrentGet(&nisoBlockerStartTimeNs);

    //
    // Todo: amanjain We need to Fix once hardware manuals are updated 
    // Arm PERF blocker
    //
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    if (LPWR_CTRL_IS_SF_ENABLED(pMsLtcState, MS, PERF))
    {
        val = REG_RD32(FECS, LW_PERF_PMASYS_SYS_FB_BLOCKER_CTRL);
        val = FLD_SET_DRF(_PERF, _PMASYS_SYS_FB_BLOCKER_CTRL,
                          _BLOCK_EN_CMD, _ARMED, val);
        REG_WR32(FECS, LW_PERF_PMASYS_SYS_FB_BLOCKER_CTRL, val);
    }
#endif // (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))

    // Arm GSP FB Blocker
    if (LPWR_CTRL_IS_SF_ENABLED(pMsLtcState, MS, GSP))
    {
        // Engage FB blocker
        val = REG_RD32(FECS, LW_PGSP_FALCON_ENGCTL);
        val = FLD_SET_DRF(_PGSP, _FALCON_ENGCTL, _STALLREQ_MODE, _TRUE, val) |
              FLD_SET_DRF(_PGSP, _FALCON_ENGCTL, _SET_STALLREQ, _TRUE, val);

        REG_WR32(FECS, LW_PGSP_FALCON_ENGCTL, val);
    }

    do
    {
        //
        // Todo: amanjain We need to Fix once hardware manuals are updated 
        // Disengage PERF blocker
        //
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
        // Poll for PERF FB blocker to engage
        if (LPWR_CTRL_IS_SF_ENABLED(pMsLtcState, MS, PERF))
        {
            val          = REG_RD32(FECS, LW_PERF_PMASYS_SYS_FB_BLOCKER_CTRL);
            bStopPolling = FLD_TEST_DRF(_PERF, _PMASYS_SYS_FB_BLOCKER_CTRL,
                                        _BLOCK_EN_STATUS, _ACKED, val);
            bStopPolling = bStopPolling && FLD_TEST_DRF(_PERF,
                                                        _PMASYS_SYS_FB_BLOCKER_CTRL,
                                                        _OUTSTANDING_REQ,
                                                        _INIT, val);
        }
#endif // (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
       
        // Poll for GSP FB blocker to engage
        if (LPWR_CTRL_IS_SF_ENABLED(pMsLtcState, MS, GSP))
        {
            val          = REG_RD32(FECS, LW_PGSP_BLOCKER_STAT);
            bStopPolling = bStopPolling && FLD_TEST_DRF(_PGSP, _BLOCKER_STAT,
                                                        _FB_BLOCKED, _TRUE, val);
        }
       
        // Check for combined timeout
        if (osPTimerTimeNsElapsedGet(&nisoBlockerStartTimeNs) >=
                (MS_LTC_NISO_FB_BLOCKER_ENGAGE_TIMEOUT_NS * 1000))
        {
            return FLCN_ERROR;
        }
    }
    while (!bStopPolling);

    return FLCN_OK;
}

/*!
 * @brief Disengage Unit2FB NISO blockers
 *
 * It includes the following blockers -
 *      - PERF
 *      - GSP
 *
 * @return  FLCN_OK     NISO blockers got dis-engaged
 *          FLCN_ERROR  Failed to dis-engage NISO blockers
 */
static FLCN_STATUS
s_msLtcNisoBlockersDisengage_GA10B(void)
{
    OBJPGSTATE *pMsLtcState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_LTC);
    LwU32       val         = 0;

    // Disengage GSP FB Blocker
    if (LPWR_CTRL_IS_SF_ENABLED(pMsLtcState, MS, GSP))
    {
        val = REG_RD32(FECS, LW_PGSP_FALCON_ENGCTL);

        // Note that this will also clear STALLREQ_MODE
        val = FLD_SET_DRF(_PGSP, _FALCON_ENGCTL, _CLR_STALLREQ,  _TRUE, val);
        REG_WR32(FECS, LW_PGSP_FALCON_ENGCTL, val);
    }

    //
    // Todo: amanjain We need to Fix once hardware manuals are updated 
    // Disengage PERF blocker
    //
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    if (LPWR_CTRL_IS_SF_ENABLED(pMsLtcState, MS, PERF))
    {
        val = REG_RD32(FECS, LW_PERF_PMASYS_SYS_FB_BLOCKER_CTRL);
        val = FLD_SET_DRF(_PERF, _PMASYS_SYS_FB_BLOCKER_CTRL,
                          _BLOCK_EN_CMD, _UNARMED, val);
        REG_WR32(FECS, LW_PERF_PMASYS_SYS_FB_BLOCKER_CTRL, val);
    }
#endif // (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))

    return FLCN_OK;
}

/*!
 * Abort clock gating, perform backout sequence as needed
 *
 * @param[in]  reason  reason for aborting mcg
 * @param[in]  stage   stage where PG is aborted
 */
static void
s_msLtcAbort_GA10B
(
    LwU16 reason,
    LwU16 stage
)
{
    OBJPGSTATE *pMsLtcState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_LTC);

    pMsLtcState->stats.abortReason = (stage << 16) | reason;

    switch (stage)
    {
        //
        // Do not change the order of case sections.
        // No break in each case except the last one.
        // Do not compile out "case" using #if(PMUCFG_FEATURE_ENABLED()).
        //
        case MS_ABORT_CHKPT_AFTER_L2_CACHE_ACTION:
        {
            // Restore L2 cache auto flush settings
            msRestoreL2AutoFlush_HAL(&Ms);

            // coverity[fallthrough]
        }
        case MS_ABORT_CHKPT_AFTER_ALL_BLOCKERS:
        case MS_ABORT_CHKPT_AFTER_NISO_BLOCKERS:
        {
            // Disengage NISO FB blockers
            s_msLtcNisoBlockersDisengage_GA10B();

            // coverity[fallthrough]
        }
        case MS_ABORT_CHKPT_AFTER_PRIV_BLOCKERS:
        {
            // Disengage method hold-off
            lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_MS_LTC,
                                          LPWR_HOLDOFF_MASK_NONE);

            // Disengage GSP priv blocker
            lpwrCtrlPrivBlockerModeSet(RM_PMU_LPWR_CTRL_ID_MS_LTC,
                                       LPWR_PRIV_BLOCKER_MODE_BLOCK_NONE,
                                       LPWR_PRIV_BLOCKER_PROFILE_MS,
                                       (MS_LTC_PRIV_BLOCKER_ENGAGE_TIMEOUT_NS * 1000));
            // coverity[fallthrough]
        }
        case MS_ABORT_CHKPT_HOST_FLUSH_BIND:
        {
            //
            // Enable the host flush if we abort
            // after disabling it.
            //
            if (REG_FLD_TEST_DRF_DEF(BAR0, _PBUS, _FLUSH_BIND_CTL, _ENABLE, _FALSE))
            {
                REG_FLD_WR_DRF_DEF(BAR0, _PBUS, _FLUSH_BIND_CTL, _ENABLE, _TRUE);
            }
            // Restore the SMCARB Timestamp
            msSmcarbTimestampDisable_HAL(&Ms, LW_FALSE);
        }
        case MS_ABORT_CHKPT_AFTER_PGON_AT_SUSPEND:
        {
            // Resume DMA and lower task priority
            appTaskCriticalEnter();
            {
                lwosDmaResume();
                lwrtosLwrrentTaskPrioritySet(OSTASK_PRIORITY(LPWR));
            }
            appTaskCriticalExit();

            // Send DMA resume notification to PG Log
            pgLogNotifyMsDmaResume();

            // coverity[fallthrough]
        }
        case MS_ABORT_CHKPT_AFTER_PGON:
        {
           break;
        }
    }
}

/*!
 * @brief Power-gate L2 RPPG
 *
 * @params[in] bBlocking   Blocking or non Blocking call
 *
 *  @return    void
 */
void
s_msLtcEngageL2Rppg_GA10B(LwBool bBlocking)
{
    LwU32 regVal;
    LwU32 profile = LPWR_SRAM_PG_PROFILE_GET(RPPG);

    // Set the profile in Sequencer for LPWR_SEQ_SRAM_CTRL_ID_MS.
    regVal = REG_RD32(CSB, LW_CPWR_PMU_RAM_TARGET);
    regVal = FLD_IDX_SET_DRF_NUM(_CPWR_PMU, _RAM_TARGET, _SEQ,
                                 LPWR_SEQ_SRAM_CTRL_ID_MS, profile, regVal);
    REG_WR32_STALL(CSB, LW_CPWR_PMU_RAM_TARGET, regVal);

    // Wait for entry completion - Blocking call
    if (bBlocking)
    {
        if (!PMU_REG32_POLL_US(LW_CPWR_PMU_RAM_STATUS,
                 DRF_SHIFTMASK(LW_CPWR_PMU_RAM_STATUS_PROFILE_SEQ(LPWR_SEQ_SRAM_CTRL_ID_MS)),
                 DRF_IDX_NUM(_CPWR, _PMU_RAM_STATUS, _PROFILE_SEQ,
                 LPWR_SEQ_SRAM_CTRL_ID_MS, profile),
                 MS_UNIFIED_SEQ_POLL_TIME_MS_GA10B,
                 PMU_REG_POLL_MODE_CSB_EQ))
        {
            PMU_BREAKPOINT();
        }
    }
}

/*!
 * @brief Power-ungate L2 RPPG
 *
 * @params[in] bBlocking   Blocking or non Blocking call
 *
 *  @return    void
 */
void
s_msLtcDisengageL2Rppg_GA10B(LwBool bBlocking)
{
    LwU32 regVal;
    LwU32 profile = LPWR_SRAM_PG_PROFILE_GET(POWER_ON);

    // Set the profile in Sequencer for LPWR_SEQ_SRAM_CTRL_ID_MS.
    regVal = REG_RD32(CSB, LW_CPWR_PMU_RAM_TARGET);
    regVal = FLD_IDX_SET_DRF_NUM(_CPWR_PMU, _RAM_TARGET, _SEQ,
                                 LPWR_SEQ_SRAM_CTRL_ID_MS, profile, regVal);
    REG_WR32_STALL(CSB, LW_CPWR_PMU_RAM_TARGET, regVal);

    // Wait for completion - Blocking call
    if (bBlocking)
    {
        if (!PMU_REG32_POLL_US(LW_CPWR_PMU_RAM_STATUS,
                 DRF_SHIFTMASK(LW_CPWR_PMU_RAM_STATUS_PROFILE_SEQ(LPWR_SEQ_SRAM_CTRL_ID_MS)),
                 DRF_IDX_NUM(_CPWR, _PMU_RAM_STATUS, _PROFILE_SEQ,
                 LPWR_SEQ_SRAM_CTRL_ID_MS, profile),
                 MS_UNIFIED_SEQ_POLL_TIME_MS_GA10B,
                 PMU_REG_POLL_MODE_CSB_EQ))
        {
            PMU_BREAKPOINT();
        }
    }
}
