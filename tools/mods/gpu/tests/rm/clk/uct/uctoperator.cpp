/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2014,2016-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <cmath>
#include <math.h>
#include <climits>
#include <sstream>
#include <iomanip>

#include "gpu/tests/rm/utility/rmtstring.h"

#include "uctoperator.h"

//
// If the cmath/math.h libraries don't support 'isfinite', then they probably
// don't support 'fpclassify'.  As such, we leverage the IEEE 754 standard which
// requires 'NAN == NAN' to be false.  If the compiler's optimizer takes this
// as a tautology, then we assume that all values are finite, so at least we
// don't falsely report syntax errors.  INF is detected by comparing to HUGE_VAL,
// which is generally the same as INF.
//
#ifndef isfinite
#define isfinite(x) ((x) == (x) && (x) < HUGE_VAL && (x) > -HUGE_VAL)
#endif

// Interpret the text as a flag operator.
uct::ExceptionList uct::FlagOperator::parse(rmt::String flagList, const rmt::FileLocation &location, const NameMap &flagNameMap)
{
    uct::ExceptionList status;

    // Make sure there are no prior flag settings.
    mask = value = 0;

    // Skip parsing if 'none' is passed
    if(flagList == "none")
        return status;

    // Iterate through all flags
    while (!flagList.empty())
    {
        // Extract the text of the next flag
        rmt::String flagText = flagList.extractShard(':');

        // '-' means to reset the flag.
        // '+' or neither means to set the flag.
        bool bSet = flagText.firstChar() != '-';

        // Discard the prefixed '-' or '+' (if any)
        if (!bSet || flagText.firstChar() == '+')
            flagText = flagText.substr(1);

        // Interpret the name of the flag.
        UINT32 flagBitmap = flagNameMap.bit(flagText);

        // Zero indicates a syntax error (not found in map)
        if (!flagBitmap)
            status.push(ReaderException(flagText + ": Invalid flag", location, __PRETTY_FUNCTION__));

        // This flag has already been specified in this subfield
        else if(mask & flagBitmap)
            status.push(ReaderException(flagText + ": Redundant flag", location, __PRETTY_FUNCTION__));

        // Set or reset the flag
        else
        {
            mask |= flagBitmap;
            if (bSet)
                value |= flagBitmap;
        }
    }

    // Done
    return status;
}

// Interpret text as a numeric operator
uct::NumericOperator::PointerOrException uct::NumericOperator::parse(
    rmt::String &text, const UnitMap &unitMap)
{
    // Spaces are not significant.
    text.removeSpace();
    text.lcase();

    // Isolate the numeric portion from the (possibly empty) list of colon-delimited flags.
    rmt::String numericValue = text.extractShard(':');

    // An empty numeric is not acceptable.
    if (numericValue.empty())
        return Exception("Missing required numeric value",
            __PRETTY_FUNCTION__, Exception::Syntax);

    //
    // Do we have a '+' or '-' prefix?  If so, this is a relative value.
    // Note that there are other syntaxes to indicate a relative value.
    // Here, we mainly want to distinguish '+' prefix from no prefix.
    //
    bool bSignPrefix = numericValue.firstChar() == '+' || numericValue.firstChar() == '-';

    // Do we have a '%' suffix?  It would indicate a multiplicative factor operator.
    if (numericValue.lastChar() == '%')
    {
        // Create the operator object.
        unique_ptr<MultiplicativeNumericOperator> pOperator =
            make_unique<MultiplicativeNumericOperator>();
        MASSERT(pOperator);

        // Colwert to a numeric in a multiplicative subfield
        rmt::String v = numericValue;
        pOperator->factor = v.extractDouble() / 100.0;

        // Check for error
        if (errno || !isfinite(pOperator->factor))
            return Exception(numericValue + ": Unreasonable floating-point value for percentage",
                __PRETTY_FUNCTION__, Exception::Syntax);

        // We shouold have only a percent sign remaining
        if (v.compare("%"))
            return Exception(numericValue + ": Superfluous text for percentage",
                __PRETTY_FUNCTION__, Exception::Syntax);

        // '+' or '-' is relative to 100%.  That is, 120% == +20% and 80% == -20%
        if (bSignPrefix)
            pOperator->factor += 1.0;

        // The result must always be positive
        if (pOperator->factor <= 0.0)
            return Exception(numericValue + ": Nonpositive value for percentage",
                __PRETTY_FUNCTION__, Exception::Syntax);

        // All is okay
        return pOperator.release();
    }

    //
    // We have an absolute numeric or relative additive numeric.
    // Seperate the numeric value from the unit of measure.
    //
    rmt::String unit = numericValue;
    double value = unit.extractDouble();

    // Check for error.
    if (errno)
        return Exception(numericValue + ": Unreasonable integer value",
            __PRETTY_FUNCTION__, Exception::Syntax);

    // Interpret and apply the unit (if any).
    if (!unit.empty())
    {
        short exponent = unitMap.find_else(unit, SHRT_MIN);

        // Special value indicates a syntax error
        if (exponent <= SHRT_MIN)
            return Exception(unit + ": Invalid unit of measure",
                __PRETTY_FUNCTION__, Exception::Syntax);

        // Apply the unit of measure as an exponent
        value *= ::pow((double) 10.0, (int) exponent);
    }

    //
    // IEEE-754 can give us precision issues (albeit very small), so we
    // are prudent in using the properly rounded whole number.
    //
    {
        double whole;
        modf(value + 0.0000005, &whole);

        //
        // If the fractional part has significant digits within six places of
        // the decimal point, the it can't be blamed on IEEE-754 precision.
        // For example, since RMAPI uses whole KHz for frequencies, we can't do "1.0625MHz".
        //
        if (value < whole - 0.0000005 || value > whole + 0.0000005)
            return Exception(numericValue + ": Loss of precision",
                __PRETTY_FUNCTION__, Exception::Syntax);

        // Use the properly rounds whole number.
        value = whole;
    }

    // If the text started with '+' or '-', we have a relative additive operator.
    if (bSignPrefix)
    {
        // Additive numeric is out of range.
        if (value < (double) AdditiveNumericOperator::addend_min ||
            value > (double) AdditiveNumericOperator::addend_max)
        {
            return Exception(numericValue + ": Range error [" +
                rmt::String::dec(AdditiveNumericOperator::addend_min) + ", " +
                rmt::String::dec(AdditiveNumericOperator::addend_max) +
                "] on additive relative numeric",
                __PRETTY_FUNCTION__, Exception::Syntax);
        }
        // Apply the numeric field as a newly created operator object and return.
        AdditiveNumericOperator *pOperator = new AdditiveNumericOperator;
        MASSERT(pOperator);
        pOperator->addend = (AdditiveNumericOperator::addend_t) value;
        return pOperator;
    }

    // Absolute numeric is out of range
    if (value > (double) AbsoluteNumericOperator::value_max)
        return Exception(numericValue + ": Range error [0, " +
            rmt::String::dec(AbsoluteNumericOperator::value_max) +
            "] on absolute numeric",
            __PRETTY_FUNCTION__, Exception::Syntax);

    // Valid absolute numeric if there is no prefiexed '+' or '-'
    AbsoluteNumericOperator *pOperator = new AbsoluteNumericOperator;
    MASSERT(pOperator);
    pOperator->value = (AbsoluteNumericOperator::value_t) value;
    return pOperator;
}

// Destruction -- nothing to do
uct::NumericOperator::~NumericOperator()
{   }

// Create an exact copy of 'this'.
uct::NumericOperator *uct::AbsoluteNumericOperator::clone() const
{
    return new AbsoluteNumericOperator(*this);
}

bool uct::AbsoluteNumericOperator::isAbsolute() const
{
    return true;
}

//
// Is the result within range?
// This function is called from PartialPState::VoltSetting::fullPState and
// PartialPState::FreqSetting::fullPState.
//
bool uct::AbsoluteNumericOperator::validRange(value_t prior, Range range) const
{
    return value > range.min && value <= range.max;
}

//
// Return the field value ignoring the prior value
// This function is called from PartialPState::VoltSetting::fullPState and
// PartialPState::FreqSetting::fullPState.
//
uct::NumericOperator::value_t uct::AbsoluteNumericOperator::apply(value_t prior) const
{
    return value;
}

//
// Subfield without flags as text.
// This function is called from PartialPState::VoltSetting::fullPState and
// PartialPState::FreqSetting::fullPState.
//
rmt::String uct::AbsoluteNumericOperator::string() const
{
    ostringstream oss;
    oss << value;
    return oss;
}

// Create an exact copy of 'this'.
uct::NumericOperator *uct::AdditiveNumericOperator::clone() const
{
    return new AdditiveNumericOperator(*this);
}

bool uct::AdditiveNumericOperator::isAbsolute() const
{
    return false;
}

//
// Is the result within range?
// This function is called from PartialPState::VoltSetting::fullPState and
// PartialPState::FreqSetting::fullPState.
//
bool uct::AdditiveNumericOperator::validRange(value_t prior, Range range) const
{
    double value = (double) prior + (double) addend;
    return value > range.min && value <= range.max;
}

//
// Add the addend to the prior value.
// This function is called from PartialPState::VoltSetting::fullPState and
// PartialPState::FreqSetting::fullPState.
//
uct::NumericOperator::value_t uct::AdditiveNumericOperator::apply(value_t prior) const
{
    return (value_t)((addend_t) prior + addend);
}

// Subfield without flags as text
// Always prefix with '+' or '-' to indicate relative
rmt::String uct::AdditiveNumericOperator::string() const
{
    ostringstream oss;
    oss.setf(ios::showpos);
    oss << addend;
    return oss;
}

// Create an exact copy of 'this'.
uct::NumericOperator *uct::MultiplicativeNumericOperator::clone() const
{
    return new MultiplicativeNumericOperator(*this);
}

bool uct::MultiplicativeNumericOperator::isAbsolute() const
{
    return false;
}

//
// Is the result within range?
// This function is called from PartialPState::VoltSetting::fullPState and
// PartialPState::FreqSetting::fullPState.
//
bool uct::MultiplicativeNumericOperator::validRange(value_t prior, Range range) const
{
    return prior * factor > range.min && prior * factor <= range.max;
}

// Multiply the factor to the prior value
uct::NumericOperator::value_t uct::MultiplicativeNumericOperator::apply(value_t prior) const
{
    return (value_t)(prior * factor);
}

// Subfield without flags as text
// Always prefix with '+' or '-' to indicate relative and suffix with '%'
// This may differ from the source text.  For example "70%" becomes "-30%".
rmt::String uct::MultiplicativeNumericOperator::string() const
{
    ostringstream oss;
    oss.setf(ios::showpos);
    oss << (factor - 1.0) * 100.0 << '%';
    return oss;
}

