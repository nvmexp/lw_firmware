/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file
//!
//! Implements Trial Spec and related settings
//!

#include <cmath>
#include <cerrno>
#include <sstream>
#include <iomanip>

#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl2080/ctrl2080fb.h"
#include "gpu/tests/rm/utility/rmtstring.h"
#include "ucttrialspec.h"
#include "ucttestreference.h"

#define QUOTE(x) #x

/*******************************************************************************
    Name Field
*******************************************************************************/

//
// Apply the name field to the trial specification.
//
// Although the 'NameField' class is defined on field.h, the implementation of
// this function is here since it is specific to trial specifications.
//
uct::ExceptionList uct::NameField::apply(TrialSpec &trial) const
{
    //
    // Since 'name' starts the trial spec, it's not possible to have a redudnancy.
    // Therefore, if this assert fires, the logic in uCT must have failed.
    //
    MASSERT(trial.name.isEmpty());

    // Same the name into the trial spec.
    trial.name = *this;
    return Exception();
}

/*******************************************************************************
    Field Vector
*******************************************************************************/

//
// Apply each field to the trial specification.
//
// Although the 'FieldVector' class is defined on field.h, the implementation of
// this function is here since it is specific to trial specifications.
//
uct::ExceptionList uct::FieldVector::apply(TrialSpec &trial) const
{
    const_iterator ppField;
    ExceptionList status;

    //
    // None of the statements in the block can be directives or indicitive of
    // partial p-state references.
    //
    MASSERT(blockType() == TrialSpecBlock);

    // Apply each field on to the trial specification.
    for (ppField = begin(); ppField != end(); ++ppField)
        status.push((*ppField)->apply(trial));

    // If there is no 'name' field, we must have an exception to explain it.
    if (status.isOkay() && trial.name.empty())
        status.push(ReaderException("Anonymous trial specification block",
            *this, __PRETTY_FUNCTION__));

    // Done
    return status;
}

/*******************************************************************************
    Test Field
*******************************************************************************/

// Test fields can be either sine tests specified by 'sine' or pstate tests
// specified by pstate

// Create a new test field object.
uct::Field *uct::TrialSpec::TestField::newField(const CtpStatement &statement)
{
    return new TestField(statement);
}

// Create an exact copy of 'this'.
uct::Field *uct::TrialSpec::TestField::clone() const
{
    return new TestField(*this);
}

// Interpret the statement argument.
uct::ExceptionList uct::TrialSpec::TestField::parse()
{
    ReferenceText   reference     = *this;      // Init file/line
    rmt::String referenceText = *this;

    // Get the test references.
    referenceTextList.empty();

    //
    // Parse by commas and insert into the list.
    // Since referenceTextList clones everything added to it, it's okay to use a stack object,
    //
    while (!referenceText.empty())
    {
        reference.text = referenceText.extractShard(',');

        // Fill the wrapper and push wrapper into reference text list
        referenceTextList.push_back(rmt::ConstClonePointer<ReferenceText>(&reference));
    }

    // Can't have the test field with no argument.
    if (referenceTextList.empty())
        return ReaderException("At least one 'pstate' reference is required", *this, __PRETTY_FUNCTION__);

    // No errors
    return Exception();
}

// Apply the field to the list in the trial spec
uct::ExceptionList uct::TrialSpec::TestField::apply(TrialSpec &trial) const
{
    // Copy the p-state references into the trial.
    trial.testReferenceText.push_back_all(referenceTextList);

    // Okay -- Return a NULL exception.
    return Exception();
}

/*******************************************************************************
    RMAPI Field
*******************************************************************************/

// Create a new field
uct::Field *uct::TrialSpec::RmapiField::newField(const CtpStatement &statement)
{
    return new RmapiField(statement);
}

// Create an exact copy of 'this'.
uct::Field *uct::TrialSpec::RmapiField::clone() const
{
    return new RmapiField(*this);
}

// Interpret the statement argument and/or domain.
// Previously, this function was called IterationSettings::parseSettings.
uct::ExceptionList uct::TrialSpec::RmapiField::parse()
{
    ExceptionList status;

    // Interpret the argument data.
    if (!compare("clk") || !compare("clock") || !compare("clocks"))
        mode = Clk;

    else if (!compare("pmu"))
        mode = Pmu;

    else if (!compare("perf") || !compare("pstate"))
        mode = Perf;

    else if (!compare("config"))
        mode = Config;

    else if (!compare("chseq") || !compare("vfinject"))     // vfinject is a deprecated alias
        mode = ChSeq;

    // Exit early on error since there is no point in checking the values.
    else
        status.push(ReaderException(defaultTo("[empty]") + ": 'rmapi' must be one of: clk, pmu, perf, config, chseq",
            *this, __PRETTY_FUNCTION__));

    // Domains do not apply. -- Should be checked by CtpStatement::read
    MASSERT(!domain);

    // Done
    return status;
}

// Apply the field to the trial spec
uct::ExceptionList uct::TrialSpec::RmapiField::apply(TrialSpec &trial) const
{
    ExceptionList status;

    // It's a problem if the settings don't match any previous 'rmapi' statement.
    if (!trial.rmapi.isEmpty() && trial.rmapi != *this)
    {
        status.push(ReaderException(defaultTo("[empty]") + ": Contradictory 'rmapi' statement",
                *this, __PRETTY_FUNCTION__));
        status.push(ReaderException(trial.rmapi.defaultTo("[empty]") + ": Original 'rmapi' statement",
                trial.rmapi, __PRETTY_FUNCTION__));
    }

    // All is okay, so save the info
    else
    {
        trial.rmapi = *this;
    }

    // Done
    return status;
}

// Simple human-readable representation (without file location)
rmt::String uct::TrialSpec::RmapiField::string() const
{
    switch (mode)
    {
        case Clk:       return " rmapi=clk";
        case Pmu:       return " rmapi=pmu";
        case Perf:      return " rmapi=perf";
        case Config:    return " rmapi=config";
        case ChSeq:     return " rmapi=chseq";
    }

    return NULL;
}

/*******************************************************************************
    Tolerance Field
*******************************************************************************/

// Create a new field
uct::Field *uct::TrialSpec::ToleranceField::newField(const CtpStatement &statement)
{
    return new ToleranceField(statement);
}

// Create an exact copy of 'this'.
uct::Field *uct::TrialSpec::ToleranceField::clone() const
{
    return new ToleranceField(*this);
}

//
// Interpret the statement argument and/or domain.
// Previously, this function was called ToleranceSet::parseSet.
// toleracenSet = (programmedToActual, ctpToProgrammed)
//
uct::ExceptionList uct::TrialSpec::ToleranceField::parse()
{
    ExceptionList status;

    // Interpret the argument data.
    // We should have exactly two floating point numbers.
    // Exit early on error since there is no point in checking the values.
    if (sscanf(c_str(), "%f,%f", &setting.measured, &setting.actual) != 2)
        return ReaderException(defaultTo("[empty]") + ": Tolerance pair must be floating point numbers",
            *this, __PRETTY_FUNCTION__);

    // Negative numbers are not lwrrently supported.
    if (setting.measured < 0.0f || setting.actual < 0.0f)
        status.push(ReaderException(defaultTo("[empty]") + ": Tolerances may not be negative",
            *this, __PRETTY_FUNCTION__));

    // Check for a reasonable upper limit.
    if (fabs(setting.measured) > 1.0f || fabs(setting.actual) > 1.0f)
        status.push(ReaderException(defaultTo("[empty]") + ": Tolerances may not exceed 1.0",
            *this, __PRETTY_FUNCTION__));

    // Done
    return status;
}

// Apply the field to the trial spec
uct::ExceptionList uct::TrialSpec::ToleranceField::apply(TrialSpec &trial) const
{
    ExceptionList status;

    // Save the tolerance spec if this is the first one
    if (trial.tolerance.isEmpty())
    {
        trial.tolerance = *this;
    }

    // It's a problem if the settings don't match.
    else if (trial.tolerance != *this)
    {
        status.push(ReaderException(defaultTo("[empty]") + ": Contradictory 'tolerance' statement",
                *this, __PRETTY_FUNCTION__));
        status.push(ReaderException(trial.tolerance.defaultTo("[empty]") + ": Original 'tolerance' statement",
                trial.tolerance, __PRETTY_FUNCTION__));
    }

    return status;
}

// Simple human-readable representation (without file location)
rmt::String uct::TrialSpec::ToleranceField::string() const
{
    std::ostringstream oss;
    oss << " tolerance={measured=" << setting.measured * 100.0    // Actual v. Measured
        <<             "% actual=" << setting.actual   * 100.0    // Target v. Actual
        << "%}";
    return oss;
}

/*******************************************************************************
    Order Field
*******************************************************************************/

// Create a new field
uct::Field *uct::TrialSpec::OrderField::newField(const CtpStatement &statement)
{
    return new OrderField(statement);
}

// Create an exact copy of 'this'.
uct::Field *uct::TrialSpec::OrderField::clone() const
{
    return new OrderField(*this);
}

// Interpret the statement argument and/or domain.
// Previously, this function was called IterationSettings::parseSettings.
uct::ExceptionList uct::TrialSpec::OrderField::parse()
{
    ExceptionList status;

    // Interpret the argument data.
    if (!compare("random"))
        mode = Random;

    else if (!compare("sequence"))
        mode = Sequence;

    // Exit early on error since there is no point in checking the values.
    else
        status.push(ReaderException(defaultTo("[empty]") + ": Order must be 'random' or 'sequence'",
            *this, __PRETTY_FUNCTION__));

    // Domains do not apply. -- Should be checked by CtpStatement::read
    MASSERT(!domain);

    // Done
    return status;
}

// Apply the field to the trial spec
uct::ExceptionList uct::TrialSpec::OrderField::apply(TrialSpec &trial) const
{
    ExceptionList status;

    // It's a problem if the settings don't match any previous 'order' statement.
    if (!trial.sort.order.isEmpty() && trial.sort.order != *this)
    {
        status.push(ReaderException(defaultTo("[empty]") + ": Contradictory 'order' statement",
                *this, __PRETTY_FUNCTION__));
        status.push(ReaderException(trial.sort.order.defaultTo("[empty]") + ": Original 'order' statement",
                trial.sort.order, __PRETTY_FUNCTION__));
    }

    // Value is okay, so save the info
    else
    {
        trial.sort.order = *this;
        status.push(trial.sort.check());
    }

    // Done
    return status;
}

// Simple human-readable representation (without file location)
rmt::String uct::TrialSpec::OrderField::string() const
{
    switch (mode)
    {
        case Sequence:  return " order=sequence";
        case Random:    return " order=random";
    }

    return NULL;
}

/*******************************************************************************
    Seed Field
*******************************************************************************/

// Create a new field
uct::Field *uct::TrialSpec::SeedField::newField(const CtpStatement &statement)
{
    return new SeedField(statement);
}

// Create an exact copy of 'this'.
uct::Field *uct::TrialSpec::SeedField::clone() const
{
    return new SeedField(*this);
}

// Interpret the statement argument.
uct::ExceptionList uct::TrialSpec::SeedField::parse()
{
    ExceptionList status;

    // Interpret the argument data.
    rmt::String text = *this;
    long s = text.extractLong();

    // Make sure the syntax is correct.
    if (errno || !text.empty() || s < 0 || (unsigned long) s > (unsigned long) UINT_MAX)
        status.push(ReaderException(defaultTo("[empty]") + ": 'seed' must be a nonnegative integer not exceeding " QUOTE(UINT_MAX),
            *this, __PRETTY_FUNCTION__));

    // Domains do not apply. -- Should be checked by CtpStatement::read
    MASSERT(!domain);

    // Okay
    if (status.isOkay())
        seed = (unsigned) s;

    // Done
    return status;
}

// Apply the field to the trial spec
uct::ExceptionList uct::TrialSpec::SeedField::apply(TrialSpec &trial) const
{
    ExceptionList status;

    // It's a problem if the settings don't match any previous 'order' statement.
    if (!trial.sort.seed.isEmpty() && trial.sort.seed != *this)
    {
        status.push(ReaderException(defaultTo("[empty]") + ": Contradictory 'seed' statement",
                *this, __PRETTY_FUNCTION__));
        status.push(ReaderException(trial.sort.seed.defaultTo("[empty]") + ": Original 'seed' statement",
                trial.sort.seed, __PRETTY_FUNCTION__));
    }

    // Value is okay, so save the info
    else
    {
        trial.sort.seed = *this;
        status.push(trial.sort.check());
    }

    // Done
    return status;
}

// Simple human-readable representation (without file location)
rmt::String uct::TrialSpec::SeedField::string() const
{
    std::ostringstream oss;
    if (!isEmpty())
        oss << " seed=" << seed;
    return oss;
}

uct::ExceptionList uct::TrialSpec::SortSetting::check() const
{
    ExceptionList status;

    // 'seed' statement doesn't make sense unless 'order' is random.
    if(!seed.isEmpty() && !order.isEmpty() && order.mode != OrderField::Random)
    {
        status.push(ReaderException(seed.defaultTo("[empty]") + ": 'seed' applies only if 'order' is 'random'",
                seed, __PRETTY_FUNCTION__));
        status.push(ReaderException(order.defaultTo("[empty]") + ": 'order' statement",
                order, __PRETTY_FUNCTION__));
    }

    // Done
    return status;
}

/*******************************************************************************
    Range Setting Field
*******************************************************************************/

// Create a new field
uct::Field *uct::TrialSpec::RangeSetting::BeginField::newField(const CtpStatement &statement)
{
    return new BeginField(statement);
}

// Create an exact copy of 'this'.
uct::Field *uct::TrialSpec::RangeSetting::BeginField::clone() const
{
    return new BeginField(*this);
}

// Interpret the statement argument.
uct::ExceptionList uct::TrialSpec::RangeSetting::BeginField::parse()
{
    ExceptionList status;

    // Keyword to start with the first p-state.
    if (!compare("first"))
    {
        begin = 0;
    }

    // Interpret the argument as a numeric.
    else
    {
        char *p;
        begin = strtoul(c_str(), &p, 0);

        // Make sure the syntax is correct.
        if (empty() || *p)
            status.push(ReaderException(defaultTo("[empty]") + ": 'begin' must be 'first' or a nonnegative integer",
                *this, __PRETTY_FUNCTION__));

        // Make sure the range is acceptable
        else if(begin == ULONG_MAX && errno)
            status.push(ReaderException(*this + ": 'begin' must not exceed " QUOTE(ULONG_MAX),
                *this, __PRETTY_FUNCTION__));
    }

    // Domains do not apply. -- Should be checked by CtpStatement::read
    MASSERT(!domain);

    // Okay
    return status;
}

// Apply the field to the trial spec
uct::ExceptionList uct::TrialSpec::RangeSetting::BeginField::apply(TrialSpec &trial) const
{
    ExceptionList status;

    // It's a problem if the settings don't match any previous 'begin' statement.
    if (!trial.range.begin.isEmpty() && trial.range.begin != *this)
    {
        status.push(ReaderException(defaultTo("[empty]") + ": Contradictory 'begin' statement",
                *this, __PRETTY_FUNCTION__));
        status.push(ReaderException(trial.range.begin.defaultTo("[empty]") + ": Original 'begin' statement",
                trial.range.begin, __PRETTY_FUNCTION__));
    }

    // All is okay, so save the info
    else
    {
        trial.range.begin = *this;
        status.push(trial.range.check());
    }

    // Done
    return status;
}

// Simple human-readable representation (without file location)
rmt::String uct::TrialSpec::RangeSetting::BeginField::string() const
{
    std::ostringstream oss;
    if (!isEmpty())
        oss << " begin=" << begin;
    return oss;
}

// Create a new field object.
uct::Field *uct::TrialSpec::RangeSetting::EndField::newField(const CtpStatement &statement)
{
    return new EndField(statement);
}

// Create an exact copy of 'this'.
uct::Field *uct::TrialSpec::RangeSetting::EndField::clone() const
{
    return new EndField(*this);
}

// Interpret the statement argument.
// Previously, IterationSettings::parseSettings contained the logic of this function.
uct::ExceptionList uct::TrialSpec::RangeSetting::EndField::parse()
{
    ExceptionList status;

    // Keyword to end with the last p-state.
    if (!compare("last"))
    {
        end = 0;
    }

    // Interpret the argument as a numeric.
    else
    {
        char *p;
        end = strtoul(c_str(), &p, 0);

        // Make sure the syntax is correct.
        if (*p || !end)
            status.push(ReaderException(defaultTo("[empty]") + ": 'end' must be 'last' or a positive integer",
                *this, __PRETTY_FUNCTION__));

        // Make sure the range is acceptable
        else if(end == ULONG_MAX && errno)
            status.push(ReaderException(*this + ": 'end' must not exceed " QUOTE(ULONG_MAX),
                *this, __PRETTY_FUNCTION__));
    }

    // Domains do not apply. -- Should be checked by CtpStatement::read
    MASSERT(!domain);

    // Okay
    return status;
}

// Apply the field to the trial spec
uct::ExceptionList uct::TrialSpec::RangeSetting::EndField::apply(TrialSpec &trial) const
{
    ExceptionList status;

    // It's a problem if the settings don't match any previous 'end' statement.
    if (!trial.range.end.isEmpty() && trial.range.end != *this)
    {
        status.push(ReaderException(defaultTo("[empty]") + ": Contradictory 'end' statement",
                *this, __PRETTY_FUNCTION__));
        status.push(ReaderException(trial.range.end.defaultTo("[empty]") + ": Original 'end' statement",
                trial.range.end, __PRETTY_FUNCTION__));
    }

    // All is okay, so save the info
    else
    {
        trial.range.end = *this;
        status.push(trial.range.check());
    }

    // Done
    return status;
}

// Simple human-readable representation (without file location)
rmt::String uct::TrialSpec::RangeSetting::EndField::string() const
{
    std::ostringstream oss;
    if (!isEmpty())
        oss << " end=" << end;
    return oss;
}

// Check that the field values are compatible
uct::ExceptionList uct::TrialSpec::RangeSetting::check() const
{
    ExceptionList status;

    // 'begin' must come before 'end' unless end is 'last' (zero).
    if(!begin.isEmpty() && !end.isEmpty() && begin.begin >= end.end)
    {
        status.push(ReaderException(end.defaultTo("[empty]") + ": Cannot 'end' before 'begin'",
                end, __PRETTY_FUNCTION__));
        status.push(ReaderException(begin.defaultTo("[empty]") + ": 'begin' statement",
                begin, __PRETTY_FUNCTION__));
    }

    // Done
    return status;
}

/*******************************************************************************
    Feature Field
*******************************************************************************/

// Translate keyword argument to index bitmap
const uct::TrialSpec::FeatureField::KeywordMap uct::TrialSpec::FeatureField::keywordMap = rmt::StringMap<uct::TrialSpec::FeatureField::Bitmap>::initialize
    ("thermalslowdown", BIT(ThermalSlowdown))   // LW2080_CTRL_THERMAL_SYSTEM_SET_SLOWDOWN_STATE_OPCODE
    ("deepidle",        BIT(DeepIdle))          // LW2080_CTRL_CMD_GPU_SET_DEEP_IDLE_MODE
    ("brokenfb",        BIT(BrokenFB))          // LW2080_CTRL_FB_INFO_INDEX_FB_IS_BROKEN (read-only)
    ("fbbroken",        BIT(BrokenFB))          // Synonym
    ("training2t",      BIT(Training2T))        // LW2080_CTRL_FB_INFO_INDEX_TRAINIG_2T (read-only)
    ("2ttraining",      BIT(Training2T));       // Synonym

// Create a new 'enable' field object.
uct::Field *uct::TrialSpec::FeatureField::newEnableField(const CtpStatement &statement)
{
    return new FeatureField(true, statement);
}

// Create a new 'disable' field object.
uct::Field *uct::TrialSpec::FeatureField::newDisableField(const CtpStatement &statement)
{
    return new FeatureField(false, statement);
}

// Create an exact copy of 'this'.
uct::Field *uct::TrialSpec::FeatureField::clone() const
{
    return new FeatureField(*this);
}

// Interpret the statement argument.
// Previously, FeatureSettings::parse contained the logic of this function.
uct::ExceptionList uct::TrialSpec::FeatureField::parse()
{
    ExceptionList status;
    rmt::String argument = *this;
    rmt::String keyword;
    Bitmap bit;

    // Initially assume that there are no features
    feature = 0;

    // Evaluate each feature keyword in the argument list
    while (!argument.empty())
    {
        // Lookup the next comma-delimiter keyword.
        keyword = argument.extractShard();
        bit = keywordMap.find_else(keyword, 0);

        // No such keyword exists.
        if (!bit)
            status.push(ReaderException(keyword + ": Invalid 'feature' keyword", *this, __PRETTY_FUNCTION__));

        // Flag the specified feature.
        else
            feature |= bit;
    }

    // Can't have the 'feature' field with no argument.
    if (status.isOkay() && !feature)
        status.push(ReaderException("At least one 'feature' keyword is required", *this, __PRETTY_FUNCTION__));

    // Done
    return status;
}

// Apply the field to the vector in the trial spec
uct::ExceptionList uct::TrialSpec::FeatureField::apply(TrialSpec &trial) const
{
    ExceptionList status;
    Vector::const_iterator pFeatureField;

    // Look at each previously parsed 'enable' or 'disable' statement
    for (pFeatureField = trial.featureVector.begin();
         pFeatureField!= trial.featureVector.end(); ++pFeatureField)
    {
        //
        // There is an intersection where the enablement v. disablement disagree.
        // There is an interxection iff (feature & pFeatureField->feature) is nonzero.
        //
        if ((feature & pFeatureField->feature & enable) !=
            (feature & pFeatureField->feature & pFeatureField->enable))
        {
            status.push(ReaderException(*this + ": Contradictory 'enable' or 'disable' statement",
                    *this, __PRETTY_FUNCTION__));
            status.push(ReaderException(*pFeatureField + ": Previous 'enable' or 'disable' statement",
                    *pFeatureField , __PRETTY_FUNCTION__));
        }
    }

    // Add it to the list unless there is an error.
    if (status.isOkay())
        trial.featureVector.push_back(*this);

    // Done
    return status;
}

// Simple human-readable representation (without file location)
rmt::String uct::TrialSpec::FeatureField::string() const
{
    KeywordMap::const_iterator pKeyword;
    bool bKeyword = false;
    std::ostringstream oss;

    // Classify the field as enable, disable, or a mix of the two.
    switch (enable)
    {
        case (Bitmap) -1:   oss << " enable=";  break;
        case (Bitmap) 0:    oss << " disable="; break;
        default:            oss << " enable/disable=";
    }

    // Indicate which features are enabled/disabled.
    for (pKeyword = keywordMap.begin();
        pKeyword != keywordMap.end(); ++pKeyword)
    {

        // This feature is specified by this field object.
        if (pKeyword->second & feature)
        {
            // If we're printed one already, separate by pipe (logical OR)
            if (bKeyword)
                oss << '|';

            // If we're disabling this feature but enabling others, use '~' to distinguish.
            if (enable && !(pKeyword->second & enable))
                oss << '~';

            // Add the keyword and indicate that we have at least one
            oss << pKeyword->first;
            bKeyword = true;
        }
    }

    // No keywords are included (shouldn't happen)
    if (!bKeyword)
        oss <<  "0x" << std::hex << std::setw(8) << std::setfill('0') << enable
            << "/0x" << std::hex << std::setw(8) << std::setfill('0') << feature;

    return oss;
}

/*******************************************************************************
    Ram Type Field
*******************************************************************************/

// Translate argument
const uct::TrialSpec::RamTypeField::KeywordMap uct::TrialSpec::RamTypeField::keywordMap = rmt::StringMap<uct::TrialSpec::RamTypeField::Bitmap>::initialize
    ("sdram",           BIT(LW2080_CTRL_FB_INFO_RAM_TYPE_SDRAM))    // SDRAM  (0x00000001)
    ("ddr1",            BIT(LW2080_CTRL_FB_INFO_RAM_TYPE_DDR1))     // DDR1   (0x00000002) SDDR1 or GDDR1  (The API can't tell the difference.)
    ("sddr2",           BIT(LW2080_CTRL_FB_INFO_RAM_TYPE_SDDR2))    // SDDR2  (0x00000003) Usage started with LW43
    ("gddr2",           BIT(LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR2))    // GDDR2  (0x00000004) Used on LW30 and some LW36
    ("gddr3",           BIT(LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR3))    // GDDR3  (0x00000005) Used on LW40 and later
    ("gddr4",           BIT(LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR4))    // GDDR4  (0x00000006) Used on G80 and later
    ("sddr3",           BIT(LW2080_CTRL_FB_INFO_RAM_TYPE_SDDR3))    // SDDR3  (0x00000007) Used on G9x and later
    ("gddr5",           BIT(LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR5))    // GDDR5  (0x00000008) Used on GT21x and later
    ("lpddr2",          BIT(LW2080_CTRL_FB_INFO_RAM_TYPE_LPDDR2))   // LPDDR2 (0x00000009) Low Power SDDR2 used on T2x and later
    ("sddr4",           BIT(LW2080_CTRL_FB_INFO_RAM_TYPE_SDDR4))    // SDDR4  (0x0000000C) Usage started with Maxwell
    ("gddr5x",          BIT(LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR5X))   // GDDR5X (0x00000010) Used on GP10x and later
    ("gddr6",           BIT(LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR6))    // GDDR6  (0x00000011) Used on TU10x and later
    ("gddr6x",          BIT(LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR6X));  // GDDR6X (0x00000012) Used on GA10x and later

// Create a new field object.
uct::Field *uct::TrialSpec::RamTypeField::newField(const CtpStatement &statement)
{
    return new RamTypeField(statement);
}

// Create an exact copy of 'this'.
uct::Field *uct::TrialSpec::RamTypeField::clone() const
{
    return new RamTypeField(*this);
}

// Interpret the statement argument.
uct::ExceptionList uct::TrialSpec::RamTypeField::parse()
{
    ExceptionList status;
    rmt::String argument = *this;
    rmt::String keyword;
    Bitmap bit;

    // Initially assume that no RAM types are supported
    bitmap = 0x00000000;

    // Evaluate each keyword in the argument list
    while (!argument.empty())
    {
        // Lookup the next comma-delimiter keyword.
        keyword = argument.extractShard();
        bit = keywordMap.find_else(keyword, 0);

        // No such keyword exists.
        if (!bit)
            status.push(ReaderException(keyword + ": Invalid 'ramtype' keyword", *this, __PRETTY_FUNCTION__));

        // Flag the specified Ram type.
        else
            bitmap |= bit;
    }

    // Can't have the 'ramtype' field with no argument.
    if (status.isOkay() && !bitmap)
        status.push(ReaderException("At least one 'ramtype' keyword is required", *this, __PRETTY_FUNCTION__));

    // Done
    return status;
}

// Apply the field to the vector in the trial spec
uct::ExceptionList uct::TrialSpec::RamTypeField::apply(TrialSpec &trial) const
{
    ExceptionList status;

    // It's a problem if the settings don't match any previous 'ramtype' statement.
    if (!trial.ramtype.isEmpty() && trial.ramtype != *this)
    {
        status.push(ReaderException(defaultTo("[empty]") + ": Contradictory 'ramtype' statement",
                *this, __PRETTY_FUNCTION__));
        status.push(ReaderException(trial.ramtype.defaultTo("[empty]") + ": Original 'ramtype' statement",
                trial.ramtype, __PRETTY_FUNCTION__));
    }

    // All is okay, so save the info
    else
    {
        trial.ramtype = *this;
    }

    // Done
    return status;
}

// Simple human-readable representation (without file location)
rmt::String uct::TrialSpec::RamTypeField::string() const
{
    // Nothing to say
    if (isEmpty())
        return NULL;

    // Indicate the field type and value.
    return " ramtype=" + typeString();
}

// Human-readable representation of specified RAM type bitmap
rmt::String uct::TrialSpec::RamTypeField::typeString(Bitmap bitmap)
{
    KeywordMap::const_iterator pKeyword;
    bool bNotFirst = false;
    std::ostringstream oss;

    // Nothing to say
    if (!bitmap)
        return "none";

    // Indicate which RAM types are supported.
    for (pKeyword = keywordMap.begin();
        pKeyword != keywordMap.end(); ++pKeyword)
    {
        // This RAM type is one of the supported types
        if (pKeyword->second & bitmap)
        {
            // If we've printed one already, separate by pipe (logical OR)
            if (bNotFirst)
                oss << '|';

            // Add the RAM-type keyword
            oss << pKeyword->first;

            // Flag that we need | for a delimiter
            bNotFirst = true;

            // Indicate which bits have been handled
            bitmap &= ~pKeyword->second;
        }
    }

    // There are bits for which no keyword is defined.
    if (bitmap && bNotFirst)
        oss << '|';
    if (bitmap)
        oss << "0x" << std::hex << std::setw(8) << std::setfill('0') << bitmap;

    // Done
    return oss;
}

/*******************************************************************************
    Prune Field
*******************************************************************************/

// Create a new field object.
uct::Field *uct::TrialSpec::PruneField::newField(const CtpStatement &statement)
{
    return new PruneField(statement);
}

// Create an exact copy of 'this'.
uct::Field *uct::TrialSpec::PruneField::clone() const
{
    return new PruneField(*this);
}

// Interpret the statement argument.
uct::ExceptionList uct::TrialSpec::PruneField::parse()
{
    ExceptionList status;
    rmt::String argument = *this;
    rmt::String domain;
    DomainBitmap bit;

    // Initially assume that no domains are pruned.
    mask = noPrune;                 // 0xffffffff

    // Apply sentinel value for the 'auto' keyword.
    if (argument == "auto")
        mask = autoPrune;

    // If it's not 'none', parse the list of frequency domains
    else if (argument != "none")
    {

        // Evaluate each keyword in the argument list
        while (!argument.empty())
        {
            // Lookup the next comma-delimiter keyword.
            domain = argument.extractShard();
            bit = DomainNameMap::freq.bit(domain);

            // No such keyword exists.
            if (!bit)
                status.push(ReaderException(domain + ": Invalid 'prune' domain", *this, __PRETTY_FUNCTION__));

            // Flag the specified Ram type.
            else
                mask &= ~bit;
        }

        // Can't have the 'prune' field with no argument.
        if (status.isOkay() && !mask)
            status.push(ReaderException("'prune' requires 'auto', 'none', or at least one domain", *this, __PRETTY_FUNCTION__));
    }

    // Done
    return status;
}

// Apply the field to the vector in the trial spec
uct::ExceptionList uct::TrialSpec::PruneField::apply(TrialSpec &trial) const
{
    PruneField copy = *this;
    ExceptionList status;

    // Interpret the 'auto' keyword sentinel.
    if (copy.mask == autoPrune)
        copy.mask = trial.context.supportedFreqDomainSet;

    // It's a problem if the settings don't match any previous 'prune' statement.
    if (!trial.prune.isEmpty() && trial.prune != copy)
    {
        status.push(ReaderException(defaultTo("[empty]") + ": Contradictory 'prune' statement",
                copy, __PRETTY_FUNCTION__));
        status.push(ReaderException(trial.prune.defaultTo("[empty]") + ": Original 'prune' statement",
                trial.prune, __PRETTY_FUNCTION__));
    }

    // All is okay, so save the info.
    else
    {
        trial.prune = copy;
    }

    // Done
    return status;
}

// Simple human-readable representation (without file location)
rmt::String uct::TrialSpec::PruneField::string(FreqDomainBitmap programmableMask) const
{
    // Nothing to say
    if (isEmpty())
        return NULL;

    // Indicate the field type and value.
    return " prune=" + domainString(mask, programmableMask);
}

// Human-readable representation of specified prune bitmap
// This means printing the zero bits from the mask.
rmt::String uct::TrialSpec::PruneField::domainString(FreqDomainBitmap pruneMask, FreqDomainBitmap programmableMask)
{
    // Nothing more to say if nothing is pruned
    if (pruneMask == noPrune)
        return "none";

    //
    // We're not pruning anything that we care about.
    // 'autoPrune' is the sentinel before the 'apply' function is called.
    // 'programmableMask' is the value applied by the 'apply' function in the trial specification.
    //
    if (pruneMask == autoPrune || pruneMask == programmableMask)
        return "auto";

    // Domains to be pruned were specified, but do not apply to this chip.
    if (!(~pruneMask & programmableMask))
        return "n/a";

    // Zeroes indicate pruned domains; Print the ones we care about.
    return DomainNameMap::freq.textList(~pruneMask & programmableMask).concatenate("|");
}

/*******************************************************************************
    Iteration
*******************************************************************************/

uct::TrialSpec::Iterator::Iterator(const TrialSpec *pTrial)
{
    MASSERT(pTrial);

    // Remember the trial specification
    this->pTrial = pTrial;

    // Number of iterations
    remaining    = pTrial->range.count();

    // Setup the initial index and 'next' function.
    switch(pTrial->sort.order.mode)
    {
    // Begin at the beginning and go in order.
    case uct::TrialSpec::OrderField::Sequence:
        next  = &Iterator::nextSequence;
        index = pTrial->range.begin.begin;
        break;

    // Begin anywhere
    case uct::TrialSpec::OrderField::Random:
        srand(pTrial->sort.seed.seed);
        next  = &Iterator::nextRandom;
        nextRandom();
        break;
    }
}

// Increment
void uct::TrialSpec::Iterator::nextSequence()
{
    ++index;
}

// Increment
void uct::TrialSpec::Iterator::nextRandom()
{
    // change (RAND_MAX +1) to RAND_MAX as it causes integer overflow warnings
    index = (Iteration) ((((double) rand()) / RAND_MAX) * pTrial->cardinality());
}

/*******************************************************************************
    Resolution
*******************************************************************************/

// List Destruction
// Nothing to do. The pointers in nameMap don't dangle since nameMap is a member of this.
uct::TrialSpec::List::~List()
{   }

uct::ExceptionList uct::TrialSpec::List::resolve(
    const VbiosPStateArray         &vbiosPStateMap,
    const TestSpec::NameMap        &testSpecMap)
{
    iterator        pTrial;
    ExceptionList   status;

    // Resolve each trial spec in the list and accumulate any exceptions.
    for (pTrial = begin(); pTrial != end(); ++pTrial)
        status.push(pTrial->resolve(vbiosPStateMap, testSpecMap));

    // Done
    return status;
}

//
// Move the test references from 'testReferenceText'
// to 'testReferenceVector', resolving them in the process.
//
uct::ExceptionList uct::TrialSpec::resolve(
    const VbiosPStateArray         &vbiosPStateMap,
    const TestSpec::NameMap        &testSpecMap)
{
    TestField::ReferenceText::Vector::const_iterator pText;
    RangeSetting::Iteration card;
    ExceptionList refStatus;
    ExceptionList status;

    // It's a problem if there are no test references
    if (testReferenceText.empty())
        return SolverException(name.defaultTo("[anonymous]") +
            ": Missing 'test' field(s) from trial specification",
            name, __PRETTY_FUNCTION__);

    for (pText = testReferenceText.begin();
         pText != testReferenceText.end(); ++pText)
    {
        shared_ptr<TestReference> ref = make_shared<TestReference>();

        refStatus = ref->resolve(**pText, (*pText)->text, vbiosPStateMap, testSpecMap);

        if (refStatus.isOkay())
            testReferenceVector.push_back(ref);
        else if (refStatus.isUnsupported())
        {
            refStatus.clear();
            continue;
        }
        else
            status.push(refStatus);
    }

    // Return if there is an error
    if (status.isSerious())
        return status;

    // Discard all the unresolved text to save memory.
    testReferenceText.clear();

    // Get the size of the result set.
    card = testReferenceVector.cardinality();

    // Make sure there is something to do.
    if (!card)
        status.push(SolverException(name.defaultTo("[anonymous]") +
            ": Test reference(s) resolve to an empty result set.",
            name, __PRETTY_FUNCTION__));

    // Validate the 'begin' parameter against the result set size.
    else if (!status.isSerious() && range.begin.begin > card)
        status.push(SolverException(name.defaultTo("[anonymous]") +
            ": 'begin' after the last iteration in the result set.",
            range.begin, __PRETTY_FUNCTION__));

    // Make sure the last iteration is not beyond the last p-state in the result set.
    // This is an error only if there are no sine tests (since Sine Test can
    // produce unlimited iterations)
    else if (!status.isSerious() && range.end.end >= card)
        range.end.end = card - 1;

    // If there is no error, the range should be within the result set.
    MASSERT(status.isSerious() || range.count() <= card);

    // Done
    return status;
}

/*******************************************************************************
    Debug
*******************************************************************************/

// Human-readable representation of this trial spec
rmt::String uct::TrialSpec::debugString(const char *info) const
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

    // Offer some detail
    oss << tolerance.string()
        << sort.order.string()
        << sort.seed.string()
        << range.begin.string()
        << range.end.string()
        << featureVector.string()
        << ramtype.string()
        << prune.string(context.supportedFreqDomainSet);

    // Additional info if 'resolve' has been called
    if (!testReferenceVector.empty())
        oss << " cardinality=" << testReferenceVector.cardinality();

    // Additional info if 'resolve' has not been called
    if(!testReferenceText.empty())
        oss << " unresolved tests = " << testReferenceText.string();

    // Done
    return oss.str();
}

