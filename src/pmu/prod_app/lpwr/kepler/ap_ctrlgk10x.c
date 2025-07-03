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
 * @file   ap_ctrlgk10x.c
 * @brief  Kepler specific Adaptive Power controller file
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
/* ------------------------ Application includes --------------------------- */
#include "main.h"
#include "pmu_objhal.h"
#include "pmu_objap.h"
#include "dbgprintf.h"

#include "config/g_ap_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Prototypes ------------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Local Functions  ------------------------------- */
/* ------------------------ Public Functions  ------------------------------ */

/*!
 * @brief Starts Histogram
 *
 * Histogram works in two modes. Mode decides the representation of histogram
 * bin boundaries -
 * 1) Fixed Mode:
 *    Histogram bin boundaries are represented as fixed powers of 2 numbers.
 *    The most significant "1" of the idle counter value determines the bin
 *    that is incremented for that idle duration.
 * 2) Float Mode:
 *    Histogram bin boundaries are represented as 8 bit floating point numbers.
 *
 * This function programs histograms in Fixed Mode. It reset histogram sub-unit
 * and starts logging.
 *
 * @param [in]  ctrlId       AP Ctrl Id
 * @param [in]  shiftWindow  SHIFTWINDOW field in histogram control register
 *
 * @return      RMU_OK       If histogram started successfully
 */
FLCN_STATUS
apCtrlHistogramResetAndStart_GMXXX
(
    LwU8 ctrlId,
    LwU8 shiftWindow
)
{
    LwU32 regVal;
    LwU8 histogramIndex;

    histogramIndex = apCtrlGetHistogramIndex(ctrlId);

    //
    // Reset and Restart histogram in Fixed Mode. Writing _RESET to _ON and
    // then to OFF is the expected method of resetting histogram.
    //
    regVal = FLD_SET_DRF(_CPWR, _PMU_HISTOGRAM_CTRL, _START, _ON, 0);
    regVal = FLD_SET_DRF(_CPWR, _PMU_HISTOGRAM_CTRL, _RESET, _ON, regVal);
    regVal = FLD_SET_DRF_NUM(_CPWR, _PMU_HISTOGRAM_CTRL, _SHIFTWINDOW, shiftWindow, regVal);
    regVal = FLD_SET_DRF(_CPWR, _PMU_HISTOGRAM_CTRL, _MODE, _FIXED, regVal);

    REG_WR32(CSB, LW_CPWR_PMU_HISTOGRAM_CTRL(histogramIndex),regVal);

    regVal = FLD_SET_DRF(_CPWR, _PMU_HISTOGRAM_CTRL, _RESET, _OFF, regVal);
    REG_WR32(CSB, LW_CPWR_PMU_HISTOGRAM_CTRL(histogramIndex),regVal);

    return FLCN_OK;
}

/*!
 * @brief Stop Histogram
 *
 * @param [in]  ctrlId       AP Ctrl Id
 *
 * @return      RMU_OK       If histogram stoped successfully
 */
FLCN_STATUS
apCtrlHistogramStop_GMXXX
(
    LwU8 ctrlId
)
{
    LwU32 regVal;
    LwU8 histogramIndex;

    histogramIndex = apCtrlGetHistogramIndex(ctrlId);

    regVal = REG_RD32(CSB, LW_CPWR_PMU_HISTOGRAM_CTRL(histogramIndex));
    regVal = FLD_SET_DRF(_CPWR, _PMU_HISTOGRAM_CTRL, _START, _OFF, regVal);
    REG_WR32(CSB, LW_CPWR_PMU_HISTOGRAM_CTRL(histogramIndex), regVal);

    return FLCN_OK;
}

