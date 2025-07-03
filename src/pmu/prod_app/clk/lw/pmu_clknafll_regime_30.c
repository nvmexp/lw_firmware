/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clknafll_regime_30.c
 * @brief  Clock code related to the V 3.0 AVFS regime logic and clock programming.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dbgprintf.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "pmu_objperf.h"
#include "perf/3x/vfe.h"
#include "volt/objvolt.h"
#include "config/g_clk_private.h"

#include "dev_pwr_csb.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
//
// Worst case time for NAFLL to lock LUT limited NDIV
// Bug: http://lwbugs/2062195
//
#define NAFLL_PLDIV_FREQLOCK_DELAY_US   20U

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Private Function Prototypes -------------------- */
static FLCN_STATUS _clkNafllProgram_30(CLK_NAFLL_DEVICE *pNafllDev, LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "_clkNafllProgram_30");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Helper interface to configure the NAFLL source to the target parameters.
 *
 * @details Refer https://confluence.lwpu.com/display/RMPER/NAFLL+Clocks+Programming#NAFLLClocksProgramming-NAFLLConfigDetermination
 *          for details on NAFLL Config Determination.
 *
 *  Step 1) Sanity Checks
 *              Ensure clock domain is supported.
 *              Ensure target frequency is Non Zero
 *              Ensure voltage domain is supported
 *              Ensure target voltage is Non Zero.
 *              Ensure that NAFLL device is supported (NOT floor-swept...)
 *              Ensure that LUT is initialized.
 *              Ensure that ADC is ON.
 *
 *  Step 2) Set Fprogram = Ftarget
 *
 *  Step 3.a) IF target regime = FFR_BELOW_DVCO_MIN
 *              Get DVCO Min frequency from NAFLL object.
 *              Set PLDIV such that PLDIV x Ftarget >= DVCOMin Frequency.
 *              Set Fprogram =  PLDIV x Ftarget
 *
 *  Step 3.b) IF current regime = FFR_BELOW_DVCO_MIN and target regime != FFR_BELOW_DVCO_MIN
 *              set PLDIV = 1
 *
 * @param[in]       pNafllDev   Pointer to the NAFLL device
 * @param[in/out]   pStatus     Pointer to the NAFLL dynamic status parameters
 * @param[in]       pTarget     Pointer to the NAFLL target parameters.
 *
 * @return FLCN_OK  NAFLL source successfully configured.
 *         ERROR    Other errors coming from higher level function calls.
 */
FLCN_STATUS
clkNafllConfig_30
(
    CLK_NAFLL_DEVICE                         *pNafllDev,
    LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus,
    RM_PMU_CLK_FREQ_TARGET_SIGNAL            *pTarget
)
{
    FLCN_STATUS     status        = FLCN_OK;

    //
    // Step 1) Sanity Checks
    // Return an error if the LUT is not initialized.
    //
    if (!CLK_NAFLL_LUT_IS_INITIALIZED(pNafllDev))
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkNafllConfig_30_exit;
    }

    // Ensure that ADCs are ON before NAFLL programming.
    if (!CLK_ADC_IS_POWER_ON_AND_ENABLED(pNafllDev->pLogicAdcDevice))
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkNafllConfig_30_exit;
    }

    //
    // Step 2) Copy in NAFLL target frequency and regime into internal NAFLL
    // status struct
    //
    pStatus->regime.targetFreqMHz   = pTarget->super.freqKHz / 1000;
    pStatus->regime.targetRegimeId  = pTarget->super.regimeId;
    pStatus->dvco.minFreqMHz        = pTarget->super.dvcoMinFreqMHz;

    // Step 3) Callwlate appropriate PLDIV Value
    status = clkNafllPldivConfig(pNafllDev, pStatus);
    if (status != FLCN_OK)
    {
        goto clkNafllConfig_30_exit;
    }

clkNafllConfig_30_exit:
    return status;
}

//!  https://confluence.lwpu.com/pages/viewpage.action?spaceKey=RMPER&title=NAFLL+Clocks+Programming#NAFLLClocksProgramming-NAFLLConfigProgramming
/*!
 * @brief Helper interface to configure the NAFLL source to the target parameters.
 *
 *  Step 1) If regime != VR_WITH_CPM
 *              Override CPM NDIV Offset output to ZERO
 *              Achieved by clearing the CPM aclwmulator :: _CPM_CLK_CFG_CLEAR = 1
 *
 *  Step 2) If regime == FFR
 *              Set CLFC disable client = LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_CLIENT_ID_PMU_REGIME_LOGIC
 *          If regime == FFR_BELOW_DVCO_MIN
 *              Set CLFC disable client = LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_CLIENT_ID_PMU_DVCO_MIN
 *          Disable CLFC if Enabled.
 *
 *  Step 3) If regime == FFR_BELOW_DVCO_MIN and PLDIV is increasing
 *              Engage / Update PLDIV
 *
 *  Step 4) Colwert Fprogram into ndiv value i.e. Program LUT override
 *              Get complete LUT entry tuple - NDIV_OFFSET and DVCO_OFFSET using VF tuple look up interface
 *                  set CPM ndiv MAX offset will be set to ZERO.
 *                  Update _LUT_SW_FREQ_REQ register with new target ndiv, ndivOffset, DVCOOffset, CPM max determined above.
 *
 *  Step 5) IF regime == FFR_BELOW_DVCO_MIN and PLDIV is decreasing
 *                          or
 *          If current regime == FFR_BELOW_DVCO_MIN and target regime != FFR_BELOW_DVCO_MIN
 *              Poll for FREQLOCK (bug 2062195) or wait long enough for the NAFLL to lock to the LUT-limited NDIV (~20us)
 *              Disengage / update PLDIV if engaged.
 *
 *  Step 6) If regime == VR_WITH_CPM
 *              Remove SW override on CPM NDIV Offset output.
 *
 *  Step 7) If regime != FFR_BELOW_DVCO_MIN
 *              Unset CLFC disable client = PMU_DVCO_MIN
 *          If regime != FFR
 *              Unset CLFC disable client = PMU_REGIME_LOGIC
 *          Enable if no other client are requesting Disable CLFC
 *
 * @param[in]       pNafllDev   Pointer to the NAFLL device
 * @param[in/out]   pStatus     Pointer to the NAFLL dynamic status parameters
 * @param[in]       pTarget     Pointer to the NAFLL target parameters.
 *
 * @return FLCN_OK  NAFLL source successfully configured.
 *         ERROR    Other errors coming from higher level function calls.
 */
FLCN_STATUS
clkNafllProgram_30
(
    CLK_NAFLL_DEVICE                         *pNafllDev,
    LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus
)
{
    FLCN_STATUS status      = FLCN_OK;
    LwU8        prevPldiv   = clkNafllPldivGet_HAL(&Clk, pNafllDev);

    //
    // Step 1) If CPM is enabled and regime != VR_WITH_CPM then override CPM NDIV
    // offset output to ZERO.
    //
    // Note - VR_WITH_CPM regime can be chosen even if CPM is disabled. When
    // CPM is disabled it is equivalent to VR regime in HW - check @ref
    // clkNafllDevTargetRegimeGet_30 for more details.
    //
    if ((PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_CPM)) &&
        (pStatus->regime.targetRegimeId != LW2080_CTRL_CLK_NAFLL_REGIME_ID_VR_WITH_CPM))
    {
        status = clkNafllCpmNdivOffsetOverride_HAL(&Clk, pNafllDev, LW_TRUE);
    }
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkNafllProgram_30_exit;
    }

    //
    // Step 2) If regime is FFR or FFR_BELOW_DVCO_MIN then disable CLFC if enabled.
    //
    if (pNafllDev->freqCtrlIdx != LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_IDX_ILWALID)
    {
        if (pStatus->regime.targetRegimeId ==
            LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR)
        {
            status = clkFreqControllerDisable(pNafllDev->freqCtrlIdx,
                LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_CLIENT_ID_PMU_REGIME_LOGIC,
                LW_TRUE);
        }
        else if (pStatus->regime.targetRegimeId ==
            LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR_BELOW_DVCO_MIN)
        {
            status = clkFreqControllerDisable(pNafllDev->freqCtrlIdx,
                LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_CLIENT_ID_PMU_DVCO_MIN,
                LW_TRUE);
        }

        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkNafllProgram_30_exit;
        }
    }

    //
    // Step 3) If regime is FFR_BELOW_DVCO_MIN then engage or update (only increase DO NOT decrease) PLDIV
    //
    if ((pStatus->regime.targetRegimeId ==
            LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR_BELOW_DVCO_MIN) &&
        (pStatus->pldiv > prevPldiv))
    {
        status = clkNafllPldivProgram(pNafllDev, pStatus);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkNafllProgram_30_exit;
        }
    }

    // Step 4) Colwert Fprogram into ndiv value
    status = _clkNafllProgram_30(pNafllDev, pStatus);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkNafllProgram_30_exit;
    }

    //
    // Step 5) If current regime == FFR_BELOW_DVCO_MIN and target regime != FFR_BELOW_DVCO_MIN
    //                  or
    //         If regime == FFR_BELOW_DVCO_MIN and PLDIV is decreasing
    //          Poll for FREQLOCK (bug 2062195) or wait long enough for the NAFLL to lock to the LUT-limited NDIV (~10us)
    //          Disengage / update (only decrease DO NOT increase) PLDIV if engaged.
    //
    if (((pStatus->regime.lwrrentRegimeId ==
            LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR_BELOW_DVCO_MIN) &&
         (pStatus->regime.targetRegimeId  !=
            LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR_BELOW_DVCO_MIN))    ||
        ((pStatus->regime.targetRegimeId  ==
            LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR_BELOW_DVCO_MIN) &&
         (pStatus->pldiv < prevPldiv)))
    {
        status = clkNafllPldivProgram(pNafllDev, pStatus);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkNafllProgram_30_exit;
        }
    }

    //
    // Step 6) If CPM is enabled and regime == VR_WITH_CPM then remove SW
    // override on CPM NDIV offset output.
    //
    // Note - VR_WITH_CPM regime can be chosen even if CPM is disabled. When
    // CPM is disabled it is equivalent to VR regime in HW - check @ref
    // clkNafllDevTargetRegimeGet_30 for more details.
    //
    if ((PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_CPM)) &&
        (pStatus->regime.targetRegimeId == LW2080_CTRL_CLK_NAFLL_REGIME_ID_VR_WITH_CPM))
    {
        status = clkNafllCpmNdivOffsetOverride_HAL(&Clk, pNafllDev, LW_FALSE);
    }
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkNafllProgram_30_exit;
    }

    //
    // Step 7) If regime is FFR or FFR_BELOW_DVCO_MIN then enable CLFC if disabled.
    //
    if (pNafllDev->freqCtrlIdx != LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_IDX_ILWALID)
    {
        if (pStatus->regime.targetRegimeId !=
            LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR_BELOW_DVCO_MIN)
        {
            status = clkFreqControllerDisable(pNafllDev->freqCtrlIdx,
                 LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_CLIENT_ID_PMU_DVCO_MIN,
                 LW_FALSE);
        }
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkNafllProgram_30_exit;
        }

        if (pStatus->regime.targetRegimeId !=
            LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR)
        {
            status = clkFreqControllerDisable(pNafllDev->freqCtrlIdx,
                LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_CLIENT_ID_PMU_REGIME_LOGIC,
                LW_FALSE);
        }
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkNafllProgram_30_exit;
        }
    }

    // Cache the dvco min hit flag
    pStatus->dvco.bMinReached =
        (pStatus->regime.targetFreqMHz < pStatus->dvco.minFreqMHz);

clkNafllProgram_30_exit:
    return status;
}

//!  https://confluence.lwpu.com/display/RMPER/NAFLL+Clocks+Programming#NAFLLClocksProgramming-SWAlgorithm
/*!
 * @brief Get the target regime ID for a given NAFLL, target frequency
 * and voltage
 *
 *  step 1) Using NAFLL object interfaces check if client had set regime
 *          override request (Using tools like MODS, PERFDEBUG....)
 *          If so and the override is valid regime, Set Regime = Client Override request
 *              Goto Algorithm_Exit
 *
 *  Step 2) Using NAFLL object interfaces get cached DVCO min for target voltage value.
 *              If the target frequency is below DVCO min
 *                  Set Regime = FFR_BELOW_DVCO_MIN
 *                  Goto Algorithm_Exit
 *
 *  Step 3) Using NAFLL object interfaces get the NAFLL reference frequency
 *          (generally 405 MHz) set in VBIOS POR.
 *              If the target frequency is below reference frequency
 *                  Set Regime = FFR
 *                  Goto Algorithm_Exit
 *
 *  Step 4) Using VF look up interfaces, get Fmax @ Varb value.
 *          4.1) IF the target frequency is below Fmax AND CLFC voltdelta <= 0
 *                  Set Regime = FR
 *                  Goto Algorithm_Exit
 *          4.2) ELSE IF the target frequency is below Fmax AND CLFC voltdelta > 0
 *                  Set Regime = VR
 *                  Goto Algorithm_Exit
 *          4.3) Else
 *                  Set Regime = VR_WITH_CPM
 *                  Goto Algorithm_Exit
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 * @param[in]   targetfreqMHz   Target frequency in MHz
 * @param[in]   flags           Clk info flags @ref LW2080_CTRL_CLK_INFO_FLAGS_<XYZ>
 * @param[in]   pVoltListItem   Pointer to entry in LW2080_CTRL_VOLT_VOLT_RAIL_LIST
 *
 * @return      The target regime ID
 * @note VR_ABOVE_NOISE_UNAWARE_VMIN not used anymore
 */
LwU8
clkNafllDevTargetRegimeGet_30
(
    CLK_NAFLL_DEVICE                       *pNafllDev,
    LwU16                                   targetFreqMHz,
    LwU16                                   dvcoMinFreqMHz,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM   *pVoltListItem
)
{
    LwU8        targetRegimeId  = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;
    LwU16       maxFreqMHz      = 0;
    LwS32       clfcVoltDeltauV = 0;
    FLCN_STATUS status          = FLCN_OK;

    if ((pNafllDev     == NULL) ||
        (pVoltListItem == NULL))
    {
        PMU_BREAKPOINT();
        goto clkNafllDevTargetRegimeGet_30_exit;
    }

    // step 1) Return regime override value if valid
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
            goto clkNafllDevTargetRegimeGet_30_exit;
        }
    }
    else if (LW2080_CTRL_CLK_NAFLL_REGIME_IS_VALID(
        pNafllDev->regimeDesc.targetRegimeIdOverride))
    {
        targetRegimeId = pNafllDev->regimeDesc.targetRegimeIdOverride;
        goto clkNafllDevTargetRegimeGet_30_exit;
    }
    else
    {
        // Do Nothing
    }

    // step 2) Determine the regime based on target frequency.
    if (targetFreqMHz < dvcoMinFreqMHz)
    {
        if (pNafllDev->bSkipPldivBelowDvcoMin)
        {
            //
            // If the Pldiv Skip for below DVCO min flag is set then
            // set to fixed frequency regime.
            //
            targetRegimeId = LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR;
        }
        else
        {
            //
            // If the target frequency is less than DVCO min then
            // set to fixed frequency below dvco min regime.
            //
            targetRegimeId = LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR_BELOW_DVCO_MIN;
        }
        goto clkNafllDevTargetRegimeGet_30_exit;
    }

    // step 3) Determine the regime based on target frequency.
    if (targetFreqMHz < pNafllDev->regimeDesc.fixedFreqRegimeLimitMHz)
    {
        //
        // If the target frequency is less than fixed frequency regime limit,
        // set to fixed frequency regime.
        //
        targetRegimeId = LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR;
        goto clkNafllDevTargetRegimeGet_30_exit;
    }

    // step 4) Using VF look up interfaces, get Fmax @ Vin value
    status = clkNafllLutFreqMHzGet(pNafllDev,
                                   pVoltListItem->voltageuV,
                                   LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,
                                   &maxFreqMHz,
                                   NULL);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkNafllDevTargetRegimeGet_30_exit;
    }

    // Get CLFC Voltage delta from input Volt List Item.
    clfcVoltDeltauV =
        pVoltListItem->voltOffsetuV[LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_CLFC];

    if (targetFreqMHz < maxFreqMHz)
    {
        if (clfcVoltDeltauV <= 0)
        {
            // step 4.1) IF the target frequency is below Fmax AND CLFC voltdelta <= 0
            targetRegimeId = LW2080_CTRL_CLK_NAFLL_REGIME_ID_FR;
        }
        else
        {
            // step 4.2) IF the target frequency is below Fmax AND CLFC voltdelta > 0
            targetRegimeId = LW2080_CTRL_CLK_NAFLL_REGIME_ID_VR;
        }
    }
    else
    {
        //
        // step 4.3) Enable VR regime with CPM
        //           CPM helps improve VF lwrve with NDIV offset margining thereby
        //           improving performace
        //
        // Today, we realistically land in this case when the target frequency
        // is same as Fmax @ Varb. When CPM is disabled (@ref PMU_PERF_NAFLL_CPM),
        // VR_WITH_CPM regime is equivalent to VR regime in HW and in an ideal
        // scenario the only difference is SW CLFC volt delta should be equal to 0.
        //
        targetRegimeId = LW2080_CTRL_CLK_NAFLL_REGIME_ID_VR_WITH_CPM;
    }

clkNafllDevTargetRegimeGet_30_exit:
    return targetRegimeId;
}

/*!
 * @brief Helper interface to callwlate the required PLDIV configuration at the
 *        target frequency for a given NAFLL
 *
 * @param[in]   pNafllDev  Pointer to the NAFLL device
 * @param[in]   pStatus    Pointer to the dynamic status param of NAFLL device.
 *
 * @return FLCN_OK
 */
FLCN_STATUS
clkNafllPldivConfig_30
(
    CLK_NAFLL_DEVICE                         *pNafllDev,
    LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus
)
{
    // Set PLDIV to default value
    pStatus->pldiv = CLK_NAFLL_PLDIV_DIV1;

    // If needed, callwlate the PLDIV value.
    if (pStatus->regime.targetRegimeId ==
            LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR_BELOW_DVCO_MIN)
    {
        if (pStatus->regime.targetFreqMHz >= pStatus->dvco.minFreqMHz)
        {
            // This should not happen expect if client override the regime.
            pStatus->pldiv = CLK_NAFLL_PLDIV_DIV1;
        }
        else if ((pStatus->regime.targetFreqMHz * CLK_NAFLL_PLDIV_DIV2) >= pStatus->dvco.minFreqMHz)
        {
            pStatus->pldiv = CLK_NAFLL_PLDIV_DIV2;
        }
        else
        {
            //
            // Max allowed PLDIV = 2 on GA100+
            // Bug 2674144
            // If we hit this break point VBIOS POR is incorrect and we have
            // DVCO Min < P8.min x 2
            //
            pStatus->pldiv = CLK_NAFLL_PLDIV_DIV2;

            //
            // PP-TODO : This is a temp code in bringup branch. In production, we will
            // remove this debug code to update PMU mailbox and replace HALT by BREAKPOINT.
            //
            // REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(10), pStatus->dvco.minFreqMHz);
            // REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(11), pStatus->regime.targetFreqMHz);
            // PMU_HALT();
        }
    }

    return LW_OK;
}

/*!
 * @brief Engage/Disengage the PLDIV at the target frequency for a given NAFLL
 *
 * @param[in]   pNafllDev  Pointer to the NAFLL device
 * @param[in]   pStatus    Pointer to the dynamic status param of NAFLL device.
 *
 * @return FLCN_OK
 */
FLCN_STATUS
clkNafllPldivProgram_30
(
    CLK_NAFLL_DEVICE                         *pNafllDev,
    LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus
)
{
    FLCN_STATUS status    = LW_OK;
    LwU8        prevPldiv = clkNafllPldivGet_HAL(&Clk, pNafllDev);

    // Halt if neither current or target regime is BELOW_DVCO_MIN
    if ((pStatus->regime.lwrrentRegimeId !=
            LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR_BELOW_DVCO_MIN) &&
        (pStatus->regime.targetRegimeId  !=
            LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR_BELOW_DVCO_MIN))
    {
        status = FLCN_ERR_ILLEGAL_OPERATION;
        PMU_BREAKPOINT();
        goto clkNafllPldivProgram_30_exit;
    }

    // Redundant requst are unexpected.
    if (pStatus->pldiv == prevPldiv)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkNafllPldivProgram_30_exit;
    }

    if (pStatus->pldiv < prevPldiv)
    {
        //
        // If PLDIV is decreasing then poll for FREQLOCK (bug 2062195) or wait
        // long enough for the NAFLL to lock to the LUT-limited NDIV (~20us).
        //
        // TODO: Add FREQLOCK Poll instead of hardcoded delay
        //
        OS_PTIMER_SPIN_WAIT_US(NAFLL_PLDIV_FREQLOCK_DELAY_US);
    }

    status = clkNafllPldivSet_HAL(&Clk, pNafllDev, pStatus->pldiv);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkNafllPldivProgram_30_exit;
    }

clkNafllPldivProgram_30_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Program target frequency at the given regime
 *
 * Note: This function doesn't engage/disengage the PLDIV and the frequency
 * controllers. This function also takes a offsetFreqMHz as a parameter.
 *
 * @param[in]       pNafllDev   Pointer to the NAFLL device
 * @param[in/out]   pStatus     Pointer to the NAFLL device dynamic status
 *
 * @return      FLCN_OK         Successfully programmed the frequency
 */
static FLCN_STATUS
_clkNafllProgram_30
(
    CLK_NAFLL_DEVICE                         *pNafllDev,
    LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus
)
{
    LwU32   status        = FLCN_OK;
    LwU16   outputFreqMHz = pStatus->regime.targetFreqMHz;
    LwU8    hwMapRegime;

    status = clkNafllGetHwRegimeBySwRegime(pStatus->regime.targetRegimeId, &hwMapRegime);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto _clkNafllProgram_30_exit;
    }

    // For BELOW_DVCO_MIN case set Fprogram = PLDIV x Ftarget
    if (pStatus->regime.targetRegimeId ==
            LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR_BELOW_DVCO_MIN)
    {
        outputFreqMHz = pStatus->pldiv * pStatus->regime.targetFreqMHz;
    }

    //
    // Step 5) Program LUT override
    //            Get complete LUT entry tuple - NDIV_OFFSET and DVCO_OFFSET using VF tuple look up interface
    //            set CPM ndiv MAX offset to ZERO.
    //            Update LUT_SW_FREQ_REQ register with new target ndiv, ndivOffset, DVCOOffset, CPM max determined above.
    //
    status = clkNafllLutOverride(pNafllDev, hwMapRegime, outputFreqMHz);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto _clkNafllProgram_30_exit;
    }

_clkNafllProgram_30_exit:
    return status;
}
