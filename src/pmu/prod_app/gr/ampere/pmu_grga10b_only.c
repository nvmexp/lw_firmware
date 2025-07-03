/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_grga10b_only.c
 * @brief  HAL-interface for the GA10B Graphics Engine.
 */
/* ------------------------- System Includes -------------------------------- */

#include "pmusw.h"
#include "dev_graphics_nobundle.h"
#include "dev_graphics_nobundle_addendum.h"
#if (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
#include "ram_profile_init_seq.h"
#include "logic_profile_init_seq.h"
#endif // (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
#include "dev_pwr_csb.h"
#include "dev_trim.h"
#include "dev_runlist.h"
#include "dev_smcarb.h"
#include "dev_runlist.h"
#include "hwproject.h"
/* ------------------------- Application Includes --------------------------- */

#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objgr.h"
#include "pmu_objlpwr.h"
#include "pmu_objfifo.h"
#include "pmu_objpmu.h"
#include "pmu_objhal.h"
#include "pmu_bar0.h"

#include "config/g_gr_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * As per RTL test results keeping the max Poll time 10msec
 */
#define GR_UNIFIED_SEQ_POLL_TIME_US_GA10B   10000

/* ------------------------- Private Prototypes ----------------------------- */
static void        s_grPgAbort_GA10B(LwU32 abortReason, LwU32 abortStage);
static void        s_grPgEngageSram_GA10B(LwBool bBlocking);
static void        s_grPgDisengageSram_GA10B(void);
static void        s_grPgEngageLogic_GA10B(LwBool bBlocking);
static void        s_grPgDisengageLogic_GA10B(void);
static void        s_grPgCpmEntry_GA10B(void);
static void        s_grPgCpmExit_GA10B(void);
static FLCN_STATUS s_grPgFifoPreemptSequence_GA10B(void);
static LwU32       s_grPgSmcFifoEngIdGet_GA10B(LwU32 sysPipeIndex);
static LwU32       s_grMaxSysPipesGet_GA10BF(void);
static LwU32       s_grNumGpcGet_GA10BF(void);

/* ------------------------ Public Functions ------------------------------- */
/*!
 * @brief Initializes the idle mask for GR.
 */
void
grInitSetIdleMasks_GA10B(void)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);
    LwU32       im0      = 0;
    LwU32       im1      = 0;
    LwU32       im2      = 0;

    im0 = DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR,     _ENABLED) |
          DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR_SYS, _ENABLED) |
          DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR_GPC, _ENABLED);

    if (FIFO_IS_ENGINE_PRESENT(CE0) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE0))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_0, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE1) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE1))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_1, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE2) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE2))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_2, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE3) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE3))
    {
        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_3, _ENABLED, im1);
    }

    if (LPWR_CTRL_IS_SF_SUPPORTED(pPgState, GR, GSP))
    {
        im2 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _GSP_PRIV_BLOCKER_GR,
                          _ENABLED, im2);
    }

    pPgState->idleMask[0] = im0;
    pPgState->idleMask[1] = im1;
    pPgState->idleMask[2] = im2;
}

/*!
 * @brief Initializes the idle mask for GR-RG.
 */
void
grRgInitSetIdleMasks_GA10B(void)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_RG);
    LwU32       im0      = 0;
    LwU32       im1      = 0;
    LwU32       im2      = 0;

    im0 = DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR,     _ENABLED) |
          DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR_SYS, _ENABLED) |
          DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR_GPC, _ENABLED);

    if (FIFO_IS_ENGINE_PRESENT(CE0) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE0))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_0, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE1) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE1))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_1, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE2) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE2))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_2, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE3) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE3))
    {
        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_3, _ENABLED, im1);
    }

    if (LPWR_CTRL_IS_SF_SUPPORTED(pPgState, GR, GSP))
    {
        im2 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _GSP_PRIV_BLOCKER_GR,
                          _ENABLED, im2);
    }

    pPgState->idleMask[0] = im0;
    pPgState->idleMask[1] = im1;
    pPgState->idleMask[2] = im2;
}

/*
 * Save GR's state
 *
 * @returns FLCN_OK when saved successfully.
 * @returns FLCN_ERROR otherwise
 */
FLCN_STATUS
grPgSave_GA10B(void)
{
    OBJPGSTATE *pGrState        = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);
    LwU32       abortReason     = 0;
    LwU32       bGspEnabled;

    // Check if in SMC mode
    grPgSmcConfigRead_HAL();

    // Checkpoint1: Check GR Idleness
    if (!grIsIdle_HAL(&Gr, &abortReason, RM_PMU_LPWR_CTRL_ID_GR_PG))
    {
        s_grPgAbort_GA10B(abortReason,
                          GR_ABORT_CHKPT_AFTER_PG_ON);

        return FLCN_ERROR;
    }

    // Engage Priv Blockers
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_UNIFIED_PRIV_BLOCKER_CTRL))
    {
        if (FLCN_OK != lpwrCtrlPrivBlockerModeSet(RM_PMU_LPWR_CTRL_ID_GR_PG,
                                        LPWR_PRIV_BLOCKER_MODE_BLOCK_ALL,
                                        LPWR_PRIV_BLOCKER_PROFILE_GR,
                                        (GR_PRIV_BLOCKER_ENGAGE_TIMEOUT_US * 1000)))
        {
            abortReason = GR_ABORT_REASON_PRIV_BLKR_ALL_TIMEOUT;
            s_grPgAbort_GA10B(abortReason,
                              GR_ABORT_CHKPT_AFTER_PRIV_BLOCKER);
        }
    }

    // Issue the PRIV path Flush
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_UNIFIED_PRIV_FLUSH_SEQ))
    {
        // Check for GSP state with GR features
        bGspEnabled  = LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, GSP);

        if (!lpwrPrivPathFlush_HAL(&Lpwr, LW_FALSE, LW_FALSE, bGspEnabled,
                                   LPWR_PRIV_FLUSH_DEFAULT_TIMEOUT_NS))
        {
            abortReason = GR_ABORT_REASON_PRIV_PATH_FLUSH_TIMEOUT;
            s_grPgAbort_GA10B(abortReason,
                              GR_ABORT_CHKPT_AFTER_PRIV_BLOCKER);
        }
    }

    // Checkpoint 2: Check GR Idleness
    if (!grIsIdle_HAL(&Gr, &abortReason, RM_PMU_LPWR_CTRL_ID_GR_PG))
    {
        s_grPgAbort_GA10B(abortReason,
                          GR_ABORT_CHKPT_AFTER_PRIV_BLOCKER);

        return FLCN_ERROR;
    }

    // Engage the Blocker in the Block Equation Mode
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_UNIFIED_PRIV_BLOCKER_CTRL))
    {
        if (FLCN_OK != lpwrCtrlPrivBlockerModeSet(RM_PMU_LPWR_CTRL_ID_GR_PG,
                                        LPWR_PRIV_BLOCKER_MODE_BLOCK_EQUATION,
                                        LPWR_PRIV_BLOCKER_PROFILE_GR,
                                        (GR_PRIV_BLOCKER_ENGAGE_TIMEOUT_US * 1000)))
        {
            abortReason = GR_ABORT_REASON_PRIV_BLKR_EQU_TIMEOUT;
            s_grPgAbort_GA10B(abortReason,
                              GR_ABORT_CHKPT_AFTER_PRIV_BLOCKER);
        }
    }

    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libFifo));

    //
    // Execute preemption sequence. As part of preemption sequence host saves
    // channel and engine context.
    //
    if (s_grPgFifoPreemptSequence_GA10B() != FLCN_OK)
    {
        s_grPgAbort_GA10B(GR_ABORT_REASON_FIFO_PREEMPT_ERROR,
                          GR_ABORT_CHKPT_AFTER_PREEMPT_SEQUENCE);
        return FLCN_ERROR;
    }
    else
    {
        // Clear idle flip corresponding to GR_FE and GR_PRIV
        grClearIdleFlip_HAL(&Gr, RM_PMU_LPWR_CTRL_ID_GR_PG);
    }

    // Enable engine holdoffs
    if (FLCN_OK != lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_GR_PG,
                                          pGrState->holdoffMask))
    {
        s_grPgAbort_GA10B(GR_ABORT_REASON_HOLDOFF_ENABLE_TIMEOUT,
                          GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_0);

        return FLCN_ERROR;
    }

    // Check if the context is still INVALID
    if (!grIsCtxIlwalid_HAL(&Gr))
    {
        s_grPgAbort_GA10B(GR_ABORT_REASON_FIFO_PREEMPT_ERROR,
                          GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_0);

        return FLCN_ERROR;
    }

    // Checkpoint 3: Check GR Idleness
    if (!grIsIdle_HAL(&Gr, &abortReason, RM_PMU_LPWR_CTRL_ID_GR_PG))
    {
        s_grPgAbort_GA10B(abortReason,
                          GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_0);

        return FLCN_ERROR;
    }

    // Save global state via FECS
    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, SAVE_GLOBAL_STATE))
    {
        if (FLCN_OK != grGlobalStateSave_HAL(&Gr, RM_PMU_LPWR_CTRL_ID_GR_PG))
        {
            s_grPgAbort_GA10B(GR_ABORT_REASON_REGLIST_SAVE_TIMEOUT,
                             GR_ABORT_CHKPT_AFTER_SAVE_REGLIST_REQUEST);
        }
    }

    // Enable the PRI erroring mechanism
    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, PRIV_RING))
    {
        grPgPrivErrHandlingActivate_HAL(&Gr, LW_TRUE);
    }

    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libFifo));

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, HW_SEQUENCE))
    {
        // Program the CPM macros
        s_grPgCpmEntry_GA10B();

        if (PMUCFG_FEATURE_ENABLED(PMU_PG_GR_UNIFIED_POWER_SEQ))
        {
            // Trigger LPWR Sequencer to engage ELPG
            grHwPgEngage_HAL(&Gr);
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
grPgRestore_GA10B(void)
{
    OBJPGSTATE *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);

    // Assert Engine/Context reset
    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, RESET))
    {
        if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, PRIV_RING))
        {
            grPgPrivErrHandlingActivate_HAL(&Gr, LW_FALSE);
        }

        grAssertFECSReset_HAL(&Gr, LW_TRUE);
        grAssertGPCReset_HAL(&Gr, LW_TRUE);

        if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, PRIV_RING))
        {
            grPgPrivErrHandlingActivate_HAL(&Gr, LW_TRUE);
        }
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PG_GR_UNIFIED_POWER_SEQ) &&
        LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, HW_SEQUENCE))
    {
        // Trigger LPWR sequencer to bring GR out of ELPG
        grHwPgDisengage_HAL(&Gr);
    }

    // Disable the PRI erroring mechanism
    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, PRIV_RING))
    {
        grPgPrivErrHandlingActivate_HAL(&Gr, LW_FALSE);
    }

    // De-assert engine and context reset
    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, RESET))
    {
        grAssertFECSReset_HAL(&Gr, LW_FALSE);
        grAssertGPCReset_HAL(&Gr, LW_FALSE);
    }

    if  (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, HW_SEQUENCE))
    {
        // Program the CPM macros
        s_grPgCpmExit_GA10B();
    }

    // Global restore
    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, SAVE_GLOBAL_STATE))
    {
        grGlobalStateRestore_HAL(&Gr, RM_PMU_LPWR_CTRL_ID_GR_PG);
    }

    // Disengage Priv blockers
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_UNIFIED_PRIV_BLOCKER_CTRL))
    {
        lpwrCtrlPrivBlockerModeSet(RM_PMU_LPWR_CTRL_ID_GR_PG,
                                      LPWR_PRIV_BLOCKER_MODE_BLOCK_NONE,
                                      LPWR_PRIV_BLOCKER_PROFILE_GR, 0);
    }

    // Disable holdoff
    lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_GR_PG,
                           LPWR_HOLDOFF_MASK_NONE);

    return FLCN_OK;
}

/*!
 * @brief Power-gate GR engine.
 */
void
grHwPgEngage_GA10B(void)
{
    OBJPGSTATE *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);

    // PowerGate RAM followed by Logic for GR
    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, ELPG_SRAM))
    {
        s_grPgEngageSram_GA10B(LW_TRUE);
    }

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, ELPG_LOGIC))
    {
        s_grPgEngageLogic_GA10B(LW_TRUE);
    }

}

/*!
 * @brief Power-ungate GR engine.
 */
void
grHwPgDisengage_GA10B(void)
{
    OBJPGSTATE *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);

    // Power-ungate Logic followed by RAM for GR
    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, ELPG_LOGIC))
    {
        s_grPgDisengageLogic_GA10B();
    }

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, ELPG_SRAM))
    {
        s_grPgDisengageSram_GA10B();
    }
}

/*!
 * Asserts FECS reset
 *
 * @param[in] bAssert   True to assert, False otherwise
 */
void
grAssertFECSReset_GA10B(LwBool bAssert)
{
    LwU32 fecsReset;
    LwU32 smcPgraphBase;
    LwU32 addrWithUpdatedBase;
    LwU32 sysPipeIdx;
    LwU32 maxSysPipe;

    // 
    // Assert/de-assert reset for the available 
    // number of FECS using SMC Pri address
    // Bug 200667461
    //
    maxSysPipe = s_grMaxSysPipesGet_GA10BF();

    for (sysPipeIdx = 0; sysPipeIdx < maxSysPipe; sysPipeIdx++)
    {
        //
        // We have to reset the available number of FECS
        // The write to the LW_PGRAPH_PRI_FECS_* register will
        // need to be written for available FECS using the SMC pri addresses.
        //

        smcPgraphBase = LW_SMC_PRIV_BASE + (sysPipeIdx * LW_SMC_PRIV_STRIDE);
        addrWithUpdatedBase = (LW_PGRAPH_PRI_FECS_CTXSW_RESET_CTL - DEVICE_BASE(LW_PGRAPH)) + smcPgraphBase;
        fecsReset = REG_RD32(FECS, addrWithUpdatedBase);

        if (bAssert)
        {
            // Assert FECS Engine and Context Reset
            fecsReset = FLD_SET_DRF(_PGRAPH, _PRI_FECS_CTXSW_RESET_CTL,
                                    _SYS_ENGINE_RESET, _ENABLED, fecsReset);
            fecsReset = FLD_SET_DRF(_PGRAPH, _PRI_FECS_CTXSW_RESET_CTL,
                                    _SYS_CONTEXT_RESET, _ENABLED, fecsReset);
        }
        else
        {
            // De-assert FECS Engine and Context Reset
            fecsReset = FLD_SET_DRF(_PGRAPH, _PRI_FECS_CTXSW_RESET_CTL,
                                    _SYS_CONTEXT_RESET, _DISABLED, fecsReset);
            fecsReset = FLD_SET_DRF(_PGRAPH, _PRI_FECS_CTXSW_RESET_CTL,
                                    _SYS_ENGINE_RESET, _DISABLED, fecsReset);
         }

         REG_WR32(FECS, addrWithUpdatedBase, fecsReset);

         // Force read for flush
         REG_RD32(FECS, addrWithUpdatedBase);

    }
}

/*!
 * Asserts GPC reset
 *
 * @param[in] bAssert   True to assert, False otherwise
 */
void
grAssertGPCReset_GA10B(LwBool bAssert)
{
    LwU32 gpcReset;

    gpcReset = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_GPCCS_CTXSW_RESET_CTL);

    // 
    // Assert and De-assert GPCS reset register using
    // LW_PGRAPH_* broadcast register
    //
    if (bAssert)
    {
        // Assert GPC Engine and Context Reset
        gpcReset = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_GPCCS_CTXSW_RESET_CTL,
                               _GPC_ENGINE_RESET, _ENABLED, gpcReset);
        gpcReset = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_GPCCS_CTXSW_RESET_CTL,
                               _GPC_CONTEXT_RESET, _ENABLED, gpcReset);
    }
    else
    {
        // De-assert GPC Engine and Context Reset
        gpcReset = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_GPCCS_CTXSW_RESET_CTL,
                               _GPC_ENGINE_RESET, _DISABLED, gpcReset);
        gpcReset = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_GPCCS_CTXSW_RESET_CTL,
                               _GPC_CONTEXT_RESET, _DISABLED, gpcReset);
    }

    REG_WR32(FECS, LW_PGRAPH_PRI_GPCS_GPCCS_CTXSW_RESET_CTL, gpcReset);

    // Force read for flush
    REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_GPCCS_CTXSW_RESET_CTL);
}

/*!
 * Check if GPU in SMC mode
 * If in SMC mode, fill the array to store the
 * sysPipeIDs corresponding to each GPC
 *
 */
FLCN_STATUS
grPgSmcConfigRead_GA10B(void)
{
    LwU32                gpcIdx;
    LwU32                sysPipeId;
    LwU32                regVal;
    LwU32                numGpc;

    //
    // we need to read the SYS_PIPE_ID for each available GPC to
    // determine the assigned SYS_PIPES and hence the number of active SMCs
    //

    // Keeping the initial value of Gr.smcSysPipeMask = 0x0
    Gr.smcSysPipeMask = 0x0;

    //
    // Read the sysPipeId for each available GPC and set the
    // corresponding bit in the Gr.smcSysPipeMask
    // For example: if sysPipeId = 0 then set BIT(0)
    //
    numGpc = s_grNumGpcGet_GA10BF();

    for (gpcIdx = 0; gpcIdx < numGpc; gpcIdx++)
    {
        regVal = REG_RD32(FECS, LW_PSMCARB_SMC_PARTITION_GPC_MAP(gpcIdx));

        // Fill in the sysPipeID in the array
        sysPipeId = DRF_VAL(_PSMCARB, _SMC_PARTITION_GPC_MAP, _SYS_PIPE_ID, regVal);

        //
        // set correct bit in Gr.smcSysPipeMask.
        // Bit 0 represent sys-pipe 0,
        // Bit 1 represents sys pipe 1 etc
        //
        Gr.smcSysPipeMask |= BIT(sysPipeId);
    }

    return FLCN_OK;
}

/*!
 * Submits a FECS method
 *
 * @param[in] method       A method to be sent to FECS.
 * @param[in] methodData   Data to be sent along with the method
 * @param[in] pollValue    value to poll on MALBOX(0) for method completion
 * 
 * @return FLCN_STATUS  FLCN_OK    If save/Restore reglist is sucessful in 4ms/10ms.
 *                      FLCN_ERROR If save/Restore reglist is unsucessful in 4ms/10ms.
 */
FLCN_STATUS
grSubmitMethod_GA10B
(
    LwU32 method,
    LwU32 methodData,
    LwU32 pollValue
)
{
    LwU32       smcPgraphBase         = 0;
    LwU32       addrWithUpdatedBase   = 0;
    LwU32       sysPipeIdx            = 0;
    LwU32       timeoutUs             = GR_FECS_SUBMIT_METHOD_TIMEOUT_US;
    FLCN_STATUS status                = FLCN_OK;

    //
    // Do a save/restore for each FECS based on SysPipeMask
    // Use the SMC pri addresses to initiate Global Save on each FECS
    //
    FOR_EACH_INDEX_IN_MASK(32, sysPipeIdx, Gr.smcSysPipeMask)
    {
        // Callwlating SMC addresses
        smcPgraphBase = LW_SMC_PRIV_BASE + (sysPipeIdx * LW_SMC_PRIV_STRIDE);

        // Clear MAILBOX(0) which is used to poll for completion.
        addrWithUpdatedBase = (LW_PGRAPH_PRI_FECS_CTXSW_MAILBOX(0) - DEVICE_BASE(LW_PGRAPH)) + smcPgraphBase;
        REG_WR32(FECS, addrWithUpdatedBase, 0);

        // Set the method data
        addrWithUpdatedBase = (LW_PGRAPH_PRI_FECS_METHOD_DATA - DEVICE_BASE(LW_PGRAPH)) + smcPgraphBase;
        REG_WR32(FECS, addrWithUpdatedBase, methodData);

        // Submit Method
        addrWithUpdatedBase = (LW_PGRAPH_PRI_FECS_METHOD_PUSH - DEVICE_BASE(LW_PGRAPH)) + smcPgraphBase;
        REG_WR32(FECS, addrWithUpdatedBase,
            DRF_NUM(_PGRAPH, _PRI_FECS_METHOD_PUSH, _ADR, method));
    }
    FOR_EACH_INDEX_IN_MASK_END;

    //
    // Bug : 3162570
    // According to the type of the method timeoutUS value is set 
    // For save the value is 4ms and for restore the value is 10ms.(Bug 3162570)
    //
    if (method == LW_PGRAPH_PRI_FECS_METHOD_PUSH_ADR_SAVE_REGLIST) 
    {
        timeoutUs = GR_FECS_SUBMIT_METHOD_SAVE_REGLIST_TIMEOUT_US;
    }
    else if (method == LW_PGRAPH_PRI_FECS_METHOD_PUSH_ADR_RESTORE_REGLIST) 
    {   
        timeoutUs = GR_FECS_SUBMIT_METHOD_RESTORE_REGLIST_TIMEOUT_US;
    }

    // poll all active FECS for completion
    FOR_EACH_INDEX_IN_MASK(32, sysPipeIdx, Gr.smcSysPipeMask)
    {
        // Callwlating SMC addresses
        smcPgraphBase = LW_SMC_PRIV_BASE + (sysPipeIdx * LW_SMC_PRIV_STRIDE);

        // wait until we get a result (timeout 4 ms)
        addrWithUpdatedBase = (LW_PGRAPH_PRI_FECS_CTXSW_MAILBOX(0) - DEVICE_BASE(LW_PGRAPH)) + smcPgraphBase;
        if (!PMU_REG32_POLL_US(
                USE_FECS(addrWithUpdatedBase),
                LW_U32_MAX, pollValue, timeoutUs,
                PMU_REG_POLL_MODE_BAR0_EQ))
        {
            status = FLCN_ERROR;
            break;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    return status;
}

/*
 * @brief Returns if the current context is invalid
 *
 * @return LW_TRUE  If current context is invalid
 *         LW_FALSE otherwise
 */
LwBool
grIsCtxIlwalid_GA10B(void)
{
    LwU32   runlistPriBase;
    LwU16   runlistEngId;
    LwU16   fifoEngId;
    LwU32   sysPipeIdx;
    LwBool  bIsCtxIlwalid;
    LwU32   regVal          = 0;
    LwBool  bIsCtxIlwalidGr   = LW_TRUE;
    LwBool  bIsCtxIlwalidGr1  = LW_TRUE;

    FOR_EACH_INDEX_IN_MASK(32, sysPipeIdx, Gr.smcSysPipeMask)
    {
        // get the fifoEngId corresponding to SysPipe mapping
        fifoEngId = s_grPgSmcFifoEngIdGet_GA10B(sysPipeIdx);

        // if fifoEngId is INVALID skip the loop and continue to the next iteration
        if (fifoEngId == PMU_ENGINE_ILWALID)
        {
            continue;
        }

        //
        // If sysPipeId = 0 then store the
        // status of context invalid for runlist of GR
        //
        runlistPriBase = GET_ENG_RUNLIST_PRI_BASE(fifoEngId);
        runlistEngId   = GET_ENG_RUNLIST_ENG_ID(fifoEngId);
        regVal = REG_RD32(FECS, runlistPriBase + LW_RUNLIST_ENGINE_STATUS0(runlistEngId));

        if (fifoEngId == PMU_ENGINE_GR)
        {
            bIsCtxIlwalidGr = FLD_TEST_DRF(_RUNLIST, _ENGINE_STATUS0, _CTX_STATUS, _ILWALID, regVal);
        }
        if (fifoEngId == PMU_ENGINE_GR1)
        {
            bIsCtxIlwalidGr1 = FLD_TEST_DRF(_RUNLIST, _ENGINE_STATUS0, _CTX_STATUS, _ILWALID, regVal);
        }

    } FOR_EACH_INDEX_IN_MASK_END;

    // Context invalid state of both the runlists
    bIsCtxIlwalid = bIsCtxIlwalidGr & bIsCtxIlwalidGr1;

    return bIsCtxIlwalid;
}

/* ------------------------- Private Functions ------------------------------- */
/*!
 * @brief Abort GR sequence, perform backout sequence as needed
 *
 * @param[in]  abortReason     Reason for aborting gr during entry sequence
 * @param[in]  abortStage      Stage of entry sequence where gr is aborted,
 *                             this determines the steps to be performed
 *                             in abort sequence
 */
static void
s_grPgAbort_GA10B
(
   LwU32  abortReason,
   LwU32  abortStage
)
{
    OBJPGSTATE   *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);
    pGrState->stats.abortReason = (abortStage | abortReason);

    switch (abortStage)
    {
        //
        // Do not change the order of case sections.
        // No break in each case except the last one.
        // Do not compile out "case" using #if(PMUCFG_FEATURE_ENABLED()).
        //
        case GR_ABORT_REASON_REGLIST_SAVE_TIMEOUT:
        case GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_0:
        {
            // Disable holdoffs
            lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_GR_PG,
                                   LPWR_HOLDOFF_MASK_NONE);

            // coverity[fallthrough]
        }
        case GR_ABORT_CHKPT_AFTER_PREEMPT_SEQUENCE:
        {
            OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libFifo));

            // coverity[fallthrough]
        }
        case GR_ABORT_CHKPT_AFTER_PRIV_BLOCKER:
        {
            // Disengage the Priv Blockers
            if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_UNIFIED_PRIV_BLOCKER_CTRL))
            {
                lpwrCtrlPrivBlockerModeSet(RM_PMU_LPWR_CTRL_ID_GR_PG,
                                           LPWR_PRIV_BLOCKER_MODE_BLOCK_NONE,
                                           LPWR_PRIV_BLOCKER_PROFILE_GR, 0);
            }
            // coverity[fallthrough]
        }
        case GR_ABORT_CHKPT_AFTER_PG_ON:
        {
            // No-op
            break;
        }
    }
}

/*!
 * @brief Power-ungate GR engine's SRAM.
 */
void
s_grPgEngageSram_GA10B
(
    LwBool bBlocking
)
{
    LwU32 val;
    LwU8  profile;

    // Get the RPG Profile
    profile = LPWR_SRAM_PG_PROFILE_GET(RPG);

    val = REG_RD32(CSB, LW_CPWR_PMU_RAM_TARGET);
    val = FLD_SET_DRF_NUM(_CPWR_PMU, _RAM_TARGET, _GR, profile, val);
    REG_WR32(CSB, LW_CPWR_PMU_RAM_TARGET, val);

    if (bBlocking)
    {
        if (!PMU_REG32_POLL_US(LW_CPWR_PMU_RAM_STATUS,
                            DRF_SHIFTMASK(LW_CPWR_PMU_RAM_STATUS_GR_FSM),
                            DRF_DEF(_CPWR, _PMU_RAM_STATUS, _GR_FSM, _LOW_POWER),
                            GR_UNIFIED_SEQ_POLL_TIME_US_GA10B,
                            PMU_REG_POLL_MODE_CSB_EQ))
        {
            PMU_HALT();
        }
    }
}

/*!
 * @brief Power-ungate GR engine's SRAM.
 */
void
s_grPgDisengageSram_GA10B(void)
{
    LwU32 val;

    val = REG_RD32(CSB, LW_CPWR_PMU_RAM_TARGET);
    val = FLD_SET_DRF(_CPWR_PMU, _RAM_TARGET, _GR, _POWER_ON, val);
    REG_WR32(CSB, LW_CPWR_PMU_RAM_TARGET, val);

    if (!PMU_REG32_POLL_US(LW_CPWR_PMU_RAM_STATUS,
                        DRF_SHIFTMASK(LW_CPWR_PMU_RAM_STATUS_GR_FSM),
                        DRF_DEF(_CPWR, _PMU_RAM_STATUS, _GR_FSM, _POWER_ON),
                        GR_UNIFIED_SEQ_POLL_TIME_US_GA10B,
                        PMU_REG_POLL_MODE_CSB_EQ))
    {
        PMU_HALT();
    }
}

/*!
 * @brief Power-gate GR engine's LOGIC.
 *
 * @param[in] bBlocking  If its a blocking call
 */
void
s_grPgEngageLogic_GA10B
(
    LwBool bBlocking
)
{
    LwU32 val;
    LwU8  profile;

    // Get the ELPG Profile
    profile = LPWR_LOGIC_PG_PROFILE_GET(ELPG);

    val = REG_RD32(CSB, LW_CPWR_PMU_LOGIC_TARGET);
    val = FLD_SET_DRF_NUM(_CPWR_PMU, _LOGIC_TARGET, _GR, profile, val);
    REG_WR32(CSB, LW_CPWR_PMU_LOGIC_TARGET, val);

    if (bBlocking)
    {
        if (!PMU_REG32_POLL_US(LW_CPWR_PMU_LOGIC_STATUS,
                           DRF_SHIFTMASK(LW_CPWR_PMU_LOGIC_STATUS_GR_FSM),
                           DRF_DEF(_CPWR, _PMU_LOGIC_STATUS, _GR_FSM, _LOW_POWER),
                           GR_UNIFIED_SEQ_POLL_TIME_US_GA10B,
                           PMU_REG_POLL_MODE_CSB_EQ))
        {
            PMU_HALT();
        }
    }
}

/*!
 * @brief Power-ungate GR engine's LOGIC.
 */
void
s_grPgDisengageLogic_GA10B(void)
{
    LwU32 val;

    val = REG_RD32(CSB, LW_CPWR_PMU_LOGIC_TARGET);
    val = FLD_SET_DRF(_CPWR_PMU, _LOGIC_TARGET, _GR, _POWER_ON, val);
    REG_WR32(CSB, LW_CPWR_PMU_LOGIC_TARGET, val);

    if (!PMU_REG32_POLL_US(LW_CPWR_PMU_LOGIC_STATUS,
                        DRF_SHIFTMASK(LW_CPWR_PMU_LOGIC_STATUS_GR_FSM),
                        DRF_DEF(_CPWR, _PMU_LOGIC_STATUS, _GR_FSM, _POWER_ON),
                        GR_UNIFIED_SEQ_POLL_TIME_US_GA10B,
                        PMU_REG_POLL_MODE_CSB_EQ))
    {
        PMU_HALT();
    }
}

/*!
 * @brief Program the CPM macros before ELPG entry
 */
void
s_grPgCpmEntry_GA10B(void)
{
    LwU32 val;

    val = REG_RD32(FECS, LW_PTRIM_GPC_BCAST_AVFS_CPM_CFG);
    val = FLD_SET_DRF(_PTRIM, _GPC_BCAST_AVFS_CPM_CFG, _CPM_HARD_MACRO_UPDATE, _INIT, val );
    val = FLD_SET_DRF_NUM(_PTRIM, _GPC_BCAST_AVFS_CPM_CFG, _INIT_CPM_ELPG, 0x1, val );
    REG_WR32(FECS, LW_PTRIM_GPC_BCAST_AVFS_CPM_CFG, val);
}

/*!
 * @brief Program the CPM macros on ELPG exit
 */
void
s_grPgCpmExit_GA10B(void)
{
    LwU32 val;

    val = REG_RD32(FECS, LW_PTRIM_GPC_BCAST_AVFS_CPM_CFG);
    val = FLD_SET_DRF_NUM(_PTRIM, _GPC_BCAST_AVFS_CPM_CFG, _CPM_HARD_MACRO_UPDATE, 0x1, val );
    val = FLD_SET_DRF(_PTRIM, _GPC_BCAST_AVFS_CPM_CFG, _INIT_CPM_ELPG, _INIT, val );
    REG_WR32(FECS, LW_PTRIM_GPC_BCAST_AVFS_CPM_CFG, val);
}

/*!
 * @brief Fifo preemption sequence
 * Issue the Preemption to Each of the SMC Sys Pipes.
 */
FLCN_STATUS
s_grPgFifoPreemptSequence_GA10B(void)
{
    FLCN_TIMESTAMP preemptStartTimeNs;
    LwU32          sysPipeIdx;
    LwU32          fifoEngId;
    LwU32          runlistPriBase;
    LwU32          regVal;
    LwU32          status = FLCN_OK;

    FOR_EACH_INDEX_IN_MASK(32, sysPipeIdx, Gr.smcSysPipeMask)
    {
        // get the fifEngId corresponding to SysPipe mapping
        fifoEngId = s_grPgSmcFifoEngIdGet_GA10B(sysPipeIdx);
        //
        // if fifoEngId is INVALID, skip the preemption
        // and continue with the next iteration
        //
        if (fifoEngId == PMU_ENGINE_ILWALID)
        {
            continue;
        }
        if (fifoPreemptSequence_HAL(&Fifo, fifoEngId, RM_PMU_LPWR_CTRL_ID_GR_PG) != FLCN_OK)
        {
            status = FLCN_ERROR;
            goto s_grPgFifoPreemptSequence_GA10B_exit;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    //
    // Poll for completion: in SMC mode, poll for all runlists
    // belonging to active sys-pipes
    //

    // Start the preemption time
    osPTimerTimeNsLwrrentGet(&preemptStartTimeNs);

    FOR_EACH_INDEX_IN_MASK(32, sysPipeIdx, Gr.smcSysPipeMask)
    {
        // get the fifoEngId corresponding to SysPipe mapping
        fifoEngId = s_grPgSmcFifoEngIdGet_GA10B(sysPipeIdx);

        // if fifoEngId is INVALID skip the loop and continue to the next iteration
        if (fifoEngId == PMU_ENGINE_ILWALID)
        {
            continue;
        }
        //
        // Wait till the preemption is complete, or it fails with either of below -
        //  1. Lpwr task got a pending wakeup event  OR
        //  2. Preemption request timed out
        //
        do
        {
            // Return early if any of the failure conditions is met
            if ((osPTimerTimeNsElapsedGet(&preemptStartTimeNs) >= (FIFO_PREEMPT_PENDING_TIMEOUT_US * 1000)) ||
                (PMUCFG_FEATURE_ENABLED(PMU_GR_WAKEUP_PENDING) &&
                 grWakeupPending()))
            {
                status = FLCN_ERROR;
                goto s_grPgFifoPreemptSequence_GA10B_exit;
            }

            runlistPriBase = GET_ENG_RUNLIST_PRI_BASE(fifoEngId);
            regVal = REG_RD32(FECS, runlistPriBase + LW_RUNLIST_PREEMPT);
        }
        while (!FLD_TEST_DRF(_RUNLIST, _PREEMPT, _RUNLIST_PREEMPT_PENDING, _FALSE, regVal));
    }
    FOR_EACH_INDEX_IN_MASK_END;

s_grPgFifoPreemptSequence_GA10B_exit:

    return status;
}

/*!
 * @brief Get Fifo Engine ID from SysPipe Mapping
 * For GA10B following is the mapping:
 * Syspipe0 => GR0
 * Syspipe1 => GR1
 */
LwU32
s_grPgSmcFifoEngIdGet_GA10B(LwU32 sysPipeIndex)
{
    LwU32 fifoEngId;

    switch (sysPipeIndex)
    {
        case 0:
        {
            fifoEngId = PMU_ENGINE_GR;
            break;
        }
        case 1:
        {
            fifoEngId = PMU_ENGINE_GR1;
            break;
        }
        default:
        {
            fifoEngId = PMU_ENGINE_ILWALID;
            break;
        }
    }
    return fifoEngId;
}

/*!
 * @brief Get max syspipes for GA10B and GA10F
 */
static LwU32
s_grMaxSysPipesGet_GA10BF(void)
{
    LwU32 maxSysPipe = 0;

    // 
    // In GA10B and GA11B: there are 2 FECS
    //
    // In GA10F: there is 1 FECS
    // GA10F uses GA10B HALS, so use
    // chip id to get the number of FECS
    // for the chip
    //
    if (IsGPU(_GA10B) || IsGPU(_GA11B))
    {
        maxSysPipe = LW_PSMCARB_MAX_PARTITIONABLE_SYS_PIPES;
    }
    else if (IsGPU(_GA10F))
    {
        maxSysPipe = 1U;
    }
    else
    {
        PMU_HALT();
    }
    return maxSysPipe;
}

/*!
 * @brief Get number of GPC for GA10B and GA10F
 */
static LwU32
s_grNumGpcGet_GA10BF(void)
{
    LwU32 numGpc = 0;

    // 
    // For GA10B:
    // In legacy mode, syspipe0 is used for 2 GPCs
    // In SMC mode, either syspipe0, syspipe1 or
    // both are used to drive 2 GPCs.
    //
    // In GA10F: there is 1 GPC and
    // In GA11B: there is are 4 GPCs
    // 
    // GA10F and GA11B use GA10B HALS, so use chip id to get
    // the number of GPCs available for the respective chip.
    //
    if (IsGPU(_GA10B))
    {
        numGpc = LW_SCAL_LITTER_NUM_GPCS;
    }
    else if (IsGPU(_GA10F))
    {
        numGpc = 1U;
    }
    else if (IsGPU(_GA11B))
    {
        numGpc = 4U;
    }

    else
    {
        PMU_HALT();
    }
    return numGpc;
}
