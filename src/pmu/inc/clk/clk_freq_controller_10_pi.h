/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_FREQ_CONTROLLER_10_PI_H
#define CLK_FREQ_CONTROLLER_10_PI_H

/*!
 * @file clk_freq_controller_10_pi.h
 * @brief @copydoc clk_freq_controller_10_pi.c
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
     * CLK_FREQ_CONTROLLER super class. Must always be the first element
     * in the structure.
     */
    CLK_FREQ_CONTROLLER_10 super;
    /*!
     * Proportional gain for this frequency controller in uV/MHz
     */
    LwSFXP20_12         propGain;
    /*!
     * Integral gain for this frequency controller in uV/MHz
     */
    LwSFXP20_12         integGain;
    /*!
     * Decay factor for the integral term.
     */
    LwSFXP20_12         integDecay;
    /*!
     * Error term for this frequency controller from the previous iteration.
     */
    LwSFXP20_12         errorPreviousMHz;
    /*!
     * Frequency Error for the current cycle
     */
    LwS32               errorkHz;
    /*!
     * Voltage delta limit range min value.
     */
    LwS32               voltDeltaMin;
    /*!
     * Voltage delta limit range max value.
     */
    LwS32               voltDeltaMax;
    /*!
     * Final voltage delta after each iteration for this controller.
     */
    LwS32               voltDeltauV;
    /*!
     * Minimum percentage time of the HW slowdown required in a
     * sampling period to poison the sample.
     */
    LwU8                slowdownPctMin;
    /*!
     * Whether to poison the sample only if the slowdown oclwred
     * on the clock domain of this frequency controller. FALSE means
     * poison the sample even if slowdown oclwred on other clock domains.
     */
    LwBool              bPoison;
    /*!
     * Timer value in ns which stores the total time hardware failsafe
     * events were enagaged.
     */
    LwU32               prevHwfsEngageTimeNs;
    /*!
     * PMU timestamp value in ns which stores the total evalution time for
     * hardware failsafe events.
     */
    FLCN_TIMESTAMP      prevHwfsEvalTimeNs;
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
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_BA_SUPPORTED)
    /*!
     * Index into the Thermal Monitor Table. To be used for BA EDPp to poison
     * the sample per baPctMin value below.
     *
     * Invalid index value means that BA support is not required and will
     * disable the poisoning behavior.
     */
    LwU8                baThermMonIdx;
    /*!
     * Minimum percentage of time BA should engage to poison the sample.
     */
    LwUFXP4_12          baPctMin;
    /*!
     * Timer value in ns which stores the total time BA was engaged.
     */
    FLCN_TIMESTAMP      prevBaEngageTimeNs;
    /*!
     * PMU timestamp value in ns which stores the total evalution time for
     * BA.
     */
    FLCN_TIMESTAMP      prevBaEvalTimeNs;
#endif

#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_DROOPY_SUPPORTED)
    /*!
     * Index into the Thermal Monitor Table. To be used for Droopy VR to poison
     * the sample per droopyPctMin value below.
     *
     * Invalid index value means that droopy support is not required and will
     * disable the poisoning behavior.
     */
    LwU8                thermMonIdx;
    /*!
     * Minimum percentage of time droopy should engage to poison the sample.
     */
    LwUFXP4_12          droopyPctMin;
    /*!
     * Timer value in ns which stores the total time droopy was engaged.
     */
    FLCN_TIMESTAMP      prevDroopyEngageTimeNs;
    /*!
     * PMU timestamp value in ns which stores the total evalution time for
     * droopy.
     */
    FLCN_TIMESTAMP      prevDroopyEvalTimeNs;
#endif
    /*!
     * Poison counter counting conselwtive poison request.
     */
    LwU32               poisonCounter;
} CLK_FREQ_CONTROLLER_10_PI;


/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkFreqControllerGrpIfaceModel10ObjSet_PI_10)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkFreqControllerGrpIfaceModel10ObjSet_PI_10");

// CLK_FREQ_CONTROLLER_PI interfaces
BoardObjIfaceModel10GetStatus       (clkFreqControllerEntryIfaceModel10GetStatus_PI_10)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkFreqControllerEntryIfaceModel10GetStatus_PI_10");
FLCN_STATUS clkFreqControllerEval_PI_10(CLK_FREQ_CONTROLLER_10_PI *pFreqControllerPI)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllerEval_PI_10");
FLCN_STATUS clkFreqControllerReset_PI_10(CLK_FREQ_CONTROLLER_10_PI *pFreqControllerPI)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllerReset_PI_10");
LwS32       clkFreqControllerVoltDeltauVGet_PI_10(CLK_FREQ_CONTROLLER_10_PI *pFreqControllerPI)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllerVoltDeltauVGet_PI_10");
FLCN_STATUS clkFreqControllerVoltDeltauVSet_PI_10(CLK_FREQ_CONTROLLER_10_PI *pFreqControllerPI, LwS32 voltDeltauV)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllerVoltDeltauVSet_PI_10");
/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_FREQ_CONTROLLER_10_PI_H
