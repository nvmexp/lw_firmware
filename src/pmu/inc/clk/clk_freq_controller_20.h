/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_FREQ_CONTROLLER_20_H
#define CLK_FREQ_CONTROLLER_20_H

/*!
 * @file clk_freq_controller_20.h
 * @brief @copydoc clk_freq_controller_20.c
 */

/* ------------------------ Includes ---------------------------------------- */
#include "clk/clk_freq_controller.h"

/* ------------------------ Macros ------------------------------------------ */
/*!
 * Macro to locate CLK_FREQ_CONTROLLERS BOARDOBJGRP.
 *
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_CLK_FREQ_CONTROLLER_20                      \
    (&(CLK_CLK_FREQ_CONTROLLERS_GET())->super.super)

/* ------------------------ Defines ----------------------------------------- */
/* ------------------------ Datatypes --------------------------------------- */
/*!
 * Clock frequency controller structure.
 */
typedef struct
{
    /*!
     * CLK_FREQ_CONTROLLER super class. Must always be the first element in the structure.
     */
    CLK_FREQ_CONTROLLER    super;
    /*!
     * Keep the controller disabled
     */
    LwBool                 bDisable;
    /*!
     * Mask of clients requested to disable this controller.
     * @ref LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_CLIENT_ID_<xyz>.
     */
    LwU32                  disableClientsMask;
    /*!
     * Mode of operation @ref LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_MODE_<xyz>.
     */
    LwU8                   freqMode;
    /*!
     * Clock domain @ref LW2080_CTRL_CLK_DOMAIN_<xyz>.
     */
    LwU8                   clkDomainIdx;
    /*!
     * Voltage controller index points to valid CLVC controller
     * This will be used to control whether to respect CLFC voltage offset correction.
     * The guideline is to ignore CLFC voltage offset of given sample if CLVC
     * voltage error (Vrequested - Vsensed) of current sample is greater than
     * or equal to global voltage error Threshold defined in controllers table header.
     */
    LwU8                   voltControllerIdx;
    /*!
     * Absolute CLVC voltage error Threshold
     * Ignore CLFC voltage offset of given sample if CLVC voltage error (Vrequested - Vsensed)
     * of current sample is greater than or equal to voltage error Threshold.
     */
    LwU32                  voltErrorThresholduV;
    /*!
     * Therm monitor index 0
     */
    LwU8                   thermMonIdx0;
    /*!
     * Minimum threshold value above which controller sample is poisoned (0.0 - 1.0)
     */
    LwUFXP4_12             threshold0;
    /*!
     * Therm monitor index 1
     */
    LwU8                   thermMonIdx1;
    /*!
     * Minimum threshold value above which controller sample is poisoned (0.0 - 1.0)
     */
    LwUFXP4_12             threshold1;
    /*!
     * Minimum threshold value above which controller sample is poisoned (0.0 - 1.0)
     */
    LwUFXP4_12             hwSlowdownThreshold;
    /*!
     * Voltage offset range min (in uV)
     */
    LwS32                  voltOffsetMinuV;
    /*!
     * Voltage offset range max (in uV)
     */
    LwS32                  voltOffsetMaxuV;
    /*!
     * Timer value in ns which stores the total time hardware failsafe
     * events were enagaged.
     */
    LwU32                  prevHwfsEngageTimeNs;
    /*!
     * PMU timestamp value in ns which stores the total evalution time for
     * hardware failsafe events.
     */
    FLCN_TIMESTAMP         prevHwfsEvalTimeNs;
    /*!
     * Timer value in ns which stores the total time BA was engaged.
     */
    FLCN_TIMESTAMP         prevBaEngageTimeNs;
    /*!
     * PMU timestamp value in ns which stores the total evalution time for
     * BA.
     */
    FLCN_TIMESTAMP         prevBaEvalTimeNs;
    /*!
     * Timer value in ns which stores the total time droopy was engaged.
     */
    FLCN_TIMESTAMP         prevDroopyEngageTimeNs;
    /*!
     * PMU timestamp value in ns which stores the total evalution time for
     * droopy.
     */
    FLCN_TIMESTAMP         prevDroopyEvalTimeNs;    
} CLK_FREQ_CONTROLLER_20;

/*!
 * Set of CLK_FREQ_CONTROLLERS. Implements BOARDOBJGRP_E32.
 */
typedef struct
{
    /*!
     * CLK_FREQ_CONTROLLERS super class. Must always be the first element in the
     * structure.
     */
    CLK_FREQ_CONTROLLERS    super;
    /*!
     * Sleep aware sampling multipler for winIdle
     */
    LwU8                    lowSamplingMultiplier;
    /*!
     * Sampling period in milliseconds at which the voltage controllers will run.
     */
    LwU32                   samplingPeriodms;
} CLK_FREQ_CONTROLLERS_20;

/* ------------------------ External Definitions ---------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
// PERF INIT interfaces
FLCN_STATUS clkFreqControllersInit_20(void)
    GCC_ATTRIB_SECTION("imem_libPerfInit", "clkFreqControllersInit_20");

// Callback interfaces
OsTmrCallbackFunc (clkFreqControllerOsCallback_20)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllerOsCallback_20");

// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkFreqControllerGrpIfaceModel10ObjSet_20_SUPER)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkFreqControllerGrpIfaceModel10ObjSet_20_SUPER");
BoardObjIfaceModel10GetStatus   (clkFreqControllerIfaceModel10GetStatus_20_SUPER)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkFreqControllerIfaceModel10GetStatus_20_SUPER");
FLCN_STATUS     clkFreqControllerSamplePoisoned_20_SUPER(CLK_FREQ_CONTROLLER_20 *pFreqController20, LwBool *pbPoisoned)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllerSamplePoisoned_20_SUPER");

// CLK_FREQ_CONTROLLERS interfaces
FLCN_STATUS clkFreqControllersLoad_20(RM_PMU_CLK_LOAD *pClkLoad)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllersLoad_20");
FLCN_STATUS clkFreqControllerEval_20(CLK_FREQ_CONTROLLER_20 *pFreqController20)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllerEval_20");
FLCN_STATUS clkFreqControllerReset_20(CLK_FREQ_CONTROLLER_20 *pFreqController20)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllerReset_20");
FLCN_STATUS clkFreqControllerDisable_20(LwU32 ctrlId, LwU8 clientId, LwBool bDisable)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllerDisable_20");

/* ------------------------ Include Derived Types --------------------------- */
#include "clk_freq_controller_20_pi.h"

#endif // CLK_FREQ_CONTROLLER_20_H
