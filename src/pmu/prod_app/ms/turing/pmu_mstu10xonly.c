/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_mstu10xonly.c
 * @brief  HAL-interface for the TU10x Memory System Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_hshub.h"
#include "dev_sec_pri.h"
#include "dev_gsp.h"
#include "dev_pwr_pri.h"
#include "dev_pwr_csb.h"
#include "dev_master.h"
#include "dev_disp.h"
#include "dev_therm.h"
#include "dev_perf.h"
#include "dev_trim.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objlpwr.h"
#include "pmu_objms.h"
#include "pmu_objfifo.h"
#include "pmu_objic.h"


#include "config/g_ms_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* --------------------------- Prototypes ------------------------------------*/
/* ------------------------ Public Functions  ------------------------------- */

/*!
 * @brief NISO Blocker Entry Sequence
 *
 * Steps:
 * 1. Engage Perf FB Blocker
 * 2. Engage Display FB Blocker
 * 3. Engage HSHUB FB Blocker
 * 4. Engage SEC2 FB Blocker
 * 5. Engage GSP FB Blocker
 * 6. Wait for all the blockers to get engaged (ARMED)
 *
 * NOTE: HSHUB blocker is not blocking all FB traffic coming to HSHUB. It
 * designed to block LwLink-to-HSHUB traffic. It is named as HSHUB blocker
 * because blocker logic seats in HSHUB unit.
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
msNisoBlockerEntry_TU10X
(
    LwU8            ctrlId,
    FLCN_TIMESTAMP *pBlockStartTimeNs,
    LwS32          *pTimeoutLeftNs,
    LwU16          *pAbortReason
)
{
    LwU32       val;
    LwBool      bStopPolling = LW_TRUE;
    OBJPGSTATE *pPgState     = GET_OBJPGSTATE(ctrlId);

    //
    // 7) Section 4.5.1 in WSP IAS:
    // For each Always-On unit, PMU will write via PRIV FECS path to engage
    // each FB interface blocker.
    //

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
        val = REG_RD32(FECS, LW_PDISP_FE_FB_BLOCKER_CTRL);
        val = FLD_SET_DRF(_PDISP, _FE_FB_BLOCKER_CTRL,
                          _BLOCK_EN_CMD, _ARMED, val);
        REG_WR32(FECS, LW_PDISP_FE_FB_BLOCKER_CTRL, val);
    }

    // Arm HSHUB blocker if HSHUB is supported and enabled
    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, HSHUB))
    {
        val = REG_RD32(FECS, LW_PFB_HSHUB_IG_SYS_FB_BLOCKER_CTRL);
        val = FLD_SET_DRF(_PFB, _HSHUB_IG_SYS_FB_BLOCKER_CTRL,
                          _BLOCK_EN_CMD, _ARMED, val);
        REG_WR32(FECS, LW_PFB_HSHUB_IG_SYS_FB_BLOCKER_CTRL, val);
    }

    // Arm Sec2 FB Blocker
    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, SEC2))
    {
        // Engage FB blocker
        val = REG_RD32(FECS, LW_PSEC_FALCON_ENGCTL);
        val = FLD_SET_DRF(_PSEC, _FALCON_ENGCTL, _STALLREQ_MODE, _TRUE, val) |
              FLD_SET_DRF(_PSEC, _FALCON_ENGCTL, _SET_STALLREQ, _TRUE, val);

        REG_WR32(FECS, LW_PSEC_FALCON_ENGCTL, val);
    }

    // Arm GSP FB Blocker
    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, GSP))
    {
        // Engage FB blocker
        val = REG_RD32(FECS, LW_PGSP_FALCON_ENGCTL);
        val = FLD_SET_DRF(_PGSP, _FALCON_ENGCTL, _STALLREQ_MODE, _TRUE, val) |
              FLD_SET_DRF(_PGSP, _FALCON_ENGCTL, _SET_STALLREQ, _TRUE, val);

        REG_WR32(FECS, LW_PGSP_FALCON_ENGCTL, val);
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

        // Poll for Disp-NISO blocker to engage
        if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, DISPLAY))
        {
            val          = REG_RD32(FECS, LW_PDISP_FE_FB_BLOCKER_CTRL);
            bStopPolling = bStopPolling && FLD_TEST_DRF(_PDISP,
                                                        _FE_FB_BLOCKER_CTRL,
                                                        _BLOCK_EN_STATUS,
                                                        _ACKED, val);
        }

        // Poll for HSHUB blocker to engage
        if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, HSHUB))
        {
            val          = REG_RD32(FECS, LW_PFB_HSHUB_IG_SYS_FB_BLOCKER_CTRL);
            bStopPolling = bStopPolling && FLD_TEST_DRF(_PFB,
                                            _HSHUB_IG_SYS_FB_BLOCKER_CTRL,
                                            _BLOCK_EN_STATUS, _ACKED, val);
            bStopPolling = bStopPolling && FLD_TEST_DRF(_PFB,
                                            _HSHUB_IG_SYS_FB_BLOCKER_CTRL,
                                            _OUTSTANDING_REQ, _INIT, val);
        }

        // Poll for SEC2 Fb blocker to engage
        if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, SEC2))
        {
            val          = REG_RD32(FECS, LW_PSEC_BLOCKER_STAT);
            bStopPolling = bStopPolling && FLD_TEST_DRF(_PSEC, _BLOCKER_STAT,
                                                        _FB_BLOCKED, _TRUE, val);
        }

        // Poll for GSP Fb blocker to engage
        if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, GSP))
        {
            val          = REG_RD32(FECS, LW_PGSP_BLOCKER_STAT);
            bStopPolling = bStopPolling && FLD_TEST_DRF(_PGSP, _BLOCKER_STAT,
                                                        _FB_BLOCKED, _TRUE, val);
        }

        // Check the timeout
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
 * 1. Disengage the GSP FB Blocker
 * 2. Disengage the SEC2 FB Blocker
 * 3. Disengage the HSHUB FB Blocker
 * 4. Disengage the Display FB Blocker
 * 5. Disengage the Perf FB Blocker
 *
 * @param[in]  ctrlId              LPWR_CTRL Id engaging the blockers
 */
void
msNisoBlockerExit_TU10X
(
    LwU8    ctrlId
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);
    LwU32       val;

    // Disengage GSP FB Blocker
    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, GSP))
    {
        val = REG_RD32(FECS, LW_PGSP_FALCON_ENGCTL);

        // Note that this will also clear STALLREQ_MODE
        val = FLD_SET_DRF(_PGSP, _FALCON_ENGCTL, _CLR_STALLREQ,  _TRUE, val);
        REG_WR32(FECS, LW_PGSP_FALCON_ENGCTL, val);
    }

    // Disengage SEC2 FB Blocker
    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, SEC2))
    {
        val = REG_RD32(FECS, LW_PSEC_FALCON_ENGCTL);

        // Note that this will also clear STALLREQ_MODE
        val = FLD_SET_DRF(_PSEC, _FALCON_ENGCTL, _CLR_STALLREQ,  _TRUE, val);
        REG_WR32(FECS, LW_PSEC_FALCON_ENGCTL, val);
    }

    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, HSHUB))
    {
        val = REG_RD32(FECS, LW_PFB_HSHUB_IG_SYS_FB_BLOCKER_CTRL);
        val = FLD_SET_DRF(_PFB, _HSHUB_IG_SYS_FB_BLOCKER_CTRL,
                          _BLOCK_EN_CMD, _UNARMED, val);
        REG_WR32(FECS, LW_PFB_HSHUB_IG_SYS_FB_BLOCKER_CTRL, val);
    }

    // Disengage Disp-NISO blocker
    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, DISPLAY))
    {
        val = REG_RD32(FECS, LW_PDISP_FE_FB_BLOCKER_CTRL);
        val = FLD_SET_DRF(_PDISP, _FE_FB_BLOCKER_CTRL,
                          _BLOCK_EN_CMD, _UNARMED, val);
        REG_WR32(FECS, LW_PDISP_FE_FB_BLOCKER_CTRL, val);
    }

    // Disengage PERF blocker
    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, PERF))
    {
        val = REG_RD32(FECS, LW_PERF_PMASYS_SYS_FB_BLOCKER_CTRL);
        val = FLD_SET_DRF(_PERF, _PMASYS_SYS_FB_BLOCKER_CTRL,
                          _BLOCK_EN_CMD, _UNARMED, val);
        REG_WR32(FECS, LW_PERF_PMASYS_SYS_FB_BLOCKER_CTRL, val);
    }
}

/*!
 * Check if host interrupt is pending
 *
 * @return   LW_TRUE  if a host interrupt is pending
 * @return   LW_FALSE otherwise
 */
LwBool
msIsHostIntrPending_TU10X(void)
{
    // In Turing we will use PFIFO interrupt check.
    if (REG_FLD_IDX_TEST_DRF_DEF(BAR0, _PMC, _INTR, 0, _PFIFO, _PENDING))
    {
        return LW_TRUE;
    }

    return LW_FALSE;
}

/*!
 * @brief Initializes the idle mask for MS engine.
 */
void
msMscgIdleMaskInit_TU10X(void)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
    LwU32       im0      = 0;
    LwU32       im1      = 0;
    LwU32       im2      = 0;

    if (FIFO_IS_ENGINE_PRESENT(GR))
    {
        im0 = (FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR, _ENABLED, im0)     |
               FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR_SYS, _ENABLED, im0) |
               FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR_GPC, _ENABLED, im0) |
               FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR_BE, _ENABLED, im0));

    }

    if (FIFO_IS_ENGINE_PRESENT(CE0))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_0, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE1))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_1, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE2))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_2, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE3))
    {
        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_3, _ENABLED, im1);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE4))
    {
        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_4, _ENABLED, im1);
    }

    //  LWDEC = BSP  (reusing MSVLD code for LWDEC)
    if (FIFO_IS_ENGINE_PRESENT(BSP))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWDEC, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(DEC1))
    {
        im2 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_LWDEC1, _ENABLED, im2);
    }

    if (FIFO_IS_ENGINE_PRESENT(DEC2))
    {
        im2 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_LWDEC2, _ENABLED, im2);
    }

    if (FIFO_IS_ENGINE_PRESENT(ENC0))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWENC0, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(ENC1))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWENC1, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(SEC2))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _SEC, _ENABLED, im0);

        if (LPWR_CTRL_IS_SF_SUPPORTED(pPgState, MS, SEC2))
        {
            im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _SEC_FBIF,
                              _ENABLED, im1);
            im2 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _SEC_PRIV_BLOCKER_MS,
                              _ENABLED, im2);

        }
    }

    if (LPWR_CTRL_IS_SF_SUPPORTED(pPgState, MS, GSP))
    {
        im2 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _GSP_FB, _ENABLED, im2);
        im2 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _GSP_PRIV_BLOCKER_MS,
                          _ENABLED, im2);
    }

    if (LPWR_CTRL_IS_SF_SUPPORTED(pPgState, MS, HSHUB))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _HSHUB_NISO, _ENABLED, im0);
    }

    // From TU10X, FBHUB_ALL only represents the NISO traffic.
    im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _FBHUB_ALL, _ENABLED, im1);

    // Host Idlness signals
    im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _HOST_NOT_QUIESCENT, _ENABLED, im0);
    im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _HOST_INT_NOT_ACTIVE, _ENABLED, im0);

    pPgState->idleMask[0] = im0;
    pPgState->idleMask[1] = im1;
    pPgState->idleMask[2] = im2;
}

/*!
 * @brief Engage/Disengage the CLK Slowdown for SYS and LWD Clk
 *
 * @param[in]   ctrlId  LPWR_CTRL Id
 * @param[in]   bEngage LW_TRUE -> Slowdown the clocks
 *                      LW_FALSE -> Restore the original value of clk
 */
void
msClkSlowDownEngage_TU10X
(
    LwU8   ctrlId,
    LwBool bEngage
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);
    LwU32       regVal = 0;

    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, CLK_SLOWDOWN_SYS))
    {
        regVal = REG_RD32(FECS, LW_PTRIM_SYS_SYS2CLK_OUT_LDIV);

        if (bEngage)
        {
            // Cache the original Value
            Ms.pClkGatingRegs->sysClkLdivConfig = regVal;

            // Slow down the clock by x4
            regVal = FLD_SET_DRF(_PTRIM_SYS, _SYS2CLK_OUT_LDIV, _ONESRC0DIV,
                                 _BY4, regVal);
            regVal = FLD_SET_DRF(_PTRIM_SYS, _SYS2CLK_OUT_LDIV, _ONESRC1DIV,
                                 _BY4, regVal);
        }
        else
        {
            // restore the original value;
            regVal = Ms.pClkGatingRegs->sysClkLdivConfig;
        }

        REG_WR32(FECS, LW_PTRIM_SYS_SYS2CLK_OUT_LDIV, regVal);
        // Read back the register to flush it
        REG_RD32(FECS, LW_PTRIM_SYS_SYS2CLK_OUT_LDIV);
    }

    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, CLK_SLOWDOWN_LWD))
    {
        regVal = REG_RD32(FECS, LW_PTRIM_SYS_LWD2CLK_OUT_LDIV);

        if (bEngage)
        {
            // Cache the original Value
            Ms.pClkGatingRegs->lwdClkLdivConfig = regVal;

            // Slow down the clock by x4
            regVal = FLD_SET_DRF(_PTRIM_SYS, _LWD2CLK_OUT_LDIV, _ONESRC0DIV,
                                 _BY4, regVal);
            regVal = FLD_SET_DRF(_PTRIM_SYS, _LWD2CLK_OUT_LDIV, _ONESRC1DIV,
                                 _BY4, regVal);
        }
        else
        {
            // restore the original value;
            regVal = Ms.pClkGatingRegs->lwdClkLdivConfig;
        }

        REG_WR32(FECS, LW_PTRIM_SYS_LWD2CLK_OUT_LDIV, regVal);
        // Read back the register to flush it
        REG_RD32(FECS, LW_PTRIM_SYS_LWD2CLK_OUT_LDIV);
    }
}
/* ------------------------- Private Functions ------------------------------ */
