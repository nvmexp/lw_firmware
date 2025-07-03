/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/utility/rmclkutil.h"

#include "uctexception.h"
#include "uctpartialpstate.h"

/*******************************************************************************
    SourceField
*******************************************************************************/

// Apply the source field to the p-state definition
uct::ExceptionList uct::TestSpec::SourceField::apply(uct::PartialPState &pstate) const
{
    rmt::String argList = *this;
    rmt::String arg;
    RmtClockUtil clkUtil;
    UINT32 source;
    uct::PartialPState::SourceSetting setting;
    DomainBitmap domainBitmap;
    size_type exceptionCount;
    ExceptionList status;

    // Add a copy of this field for each domain.
    for (domainBitmap = DomainBitmap_Last; domainBitmap; domainBitmap >>= 1)
    {
        // Include only those domains specified in the statement.
        if (domainBitmap & domain)
        {
            setting.domain = BIT_IDX_32(domainBitmap);
        }
    }

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
        if (status.size() <= exceptionCount) {
            setting.source = source;
            pstate.settingVector.push(setting);
        }
    }

    // Done
    return status;
}

/*******************************************************************************
    NameField and FieldVector
*******************************************************************************/

//
// Apply the name field to the partial p-state definition.
//
// Although the 'NameField' class is defined on field.h, the implementation of
// this function is here since it is specific to partial p-state definitions.
//
uct::ExceptionList uct::NameField::apply(PartialPState &pstate) const
{
    //
    // Since 'name' starts the partial p-state, it's not possible to have a redudnancy.
    // Therefore, if this assert fires, the logic in uCT must have failed.
    //
    MASSERT(pstate.name.isEmpty());

    // Same the name into the trial spec.
    pstate.name = *this;
    return Exception();
}

//
// Apply each field to the partial p-state definition.
//
// Although the 'FieldVector' class is defined on field.h, the implementation of
// this function is here since it is specific to partial p-state definitions.
//
uct::ExceptionList uct::FieldVector::apply(PartialPState &pstate) const
{
    const_iterator ppField;
    ExceptionList status;

    MASSERT(blockType() == PartialPStateBlock);

    for (ppField = begin(); ppField != end(); ++ppField)
        status.push((*ppField)->apply(pstate));

    if (status.isOkay() && pstate.name.empty())
        status.push(ReaderException("Anonymous partial p-state definition block",
            *this, __PRETTY_FUNCTION__));

    return status;
}

/*******************************************************************************
    SettingVector
*******************************************************************************/

// Add a new setting object to this vector.
void uct::PartialPState::Setting::Vector::push(const FreqSetting &setting)
{
    // The domain must have valid range to avoid overflopwing the array.
    MASSERT(setting.domain < Domain_Count);

    // Keep track of the union of all setting domains
    freqDomainSet |= BIT(setting.domain);

    // Add the setting to the list for this domain.
    freqDomainMap[setting.domain].push_back(setting);
}

// Add a new setting object to this vector.
void uct::PartialPState::Setting::Vector::push(const VoltSetting &setting)
{
    // The domain must have valid range to avoid overflopwing the array.
    MASSERT(setting.domain < Domain_Count);

    // Keep track of the union of all setting domains
    voltDomainSet |= BIT(setting.domain);

    // Add the setting to the list for this domain.
    this->voltDomainMap[setting.domain].push_back(setting);
}

// Add a new setting object to this vector.
void uct::PartialPState::Setting::Vector::push(const SourceSetting &setting)
{
    // The domain must have valid range to avoid overflopwing the array.
    MASSERT(setting.domain < Domain_Count);

    // Add the setting to the list for this domain.
    this->sourceDomainMap[setting.domain].push_back(setting);
}

// Number of elements in the cartesian product
// MSVC doesn't like "rmt::Vector<Setting>::size_type" because Setting is abstract.
unsigned long long uct::PartialPState::Setting::Vector::cardinality() const
{
    Domain domain;
    unsigned long long cardinality = 1;

    // We can't apply settings that are not there!
    MASSERT(!empty());

    // Multiply by the number of settings in each nonempty domain.
    for (domain = 0; domain < Domain_Count; ++domain)
        if (freqDomainMap[domain].size())
            cardinality *= freqDomainMap[domain].size();

    // What's good for the frequency domains is good for the voltage domains.
    for (domain = 0; domain < Domain_Count; ++domain)
        if (voltDomainMap[domain].size())
            cardinality *= voltDomainMap[domain].size();

    // That's all there is to it!
    return cardinality;
}

// Apply the 'i'th setting in the cardesian product.
// This function is called by PartialPState::fullPState.
// MSVC doesn't like "rmt::Vector<Setting>::size_type" because Setting is abstract.
uct::ExceptionList uct::PartialPState::Setting::Vector::fullPState(FullPState &fullPState, unsigned long long i, FreqDomainBitmap freqMask) const
{
    Domain domain;
    rmt::Vector<FreqSetting>::size_type freqSize;
    rmt::Vector<VoltSetting>::size_type voltSize;
    unsigned long long idx;
    UINT32 u;
    ExceptionList status;

    // Apply a setting from each nonempty list of frequency domain settings.
    for (domain = 0; domain < Domain_Count; ++domain)
    {
        // Ignore domains for which no settings have been specified.
        freqSize = freqDomainMap[domain].size();
        if (freqSize)
        {
            idx = i % freqSize;

            // Apply the numeric operator unless this frequency domain has been pruned.
            if (BIT(domain) & freqMask)
                status.push(freqDomainMap[domain][idx].fullPState(fullPState));

            // If there is a source setting we should apply it. If we have less # of
            // source settings than we have frequency combinations then any more
            // frequency combinations will use the last source setting. Otherwise
            // we leave it as default
            u = static_cast<UINT32>(sourceDomainMap[domain].size());
            if(u != 0) {
                if(idx < u)
                    status.push(sourceDomainMap[domain][idx].fullPState(fullPState));
                else
                    status.push(sourceDomainMap[domain][u-1].fullPState(fullPState));
            }

            // Advance the index beyond this frequency domain.
            i /= freqSize;
        }
    }

    // Apply a setting from each nonempty list of voltage domain settings.
    for (domain = 0; domain < Domain_Count; ++domain)
    {
        // Ignore domains for which no settings have been specified.
        voltSize = voltDomainMap[domain].size();
        if (voltSize)
        {
            // Apply the numeric operator unless this voltage domain.
            status.push(voltDomainMap[domain][i % voltSize].fullPState(fullPState));

            // Advance the index beyond this voltage domain.
            i /= voltSize;
        }
    }

    // If there is anything left over, the 'i' was out of range.
    MASSERT(i == 0);

    // The fully-defined p-state is invalid if we had any exceptions.
    if (!status.isOkay())
        fullPState.ilwalidate();

    // Done
    return status;
}

// Human-readable representation of this partial p-state
rmt::String uct::PartialPState::Setting::Vector::string() const
{
    Domain domain;
    rmt::Vector<FreqSetting>::size_type freqSize, f;
    rmt::Vector<VoltSetting>::size_type voltSize, v;
    std::ostringstream oss;

    oss << " cardinality=" << cardinality();

    // Get the settings from each nonempty list of frequency domain settings.
    for (domain = 0; domain < Domain_Count; ++domain)
    {
        // Ignore domains for which no settings have been specified.
        freqSize = freqDomainMap[domain].size();
        if (freqSize)
        {
            // Print the domain followed by the settings
            oss << " " << DomainNameMap::freq.text(BIT(domain)) << "={";
            oss << freqDomainMap[domain][0].string();
            for (f = 1; f < freqSize; ++f)
                oss << ", " << freqDomainMap[domain][f].string();
            oss << "}";
        }
    }

    // Get the settings from each nonempty list of voltage domain settings.
    for (domain = 0; domain < Domain_Count; ++domain)
    {
        // Ignore domains for which no settings have been specified.
        voltSize = voltDomainMap[domain].size();
        if (voltSize)
        {
            // Print the domain followed by the settings
            oss << " " << DomainNameMap::volt.text(BIT(domain)) << "={";
            oss << voltDomainMap[domain][0].string();
            for (v = 1; v < voltSize; ++v)
                oss << ", " << voltDomainMap[domain][v].string();
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
uct::PartialPState::Setting::~Setting()
{   }

/*******************************************************************************
    SourceSetting
*******************************************************************************/

// Apply this source setting to a fully-defined p-state.
// This function is called by PartialPState::Setting::Vector::fullPState
uct::SolverException uct::PartialPState::SourceSetting::fullPState(uct::FullPState &fullPState) const
{
    // Earlier syntax checking should ensure that the domain is valid.
    MASSERT(domain < FreqDomain_Count);

    // At this point the source has already been validated
    fullPState.source[domain] = source;

    // No error
    return SolverException();
}

// Human-readable representation of this setting
rmt::String uct::PartialPState::SourceSetting::string() const
{
    RmtClockUtil clkUtil;

    // Return the pertinent info as a string
    return rmt::String(clkUtil.GetClkSourceName(source));
}

/*******************************************************************************
    FreqSetting
*******************************************************************************/

// Tests as skipped for frequencies outside this range
const uct::NumericOperator::Range uct::PartialPState::FreqSetting::range =
    { FREQ_RANGE_MIN, FREQ_RANGE_MAX };   // 0 < f <= 100GHz

// Frequency Text
rmt::String uct::PartialPState::FreqSetting::dec(NumericOperator::value_t kilohertz)
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
// This function is called by PartialPState::Setting::Vector::fullPState
uct::SolverException uct::PartialPState::FreqSetting::fullPState(uct::FullPState &fullPState) const
{
    // Earlier syntax checking should ensure that the domain is valid.
    MASSERT(domain < FreqDomain_Count);

    // We must have a 'prior' value unless this is an absolute operator.
    if (!fullPState.freq[domain] && !numeric->isAbsolute())
        return SolverException(fullPState.name.defaultTo("<anonymous>") +
            ": Absolute operator required because " +
            DomainNameMap::freq.text(BIT(domain)) +
            " is not specified in the base (vbios) p-state",
            fullPState.start, __PRETTY_FUNCTION__, Exception::Skipped);

    // Make sure we won't overflow or underflow.
    if (!numeric->validRange(fullPState.freq[domain], range))
        return SolverException( fullPState.name.defaultTo("<anonymous>") +
            ": Result " + DomainNameMap::freq.text(BIT(domain)) +
            " frequency out of range (" + dec(range.min) + "," + dec(range.max) + "]",
            fullPState.start, __PRETTY_FUNCTION__, Exception::Skipped);

    // Apply the numeric operator to the frequency.
    fullPState.freq[domain] = numeric->apply(fullPState.freq[domain]);

    // Apply the flag operator to the flags.
    fullPState.flag[domain] = flag.apply(fullPState.flag[domain]);

    // No error
    return SolverException();
}

// Human-readable representation of this setting
rmt::String uct::PartialPState::FreqSetting::string() const
{
    // Return the pertinent info as a string
    return numeric->string() + flag.string(TestSpec::flagNameMap);
}

/*******************************************************************************
    VoltSetting
*******************************************************************************/

// Tests as skipped for voltages outside this range.
const uct::NumericOperator::Range uct::PartialPState::VoltSetting::range =
    { 0, 10000000 };        // 0 < v <= 10V

// Voltage Text
rmt::String uct::PartialPState::VoltSetting::dec(NumericOperator::value_t microvolts)
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
uct::SolverException uct::PartialPState::VoltSetting::fullPState(FullPState &fullPState) const
{
    // Earlier syntax checking should ensure that the domain is valid.
    MASSERT(domain < VoltDomain_Count);

    // We must have a 'prior' value unless this is an absolute operator.
    if (!fullPState.volt[domain] && !numeric->isAbsolute())
        return SolverException(fullPState.name.defaultTo("<anonymous>") +
            ": Absolute operator required because " +
            DomainNameMap::volt.text(BIT(domain)) +
            " is not specified in the base p-state",
            fullPState.start, __PRETTY_FUNCTION__, Exception::Skipped);

    // Make sure we won't overflow or underflow.
    if (!numeric->validRange(fullPState.volt[domain], range))
        return SolverException(fullPState.name.defaultTo("<anonymous>") +
            ": Result " + DomainNameMap::volt.text(BIT(domain)) +
            " voltage out of range (" + dec(range.min) + ", " + dec(range.max) + "]",
            fullPState.start, __PRETTY_FUNCTION__, Exception::Skipped);

    // Apply the numeric operator to the voltage.
    fullPState.volt[domain] = numeric->apply(fullPState.volt[domain]);

    // No error
    return SolverException();
}

// Human-readable representation of this setting
rmt::String uct::PartialPState::VoltSetting::string() const
{
    // Return the pertinent info as a string
    return numeric->string();
}

/*******************************************************************************
    FreqField
*******************************************************************************/

// See uctdomain.cpp for the domain name map.

// Create a new field object.
uct::Field *uct::PartialPState::FreqField::newField(const CtpStatement &statement)
{
    return new FreqField(statement);
}

// Create an exact copy of 'this'.
uct::Field *uct::PartialPState::FreqField::clone() const
{
    return new FreqField(*this);
}

// Interpret the statement argument.
uct::ExceptionList uct::PartialPState::FreqField::parse()
{
    ExceptionList status;

    // Domains are not optional
    if (!domain)
        status.push(ReaderException("Missing required frequency (clock) domain",
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
// Previously, UCTVoltField::parseFieldShard and UCTFreqField::parseFieldShard contained the logic of this function.
uct::ExceptionList uct::PartialPState::FreqField::apply(PartialPState &pstate) const
{
    rmt::String argList = *this;
    rmt::String arg;
    NumericOperator::PointerOrException numeric;
    FreqSetting setting;
    DomainBitmap domainBitmap;
    size_type exceptionCount;
    ExceptionList status;

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

        // Look for optional flags
        if (!arg.empty())
            status.push(setting.flag.parse(arg, *this, TestSpec::flagNameMap));

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
                    pstate.settingVector.push(setting);
                }
            }
        }
    }

    // Done
    return status;
}

// Simple human-readable representation (without file location)
rmt::String uct::PartialPState::FreqField::string() const
{
    if (isEmpty())
        return NULL;

    // This is a bit odd since we are not showing the parsed data.
    return DomainNameMap::freq.nameList(domain) + "freq=" + *this;
}

/*******************************************************************************
    VoltField
*******************************************************************************/

// See uctdomain.cpp for the voltage domain map.

// Create a new field object.
uct::Field *uct::PartialPState::VoltField::newField(const CtpStatement &statement)
{
    return new VoltField(statement);
}

// Create an exact copy of 'this'.
uct::Field *uct::PartialPState::VoltField::clone() const
{
    return new VoltField(*this);
}

// Interpret the statement argument.
uct::ExceptionList uct::PartialPState::VoltField::parse()
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
// Previously, UCTVoltField::parseFieldShard and UCTFreqField::parseFieldShard contained the logic of this function.
uct::ExceptionList uct::PartialPState::VoltField::apply(PartialPState &pstate) const
{
    rmt::String argList = *this;
    rmt::String arg;
    NumericOperator::PointerOrException numeric;
    VoltSetting setting;
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
                    pstate.settingVector.push(setting);
                }
            }
        }
    }

    // Done
    return status;
}

// Simple human-readable representation (without file location)
rmt::String uct::PartialPState::VoltField::string() const
{
    if (isEmpty())
        return NULL;

    // This is a bit odd since we are not showing the parsed data.
    return DomainNameMap::volt.nameList(domain) + "volt=" + *this;
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
bool uct::PartialPState::supportPStateMode(const VbiosPState &vbios) const
{
    //
    // If this object contains any frequency (clock) domains or voltage domains
    // that are not defined in the VBIOS, then the fully-defined p-state that
    // results will also.  Therefore, it would not be possible to use the result
    // p-state to modify the VBIOS p-state for use in the p-state (perf) RMAPI.
    //
    return !settingVector.containsAnyDomain(
        ~vbios.freqDomainSet(), ~vbios.voltDomainSet());
}

//
// Compute the 'i'th fully-defined p-state in the result set (cartesian
// product) using the specified p-state as the basis.
// This function is called by PStateReference::fullPState.
//
uct::ExceptionList uct::PartialPState::fullPState(
    uct::FullPState &fullPState,
    unsigned long long i,
    FreqDomainBitmap freqMask) const
{
    ExceptionList status = settingVector.fullPState(fullPState, i, freqMask);
    fullPState.name += rmt::String(":") + this->name;
    return status;
}

// Human-readable representation of this partial p-state
rmt::String uct::PartialPState::debugString(const char *info) const
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

