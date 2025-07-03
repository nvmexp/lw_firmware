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
 * @file
 * @see     https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @see     Bug 2557340: Turn off VPLL, HUBPLL and DISPPLL based on usecase
 * @see     Bug 2571031: Ampere DISPCLK/HUBCLK: Switching to bypass after initialization
 * @see     Perforce:  hw/doc/gpu/SOC/Clocks/Documentation/Display/Unified_Display_Clocking_Structure.vsdx
 * @author  Antone Vogt Varvak
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */

#include "clk3/clk3.h"
#include "clk3/fs/clkpdiv.h"
#include "clk3/generic_dev_trim.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

CLK_DEFINE_VTABLE__FREQSRC(PDiv);

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */

//
// Functions here that are inlined are called only once.  This reduces stack
// pressure without increasing the IMEM size.
//
static LW_INLINE FLCN_STATUS    clkConfig_ProgrammableInputNode_PDiv(
    ClkPDiv                     *this,
    ClkSignal                   *pOutput,
    ClkPhaseIndex                phase,
    const ClkTargetSignal       *pTarget,
    LwBool                       bHotSwitch);

static LW_INLINE FLCN_STATUS    clkConfig_ReadOnlyInputNode_PDiv(
    ClkPDiv                     *this,
    ClkSignal                   *pOutput,
    ClkPhaseIndex                phase,
    const ClkTargetSignal       *pTarget,
    LwBool                       bHotSwitch);


/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Virtual Implemenations ------------------------- */

/*!
 * @brief       Get the frequency.
 * @memberof    ClkPDiv
 * @see         ClkFreqSrc_Virtual::clkRead
 *
 * @details     This implementation reads the divider after calling clkRead
 *              on the input.  From that it computes the output frequency.
 *
 * @param[in]   this        Instance of ClkPDiv from which to read
 * @param[out]  pOutput     Frequency will be written to this struct.
 */
FLCN_STATUS
clkRead_PDiv
(
    ClkPDiv     *this,
    ClkSignal   *pOutput,
    LwBool       bActive
)
{
    LwU32           regData;
    ClkPhaseIndex   p;
    FLCN_STATUS     status;

    // Read the input frequency.
    status = clkRead_Wire(&(this->super), pOutput, bActive);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Read the divider value.
    regData = CLK_REG_RD32(this->regAddr);

    // Extract the divider value.
    this->phase[0].divValue = (regData >> this->base) & (LWBIT32(this->size) - 1);

    // Postcondition: Same state in all phases
    for (p = 1; p < CLK_MAX_PHASE_COUNT; p++)
    {
        this->phase[p] = this->phase[0];
    }

    // The divider is powered off.
    if (this->phase[0].divValue == LW_PVTRIM_SPPLL_PDIV_POWERDOWN)
    {
        *pOutput = clkReset_Signal;
        return FLCN_ERR_ILWALID_STATE;
    }

    // Compute the output value if this LDIV unit.
    pOutput->freqKHz = pOutput->freqKHz / this->phase[0].divValue;

    // Done
    return FLCN_OK;
}


/*!
 * @brief       Configure the PDIV.
 * @memberof    ClkPDiv
 * @see         ClkFreqSrc_Virtual::clkConfig
 * @see         lwbugs/2557340/48
 *
 * @param[in]   this        Instance of ClkPDiv to configure
 * @param[out]  pOutput     Actual output frequency
 * @param[in]   phase       Index of the phase array to configure
 * @param[in]   pTarget     Target clock signal
 * @param[in]   bHotSwitch  True iff this object will be programmed while in use
 */
FLCN_STATUS
clkConfig_PDiv
(
    ClkPDiv                 *this,
    ClkSignal               *pOutput,
    ClkPhaseIndex            phase,
    const ClkTargetSignal   *pTarget,
    LwBool                   bHotSwitch
)
{
    ClkPhaseIndex   p;
    FLCN_STATUS     status;

    // If the input is unchanging, use the optimized config function.
    if (CLK_IS_READ_ONLY__FREQSRC(this->super.pInput))
    {
        status = clkConfig_ReadOnlyInputNode_PDiv(this, pOutput, phase,
            pTarget, bHotSwitch);
    }

    // We must adjust the input as well as the PDIV.
    else
    {
        status = clkConfig_ProgrammableInputNode_PDiv(this, pOutput, phase,
            pTarget, bHotSwitch);
    }

    //
    // Note: If we decide to support PDIV sliding in the case of 'bHotSwitch',
    // we'll need to add a 'ClkPDivPhase::bHotSwitch' field and set it here so
    // that 'clkProgram' can do the sliding only when 'ClkPDivPhase::bHotSwitch'
    // is true.
    //

    //
    // Postcondition:  The configuration each subsequent phase must the the same
    // as this phase.  (In other words assume that the status of the hardware
    // will not change in subsequent changes unless 'clkConfig' is called on them.)
    //
    for (p = phase + 1; p < CLK_MAX_PHASE_COUNT; p++)
    {
        this->phase[p] = this->phase[phase];
    }

    // Done
    return status;
}


/*!
 * @brief       Program the PDIV
 * @memberof    ClkPDiv
 * @see         ClkFreqSrc_Virtual::clkProgram
 *
 * @pre         phase > 0
 *
 * @param[in]   this        Instance of ClkPDiv to program
 * @param[in]   phase       Index of the phase array to program
 */
FLCN_STATUS
clkProgram_PDiv
(
    ClkPDiv         *this,
    ClkPhaseIndex    phase
)
{
    // Program the PDIV only if it is changing v. prior phase
    if (this->phase[phase].divValue != this->phase[phase - 1].divValue)
    {
        // Read the configuration register to preserver other flags
        LwU32 regDataNew, regDataOld;
        regDataNew = regDataOld = CLK_REG_RD32(this->regAddr);

        // Apply the correct divider value.
        regDataNew &= ((~(LWBIT32(this->size) - 1) << this->base) | (LWBIT32(this->base) - 1));
        regDataNew |= this->phase[phase].divValue << this->base;

        //
        // Write only if the value has changed.
        // This also powers on the PDIV if necessary.
        //
        if (regDataNew != regDataOld)
        {
            CLK_REG_WR32(this->regAddr, regDataNew);
        }
    }

    // Done
    return LW_OK;
}


/*!
 * @brief       Power off the PDIV if not active.
 * @see         ClkFreqSrc_Virtual::clkCleanup
 *
 * @param[in]   this        Instance of ClkPDiv to conditionally power off
 * @param[in]   bActive     True unless this object isn't being used
 */
FLCN_STATUS
clkCleanup_PDiv
(
    ClkPDiv     *this,
    LwBool       bActive
)
{
    FLCN_STATUS   status;
    ClkPhaseIndex p;

    // If this PDIV is not part of the active path, turn it off.
    if (!bActive)
    {
        // Read the configuration register to preserver other flags
        LwU32 regDataNew, regDataOld;
        regDataNew = regDataOld = CLK_REG_RD32(this->regAddr);

        // Power-down uses a sentinel value.
        regDataNew &= ((~(LWBIT32(this->size) - 1) << this->base) | (LWBIT32(this->base) - 1));
        regDataNew |= LW_PVTRIM_SPPLL_PDIV_POWERDOWN << this->base;

        // Power down the PDIV.
        if (regDataNew != regDataOld)
        {
            CLK_REG_WR32(this->regAddr, regDataNew);
        }

        // Flag that we are powering down the PDIV
        this->phase[CLK_MAX_PHASE_COUNT - 1].divValue = LW_PVTRIM_SPPLL_PDIV_POWERDOWN;
    }

    // Cleanup the input as needed.
    status = clkCleanup_Wire(&this->super, bActive);

    // Update the PLL's phase array to reflect the last phase
    for (p = 0; p < CLK_MAX_PHASE_COUNT - 1; p++)
    {
        this->phase[p] = this->phase[CLK_MAX_PHASE_COUNT - 1];
    }

    // Done
    return status;
}

/*!
 * @see         ClkFreqSrc_Virtual::clkPrint
 * @brief       Print debugging info
 *
 * @memberof    ClkPDiv
 *
 * @param[in]   this        Object to print
 * @param[in]   phaseCount  Number of phases for config/program
 */
#if CLK_PRINT_ENABLED
void
clkPrint_PDiv
(
    ClkPDiv         *this,
    ClkPhaseIndex    phaseCount
)
{
    // Print the divider value for each programming phase.
    // Typically, zero means powered down.
    CLK_PRINTF("PDIV:");
    for (ClkPhaseIndex p = 0; p < phaseCount; ++p)
    {
        CLK_PRINTF(" phase[%u]=%u", (unsigned) p, (unsigned) this->phase[p].divValue);
    }
    CLK_PRINTF("\n");
    clkPrint_Wire(&this->super, phaseCount);
}
#endif


/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief       Configure the PDIV if the input is not programmable.
 *
 * @details     If the input is not programmable, then configuring is much easier.
 *              Instead of looping through every possible PDIV value, the best
 *              values is computed based on the input frequency.
 */
static LW_INLINE FLCN_STATUS
clkConfig_ReadOnlyInputNode_PDiv
(
    ClkPDiv                 *this,
    ClkSignal               *pOutput,
    ClkPhaseIndex            phase,
    const ClkTargetSignal   *pTarget,
    LwBool                   bHotSwitch
)
{
    // The two PDIV values closed to the desired value
    LwU32 divAbove;
    LwU32 divBelow;

    // Delta between desired and actual
    LwU32 aboveDeltaKHz;
    LwU32 belowDeltaKHz;

    // Range of allowed PDIV values
    LwRangeU8 range;

    // Block to isolate locals that are used only in this call
    {
        FLCN_STATUS status;

        //
        // Get the input frequency.
        //
        // Q: Why call clkConfig instead of clkRead since the input is read-only?
        // A: clkRead lives in a different overlay, so we would have to swap it in,
        //    which may increase latency.
        //
        // Q: Why use clkReset_TargetSignal?
        // A: It demands nothing.  The range is zero to infinity.  It's flags
        //    are don't-care.  The target frequency does not matter in the
        //    ClkReadOnly and ClkXtal implementations.  Since the purpose is
        //    merely to get the frequency, this is what we want.
        //
        status = clkConfig_Wire(&(this->super), pOutput, phase,
                                    &clkReset_TargetSignal, bHotSwitch);

        // Give up if the input generates an error.
        if (status != FLCN_OK)
        {
            return status;
        }
    }

    // Get the PDIV value above and below the target
    divAbove = pOutput->freqKHz / pTarget->super.freqKHz;
    divBelow = LW_UNSIGNED_DIV_CEIL(pOutput->freqKHz, pTarget->super.freqKHz);

    // Now make sure they don't exceed the overall range.
    range.min = LW_PVTRIM_SPPLL_PDIV_MIN;
    range.max = LW_PVTRIM_SPPLL_PDIV_MAX;
    divAbove = LW_VALUE_WITHIN_INCLUSIVE_RANGE(range, divAbove);
    divBelow = LW_VALUE_WITHIN_INCLUSIVE_RANGE(range, divBelow);

    //
    // Compute the difference between the target and actual output frequencies
    // using the lesser divider value.  Set to huge if not within range.
    //
    aboveDeltaKHz = 0xffffffff;
    if (LW_WITHIN_INCLUSIVE_RANGE(pTarget->rangeKHz, pOutput->freqKHz / divAbove))
    {
        aboveDeltaKHz = LW_DELTA(pOutput->freqKHz / divAbove,
            pTarget->super.freqKHz);
    }

    //
    // Compute the difference between the target and actual output frequencies
    // using the greater  divider value.  Set to huge if not within range.
    //
    belowDeltaKHz = 0xffffffff;
    if (LW_WITHIN_INCLUSIVE_RANGE(pTarget->rangeKHz, pOutput->freqKHz / divBelow))
    {
        belowDeltaKHz = LW_DELTA(pOutput->freqKHz / divBelow,
            pTarget->super.freqKHz);
    }

    // There is no working configuration
    if (aboveDeltaKHz == 0xffffffff && belowDeltaKHz == 0xffffffff)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_FREQ_NOT_SUPPORTED;
    }

    // Pick the best one
    if (aboveDeltaKHz < belowDeltaKHz)
    {
        this->phase[phase].divValue = divAbove;
    }
    else
    {
        this->phase[phase].divValue = divBelow;
    }

    // Update pOutput to reflect how this PDIV alters the signal
    pOutput->freqKHz /= this->phase[phase].divValue;

    // Done
    return LW_OK;
}


/*
 * @brief       Configure the PDIV if the input can change
 *
 * @details     If the input can change while programming, configuring the PDIV
 *              is a bit more complicated.  This function loops through the
 *              possible PDIV value and determines the PDIV value to choose to
 *              obtain the best frequency.
 */
static LW_INLINE FLCN_STATUS
clkConfig_ProgrammableInputNode_PDiv
(
    ClkPDiv                 *this,
    ClkSignal               *pOutput,
    ClkPhaseIndex            phase,
    const ClkTargetSignal   *pTarget,
    LwBool                   bHotSwitch
)
{
    //
    // This logic should be implemented if/when we have a use-case.  It would
    // need to compute values for both before and after programming the input.
    // See clkldivunit.c and clkConfig_ProgrammableInputNode_LdivUnit.
    //
    PMU_BREAKPOINT();
    return FLCN_ERR_ILLEGAL_OPERATION;
}

