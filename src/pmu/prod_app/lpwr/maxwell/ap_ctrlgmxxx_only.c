/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   ap_ctrlgmxxx_only.c
 * @brief  Maxwell specific Adaptive Power controller file
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
/* ------------------------ Application includes --------------------------- */
#include "main.h"
#include "pmu_objhal.h"
#include "pmu_objap.h"
#include "dbgprintf.h"
#include "dev_pwr_csb_addendum.h"

#include "config/g_ap_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Prototypes ------------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
#define AP_CFG_HISTOGRAM_NUM_BIN_PER_REGISTER__SIZE_1                       \
       (RM_PMU_AP_CFG_HISTOGRAM_BIN_N / LW_CPWR_PMU_HISTOGRAM_BINS_IDX__SIZE_1)
/* ------------------------ Local Functions  ------------------------------- */
/* ------------------------ Public Functions  ------------------------------ */

/*!
 * @brief Read Histogram
 *
 * @param [in]  ctrlId       AP Ctrl Id
 *
 * @return      RMU_OK       If histogram read completed successfully
 */
FLCN_STATUS
apCtrlHistogramRead_GMXXX
(
    LwU8 ctrlId
)
{
    LwU32    regVal;
    LwU32    i;
    AP_CTRL *pApCtrl = AP_GET_CTRL(ctrlId);
    LwU8     histogramIndex;
    LwU8     idleCntrIdx;

    histogramIndex = apCtrlGetHistogramIndex(ctrlId);
    idleCntrIdx    = apCtrlGetIdleMaskIndex(ctrlId);

    //
    // WAR398576: Comment #24: Insert Idle egde before reading histogram
    //
    // HW updates histogram bins only at edge of idle signal. Insert idle edge
    // by toggling counter mode between FORCE_BUSY and AUTO_IDLE to flush last
    // idle period before capturing histogram.
    //
    // We don't have Histogram Bin upper boundaries. Histogram Bin15 contains
    // number of idle_period >= 2^15 oclwrrence. If idle signal stays high for
    // so long that idle counter reach its maximum(0xFFFFFFFF) then counter
    // will stay there until next busy egde. Inserting this edge will increment
    // Bin15 and reset counter.
    //
    // Benefits:
    // Histogram bins will have non-zero value even if its idle from long time.
    // All bins are zero represents HW being completely busy.
    //
    pgApConfigCntrMode_HAL(&Pg, idleCntrIdx, PG_SUPP_IDLE_COUNTER_MODE_FORCE_BUSY);
    pgApConfigCntrMode_HAL(&Pg, idleCntrIdx, PG_SUPP_IDLE_COUNTER_MODE_AUTO_IDLE);

    regVal = REG_RD32(CSB, LW_CPWR_PMU_HISTOGRAM_CTRL(histogramIndex));
    regVal = FLD_SET_DRF(_CPWR, _PMU_HISTOGRAM_CTRL, _LOAD, _TRIG, regVal);
    REG_WR32(CSB, LW_CPWR_PMU_HISTOGRAM_CTRL(histogramIndex), regVal);

    // Update bin0Cycles
    pApCtrl->bin0Cycles = BIT(DRF_VAL(_CPWR, _PMU_HISTOGRAM_CTRL,
                                      _SHIFTWINDOW, regVal));

    for (i = 0; i < LW_CPWR_PMU_HISTOGRAM_BINS_IDX__SIZE_1; i++)
    {
        regVal = REG_RD32(CSB, LW_CPWR_PMU_HISTOGRAM_BINS_IDX(histogramIndex, i));
        pApCtrl->pStat->bin[(i * AP_CFG_HISTOGRAM_NUM_BIN_PER_REGISTER__SIZE_1)    ] =
            (LwU8)DRF_VAL(_CPWR, _PMU_HISTOGRAM_BINS3TO0, _00, regVal);

        pApCtrl->pStat->bin[(i * AP_CFG_HISTOGRAM_NUM_BIN_PER_REGISTER__SIZE_1) + 1] =
            (LwU8)DRF_VAL(_CPWR, _PMU_HISTOGRAM_BINS3TO0, _01, regVal);

        pApCtrl->pStat->bin[(i * AP_CFG_HISTOGRAM_NUM_BIN_PER_REGISTER__SIZE_1) + 2] =
            (LwU8)DRF_VAL(_CPWR, _PMU_HISTOGRAM_BINS3TO0, _02, regVal);

        pApCtrl->pStat->bin[(i * AP_CFG_HISTOGRAM_NUM_BIN_PER_REGISTER__SIZE_1) + 3] =
            (LwU8)DRF_VAL(_CPWR, _PMU_HISTOGRAM_BINS3TO0, _03, regVal);
    }

    // Generate final Bins by merging SW histogram with HW histogram
    if (PMUCFG_FEATURE_ENABLED(PMU_AP_WAR_INCREMENT_HIST_CNTR))
    {
        apCtrlMergeSwHistBin(ctrlId);
    }

    return FLCN_OK;
}
