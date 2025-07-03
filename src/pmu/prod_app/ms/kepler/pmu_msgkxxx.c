/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_msgkxxx.c
 * @brief  HAL-interface for the GKxxx Memory System Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu/ssurface.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_falcon_csb.h"
#include "dev_pwr_pri.h"
#include "dev_fb.h"
#include "dev_fbpa.h"
#include "dev_fbpa_addendum.h"
#include "dev_ltc.h"
#include "dev_master.h"
#include "dev_xbar.h"
#include "dev_trim.h"
#include "dev_trim_addendum.h"
#include "dev_pri_ringstation_fbp.h"
#include "dev_pri_ringstation_gpc.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_graphics_nobundle.h"
#include "dev_top.h"
#if (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
#include "dev_lw_xve.h"
#include "dev_flush.h"
#endif
#include "dev_bus.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objms.h"
#include "pmu_objlpwr.h"
#include "pmu_objic.h"
#include "pmu_objpmu.h"
#include "pmu_objdisp.h"
#include "pmu_disp.h"
#include "pmu_objfifo.h"
#include "pmu_objtimer.h"
#include "pmu_objfb.h"
#include "pmu_objgcx.h"
#include "pmu_oslayer.h"
#include "pmu_bar0.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"

#include "dbgprintf.h"
#include "config/g_ms_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* --------------------------- Prototypes ------------------------------------*/
/* ------------------------ Public Functions  ------------------------------- */
/*!
 * Initialize MS object by allocating internal data structures
 */
void
msPrivBlockerXveTimeoutInit_GMXXX(void)
{
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    LwU32     val;

    //
    // Enable the BAR Blocker interrupt timeout. If PMU SW is hosed with
    // MSCG engaged for any reason (like a crash due to stack overrun or
    // a and breakpoint being hit), then this timeout will fire after _VAL
    // usec disengage the XVE blocker so that it does not become a fatal
    // error and hang the PCIe bus.
    //
    val = REG_RD32(BAR0_CFG, LW_XVE_BAR_BLOCKER_INTR_TIMEOUT);

    //
    // For emulation run, we should keep the timeout to max possible value.
    // Timeout field is 16 bit value, so max timeout is 0xFFFF. Emulation
    // runs are slow and we might hit timeout during MSCG entry. So we are
    // keeping the timeout to max possible value.
    //
    if (IsEmulation())
    {
        val = FLD_SET_DRF_NUM(_XVE, _BAR_BLOCKER_INTR_TIMEOUT,
                              _VAL, MS_XVE_BLOCKER_TIMEOUT_EMU_US, val);
    }

    val = FLD_SET_DRF(_XVE, _BAR_BLOCKER_INTR_TIMEOUT, _EN, _ENABLED, val);
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_INTR_TIMEOUT, val);
#endif // (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
}

/*!
 * @brief Enable MSCG Wake-up interrupts
 */
void
msEnableIntr_GMXXX(void)
{
    LwU32                    intrMask      = Ms.intrRiseEnMask;
    DISP_RM_ONESHOT_CONTEXT* pRmOneshotCtx = DispContext.pRmOneshotCtx;

    appTaskCriticalEnter();
    {
        //
        // if OSM Mode active, it mean, current MS Feature
        // would want to ingore the ISO_STUTTER Wakeup event
        // because display will not access the FB in one shot
        // mode. So we need to keep MSPG_WAKE Interrupt Disabled
        //
        // This was already done during the event LPWR_EVENT_ID_MS_OSM_NISO_START
        //
        if (PMUCFG_FEATURE_ENABLED(PMU_MS_OSM) &&
            (pRmOneshotCtx != NULL)            &&
            (pRmOneshotCtx->bActive)           &&
             pRmOneshotCtx->bNisoMsActive)
        {
            intrMask = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_RISING_EN,
                                   _MSPG_WAKE, _DISABLED, Ms.intrRiseEnMask);
        }

        // Make sure that all current non-MS interrupts will remain untouched
        intrMask |= REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN);

        REG_WR32_STALL(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN, intrMask);
    }
    appTaskCriticalExit();
}

/*!
 * @brief Engage all the priv blockers
 *
 * First engage all the priv blockers and then poll on them to make sure they
 * engaged.
 *
 * @param[in] bEquationMode       LW_FALSE => Engage the priv blockers in
 *                                            BLOCK_EVERYTHING mode
 *                                LW_TRUE  => Engage the priv blockers in
 *                                            BLOCK_EQUATION mode
 * @param[in]  pBlockStartTimeNs  start time for the block sequence
 * @param[out] pBlockStartTimeNs  timeout left
 *
 * @return LW_TRUE  => all the priv blockers engaged successfully
 *         LW_FALSE => one or more priv blockers didn't engage successfully, so
 *                     we should abort the entry
 */
LwBool
msPrivBlockerEngage_GMXXX
(
    LwBool          bEquationMode,
    FLCN_TIMESTAMP *pBlockStartTimeNs,
    LwS32          *pTimeoutLeftNs
)
{
#if (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
    LwU32 val = 0;

    // 1. XVE BAR blocker
    if (bEquationMode)
    {
        // BLOCK_EQUATION mode
        val = FLD_SET_DRF(_XVE, _BAR_BLOCKER,
                          _CTRL_BLOCKER_MODE, _BLOCK_EQUATION, 0);
        val = FLD_SET_DRF(_XVE, _BAR_BLOCKER,
                          _CTRL_BLOCKER_MODE_OVERWRITE, _ENABLED, val);
    }
    else
    {
        // BLOCK_EVERYTHING mode
        val = FLD_SET_DRF(_XVE, _BAR_BLOCKER,
                          _CTRL_BLOCKER_MODE, _BLOCK_EVERYTHING, 0);
        val = FLD_SET_DRF(_XVE, _BAR_BLOCKER,
                          _CTRL_BLOCKER_MODE_OVERWRITE, _ENABLED, val);
    }
    REG_WR32(FECS_CFG, LW_XVE_BAR_BLOCKER, val);

    //
    // 2. We only poll to make sure that the XVE BAR blocker engaged
    // successfully when engaging in equation mode. This is the mode that the
    // blocker will remain in, when we are in MSCG.
    //
    if (bEquationMode)
    {
        if (msCheckTimeout(Ms.abortTimeoutNs, pBlockStartTimeNs, pTimeoutLeftNs) ||
            !PMU_REG32_POLL_NS(USE_FECS(XVE_REG(LW_XVE_BAR_BLOCKER)),
                 DRF_SHIFTMASK(LW_XVE_BAR_BLOCKER_CTRL_BLOCKER_MODE),
                 DRF_DEF(_XVE, _BAR_BLOCKER, _CTRL_BLOCKER_MODE, _BLOCK_EQUATION),
                 (LwU32) (*pTimeoutLeftNs),
                 PMU_REG_POLL_MODE_BAR0_EQ))
        {
            return LW_FALSE;
        }
    }
#endif
    return LW_TRUE;
}

/*!
 * @brief Exit Sequence for Priv Blockers
 *
 * Step:
 * 1. Disengage the XVE BAR Blocker.
 * 2. Re enable the XVE BAR Blocker GPIO Interrupt.
 */
void
msPrivBlockerExit_GMXXX
(
    LwU8 ctrlId
)
{
#if (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
    //
    // 1. XVE BAR blocker
    //
    // Disabling the XVE blocker (same as writing BLOCK_NOTHING) also clears
    // the XVE_INTR_STAT. This removes the need to explicitly clear the
    // XVE_INTR_STAT bit.
    //
    REG_WR32(FECS_CFG, LW_XVE_BAR_BLOCKER,
             LW_XVE_BAR_BLOCKER_CTRL_BLOCKER_MODE_BLOCK_NOTHING |
             DRF_SHIFTMASK(LW_XVE_BAR_BLOCKER_CTRL_BLOCKER_MODE_OVERWRITE));

    //
    // Explicitly enable the XVE_BAR_BLOCKER GPIO_1_INTR interrupt after
    // disengaging the blocker. This function is called during aborts and
    // if the abort point that called this function was before we enabled
    // XVE_BAR_BLOCKER GPIO_1 then we would want to explicity enable it
    //
    REG_FLD_WR_DRF_DEF(CSB, _CPWR_PMU, _GPIO_1_INTR_RISING_EN,
               _XVE_BAR_BLOCKER, _ENABLED);
#endif
}

/*!
 * Flush L2 and poll until L2 is done
 *
 * @param[in]   ctrlId             LPWR_CTRL Id from MS Group
 * @param[in]   pBlockStartTimeNs  Start time of the block sequence
 * @param[out]  pTimeoutLeftNs     Time left in the block sequence
 */
FLCN_STATUS
msL2FlushAndIlwalidate_GMXXX
(
    LwU8            ctrlId,
    FLCN_TIMESTAMP *pBlockStartTimeNs,
    LwS32          *pTimeoutLeftNs
)
{
    //
    // 15) Section 4.5.1 in WSP IAS:
    // PMU writes a register in L2 to flush L2.
    //
    REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_G_ELPG,
                      FLD_SET_DRF(_PLTCG, _LTCS_LTSS_G_ELPG, _FLUSH, _PENDING,
                      REG_RD32(FECS, LW_PLTCG_LTCS_LTSS_G_ELPG)));

    //
    // 16.1) Section 4.5.1 in WSP IAS:
    // PMU polls a register in L2 to make sure L2 is flushed.
    // Bug 746537: During a broadcast read from LW_PLTCG_LTCS_LTSS_G_ELPG,
    // the HW will issue a read to each unicast address and OR the read data.
    //
    while (FLD_TEST_DRF(_PLTCG, _LTCS_LTSS_G_ELPG,
                        _FLUSH, _PENDING,
                        REG_RD32(FECS, LW_PLTCG_LTCS_LTSS_G_ELPG)))
    {
        if (pgCtrlIdleFlipped_HAL(&Pg, ctrlId) ||
            msCheckTimeout(MS_DEFAULT_ABORT_TIMEOUT1_NS,
                           pBlockStartTimeNs, pTimeoutLeftNs))
        {
            return FLCN_ERROR;
        }
    }

    return FLCN_OK;
}


#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
/*!
 * @brief Execute Entry Sequence for  Host Flush and Bind
 */
FLCN_STATUS
msHostFlushBindEntry_GMXXX
(
    LwU16  *pAbortReason
)
{
    LwU32 regVal = 0;

    // Suppress the host flush/bind
    REG_FLD_WR_DRF_DEF(BAR0, _PBUS, _FLUSH_BIND_CTL, _ENABLE, _FALSE);

    //
    // Read the host flush back to verify it is disabled. Abort if there are
    // any pending or outstanding flushes which causes the FLUSH to not get
    // disabled
    //
    regVal = REG_RD32(BAR0, LW_PBUS_FLUSH_BIND_CTL);
    if (FLD_TEST_DRF( _PBUS, _FLUSH_BIND_CTL, _ENABLE, _TRUE, regVal))
    {
        *pAbortReason = MS_ABORT_REASON_HOST_FLUSH_ENABLED;
        return FLCN_ERROR;
    }
    return FLCN_OK;
}

/*!
 * @brief Execute Exit Sequence for Host Flush and Bind
 */
void
msHostFlushBindExit_GMXXX(void)
{
    LwU32 regVal = 0;
    //
    // Disable flush/bind dispatch suppression in host.
    // In other words - enable host flush as part of unblock
    //
    regVal = REG_RD32(BAR0, LW_PBUS_FLUSH_BIND_CTL);
    if (FLD_TEST_DRF( _PBUS, _FLUSH_BIND_CTL, _ENABLE, _FALSE, regVal))
    {
        REG_FLD_WR_DRF_DEF(BAR0, _PBUS, _FLUSH_BIND_CTL, _ENABLE, _TRUE);
    }
}
#endif

/*!
 * @brief Toggle Priv Ring Access
 *
 * Helper function to Toggle Priv Ring Access. This is used when Priv Ring access
 * needs to be enabled / disabled during Ungating / Gating Clocks From Bypass Path.
 *
 * @param[in]   bDisable    LW_FALSE => Restore Priv Ring Access
 *                          LW_TRUE  => Disable Priv Ring Access
 *
 */
void
msDisablePrivAccess_GMXXX (LwBool bDisable)
{
    LwU32 val = 0;

    if (!bDisable)
    {
        // Restore FECS access to the priv_ring.
        REG_WR32(FECS, LW_PPRIV_SYS_PRI_GLOBAL_CONFIG,
                          Ms.pClkGatingRegs->priGlobalConfig);
        REG_WR32(FECS, LW_PPRIV_SYS_PRIV_DECODE_CONFIG,
                          Ms.pClkGatingRegs->privDecodeConfig);
    }
    else
    {
        // Setup FECS to reject any access to the priv_ring.
        Ms.pClkGatingRegs->privDecodeConfig =
            REG_RD32(FECS, LW_PPRIV_SYS_PRIV_DECODE_CONFIG);
        val = FLD_SET_DRF(_PPRIV, _SYS_PRIV_DECODE_CONFIG,
                          _RING, _ERROR_ON_RING_NOT_STARTED,
                          Ms.pClkGatingRegs->privDecodeConfig);
        REG_WR32(FECS, LW_PPRIV_SYS_PRIV_DECODE_CONFIG, val);
        Ms.pClkGatingRegs->priGlobalConfig =
            REG_RD32(FECS, LW_PPRIV_SYS_PRI_GLOBAL_CONFIG);
        val = FLD_SET_DRF(_PPRIV, _SYS_PRI_GLOBAL_CONFIG, _RING_ACCESS,
                          _DISABLED, Ms.pClkGatingRegs->priGlobalConfig);
        REG_WR32(FECS, LW_PPRIV_SYS_PRI_GLOBAL_CONFIG, val);
    }
}

/*!
 * @brief Initialize FB parameters
 */
void
msInitFbParams_GMXXX(void)
{
    OBJSWASR *pSwAsr = MS_GET_SWASR();
    LwU32     val;

    val = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_BROADCAST);

    //
    // Kepler supports GDDR5 memory types. Therefore we are
    // setting this variable if GDDR5 is present in system.
    //
    pSwAsr->bIsGDDRx = FLD_TEST_DRF(_PFB, _FBPA_FBIO_BROADCAST, _DDR_MODE,
                                    _GDDR5, val);

    pSwAsr->bIsGDDR3MirrorCmdMapping =
        (FLD_TEST_DRF(_PFB, _FBPA_FBIO_BROADCAST, _CMD_MAPPING,
                      _GDDR3_GT215_COMP_MIRR, val )             |
         FLD_TEST_DRF(_PFB, _FBPA_FBIO_BROADCAST, _CMD_MAPPING,
                      _GDDR3_GT215_COMP_MIRR1, val));
}

/*!
 * @brief Disable (mask) all the PRIV blocker interrupts to PMU
 */
void
msPrivBlockersIntrDisable_GMXXX
(
    LwBool bDisable
)
{
    if (bDisable)
    {
        // 1. XVE BAR blocker Interrupt disable
        REG_FLD_WR_DRF_DEF(CSB, _CPWR_PMU, _GPIO_1_INTR_RISING_EN,
                   _XVE_BAR_BLOCKER, _DISABLED);

        // 2. SEC2 priv blocker Interrupt disable
        if (PMUCFG_FEATURE_ENABLED(PMU_PG_SEC2))
        {
            msSec2PrivBlockerIntrDisable_HAL(&Ms);
        }
    }
    else
    {
        // 1. XVE BAR blocker Interrupt enable
        REG_FLD_WR_DRF_DEF(CSB, _CPWR_PMU, _GPIO_1_INTR_RISING_EN,
                _XVE_BAR_BLOCKER, _ENABLED);

        // 2. SEC2 priv blocker Interrupt enable
        if (PMUCFG_FEATURE_ENABLED(PMU_PG_SEC2))
        {
            msSec2PrivBlockerIntrEnable_HAL(&Ms);
        }
    }
}

/*!
 * @brief Check for various subsystem's idleness
 * which can cause the Memory traffic
 *
 * @param[in]      ctrlId - LPWR_CTRL Id
 * @param[in/out] pAbortReason pointer to rerurn the abort reason
 *
 * return LW_TRUE  -> Subsystems are idle
 *        LW_FALSE -> Subsystems are busy
 */
LwBool
msIsIdle_GMXXX
(
    LwU8   ctrlId,
    LwU16 *pAbortReason
)
{
    LwBool bIsIdle = LW_FALSE;

    // Abort if host interrupts are pending
    if (msIsHostIntrPending_HAL(&Ms))
    {
        *pAbortReason = MS_ABORT_REASON_HOST_INTR_PENDING;
    }
    // Abort if any wakeup event is pending
    else if (msWakeupPending())
    {
        *pAbortReason = MS_ABORT_REASON_WAKEUP_PENDING;
    }
    // Abort if idle status is flipped
    else if (pgCtrlIdleFlipped_HAL(&Pg, ctrlId))
    {
        *pAbortReason = MS_ABORT_REASON_FB_NOT_IDLE;
    }
    // Abort if SEC2 wakeup pending
    else if (PMUCFG_FEATURE_ENABLED(PMU_PG_SEC2_RTOS_WAR_200089154) &&
             msSec2WakeupPending_HAL(&Ms))
    {
        *pAbortReason = MS_ABORT_REASON_SEC2_WAKEUP_PENDING;
    }
    else
    {
        bIsIdle = LW_TRUE;
    }

    return bIsIdle;
}

/*!
 * @brief Check for various subsystem's idleness
 * which can cause the Memory traffic
 *
 * @param[in/out] pAbortReason   pointer to rerurn the abort reason
 * @param[in]     pStartTimeNs   pointer to start time of flush flush sequence
 * @param[in/out] pTimeoutLeftNs pointer to rerurn remaining time
 *
 * return LW_TRUE  -> Flush is complete
 *        LW_FALSE -> Flush is not complete
 */
LwBool
msPrivPathFlush_GMXXX
(
    LwU16          *pAbortReason,
    FLCN_TIMESTAMP *pStartTimeNs,
    LwS32          *pTimeoutLeftNs
)
{
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    //
    // 2) Section 4.5.1 in WSP IAS:
    // PMU waits for XVE BAR blocker to drain the path to host arbiter.
    // This is done by sending a downstream posted write to
    // WSP_GP_MSGBOX register in pmu.
    // Abort immediately if flipped bit is asserted during polling.
    //
    REG_WR32(CSB, LW_CPWR_PMU_WSP_GP_MSGBOX, 0);

    REG_WR32(FECS_CFG, LW_XVE_BAR_BLOCKER_FLUSH_WR_ADDR,
             LW_PPWR_PMU_WSP_GP_MSGBOX);
    REG_WR32(FECS_CFG, LW_XVE_BAR_BLOCKER_FLUSH_WR_DATA,
             PMU_LPWR_PRIV_PATH_FLUSH_VAL);
    REG_WR32(FECS_CFG, LW_XVE_BAR_BLOCKER_FLUSH_CTRL,
             LW_XVE_BAR_BLOCKER_FLUSH_CTRL_WR_CMD_TRIGGER);

    if (msCheckTimeout(Ms.abortTimeoutNs, pStartTimeNs, pTimeoutLeftNs) ||
        !PMU_REG32_POLL_NS(LW_CPWR_PMU_WSP_GP_MSGBOX,
                           DRF_SHIFTMASK(LW_CPWR_PMU_WSP_GP_MSGBOX_VAL),
                           PMU_LPWR_PRIV_PATH_FLUSH_VAL,
                           (LwU32) *pTimeoutLeftNs,
                           PMU_REG_POLL_MODE_CSB_EQ)                         ||
                           pgCtrlIdleFlipped_HAL(&Pg, RM_PMU_LPWR_CTRL_ID_MS_MSCG))
    {
        *pAbortReason = MS_ABORT_REASON_POLLING_MSGBOX_TIMEOUT;
        return LW_FALSE;
    }

    //
    // 3) Section 4.5.1 in WSP IAS:
    // PMU will read the LW_XVE_DBG0 register to ensure no
    // outstanding non-posted read transactions.
    // (LW_XVE_DBG0_OUTSTANDING_DOWNSTREAM_READ==0)
    //
    if (!FLD_TEST_DRF_NUM(_XVE, _DBG0, _OUTSTANDING_DOWNSTREAM_READ,
                          0, REG_RD32(FECS_CFG, LW_XVE_DBG0)))
    {
        *pAbortReason = MS_ABORT_REASON_NPREAD_TIMEOUT;
        return LW_FALSE;
    }

    //
    // 4) Section 4.5.1 in WSP IAS:
    // PMU will flush the PRIV bus by issuing three FECS PRI_FENCE reads
    // (GPC, FBP, SYS). No order requirements here.
    //
    if (msCheckTimeout(Ms.abortTimeoutNs, pStartTimeNs, pTimeoutLeftNs) ||
        !PMU_REG32_POLL_NS(USE_FECS(LW_PPRIV_FBP_PRI_FENCE),
                           DRF_SHIFTMASK(LW_PPRIV_FBP_PRI_FENCE_V),
                           LW_PPRIV_FBP_PRI_FENCE_V_0,
                           (LwU32) *pTimeoutLeftNs,
                           PMU_REG_POLL_MODE_BAR0_EQ))
    {
        *pAbortReason = MS_ABORT_REASON_FBP_FENCE_TIMEOUT;
        return LW_FALSE;
    }

    if (msCheckTimeout(Ms.abortTimeoutNs, pStartTimeNs, pTimeoutLeftNs) ||
        !PMU_REG32_POLL_NS(USE_FECS(LW_PPRIV_GPC_PRI_FENCE),
                           DRF_SHIFTMASK(LW_PPRIV_GPC_PRI_FENCE_V),
                           LW_PPRIV_GPC_PRI_FENCE_V_0,
                           (LwU32) *pTimeoutLeftNs,
                           PMU_REG_POLL_MODE_BAR0_EQ))
    {
        *pAbortReason = MS_ABORT_REASON_GPC_FENCE_TIMEOUT;
        return LW_FALSE;
    }

    if (msCheckTimeout(Ms.abortTimeoutNs, pStartTimeNs, pTimeoutLeftNs) ||
        !PMU_REG32_POLL_NS(USE_FECS(LW_PPRIV_SYS_PRI_FENCE),
                                    DRF_SHIFTMASK(LW_PPRIV_SYS_PRI_FENCE_V),
                                    LW_PPRIV_SYS_PRI_FENCE_V_0,
                                    (LwU32) *pTimeoutLeftNs,
                                    PMU_REG_POLL_MODE_BAR0_EQ))
    {
        *pAbortReason = MS_ABORT_REASON_SYS_FENCE_TIMEOUT;
        return LW_FALSE;
    }
#endif // (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))

    return LW_TRUE;
}

/*!
 * @brief Execute the Priv Blocker Entry Sequence
 *
 * Steps:
 * 1. Disable the Priv Blocker interrupt.
 * 2. Engage the XVE BAR Blocker in BLOCK_ALL mode.
 * 3. Issue the XVE PRI path Flush.
 * 4. Check for P2P Traffic.
 * 5. Move the Blocker to BLOCK_EQ mode.
 * 6. Re enable the Priv Blocker interrupts.
 *
 * @param[in]  ctrlId            LPWR_CTRL Id engaging the blocker
 * @param[in]  pBlockStartTimeNs the start time for the sequence
 * @param[out] pTimeoutLeftNs    timeout left
 * @param[out] pAbortReasons     AbortReason
 *
 * @return LW_TRUE      if Blockers are engaged
 *         LW_FALSE     otherwise
 */
LwBool
msPrivBlockerEntry_GMXXX
(
    LwU8            ctrlId,
    FLCN_TIMESTAMP *pBlockStartTimeNs,
    LwS32          *pTimeoutLeftNs,
    LwU16          *pAbortReason
)
{
    //
    // Disable priv blocker interrupts. When in BLOCK_EVERTHING mode even the
    // Allowlist accesses can trigger the priv blocker interrupts and we don't
    // want those interrupts to cause aborts while we are exelwting the MS
    // entry sequence.
    //
    msPrivBlockersIntrDisable_HAL(&Ms, LW_TRUE);

    // Engage the blocker into Block ALL mode
    if (!msPrivBlockerEngage_HAL(&Ms, LW_FALSE, pBlockStartTimeNs,
                                 pTimeoutLeftNs))
    {
        *pAbortReason = MS_ABORT_REASON_BLK_EVERYTHING_TIMEOUT;
        return LW_FALSE;
    }

    // Issue the flush
    if (!msPrivPathFlush_HAL(&Ms, pAbortReason, pBlockStartTimeNs,
                                pTimeoutLeftNs))
    {
        return LW_FALSE;
    }

    // Check the Idleness after the flush
    if (!msIsIdle_HAL(&Ms, ctrlId, pAbortReason))
    {
        return LW_FALSE;
    }

#if (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
    //
    // PMU will need to read the LW_XVE_DBG0 register to ensure no
    // outstanding P2P requests in the XVE P2P FIFO
    // (LW_XVE_DBG0_FB_IDLE==1).
    //
    if (FLD_TEST_DRF_NUM(_XVE, _DBG0, _FB_IDLE, 0,
                          REG_RD32(FECS_CFG, LW_XVE_DBG0)))
    {
        *pAbortReason = MS_ABORT_REASON_P2P_TIMEOUT;
        return LW_FALSE;
    }
#endif

    // Engage the blocker into Block Equation mode
    if (!msPrivBlockerEngage_HAL(&Ms, LW_TRUE, pBlockStartTimeNs,
                                 pTimeoutLeftNs))
    {
        *pAbortReason = MS_ABORT_REASON_BLK_EQUATION_TIMEOUT;
        return LW_FALSE;
    }

    //
    // Enable priv blocker interrupts. At this point we are in block equation
    // mode. Only Denylist access can trigger the priv blockers from this
    // point onwards. Enable the interrupts so that PMU ucode can see them.
    //
    msPrivBlockersIntrDisable_HAL(&Ms, LW_FALSE);

    return LW_TRUE;
}

/* ------------------------ Private Functions ------------------------------- */
