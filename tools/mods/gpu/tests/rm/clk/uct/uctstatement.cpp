/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2017, 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file
//!
//! \brief      Statements and statement blocks from the Clock Test Profile
//!

#include <stdio.h>
#include <stdlib.h>
#include <iomanip>
#include <deque>
#include <algorithm>

#include "lwtypes.h"
#include "ctrl/ctrl2080/ctrl2080clk.h"
#include "ctrl/ctrl2080/ctrl2080perf.h"

#include "gpu/tests/rm/utility/rmtstring.h"
#include "uctfilereader.h"
#include "uctstatement.h"
#include "uctexception.h"
#include "uctfield.h"
#include "uctpartialpstate.h"
#include "uctsinespec.h"
#include "ucttrialspec.h"
#include "uctvflwrve.h"

// WARNING: The order here must match uct::CtpStatement::Type::Index.
const uct::CtpStatement::Type::Info uct::CtpStatement::Type::all[TypeCount] =
{

    {   "<unknown>",    CtpStatement::Type::None,                       // Unknown statement type
                        NULL,                                           // Fields do not apply
                        NULL},                                          // Domains do not apply

    {   "<comment>",    CtpStatement::Type::None,                       // Comment (or blank line)
                        NULL,                                           // Fields do not apply
                        NULL},                                          // Domains do not apply

// 'DirectiveBlock' does not apply to <eof> because we don't want to parse/apply a new statement block.
    {   "<eof>",        CtpStatement::Type::EofDirective        |       // End of file pseudo-directive
                        CtpStatement::Type::BlockStart,
                        NULL,                                           // Fields do not apply
                        NULL},                                          // Domains do not apply

    {   "include",      CtpStatement::Type::DirectiveBlock      |       // 'include' directive
                        CtpStatement::Type::BlockStart,                 // Singleton block
                        uct::CtpFileReader::IncludeField::newField,     // Create field-specific object
                        NULL},                                          // Domains do not apply

    {   "dryrun",       CtpStatement::Type::DirectiveBlock      |       // 'dryrun' directive
                        CtpStatement::Type::BlockStart,                 // Singleton block
                        uct::CtpFileReader::DryRunField::newField,      // Create field-specific object
                        NULL},                                          // Domains do not apply

    {   "name",         CtpStatement::Type::PartialPStateBlock  |       // 'name' field
                        CtpStatement::Type::TrialSpecBlock      |       // Both nondirective block types
                        CtpStatement::Type::SineTestBlock       |
                        CtpStatement::Type::VfLwrveBlock        |
                        CtpStatement::Type::BlockStart,                 // Starts the statement block
                        uct::NameField::newField,                       // Create field-specific object
                        NULL},                                          // Domains do not apply

    {   "freq",         CtpStatement::Type::PartialPStateBlock,         // Frequencies for the pseudo-p-state
                        PartialPState::FreqField::newField,
                       &DomainNameMap::freq},                           // Clock domain is required syntax

    {   "source",       CtpStatement::Type::PartialPStateBlock  |       // Source for the freq domain in a pseudo-p-state
                        CtpStatement::Type::SineTestBlock,              // Source for the freq domain in a sine test
                        uct::TestSpec::SourceField::newField,
                       &DomainNameMap::freq},                           // Clock domain is required syntax

    {   "volt",         CtpStatement::Type::PartialPStateBlock,         // Voltages for the pseudo-p-state
                        PartialPState::VoltField::newField,
                       &DomainNameMap::volt},                           // Voltage domain is required syntax

    {   "test",         CtpStatement::Type::TrialSpecBlock,             // Test reference in the trial spec
                        uct::TrialSpec::TestField::newField,            // Create field-specific object
                        NULL},                                          // Domains do not apply

    {   "rmapi",        CtpStatement::Type::TrialSpecBlock,             // RMAPI points used in the trials
                        uct::TrialSpec::RmapiField::newField,           // Replaces former 'mode' field
                        NULL},                                          // Domains do not apply

    {   "tolerance",    CtpStatement::Type::TrialSpecBlock,             // Tolerance for the trial spec
                        uct::TrialSpec::ToleranceField::newField,
                        NULL},                                          // Domains do not apply

    {   "order",        CtpStatement::Type::TrialSpecBlock,             // P-State order (e.g. random v. sequential)
                        uct::TrialSpec::OrderField::newField,           // Replaces former 'iterations' field
                        NULL},                                          // Domains do not apply

    {   "seed",         CtpStatement::Type::TrialSpecBlock,             // P-State order (e.g. random v. sequential)
                        uct::TrialSpec::SeedField::newField,            // Replaces former 'iterations' field
                        NULL},                                          // Domains do not apply

    {   "begin",        CtpStatement::Type::TrialSpecBlock,             // Number of iterations to skip
                        uct::TrialSpec::RangeSetting::BeginField::newField,
                        NULL},                                          // Domains do not apply

    {   "end",          CtpStatement::Type::TrialSpecBlock,             // Iteration count in the trials
                        uct::TrialSpec::RangeSetting::EndField::newField,
                        NULL},                                          // Replaces former 'iterations' field

    {   "enable",       CtpStatement::Type::TrialSpecBlock,             // Features that must be enabled for the trial
                        uct::TrialSpec::FeatureField::newEnableField,   // 'enable' is an instance of a feature field.
                        NULL},                                          // Domains do not apply

    {   "disable",      CtpStatement::Type::TrialSpecBlock,             // Features that must be disabled for the trial
                        uct::TrialSpec::FeatureField::newDisableField,  // 'disable' is an instance of a feature field.
                        NULL},                                          // Domains do not apply

    {   "ramtype",      CtpStatement::Type::TrialSpecBlock,             // Required memory types for the trial
                        uct::TrialSpec::RamTypeField::newField,         // Create field-specific object
                        NULL},                                          // Domains do not apply

    {   "prune",        CtpStatement::Type::TrialSpecBlock,             // Pruned domains for the trial
                        uct::TrialSpec::PruneField::newField,           // Create field-specific object
                        NULL},                                          // Domains do not apply

    {   "flags",        CtpStatement::Type::SineTestBlock,              // Flag parameter, required and also
                        uct::SineSpec::FlagField::newField,             // specifies what domain the test block is for
                       &DomainNameMap::freq},

    {   "alpha",        CtpStatement::Type::SineTestBlock,              // Parameter alpha
                        uct::SineSpec::ParamField::newField,
                       &DomainNameMap::freq},

    {   "beta",         CtpStatement::Type::SineTestBlock,              // Parameter beta
                        uct::SineSpec::ParamField::newField,
                       &DomainNameMap::freq},

    {   "gamma",        CtpStatement::Type::SineTestBlock,              // Parameter gamme
                        uct::SineSpec::ParamField::newField,
                       &DomainNameMap::freq},

    {   "omega",        CtpStatement::Type::SineTestBlock,              // Parameter omega
                        uct::SineSpec::ParamField::newField,
                       &DomainNameMap::freq},

    {   "iterations",   CtpStatement::Type::SineTestBlock,              // Iterations settings determines how many
                        uct::SineSpec::IterationsField::newField,       // runs the Sine Spec has i = (0, val]
                        NULL},

    {   "vflwrve",      CtpStatement::Type::VfLwrveBlock,
                        VfLwrve::VfLwrveField::newField,
                        &DomainNameMap::freq},

};

// Destruct -- Nothing to do
uct::CtpStatement::~CtpStatement()  {   }

// Read a statement.
uct::ReaderException uct::CtpStatement::read(rmt::TextFileInputStream &is)
{
    rmt::String typekey;
    rmt::String domainText;
    size_t pos;
    bool bEqualSign;

    // Discard previous state.
    clear();

    // Read a line from the file and parse
    is.getline(*this);

    // Unable to read
    if (is.fail() && !is.eof())
    {
        return ReaderException("Unable to read CTP file", *this, __PRETTY_FUNCTION__, ReaderException::Fatal);
    }

    // Discard leading and training whitespace
    trim();

    // Print the line.  (End-of-file markers may be redundant.)
    Printf(Tee::PriHigh, "%s%s\n", locationString().c_str(),
        this->defaultTo(is.eof()? "<eof>" : "<empty>").c_str());

    // Colwert to lowercase.  Nothing is case sensitive.
    lcase();

    // End-of-file is reported as a pseudo-statement at the end of the file.
    if (empty() && is.eof())
    {
        type = Type::Eof;
        return ReaderException();
    }

    // We have a comment or blank line... nothing more to do.
    if (empty() || at(0) == '#')
    {
        type = Type::Comment;
        return ReaderException();
    }

    // Split the line into key and val parts.  Return error if no = exists.
    pos = find_first_of('=');

    // If it has no equal sign, then use a space instead.
    bEqualSign = pos != npos;
    if (!bEqualSign)
        pos = find_first_of(whitespace);

    // Line contains only a single token.
    if (pos == npos)
        return ReaderException("Missing argument(s)", *this, __PRETTY_FUNCTION__);

    // Extract the key from the value using the equal sign as a delimiter.
    typekey = substr(0, pos);
    typekey.rtrim();

    // Discard it from the value along with any superfluous spaces
    erase(0, pos+1);
    ltrim();

    // Remove double quotes if they exist on the ends.
    if (length() > 1 && this->at(0) == '"' && this->at(length() - 1) == '"')
    {
        erase(length() - 1);
        erase((size_t) 0, 1);
    }

    // Without quotes, we remove all whitespace.
    else
    {
        removeSpace();
    }

    //
    // Extract the domain (if any)/param (if Sine Test) using a period as a delimiter.
    //
    // TODO: Support multiple domains by parsing here by all periods.
    //
    pos = typekey.find_last_of('.');
    if (pos != npos)
    {
        domainText = typekey.substr(0, pos);
        domainText.rtrim();
        typekey.erase(0,pos+1);
        typekey.ltrim();
    }

    // Translate the field type
    type = Type::find(typekey);
    if (!type)
        return ReaderException(typekey.defaultTo("[empty]") + ": Invalid field type", *this, __PRETTY_FUNCTION__);

    //
    // This statement type requires [<domain> "."] in its syntax (e.g. 'freq'
    // or 'volt', or the params in Sine Test).  Lookup the domain in the map
    // specific to this field type.
    //
    if (type->domainNameMap)
    {
        // The domain is not optional for the field type.
        if (domainText.empty())
            return ReaderException(typekey + ": Missing required domain", *this, __PRETTY_FUNCTION__);

        // Translate the domain text into the bitmap.
        domain = type->domainNameMap->bit(domainText);
        if (!domain)
            return ReaderException(domainText + ": Invalid '" + typekey + "' domain", *this, __PRETTY_FUNCTION__);
    }

    // Zero if domain does not apply.
    else
    {
        domain = 0x00000000;

        // The domain is not permitted for the field tyoe
        if (!domainText.empty())
            return ReaderException(domainText + ": Superfluous domain for '" + typekey + "' statement", *this, __PRETTY_FUNCTION__);
    }

    // We require an equal sign unless it is a directive (or comment).
    if (!bEqualSign && !isDirective())
        return ReaderException("Missing equal sign", *this, __PRETTY_FUNCTION__);

    // All went swimmingly!
    return ReaderException();
}

// Human-readable string representation of this statement
rmt::String uct::CtpStatement::debugString(const char *info) const
{
    std::ostringstream oss;

    // File name and line number (location) if applicable
    oss << locationString();

    // Additional info if any
    if (info && *info)
        oss << info << ": ";

    // Empty is special
    if (isEmpty())
        oss << "empty";

    else
    {
        if (type)
        {
            oss << "keyword=" << type->keyword;

            // Class bit flags categorize the statement types,
            oss << "  type=";
            UINT08 att = type->attribute;

            // This statement has not class flags.
            if (!att)
                oss << "none";

            // New block starts with this statement.
            if (att & Type::BlockStart)
            {
                oss << "start";
                att &= ~Type::BlockStart;
                if (att)
                    oss << '|';
            }

            // No other statements in this block -- implies _BlockStart
            if (att & Type::DirectiveBlock)
            {
                oss << "directive";
                att &= ~Type::DirectiveBlock;
                if (att)
                    oss << '|';
            }

            // Appears only in partial p-state spec
            if (att & Type::PartialPStateBlock)
            {
                oss << "pstate";
                att &= ~Type::PartialPStateBlock;
                if (att)
                    oss << '|';
            }

            // Appears only in sine test block
            if (att & Type::SineTestBlock)
            {
                oss << "sine";
                att &= ~Type::SineTestBlock;
                if (att)
                    oss << '|';
            }

            // Appears only in vflwrve test spec
            if (att & Type::VfLwrveBlock)
            {
                oss << "vflwrve";
                att &= ~Type::VfLwrveBlock;
                if (att)
                    oss << '|';
            }

            // Appears only in trial spec
            if (att & Type::TrialSpecBlock)
            {
                oss << "trial";
                att &= ~Type::TrialSpecBlock;
                if (att)
                    oss << '|';
            }

            // Catch all -- 'att' should be zero.
            if (att)
                oss << rmt::String::hex(att, 8).c_str();

            if (+domain)
                oss << "  domain=" << rmt::String::hex(domain,  8);
        }

        // The statement data itself
        oss << "  arg=" << defaultTo("[none]").c_str();
    }

    // Done
    return oss.str();
}

