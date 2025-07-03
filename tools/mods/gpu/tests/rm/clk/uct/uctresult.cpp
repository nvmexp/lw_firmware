/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file
//!
//! Implements various exceptions classes
//!

#include <math.h>           // Needed for INFINITY, etc.
#include <sstream>

#include "gpu/tests/rm/utility/rmtstring.h"
#include "gpu/utility/rmclkutil.h"
#include "uctresult.h"

// Define INFINITY as something usable in case the library doesn't.
#ifndef INFINITY
#ifdef HUGE_VAL
#define INFINITY HUGE_VAL
#else
#define INFINITY 1.0e999999999
#endif
#endif

/*******************************************************************************
    Result
*******************************************************************************/

// Frequency or voltage as text
rmt::String uct::Result::valueString(UINT32 value)
{
    return value ? rmt::String::dec(value) :
            rmt::String("------");                  // Hyphens for n/a (zero)
}

// Delta value as formatted text
rmt::String uct::Result::deltaString(UINT32 a, UINT32 b)
{
    // No data available
    if (!a || !b)
        return "-----";

    // Compute the delta
    double delta = ((double) a - b) / b;

    // Would round to zero with one decimal place.
    if (-0.0005 < delta && delta < 0.0005)
        return "-----";

    // Reasonable values; use percent
    if (-100.0 < delta && delta < 100.0)
        return (rmt::String(delta >= 0? "+" : NULL) +
            rmt::String::decf(delta * 100.0, 1) + '%');

    // Large finite values
    if (-INFINITY < delta && delta < +INFINITY)
        return "*****";

    // Shouldn't happen
    return NULL;
}

rmt::String uct::Result::reportString(bool bTargetOnly) const
{
    RmtClockUtil clkUtil;
    FreqDomain fd;
    VoltDomain vd;
    ostringstream oss;
    size_t nameWidth;

    // Determine the length of the longest domain name
    nameWidth = 0;

    // Iternate over frequency domains
    for (fd = 0; fd < FreqDomain_Count; ++fd)
    {
        if (target.freq[fd] && nameWidth < DomainNameMap::freq.name(fd).length())
            nameWidth = DomainNameMap::freq.name(fd).length();
    }

    // Iternate over voltage domains
    for (vd = 0; vd < VoltDomain_Count; ++vd)
    {
        if (target.volt[vd] && nameWidth < DomainNameMap::volt.name(vd).length())
            nameWidth = DomainNameMap::volt.name(vd).length();
    }

    // Print column header line
    oss << rmt::String::repeat(nameWidth + 1) << "  target flag source";
    if (!bTargetOnly)
        oss << "    actual flag  source delta  measured  delta";
    oss << endl;

    // Iternate over frequency domains
    for (fd = 0; fd < FreqDomain_Count; ++fd)
    {
        // Skip if not defined
        if (target.freq[fd])
        {
            // Print target info
            oss << DomainNameMap::freq.name(fd).lpad(nameWidth) << ":"                  // Clock domain name
                << valueString(target.freq[fd]).lpad(8)                                 // Target frequency in KHz
                << target.forceFlagCode(fd).lpad(5)                                     // Target signal path flag
                << rmt::String(clkUtil.GetClkSourceName(target.source[fd])).lpad(8);    // Target clk source

            // Print everything unless caller says otherwise
            if (!bTargetOnly)
                oss << valueString(actual.freq[fd]).lpad(10)                                // Actual frequency in KHz
                    << actual.forceFlagCode(fd).lpad(5)                                     // Actual signal path flag
                    << rmt::String(clkUtil.GetClkSourceName(actual.source[fd])).lpad(9)     // Actual clk source
                    << deltaString(actual.freq[fd], target.freq[fd]).lpad(7)                // Delta (actual v. target) as a percentage
                    << valueString(measured.freq[fd]).lpad(10)                              // Measured frequency in KHz
                    << deltaString(measured.freq[fd], actual.freq[fd]).lpad(7);             // Delta (measured v. actual) as a percentage

            // Next
            oss << endl;
        }
    }

    // Iternate over voltage domains
    for (vd = 0; vd < VoltDomain_Count; ++vd)
    {
        // Skip if not defined
        if (target.volt[vd])
        {
            // Print target info
            oss << DomainNameMap::volt.name(vd).lpad(nameWidth) << ":"  // Voltage domain name
                << valueString(target.volt[vd]).lpad(8)                 // Target voltage in microvolts
                << "  ---";                                             // Flags (not supported for voltage)

            // Print everything unless caller says otherwise
            if (!bTargetOnly)
                oss << valueString(actual.volt[vd]).lpad(10)                    // Actual voltage in microvolts
                    << "  ---"                                                  // Flags (not supported for voltage)
                    << deltaString(actual.volt[vd], target.volt[vd]).lpad(7)    // Delta (actual v. target) as a percentage
                    << valueString(measured.volt[vd]).lpad(10)                  // Measured voltage in microvolts
                    << deltaString(measured.volt[vd], actual.volt[vd]).lpad(7); // Delta (measured v. actual) as a percentage

            // Next
            oss << endl;
        }
    }

    // Done
    return oss;
}

uct::ExceptionList uct::Result::checkResult(const TrialSpec::ToleranceSetting tolerance, bool bCheckMeasured, bool bCheckSource, UINT32 clkDomainDvcoMinMask)
{
    FreqDomain fd;
    ExceptionList status;
    RmtClockUtil clkUtil;
    rmt::String name;

    for (fd = 0; fd < FreqDomain_Count; ++fd)
    {
        if (!this->target.freq[fd])
        {
            MASSERT(this->actual.freq[fd] == 0);
            if (bCheckMeasured)
            {
                MASSERT(this->measured.freq[fd] == 0);
            }
        }
        else
        {
            name = DomainNameMap::freq.name(fd);

            // Check to see if 'actual' is within delta of target freq based on tolerance setting.
            if (this->actual.freq[fd] < (1.0f - tolerance.actual) * this->target.freq[fd] ||
                this->actual.freq[fd] > (1.0f + tolerance.actual) * this->target.freq[fd])
            {
                status.push(Exception(name+ ": Actual frequency of " +
                    rmt::String::dec(this->actual.freq[fd]) + "KHz is not within " +
                    rmt::String::dec((unsigned long) (tolerance.actual * 100.0f)) + "% of the "+
                    rmt::String::dec(this->target.freq[fd]) + "KHz target.",
                    __PRETTY_FUNCTION__, Exception::Failure));
            }

            //
            // Check to see if the flags match.  RMAPI doesn't always reports flags.
            // But if it does, we should make sure they are not in opposition.
            if ((FLD_TEST_DRF(2080, _CTRL_CLK_INFO_FLAGS, _FORCE_BYPASS, _ENABLE, this->actual.flag[fd])  &&
                 FLD_TEST_DRF(2080, _CTRL_CLK_INFO_FLAGS, _FORCE_PLL,    _ENABLE, this->target.flag[fd])) ||
                (FLD_TEST_DRF(2080, _CTRL_CLK_INFO_FLAGS, _FORCE_PLL,    _ENABLE, this->actual.flag[fd])  &&
                 FLD_TEST_DRF(2080, _CTRL_CLK_INFO_FLAGS, _FORCE_BYPASS, _ENABLE, this->target.flag[fd])))
            {
                status.push(Exception(name+ ": _FORCE flag mismatch.  RMAPI reports " +
                    rmt::String::hex(this->actual.flag[fd], 8) + ".",
                    __PRETTY_FUNCTION__, Exception::Failure));
            }

            if(bCheckSource)
            {
                // Check if the source match, we don't care too much if the target ask for
                // a default source
                if((this->actual.source[fd] != this->target.source[fd])
                    && (this->target.source[fd] != clkUtil.GetClkSourceValue("DEFAULT")))
                {
                    status.push(Exception(name+ ": Source of " + clkUtil.GetClkSourceName(this->actual.source[fd])
                        + " does not match requested "
                        + clkUtil.GetClkSourceName(this->target.source[fd]),
                        __PRETTY_FUNCTION__, Exception::Failure));
                }
            }

            if (bCheckMeasured)
            {
                // Check to see if 'actual' is within delta of measured freq based on tolerance setting.
                if (this->measured.freq[fd] < (1.0f - tolerance.measured) * this->actual.freq[fd] ||
                        this->measured.freq[fd] > (1.0f + tolerance.measured) * this->actual.freq[fd])
                    {
                        if (clkDomainDvcoMinMask & BIT(fd))
                        {
                            Printf(Tee::PriHigh,"DVCO minimum reached for frequency %d, \
                                therefore skipping error check for %d\n", this->actual.freq[fd], fd);
                            continue;
                        }

                        status.push(Exception(name+ ": Measured frequency of " +
                            rmt::String::dec(this->measured.freq[fd]) + "KHz is not within " +
                            rmt::String::dec((unsigned long) (tolerance.measured * 100.0f)) + "% of the "+
                            rmt::String::dec(this->actual.freq[fd]) + "KHz actual frequency.",
                            __PRETTY_FUNCTION__, Exception::Failure));
                    }
            }
        }
    }

    return status;
}

