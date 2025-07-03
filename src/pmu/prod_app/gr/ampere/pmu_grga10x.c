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
 * @file   pmu_grga10x.c
 * @brief  HAL-interface for the GA10X Graphics Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_graphics_nobundle.h"
#include "dev_graphics_nobundle_addendum.h"
#if (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
#include "ram_profile_init_seq.h"
#endif // (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
#include "dev_pwr_csb.h"
#include "dev_smcarb.h"
#include "dev_pwr_csb.h"
#include "dev_fb.h"
#include "mscg_wl_bl_init.h"
#include "dev_sec_pri.h"
#include "dev_gsp.h"
#include "dev_runlist.h"
#include "dev_chiplet_pwr.h"
#include "dev_pri_ringstation_gpc.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objgr.h"
#include "pmu_objlpwr.h"
#include "pmu_objpmu.h"
#include "pmu_bar0.h"
#include "pmu_objic.h"
#include "pmu_objfifo.h"
#include "config/g_gr_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * @breif Timeout for unbind operation
 */
#define FIFO_UNBIND_TIMEOUT_US_GA10X                 1000

/*!
 * Max Poll time can be 80 us according to callwlations i.e.
 * (NUM_ACTIVE_GROUPS * 8(zones present in every partition) * (ZONE_DELAY) * PMU_Utilsclk_cycle)
 * (64 * 8 * 15(max as it is 4 bit value) * (9.8ns)
 */
#define GR_UNIFIED_SEQ_POLL_TIME_US_GA10X   80

/* ------------------------ Private Prototypes ----------------------------- */
static void        s_grPgAbort_GA10X(LwU32 abortReason, LwU32 abortStage);
static void        s_grPgEngageSram_GA10X(LwBool bBlocking);
static void        s_grPgDisengageSram_GA10X(void);
static FLCN_STATUS s_grFifoPreemption_GA10X(LwU8 ctrlId);
static LwBool      s_grPrivBlockerEngage_GA10X(LwU32 *pAbortReason, LwU8 ctrlId)
#if UPROC_RISCV
                   GCC_ATTRIB_SECTION("imem_lpwrResident", "s_grPrivBlockerEngage_GA10X")
#else
                   GCC_ATTRIB_SECTION("imem_resident", "s_grPrivBlockerEngage_GA10X")
#endif
                   GCC_ATTRIB_NOINLINE();
static void        s_grPrivBlockerDisengage_GA10X(LwU8 ctrlId);
static void        s_grAutoIncrBufferReset_GA10X(void);
static FLCN_STATUS s_grRgGlobalStateSave_GA10X(LwU8 ctrlId);
static void        s_grRgGlobalStateRestore_GA10X(LwU8 ctrlId);
static void        s_grRgPerfSeqEntry_GA10X(void);
static void        s_grRgPerfSeqExit_GA10X(void);
static void        s_grRgPerfSeqPostExit_GA10X(void);
static void        s_grRgAbort_GA10X(LwU32 abortReason, LwU32 abortStage);

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Initializes holdoff mask for GR-RG.
 */
void
grRgInitSetHoldoffMask_GA10X(void)
{
    OBJPGSTATE *pGrRgState  = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_RG);
    LwU32       holdoffMask = 0;

    if (FIFO_IS_ENGINE_PRESENT(GR))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_GR));
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
    if (FIFO_IS_ENGINE_PRESENT(SEC2))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_SEC2));
    }

    pGrRgState->holdoffMask = holdoffMask;
}

/*!
 * @brief Initialize the data structure to manage GR register cache
 *
 * We are doing the following here -
 *  - Populate the different register lists with corresponding register info
 *  - Callwlate count of each type of register list
 *  - Callwlate total no. of registers to be saved/restored
 *  - Allocate memory for save/restore register buffer
 *  - Allocate memory for auto increment control register buffer
 *  - Update the total count of registers to be saved/restored
 *  - Populate the register cache array with all the register addresses
 *  - Update the count of auto increment control registers
 *  - Populate the auto increment control register buffer
 *
 * Once the above is done, we get the following -
 *  1. Save/restore register buffer -> This will be used to save and restore the
 *                                     graphics registers using addr/data pair
 *  2. Auto increment control register buffer -> This will be used to reset the control address
*                                               for each auto increment register to 0
 */
FLCN_STATUS
grGlobalStateInitPmu_GA10X(void)
{
    GR_SEQ_CACHE  *pGrSeqCache            = GR_SEQ_GET_CACHE();
    LwU8           ovlIdx                 = OVL_INDEX_DMEM(lpwr);
    LwU32          grRegMiscListCount     = 0;
    LwU32          grRegIndexListCount    = 0;
    LwU32          grRegAutoIncrListCount = 0;
    LwU32          grTotalRegCount        = 0;
    LwU8           regIdx                 = 0;
    LwU8           idx                    = 0;
    LwU8           innerIdx               = 0;

    // List of normal GR registers to be saved/restored
    LwU32 grRegMiscList[] =
    {
#if !(PMU_PROFILE_AD10B_RISCV || PMU_PROFILE_GA10F_RISCV)
        //
        // ARANJAN-TODO: fork out different behavior for GA10B, GA10F, GA11B and AD10B here.
        // We need a HAL for _GA10B/GA11B, where these exist, and a HAL for _GA10F/AD10B, where these don't exist.
        //
        LW_PCHIPLET_PWR_GPCS_WEIGHT_6,
        LW_PCHIPLET_PWR_GPCS_WEIGHT_7,
        LW_PCHIPLET_PWR_GPCS_CONFIG_1,
        LW_PCHIPLET_PWR_GPCS_CONFIG_2,
#endif // !(PMU_PROFILE_AD10B_RISCV || PMU_PROFILE_GA10F_RISCV)
        LW_PGRAPH_INTR_CTRL,
        LW_PGRAPH_INTR_RETRIGGER,
        LW_PGRAPH_INTR_NOTIFY_CTRL,
        LW_PGRAPH_FE_DEBUG_INTR,
        LW_PGRAPH_PIPE_BUNDLE_ADDRESS,
        LW_PGRAPH_PIPE_BUNDLE_DATA,
        LW_PGRAPH_PIPE_BUNDLE_DATA_HI,
        LW_PGRAPH_PIPE_BUNDLE_CONFIG,
        LW_PGRAPH_PRI_FE_DEBUG_METHOD_0,
        LW_PGRAPH_PRI_FE_DEBUG_METHOD_1,
        LW_PGRAPH_PRI_MME_SHADOW_RAM_INDEX,
        LW_PGRAPH_PRI_MME_SHADOW_RAM_DATA,
        LW_PGRAPH_PRI_MME_CONFIG,
        LW_PGRAPH_PRI_MME_DEBUG,
        LW_PGRAPH_PRI_MME_INSTR_INS,
        LW_PGRAPH_PRI_MME_INSTR_INS2,
        LW_PGRAPH_PRI_MME_INSTR_RAM_GFX_VEID_CONFIG,
        LW_PGRAPH_PRI_MME_FE1_CONFIG,
        LW_PGRAPH_PRI_MME_FE1_DEBUG,
        LW_PGRAPH_PRI_MME_FE1_INSTR_INS,
        LW_PGRAPH_PRI_MME_FE1_INSTR_INS2,
        LW_PGRAPH_PRI_MME_FE1_INSTR_RAM_GFX_VEID_CONFIG,
        LW_PGRAPH_PRI_PD_OUTPUT_BATCH_STALL,
        LW_PGRAPH_PRI_GPCS_TPCS_PE_BLKCG_CG,
        LW_PGRAPH_PRI_GPCS_TPCS_PE_BLK_ACTIVITY_WEIGHTS_A,
        LW_PGRAPH_PRI_GPCS_TPCS_PE_BLK_ACTIVITY_WEIGHTS_B,
        LW_PGRAPH_PRI_GPCS_TPCS_PE_BLK_ACTIVITY_WEIGHTS_C,
        LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL,
        LW_PGRAPH_PRI_GPCS_TPCS_SM_POWER_HIPWR_THROTTLE,
        LW_PGRAPH_PRI_GPCS_PPCS_PES_BLKCG_CG,
        LW_PGRAPH_PRI_GPCS_PPCS_PES_BLK_ACTIVITY_WEIGHTS_A,
        LW_PGRAPH_PRI_GPCS_PPCS_PES_BLK_ACTIVITY_WEIGHTS_B,
        LW_PGRAPH_PRI_GPCS_PPCS_PES_BLK_ACTIVITY_WEIGHTS_C,
        LW_PGRAPH_PRI_GPCS_PPCS_PES_BLK_ACTIVITY_WEIGHTS_D,
        LW_PGRAPH_PRI_GPCS_PPCS_PES_BLK_ACTIVITY_WEIGHTS_E,
        LW_PGRAPH_PRI_GPCS_ROPS_ZROP_BLK_ACTIVITY_A,
        LW_PGRAPH_PRI_GPCS_ROPS_ZROP_BLK_ACTIVITY_B,
        LW_PGRAPH_PRI_GPCS_ROPS_ZROP_BLK_ACTIVITY_C,
        LW_PGRAPH_PRI_GPCS_ROPS_ZROP_BLK_ACTIVITY_D,
        LW_PGRAPH_PRI_GPCS_ROPS_CROP_BLK_ACT_A,
        LW_PGRAPH_PRI_GPCS_ROPS_CROP_BLK_ACT_B,
        LW_PGRAPH_PRI_GPCS_ROPS_CROP_BLK_ACT_C,
        LW_PGRAPH_PRI_GPCS_ROPS_CROP_BLK_ACT_D,
        LW_PGRAPH_PRI_GPCS_ROPS_CROP_BLK_ACT_E,
        LW_PGRAPH_PRI_GPCS_ROPS_CROP_BLK_ACT_F,
        LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_CG2,
    };

    // List of indexed GR registers to be saved/restored
    GR_REG_INDEX grRegIndexList[] =
    {
        //
        // Base address is taken as 'index = 0'
        // Offset is callwlated using 'address(1st index) - address(0th index)'
        //
        {
            .baseAddr = LW_PGRAPH_PRI_MME_INSTR_RAM_COMPUTE_VEID_CONFIG(0),
            .offset   = LW_PGRAPH_PRI_MME_INSTR_RAM_COMPUTE_VEID_CONFIG(1) - LW_PGRAPH_PRI_MME_INSTR_RAM_COMPUTE_VEID_CONFIG(0),
            .count    = LW_PGRAPH_PRI_MME_INSTR_RAM_COMPUTE_VEID_CONFIG__SIZE_1,
        },
        {
            .baseAddr = LW_PGRAPH_PRI_MME_FE1_INSTR_RAM_COMPUTE_VEID_CONFIG(0),
            .offset   = LW_PGRAPH_PRI_MME_FE1_INSTR_RAM_COMPUTE_VEID_CONFIG(1) - LW_PGRAPH_PRI_MME_FE1_INSTR_RAM_COMPUTE_VEID_CONFIG(0),
            .count    = LW_PGRAPH_PRI_MME_FE1_INSTR_RAM_COMPUTE_VEID_CONFIG__SIZE_1,
        },
    };

    // List of auto increment GR registers to be saved/restored
    GR_REG_AUTO_INCR grRegAutoIncrList[] =
    {
        // Count is callwlated using 'INDEX_MAX + 1' as index starts from 0
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
        {
            .ctrlAddr = LW_PGRAPH_PRI_GPCS_TPCS_SM_POWER_DIDT_WEIGHTS_CTL,
            .dataAddr = LW_PGRAPH_PRI_GPCS_TPCS_SM_POWER_DIDT_WEIGHTS_DATA,
            .count    = LW_PGRAPH_PRI_GPCS_TPCS_SM_POWER_DIDT_WEIGHTS_CTL_INDEX_MAX + 1,
        },
    };

    // Callwlate the no. of entries in each list
    grRegMiscListCount     = sizeof(grRegMiscList) / sizeof(LwU32);
    grRegIndexListCount    = sizeof(grRegIndexList) / sizeof(GR_REG_INDEX);
    grRegAutoIncrListCount = sizeof(grRegAutoIncrList) / sizeof(GR_REG_AUTO_INCR);

    //
    // Callwlate total no. of registers to be saved/restored by adding
    // register counts from each list
    //
    grTotalRegCount += grRegMiscListCount;
    for (idx = 0; idx < grRegIndexListCount; idx++)
    {
        grTotalRegCount += (grRegIndexList[idx].count);
    }
    for (idx = 0; idx < grRegAutoIncrListCount; idx++)
    {
        grTotalRegCount += (grRegAutoIncrList[idx].count);
    }

    // Allocate memory for register buffer
    pGrSeqCache->pGlobalStateBuffer =
        lwosCallocType(ovlIdx, grTotalRegCount, GR_REG);
    if (pGrSeqCache->pGlobalStateBuffer == NULL)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_NO_FREE_MEM;
    }

    // Allocate memory for auto increment control register array
    pGrSeqCache->pAutoIncrBuffer =
        lwosCallocType(ovlIdx, grRegAutoIncrListCount, LwU32);
    if (pGrSeqCache->pAutoIncrBuffer == NULL)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_NO_FREE_MEM;
    }

    // Update the total count of registers to be saved/restored in seq buffer
    pGrSeqCache->globalStateRegCount = grTotalRegCount;

    // Populate register buffer with normal registers address
    for (idx = 0; idx < grRegMiscListCount; idx++)
    {
        pGrSeqCache->pGlobalStateBuffer[regIdx].addr = grRegMiscList[idx];
        regIdx++;
    }
    // Populate register buffer with index registers address
    for (idx = 0; idx < grRegIndexListCount; idx++)
    {
        for (innerIdx = 0; innerIdx < grRegIndexList[idx].count; innerIdx++)
        {
            pGrSeqCache->pGlobalStateBuffer[regIdx].addr =
                (grRegIndexList[idx].baseAddr + (innerIdx * grRegIndexList[idx].offset));
            regIdx++;
        }
    }
    // Populate register buffer with auto increment registers address
    for (idx = 0; idx < grRegAutoIncrListCount; idx++)
    {
        for (innerIdx = 0; innerIdx < grRegAutoIncrList[idx].count; innerIdx++)
        {
            pGrSeqCache->pGlobalStateBuffer[regIdx].addr = grRegAutoIncrList[idx].dataAddr;
            regIdx++;
        }
    }

    // Update the count of auto increment control registers
    pGrSeqCache->autoIncrBufferRegCount = grRegAutoIncrListCount;

    // Populate auto increment control register buffer
    for (idx = 0; idx < grRegAutoIncrListCount; idx++)
    {
        pGrSeqCache->pAutoIncrBuffer[idx] = grRegAutoIncrList[idx].ctrlAddr;
    }

    return FLCN_OK;
}

/*!
 * @brief  Set SM Routing mode for Ampere and later.
 *
 * @param[in]  gpcCounter           GPC Counter
 * @param[in]  tpcCounter           TPC Counter
 * @param[in]  bIsBroadcast         Whether to update unicast or broadcast routing register
 * @param[in]  routingSel           To determine the mode for SM routing register.
 *
 * @return FLCN_STATUS FLCN_OK on sucess, else FLCN_ERR_ILWALID_ARGUMENT.
 */
FLCN_STATUS
grSmRoutingModeSet_GA10X
(
    LwU32                   gpcCounter,
    LwU32                   tpcCounter,
    LwBool                  bIsBroadcast,
    GR_TPC_SM_ROUTING_MODE  routingSel
)
{
    LwU32       regOffset = 0;
    LwU32       regAddr;
    LwU32       regVal;

    if (bIsBroadcast)
    {
        regAddr = LW_PGRAPH_PRI_GPCS_TPCS_TEX_M_ROUTING;
    }
    else
    {
        regOffset = (LW_GPC_PRI_STRIDE * gpcCounter) +
                    (LW_TPC_IN_GPC_STRIDE * tpcCounter);

        regAddr = LW_PGRAPH_PRI_GPC0_TPC0_TEX_M_ROUTING + regOffset;
    }

    regVal = REG_RD32(FECS, regAddr);

    switch (routingSel)
    {
        case GR_TPC_SM_ROUTING_MODE_DEFAULT:
            regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_TEX_M_ROUTING, _SEL, _DEFAULT, regVal);
            break;

        case GR_TPC_SM_ROUTING_MODE_PIPE0:
            regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_TEX_M_ROUTING, _SEL, _PIPE0, regVal);
            break;

        case GR_TPC_SM_ROUTING_MODE_PIPE1:
            regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_TEX_M_ROUTING, _SEL, _PIPE1, regVal);
            break;

        default:
            PMU_BREAKPOINT();
            return FLCN_ERR_ILWALID_ARGUMENT;
    }

     REG_WR32(FECS, regAddr, regVal);

     return FLCN_OK;
}

/*!
 * @brief Issue the SRAM ECC Control scrubbing
 *
 * @return FLCN_STATUS FLCN_OK on success, else returns appropriate FLCN_ERR_*.
 */
FLCN_STATUS
grIssueSramEccScrub_GA10X(void)
{
    LwU32          regVal       = 0;
    LwU32          noSmRegs     = GR_TPC_SM_PIPES_COUNT;
    LwU32          smNum        = 0;
    LwBool         bStopPolling = LW_FALSE;
    FLCN_TIMESTAMP eccScrubStartTimeNs;
    FLCN_STATUS    status;

    //
    // Set SM Routing mode to default (Writes are broadcasted and reads are directed to PIPE0)
    // and set broadcast mode = TRUE since we are writing to a broadcast reg.
    //
    status = grSmRoutingModeSet_HAL(&Gr, 0, 0, LW_TRUE, GR_TPC_SM_ROUTING_MODE_DEFAULT);
    if (status != FLCN_OK)
    {
        return status;
    }

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
    REG_WR32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL, regVal);

    // L1_DATA_ECC
    regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_L1_DATA_ECC_CONTROL);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_L1_DATA_ECC_CONTROL,
                         _SCRUB_EL1_0, _TASK, regVal);
    REG_WR32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_L1_DATA_ECC_CONTROL, regVal);

    // L1_TAG_ECC
    regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_L1_TAG_ECC_CONTROL);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_L1_TAG_ECC_CONTROL,
                         _SCRUB_EL1_0, _TASK, regVal);
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
                         _SCRUB_BARRIER_SM0, _TASK, regVal);
    REG_WR32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_CBU_ECC_CONTROL, regVal);

    //
    // ICACHE_ECC_CONTROL
    //
    // With SM ICACHE fields restructuring on Ampere+:
    //
    // L0 Data and L0 PreDecode moved to per SM register named LW_PGRAPH_PRI_GPCS_TPCS_SM_RAMS_ECC_CONTROL
    // L1 PreDecode is deleted.
    // L1 Data remains in TPC level register LW_PGRAPH_PRI_GPCS_TPCS_SM_ICACHE_ECC_CONTROL
    //

    // ICACHE ECC CONTROL - L0 Data, L0 PreDecode
    regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_RAMS_ECC_CONTROL);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_RAMS_ECC_CONTROL,
                         _SCRUB_L0IC_DATA, _TASK, regVal);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_RAMS_ECC_CONTROL,
                         _SCRUB_L0IC_PREDECODE, _TASK, regVal);
    REG_WR32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_RAMS_ECC_CONTROL, regVal);

    // ICACHE ECC CONTROL - L1 Data
    regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_ICACHE_ECC_CONTROL);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_ICACHE_ECC_CONTROL,
                         _SCRUB_L1_DATA, _TASK, regVal);
    REG_WR32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_ICACHE_ECC_CONTROL, regVal);

    // Start time for Scrub polling
    osPTimerTimeNsLwrrentGet(&eccScrubStartTimeNs);

    // Poll for Scrubbing to finish
    do
    {
        bStopPolling = LW_TRUE;

        for (smNum = 0; smNum < noSmRegs; smNum++)
        {
            // Set Routing mode
            status = grSmRoutingModeSet_HAL(&Gr, 0, 0, LW_TRUE, GR_TPC_SM_ROUTING_MODE_PIPE0 + smNum);
            if (status != FLCN_OK)
            {
                return status;
            }

            // SM_LRF_ECC
            regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL);
            bStopPolling = bStopPolling &&
                           (FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL,
                                         _SCRUB_QRFDP0, _INIT, regVal)           &&
                            FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL,
                                         _SCRUB_QRFDP1, _INIT, regVal)           &&
                            FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL,
                                         _SCRUB_QRFDP2, _INIT, regVal)           &&
                            FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL,
                                         _SCRUB_QRFDP3, _INIT, regVal));

            // L1 DATA ECC
            regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_L1_DATA_ECC_CONTROL);
            bStopPolling = bStopPolling &&
                           (FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_L1_DATA_ECC_CONTROL,
                                         _SCRUB_EL1_0, _INIT, regVal));

            // L1 TAG ECC
            regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_L1_TAG_ECC_CONTROL);
            bStopPolling = bStopPolling &&
                           (FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_L1_TAG_ECC_CONTROL,
                                         _SCRUB_EL1_0, _INIT, regVal)             &&
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
                                         _SCRUB_BARRIER_SM0, _INIT, regVal));

            //
            // With SM ICACHE fields restructuring on Ampere+:
            //
            // L0 Data and L0 PreDecode moved to per SM register named LW_PGRAPH_PRI_GPCS_TPCS_SM_RAMS_ECC_CONTROL
            // L1 PreDecode is deleted.
            // L1 Data remains in TPC level register LW_PGRAPH_PRI_GPCS_TPCS_SM_ICACHE_ECC_CONTROL
            //

            // ICACHE ECC CONTROL - L0 Data, L0 PreDecode
            regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_RAMS_ECC_CONTROL);
            bStopPolling = bStopPolling &&
                           (FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_RAMS_ECC_CONTROL,
                                         _SCRUB_L0IC_DATA, _INIT, regVal)         &&
                            FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_RAMS_ECC_CONTROL,
                                         _SCRUB_L0IC_PREDECODE, _INIT, regVal));
        }

        //
        // ICACHE ECC CONTROL - L1 Data.
        // This is a TPC level register and not a per SM level register.
        // Hence, this doesn't need SM Routing register to be programmed.
        //
        regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_SM_ICACHE_ECC_CONTROL);
        bStopPolling = bStopPolling &&
                       (FLD_TEST_DRF(_PGRAPH, _PRI_GPCS_TPCS_SM_ICACHE_ECC_CONTROL,
                                     _SCRUB_L1_DATA, _INIT, regVal));

        // Check for timeout for ECC Scrubbing
        if (osPTimerTimeNsElapsedGet(&eccScrubStartTimeNs) >=
            GR_SRAM_ECC_SCRUB_TIMEOUT_NS)
        {
            PMU_BREAKPOINT();
            return FLCN_ERR_TIMEOUT;
        }
    } while (!bStopPolling);

    return FLCN_OK;
}

/*!
 * @brief Interface to enable/disable SMCARB free running timestamp
 *
 * SMCARB sends periodic data to CXBAR_GXC unit which is going to go
 * into reset/shutdown. So we need to disable/enable it during GPC-RG
 *
 * @param[in] bDisable   boolean to specify enablement or disablement
 */
void
grSmcarbTimestampDisable_GA10X(LwBool bDisable)
{
    LwU32 regVal;

    if (bDisable)
    {
        // Read and cache the smcarb timestamp value
        Gr.pSeqCache->smcarbTimestamp = regVal = REG_RD32(FECS, LW_PSMCARB_TIMESTAMP_CTRL);

        // Disable free running timestamp
        regVal = FLD_SET_DRF(_PSMCARB, _TIMESTAMP_CTRL, _DISABLE_TICK, _TRUE, regVal);

        // Write the disabled value
        REG_WR32(FECS, LW_PSMCARB_TIMESTAMP_CTRL, regVal);
    }
    else
    {
        // Restore the cached value
        REG_WR32(FECS, LW_PSMCARB_TIMESTAMP_CTRL, Gr.pSeqCache->smcarbTimestamp);
    }

    // Force read to flush the write
    REG_RD32(FECS, LW_PSMCARB_TIMESTAMP_CTRL);
}

/*!
 * @brief Interface to disable/enable utils clock CG
 *
 * @param[in] bDisable   boolean to specify disable or enable CG
 */
void
grUtilsCgDisable_GA10X(LwBool bDisable)
{
    LwU32 regVal;

    if (bDisable)
    {
        // Read and cache the utils clock CG settings
        Gr.pSeqCache->utilsClockCG = regVal = REG_RD32(CSB, LW_CPWR_PMU_ELPG_SEQ_CG);

        // Disable utils clock CG
        regVal = FLD_SET_DRF(_CPWR_PMU, _ELPG_SEQ_CG, _EN, _DISABLED, regVal);

        // Write back the disabled settings
        REG_WR32_STALL(CSB, LW_CPWR_PMU_ELPG_SEQ_CG, regVal);
    }
    else
    {
        // Restore the cached settings
        REG_WR32_STALL(CSB, LW_CPWR_PMU_ELPG_SEQ_CG, Gr.pSeqCache->utilsClockCG);
    }
}

/*!
 * @brief Interface to enable/disable GR MMU unbind
 *
 * For Unbind, we need to issue the Unbind and wait on the status
 *             to make sure Unbind is complete
 * For Rebind, we only issue the request to disable MMU unbind.
 *             The actual rebind happens at the next context load
 *
 * @param[in] bUnbind   boolean to specify enable/disable unbind
 */
FLCN_STATUS
grUnbind_GA10X(LwU8 ctrlId, LwBool bUnbind)
{
    LwU32 regVal;

    if (bUnbind)
    {
        // Trigger MMU unbind
        regVal = REG_RD32(BAR0, LW_PFB_PRI_MMU_POWERGATE_CFG);
        regVal = FLD_SET_DRF(_PFB, _PRI_MMU_POWERGATE_CFG, _GPCMMU, _DISABLED, regVal);
        REG_WR32(BAR0, LW_PFB_PRI_MMU_POWERGATE_CFG, regVal);

        // Poll for completion of unbind request
        if (!PMU_REG32_POLL_US(
             LW_PFB_PRI_MMU_POWERGATE_CFG,
             DRF_SHIFTMASK(LW_PFB_PRI_MMU_POWERGATE_CFG_GPCMMU_STATUS),
             DRF_DEF(_PFB, _PRI_MMU_POWERGATE_CFG, _GPCMMU_STATUS, _DISABLED),
             FIFO_UNBIND_TIMEOUT_US_GA10X, PMU_REG_POLL_MODE_BAR0_EQ))
        {
            PMU_BREAKPOINT();
            return FLCN_ERR_TIMEOUT;
        }
    }
    else
    {
        // Disable MMU unbind
        regVal = REG_RD32(BAR0, LW_PFB_PRI_MMU_POWERGATE_CFG);
        regVal = FLD_SET_DRF(_PFB, _PRI_MMU_POWERGATE_CFG, _GPCMMU, _ENABLED, regVal);
        REG_WR32(BAR0, LW_PFB_PRI_MMU_POWERGATE_CFG, regVal);
    }

    return FLCN_OK;
}

/*!
 * @brief Returns if the current context is invalid
 *
 * @return LW_TRUE  If current context is invalid
 *         LW_FALSE otherwise
 */
LwBool
grIsCtxIlwalid_GA10X(void)
{
    LwU32 runlistPriBase = GET_ENG_RUNLIST_PRI_BASE(PMU_ENGINE_GR);
    LwU16 runlistEngId   = GET_ENG_RUNLIST_ENG_ID(PMU_ENGINE_GR);
    LwU32 regVal         = 0;

    regVal = REG_RD32(BAR0, runlistPriBase + LW_RUNLIST_ENGINE_STATUS0(runlistEngId));

    return FLD_TEST_DRF(_RUNLIST, _ENGINE_STATUS0, _CTX_STATUS, _ILWALID, regVal);
}

/*!
 * @brief GPC_RG entry sequence
 *
 * Following are the sequence of steps -
 * Check if buffers are ready, else ABORT
 * Check GR Idleness, else ABORT
 * Grab GP-RG semaphore, ABORT if not available
 * Disable GR-ELCG
 * Disable SMCARB free-running timestamp
 * Engage priv Blockers, else ABORT
 *   -Engage blockers in BLOCK_ALL mode
 *   -Flush priv path
 *   -Engage blockers in BLOCK_EQ mode
 * Attach libFifo overlay
 * Check GR Idleness, else ABORT
 * Execute preemption sequence, else ABORT
 * Enable holdoff, else ABORT
 * Check if the context is still INVALID
 * Check GR Idleness, else ABORT
 * Save global GR state via FECS
 * Trigger MMU Unbind, else ABORT
 * Check GR Idleness, else ABORT
 * Detach libFifo overlay
 * Perf Sequencer entry step
 *   -De-init GPC clock
 *   -Enable PRI error mechanism for GPC access
 *   -Block propagation of reset for external units
 *   -Assert full GPC Reset
 *   -Assert GPC rail-gating clamp
 *   -Gate LWVDD rail
 *
 * @returns FLCN_OK    when entry successfully.
 * @returns FLCN_ERROR otherwise
 */
FLCN_STATUS
grGrRgEntry_GA10X(void)
{
    OBJPGSTATE     *pGrState    = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_RG);
    LwU32           abortReason = 0;
    FLCN_STATUS     status      = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, clkLibClk3)
        CHANGE_SEQ_OVERLAYS_LPWR_SCRIPTS
    };

    // Attach required overlays
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Check GR Idleness
        if (!grIsIdle_HAL(&Gr, &abortReason, RM_PMU_LPWR_CTRL_ID_GR_RG))
        {
            s_grRgAbort_GA10X(abortReason,
                             GR_ABORT_CHKPT_AFTER_PG_ON);

            status = FLCN_ERROR;
            goto grGrRgEntry_GA10X_exit;
        }

        //
        // Disable the clocks accesses for GPC clock to make sure we don't end
        // up issuing register accesses in the rail gated domain while GPC-RG is
        // engaged. A non-okay status here indicates that we weren't able to disable
        // the access as there's a GPC clock read / write op in progress - we need to
        // abort the entry in such a case.
        //
        if (PMUCFG_FEATURE_ENABLED(PMU_CLK_LPWR))
        {
            if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, PRIV_RING) ||
                LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, RESET))
            {
                if (FLCN_OK != clkFreqDomainsDisable(LW2080_CTRL_CLK_DOMAIN_GPCCLK,      // clkDomainMask
                                                     LW2080_CTRL_CLK_CLIENT_ID_GR_RG,    // clientId
                                                     LW_TRUE,                            // bDisable
                                                     LW_TRUE))                           // bConditional
                {
                    s_grRgAbort_GA10X(GR_ABORT_REASON_DISABLE_CLK_ACCESS_ERROR,
                                      GR_ABORT_CHKPT_AFTER_PG_ON);
                    status = FLCN_ERROR;
                    goto grGrRgEntry_GA10X_exit;
                }
            }
        }

        //
        // Grab shared GR-RG semaphore with VF Switch. If semaphore is not
        // available than we have an in-progess VF switch, abort.
        //
        if (!grRgSemaphoreTake())
        {
            s_grRgAbort_GA10X(GR_ABORT_REASON_MUTEX_ACQUIRE_ERROR,
                              GR_ABORT_CHKPT_AFTER_DISABLE_CLK_ACCESS);

            status = FLCN_ERR_MUTEX_ACQUIRED;
            goto grGrRgEntry_GA10X_exit;
        }

        // Disable GR-ELCG
        lpwrCgElcgDisable(PMU_ENGINE_GR, LPWR_CG_ELCG_CTRL_REASON_GR_RG);

        // Disable the SMCARCB free-running timestamp
        grSmcarbTimestampDisable_HAL(&Gr, LW_TRUE);

        // Engage Priv Blockers
        if (!s_grPrivBlockerEngage_GA10X(&abortReason, RM_PMU_LPWR_CTRL_ID_GR_RG))
        {
            s_grRgAbort_GA10X(abortReason,
                             GR_ABORT_CHKPT_AFTER_PRIV_BLOCKER);

            status = FLCN_ERROR;
            goto grGrRgEntry_GA10X_exit;
        }

        // Attach libFifo overlay
        OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libFifo));

        // Check GR Idleness
        if (!grIsIdle_HAL(&Gr, &abortReason, RM_PMU_LPWR_CTRL_ID_GR_RG))
        {
            s_grRgAbort_GA10X(abortReason,
                             GR_ABORT_CHKPT_AFTER_OVERLAY_LOAD);

            status = FLCN_ERROR;
            goto grGrRgEntry_GA10X_exit;
        }

        //
        // Execute preemption sequence.
        // As part of preemption sequence host saves
        // channel and engine context.
        //
        if (s_grFifoPreemption_GA10X(RM_PMU_LPWR_CTRL_ID_GR_RG) != FLCN_OK)
        {
            s_grRgAbort_GA10X(GR_ABORT_REASON_FIFO_PREEMPT_ERROR,
                             GR_ABORT_CHKPT_AFTER_PREEMPT_SEQUENCE);

            status = FLCN_ERROR;
            goto grGrRgEntry_GA10X_exit;
        }

        // Enable holdoff
        if (FLCN_OK != lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_GR_RG,
                                                     pGrState->holdoffMask))
        {
            s_grRgAbort_GA10X(GR_ABORT_REASON_HOLDOFF_ENABLE_TIMEOUT,
                             GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_0);

            status = FLCN_ERROR;
            goto grGrRgEntry_GA10X_exit;
        }

        // Check if the context is still INVALID
        if (!grIsCtxIlwalid_HAL(&Gr))
        {
            s_grRgAbort_GA10X(GR_ABORT_REASON_FIFO_PREEMPT_ERROR,
                            GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_0);

            status = FLCN_ERROR;
            goto grGrRgEntry_GA10X_exit;
        }

        // Check GR Idleness
        if (!grIsIdle_HAL(&Gr, &abortReason, RM_PMU_LPWR_CTRL_ID_GR_RG))
        {
            s_grRgAbort_GA10X(abortReason,
                             GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_0);

            status = FLCN_ERROR;
            goto grGrRgEntry_GA10X_exit;
        }

        if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, SAVE_GLOBAL_STATE))
        {
            // Save global GR state via FECS and PMU
            if (FLCN_OK != s_grRgGlobalStateSave_GA10X(RM_PMU_LPWR_CTRL_ID_GR_RG))
            {
                s_grRgAbort_GA10X(GR_ABORT_REASON_REGLIST_SAVE_TIMEOUT,
                                 GR_ABORT_CHKPT_AFTER_SAVE_REGLIST_REQUEST);
            }
        }

        if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, UNBIND))
        {
            // Trigger GR MMU Unbind
            if (FLCN_OK != grUnbind_HAL(&Gr, RM_PMU_LPWR_CTRL_ID_GR_RG, LW_TRUE))
            {
                s_grRgAbort_GA10X(GR_ABORT_REASON_UNBIND,
                                 GR_ABORT_CHKPT_AFTER_MMU_UNBIND);

                status = FLCN_ERROR;
                goto grGrRgEntry_GA10X_exit;
            }
        }

        // Check GR Idleness
        if (!grIsIdle_HAL(&Gr, &abortReason, RM_PMU_LPWR_CTRL_ID_GR_RG))
        {
            s_grRgAbort_GA10X(abortReason,
                             GR_ABORT_CHKPT_AFTER_MMU_UNBIND);

            status = FLCN_ERROR;
            goto grGrRgEntry_GA10X_exit;
        }

        // Detach libFifo overlay
        OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libFifo));

        // Perf sequencer entry step
        s_grRgPerfSeqEntry_GA10X();

grGrRgEntry_GA10X_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief GPC_RG exit sequence
 *
 * Following are the sequence of steps -
 * Perf sequencer exit step
 *   -Ungate LWVDD rail
 *   -Disable utils clock CG
 *   -Assert engine & context reset for BECS and FECS
 *   -Unblock propagation of reset for external units
 *   -De-assert GPC rail-gating clamp
 *   -Poll for RAM repair completion
 *   -De-assert engine & context reset for BECS and FECS
 *   -De-assert full GPC reset
 *   -Disable PRI error mechanism for GPC access
 *   -Program SMC's PRI GPCCS_ENGINE_RESET_CTL
 *   -Re-enable utils clock CG
 *   -Init GPC clock
 * Perform GPCCS bootstrapping
 * Disable MMU Unbind
 * Restore global GR state via FECS
 * Restore BA state
 * Disengage priv blockers
 * Disable engine holdoff
 * Re-enable SMCARB free-running timestamp
 * Re-enable GR-ELCG
 * Perf sequencer post exit step
 *   -Switch GPCNAFLL to VCO path
 *   -Enable the access to GPC clock
 * Release the semaphore
 *
 * @returns FLCN_OK
 */
FLCN_STATUS
grGrRgExit_GA10X(void)
{
    OBJPGSTATE     *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_RG);
    FLCN_STATUS     status   = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        CHANGE_SEQ_OVERLAYS_LPWR_SCRIPTS
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Perf sequencer exit step
        s_grRgPerfSeqExit_GA10X();

        if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, RG_GPCCS_BS))
        {
            // Perform GPCCS bootstrapping
            grRgGpccsBootstrap_HAL(&Gr);
        }

        if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, UNBIND))
        {
            // Disable MMU unbind
            grUnbind_HAL(&Gr, RM_PMU_LPWR_CTRL_ID_GR_RG, LW_FALSE);
        }

        if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, SAVE_GLOBAL_STATE))
        {
            // Restore global GR state via FECS and PMU
            s_grRgGlobalStateRestore_GA10X(RM_PMU_LPWR_CTRL_ID_GR_RG);
        }

        // Disengage priv blockers
        s_grPrivBlockerDisengage_GA10X(RM_PMU_LPWR_CTRL_ID_GR_RG);

        // Disable engine holdoff
        lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_GR_RG,
                                      LPWR_HOLDOFF_MASK_NONE);

        // Re-enable SMCARB free-running timestamp
        grSmcarbTimestampDisable_HAL(&Gr, LW_FALSE);

        // Re-enable GR-ELCG
        lpwrCgElcgEnable(PMU_ENGINE_GR, LPWR_CG_ELCG_CTRL_REASON_GR_RG);

        // Perf sequencer post exit step
        s_grRgPerfSeqPostExit_GA10X();

        // Release the semaphore
        grRgSemaphoreGive();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief GPC_RG entry pre voltage rail gate LPWR sequence
 * that is shared with PERF GPC_RG entry sequence.
 *
 * @returns FLCN_OK
 */
FLCN_STATUS
grGrRgEntryPreVoltGateLpwr_GA10X(void)
{
    OBJPGSTATE *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_RG);

    //
    // Issue GPC chiplet flush to ensure there are no outstanding
    // writes before we disable priv ring and assert GPC reset
    //
    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, PRIV_RING) ||
        LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, RESET))
    {
        REG_WR32(FECS, LW_PPRIV_GPC_GPCS_PRI_FENCE, 0);
        REG_RD32(FECS, LW_PPRIV_GPC_GPCS_PRI_FENCE);
    }

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, PRIV_RING))
    {
        // Enable PRI error mechanism for GPC access
        grRgPriErrHandlingActivate_HAL(&Gr, LW_TRUE);
    }

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, RESET))
    {
        // Block propagation of reset for external units
        grRgExtResetBlock_HAL(&Gr, LW_TRUE);

        // Assert full GPC Reset
        grRgGpcResetAssert_HAL(&Gr, LW_TRUE);
        OS_PTIMER_SPIN_WAIT_NS(1000);

        //
        // Clampping sequence is coupled with block/unblock the reset.
        // So we are using the RESET Subefeature mask to assert the
        // clamps.
        //
        // Assert GPC rail-gating clamp
        //
        grRgClampAssert_HAL(&Gr, LW_TRUE);
        OS_PTIMER_SPIN_WAIT_NS(1000);
    }

    return FLCN_OK;
}

/*!
 * @brief GPC_RG exit pre voltage rail ungate LPWR sequence
 * that is shared with PERF GPC_RG exit sequence.
 *
 * @returns FLCN_OK
 */
FLCN_STATUS
grGrRgExitPreVoltUngateLpwr_GA10X(void)
{
    return FLCN_OK;
}

/*!
 * @brief GPC_RG exit post voltage rail ungate LPWR sequence
 * that is shared with PERF GPC_RG exit sequence.
 *
 * @returns FLCN_OK
 */
FLCN_STATUS
grGrRgExitPostVoltUngateLpwr_GA10X(void)
{
    OBJPGSTATE *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_RG);

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, RESET))
    {
        // Disable utils clock CG
        grUtilsCgDisable_HAL(&Gr, LW_TRUE);

        // Assert engine & context reset for BECS and FECS
        grRgEngineResetAssert_HAL(&Gr, LW_TRUE);

        // Unblock propagation of reset for external units
        grRgExtResetBlock_HAL(&Gr, LW_FALSE);
        OS_PTIMER_SPIN_WAIT_NS(1000);

        // De-assert GPC rail-gating clamp
        grRgClampAssert_HAL(&Gr, LW_FALSE);
        OS_PTIMER_SPIN_WAIT_NS(1000);

        //
        // RAM Repair is coupled with Clampping and clamping sequence
        // is coupled with block/unblock the reset.
        // So we are using the RESET Subefeature mask to de-assert the
        // clamps and polling for RAM Repair.
        //
        // Poll for RAM repair completion
        //
        grRamRepairCompletionPoll_HAL(&Gr);

        // De-assert engine & context reset for BECS and FECS
        grRgEngineResetAssert_HAL(&Gr, LW_FALSE);

        // De-assert full GPC reset
        grRgGpcResetAssert_HAL(&Gr, LW_FALSE);
        OS_PTIMER_SPIN_WAIT_NS(1000);
    }

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, PRIV_RING))
    {
        // Disable PRI error mechanism for GPC access
        grRgPriErrHandlingActivate_HAL(&Gr, LW_FALSE);
    }

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, RESET))
    {
        // Program SMC's PRI GPCCS_ENGINE_RESET_CTL
        grRgGpcResetControl_HAL(&Gr);

        // Enable utils clock CG
        grUtilsCgDisable_HAL(&Gr, LW_FALSE);
    }

    return FLCN_OK;
}

/*!
 * Override the GR-PG PRIV access Error Handling Support based on PLM
 */
void
grPgPrivErrHandlingSupportOverride_GA10X(void)
{
    OBJPGSTATE *pGrState = NULL;
    LwU32       regVal   = 0;

    //
    // Read the PLM for this register. If PLM is set for Level 3 write access only,
    // then remove the support for PRIV_RING Error handling
    //
    regVal = REG_RD32(CSB, LW_CPWR_PMU_GR_PRI_ERROR_PRIV_LEVEL_MASK);
    if (FLD_TEST_DRF(_CPWR, _PMU_GR_PRI_ERROR_PRIV_LEVEL_MASK, _WRITE_PROTECTION,
                     _LEVEL3_ENABLED_FUSE1, regVal))
    {
        pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);
        // Since Level 3 PLM is set, remove the PRIV_RING Error support
        pGrState->supportMask = LPWR_SF_MASK_CLEAR(GR, PRIV_RING, pGrState->supportMask);
    }
}

/*!
 * Activate GR PRIV access Error Handling
 *
 * @param[in] bActivate    True: Enable the error for priv access,
 *                         False: Disable the error for priv access
 */
void
grPgPrivErrHandlingActivate_GA10X(LwBool bActivate)
{
    LwU32 regVal = 0;

    regVal = REG_RD32(CSB, LW_CPWR_PMU_GR_PRI_ERROR);

    if (bActivate)
    {
        regVal = FLD_SET_DRF(_CPWR_PMU, _GR_PRI_ERROR, _CONFIG, _ENABLE, regVal);
    }
    else
    {
        regVal = FLD_SET_DRF(_CPWR_PMU, _GR_PRI_ERROR, _CONFIG, _DISABLE, regVal);
    }

    REG_WR32_STALL(CSB, LW_CPWR_PMU_GR_PRI_ERROR, regVal);
}

/*!
 * @brief Power-gate GR engine.
 */
void
grHwPgEngage_GA10X(void)
{
    OBJPGSTATE *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, ELPG_SRAM))
    {
        s_grPgEngageSram_GA10X(LW_TRUE);
    }
}

/*!
 * @brief Power-ungate GR engine.
 */
void
grHwPgDisengage_GA10X(void)
{
    OBJPGSTATE *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, ELPG_SRAM))
    {
        s_grPgDisengageSram_GA10X();
    }
}

/*!
 * @brief Save Global state via FECS
 *
 * @return FLCN_STATUS  FLCN_OK    If save reglist is sucessful in 4ms.
 *                      FLCN_ERROR If save reglist is unsucessful in 4ms.
 */
FLCN_STATUS
grGlobalStateSave_GA10X(LwU8 ctrlId)
{
    LwU32       methodData = 0;
    FLCN_STATUS status     = FLCN_OK;

    // Update the method data based on the Lpwr ctrl id
    if (ctrlId == RM_PMU_LPWR_CTRL_ID_GR_PG)
    {
        methodData = LW_PGRAPH_PRI_FECS_METHOD_DATA_V_ELPG_LIST;
    }
    else if (ctrlId == RM_PMU_LPWR_CTRL_ID_GR_RG)
    {
        methodData = LW_PGRAPH_PRI_FECS_METHOD_DATA_V_GPCRG_LIST;
    }

    //
    // Save global state via FECS
    //
    // Method data = 0x1/0x2 (ELPG/GPC-RG register list)
    // Poll value  = 0x1
    //
    status = grSubmitMethod_HAL(&Gr, LW_PGRAPH_PRI_FECS_METHOD_PUSH_ADR_SAVE_REGLIST,
                       methodData, 0x1);

    if (status == FLCN_OK)
    {
        //
        // State save introduces GR traffic. This asserts idle flip. Clear
        // idle flip to avoid this false alarm.
        //
        grClearIdleFlip_HAL(&Gr, ctrlId);
    }

    return status;
}

/*!
 * @brief Restore the global non-ctxsw state via FECS
 */
void
grGlobalStateRestore_GA10X(LwU8 ctrlId)
{
    LwU32       methodData = 0;
    FLCN_STATUS status     = FLCN_OK;

    // Update the method data based on the Lpwr ctrl id
    if (ctrlId == RM_PMU_LPWR_CTRL_ID_GR_PG)
    {
        methodData = LW_PGRAPH_PRI_FECS_METHOD_DATA_V_ELPG_LIST;
    }
    else if (ctrlId == RM_PMU_LPWR_CTRL_ID_GR_RG)
    {
        methodData = LW_PGRAPH_PRI_FECS_METHOD_DATA_V_GPCRG_LIST;
    }

    //
    // Restore global state via FECS
    //
    // Method data = 0x1/0x2 (ELPG/GPC-RG register list)
    // Poll value  = 0x1
    //
    status = grSubmitMethod_HAL(&Gr, LW_PGRAPH_PRI_FECS_METHOD_PUSH_ADR_RESTORE_REGLIST,
                                methodData, 0x1);

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }
}

/*!
 * Save GR's state
 *
 * @returns FLCN_OK    when saved successfully.
 * @returns FLCN_ERROR otherwise
 */
FLCN_STATUS
grPgSave_GA10X(void)
{
    OBJPGSTATE *pGrState    = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);
    LwU32       abortReason = 0;

    // Checkpoint1: Check GR Idleness
    if (!grIsIdle_HAL(&Gr, &abortReason, RM_PMU_LPWR_CTRL_ID_GR_PG))
    {
        s_grPgAbort_GA10X(abortReason,
                          GR_ABORT_CHKPT_AFTER_PG_ON);

        return FLCN_ERROR;
    }

    //
    // TODO-pankumar: This is temp code to unblock ARCH verification.
    // We are working on a proper framework for FG-RPPG
    //
    // Disable the FG-RPPG
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_FG_RPPG) &&
        LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, FG_RPPG_EXCLUSION))
    {
        if (FLCN_OK != lpwrSeqSramFgRppgDisable_HAL(&Lpwr))
        {
            abortReason = GR_ABORT_REASON_FG_RPPG_EXCLUSION_FAILED;
            s_grPgAbort_GA10X(abortReason, GR_ABORT_CHKPT_AFTER_PG_ON);
        }
    }

    // Engage Priv Blockers
    if (!s_grPrivBlockerEngage_GA10X(&abortReason, RM_PMU_LPWR_CTRL_ID_GR_PG))
    {
        s_grPgAbort_GA10X(abortReason,
                          GR_ABORT_CHKPT_AFTER_PRIV_BLOCKER);

        return FLCN_ERROR;
    }

    // Checkpoint 3: Check GR Idleness
    if (!grIsIdle_HAL(&Gr, &abortReason, RM_PMU_LPWR_CTRL_ID_GR_PG))
    {
        s_grPgAbort_GA10X(abortReason,
                          GR_ABORT_CHKPT_AFTER_PRIV_BLOCKER);

        return FLCN_ERROR;
    }

    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libFifo));

    //
    // Execute preemption sequence. As part of preemption sequence host saves
    // channel and engine context.
    //
    if (fifoPreemptSequence_HAL(&Fifo, PMU_ENGINE_GR, RM_PMU_LPWR_CTRL_ID_GR_PG) != FLCN_OK)
    {
        s_grPgAbort_GA10X(GR_ABORT_REASON_FIFO_PREEMPT_ERROR,
                         GR_ABORT_CHKPT_AFTER_PREEMPT_SEQUENCE);

        return FLCN_ERROR;
    }
    else
    {
        // Clear idle flip corresponding to GR_FE and GR_PRIV
        grClearIdleFlip_HAL(&Gr, RM_PMU_LPWR_CTRL_ID_GR_PG);
    }

    // Engage the Holdoff
    if (FLCN_OK != lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_GR_PG,
                                                 pGrState->holdoffMask))
    {
        s_grPgAbort_GA10X(GR_ABORT_REASON_HOLDOFF_ENABLE_TIMEOUT,
                         GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_1);

        return FLCN_ERROR;
    }

    // Check if the context is still INVALID
    if (!grIsCtxIlwalid_HAL(&Gr))
    {
        s_grPgAbort_GA10X(GR_ABORT_REASON_FIFO_PREEMPT_ERROR,
                GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_1);

        return FLCN_ERROR;
    }

    // Checkpoint 4: Check GR Idleness
    if (!grIsIdle_HAL(&Gr, &abortReason, RM_PMU_LPWR_CTRL_ID_GR_PG))
    {
        s_grPgAbort_GA10X(abortReason,
                         GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_1);

        return FLCN_ERROR;
    }

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, PRIV_RING))
    {
        grPgPrivErrHandlingActivate_HAL(&Gr, LW_TRUE);
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
grPgRestore_GA10X(void)
{
    OBJPGSTATE *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);

    if (PMUCFG_FEATURE_ENABLED(PMU_PG_GR_UNIFIED_POWER_SEQ) &&
        LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, HW_SEQUENCE))
    {
        grHwPgDisengage_HAL(&Gr);
    }

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, PRIV_RING))
    {
        grPgPrivErrHandlingActivate_HAL(&Gr, LW_FALSE);
    }

    // Disengage the Priv Blockers
    s_grPrivBlockerDisengage_GA10X(RM_PMU_LPWR_CTRL_ID_GR_PG);

    // Disable holdoff
    lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_GR_PG,
                                  LPWR_HOLDOFF_MASK_NONE);

    // Re enable the FG-RPPG
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_FG_RPPG) &&
        LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, FG_RPPG_EXCLUSION))
    {
        lpwrSeqSramFgRppgEnable_HAL(&Lpwr);
    }
    return FLCN_OK;
}

/*!
 * @brief Clear idle flip corresponding to  GR_FE and GR_PRIV
 */
void
grClearIdleFlip_GA10X(LwU8 ctrlId)
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
    val = FLD_SET_DRF(_CPWR, _PMU_PG_IDLE_FLIP_STATUS, _CE_0,
                      _IDLE, val);
    val = FLD_SET_DRF(_CPWR, _PMU_PG_IDLE_FLIP_STATUS, _CE_1,
                      _IDLE, val);
    REG_WR32_STALL(CSB, LW_CPWR_PMU_PG_IDLE_FLIP_STATUS, val);
}

/*!
 * @brief Save required Graphics registers
 */
void
grGlobalStateSavePmu_GA10X(void)
{
    GR_SEQ_CACHE  *pGrSeqCache = GR_SEQ_GET_CACHE();
    LwU32          regIdx      = 0;

    // Reset the auto increment control registers with 0
    s_grAutoIncrBufferReset_GA10X();

    // Save the register values in the data field
    for (regIdx = 0; regIdx < pGrSeqCache->globalStateRegCount; regIdx++)
    {
        pGrSeqCache->pGlobalStateBuffer[regIdx].data =
                REG_RD32(FECS, pGrSeqCache->pGlobalStateBuffer[regIdx].addr);
    }
}

/*!
 * @brief Restore required Graphics registers
 */
void
grGlobalStateRestorePmu_GA10X(void)
{
    GR_SEQ_CACHE  *pGrSeqCache = GR_SEQ_GET_CACHE();
    LwU32          regIdx      = 0;

    // Reset the auto increment control registers with 0
    s_grAutoIncrBufferReset_GA10X();

    // Restore the register values from the data field
    for (regIdx = 0; regIdx < pGrSeqCache->globalStateRegCount; regIdx++)
    {
        REG_WR32(FECS, pGrSeqCache->pGlobalStateBuffer[regIdx].addr,
                       pGrSeqCache->pGlobalStateBuffer[regIdx].data);
    }
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * Abort GR sequence, perform backout sequence as needed
 *
 * @param[in]  abortReason     Reason for aborting gr during entry sequence
 * @param[in]  abortStage      Stage of entry sequence where gr is aborted,
 *                             this determines the steps to be performed
 *                             in abort sequence
 */
static void
s_grRgAbort_GA10X
(
   LwU32  abortReason,
   LwU32  abortStage
)
{
    OBJPGSTATE *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_RG);

    // Update the abort reason
    pGrState->stats.abortReason = (abortStage | abortReason);

    switch (abortStage)
    {
        //
        // Do not change the order of case sections.
        // No break in each case except the last one.
        // Do not compile out "case" using #if(PMUCFG_FEATURE_ENABLED()).
        //
        case GR_ABORT_CHKPT_AFTER_MMU_UNBIND:
        {
            if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, UNBIND))
            {
                // Disable GR MMU Rebind
                grUnbind_HAL(&Gr, RM_PMU_LPWR_CTRL_ID_GR_RG, LW_FALSE);
            }

            // coverity[fallthrough]
        }
        case GR_ABORT_REASON_REGLIST_SAVE_TIMEOUT:
        case GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_0:
        {
            // Disable holdoffs
            lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_GR_RG,
                                          LPWR_HOLDOFF_MASK_NONE);

            // coverity[fallthrough]
        }
        case GR_ABORT_CHKPT_AFTER_PREEMPT_SEQUENCE:
        case GR_ABORT_CHKPT_AFTER_OVERLAY_LOAD:
        {
            OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libFifo));

            // coverity[fallthrough]
        }

        case GR_ABORT_CHKPT_AFTER_PRIV_BLOCKER:
        {
            // Disengage the Priv Blockers
            s_grPrivBlockerDisengage_GA10X(RM_PMU_LPWR_CTRL_ID_GR_RG);

            // Re-enable SMCARB free-running timestamp
            grSmcarbTimestampDisable_HAL(&Gr, LW_FALSE);

            // Re-enable GR-ELCG
            lpwrCgElcgEnable(PMU_ENGINE_GR, LPWR_CG_ELCG_CTRL_REASON_GR_RG);

            // Release the semaphore.
            grRgSemaphoreGive();

            // coverity[fallthrough]
        }
        case GR_ABORT_CHKPT_AFTER_DISABLE_CLK_ACCESS:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_LPWR))
            {
                if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, PRIV_RING) ||
                    LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, RESET))
                {
                    FLCN_STATUS status = FLCN_OK;

                    status = clkFreqDomainsDisable(LW2080_CTRL_CLK_DOMAIN_GPCCLK,      // clkDomainMask
                                                   LW2080_CTRL_CLK_CLIENT_ID_GR_RG,    // clientId
                                                   LW_FALSE,                           // bDisable
                                                   LW_FALSE);                          // bConditional
                    PMU_HALT_COND(status == FLCN_OK);
                }
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
 * @brief Power-gate GR engine's SRAM.
 *
 * @param[in] bBlocking     If its a blocking call
 */
void
s_grPgEngageSram_GA10X
(
    LwBool bBlocking
)
{
#if (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
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
                            GR_UNIFIED_SEQ_POLL_TIME_US_GA10X,
                            PMU_REG_POLL_MODE_CSB_EQ))
        {
            PMU_HALT();
        }
    }
#endif // (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
}

/*!
 * @brief Power-ungate GR engine's SRAM.
 */
void
s_grPgDisengageSram_GA10X(void)
{
    LwU32 val;

    val = REG_RD32(CSB, LW_CPWR_PMU_RAM_TARGET);
    val = FLD_SET_DRF(_CPWR_PMU, _RAM_TARGET, _GR, _POWER_ON, val);
    REG_WR32(CSB, LW_CPWR_PMU_RAM_TARGET, val);

    if (!PMU_REG32_POLL_US(LW_CPWR_PMU_RAM_STATUS,
                        DRF_SHIFTMASK(LW_CPWR_PMU_RAM_STATUS_GR_FSM),
                        DRF_DEF(_CPWR, _PMU_RAM_STATUS, _GR_FSM, _POWER_ON),
                        GR_UNIFIED_SEQ_POLL_TIME_US_GA10X,
                        PMU_REG_POLL_MODE_CSB_EQ))
    {
        PMU_HALT();
    }
}

/*!
 * @brief Channel preemption sequence
 *
 * We need to clear Idle Flip bit here because this step will itself
 * generate internal traffic and flip idleness
 */
static FLCN_STATUS
s_grFifoPreemption_GA10X(LwU8 ctrlId)
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
s_grPgAbort_GA10X
(
   LwU32  abortReason,
   LwU32  abortStage
)
{
    OBJPGSTATE *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);

    pGrState->stats.abortReason = (abortStage | abortReason);

    switch (abortStage)
    {
        //
        // Do not change the order of case sections.
        // No break in each case except the last one.
        // Do not compile out "case" using #if(PMUCFG_FEATURE_ENABLED()).
        //
        case GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_1:
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
            s_grPrivBlockerDisengage_GA10X(RM_PMU_LPWR_CTRL_ID_GR_PG);

            // coverity[fallthrough]
        }
        case GR_ABORT_CHKPT_AFTER_PG_ON:
        {
            //
            // Re enable the FG-RPPG
            // Please note: This is temp code, do not follow this practice
            //
            if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_FG_RPPG) &&
                LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, FG_RPPG_EXCLUSION))
            {
                lpwrSeqSramFgRppgEnable_HAL(&Lpwr);
            }
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
s_grPrivBlockerEngage_GA10X
(
    LwU32 *pAbortReason,
    LwU8   ctrlId
)
{
    LwBool bXveEnabled  = LW_FALSE;
    LwBool bSec2Enabled = LW_FALSE;
    LwBool bGspEnabled  = LW_FALSE;
    LwBool bSuccess     = LW_TRUE;

    // Enter the Critical Section
    appTaskCriticalEnter();

    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_UNIFIED_PRIV_BLOCKER_CTRL))
    {
        //
        // There are few registers in LW_PGRAPH range which are accessed by
        // ISR/DPC path in RM. As these registers are accessible when feature
        // is engaged, we can allow the access to go through. So we are enabling
        // Allow ranges here which contain these registers.
        // As this API is shared by GR-RPG as well, so restricting it to GPC-RG
        // only to let GR-RPG to remain untouched.
        //
        if (ctrlId == RM_PMU_LPWR_CTRL_ID_GR_RG)
        {
            // Enable Allow ranges
            lpwrPrivBlockerXveGrProfileAllowRangesEnable_HAL(&Lpwr, LW_TRUE);
        }

        if (FLCN_OK != lpwrCtrlPrivBlockerModeSet(ctrlId,
                                        LPWR_PRIV_BLOCKER_MODE_BLOCK_ALL,
                                        LPWR_PRIV_BLOCKER_PROFILE_GR,
                                        (GR_PRIV_BLOCKER_ENGAGE_TIMEOUT_US * 1000)))
        {
            *pAbortReason = GR_ABORT_REASON_PRIV_BLKR_ALL_TIMEOUT;

            bSuccess = LW_FALSE;
            goto s_grPrivBlockerEngage_GA10X_exit;
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

            bSuccess = LW_FALSE;
            goto s_grPrivBlockerEngage_GA10X_exit;
        }
    }

    // Check GR Idleness
    if (!grIsIdle_HAL(&Gr, pAbortReason, ctrlId))
    {
        bSuccess = LW_FALSE;
        goto s_grPrivBlockerEngage_GA10X_exit;
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

            bSuccess = LW_FALSE;
            goto s_grPrivBlockerEngage_GA10X_exit;
        }
    }

s_grPrivBlockerEngage_GA10X_exit:

    //
    // Because of the Bug 200657265 , we need to move the whole blocker
    // engage/disengage operation into critical section and resident section.
    // Idelly in non success case, blocker disengagement should happen in GR
    // Abort sequence, but to minimize the resident code, we are doing it here
    // explicitly.
    //
    // In the GR Abort sequence we will still call the function to disengage
    // the blocker, but that will be a NO-OP operation.
    //
    // Please note, this is very bad way of managing the code here, but due to
    // crunch of time and and minimise the impact we are doing it this way.
    //
    // In Long term we are going to enhance this solution in more organized way.
    // So do not follow this practice anywhere else.
    //
    if (!bSuccess)
    {
        lpwrCtrlPrivBlockerModeSet(ctrlId, LPWR_PRIV_BLOCKER_MODE_BLOCK_NONE,
                                   LPWR_PRIV_BLOCKER_PROFILE_GR, 0);
    }

    // Exit Critical Section
    appTaskCriticalExit();

    return bSuccess;
}

/*!
 * @brief Disengage the GR Priv Blockers
 *
 * @param[in]  ctrlId  LPWR_CTRL Id
 */
void
s_grPrivBlockerDisengage_GA10X
(
    LwU8   ctrlId
)
{
    // Disengage the Priv Blockers
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_UNIFIED_PRIV_BLOCKER_CTRL))
    {
        lpwrCtrlPrivBlockerModeSet(ctrlId, LPWR_PRIV_BLOCKER_MODE_BLOCK_NONE,
                                   LPWR_PRIV_BLOCKER_PROFILE_GR, 0);

        //
        // In GPC-RG entry, we enabled Allow ranges. We need to disable it
        // in the exit as this API is shared between GPC-RG and GR-RPG.
        //
        if (ctrlId == RM_PMU_LPWR_CTRL_ID_GR_RG)
        {
            // Disable Allow ranges
            lpwrPrivBlockerXveGrProfileAllowRangesEnable_HAL(&Lpwr, LW_FALSE);
        }
    }
}

/*!
 * @brief Save Global GR state
 *
 * It uses the following to save -
 *  Non-secure registers - via FECS
 *  Secure registers     - via PMU
 *
 * @return FLCN_STATUS  FLCN_OK    If save reglist is sucessful in 4ms.
 *                      FLCN_ERROR If save reglist is unsucessful in 4ms.
 */
static FLCN_STATUS
s_grRgGlobalStateSave_GA10X(LwU8 ctrlId)
{
    FLCN_STATUS status = FLCN_OK;
    // Save global GR state via PMU
    grGlobalStateSavePmu_HAL(&Gr);

    // Save global GR state via FECS
    status = grGlobalStateSave_HAL(&Gr, ctrlId);

    return status;
}

/*!
 * @brief Restore Global GR state
 *
 * It uses the following to restore -
 *  Non-secure registers - via FECS
 *  Secure registers     - via PMU
 *
 * NOTE - We need to flush the writes at the end
 */
static void
s_grRgGlobalStateRestore_GA10X(LwU8 ctrlId)
{
    // Restore global GR state via FECS
    grGlobalStateRestore_HAL(&Gr, ctrlId);

    // Restore global GR state via PMU
    grGlobalStateRestorePmu_HAL(&Gr);

    //
    // Bug : 200443717
    // Read a PGRAPH as well as FECS fence to ensure
    // all the writes are flushed out before further access
    //
    REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_TPCS_PE_BLK_ACTIVITY_WEIGHTS_A);
    REG_RD32(FECS, LW_PPRIV_GPC_PRI_FENCE);
}

/*!
 * @brief Reset the auto increment control registers to 0
 */
static void
s_grAutoIncrBufferReset_GA10X(void)
{
    GR_SEQ_CACHE  *pGrSeqCache = GR_SEQ_GET_CACHE();
    LwU32          idx         = 0;

    for (idx = 0; idx < pGrSeqCache->autoIncrBufferRegCount; idx++)
    {
        REG_WR32(FECS, pGrSeqCache->pAutoIncrBuffer[idx], 0);
    }
}

/*!
 * @brief Perf Sequencer entry step for GPC-RG
 *
 * It includes the following steps -
 * - De-init GPC clock
 * - Enable PRI error mechanism for GPC access
 * - Block propagation of reset for external units
 * - Assert full GPC Reset
 * - Assert GPC rail-gating clamp
 * - Gate LWVDD rail
 */
static void
s_grRgPerfSeqEntry_GA10X(void)
{
    OBJPGSTATE *pGrRgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_RG);
    LW_STATUS   status     = LW_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_LPWR) &&
        LPWR_CTRL_IS_SF_ENABLED(pGrRgState, GR, RG_PERF_CHANGE_SEQ))
    {
        // Update debugging info.
        g_CHSEQ_LPWR_DEBUG[11]++;

        status = perfDaemonChangeSeqLpwrScriptExelwte(
                    LW2080_CTRL_PERF_CHANGE_SEQ_SCRIPT_ID_LPWR_ENTRY);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            return;
        }
    }
    else
    {
        if (LPWR_CTRL_IS_SF_ENABLED(pGrRgState, GR, RESET))
        {
            // De-init GPC clock
            status = perfDaemonChangeSeqScriptExelwteStep_CLK_DEINIT();
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                return;
            }
        }

        // Pre Volt gating steps
        grGrRgEntryPreVoltGateLpwr_HAL(&Gr);

        if (LPWR_CTRL_IS_SF_ENABLED(pGrRgState, GR, HW_SEQUENCE))
        {
            // Gate LWVDD rail
            grRgRailGate_HAL(&Gr, LW_TRUE);
        }
    }
}

/*!
 * @brief Perf Sequencer exit step for GPC-RG
 *
 * It includes the following steps -
 * - Ungate LWVDD rail
 * - Disable utils clock CG
 * - Assert engine & context reset for BECS and FECS
 * - Unblock propagation of reset for external units
 * - De-assert GPC rail-gating clamp
 * - Poll for RAM repair completion
 * - De-assert engine & context reset for BECS and FECS
 * - De-assert full GPC reset
 * - Disable PRI error mechanism for GPC access
 * - Program SMC's PRI GPCCS_ENGINE_RESET_CTL
 * - Enable utils clock CG
 * - Init GPC clock
 */
static void
s_grRgPerfSeqExit_GA10X(void)
{
    OBJPGSTATE *pGrRgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_RG);
    LW_STATUS   status     = LW_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_LPWR) &&
        LPWR_CTRL_IS_SF_ENABLED(pGrRgState, GR, RG_PERF_CHANGE_SEQ))
    {
        // Update debugging info.
        g_CHSEQ_LPWR_DEBUG[12]++;

        status = perfDaemonChangeSeqLpwrScriptExelwte(
                    LW2080_CTRL_PERF_CHANGE_SEQ_SCRIPT_ID_LPWR_EXIT);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            return;
        }
    }
    else
    {
        if (LPWR_CTRL_IS_SF_ENABLED(pGrRgState, GR, HW_SEQUENCE))
        {
            // Ungate LWVDD rail
            grRgRailGate_HAL(&Gr, LW_FALSE);
        }

        // Post Volt gating steps
        grGrRgExitPostVoltUngateLpwr_HAL(&Gr);

        if (LPWR_CTRL_IS_SF_ENABLED(pGrRgState, GR, RESET))
        {
            // Init GPC clock
            status = perfDaemonChangeSeqScriptExelwteStep_CLK_INIT();
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                return;
            }
        }
    }
}

/*!
 * @brief Perf Sequencer post exit step for GPC-RG
 *
 * It includes the following steps -
 * - Switch GPCNAFLL to VCO path
 * - Enable the access to GPC clock
 */
static void
s_grRgPerfSeqPostExit_GA10X(void)
{
    OBJPGSTATE *pGrRgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_RG);
    LW_STATUS   status     = LW_OK;
    DISPATCH_PERF_DAEMON_CHANGE_SEQ_LPWR_POST_SCRIPT dispatchPostScriptExelwte = {{0}};

    //
    // Send notification to perf daemon task queue for exelwting
    // post script of GPC-RG
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_LPWR) &&
        LPWR_CTRL_IS_SF_ENABLED(pGrRgState, GR, RG_PERF_CHANGE_SEQ))
    {
        // Update debugging info.
        g_CHSEQ_LPWR_DEBUG[13]++;

        // Disallow all PgCtrls except EI
        grRgPerfSeqDisallowAll(LW_TRUE);

        dispatchPostScriptExelwte.hdr.eventType = PERF_DAEMON_EVENT_ID_PERF_CHANGE_SEQ_LPWR_POST_SCRIPT_EXELWTE;
        dispatchPostScriptExelwte.lpwrScriptId  = LW2080_CTRL_PERF_CHANGE_SEQ_SCRIPT_ID_LPWR_POST;

        (void)lwrtosQueueIdSendBlocking(LWOS_QUEUE_ID(PMU, PERF_DAEMON),
                &dispatchPostScriptExelwte, sizeof(dispatchPostScriptExelwte));
    }
    else
    {
        if (LPWR_CTRL_IS_SF_ENABLED(pGrRgState, GR, RESET))
        {
            // Restore the remainder of clock settings - non-critical exit
            status = perfDaemonChangeSeqScriptExelwteStep_CLK_RESTORE();
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                return;
            }
        }

        //
        // Re-enable the access to GPC clock if the corresponding clock feature is
        // enabled. Re-enabling of clocks needs to be done here when perf change
        // sequencer is disabled, otherwise it is done in
        // @ref lpwrGrRgPerfScriptCompletionEvtHandler.
        //
        if (PMUCFG_FEATURE_ENABLED(PMU_CLK_LPWR))
        {
            if (LPWR_CTRL_IS_SF_ENABLED(pGrRgState, GR, PRIV_RING) ||
                LPWR_CTRL_IS_SF_ENABLED(pGrRgState, GR, RESET))
            {
                OSTASK_OVL_DESC ovlDescList[] = {
                    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, clkLibClk3)
                };

                OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
                {
                    status = clkFreqDomainsDisable(LW2080_CTRL_CLK_DOMAIN_GPCCLK,      // clkDomainMask
                                                    LW2080_CTRL_CLK_CLIENT_ID_GR_RG,    // clientId
                                                    LW_FALSE,                           // bDisable
                                                    LW_FALSE);                          // bConditional
                }
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

                // Check for the returned status now
                if (status != FLCN_OK)
                {
                    // We halt here as a non-okay status here is not recoverable at this point
                    PMU_HALT();
                    return;
                }
            }
        }
    }
}
