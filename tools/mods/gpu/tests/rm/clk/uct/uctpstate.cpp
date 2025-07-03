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

//!
//! \file
//!
//! Implements PState abstractions
//!

#include <sstream>
#include <iomanip>

#include "lwtypes.h"
#include "ctrl/ctrl2080/ctrl2080clk.h"
#include "ctrl/ctrl2080/ctrl2080perf.h"

#include "gpu/tests/rm/utility/rmtstream.h"
#include "gpu/utility/rmclkutil.h"

#include "uctexception.h"
#include "uctpstate.h"
#include "ucttrialspec.h"

/*******************************************************************************
    ApiFlagMap
*******************************************************************************/

UINT32 uct::ApiFlagMap::translateClk2Perf(UINT32 clk)
{
    UINT32 perf = 0x00000000;

    if (      FLD_TEST_DRF(2080, _CTRL_CLK_INFO_FLAGS,          _FORCE_PLL, _ENABLE,  clk))
        perf = FLD_SET_DRF(2080, _CTRL_PERF_CLK_DOM_INFO_FLAGS, _FORCE_PLL, _ENABLE,  perf);
    else
        perf = FLD_SET_DRF(2080, _CTRL_PERF_CLK_DOM_INFO_FLAGS, _FORCE_PLL, _DISABLE, perf);

    if (      FLD_TEST_DRF(2080, _CTRL_CLK_INFO_FLAGS,          _FORCE_NAFLL, _ENABLE,  clk))
        perf = FLD_SET_DRF(2080, _CTRL_PERF_CLK_DOM_INFO_FLAGS, _FORCE_NAFLL, _ENABLE,  perf);
    else
        perf = FLD_SET_DRF(2080, _CTRL_PERF_CLK_DOM_INFO_FLAGS, _FORCE_NAFLL, _DISABLE, perf);

    if (      FLD_TEST_DRF(2080, _CTRL_CLK_INFO_FLAGS,          _FORCE_BYPASS, _ENABLE,  clk))
        perf = FLD_SET_DRF(2080, _CTRL_PERF_CLK_DOM_INFO_FLAGS, _FORCE_BYPASS, _ENABLE,  perf);
    else
        perf = FLD_SET_DRF(2080, _CTRL_PERF_CLK_DOM_INFO_FLAGS, _FORCE_BYPASS, _DISABLE, perf);

    return perf;
}

UINT32 uct::ApiFlagMap::translatePerf2Clk(UINT32 perf)
{
    UINT32 clk = 0x00000000;

    if (FLD_TEST_DRF(2080, _CTRL_PERF_CLK_DOM_INFO_FLAGS, _FORCE_PLL, _ENABLE,  perf))
        clk = FLD_SET_DRF(2080, _CTRL_CLK_INFO_FLAGS,     _FORCE_PLL, _ENABLE,  clk);
    else
        clk = FLD_SET_DRF(2080, _CTRL_CLK_INFO_FLAGS,     _FORCE_PLL, _DISABLE, clk);

    if (FLD_TEST_DRF(2080, _CTRL_PERF_CLK_DOM_INFO_FLAGS, _FORCE_NAFLL, _ENABLE,  perf))
        clk = FLD_SET_DRF(2080, _CTRL_CLK_INFO_FLAGS,     _FORCE_NAFLL, _ENABLE,  clk);
    else
        clk = FLD_SET_DRF(2080, _CTRL_CLK_INFO_FLAGS,     _FORCE_NAFLL, _DISABLE, clk);

    if (FLD_TEST_DRF(2080, _CTRL_PERF_CLK_DOM_INFO_FLAGS, _FORCE_BYPASS, _ENABLE,  perf))
        clk = FLD_SET_DRF(2080, _CTRL_CLK_INFO_FLAGS,     _FORCE_BYPASS, _ENABLE,  clk);
    else
        clk = FLD_SET_DRF(2080, _CTRL_CLK_INFO_FLAGS,     _FORCE_BYPASS, _DISABLE, clk);

    return clk;
}

const UINT32 uct::ApiFlagMap::clkFlagSet =
    DRF_SHIFTMASK(LW2080_CTRL_CLK_INFO_FLAGS_FORCE_PLL)     |
    DRF_SHIFTMASK(LW2080_CTRL_CLK_INFO_FLAGS_FORCE_BYPASS)  |
    DRF_SHIFTMASK(LW2080_CTRL_CLK_INFO_FLAGS_FORCE_NAFLL);

/*******************************************************************************
    PState
*******************************************************************************/

// Mark the p-state as invalid (i.e. null)
void uct::PState::ilwalidate()
{
    FreqDomain  fd;
    VoltDomain  vd;

    // Mark each fequency (clock) domain as invalid.
    for (fd = 0; fd < FreqDomain_Count; ++fd)
        freq[fd] = source[fd] = flag[fd] = 0;

    // Mark each voltage domain as invalid.
    for (vd = 0; vd < VoltDomain_Count; ++vd)
        volt[vd] = 0;
}

// Returns true if any source has a non-default value
bool uct::PState::hasSourceConfig() const
{
    FreqDomain fd;
    bool hasSource = false;

    for(fd = 0; fd < FreqDomain_Count; ++fd)
        hasSource |= (source[fd] != LW2080_CTRL_CLK_SOURCE_DEFAULT);

    return hasSource;
}

/*******************************************************************************
    FullPState
*******************************************************************************/
const rmt::String uct::FullPState::initPStateName = "initPState";

// Mark the p-state as invalid (i.e. null)
void uct::FullPState::ilwalidate()
{
    // Mark as invalid the p-state as a whole.
    base = IlwalidVbiosPStateIndex;

    // Mark this is not a init pstate
    bInitPState = false;

    // Mark everything in the super class.
    PState::ilwalidate();
}

// Is this a VBIOS p-state?
bool uct::FullPState::matches(const VbiosPStateArray &vbios) const
{
    return vbios.isValid(base) && matches(vbios[base]);
}

// Are these p-states equivalent?
bool uct::FullPState::matches(const FullPState &that) const
{
    FreqDomain fd;
    VoltDomain vd;

    // Unequal if we find a freq or flag that is unequal.
    for (fd = 0; fd < FreqDomain_Count; ++fd)
        if (freq[fd] != that.freq[fd] ||
            flag[fd] != that.flag[fd])
            return false;

    // Unequal if we find a voltage that is unequal.
    for (vd = 0; vd < VoltDomain_Count; ++vd)
        if (volt[vd] != that.volt[vd])
            return false;

    // Equal otherwise
    return true;
}

rmt::String uct::FullPState::string() const
{
    FreqDomain fd;
    VoltDomain vd;
    RmtClockUtil clkUtil;
    ostringstream oss;

    if (!isValid())
        return NULL;

    // Name and VBIOS p-state used as the base
    oss << start.locationString() << name.defaultTo("<anonymous>")
        << "\n\tbase=P" << (unsigned) base;

    // Iternate over frequency domains
    for (fd = 0; fd < FreqDomain_Count; ++fd)
    {
        // Skip if not defined
        if (freq[fd])
        {
            // Start with the domain name and frequency
            oss << "\n\t" << DomainNameMap::freq.name(fd)
                << ": freq=" << freq[fd] << "KHz";

            oss << " source=" << clkUtil.GetClkSourceName(source[fd]);

            // Add flags if applicable
            // TODO: Translate these flags to text
            if (flag[fd])
                oss << " flag=" << rmt::String::hex(flag[fd], 8);
        }
    }

    // Iternate over voltage domains
    for (vd = 0; vd < VoltDomain_Count; ++vd)
    {
        // Skip if not defined
        if (volt[vd])
        {
            // Output the domain name and voltage
            oss << "\n\t" << DomainNameMap::volt.name(vd)
                << ": volt=" << volt[vd] << "uv";
        }
    }

    // Done
    return oss;
}

