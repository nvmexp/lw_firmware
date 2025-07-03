/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see         https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author      Eric Colter
 * @author      Lawrence Chang (ClkEmbeddedLdiv)
 * @author      Antone Vogt-Varvak
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */

#include "clk3/clk3.h"
#include "clk3/fs/clkldivunit.h"
#include "clk3/generic_dev_trim.h"


/* ------------------------- Type Definitions ------------------------------- */

/*
 * @brief       A type to determine how far outside of a range a value is
 *
 * @details     This type stores whether an unsigned integer is within a
 *              range, above the range and by how much, or below the range and
 *              by how much.
 *
 *              It is used to compare different intergers to see which most
 *              closely satisfies the range.
 *
 *              The last 31 bits make an unsigned int to represent the distance
 *              from the range. The most significant bit is 1 if the value is
 *              above the range, and 0 if it isn't.
 */
typedef LwU32 RangeViolationType;


/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

CLK_DEFINE_VTABLE__FREQSRC(LdivUnit);


/* ------------------------- Macros and Defines ----------------------------- */

#define MIN__DIVIDER_VALUE_2X 2

#define MAX__DIVIDER_VALUE_2X 63

#define OVERSHOOT_FLAG 0x80000000

#define TEST_RANGE_VIOLATION(range, value)                              \
    (value > range.max ? value - (range.max | OVERSHOOT_FLAG) :         \
    (value < range.min ? range.min - value : 0))


/* ------------------------- Prototypes ------------------------------------- */

//
// Functions here that are inlined are called only once.  This reduces stack
// pressure without increasing the IMEM size.
//
static LW_INLINE FLCN_STATUS    clkConfig_ProgrammableInputNode_LdivUnit(
    ClkLdivUnit                 *this,
    ClkSignal                   *pOutput,
    ClkPhaseIndex                phase,
    const ClkTargetSignal       *pTarget,
    LwBool                       bHotSwitch);

static LW_INLINE FLCN_STATUS    clkConfig_ReadOnlyInputNode_LdivUnit(
    ClkLdivUnit                 *this,
    ClkSignal                   *pOutput,
    ClkPhaseIndex                phase,
    const ClkTargetSignal       *pTarget,
    LwBool                       bHotSwitch);

static LW_INLINE LwBool         clkConfig_HotSwitch_LdivUnit(
    ClkLdivUnit                 *this,
    const ClkTargetSignal       *pTarget,
    ClkLdivUnitPhase            *pPrevPhase,
    ClkLdivUnitPhase            *pThisPhase);

static LwU32                    clkWriteRegField_LdivUnit(
    ClkLdivUnit                 *this,
    LwU32                        regData,
    LwU8                         ldiv)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkWriteRegField_LdivUnit");

static LwU8                     clkReadRegField_LdivUnit(
    ClkLdivUnit                 *this,
    LwU32                        regData);

static LW_INLINE void           clkProgram_Before_LdivUnit(
    ClkLdivUnit                 *this,
    ClkPhaseIndex                phase);

static LW_INLINE FLCN_STATUS    clkProgram_Input_LdivUnit(
    ClkLdivUnit                 *this,
    ClkPhaseIndex                phase);

static LW_INLINE void           clkProgram_After_LdivUnit(
    ClkLdivUnit                 *this,
    ClkPhaseIndex                phase);


/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Virtual Implemenations ------------------------- */

/*!
 * @see         ClkFreqSrc_Virtual::clkRead
 * @memberof    ClkLdivUnit
 * @brief       Read the coefficients from hardware
 *
 * @details     Determine the current selected input by reading the hardware,
 *              then daisy-chain through the input node to get the frequency.
 *              Set all values of the phase array to align with the current
 *              hardware state.
 */
FLCN_STATUS
clkRead_LdivUnit
(
    ClkLdivUnit         *this,
    ClkSignal           *pOutput,
    LwBool               bActive
)
{
    LwU32               ldivRegData;
    ClkLdivUnitPhase    phaseData;
    ClkPhaseIndex       p;
    LwU8                ldivValue;
    FLCN_STATUS         status;

    // Read the divider value and the input frequency.
    ldivRegData = CLK_REG_RD32(this->ldivRegAddr);
    status = clkRead_Wire(&(this->super), pOutput, bActive);
    ldivValue = clkReadRegField_LdivUnit(this, ldivRegData);

    // Save these data as per-phase status.
    phaseData.inputFreqKHz = pOutput->freqKHz;
    phaseData.ldivBefore = phaseData.ldivAfter = ldivValue;

    // Postcondition: Same state in all phases
    for (p = 0; p < CLK_MAX_PHASE_COUNT; p++)
    {
        this->phase[p] = phaseData;
    }

    // Compute the output value if this LDIV unit.
    pOutput->freqKHz = pOutput->freqKHz * 2 / ldivValue;

    // Indiate fractional divide if appropriate.
    if (ldivValue % 2)
    {
        pOutput->fracdiv = LW_TRISTATE_TRUE;
    }

    return status;
}


/*!
 * @see         ClkFreqSrc_Virtual::clkConfig
 * @memberof    ClkLdivUnit
 * @brief       Configure the hardware to be programmed to a certain frequency
 *
 * @details     Loop through different combinations of pl, n and m to determine
 *              which will result in the output closest to the desired
 *              frequency.  Set this phase and all subsequent phases of the
 *              phase array to these values.
 */
FLCN_STATUS clkConfig_LdivUnit
(
    ClkLdivUnit             *this,
    ClkSignal               *pOutput,
    ClkPhaseIndex            phase,
    const ClkTargetSignal   *pTarget,
    LwBool                   bHotSwitch
)
{
    FLCN_STATUS status;
    ClkPhaseIndex p;

    // This Ldiv is essentially a wire, so just call the input.
    if (this->maxLdivValue == 2)
    {
        this->phase[phase - 1].ldivAfter =    2;
        this->phase[phase - 1].ldivBefore =   2;
        status = clkConfig_Wire(&(this->super), pOutput, phase,
            pTarget, bHotSwitch);
    }

    // If the input is unchanging, use the optomized config function.
    else if (CLK_IS_READ_ONLY__FREQSRC(this->super.pInput))
    {
        status = clkConfig_ReadOnlyInputNode_LdivUnit(this, pOutput, phase,
            pTarget, bHotSwitch);
    }

    // We must adjust the input as well as the ldiv.
    else
    {
        status = clkConfig_ProgrammableInputNode_LdivUnit(this, pOutput, phase,
            pTarget, bHotSwitch);
    }

    //
    // Postcondition:  The configuration each subsequent phase must the the same
    // as this phase.  (In other words assume that the status of the hardware
    // will not chagne in subsequent changes unless 'clkConfig' is called on them.)
    //
    for (p = phase + 1; p < CLK_MAX_PHASE_COUNT; p++)
    {
        this->phase[p] = this->phase[phase];
    }

    // Done
    return status;
}


/*!
 * @see         ClkFreqSrc_Virtual::clkProgram
 * @brief       Program the hardware of the Ldiv to match the phase array
 *
 * @details     Program the LdivUnit in multiple steps.  Each step corresponds
 *              to its own helper function.
 *
 *              1.  Set the register to the ldivBefore value.  This is sometimes
 *                  needed to prevent intermediate frequencies from going out of
 *                  range.
 *
 *              2.  Program the input to the ldiv.
 *
 *              3.  Program the ldivAfter value.
 */
FLCN_STATUS clkProgram_LdivUnit
(
    ClkLdivUnit     *this,
    ClkPhaseIndex    phase
)
{
    FLCN_STATUS status;

    // Program the LDIV value needed before we program the input (if needed).
    clkProgram_Before_LdivUnit(this, phase);

    // Program the input (if needed).
    status = clkProgram_Input_LdivUnit(this, phase);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Program the LDIV value needed after we program the input (if needed).
    clkProgram_After_LdivUnit(this, phase);

    // Done.
    return FLCN_OK;
}


/*!
 * @see         ClkFreqSrc_Virtual::clkCleanup
 * @memberof    ClkLdivUnit
 * @brief       Powerdown the Ldiv if it is unused to save power.
 */
FLCN_STATUS clkCleanup_LdivUnit
(
    ClkLdivUnit *this,
    LwBool       bActive
)
{
    LwU32 ldivRegData;
    ClkPhaseIndex p;

    // Cleanup hardware if applicable
    if (!bActive && this->ldivCleanup)
    {
        ldivRegData = CLK_REG_RD32(this->ldivRegAddr);
        ldivRegData = clkWriteRegField_LdivUnit(this, ldivRegData, this->ldivCleanup);
        CLK_REG_WR32(this->ldivRegAddr, ldivRegData);
    }

    // Postcondition: Same state in all phases
    for (p = 0; p < CLK_MAX_PHASE_COUNT - 1; p++)
    {
        this->phase[p] = this->phase[CLK_MAX_PHASE_COUNT - 1];
    }

    return clkCleanup_Wire(&this->super, bActive);
}


/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief       Configure the ldiv if the input is locked
 *
 * @details     If the input is locked (Either a read only object or a xtal),
 *              then configuring is much easier.  Instead of looping through
 *              every possibl ldiv value, the best ldiv value can simply be
 *              callwlated.
 */
static LW_INLINE FLCN_STATUS
clkConfig_ReadOnlyInputNode_LdivUnit
(
    ClkLdivUnit             *this,
    ClkSignal               *pOutput,
    ClkPhaseIndex            phase,
    const ClkTargetSignal   *pTarget,
    LwBool                   bHotSwitch
)
{
    // The two ldiv values closed to the desired value
    LwU32 ldivAbove;
    LwU32 ldivBelow;

    // Delta between desired and actual
    LwU32 aboveDeltaKHz;
    LwU32 belowDeltaKHz;

    // Range of allowed ldiv values
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

        // Give up if the input generates an error
        if (status != FLCN_OK)
        {
            return status;
        }
    }

    // Get the ldiv value above and below the target
    ldivBelow = LW_UNSIGNED_DIV_CEIL(pOutput->freqKHz * 2, pTarget->super.freqKHz);
    ldivAbove = pOutput->freqKHz * 2 / pTarget->super.freqKHz;

    // Now make sure they don't exceed the overall range
    range.min = MIN__DIVIDER_VALUE_2X;
    range.max = this->maxLdivValue;
    ldivAbove = LW_VALUE_WITHIN_INCLUSIVE_RANGE(range, ldivAbove);
    ldivBelow = LW_VALUE_WITHIN_INCLUSIVE_RANGE(range, ldivBelow);

    //
    // Round up or down if they are fractional and not within the allowed
    // fractional range OR if they are fractional and there are no fracdivs
    // left
    //
    if (ldivBelow % 2 &&
        (pOutput->fracdiv >= pTarget->super.fracdiv ||
        (ldivBelow > this->maxFracDivValue)))
    {
        //
        // The prefered action is to round up, but the value is already at the
        // boundary, it needs to be rounded down instead.  This is slightly
        // different than the logic in CLocks 2.x (Pascal and before).
        //
        ldivBelow = ldivBelow == this->maxLdivValue ?
            ldivBelow - 1 : ldivBelow + 1;
    }
    if (ldivAbove % 2 &&
        (pOutput->fracdiv >= pTarget->super.fracdiv ||
        (ldivAbove > this->maxFracDivValue)))
    {
        //
        // The prefered action is to round up, but the value is already at the
        // boundary, it needs to be rounded down instead.  This is slightly
        // different than the logic in CLocks 2.x (Pascal and before).
        //
        ldivAbove = ldivAbove == MIN__DIVIDER_VALUE_2X ?
            ldivAbove + 1 : ldivBelow - 1;
    }


    aboveDeltaKHz = 0xffffffff;
    belowDeltaKHz = 0xffffffff;

    if (LW_WITHIN_INCLUSIVE_RANGE(pTarget->rangeKHz,
        pOutput->freqKHz*2 / ldivAbove))
    {
        aboveDeltaKHz = LW_DELTA(pOutput->freqKHz*2 / ldivAbove,
            pTarget->super.freqKHz);
    }

    if (LW_WITHIN_INCLUSIVE_RANGE(pTarget->rangeKHz,
        pOutput->freqKHz*2 / ldivBelow))
    {
        belowDeltaKHz = LW_DELTA(pOutput->freqKHz*2 / ldivBelow,
            pTarget->super.freqKHz);
    }

    // There is no working configuration
    if (aboveDeltaKHz == 0xffffffff && belowDeltaKHz == 0xffffffff)
    {
        return FLCN_ERR_FREQ_NOT_SUPPORTED;
    }

    // Pick the best one
    if (aboveDeltaKHz < belowDeltaKHz)
    {
        this->phase[phase].ldivAfter = ldivAbove;
    }
    else
    {
        this->phase[phase].ldivAfter = ldivBelow;
    }

    //
    // If the input is already locked, there are no "In between" values during
    // programming as the input is not programmable. Thus, ldivBefore is not
    // needed.
    //
    this->phase[phase].ldivBefore = this->phase[phase - 1].ldivAfter;
    this->phase[phase].inputFreqKHz = pOutput->freqKHz;

    // Now to update pOutput to reflect how this ldiv alters the signal
    pOutput->freqKHz *= 2;
    pOutput->freqKHz /= this->phase[phase].ldivAfter;

    // Flag if the clock signal is no longer 50% duty cycle.
    if (this->phase[phase].ldivAfter % 2)
    {
        pOutput->fracdiv = LW_TRISTATE_TRUE;
    }

    // Done
    return LW_OK;
}


/*
 * @brief       Configure the ldiv if the input can change
 *
 * @details     If the input can change while programming, configuring the ldiv
 *              is a bit more complicated.  This function loops through the
 *              possible ldiv values and determines the ldiv value to choose to
 *              obtain the best frequency.
 */
static LW_INLINE FLCN_STATUS
clkConfig_ProgrammableInputNode_LdivUnit
(
    ClkLdivUnit             *this,
    ClkSignal               *pOutput,
    ClkPhaseIndex            phase,
    const ClkTargetSignal   *pTarget,
    LwBool                   bHotSwitch
)
{
    struct ConfigResult
    {
    /*!
     * @brief   The target to set the input to
     */
        ClkTargetSignal     inputTarget;

    /*!
     * @brief   The state of the phase array for this configuration
     */
        ClkLdivUnitPhase    phaseData;

    /*!
     * @brief   Deviatoin of frequency from target
     */
        LwU32               deltaKHz;

    /*!
     * @brief   Bitmapped score of priority
     */
        LwU8                score;

    /*!
     * @brief   Status of the input
     */
        FLCN_STATUS         status;

    } best, test;

    LwU32   outputFreqKHz;
    LwU8    loopLdiv;
    LwBool  bOutOfPreferedRange;

    // Initialize best to be the most pessimistic
    best.deltaKHz = 0xffffffff;
    best.score =    0xff;
    best.status =   FLCN_ERROR;

    //
    // Iterate through all of the allowed values of ldiv to find the best
    // configuration
    //
    for (loopLdiv = MIN__DIVIDER_VALUE_2X; loopLdiv <= this->maxLdivValue; loopLdiv++)
    {
        // Skip fractional divide if it is not permitted.
        if  (loopLdiv % 2                                   &&
            (loopLdiv > this->maxFracDivValue               ||
            pTarget->super.fracdiv == LW_TRISTATE_FALSE))
        {
            continue;
        }

        // Set up the target for the input.
        test.inputTarget = *pTarget;
        CLK_MULTIPLY__TARGET_SIGNAL(test.inputTarget, loopLdiv);
        CLK_DIVIDE__TARGET_SIGNAL(test.inputTarget, 2);

        //
        // If we're applying fractional divide in this object, then we must
        // limit the number of times fractional divide is applied in the
        // input. 'loopLdiv' is odd if we're applying fracdiv in this object.
        // From the conditional 'continue' statement above, we know that the
        // target 'fracdiv' is positive (i.e. enabled).  If it's being used
        // here, then decrement the number of times it can be used by the
        // input.
        //
        // For example, let's say (as would typically be the case) that
        // pTarget->super.fracdiv == 1 in gpc.ldiv.  Then
        // test.inputTarget.super.fracdiv == 0 when it gets to gpc.osm2.byp to
        // disable fracdiv.  If we didn't use fracdiv in gpc.ldiv, we could
        // hypothetically do so in gpc.osm2.byp, (a feature that we don't
        // actually use as of December 2013).
        //
        if (loopLdiv % 2)
        {
            --test.inputTarget.super.fracdiv;
        }

        //
        // Configure the input according to the target, Just reuse the pOutput
        // to save stack space
        //
        test.status = clkConfig_Wire(&(this->super), pOutput, phase,
            &test.inputTarget, bHotSwitch);

        // Don't consider this if an error is raised
        if (test.status != FLCN_OK)
        {
            //
            // If no error has yet been recorded, then remember this error
            // code. In effect, if all iterations result in error, this
            // function returns the status code of the first iteration (with
            // minimum LDIV value).
            //
            if (best.status == FLCN_ERROR)
            {
                best.status = test.status;
            }

            continue;
        }

        // Set up the phase state of the test
        test.phaseData.ldivAfter = loopLdiv;
        test.phaseData.inputFreqKHz = pOutput->freqKHz;

        //
        // If this is to be programmed while active (hot switch), we need to
        // pick our ldivBefore carefully.  Otherwise just set it to ldivAfter.
        //
        if (bHotSwitch)
        {
            // Determine what to set ldivBefore to.
            bOutOfPreferedRange = clkConfig_HotSwitch_LdivUnit(this, pTarget,
                &(this->phase[phase - 1]), &test.phaseData);

            // Make sure this configuration isn't invalid
            if (test.phaseData.ldivBefore == 0)
            {
                continue;
            }
        }

        else
        {
            test.phaseData.ldivBefore = test.phaseData.ldivAfter;
            bOutOfPreferedRange = LW_FALSE;
        }

        outputFreqKHz = test.phaseData.inputFreqKHz * 2 /
            test.phaseData.ldivAfter;
        test.deltaKHz = LW_DELTA(outputFreqKHz, pTarget->super.freqKHz);

        //
        // Make sure this is within the allowed range and that a suitable
        // before value was found
        //
        if (!LW_WITHIN_INCLUSIVE_RANGE(pTarget->rangeKHz, outputFreqKHz) ||
            test.phaseData.ldivBefore == 0)
        {
            test.status = FLCN_ERR_FREQ_NOT_SUPPORTED;
        }

        if (test.status == FLCN_OK)
        {
            //
            // In case deltaKHz is a tie, score the test result.
            // Higher values are results that are discouraged.
            // BIT(5) = Frequency not between the target and current frequency.
            // BIT(4) = Fractional divide is used in the output signal.
            // BIT(3) = Fractional divide is used in the intermediate signal.
            // BIT(2) = Fractional divide is used in the input signal.
            // BIT(1) = Reserved for future use.
            // BIT(0) = The input signal differs from the previous phase.
            //
            test.score =
                ((bOutOfPreferedRange)                                  << 5) |
                ((test.phaseData.ldivAfter  % 2)                        << 4) |
                ((test.phaseData.ldivBefore % 2)                        << 3) |
                ((!!pOutput->fracdiv)                                   << 2) |
                ((test.phaseData.inputFreqKHz != this->phase[phase - 1].inputFreqKHz));

            //
            // If the test result beats the best result thus far (i.e. lower
            // delta), use it.  If it is a tie, use it if it has a better
            // score (per logic above).  If that's also a tie, then keep the
            // earlier result because we prefer a lower divider value and
            // therefore (probably) a lower input frequency.
            //
            if (test.deltaKHz <  best.deltaKHz  ||
               (test.deltaKHz == best.deltaKHz  &&
                test.score    <  best.score))
            {
                best          = test;
            }

            // Stop the search if delta is within 0.5%
            if (best.deltaKHz < pTarget->super.freqKHz / 200)
            {
                break;
            }
        }
    }

    //
    // If the error is nondescript, improve it a bit.  This probably means the
    // daisy-chain was never called, perhaps because the loop was not entered.
    //
    if (best.status == FLCN_ERROR)
    {
        best.status =  FLCN_ERR_FREQ_NOT_SUPPORTED;
    }

    // No error.
    if (best.status == FLCN_OK)
    {
        FLCN_STATUS status;

        //
        // Save the LDIV values unless there is an error.
        //
        // NOTE: Coverity UNINIT checker complains with this message:
        // Using uninitialized value "best.phaseData". Field "best.phaseData.inputFreqKHz" is uninitialized.
        // However, 'best.status' is set to FLCN_OK only if 'best'is assigned
        // to 'test' after 'test.phaseData.inputFreqKHz has been initialized.
        //
        // These cases apply:
        // (a)  The 'loopLdiv' loop is never entered or each interation hits
        //      the 'continue' conditional near the start.  However, the value
        //      'best.status' would be FLCN_ERROR, the initial value, and
        //      logic would not reach this point.
        // (b)  The call to 'clkConfig_Wire' returns an error in all iterations.
        //      Again, 'best.status' would be FLCN_ERROR.
        // (c)  'test.phaseData.inputFreqKHz' is initialized shortly after the
        //      call to 'clkConfig_Wire'.  'best' is not changed after that
        //      point except to be assigned to 'test'.  This is the only path
        //      where 'best.status == FLCN_OK' as a result.
        //
        this->phase[phase] = best.phaseData;

        //
        // Reconfigure the input node per contract.  The postcondition is
        // that, when this function returns, the input is configured as it
        // needs to be.
        //
        status = clkConfig_Wire(&(this->super), pOutput, phase, &best.inputTarget, bHotSwitch);
        if (status != FLCN_OK)
        {
            return status;
        }

        //
        // Update the output signal.  The ouput signal is the signal coming
        // out of the input node.  It needs to be the signal coming out from
        // this node.
        //
        pOutput->freqKHz *= 2;
        pOutput->freqKHz /= best.phaseData.ldivAfter;
        pOutput->fracdiv += (best.phaseData.ldivAfter % 2);

        // Return the worse of the two.
    }

    // Done
    return best.status;
}


/*
 * @brief       Add a specific ldiv value to the the ldiv register
 *
 * @param[in]   this        The ldiv object
 * @param[in]   regData     The data read from the ldiv register
 * @param[in]   ldiv        The new value of the ldiv.  Divide this by 2 to get
 *                          the divider value, for example, and ldiv of 5 would
 *                          configure the ldiv to divide the input frequency by
 *                          2.5
 *
 * @return      The value to write to the ldiv register.
 */
static LwU32
clkWriteRegField_LdivUnit
(
    ClkLdivUnit     *this,
    LwU32            regData,
    LwU8             ldiv
)
{
    return FLD_SET_DRF_NUM(_PTRIM, _SYS_CLK_LDIV, _DIV(this->divider), ldiv - 2, regData);
}


/*
 * @brief       Get the ldiv field from the raw ldiv register data
 *
 * @param[in]   this        The ldiv object
 * @param[in]   regData     The data read from the ldiv register
 *
 * @return      The value of the ldiv. Divide this by 2 to get the divider
 *              value. for example an ldiv of 7 would configure the ldiv to
 *              divide the input frequency by 3.5.
 */
static LwU8
clkReadRegField_LdivUnit
(
    ClkLdivUnit *this,
    LwU32        regData
)
{
    return DRF_VAL(_PTRIM, _SYS_CLK_LDIV, _DIV(this->divider), regData) + 2;
}


/*
 *@brief        Program the ldiv to the ldivBefore value
 */
static LW_INLINE void
clkProgram_Before_LdivUnit
(
    ClkLdivUnit     *this,
    ClkPhaseIndex    phase
)
{
    LwU32 ldivRegData;

    // We only need to program the ldiv if the ldiv has changed
    if (this->phase[phase].ldivBefore != this->phase[phase - 1].ldivAfter)
    {
        ldivRegData = CLK_REG_RD32(this->ldivRegAddr);
        ldivRegData = clkWriteRegField_LdivUnit(this, ldivRegData,
            this->phase[phase].ldivBefore);
        CLK_REG_WR32(this->ldivRegAddr, ldivRegData);
    }
}


/*
 *@brief        Program the input to the ldiv
 */
static LW_INLINE FLCN_STATUS
clkProgram_Input_LdivUnit
(
    ClkLdivUnit     *this,
    ClkPhaseIndex    phase
)
{
    // Program the input.
    return clkProgram_Wire(&(this->super), phase);
}


/*
 *@brief        Program the ldiv to the ldivAfter value
 */
static LW_INLINE void
clkProgram_After_LdivUnit
(
    ClkLdivUnit     *this,
    ClkPhaseIndex   phase
)
{
    LwU32 ldivRegData;

    // We only need to program the ldiv if the ldiv has changed
    if (this->phase[phase].ldivAfter != this->phase[phase].ldivBefore)
    {
        ldivRegData = CLK_REG_RD32(this->ldivRegAddr);
        ldivRegData = clkWriteRegField_LdivUnit(this, ldivRegData,
            this->phase[phase].ldivAfter);
        CLK_REG_WR32(this->ldivRegAddr, ldivRegData);
    }
}


/*!
 * @memberof    ClkLdivUnit
 *
 * @brief       Configure ldivBefore and ldivAfter for a hot switch.
 *
 * @version     Clocks 3.1 and after
 *
 * @details     If the LDIV isnt being changed (ldivAfter of this phase is
 *              the same as ldivAfter of the previous phase), then ldivBefore
 *              is simply set to ldivAfter.
 *
 *              Otherwise, it chooses an intermediate linear divider value
 *              (ldivBefore) from the values between the previous phase and
 *              this phase (ldivBefore). The criteria for the best value is
 *              based on many factors; see inline comments for more details.
 *
 *              clkConfig is contractually obligated to make sure that all
 *              intermediate frequencies are within the actual frequencies for
 *              the previous phase and this.  It it is unable to find a suitable
 *              intermediate value to satisfy this, it will try to pick one
 *              that undershoots the frequency range.  As a last resort, in
 *              some rare corner cases (usually ilwolving fractional divide),
 *              it will have to choose a frequency that overshoots the range.
 *
 *              If an intermediate frequency cannot be found that remains within
 *              the target range, the ldivBefore is set to zero to indicate
 *              that no valid match could be found.
 *
 *              For example, suppose we are switching from 1400MHz to 540MHz
 *              using these values:
 *                  phase   input   ldiv    output
 *                  prev    1400    2/2     1400
 *                  this    1620    6/2      540
 *
 *              If we were to write the ldiv value before programming the input,
 *              we would have an intermittent frequency that is too low:
 *                  phase   input   ldiv    output
 *                  prev    1400    2/2     1400
 *                          1400    6/2      466    bad!  <540
 *                  this    1620    6/2      540
 *
 *              If we were to write the ldiv value after programming the input,
 *              we would have an intermittent frequency that is too high:
 *                  phase   input   ldiv    output
 *                  prev    1400    2/2     1400
 *                          1620    2/2     1620    bad!  >1400
 *                  this    1620    6/2      540
 *
 *              The correct way to make this switch is to first write 4/2 to the
 *              linear divider, program the input, then use 6/2, as follows:
 *                  phase   input   ldiv    output
 *                  prev    1400    2/2     1400
 *                          1400    4/2      700    okay
 *                          1620    4/2      810    okay
 *                  this    1620    6/2      540
 *              clkDbgGetInternalState: transition=1400=>700=>810=>540MHz
 *
 *              Some cases have no proper solution.  For example, if we switch
 *              from 1400MHz using 2/2 to 1080MHz using 3/2, we have this if
 *              we switch the ldiv first:
 *                  phase   input   ldiv    output
 *                  prev    1400    2/2     1400
 *                          1400    3/2      933    bad!  <1080
 *                  this    1620    3/2     1080
 *              clkDbgGetInternalState: transition=1400=>933=>1080=>1080MHz
 *
 *              And this if we switch the input first:
 *                  phase   input   ldiv    output
 *                  prev    1400    2/2     1400
 *                          1620    2/2     1620    bad!  >1400
 *                  this    1620    3/2     1080
 *              clkDbgGetInternalState: transition=1400=>1400=>1620=>1080MHz
 *
 *              For cases like these, the ldiv value is set to undershoot the
 *              range as that is preferred to overshooting it.
 *                  phase   input   ldiv    output
 *                  prev    1400    2/2     1400
 *                          1400    3/2      933    bad!  <1080
 *                  this    1620    3/2     1080
 *              clkDbgGetInternalState: transition=1400=>933=>1080=>1080MHz
 *
 *              Sometimes when switching to a representation that requires
 *              fracdiv to one that doesnt, or vice versa, the ldiv is forced to
 *              overshoot the range.  Intermediate value can't use fracdiv
 *              unless both the prev and this freq use them.  Switching the
 *              input first in the following scenario would be preferred as it
 *              would undershoot the frequency, but this is not allowed.
 *                  phase   input   ldiv    output
 *                  prev    1120.5  3/2     747
 *                          1107    3/2     738   middle ldiv cant be fractional unless both endpoints are too!
 *                  this    1107    2/2     1107
 *
 *              The ldiv must be changed first to avoid a middle frequency that
 *              uses fractional divide.  This results in a middle frequency that
 *              overshoots the range.
 *                  phase   input   ldiv    output
 *                  prev    1120.5  3/2     747
 *                          1120.5  2/2     1120.5  overshoot not desired, but necessary.
 *                  this    1107    2/2     1107
 *
 *              The lack of such logic was the root cause of bugs 685377 and
 *              837484 for GF108/HUB2CLK.  _clkProgramClk_WAR685377 was created
 *              in the older clocks code to deal with this arithmetic.
 *
 *              The programming sequence then is assumed to be:
 *              (1) Write ClkLdivUnit::ldivBeforeReg.
 *              (2) Call clkProgram on the input node.
 *              (3) Write ClkLdivUnit::ldivAfterReg.
 *              This is what clkProgram_LdivUnit does.
 *
 *
 * @see             ClkLdivUnit::ldivBeforeReg
 * @see             ClkLdivUnit::ldivAfterReg
 *
 * @note            nolwirtual:     Function must be called directly.
 * @note            private:        Function may be called from this class only.
 *
 * @param[in]       this                    ClkLdivUnit object pointer (this).
 * @param[in]       pTarget                 A pointer to the target clock
 *                                          signals.
 * @param[out]      pPrevPhase              Pointer to previous phase's phase
 *                                          array entry
 * @param[in/out]   pThisPhase              Pointer to this phase's phase array
 *                                          entry
 *
 * @retval          LW_FALSE        The intermediate frequencies are within
 *                                  range
 * @retval          LW_TRUE         The intermediate frequencies are out of the
 *                                  desired range
 */
static LW_INLINE LwBool
clkConfig_HotSwitch_LdivUnit
(
    ClkLdivUnit             *this,
    const ClkTargetSignal   *pTarget,
    ClkLdivUnitPhase        *pPrevPhase,
    ClkLdivUnitPhase        *pThisPhase
)
{
    struct ConfigResult
    {
        /*!
         * @brief   How far the worst intermediate frequency is from the
         *          this<->prev range
         */
        RangeViolationType rangeViolation;

        /*!
         * @brief   The selected ldivBefore value
         */
        LwU8 ldiv;

        /*!
         * @brief   A bitmapped number to indicate how desirable this
         *          configuration is.
         */
        LwU8 score;
    } test, best;

    // Frequencies are preferred to stay within this range
    LwRangeU32 desiredFreqRangeKHz;

    // Frequencies outside this range are unacceptable
    LwRangeU32 requiredFreqRangeKHz;

    // How much the beforeLdivValue violates the desired range
    RangeViolationType  beforeRangeViolation;

    //
    // The following four frequencies are output in the order they appear
    // (1) Frequency at the end of hte previous phase
    // (2) Frequency after ldiv is set to ldivBefore
    // (3) Frequency after input is programmed
    // (4) Frequency after ldiv is set to ldivAfter
    //
    LwU32   prevOutputFreqKHz;
    LwU32   beforeOutputFreqKHz;
    LwU32   afterOutputFreqKHz;
    LwU32   thisOutputFreqKHz;

    // Ldiv values at the END of this phase and the previous phase
    LwU8    prevLdiv,   thisLdiv;

    LwU8    maxLdiv,    minLdiv;

    LwBool bEndpointFracDivOkay;

    // Range of possible LDIV values
    thisLdiv = pThisPhase->ldivAfter;
    prevLdiv = pPrevPhase->ldivAfter;
    maxLdiv = LW_MAX(thisLdiv, prevLdiv);
    minLdiv = LW_MIN(thisLdiv, prevLdiv);

    //
    // If ldiv hasn't changed since last phase, (ie this ldiv isnt being
    // programmed), then just set the ldivBefore to the ldivAfter
    //
    if (pPrevPhase->ldivAfter == pThisPhase->ldivAfter)
    {
        pThisPhase->ldivBefore = pThisPhase->ldivAfter;
        return LW_FALSE;
    }

    //
    // Fractional divide can only be allowed in ldivBefore if the ldiv value
    // before and the ldivValue after it are both fractional divide as well.
    // This is the only way to know that it is absolutely safe and allowed to
    // use fractional divide.
    //
    bEndpointFracDivOkay = (maxLdiv % 2) && (minLdiv %2);

    // Correct for an invalid condition from the previous phase.
    if (maxLdiv % 2 && !bEndpointFracDivOkay)
    {
        maxLdiv--;
    }
    if (minLdiv % 2 && !bEndpointFracDivOkay)
    {
        minLdiv++;
    }

    //
    // Contractually, clkConfig must keep intermediate frequencies between this
    // target and the actual previous output.  (See prohibition 3 below.)
    //
    prevOutputFreqKHz = pPrevPhase->inputFreqKHz * 2 / pPrevPhase->ldivAfter;
    thisOutputFreqKHz = pThisPhase->inputFreqKHz * 2 / pThisPhase->ldivAfter;
    desiredFreqRangeKHz.min = LW_MIN(prevOutputFreqKHz, thisOutputFreqKHz);
    desiredFreqRangeKHz.max = LW_MAX(prevOutputFreqKHz, thisOutputFreqKHz);
    requiredFreqRangeKHz = pTarget->rangeKHz;


    //
    // Choose an intermediate value for the linear divider from the value
    // between the previous phase (prevLdiv) and the target (thisLdiv).
    // Rate each ldiv value based on a priority that considers several factors.
    // Some factors are prohibitive while others are preferenced.  Here are the
    // prohibitive factors:
    //
    // (1) Do not use fracdiv except at the endpoints (minLdiv and maxLdiv)
    //     because fracdiv is okay only if we are going to use it anyway.
    //
    // (2) Do not use fracdiv above maxFracDivValue since it is unsupported in
    //     hardware.
    //
    // (3) Do not let the first (beforeOutputFreqKHz) or second
    //     (afterOutputFreqKHz) intermediate frequency fall outside of the
    //     target range specified as a parameter.
    //
    // Here are the preference factors in priority order:
    // (1) Avoid any intermediate frequencies that overshoot the prev <--> this
    //     range. For example 999MHz -> 1188MHz -> 584 MHz.  In practice, this
    //     case is often superseded by prohibition (4) above.
    //
    // (2) Avoid any intermediate frequencies that undershoot the prev <--> this
    //     range. For exmaple 999MHz -> 499.5MHz -> 584MHz.
    //
    // (3) Prefer the actual ldiv (thisLdiv) to avoid the register write that
    //     would happen after the input is switched.
    //
    // (4) Prefer the previous ldiv (prevLdiv) to avoid the register write that
    //     would happen before the input is switched.
    //
    // (5) Prefer the mean average rounded down.
    //
    // (6) Prefer the mean average rounded up.
    //
    // The local variables used to track preference factors are:
    // (1&2) test.rangeViolation and best.rangeViolation
    // (3) BIT(3) of testScore and bestScore
    // (4) BIT(2) of testScore and bestScore
    // (5) BIT(1) of testScore and bestScore
    // (6) BIT(0) of testScore and bestScore
    // In each case, smaller numbers are better.
    //

    best.rangeViolation = 0xffffffff;
    best.score = 0xff;
    best.ldiv = 0;
    for (test.ldiv = minLdiv; test.ldiv <= maxLdiv; test.ldiv++)
    {
        // Output immediately after setting ldiv to ldivBefore
        beforeOutputFreqKHz = pPrevPhase->inputFreqKHz * 2 / test.ldiv;

        // Output immediately after programming input
        afterOutputFreqKHz = pThisPhase->inputFreqKHz * 2 / test.ldiv;

        // Check for unacceptable criteria
        if ((test.ldiv % 2 && test.ldiv > minLdiv && test.ldiv < maxLdiv)         ||   // (1)
            (test.ldiv % 2 && test.ldiv > this->maxFracDivValue)                  ||   // (2)
            !LW_WITHIN_INCLUSIVE_RANGE(requiredFreqRangeKHz, beforeOutputFreqKHz) ||   // (3)
            !LW_WITHIN_INCLUSIVE_RANGE(requiredFreqRangeKHz, afterOutputFreqKHz))
        {
            continue;
        }

        // Determine how far outside the target range the worst intermediate frequency is
        test.rangeViolation = TEST_RANGE_VIOLATION(desiredFreqRangeKHz,
            afterOutputFreqKHz);
        beforeRangeViolation = TEST_RANGE_VIOLATION(desiredFreqRangeKHz,
            beforeOutputFreqKHz);
        if (beforeRangeViolation > test.rangeViolation)
        {
            test.rangeViolation = beforeRangeViolation;
        }

        // Check for discouraged criteria.
        test.score =
            ((test.ldiv != thisLdiv)                                << 3) |  // (3)
            ((test.ldiv != prevLdiv)                                << 2) |  // (4)
            ((test.ldiv / 2 != (thisLdiv + prevLdiv) / 4)           << 1) |  // (5)
            ((test.ldiv / 2 != ((thisLdiv + prevLdiv) / 2 + 1) / 2) << 0);   // (6)

        // If this is better, use it.
        if ((test.rangeViolation <  best.rangeViolation) ||                  // (1)
            (test.rangeViolation == best.rangeViolation && test.score < best.score))
        {
            best = test;
        }
    }

    //
    // It's possible for the loop to 'continue' at every iteration if the
    // target range is too restrictive.  In this case set ldivBefore to zero
    // to indicate that this is invalid
    //
    pThisPhase->ldivBefore = best.ldiv;
    return (best.rangeViolation > 0);
}
