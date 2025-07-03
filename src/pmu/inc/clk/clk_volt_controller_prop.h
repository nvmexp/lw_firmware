/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef _CLK_VOLT_CONTROLLER_PROP_H_
#define _CLK_VOLT_CONTROLLER_PROP_H_

/*!
 * @file clk_volt_controller_prop.h
 * @brief @copydoc clk_volt_controller_prop.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_volt_controller.h"
#include "lib/lib_avg.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Clock PROP voltage controller structure.
 */
typedef struct
{
    /*!
     * CLK_VOLT_CONTROLLER super class. Must always be the first element
     * in the structure.
     */
    CLK_VOLT_CONTROLLER    super;
    /*!
     * Absolute value of Positive Voltage Hysteresis in uV (0 => no hysteresis).
     * (hysteresis to apply when voltage has positive delta)
     */
    LwS32                  voltHysteresisPositive;
    /*!
     * Absolute value of Negative Voltage Hysteresis in uV (0 => no hysteresis).
     * (hysteresis to apply when voltage has negative delta)
     */
    LwS32                  voltHysteresisNegative;
    /*!
     * Proportional gain for this voltage controller in uV/MHz
     */
    LwSFXP20_12            propGain;
    /*!
     * Voltage Error (in uV) for the current cycle
     */
    LwS32                  erroruV;
    /*!
     * Sensed voltage
     */
    LwU32                  senseduV;
    /*!
     * Measured voltage
     */
    LwU32                  measureduV;
    /*!
     * Counter sample to tracks of target avg voltage per controller cycle
     */
    COUNTER_SAMPLE         voltuVTargetSample;
    /*!
     * Poison counter counting conselwtive poison request
     */
    LwU32                  poisonCounter;
} CLK_VOLT_CONTROLLER_PROP;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkVoltControllerGrpIfaceModel10ObjSet_PROP)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkVoltControllerGrpIfaceModel10ObjSet_PROP");
BoardObjIfaceModel10GetStatus     (clkVoltControllerEntryIfaceModel10GetStatus_PROP)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkVoltControllerEntryIfaceModel10GetStatus_PROP");

// CLK_VOLT_CONTROLLER_PROP interfaces
ClkVoltControllerEval  (clkVoltControllerEval_PROP)
    GCC_ATTRIB_SECTION("imem_libClkVoltController", "clkVoltControllerEval_PROP");
ClkVoltControllerReset (clkVoltControllerReset_PROP)
    GCC_ATTRIB_SECTION("imem_libClkVoltController", "clkVoltControllerReset_PROP");
LwBool                  clkVoltControllerSamplePoisoned_PROP(CLK_VOLT_CONTROLLER_PROP *pVoltControllerPROP)
    GCC_ATTRIB_SECTION("imem_libClkVoltController", "clkVoltControllerSamplePoisoned_PROP");
LwBool                  clkVoltControllerSamplePoisonedGrRg_PROP(CLK_VOLT_CONTROLLER_PROP *pVoltControllerPROP)
    GCC_ATTRIB_SECTION("imem_libClkVoltController", "clkVoltControllerSamplePoisonedGrRg_PROP");
LwBool                  clkVoltControllerSamplePoisonedDroopy_PROP(CLK_VOLT_CONTROLLER_PROP *pVoltControllerPROP)
    GCC_ATTRIB_SECTION("imem_libClkVoltController", "clkVoltControllerSamplePoisonedDroopy_PROP");

/* ------------------------ Include Derived Types -------------------------- */

#endif // _CLK_VOLT_CONTROLLER_PROP_H_
