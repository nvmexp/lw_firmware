/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2012,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file clk_gk20a.cpp
//! \brief Implementation of GK20A clk hal
//!
#include "clkbase.h"
#include "core/include/memcheck.h"

ClockUtilGK20A::ClockUtilGK20A(LwU32 hClient, LwU32 hDev, LwU32 hSubDev)
    :ClockUtilGK104(hClient, hDev, hSubDev)
{
    m_xtalFreqKHz = 0;
    memset(&m_RamType, 0, sizeof(m_RamType));

    // Callwlate these frequencies before they are used
    GetXtalFreqKHz();
    m_oneSrcFreqKHz = m_xtalFreqKHz; // xtal is the only source
}

//!
//! @brief Identify if a clock is a root clock
//!   For GK20A, there are no root clocks
//!
//! @param[in]  clkDomain
//! @param[out] None

//! @returns    TRUE if the clock is a root clock
//-----------------------------------------------------------------------------
LwBool ClockUtilGK20A::clkIsRootClock(LwU32 clkDomain)
{
    return LW_FALSE;
}

//!
//!@brief Used to get the onesrc frequency
//!
//!@return The OneSrc Freq for the chip in KHz
//!----------------------------------------------------------------------------
LwU32 ClockUtilGK20A::GetOneSrcFreqKHz()
{
    return m_xtalFreqKHz;
}

//!
//! @brief Identify if a clock is a derivative clock
//!   For GK20A, there are no derivative clocks
//!
//! @param[in]  clkDomain
//! @param[out] None

//! @returns    TRUE if the clock is a derivative clock
//-----------------------------------------------------------------------------
LwBool ClockUtilGK20A::clkIsDerivativeClock(LwU32 clkDomain)
{
    return LW_FALSE;
}

//!
//! @brief Returns default PLL source for the clock
//!   For a core clock, it returns the PLL used as Source.
//!
//! @param[in]  clkDomain
//! @param[out] None

//! @returns LW2080_CTRL_CLK_SOURCE_DEFAULT if clkDomain is not a Core clock
//-----------------------------------------------------------------------------
LwU32 ClockUtilGK20A::clkGetCoreClkPllSource(LwU32 clkDomain)
{
    if (LW2080_CTRL_CLK_DOMAIN_GPC2CLK == clkDomain)
        return LW2080_CTRL_CLK_SOURCE_GPCPLL;

    return LW2080_CTRL_CLK_SOURCE_DEFAULT;
}

//!
//!@brief Tests basic clock path &  simple frequency change
//!
//!@return OK if all tests passed, specific RC if any test failed
//!----------------------------------------------------------------------------
RC ClockUtilGK20A::ClockSanityTest()
{
    RC rc;
    vector<LW2080_CTRL_CLK_INFO> vOrigClkInfo;
    vector<LW2080_CTRL_CLK_INFO> vClkInfo;
    vector<LW2080_CTRL_CLK_INFO> vNewClks;
    LwU32 errorCount = 0;

    // Cache the original clock freqs
    CHECK_RC(GetProgrammableClocks(vOrigClkInfo));
    vClkInfo = vOrigClkInfo;

    for (LwU32 i = 0; i < vClkInfo.size(); ++i)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(vClkInfo[i].clkDomain);
        const char * const sourceName = m_ClkUtil.GetClkSourceName(vClkInfo[i].clkSource);
        LwU32 clkSrcPll = clkGetCoreClkPllSource(vClkInfo[i].clkDomain);

        // Check 1: No clk should be running on default source(No Source!)
        if (vClkInfo[i].clkSource == LW2080_CTRL_CLK_SOURCE_DEFAULT)
        {
            Printf(Tee::PriHigh, "Invalid Clock Source \n");
            Printf(Tee::PriHigh, "ERROR: %s is running on No Source.\n",
                   domainName);
            errorCount++;
        }

        //
        // Check 2: If a clock is running on any source(even a PLL),it should
        // be on and running. Otherwise the clock actual frequency will be zero.
        //
        else if (vClkInfo[i].actualFreq == 0)
        {
            Printf(Tee::PriHigh, "Invalid Clock Source Configuration \n");
            Printf(Tee::PriHigh, "ERROR: The following clock source is not"
                   " running -> %s .\n", sourceName);
            errorCount++;
        }

        // Check 3: Clock source can either be XTAL or it's own PLL
        else if ((vClkInfo[i].clkSource != LW2080_CTRL_CLK_SOURCE_XTAL) &&
                 (vClkInfo[i].clkSource != clkSrcPll))
        {
            Printf(Tee::PriHigh, "Invalid Clock Tree Configuration \n");
            Printf(Tee::PriHigh, "ERROR: %s not running on XTAL or it's PLL.\n",
                domainName);
            errorCount++;
        }

    }

    // If any of the above checks failed, bail now
    if (errorCount)
    {
        rc =  RC::LWRM_ERROR;
        return rc;
    }

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Set the new frequency for all the clocks\n");

    for (LwU32 i = 0; i < vClkInfo.size(); ++i)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(vClkInfo[i].clkDomain);
        LwU32 clkSrcPll = clkGetCoreClkPllSource(vClkInfo[i].clkDomain);
        LW2080_CTRL_CLK_PLL_INFO pllInfo = {0};
        LwU32 freqKHz;

        //
        // If this clk domain has its own PLL, check for its operating limits
        // and pick a frequency within this limit that isn't the current frequency.
        //
        if (clkSrcPll != LW2080_CTRL_CLK_SOURCE_DEFAULT)
        {
            CHECK_RC(clkGetPllInfo(clkSrcPll, pllInfo));

            for (freqKHz = pllInfo.maxVcoFreq; freqKHz >= pllInfo.milwcoFreq; freqKHz -= 50000)
            {
                if (vClkInfo[i].actualFreq != freqKHz)
                {
                    vClkInfo[i].targetFreq = freqKHz;
                    break;
                }
            }
        }
        else // Non-core clk, not supported for CheetAh GPU clocks.
        {
            Printf(Tee::PriHigh, "CheetAh doesn't support non-core clocks!\n");
            return RC::SOFTWARE_ERROR;
        }
        Printf(Tee::PriHigh, "Programming %s to Frequency %d\n",
               domainName, (int)vClkInfo[i].targetFreq);
    }

    CHECK_RC(ProgramAndVerifyClockFreq(vClkInfo, vNewClks));

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Restoring the original frequency for all the clks\n");
    Printf(Tee::PriHigh, "\n");

    for (LwU32 i = 0; i < vOrigClkInfo.size(); ++i)
        vOrigClkInfo[i].targetFreq = vOrigClkInfo[i].actualFreq;

    CHECK_RC(ProgramAndVerifyClockFreq(vOrigClkInfo, vNewClks));

    return rc;
}

