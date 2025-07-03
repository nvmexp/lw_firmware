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
#include "hwproject.h"
/* ------------------------- Application Includes --------------------------- */
#include "pmu_bar0.h"
#include "pmu_objlpwr.h"
#include "pmu_objdfpr.h"
#include "pmu_objfifo.h"
#include "pmu_objic.h"

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
 * @brief Initializes the idle mask for dfpr Prefetch Layer i.e Layer 1
 */
void
dfprIdleMaskInit_AD10X(void)
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

    if (FIFO_IS_ENGINE_PRESENT(DEC2))
    {
        im2 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_LWDEC2, _ENABLED, im2);
    }

    if (FIFO_IS_ENGINE_PRESENT(DEC3))
    {
        im2 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_LWDEC3, _ENABLED, im2);
    }

    if (FIFO_IS_ENGINE_PRESENT(ENC0))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWENC0, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(ENC1))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWENC1, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(ENC2))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWENC2, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(OFA0))
    {
        im2 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_OFA, _ENABLED, im2);
    }

    if (FIFO_IS_ENGINE_PRESENT(JPG0))
    {
        im2 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_LWJPG0, _ENABLED, im2);
    }

    pPgState->idleMask[0] = im0;
    pPgState->idleMask[1] = im1;
    pPgState->idleMask[2] = im2;
}

/*!
 * @brief L2 Cache Demote Operation
 */
FLCN_STATUS
dfprL2CacheDemote_AD10X
(
    LwU16 *pAbortReason
)
{
    FLCN_TIMESTAMP demoteStartTimeNs;
    FLCN_STATUS    status       = FLCN_OK;
    LwU32          regVal;
    LwU32          ltcOffset;
    LwU32          idx;
    LwU16          abortReason  = 0;
    LwBool         bStopPolling = LW_FALSE;

    // Read all the LTC Slices to make sure there is no existing Pending operation.
    for (idx = 0; idx < Dfpr.numLtc; idx++)
    {
        // get offset for this ltc
        ltcOffset = LW_LTC_PRI_STRIDE * idx;

        regVal = REG_RD32(FECS, (LW_PLTCG_LTC0_LTSS_TSTG_CMGMT_1 + ltcOffset));

        // If Demote or Clean operation is pending on any of the LTC, Abort the sequence.
        if ((FLD_TEST_DRF(_PLTCG, _LTCS_LTSS_TSTG_CMGMT_1, _CLEAN, _PENDING, regVal)) ||
            (FLD_TEST_DRF(_PLTCG, _LTCS_LTSS_TSTG_CMGMT_1, _DEMOTE, _PENDING, regVal)))
        {
            abortReason = DFPR_ABORT_REASON_L2_OPS_PENDING;
            status = FLCN_ERROR;

            goto dfprL2CacheDemote_AD10X_exit;
        }
    }

    regVal = REG_RD32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_CMGMT_1);

    // Issue the Demote command
    regVal = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_TSTG_CMGMT_1, _DEMOTE, _PENDING, regVal);
    REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_CMGMT_1, regVal);

     // Start timer for Demote polling
    osPTimerTimeNsLwrrentGet(&demoteStartTimeNs);

    // Read all the LTC Slices to make sure there is no existing Pending operation.
    for (idx = 0; idx < Dfpr.numLtc; idx++)
    {
        // get offset for this ltc
        ltcOffset = LW_LTC_PRI_STRIDE * idx;

        // Poll for Demote to finish for each LTC in time.
        do
        {
            regVal = REG_RD32(FECS, (LW_PLTCG_LTC0_LTSS_TSTG_CMGMT_1 + ltcOffset));

            // Check Demote operation is pending or not on each LTC.
            bStopPolling = FLD_TEST_DRF(_PLTCG, _LTCS_LTSS_TSTG_CMGMT_1,
                                         _DEMOTE, _NOT_PENDING, regVal);

            // Check for timeout value
            if (osPTimerTimeNsElapsedGet(&demoteStartTimeNs) >=
                                         DFPR_L2_DEMOTE_TIMEOUT_NS)
            {
                abortReason = DFPR_ABORT_REASON_L2_DEMOTE_TIMEOUT;
                status = FLCN_ERR_TIMEOUT;
                PMU_BREAKPOINT();

                goto dfprL2CacheDemote_AD10X_exit;
            }
        }
        while (!bStopPolling);
    }
dfprL2CacheDemote_AD10X_exit:

    if (pAbortReason != NULL)
    {
        *pAbortReason = abortReason;
    }

    return status;
}

/*!
 * Change the L2 Cache EVICT_LAST related Settings
 *
 *  1. Save the SET_0, SET_4 and CFG0 registers.
 *  2. Change the Max Ways to 14
 *  3  Disable the BG_DEMOTION Flag
 *  4. Disable the OPPORTUNISTIC_PREFETCH bit.
 *
 * @param[in]   ctrlId  LPWR_CTRL Id
 *
 */
FLCN_STATUS
dfprLtcEntrySeq_AD10X
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

    regVal = REG_RD32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_CFG_0);

    // Cache the CFG_0 register
    Dfpr.l2CacheReg.l2CacheCfg0 = regVal;

    // Set the OPPORTUNISTIC_PREFETCH == DISABLE
    regVal = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_TSTG_CFG_0,
                         _OPPORTUNISTIC_PREFETCH, _DISABLED, regVal);

    //
    // Changes for HW Bug: 3440373
    // Entry:
    // Set the ISO_RD_EVICT_LAST = ENABLED
    // Exit:
    // Set the ISO_RD_EVICT_LAST = Disabled
    //
    regVal = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_TSTG_CFG_0,
                         _DIFR_ISO_RD_EVICT_LAST, _ENABLED, regVal);

    REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_CFG_0, regVal);
    return FLCN_OK;
}

/*!
 * Restore the L2 Cache Dirty Data Configuration
 *
 * @param[in]   ctrlId  LPWR_CTRL Id
 *
 *  Exit Steps:
 *
 *  1. Restore the setMgmt0
 *  2. Restore the setMgmt4
 *  3. Restore the cfg0
 *
 */
void
dfprLtcExitSeq_AD10X
(
    LwU8 ctrlId
)
{
    // Restore the OPPORTUNISTIC_PREFETCH Flag Settings
    REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_CFG_0,
             Dfpr.l2CacheReg.l2CacheCfg0);

    // Restore the BG_DEMOTE Flag Settings
    REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_4,
             Dfpr.l2CacheReg.l2CacheSetMgmt4);

    // Resotre the DIFR MXA Ways Settings
    REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_0,
             Dfpr.l2CacheReg.l2CacheSetMgmt0);
}
