/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file
//!
//! Implements Sine Test abstractions
//!

#include <sstream>
#include <math.h>

#include "gpu/utility/rmclkutil.h"

#include "uctexception.h"
#include "uctoperator.h"
#include "uctpstate.h"
#include "uctsinespec.h"
#include "uctfield.h"
#include "ucttrialspec.h"

/*******************************************************************************
    SourceField
*******************************************************************************/

// Apply the source field to the  definition
uct::ExceptionList uct::TestSpec::SourceField::apply(uct::SineSpec &sinespec) const
{
    rmt::String argList = *this;
    rmt::String arg;
    RmtClockUtil clkUtil;
    UINT32 source;
    FreqDomain domainBit = BIT_IDX_32(this->domain);
    uct::SineSpec::SineSetting &setting = sinespec.createSettingIfMissing(domainBit);
    size_type exceptionCount;
    ExceptionList status;

    // Finish parsing each comma-delimited subfield
    while (!argList.empty())
    {
        exceptionCount = status.size();
        arg = argList.extractShard(',');

        source = clkUtil.GetClkSourceValue(arg.c_str());

        // Validate
        if(source == 0)
            status.push(ReaderException("Invalid clock source " + arg,
                *this, __PRETTY_FUNCTION__));

        // If this subfield had no exceptions, add it to the list.
        if (status.size() <= exceptionCount)
            setting.source.push_back(source);
    }

    // Done
    return status;
}

/*******************************************************************************
    NameField and FieldVector
*******************************************************************************/

//
// Apply the name field to the sine spec.
//
uct::ExceptionList uct::NameField::apply(SineSpec &sinespec) const
{
    //
    // Since 'name' starts the sinespec, it's not possible to have a redudnancy.
    // Therefore, if this assert fires, the logic in UCT must have failed.
    //
    MASSERT(sinespec.name.isEmpty());

    sinespec.name = *this;
    return Exception();
}

//
// Apply each line in a block into the sine spec
//
uct::ExceptionList uct::FieldVector::apply(SineSpec &sinespec) const
{
    const_iterator ppField;
    ExceptionList status;

    MASSERT(blockType() == SineSpecBlock);

    for (ppField = begin(); ppField != end(); ++ppField)
        status.push((*ppField)->apply(sinespec));

    // All fields applied successfully, we will error check on a block level
    if (status.isOkay())
    {
        ExceptionList ex = sinespec.checkErrors();
        if(!ex.isOkay())
            status.push(ReaderException(ex.front()->message, *this, __PRETTY_FUNCTION__));
    }

    return status;
}

/*******************************************************************************
    SineSetting
*******************************************************************************/

//
// Initialize variables to default values
//
uct::SineSpec::SineSetting::SineSetting()
{
    // Initialize variables to a default value. These can be checked later to see
    // if they have been overwritten by an actual user definition
    domain = LW2080_CTRL_CLK_DOMAIN_UNDEFINED;

    // Initialize parameters to defaul values
    alpha = beta = gamma = omega = 0.0;
}

//
// Check that all parameters in the setting is correct
//
uct::ExceptionList uct::SineSpec::SineSetting::checkErrors() const
{
    ExceptionList status;

    // At this point all fields should be parsed for this block, if any values
    // are missing or is default it is not because of invalid values since that
    // would have triggered an error on reading the field.

    if(domain == LW2080_CTRL_CLK_DOMAIN_UNDEFINED)
        status.push(Exception("Domain name missing", __PRETTY_FUNCTION__,
            Exception::Syntax));

    if(alpha == 0)
        status.push(Exception(DomainNameMap::freq.text(BIT(domain)) +
            ": Alpha missing or is zero, misspelt domain?",
            __PRETTY_FUNCTION__, Exception::Syntax));

    if(beta == 0)
        status.push(Exception(DomainNameMap::freq.text(BIT(domain)) +
            ": Beta missing or is zero, misspelt domain?",
            __PRETTY_FUNCTION__, Exception::Syntax));

    if(gamma == 0)
        status.push(Exception(DomainNameMap::freq.text(BIT(domain)) +
            ": Gamma missing or is zero, misspelt domain?",
            __PRETTY_FUNCTION__, Exception::Syntax));

    return status;
}

//
// Compute the ith iteration frequency value and apply it to the
// full pstate (subject to freqMask pruning)
//
uct::SolverException uct::SineSpec::SineSetting::fullPState(
    uct::FullPState &fullPState,
    unsigned long long i,
    FreqDomainBitmap freqMask) const
{
    UINT32 freq;
    UINT32 u;

    MASSERT(domain < FreqDomain_Count);

    // Check if this domain is pruned, just return if it is
    if (!(BIT(domain) & freqMask))
        return SolverException();

    // Compute the frequency value for the current iteration
    //  f = alpha + beta * t^omega * sin(gamma * t)
    freq = (UINT32) (alpha + beta * pow((double)i,omega) * sin(gamma * (double)i));

    // Check if the computed frequency is within range
    if(freq <= FREQ_RANGE_MIN || freq > FREQ_RANGE_MAX)
    {
            return SolverException( fullPState.name.defaultTo("<anonymous>") +
                ": Result " + DomainNameMap::freq.text(BIT(domain)) +
                "=" + rmt::String::decf(freq, 0) + " frequency out of range (" +
                rmt::String::decf(FREQ_RANGE_MIN, 0) + "," +
                rmt::String::decf(FREQ_RANGE_MAX, 0) + "]",
                fullPState.start, __PRETTY_FUNCTION__, Exception::Skipped);
    }

    // assign freq value to domain
    fullPState.freq[domain] = freq;

    // do we have source values we can apply?
    // see uct::PartialPState::Setting::Vector::fullPState
    u = static_cast<UINT32>(source.size());
    if(u > 0) {
        if(i < u)
            fullPState.source[domain] = source[i];
        else
            fullPState.source[domain] = source[u-1];
    }

    // apply flags
    fullPState.flag[domain] = flags.apply(fullPState.flag[domain]);

    return SolverException();
}

//
// Human readable string representation of the setting
//
rmt::String uct::SineSpec::SineSetting::string() const
{
    // print the setting information
    std::ostringstream oss;
    oss << "Domain: "  << DomainNameMap::freq.text(BIT(domain));
    if (flags.mask != 0)
        oss << " with flags " << flags.string(TestSpec::flagNameMap);
    oss << " [alpha: " << alpha;
    oss << ", beta: "  << beta;
    oss << ", gamma: " << gamma;
    oss << ", omega: " << omega;
    oss << "]";

    return oss.str();
}

/*******************************************************************************
    SineSpec
*******************************************************************************/

//
// Error checking on the block level, making sure that the block has a name,
// valid iterations etc, and then delegate to individual settings to see if
// they are correct
//
uct::ExceptionList uct::SineSpec::checkErrors() const
{
    ExceptionList status;
    std::map<FreqDomain, SineSetting>::const_iterator it;

    // At this point all fields should be parsed for this block, if any values
    // are missing it is not because of invalid values since that would have
    // triggered an error on reading the field.
    //
    // Here default values means that the field was missing from the block for
    // that domain
    if (name.empty())
        status.push(
            Exception("Anonymous Sine Spec definition block, missing name field?",
                __PRETTY_FUNCTION__, Exception::Syntax));

    if (iterations <= 0)
        status.push(Exception("Missing required iterations field",
            __PRETTY_FUNCTION__, Exception::Syntax));

    for (it = settingMap.begin(); it != settingMap.end(); ++it)
        status.push(it->second.checkErrors());

    return status;
}

//
// Human readable string representation of the setting
//
uct::SineSpec::SineSetting &uct::SineSpec::createSettingIfMissing(FreqDomain domain)
{
    std::map<FreqDomain, SineSetting>::iterator it = settingMap.find(domain);

    // Return element if exist, otherwise create new and return
    if (it == settingMap.end())
    {
        settingMap.insert(
            std::pair<FreqDomain, SineSetting>(domain, SineSetting())
        );
        it = settingMap.find(domain);
        it->second.domain = domain;
    }

    return it->second;
}

//
// Delegate computation and application to full pstate to the individual settings
//
uct::ExceptionList uct::SineSpec::fullPState(
    uct::FullPState &fullPState,
    unsigned long long i,
    FreqDomainBitmap freqMask) const
{
    ExceptionList status;

    std::map<FreqDomain, SineSetting>::const_iterator it;

    // Set the name for this generated full pstate
    fullPState.name += rmt::String(":") + this->name;

    for (it = settingMap.begin(); it != settingMap.end(); ++it)
        status.push(it->second.fullPState(fullPState, i, freqMask));

    return status;
}

//
// Human-readable representation of this Sine specification
//
rmt::String uct::SineSpec::debugString(const char *info) const
{
    std::ostringstream oss;
    std::map<FreqDomain, SineSetting>::const_iterator it;

    // File name and line number (location) if applicable
    oss << name.locationString();

    // Additional info if any
    if (info && *info)
        oss << info << ": ";

    // Trial name if available
    if (!name.empty())
        oss << name << ": ";

    // Setting info
    for (it = settingMap.begin(); it != settingMap.end(); ++it)
    {
        oss << it->second.string();
        oss << ", ";
    }

    // Done
    return oss.str();
}

//
// Check that all domains in this sine spec is supported by the vbios
//
bool uct::SineSpec::supportPStateMode(const VbiosPState &vbios) const
{
    std::map<FreqDomain, SineSetting>::const_iterator it;

    for (it = settingMap.begin(); it != settingMap.end(); ++it)
        if (it->first & vbios.freqDomainSet())
            return false;

    return true;
}

/*******************************************************************************
    SineSpec Iterations Field
*******************************************************************************/

// Create a new field object
uct::Field *uct::SineSpec::IterationsField::newField(const CtpStatement &statement)
{
    return new IterationsField(statement);
}

// Create an exact copy of 'this'
uct::Field *uct::SineSpec::IterationsField::clone() const
{
    return new IterationsField(*this);
}

// Interpret the statement argument
uct::ExceptionList uct::SineSpec::IterationsField::parse()
{
    ExceptionList status;

    // argument is required
    if (empty())
        status.push(ReaderException("Iteration value is required", *this,
            __PRETTY_FUNCTION__));

    // actual parsing is done in apply

    return status;
}

// Apply this statement to a sine spec
uct::ExceptionList uct::SineSpec::IterationsField::apply(SineSpec &sinespec) const
{
    ExceptionList status;

    rmt::String arg = *this;
    long val = arg.extractLong();

    if(val <= 0)
        status.push(
            ReaderException("Invalid iteration value (positive integer expeceted)",
            *this, __PRETTY_FUNCTION__));
    else
        sinespec.iterations = (unsigned long) val;

    return status;
}

/*******************************************************************************
    SineSpec Domain Field
*******************************************************************************/

// Create a new field object
uct::Field *uct::SineSpec::FlagField::newField(const CtpStatement &statement)
{
    return new FlagField(statement);
}

// Create an exact copy of 'this'
uct::Field *uct::SineSpec::FlagField::clone() const
{
    return new FlagField(*this);
}

// Interpret the statement argument
uct::ExceptionList uct::SineSpec::FlagField::parse()
{
    ExceptionList status;

    // arguement (domain name) is required
    if (empty())
        status.push(ReaderException("Missing required domain value",
            *this, __PRETTY_FUNCTION__));

    // actual parsing done in apply()

    return status;
}

// Apply this statement to a sine spec
uct::ExceptionList uct::SineSpec::FlagField::apply(SineSpec &sinespec) const
{
    ExceptionList status;
    FreqDomain domainBit = BIT_IDX_32(this->domain);

    SineSetting &setting = sinespec.createSettingIfMissing(domainBit);

    // parse the flags
    status.push(setting.flags.parse(*this, *this, TestSpec::flagNameMap));

    return status;
}

/*******************************************************************************
    SineSpec Param Field
*******************************************************************************/

// Create a new field object
uct::Field *uct::SineSpec::ParamField::newField(const CtpStatement &statement)
{
    return new ParamField(statement);
}

// Create an exact copy of 'this'
uct::Field *uct::SineSpec::ParamField::clone() const
{
    return new ParamField(*this);
}

// Interpret the statement argument
uct::ExceptionList uct::SineSpec::ParamField::parse()
{
    ExceptionList status;

    // The parameter value is not optional
    if (empty())
        status.push(ReaderException("Missing required param value",
            *this, __PRETTY_FUNCTION__));

    return status;
}

// Apply this statement to a sine spec
uct::ExceptionList uct::SineSpec::ParamField::apply(SineSpec &sinespec) const
{
    rmt::String argText = *this;
    rmt::String arg;
    ExceptionList status;
    ReaderException e;
    rmt::String unit;
    double val;
    FreqDomain domainBit = BIT_IDX_32(this->domain);
    bool isFreqVal = (type == Type::Alpha || type == Type::Beta);

    SineSetting &setting = sinespec.createSettingIfMissing(domainBit);

    // We expect a single value for all of the fields in this block
    arg = argText.extractShard(',');
    if (arg.empty() || !argText.empty())
    {
        status.push(ReaderException("Field expects exactly one argument",
            *this, __PRETTY_FUNCTION__));
        return status;
    }

    // parse double
    unit = arg;
    val = unit.extractDouble();

    // check that there is no error
    if(errno)
        status.push(ReaderException(
            rmt::String(type->keyword) + ": Invalid number, floating-point expected",
            *this, __PRETTY_FUNCTION__));

    // Interpret and apply the unit (if any).
    if (!unit.empty())
    {
        if (isFreqVal)
        {
            short exponent = TestSpec::unitMapKHz.find_else(unit, SHRT_MIN);

            // Special value indicates a syntax error
            if (exponent <= SHRT_MIN)
                status.push(ReaderException(unit + ": Invalid unit of measure",
                    *this, __PRETTY_FUNCTION__));

            // Apply the unit of measure as an exponent
            val *= ::pow((double) 10.0, (int) exponent);
        }
        else
        {
            status.push(ReaderException(
                rmt::String(type->keyword) + ": Cannot have unit",
                *this, __PRETTY_FUNCTION__));
        }
    }

    // enforce: alpha >= 0, 0 <= omega <= 1, gamma >= 0
    if(type != Type::Beta && val < 0)
        status.push(ReaderException(
            rmt::String(type->keyword) + ": Parameter must be non-negative",
            *this, __PRETTY_FUNCTION__));

    if(type == Type::Omega && val > 1)
        status.push(ReaderException(
            rmt::String(type->keyword) + ": Omega must be within range [0,1]",
            *this, __PRETTY_FUNCTION__));

    // For alpha & beta, Check for a value that has more decimal places that we can handle.
    // For example, since RMAPI uses whole KHz for frequencies, we can't do "1.0625MHz".
    if(isFreqVal)
    {
        double whole;
        modf(val, &whole);
        if (val != whole)
            status.push(ReaderException(arg + ": Loss of precision", *this,
                __PRETTY_FUNCTION__));
    }

    switch(type)
    {
    case Type::Alpha:
        setting.alpha = val;
        break;
    case Type::Beta:
        setting.beta = val;
        break;
    case Type::Gamma:
        setting.gamma = val;
        break;
    case Type::Omega:
        setting.omega = val;
        break;
    default:
        MASSERT(0);
    }

    // Done
    return status;
}

