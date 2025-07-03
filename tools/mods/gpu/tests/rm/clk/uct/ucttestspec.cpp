/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/utility/rmclkutil.h"

#include "ucttestspec.h"
#include "uctoperator.h"

// Colwersion between supported flag bits and names
const uct::FlagOperator::NameMap uct::TestSpec::flagNameMap
 = uct::FlagOperator::NameMap::initialize
    ("force_pll",       DRF_DEF(2080, _CTRL_CLK_INFO_FLAGS, _FORCE_PLL,    _ENABLE))
    ("force_bypass",    DRF_DEF(2080, _CTRL_CLK_INFO_FLAGS, _FORCE_BYPASS, _ENABLE))
    ("force_nafll",     DRF_DEF(2080, _CTRL_CLK_INFO_FLAGS, _FORCE_NAFLL,  _ENABLE));

// Colwersion between exponents and kilohertz
const uct::NumericOperator::UnitMap uct::TestSpec::unitMapKHz
 = uct::NumericOperator::UnitMap::initialize
    ("khz",  0)         // Kilohertz is the standard used in the RMAPI.
    ("mhz",  3)         // Multiply by 1000 to colwert megahertz to kilohertz.
    ("ghz",  6)         // Multiply by 10^6 to colwert gigahertz to kilohertz.
    ("hz",  -3);        // Divide by 1000 to colwert hertz to kilohertz.

// Colwertion between exponents and microvolts
const uct::NumericOperator::UnitMap uct::TestSpec::unitMapuV
 = uct::NumericOperator::UnitMap::initialize
    ("uv", 0)       // Microvolts is the standard used in the RMAPI.
    ("mv", 3)       // Multiply by 1000 to colwert millivolts to microvolts.
    ("v",  6);      // Multiply by 10^6 to colwert volts to microvolts.

/*******************************************************************************
    SourceField
*******************************************************************************/

// Create a new field
uct::Field *uct::TestSpec::SourceField::newField(const CtpStatement &statement)
{
    return new SourceField(statement);
}

// Create an exact copy of 'this'.
uct::Field *uct::TestSpec::SourceField::clone() const
{
    return new SourceField(*this);
}

// Interpret the statement argument.
uct::ExceptionList uct::TestSpec::SourceField::parse()
{
    // This field cannot be empty
    if (empty())
        return ReaderException("Source(s) required in a 'source' field.",
            *this, __PRETTY_FUNCTION__);

    // Okay
    return Exception();
}

