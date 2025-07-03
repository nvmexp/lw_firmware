/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_VOLT_CONTROLLER_H
#define CLK_VOLT_CONTROLLER_H

/*!
 * @file clk_volt_controller.h
 * @brief @copydoc clk_volt_controller.c
 */

/* ------------------------ Includes ---------------------------------------- */
#include "boardobj/boardobjgrp.h"
#include "lib/lib_fxp.h"
#include "lwostimer.h"
#include "lwostmrcallback.h"
#include "therm/objtherm.h"
#include "pmu_objperf.h"
#include "therm/thrmmon.h"
#include "pmu_objclk.h"
#include "volt/voltrail.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct CLK_VOLT_CONTROLLER CLK_VOLT_CONTROLLER, CLK_VOLT_CONTROLLER_BASE;

/* ------------------------ Macros ------------------------------------------ */
/*!
 * @brief accessor macro for CLK_VOLT_CONTROLLER object
 */
#define CLK_VOLT_CONTROLLERS_GET()        (&(Clk.voltControllers))

/*!
 * Macro to locate CLK_VOLT_CONTROLLERS BOARDOBJGRP.
 *
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_CLK_VOLT_CONTROLLER                         \
    (&(CLK_VOLT_CONTROLLERS_GET())->super.super)

/*!
 * @brief Is controller disabled?
 * TRUE If disabled at group level or instance level
 */
#define CLK_VOLT_CONTROLLER_IS_DISABLED(pVoltController)                      \
 ((Clk.voltControllers.disableClientsMask != 0) ||                            \
 ((pVoltController)->disableClientsMask != 0))

/*!
 * Macro to reset the sample max voltage offset of given input rail index
 *
 * @param[in] railIdx Voltage Rail index
 */
#define CLK_VOLT_CONTROLLERS_SAMPLE_MAX_VOLT_OFFSET_UV_RESET(railIdx)         \
    (CLK_VOLT_CONTROLLERS_GET())->sampleMaxVoltOffsetuV[(railIdx)] =          \
        RM_PMU_VOLT_VALUE_0V_IN_UV_SIGNED

/*!
 * Macro to query current volt offset (in uV)
 */
#define CLK_VOLT_CONTROLLER_VOLT_OFFSET_UV_GET(pVoltController)               \
    ((pVoltController)->voltOffsetuV)

/*!
 * Macro to set current volt offset (in uV)
 */
#define CLK_VOLT_CONTROLLER_VOLT_OFFSET_UV_SET(pVoltController, offsetuV)    \
    (((pVoltController)->voltOffsetuV) = (offsetuV))

/*!
 * Macro to query sampling period (ms)
 */
#define CLK_VOLT_CONTROLLER_SAMPLING_PERIOD_GET()                             \
    (Clk.voltControllers.samplingPeriodms)

/*!
 * Macro to query low sampling multiplier
 */
#define CLK_VOLT_CONTROLLER_LOW_SAMPLING_MULTIPLIER_GET()                     \
    (Clk.voltControllers.lowSamplingMultiplier)

/*!
 * Overlays for SENSED_VOLTAGE
 */
#define CLK_VOLT_CONTROLLER_SENSED_VOLTAGE_OVERLAYS                          \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf)                              \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perf)                              \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkVolt)                        \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVoltApi)

/* ------------------------ Defines ----------------------------------------- */
/*!
 * @interface CLK_VOLT_CONTROLLER
 *
 * Helper interface to evaluate the clock voltage controller
 *
 * @param[in] pVoltController   Voltage controller object pointer
 *
 * @return FLCN_OK
 *     Voltage offsets successfully updated
 * @return Other errors
 *     An unexpected error oclwrred during iteration.
 */
#define ClkVoltControllerEval(fname) FLCN_STATUS (fname) (CLK_VOLT_CONTROLLER *pVoltController)

/*!
 * @interface CLK_VOLT_CONTROLLER
 *
 * Helper interface to reset the clock voltage controller
 *
 * @param[in] pVoltController   Voltage controller object pointer
 *
 * @return FLCN_OK
 *     Voltage offsets successfully reset
 * @return Other errors
 *     An unexpected error oclwrred during iteration.
 */
#define ClkVoltControllerReset(fname) FLCN_STATUS (fname) (CLK_VOLT_CONTROLLER *pVoltController)

/* ------------------------ Datatypes --------------------------------------- */
/*!
 * Clock voltage controller (CLVC) structure.
 */
struct CLK_VOLT_CONTROLLER
{
    /*!
     * BOARDOBJ super class. Must always be the first element in the structure.
     */
    BOARDOBJ                         super;
    /*!
     * Index of the voltage rail this instance of CLK_VOLT_CONTROLLER will monitor
     */
    LwU8                             voltRailIdx;
    /*!
     * ADC operating mode @ref LW2080_CTRL_CLK_CLK_VOLT_CONTROLLER_ADC_MODE_<XYZ>
     */
    LwU8                             adcMode;
    /*!
     * Index into the Thermal Monitor Table. To be used for Droopy VR to poison
     * the sample per droopyPctMin value below.
     *
     * Invalid index value means that droopy support is not required and will
     * disable the poisoning behavior.
     */
    LwU8                             thermMonIdx;
    /*!
     * Minimum percentage of time droopy should engage to poison the sample.
     */
    LwUFXP4_12                       droopyPctMin;
    /*!
     * Voltage offset limit range min value in uV.
     */
    LwS32                            voltOffsetMinuV;
    /*!
     * Voltage offset limit range max value in uV.
     */
    LwS32                            voltOffsetMaxuV;
    /*!
     * [out] Voltage offset (in uV) after each iteration for this controller.
     */
    LwS32                            voltOffsetuV;
    /*!
     * Mask of ADC devices monitored by this controller instance
     */
    LwU32                            adcMask;
    /*!
     * Timer value in ns which stores the total time droopy was engaged.
     */
    FLCN_TIMESTAMP                   prevDroopyEngageTimeNs;
    /*!
     * PMU timestamp value in ns which stores the total evalution time for
     * droopy.
     */
    FLCN_TIMESTAMP                   prevDroopyEvalTimeNs;
    /*!
     * Holds sensed voltage read from ADCs
     */
    VOLT_RAIL_SENSED_VOLTAGE_DATA   *pSensedVoltData;
    /*!
     * Mask of clients requested to disable this controller.
     * @ref LW2080_CTRL_CLK_CLK_VOLT_CONTROLLER_CLIENT_ID_<xyz>.
     * If any client is set in the mask, this controller instance
     * will remain disabled
     */
    LwU32                            disableClientsMask;
    /*!
     * GR RG gating counter tracks GR-RG cycles since the rev sample
     */
    LwU32                            grRgGatingCount;
};

/*!
 * Set of CLK_VOLT_CONTROLLERS. Implements BOARDOBJGRP_E32.
 */
typedef struct
{
    /*!
     * BOARDOBJGRP_E32 super class. Must always be the first element in the
     * structure.
     */
    BOARDOBJGRP_E32    super;
    /*!
     * Sleep aware sampling multipler for winIdle
     */
    LwU8               lowSamplingMultiplier;
    /*!
     * Sampling period in milliseconds at which the voltage controllers will run.
     */
    LwU32              samplingPeriodms;
    /*!
     * Voltage offset threshold (in uV).
     * If offset <= threshold, SW will respect offset from frequency controller.
     */
    LwS32              voltOffsetThresholduV;
    /*!
     * Mask of clients requested to disable all controllers in this group.
     * @ref LW2080_CTRL_CLK_CLK_VOLT_CONTROLLERS_CLIENT_ID_<xyz>.
     * If any client bit is set in the mask, all controllers in the gorup
     * will remain disabled
     */
    LwU32              disableClientsMask;
    /*!
     * Per sample voltage offset for all voltage controllers
     */
    LwS32              sampleMaxVoltOffsetuV[LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS];
    /*!
     * Final voltage offset after each iteration for this controller.
     */
    LwS32              totalMaxVoltOffsetuV[LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS];
} CLK_VOLT_CONTROLLERS;

/* ------------------------ External Definitions ---------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10CmdHandler  (clkVoltControllerBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkVoltControllerBoardObjGrpIfaceModel10Set");
BoardObjGrpIfaceModel10CmdHandler  (clkVoltControllerBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkVoltControllerBoardObjGrpIfaceModel10GetStatus");

// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet      (clkVoltControllerGrpIfaceModel10ObjSet_SUPER)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkVoltControllerGrpIfaceModel10ObjSet_SUPER");
BoardObjIfaceModel10GetStatus          (clkVoltControllerIfaceModel10GetStatus_SUPER)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkVoltControllerIfaceModel10GetStatus_SUPER");

// CLK_VOLT_CONTROLLER interfaces
ClkVoltControllerEval  (clkVoltControllerEval)
    GCC_ATTRIB_SECTION("imem_libClkVoltController", "clkVoltControllerEval");
ClkVoltControllerEval  (clkVoltControllerEval_SUPER)
    GCC_ATTRIB_SECTION("imem_libClkVoltController", "clkVoltControllerEval_SUPER");
ClkVoltControllerReset (clkVoltControllerReset)
    GCC_ATTRIB_SECTION("imem_libClkVoltController", "clkVoltControllerReset");
OsTmrCallbackFunc      (clkVoltControllerOsCallback)
    GCC_ATTRIB_SECTION("imem_libClkVoltController", "clkVoltControllerOsCallback");
LwBool                  clkVoltControllerSamplePoisonedGrRg_SUPER(CLK_VOLT_CONTROLLER *pVoltController, LwBool *pbPoison)
    GCC_ATTRIB_SECTION("imem_libClkVoltController", "clkVoltControllerSamplePoisonedGrRg_SUPER");
FLCN_STATUS clkVoltControllerDisable(LwU8 clientId, LwU8 voltRailIdx, LwBool bDisable)
    GCC_ATTRIB_SECTION("imem_libClkVoltController", "clkVoltControllerDisable");
FLCN_STATUS clkVoltControllersDisable(LwU8 clientId, LwBool bDisable)
    GCC_ATTRIB_SECTION("imem_libClkVoltController", "clkVoltControllersDisable");
LwBool                  clkVoltControllerSamplePoisonedDroopy_SUPER(CLK_VOLT_CONTROLLER *pVoltController, LwBool *pbPoison)
    GCC_ATTRIB_SECTION("imem_libClkVoltController", "clkVoltControllerSamplePoisonedDroopy_SUPER");

// CLK_VOLT_CONTROLLERS interfaces
FLCN_STATUS             clkVoltControllersTotalMaxVoltOffsetuVUpdate(CLK_VOLT_CONTROLLER *pVoltController)
    GCC_ATTRIB_SECTION("imem_libClkVoltController", "clkFreqControllersTotalMaxVoltOffsetuVUpdate");

/* ------------------------ Include Derived Types --------------------------- */
#include "clk/clk_volt_controller_prop.h"

#endif // CLK_VOLT_CONTROLLER_H
