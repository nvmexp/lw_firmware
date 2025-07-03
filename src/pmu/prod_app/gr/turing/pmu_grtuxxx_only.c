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
 * @file   pmu_grtuxxx_only.c
 * @brief  HAL-interface for the TUXXX Graphics Engine.
 */
/* ------------------------- System Includes -------------------------------- */

#include "pmusw.h"
#include "dmemovl.h"
#include "dev_graphics_nobundle.h"
#include "dev_pmgr.h"
#include "dev_master.h"
#include "dev_fifo.h"
#include "dev_perf.h"
#include "dev_bus.h"

/* ------------------------- Application Includes --------------------------- */

#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objlpwr.h"
#include "pmu_objgr.h"
#include "pmu_objfifo.h"
#include "dev_pwr_csb.h"
#include "mscg_wl_bl_init.h"
#include "pmu_bar0.h"
#include "pmu/pmumailboxid.h"

#include "config/g_gr_private.h"

/* ------------------------- Macros and Defines ----------------------------- */

/*!
 * Max Poll time can be 80 us according to callwlations i.e.
 * (NUM_ACTIVE_GROUPS * 8(zones present in every partition) * (ZONE_DELAY) * PMU_Utilsclk_cycle)
 * (64 * 8 * 15(max as it is 4 bit value) * (9.8ns)
 */
#define GR_UNIFIED_SEQ_POLL_TIME_US_TU10X   80

/*!
 * @brief Broadcast access for FBP and GPC CG chiplet registers
 *
 * TODO : Move these to dev_perf_addendum.h file with proper names
 */
#define LW_PERF_PMMFBPROUTER_CG2_BC    0x00251A18
#define LW_PERF_PMMGPCROUTER_CG2_BC    0x00251818

/* ------------------------ Private Prototypes ----------------------------- */
static void s_grPgEngageSram_TU10X(LwBool bBlocking);
static void s_grPgDisengageSram_TU10X(void);
static void s_grPgEngageLogic_TU10X(LwBool bBlocking);
static void s_grPgDisengageLogic_TU10X(void);

/* ------------------------ Public Functions ------------------------------- */

/*!
 * @brief Initializes the idle mask for GR.
 */
void
grInitSetIdleMasks_TU10X(void)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);
    LwU32       im0      = 0;
    LwU32       im1      = 0;
    LwU32       im2      = 0;

    im0 = DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR,     _ENABLED) |
          DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR_SYS, _ENABLED) |
          DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR_GPC, _ENABLED) |
          DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR_BE,  _ENABLED);

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

    if (FIFO_IS_ENGINE_PRESENT(CE4) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE4))
    {
        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_4, _ENABLED, im1);
    }

    if (FIFO_IS_ENGINE_PRESENT(SEC2))
    {
        im2 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _SEC_PRIV_BLOCKER_GR,
                          _ENABLED, im2);
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
 * @brief Initializes the idle mask for GR.
 */
void
grPassiveIdleMasksInit_TU10X(void)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PASSIVE);
    LwU32       im0      = 0;
    LwU32       im1      = 0;
    LwU32       im2      = 0;

    im0 = DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR,     _ENABLED) |
          DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR_SYS, _ENABLED) |
          DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR_GPC, _ENABLED) |
          DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR_BE,  _ENABLED);

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

    if (FIFO_IS_ENGINE_PRESENT(CE4) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE4))
    {
        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_4, _ENABLED, im1);
    }

    if (FIFO_IS_ENGINE_PRESENT(SEC2))
    {
        im2 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _SEC_PRIV_BLOCKER_GR,
                          _ENABLED, im2);
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
 * @brief Clear idle flip corresponding to  GR_FE and GR_PRIV
 */
void
grClearIdleFlip_TU10X(LwU8 ctrlId)
{
    LwU32 hwEngIdx = PG_GET_ENG_IDX(ctrlId);
    LwU32 val;

    // Set IDX to current engine for Idle flip
    val = REG_RD32(CSB, LW_CPWR_PMU_PG_MISC);
    val = FLD_SET_DRF_NUM(_CPWR, _PMU_PG_MISC, _IDLE_FLIP_STATUS_IDX,
                          hwEngIdx, val);
    REG_WR32(CSB, LW_CPWR_PMU_PG_MISC, val);

    // Set GR_FE and GR_PRIV bits to be IDLE
    val = REG_RD32(CSB, LW_CPWR_PMU_PG_IDLE_FLIP_STATUS);
    val = FLD_SET_DRF(_CPWR, _PMU_PG_IDLE_FLIP_STATUS, _GR_FE,
                      _IDLE, val);
    val = FLD_SET_DRF(_CPWR, _PMU_PG_IDLE_FLIP_STATUS, _GR_PRIV_IDLE_SYS,
                      _IDLE, val);
    val = FLD_SET_DRF(_CPWR, _PMU_PG_IDLE_FLIP_STATUS, _GR_PRIV_IDLE_GPC,
                      _IDLE, val);
    val = FLD_SET_DRF(_CPWR, _PMU_PG_IDLE_FLIP_STATUS, _GR_PRIV_IDLE_BE,
                      _IDLE, val);
    val = FLD_SET_DRF(_CPWR, _PMU_PG_IDLE_FLIP_STATUS, _CE_0,
                      _IDLE, val);
    val = FLD_SET_DRF(_CPWR, _PMU_PG_IDLE_FLIP_STATUS, _CE_1,
                      _IDLE, val);
    REG_WR32_STALL(CSB, LW_CPWR_PMU_PG_IDLE_FLIP_STATUS, val);
}

/*!
 * @brief Power-gate GR engine.
 */
void
grHwPgEngage_TU10X(void)
{
    OBJPGSTATE *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, ELPG_SRAM))
    {
        s_grPgEngageSram_TU10X(LW_TRUE);
    }

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, ELPG_LOGIC))
    {
        s_grPgEngageLogic_TU10X(LW_FALSE);
    }
}

/*!
 * @brief Power-ungate GR engine.
 */
void
grHwPgDisengage_TU10X(void)
{
    OBJPGSTATE *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, ELPG_LOGIC))
    {
        s_grPgDisengageLogic_TU10X();
    }

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, ELPG_SRAM))
    {
        s_grPgDisengageSram_TU10X();
    }
}

/*!
 * @brief Issue the SRAM ECC Control scrubbing
 *
 * @return FLCN_STATUS FLCN_OK on sucess, else returns appropriate FLCN_ERR_*.
 */
FLCN_STATUS
grIssueSramEccScrub_TUXXX(void)
{
    LwU32          regVal       = 0;
    LwBool         bStopPolling = LW_FALSE;
    FLCN_TIMESTAMP eccScrubStartTimeNs;

    // SM_LRF_ECC
    regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL,
                         _SCRUB_QRFDP0, _TASK, regVal);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL,
                         _SCRUB_QRFDP1, _TASK, regVal);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL,
                         _SCRUB_QRFDP2, _TASK, regVal);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL,
                         _SCRUB_QRFDP3, _TASK, regVal);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL,
                         _SCRUB_QRFDP4, _TASK, regVal);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL,
                         _SCRUB_QRFDP5, _TASK, regVal);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL,
                         _SCRUB_QRFDP6, _TASK, regVal);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL,
                         _SCRUB_QRFDP7, _TASK, regVal);
    REG_WR32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL, regVal);

    // L1_DATA_ECC
    regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_L1_DATA_ECC_CONTROL);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_L1_DATA_ECC_CONTROL,
                         _SCRUB_EL1_0, _TASK, regVal);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_L1_DATA_ECC_CONTROL,
                         _SCRUB_EL1_1, _TASK, regVal);
    REG_WR32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_L1_DATA_ECC_CONTROL, regVal);

    // L1_TAG_ECC
    regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_L1_TAG_ECC_CONTROL);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_L1_TAG_ECC_CONTROL,
                         _SCRUB_EL1_0, _TASK, regVal);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_L1_TAG_ECC_CONTROL,
                         _SCRUB_EL1_1, _TASK, regVal);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_L1_TAG_ECC_CONTROL,
                         _SCRUB_PIXPRF, _TASK, regVal);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_L1_TAG_ECC_CONTROL,
                         _SCRUB_MISS_FIFO, _TASK, regVal);
    REG_WR32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_L1_TAG_ECC_CONTROL, regVal);

    // CBU_ECC_CONTROL
    regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_CBU_ECC_CONTROL);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_CBU_ECC_CONTROL,
                         _SCRUB_WARP_SM0, _TASK, regVal);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_CBU_ECC_CONTROL,
                         _SCRUB_WARP_SM1, _TASK, regVal);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_CBU_ECC_CONTROL,
                         _SCRUB_BARRIER_SM0, _TASK, regVal);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_CBU_ECC_CONTROL,
                         _SCRUB_BARRIER_SM1, _TASK, regVal);
    REG_WR32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_CBU_ECC_CONTROL, regVal);

    // ICACHE_ECC_CONTROL
    regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_ICACHE_ECC_CONTROL);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_ICACHE_ECC_CONTROL,
                         _SCRUB_L0_DATA, _TASK, regVal);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_ICACHE_ECC_CONTROL,
                         _SCRUB_L0_PREDECODE, _TASK, regVal);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_ICACHE_ECC_CONTROL,
                         _SCRUB_L1_DATA, _TASK, regVal);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_ICACHE_ECC_CONTROL,
                         _SCRUB_L1_PREDECODE, _TASK, regVal);
    REG_WR32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_ICACHE_ECC_CONTROL, regVal);

    // Start time for Scrub polling
    osPTimerTimeNsLwrrentGet(&eccScrubStartTimeNs);

    // Poll for Scrubbing to finish
    do
    {
        // SM_LRF_ECC
        regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL);
        bStopPolling = (FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL,
                                     _SCRUB_QRFDP0, _INIT, regVal)           &&
                        FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL,
                                     _SCRUB_QRFDP1, _INIT, regVal)           &&
                        FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL,
                                     _SCRUB_QRFDP2, _INIT, regVal)           &&
                        FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL,
                                     _SCRUB_QRFDP3, _INIT, regVal)           &&
                        FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL,
                                     _SCRUB_QRFDP4, _INIT, regVal)           &&
                        FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL,
                                     _SCRUB_QRFDP5, _INIT, regVal)           &&
                        FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL,
                                     _SCRUB_QRFDP6, _INIT, regVal)           &&
                        FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL,
                                     _SCRUB_QRFDP7, _INIT, regVal));

        // L1 DATA ECC
        regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_L1_DATA_ECC_CONTROL);
        bStopPolling = bStopPolling &&
                       (FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_L1_DATA_ECC_CONTROL,
                                     _SCRUB_EL1_0, _INIT, regVal)             &&
                        FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_L1_DATA_ECC_CONTROL,
                                     _SCRUB_EL1_1, _INIT, regVal));

        // L1 TAG ECC
        regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_L1_TAG_ECC_CONTROL);
        bStopPolling = bStopPolling &&
                       (FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_L1_TAG_ECC_CONTROL,
                                     _SCRUB_EL1_0, _INIT, regVal)             &&
                        FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_L1_TAG_ECC_CONTROL,
                                     _SCRUB_EL1_1, _INIT, regVal)             &&
                        FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_L1_TAG_ECC_CONTROL,
                                     _SCRUB_PIXPRF, _INIT, regVal)            &&
                        FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_L1_TAG_ECC_CONTROL,
                                     _SCRUB_MISS_FIFO, _INIT, regVal));

        // CBU ECC CONTROL
        regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_CBU_ECC_CONTROL);
        bStopPolling = bStopPolling &&
                       (FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_CBU_ECC_CONTROL,
                                     _SCRUB_WARP_SM0, _INIT, regVal)          &&
                        FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_CBU_ECC_CONTROL,
                                     _SCRUB_WARP_SM1, _INIT, regVal)          &&
                        FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_CBU_ECC_CONTROL,
                                     _SCRUB_BARRIER_SM0, _INIT, regVal)       &&
                        FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_CBU_ECC_CONTROL,
                                     _SCRUB_BARRIER_SM1, _INIT, regVal));

        // ICACHE ECC CONTROL
        regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_ICACHE_ECC_CONTROL);
        bStopPolling = bStopPolling &&
                       (FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_ICACHE_ECC_CONTROL,
                                     _SCRUB_L0_DATA, _INIT, regVal)           &&
                        FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_ICACHE_ECC_CONTROL,
                                     _SCRUB_L0_PREDECODE, _INIT, regVal)      &&
                        FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_ICACHE_ECC_CONTROL,
                                     _SCRUB_L1_DATA, _INIT, regVal)           &&
                        FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_ICACHE_ECC_CONTROL,
                                     _SCRUB_L1_PREDECODE, _INIT, regVal));

        // Check for timeout for ECC Scrubbing
        if (osPTimerTimeNsElapsedGet(&eccScrubStartTimeNs) >=
            GR_SRAM_ECC_SCRUB_TIMEOUT_NS)
        {
            //
            // Capture GR debug information to be exposed to RM
            // NOTE: This is a temporary code being added to debug OCA issues
            //       (Bug 3162570)
            //
            grDebugInfoCapture_HAL(&Gr);

            PMU_BREAKPOINT();
            return FLCN_ERR_TIMEOUT;
        }
    } while (!bStopPolling);

    return FLCN_OK;
}

/*!
 * @brief Check for graphics sub system interrupts
 *
 * @return LW_TRUE  If Interrupt pending on GR
 *         LW_FALSE Otherwise
 */
LwBool
grIsIntrPending_TU10X(void)
{
    LwU32 intr0;

    intr0 = REG_RD32(BAR0, LW_PMC_INTR(0));

    if (FLD_TEST_DRF(_PMC, _INTR, _PGRAPH, _PENDING, intr0) ||
        FLD_TEST_DRF(_PMC, _INTR, _CE0,    _PENDING, intr0) ||
        FLD_TEST_DRF(_PMC, _INTR, _CE1,    _PENDING, intr0))
    {
        return LW_TRUE;
    }

    return LW_FALSE;
}

/*!
 * @brief Returns if the current context is invalid
 *
 * @return LW_TRUE  If current context is invalid
 *         LW_FALSE otherwise
 */
LwBool
grIsCtxIlwalid_TU10X(void)
{
    LwU32 fifoEngId = GET_FIFO_ENG(PMU_ENGINE_GR);
    LwU32 regVal    = 0;

    regVal = REG_RD32(BAR0, LW_PFIFO_ENGINE_STATUS(fifoEngId));

    return FLD_TEST_DRF(_PFIFO, _ENGINE_STATUS, _CTX_STATUS, _ILWALID, regVal);
}

/*!
 * @brief Initialize the data structure to manage the PERFMON state
 */
FLCN_STATUS
grPerfmonWarStateInit_TU10X(void)
{
    LwU8  ovlIdx = OVL_INDEX_DMEM(lpwr);
    LwU32 idx;

    LwU32 regList[GR_PERMON_WAR_CACHE_SIZE] =
    {
        LW_PERF_PMMSYSROUTER_CG2,
        LW_PERF_PMMFBPROUTER_CG2_BC,
        LW_PERF_PMMGPCROUTER_CG2_BC,
        LW_PERF_PMASYS_CG2,
    };


    // Allocate the PERFMON STATE cache
    Gr.pPerfmonWarCache = lwosCallocType(ovlIdx, 1, GR_PERMON_WAR_CACHE);
    if (Gr.pPerfmonWarCache == NULL)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_NO_FREE_MEM;
    }

    // Initalize the PERFMON register addresses in the cache
    for (idx = 0; idx < GR_PERMON_WAR_CACHE_SIZE; idx++)
    {
        Gr.pPerfmonWarCache->perfmonReg[idx].addr = regList[idx];
    }

    return LW_OK;
}

/*!
 * @brief Save the PERFMON state in the cache
 */
void
grPerfmonWarStateSave_TU10X(void)
{
    LwU32 idx;

    // Save the PERFMON register data in the cache
    for (idx = 0; idx < GR_PERMON_WAR_CACHE_SIZE; idx++)
    {
        Gr.pPerfmonWarCache->perfmonReg[idx].data = 
            REG_RD32(FECS, Gr.pPerfmonWarCache->perfmonReg[idx].addr);
    }
}

/*!
 * @brief Restore the PERFMON state from the cache
 */
void
grPerfmonWarStateRestore_TU10X(void)
{
    LwU32 idx;

    // Save the PERFMON register data in the cache
    for (idx = 0; idx < GR_PERMON_WAR_CACHE_SIZE; idx++)
    {
        REG_WR32(FECS, Gr.pPerfmonWarCache->perfmonReg[idx].addr, 
                       Gr.pPerfmonWarCache->perfmonReg[idx].data);
    }

    // Flush the above writes by reading the last register
    REG_RD32(FECS, Gr.pPerfmonWarCache->perfmonReg[(GR_PERMON_WAR_CACHE_SIZE - 1)].addr);
}

/*!
 * @brief Reset Perfmon Unit
 *
 * Following is the sequence -
 *  1. Cache LW_PBUS_ACCESS register
 *  2. Disable Non-posted XVE access
 *  3. Disable Posted XVE access
 *  4. Cache LW_PMC_ENABLE register
 *  5. Disable PERFMON in LW_PMC_ENABLE
 *  6. Restore LW_PMC_ENABLE
 *  7. Restore LW_PBUS_ACCESS
 */
void 
grPerfmonWarReset_TU10X(void)
{
    LwU32 cachePbus;
    LwU32 cachePmc;
    LwU32 reg32;

    appTaskCriticalEnter();
    {
        // Cache LW_PBUS_ACCESS register
        cachePbus = reg32 = REG_RD32(BAR0, LW_PBUS_ACCESS);

        // Disable Non-posted XVE access and flush the write
        reg32 = FLD_SET_DRF(_PBUS, _ACCESS, _XVE_N, _DISABLED, reg32);
        REG_WR32(BAR0, LW_PBUS_ACCESS, reg32);
        reg32 = REG_RD32(BAR0, LW_PBUS_ACCESS);

        // Disable Posted XVE access and flush the write
        reg32 = FLD_SET_DRF(_PBUS, _ACCESS, _XVE_P, _DISABLED, reg32);
        REG_WR32(BAR0, LW_PBUS_ACCESS, reg32);
        reg32 = REG_RD32(BAR0, LW_PBUS_ACCESS);

        // Cache LW_PMC_ENABLE register
        cachePmc = reg32 = REG_RD32(BAR0, LW_PMC_ENABLE);

        if (FLD_TEST_DRF(_PMC, _ENABLE, _PERFMON, _ENABLED, reg32))
        {
            // Disable PERFMON in LW_PMC_ENABLE
            reg32 = FLD_SET_DRF(_PMC, _ENABLE, _PERFMON, _DISABLED, reg32);
            REG_WR32(BAR0, LW_PMC_ENABLE, reg32);
            reg32 = REG_RD32(BAR0, LW_PMC_ENABLE);

            // Restore LW_PMC_ENABLE (re-enable PERFMON)
            REG_WR32(BAR0, LW_PMC_ENABLE, cachePmc);
            reg32 = REG_RD32(BAR0, LW_PMC_ENABLE);
        }

        // Restore LW_PBUS_ACCESS
        REG_WR32(BAR0, LW_PBUS_ACCESS, cachePbus);
        reg32 = REG_RD32(BAR0, LW_PBUS_ACCESS);
    }
    appTaskCriticalExit();
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Power-gate GR engine's SRAM.
 *
 * @param[in] bBlocking     If its a blocking call
 */
void
s_grPgEngageSram_TU10X
(
    LwBool bBlocking
)
{
    LwU32 val;

    val = REG_RD32(CSB, LW_CPWR_PMU_RAM_TARGET);
    val = FLD_SET_DRF(_CPWR_PMU, _RAM_TARGET, _GR, _SD, val);
    REG_WR32(CSB, LW_CPWR_PMU_RAM_TARGET, val);

    if (bBlocking)
    {
        if (!PMU_REG32_POLL_US(LW_CPWR_PMU_RAM_STATUS,
                            DRF_SHIFTMASK(LW_CPWR_PMU_RAM_STATUS_GR),
                            DRF_DEF(_CPWR, _PMU_RAM_STATUS, _GR, _SD),
                            GR_UNIFIED_SEQ_POLL_TIME_US_TU10X,
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
s_grPgDisengageSram_TU10X(void)
{
    LwU32 val;

    val = REG_RD32(CSB, LW_CPWR_PMU_RAM_TARGET);
    val = FLD_SET_DRF(_CPWR_PMU, _RAM_TARGET, _GR, _POWER_ON, val);
    REG_WR32(CSB, LW_CPWR_PMU_RAM_TARGET, val);

    if (!PMU_REG32_POLL_US(LW_CPWR_PMU_RAM_STATUS,
                        DRF_SHIFTMASK(LW_CPWR_PMU_RAM_STATUS_GR),
                        DRF_DEF(_CPWR, _PMU_RAM_STATUS, _GR, _POWER_ON),
                        GR_UNIFIED_SEQ_POLL_TIME_US_TU10X,
                        PMU_REG_POLL_MODE_CSB_EQ))
    {
        PMU_HALT();
    }
}

/*!
 * @brief Power-gate GR engine's LOGIC.
 *
 * @param[in] bBlocking     If its a blocking call
 */
void
s_grPgEngageLogic_TU10X
(
    LwBool bBlocking
)
{
    LwU32 val;

    val = REG_RD32(CSB, LW_CPWR_PMU_LOGIC_TARGET);
    val = FLD_SET_DRF(_CPWR_PMU, _LOGIC_TARGET, _GR, _PG, val);
    REG_WR32(CSB, LW_CPWR_PMU_LOGIC_TARGET, val);

    if (bBlocking)
    {
        if (!PMU_REG32_POLL_US(LW_CPWR_PMU_LOGIC_STATUS,
                            DRF_SHIFTMASK(LW_CPWR_PMU_LOGIC_STATUS_GR),
                            DRF_DEF(_CPWR, _PMU_LOGIC_STATUS, _GR, _PG),
                            GR_UNIFIED_SEQ_POLL_TIME_US_TU10X,
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
s_grPgDisengageLogic_TU10X(void)
{
    LwU32 val;

    val = REG_RD32(CSB, LW_CPWR_PMU_LOGIC_TARGET);
    val = FLD_SET_DRF(_CPWR_PMU, _LOGIC_TARGET, _GR, _POWER_ON, val);
    REG_WR32(CSB, LW_CPWR_PMU_LOGIC_TARGET, val);

    if (!PMU_REG32_POLL_US(LW_CPWR_PMU_LOGIC_STATUS,
                        DRF_SHIFTMASK(LW_CPWR_PMU_LOGIC_STATUS_GR),
                        DRF_DEF(_CPWR, _PMU_LOGIC_STATUS, _GR, _POWER_ON),
                        GR_UNIFIED_SEQ_POLL_TIME_US_TU10X,
                        PMU_REG_POLL_MODE_CSB_EQ))
    {
        PMU_HALT();
    }
}
