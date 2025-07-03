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
 * @file   pmu_msgpxxxgvxx.c
 * @brief  HAL-interface for the PASCAL and VOLTA Memory System Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "hwproject.h"
#include "dev_lw_xve.h"
#include "dev_lw_xve1.h"
#include "dev_lw_xp.h"
#include "dev_master.h"
#include "dev_bus.h"
#include "dev_fifo.h"
#include "dev_timer.h"
#include "dev_pbdma.h"
#include "dev_top.h"
#include "dev_disp.h"
#include "dev_perf.h"
#include "dev_hdafalcon_pri.h"
#include "dev_hdacodec.h"
#include "dev_pwr_pri.h"
#include "dev_fuse.h"
#include "dev_pmgr.h"
#include "mscg_wl_bl_init.h"
#include "dev_sec_pri.h"
#include "dev_pwr_csb.h"
#include "dev_top.h"
#include "dev_trim.h"
#include "dev_sec_pri.h"
#include "dev_fbpa.h"
#include "dev_therm.h"
#include "dev_flush.h"
#include "dev_ram.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objlpwr.h"
#include "pmu_objms.h"
#include "pmu_objpmu.h"
#include "pmu_objfifo.h"
#include "pmu_objic.h"
#include "pmu_bar0.h"

#include "dbgprintf.h"
#include "config/g_ms_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* --------------------------- Prototypes ------------------------------------*/
static void s_msProgramSec2PrivBlockerBlRanges_GP10X(void)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "s_msProgramSec2PrivBlockerBlRanges_GP10X");
static void s_msProgramSec2PrivBlockerWlRanges_GP10X(void)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "_msProgramSec2PrivWlockerBlRanges_GP10X");

static LwBool s_msSec2PrivFbBlockersEngage_GP10X(LwBool, FLCN_TIMESTAMP *, LwS32 *);
static LwBool s_msSec2BlockersEngagedPoll_GP10X(FLCN_TIMESTAMP *, LwS32 *);

/* ------------------------ Public Functions  ------------------------------- */

/*!
 * @brief Initializes the idle mask for MS engine.
 */
void
msMscgIdleMaskInit_GP10X(void)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
    LwU32       im0      = 0;
    LwU32       im1      = 0;

    if (FIFO_IS_ENGINE_PRESENT(GR))
    {
        im0 = (FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR, _ENABLED, im0)     |
                FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR_SYS, _ENABLED, im0) |
                FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR_GPC, _ENABLED, im0) |
                FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR_BE, _ENABLED, im0));

        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _GR_INTR_NOSTALL,
                            _ENABLED, im1);
    }
    if (FIFO_IS_ENGINE_PRESENT(CE0))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_0, _ENABLED, im0);

        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE0_INTR_NOSTALL,
                            _ENABLED, im1);
    }
    if (FIFO_IS_ENGINE_PRESENT(CE1))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_1, _ENABLED, im0);

        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE1_INTR_NOSTALL,
                            _ENABLED, im1);
    }
    if (FIFO_IS_ENGINE_PRESENT(CE2))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_2, _ENABLED, im0);

        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE2_INTR_NOSTALL,
                          _ENABLED, im1);
    }
    if (FIFO_IS_ENGINE_PRESENT(CE3))
    {
        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_3, _ENABLED, im1);

        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE3_INTR_NOSTALL,
                          _ENABLED, im1);
    }
    //  LWDEC = BSP  (reusing MSVLD code for LWDEC)
    if (FIFO_IS_ENGINE_PRESENT(BSP))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWDEC, _ENABLED, im0);

        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _VD_LWDEC_INTR_NOSTALL,
                          _ENABLED, im1);
    }

    if (FIFO_IS_ENGINE_PRESENT(ENC0))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWENC0, _ENABLED, im0);

        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _VD_LWENC0_INTR_NOSTALL,
                          _ENABLED, im1);
    }
    if (FIFO_IS_ENGINE_PRESENT(ENC1))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWENC1, _ENABLED, im0);

        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _VD_LWENC1_INTR_NOSTALL,
                          _ENABLED, im1);
    }
    if (FIFO_IS_ENGINE_PRESENT(SEC2))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _SEC, _ENABLED, im0);

        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _SEC_INTR_NOSTALL,
                          _ENABLED, im1);

        if (PMUCFG_FEATURE_ENABLED(PMU_PG_SEC2) &&
            LPWR_CTRL_IS_SF_SUPPORTED(pPgState, MS, SEC2))
        {
            //
            // If RTOS is supported, additionally use the SEC2 BAR0 interface
            // in the idle mask. We won't use the SEC2 FBIF interface in the
            // mask since there are glitches on that signal while engaging and
            // disengaging the blockers (bug 200113462). This is okay because
            // we will depend on HUB/niso_all_but_iso idle signal to detect
            // FB traffic.
            //
            im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _SEC_BAR0,
                              _ENABLED, im1);

            if (PMUCFG_FEATURE_ENABLED(PMU_PG_SEC2_RTOS_WAR_200089154))
            {
                im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _SW1, _ENABLED, im1);
            }
        }
    }

    im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _MS_NISO_EX_ISO, _ENABLED, im1);

    // Host Idlness signals
    im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _HOST_NOT_QUIESCENT, _ENABLED, im0);
    im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _HOST_INT_NOT_ACTIVE, _ENABLED, im0);

    pPgState->idleMask[0] = im0;
    pPgState->idleMask[1] = im1;
}

/*!
 * @brief Enable MISCIO MSCG interrutps
 *
 * Function to enable the various MISCIO interrupts The display stutter and
 * display blocker interrupts should only be enabled if display is enabled.
 */
void
msInitAndEnableIntr_GP10X(void)
{
    LwU32 val = 0;

    val = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_RISING_EN,
                      _HOST_FB_BLOCKER, _ENABLED, val);
    val = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_RISING_EN,
                       _XVE_BAR_BLOCKER, _ENABLED, val);

    if (lpwrGrpCtrlIsSfSupported(RM_PMU_LPWR_GRP_CTRL_ID_MS,
                                 RM_PMU_LPWR_CTRL_FEATURE_ID_MS_PERF))
    {
        val = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_RISING_EN,
                          _PERF_FB_BLOCKER, _ENABLED, val);
    }

    if (lpwrGrpCtrlIsSfSupported(RM_PMU_LPWR_GRP_CTRL_ID_MS,
                                 RM_PMU_LPWR_CTRL_FEATURE_ID_MS_DISPLAY))
    {
        val = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_RISING_EN,
                          _DNISO_FB_BLOCKER, _ENABLED, val);

        if (lpwrGrpCtrlIsSfSupported(RM_PMU_LPWR_GRP_CTRL_ID_MS,
                                     RM_PMU_LPWR_CTRL_FEATURE_ID_MS_ISO_STUTTER))
        {
            val = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_RISING_EN,
                              _MSPG_WAKE, _ENABLED, val);
            val = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_RISING_EN,
                              _ISO_FB_BLOCKER, _ENABLED, val);
        }
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PG_SEC2) &&
        lpwrGrpCtrlIsSfSupported(RM_PMU_LPWR_GRP_CTRL_ID_MS,
                                 RM_PMU_LPWR_CTRL_FEATURE_ID_MS_SEC2))
    {
        val = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_RISING_EN,
                          _SEC_FB_BLOCKER, _ENABLED, val);

        val = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_RISING_EN,
                          _SEC_PRIV_BLOCKER, _ENABLED, val);
    }

    Ms.intrRiseEnMask |= val;

    REG_WR32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN, val);
}

/*!
 * @brief Colwert HW interrupts to wakeup Mask
 *
 * This function colwerts miscellaneous interrupts to wakeup mask.
 *
 * @param[in]  intrStat
 *
 * @return Wake-up Mask
 */
LwU32
msColwertIntrsToWakeUpMask_GP10X
(
    LwU32 intrStat
)
{
    LwU32 wakeUpMask = 0;

    if (PENDING_IO_BANK1(_XVE_BAR_BLOCKER , _RISING, intrStat))
    {
        wakeUpMask |= PG_WAKEUP_EVENT_XVE_PRIV_BLOCKER;
    }
    if (PENDING_IO_BANK1(_HOST_FB_BLOCKER , _RISING, intrStat))
    {
        wakeUpMask |= PG_WAKEUP_EVENT_MS_HOST_BLOCKER;
    }
    if (PENDING_IO_BANK1(_PERF_FB_BLOCKER , _RISING, intrStat))
    {
        wakeUpMask |= PG_WAKEUP_EVENT_MS_PM_BLOCKER;
    }
    if (PENDING_IO_BANK1(_ISO_FB_BLOCKER  , _RISING, intrStat))
    {
        wakeUpMask |= PG_WAKEUP_EVENT_MS_ISO_BLOCKER;
    }
    if (PENDING_IO_BANK1(_DNISO_FB_BLOCKER, _RISING, intrStat))
    {
        wakeUpMask |= PG_WAKEUP_EVENT_MS_DISPLAY_BLOCKER;
    }
    if (PENDING_IO_BANK1(_MSPG_WAKE,        _RISING, intrStat))
    {
        wakeUpMask |= PG_WAKEUP_EVENT_MS_ISO_STUTTER;
    }
    if (PMUCFG_FEATURE_ENABLED(PMU_PG_SEC2))
    {
        if (PENDING_IO_BANK1(_SEC_FB_BLOCKER,   _RISING, intrStat))
        {
            wakeUpMask |= PG_WAKEUP_EVENT_SEC2_FB_BLOCKER;
        }
        if (PENDING_IO_BANK1(_SEC_PRIV_BLOCKER, _RISING, intrStat))
        {
            wakeUpMask |= PG_WAKEUP_EVENT_SEC2_PRIV_BLOCKER;
        }
    }

    return wakeUpMask;
}

/*!
 * @brief Check if any non-stalling interrupt is pending on each engine.
 *
 * @return   LW_TRUE  if a interrupt is pending on any engine
 * @return   LW_FALSE otherwise
 */
LwBool
msIsEngineIntrPending_GP10X(void)
{
    LwBool      bIntrPending = LW_FALSE;
    LwU32       val          = 0;

    val = REG_RD32(BAR0, LW_PMC_INTR(1));
    bIntrPending = (DRF_VAL(_PMC, _INTR, _PGRAPH, val) ||
                    DRF_VAL(_PMC, _INTR, _LWDEC,  val) ||
                    DRF_VAL(_PMC, _INTR, _SEC,    val) ||
                    DRF_VAL(_PMC, _INTR, _CE0,    val) ||
                    DRF_VAL(_PMC, _INTR, _CE1,    val) ||
                    DRF_VAL(_PMC, _INTR, _CE2   , val) ||
                    DRF_VAL(_PMC, _INTR, _CE3   , val) ||
                    DRF_VAL(_PMC, _INTR, _LWENC,  val) ||
                    DRF_VAL(_PMC, _INTR, _LWENC1, val));

    return bIntrPending;
}

/*!
 * @brief Program priv blocker Disallow ranges for MS
 */
void
msProgramPrivBlockerDisallowRanges_GP10X
(
    LwU32 blockerInitMask
)
{
    OBJPGSTATE *pMsState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);

    if (PMUCFG_FEATURE_ENABLED(PMU_PG_SEC2)                &&
        pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_MSCG)     &&
        LPWR_CTRL_IS_SF_SUPPORTED(pMsState, MS, SEC2) &&
        ((blockerInitMask & BIT(RM_PMU_PRIV_BLOCKER_ID_SEC2)) != 0))
    {
        s_msProgramSec2PrivBlockerBlRanges_GP10X();
    }
}

/*!
 * @brief Program priv blocker Allow ranges for MS
 */
void
msProgramPrivBlockerAllowRanges_GP10X
(
    LwU32 blockerInitMask
)
{
    OBJPGSTATE *pMsState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);

    if (PMUCFG_FEATURE_ENABLED(PMU_PG_SEC2)           &&
        pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_MSCG)     &&
        LPWR_CTRL_IS_SF_SUPPORTED(pMsState, MS, SEC2) &&
        ((blockerInitMask & BIT(RM_PMU_PRIV_BLOCKER_ID_SEC2)) != 0))
    {
        s_msProgramSec2PrivBlockerWlRanges_GP10X();
    }
}

/*!
 * @brief Resets IDLE_FLIPPED bit if FB traffic generated is only from PMU
 *
 * This function resets the IDLE_FLIPPED bit if the FB traffic is from PMU due
 * to loading tasks/overlays. This will avoid aborting the first MSCG cycle
 * and improve residency.
 */
FLCN_STATUS
msIdleFlippedReset_GP10X
(
    LwU8    ctrlId
)
{
    LwU32       pgStat;
    LwU32       hwEngIdx = PG_GET_ENG_IDX(ctrlId);
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);

    if (!PMUCFG_FEATURE_ENABLED(PMU_PG_IDLE_FLIPPED_RESET) ||
        !LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, IDLE_FLIPPED_RESET))
    {
        return FLCN_OK;
    }

    //
    // Reset IDLE_FLIPPED bit if
    // - If IDLE_FLIPPED set only due to PMU AND
    // - Non LPWR tasks are idle
    //
    pgStat = REG_RD32(CSB, LW_CPWR_PMU_PG_STAT(hwEngIdx));
    if (FLD_TEST_DRF(_CPWR_PMU, _PG_STAT, _IDLE_FLIPPED, _ASSERTED, pgStat) &&
        FLD_TEST_DRF(_CPWR_PMU, _PG_STAT, _IDLE_FLIPPED_NON_PMU, _IDLE, pgStat))
    {
        if (!PMUCFG_FEATURE_ENABLED(PMU_PG_TASK_MGMT) ||
            lpwrIsNonLpwrTaskIdle())
        {
            pgStat = FLD_SET_DRF(_CPWR_PMU, _PG_STAT, _IDLE_FLIPPED, _DEASSERTED, pgStat);
            REG_WR32(CSB, LW_CPWR_PMU_PG_STAT(hwEngIdx), pgStat);
        }
    }

    return FLCN_OK;
}

/*!
 * Check if host interrupt is pending
 *
 * @return   LW_TRUE  if a host interrupt is pending
 * @return   LW_FALSE otherwise
 */
LwBool
msIsHostIntrPending_GP10X(void)
{
    // for Pascal LW_PMC_INTR__SIZE_1 == 2
    if (REG_FLD_IDX_TEST_DRF_DEF(BAR0, _PMC, _INTR, 0, _PFIFO, _PENDING) ||
        REG_FLD_IDX_TEST_DRF_DEF(BAR0, _PMC, _INTR, 1, _PFIFO, _PENDING))
    {
        return LW_TRUE;
    }
    return LW_FALSE;
}

/*!
 * @brief Program and enable Allow Ranges
 *
 * Allow Range allows us to Allowlist a register or a range which has been
 * mistakenly Denylisted in the XVE blocker by HW. This is essentially a CYA.
 */
void
msProgramAllowRanges_GP10X(void)
{
    OBJPGSTATE *pMsState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
    LwU32       val;

    if (PMUCFG_FEATURE_ENABLED(PMU_PG_SEC2) &&
        LPWR_CTRL_IS_SF_SUPPORTED(pMsState, MS, SEC2))
    {
        //
        // When SEC2 RTOS is supported in the MSCG sequence, add LW_PSEC
        // registers XVE BAR blocker Allowlist. SEC2 RTOS will make us of
        // dedicated FB and priv blockers in order to wake us up from MSCG. By
        // Denylisting them, we would end up waking MSCG unnecessarily when,
        // for example, a command is posted to SEC2, that doesn't result in
        // accesses to memory or other clock gated domains.
        //

        REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_ALLOW_RANGE_START_ADDR(0),
                           DEVICE_BASE(LW_PSEC));
        REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_ALLOW_RANGE_END_ADDR(0),
                           DEVICE_EXTENT(LW_PSEC));

        val = REG_RD32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA);
        val = FLD_SET_DRF(_XVE, _BAR_BLOCKER_CYA, _ALLOW_RANGE0, _ENABLED, val);
        REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA, val);
    }
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
msPrivBlockerEngage_GP10X
(
    LwBool          bEquationMode,
    FLCN_TIMESTAMP *pBlockStartTimeNs,
    LwS32          *pTimeoutLeftNs
)
{
    OBJPGSTATE *pMsState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
    LwU32       val      = 0;

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
    // 2. SEC2 priv blocker (if present). Note that this also engages the SEC2
    // FB blocker, since we can engage them atomically
    //
    //
    // SW WAR for SEC2 RTOS (bug 200113462)
    //
    // To engage the SEC2 Priv and FB Blocker we need to follow the below sequence
    //
    // For Block Everything Mode:
    //
    // 1. First engage the Method holdoff for SEC2.
    // 2. Poll for the holdoff to engage otherwise abort MSCG sequence.
    // 3. Then engage the SEC2 Priv blocker (in BLOCK_EVERYTHING mode) and FB Blocker
    // 4. Poll for the blockers to get engaged.
    //
    // For Block Equation Mode:
    // 1. While we are engaging the Blockers in Block Equation mode, SEC2 holdoff
    // is already engaged (during the BLOCK Everything Mode).
    // 2. Just move the Priv Blocker to Block equation mode.
    // 3. Poll for the blockers to get engaged.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PG_SEC2) &&
        LPWR_CTRL_IS_SF_ENABLED(pMsState, MS, SEC2))
    {
        // Engage the SEC2 Priv and FB Blocker.
        if (!s_msSec2PrivFbBlockersEngage_GP10X(bEquationMode,
                                                pBlockStartTimeNs,
                                                pTimeoutLeftNs))
        {
            return LW_FALSE;
        }

        // Poll for both blockers to get engaged.
        if (!s_msSec2BlockersEngagedPoll_GP10X(pBlockStartTimeNs,
                                               pTimeoutLeftNs))
        {
            return LW_FALSE;
        }
    }
    //
    // 3. We only poll to make sure that the XVE BAR blocker engaged
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

    return LW_TRUE;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Program SEC2 priv blocker Allow ranges
 */
static void
s_msProgramSec2PrivBlockerWlRanges_GP10X(void)
{
    LwU32 val = 0;

    //
    // Program the Allowed ranges starting from RANGE0_START using
    // autoincrement on write
    //
    val = FLD_SET_DRF_NUM(_PSEC, _BLOCKER_BAR0_RANGEC, _OFFS, 0, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_RANGEC, _BLK, _WL, val)    |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_RANGEC, _AINCW, _TRUE, val);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGEC, val);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_START_0);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_END_0);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_START_1);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_END_1);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_START_2);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_END_2);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_START_3);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_END_3);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_START_4);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_END_4);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_START_5);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_END_5);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_START_6);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_END_6);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_START_7);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_END_7);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_START_8);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_END_8);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_START_9);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_END_9);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_START_10);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_END_10);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_START_11);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_END_11);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_START_12);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_END_12);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_START_13);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_END_13);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_START_14);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_END_14);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_START_15);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_END_15);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_START_16);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_END_16);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_START_17);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_END_17);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_START_18);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_WL_RANGE_END_18);

    // Enable the ranges programmed
    val = 0;
    val = FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_WL_CFG, _RANGE0,  _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_WL_CFG, _RANGE1,  _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_WL_CFG, _RANGE2,  _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_WL_CFG, _RANGE3,  _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_WL_CFG, _RANGE4,  _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_WL_CFG, _RANGE5,  _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_WL_CFG, _RANGE6,  _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_WL_CFG, _RANGE7,  _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_WL_CFG, _RANGE8,  _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_WL_CFG, _RANGE9,  _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_WL_CFG, _RANGE10, _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_WL_CFG, _RANGE11, _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_WL_CFG, _RANGE12, _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_WL_CFG, _RANGE13, _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_WL_CFG, _RANGE14, _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_WL_CFG, _RANGE15, _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_WL_CFG, _RANGE16, _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_WL_CFG, _RANGE17, _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_WL_CFG, _RANGE18, _ENABLED, val);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_WL_CFG, val);
}

/*!
 * @brief Program SEC2 priv blocker Disallow ranges
 */
static void
s_msProgramSec2PrivBlockerBlRanges_GP10X(void)
{
    LwU32 val = 0;

    //
    // Program the Disallow ranges starting from RANGE0_START using
    // autoincrement on write
    //
    val = FLD_SET_DRF_NUM(_PSEC, _BLOCKER_BAR0_RANGEC, _OFFS, 0, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_RANGEC, _BLK, _BL, val)    |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_RANGEC, _AINCW, _TRUE, val);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGEC, val);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_START_0);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_END_0);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_START_1);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_END_1);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_START_2);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_END_2);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_START_3);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_END_3);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_START_4);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_END_4);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_START_5);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_END_5);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_START_6);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_END_6);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_START_7);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_END_7);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_START_8);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_END_8);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_START_9);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_END_9);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_START_10);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_END_10);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_START_11);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_END_11);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_START_12);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_END_12);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_START_13);
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_RANGED, SEC_BL_RANGE_END_13);

    // Enable the ranges programmed
    val = 0;
    val = FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_BL_CFG, _RANGE0,  _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_BL_CFG, _RANGE1,  _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_BL_CFG, _RANGE2,  _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_BL_CFG, _RANGE3,  _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_BL_CFG, _RANGE4,  _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_BL_CFG, _RANGE5,  _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_BL_CFG, _RANGE6,  _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_BL_CFG, _RANGE7,  _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_BL_CFG, _RANGE8,  _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_BL_CFG, _RANGE9,  _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_BL_CFG, _RANGE10, _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_BL_CFG, _RANGE11, _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_BL_CFG, _RANGE12, _ENABLED, val) |
          FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_BL_CFG, _RANGE13, _ENABLED, val);

    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_BL_CFG, val);
}

/*!
 * @brief Program and enable Disallow Ranges
 *
 * Disallow Range allows us to Disallow a register or a range which has been
 * mistakenly allowed in the XVE blocker by HW. This is essentially a CYA.
 *
 * Disallowing register range is two step process -
 * 1) Add start and end address in DISALLOW_RANGE_START_ADDR_ADDR(i) and
 *    DISALLOW_RANGE_END_ADDR_ADDR(i)
 * 2) Enable DISALLOW_RANGEi
 */
void
msProgramDisallowRanges_GP10X(void)
{
    LwU32       barBlockerVal;
    LwU8        gpioMutexPhyId = MUTEX_LOG_TO_PHY_ID(LW_MUTEX_ID_GPIO);
    OBJPGSTATE *pMsState       = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);

    barBlockerVal = REG_RD32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA);

    //
    // In the true sense LW_PPWR_PMU_MUTEX(gpioMutexPhyId) is not really
    // a Disallow register for MSCG. We have disallowed GPIO Mutex register
    // so that any RM access to GPIO will disengage PSI by waking MSCG.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_MS) &&
        LPWR_CTRL_IS_SF_SUPPORTED(pMsState, MS, PSI))
    {
        // Add register in CYA_DISALLOW_RANGE0
        REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_START_ADDR(0),
                 LW_PPWR_PMU_MUTEX(gpioMutexPhyId));
        REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_END_ADDR(0),
                 LW_PPWR_PMU_MUTEX(gpioMutexPhyId));

        // Enable DISALLOW_RANGE0
        barBlockerVal = FLD_SET_DRF(_XVE, _BAR_BLOCKER_CYA, _DISALLOW_RANGE0,
                                    _ENABLED, barBlockerVal);
    }

    //
    // All the below BL ranges are derived from the Bug: 200354284 comment #10.
    // All these ranges are provided by HOST HW Folks.
    //
    // Add register LW_PFIFO_ERROR_SCHED_DISABLE, LW_PFIFO_ERROR_SCHED_DISABLE,
    // LW_PFIFO_RUNLIST_PREEMPT, in CYA_DISALLOW_RANGE1
    //
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_START_ADDR(1),
             LW_PFIFO_ERROR_SCHED_DISABLE);
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_END_ADDR(1),
             LW_PFIFO_RUNLIST_PREEMPT);

    // Enable DISALLOW_RANGE1
    barBlockerVal = FLD_SET_DRF(_XVE, _BAR_BLOCKER_CYA, _DISALLOW_RANGE1,
                                _ENABLED, barBlockerVal);

    // Add register space LW_PCCSR in CYA_DISALLOW_RANGE2
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_START_ADDR(2),
             DEVICE_BASE(LW_PCCSR));
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_END_ADDR(2),
             DEVICE_EXTENT(LW_PCCSR));

    // Enable DISALLOW_RANGE2
    barBlockerVal = FLD_SET_DRF(_XVE, _BAR_BLOCKER_CYA, _DISALLOW_RANGE2,
                                _ENABLED, barBlockerVal);

    // Add register range LW_UFLUSH in CYA_DISALLOW_RANGE4
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_START_ADDR(3),
             DEVICE_BASE(LW_UFLUSH));
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_END_ADDR(3),
             DEVICE_EXTENT(LW_UFLUSH));

    // Enable DISALLOW_RANGE3
    barBlockerVal = FLD_SET_DRF(_XVE, _BAR_BLOCKER_CYA, _DISALLOW_RANGE3,
                                _ENABLED, barBlockerVal);

    // Add register range LW_PRAMIN in CYA_DISALLOW_RANGE5
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_START_ADDR(4),
             DEVICE_BASE(LW_PRAMIN));
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_END_ADDR(4),
             DEVICE_EXTENT(LW_PRAMIN));

    // Enable DISALLOW_RANGE4
    barBlockerVal = FLD_SET_DRF(_XVE, _BAR_BLOCKER_CYA, _DISALLOW_RANGE4,
                                _ENABLED, barBlockerVal);

    // Add register LW_PFIFO_FB_IFACE in CYA_DISALLOW_RANGE5
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_START_ADDR(5),
             LW_PFIFO_FB_IFACE);
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_END_ADDR(5),
             LW_PFIFO_FB_IFACE);

    // Enable DISALLOW_RANGE5
    barBlockerVal = FLD_SET_DRF(_XVE, _BAR_BLOCKER_CYA, _DISALLOW_RANGE5,
                                _ENABLED, barBlockerVal);

    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA, barBlockerVal);
}

/*!
 * @brief Poll to make sure that the SEC2 priv and FB blockers engaged
 *
 * @param[in]  pBlockStartTimeNs  start time for the block sequence
 * @param[out] pBlockStartTimeNs  timeout left
 */
static LwBool
s_msSec2BlockersEngagedPoll_GP10X
(
    FLCN_TIMESTAMP *pBlockStartTimeNs,
    LwS32          *pTimeoutLeftNs
)
{
    // Poll to make sure that the SEC2 FB and priv blockers engaged
    if (msCheckTimeout(Ms.abortTimeoutNs, pBlockStartTimeNs, pTimeoutLeftNs) ||
        !PMU_REG32_POLL_NS(LW_PSEC_FALCON_ENGCTL,
                           DRF_SHIFTMASK(LW_PSEC_FALCON_ENGCTL_STALLACK),
                           DRF_DEF(_PSEC, _FALCON_ENGCTL, _STALLACK, _TRUE),
                           (LwU32) (*pTimeoutLeftNs),
                           PMU_REG_POLL_MODE_BAR0_EQ)                        ||
        pgCtrlIdleFlipped_HAL(&Pg, RM_PMU_LPWR_CTRL_ID_MS_MSCG))
    {
        return LW_FALSE;
    }

    return LW_TRUE;
}

/*!
 * @brief Engage both the SEC2 priv and FB blockers atomically.
 *
 * @param[in] bEquationMode LW_FALSE => Engage the priv blocker in
 *                                      BLOCK_EVERYTHING mode
 *                          LW_TRUE  => Engage the priv blocker in
 *                                      BLOCK_EQUATION mode
 *
 * @return LW_TRUE  => all the priv blockers engaged successfully
 *         LW_FALSE => one or more priv blockers didn't engage successfully, so
 *                     we should abort the entry
 */
static LwBool
s_msSec2PrivFbBlockersEngage_GP10X
(
    LwBool          bEquationMode,
    FLCN_TIMESTAMP *pBlockStartTimeNs,
    LwS32          *pTimeoutLeftNs
)
{
    LwU32 val = 0;

    // First Enagage the SEC2 Holdoff for Bug: 200113462 for Block All Mode
    if (!bEquationMode)
    {
        if (FLCN_OK != lpwrHoldoffMaskSet_HAL(&Lpwr,
                                              LPWR_HOLDOFF_CLIENT_ID_SEC2_BLK_WAR,
                                              BIT(GET_FIFO_ENG(PMU_ENGINE_SEC2))))
        {
            return LW_FALSE;
        }

        // Check for timeout
        if (msCheckTimeout(Ms.abortTimeoutNs, pBlockStartTimeNs, pTimeoutLeftNs))
        {
            return LW_FALSE;
        }
    }

    // 1. Set the priv blocker mode
    if (bEquationMode)
    {
        // BLOCK_EQUATION mode
        val = FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_CTRL,
                          _BLOCKMODE, _EQUATION, val) |
              FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_CTRL,
                          _BLOCKMODE_TRIG, _TRUE, val);
    }
    else
    {
        //
        // When programming in BLOCK_EVERYTHING mode, also disable the blocker
        // from raising an interrupt to PMU when an access in the Allowlist
        // range is blocked. This is done for the following reason:
        // If a Allowlist transaction is made after we program the blocker in
        // BLOCK_EVERYTHING mode, SEC2 will raise an interrupt to PMU. The PMU
        // won't see it now since it should have already masked it. When we
        // switch to BLOCK_EQUATION mode, SEC2 will clear the interrupt on its
        // side. But the interrupt, being level-triggered, will stay. As soon
        // as the PMU unmasks the interrupt, it will see it and cause an MSCG
        // abort.
        //
        val = FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_CTRL,
                          _BLOCKMODE, _EVERYTHING, val) |
              FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_CTRL,
                          _BLOCKMODE_TRIG, _TRUE, val)  |
              FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_CTRL,
                          _BLOCKEVERYTHING_CYA, _NO_WL_INTR, val) |
              FLD_SET_DRF(_PSEC, _BLOCKER_BAR0_CTRL,
                          _BLOCKEVERYTHING_CYA_TRIG, _TRUE, val);
    }
    REG_WR32(FECS, LW_PSEC_BLOCKER_BAR0_CTRL, val);

    // 2. Engage both the FB and priv blockers
    val = REG_RD32(FECS, LW_PSEC_FALCON_ENGCTL);
    val = FLD_SET_DRF(_PSEC, _FALCON_ENGCTL, _STALLREQ_MODE, _TRUE, val) |
          FLD_SET_DRF(_PSEC, _FALCON_ENGCTL, _SET_STALLREQ,  _TRUE, val);

    REG_WR32(FECS, LW_PSEC_FALCON_ENGCTL, val);

    return LW_TRUE;
}
