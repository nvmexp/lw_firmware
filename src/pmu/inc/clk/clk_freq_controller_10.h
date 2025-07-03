/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_FREQ_CONTROLLER_10_H
#define CLK_FREQ_CONTROLLER_10_H

/*!
 * @file clk_freq_controller_10.h
 * @brief @copydoc clk_freq_controller_10.c
 */

/* ------------------------ Includes ---------------------------------------- */
#include "lwostmrcallback.h"
#include "clk/clk_freq_controller.h"

/* ------------------------ Macros ------------------------------------------ */
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
} CLK_FREQ_CONTROLLER_10;

/*!
 * Set of CLK_FREQ_CONTROLLERS. Implements BOARDOBJGRP_E32.
 */
typedef struct
{
    /*!
     * CLK_FREQ_CONTROLLERS super class. Must always be the first element in the
     * structure.
     */
    CLK_FREQ_CONTROLLERS super;
} CLK_FREQ_CONTROLLERS_10;

/* ------------------------ External Definitions ---------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
// PERF INIT interfaces
FLCN_STATUS clkFreqControllersInit_10(void)
    GCC_ATTRIB_SECTION("imem_libPerfInit", "clkFreqControllersInit_10");

// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkFreqControllerGrpIfaceModel10ObjSet_10_SUPER)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkFreqControllerGrpIfaceModel10ObjSet_10_SUPER");
BoardObjIfaceModel10GetStatus   (clkFreqControllerIfaceModel10GetStatus_10_SUPER)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkFreqControllerIfaceModel10GetStatus_10_SUPER");

// CLK_FREQ_CONTROLLERS interfaces
FLCN_STATUS clkFreqControllersLoad_10(RM_PMU_CLK_LOAD *pClkLoad)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllersLoad_10");
FLCN_STATUS clkFreqControllersFinalVoltDeltauVSet(LwS32)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllersFinalVoltDeltauVSet");
FLCN_STATUS clkFreqControllersLoad_WRAPPER(LwBool bLoad)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllersLoad_WRAPPER");
OsTmrCallbackFunc (clkFreqControllerOsCallback_10)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllerOsCallback_10");

// CLK_FREQ_CONTROLLER interfaces
FLCN_STATUS clkFreqControllerEval_10(CLK_FREQ_CONTROLLER_10 *pFreqController)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllerEval_10");
FLCN_STATUS clkFreqControllerReset_10(CLK_FREQ_CONTROLLER_10 *pFreqController)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllerReset_10");
FLCN_STATUS clkFreqControllerDisable_10(LwU32 ctrlId, LwU8 clientId, LwBool bDisable)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllerDisable_10");

/* ------------------------ Include Derived Types --------------------------- */
#include "clk_freq_controller_10_pi.h"

#endif // CLK_FREQ_CONTROLLER_10_H
