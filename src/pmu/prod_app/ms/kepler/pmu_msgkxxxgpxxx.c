/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_msgkxxxgpxxx.c
 * @brief  Kepler to Pascal specific MS object file
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_ihub.h"
#include "dev_fifo.h"
#include "dev_perf.h"
#include "dev_disp.h"
#include "dev_top.h"
#include "dev_xbar.h"

/* ------------------------ Application includes --------------------------- */
#include "main.h"
#include "pmu_objhal.h"
#include "pmu_objms.h"
#include "pmu_objic.h"
#include "dbgprintf.h"

#include "config/g_ms_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Prototypes ------------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Local Functions  ------------------------------- */
/* ------------------------ Public Functions  ------------------------------ */

/*!
 * @brief Configure entry conditions for MS
 *
 * Helper to remove GR-ELPG, Video ELPG, ISO stutter, DWB from entry equation
 * of PG_ENG HW SM of MS.
 */
void
msEntryConditionConfig_GMXXX
(
    LwU8 ctrlId
)
{
    OBJPGSTATE *pMsState = GET_OBJPGSTATE(ctrlId);
    LwU32       val;

    // Update the GR and MSCG dependencies
    lpwrFsmDependencyInit_HAL(&Lpwr, PG_ENG_IDX_ENG_GR, PG_ENG_IDX_ENG_MS);

    // Update the rest of override.
    val = REG_RD32(CSB, LW_CPWR_PMU_PG_OVERRIDE);

    // Always override Video ELPG as its not POR for kepler and onward GPUs
    val = FLD_SET_DRF(_CPWR_PMU, _PG_OVERRIDE, _VD, _ENABLE, val);

    // Check for ISO stutter override
    if (!LPWR_CTRL_IS_SF_SUPPORTED(pMsState, MS, ISO_STUTTER))
    {
        val = FLD_SET_DRF(_CPWR_PMU, _PG_OVERRIDE, _DISP, _ENABLE, val);
    }

#ifdef LW_CPWR_PMU_PG_OVERRIDE_DWB_ENABLE
    // Check for DWB MSCG override
    if (!LPWR_CTRL_IS_SF_SUPPORTED(pMsState, MS, DWB))
    {
        val = FLD_SET_DRF(_CPWR_PMU, _PG_OVERRIDE, _DWB, _ENABLE, val);
    }
#endif // LW_CPWR_PMU_PG_OVERRIDE_DWB_ENABLE

    REG_WR32(CSB, LW_CPWR_PMU_PG_OVERRIDE, val);
}

/*!
 * @brief Enable/Disable iso stutter override for MSCG
 *
 * Adds/Removes display to/from MSCG entry conditions.
 *
 * @param[in] ctrlId              LPWR_CTRL Id from MS Group
 * @param[in] bAddIsoStutter      LW_TRUE     Add display to MSCG
 *                                            entry conditions
 *                                LW_FALSE    Remove display from MSCG
 *                                            entry condition
 */
void
msIsoStutterOverride_GMXXX
(
    LwU8   ctrlId,
    LwBool bAddIsoStutter
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);
    LwU32       pgOverride;
    LwU32       intr1;

    //
    // This check will ensure that current state of ISO_STUTTER subfeature
    // is in sync with request to add/remove ISO STUTTER.
    //
    // if bAddIsoStutter is LW_TRUE, then ISO_STUTTER subfeature must be in
    // disabled state and vice versa. Then SFM will update the subfeature as per
    // the latest requested state.
    //
    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, ISO_STUTTER) != bAddIsoStutter)
    {
        pgOverride = REG_RD32(CSB, LW_CPWR_PMU_PG_OVERRIDE);
        intr1      = REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN);

        pgOverride = bAddIsoStutter ? FLD_SET_DRF(_CPWR_PMU, _PG_OVERRIDE, _DISP, _DISABLE, pgOverride):
                                      FLD_SET_DRF(_CPWR_PMU, _PG_OVERRIDE, _DISP, _ENABLE, pgOverride);

        intr1      = bAddIsoStutter ? FLD_SET_DRF(_CPWR_PMU, _GPIO_1_INTR_RISING_EN, _MSPG_WAKE, _ENABLED, intr1):
                                      FLD_SET_DRF(_CPWR_PMU, _GPIO_1_INTR_RISING_EN, _MSPG_WAKE, _DISABLED, intr1);

        REG_WR32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN, intr1);
        REG_WR32(CSB, LW_CPWR_PMU_PG_OVERRIDE, pgOverride);
    }
}

/*!
 * @brief Wait till memory subsystem goes idle
 *
 * Function polls FBPA and LTC idle status to make sure FB subsystem is idle.
 * Also it ensure that all FB sub-units are idle.
 *
 * @param[in]   ctrlId             lpwr ctrl Id
 * @param[in]   pBlockStartTimeNs  Start time of the block sequence
 * @param[out]  pTimeoutLeftNs     Time left in the block sequence
 *
 * @return      FLCN_OK     FB sub-system is idle
 *              FLCN_ERROR  FB sub-system is not idle
 */
FLCN_STATUS
msWaitForSubunitsIdle_GMXXX
(
    LwU8            ctrlId,
    FLCN_TIMESTAMP *pBlockStartTimeNs,
    LwS32          *pTimeoutLeftNs
)
{
    LwBool bIdle = LW_FALSE;
    LwU32  val;

    // Poll for FBPA and LTC idle
    do
    {
        val = REG_RD32(CSB, LW_CPWR_PMU_IDLE_STATUS);

        bIdle = FLD_TEST_DRF(_CPWR_PMU, _IDLE_STATUS, _FB_PA,    _IDLE, val) &&
                FLD_TEST_DRF(_CPWR_PMU, _IDLE_STATUS, _LTC_IDLE, _IDLE, val);

        if (pgCtrlIdleFlipped_HAL(&Pg, ctrlId) ||
            msCheckTimeout(MS_DEFAULT_ABORT_TIMEOUT1_NS,
                           pBlockStartTimeNs, pTimeoutLeftNs))
        {
            return FLCN_ERROR;
        }
    } while (!bIdle);

    // Ensure that all FB subunits idle
    bIdle = FLD_TEST_DRF(_CPWR_PMU, _IDLE_STATUS, _XBAR_IDLE, _IDLE, val);

    val   = REG_RD32(CSB, LW_CPWR_PMU_IDLE_STATUS_1);
    bIdle = bIdle                                                             &&
            FLD_TEST_DRF(_CPWR_PMU, _IDLE_STATUS_1, _MS_NISO_ALL, _IDLE, val) &&
            FLD_TEST_DRF(_CPWR_PMU, _IDLE_STATUS_1, _MS_MMU,      _IDLE, val);
    if (!bIdle)
    {
       return FLCN_ERROR;
    }

    return FLCN_OK;
}

/*!
 * @brief Enable or Disable the force fill mode in Iso Hub
 * PMU should set ihub in force fill mode when not in MSCG so as to keep filling
 * the mempool past the high watermark till the limit is reached.
 * As part of MSCG engage, it should disable the force fill mode.
 *
 * Note: This function will be called during Init Phase only. In this
 * function we will use support Mask to enable Force Fill.
 */
void
msDispForceFillInit_GMXXX(void)
{
    OBJPGSTATE *pMsState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
    LwU32       val      = 0;

    //
    // Ideally this should also be colwerted to LpwrGrp API, but
    // in the interest of IMEM, not colwerting it.
    //
    if (LPWR_CTRL_IS_SF_SUPPORTED(pMsState, MS, ISO_STUTTER))
    {
        val = REG_RD32(FECS, LW_PDISP_ISOHUB_PMU_CTL);
        val = FLD_SET_DRF(_PDISP, _ISOHUB_PMU_CTL, _FORCE_FILL, _ENABLE, val);
        REG_WR32(FECS, LW_PDISP_ISOHUB_PMU_CTL, val);
    }
}

/*!
 * @brief Enable or Disable the force fill mode in Iso Hub
 * PMU should set ihub in force fill mode when not in MSCG so as to keep filling
 * the mempool past the high watermark till the limit is reached.
 * As part of MSCG engage, it should disable the force fill mode.
 *
 * Note: This function will be called from Core MSCG sequence.
 * Here we need to check enabled Mask.
 *
 * @param[in]  bEnable  LW_TRUE  - enable.
 *                      LW_FALSE - disable
 */
void
msDispForceFillEnable_GMXXX(LwBool bEnable)
{
    LwU32 val = 0;

    val = REG_RD32(FECS, LW_PDISP_ISOHUB_PMU_CTL);

    val = bEnable ? FLD_SET_DRF(_PDISP, _ISOHUB_PMU_CTL,
                                _FORCE_FILL, _ENABLE, val) :
                    FLD_SET_DRF(_PDISP, _ISOHUB_PMU_CTL,
                                _FORCE_FILL, _DISABLE, val);
    REG_WR32(FECS, LW_PDISP_ISOHUB_PMU_CTL, val);
}

/*!
 * @brief Enable MISCIO MSCG interrutps
 *
 * Function to enable the various MISCIO interrupts The display stutter and
 * display blocker interrupts should only be enabled if display is enabled
 */
void
msInitAndEnableIntr_GMXXX(void)
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

    Ms.intrRiseEnMask |= val;

    REG_WR32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN, val);
}

/*!
 * Colwert MISCIO intrstat to wakeup type for MSCG+SW-ASR
 *
 * @param[in]  intrStat
 *
 * @return Wake-up Mask
 */
LwU32
msColwertIntrsToWakeUpMask_GMXXX
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
    return wakeUpMask;
}

/*!
 * @brief NISO Blocker Entry Sequence
 *
 * Steps:
 * 1. Disable host to fb interface.
 * 2. Engage Perf FB Blocker
 * 3. Engage Display FB Blocker
 * 4. Wait for all the blockers to get engaged (ARMED)
 *
 * @param[in]  ctrlId              LPWR_CTRL Id engaging the blockers
 * @param[in]  pBlockStartTimeNs  Start time of the block sequence
 * @param[out] pTimeoutLeftNs     Time left in the block sequence
 * @param[out] pAbortReason       Abort reason
 *
 * @return  FLCN_OK     NISO blockers got engaged
 *          FLCN_ERROR  Failed to engage NISO blockers
 */
FLCN_STATUS
msNisoBlockerEntry_GMXXX
(
    LwU8            ctrlId,
    FLCN_TIMESTAMP *pBlockStartTimeNs,
    LwS32          *pTimeoutLeftNs,
    LwU16          *pAbortReason
)
{
    OBJPGSTATE *pPgState     = GET_OBJPGSTATE(ctrlId);
    LwBool      bStopPolling = LW_TRUE;
    LwU32       val;

    //
    // 7) Section 4.5.1 in WSP IAS:
    // For each Always-On unit, PMU will write via PRIV FECS path to engage
    // each FB interface blocker.
    //

    // Disable HOST2FB interface
    REG_FLD_WR_DRF_DEF(BAR0, _PFIFO, _FB_IFACE, _CONTROL, _DISABLE);

    // Arm PERF blocker
    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, PERF))
    {
        val = REG_RD32(FECS, LW_PERF_PMASYS_SYS_FB_BLOCKER_CTRL);
        val = FLD_SET_DRF(_PERF, _PMASYS_SYS_FB_BLOCKER_CTRL,
                          _BLOCK_EN_CMD, _ARMED, val);
        REG_WR32(FECS, LW_PERF_PMASYS_SYS_FB_BLOCKER_CTRL, val);
    }

    // Arm Disp-NISO blocker if Display is supported and enabled
    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, DISPLAY))
    {
        val = REG_RD32(FECS, LW_PDISP_DMI_FB_BLOCKER_CTRL);
        val = FLD_SET_DRF(_PDISP, _DMI_FB_BLOCKER_CTRL,
                          _BLOCK_EN_CMD, _ARMED, val);
        REG_WR32(FECS, LW_PDISP_DMI_FB_BLOCKER_CTRL, val);
    }

    //
    // 8)  Section 4.5.1 in WSP IAS:
    // For each Always-On unit, PMU will poll via PRIV FECS path to confirm:
    // a. The FB interface blocker is engaged.
    // b. The unit has no outstanding FB read requests in flight,
    //    no unack'ed write requests, and all flushes have been
    //    complete (based on flush_resp_vld).
    //
    do
    {
        // Poll for PERF blocker to engage
        if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, PERF))
        {
            val          = REG_RD32(FECS, LW_PERF_PMASYS_SYS_FB_BLOCKER_CTRL);
            bStopPolling = FLD_TEST_DRF(_PERF, _PMASYS_SYS_FB_BLOCKER_CTRL,
                                        _BLOCK_EN_STATUS, _ACKED, val);
            bStopPolling = bStopPolling && FLD_TEST_DRF(_PERF,
                                                        _PMASYS_SYS_FB_BLOCKER_CTRL,
                                                        _OUTSTANDING_REQ,
                                                        _INIT, val);
        }

        // Poll for HOST2FB interface disablement
        val          = REG_RD32(BAR0, LW_PFIFO_FB_IFACE);
        bStopPolling = bStopPolling && FLD_TEST_DRF(_PFIFO, _FB_IFACE,
                                                    _STATUS, _DISABLED, val);

        // Poll for Disp-NISO blocker to engage
        if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, DISPLAY))
        {
            val          = REG_RD32(FECS, LW_PDISP_DMI_FB_BLOCKER_CTRL);
            bStopPolling = bStopPolling && FLD_TEST_DRF(_PDISP,
                                                        _DMI_FB_BLOCKER_CTRL,
                                                        _BLOCK_EN_STATUS,
                                                        _ACKED, val);
            bStopPolling = bStopPolling && FLD_TEST_DRF(_PDISP,
                                                        _DMI_FB_BLOCKER_CTRL,
                                                        _OUTSTANDING_REQ,
                                                        _INIT, val);
        }

        // Timeout check for blocker engagement.
        if (msCheckTimeout(Ms.abortTimeoutNs, pBlockStartTimeNs, pTimeoutLeftNs))
        {
            *pAbortReason = MS_ABORT_REASON_FB_BLOCKER_TIMEOUT;
            return FLCN_ERROR;
        }
    }
    while (!bStopPolling);

    return FLCN_OK;
}

/*!
 * @brief NISO Blocker Eexit Sequence
 *
 * Steps:
 * 1. Disengage the Display FB Blocker
 * 2. Disengage the Perf FB Blocker
 * 3. Enable the host to FB interface
 *
 * @param[in]  ctrlId              LPWR_CTRL Id engaging the blockers
 *
 * This is helper function dis-engages NISO FB blockers. It dis-engages
 * Disp-NISO, PERF blocker and then disables host2fb interface.
 */
void
msNisoBlockerExit_GMXXX
(
    LwU8    ctrlId
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);
    LwU32       val;

    // Disengage Disp-NISO blocker
    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, DISPLAY))
    {
        val = REG_RD32(FECS, LW_PDISP_DMI_FB_BLOCKER_CTRL);
        val = FLD_SET_DRF(_PDISP, _DMI_FB_BLOCKER_CTRL,
                          _BLOCK_EN_CMD, _UNARMED, val);
        REG_WR32(FECS, LW_PDISP_DMI_FB_BLOCKER_CTRL, val);
    }

    // Disengage PERF blocker
    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, PERF))
    {
        val = REG_RD32(FECS, LW_PERF_PMASYS_SYS_FB_BLOCKER_CTRL);
        val = FLD_SET_DRF(_PERF, _PMASYS_SYS_FB_BLOCKER_CTRL,
                          _BLOCK_EN_CMD, _UNARMED, val);
        REG_WR32(FECS, LW_PERF_PMASYS_SYS_FB_BLOCKER_CTRL, val);
    }

    //
    // Enable HOST2FB interface
    //
    // Unblock the HOST2FB Blocker at the last. The write to disable the
    // Host2FB blocker has to go through host path, so it should be the last
    // to be unblocked among all the unit2FB blockers
    //
    REG_FLD_WR_DRF_DEF(BAR0, _PFIFO, _FB_IFACE, _CONTROL, _ENABLE);
}

/*!
 * @brief ISO Blocker Entry sequence
 *
 * Steps:
 * 1. Disable the Display Mempool force fill mode.
 * 2. Wait for the mempool to go into Drain Mode.
 * 3. Engage the Disp ISO blocker.
 *
 * @param[in]   ctrlId             LPWR_CTRL Id
 * @param[in]   pBlockStartTimeNs  Start time of the block sequence
 * @param[out]  pTimeoutLeftNs     Time left in the block sequence
 * @param[out]  pAbortReason       Abort reason
 *
 * @return  FLCN_OK     NISO blockers got engaged
 *          FLCN_ERROR  Failed to engage NISO blockers
 */
FLCN_STATUS
msIsoBlockerEntry_GMXXX
(
    LwU8            ctrlId,
    FLCN_TIMESTAMP *pBlockStartTimeNs,
    LwS32          *pTimeoutLeftNs,
    LwU16          *pAbortReason
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);
    LwU32       val;

    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, ISO_STUTTER))
    {
        // Disengage force_fill mode in display.
        msDispForceFillEnable_HAL(&Ms, LW_FALSE);

        //
        // Poll for mempool draining if ISOHUB is enabled for current cycle.
        //
        // Its possible to remove display (and thus ISOHUB) from MSCG pre-condition
        // MSCG ignores watermarks for such mode. Polling for mempool_draining can
        // reduce MSCG residency when we are trying to engage MSCG when we are
        // already below the low watermark when mempool draining would be deasserted
        // and this poll will never pass.
        //
        if (msCheckTimeout(MS_DEFAULT_ABORT_TIMEOUT1_NS,
                           pBlockStartTimeNs, pTimeoutLeftNs) ||
            !PMU_REG32_POLL_NS(LW_CPWR_PMU_GPIO_1_INPUT,
                 DRF_SHIFTMASK(LW_CPWR_PMU_GPIO_1_INPUT_MEMPOOL_DRAINING),
                 DRF_DEF(_CPWR_PMU, _GPIO_1_INPUT, _MEMPOOL_DRAINING, _TRUE),
                 (LwU32) *pTimeoutLeftNs, PMU_REG_POLL_MODE_CSB_EQ))
        {
            *pAbortReason = MS_ABORT_REASON_DRAINING_TIMEOUT;
            return FLCN_ERROR;
        }

        //
        // Engage the ISO Blocker since now display is draining mode, so its
        // not requesting new data from FB.
        //
        val = REG_RD32(FECS, LW_PDISP_ISOHUB_FB_BLOCKER_CTRL);
        val = FLD_SET_DRF(_PDISP, _ISOHUB_FB_BLOCKER_CTRL,
                          _BLOCK_EN_CMD, _ARMED, val);
        REG_WR32(FECS, LW_PDISP_ISOHUB_FB_BLOCKER_CTRL, val);

        if (msCheckTimeout(MS_DEFAULT_ABORT_TIMEOUT1_NS,
                           pBlockStartTimeNs, pTimeoutLeftNs) ||
            !PMU_REG32_POLL_NS(USE_FECS(LW_PDISP_ISOHUB_FB_BLOCKER_CTRL),
                DRF_SHIFTMASK(LW_PDISP_ISOHUB_FB_BLOCKER_CTRL_BLOCK_EN_STATUS),
                DRF_DEF(_PDISP, _ISOHUB_FB_BLOCKER_CTRL, _BLOCK_EN_STATUS,
                 _ACKED), (LwU32) *pTimeoutLeftNs, PMU_REG_POLL_MODE_BAR0_EQ))
        {
            *pAbortReason = MS_ABORT_REASON_ISO_BLOCKER_TIMEOUT;
            return FLCN_ERROR;
        }
    }

    return FLCN_OK;
}

/*!
 * @brief Exit Sequecne for ISO blockers
 *
 * Steps:
 * 1. Disengage the Disp ISO Blocker
 * 2. Enable display mempool force fill mode
 *
 */
void
msIsoBlockerExit_GMXXX
(
    LwU8 ctrlId
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);
    LwU32       val;

    //
    // We should always check for support of ISOSTUTTER
    // even though if we are in OSM mode, we must engage
    // these blockers.
    //
    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, ISO_STUTTER))
    {
        val = REG_RD32(FECS, LW_PDISP_ISOHUB_FB_BLOCKER_CTRL);
        val = FLD_SET_DRF(_PDISP, _ISOHUB_FB_BLOCKER_CTRL,
                          _BLOCK_EN_CMD, _UNARMED, val);
        REG_WR32(FECS, LW_PDISP_ISOHUB_FB_BLOCKER_CTRL, val);

        // Return the display mempool to force_fill mode.
        msDispForceFillEnable_HAL(&Ms, LW_TRUE);
    }
}
