/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/utility/rmclkutil.h"

#include "uctexception.h"
#include "uctavfstestspec.h"

/*******************************************************************************
    NameField and FieldVector
*******************************************************************************/

//
// Apply the name field to the avfs test definition.
//
// Although the 'NameField' class is defined on field.h, the implementation of
// this function is here since it is specific to avfs test definitions.
//
uct::ExceptionList uct::NameField::apply(AvfsTestSpec &avfsTestSpec) const
{
    //
    // Since 'name' starts the avfs test, it's not possible to have a redudnancy.
    // Therefore, if this assert fires, the logic in uCT must have failed.
    //
    MASSERT(avfsTestSpec.name.isEmpty());

    // Same the name into the trial spec.
    avfsTestSpec.name = *this;
    return Exception();
}

//
// Apply each field to the partial p-state definition.
//
// Although the 'FieldVector' class is defined on field.h, the implementation of
// this function is here since it is specific to avfs test spec definitions.
//
uct::ExceptionList uct::FieldVector::apply(AvfsTestSpec &avfsTestSpec) const
{
    const_iterator ppField;
    ExceptionList status;

    MASSERT(blockType() == AvfsTestSpecBlock);

    for (ppField = begin(); ppField != end(); ++ppField)
        status.push((*ppField)->apply(avfsTestSpec));

    if (status.isOkay() && avfsTestSpec.name.empty())
        status.push(ReaderException("Anonymous partial p-state definition block",
            *this, __PRETTY_FUNCTION__));

    return status;
}

/*******************************************************************************
    SettingVector
*******************************************************************************/

// Add a new setting object to this vector.
void uct::AvfsTestSpec::Setting::Vector::push(const LutFreqSetting &setting)
{
    // The domain must have valid range to avoid overflopwing the array.
    MASSERT(setting.domain < Domain_Count);

    // Keep track of the union of all setting domains
    adcvoltDomainSet |= BIT(setting.domain);

    // Add the setting to the list for this domain.
    lutfreqDomainMap[setting.domain].push_back(setting);
}

// Add a new setting object to this vector.
void uct::AvfsTestSpec::Setting::Vector::push(const AdcVoltSetting &setting)
{
    // The domain must have valid range to avoid overflopwing the array.
    MASSERT(setting.domain < Domain_Count);

    // Keep track of the union of all setting domains
    adcvoltDomainSet |= BIT(setting.domain);

    // Add the setting to the list for this domain.
    adcvoltDomainMap[setting.domain].push_back(setting);
}

// Number of elements in a non-empty nafll clock domain
unsigned long long uct::AvfsTestSpec::Setting::Vector::cardinality() const
{
    Domain domain;
    unsigned long long cardinality = 0;
    rmt::Vector<LutFreqSetting>::size_type lutfreqSize;
    rmt::Vector<AdcVoltSetting>::size_type adcvoltSize;

    // We can't apply settings that are not there!
    MASSERT(!empty());

    // Just return the number of setting of the first non-empty ADC voltage domain
    for (domain = 0; domain < Domain_Count; ++domain)
        if ((adcvoltSize = adcvoltDomainMap[domain].size()))
        {
            if (!cardinality)
            {
                // found the FIRST non-empty domain, set cardinality
                cardinality = adcvoltDomainMap[domain].size();
            }
            else
            {
                // there is already a non-empty domain, assert that the sizes match
                MASSERT(cardinality == adcvoltSize);
                if ((lutfreqSize = lutfreqDomainMap[domain].size()))
                {
                    // Also if there is an associated LUT frequency domain, then
                    // the size should match as well.
                    MASSERT(adcvoltSize == lutfreqSize);
                }
            }
        }

    if (cardinality)
        return cardinality;

    // Just return the number of setting of the first non-empty LUT frequency domain
    for (domain = 0; domain < Domain_Count; ++domain)
        if ((lutfreqSize = lutfreqDomainMap[domain].size()))
        {
            if (!cardinality)
            {
                // found the FIRST non-empty domain, set cardinality
                cardinality = lutfreqDomainMap[domain].size();
            }
            else
            {
                // there is already a non-empty domain, assert that the sizes match
                MASSERT(cardinality == lutfreqSize);
                if ((adcvoltSize = adcvoltDomainMap[domain].size()))
                {
                    // Also if there is an associated ADC voltage domain, then
                    // the size should match as well.
                    MASSERT(adcvoltSize == lutfreqSize);
                }
            }
        }

    return cardinality;
}

// Apply the 'i'th setting for the cardinality.
// This function is called by PartialPState::fullPState.
uct::ExceptionList uct::AvfsTestSpec::Setting::Vector::fullPState(FullPState &fullPState, unsigned long long i, FreqDomainBitmap freqMask) const
{
    Domain domain;
    ExceptionList status;

    // Apply a setting from each nonempty list of frequency domain settings.
    for (domain = 0; domain < Domain_Count; ++domain)
    {
        // Ignore domains for which no settings have been specified.
        if (adcvoltDomainMap[domain].size())
        {
            // Apply the numeric operator unless this frequency domain has been pruned.
            if (BIT(domain) & freqMask)
            {
                status.push(adcvoltDomainMap[domain][i].fullPState(fullPState));
                if (lutfreqDomainMap[domain].size())
                    status.push(lutfreqDomainMap[domain][i].fullPState(fullPState));
            }
        }
        else if (lutfreqDomainMap[domain].size())
        {
            // Apply the numeric operator unless this frequency domain has been pruned.
            if (BIT(domain) & freqMask)
            {
                status.push(lutfreqDomainMap[domain][i].fullPState(fullPState));
                if (adcvoltDomainMap[domain].size())
                    status.push(adcvoltDomainMap[domain][i].fullPState(fullPState));
            }
        }
    }

    // The fully-defined p-state is invalid if we had any exceptions.
    if (!status.isOkay())
        fullPState.ilwalidate();

    // Done
    return status;
}

// Human-readable representation of this partial p-state
// TBD
rmt::String uct::AvfsTestSpec::Setting::Vector::string() const
{
    Domain domain;
    rmt::Vector<LutFreqSetting>::size_type lutfreqSize, f;
    std::ostringstream oss;

    oss << " cardinality=" << cardinality();

    // Get the settings from each nonempty list of frequency domain settings.
    for (domain = 0; domain < Domain_Count; ++domain)
    {
        // Ignore domains for which no settings have been specified.
        if ((lutfreqSize = lutfreqDomainMap[domain].size()))
        {
            // Print the domain followed by the settings
            oss << " " << DomainNameMap::adcvolt.text(BIT(domain)) << "={";
            oss << lutfreqDomainMap[domain][0].string();
            for (f = 1; f < lutfreqSize; ++f)
                oss << ", " << lutfreqDomainMap[domain][f].string();
            oss << "}";
        }
    }

    // Done
    return oss.str();
}

/*******************************************************************************
    Setting
*******************************************************************************/

// Virtual destructor -- nothing to do
uct::AvfsTestSpec::Setting::~Setting()
{   }

/*******************************************************************************
    LutFreqSetting
*******************************************************************************/

// Tests as skipped for frequencies outside this range
const uct::NumericOperator::Range uct::AvfsTestSpec::LutFreqSetting::range =
    { FREQ_RANGE_MIN, FREQ_RANGE_MAX };   // 0 < f <= 100GHz

// Frequency Text
rmt::String uct::AvfsTestSpec::LutFreqSetting::dec(NumericOperator::value_t kilohertz)
{
    // Is it reasonable to colwert to gigahertz?
    if (kilohertz % 1000000 == 0)
        return rmt::String::dec(kilohertz / 1000000) + "GHz";

    // Is it reasonable to colwert to megaherta?
    if (kilohertz % 1000 == 0)
        return rmt::String::dec(kilohertz / 1000) + "MHz";

    // Kilohertz it is!
    return rmt::String::dec(kilohertz) + "KHz";
}

// Apply this frequency setting to a fully-defined p-state.
// This function is called by AvfsTestSpec::Setting::Vector::fullPState
uct::SolverException uct::AvfsTestSpec::LutFreqSetting::fullPState(uct::FullPState &fullPState) const
{
    // Earlier syntax checking should ensure that the domain is valid.
    MASSERT(domain < AdcVoltDomain_Count);

    // We must have a 'prior' value unless this is an absolute operator.
    if (!fullPState.lutfreq[domain] && !numeric->isAbsolute())
        return SolverException(fullPState.name.defaultTo("<anonymous>") +
            ": Absolute operator required because " +
            DomainNameMap::adcvolt.text(BIT(domain)) +
            " is not specified in the base (vbios) p-state",
            fullPState.start, __PRETTY_FUNCTION__, Exception::Skipped);

    // Make sure we won't overflow or underflow.
    if (!numeric->validRange(fullPState.lutfreq[domain], range))
        return SolverException( fullPState.name.defaultTo("<anonymous>") +
            ": Result " + DomainNameMap::adcvolt.text(BIT(domain)) +
            " frequency out of range (" + dec(range.min) + "," + dec(range.max) + "]",
            fullPState.start, __PRETTY_FUNCTION__, Exception::Skipped);

    // Apply the numeric operator to the frequency.
    fullPState.lutfreq[domain] = numeric->apply(fullPState.lutfreq[domain]);

    // No error
    return SolverException();
}

// Human-readable representation of this setting
rmt::String uct::AvfsTestSpec::LutFreqSetting::string() const
{
    // Return the pertinent info as a string
    return numeric->string();
}

/*******************************************************************************
    AdcVoltSetting
*******************************************************************************/

// Tests as skipped for voltages outside this range.
// Range TBD
const uct::NumericOperator::Range uct::AvfsTestSpec::AdcVoltSetting::range =
    { 0, 10000000 };        // 0 < v <= 10V

// Voltage Text
rmt::String uct::AvfsTestSpec::AdcVoltSetting::dec(NumericOperator::value_t microvolts)
{
    // Is it reasonable to colwert to volts?
    if (microvolts % 1000000 == 0)
        return rmt::String::dec(microvolts/ 1000000) + "V";

    // Is it reasonable to colwert to millivolts?
    if (microvolts % 1000 == 0)
        return rmt::String::dec(microvolts/ 1000) + "mV";

    // Microvolts it is!
    return rmt::String::dec(microvolts) + "uV";
}

// Apply this voltage setting to a fully-defined p-state.
// This function is called by PartialPState::Setting::Vector::fullPState
// TBD Need to check if required
uct::SolverException uct::AvfsTestSpec::AdcVoltSetting::fullPState(FullPState &fullPState) const
{
    // Earlier syntax checking should ensure that the domain is valid.
    MASSERT(domain < AdcVoltDomain_Count);

    // We must have a 'prior' value unless this is an absolute operator.
    if (!fullPState.adcvolt[domain] && !numeric->isAbsolute())
        return SolverException(fullPState.name.defaultTo("<anonymous>") +
            ": Absolute operator required because " +
            DomainNameMap::adcvolt.text(BIT(domain)) +
            " is not specified in the base p-state",
            fullPState.start, __PRETTY_FUNCTION__, Exception::Skipped);

    // Make sure we won't overflow or underflow.
    if (!numeric->validRange(fullPState.adcvolt[domain], range))
        return SolverException(fullPState.name.defaultTo("<anonymous>") +
            ": Result " + DomainNameMap::adcvolt.text(BIT(domain)) +
            " voltage out of range (" + dec(range.min) + ", " + dec(range.max) + "]",
            fullPState.start, __PRETTY_FUNCTION__, Exception::Skipped);

    // Apply the numeric operator to the voltage.
    fullPState.adcvolt[domain] = numeric->apply(fullPState.adcvolt[domain]);

    // No error
    return SolverException();
}

// Human-readable representation of this setting
rmt::String uct::AvfsTestSpec::AdcVoltSetting::string() const
{
    // Return the pertinent info as a string
    return numeric->string();
}

/*******************************************************************************
    LutFreqField
*******************************************************************************/

// See uctdomain.cpp for the domain name map.

// Create a new field object.
uct::Field *uct::AvfsTestSpec::LutFreqField::newField(const CtpStatement &statement)
{
    return new LutFreqField(statement);
}

// Create an exact copy of 'this'.
uct::Field *uct::AvfsTestSpec::LutFreqField::clone() const
{
    return new LutFreqField(*this);
}

// Interpret the statement argument.
uct::ExceptionList uct::AvfsTestSpec::LutFreqField::parse()
{
    ExceptionList status;

    // Domains are not optional
    if (!domain)
        status.push(ReaderException("Missing required adcvolt clock domain",
            *this, __PRETTY_FUNCTION__));

    // The argument is not optional
    if (empty())
        status.push(ReaderException("Missing required frequency specification",
            *this, __PRETTY_FUNCTION__));

    // Parsing is completed in the 'apply' function rather than here so that we
    // don't need a list in this object of pairs of NumericOperator and FlagOperator.
    // This would not only require more memory, but it means we would have to
    // have a new struct.  We don't use a list of Setting objects because
    // Setting::domain would be meaningless since it is only one domnain; the
    // domain bitmap is in this object as CtpStatement::domain.

    return status;
}

// Finish parsing and apply the field to the partial p-state definition.
uct::ExceptionList uct::AvfsTestSpec::LutFreqField::apply(AvfsTestSpec &avfsTestSpec) const
{
    rmt::String                         argList = *this;
    rmt::String                         arg;
    NumericOperator::PointerOrException numeric;
    LutFreqSetting                      setting;
    DomainBitmap                        domainBitmap;
    size_type                           exceptionCount;
    ExceptionList                       status;
    UINT16                              freqMHz;
    UINT08                              ratio;
    AbsoluteNumericOperator            *pOperator = new AbsoluteNumericOperator;

    MASSERT(pOperator);

    // ratio clocks would be ideally @90 % of the GPC
    ratio = (domain == LW2080_CTRL_CLK_DOMAIN_GPC2CLK) ? 100 :
            (domain == LW2080_CTRL_CLK_DOMAIN_XBAR2CLK) ? 97 : 90;
    //
    // Check if 'all' specified - meaning stress test with random frequencies,
    // in that case just generate the random set and exit.
    //
    if (!strncmp(argList.c_str(), "all", argList.size()))
    {
        //
        // Step size of 1 MHz feels like overkill, since the ndiv step size is
        // anyways ~26MHz. Its like we are mapping 20+ trials to the same.
        // TODO: Get the supported MIN/MAX/STEP-size using an RMCTRL call
        //
        for(freqMHz  = LW2080_CTRL_CLK_EMU_SUPPORTED_MIN_FREQUENCY_MHZ;
            freqMHz <= LW2080_CTRL_CLK_EMU_SUPPORTED_MAX_FREQUENCY_MHZ;
            freqMHz += 13)
        {
            setting.domain = BIT_IDX_32(domain);
            pOperator->value = (AbsoluteNumericOperator::value_t)(freqMHz *
                                   ratio * 10);
            setting.numeric  = pOperator;
            avfsTestSpec.settingVector.push(setting);
        }
        return status;
    }

    // Finish parsing each comma-delimited subfield
    while (!argList.empty())
    {
        exceptionCount = status.size();
        arg = argList.extractShard(',');

        // Extract and parse the numeric operator
        numeric = NumericOperator::parse(arg, TestSpec::unitMapKHz);
        MASSERT(numeric.pointer || numeric.exception);

        // Take the resulting operator (in heap).
        setting.numeric.take(numeric.pointer);

        // Check for error
        if (numeric.exception)
            status.push(ReaderException(numeric.exception.message, *this, __PRETTY_FUNCTION__));

        // If this subfield had no exceptions, add it to the list.
        if (status.size() <= exceptionCount)
        {
            // Add a copy of this field for each domain.
            for (domainBitmap = DomainBitmap_Last; domainBitmap; domainBitmap >>= 1)
            {
                // Include only those domains specified in the statement.
                if (domainBitmap & domain)
                {
                    // Setting::Vector::push copies 'setting' for this domain.
                    setting.domain = BIT_IDX_32(domainBitmap);
                    avfsTestSpec.settingVector.push(setting);
                }
            }
        }
    }

    // Done
    return status;
}

// Simple human-readable representation (without file location)
rmt::String uct::AvfsTestSpec::LutFreqField::string() const
{
    if (isEmpty())
        return NULL;

    // This is a bit odd since we are not showing the parsed data.
    return DomainNameMap::adcvolt.nameList(domain) + "lut-freq=" + *this;
}

/*******************************************************************************
    AdcVoltField
*******************************************************************************/

// See uctdomain.cpp for the voltage domain map.

// Create a new field object.
uct::Field *uct::AvfsTestSpec::AdcVoltField::newField(const CtpStatement &statement)
{
    return new AdcVoltField(statement);
}

// Create an exact copy of 'this'.
uct::Field *uct::AvfsTestSpec::AdcVoltField::clone() const
{
    return new AdcVoltField(*this);
}

// Interpret the statement argument.
uct::ExceptionList uct::AvfsTestSpec::AdcVoltField::parse()
{
    ExceptionList status;

    // Domains are not optional
    if (!domain)
        status.push(ReaderException("Missing required voltage domain",
            *this, __PRETTY_FUNCTION__));

    // The argument is not optional
    if (empty())
        status.push(ReaderException("Missing required voltage specification",
            *this, __PRETTY_FUNCTION__));

    // Parsing is completed in the 'apply' function rather than here so that we
    // don't need a list in this object of paris of NumericOperator and FlagOperator.
    // This would not only require more memory, but it means we would have to
    // have a new struct.  We don't use a list of Setting objects because
    // Setting::domain would be meaningless since it is only one domnain; the
    // domain bitmap is in this object as CtpStatement::domain.

    return status;
}

// Finish parsing and apply the field to the partial p-state definition.
uct::ExceptionList uct::AvfsTestSpec::AdcVoltField::apply(AvfsTestSpec &avfsTestSpec) const
{
    rmt::String argList = *this;
    rmt::String arg;
    NumericOperator::PointerOrException numeric;
    AdcVoltSetting setting;
    DomainBitmap domainBitmap;
    size_type exceptionCount;
    ExceptionList status;

    // Finish parsing each comma-delimited subfield
    while (!argList.empty())
    {
        exceptionCount = status.size();
        arg = argList.extractShard(',');

        // Extract and parse the numeric operator
        numeric = NumericOperator::parse(arg, TestSpec::unitMapuV);
        MASSERT(numeric.pointer || numeric.exception);

        // Remember the resulting operator
        setting.numeric = numeric.pointer;

        // Check for error
        if (numeric.exception)
            status.push(ReaderException(numeric.exception.message, *this, __PRETTY_FUNCTION__));

        // Flags do not apply to voltage specifications.
        while (!arg.empty())
            status.push(ReaderException(arg.extractShard(':') + ": Superfluous flag on voltage specification",
                *this, __PRETTY_FUNCTION__));

        // If this subfield had no exceptions, add it to the list.
        if (status.size() <= exceptionCount)
        {
            // Add a copy of this field for each domain.
            for (domainBitmap = DomainBitmap_Last; domainBitmap; domainBitmap >>= 1)
            {
                // Include only those domains specified in the statement.
                if (domainBitmap & domain)
                {
                    // Setting::Vector::push copies 'setting' for this domain.
                    setting.domain = BIT_IDX_32(domainBitmap);
                    avfsTestSpec.settingVector.push(setting);
                }
            }
        }
    }

    // Done
    return status;
}

// Simple human-readable representation (without file location)
rmt::String uct::AvfsTestSpec::AdcVoltField::string() const
{
    if (isEmpty())
        return NULL;

    // This is a bit odd since we are not showing the parsed data.
    return DomainNameMap::adcvolt.nameList(domain) + "adcvolt=" + *this;
}

//!
//! \brief Utility that checks if this pstate can be supported by PSTATE RMAPI
//!
//! This is important because if there is a clock/voltage domain in this pstate
//! class, then setting the pstate using RM will fail
//!
//! \param[in]  vbios   a vbios pstate to be used to see which domains are
//!                     supported
//!
//! \return true if this pstate is supported by RM's SET_PSTATE api. false
//!         otherwise
//------------------------------------------------------------------------------
bool uct::AvfsTestSpec::supportPStateMode(const VbiosPState &vbios) const
{
    //
    // If this object contains any frequency (clock) domains or voltage domains
    // that are not defined in the VBIOS, then the fully-defined p-state that
    // results will also.  Therefore, it would not be possible to use the result
    // p-state to modify the VBIOS p-state for use in the p-state (perf) RMAPI.
    //
    return TRUE;
}

//
// Compute the 'i'th fully-defined p-state in the result set (cartesian
// product) using the specified p-state as the basis.
// This function is called by PStateReference::fullPState.
//
uct::ExceptionList uct::AvfsTestSpec::fullPState(
    uct::FullPState &fullPState,
    unsigned long long i,
    FreqDomainBitmap freqMask) const
{
    ExceptionList status = settingVector.fullPState(fullPState, i, freqMask);
    fullPState.name += rmt::String(":") + this->name;
    return status;
}

// Human-readable representation of this partial p-state
rmt::String uct::AvfsTestSpec::debugString(const char *info) const
{
    std::ostringstream oss;

    // File name and line number (location) if applicable
    oss << name.locationString();

    // Additional info if any
    if (info && *info)
        oss << info << ": ";

    // Trial name if available
    if (!name.empty())
        oss << name << ": ";

    // Setting info
    if (settingVector.empty())
        oss << "empty";
    else
        oss << settingVector.string();

    // Done
    return oss.str();
}

