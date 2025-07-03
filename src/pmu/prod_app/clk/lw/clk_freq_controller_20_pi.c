/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_freq_controller_20_pi.c
 *
 * @brief Module managing all state related to the CLK_FREQ_CONTROLLER_20_PI
 * structure.
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
static LwBool s_clkFreqControllerSamplePoisoned_PI_20(CLK_FREQ_CONTROLLER_20_PI *pFreqControllerPI)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "s_clkFreqControllerSamplePoisoned_PI_20");

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_FREQ_CONTROLLER_20_PI class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkFreqControllerGrpIfaceModel10ObjSet_PI_20
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_CLK_CLK_FREQ_CONTROLLER_20_PI_BOARDOBJ_SET *pSet = NULL;
    CLK_FREQ_CONTROLLER_20_PI *pFreqControllerPI = NULL;
    FLCN_STATUS                status;

    // Call into CLK_FREQ_CONTROLLER super-constructor
    status = clkFreqControllerGrpIfaceModel10ObjSet_20_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkFreqControllerGrpIfaceModel10ObjSet_PI_20_exit;
    }
    pSet = (RM_PMU_CLK_CLK_FREQ_CONTROLLER_20_PI_BOARDOBJ_SET *)pBoardObjDesc;
    pFreqControllerPI = (CLK_FREQ_CONTROLLER_20_PI *)*ppBoardObj;

    // Copy the CLK_FREQ_CONTROLLER_PI-specific data.
    pFreqControllerPI->propGain       = pSet->propGain;
    pFreqControllerPI->integGain      = pSet->integGain;
    pFreqControllerPI->integDecay     = pSet->integDecay;
    pFreqControllerPI->freqHystPosMHz = pSet->freqHystPosMHz;
    pFreqControllerPI->freqHystNegMHz = pSet->freqHystNegMHz;
    pFreqControllerPI->poisonCounter  = 0;

clkFreqControllerGrpIfaceModel10ObjSet_PI_20_exit:
    return status;
}

/*!
 * Evaluates the PI 2.0 frequency controller.
 *
 * @param[in] pFreqControllerPI   PI frequency controller object pointer
 *
 * @return Status returned from functions called within.
 * @return FLCN_OK
 *      Successfully evaluated the frequency controller.
 */
FLCN_STATUS
clkFreqControllerEval_PI_20
(
    CLK_FREQ_CONTROLLER_20_PI *pFreqControllerPI
)
{
    FLCN_STATUS status       = FLCN_OK;
    LwS32       voltOffsetuV = RM_PMU_VOLT_VALUE_0V_IN_UV_SIGNED;
    LwU32       targetFreqkHz;
    LwU32       measuredFreqkHz;


    // Always init the values to zero.
    pFreqControllerPI->errorkHz     = 0;
    pFreqControllerPI->voltOffsetuV = RM_PMU_VOLT_VALUE_0V_IN_UV_SIGNED;

    // Get average target clock frequency (MHz) of the past cycle
    targetFreqkHz = clkFreqCountedAvgComputeFreqkHz(
            pFreqControllerPI->super.super.clkDomain,
            &pFreqControllerPI->targetSample);

    // Get average measured clock frequency (MHz) in the past cycle
    measuredFreqkHz = clkCntrAvgFreqKHz(pFreqControllerPI->super.super.clkDomain,
        pFreqControllerPI->super.super.partIdx,
        &pFreqControllerPI->measuredSample);

    // Bail early if sample is poisoned
    if (!s_clkFreqControllerSamplePoisoned_PI_20(pFreqControllerPI))
    {
        LwSFXP20_12 propTerm;
        LwSFXP20_12 integTerm;
        LwSFXP20_12 errorTotalMHz;
        LwSFXP20_12 errorLwrrentMHz;
        LwS32 errorkHz = targetFreqkHz - measuredFreqkHz;

        // Reset poison counter
        pFreqControllerPI->poisonCounter = 0;

        errorkHz = targetFreqkHz - measuredFreqkHz;

        // Cache the current error to be reported out to external tool
        pFreqControllerPI->errorkHz = errorkHz;

        // Return early if the error is within the +/- hysteresis.
        if ((errorkHz >= (pFreqControllerPI->super.super.freqHystNegMHz * 1000)) &&
            (errorkHz <= (pFreqControllerPI->super.super.freqHystPosMHz * 1000)))
        {
            status = FLCN_OK;
            goto clkFreqControllerEval_PI_20_exit;
        }

        // Error (MHz) of the current cycle colwerted to LwSFXP20_12
        errorLwrrentMHz = 
        LW_TYPES_S32_TO_SFXP_X_Y_SCALED(20, 12, errorkHz, 1000);

        // Proportional term (uV) = propGain (uV/MHz) * errorLwrrent (MHz).
        propTerm = multSFXP20_12(pFreqControllerPI->propGain, errorLwrrentMHz);

        // Total Error(MHz) = errorLwrrentMHz(MHz) + integDecay * prevError(MHz)
        errorTotalMHz = errorLwrrentMHz +
        multSFXP20_12(pFreqControllerPI->integDecay,
          pFreqControllerPI->errorPreviousMHz);

        // Cache the errorTotal in errorPreviousMHz to be used in the next cycle
        pFreqControllerPI->errorPreviousMHz = errorTotalMHz;

        // Integral term (uV) = integGain (uV/MHz) * errorTotal (MHz)
        integTerm = multSFXP20_12(pFreqControllerPI->integGain, errorTotalMHz);

        // Offset voltage (uV) for this iteration =
        // Proportional term (uV) + Integral term (uV)
        voltOffsetuV = LW_TYPES_SFXP_X_Y_TO_S32(20, 12, (propTerm + integTerm));
    }
    else
    {
        pFreqControllerPI->poisonCounter++;
    }

    //
    // Adjust the per sample delta with last applied final delta and trim the
    // new final delta with POR ranges.
    //
    voltOffsetuV += clkFreqControllersFinalVoltDeltauVGet();

    if (voltOffsetuV < pFreqControllerPI->super.voltOffsetMinuV)
    {
        voltOffsetuV = pFreqControllerPI->super.voltOffsetMinuV;
    }
    else if (voltOffsetuV > pFreqControllerPI->super.voltOffsetMaxuV)
    {
        voltOffsetuV = pFreqControllerPI->super.voltOffsetMaxuV;
    }

    // Store per sample voltage offset of this iteration
    pFreqControllerPI->voltOffsetuV =
        voltOffsetuV - clkFreqControllersFinalVoltDeltauVGet();

clkFreqControllerEval_PI_20_exit:
    return status;
}

/*!
 * Reset the PI 2.0 frequency controller
 *
 * @param[in] pFreqControllerPI   PI frequency controller object pointer
 *
 * @return FLCN_OK
 */
FLCN_STATUS
clkFreqControllerReset_PI_20
(
    CLK_FREQ_CONTROLLER_20_PI *pFreqControllerPI
)
{
    // Reset other parameters
    pFreqControllerPI->poisonCounter    = 0;
    pFreqControllerPI->errorPreviousMHz = LW_TYPES_S32_TO_SFXP_X_Y(20, 12, 0);
    pFreqControllerPI->voltOffsetuV     = RM_PMU_VOLT_VALUE_0V_IN_UV_SIGNED;
    pFreqControllerPI->errorkHz         = 0;

    return FLCN_OK;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
clkFreqControllerEntryIfaceModel10GetStatus_PI_20
(
    BOARDOBJ_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJ *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_CLK_CLK_FREQ_CONTROLLER_PI_BOARDOBJ_GET_STATUS *pGetStatus;
    CLK_FREQ_CONTROLLER_20_PI *pFreqControllerPI;
    FLCN_STATUS                status = FLCN_OK;

    // Call super-class implementation.
    status = clkFreqControllerIfaceModel10GetStatus_20_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto clkFreqControllerEntryIfaceModel10GetStatus_PI_20_exit;
    }
    pFreqControllerPI = (CLK_FREQ_CONTROLLER_20_PI *)pBoardObj;
    pGetStatus  = (RM_PMU_CLK_CLK_FREQ_CONTROLLER_PI_BOARDOBJ_GET_STATUS *)pPayload;

    // Set the query parameters.
    pGetStatus->poisonCounter = pFreqControllerPI->poisonCounter;
    pGetStatus->voltDeltauV   = pFreqControllerPI->voltOffsetuV;
    pGetStatus->errorkHz      = pFreqControllerPI->errorkHz;

clkFreqControllerEntryIfaceModel10GetStatus_PI_20_exit:
    return status;
}

/*!
 * Get the voltage offset for a PI 2.0 frequency controller.
 *
 * @param[in] pFreqControllerPI   PI frequency controller object pointer
 *
 * @return Voltage offset in uV.
 */
LwS32
clkFreqControllerVoltDeltauVGet_PI_20
(
    CLK_FREQ_CONTROLLER_20_PI *pFreqControllerPI
)
{
    return pFreqControllerPI->voltOffsetuV;
}

/*!
 * Set the voltage offset for a PI 2.0 frequency controller.
 *
 * @param[in] pFreqControllerPI   PI frequency controller object pointer
 * @param[in] voltOffsetuV        Voltage value to be set (in UV)
 *
 * Returns FLCN_OK
 */
FLCN_STATUS
clkFreqControllerVoltDeltauVSet_PI_20
(
    CLK_FREQ_CONTROLLER_20_PI *pFreqControllerPI,
    LwS32                      voltOffsetuV
)
{
    pFreqControllerPI->voltOffsetuV = voltOffsetuV;
    return FLCN_OK;
}

/* ------------------------- Private Functions ----------------------------- */
/*!
 * Check if the frequency controller sample is poisoned.
 * 
 * @param[in] pFreqControllerPI   PI frequency controller object pointer
 *
 * @return LW_TRUE      The sample is poisoned.
 * @return LW_FALSE     Otherwise.
 */
static LwBool
s_clkFreqControllerSamplePoisoned_PI_20
(
    CLK_FREQ_CONTROLLER_20_PI *pFreqControllerPI
)
{
    CLK_FREQ_CONTROLLER_20 *pFreqController =
        (CLK_FREQ_CONTROLLER_20 *)pFreqControllerPI;
    LwBool                  bPoisoned       = LW_FALSE;
    FLCN_STATUS             status          = FLCN_OK;

    status = clkFreqControllerSamplePoisoned_20_SUPER(pFreqController, &bPoisoned);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_clkFreqControllerSamplePoisoned_PI_20_exit;
    }

s_clkFreqControllerSamplePoisoned_PI_20_exit:
    return bPoisoned;
}
