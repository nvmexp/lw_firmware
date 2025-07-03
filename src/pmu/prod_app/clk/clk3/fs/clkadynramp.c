/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see     https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author  Chandrabhanu Mahapatra
 */

#include "clk3/clk3.h"
#include "clk3/fs/clkadynramp.h"
#include "clk3/generic_dev_trim.h"
#include "osptimer.h"

/*******************************************************************************
    Virtual Table
*******************************************************************************/

CLK_DEFINE_VTABLE__FREQSRC(ADynRamp);


/*******************************************************************************
    Constants and Structure
*******************************************************************************/

#define CLK_RETRY_COUNT_MAX_PLL 5


/*******************************************************************************
    Protected Macros
*******************************************************************************/


/*******************************************************************************
    Protected Function Prototypes
*******************************************************************************/

//
// Functions here that are inlined are called only once.  This reduces stack
// pressure without increasing the IMEM size.
//
static LW_INLINE FLCN_STATUS clkProgram_ADynRamp_APll(ClkADynRamp *this, ClkPhaseIndex phase);
static LW_INLINE void clkProgram_Sdm(ClkADynRamp *this, ClkPhaseIndex phase);


/*******************************************************************************
    Reading
*******************************************************************************/

/*!
 * @see         ClkFreqSrc_Virtual::clkRead
 * @memberof    ClkADynRamp
 * @brief       Read the coefficients from hardware
 *
 * @details     Determine the current selected input by reading the hardware,
 *              then daisy-chain through the input node to get the frequency.
 *              Set all values of the phase array to align with the current
 *              hardware state.
 *
 *              The bActive memeber of the phase array, however, is an exception
 *              to this.  clkRead sets the bActive member of phase 0 to match
 *              the hardware. For all subsequent phases, however, it is set to
 *              false.
 *
 * @return      The status returned from calling clkRead on the input
 */
FLCN_STATUS
clkRead_ADynRamp
(
    ClkADynRamp *this,
    ClkSignal   *pOutput,
    LwBool       bActive
)
{
    LwU32            cfgRegData;
    LwU32            cfg2RegData;
    LwU32            ssd0RegData;
    FLCN_STATUS      status;
    ClkADynRampPhase adynRampPhase;
    ClkAPllPhase     apllPhase;
    ClkPhaseIndex    p;

    // Read PLL coefficients
    status = clkRead_APll(&this->super, pOutput, bActive);
    if (status != FLCN_OK)
    {
        return status;
    }

    //
    // If the pll is on, then read the coefficients from hardware, and then
    // read the input
    //
    cfgRegData = CLK_REG_RD32(this->super.cfgRegAddr);
    cfg2RegData = CLK_REG_RD32(this->cfg2RegAddr);
    if (FLD_TEST_DRF(_PTRIM, _SYS_PLL_CFG, _ENABLE, _YES, cfgRegData) &&
        FLD_TEST_DRF(_PVTRIM, _SYS_PLL_CFG2, _SSD_EN_SDM, _YES, cfg2RegData))
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_DEFINE_LIB_LW64
        };

        ssd0RegData = CLK_REG_RD32(this->ssd0RegAddr);
        adynRampPhase.sdm = DRF_VAL(_PVTRIM, _SYS_PLL_SSD0, _SDM_DIN, ssd0RegData);

        apllPhase = this->super.phase[0];

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            pOutput->freqKHz =
                (LwU32)(((LwU64)apllPhase.inputFreqKHz *
                         (apllPhase.ndiv * 8192 + 4096 + adynRampPhase.sdm)) /
                        (apllPhase.mdiv * apllPhase.pldiv * 8192));
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }
    else
    {
        adynRampPhase.sdm = 0xF000;
    }

    // Update the phase array
    for (p = 0; p < CLK_MAX_PHASE_COUNT; p++)
    {
        this->phase[p] = adynRampPhase;
    }

    // Done
    return status;
}


/*******************************************************************************
    Configuration
*******************************************************************************/

/*!
 * @see         ClkFreqSrc_Virtual::clkConfig
 * @memberof    ClkADynRamp
 * @brief       Configure the hardware to be programmed to a certain frequency
 *
 * @details     Loop through different combinations of pl, n and m to determine
 *              which will result in the output closest to the desired
 *              frequency.  Set this phase and all subsequent phases of the
 *              phase array to these values.
 *
 *              The bActive member of the phase array behaves differently
 *              however, as calling clkConfig_ADynRamp on phase n only sets the
 *              value of the phase array for that phase.  For phases greater
 *              than n, bActive remains unchanged.
 *
 */
FLCN_STATUS
clkConfig_ADynRamp
(
    ClkADynRamp            *this,
    ClkSignal              *pOutput,
    ClkPhaseIndex           phase,
    const ClkTargetSignal  *pTarget,
    LwBool                  bHotSwitch
)
{
    ClkADynRampPhase adynRampPhase;
    ClkAPllPhase     apllPhase;
    ClkPhaseIndex    p;
    LwS16            testsdm;
    FLCN_STATUS      status = LW_OK;
    LwU32            deltaKHz = 0xffffffff;
    LwU32            outputFreqKHz;

    // Configure PLL
    status = clkConfig_APll(&this->super, pOutput, phase, pTarget, bHotSwitch);
    if (status != FLCN_OK)
    {
        return status;
    }

    // save the best configuration
    apllPhase = this->super.phase[phase];

    //
    // With SDM output frequency closer to target can be acheived
    // pTarget->super.freqKHz = (inputFreqKHz * (ndiv + ((sdm + 4096)/8192))) / (mdiv * pldiv)
    //
    // callwlate SDM
    adynRampPhase.sdm  = (LwS16)((((LwU64)pTarget->super.freqKHz * apllPhase.mdiv * apllPhase.pldiv * 8192) /
                                         apllPhase.inputFreqKHz) -
                                         apllPhase.ndiv * 8192 - 4096);

    // find and set the best sdm value i.e. one minimum frequency delta and where outputFreqKHz >= pTarget->super.freqKHz
    for (testsdm = adynRampPhase.sdm; testsdm <= adynRampPhase.sdm + 1; testsdm++)
    {
        // callwlate output frequency
        outputFreqKHz = (LwU32)(((LwU64)apllPhase.inputFreqKHz *
                                      (apllPhase.ndiv * 8192 + 4096 + testsdm)) /
                                      (apllPhase.mdiv * apllPhase.pldiv * 8192));

        if (LW_VALUE_WITHIN_INCLUSIVE_RANGE(pTarget->rangeKHz, outputFreqKHz) &&
           (outputFreqKHz >= pTarget->super.freqKHz) &&
            LW_DELTA(outputFreqKHz, pTarget->super.freqKHz) < deltaKHz)
        {
            adynRampPhase.sdm = testsdm;

            //callwlate frequency delta w.r.t to target frequency
            deltaKHz = LW_DELTA(outputFreqKHz, pTarget->super.freqKHz);

            // The output of the VCO is based on the input.
            pOutput->freqKHz = outputFreqKHz;
        }
    }

    // Make sure target range is never violated for the above sdm value
    if (deltaKHz == 0xffffffff)
    {
        status = FLCN_ERR_FREQ_NOT_SUPPORTED;
        goto clkConfig_ADynRamp_exit;
    }

    // Make sure that sdm value is lower than the max supported value
    while (adynRampPhase.sdm > LW_PVTRIM_SYS_PLL_SSD0_SDM_DIN_MAX && apllPhase.ndiv < this->super.pPllInfoTableEntry->MaxN)
    {
        apllPhase.ndiv++;
        adynRampPhase.sdm -= 8192;
    }

    // Make sure that sdm value is higher than the min supported value
    while (adynRampPhase.sdm < LW_PVTRIM_SYS_PLL_SSD0_SDM_DIN_MIN && apllPhase.ndiv > this->super.pPllInfoTableEntry->MinN)
    {
        apllPhase.ndiv--;
        adynRampPhase.sdm += 8192;
    }

    // return if target frequency cannot be achieved without violating sdm constraints
    if (adynRampPhase.sdm > LW_PVTRIM_SYS_PLL_SSD0_SDM_DIN_MAX || adynRampPhase.sdm < LW_PVTRIM_SYS_PLL_SSD0_SDM_DIN_MIN)
    {
        status = FLCN_ERR_FREQ_NOT_SUPPORTED;
        goto clkConfig_ADynRamp_exit;
    }

    // Update the phase array
    for (p = phase; p < CLK_MAX_PHASE_COUNT; p++)
    {
        this->phase[p]       = adynRampPhase;
        this->super.phase[p] = apllPhase;
    }

clkConfig_ADynRamp_exit:
    return status;
}


/*******************************************************************************
    Programming
*******************************************************************************/

/*!
 * @see         ClkFreqSrc_Virtual::clkProgram
 * @brief       Program the pll
 */
FLCN_STATUS
clkProgram_ADynRamp
(
    ClkADynRamp    *this,
    ClkPhaseIndex   phase
)
{
    if (this->super.phase[phase - 1].bActive)
    {
        FLCN_STATUS status;

        // Program the input (reference frequency)
        status = clkProgram_Wire(&this->super.super, phase);
        if (status != FLCN_OK)
        {
            return status;
        }

        // If the PLL is active, ramp up or down NDIV
        return clkProgram_ADynRamp_APll(this, phase);
    }
    else
    {
        // program the sdm value
        clkProgram_Sdm(this, phase);

        // Program the rest of PLL
        return clkProgram_APll(&this->super, phase);
    }
}


/*!
 * @memberof    ClkADynRamp
 * @brief       Dynamically ramp the NDIV up or down to the target value
 * @see         https://wiki.lwpu.com/engwiki/index.php/VLSI/Mixed_Signal_PLL_Design/Dynamic_frequency_ramp_up/down
 * @protected
 *
 * @details     If the PLL is being programmed while active (hot), then this
 *              function is used to dynamically ramp up or down the NDIV and
 *              SDM_DIN value. Then there is a waiting period for dynamic ramp
 *              to get reflected.
 *
 *              However, the MDIV and PLDIV values must be unchanged from the
 *              previous phase.
 *
 * @post        On success, NDIV has reached the target value in hardware.
 *
 * @param[in]   this    the dynamic ramp pll object to program
 * @param[in]   phase   the phase to program the pll object to.
 */
static LW_INLINE FLCN_STATUS clkProgram_ADynRamp_APll
(
    ClkADynRamp    *this,
    ClkPhaseIndex   phase
)
{
    LwU32 cfgRegData;
    LwU32 cfg2RegData;
    LwU32 coeffRegData;
    LwU32 ssd0RegData;
    LwU32 dynRegData;
    LwU8 ndiv;
    LwS16 sdm;

    // The MDIV and PLDIV can not be programmed while active.
    PMU_HALT_COND(this->super.phase[phase].mdiv  == this->super.phase[phase - 1].mdiv);
    PMU_HALT_COND(this->super.phase[phase].pldiv == this->super.phase[phase - 1].pldiv);

    // Determine the value of NDIV.
    coeffRegData = CLK_REG_RD32(this->super.coeffRegAddr);
    ndiv = DRF_VAL(_PTRIM, _SYS_APLL_COEFF, _NDIV, coeffRegData);

    // Determine the value of SDM.
    ssd0RegData = CLK_REG_RD32(this->ssd0RegAddr);
    sdm = DRF_VAL(_PVTRIM, _SYS_PLL_SSD0, _SDM_DIN, ssd0RegData);

    //
    // Read the current configuration.  The config register is read/written
    // inside the retry loop to ensure that there are no pending register
    // transations after PLL power down.  The additional delay can't hurt.
    //
    cfgRegData = CLK_REG_RD32(this->super.cfgRegAddr);
    cfg2RegData = CLK_REG_RD32(this->cfg2RegAddr);
    dynRegData = CLK_REG_RD32(this->dynRegAddr);

    if (ndiv != this->super.phase[phase].ndiv ||
        sdm  != this->phase[phase].sdm)
    {
        // Program NDIV_NEW.
        coeffRegData = FLD_SET_DRF_NUM(_PVTRIM, _SYS_PLL_COEFF, _NDIV_NEW, this->super.phase[phase].ndiv, coeffRegData);
        CLK_REG_WR32(this->super.coeffRegAddr, coeffRegData);

        //
        // Enable SDM so that it works.
        // EN_SDM=1, EN_SSC=0, EN_DITHER=1, bug <200107727>.
        //
        cfg2RegData = FLD_SET_DRF(_PVTRIM, _SYS_PLL_CFG2, _SSD_EN_SDM,    _YES, cfg2RegData);
        cfg2RegData = FLD_SET_DRF(_PVTRIM, _SYS_PLL_CFG2, _SSD_EN_SSC,     _NO, cfg2RegData);
        cfg2RegData = FLD_SET_DRF(_PVTRIM, _SYS_PLL_CFG2, _SSD_EN_DITHER, _YES, cfg2RegData);
        CLK_REG_WR32(this->cfg2RegAddr, cfg2RegData);

        // Program SDM_DIN_NEW.
        dynRegData = FLD_SET_DRF_NUM(_PVTRIM, _SYS_PLL_DYN, _SDM_DIN_NEW, this->phase[phase].sdm, dynRegData);
        CLK_REG_WR32(this->dynRegAddr, dynRegData);

        // Seting EN_DYNRAMP = 1.
        cfgRegData = FLD_SET_DRF(_PVTRIM, _SYS_PLL_CFG, _EN_PLL_DYNRAMP, _YES, cfgRegData);
        CLK_REG_WR32(this->super.cfgRegAddr, cfgRegData);

        //
        // Wait the specific amount of time (typically 500000ns) for dynramp to complete.
        //
        OS_PTIMER_SPIN_WAIT_NS(this->dynRampNs);

        cfgRegData = CLK_REG_RD32(this->super.cfgRegAddr);
        if (DRF_VAL(_PVTRIM, _SYS_PLL_CFG, _DYNRAMP_DONE, cfgRegData) == LW_PVTRIM_SYS_PLL_CFG_DYNRAMP_DONE_TRUE)
        {
            // Update NDIV_NEW.
            coeffRegData = FLD_SET_DRF_NUM(_PTRIM, _SYS_APLL_COEFF, _NDIV, this->super.phase[phase].ndiv, coeffRegData);
            CLK_REG_WR32(this->super.coeffRegAddr, coeffRegData);

            // Update the SDM value.
            ssd0RegData = FLD_SET_DRF_NUM(_PVTRIM, _SYS_PLL_SSD0, _SDM_DIN, this->phase[phase].sdm, ssd0RegData);
            CLK_REG_WR32(this->ssd0RegAddr, ssd0RegData);

            // Setting EN_DYNRAMP = 0.
            cfgRegData = FLD_SET_DRF(_PVTRIM, _SYS_PLL_CFG, _EN_PLL_DYNRAMP, _NO, cfgRegData);
            CLK_REG_WR32(this->super.cfgRegAddr, cfgRegData);
        }
        else
        {
            dbgPrintf(LEVEL_ALWAYS, "Dynamic ramp completion timeout\n");
            return FLCN_ERR_TIMEOUT;
        }
    }

    // Done.
    return FLCN_OK;
}


/*!
 * @memberof    ClkADynRamp
 * @brief       Enable and program SDM_DIN value
 * @protected
 *
 * @param[in]   this    the dynamic ramp pll object to program
 * @param[in]   phase   the phase to program the pll object to.
 */
static LW_INLINE void clkProgram_Sdm
(
    ClkADynRamp    *this,
    ClkPhaseIndex   phase
)
{

    LwU32 cfgRegData;
    LwU32 cfg2RegData;
    LwU32 ssd0RegData;

    //
    // Read the current configuration.  The config register is read/written
    // inside the retry loop to ensure that there are no pending register
    // transations after PLL power down.  The additional delay can't hurt.
    //
    cfgRegData = CLK_REG_RD32(this->super.cfgRegAddr);
    cfg2RegData = CLK_REG_RD32(this->cfg2RegAddr);
    ssd0RegData = CLK_REG_RD32(this->ssd0RegAddr);

    // Disable sync mode, power on the PLL making sure it is disabled.
    cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _IDDQ,      _POWER_ON, cfgRegData);
    cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _SYNC_MODE, _DISABLE,  cfgRegData);
    cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _ENABLE,    _NO,       cfgRegData);
    CLK_REG_WR32(this->super.cfgRegAddr, cfgRegData);

    // write the SDM value
    ssd0RegData = FLD_SET_DRF_NUM(_PVTRIM, _SYS_PLL_SSD0, _SDM_DIN, this->phase[phase].sdm, ssd0RegData);
    CLK_REG_WR32(this->ssd0RegAddr, ssd0RegData);

    //
    // Enable SDM so that it works
    // EN_SDM=1, EN_SSC=0, EN_DITHER=1, bug <200107727>
    //
    cfg2RegData = FLD_SET_DRF(_PVTRIM, _SYS_PLL_CFG2, _SSD_EN_SDM,    _YES, cfg2RegData);
    cfg2RegData = FLD_SET_DRF(_PVTRIM, _SYS_PLL_CFG2, _SSD_EN_SSC,     _NO, cfg2RegData);
    cfg2RegData = FLD_SET_DRF(_PVTRIM, _SYS_PLL_CFG2, _SSD_EN_DITHER, _YES, cfg2RegData);
    CLK_REG_WR32(this->cfg2RegAddr, cfg2RegData);
}


/*******************************************************************************
    Clean-Up
*******************************************************************************/

/*!
 * @see         ClkFreqSrc_Virtual::clkCleanup
 * @memberof    ClkADynRamp
 * @brief       Power down the PLL if no longer used.
 *
 * @details     clkCleanup functions usually have the post condition that
 *              the last element of the phase array is copied to all other
 *              phases of the phase array.  This is true for this function
 *              as well with the exception of the bActive member of the phase
 *              struct.  The bActive member is set to false for all phases
 *              except phase 0 where it is set to the value of the bActive
 *              input.
 */
FLCN_STATUS
clkCleanup_ADynRamp
(
    ClkADynRamp *this,
    LwBool       bActive
)
{
    FLCN_STATUS status;
    ClkPhaseIndex p;

    // If this PLL is not part of the active path, turn it off.
    if (!bActive)
    {
        // Read the configuration register to preserver other flags
        LwU32 cfg2RegData = CLK_REG_RD32(this->cfg2RegAddr);

        // Disable SDM
        cfg2RegData = FLD_SET_DRF(_PVTRIM, _SYS_PLL_CFG2, _SSD_EN_SDM,    _NO, cfg2RegData);
        cfg2RegData = FLD_SET_DRF(_PVTRIM, _SYS_PLL_CFG2, _SSD_EN_SSC,    _NO, cfg2RegData);
        cfg2RegData = FLD_SET_DRF(_PVTRIM, _SYS_PLL_CFG2, _SSD_EN_DITHER, _NO, cfg2RegData);
        CLK_REG_WR32(this->cfg2RegAddr, cfg2RegData);
    }

    // Cleanup the input as needed.
    status = clkCleanup_APll(&this->super, bActive);

    //
    // Update the pll's phase array to reflect the last phase
    // Only phase 0 can have the bActive flag set, however
    //
    for (p = 0; p < CLK_MAX_PHASE_COUNT - 1; p++)
    {
        this->phase[p] = this->phase[CLK_MAX_PHASE_COUNT - 1];
    }

    return status;
}


/*******************************************************************************
    Print
*******************************************************************************/

/*!
 * @see         ClkFreqSrc_Virtual::clkPrint
 * @brief       Print debugging info
 *
 * @memberof    ClkADynRamp
 *
 * @param[in]   this        Object to print
 * @param[in]   phaseCount  Number of phases for config/program
 */
#if CLK_PRINT_ENABLED
void
clkPrint_ADynRamp
(
    ClkADynRamp     *this,
    ClkPhaseIndex    phaseCount
)
{
    // Print the `super` PLL data along with this data instead of daisy-chaining
    // so that the information is better organized for the user.
    LwU8 id = (super.pPllInfoTableEntry ? super.pPllInfoTableEntry->id : p);
    CLK_PRINTF("PLL with Dynamic Ramp support: id=0x%02x source=0x%02x retryCount=%u\n",
            (unsigned) id, (unsigned) this->super.source, (unsigned) this->super.retryCount);

    // Print each programming phase.
    for(ClkPhaseIndex p = 0; p < phaseCount; ++p)
    {
        // Zero for a nonexistent PLDIV.
        // 2 for special DIV2 divider (instead of PLDIV).
        // See the schematic DAG to distinguish between DIV2 v. PLDIV when it prints 2.
        LwU8 pldiv;
        if (this->super.bDiv2Enable)
            pldiv = 2;
        else if (this->super.bPldivEnable)
            pldiv = 0;
        else
            pldiv = this->super.phase[p].pldiv;

        // Cast 'sdm' to (int) to get a sign-extended decimal value.
        // Cast 'sdm' to (unsigned) to get the pure hex value.
        CLK_PRINTF("PLL phase %u: mdiv=%u ndiv=%u pdiv=%u sdm=%d=0x%04x active=%u input=%uKHz\n",
            (unsigned) p,
            (unsigned) this->super.phase[p].mdiv,
            (unsigned) this->super.phase[p].ndiv,
            (unsigned) pldiv,
            (int) this->phase[p].sdm, (unsigned) this->phase[p].sdm,
            (unsigned) this->super.phase[p].bActive,
            (unsigned) this->super.phase[p].inputFreqKHz);
    }

    // Read and print hardware state.
    CLK_PRINT_REG("PLL with Dynamic Ramp support:   CFG", this->super.cfgRegAddr);
    CLK_PRINT_REG("PLL with Dynamic Ramp support: COEFF", this->super.coeffRegAddr);
    CLK_PRINT_REG("PLL with Dynamic Ramp support:  CFG2", this->cfg2RegAddr);
    CLK_PRINT_REG("PLL with Dynamic Ramp support:  SSD0", this->ssd0RegAddr);
    CLK_PRINT_REG("PLL with Dynamic Ramp support:   DYN", this->dynRegAddr);

    // Print the input object information.
    clkPrint_Wire(&this->super, phaseCount);
}
#endif      // CLK_PRINT_ENABLED
