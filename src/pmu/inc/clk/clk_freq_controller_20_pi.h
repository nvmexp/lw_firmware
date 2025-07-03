/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_FREQ_CONTROLLER_20_PI_H
#define CLK_FREQ_CONTROLLER_20_PI_H

/*!
 * @file clk_freq_controller_20_pi.h
 * @brief @copydoc clk_freq_controller_20_pi.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_freq_controller.h"
#include "clk/clk_freq_counted_avg.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Clock PI frequency controller structure.
 */
typedef struct
{
    /*!
     * CLK_FREQ_CONTROLLER_20 super class. Must always be the first element
     * in the structure.
     */
    CLK_FREQ_CONTROLLER_20    super;
    /*!
     * Proportional gain for this frequency controller in uV/MHz
     */
    LwSFXP20_12               propGain;
    /*!
     * Integral gain for this frequency controller in uV/MHz
     */
    LwSFXP20_12               integGain;
    /*!
     * Decay factor for the integral term.
     */
    LwSFXP20_12               integDecay;
    /*!
     * Absolute value of Positive Frequency Hysteresis in MHz (0 => no hysteresis).
     * (hysteresis to apply when frequency has positive delta)
     */
    LwS32                     freqHystPosMHz;
    /*!
     * Absolute value of Negative Frequency Hysteresis in MHz (0 => no hysteresis).
     * (hysteresis to apply when frequency has negative delta)
     */
    LwS32                     freqHystNegMHz;
    /*!
     * Error term for this frequency controller from the previous iteration.
     */
    LwSFXP20_12               errorPreviousMHz;
    /*!
     * Frequency Error for the current cycle
     */
    LwS32                     errorkHz;
    /*!
     * Final voltage delta after each iteration for this controller.
     */
    LwS32                     voltOffsetuV;
    /*!
     * Measured frequency counter sample info.
     */
    LW2080_CTRL_CLK_CNTR_SAMPLE_ALIGNED measuredSample;
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_COUNTED_AVG)
    /*!
     * Target frequency counter sample info.
     */
    CLK_FREQ_COUNTER_SAMPLE     targetSample;
#endif
    /*!
     * Poison counter counting conselwtive poison request.
     */
    LwU32                       poisonCounter;
} CLK_FREQ_CONTROLLER_20_PI;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkFreqControllerGrpIfaceModel10ObjSet_PI_20)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkFreqControllerGrpIfaceModel10ObjSet_PI_20");

// CLK_FREQ_CONTROLLER_PI interfaces
BoardObjIfaceModel10GetStatus       (clkFreqControllerEntryIfaceModel10GetStatus_PI_20)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkFreqControllerEntryIfaceModel10GetStatus_PI_20");
FLCN_STATUS clkFreqControllerEval_PI_20(CLK_FREQ_CONTROLLER_20_PI *pFreqControllerPI)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllerEval_PI_20");
FLCN_STATUS clkFreqControllerReset_PI_20(CLK_FREQ_CONTROLLER_20_PI *pFreqControllerPI)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllerReset_PI_20");
LwS32       clkFreqControllerVoltDeltauVGet_PI_20(CLK_FREQ_CONTROLLER_20_PI *pFreqControllerPI)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllerVoltDeltauVGet_PI_20");
FLCN_STATUS clkFreqControllerVoltDeltauVSet_PI_20(CLK_FREQ_CONTROLLER_20_PI *pFreqControllerPI, LwS32 voltOffsetuV)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllerVoltDeltauVSet_PI_20");

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_FREQ_CONTROLLER_20_PI_H
