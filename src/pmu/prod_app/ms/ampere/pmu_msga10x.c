/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_ga10x.c
 * @brief  HAL-interface for the GA10X Memory System Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_disp.h"
#include "dev_hshub_SW.h"
#include "mscg_wl_bl_init.h"
#include "dev_sec_pri.h"
#include "dev_gsp.h"
#include "dev_pwr_pri.h"
#include "dev_pwr_csb.h"
#include "dev_smcarb.h"
#include "dev_fbpa.h"
#include "dev_ltc.h"
#include "dev_perf.h"
#include "dev_trim.h"
#include "dev_pri_ringmaster.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objlpwr.h"
#include "pmu_objms.h"
#include "pmu_objfifo.h"
#include "pmu_objic.h"
#include "ctrl/ctrl2080/ctrl2080fb.h"

#include "config/g_ms_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

#define L2_CACHE_OPS_ABORT_TIMEOUT_NS       (100 * 1000)

/*!
 * @brief Timeout for acquiring  mutex. We will abort MS-RPG if PMU
 *        fails to acquire this mutex.
 */
#define MS_RPG_MUTEX_ACQUIRE_TIMEOUT_NS       (10000) // 10us

/*
 * L2 Cache TIER1 and TIER2 Threshold values for DIFR Feature
 *
 * Make sure TIER2 > TIER1 Always
 */
#define MS_DIFR_L2_CACHE_SET_TIER1_DIRTY_THRESHOLD   14
#define MS_DIFR_L2_CACHE_SET_TIER2_DIRTY_THRESHOLD   16

/*
 * L2 Cache PLC(Post L2 Compression) Threshold Values for DIFR
 */
#define MS_DIFR_L2_CACHE_SET_PLC_THRESHOLD           16

//
// Compile time check to ensure that TIER1 and TIER2 settings are correct
//
ct_assert(MS_DIFR_L2_CACHE_SET_TIER2_DIRTY_THRESHOLD >
          MS_DIFR_L2_CACHE_SET_TIER1_DIRTY_THRESHOLD);


/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Static Functions -------------------------------- */
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
static LwU32 s_msNumHshubsGet_GA10X(void);
#endif
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
msNisoBlockerEntry_GA10X
(
    LwU8            ctrlId,
    FLCN_TIMESTAMP *pBlockStartTimeNs,
    LwS32          *pTimeoutLeftNs,
    LwU16          *pAbortReason
)
{
    LwU32       val;
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    LwU32       i;
    LwU32       numHshub     = s_msNumHshubsGet_GA10X();
#endif
    LwBool      bStopPolling = LW_TRUE;
    OBJPGSTATE *pPgState     = GET_OBJPGSTATE(ctrlId);

    //
    // 7) Section 4.5.1 in WSP IAS:
    // For each Always-On unit, PMU will write via PRIV FECS path to engage
    // each FB interface blocker.
    //

#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    // Arm PERF blocker
    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, PERF))
    {
        val = REG_RD32(FECS, LW_PERF_PMASYS_SYS_FB_BLOCKER_CTRL);
        val = FLD_SET_DRF(_PERF, _PMASYS_SYS_FB_BLOCKER_CTRL,
                          _BLOCK_EN_CMD, _ARMED, val);
        REG_WR32(FECS, LW_PERF_PMASYS_SYS_FB_BLOCKER_CTRL, val);
    }
#endif

    // Arm Disp-NISO blocker if Display is supported and enabled
    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, DISPLAY))
    {
        val = REG_RD32(FECS, LW_PDISP_FE_FB_BLOCKER_CTRL);
        val = FLD_SET_DRF(_PDISP, _FE_FB_BLOCKER_CTRL,
                          _BLOCK_EN_CMD, _ARMED, val);
        REG_WR32(FECS, LW_PDISP_FE_FB_BLOCKER_CTRL, val);
    }

#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    // Arm HSHUB blocker if HSHUB is supported and enabled
    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, HSHUB))
    {
        for (i = 0; i < numHshub; i++)
        {
            val = REG_RD32(FECS, LW_PFB_HSHUB_IG_SYS_FB_BLOCKER_CTRL(i));
            val = FLD_SET_DRF(_PFB, _HSHUB_IG_SYS_FB_BLOCKER_CTRL,
                              _BLOCK_EN_CMD, _ARMED, val);
            REG_WR32(FECS, LW_PFB_HSHUB_IG_SYS_FB_BLOCKER_CTRL(i), val);
        }
    }
#endif

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
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
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
#endif

        // Poll for Disp-NISO blocker to engage
        if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, DISPLAY))
        {
            val          = REG_RD32(FECS, LW_PDISP_FE_FB_BLOCKER_CTRL);
            bStopPolling = bStopPolling && FLD_TEST_DRF(_PDISP,
                                                        _FE_FB_BLOCKER_CTRL,
                                                        _BLOCK_EN_STATUS,
                                                        _ACKED, val);
        }

#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
        // Poll for HSHUB blocker to engage
        if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, HSHUB))
        {
            for (i = 0; i < numHshub; i++)
            {
                val          = REG_RD32(FECS, LW_PFB_HSHUB_IG_SYS_FB_BLOCKER_CTRL(i));
                bStopPolling = bStopPolling && FLD_TEST_DRF(_PFB,
                                                _HSHUB_IG_SYS_FB_BLOCKER_CTRL,
                                                _BLOCK_EN_STATUS, _ACKED, val);
                bStopPolling = bStopPolling && FLD_TEST_DRF(_PFB,
                                                _HSHUB_IG_SYS_FB_BLOCKER_CTRL,
                                                _OUTSTANDING_REQ, _INIT, val);
            }
        }
#endif

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

        // check the timeout
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
msNisoBlockerExit_GA10X
(
    LwU8    ctrlId
)
{
    LwU32       val;
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    LwU32       i;
    LwU32       numHshub = s_msNumHshubsGet_GA10X();
#endif
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);

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

#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    // Disengage HSHUB FB Blocker
    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, HSHUB))
    {
        for (i = 0; i < numHshub; i++)
        {
            val = REG_RD32(FECS, LW_PFB_HSHUB_IG_SYS_FB_BLOCKER_CTRL(i));
            val = FLD_SET_DRF(_PFB, _HSHUB_IG_SYS_FB_BLOCKER_CTRL,
                              _BLOCK_EN_CMD, _UNARMED, val);
            REG_WR32(FECS, LW_PFB_HSHUB_IG_SYS_FB_BLOCKER_CTRL(i), val);
        }
    }
#endif

    // Disengage Disp-NISO blocker
    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, DISPLAY))
    {
        val = REG_RD32(FECS, LW_PDISP_FE_FB_BLOCKER_CTRL);
        val = FLD_SET_DRF(_PDISP, _FE_FB_BLOCKER_CTRL,
                          _BLOCK_EN_CMD, _UNARMED, val);
        REG_WR32(FECS, LW_PDISP_FE_FB_BLOCKER_CTRL, val);
    }

#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    // Disengage PERF blocker
    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, PERF))
    {
        val = REG_RD32(FECS, LW_PERF_PMASYS_SYS_FB_BLOCKER_CTRL);
        val = FLD_SET_DRF(_PERF, _PMASYS_SYS_FB_BLOCKER_CTRL,
                          _BLOCK_EN_CMD, _UNARMED, val);
        REG_WR32(FECS, LW_PERF_PMASYS_SYS_FB_BLOCKER_CTRL, val);
    }
#endif
}

/*!
 * @brief Initializes the idle mask for MS engine.
 */
void
msMscgIdleMaskInit_GA10X(void)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
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

    if (FIFO_IS_ENGINE_PRESENT(CE4))
    {
        im[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_4, _ENABLED, im[1]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_CE4,im);
    }

    //  LWDEC = BSP  (reusing MSVLD code for LWDEC)
    if (FIFO_IS_ENGINE_PRESENT(BSP))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWDEC, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_BSP, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(DEC1))
    {
        im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_LWDEC1, _ENABLED, im[2]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_DEC1, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(ENC0))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWENC0, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_ENC0, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(SEC2))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _SEC, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_SEC2, im);

        if (LPWR_CTRL_IS_SF_SUPPORTED(pPgState, MS, SEC2))
        {
            im[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _SEC_FBIF,
                                _ENABLED, im[1]);
            im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _SEC_PRIV_BLOCKER_MS,
                                _ENABLED, im[2]);

        }
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

    // Add the XVE Bar Blocker MS Idle signal
    im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _XVE_BAR_BLK_MS, _ENABLED, im[2]);

    pPgState->idleMask[0] = im[0];
    pPgState->idleMask[1] = im[1];
    pPgState->idleMask[2] = im[2];
}

/*!
 * @brief Initializes the idle mask for MS passive.
 */
void
msPassiveIdleMaskInit_GA10X(void)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_PASSIVE);
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

    if (FIFO_IS_ENGINE_PRESENT(CE4))
    {
        im[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_4, _ENABLED, im[1]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_CE4, im);
    }

    //  LWDEC = BSP  (reusing MSVLD code for LWDEC)
    if (FIFO_IS_ENGINE_PRESENT(BSP))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWDEC, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_BSP, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(DEC1))
    {
        im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_LWDEC1, _ENABLED, im[2]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_DEC1, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(ENC0))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWENC0, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_ENC0, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(SEC2))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _SEC, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_SEC2, im);

        if (LPWR_CTRL_IS_SF_SUPPORTED(pPgState, MS, SEC2))
        {
            im[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _SEC_FBIF,
                                _ENABLED, im[1]);
            im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _SEC_PRIV_BLOCKER_MS,
                                _ENABLED, im[2]);

        }
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

    // FBHUB_ALL only represents the NISO traffic.
    im[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _FBHUB_ALL, _ENABLED, im[1]);

    // Add the XVE Bar Blocker MS Idle signal
    im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _XVE_BAR_BLK_MS, _ENABLED, im[2]);

    pPgState->idleMask[0] = im[0];
    pPgState->idleMask[1] = im[1];
    pPgState->idleMask[2] = im[2];
}

/*!
 * @brief Configure entry conditions for MS-Passive
 *
 * If MS-Passive is coupled with GR features, then that GR feature
 * becomes a pre-condition for MS-Passive to engage
 */
void
msPassiveEntryConditionConfig_GA10X(void)
{
    OBJPGSTATE *pMsPassiveState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_PASSIVE);
    LwU32       val;
    LwU32       newVal;
    LwU32       lpwrCtrlId;
    LwU32       hwEngIdx;

    // Check for GR coupled MS_LTC support
    if (!IS_GR_DECOUPLED_MS_SUPPORTED(pMsPassiveState))
    {
        // Read child precondition register
        val = newVal = REG_RD32(CSB, LW_CPWR_PMU_PG_ON_TRIGGER_MASK(PG_ENG_IDX_ENG_MS_PASSIVE));

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
            REG_WR32(CSB, LW_CPWR_PMU_PG_ON_TRIGGER_MASK(PG_ENG_IDX_ENG_MS_PASSIVE), newVal);
        }
    }
}

/*!
 * @brief Initialize FB parameters
 */
void
msInitFbParams_GA10X(void)
{
    OBJSWASR *pSwAsr = MS_GET_SWASR();
    LwU32     val;

    val = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_BROADCAST);

    // Set the DRAM Type
    if (FLD_TEST_DRF(_PFB, _FBPA_FBIO_BROADCAST, _DDR_MODE,
                     _GDDR5, val) ||
        FLD_TEST_DRF(_PFB, _FBPA_FBIO_BROADCAST, _DDR_MODE,
                     _GDDR5X, val))
    {
        //
        // We support both GDDR5 and GDDR5x memory types. Therefor setting
        // this varible if memory type is either GDDR5 or GDDR5x.
        //
        // Also we have same SW ASR sequence for both GDDR5 and GDDR5X.
        //
        pSwAsr->bIsGDDRx = LW_TRUE;

        // We can use GDDR5 type for both GDDR5 and GDDR5X
        pSwAsr->dramType = LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR5;
    }
    else if (FLD_TEST_DRF(_PFB, _FBPA_FBIO_BROADCAST, _DDR_MODE,
                          _GDDR6, val) ||
             FLD_TEST_DRF(_PFB, _FBPA_FBIO_BROADCAST, _DDR_MODE,
                          _GDDR6X, val))
    {
        //
        // Ths flag is shared by GDDR5/5X/6/6x since some of the SW ASR
        // code is common accros GDDR5/5X/6/6x DRAM.
        //
        pSwAsr->bIsGDDRx = LW_TRUE;

        //
        // We need to have separate flag for GDDR6/GDDR6x to implement GDDR6 related
        // SW ASR chagnes which are different from GDDR5/5x.
        //
        // Since the self refresh sequence is common for GDDR6 and GDDR6X (Bug 200506766)
        // we can use the same falg for both type of memories.
        //
        pSwAsr->bIsGDDR6 = LW_TRUE;
        pSwAsr->dramType = LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR6;
    }
    else if (FLD_TEST_DRF(_PFB, _FBPA_FBIO_BROADCAST, _DDR_MODE,
                          _SDDR23, val))
    {
        pSwAsr->dramType = LW2080_CTRL_FB_INFO_RAM_TYPE_SDDR3;
    }
    else if (FLD_TEST_DRF(_PFB, _FBPA_FBIO_BROADCAST, _DDR_MODE,
                          _SDDR4, val))
    {
        pSwAsr->dramType = LW2080_CTRL_FB_INFO_RAM_TYPE_SDDR4;
    }

    pSwAsr->bIsGDDR3MirrorCmdMapping =
        (FLD_TEST_DRF(_PFB, _FBPA_FBIO_BROADCAST, _CMD_MAPPING,
                      _GDDR3_GT215_COMP_MIRR, val )             |
         FLD_TEST_DRF(_PFB, _FBPA_FBIO_BROADCAST, _CMD_MAPPING,
                      _GDDR3_GT215_COMP_MIRR1, val));

    if (PMUCFG_FEATURE_ENABLED(PMU_MS_HALF_FBPA))
    {
        // Determine if Half-FBPA is enabled on the system
        msInitHalfFbpaMask_HAL();
    }

    // Read the Serial priv control for FBIO
    pSwAsr->regs.fbpaSwConfig = REG_RD32(FECS, LW_PFB_FBPA_SW_CONFIG);

    //
    // Configure the FB_PA Idle signal to only include the L2-FB
    // traffic - Bug: 200667694 has the details.
    //
    msFbpaIdleConfig_HAL(&Ms);

    if (PMUCFG_FEATURE_ENABLED(PMU_MSCG_L2_CACHE_OPS))
    {
        val = REG_RD32(FECS, LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_L2);

        // Cache the Number of Active LTCs
        Ms.numLtc = DRF_VAL(_PPRIV, _MASTER_RING_ENUMERATE_RESULTS_L2, _COUNT, val);
    }
}

/*!
 * @brief Enable MISCIO MSCG interrutps
 *
 * Function to enable the various MISCIO interrupts The display stutter and
 * display blocker interrupts should only be enabled if display is enabled.
 */
void
msInitAndEnableIntr_GA10X(void)
{
    LwU32 val = 0;

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

    if (lpwrGrpCtrlIsSfSupported(RM_PMU_LPWR_GRP_CTRL_ID_MS,
                                 RM_PMU_LPWR_CTRL_FEATURE_ID_MS_HSHUB))
    {
        // HSHUB0 Blocker Interrupt
        val = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_RISING_EN,
                          _HSHUB_FB_BLOCKER, _ENABLED, val);

        // HSHUB1 Blocker Interrupt
        val = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_RISING_EN,
                          _HSHUB1_FB_BLOCKER, _ENABLED, val);
    }

    if (lpwrGrpCtrlIsSfSupported(RM_PMU_LPWR_GRP_CTRL_ID_MS,
                                 RM_PMU_LPWR_CTRL_FEATURE_ID_MS_SEC2))
    {
        val = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_RISING_EN,
                          _SEC_FB_BLOCKER, _ENABLED, val);
    }

    if (lpwrGrpCtrlIsSfSupported(RM_PMU_LPWR_GRP_CTRL_ID_MS,
                                 RM_PMU_LPWR_CTRL_FEATURE_ID_MS_GSP))
    {
        val = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_RISING_EN,
                          _GSP_FB_BLOCKER, _ENABLED, val);
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
msColwertIntrsToWakeUpMask_GA10X
(
    LwU32 intrStat
)
{
    LwU32 wakeUpMask = 0;

    if (PENDING_IO_BANK1(_PERF_FB_BLOCKER, _RISING, intrStat))
    {
        wakeUpMask |= PG_WAKEUP_EVENT_MS_PM_BLOCKER;
    }
    if (PENDING_IO_BANK1(_ISO_FB_BLOCKER, _RISING, intrStat))
    {
        wakeUpMask |= PG_WAKEUP_EVENT_MS_ISO_BLOCKER;
    }
    if (PENDING_IO_BANK1(_DNISO_FB_BLOCKER, _RISING, intrStat))
    {
        wakeUpMask |= PG_WAKEUP_EVENT_MS_DISPLAY_BLOCKER;
    }
    if (PENDING_IO_BANK1(_MSPG_WAKE, _RISING, intrStat))
    {
        wakeUpMask |= PG_WAKEUP_EVENT_MS_ISO_STUTTER;
    }
    if (PENDING_IO_BANK1(_HSHUB_FB_BLOCKER, _RISING, intrStat) ||
        PENDING_IO_BANK1(_HSHUB1_FB_BLOCKER, _RISING, intrStat))
    {
        wakeUpMask |= PG_WAKEUP_EVENT_MS_HSHUB_BLOCKER;
    }
    if (PENDING_IO_BANK1(_SEC_FB_BLOCKER, _RISING, intrStat))
    {
        wakeUpMask |= PG_WAKEUP_EVENT_SEC2_FB_BLOCKER;
    }
    if (PENDING_IO_BANK1(_GSP_FB_BLOCKER, _RISING, intrStat))
    {
        wakeUpMask |= PG_WAKEUP_EVENT_GSP_FB_BLOCKER;
    }

    return wakeUpMask;
}

/*!
 * @brief Function to engage/disengage MS SRAM Power Gating
 *
 * @param[in]  ctrlId   LPWR_CTRL Id requesting to execture the SRAM Sequencer
 *                      sequence with MS Group
 * @param[in]  bEngage  LW_TRUE - Engage the Sequencer
 *                      LW_FALSE - Disengage the Sequencer
 *
 *
 */
void
msSramPowerGatingEngage_GA10X
(
    LwU8   ctrlId,
    LwBool bEngage
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);

    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_SEQ) &&
        LPWR_SEQ_IS_SRAM_CTRL_SUPPORTED(LPWR_SEQ_SRAM_CTRL_ID_MS))
    {
        if (bEngage)
        {
            if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, RPG))
            {

                lpwrSeqSramStateSet(LPWR_SEQ_SRAM_CTRL_ID_MS,
                                    LPWR_SEQ_STATE_SRAM_RPG, LW_FALSE);
            }
            else if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, RPPG))
            {
                lpwrSeqSramStateSet(LPWR_SEQ_SRAM_CTRL_ID_MS,
                                    LPWR_SEQ_STATE_SRAM_RPPG, LW_FALSE);
            }
        }
        else
        {
            if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, RPPG) ||
                LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, RPG))
            {
                lpwrSeqSramStateSet(LPWR_SEQ_SRAM_CTRL_ID_MS,
                                    LPWR_SEQ_STATE_PWR_ON, LW_TRUE);
            }
        }
    }
}

/*!
 * Change the L2 Cache Dirty Data Configuration
 *
 *  1. Disable the Dirty Data Cleaning based on threshold.
 *  2. Increase the Dirty Data threshould in L2 Sets
 *
 * @param[in]   ctrlId  LPWR_CTRL Id
 *
 */
FLCN_STATUS
msDifrSwAsrLtcEntry_GA10X
(
    LwU8 ctrlId
)
{
    LwU32 regVal;

    regVal = REG_RD32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_3);

    // Cache the MGMT_3 register
    Ms.pMsDifr->l2CacheReg.l2CachSetMgmt3 = regVal;

    // Disable the DIRTY DATA Cleaning.
    regVal = REG_RD32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_3);
    regVal = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_TSTG_SET_MGMT_3,
                         _DONT_CLEAN_OLDEST_DIRTY, _ENABLED, regVal);
    REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_3, regVal);

    regVal = REG_RD32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_5);

    // Cache the MGMT_5 register
    Ms.pMsDifr->l2CacheReg.l2CachSetMgmt5 = regVal;

    // Update the TIER1 and TIER 2 settings
    regVal = FLD_SET_DRF_NUM(_PLTCG, _LTCS_LTSS_TSTG_SET_MGMT_5,
                             _TIER1_CLEAN_THRESHOLD,
                             MS_DIFR_L2_CACHE_SET_TIER1_DIRTY_THRESHOLD, regVal);
    regVal = FLD_SET_DRF_NUM(_PLTCG, _LTCS_LTSS_TSTG_SET_MGMT_5,
                             _TIER2_CLEAN_THRESHOLD,
                             MS_DIFR_L2_CACHE_SET_TIER2_DIRTY_THRESHOLD, regVal);
    REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_5, regVal);

    regVal = REG_RD32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_1);
    // Cache the MGMT_1 register
    Ms.pMsDifr->l2CacheReg.l2CachSetMgmt1 = regVal;

    // Update the PLC THRESHOULD settings
    regVal = FLD_SET_DRF_NUM(_PLTCG, _LTCS_LTSS_TSTG_SET_MGMT_1,
                             _PLC_THRESHOLD,
                             MS_DIFR_L2_CACHE_SET_PLC_THRESHOLD, regVal);
    REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_1, regVal);

    return FLCN_OK;
}

/*!
 * Restore the L2 Cache Dirty Data Configuration
 *
 * @param[in]   ctrlId  LPWR_CTRL Id
 *
 */
void
msDifrSwAsrLtcExit_GA10X
(
    LwU8 ctrlId
)
{
    // Restore the PLC Threshould Settings
    REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_1,
             Ms.pMsDifr->l2CacheReg.l2CachSetMgmt1);

    // Restore the TIER1 and TIER2 Settings
    REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_5,
             Ms.pMsDifr->l2CacheReg.l2CachSetMgmt5);

    // Resotre the DIFR DATA Cleaning settings
    REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_3,
             Ms.pMsDifr->l2CacheReg.l2CachSetMgmt3);
}

/*!
 * Flush L2 and poll until L2 is done
 *
 * @param[in]   pBlockStartTimeNs  Start time of the block sequence
 * @param[out]  pTimeoutLeftNs     Time left in the block sequence
 */
FLCN_STATUS
msL2FlushAndIlwalidate_GA10X
(
    LwU8            ctrlId,
    FLCN_TIMESTAMP *pBlockStartTimeNs,
    LwS32          *pTimeoutLeftNs
)
{
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    OBJPGSTATE    *pPgState = GET_OBJPGSTATE(ctrlId);
#endif
    FLCN_TIMESTAMP abortFlushStartTimeNs;
    LwU32          regVal;
    LwS32          timeoutLeftNs = L2_CACHE_OPS_ABORT_TIMEOUT_NS;

    regVal = REG_RD32(CSB, LW_CPWR_PMU_IDLE_STATUS);

    //
    // Check for L2 Idleness to make sure there is no pending L2
    // Operations(Flush/Ilwalidate/Clean)
    //
    if (FLD_TEST_DRF(_CPWR_PMU, _IDLE_STATUS, _LTC_IDLE, _BUSY, regVal))
    {
        return FLCN_ERROR;
    }

    //
    // 15) Section 4.5.1 in WSP IAS:
    // PMU writes a register in L2 to flush L2.
    //
    REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_G_ELPG,
                      FLD_SET_DRF(_PLTCG, _LTCS_LTSS_G_ELPG, _FLUSH, _PENDING,
                      REG_RD32(FECS, LW_PLTCG_LTCS_LTSS_G_ELPG)));

    // Start the flush timer
    osPTimerTimeNsLwrrentGet(&abortFlushStartTimeNs);
    //
    // 16.1) Section 4.5.1 in WSP IAS:
    // PMU polls a register in L2 to make sure L2 is flushed.
    // Bug 746537: During a broadcast read from LW_PLTCG_LTCS_LTSS_G_ELPG,
    // the HW will issue a read to each unicast address and OR the read data.
    //
    do
    {
        if (FLD_TEST_DRF(_PLTCG, _LTCS_LTSS_G_ELPG, _FLUSH, _PENDING,
                         REG_RD32(FECS, LW_PLTCG_LTCS_LTSS_G_ELPG)))
        {
            // Check for timeout for L2 FLUSH
            if (msCheckTimeout(L2_CACHE_OPS_ABORT_TIMEOUT_NS, &abortFlushStartTimeNs,
                               &timeoutLeftNs))
            {
                return FLCN_ERROR;
            }
        }
        else
        {
            break;
        }
    }
    while(1);

    // Check if still good to proceed further
    if (pgCtrlIdleFlipped_HAL(&Pg, ctrlId) ||
        msCheckTimeout(Ms.abortTimeoutNs, pBlockStartTimeNs, pTimeoutLeftNs))
    {
        return FLCN_ERROR;
    }

#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    // If MS-RPG is enabled, we need to ilwalidate the L2 Cache contents as well.
    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, RPG))
    {
        // Ilwalidate the Compression Bits Cache
        regVal = REG_RD32(FECS, LW_PLTCG_LTCS_LTSS_CBC_CTRL_1);
        regVal = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_CBC_CTRL_1, _CLEAN, _INACTIVE, regVal);
        regVal = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_CBC_CTRL_1, _ILWALIDATE, _ACTIVE, regVal);
        regVal = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_CBC_CTRL_1, _CLEAR, _INACTIVE, regVal);
        regVal = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_CBC_CTRL_1, _DISABLE_ILWALIDATE, _INACTIVE, regVal);
        REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_CBC_CTRL_1, regVal);
        regVal = REG_RD32(FECS, LW_PLTCG_LTCS_LTSS_CBC_CTRL_1);

        // Ilwalidate the Data Cache
        regVal = REG_RD32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_CMGMT_0);
        regVal = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_TSTG_CMGMT_0, _ILWALIDATE, _PENDING, regVal);
        regVal = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_TSTG_CMGMT_0, _MAX_CYCLES_BETWEEN_ILWALIDATES, _3, regVal);
        regVal = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_TSTG_CMGMT_0, _ILWALIDATE_EVICT_LAST_CLASS,   _TRUE, regVal);
        regVal = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_TSTG_CMGMT_0, _ILWALIDATE_EVICT_NORMAL_CLASS, _TRUE, regVal);
        regVal = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_TSTG_CMGMT_0, _ILWALIDATE_EVICT_FIRST_CLASS,  _TRUE, regVal);
        regVal = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_TSTG_CMGMT_0, _DISABLE_ILWALIDATE, _INACTIVE, regVal);
        REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_CMGMT_0, regVal);
        regVal = REG_RD32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_CMGMT_0);

        // Poll for Ilwalidate to finish
        do
        {
            if (FLD_TEST_DRF(_CPWR_PMU, _IDLE_STATUS, _LTC_IDLE, _BUSY,
                             REG_RD32(CSB, LW_CPWR_PMU_IDLE_STATUS)))
            {
                // Check for timeout for L2 FLUSH ABort
                if (msCheckTimeout(L2_CACHE_OPS_ABORT_TIMEOUT_NS,
                                   &abortFlushStartTimeNs, &timeoutLeftNs))
                {
                    return FLCN_ERROR;
                }
            }
            else
            {
                break;
            }
        }
        while(1);
    }
#endif  // (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    return FLCN_OK;
}

/*!
 * Wait for L2 Flush to finish
 */
void
msWaitForL2FlushFinish_GA10X(void)
{
    FLCN_TIMESTAMP flushWaitTimeNs;

    // Start the flush timer
    osPTimerTimeNsLwrrentGet(&flushWaitTimeNs);

    do
    {
        // Wait for L2 Flush to finish
        if (!FLD_TEST_DRF(_PLTCG, _LTCS_LTSS_G_ELPG, _FLUSH, _PENDING,
                          REG_RD32(FECS, LW_PLTCG_LTCS_LTSS_G_ELPG)))
        {
            break;
        }

        // Wait for max 1mS
        if (osPTimerTimeNsElapsedGet(&flushWaitTimeNs) >=
                                     (10 * L2_CACHE_OPS_ABORT_TIMEOUT_NS))
        {
            PMU_BREAKPOINT();
            break;
        }
    }
    while(1);
}

/*!
 * @brief Wait till memory subsystem goes idle
 *
 * Function polls  LTC idle status to make sure FB subsystem is idle.
 * Also it ensure that all FB sub-units are idle.
 *
 * @param[in]   ctrlId            LPWR_CTRL Id from MS Group
 * @param[in]   pBlockStartTimeNs  Start time of the block sequence
 * @param[out]  pTimeoutLeftNs     Time left in the block sequence
 *
 * @return      FLCN_OK     FB sub-system is idle
 *              FLCN_ERROR  FB sub-system is not idle
 */
FLCN_STATUS
msWaitForSubunitsIdle_GA10X
(
    LwU8            ctrlId,
    PFLCN_TIMESTAMP pBlockStartTimeNs,
    LwS32          *pTimeoutLeftNs
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);
    LwBool      bIdle    = LW_FALSE;
    LwU32       idleStatus;
    LwU32       idleStatus1;

    //
    // Poll for LTC idle. On GA10X, we are removing the FB_PA idle
    // check as we relying on the fact that LTC idle covers all the
    // read/write request from end user
    //
    // Details are in the Bug: 200638885
    //
    do
    {
        idleStatus = REG_RD32(CSB, LW_CPWR_PMU_IDLE_STATUS);

        bIdle = FLD_TEST_DRF(_CPWR_PMU, _IDLE_STATUS, _LTC_IDLE,
                             _IDLE, idleStatus);

        if (pgCtrlIdleFlipped_HAL(&Pg, ctrlId) ||
            msCheckTimeout(MS_DEFAULT_ABORT_TIMEOUT1_NS,
                           pBlockStartTimeNs, pTimeoutLeftNs))
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
    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, HSHUB))
    {
        bIdle = bIdle                                                    &&
                FLD_TEST_DRF(_CPWR_PMU, _IDLE_STATUS, _HSHUB_ALL, _IDLE,
                             idleStatus)                                 &&
                FLD_TEST_DRF(_CPWR_PMU, _IDLE_STATUS_1, _HSHUB_MMU, _IDLE,
                             idleStatus1);
    }

    if (!bIdle)
    {
       return FLCN_ERROR;
    }

    return FLCN_OK;
}

/*!
 * @brief Function to set Idle mask for DIFR SW_ASR - FSM
 */
void
msDifrIdleMaskInit_GA10X
(
    LwU8 ctrlId
)
{
    OBJPGSTATE *pPgState                     = GET_OBJPGSTATE(ctrlId);
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

    if (FIFO_IS_ENGINE_PRESENT(CE4))
    {
        im[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_4, _ENABLED, im[1]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_CE4,im);
    }

    //  LWDEC = BSP  (reusing MSVLD code for LWDEC)
    if (FIFO_IS_ENGINE_PRESENT(BSP))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWDEC, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_BSP, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(DEC1))
    {
        im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_LWDEC1, _ENABLED, im[2]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_DEC1, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(ENC0))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWENC0, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_ENC0, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(SEC2))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _SEC, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_SEC2, im);

        if (LPWR_CTRL_IS_SF_SUPPORTED(pPgState, MS, SEC2))
        {
            im[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _SEC_FBIF,
                                _ENABLED, im[1]);
            im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _SEC_PRIV_BLOCKER_MS,
                                _ENABLED, im[2]);

        }
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

    // Add the XVE Bar Blocker MS Idle signal
    im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _XVE_BAR_BLK_MS, _ENABLED, im[2]);

    pPgState->idleMask[0] = im[0];
    pPgState->idleMask[1] = im[1];
    pPgState->idleMask[2] = im[2];
}

/*!
 * @brief Function to issue the L2 Cache Ops
 *
 * Note: This function must be exelwted by LPWR_LP Task only
 * under the mutex
 */
FLCN_STATUS
msLtcCacheOps_GA10X
(
    LwU8    l2CacheOps
)
{
    FLCN_TIMESTAMP pollStartTimeNs;
    FLCN_STATUS    status = FLCN_OK;
    LwU32          regAddrRead;
    LwU32          regAddrWrite;
    LwU32          valueMask;
    LwU32          pollValue;
    LwU32          writeValue = 0;
    LwU32          regVal;
    LwU32          idx;
    LwU32          ltcOffset;
    LwBool         bStopPolling;

    switch (l2CacheOps)
    {
        // Clean the CBC Cache
        case MS_L2_CACHE_CBC_CLEAN:
        {
            regAddrRead  = LW_PLTCG_LTC0_LTSS_CBC_CTRL_1;
            regAddrWrite = LW_PLTCG_LTCS_LTSS_CBC_CTRL_1;
            valueMask    = DRF_SHIFTMASK(LW_PLTCG_LTCS_LTSS_CBC_CTRL_1_CLEAN);
            pollValue    = DRF_DEF(_PLTCG, _LTCS_LTSS_CBC_CTRL_1, _CLEAN, _INACTIVE);
            writeValue   = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_CBC_CTRL_1, _CLEAN, _ACTIVE, writeValue);
            break;
        }

        // Clean the Data Bits Cache
        case MS_L2_CACHE_DATA_CLEAN:
        {
            regAddrRead = LW_PLTCG_LTC0_LTSS_TSTG_CMGMT_1;
            regAddrWrite = LW_PLTCG_LTCS_LTSS_TSTG_CMGMT_1;
            valueMask    = DRF_SHIFTMASK(LW_PLTCG_LTCS_LTSS_TSTG_CMGMT_1_CLEAN);
            pollValue    = DRF_DEF(_PLTCG, _LTCS_LTSS_TSTG_CMGMT_1, _CLEAN, _NOT_PENDING);

            writeValue = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_TSTG_CMGMT_1, _CLEAN, _PENDING, writeValue);
            writeValue = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_TSTG_CMGMT_1, _CLEAN_EVICT_LAST_CLASS, _TRUE, writeValue);
            writeValue = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_TSTG_CMGMT_1, _CLEAN_EVICT_NORMAL_CLASS, _TRUE, writeValue);
            writeValue = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_TSTG_CMGMT_1, _CLEAN_EVICT_FIRST_CLASS, _TRUE, writeValue);

            break;
        }

        default:
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto msLtcCacheOps_GA10X_exit;
            break;
        }
    }

    //
    // Check if L2 Cache ops is arelady pending or not
    //
    // Read all the LTC Slices to make sure there is no existing Pending operation.
    for (idx = 0; idx < Ms.numLtc; idx++)
    {
        // get offset for this ltc
        ltcOffset = LW_LTC_PRI_STRIDE * idx;

        regVal = REG_RD32(FECS, (regAddrRead + ltcOffset));

        // Check if requested operation is already pending, if so, bail out
        if ((regVal & valueMask) != pollValue)
        {
            status = FLCN_ERROR;

            goto msLtcCacheOps_GA10X_exit;
        }
    }

    // Issue the Cache Ops.
    regVal = REG_RD32(FECS, regAddrWrite);
    regVal = regVal | writeValue;
    REG_WR32(FECS, regAddrWrite, regVal);

    // Start timer for Operation to finish
    osPTimerTimeNsLwrrentGet(&pollStartTimeNs);

    // Read all the LTC Slices to make sure there is no existing Pending operation.
    for (idx = 0; idx < Ms.numLtc; idx++)
    {
        // get offset for this ltc
        ltcOffset = LW_LTC_PRI_STRIDE * idx;

        bStopPolling = LW_FALSE;

        // Poll for Demote to finish for each LTC in time.
        do
        {
            regVal = REG_RD32(FECS, (regAddrRead + ltcOffset));

            // Check Clean operation is pending or not on each LTC.

            if ((regVal & valueMask) == pollValue)
            {
                bStopPolling = LW_TRUE;
            }

            // Check for timeout value
            if ((osPTimerTimeNsElapsedGet(&pollStartTimeNs) >=
                                         L2_CACHE_OPS_ABORT_TIMEOUT_NS))
            {
                status = FLCN_ERR_TIMEOUT;
                goto msLtcCacheOps_GA10X_exit;
            }
        }
        while (!bStopPolling);
    }

msLtcCacheOps_GA10X_exit:

    return status;
}

/*!
 * @brief Function to execute the MS-RPG Pre entry Sequence
 *
 * Note: This function must be exelwted by LPWR_LP Task only
 */
FLCN_STATUS
msRpgPreEntrySeq_GA10X(void)
{
    FLCN_STATUS status               = FLCN_OK;
    LwU32       regVal;
    LwBool      bTier2SettingRestore = LW_FALSE;
    LwBool      bMutexAcquired       = LW_FALSE;


    //
    // Acquire the Mutex, if we failed to acquire the mutex, then MS-RPG
    // will not engage in the current cycle
    //
    status = mutexAcquire(LW_MUTEX_ID_GR, MS_RPG_MUTEX_ACQUIRE_TIMEOUT_NS, &Ms.pMsRpg->mutexToken);
    if (status != FLCN_OK)
    {
        goto msRpgPreEntrySeq_GA10X_exit;
    }
    else
    {
        bMutexAcquired = LW_TRUE;
    }


    //
    // Step 1: update the TIER2 Threshold to zero
    // Step 2: Clean the Compression Cache and poll for completion.
    // Step 3: Clean the Data Cache and poll for completion.
    //


    // Step 1: Update the Teir2 Threshould to zero
    regVal = REG_RD32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_5);

    // Cache the MGMT_5 register
    Ms.pMsRpg->l2CachSetMgmt5 = regVal;

    // Update the TIER 2 settings to zero
    regVal = FLD_SET_DRF_NUM(_PLTCG, _LTCS_LTSS_TSTG_SET_MGMT_5,
                             _TIER2_CLEAN_THRESHOLD,
                             0, regVal);
    REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_5, regVal);

    //
    // Set the flag so that we can restore the Tier2 settings in case
    // of failure
    //
    bTier2SettingRestore = LW_TRUE;

    // Step 2: Clean the Compression Bits Cache
    status = msLtcCacheOps_HAL(&Ms, MS_L2_CACHE_CBC_CLEAN);
    if (status != LW_OK)
    {
        goto msRpgPreEntrySeq_GA10X_exit;
    }

    // Step 3: Clean the Data Bits Cache
    status = msLtcCacheOps_HAL(&Ms, MS_L2_CACHE_DATA_CLEAN);

msRpgPreEntrySeq_GA10X_exit:

    if (status != FLCN_OK)
    {
        // Restore Teir2 Settings
        if (bTier2SettingRestore)
        {
            REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_5, Ms.pMsRpg->l2CachSetMgmt5);
        }
    }

    // If Mutex was acquired, please release the mutex
    if (bMutexAcquired)
    {
        mutexRelease(LW_MUTEX_ID_GR, Ms.pMsRpg->mutexToken);
    }

    return status;
}

/*!
 * @brief Function to execute the MS-RPG Post Exit Sequence
 */
void
msRpgPostExitSeq_GA10X(void)
{
    REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_5, Ms.pMsRpg->l2CachSetMgmt5);
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Reads the register to get the number of HSHUBs
 *
 * @return No. of HSHUBs
 */
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
static LwU32
s_msNumHshubsGet_GA10X(void)
{
    LwU32 reg = REG_RD32(FECS, LW_PFB_HSHUB_PRG_CONFIG(0));
    return (DRF_VAL(_PFB_HSHUB, _PRG_CONFIG, _NUM_HSHUBS, reg));
}
#endif
/*!
 * @brief Engage/Disengage the CLK Slowdown for SYS and LWD Clk
 *
 * @param[in]  ctrlId   LPWR_CTRL Id requesting Clock Slowdown
 * @param[in]  bEngage LW_TRUE -> Slowdown the clocks
 *                      LW_FALSE -> Restore the original value of clk
 */
void
msClkSlowDownEngage_GA10X
(
    LwU8   ctrlId,
    LwBool bEngage
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);
    LwU32       regVal   = 0;

    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, CLK_SLOWDOWN_SYS))
    {
        regVal = REG_RD32(FECS, LW_PTRIM_SYS_SYSCLK_OUT_SWITCH_DIVIDER);

        if (bEngage)
        {
            // Cache the original Value
            Ms.pClkGatingRegs->sysClkLdivConfig = regVal;

            // Slow down the clock by x4
            regVal = FLD_SET_DRF(_PTRIM_SYS, _SYSCLK_OUT_SWITCH_DIVIDER, _DIVIDER_SEL,
                                 _BY4, regVal);
        }
        else
        {
            // restore the original value;
            regVal = Ms.pClkGatingRegs->sysClkLdivConfig;
        }

        REG_WR32(FECS, LW_PTRIM_SYS_SYSCLK_OUT_SWITCH_DIVIDER, regVal);
        // Read back the register to flush it
        REG_RD32(FECS, LW_PTRIM_SYS_SYSCLK_OUT_SWITCH_DIVIDER);
    }

    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, CLK_SLOWDOWN_LWD))
    {
        regVal = REG_RD32(FECS, LW_PTRIM_SYS_LWDCLK_OUT_SWITCH_DIVIDER);

        if (bEngage)
        {
            // Cache the original Value
            Ms.pClkGatingRegs->lwdClkLdivConfig = regVal;

            // Slow down the clock by x4
            regVal = FLD_SET_DRF(_PTRIM_SYS, _SYSCLK_OUT_SWITCH_DIVIDER, _DIVIDER_SEL,
                                 _BY4, regVal);
        }
        else
        {
            // restore the original value;
            regVal = Ms.pClkGatingRegs->lwdClkLdivConfig;
        }

        REG_WR32(FECS, LW_PTRIM_SYS_LWDCLK_OUT_SWITCH_DIVIDER, regVal);
        // Read back the register to flush it
        REG_RD32(FECS, LW_PTRIM_SYS_LWDCLK_OUT_SWITCH_DIVIDER);
    }
}
