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
 * @file   pmu_grtu10X.c
 * @brief  HAL-interface for the TU10X Graphics Engine.
 */
/* ------------------------- System Includes -------------------------------- */

#include "pmusw.h"
#include "dmemovl.h"
/* ------------------------- Application Includes --------------------------- */

#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objgr.h"
#include "pmu_objlpwr.h"
#include "pmu_objfifo.h"
#include "pmu_objperf.h"
#include "dev_master.h"
#include "pmu_objfuse.h"
#include "dev_ltc.h"
#include "dev_pwr_csb.h"
#include "dev_graphics_nobundle.h"
#include "dev_graphics_nobundle_addendum.h"
#include "dev_trim.h"
#include "dev_chiplet_pwr.h"
#include "dev_fb.h"
#include "dev_top.h"
#include "dev_fuse.h"
#include "dev_pri_ringstation_gpc.h"
#include "hwproject.h"

#include "config/g_gr_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * @breif Timeout for unbind operation
 */
#define FIFO_UNBIND_TIMEOUT_US_TU10X        1000

/*
 * Macros to define number of BA Registers
 */
#define GR_BA_REG_COUNT_TU10X               4
#define GR_BA_INDEX_REG_COUNT_TU10X         5

/* ------------------------ Private Prototypes ----------------------------- */

static void        s_grPgAbort_TU10X(LwU32 abortReason, LwU32 abortStage);
static FLCN_STATUS s_grFifoPreemption_TU10X(LwU8 ctrlId);
static LwBool      s_grPrivBlockerEngage_TU10X(LwU32 *pAbortReason, LwU8 ctrlId);
static void        s_grPrivBlockerDisengage_TU10X(LwU8 ctrlId);
static LwU32       s_grClkFreqKHzGet_TU10X(LwU32 clkDomain);

/* ------------------------ Public Functions ------------------------------- */

/*!
 * @brief Initializes holdoff mask for GR.
 */
void
grInitSetHoldoffMask_TU10X(void)
{
    OBJPGSTATE *pGrState    = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);
    LwU32       holdoffMask = 0;

    if (FIFO_IS_ENGINE_PRESENT(GR))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_GR));
    }
    if (FIFO_IS_ENGINE_PRESENT(GR1))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_GR1));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE0) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE0))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE0));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE1) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE1))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE1));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE2) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE2))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE2));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE3) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE3))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE3));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE4) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE4))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE4));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE5) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE5))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE5));
    }

    pGrState->holdoffMask = holdoffMask;
}

/*!
 * @brief Initialize the data structure
 * to manage the BA state
 */
FLCN_STATUS
grBaStateInit_TU10X(void)
{
    GR_BA_STATE *pGrBaState = NULL;
    LwU8         ovlIdx     = OVL_INDEX_DMEM(lpwr);
    LwU8         idx;

    //
    // Details on these regsiters are updated in Bug: 200366399
    // BA Register list for struct GR_BA_REG
    //
    LwU32 baRegList[] =
    {
        LW_PGRAPH_PRI_GPCS_TPCS_PE_BLK_ACTIVITY_WEIGHTS_A,
        LW_PGRAPH_PRI_GPCS_TPCS_PE_BLK_ACTIVITY_WEIGHTS_B,
        LW_PGRAPH_PRI_GPCS_TPCS_PE_BLK_ACTIVITY_WEIGHTS_C,
        LW_PGRAPH_PRI_GPCS_TPCS_SM_POWER_HIPWR_THROTTLE,
    };

    // BA Index register list for struct GR_BA_INDEX_REG
    GR_BA_INDEX_REG baIndxRegList[] =
    {
        //
        // We have to set value of count = _INDEX_MAX + 1. Reason is
        // the value count represents maximum number of index registers
        // present and indexing starts from idx = 0 to idx = MaxCount.
        // So if Max count = 2, then there are in total 3 index registers.
        // so we have to allocate the memory for 2 + 1 = 3 registers
        // and indexing will start from 0 and will go upto 2.
        //
        {
            .ctrlAddr = LW_PGRAPH_PRI_GPCS_TPCS_TEX_M_BLK_ACT_INDEX,
            .dataAddr = LW_PGRAPH_PRI_GPCS_TPCS_TEX_M_BLK_ACT_DATA,
            .count    = LW_PGRAPH_PRI_GPCS_TPCS_TEX_M_BLK_ACT_INDEX_INDEX_MAX + 1,
        },
        {
            .ctrlAddr = LW_PGRAPH_PRI_GPCS_TPCS_SM_QUAD_BA_CONTROL,
            .dataAddr =  LW_PGRAPH_PRI_GPCS_TPCS_SM_QUAD_BA_DATA,
            .count    = LW_PGRAPH_PRI_GPCS_TPCS_SM_QUAD_BA_CONTROL_INDEX_MAX + 1,
        },
        {
            .ctrlAddr = LW_PGRAPH_PRI_GPCS_TPCS_SM_SFE_BA_CONTROL,
            .dataAddr = LW_PGRAPH_PRI_GPCS_TPCS_SM_SFE_BA_DATA,
            .count    = LW_PGRAPH_PRI_GPCS_TPCS_SM_SFE_BA_CONTROL_INDEX_MAX + 1,
        },
        {
            .ctrlAddr = LW_PGRAPH_PRI_GPCS_TPCS_SM_MIO_BA_CONTROL,
            .dataAddr = LW_PGRAPH_PRI_GPCS_TPCS_SM_MIO_BA_DATA,
            .count    = LW_PGRAPH_PRI_GPCS_TPCS_SM_MIO_BA_CONTROL_INDEX_MAX + 1,
        },
        {
            .ctrlAddr = LW_PGRAPH_PRI_GPCS_TPCS_SM_TTU_BA_CONTROL,
            .dataAddr = LW_PGRAPH_PRI_GPCS_TPCS_SM_TTU_BA_DATA,
            .count    = LW_PGRAPH_PRI_GPCS_TPCS_SM_TTU_BA_CONTROL_INDEX_MAX + 1,
        },
    };


    // Allocate the BA STATE data strulwtre
    Gr.pGrBaState = pGrBaState = lwosCallocType(ovlIdx, 1, GR_BA_STATE);
    if (pGrBaState == NULL)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_NO_FREE_MEM;
    }

    // Initalize the BA register structure counts
    pGrBaState->regCount      = GR_BA_REG_COUNT_TU10X;
    pGrBaState->indexRegCount = GR_BA_INDEX_REG_COUNT_TU10X;


    //
    // Allocate memory for BA Registers structure
    // Number of structures allocated are equal to pGrBaState->regCount
    //
    pGrBaState->pReg = lwosCallocType(ovlIdx, GR_BA_REG_COUNT_TU10X, GR_BA_REG);
    if (pGrBaState->pReg == NULL)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_NO_FREE_MEM;
    }


    //
    // Allocate memory for BA Index registers structure
    // Number of structures allocated are equal to pGrBaState->indexRegCount
    //
    pGrBaState->pIndexReg =
        lwosCallocType(ovlIdx, GR_BA_INDEX_REG_COUNT_TU10X, GR_BA_INDEX_REG);
    if (pGrBaState->pIndexReg == NULL)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_NO_FREE_MEM;
    }

    // Initalize the BA REG register addresses
    for (idx = 0; idx < pGrBaState->regCount; idx++)
    {
        // Initialize the register address
        pGrBaState->pReg[idx].addr = baRegList[idx];
    }

    // Initalize the BA Index REG register addresses
    for (idx = 0; idx < pGrBaState->indexRegCount; idx++)
    {
        // Initialize the register address and number of registers
        pGrBaState->pIndexReg[idx].ctrlAddr = baIndxRegList[idx].ctrlAddr;
        pGrBaState->pIndexReg[idx].dataAddr = baIndxRegList[idx].dataAddr;
        pGrBaState->pIndexReg[idx].count    = baIndxRegList[idx].count;


         // Allocate the memory for all index registers's data
        pGrBaState->pIndexReg[idx].pData =
            lwosCallocType(ovlIdx, pGrBaState->pIndexReg[idx].count, LwU32);
        if (pGrBaState->pIndexReg[idx].pData == NULL)
        {
            PMU_BREAKPOINT();
            return FLCN_ERR_NO_FREE_MEM;
        }
    }
    return FLCN_OK;
}

/*!
 * @brief Save all the BA Regs
 */
void
grBaStateSave_TU10X(void)
{
    GR_BA_REG       *pGrBaReg    = Gr.pGrBaState->pReg;
    GR_BA_INDEX_REG *pGrBaIdxReg = Gr.pGrBaState->pIndexReg;
    LwU32            regNum;
    LwU32            idx;

    // Save GR_BA_REG state
    for (regNum = 0; regNum < Gr.pGrBaState->regCount; regNum++)
    {
        pGrBaReg[regNum].data = REG_RD32(FECS, pGrBaReg[regNum].addr);
    }

    // Save GR_BA_INDEX_REG state
    for (regNum = 0; regNum < Gr.pGrBaState->indexRegCount; regNum++)
    {
        //
        // Set the index to zero. Index will keep on incrementing automcatically
        // as we read the Data regsites.
        //
        REG_WR32(FECS, pGrBaIdxReg[regNum].ctrlAddr, 0);

        for (idx = 0; idx < pGrBaIdxReg[regNum].count; idx++)
        {
            pGrBaIdxReg[regNum].pData[idx] = REG_RD32(FECS,
                                                      pGrBaIdxReg[regNum].dataAddr);
        }
    }
}

/*!
 * @brief Restore all the BA Regs
 */
void
grBaStateRestore_TU10X(void)
{
    GR_BA_REG       *pGrBaReg    = Gr.pGrBaState->pReg;
    GR_BA_INDEX_REG *pGrBaIdxReg = Gr.pGrBaState->pIndexReg;
    LwU32            regNum;
    LwU32            idx;

    // Restore GR_BA_REG state
    for (regNum = 0; regNum < Gr.pGrBaState->regCount; regNum++)
    {
        REG_WR32(FECS, pGrBaReg[regNum].addr, pGrBaReg[regNum].data);
    }

    // Restore GR_BA_INDEX_REG state
    for (regNum = 0; regNum < Gr.pGrBaState->indexRegCount; regNum++)
    {
        //
        // Set the index to zero. Index will keep on incrementing automcatically
        // as we write the Data regsites.
        //
        REG_WR32(FECS, pGrBaIdxReg[regNum].ctrlAddr, 0);

        for (idx = 0; idx < pGrBaIdxReg[regNum].count; idx++)
        {
            REG_WR32(FECS, pGrBaIdxReg[regNum].dataAddr,
                     pGrBaIdxReg[regNum].pData[idx]);
        }
    }

    //
    // Bug : 200443717
    // Read a PGRAPH GPCS_TPCS BA register as well as FECS fence to ensure 
    // all the writes issued by BA are flushed out before further access
    //
    REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_PE_BLK_ACTIVITY_WEIGHTS_A);
    REG_RD32(FECS, LW_PPRIV_GPC_PRI_FENCE);
}

/*!
 * @brief Restore the global non-ctxsw state via FECS
 */
void
grGlobalStateRestore_TU10X(LwU8 ctrlId)
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
}

/*!
 * @brief Check if MMU PRI FIFO is empty
 *
 * return   LW_TRUE     PRI FIFO is empty
 *          LW_FALSE    Otherwise
 */
LwBool
grIsMmuPriFifoEmpty_TU10X(void)
{
    LwU32 val;

    val = REG_RD32(BAR0, LW_PFB_PRI_MMU_CTRL);
    return (FLD_TEST_DRF(_PFB, _PRI_MMU_CTRL, _PRI_FIFO_EMPTY, _TRUE, val)) ?
            LW_TRUE : LW_FALSE;
}

/*!
 * @brief Unbind GR engine
 *
 * Issue an unbind request by binding to peer aperture(0x1 = peer).
 *
 * The mmu fault will not happen when the bind to peer aperture is issued.
 * The fault happens if an engine attempts a translation after the bind.
 *
 * Engine needs to be rebound to a different aperture before it issues any
 * requests to the MMU to avoid fault.
 *
 * For graphics - this will ilwalidate the GPC MMU, L2 TLBs. Hub MMU will
 * not send ilwalidates to GPC MMU when gr engine is not bound.
 *
 * Unbind ilwalidates all GR TLB entries. This will avoid GR traffic caused
 * by eviction of TLB entries.
 *
 * NOTE: Until Turing, we can only issue an Unbind request. We don't need to
 *       issue any request to disable the Unbind
 *
 * @param[in] bUnbind   boolean to specify enable/disable unbind
 *                      We never send LW_FALSE for this HAL
 */
FLCN_STATUS
grUnbind_TU10X(LwU8 ctrlId, LwBool bUnbind)
{
    LwU32 val;

    if (bUnbind)
    {
        //
        // Use critical section to avoid context switch. This helps minimizing
        // time between checking for FIFO empty and putting new request in FIFO.
        //
        appTaskCriticalEnter();

        // Return early if PRI fifo is not empty
        if (!grIsMmuPriFifoEmpty_HAL(&Gr))
        {
            appTaskCriticalExit();
    
            return FLCN_ERROR;
        }

        // Return early if previous bind request is in progress
        val = REG_RD32(BAR0, LW_PFB_PRI_MMU_BIND);
        if (FLD_TEST_DRF(_PFB, _PRI_MMU_BIND, _TRIGGER, _TRUE, val))
        {
            appTaskCriticalExit();
    
            return FLCN_ERROR;
        }

        // Trigger unbind request by setting aperture to 0x1
        REG_WR32(BAR0, LW_PFB_PRI_MMU_BIND_IMB,
                DRF_NUM(_PFB, _PRI_MMU_BIND_IMB, _APERTURE, 0x1));

        REG_WR32(BAR0, LW_PFB_PRI_MMU_BIND,
                DRF_NUM(_PFB, _PRI_MMU_BIND, _ENGINE_ID,
                        LW_PFB_PRI_MMU_BIND_ENGINE_ID_VEID0) |
                DRF_DEF(_PFB, _PRI_MMU_BIND, _TRIGGER, _TRUE));
    
        appTaskCriticalExit();

        // Poll for completion of unbind request
        if (!PMU_REG32_POLL_US(
                LW_PFB_PRI_MMU_BIND,
                DRF_SHIFTMASK(LW_PFB_PRI_MMU_BIND_TRIGGER),
                DRF_DEF(_PFB, _PRI_MMU_BIND, _TRIGGER, _FALSE),
                FIFO_UNBIND_TIMEOUT_US_TU10X, PMU_REG_POLL_MODE_BAR0_EQ))
        {
            PMU_BREAKPOINT();
        }

        //
        // There is possibility that cache eviction came when PMU was processing
        // unbind. This eviction can trigger GR traffic. This will assert idle
        // flip. We have reach to point of no return. Thus clear idle flip here.
        //
        grClearIdleFlip_HAL(&Gr, ctrlId);
    }

    return FLCN_OK;
}

/*!
 * Save GR's state
 *
 * @returns FLCN_OK    when saved successfully.
 * @returns FLCN_ERROR otherwise
 */
FLCN_STATUS
grPgSave_TU10X(void)
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
        s_grPgAbort_TU10X(abortReason,
                          GR_ABORT_CHKPT_AFTER_PG_ON);

        return FLCN_ERROR;
    }

    // Engage Priv Blockers (Turing and later GPUs)
    if (!s_grPrivBlockerEngage_TU10X(&abortReason, RM_PMU_LPWR_CTRL_ID_GR_PG))
    {
        s_grPgAbort_TU10X(abortReason,
                          GR_ABORT_CHKPT_AFTER_PRIV_BLOCKER);

        return FLCN_ERROR;
    }

    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libFifo));

    if (!LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, NEW_HOST_SEQ))
    {
        //
        // While disabling engine activity (scheduler, preemption), we need to make
        // sure RM does not touch these registers.
        //
        if (fifoMutexAcquire(&pGrSeqCache->fifoMutexToken) != FLCN_OK)
        {
            s_grPgAbort_TU10X(GR_ABORT_REASON_MUTEX_ACQUIRE_ERROR,
                              GR_ABORT_CHKPT_AFTER_OVERLAY_LOAD);
    
            return FLCN_ERROR;
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
            s_grPgAbort_TU10X(abortReason,
                              GR_ABORT_CHKPT_AFTER_SCHEDULER_DISABLE);

            return FLCN_ERROR;
        }

        // Enable engine holdoffs before exelwting preempt sequence
        if (FLCN_OK != lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_GR_PG,
                                                     pGrState->holdoffMask))
        {
            s_grPgAbort_TU10X(GR_ABORT_REASON_HOLDOFF_ENABLE_TIMEOUT,
                              GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_0);

            return FLCN_ERROR;
        }
    }

    // Checkpoint 3: Check GR Idleness
    if (!grIsIdle_HAL(&Gr, &abortReason, RM_PMU_LPWR_CTRL_ID_GR_PG))
    {
        s_grPgAbort_TU10X(abortReason,
                         GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_0);

        return FLCN_ERROR;
    }

    //
    // Execute preemption sequence. As part of preemption sequence host saves
    // channel and engine context.
    //
    if (s_grFifoPreemption_TU10X(RM_PMU_LPWR_CTRL_ID_GR_PG) != FLCN_OK)
    {
        s_grPgAbort_TU10X(GR_ABORT_REASON_FIFO_PREEMPT_ERROR,
                         GR_ABORT_CHKPT_AFTER_PREEMPT_SEQUENCE);

        return FLCN_ERROR;
    }

    // Re-enable holdoff: Holdoff was disengaged by preemption sequence
    if (FLCN_OK != lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_GR_PG,
                                                 pGrState->holdoffMask))
    {
        s_grPgAbort_TU10X(GR_ABORT_REASON_HOLDOFF_ENABLE_TIMEOUT,
                         GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_1);

        return FLCN_ERROR;
    }

    if (!LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, NEW_HOST_SEQ))
    {
        // Re-enable scheduling for GR runlist
        if (!pGrSeqCache->bSkipSchedEnable)
        {
            fifoSchedulerEnable_HAL(&Fifo, PMU_ENGINE_GR);
            pGrSeqCache->bSkipSchedEnable = LW_TRUE;
        }
    }

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, NEW_HOST_SEQ))
    {
        // Check if the context is still INVALID
        if (!grIsCtxIlwalid_HAL(&Gr))
        {
            s_grPgAbort_TU10X(GR_ABORT_REASON_FIFO_PREEMPT_ERROR,
                            GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_1);

            return FLCN_ERROR;
        }
    }

    // Checkpoint 4: Check GR Idleness
    if (!grIsIdle_HAL(&Gr, &abortReason, RM_PMU_LPWR_CTRL_ID_GR_PG))
    {
        s_grPgAbort_TU10X(abortReason,
                         GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_1);

        return FLCN_ERROR;
    }

    // Save global state via FECS
    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, SAVE_GLOBAL_STATE))
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_GR_PERFMON_RESET_WAR))
        {
            // Save the PERFMON CG state
            grPerfmonWarStateSave_HAL(&Gr);
        }

        if (FLCN_OK != grGlobalStateSave_HAL(&Gr, RM_PMU_LPWR_CTRL_ID_GR_PG))
        {
            s_grPgAbort_TU10X(GR_ABORT_REASON_REGLIST_SAVE_TIMEOUT,
                             GR_ABORT_CHKPT_AFTER_SAVE_REGLIST_REQUEST);
        }
    }

    //
    // Wait for the GR subunit to become idle. GR Entry
    // sequence can cause the GR signals to reprot busy
    // momentarily.
    //
    if (!grWaitForSubunitsIdle_HAL(&Gr))
    {
        s_grPgAbort_TU10X(GR_ABORT_REASON_SUBUNIT_BUSY,
                          GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_1);
        return FLCN_ERROR;
    }

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, PRIV_RING))
    {
        grPgPrivErrHandlingActivate_HAL(&Gr, LW_TRUE);
    }

    if (!LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, NEW_HOST_SEQ))
    {
        //
        // All FIFO register accesses should be done by this time. Release the
        // fifo mutex that was acquired at the start of this function.
        //
        PMU_HALT_COND(pGrSeqCache->fifoMutexToken != LW_U8_MAX);
        fifoMutexRelease(pGrSeqCache->fifoMutexToken);
    }

    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libFifo));

    if (PMUCFG_FEATURE_ENABLED(PMU_PG_GR_UNIFIED_POWER_SEQ) &&
        LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, HW_SEQUENCE))
    {
        grHwPgEngage_HAL(&Gr);
    }

    return FLCN_OK;
}

/*!
 * Restores the state of GR engine
 *
 * @returns FLCN_OK
 */
FLCN_STATUS
grPgRestore_TU10X(void)
{
    OBJPGSTATE *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);
    FLCN_STATUS status;

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
        if (PMUCFG_FEATURE_ENABLED(PMU_GR_PERFMON_RESET_WAR))
        {
            // Reset Perfmon unit
            grPerfmonWarReset_HAL(&Gr);
        }

        grPgAssertEngineReset(LW_FALSE);
    }

    // Issue the SRAM ECC scrubbing
    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, ECC_SCRUB_SRAM))
    {
        status = grIssueSramEccScrub_HAL(&Gr);
        if (status != FLCN_OK)
        {
            return status;
        }
    }

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, SAVE_GLOBAL_STATE))
    {
        grGlobalStateRestore_HAL(&Gr, RM_PMU_LPWR_CTRL_ID_GR_PG);

        // Restore the BA registers.
        grBaStateRestore_HAL(&Gr);

        if (PMUCFG_FEATURE_ENABLED(PMU_GR_PERFMON_RESET_WAR))
        {
            // Save the PERFMON CG state
            grPerfmonWarStateRestore_HAL(&Gr);
        }
    }

    // Disengage the Priv Blockers
    s_grPrivBlockerDisengage_TU10X(RM_PMU_LPWR_CTRL_ID_GR_PG);

    // Disable holdoff
    lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_GR_PG,
                                  LPWR_HOLDOFF_MASK_NONE);

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Channel preemption sequence
 *
 * We need to clear Idle Flip bit here because this step will itself
 * generate internal traffic and flip idleness
 */
static FLCN_STATUS
s_grFifoPreemption_TU10X(LwU8 ctrlId)
{
    FLCN_STATUS status;

    status = fifoPreemptSequence_HAL(&Fifo, PMU_ENGINE_GR, ctrlId);

    // Clear idle flip corresponding to GR_FE and GR_PRIV
    grClearIdleFlip_HAL(&Gr, ctrlId);

    return status;
}

/*!
 * @brief Abort GR sequence, perform backout sequence as needed
 *
 * @param[in]  abortReason     Reason for aborting gr during entry sequence
 * @param[in]  abortStage      Stage of entry sequence where gr is aborted,
 *                             this determines the steps to be performed
 *                             in abort sequence
 */
static void
s_grPgAbort_TU10X
(
   LwU32  abortReason,
   LwU32  abortStage
)
{
    GR_SEQ_CACHE *pGrSeqCache = GR_SEQ_GET_CACHE();
    OBJPGSTATE   *pGrState    = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);

    pGrState->stats.abortReason = (abortStage | abortReason);

    switch (abortStage)
    {
        //
        // Do not change the order of case sections.
        // No break in each case except the last one.
        // Do not compile out "case" using #if(PMUCFG_FEATURE_ENABLED()).
        //
        case GR_ABORT_REASON_REGLIST_SAVE_TIMEOUT:
        case GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_1:
        {
            // Disable Holdoff for new host sequence here
            if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, NEW_HOST_SEQ))
            {
                // Disable holdoffs
                lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_GR_PG,
                                              LPWR_HOLDOFF_MASK_NONE);
            }


            // coverity[fallthrough]
        }
        case GR_ABORT_CHKPT_AFTER_PREEMPT_SEQUENCE:
        case GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_0:
        {
            // Disable Holdoff for legacy host sequence here
            if (!LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, NEW_HOST_SEQ))
            {
                // Disable holdoffs
                lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_GR_PG,
                                              LPWR_HOLDOFF_MASK_NONE);
            }

            // coverity[fallthrough]
        }
        case GR_ABORT_CHKPT_AFTER_SCHEDULER_DISABLE:
        {
            if (!LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, NEW_HOST_SEQ))
            {
                // Enable scheduling
                if (!pGrSeqCache->bSkipSchedEnable)
                {
                    fifoSchedulerEnable_HAL(&Fifo, PMU_ENGINE_GR);
                }

                // Release the fifo mutex
                PMU_HALT_COND(pGrSeqCache->fifoMutexToken != LW_U8_MAX);
                fifoMutexRelease(pGrSeqCache->fifoMutexToken);
            }

            // coverity[fallthrough]
        }
        case GR_ABORT_CHKPT_AFTER_OVERLAY_LOAD:
        {
            OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libFifo));

            // coverity[fallthrough]
        }
        case GR_ABORT_CHKPT_AFTER_PRIV_BLOCKER:
        {
            // Disengage the Priv Blockers
            s_grPrivBlockerDisengage_TU10X(RM_PMU_LPWR_CTRL_ID_GR_PG);

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
 * @brief Engage the GR Priv Blockers
 *
 * Below are the steps this function does:
 *      1. Engage the Blocker to Block All Mode
 *      2. Issue the priv flush
 *      3. Check for GR Idleness
 *      4. Move the Blocker to Block Equation Mode
 *
 *  TODO-pankumar: split this function for GR-PG and GR-RG
 *  to make it more readable
 *
 * @param[in]  pAbortReason    Pointer to send back Abort Reason
 *
 * @return     LW_TRUE         Blockers are engaged.
 *             LW_FALSE        otherwise
 */
LwBool
s_grPrivBlockerEngage_TU10X
(
    LwU32 *pAbortReason,
    LwU8   ctrlId
)
{
    LwBool bXveEnabled  = LW_FALSE;
    LwBool bSec2Enabled = LW_FALSE;
    LwBool bGspEnabled  = LW_FALSE;

    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_UNIFIED_PRIV_BLOCKER_CTRL))
    {
        if (FLCN_OK != lpwrCtrlPrivBlockerModeSet(ctrlId,
                                        LPWR_PRIV_BLOCKER_MODE_BLOCK_ALL,
                                        LPWR_PRIV_BLOCKER_PROFILE_GR,
                                        (GR_PRIV_BLOCKER_ENGAGE_TIMEOUT_US * 1000)))
        {
            *pAbortReason = GR_ABORT_REASON_PRIV_BLKR_ALL_TIMEOUT;
            return LW_FALSE;
        }
    }

    // Issue the PRIV path Flush
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_UNIFIED_PRIV_FLUSH_SEQ))
    {
        OBJPGSTATE *pGrState = GET_OBJPGSTATE(ctrlId);

        // Check for XVE/SEC/GSP state with GR features
        bSec2Enabled = LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, SEC2);
        bGspEnabled  = LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, GSP);
        bXveEnabled  = LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, XVE);

        if (!lpwrPrivPathFlush_HAL(&Lpwr, bXveEnabled, bSec2Enabled, bGspEnabled,
                                   LPWR_PRIV_FLUSH_DEFAULT_TIMEOUT_NS))
        {
            *pAbortReason = GR_ABORT_REASON_PRIV_PATH_FLUSH_TIMEOUT;
            return LW_FALSE;
        }
    }

    // Check GR Idleness
    if (!grIsIdle_HAL(&Gr, pAbortReason, ctrlId))
    {
        return LW_FALSE;
    }

    // Engage the Blocker in the Block Equation Mode
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_UNIFIED_PRIV_BLOCKER_CTRL))
    {
        if (FLCN_OK != lpwrCtrlPrivBlockerModeSet(ctrlId,
                                        LPWR_PRIV_BLOCKER_MODE_BLOCK_EQUATION,
                                        LPWR_PRIV_BLOCKER_PROFILE_GR,
                                        (GR_PRIV_BLOCKER_ENGAGE_TIMEOUT_US * 1000)))
        {
            *pAbortReason = GR_ABORT_REASON_PRIV_BLKR_EQU_TIMEOUT;
            return LW_FALSE;
        }
    }

    return LW_TRUE;
}

/*!
 * @brief Disengage the GR Priv Blockers
 *
 * @param[in]  ctrlId  LPWR_CTRL Id
 */
void
s_grPrivBlockerDisengage_TU10X
(
    LwU8   ctrlId
)
{
    // Disengage the Priv Blockers
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_UNIFIED_PRIV_BLOCKER_CTRL))
    {
        lpwrCtrlPrivBlockerModeSet(ctrlId, LPWR_PRIV_BLOCKER_MODE_BLOCK_NONE,
                                   LPWR_PRIV_BLOCKER_PROFILE_GR, 0);
    }
}

/*!
 * Capture GR debug info to be exposed to RM in case of PMU HALT
 * 
 * Lwrrently we are exposing the following -
 *  - FECS mailbox registers (2,3,6 and 8)
 *  - ECC control registers
 *  - GPCCS mailbox registers (2 and 3 for all GPCCS)
 *  - Clock frequencies
 *  - FECS method status register (0)
 *  - FECS method processing time
 */
void
grDebugInfoCapture_TU10X(void)
{
    LwU32 regVal         = 0;
    LwU32 gpcId          = 0;
    LwU32 gpcLogicalId   = 0;
    LwU32 mailboxId      = 0;
    LwU32 maxNumGpcs     = 0;
    LwU32 gpcEnabledMask = 0;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_CLK_COUNTER
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);

    // Cache FECS mailbox registers (THERM_SCRATCH 0-3)
    regVal = REG_RD32(FECS, LW_PGRAPH_PRI_FECS_CTXSW_MAILBOX(2));
    REG_WR32(CSB, LW_CPWR_THERM_SCRATCH(0), regVal);
    regVal = REG_RD32(FECS, LW_PGRAPH_PRI_FECS_CTXSW_MAILBOX(3));
    REG_WR32(CSB, LW_CPWR_THERM_SCRATCH(1), regVal);
    regVal = REG_RD32(FECS, LW_PGRAPH_PRI_FECS_CTXSW_MAILBOX(6));
    REG_WR32(CSB, LW_CPWR_THERM_SCRATCH(2), regVal);
    regVal = REG_RD32(FECS, LW_PGRAPH_PRI_FECS_CTXSW_MAILBOX(8));
    REG_WR32(CSB, LW_CPWR_THERM_SCRATCH(3), regVal);

    // Cache ECC control registers (THERM SCRATCH 4-7 and PMU_QUEUE_TAIL 4)
    regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL);
    REG_WR32(CSB, LW_CPWR_THERM_SCRATCH(4), regVal);
    regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_L1_DATA_ECC_CONTROL);
    REG_WR32(CSB, LW_CPWR_THERM_SCRATCH(5), regVal);
    regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_L1_TAG_ECC_CONTROL);
    REG_WR32(CSB, LW_CPWR_THERM_SCRATCH(6), regVal);
    regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_CBU_ECC_CONTROL);
    REG_WR32(CSB, LW_CPWR_THERM_SCRATCH(7), regVal);
    regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_ICACHE_ECC_CONTROL);
    REG_WR32(CSB, LW_CPWR_PMU_QUEUE_TAIL(4), regVal);

    // Get the GPC enabled mask using max supported GPCs and floorswept GPCs
    maxNumGpcs = REG_FLD_RD_DRF(BAR0, _PTOP, _SCAL_NUM_GPCS, _VALUE);
    gpcEnabledMask = (LWBIT32(maxNumGpcs) - 1) & (~fuseRead(LW_FUSE_STATUS_OPT_GPC));

    // Cache GPCCS mailbox registers (PMU_MAILBOX 0-11)
    FOR_EACH_INDEX_IN_MASK(32, gpcId, gpcEnabledMask) 
    {
        regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPC0_GPCCS_CTXSW_MAILBOX(2) + (gpcLogicalId * LW_GPC_PRI_STRIDE));
        REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(mailboxId), regVal);
        mailboxId++;

        regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPC0_GPCCS_CTXSW_MAILBOX(3) + (gpcLogicalId * LW_GPC_PRI_STRIDE));
        REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(mailboxId), regVal);
        mailboxId++;

        gpcLogicalId++;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    // Cache the clock frequencies (PMU_QUEUE_HEAD 4-7)
    regVal = s_grClkFreqKHzGet_TU10X(LW2080_CTRL_CLK_DOMAIN_GPCCLK);
    REG_WR32(CSB, LW_CPWR_PMU_QUEUE_HEAD(4), regVal);
    regVal = s_grClkFreqKHzGet_TU10X(LW2080_CTRL_CLK_DOMAIN_SYSCLK);
    REG_WR32(CSB, LW_CPWR_PMU_QUEUE_HEAD(5), regVal);
    regVal = s_grClkFreqKHzGet_TU10X(LW2080_CTRL_CLK_DOMAIN_MCLK);
    REG_WR32(CSB, LW_CPWR_PMU_QUEUE_HEAD(6), regVal);
    regVal = s_grClkFreqKHzGet_TU10X(LW2080_CTRL_CLK_DOMAIN_LWDCLK);
    REG_WR32(CSB, LW_CPWR_PMU_QUEUE_HEAD(7), regVal);

    // Cache the FECS method status register (PMU_QUEUE_TAIL 5)
    regVal = REG_RD32(FECS, LW_PGRAPH_PRI_FECS_CTXSW_MAILBOX(0));
    REG_WR32(CSB, LW_CPWR_PMU_QUEUE_TAIL(5), regVal);

    // Cache the FECS method processing time (PMU_QUEUE_TAIL 6)
    REG_WR32(CSB, LW_CPWR_PMU_QUEUE_TAIL(6), Gr.stateMethodTimeUs);

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
}

/*!
 * Get the frequency of given clock domain
 *
 * @param[in]  clkDomain     Clock domain to get the frequency
 *
 * @return Frequency of the given clock domain in Khz
 */
static LwU32
s_grClkFreqKHzGet_TU10X
(
    LwU32 clkDomain
)
{
    CLK_CNTR_AVG_FREQ_START clkCntrSample;
    LwU32                   clkFreqkHz;

    // 1. Get the initial clock counter reading
    clkFreqkHz = clkCntrAvgFreqKHz(clkDomain, 
                    LW2080_CTRL_CLK_DOMAIN_PART_IDX_UNDEFINED, &clkCntrSample);

    // 2. Wait for 2 us
    OS_PTIMER_SPIN_WAIT_US(2);

    // 3. Diff the counter to callwlate the frequency.
    clkFreqkHz = clkCntrAvgFreqKHz(clkDomain, 
                    LW2080_CTRL_CLK_DOMAIN_PART_IDX_UNDEFINED, &clkCntrSample);

    return clkFreqkHz;
}
