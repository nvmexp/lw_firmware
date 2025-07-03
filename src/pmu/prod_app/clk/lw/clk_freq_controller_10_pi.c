/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_freq_controller_10_pi.c
 *
 * @brief Module managing all state related to the CLK_FREQ_CONTROLLER_10_PI
 * structure.
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "therm/objtherm.h"
#include "therm/thrmmon.h"
#include "lib/lib_fxp.h"
#include "lib_lw64.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
static LwBool s_clkFreqControllerSamplePoisoned_PI(CLK_FREQ_CONTROLLER_10_PI *pFreqControllerPI)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "s_clkFreqControllerSamplePoisoned_PI");
static LwBool s_clkFreqControllerSamplePoisonedHwFs_PI(CLK_FREQ_CONTROLLER_10_PI *pFreqControllerPI)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "s_clkFreqControllerSamplePoisonedHwFs_PI");
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_BA_SUPPORTED)
static LwBool _clkFreqControllerSamplePoisonedBA_PI(CLK_FREQ_CONTROLLER_10_PI *pFreqControllerPI)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "_clkFreqControllerSamplePoisonedBA_PI");
#endif
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_DROOPY_SUPPORTED)
static LwBool s_clkFreqControllerSamplePoisonedDroopy_PI(CLK_FREQ_CONTROLLER_10_PI *pFreqControllerPI)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "s_clkFreqControllerSamplePoisonedDroopy_PI");
#endif

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_FREQ_CONTROLLER_10_PI class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkFreqControllerGrpIfaceModel10ObjSet_PI_10
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_CLK_CLK_FREQ_CONTROLLER_10_PI_BOARDOBJ_SET *pSet = NULL;
    CLK_FREQ_CONTROLLER_10_PI *pFreqControllerPI = NULL;
    FLCN_STATUS             status;

    // Call into CLK_FREQ_CONTROLLER_10 super-constructor
    status = clkFreqControllerGrpIfaceModel10ObjSet_10_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkFreqControllerGrpIfaceModel10ObjSet_PI_10_exit;
    }
    pSet = (RM_PMU_CLK_CLK_FREQ_CONTROLLER_10_PI_BOARDOBJ_SET *)pBoardObjDesc;
    pFreqControllerPI = (CLK_FREQ_CONTROLLER_10_PI *)*ppBoardObj;

    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        if (pSet->voltDeltaMin > pSet->voltDeltaMax)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto clkFreqControllerGrpIfaceModel10ObjSet_PI_10_exit;
        }
    }

    // Copy the CLK_FREQ_CONTROLLER_10_PI-specific data.
    pFreqControllerPI->propGain         = pSet->propGain;
    pFreqControllerPI->integGain        = pSet->integGain;
    pFreqControllerPI->integDecay       = pSet->integDecay;
    pFreqControllerPI->voltDeltaMin     = pSet->voltDeltaMin;
    pFreqControllerPI->voltDeltaMax     = pSet->voltDeltaMax;
    pFreqControllerPI->slowdownPctMin   = pSet->slowdownPctMin;
    pFreqControllerPI->bPoison          = pSet->bPoison;
    pFreqControllerPI->poisonCounter    = 0;

    // Only if BA feature is enabled.
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_BA_SUPPORTED)
    pFreqControllerPI->baThermMonIdx    = pSet->baThermMonIdx;
    pFreqControllerPI->baPctMin         = pSet->baPctMin;
#endif

    // Only if droopy feature is enabled.
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_DROOPY_SUPPORTED)
    pFreqControllerPI->thermMonIdx      = pSet->thermMonIdx;
    pFreqControllerPI->droopyPctMin     = pSet->droopyPctMin;
#endif

clkFreqControllerGrpIfaceModel10ObjSet_PI_10_exit:
    return status;
}

/*!
 * Evaluates the PI frequency controller.
 *
 * @param[in] pFreqControllerPI   PI frequency controller object pointer
 *
 * @return Status returned from functions called within.
 * @return FLCN_OK
 *      Successfully evaluated the frequency controller.
 */
FLCN_STATUS
clkFreqControllerEval_PI_10
(
    CLK_FREQ_CONTROLLER_10_PI *pFreqControllerPI
)
{
    FLCN_STATUS status                  = FLCN_OK;
    LwS32       voltDeltauV             = 0;
    LwBool      bFreqCountedAvgCompute  = LW_FALSE;
    LwU32       targetFreqkHz;
    LwU32       measuredFreqkHz;
    OSTASK_OVL_DESC ovlDescList[] = {
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLKS_ON_PMU))
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clk3x)
#else
        OSTASK_OVL_DESC_ILWALID
#endif
    };

    // Always init the values to zero.
    pFreqControllerPI->errorkHz    = 0;
    pFreqControllerPI->voltDeltauV = 0;

    //
    // Compute average counted frequnecy if
    // Case 1 : Continuous mode is supported and enabled i.e. on Volta+ gpus.
    // Case 2 : Continuous mode is not supported but average counted frequency
    //          feature is enabled i.e. on Pascal gpus.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_CONTINUOUS_MODE_SUPPORTED))
    {
        bFreqCountedAvgCompute = (CLK_CLK_FREQ_CONTROLLERS_GET())->bContinuousMode;
    }
    else
    {
        bFreqCountedAvgCompute = PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_COUNTED_AVG);
    }

    if (bFreqCountedAvgCompute)
    {
        // Get average target clock frequency (MHz) of the past cycle
        targetFreqkHz = clkFreqCountedAvgComputeFreqkHz(
                pFreqControllerPI->super.super.clkDomain,
                &pFreqControllerPI->targetSample);
    }
    else
    {
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
        // Get the latest target frequency
        targetFreqkHz =
            clkNafllFreqMHzGet(pFreqControllerPI->super.super.pNafllDev) * 1000;
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }

    // Get average measured clock frequency (MHz) in the past cycle
    measuredFreqkHz = clkCntrAvgFreqKHz(pFreqControllerPI->super.super.clkDomain,
        pFreqControllerPI->super.super.partIdx,
        &pFreqControllerPI->measuredSample);

    // Compute voltage delta only if this sample is not poisoned.
    if (!s_clkFreqControllerSamplePoisoned_PI(pFreqControllerPI))
    {
        LwSFXP20_12 propTerm;
        LwSFXP20_12 integTerm;
        LwSFXP20_12 errorTotalMHz;
        LwSFXP20_12 errorLwrrentMHz;
        LwS32 errorkHz = targetFreqkHz - measuredFreqkHz;

        // Reset poison counter
        pFreqControllerPI->poisonCounter = 0;

        // Cache the current error to be reported out to external tool
        pFreqControllerPI->errorkHz = errorkHz;

        // Return early if the error is within the +/- hysteresis.
        if ((errorkHz >= (pFreqControllerPI->super.super.freqHystNegMHz * 1000)) &&
            (errorkHz <= (pFreqControllerPI->super.super.freqHystPosMHz * 1000)))
        {
            goto clkFreqControllerEval_PI_10_exit;
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

        // Delta voltage (uV) for this iteration =
        // Proportional term (uV) + Integral term (uV)
        voltDeltauV = LW_TYPES_SFXP_X_Y_TO_S32(20, 12, (propTerm + integTerm));
    }
    else
    {
        pFreqControllerPI->poisonCounter++;
    }

    //
    // Adjust the per sample delta with last applied final delta and trim the
    // new final delta with POR ranges.
    //
    voltDeltauV += clkFreqControllersFinalVoltDeltauVGet();

    if (voltDeltauV < pFreqControllerPI->voltDeltaMin)
    {
        voltDeltauV = pFreqControllerPI->voltDeltaMin;
    }
    else if (voltDeltauV > pFreqControllerPI->voltDeltaMax)
    {
        voltDeltauV = pFreqControllerPI->voltDeltaMax;
    }

    // Store per sample voltage delta.
    pFreqControllerPI->voltDeltauV =
        voltDeltauV - clkFreqControllersFinalVoltDeltauVGet();

clkFreqControllerEval_PI_10_exit:
    return status;
}

/*!
 * Reset the PI frequency controller
 *
 * @param[in] pFreqControllerPI   PI frequency controller object pointer
 *
 * @return FLCN_OK
 */
FLCN_STATUS
clkFreqControllerReset_PI_10
(
    CLK_FREQ_CONTROLLER_10_PI *pFreqControllerPI
)
{
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_COUNTED_AVG)
    {
        // Update target frequency counter info
        (void)clkFreqCountedAvgComputeFreqkHz(pFreqControllerPI->super.super.clkDomain,
                                              &pFreqControllerPI->targetSample);
    }
#endif

    // Update measured frequency counter info
    (void)clkCntrAvgFreqKHz(pFreqControllerPI->super.super.clkDomain,
        pFreqControllerPI->super.super.partIdx,
        &pFreqControllerPI->measuredSample);

    // Reset other parameters
    pFreqControllerPI->errorPreviousMHz = LW_TYPES_S32_TO_SFXP_X_Y(20, 12, 0);
    pFreqControllerPI->voltDeltauV      = 0;
    pFreqControllerPI->errorkHz         = 0;
    pFreqControllerPI->poisonCounter    = 0;

    return FLCN_OK;
}

/*!
 * Get the voltage delta for a PI 1.0 frequency controller.
 *
 * @param[in] pFreqControllerPI   PI frequency controller object pointer
 *
 * @return Voltage delta in uV.
 */
LwS32
clkFreqControllerVoltDeltauVGet_PI_10
(
    CLK_FREQ_CONTROLLER_10_PI *pFreqControllerPI
)
{
    return pFreqControllerPI->voltDeltauV;
}

/*!
 * Set the voltage offset for a PI 1.0 frequency controller.
 *
 * @param[in] pFreqControllerPI   PI frequency controller object pointer
 * @param[in] voltDeltauV        Voltage value to be set (in UV)
 *
 * Returns FLCN_OK
 */
FLCN_STATUS
clkFreqControllerVoltDeltauVSet_PI_10
(
    CLK_FREQ_CONTROLLER_10_PI *pFreqControllerPI,
    LwS32                      voltDeltauV
)
{
    pFreqControllerPI->voltDeltauV = voltDeltauV;
    return FLCN_OK;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
clkFreqControllerEntryIfaceModel10GetStatus_PI_10
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_CLK_CLK_FREQ_CONTROLLER_PI_BOARDOBJ_GET_STATUS *pGetStatus;
    CLK_FREQ_CONTROLLER_10_PI *pFreqControllerPI;
    FLCN_STATUS                                            status;

    // Call super-class implementation.
    status = clkFreqControllerIfaceModel10GetStatus_10_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto clkFreqControllerEntryIfaceModel10GetStatus_PI_10_exit;
    }
    pFreqControllerPI = (CLK_FREQ_CONTROLLER_10_PI *)pBoardObj;
    pGetStatus  = (RM_PMU_CLK_CLK_FREQ_CONTROLLER_PI_BOARDOBJ_GET_STATUS *)pPayload;

    // Set the query parameters.
    pGetStatus->poisonCounter = pFreqControllerPI->poisonCounter;
    pGetStatus->voltDeltauV   = pFreqControllerPI->voltDeltauV;
    pGetStatus->errorkHz      = pFreqControllerPI->errorkHz;

clkFreqControllerEntryIfaceModel10GetStatus_PI_10_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * Check if the frequency controller sample is poisoned.
 * 
 * @param[in] pFreqControllerPI   PI frequency controller object pointer
 *
 * @return LW_TRUE      The sample is poisoned.
 * @return LW_FALSE     Otherwise.
 */
static LwBool
s_clkFreqControllerSamplePoisoned_PI
(
    CLK_FREQ_CONTROLLER_10_PI *pFreqControllerPI
)
{
    LwBool bPoisonedHwfs;
    LwBool bPoisonedBA     = LW_FALSE;
    LwBool bPoisonedDroopy = LW_FALSE;

    // Check if sample is poisoned because of HW failsafe events (excluding BA).
    bPoisonedHwfs = s_clkFreqControllerSamplePoisonedHwFs_PI(pFreqControllerPI);

    // Check if sample is poisoned because of BA slowdown.
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_BA_SUPPORTED)
    bPoisonedBA = _clkFreqControllerSamplePoisonedBA_PI(pFreqControllerPI);
#endif

    // Check if sample is poisoned because of droopy.
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_DROOPY_SUPPORTED)
    bPoisonedDroopy = s_clkFreqControllerSamplePoisonedDroopy_PI(pFreqControllerPI);
#endif

    return (bPoisonedHwfs || bPoisonedBA || bPoisonedDroopy);
}

/*!
 * Check if the frequency controller sample is poisoned because of 
 * hardware failsafe events engaged (excluding BA).
 *
 * The sample is poisoned if any kind of thermal slowdowns are engaged
 * for more than the percentage limit.
 * The thermal violation timer tracks the total time thermal slowdowns
 * are engaged since boot so we take the difference between two samples
 * to get the enagaged time for controller sampling period, colwert into
 * percentage and check with the percentage limit.
 * 
 * @param[in] pFreqControllerPI   PI frequency controller object pointer
 *
 * @return LW_TRUE - The sample is poisoned.
 * @return LW_FALSE - Otherwise.
 */
static LwBool
s_clkFreqControllerSamplePoisonedHwFs_PI
(
    CLK_FREQ_CONTROLLER_10_PI *pFreqControllerPI
)
{
    FLCN_TIMESTAMP  timeNowns;
    FLCN_TIMESTAMP  elapsedTimens;
    LwU32           slowdownTimeNowns;
    LwU32           elapsedSlowdownTimens;
    LwU8            pctVal;
    LwBool          bPoison = LW_FALSE;

    appTaskCriticalEnter();
    {
        // Get current timer values.
        osPTimerTimeNsLwrrentGet(&timeNowns);
        (void)thermEventViolationGetTimer32(RM_PMU_THERM_EVENT_META_ANY_HW_FS,
                                            &slowdownTimeNowns);
    }
    appTaskCriticalExit();

    // Compute elapsed time.
    lw64Sub(&elapsedTimens.data, &timeNowns.data,
        &pFreqControllerPI->prevHwfsEvalTimeNs.data);

    elapsedSlowdownTimens = slowdownTimeNowns -
        pFreqControllerPI->prevHwfsEngageTimeNs;

    // cache the current time for the next iteration
    pFreqControllerPI->prevHwfsEvalTimeNs.data  = timeNowns.data;

    // cache the current slowdown time for the next iteration
    pFreqControllerPI->prevHwfsEngageTimeNs     = slowdownTimeNowns;

    // Colwert time to percentage.
    elapsedSlowdownTimens *= 100;

    PMU_HALT_COND(elapsedTimens.parts.lo != 0);
    pctVal = LW_UNSIGNED_ROUNDED_DIV(elapsedSlowdownTimens,
        elapsedTimens.parts.lo);

    //
    // Poison the sample if slowdown engaged for more than the percentage
    // time limit.
    // Slowdown is enabled only on GPCCLK/GPC2CLK, for non-GPC domains check
    // the flag.
    //
    if ((pctVal > pFreqControllerPI->slowdownPctMin) &&
        ((pFreqControllerPI->super.super.clkDomain == CLK_DOMAIN_MASK_GPC) ||
         (pFreqControllerPI->bPoison &&
          (pFreqControllerPI->super.super.clkDomain != CLK_DOMAIN_MASK_GPC))))
    {
        bPoison = LW_TRUE;
    }

    return bPoison;
}

#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_BA_SUPPORTED)
/*!
 * Check if the frequency controller sample is poisoned because of
 * BA EDPp engaged.
 *
 * The sample is poisoned if BA was enagaged for more time than the
 * minimum limit @ref baPctMin.
 *
 * @param[in] pFreqControllerPI   PI frequency controller object pointer
 *
 * @return LW_TRUE - The sample is poisoned.
 * @return LW_FALSE - Otherwise.
 */
static LwBool
_clkFreqControllerSamplePoisonedBA_PI
(
    CLK_FREQ_CONTROLLER_10_PI *pFreqControllerPI
)
{
    LwUFXP52_12     num_UFXP52_12;
    LwUFXP52_12     quotient_UFXP52_12;
    LwUFXP4_12      baPct;
    FLCN_TIMESTAMP  lwrrBaEvalTimeNs;
    FLCN_TIMESTAMP  elapsedBaEvalTimeNs;
    FLCN_TIMESTAMP  lwrrBaEngageTimeNs;
    FLCN_TIMESTAMP  elapsedBaEngageTimeNs;
    THRM_MON       *pThrmMon;
    FLCN_STATUS     status  = FLCN_OK;
    LwBool          bPoison = LW_FALSE;

    OSTASK_OVL_DESC ovlDescListTherm[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, therm)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListTherm);
    {
        // Return early if Thermal Monitor Index is invalid.
        if (LW2080_CTRL_THERMAL_THERM_MONITOR_IDX_ILWALID ==
            pFreqControllerPI->baThermMonIdx)
        {
            goto _clkFreqControllerSamplePoisonedBA_PI_exit;
        }

        // Get Thermal Monitor object corresponding to index.
        pThrmMon = THRM_MON_GET(pFreqControllerPI->baThermMonIdx);
        if (pThrmMon == NULL)
        {
            PMU_BREAKPOINT();
            goto _clkFreqControllerSamplePoisonedBA_PI_exit;
        }

        // Get current timer values.
        status = thrmMonTimer64Get(pThrmMon, &lwrrBaEvalTimeNs, &lwrrBaEngageTimeNs);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto _clkFreqControllerSamplePoisonedBA_PI_exit;
        }

        // Compute elapsed time values.
        lw64Sub(&elapsedBaEvalTimeNs.data, &lwrrBaEvalTimeNs.data,
                &pFreqControllerPI->prevBaEvalTimeNs.data);
        lw64Sub(&elapsedBaEngageTimeNs.data, &lwrrBaEngageTimeNs.data,
                &pFreqControllerPI->prevBaEngageTimeNs.data);

        //
        // BA engage time should not be higher than the evaluation time and
        // evaluation time shouldn't be Zero.
        //
        if (lwU64CmpGT(&elapsedBaEngageTimeNs.data, &elapsedBaEvalTimeNs.data) ||
            (elapsedBaEvalTimeNs.data == LW_TYPES_FXP_ZERO))
        {
            PMU_BREAKPOINT();
            goto _clkFreqControllerSamplePoisonedBA_PI_exit;
        }

        // Cache current time values for next iteration.
        pFreqControllerPI->prevBaEvalTimeNs   = lwrrBaEvalTimeNs;
        pFreqControllerPI->prevBaEngageTimeNs = lwrrBaEngageTimeNs;

        //
        // Compute percentage time(as ratio in LwUFXP4_12) BA was engaged.
        // Note that imem overlays required for 64-bit math below are already
        // attached in _clkFreqControllerOsCallback function and this function
        // is eventually called from there.
        //
        num_UFXP52_12 = LW_TYPES_U64_TO_UFXP_X_Y(52, 12, elapsedBaEngageTimeNs.data);
        lwU64DivRounded(&quotient_UFXP52_12, &num_UFXP52_12, &elapsedBaEvalTimeNs.data);
        baPct = LwU32_LO16(LwU64_LO32(quotient_UFXP52_12));

        // Poison the sample if BA slowdown engaged more than the min limit.
        if (baPct > pFreqControllerPI->baPctMin)
        {
            bPoison = LW_TRUE;
        }

_clkFreqControllerSamplePoisonedBA_PI_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListTherm);

    return bPoison;
}
#endif

#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_DROOPY_SUPPORTED)
/*!
 * Check if the frequency controller sample is poisoned because of
 * droopy VR engaged.
 *
 * The sample is poisoned if droopy was enagaged for more time than the
 * minimum limit @ref droopyPctMin.
 * 
 * @param[in] pFreqControllerPI   PI frequency controller object pointer
 *
 * @return LW_TRUE - The sample is poisoned.
 * @return LW_FALSE - Otherwise.
 */
static LwBool
s_clkFreqControllerSamplePoisonedDroopy_PI
(
    CLK_FREQ_CONTROLLER_10_PI *pFreqControllerPI
)
{
    LwUFXP52_12     num_UFXP52_12;
    LwUFXP52_12     quotient_UFXP52_12;
    LwUFXP4_12      droopyPct;
    FLCN_TIMESTAMP  lwrrDroopyEvalTimeNs;
    FLCN_TIMESTAMP  elapsedDroopyEvalTimeNs;
    FLCN_TIMESTAMP  lwrrDroopyEngageTimeNs;
    FLCN_TIMESTAMP  elapsedDroopyEngageTimeNs;
    THRM_MON       *pThrmMon;
    FLCN_STATUS     status  = FLCN_OK;
    LwBool          bPoison = LW_FALSE;

    OSTASK_OVL_DESC ovlDescListTherm[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, therm)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListTherm);
    {
        // Return early if Thermal Monitor Index is invalid.
        if (LW2080_CTRL_THERMAL_THERM_MONITOR_IDX_ILWALID ==
            pFreqControllerPI->thermMonIdx)
        {
            goto _clkFreqControllerSamplePoisoned_DROOPY_PI_exit;
        }

        // Get Thermal Monitor object corresponding to index.
        pThrmMon = THRM_MON_GET(pFreqControllerPI->thermMonIdx);
        if (pThrmMon == NULL)
        {
            PMU_BREAKPOINT();
            goto _clkFreqControllerSamplePoisoned_DROOPY_PI_exit;
        }

        // Get current timer values.
        status = thrmMonTimer64Get(pThrmMon, &lwrrDroopyEvalTimeNs, &lwrrDroopyEngageTimeNs);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto _clkFreqControllerSamplePoisoned_DROOPY_PI_exit;
        }

        // Compute elapsed time values.
        lw64Sub(&elapsedDroopyEvalTimeNs.data, &lwrrDroopyEvalTimeNs.data,
                &pFreqControllerPI->prevDroopyEvalTimeNs.data);
        lw64Sub(&elapsedDroopyEngageTimeNs.data, &lwrrDroopyEngageTimeNs.data,
                &pFreqControllerPI->prevDroopyEngageTimeNs.data);

        //
        // Droopy engage time should not be higher than the evaluation time and
        // evaluation time shouldn't be Zero.
        //
        if (lwU64CmpGT(&elapsedDroopyEngageTimeNs.data, &elapsedDroopyEvalTimeNs.data) ||
            (elapsedDroopyEvalTimeNs.data == LW_TYPES_FXP_ZERO))
        {
            PMU_BREAKPOINT();
            goto _clkFreqControllerSamplePoisoned_DROOPY_PI_exit;
        }

        // Cache current time values for next iteration.
        pFreqControllerPI->prevDroopyEvalTimeNs   = lwrrDroopyEvalTimeNs;
        pFreqControllerPI->prevDroopyEngageTimeNs = lwrrDroopyEngageTimeNs;

        //
        // Compute percentage time(as ratio in LwUFXP4_12) droopy was engaged.
        // Note that imem overlays required for 64-bit math below are already
        // attached in _clkFreqControllerOsCallback function and this function
        // is eventually called from there.
        //
        num_UFXP52_12 = LW_TYPES_U64_TO_UFXP_X_Y(52, 12, elapsedDroopyEngageTimeNs.data);
        lwU64DivRounded(&quotient_UFXP52_12, &num_UFXP52_12, &elapsedDroopyEvalTimeNs.data);
        droopyPct = LwU32_LO16(LwU64_LO32(quotient_UFXP52_12));

        // Poison the sample if droopy engaged more than the min limit.
        if (droopyPct > pFreqControllerPI->droopyPctMin)
        {
            bPoison = LW_TRUE;
        }

_clkFreqControllerSamplePoisoned_DROOPY_PI_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListTherm);

    return bPoison;
}
#endif
