/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dfprga10x.c
 * @brief  HAL-interface for the dfpr Layer 1 i.e Prefetch Layer
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"
#include "dev_ltc.h"
#include "dev_pri_ringmaster.h"
/* ------------------------- Application Includes --------------------------- */
#include "pmu_objlpwr.h"
#include "pmu_objdfpr.h"
#include "pmu_objfifo.h"
#include "pmu_objic.h"
#include "pmu_bar0.h"

#include "config/g_dfpr_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Static Functions -------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* --------------------------- Prototypes ------------------------------------*/
/* ------------------------ Public Functions  ------------------------------- */

/*!
 * @brief Initializes the idle mask for dfpr Layer i.e Layer 1
 */
void
dfprIdleMaskInit_GA10X(void)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_DFPR);
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

    if (FIFO_IS_ENGINE_PRESENT(CE4) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE4))
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

    if (FIFO_IS_ENGINE_PRESENT(ENC0))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWENC0, _ENABLED, im0);
    }

    pPgState->idleMask[0] = im0;
    pPgState->idleMask[1] = im1;
    pPgState->idleMask[2] = im2;
}

/*!
 * @brief Configure entry conditions for dfpr Prefetch - Layer 1 HW FSM
 *
 * Helper to program entry conditions for dfpr Layer 1 LPWR_ENG(DIFR_PREFETCH).
 * HW entry conditions -
 *  GR Feature is engaged
 */
void
dfprEntryConditionConfig_GA10X(void)
{
    LwU32 lpwrCtrlId;
    LwU32 parentHwEngIdx;
    LwU32 childHwEngIdx = PG_GET_ENG_IDX(RM_PMU_LPWR_CTRL_ID_DFPR);

    //
    // GR Pre conditions for dfpr Prefetch Layer
    //

    // Check for GR coupled dfpr support
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_GR) &&
        LPWR_ARCH_IS_SF_SUPPORTED(GR_COUPLED_DFPR))
    {
        FOR_EACH_INDEX_IN_MASK(32, lpwrCtrlId, lpwrGrpCtrlMaskGet(RM_PMU_LPWR_GRP_CTRL_ID_GR))
        {
            parentHwEngIdx = PG_GET_ENG_IDX(lpwrCtrlId);

            // Call the Dependecny Handler Function
            lpwrFsmDependencyInit_HAL(&Lpwr, parentHwEngIdx, childHwEngIdx);
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
}

/*!
 * Initialize various L2 HW parameters
 *
 */
void
dfprLtcInit_GA10X(void)
{
    LwU32 regVal = 0;

    regVal = REG_RD32(FECS, LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_L2);

    // Cache the Number of Active LTCs
    Dfpr.numLtc = DRF_VAL(_PPRIV, _MASTER_RING_ENUMERATE_RESULTS_L2, _COUNT, regVal);
}

/*!
 * Change the L2 Cache EVICT_LAST Settings
 *
 *  1. Save the SET_0 and SET_4 registers.
 *  2. Change the Max Ways to 14
 *  3  Disable the BG_DEMOTION Flag
 *
 * @param[in]   ctrlId  LPWR_CTRL Id
 *
 */
FLCN_STATUS
dfprLtcEntrySeq_GA10X
(
    LwU8 ctrlId
)
{
    LwU32 regVal;

    regVal = REG_RD32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_0);

    // Cache the MGMT_0 register
    Dfpr.l2CacheReg.l2CacheSetMgmt0 = regVal;

    // Set the MAX_WAYS for EVICT_LAST = 14
    regVal = FLD_SET_DRF_NUM(_PLTCG, _LTCS_LTSS_TSTG_SET_MGMT_0,
                             _MAX_WAYS_EVICT_LAST, RM_PMU_DIFR_NUM_MAX_WAYS_EVICT_LAST, regVal);
    REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_0, regVal);

    regVal = REG_RD32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_4);

    // Cache the MGMT_4 register
    Dfpr.l2CacheReg.l2CacheSetMgmt4 = regVal;

    // Update the BG_DEMOTE Flag to DISABLED
    regVal = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_TSTG_SET_MGMT_4,
                             _BG_DEMOTION, _DISABLED, regVal);
    REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_4, regVal);

    // Cache the CFG_0 register
    Dfpr.l2CacheReg.l2CacheCfg0 = regVal;

    //
    // Changes for HW Bug: 3440373
    // Set the LW_PLTCG_LTC0_LTS0_TSTG_CFG_0_ISO_WITHOUT_ICC = DISABLED
    // Set the LW_PLTCG_LTC0_LTS0_TSTG_CFG_0_SPARE[24] = 1
    //
    regVal = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_TSTG_CFG_0,
                         _ISO_WITHOUT_ICC, _DISABLED, regVal);

    regVal = FLD_SET_DRF_NUM(_PLTCG, _LTCS_LTSS_TSTG_CFG_0,
                             _SPARE, 0x1, regVal);

    REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_CFG_0, regVal);

    return FLCN_OK;
}

/*!
 * Restore the L2 Cache Dirty Data Configuration
 *
 * @param[in]   ctrlId  LPWR_CTRL Id
 *
 */
void
dfprLtcExitSeq_GA10X
(
    LwU8 ctrlId
)
{
    // Restore the OPPORTUNISTIC_PREFETCH Flag Settings
    REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_CFG_0,
             Dfpr.l2CacheReg.l2CacheCfg0);

    // Resotre the DIFR MXA Ways Settings
    REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_0,
             Dfpr.l2CacheReg.l2CacheSetMgmt0);

    // Restore the BG_DEMOTE Flag Settings
    REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_4,
             Dfpr.l2CacheReg.l2CacheSetMgmt4);
}
