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
 * @file   pmu_mstu10x.c
 * @brief  HAL-interface for the TU10X Memory System Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"
#include "dev_master.h"
#include "dev_disp.h"
#include "dev_perf.h"
#include "dev_trim.h"
#if (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
#include "dev_lw_xve.h"
#endif // (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
#include "dev_bus.h"
#include "dev_sec_pri.h"
#include "dev_gsp.h"
#include "dev_ram.h"
#include "dev_fbpa.h"
#include "dev_fuse.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_ltc.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objlpwr.h"
#include "pmu_objms.h"
#include "pmu_objfifo.h"
#include "pmu_objic.h"
#include "ctrl/ctrl2080/ctrl2080fb.h"

#include "config/g_ms_private.h"

//
// Compile time check to ensure that SEC and GSP Priv blocker has same defines
// for Blocker mode.
//
ct_assert(LW_PSEC_PRIV_BLOCKER_CTRL_BLOCKMODE_BLOCK_NONE ==
                                LW_PGSP_PRIV_BLOCKER_CTRL_BLOCKMODE_BLOCK_NONE);
ct_assert(LW_PSEC_PRIV_BLOCKER_CTRL_BLOCKMODE_BLOCK_GR ==
                                LW_PGSP_PRIV_BLOCKER_CTRL_BLOCKMODE_BLOCK_GR);
ct_assert(LW_PSEC_PRIV_BLOCKER_CTRL_BLOCKMODE_BLOCK_MS ==
                                LW_PGSP_PRIV_BLOCKER_CTRL_BLOCKMODE_BLOCK_MS);
ct_assert(LW_PSEC_PRIV_BLOCKER_CTRL_BLOCKMODE_BLOCK_GR_MS ==
                                LW_PGSP_PRIV_BLOCKER_CTRL_BLOCKMODE_BLOCK_GR_MS);
ct_assert(LW_PSEC_PRIV_BLOCKER_CTRL_BLOCKMODE_BLOCK_ALL ==
                                LW_PGSP_PRIV_BLOCKER_CTRL_BLOCKMODE_BLOCK_ALL);

//
// Compile time check to ensure that All LowPower feature which uses LPWR_ENG
// and are precondition for MSCG to engage, should be using the LPWR_ENG number
// 0 to 3 only. MSCG usages the LPWR_ENG number 4
//
ct_assert(PG_ENG_IDX_ENG_GR    < LW_CPWR_PMU_PG_ON_TRIGGER_MASK_ENG_OFF_AND__SIZE_1);
ct_assert(PG_ENG_IDX_ENG_GR_RG < LW_CPWR_PMU_PG_ON_TRIGGER_MASK_ENG_OFF_AND__SIZE_1);

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* --------------------------- Prototypes ------------------------------------*/
/* ------------------------ Public Functions  ------------------------------- */

/*!
 * @brief Initializes the Holdoff mask for MS engine.
 */
void
msHoldoffMaskInit_TU10X(void)
{
    OBJPGSTATE *pMsState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
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
    if (FIFO_IS_ENGINE_PRESENT(CE4))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE4));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE5))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE5));
    }
    //  LWDEC = BSP  (reusing MSVLD code for LWDEC)
    if (FIFO_IS_ENGINE_PRESENT(BSP))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_BSP));
    }
    if (FIFO_IS_ENGINE_PRESENT(DEC1))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_DEC1));
    }
    if (FIFO_IS_ENGINE_PRESENT(DEC2))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_DEC2));
    }
    if (FIFO_IS_ENGINE_PRESENT(ENC0))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_ENC0));
    }
    if (FIFO_IS_ENGINE_PRESENT(ENC1))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_ENC1));
    }
    if (FIFO_IS_ENGINE_PRESENT(ENC2))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_ENC2));
    }
    if (FIFO_IS_ENGINE_PRESENT(SEC2))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_SEC2));
    }

    pMsState->holdoffMask = hm;
}

/*!
 * @brief Enable MISCIO MSCG interrutps
 *
 * Function to enable the various MISCIO interrupts The display stutter and
 * display blocker interrupts should only be enabled if display is enabled.
 */
void
msInitAndEnableIntr_TU10X(void)
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
        val = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_RISING_EN,
                          _HSHUB_FB_BLOCKER, _ENABLED, val);
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
 * @brief Initialize FB parameters
 */
void
msInitFbParams_TU10X(void)
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
                          _GDDR6, val))
    {
        //
        // Ths flag is shared by GDDR5/5X/6 since some of the SW ASR
        // code is common accros GDDR5/5X/6 DRAM.
        //
        pSwAsr->bIsGDDRx = LW_TRUE;

        //
        // We need to have separate flag for GDDR6 to implement GDDR6 related
        // SW ASR chagnes which are different from GDDR5/5x.
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
msColwertIntrsToWakeUpMask_TU10X
(
    LwU32 intrStat
)
{
    LwU32 wakeUpMask = 0;

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
    if (PENDING_IO_BANK1(_HSHUB_FB_BLOCKER, _RISING, intrStat))
    {
        wakeUpMask |= PG_WAKEUP_EVENT_MS_HSHUB_BLOCKER;
    }
    if (PENDING_IO_BANK1(_SEC_FB_BLOCKER,   _RISING, intrStat))
    {
        wakeUpMask |= PG_WAKEUP_EVENT_SEC2_FB_BLOCKER;
    }
    if (PENDING_IO_BANK1(_GSP_FB_BLOCKER,   _RISING, intrStat))
    {
        wakeUpMask |= PG_WAKEUP_EVENT_GSP_FB_BLOCKER;
    }

    return wakeUpMask;
}

/*!
 * @brief Wait till memory subsystem goes idle
 *
 * Function polls FBPA and LTC idle status to make sure FB subsystem is idle.
 * Also it ensure that all FB sub-units are idle.
 *
 * @param[in]    ctrlId            LPWR_CTRL Id from MS Group
 * @param[in]   pBlockStartTimeNs  Start time of the block sequence
 * @param[out]  pTimeoutLeftNs     Time left in the block sequence
 *
 * @return      FLCN_OK     FB sub-system is idle
 *              FLCN_ERROR  FB sub-system is not idle
 */
FLCN_STATUS
msWaitForSubunitsIdle_TU10X
(
    LwU8            ctrlId,
    FLCN_TIMESTAMP *pBlockStartTimeNs,
    LwS32          *pTimeoutLeftNs
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);
    LwBool      bIdle    = LW_FALSE;
    LwU32       idleStatus;
    LwU32       idleStatus1;

    // Poll for FBPA and LTC idle
    do
    {
        idleStatus = REG_RD32(CSB, LW_CPWR_PMU_IDLE_STATUS);

        bIdle = FLD_TEST_DRF(_CPWR_PMU, _IDLE_STATUS, _FB_PA,
                             _IDLE, idleStatus) &&
                FLD_TEST_DRF(_CPWR_PMU, _IDLE_STATUS, _LTC_IDLE,
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
msIsoBlockerEntry_TU10X
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
                 (LwU32) *pTimeoutLeftNs,
                 PMU_REG_POLL_MODE_CSB_EQ))
        {
            *pAbortReason = MS_ABORT_REASON_DRAINING_TIMEOUT;
            return FLCN_ERROR;
        }

        //
        // Engage the ISO Blocker since now display is draining mode, so its
        // not requesting new data from FB.
        //
        val = REG_RD32(FECS, LW_PDISP_IHUB_COMMON_FB_BLOCKER_CTRL);
        val = FLD_SET_DRF(_PDISP, _IHUB_COMMON_FB_BLOCKER_CTRL,
                          _BLOCK_EN_CMD, _ARMED, val);
        REG_WR32(FECS, LW_PDISP_IHUB_COMMON_FB_BLOCKER_CTRL, val);

        if (msCheckTimeout(MS_DEFAULT_ABORT_TIMEOUT1_NS,
                           pBlockStartTimeNs, pTimeoutLeftNs) ||
            !PMU_REG32_POLL_NS(USE_FECS(LW_PDISP_IHUB_COMMON_FB_BLOCKER_CTRL),
                DRF_SHIFTMASK(LW_PDISP_IHUB_COMMON_FB_BLOCKER_CTRL_BLOCK_EN_STATUS),
                DRF_DEF(_PDISP, _IHUB_COMMON_FB_BLOCKER_CTRL, _BLOCK_EN_STATUS,
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
msIsoBlockerExit_TU10X
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
        val = REG_RD32(FECS, LW_PDISP_IHUB_COMMON_FB_BLOCKER_CTRL);
        val = FLD_SET_DRF(_PDISP, _IHUB_COMMON_FB_BLOCKER_CTRL,
                          _BLOCK_EN_CMD, _UNARMED, val);
        REG_WR32(FECS, LW_PDISP_IHUB_COMMON_FB_BLOCKER_CTRL, val);

        // Return the display mempool to force_fill mode.
        msDispForceFillEnable_HAL(&Ms, LW_TRUE);
    }
}

/*!
 * @brief Configure entry conditions for MS
 *
 * Helper to program entry conditions for MS Group LPWR_ENG(MS). MS has two
 * HW entry conditions -
 * 1) GR Feature is engaged            AND
 * 2) ISO Stutter i.e. MSPG_OK is asserted
 */
void
msEntryConditionConfig_TU10X
(
    LwU8 ctrlId
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);
    LwU32       val;
    LwU32       lpwrCtrlId;
    LwU32       parentHwEngIdx;
    LwU32       childHwEngIdx = PG_GET_ENG_IDX(ctrlId);

    //
    // GR Pre conditions for MS Based Features
    //

    //
    // Check for GR coupled MS support
    //
    // Note:  For DIFR Layer 2/Layer 3, we are not making GR as precondition because
    // DIFR Layer 1 (DFPR) is precondtion for Layer 2 and GR is already a
    // precondition for DFPR.
    //
    if ((!IS_GR_DECOUPLED_MS_SUPPORTED(pPgState)) &&
        (ctrlId != RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR) &&
        (ctrlId != RM_PMU_LPWR_CTRL_ID_MS_DIFR_CG))
    {
        //
        // GR coupled MS: GR is pre-condtion for MS
        //
        // Parent HW ENG IDX: PG_ENG_IDX_ENG_GR or PG_ENG_IDX_ENG_GR_RG
        // Child HW ENG IDX : PG_ENG_IDX_ENG_MS
        //
        // So the final pre-condition logic for MSCG is =
        // (ENG_GR_PG engaged || ENG_GR_RG engaged || GR_PASSIVE engaged) && MSPG_OK
        //
        // So we will move the ENG_GR_PG|GR_RG|GR_PASSIVE condition to
        // GROUP_0 and this group will be ANDED with MSPG_OK
        //
        // i.e.
        // Final Condition = GROUP_0 && MSPG_OK
        //
        // where GROUP_0 = ENG_GR_PG engaged || ENG_GR_RG engaged || GR_PASSIVE engaged
        //
        // Steps to update pre-condtion
        // Step1: Read child precondition register
        //        LW_CPWR_PMU_PG_ON_TRIGGER_MASK(ChildIdx) or
        //        LW_CPWR_PMU_PG_ON_TRIGGER_MASK_1(ChildIdx)
        //        LW_CPWR_PMU_PG_ON_TRIGGER_MASK_2(ChildIdx)
        // Step2: Set parent fields in child pre-condition register
        //        LW_CPWR_PMU_PG_ON_TRIGGER_MASK_ENG(ParentIdx1) OR
        //        LW_CPWR_PMU_PG_ON_TRIGGER_MASK_ENG(ParentIdx2)
        // Step3: Update child pre-condition register
        //

        FOR_EACH_INDEX_IN_MASK(32, lpwrCtrlId, lpwrGrpCtrlMaskGet(RM_PMU_LPWR_GRP_CTRL_ID_GR))
        {
            parentHwEngIdx = PG_GET_ENG_IDX(lpwrCtrlId);

            // Configure the Parent Child dependencies
            lpwrFsmDependencyInit_HAL(&Lpwr, parentHwEngIdx, childHwEngIdx);
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }

    // Display ISO HUB Signal Pre conditions

    //
    // Check for ISO stutter support
    // Note: For DIFR Layer 2, we are also having ISO_STUTTER SFM Bit
    // because it will monitor the MSPG_OK Signal from ISOHUB
    //
    if (LPWR_CTRL_IS_SF_SUPPORTED(pPgState, MS, ISO_STUTTER))
    {
        // Read child precondition register
        val = REG_RD32(CSB, LW_CPWR_PMU_PG_ON_TRIGGER_MASK_2(childHwEngIdx));

        // Set parent fields in child pre-condition register - AND condition
        val = FLD_SET_DRF(_CPWR_PMU, _PG_ON_TRIGGER_MASK_2,
                          _MSPG_OK_AND, _ENABLE, val);

        // Update child pre-condition register
        REG_WR32(CSB, LW_CPWR_PMU_PG_ON_TRIGGER_MASK_2(childHwEngIdx), val);
    }

    //
    // Extra Preconditions for DIFR Layer 2
    // Layer 2 depends on Layer 1 FSM. This is by design.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_MS_DIFR_SW_ASR) &&
        (ctrlId == RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR))
    {
        // Make Sure DFPR is supported
        if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_DFPR)  &&
            pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_DFPR) &&
            LPWR_ARCH_IS_SF_SUPPORTED(DFPR_COUPLED_DIFR_SW_ASR))
        {
            // Configure the Parent Child dependencies
            lpwrFsmDependencyInit_HAL(&Lpwr, PG_GET_ENG_IDX(RM_PMU_LPWR_CTRL_ID_DFPR),
                                      childHwEngIdx);
        }
    }

    //
    // Extra Preconditions for DIFR Layer 3
    // Layer 3 depends on Layer 2 FSM. This is by design. We can
    // not have override for this even for debugging.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_MS_DIFR_CG) &&
        (ctrlId == RM_PMU_LPWR_CTRL_ID_MS_DIFR_CG))
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_MS_DIFR_SW_ASR) &&
            pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR))
        {
            // Configure the Parent Child dependencies
            lpwrFsmDependencyInit_HAL(&Lpwr, PG_GET_ENG_IDX(RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR),
                                      childHwEngIdx);
        }
    }
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
msDispForceFillInit_TU10X(void)
{
    LwU32       val      = 0;

    // If any MS Group feature support ISO Stutter program the Force Fill Mode
    if (lpwrGrpCtrlIsSfSupported(RM_PMU_LPWR_GRP_CTRL_ID_MS,
                                 RM_PMU_LPWR_CTRL_FEATURE_ID_MS_ISO_STUTTER))
    {
        val = REG_RD32(FECS, LW_PDISP_IHUB_COMMON_PMU_CTL);
        val = FLD_SET_DRF(_PDISP, _IHUB_COMMON_PMU_CTL,
                          _FORCE_FILL, _ENABLE, val);
        REG_WR32(FECS, LW_PDISP_IHUB_COMMON_PMU_CTL, val);
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
msDispForceFillEnable_TU10X(LwBool bEnable)
{
    LwU32 val = 0;

    val = REG_RD32(FECS, LW_PDISP_IHUB_COMMON_PMU_CTL);

    val = bEnable ? FLD_SET_DRF(_PDISP, _IHUB_COMMON_PMU_CTL,
                                _FORCE_FILL, _ENABLE, val) :
                    FLD_SET_DRF(_PDISP, _IHUB_COMMON_PMU_CTL,
                                _FORCE_FILL, _DISABLE, val);
    REG_WR32(FECS, LW_PDISP_IHUB_COMMON_PMU_CTL, val);
}

/*!
 * @brief Enable/Disable iso stutter override for MSCG
 *
 * Adds/Removes display to/from MSCG entry conditions.
 *
 * @param[in] bAddIsoStutter      LW_TRUE     Add display to MSCG
 *                                            entry conditions
 *                                LW_FALSE    Remove display from MSCG
 *                                            entry condition
 */
void
msIsoStutterOverride_TU10X
(
    LwU8   ctrlId,
    LwBool bAddIsoStutter
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);
    LwU32       msDispOverride;
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
        msDispOverride = REG_RD32(CSB, LW_CPWR_PMU_PG_ON_TRIGGER_MASK_2(PG_GET_ENG_IDX(ctrlId)));
        intr1          = REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN);

        if (bAddIsoStutter)
        {
            msDispOverride = FLD_SET_DRF(_CPWR_PMU, _PG_ON_TRIGGER_MASK_2,
                                         _MSPG_OK_AND, _ENABLE, msDispOverride);
            intr1          = FLD_SET_DRF(_CPWR_PMU, _GPIO_1_INTR_RISING_EN,
                                         _MSPG_WAKE, _ENABLED, intr1);
        }
        else
        {
            msDispOverride = FLD_SET_DRF(_CPWR_PMU, _PG_ON_TRIGGER_MASK_2,
                                         _MSPG_OK_AND, _DISABLE, msDispOverride);
            intr1          = FLD_SET_DRF(_CPWR_PMU, _GPIO_1_INTR_RISING_EN,
                                         _MSPG_WAKE, _DISABLED, intr1);
        }

        REG_WR32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN, intr1);
        REG_WR32(CSB, LW_CPWR_PMU_PG_ON_TRIGGER_MASK_2(PG_GET_ENG_IDX(ctrlId)),
                 msDispOverride);
    }
}

/*!
 * @brief Initialize the MSCG Stop Clock Profile
 *
 * @param[in] clkMask       Mask of clocks which are supported
 */
void
msStopClockInit_TU10X
(
    LwU32 clkMask
)
{
    LwU32 regVal = REG_RD32(FECS, LW_PTRIM_SYS_CG);

    if (IS_CLK_SUPPORTED(GPC, clkMask))
    {
        regVal = FLD_SET_DRF(_PTRIM_SYS, _CG,
                             _MSCG_STOPCLK_GPCCLK_EN, _YES, regVal);
    }

    if (IS_CLK_SUPPORTED(XBAR, clkMask))
    {
        regVal = FLD_SET_DRF(_PTRIM_SYS, _CG,
                             _MSCG_STOPCLK_XBARCLK_EN, _YES, regVal);
    }

    REG_WR32(FECS, LW_PTRIM_SYS_CG, regVal);
}

/*!
 * @brief Gate the clock using MSCG Stop clock profile
 */
void
msStopClockGate_TU10X(void)
{
    LwU32 regVal = REG_RD32(CSB, LW_CPWR_PMU_GPIO_OUTPUT_SET);

    regVal = FLD_SET_DRF(_CPWR, _PMU_GPIO_OUTPUT_SET,
                         _STOPCLOCKS1, _TRUE, regVal);
    REG_WR32_STALL(CSB, LW_CPWR_PMU_GPIO_OUTPUT_SET, regVal);
}

/*!
 * @brief Ungate the clock using MSCG Stop clock profile
 */
void
msStopClockUngate_TU10X(void)
{
    LwU32 regVal = REG_RD32(CSB, LW_CPWR_PMU_GPIO_OUTPUT_CLEAR);

    regVal = FLD_SET_DRF(_CPWR, _PMU_GPIO_OUTPUT_CLEAR,
                         _STOPCLOCKS1, _TRUE, regVal);
    REG_WR32_STALL(CSB, LW_CPWR_PMU_GPIO_OUTPUT_CLEAR, regVal);
}

/*!
 * @brief Resets IDLE_FLIPPED bit if FB traffic generated is only from PMU
 *
 * This function resets the IDLE_FLIPPED bit if the FB traffic is from PMU due
 * to loading tasks/overlays. This will avoid aborting the first MSCG cycle
 * and improve residency.
 */
FLCN_STATUS
msIdleFlippedReset_TU10X
(
    LwU8    ctrlId
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);
    LwU32       hwEngIdx = PG_GET_ENG_IDX(ctrlId);
    LwU32       regVal;
    FLCN_STATUS status   = FLCN_OK;

    if (!PMUCFG_FEATURE_ENABLED(PMU_PG_IDLE_FLIPPED_RESET) ||
        !LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, IDLE_FLIPPED_RESET))
    {
        goto msIdleFlippedReset_TU10X_exit;
    }

    //
    // Reset IDLE_FLIPPED bit if
    // - If IDLE_FLIPPED is set only due to PMU &
    // - Non LPWR tasks are idle
    //
    regVal = REG_RD32(CSB, LW_CPWR_PMU_PG_STAT(hwEngIdx));
    if (FLD_TEST_DRF(_CPWR_PMU, _PG_STAT, _IDLE_FLIPPED, _ASSERTED, regVal) &&
        FLD_TEST_DRF(_CPWR_PMU, _PG_STAT, _IDLE_FLIPPED_NON_PMU, _DEASSERTED, regVal))
    {
        if (!PMUCFG_FEATURE_ENABLED(PMU_PG_TASK_MGMT) ||
            lpwrIsNonLpwrTaskIdle())
        {
            // Set IDX to current engine for Idle flip
            regVal = REG_RD32(CSB, LW_CPWR_PMU_PG_MISC);
            regVal = FLD_SET_DRF_NUM(_CPWR, _PMU_PG_MISC, _IDLE_FLIP_STATUS_IDX,
                                     hwEngIdx, regVal);

            REG_WR32(CSB, LW_CPWR_PMU_PG_MISC, regVal);

            regVal = REG_RD32(CSB, LW_CPWR_PMU_PG_IDLE_FLIP_STATUS_1);

            //
            // Clear the FBHUB Idle signal
            // PMU is client to FBHUB only, so if PMU issues any FB traffic, that traffic
            // will be captured by FBHUB_ALL signal only.
            // Therefor, to flip idle reset caused due the PMU only FB access, we need
            // to clear the FBHUB_ALL signal.
            //
            regVal = FLD_SET_DRF(_CPWR, _PMU_PG_IDLE_FLIP_STATUS_1,
                                 _FBHUB_ALL, _IDLE, regVal);
            REG_WR32_STALL(CSB, LW_CPWR_PMU_PG_IDLE_FLIP_STATUS_1, regVal);

            // Check if all the units are idle or not
            if (!pgCtrlIsSubunitsIdle_HAL(&Pg, ctrlId))
            {
                status = FLCN_ERROR;
            }
        }
    }

msIdleFlippedReset_TU10X_exit:

    return status;
}

/*!
 * @brief Initialize Half FBPA Mask
 */
void msInitHalfFbpaMask_TU10X(void)
{
    OBJSWASR *pSwAsr  = MS_GET_SWASR();
    LwU32     regVal  = 0;
    LwU32     idx     = 0;

    // Init the mask with zero
    pSwAsr->halfFbpaMask = 0;

    regVal = REG_RD32(FECS, LW_FUSE_STATUS_OPT_HALF_FBPA);

    //
    // To determine the HALF FBPA Mask, we need to consider the
    // following bits in the fuse:
    // BIT 0  is set -> Partition A is in HALF FBPA Mode
    // BIT 2  is set -> Partition B is in HALF FBPA Mode
    // BIT 4  is set -> Partition C is in HALF FBPA Mode
    // BIT 6  is set -> Partition D is in HALF FBPA Mode
    // BIT 8  is set -> Partition E is in HALF FBPA Mode
    // BIT 10 is set -> Partition F is in HALF FBPA Mode
    // i.e for every active FBIO, we need to check the even
    // bit to determine if this FBIO is in HALF FBPA mode or
    // not
    //
    FOR_EACH_INDEX_IN_MASK(32, idx, pSwAsr->regs.mmActiveFBIOs)
    {
        if ((regVal & BIT(idx * 2)) != 0)
        {
            pSwAsr->halfFbpaMask |= BIT(idx);
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

/*!
 * @brief Function to engage/disengage MS SRAM Power Gating
 *
 * @param[in]  ctrlId   LPWR_CTRL Id requesting to execture the SRAM Sequencer
 *                      sequence with MS Group
 * @param[in]  bEngage  LW_TRUE - Engage the Sequencer
 *                      LW_FALSE - Disengage the Sequencer
 *
 */
void
msSramPowerGatingEngage_TU10X
(
    LwU8   ctrlId,
    LwBool bEngage
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);

    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_SEQ)                      &&
        LPWR_SEQ_IS_SRAM_CTRL_SUPPORTED(LPWR_SEQ_SRAM_CTRL_ID_MS) &&
        (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, RPPG)))
    {
        if (bEngage)
        {
            lpwrSeqSramStateSet(LPWR_SEQ_SRAM_CTRL_ID_MS,
                                LPWR_SEQ_STATE_SRAM_RPPG, LW_FALSE);
        }
        else
        {
            lpwrSeqSramStateSet(LPWR_SEQ_SRAM_CTRL_ID_MS,
                                LPWR_SEQ_STATE_PWR_ON, LW_TRUE);
        }
    }
}

/*!
 * @brief Execute the priv blocker Entry sequence
 *
 * Steps:
 * 1. Engage the XVE/SEC2/GSP Priv blocker in BLOCK_ALL Mode.
 * 2. Issue the flush for XVE/SEC2/GSP Priv Path.
 * 3. Idle and P2P Idle check.
 * 4. Move the XVE/SEC2/GSP Priv blocker to BLOCK_EQ mode.
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
msPrivBlockerEntry_TU10X
(
    LwU8            ctrlId,
    FLCN_TIMESTAMP *pBlockStartTimeNs,
    LwS32          *pTimeoutLeftNs,
    LwU16          *pAbortReason
)
{
     OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);

     // Engage the blocker into Block ALL mode
     if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_UNIFIED_PRIV_BLOCKER_CTRL))
     {
         if (msCheckTimeout(Ms.abortTimeoutNs, pBlockStartTimeNs, pTimeoutLeftNs) ||
             (FLCN_OK != lpwrCtrlPrivBlockerModeSet(ctrlId,
                                                    LPWR_PRIV_BLOCKER_MODE_BLOCK_ALL,
                                                    LPWR_PRIV_BLOCKER_PROFILE_MS,
                                                    *pTimeoutLeftNs)))
         {
             *pAbortReason = MS_ABORT_REASON_BLK_EVERYTHING_TIMEOUT;
             return LW_FALSE;
         }

         // Issue the flush for the PRIV Path
         if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_UNIFIED_PRIV_FLUSH_SEQ))
         {
             if (msCheckTimeout(Ms.abortTimeoutNs, pBlockStartTimeNs, pTimeoutLeftNs) ||
                 !lpwrPrivPathFlush_HAL(&Lpwr, LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, XVE),
                                        LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, SEC2),
                                        LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, GSP),
                                        (LwU32)*pTimeoutLeftNs))
             {
                 *pAbortReason = MS_ABORT_REASON_PRIV_FLUSH_TIMEOUT;
                 return LW_FALSE;
             }
         }
         else if (!msPrivPathFlush_HAL(&Ms, pAbortReason, pBlockStartTimeNs,
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
#endif // (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))

         // Engage the blocker into Block EQ mode
         if (msCheckTimeout(Ms.abortTimeoutNs, pBlockStartTimeNs, pTimeoutLeftNs) ||
             (FLCN_OK != lpwrCtrlPrivBlockerModeSet(ctrlId,
                                                    LPWR_PRIV_BLOCKER_MODE_BLOCK_EQUATION,
                                                    LPWR_PRIV_BLOCKER_PROFILE_MS,
                                                    *pTimeoutLeftNs)))
         {
             *pAbortReason = MS_ABORT_REASON_BLK_EQUATION_TIMEOUT;
             return LW_FALSE;
         }
     }
     else
     {
         // We should never reach here. If we reach, then its a functional BUG.
         PMU_BREAKPOINT();
     }

     return LW_TRUE;
}

/*!
 * @brief Execute the Priv blocker Exit sequence
 */
void
msPrivBlockerExit_TU10X
(
    LwU8 ctrlId
)
{
    // Disengage the Priv Blockers
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_UNIFIED_PRIV_BLOCKER_CTRL))
    {
        lpwrCtrlPrivBlockerModeSet(ctrlId,
                                   LPWR_PRIV_BLOCKER_MODE_BLOCK_NONE,
                                   LPWR_PRIV_BLOCKER_PROFILE_MS, 0);
    }
}
