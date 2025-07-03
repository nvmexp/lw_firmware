/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_FREQ_CONTROLLER_H
#define CLK_FREQ_CONTROLLER_H

/*!
 * @file clk_freq_controller.h
 * @brief @copydoc clk_freq_controller.c
 */

/* ------------------------ Includes ---------------------------------------- */
#include "pmu_objclk.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct CLK_FREQ_CONTROLLER CLK_FREQ_CONTROLLER, CLK_FREQ_CONTROLLER_BASE;

/* ------------------------ Macros ------------------------------------------ */
/*!
 * Helper macro to return a pointer to the CLK_FREQ_CONTROLLERS object.
 *
 * @return Pointer to the CLK_FREQ_CONTROLLERS object.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER))
#define CLK_CLK_FREQ_CONTROLLERS_GET()                                        \
    (Clk.pFreqControllers)
#else
#define CLK_CLK_FREQ_CONTROLLERS_GET()                                        \
    ((CLK_FREQ_CONTROLLERS *)NULL)
#endif

/*!
 * Macro to locate CLK_FREQ_CONTROLLERS BOARDOBJGRP.
 *
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_CLK_FREQ_CONTROLLER                         \
    (&(CLK_CLK_FREQ_CONTROLLERS_GET())->super.super)

/*!
 * @brief       Accesor for a CLK_FREQ_CONTROLLER BOARDOBJ by BOARDOBJGRP index.
 *
 * @param[in]   _pstateIdx  BOARDOBJGRP index for a CLK_FREQ_CONTROLLER BOARDOBJ.
 *
 * @return      Pointer to a CLK_FREQ_CONTROLLER object at the provided BOARDOBJGRP index.
 *
 * @memberof    CLK_FREQ_CONTROLLER
 *
 * @public
 */
#define CLK_FREQ_CONTROLLER_GET(_idx)                                                  \
    (BOARDOBJGRP_OBJ_GET(CLK_FREQ_CONTROLLER, (_idx)))

/*!
 * Helper macro to retrieve the version @ref LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_VERSION_<xyz>
 * of a CLK_FREQ_CONTROLLERS.
 *
 *
 * @return @ref CLK_FREQ_CONTROLLERS::version
 */
#define clkFreqControllersVersionGet()                                        \
    CLK_CLK_FREQ_CONTROLLERS_GET()->version

/*!
 * Macro to check if continuous mode is enabled.
 * @details On Pascal Gpus, frequency controllers are disabled and reset during
 *          VF switches and that works because controllers are enabled only on
 *          compute and auto SKUs which do not have frequent VF switches
 *          otherwise controllers would be ineffective.
 *          On Volta+ Gpus, the POR is to enable frequency controllers on all
 *          SKUs including VdChip where VF switches are frequent hence the
 *          requirement to continuously run controllers(i.e. without reset)
 *          during VF switches with special case handling when callback function
 *          collides with VF switch @ref bCallbackSkipped.
 */
#define CLK_FREQ_CONTROLLER_IS_CONT_MODE_ENABLED                              \
    ((CLK_CLK_FREQ_CONTROLLERS_GET())->bContinuousMode &&                                 \
     PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_CONTINUOUS_MODE_SUPPORTED))

/*!
 * Macro to query sampling period (ms)
 */
#define CLK_FREQ_CONTROLLER_SAMPLING_PERIOD_GET()                             \
    ((CLK_CLK_FREQ_CONTROLLERS_GET())->samplingPeriodms)

/*!
 * Macro to query low sampling multiplier
 */
#define CLK_FREQ_CONTROLLER_LOW_SAMPLING_MULTIPLIER_GET()                     \
    (CLK_FREQ_CONTROLLER_LOW_SAMPLING_MULTIPLIER)


/* ------------------------ Defines ----------------------------------------- */
/*!
 * @brief   List of an overlay descriptors required by a freq. controller code.
 *
 * @note    Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM after
 *          updates to this macro. Failure to do so might result in falcon halt.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER))
    #if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_COUNTED_AVG))
    #define OSTASK_OVL_DESC_DEFINE_LIB_CLK_FREQ_CONTROLLER                    \
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkFreqController)           \
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkFreqCountedAvg)           \
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clkControllers)                 \
        OSTASK_OVL_DESC_DEFINE_CLK_COUNTER
    #else
    #define OSTASK_OVL_DESC_DEFINE_LIB_CLK_FREQ_CONTROLLER                    \
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkFreqController)           \
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clkControllers)                 \
        OSTASK_OVL_DESC_DEFINE_CLK_COUNTER
    #endif
#else
    #define OSTASK_OVL_DESC_DEFINE_LIB_CLK_FREQ_CONTROLLER                    \
        OSTASK_OVL_DESC_ILWALID
#endif

/*
 * Low sampling multiplier to be used during sleep mode.
 */
#define CLK_FREQ_CONTROLLER_LOW_SAMPLING_MULTIPLIER                         10U
/* ------------------------ Datatypes --------------------------------------- */
/*!
 * Clock frequency controller structure.
 */
struct CLK_FREQ_CONTROLLER
{
    /*!
     * BOARDOBJ super class. Must always be the first element in the structure.
     */
    BOARDOBJ    super;
    /*!
     * Frequency controller ID @ref LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_ID_<xyz>
     */
    LwU8        controllerId;
    /*!
     * Mode for the frequency from partitions for a clock domain.
     * @ref LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_PARTS_FREQ_MODE_<xyz>.
     */
    LwU8        partsFreqMode;
    /*!
     * Partition index @ref LW2080_CTRL_CLK_DOMAIN_PART_IDX_<xyz>.
     */
    LwU8        partIdx;
    /*!
     * Keep the controller disabled
     */
    LwBool      bDisable;
    /*!
     * Clock domain @ref LW2080_CTRL_CLK_DOMAIN_<xyz>.
     */
    LwU32       clkDomain;
    /*!
     * Mask of clients requested to disable this controller.
     * @ref LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_CLIENT_ID_<xyz>.
     */
    LwU32       disableClientsMask;
    /*!
     * Frequency Cap in MHZ to be applied when V/F point is above
     * Noise Unaware Vmin
     */
    LwS16       freqCapNoiseUnawareVminAbove;
    /*!
     * Frequency Cap in MHz to be applied when V/F point is below
     * Noise Unaware Vmin
     */
    LwS16       freqCapNoiseUnawareVminBelow;
    /*!
     * Absolute value of Positive Frequency Hysteresis in MHz (0 => no hysteresis).
     * (hysteresis to apply when frequency has positive delta)
     */
    LwS16       freqHystPosMHz;
    /*!
     * Absolute value of Negative Frequency Hysteresis in MHz (0 => no hysteresis).
     * (hysteresis to apply when frequency has negative delta)
     */
    LwS16       freqHystNegMHz;
    /*!
     * Pointer to the NAFLL device that indexes into this controller
     */
    CLK_NAFLL_DEVICE *pNafllDev;

};

/*!
 * Set of CLK_FREQ_CONTROLLERS. Implements BOARDOBJGRP_E32.
 */
typedef struct
{
    /*!
     * BOARDOBJGRP_E32 super class. Must always be the first element in the
     * structure.
     */
    BOARDOBJGRP_E32 super;
    /*!
     * Frequency Controller structure version - @ref LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_VERSION_<xyz>
     */
    LwU8            version;
    /*!
     * Sampling period at which the frequency controllers will run.
     */
    LwU32           samplingPeriodms;
    /*!
     * Final voltage delta applied after each iteration.
     * This is the max voltage delta from all controllers.
     */
    LwS32           finalVoltDeltauV;
    /*!
     * Voltage policy table index required for applying voltage delta.
     */
    LwU8            voltPolicyIdx;
    /*!
     * Boolean to indicate if continuous mode is enabled.
     */
    LwBool          bContinuousMode;
    /*!
     * Boolean to indicate if callback function was skipped during VF switch.
     * The OS callback function may collide with VF switch and that may skip
     * the controller evaluation all time in the worst case.
     * This flag is set when collision happens and unset when we forcefully
     * call the callback function after completion of VF switch.
     */
    LwBool          bCallbackSkipped;
    /*!
     * Cached copy of the MSCG gating count
     */
    LwU32           msGatingCount;
} CLK_FREQ_CONTROLLERS;

/* ------------------------ External Definitions ---------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
// PERF INIT interfaces
FLCN_STATUS clkFreqControllersInit (void)
    GCC_ATTRIB_SECTION("imem_libPerfInit", "clkFreqControllersInit");

// Callback interfaces
OsTimerCallback (clkFreqControllerCallback)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllerCallback")
    GCC_ATTRIB_USED();
OsTmrCallbackFunc (clkFreqControllerOsCallback)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllerOsCallback")
    GCC_ATTRIB_USED();


// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10CmdHandler  (clkFreqControllerBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkFreqControllerBoardObjGrpIfaceModel10Set");

// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkFreqControllerGrpIfaceModel10ObjSet_SUPER)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkFreqControllerGrpIfaceModel10ObjSet_SUPER");
FLCN_STATUS     clkFreqControllerEval_SUPER(CLK_FREQ_CONTROLLER *pFreqController)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllerEval_SUPER");

// CLK_FREQ_CONTROLLERS interfaces
BoardObjGrpIfaceModel10CmdHandler (clkFreqControllerBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkFreqControllerBoardObjGrpIfaceModel10GetStatus");
BoardObjIfaceModel10GetStatus   (clkFreqControllerIfaceModel10GetStatus_SUPER)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkFreqControllerIfaceModel10GetStatus_SUPER");
FLCN_STATUS     clkFreqControllersLoad(RM_PMU_CLK_LOAD *pClkLoad)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllersLoad");
void            clkFreqControllersFinalVoltDeltauVUpdate(void)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllersFinalVoltDeltauVUpdate");
LwS32           clkFreqControllersFinalVoltDeltauVGet(void)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllersFinalVoltDeltauVGet");

// CLK_FREQ_CONTROLLER interfaces
FLCN_STATUS clkFreqControllerEval(CLK_FREQ_CONTROLLER *pFreqController)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllerEval");
FLCN_STATUS clkFreqControllerReset(CLK_FREQ_CONTROLLER *pFreqController)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllerReset");
FLCN_STATUS clkFreqControllerDisable(LwU32 ctrlId, LwU8 clientId, LwBool bDisable)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllerDisable");

LwS32       clkFreqControllerVoltDeltauVGet(CLK_FREQ_CONTROLLER *pFreqController)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllerVoltDeltauVGet");
FLCN_STATUS clkFreqControllerVoltDeltauVSet(CLK_FREQ_CONTROLLER *pFreqController, LwS32 voltOffsetuV)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "clkFreqControllerVoltDeltauVSet");

/* ------------------------ Include Derived Types --------------------------- */
#include "clk/clk_freq_controller_10.h"
#include "clk/clk_freq_controller_20.h"

#endif // CLK_FREQ_CONTROLLER_H
