/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see     https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author  Eric Colter
 * @author  Antone Vogt-Varvak
 * @author  Ming Zhong
 * @author  Prafull Gupta
 */

/* ------------------------- System Includes -------------------------------- */

#include "osptimer.h"

/* ------------------------- Application Includes --------------------------- */

#include "clk3/clk3.h"
#include "clk3/fs/clkapll.h"
#include "clk3/generic_dev_trim.h"

/* ------------------------- Macros and Defines ----------------------------- */

#define DELAY_READY_NS 5000
#define CLK_RETRY_COUNT_MAX_PLL 5

/*!
 * @brief: True if the new config is better than the old (prioritize accuracy)
 */
#define CONFIG_COMPARE(new, old)                                               \
     ((new).deltaKHz            <   (old).deltaKHz          ||                 \
     ((new).deltaKHz            ==  (old).deltaKHz          &&                 \
      (new).status == FLCN_OK   &&  (old).status != FLCN_OK))


/* ------------------------- Type Definitions ------------------------------- */

typedef struct
{
/*!
 * @brief   Path from this node to the clock source
 */
    ClkSignalPath path;

/*!
 * @brief   Delta from target frequency in KHz
 */
    LwU32       deltaKHz;

/*!
 * @brief   Input frequency required
 */
    LwU32       inputKHz;

/*!
 * @brief   Status (OK or WARN) associated with this config
 */
    FLCN_STATUS status;

/*!
 * @brief   LW_PTRIM_SYS_PLL_COEFF_MDIV
 */
    LwU8        mdiv;

/*!
 * @brief   LW_PTRIM_SYS_PLL_COEFF_NDIV
 */
    LwU8        ndiv;

/*!
 * @brief   LW_PTRIM_SYS_PLL_COEFF_PLDIV
 */
    LwU8        pldiv;

} ClkConfigResult_APll;


/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

CLK_DEFINE_VTABLE__FREQSRC(APll);

// Initailizing the ConfigResultStructure to be the worst case
const ClkConfigResult_APll clkReset_ConfigResult_APll =
{
    .path =             CLK_SIGNAL_PATH_EMPTY,
    .deltaKHz =         0xffffffff,
    .inputKHz =         0xffffffff,
    .status =           FLCN_ERR_FREQ_NOT_SUPPORTED,
    .mdiv =             0,
    .ndiv =             0,
    .pldiv =            0,
};

/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */

//
// Functions here that are inlined are called only once.  This reduces stack
// pressure without increasing the IMEM size.
//
static LW_INLINE void clkConfig_FindBest_APll(ClkAPll *this, const ClkTargetSignal *pTarget, ClkConfigResult_APll *pBest, ClkPhaseIndex phase);
static LW_INLINE void clkConfig_FindBest_VariableInput_APll(ClkAPll *this, const ClkTargetSignal *pTarget, ClkConfigResult_APll *pBest, ClkPhaseIndex phase);
static LW_INLINE void clkConfig_FindBest_FixedInput_APll(ClkAPll *this, const ClkTargetSignal *pTarget, ClkConfigResult_APll *pBest, ClkPhaseIndex phase);
static LW_INLINE FLCN_STATUS clkProgram_Direct_APll(ClkAPll *this, ClkPhaseIndex phase);
static LW_INLINE FLCN_STATUS clkProgram_Slide_APll(ClkAPll *this, ClkPhaseIndex phase);
LwBool clkProgram_IsLocked_APll(void *pArgs)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkProgram_IsLocked_APll");

/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Virtual Implemenations ------------------------- */

/*******************************************************************************
    Reading
*******************************************************************************/

/*!
 * @see         ClkFreqSrc_Virtual::clkRead
 * @memberof    ClkAPll
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
clkRead_APll
(
    ClkAPll     *this,
    ClkSignal   *pOutput,
    LwBool       bActive
)
{
    LwU32           cfgRegData;
    FLCN_STATUS     status;
    ClkAPllPhase    hardwarePhase;
    ClkPhaseIndex   p;

    // If this pll isn't on, set coefficients to be zero
    cfgRegData = CLK_REG_RD32(this->cfgRegAddr);
    if (FLD_TEST_DRF(_PTRIM, _SYS_PLL_CFG, _ENABLE, _NO, cfgRegData))
    {
        hardwarePhase.mdiv = 0;
        hardwarePhase.ndiv = 0;
        hardwarePhase.pldiv = 0;
        hardwarePhase.bActive = LW_FALSE;
        hardwarePhase.inputFreqKHz = 0;
        pOutput->freqKHz = 0;
        status = FLCN_OK;
    }

    //
    // If the pll is on, then read the coefficients from hardware, and then
    // read the input
    //
    else
    {
        //
        // Analog PLLs have one register for coefficients, Hybrids have two.
        // MDIV and PLDIV are in the same register/field for all PLL types.
        //
        LwU32 coeffRegData = CLK_REG_RD32(this->coeffRegAddr);
        hardwarePhase.mdiv = DRF_VAL(_PTRIM, _SYS_PLL_COEFF, _MDIV, coeffRegData);
        hardwarePhase.ndiv = DRF_VAL(_PTRIM, _SYS_APLL_COEFF, _NDIV, coeffRegData);

        //
        // DRAMPLL does not have a PLDIV.  Instead it has a SEL_DIVBY2 which acts
        // as a glitchy version of PLDIV that always divides by 2.
        //
        if (this->bDiv2Enable &&
            FLD_TEST_DRF(_PFB, _FBPA_PLL_COEFF, _SEL_DIVBY2, _ENABLE, coeffRegData))
        {
            hardwarePhase.pldiv = 2;
        }

        //
        // If this PLL contains a PLDIV, then get its value.
        //
        else if (this->bPldivEnable)
        {
            hardwarePhase.pldiv = DRF_VAL(_PTRIM, _SYS_PLL_COEFF, _PLDIV,  coeffRegData);
        }

        // This PLL has only a VCO, so set the divider value to 1.
        else
        {
            hardwarePhase.pldiv = 1;
        }

        hardwarePhase.bActive = LW_FALSE;

        status = clkRead_Wire(&this->super, pOutput, bActive);
        hardwarePhase.inputFreqKHz = pOutput->freqKHz;

        pOutput->freqKHz *= hardwarePhase.ndiv;
        pOutput->freqKHz /= hardwarePhase.mdiv;
        pOutput->freqKHz /= hardwarePhase.pldiv;
    }

    // Update the phase array
    for (p = 0; p < CLK_MAX_PHASE_COUNT; p++)
    {
        this->phase[p] = hardwarePhase;
    }

    //
    // Phase 0 is the only phase in which bActive reflects whether or not the
    // PLL is active.
    //
    this->phase[0].bActive = bActive;

    //
    // 'source' indicates which sort pf object is closest to the root:
    // PLL, NAFLL, XTAL, OSM; and if it is a PLL, whether it is the domain-
    // specific PLL (e.g. DISPPLL or HUBPLL) or one of the SPPLLs.  As such,
    // on the return, this logic overrides any previous 'source' setting
    // from a child in the schematic dag.
    //
    pOutput->source = this->source;

    // Done
    return status;
}


/*******************************************************************************
    Configuration
*******************************************************************************/

/*!
 * @see         ClkFreqSrc_Virtual::clkConfig
 * @memberof    ClkAPll
 * @brief       Configure the hardware to be programmed to a certain frequency
 *
 * @details     Loop through different combinations of pl, n and m to determine
 *              which will result in the output closest to the desired
 *              frequency.  Set this phase and all subsequent phases of the
 *              phase array to these values.
 *
 *              The bActive member of the phase array behaves differently
 *              however, as calling clkConfig_APll on phase n only sets the
 *              value of the phase array for that phase.  For phases greater
 *              than n, bActive remains unchanged.
 *
 */
FLCN_STATUS
clkConfig_APll
(
    ClkAPll                 *this,
    ClkSignal               *pOutput,
    ClkPhaseIndex            phase,
    const ClkTargetSignal   *pTarget,
    LwBool                   bHotSwitch
)
{
    //!< The best configuration found
    ClkConfigResult_APll bestConfig;

    //!< The state to set the phase array to
    ClkAPllPhase phaseInfo;

    //!< Iterator to loop through phase array
    ClkPhaseIndex p;

    // Everything seems good, find the best and set the phase array;
    clkConfig_FindBest_APll(this, pTarget, &bestConfig, phase);

    // Update the phase array
    phaseInfo.ndiv = bestConfig.ndiv;
    phaseInfo.mdiv = bestConfig.mdiv;
    phaseInfo.pldiv = bestConfig.pldiv;
    phaseInfo.bActive = LW_FALSE;
    phaseInfo.inputFreqKHz = bestConfig.inputKHz;
    for (p = phase; p < CLK_MAX_PHASE_COUNT; p++)
    {
        this->phase[p] = phaseInfo;
    }

    //
    // It is important to note that the bActive element of the phase array does
    // not represent the same thing as the bHotSwitch parameter to this function.
    //
    // bHotSwitch indicates whether or not this PLL will be in use while it is
    // being programmed.  bActive represents whether or not this PLL will be
    // part of the active signal path once this phase has finished programming.
    //
    // Because clkConfig is being called on this phase, this PLL will definitely
    // be part of the active path at the end of programming for this phase.
    // Thus, bActive is always be set to true.
    //
    // It is also important to note that bActive is set differently that other
    // members in the phase array.  Generally when configuring phase n, the phase
    // array for phase n and all phases > n are set to match the configuration.
    // When  configuring a PLL's phase array, however, the bActive member is only
    // set for phase n and is left alone for all phases > n.
    //
    this->phase[phase].bActive = LW_TRUE;

    //
    // Update status if and only if there was no error. This is to prevent
    // division by zero if mdiv or pldiv are not defined
    //
    if (bestConfig.status == FLCN_OK)
    {
        pOutput->freqKHz = bestConfig.inputKHz * bestConfig.ndiv /
            bestConfig.mdiv / bestConfig.pldiv;
        pOutput->fracdiv = LW_TRISTATE_FALSE;
        pOutput->path = bestConfig.path;
    }

    //
    // 'source' indicates which sort pf object is closest to the root:
    // PLL, NAFLL, XTAL, OSM; and if it is a PLL, whether it is the domain-
    // specific PLL (e.g. DISPPLL or HUBPLL) or one of the SPPLLs.  As such,
    // on the return, this logic overrides any previous 'source' setting
    // from a child in the schematic dag.
    //
    pOutput->source = this->source;

    // Done
    return bestConfig.status;
}


/*!
 * @memberof    ClkAPll
 * @brief       Loop through various configurations to find the best.
 * @version     Clocks 3.1 and after
 * @see         clkConfig_FindBest_VariableInput_APll
 * @see         clkConfig_FindBest_FixedInput_APll
 * @protected
 *
 * @details     This function selects the best configuration.
 *
 *              The actual frequency output of this object is:
 *              this->output.freqKHz = (inputKHz * ndiv) / (mdiv * pldiv);
 *
 *              'status' reflects the status of the input node.
 *
 *              If referenceRangeKHz.min == referenceRangeKHz.max or the input
 *              is locked, this function calls
 *              clkConfig_FindBest_FixedInput_APll. Otherwise, it calls
 *              clkConfig_FindBest_VariableInput_APll.
 *
 * @todo        Consider reducing the examined ranges of ndiv and mdiv.  The
 *              ndiv and mdiv range specified in the VBIOS info tables often
 *              times contain many values of m and n that will always result
 *              in violation of the ranges of one of the frequency ranges.
 *              For these PLLs, the PLL Info Tables Entries can have a more
 *              restrictive range than what is provided in the VBIOS to stop
 *              the clocks code from wasting time and considering these values.
 *              See //sw/docs/resman/presentations/clks/clk3/Reducing-range-of-m-and-n.docx
 *
 *              Its probably better, however if this is taken into consideration
 *              when initializing the PLL Info Table so it doesnt have to be
 *              recallwlated everytime clkConfig is called.
 *
 * @note        nolwirtual:     Function must be called directly (not using a pointer).
 * @note        protected:      Only functions within this class may call this function.
 *
 * @pre         The input node is connected in the schematic dag.
 *
 * @pre         Spread spectrum is not supported, thus target.spread must be off.
 *
 * @post        On successful return (FLCN_OK or warning), 'pBest' contains valid
 *              data and the input object has been configured.  If unsuccessful,
 *              it is unspecified whether or not members in 'pBest' other than
 *              'status' change.
 *
 * @ilwariant   This function neither reads nor writes any registers.
 *
 * @param[in]   this            Frequency source object pointer
 * @param[in]   pTarget         Target frequency, etc.
 * @param[in]   path            Signal path to be used by the input node
 * @param[out]  pBest           Best parameters to use for configuration
 */
static LW_INLINE void
clkConfig_FindBest_APll
(
    ClkAPll                 *this,
    const ClkTargetSignal   *pTarget,
    ClkConfigResult_APll    *pBest,
    ClkPhaseIndex            phase
)
{
    //
    // 'this->pPllInfoTableEntry' is initialized either at construction or
    // at compile-time, and cannot be NULL.  Do not insert a NULL check with
    // 'return' or similar.  If initialization fails and this is NULL, the
    // PMU halts (per CL 23619977), so there is no point in asserting here.
    // PLL table entries are generally less than 256 bytes in size.
    //

    if (CLK_IS_READ_ONLY__FREQSRC(&this->super.super) ||
        this->pPllInfoTableEntry->MinRef >= this->pPllInfoTableEntry->MaxRef)
    {
        clkConfig_FindBest_FixedInput_APll(this, pTarget, pBest, phase);
    }
    else
    {
        clkConfig_FindBest_VariableInput_APll(this, pTarget, pBest, phase);
    }
}


/*!
 * @memberof    ClkAPll
 * @brief       Loop through various configurations to find the best.
 * @version     Clocks 3.1 and after
 * @protected
 *
 * @details     This implementation find the best 'mdiv' and 'ndiv' values
 *              given that the input frequency is fixed.  This implementation
 *              is more efficient than clkConfig_FindBest_VariableInput_APll.
 *              The complexity of this implementation is generally O(n^2).
 *
 *              The actual frequency output of this object is:
 *              this->output.freqKHz = (inputKHz * ndiv) / (mdiv * pldiv);
 *
 *              'status' reflects the status of the input node.
 *
 *              This function is inlined to save stack space. Since it is called
 *              from only one place, inlining does not increase the binary size.
 *
 * @note        nolwirtual: Function must be called directly (not using a pointer).
 *
 * @pre         The input node is connected in the schematic dag.
 *
 * @pre         Spread spectrum is not supported, thus target.spread must be off.
 *
 * @post        On successful return (FLCN_OK or warning), 'pBest' contains valid
 *              data and the input object has been configured.  If unsuccessful,
 *              it is unspecified whether or not members in 'pBest' other than
 *              'status' change.
 *
 * @ilwariant   This function neither reads nor writes any registers.
 *
 * @param[in]   this            Frequency source object pointer
 * @param[in]   pTarget         Target frequency, etc.
 * @param[in]   path            Signal path to be used by the input node
 * @param[out]  pBest           Best parameters to use for configuration
 */
static LW_INLINE void
clkConfig_FindBest_FixedInput_APll
(
    ClkAPll                 *this,
    const ClkTargetSignal   *pTarget,
    ClkConfigResult_APll    *pBest,
    ClkPhaseIndex            phase
)
{
    //
    // In order to have wound up in this function, the input is either:
    // 1. Fixed in place by the VBIOS and needs to be configured
    // 2. Is "read only" for example a crystal.
    //
    // The input is configured to be the max of the reference range because in
    // case 1, the max is equal to the min, and this is the required frequency.
    // In case 2, it doesn't matter because the configuration will just fail to
    // change the frequency and will instead return the actual frequency.

    ClkTargetSignal inputTarget;
    ClkSignal inputSignal;
    ClkConfigResult_APll test;

    // Allowed ranges
    LwRangeU8  mdivRange;
    LwRangeU8  ndivRange;
    LwRangeU8  pldivRange;
    LwRangeU32 vcoRangeKHz;

    //
    // An extra large range object to do math on without having to worry about
    // overflow
    //
    LwRangeU32 range;

    // Aliases which do not change their values
    const LwRangeU32 updateRangeKHz =
    {
        .min = this->pPllInfoTableEntry->MinUpdate * 1000,
        .max = this->pPllInfoTableEntry->MaxUpdate * 1000,
    };

    const LwRangeU32 inputRangeKHz =
    {
        .min = this->pPllInfoTableEntry->MinRef * 1000,
        .max = this->pPllInfoTableEntry->MaxRef * 1000,
    };

    // Callwlated allowed vco frequency
    LwU32 vcoKHz;

    // Copy the target information
    inputTarget = *pTarget;
    inputTarget.super.freqKHz = this->pPllInfoTableEntry->MaxRef * 1000;
    inputTarget.rangeKHz.min  = this->pPllInfoTableEntry->MinRef * 1000;
    inputTarget.rangeKHz.max  = this->pPllInfoTableEntry->MaxRef * 1000;

    // Disable fracdiv as PLLs must have 50% duty cycle.  Disable spread
    inputTarget.super.fracdiv = LW_TRISTATE_FALSE;

    // Initialize pBest to the worst possible configuration
    *pBest = clkReset_ConfigResult_APll;

    // Configure the input (Use Wire function to take care of cycles)
    test.status = clkConfig_Wire(&this->super, &inputSignal, phase,
        &inputTarget, LW_FALSE);

    //
    // The input is static LW_INLINE, so if it returns an error, something
    // about it is wrong. Just give up and make sure the error code makes it
    // to the calling function.
    //
    if (test.status != FLCN_OK)
    {
        pBest->status = test.status;
        return;
    }

    // Colwert the VCO range to KHz.
    vcoRangeKHz.min = this->pPllInfoTableEntry->MilwCO * 1000;
    vcoRangeKHz.max = this->pPllInfoTableEntry->MaxVCO * 1000;

    // Aquire the input frequency
    test.inputKHz = inputSignal.freqKHz;
    test.path = inputSignal.path;

    //
    // Initialize the coefficient ranges from the PLL Info Table.  Narrow the
    // m and n div ranges to remove all values that will always result in
    // invalid configurations. See
    // /sw/docs/resman/presentations/clks/clk3/Reducing-range-of-m-and-n.docx
    //
    // TODO: Implement range narrowing on initialization instead.
    //
    mdivRange.min = this->pPllInfoTableEntry->MinM;
    mdivRange.max = this->pPllInfoTableEntry->MaxM;
    range.max = inputRangeKHz.max / updateRangeKHz.min;
    range.min = LW_UNSIGNED_DIV_CEIL(inputRangeKHz.min, updateRangeKHz.max);
    LW_ASSIGN_INTERSECTION_RANGE(mdivRange, range);

    ndivRange.min = this->pPllInfoTableEntry->MinN;
    ndivRange.max = this->pPllInfoTableEntry->MaxN;
    range.max = vcoRangeKHz.max / updateRangeKHz.min;
    range.min = LW_UNSIGNED_DIV_CEIL(vcoRangeKHz.min, updateRangeKHz.max);
    LW_ASSIGN_INTERSECTION_RANGE(ndivRange, range);

    // Similarly for the PLDIV unless it's disabled.
    if (this->bPldivEnable)
    {
        pldivRange.min = this->pPllInfoTableEntry->MinPl;
        pldivRange.max = this->pPllInfoTableEntry->MaxPl;
    }
    else
    {
        pldivRange.max = 1;
        pldivRange.min = 1;
    }

    //
    // Disallow changing of m and pl if we are programming hot We are hot if
    // the previous phase was hot.  This is because if this pll was in use
    // last phase and is being configured this phase, then it is being used
    // two phases in a row.  When a pll is only used one phase in a row, it is
    // programmed cold, before a mux switches it to be the active output.
    // When a pll is being used two phases in a row, there is no way a mux
    // above could be switched, so it must be being programmed hot.
    //
    if (this->phase[phase - 1].bActive)
    {
        pldivRange.max = pldivRange.min = this->phase[phase - 1].pldiv;
        mdivRange.max  = mdivRange.min  = this->phase[phase - 1].mdiv;
    }

    //
    // Determine values of mdiv that don't violate the updateRange
    //      fUpdate = fInput / m
    //      fUpdate_min <= (fInput / m) && (fInput / m) <= fUpdate_max
    //      m <= (fInput / fUpdate_min) && (fInput / fUpdate_max) <= m
    //
    // So the value of m must be within these two inclusive ranges:
    //      [(fInput / fUpdate_max), (fInput / fUpdate_min)]
    //      [m_min, m_max]
    //
    range.max = inputSignal.freqKHz / updateRangeKHz.min;
    range.min = LW_UNSIGNED_DIV_CEIL(inputSignal.freqKHz, updateRangeKHz.max);

    LW_ASSIGN_INTERSECTION_RANGE(mdivRange, range);

    // Iterate through all of the legal values of mdiv
    for (test.mdiv = mdivRange.min; test.mdiv <= mdivRange.max; test.mdiv++)
    {
        LwRangeU8 ndivRangeGivenMdiv;

        //
        // We can do a few optimization here to reduce the range of n
        //
        // Determine the legal values of ndiv to not violate vcoRange
        //      fVco = fInput / m * n
        //      fVco_min <= (fInput * n / m) <= fVco_max
        //      (fVco_min * m / fInput) <= n <= (fVco_min * m / fInput)
        // So n must be within the range
        // [(fVco_min * m / fInput), (fVco_min * m / fInput)]
        //
        // Determine the legal values of ndiv to not violate the outputRange
        //      fOutput = fInput * n / pl / m
        //      n = (fOutput * m * pl) / fInput
        // fInput and m are known at this point in the program.  Using the
        // equation for n we just derived, it is apparent that n increases as
        // pl increases, and n increases as fOutput increases. Therefore, the
        // range of n can be written as
        //      n <= (fOutput_max * m * pl_max) / fInput
        //      n >= (fOutput_min * m * pl_min) / fInput
        //
        // So the value of m must be within these two inclusive ranges:
        //      (1) [n_min, n_max]
        //      (2) [(fVco_min * m / fInput), (fVco_min * m / fInput)]
        //      (3) [(fOutput_min * m * pl_min / fInput),
        //           (fOutput_max * m * pl_max / fInput)]
        //
        // In the calls to LW_ASSIGN_INTERSECTION_RANGE below, we're comparing
        // LwU8 'ndivRangeGivenMdiv' with LwU32 'range', which may (or may not)
        // raise the ire of a static analyzer such as Coverity.  If so, it would be
        // a false positive, however, because the assignment to 'ndivRangeGivenMdiv'
        // happens only to narrow the range.
        //

        // Callwlate the first range
        ndivRangeGivenMdiv = ndivRange;

        // Callwlate the second range
        range = vcoRangeKHz;
        LW_MULTIPLY_RANGE(range, test.mdiv);
        LW_DIVIDE_NARROW_RANGE(range, test.inputKHz);
        LW_ASSIGN_INTERSECTION_RANGE(ndivRangeGivenMdiv, range);

        // Callwlate the third range
        range.min = LW_UNSIGNED_DIV_CEIL(
            pTarget->rangeKHz.min * test.mdiv * pldivRange.min, test.inputKHz);
        range.max =
            pTarget->rangeKHz.max * test.mdiv * pldivRange.max / test.inputKHz;
        LW_ASSIGN_INTERSECTION_RANGE(ndivRangeGivenMdiv, range);

        // Iterate through the allowed values of ndiv
        for (test.ndiv = ndivRangeGivenMdiv.min; test.ndiv <= ndivRangeGivenMdiv.max; test.ndiv++)
        {
            vcoKHz = test.inputKHz * test.ndiv / test.mdiv;

            // select an appropriate value of pldiv
            test.pldiv = LW_VALUE_WITHIN_INCLUSIVE_RANGE(pldivRange,
                vcoKHz / pTarget->super.freqKHz);

            // callwlate the delta
            test.deltaKHz = LW_DELTA(
                test.inputKHz * test.ndiv / test.mdiv / test.pldiv,
                pTarget->super.freqKHz);

            // ndivRangeGivenMdiv should prevent overclocking if it's correct.
            PMU_HALT_COND(vcoKHz <= this->pPllInfoTableEntry->MaxVCO * 1000);

            //
            // There is no need to check that this configuration fits all of
            // the ranges, as that was already ensured through the range
            // narrowing process
            //

            // determine if this is better than the previous configuration
            if (CONFIG_COMPARE(test, *pBest))
            {
                *pBest = test;
            }
        } // end ndiv for
    } // end mdiv for
}   // end function


/*!
 * @memberof    ClkAPll
 * @brief       Loop through various configurations to find the best.
 * @version     Clocks 3.1 and after
 * @protected
 *
 * @details     This implementation iterates through possible 'mdiv', 'ndiv' and
 *              'pldiv' spelwlatively configuring the input for each and return
 *              the best. The complexity of this function is O(n^3) * O(input).
 *
 *              The actual frequency output of this object is:
 *              this->output.freqKHz = (inputKHz * ndiv) / (mdiv * pldiv);
 *
 *              'status' reflects the status of the input node.
 *
 *              This function is inlined to save stack space. Since it is called
 *              from only one place, inlining does not increase the binary size.
 *
 * @note        nolwirtual: Function must be called directly (not using a pointer).
 *
 * @pre         The input node is connected in the schematic dag.
 *
 * @pre         Spread spectrum is not supported, thus target.spread must be
 *              off.
 *
 * @post        On successful return (FLCN_OK or warning), 'pBest' contains valid
 *              data and the input object has been configured.  If unsuccessful,
 *              it is unspecified whether or not members in 'pBest' other than
 *              'status' change.
 *
 * @ilwariant   This function neither reads nor writes any registers.
 *
 * @param[in]   this            Frequency source object pointer
 * @param[in]   pTarget         Target frequency, etc.
 * @param[in]   path            Signal path to be used by the input node
 * @param[out]  pBest           Best parameters to use for configuration
 */
static LW_INLINE void
clkConfig_FindBest_VariableInput_APll
(
    ClkAPll                 *this,
    const ClkTargetSignal   *pTarget,
    ClkConfigResult_APll    *pBest,
    ClkPhaseIndex            phase
)
{
    // Structs to help determine the best configuration
    ClkConfigResult_APll test;
    ClkConfigResult_APll best;

    ClkTargetSignal inputTarget;
    ClkSignal inputSignal;

    // Aliases which do not change their values.
    const LwRangeU32 updateRangeKHz =
    {
        .min = this->pPllInfoTableEntry->MinUpdate * 1000,
        .max = this->pPllInfoTableEntry->MaxUpdate * 1000,
    };

    const LwRangeU32 inputRangeKHz =
    {
        .min = this->pPllInfoTableEntry->MinRef * 1000,
        .max = this->pPllInfoTableEntry->MaxRef * 1000,
    };

    // Callwlated ranges
    LwRangeU32 vcoRangeKHz;
    LwRangeU8  pldivRange;
    LwRangeU8  ndivRange;
    LwRangeU8  mdivRange;

    // A large range struct to do math on without worrying about overflow
    LwRangeU32 range;

    // Callwlated frequencies
    LwU32 updateKHz;
    LwU32 vcoKHz;

    // The results from the most recent attempt to configure the input
    FLCN_STATUS lastError;
    LwU32 lastInputKHz;

    // Colwert the VCO range to KHz.
    vcoRangeKHz.min = this->pPllInfoTableEntry->MilwCO * 1000;
    vcoRangeKHz.max = this->pPllInfoTableEntry->MaxVCO * 1000;

    //
    // TODO: Implement range narrowing on initialization instead.
    // see /sw/docs/resman/presentations/clks/clk3/Reducing-range-of-m-and-n.docx
    //
    mdivRange.min = this->pPllInfoTableEntry->MinM;
    mdivRange.max = this->pPllInfoTableEntry->MaxM;
    range.max = inputRangeKHz.max / updateRangeKHz.min;
    range.min = LW_UNSIGNED_DIV_CEIL(inputRangeKHz.min, updateRangeKHz.max);
    LW_ASSIGN_INTERSECTION_RANGE(mdivRange, range);

    ndivRange.min = this->pPllInfoTableEntry->MinN;
    ndivRange.max = this->pPllInfoTableEntry->MaxN;
    range.max = vcoRangeKHz.max / updateRangeKHz.min;
    range.min = LW_UNSIGNED_DIV_CEIL(vcoRangeKHz.min, updateRangeKHz.max);
    LW_ASSIGN_INTERSECTION_RANGE(ndivRange, range);

    // Ditto for the PLDIV unless it is disabled.
    if (this->bPldivEnable)
    {
        pldivRange.min = this->pPllInfoTableEntry->MinPl;
        pldivRange.max = this->pPllInfoTableEntry->MaxPl;
    }
    else
    {
        pldivRange.max = 1;
        pldivRange.min = 1;
    }

    //
    // Disallow changing of m and pl if we are programming hot We are hot if
    // the previous phase was hot.  This is because if this pll was in use
    // last phase and is being configured this phase, then it is being used
    // two phases in a row.  When a pll is only used one phase in a row, it is
    // programmed cold, before a mux switches it to be the active output.
    // When a pll is being used two phases in a row, there is no way a mux
    // above could be switched, so it must be being programmed hot.
    //
    if (this->phase[phase - 1].bActive)
    {
        pldivRange.max = pldivRange.min = this->phase[phase - 1].pldiv;
        mdivRange.max  = mdivRange.min  = this->phase[phase - 1].mdiv;
    }


    // Initialize with pessimistic values
    best = clkReset_ConfigResult_APll;
    lastInputKHz = 0;
    inputSignal = clkEmpty_Signal;
    lastError = FLCN_OK;
    test.status = FLCN_ERR_FREQ_NOT_SUPPORTED;

    // Initialize input target
    inputTarget = *pTarget;
    inputTarget.super.fracdiv = LW_TRISTATE_FALSE;
    inputTarget.rangeKHz = inputRangeKHz;

    //
    // Iterate through all the allowed values of mdiv. Iterate through m first
    // because it tends to have the smallest range and oftentimes has only one
    // value
    //
    for (test.mdiv = mdivRange.min; test.mdiv <= mdivRange.max; test.mdiv++)
    {
        LwRangeU32 updateRangeKHzGivenMdiv;
        LwRangeU8  ndivRangeGivenMdiv;

        //
        // Now that we know m, we can narrow down the allowed update range
        //      fInput / m = fUpdate
        //      fInput = fUpdate * m
        //      fInput_min <= (fUpdate * m) <= fInput_max
        //      (fInput_min / m) <= fUpdate <= (fInput_max / m)
        //
        // So the update frequency must be within the two ranges [(min), (max)]:
        //      [(fInput_min / m), (fInput_max / m)]
        //      [(fUpdate_min), (fUpdate_max)]
        //
        updateRangeKHzGivenMdiv = inputRangeKHz;
        LW_DIVIDE_NARROW_RANGE(updateRangeKHzGivenMdiv, test.mdiv);
        LW_ASSIGN_INTERSECTION_RANGE(updateRangeKHzGivenMdiv, updateRangeKHz);

        //
        // Next, iterations of n will be eliminated based on the vco range
        //      fVco = fUpdate * n
        //      n = fVco / fUpdate
        //      n is at a maximum when (fVco, fUpdate) = (fVco_max, fUpdate_min)
        //      n is at a minimum when (fVco, fUpdate) = (fVco_min, fUpdate_max)
        //      (fVco_min / fUpdate_max) <= n <= (fVco_max / fUpdate_min)
        //
        // So the value of n must be within the two ranges [(min), (max)]:
        //      [(fVco_min / fUpdate_max), (fVco_max / fUpdate_min)]
        //      [(n_min), (n_max)]
        //
        ndivRangeGivenMdiv.min =
            LW_UNSIGNED_DIV_CEIL(vcoRangeKHz.min, updateRangeKHzGivenMdiv.max);
        ndivRangeGivenMdiv.max = vcoRangeKHz.max / updateRangeKHzGivenMdiv.min;
        LW_ASSIGN_INTERSECTION_RANGE(ndivRangeGivenMdiv, ndivRange);

        // Loop through the more restrictive range of n
        for (test.ndiv = ndivRangeGivenMdiv.min; test.ndiv <= ndivRangeGivenMdiv.max; test.ndiv++)
        {
            // Loop through the allowed values of pldiv
            for (test.pldiv = pldivRange.min; test.pldiv <= pldivRange.max; test.pldiv++)
            {
                // Adjust the input frequency to make sure it is within range.
                inputTarget.super.freqKHz = LW_VALUE_WITHIN_INCLUSIVE_RANGE(
                    inputRangeKHz,
                    (pTarget->super.freqKHz * test.mdiv * test.pldiv) /
                    test.ndiv);

                // Dont need to reconfigure if this is the same input value as before
                if (lastInputKHz != inputTarget.super.freqKHz)
                {
                    lastInputKHz = inputTarget.super.freqKHz;
                    test.status = clkConfig_Wire(
                        &this->super,
                        &inputSignal,
                        phase,
                        &inputTarget,
                        LW_FALSE);
                }

                // ndivRangeGivenMdiv should prevent overclocking if it's correct.
                PMU_HALT_COND(inputSignal.freqKHz * test.ndiv / test.mdiv <=
                    this->pPllInfoTableEntry->MaxVCO * 1000);

                test.inputKHz = inputSignal.freqKHz;
                test.path = inputSignal.path;

                // Give up if there is an error
                if (test.status != FLCN_OK)
                {
                    lastError = test.status;
                    continue;
                }

                // Quit if the configuration is invalid
                updateKHz = test.inputKHz / test.mdiv;
                vcoKHz = test.inputKHz * test.ndiv / test.mdiv;

                if (!LW_WITHIN_INCLUSIVE_RANGE(inputRangeKHz, test.inputKHz) ||
                   !LW_WITHIN_INCLUSIVE_RANGE(updateRangeKHz, updateKHz) ||
                   !LW_WITHIN_INCLUSIVE_RANGE(vcoRangeKHz, vcoKHz) ||
                   !LW_WITHIN_INCLUSIVE_RANGE(pTarget->rangeKHz, vcoKHz / test.pldiv))
                {
                    continue;
                }

                // Callwlate the delta KHz
                test.deltaKHz = LW_DELTA(
                    vcoKHz / test.pldiv,
                    pTarget->super.freqKHz);

                // Choose new best configurations
                if (CONFIG_COMPARE(test, best))
                {
                    best = test;
                }
            } // End pldiv loop
        }   // End ndiv loop
    }   // End mdiv loop

    // Return the best
    *pBest = best;

    // If pBest is FLCN_ERR_FREQ_NOT_SUPPORTED, try to make it more specific
    if (pBest->status == FLCN_ERR_FREQ_NOT_SUPPORTED &&
        lastError != FLCN_OK)
    {
        pBest->status = lastError;
    }

    // reconfigure the input
    inputTarget.super.freqKHz = pBest->inputKHz;
    clkConfig_Wire(&this->super, &inputSignal, phase, &inputTarget, LW_FALSE);
}


/*******************************************************************************
    Programming
*******************************************************************************/

/*!
 * @see         ClkFreqSrc_Virtual::clkProgram
 * @brief       Program the pll
 */
FLCN_STATUS
clkProgram_APll
(
    ClkAPll        *this,
    ClkPhaseIndex   phase
)
{
    FLCN_STATUS status;

    PMU_HALT_COND(this->settleNs > 0);         // Required by clkProgram_Direct_APll

    // Program the input (reference frequency)
    status = clkProgram_Wire(&this->super, phase);
    if (status != FLCN_OK)
    {
        return status;
    }

    // If the PLL is active, slide
    if (this->phase[phase - 1].bActive)
    {
        return clkProgram_Slide_APll(this, phase);
    }

    // Otherwise, program directly.
    return clkProgram_Direct_APll(this, phase);
}


/*!
 * @memberof    ClkAPll
 * @brief       Directly program the PLL
 * @protected
 *
 * @details     This implementation programs the PLL directly (without NDIV
 *              sliding).  As such, it powers down the PLL and therefore should
 *              not be used while the PLL is active.  All three dividers
 *              (MDIV, NDIV, PLDIV) as programmed as needed by this implementation.
 *
 *              If the PLL fails to lock to the new frequency, it is powered
 *              down and the programming sequence is retried up to five times.
 *              In older chips, the PLL would fail to lock very rarely, and
 *              one retry is generally adequate.  If it fails five times, this
 *              function returns FLCN_ERR_TIMEOUT.
 *
 * @note        This implementation is inadequate for MPLL/REFMPLL since we must
 *              query LW_PTRIM_SYS_MEM_PLL_STATUS (instead of LW_PTRIM_SYS_PLL_CFG).
 *              The field definitions (not just the address) differ.
 *
 * @post        If too much time has elapsed before the PLL settles, then
 *              this->retryCount is incremented and the PLL is powered off.
 *
 *              If retryCount > CLK_RETRY_COUNT_MAX_PLL, then FLCN_ERR_TIMEOUT
 *              is returned.
 *
 *
 * @param[in]   this            Frequency source object pointer
 * @param[in]   phase           The phase to program the hardware to
 *
 * @retval      FLCN_ERR_TIMEOUT    PLL failed to lock five times.
 */
static LW_INLINE FLCN_STATUS clkProgram_Direct_APll
(
    ClkAPll         *this,
    ClkPhaseIndex    phase
)
{
    LwU32 cfgRegData;

    // Retry loop -- If there is a timeout, try again several times.
    for (this->retryCount = 0;
         this->retryCount < CLK_RETRY_COUNT_MAX_PLL;
       ++this->retryCount)
    {
        LwU32 coeffRegData;

        //
        // Read the current configuration.  The config register is read/written
        // inside the retry loop to ensure that there are no pending register
        // transations after PLL power down.  The additional delay can't hurt.
        //
        cfgRegData = CLK_REG_RD32(this->cfgRegAddr);

        // Disable sync mode, power on the PLL making sure it is disabled.
        cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _IDDQ,      _POWER_ON, cfgRegData);
        cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _SYNC_MODE, _DISABLE,  cfgRegData);
        cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _ENABLE,    _NO,       cfgRegData);
        CLK_REG_WR32(this->cfgRegAddr, cfgRegData);

        //
        // Starting with the PLLs used on Pascal, there must be a delay of
        // 5us between power up and programming the coefficients and enablement.
        // lwrtosLwrrentTaskDelayTicks has a typical minimum delay of 30us, so it's not used here.
        //
        OS_PTIMER_SPIN_WAIT_NS(DELAY_READY_NS);

        //
        // Read the current coefficients.  The coefficient register is read/written
        // inside the retry loop to ensure that there are no pending register
        // transations after PLL power up.  The additional delay can't hurt.
        //
        coeffRegData = CLK_REG_RD32(this->coeffRegAddr);

        // Turn off sync mode and disable the PLL.
        cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _SYNC_MODE, _DISABLE, cfgRegData);
        cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _ENABLE,    _NO,      cfgRegData);
        CLK_REG_WR32(this->cfgRegAddr, cfgRegData);

        // Update the coefficients.
        coeffRegData = FLD_SET_DRF_NUM(_PTRIM, _SYS_PLL_COEFF, _MDIV, this->phase[phase].mdiv, coeffRegData);
        coeffRegData = FLD_SET_DRF_NUM(_PTRIM, _SYS_APLL_COEFF, _NDIV, this->phase[phase].ndiv, coeffRegData);

        if (this->bPldivEnable)
        {
            coeffRegData = FLD_SET_DRF_NUM(_PTRIM, _SYS_PLL_COEFF, _PLDIV, this->phase[phase].pldiv, coeffRegData);
        }

        // Write the coefficient reguster.
        CLK_REG_WR32(this->coeffRegAddr, coeffRegData);

        // Enable the PLL.
        cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _ENABLE, _YES, cfgRegData);
        CLK_REG_WR32(this->cfgRegAddr, cfgRegData);

        //
        // Wait for the PLL to lock.  Exit on success.
        //
        // 'settleNs' is typically 64us, but the PLL settles in about 1/3 of that.
        //
        // TODO: Consider using lwrtosLwrrentTaskDelayTicks which has a typical delay of 30us,
        // but has the advantage of allowing other tasks to run while we wait.
        //
        if (OS_PTIMER_SPIN_WAIT_NS_COND(clkProgram_IsLocked_APll, this, this->settleNs))
        {
            break;
        }

        // Check one more time to make sure we're not caught in a race condition.
        if (clkProgram_IsLocked_APll(this))
        {
            break;
        }

        //
        // We timed out, so prepare to retry by disabling the PLL and powering
        // it off.  Also make sure sync mode is disabled.
        //
        cfgRegData = CLK_REG_RD32(this->cfgRegAddr);
        cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _IDDQ,      _POWER_OFF, cfgRegData);
        cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _SYNC_MODE, _DISABLE,   cfgRegData);
        cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _ENABLE,    _NO,        cfgRegData);
        CLK_REG_WR32(this->cfgRegAddr, cfgRegData);
    }

    // We retried several times and failed.
    if (this->retryCount >= CLK_RETRY_COUNT_MAX_PLL)
    {
        dbgPrintf(LEVEL_ALWAYS, "PLL lock timeout\n");
        return FLCN_ERR_TIMEOUT;
    }

    // The PLL is locked, enable SYNC mode
    cfgRegData = CLK_REG_RD32(this->cfgRegAddr);
    cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _SYNC_MODE, _ENABLE, cfgRegData);
    CLK_REG_WR32(this->cfgRegAddr, cfgRegData);

    // Done
    return FLCN_OK;
}


/*!
 * @memberof    ClkAPll
 * @brief       Slowly slide the NDIV up or down to the target value
 * @see         https://wiki.lwpu.com/engwiki/index.php/VLSI/Mixed_Signal_PLL_Design/Dynamic_frequency_ramp_up/down
 * @protected
 *
 * @details     If the PLL is being programmed while active (hot), then this
 *              function is used to slowly adjust the NDIV value.  The NDIV
 *              value can is incremented or decremented by one, and then there
 *              is a waiting period before it is changed again.
 *
 *              However, the MDIV and PLDIV values must be unchanged from the
 *              previous phase.
 *
 * @post        On success, NDIV has reached the target value in hardware.
 *
 * @note        In previous versions of the clock code, interrupts were disabled
 *              during the entire NDIV slide so that it happened at a regular,
 *              predictable rate.  This has not been implemented in Clocks 3.x
 *              because it was deemed unnecessary.  The goal of the NDIV slide
 *              is to ensure that the NDIV doesn't change too quickly.  If it
 *              changes too slowly, it shouldn't cause any problems.
 *
 * @param[in]   this    the pll object to program
 * @param[in]   phase   the phase to program the pll object to.
 */
static LW_INLINE FLCN_STATUS clkProgram_Slide_APll
(
    ClkAPll         *this,
    ClkPhaseIndex    phase
)
{
    LwU32 ndivRegData;
    LwU8 ndiv;

    // The MDIV and PLDIV can not be programmed while active.
    PMU_HALT_COND(this->phase[phase].mdiv  == this->phase[phase - 1].mdiv);
    PMU_HALT_COND(this->phase[phase].pldiv == this->phase[phase - 1].pldiv);

    // Determine the current value of NDIV.
    ndivRegData = CLK_REG_RD32(this->coeffRegAddr);
    ndiv = DRF_VAL(_PTRIM, _SYS_APLL_COEFF, _NDIV, ndivRegData);

    // Loop until we reach the target.
    if (ndiv != this->phase[phase].ndiv)
    {
        while (LW_TRUE)
        {
            // Increment if ndiv is below target
            if (ndiv < this->phase[phase].ndiv)
            {
                ++ndiv;
            }

            // Decrement if ndiv is above target
            else if (ndiv > this->phase[phase].ndiv)
            {
                --ndiv;
            }

            // Program the new ndiv.
            ndivRegData = FLD_SET_DRF_NUM(_PTRIM, _SYS_APLL_COEFF, _NDIV, ndiv, ndivRegData);
            CLK_REG_WR32(this->coeffRegAddr, ndivRegData);

            // Done if we have reached the target
            if (ndiv == this->phase[phase].ndiv)
            {
                break;
            }

            //
            //
            // Wait the specific amount of time (typically 100ns) between NDIV updates.
            // lwrtosLwrrentTaskDelayTicks has a typical minimum delay of 30000ns, so it's not used here.
            //
            OS_PTIMER_SPIN_WAIT_NS(this->slideStepDelayNs);
        }
    }

    // Done
    return FLCN_OK;
}


/*!
 * @brief       Is PLL Locked?
 * @see         OS_PTIMER_COND_FUNC
 * @memberof    ClkAPll
 *
 * @details     This function returns true if the PLL has locked onto the
 *              programmed frequency.
 *
 * @note        This function's signature conforms to OS_PTIMER_COND_FUNC
 *              so that it can be called by OS_PTIMER_SPIN_WAIT_NS_COND.
 *              As such, it must have an entry in pmu_sw/build/Analyze.pm
 *              for $fpMappings.
 *
 * @warning     pArgs is typecast to a ClkAPll pointer.
 *
 * @param[in]   pArgs   Pointer to this ClkAPll object
 *
 * @retval      true    The PLL has locked.
 */
LwBool clkProgram_IsLocked_APll(void *pArgs)
{
    // The argument points to the PLL object.
    const ClkAPll *this = pArgs;

    // Read the current value of the cfg register
    LwU32 cfgRegData = CLK_REG_RD32(this->cfgRegAddr);

    // True iff the lock has been acquired.
    return FLD_TEST_DRF(_PTRIM, _SYS_PLL_CFG, _PLL_LOCK, _TRUE, cfgRegData);
}


/*******************************************************************************
    Clean-Up
*******************************************************************************/

/*!
 * @see         ClkFreqSrc_Virtual::clkCleanup
 * @memberof    ClkAPll
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
clkCleanup_APll
(
    ClkAPll *this,
    LwBool   bActive
)
{
    FLCN_STATUS status;
    ClkPhaseIndex p;

    // If this PLL is not part of the active path, turn it off.
    if (!bActive)
    {
        // Read the configuration register to preserver other flags
        LwU32 cfgRegData = CLK_REG_RD32(this->cfgRegAddr);

        // Disable sync
        cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _SYNC_MODE, _DISABLE, cfgRegData);
        CLK_REG_WR32(this->cfgRegAddr, cfgRegData);

        // Disable pll
        cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _ENABLE, _NO, cfgRegData);
        CLK_REG_WR32(this->cfgRegAddr, cfgRegData);

        // Power down pll
        cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _IDDQ, _POWER_OFF, cfgRegData);
        CLK_REG_WR32(this->cfgRegAddr, cfgRegData);
    }

    // Cleanup the input as needed.
    status = clkCleanup_Wire(&this->super, bActive);

    //
    // Update the pll's phase array to reflect the last phase
    // Only phase 0 can have the bActive flag set, however
    //
    this->phase[CLK_MAX_PHASE_COUNT - 1].bActive = LW_FALSE;
    for (p = 0; p < CLK_MAX_PHASE_COUNT - 1; p++)
    {
        this->phase[p] = this->phase[CLK_MAX_PHASE_COUNT - 1];
    }

    this->phase[0].bActive = bActive;

    return status;
}


/*******************************************************************************
    Print
*******************************************************************************/

/*!
 * @see         ClkFreqSrc_Virtual::clkPrint
 * @brief       Print debugging info
 *
 * @memberof    ClkAPll
 *
 * @param[in]   this        Object to print
 * @param[in]   phaseCount  Number of phases for config/program
 */
#if CLK_PRINT_ENABLED
void
clkPrint_APll
(
    ClkAPll         *this,
    ClkPhaseIndex    phaseCount
)
{
    LwU8 id = (this->pPllInfoTableEntry ? this->pPllInfoTableEntry->id : 0x00);
    CLK_PRINTF("PLL: id=0x%02x source=0x%02x retryCount=%u\n",
            (unsigned) id, (unsigned) this->source, (unsigned) this->retryCount);

    // Print each programming phase.
    for(ClkPhaseIndex p = 0; p < phaseCount; ++p)
    {
        // Zero for a nonexistent PLDIV.
        // 2 for special DIV2 divider (instead of PLDIV).
        // See the schematic DAG to distinguish between DIV2 v. PLDIV when it prints 2.
        LwU8 pldiv;
        if (this->bDiv2Enable)
            pldiv = 2;
        else if (this->bPldivEnable)
            pldiv = 0;
        else
            pldiv = this->phase[p].pldiv;

        CLK_PRINTF("PLL phase %u: mdiv=%u ndiv=%u pdiv=%u active=%u input=%uKHz\n",
            (unsigned) p,
            (unsigned) this->phase[p].mdiv,
            (unsigned) this->phase[p].ndiv,
            (unsigned) pldiv,
            (unsigned) this->phase[p].bActive,
            (unsigned) this->phase[p].inputFreqKHz);
    }

    // Read and print hardware state.
    CLK_PRINT_REG("PLL with SDM support:   CFG", this->cfgRegAddr);
    CLK_PRINT_REG("PLL with SDM support: COEFF", this->coeffRegAddr);

    // Print the input object information.
    clkPrint_Wire(&this->super, phaseCount);
}
#endif
