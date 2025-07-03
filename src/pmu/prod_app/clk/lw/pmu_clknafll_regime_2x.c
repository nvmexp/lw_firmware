/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clknafll_regime_2x.c
 * @brief  Clock code common to the V1.0 and 2.0 AVFS regime logic and clock programming.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dbgprintf.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "config/g_clk_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Private Function Prototypes -------------------- */
static FLCN_STATUS s_clkNafllProgram(CLK_NAFLL_DEVICE *pNafllDev, LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "s_clkNafllProgram");
static FLCN_STATUS s_clkNafllProgramFFR(CLK_NAFLL_DEVICE *pNafllDev, LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus, LwU16 freqMHz)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "s_clkNafllProgramFFR");
static FLCN_STATUS s_clkNafllProgramFR (CLK_NAFLL_DEVICE *pNafllDev, LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus, LwU16 freqMHz)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "s_clkNafllProgramFR");
static FLCN_STATUS s_clkNafllProgramVR (CLK_NAFLL_DEVICE *pNafllDev, LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus, LwU16 freqMHz)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "s_clkNafllProgramVR");
static FLCN_STATUS s_clkNafllFreqCtrlUpdate(CLK_NAFLL_DEVICE *pNafllDev, LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "s_clkNafllFreqCtrlUpdate");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Helper interface to configure the NAFLL source to the target parameters.
 *
 * @param[in]       pNafllDev   Pointer to the NAFLL device
 * @param[in,out]   pStatus     Pointer to the NAFLL dynamic status parameters
 * @param[in]       pTarget     Pointer to the NAFLL target parameters.
 *
 * @return FLCN_OK  NAFLL source successfully configured.
 * @return other    Descriptive error code from sub-routines on failure
 */
FLCN_STATUS
clkNafllConfig_2X
(
    CLK_NAFLL_DEVICE                         *pNafllDev,
    LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus,
    RM_PMU_CLK_FREQ_TARGET_SIGNAL            *pTarget
)
{
    FLCN_STATUS     status        = FLCN_OK;

    //
    // If the target frequency is 0, use the cached value of current
    // frequency to decide the regime.
    //
    pStatus->regime.targetFreqMHz = (LwU16)(pTarget->super.freqKHz / 1000U);

    //
    // Respect the regime requested by client such as perf change sequencer.
    // Do not callwlate it based on target frequency and voltage as client
    // could override it (ex. change sequencer overrides in PRE_VOLT_CLK)
    //
    pStatus->regime.targetRegimeId = pTarget->super.regimeId;

    // Update the DVCO min frequency.
    pStatus->dvco.minFreqMHz       = pTarget->super.dvcoMinFreqMHz;

    if (!pNafllDev->bSkipPldivBelowDvcoMin)
    {
        status = clkNafllPldivConfig(pNafllDev, pStatus);
        if (status != FLCN_OK)
        {
            goto clkNafllConfig_2X_exit;
        }
    }

clkNafllConfig_2X_exit:
    return status;
}

/*!
 * @brief Helper interface to configure the NAFLL source to the target parameters.
 *
 * @param[in]       pNafllDev   Pointer to the NAFLL device
 * @param[in,out]   pStatus     Pointer to the NAFLL dynamic status parameters
 *
 * @return FLCN_OK  NAFLL source successfully configured.
 * @return other    Descriptive error code from sub-routines on failure
 */
FLCN_STATUS
clkNafllProgram_2X
(
    CLK_NAFLL_DEVICE                         *pNafllDev,
    LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus
)
{
    FLCN_STATUS status = FLCN_OK;

    if (!pNafllDev->bSkipPldivBelowDvcoMin &&
        (pStatus->pldivEngage == LW_TRISTATE_FALSE))
    {
        //
        // Disengage pldiv if the target frequency is above the DVCO minimum
        // frequency. Note, this must happen before the target frequency and/or
        // regime have to be changed.
        //
        status = clkNafllPldivProgram(pNafllDev, pStatus);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkNafllProgram_exit;
        }
    }

    //
    // This function will program the NAFLL to a given frequency and regime
    // and a zero offset.
    // The frequency controller will determine the offset when it runs.
    //
    pStatus->regime.offsetFreqMHz = 0;
    status = s_clkNafllProgram(pNafllDev, pStatus);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkNafllProgram_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER))
    {
        //
        // Update frequency controller if needs to be enabled/disabled because of
        // the change in regime or frequency.
        //
        status = s_clkNafllFreqCtrlUpdate(pNafllDev, pStatus);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkNafllProgram_exit;
        }
    }

    if (!pNafllDev->bSkipPldivBelowDvcoMin &&
        (pStatus->pldivEngage == LW_TRISTATE_TRUE))
    {
        //
        // Engage pldiv if the target frequency is below the DVCO minimum
        // frequency. Note, this must happen AFTER the target frequency is
        // programmed, to allow the frequency to drop as close to DVCO min as
        // possible. This helps in minimizing the di/dt when PLDIV is to be
        // engaged.
        //
        status = clkNafllPldivProgram(pNafllDev, pStatus);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkNafllProgram_exit;
        }
    }

clkNafllProgram_exit:
    return status;
}

/*!
 * @brief Get the target regime ID for a given NAFLL, target frequency
 * and voltage
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 * @param[in]   targetfreqMHz   Target frequency in MHz
 * @param[in]   pVoltListItem   Pointer to entry in LW2080_CTRL_VOLT_VOLT_RAIL_LIST
 *
 * @return  The target regime ID, on success
 * @return  @ref LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID on failure
 */
LwU8
clkNafllDevTargetRegimeGet_2X
(
    CLK_NAFLL_DEVICE                       *pNafllDev,
    LwU16                                   targetFreqMHz,
    LwU16                                   dvcoMinFreqMHz,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM   *pVoltListItem
)
{
    LwU8 targetRegimeId = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;

    // TODO - Add sanity check for voltlist once forcedRegime is deleted.
    if ((pNafllDev == NULL) ||
        (pVoltListItem == NULL))
    {
        PMU_BREAKPOINT();
        return LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;
    }

    // Determine the regime based on target frequency.
    if (targetFreqMHz <  pNafllDev->regimeDesc.fixedFreqRegimeLimitMHz)
    {
        //
        // If the target frequency is less than fixed frequency regime limit,
        // set to fixed frequency regime.
        //
        targetRegimeId = LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR;
    }
    else // if (targetFreqMHz >= pNafllDev->regimeDesc.fixedFreqRegimeLimitMHz)
    {
        targetRegimeId = LW2080_CTRL_CLK_NAFLL_REGIME_ID_FR;
    }

    //
    // If the target regime ID override is set to VR_ABOVE_VMIN, override to
    // voltage regime if the voltage is above the noise unaware Vmin. Note, the
    // client must pass the voltage and the noise-unaware Vmin values to this
    // routine
    //
    if (pNafllDev->regimeDesc.targetRegimeIdOverride ==
        LW2080_CTRL_CLK_NAFLL_REGIME_ID_VR_ABOVE_NOISE_UNAWARE_VMIN)
    {
        //
        // For the regime to be override to VR, the caller must pass a valid
        // value of voltageuV and voltageNoiseUnawareVminuV. If not, then
        // skip overriding the regime to VR
        //
        if ((pVoltListItem->voltageuV != RM_PMU_VOLT_VALUE_0V_IN_UV) &&
            (pVoltListItem->voltageMinNoiseUnawareuV != RM_PMU_VOLT_VALUE_0V_IN_UV) &&
            (pVoltListItem->voltageuV > pVoltListItem->voltageMinNoiseUnawareuV))
        {
            targetRegimeId = LW2080_CTRL_CLK_NAFLL_REGIME_ID_VR;
        }
    }
    else if (LW2080_CTRL_CLK_NAFLL_REGIME_IS_VALID(
        pNafllDev->regimeDesc.targetRegimeIdOverride))
    {
        targetRegimeId = pNafllDev->regimeDesc.targetRegimeIdOverride;
    }
    else
    {
        // Do nothing
    }

    return targetRegimeId;
}

/*!
 * @brief Given a NAFLL device - get the current offset frequency
 *
 * @param[in]   pNafllDev Pointer to the NAFLL device
 *
 * @pre         Client MUST attach the clk3x DMEM overlay if
 *              @ref PMU_CLK_CLKS_ON_PMU feature is enabled.
 *
 * @return      Cached value of the current offset frequency
 */
LwU16
clkNafll2xOffsetFreqMHzGet
(
   CLK_NAFLL_DEVICE *pNafllDev
)
{
    return pNafllDev->pStatus->regime.offsetFreqMHz;
}

/*!
 * @brief Given a NAFLL device - set the target offset frequency
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 * @param[in]   offsetFreqMHz   Offset frequency in MHz
 *
 * @pre         Client MUST attach the clk3x DMEM overlay if
 *              @ref PMU_CLK_CLKS_ON_PMU feature is enabled.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT if given Nafll device is NULL.
 * @return other                     Descriptive status code from sub-routines
 *                                   on success/failure
 */
FLCN_STATUS
clkNafll2xOffsetFreqMHzSet
(
   CLK_NAFLL_DEVICE *pNafllDev,
   LwU16             offsetFreqMHz
)
{
    if (pNafllDev == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    pNafllDev->pStatus->regime.targetFreqMHz  =
        pNafllDev->pStatus->regime.lwrrentFreqMHz;
    pNafllDev->pStatus->regime.targetRegimeId =
        pNafllDev->pStatus->regime.lwrrentRegimeId;
    pNafllDev->pStatus->regime.offsetFreqMHz  = offsetFreqMHz;

    return s_clkNafllProgram(pNafllDev, pNafllDev->pStatus);
}

/*!
 * @brief Helper interface to callwlate the required PLDIV configuration at the
 *        target frequency for a given NAFLL
 *
 * @param[in]     pNafllDev  Pointer to the NAFLL device
 * @param[in,out] pStatus    Pointer to the dynamic status param of NAFLL device.
 *
 * @return FLCN_OK
 */
FLCN_STATUS
clkNafllPldivConfig_2X
(
    CLK_NAFLL_DEVICE                         *pNafllDev,
    LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus
)
{
    pStatus->pldivEngage = LW_TRISTATE_INDETERMINATE;

    if (pStatus->regime.targetFreqMHz < pStatus->dvco.minFreqMHz)
    {
        //
        // Crude algorithm. If the target frequency is between minFreqMHz
        // and (minFreqMHz / 2), use DIV2. Else use DIV4.
        // Note: We are not using odd divisor (DIV3) as odd divisor have
        // more jitters. The downside is that we don't get an accurate
        // frequency. This may very well change in the future.
        //
        pStatus->pldiv = (pStatus->regime.targetFreqMHz >=
                 LW_UNSIGNED_ROUNDED_DIV(pStatus->dvco.minFreqMHz, 2U)) ?
                     CLK_NAFLL_PLDIV_DIV2 : CLK_NAFLL_PLDIV_DIV4;
        pStatus->pldivEngage = LW_TRISTATE_TRUE;
    }
    else
    {
        //
        // The requested frequency is above DVCO min. Set pldiv to DIV1 if
        // not already at DIV1.
        //
        if (pStatus->pldiv > CLK_NAFLL_PLDIV_DIV1)
        {
            // pldiv is engaged. set it to DIV1
            pStatus->pldiv       = CLK_NAFLL_PLDIV_DIV1;
            pStatus->pldivEngage = LW_TRISTATE_FALSE;
        }
    }

    return FLCN_OK;
}

/*!
 * @brief Engage/Disengage the PLDIV at the target frequency for a given NAFLL
 *
 * @param[in]   pNafllDev  Pointer to the NAFLL device
 * @param[in]   pStatus    Pointer to the dynamic status param of NAFLL device.
 *
 *
 * @return FLCN_OK  On success
 * @return other    Descriptive error code from sub-routines on failure
 *
 * @ref clkNafllPldivSet_HAL
 */
FLCN_STATUS
clkNafllPldivProgram_2X
(
    CLK_NAFLL_DEVICE                         *pNafllDev,
    LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus
)
{
    FLCN_STATUS status = FLCN_OK;

    if (pStatus->pldivEngage == LW_TRISTATE_TRUE)
    {
        //
        // We are trying to target a frequency less than dvcoMin. The caller
        // must have already initiated the frequency change. Either check
        // the DVCO trim setting or use a hardcoded wait of 5us for the
        // DVCO Min to be reached. For now use a hardcoded delay since the
        // DVCO trim settings are not exposed via priv registers which are
        // timed correctly.
        //
        OS_PTIMER_SPIN_WAIT_US(5U);
    }

    status = clkNafllPldivSet_HAL(&Clk, pNafllDev, pStatus->pldiv);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkNafllPldivProgram_2X_exit;
    }

clkNafllPldivProgram_2X_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Program target frequency at the given regime
 *
 * @details This function doesn't engage/disengage the PLDIV and the frequency
 * controllers. This function also takes a offsetFreqMHz as a parameter.
 *
 * @param[in]       pNafllDev   Pointer to the NAFLL device
 * @param[in,out]   pStatus     Pointer to the NAFLL device dynamic status
 *
 * @return  FLCN_OK                Successfully programmed the frequency
 * @return  FLCN_ERR_ILWALID_STATE NAFLL device is in inconsistent state
 *                                 like if LUT in not initialized
 *                                 or ADC is not powered-on/enabled
 * @return  other                  Descriptive error code from sub-routines
 *                                 on failure
 */
static FLCN_STATUS
s_clkNafllProgram
(
    CLK_NAFLL_DEVICE                         *pNafllDev,
    LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus
)
{
    FLCN_STATUS  status = FLCN_OK;
    LwU16  freqMHz =
        (pStatus->regime.targetFreqMHz + pStatus->regime.offsetFreqMHz);

    //
    // Return an error if the LUT is not initialized. The expectation is
    // that one of the LUT tables will be setup before the call to make a
    // NAFLL frequency / regime programming. Note that it is not absolutely
    // required for FFR regime but in general SW do not allow NAFLL programming
    // before LUT initialization.
    //
    if (!pNafllDev->lutDevice.bInitialized)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto s_clkNafllProgram_exit;
    }

    // Ensure that ADCs are ON before NAFLL programming.
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICES_ALWAYS_ON))
    {
        if (!CLK_ADC_IS_POWER_ON_AND_ENABLED(pNafllDev->pLogicAdcDevice))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
            goto s_clkNafllProgram_exit;
        }
    }

    switch (pStatus->regime.targetRegimeId)
    {
        case LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR:
        {
            status = s_clkNafllProgramFFR(pNafllDev, pStatus, freqMHz);
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_REGIME_ID_FR:
        {
            status = s_clkNafllProgramFR(pNafllDev, pStatus, freqMHz);
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_REGIME_ID_VR:
        {
            status = s_clkNafllProgramVR(pNafllDev, pStatus, freqMHz);
            break;
        }
        default:
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            break;
        }
    }
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_clkNafllProgram_exit;
    }

    // Cache the dvco min hit flag
    pStatus->dvco.bMinReached =
        (pStatus->regime.targetFreqMHz < pStatus->dvco.minFreqMHz);

s_clkNafllProgram_exit:
    return status;
}

/*!
 * @brief Program target frequency for fixed frequency regime
 *
 * @param[in]   pNafllDev   Pointer to the NAFLL device
 * @param[in]   pStatus     Pointer to the NAFLL device dynamic status
 * @param[in]   freqMHz     Target frequency in MHz
 *
 * @return FLCN_OK  Successfully programmed the frequency
 * @return other    Descriptive error code from sub-routines on failure
 */
static FLCN_STATUS
s_clkNafllProgramFFR
(
    CLK_NAFLL_DEVICE                         *pNafllDev,
    LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus,
    LwU16                                     freqMHz
)
{
    FLCN_STATUS status;

    status = clkNafllLutOverride(pNafllDev,
                 LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_SW_REQ,
                 freqMHz);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_clkNafllProgramFFR_exit;
    }

    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICES_ALWAYS_ON))
    {
        // Disable the Logic ADC.
        status = clkAdcPowerOnAndEnable(pNafllDev->pLogicAdcDevice,
                                   LW2080_CTRL_CLK_ADC_CLIENT_ID_PMU,
                                   pNafllDev->id,
                                   LW_FALSE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_clkNafllProgramFFR_exit;
        }

        // Disable the Sram ADC.
        if (pNafllDev->pSramAdcDevice != NULL)
        {
            status = clkAdcPowerOnAndEnable(pNafllDev->pSramAdcDevice,
                                      LW2080_CTRL_CLK_ADC_CLIENT_ID_PMU,
                                      pNafllDev->id,
                                      LW_FALSE);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto s_clkNafllProgramFFR_exit;
            }
        }
    }

s_clkNafllProgramFFR_exit:
    return status;
}

/*!
 * @brief Program the target frequency for frequency regime
 *
 * @copydetails s_clkNafllProgramFFR()
 */
static FLCN_STATUS
s_clkNafllProgramFR
(
    CLK_NAFLL_DEVICE                         *pNafllDev,
    LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus,
    LwU16                                     freqMHz
)
{
    FLCN_STATUS status = FLCN_OK;

    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICES_ALWAYS_ON))
    {
        if (LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR == pStatus->regime.lwrrentRegimeId)
        {
            // Enable Logic ADC if it's not already enabled.
            status = clkAdcPowerOnAndEnable(pNafllDev->pLogicAdcDevice,
                                            LW2080_CTRL_CLK_ADC_CLIENT_ID_PMU,
                                            pNafllDev->id,
                                            LW_TRUE);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto s_clkNafllProgramFR_exit;
            }

            // Enable Sram ADC if supported it's not already enabled.
            if (pNafllDev->pSramAdcDevice != NULL)
            {
                status = clkAdcPowerOnAndEnable(pNafllDev->pSramAdcDevice,
                                          LW2080_CTRL_CLK_ADC_CLIENT_ID_PMU,
                                          pNafllDev->id,
                                          LW_TRUE);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_clkNafllProgramFR_exit;
                }
            }
        }
    }

    status = clkNafllLutOverride(pNafllDev,
                 LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_MIN,
                 freqMHz);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_clkNafllProgramFR_exit;
    }

s_clkNafllProgramFR_exit:
    return status;
}

/*!
 * @brief Program the target frequency for the voltage regime
 *
 * @copydetails s_clkNafllProgramFFR()
 */
static FLCN_STATUS
s_clkNafllProgramVR
(
    CLK_NAFLL_DEVICE                         *pNafllDev,
    LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus,
    LwU16                                     freqMHz
)
{
    FLCN_STATUS status = FLCN_OK;

    // Return early if we are already in voltage regime
    if (LW2080_CTRL_CLK_NAFLL_REGIME_ID_VR == pStatus->regime.lwrrentRegimeId)
    {
        status = FLCN_OK;
        goto s_clkNafllProgramVR_exit;
    }

    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICES_ALWAYS_ON))
    {
        //
        // Enable the ADC if it has not enabled already. This can happen if the
        // lwrrentRegimeId is FFR.
        //
        if (LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR == pStatus->regime.lwrrentRegimeId)
        {
            status = clkAdcPowerOnAndEnable(pNafllDev->pLogicAdcDevice,
                                            LW2080_CTRL_CLK_ADC_CLIENT_ID_PMU,
                                            pNafllDev->id,
                                            LW_TRUE);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto s_clkNafllProgramVR_exit;
            }

            if (pNafllDev->pSramAdcDevice != NULL)
            {
                status = clkAdcPowerOnAndEnable(pNafllDev->pSramAdcDevice,
                                               LW2080_CTRL_CLK_ADC_CLIENT_ID_PMU,
                                               pNafllDev->id,
                                               LW_TRUE);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_clkNafllProgramVR_exit;
                }
            }
        }
    }

    // Set the LUT override to _USE_HW
    status = clkNafllLutOverride(pNafllDev,
                LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_HW_REQ,
                freqMHz);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_clkNafllProgramVR_exit;
    }

s_clkNafllProgramVR_exit:
    return status;
}

/*!
 * @brief   Enable/Disable frequency controller for the target NAFLL based on
 *          the change in NAFLL regime and/or frequency.
 *
 * @param[in]       pNafllDev   Pointer to the NAFLL device
 * @param[in,out]   pStatus     Pointer to the NAFLL device dynamic status
 *
 * @return FLCN_OK  Successfully enabled/disabled frequency controller.
 * @return other    Descriptive error code from sub-routines on failure
 */
static FLCN_STATUS
s_clkNafllFreqCtrlUpdate
(
    CLK_NAFLL_DEVICE                         *pNafllDev,
    LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus
)
{
    FLCN_STATUS status = FLCN_OK;

    // Return early if there is no frequency controller for this NAFLL.
    if (pNafllDev->freqCtrlIdx == LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_IDX_ILWALID)
    {
        goto s_clkNafllFreqCtrlUpdate_exit;
    }

    //
    // Enable/disable controller because of regime change.
    // Case 1. Disable, if regime changed from FR to any other regime.
    // Case 2. Enable, if regime changed to FR from any other regime.
    //
    if ((pStatus->regime.lwrrentRegimeId != pStatus->regime.targetRegimeId) &&
        ((LW2080_CTRL_CLK_NAFLL_REGIME_ID_FR == pStatus->regime.lwrrentRegimeId) ||
         (LW2080_CTRL_CLK_NAFLL_REGIME_ID_FR == pStatus->regime.targetRegimeId)))
    {
        status = clkFreqControllerDisable(pNafllDev->freqCtrlIdx,
            LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_CLIENT_ID_PMU_REGIME_LOGIC,
            (LW2080_CTRL_CLK_NAFLL_REGIME_ID_FR != pStatus->regime.targetRegimeId));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_clkNafllFreqCtrlUpdate_exit;
        }
    }

    //
    // Enable/disable controller because of worst case DVCO min.
    // Case 1. Disable, if frequency is going below worst case DVCO min.
    // Case 2. Enable, if frequency is going above(or equal to) worst case DVCO min.
    //
    if (((pStatus->regime.lwrrentFreqMHz <  Clk.clkNafll.maxDvcoMinFreqMHz)  &&
         (pStatus->regime.targetFreqMHz  >= Clk.clkNafll.maxDvcoMinFreqMHz)) ||
        ((pStatus->regime.lwrrentFreqMHz >= Clk.clkNafll.maxDvcoMinFreqMHz)  &&
         (pStatus->regime.targetFreqMHz  <  Clk.clkNafll.maxDvcoMinFreqMHz)))
    {
        status = clkFreqControllerDisable(pNafllDev->freqCtrlIdx,
            LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_CLIENT_ID_PMU_WORST_DVCO_MIN,
            (pStatus->regime.targetFreqMHz < Clk.clkNafll.maxDvcoMinFreqMHz));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_clkNafllFreqCtrlUpdate_exit;
        }
    }

    //
    // Enable/disable controller because of DVCO min.
    // Case 1. Disable, if frequency is going below DVCO min.
    // Case 2. Enable, if frequency is going above(or equal to) DVCO min.
    //
    if (((pStatus->regime.lwrrentFreqMHz <  pStatus->dvco.minFreqMHz)  &&
         (pStatus->regime.targetFreqMHz  >= pStatus->dvco.minFreqMHz)) ||
        ((pStatus->regime.lwrrentFreqMHz >= pStatus->dvco.minFreqMHz)  &&
         (pStatus->regime.targetFreqMHz  <  pStatus->dvco.minFreqMHz)))
    {
        status = clkFreqControllerDisable(pNafllDev->freqCtrlIdx,
            LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_CLIENT_ID_PMU_DVCO_MIN,
            (pStatus->regime.targetFreqMHz < pStatus->dvco.minFreqMHz));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_clkNafllFreqCtrlUpdate_exit;
        }
    }

s_clkNafllFreqCtrlUpdate_exit:
    return status;
}
