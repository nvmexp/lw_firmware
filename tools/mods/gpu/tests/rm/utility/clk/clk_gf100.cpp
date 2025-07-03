/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file clk_gf100.cpp
//! \brief Implementation of GF100 clk hal
//!

#include "clkbase.h"
#include "core/include/memcheck.h"

#define FP_DIV_LIMIT          17 //LW_PTRIM_SYS_GPC2CLK_OUT_BYPDIV_BY17
#define MIN_DIV2X             2  //LW_PTRIM_SYS_GPC2CLK_OUT_BYPDIV_BY1
#define MIN_MCLK_REF_FREQ_KHZ 1000000

ClockUtilGF100::ClockUtilGF100(LwU32 hClient, LwU32 hDev, LwU32 hSubDev)
    :ClockUtil(hClient, hDev, hSubDev)
{
    m_oneSrcFreqKHz = 0;
    m_xtalFreqKHz = 0;
    memset(&m_RamType, 0, sizeof(m_RamType));

    // Callwlate these frequencies before they are used
    GetOneSrcFreqKHz();
    GetXtalFreqKHz();
}

ClockUtilGF100::~ClockUtilGF100()
{
    mClkFreqLUT.clear();
}

//!
//! @brief Prints all relevant information about the clk domains passed in
//!
//! @param clkDomains Mask of clkDomains whose details are to be printed
//!
//! @return OK if all's well, specific RC if failed.
//-----------------------------------------------------------------------------
RC ClockUtilGF100::PrintClocks(const LwU32 clkDomains)
{
    RC rc;
    LwRmPtr pLwRm;
    vector<LW2080_CTRL_CLK_INFO> vClkInfo;

    CHECK_RC(GetClocks(clkDomains, vClkInfo));

    for (LwU32 i = 0; i < vClkInfo.size(); ++i)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(vClkInfo[i].clkDomain);
        const char * const sourceName = m_ClkUtil.GetClkSourceName(vClkInfo[i].clkSource);
        LW2080_CTRL_CLK_MEASURE_FREQ_PARAMS clkCounterFreqParams = {0};

        Printf(Tee::PriHigh,"Clock Domain is %s .\n", domainName);
        Printf(Tee::PriHigh,"Clock Source is %s. \n", sourceName);
        Printf(Tee::PriHigh,"Actual Frequency is  %d \n", (int)vClkInfo[i].actualFreq);
        Printf(Tee::PriHigh,"Target Frequency is  %d \n", (int)vClkInfo[i].targetFreq);
        Printf(Tee::PriHigh,"Flag Value is  %d \n", (LwU32)vClkInfo[i].flags);

        // Clk counters are not implemented on FModel
        if (Platform::GetSimulationMode() != Platform::Fmodel)
        {
            // Get the Clock Counter Frequency for this clock.
            clkCounterFreqParams.clkDomain = vClkInfo[i].clkDomain;

            CHECK_RC(pLwRm->Control(m_hSubDev,
                                    LW2080_CTRL_CMD_CLK_MEASURE_FREQ,
                                    &clkCounterFreqParams,
                                    sizeof (clkCounterFreqParams)));

            Printf(Tee::PriHigh, "Clock Counter Frequency is %d\n",
                   (int)clkCounterFreqParams.freqKHz);
        }
        Printf(Tee::PriHigh,"\n");
    }
    return rc;
}

//!
//! @brief Identify if a clock is a root clock
//!
//! @param[in]  clkDomain
//! @param[out] None

//! @returns    TRUE if the clock is a root clock
//-----------------------------------------------------------------------------
LwBool ClockUtilGF100::clkIsRootClock(LwU32 clkDomain)
{
    if ((LW2080_CTRL_CLK_DOMAIN_DISPCLK == clkDomain)  ||
        (LW2080_CTRL_CLK_DOMAIN_UTILSCLK == clkDomain) ||
        (LW2080_CTRL_CLK_DOMAIN_HOSTCLK == clkDomain)  ||
        (LW2080_CTRL_CLK_DOMAIN_PWRCLK == clkDomain))
        return LW_TRUE;
    return LW_FALSE;
}

//!
//! @brief Identify if a clock is a derivative clock
//!
//! @param[in]  clkDomain
//! @param[out] None

//! @returns    TRUE if the clock is a derivative clock
//-----------------------------------------------------------------------------
LwBool ClockUtilGF100::clkIsDerivativeClock(LwU32 clkDomain)
{
    if ((LW2080_CTRL_CLK_DOMAIN_SYS2CLK == clkDomain)  ||
        (LW2080_CTRL_CLK_DOMAIN_HUB2CLK == clkDomain)  ||
        (LW2080_CTRL_CLK_DOMAIN_LEGCLK == clkDomain)   ||
        (LW2080_CTRL_CLK_DOMAIN_MSDCLK == clkDomain))
        return LW_TRUE;
    return LW_FALSE;
}

//!
//! @brief Gets mask of all programmable domains
//!
//! @return Mask of all programmable domains
//!----------------------------------------------------------------------------
LwU32 ClockUtilGF100::GetProgrammableDomains()
{
    if (!m_ProgClkDomains)
    {
        m_ProgClkDomains = ClockUtil::GetProgrammableDomains();

        //
        // (Bug 675826) Remove HostClk from list of programmable domains
        // for all Fermi Fmodels, as HostClk registers aren't modelled there.
        //
        if (Platform::GetSimulationMode() == Platform::Fmodel)
            m_ProgClkDomains &= ~(LW2080_CTRL_CLK_DOMAIN_HOSTCLK);

        // Programmable domains should not be empty!
        MASSERT(m_ProgClkDomains);
    }

    return m_ProgClkDomains;
}

//!
//! @brief Programs the clocks to the frequencies specified and verifies them
//!        afterwards to ensure the clocks changed successfully.
//!
//! @param vSetClkInfo Const array of clocks to be programmed
//! @param vNewClkInfo Current Frequencies of all programmable clocks
//!                    after the clocks have been programmed to requested freq.
//!
//! @return OK if the programming and verification passed, specific RC if failed
//!----------------------------------------------------------------------------
RC ClockUtilGF100::ProgramAndVerifyClockFreq
(
    const vector<LW2080_CTRL_CLK_INFO>& vSetClkInfo,
    vector<LW2080_CTRL_CLK_INFO>& vNewClkInfo
)
{
    RC rc;
    vector<LW2080_CTRL_CLK_INFO> vFilteredClkInfo;

    vFilteredClkInfo = vSetClkInfo;

    SYNC_POINT(CLKUTIL_SP_PROGRAMCLK_START);

#if LWOS_IS_WINDOWS
    for (LwU32 i = 0; i < vFilteredClkInfo.size(); ++i)
    {
        LwU32 numClkDomainID = vFilteredClkInfo[i].clkDomain;
        const char * const domainName = m_ClkUtil.GetClkDomainName(numClkDomainID);

        // Root clocks can be overclocked above P0 freq
        if (clkIsRootClock(numClkDomainID))
        {
            continue;
        }

        if (mClkFreqLUT[numClkDomainID].maxOperationalFreqKHz &&
            vFilteredClkInfo[i].targetFreq > mClkFreqLUT[numClkDomainID].maxOperationalFreqKHz)
        {
            Printf(Tee::PriHigh, "Cannot set freq %d for clock domain %s as its greater than %d as defined in P0\n",
                   vFilteredClkInfo[i].targetFreq, domainName, mClkFreqLUT[numClkDomainID].maxOperationalFreqKHz);

            vFilteredClkInfo[i].targetFreq = mClkFreqLUT[numClkDomainID].maxOperationalFreqKHz;
            Printf(Tee::PriHigh, "Programming %s to Frequency %d\n", domainName, (int)vFilteredClkInfo[i].targetFreq);
        }

        if (mClkFreqLUT[numClkDomainID].minOperationalFreqKHz &&
            vFilteredClkInfo[i].targetFreq < mClkFreqLUT[numClkDomainID].minOperationalFreqKHz)
        {
            Printf(Tee::PriHigh, "Cannot set freq %d for clock domain %s as its lesser than xtal freq %d\n",
                   vFilteredClkInfo[i].targetFreq, domainName, mClkFreqLUT[numClkDomainID].minOperationalFreqKHz);

            vFilteredClkInfo[i].targetFreq = mClkFreqLUT[numClkDomainID].minOperationalFreqKHz;
            Printf(Tee::PriHigh, "Programming %s to Frequency %d\n", domainName, (int)vFilteredClkInfo[i].targetFreq);
        }
    }
#endif

    CHECK_RC(ClockUtil::ProgramAndVerifyClockFreq(vFilteredClkInfo, vNewClkInfo));

    SYNC_POINT(CLKUTIL_SP_PROGRAMCLK_STOP);
    return rc;
}

//!
//! @brief Verifies if the current clk frequencies are matching the target
//!        frequencies of the given clk domains
//!
//! @param vSetClkInfo Vector containing target frequencies
//! @param vNewClkInfo Vector containing current frequencies of all clocks
//!
//! @return
//!----------------------------------------------------------------------------
RC ClockUtilGF100::VerifyClockFreq
(
    const vector<LW2080_CTRL_CLK_INFO>& vSetClkInfo,
    vector<LW2080_CTRL_CLK_INFO>& vNewClkInfo
)
{
    RC rc;
    LwU32 tolerance100X = 0;
    LwRmPtr pLwRm;
    LW2080_CTRL_CLK_MEASURE_FREQ_PARAMS clkCounterFreqParams;

    // Get the current frequencies of the clks we just programmed
    vNewClkInfo = vSetClkInfo;
    CHECK_RC(GetClocks(vNewClkInfo));

    for (LwU32 i = 0; i < vSetClkInfo.size(); ++i)
    {
        LwU32 actualClkFreq, expClkFreq;

        MASSERT(vSetClkInfo[i].clkDomain == vNewClkInfo[i].clkDomain);

        tolerance100X = GetFreqTolerance100X(vSetClkInfo[i]);

        Printf(Tee::PriHigh, "Clk Name : %13s,    ",
               m_ClkUtil.GetClkDomainName(vSetClkInfo[i].clkDomain));

        Printf(Tee::PriHigh, "Source : %10s,    ",
               m_ClkUtil.GetClkSourceName(vNewClkInfo[i].clkSource));

        expClkFreq = vSetClkInfo[i].targetFreq;
        Printf(Tee::PriHigh, "Expected Freq : %7d KHz, ",
               (UINT32)(expClkFreq));

        actualClkFreq = vNewClkInfo[i].actualFreq;
        Printf(Tee::PriHigh, "Actual Freq (SW) : %7d KHz, ",
               (UINT32)(actualClkFreq));

        // Clk counters are not implemented on FModel
        if (Platform::GetSimulationMode() != Platform::Fmodel)
        {
            clkCounterFreqParams.clkDomain = vSetClkInfo[i].clkDomain;

            // Get the Clock Counter Frequency for this clock
            CHECK_RC(pLwRm->Control(m_hSubDev,
                                    LW2080_CTRL_CMD_CLK_MEASURE_FREQ,
                                    &clkCounterFreqParams,
                                    sizeof (clkCounterFreqParams)));

            actualClkFreq = clkCounterFreqParams.freqKHz;
            Printf(Tee::PriHigh, "Actual Freq (HW Clock Counter) : %7d KHz, ",
               (LwU32)actualClkFreq);
        }

        double fPercentDiff
            = ((double)actualClkFreq - (double)expClkFreq) *
              100.0 / (double)expClkFreq;

        Printf(Tee::PriHigh, "Difference : %+5.1f %%\n", fPercentDiff);
        MASSERT(abs(fPercentDiff * 100.0) <= tolerance100X);
        if (abs(fPercentDiff * 100.0) > tolerance100X)
        {
            Printf(Tee::PriHigh, "ERROR: Value exceeds tolerance %u\n", (unsigned)tolerance100X);
        }
    }
    return rc;
}

//!
//! @brief clkSelectTargetFreq function.
//!
//!  This function selects the closest Bypass or VCO path frequency for
//!  the given reference frequency
//!  This helper function is used to get the closest Bypass or VCO Path Frequency
//!  for a given Frequency(refFreqKHz) that serves as a refrence for caluclation.
//!  refFreqKHz is mostly the current clock state Freq, based on decision of whether
//!  its a Bypass Freq or not we decide the next stage frequency.
//!
//! @param bSwitchToVCOPath Whether the targetFreqKHz required is a VCO freq
//! @param clkDomain        Clk Domain
//! @param refFreqKHz       Reference frequency, usually the current frequency
//! @param targetFreqKHz    Reference to the callwlated target freq
//!
//! @return OK if the callwlation passed, specific RC if failed.
//!----------------------------------------------------------------------------
RC ClockUtilGF100::clkSelectTargetFreq
(
    LwBool bSwitchToVCOPath,
    LwU32  clkDomain,
    LwU32  refFreqKHz,
    LwU32& targetFreqKHz
)
{
    LwRmPtr pLwRm;
    LW2080_CTRL_CLK_GET_CLOSEST_BYPASS_FREQ_PARAMS clkBypassInfoParams = {0};
    LwU32 targetFreq = 0;
    LwU32 div = 0;
    bool bPresentPathVCO = true;
    RC rc = OK;

    clkBypassInfoParams.clkDomain = clkDomain;
    clkBypassInfoParams.targetFreqKHz = refFreqKHz;

    if (clkBypassInfoParams.targetFreqKHz == 0)
    {
        bPresentPathVCO = false;
    }
    else
    {
        div = (2 * m_oneSrcFreqKHz)/clkBypassInfoParams.targetFreqKHz;

        if ((div && (2 * m_oneSrcFreqKHz / div) == clkBypassInfoParams.targetFreqKHz) ||
            (clkBypassInfoParams.targetFreqKHz == m_oneSrcFreqKHz /FP_DIV_LIMIT))
        {
            bPresentPathVCO = false;
        }
    }

    if (bSwitchToVCOPath == false)
    {
        // We want to switch to ALT Path from ALT?VCO Path.
        CHECK_RC(pLwRm->Control(m_hSubDev,
                                LW2080_CTRL_CMD_CLK_GET_CLOSEST_BYPASS_FREQ,
                                &clkBypassInfoParams,
                                sizeof (clkBypassInfoParams)));

        // Check if there exists an upper limit on frequency for this domain
        if (mClkFreqLUT[clkDomain].maxOperationalFreqKHz)
        {
            while (clkBypassInfoParams.maxBypassFreqKHz >
                   mClkFreqLUT[clkDomain].maxOperationalFreqKHz)
            {
                clkBypassInfoParams.targetFreqKHz
                    = clkBypassInfoParams.minBypassFreqKHz;

                CHECK_RC(pLwRm->Control(m_hSubDev,
                     LW2080_CTRL_CMD_CLK_GET_CLOSEST_BYPASS_FREQ,
                     &clkBypassInfoParams,
                     sizeof (clkBypassInfoParams)));
            }
        }

        // Set it to closest higher alt path frequency from the present state
        targetFreq = clkBypassInfoParams.maxBypassFreqKHz;

        // Set it to the lower closest Alt path frequency from the present state
        // if it crosses the OneSrc Freq limit
        // Also, if ref freq is equal to the max bypass freq, set target freq to next lower freq
        if (targetFreq > m_oneSrcFreqKHz || refFreqKHz == targetFreq)
            targetFreq = clkBypassInfoParams.minBypassFreqKHz;
    }
    else
    {

        CHECK_RC(pLwRm->Control(m_hSubDev,
                                LW2080_CTRL_CMD_CLK_GET_CLOSEST_BYPASS_FREQ,
                                &clkBypassInfoParams,
                                sizeof (clkBypassInfoParams)));

        if (bPresentPathVCO == false)
        {
            //
            // Switching from ALT Path to Ref Path.
            // Set it to the higher closest VCO path frequency from the present state.
            //
            targetFreq = (clkBypassInfoParams.maxBypassFreqKHz + clkBypassInfoParams.targetFreqKHz) / 2;
            // FERMITODO:CLK Should there be any upper limit on VCO Freq?
        }

        else
        {
            //
            // Switching from Ref Path to Ref Path.  We must ensure that we choose a
            // higher frequency so that we don't get stuck.  We use a half step above
            // the existing maximum for this reason.
            //
            clkBypassInfoParams.targetFreqKHz = clkBypassInfoParams.maxBypassFreqKHz +
                (clkBypassInfoParams.maxBypassFreqKHz - clkBypassInfoParams.minBypassFreqKHz) / 2;

            CHECK_RC(pLwRm->Control(m_hSubDev,
                LW2080_CTRL_CMD_CLK_GET_CLOSEST_BYPASS_FREQ,
                &clkBypassInfoParams,
                sizeof (clkBypassInfoParams)));

            //
            // If the API reports the same value for both min and max, we take that to mean we've
            // reached a maximum and, as such, choose a frequency that is a half step above this point.
            //
            if (clkBypassInfoParams.maxBypassFreqKHz == clkBypassInfoParams.minBypassFreqKHz)
                targetFreq = clkBypassInfoParams.targetFreqKHz;

            // Otherwise, we may choose the mean average between the bypass frequencies
            else
                targetFreq = (clkBypassInfoParams.maxBypassFreqKHz + clkBypassInfoParams.minBypassFreqKHz) / 2;

            // FERMITODO:CLK Should there be any upper limit on VCO Freq?
        }

#if LWOS_IS_WINDOWS
        if (clkDomain == LW2080_CTRL_CLK_DOMAIN_MCLK && (targetFreq < MIN_MCLK_REF_FREQ_KHZ || refFreqKHz == targetFreq))
        {
            LwU32 vcoFreqKHz1 = ((m_oneSrcFreqKHz * 2) / MIN_DIV2X + (m_oneSrcFreqKHz * 2) / (MIN_DIV2X + 1))/ 2;
            LwU32 vcoFreqKHz2 = mClkFreqLUT[clkDomain].maxOperationalFreqKHz;

            if (abs((int)(refFreqKHz - vcoFreqKHz1)) > abs((int)(refFreqKHz - vcoFreqKHz2)))
                targetFreq = vcoFreqKHz1;
            else
                targetFreq = vcoFreqKHz2;
        }
#endif
    }

    targetFreqKHz = targetFreq;
    return rc;
}

//!
//!@brief Used to get the onesrc frequency
//!
//!@return The OneSrc Freq for the chip in KHz
//!----------------------------------------------------------------------------
LwU32 ClockUtilGF100::GetOneSrcFreqKHz()
{
    if (!m_oneSrcFreqKHz)
    {
        clkSelectTargetFreq(LW_FALSE, LW2080_CTRL_CLK_DOMAIN_GPC2CLK, 0xffffffff, m_oneSrcFreqKHz);
        MASSERT(m_oneSrcFreqKHz);
    }

    return m_oneSrcFreqKHz;
}

//!
//!@brief Used to get teh xtal frequency
//!
//!@return The Xtal freq for the chip in KHz
//!----------------------------------------------------------------------------
LwU32 ClockUtilGF100::GetXtalFreqKHz()
{
    if (!m_xtalFreqKHz)
    {
        GetClkSrcFreqKHz(LW2080_CTRL_CLK_SOURCE_XTAL, m_xtalFreqKHz);
        MASSERT(m_xtalFreqKHz);
    }

    return m_xtalFreqKHz;
}

//!
//!@brief Used to get the memory type
//!
//!@return The memory type in formats as defined in ctrl2080fb.h
//!----------------------------------------------------------------------------
LwU32 ClockUtilGF100::GetRamType()
{
    if (!m_RamType.data)
    {
        RC rc;
        LwRmPtr pLwRm;
        LW2080_CTRL_FB_GET_INFO_PARAMS fbGetInfoParams;

        // Get the memory type.
        m_RamType.index                   = LW2080_CTRL_FB_INFO_INDEX_RAM_TYPE;
        m_RamType.data                    = 0;
        fbGetInfoParams.fbInfoListSize    = 1;
        fbGetInfoParams.fbInfoList        = LW_PTR_TO_LwP64(&m_RamType);

        CHECK_RC(pLwRm->Control(m_hSubDev,
                                LW2080_CTRL_CMD_FB_GET_INFO,
                                &fbGetInfoParams,
                                sizeof (fbGetInfoParams)));
    }

    return m_RamType.data;
}

RC ClockUtilGF100::ClockSanityTest()
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
        bool checkClk = true;

        //
        // Exclude PCLK from any such verification for Now.
        // Later we can enhance the test to verify that if
        // the head is active then the source for PCLK should
        // not be XTAL, otherwise it should be XTAL as a sanity check.
        //
        for (LwU32 pi = 0; pi < LW2080_CTRL_CLK_DOMAIN_PCLK__SIZE_1; pi++)
        {
            if (LW2080_CTRL_CLK_DOMAIN_PCLK(pi) == vClkInfo[i].clkDomain)
                checkClk = false;
        }

        if (!checkClk)
            continue;

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
        if ((vClkInfo[i].clkSource != LW2080_CTRL_CLK_SOURCE_DEFAULT) &&
            (vClkInfo[i].actualFreq == 0))
        {
            Printf(Tee::PriHigh, "Invalid Clock Source Configuration \n");
            Printf(Tee::PriHigh, "ERROR: The following clock source is not"
                   " running -> %s .\n", sourceName);
            errorCount++;
        }

        // Check 3: None of the clocks should be running on XTAL.
        if (vClkInfo[i].clkSource == LW2080_CTRL_CLK_SOURCE_XTAL)
        {
            Printf(Tee::PriHigh, "Invalid Clock Tree Configuration \n");
            Printf(Tee::PriHigh, "ERROR: %s is still running on XTAL.\n",
                domainName);
            errorCount++;
        }

        //
        // CHECK 4: UTILSCLK should never run a frequency other than 108MHz.
        // On emulation however, it's sometimes set to a higher freq for PLL
        // netlists to boot faster (Bug 714446).
        //
        if ((vClkInfo[i].clkDomain == LW2080_CTRL_CLK_DOMAIN_UTILSCLK) &&
            (vClkInfo[i].actualFreq != 108000))
        {
            Printf(Tee::PriHigh,"Invalid Configuration for UTILSCLK\n");
            if (Platform::GetSimulationMode() == Platform::Fmodel ||
                Platform::GetSimulationMode() == Platform::RTL    ||
                Platform::GetSimulationMode() == Platform::Hardware)
            {
                Printf(Tee::PriHigh, "ERROR: UTILSCLK is running on Freq: %d\n",
                       (int)(vClkInfo[i].actualFreq));
                errorCount++;
            }
            else
            {
                Printf(Tee::PriHigh, "WARNING: UTILSCLK is running on Freq: %d\n",
                      (int)vClkInfo[i].actualFreq);
            }
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

        //
        // Program clks to an attainable bypass freq. Goal is to check that
        // all clk domains can successfully switch to a new frequency and back
        //
        clkSelectTargetFreq(LW_FALSE, vClkInfo[i].clkDomain,
            vClkInfo[i].actualFreq / 2, vClkInfo[i].targetFreq);

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

//!
//!@brief Tests the clock paths for root, sys-derived and core clocks
//!
//!@return OK if all tests passed, specific RC if any test failed
//!----------------------------------------------------------------------------
RC ClockUtilGF100::ClockPathTest()
{
    RC rc;
    vector<LW2080_CTRL_CLK_INFO> vOrigClkInfo;
    vector<LW2080_CTRL_CLK_INFO> vSetClkInfo;

    //
    // Initialize the data structure that will have the min/max frequencies
    // that can be achieved for each domain. A lower/upper limit of 0 implies
    // that there is no limit applicable for that domain
    //
    for (LwU32 i = 0; i < 32; ++i)
    {
        if (BIT(i) & GetProgrammableDomains())
        {
            LW208F_CTRL_CLK_GET_OPERATIONAL_FREQ_LIMITS_PARAMS clkLimits = {0};
            clkLimits.clkDomain = BIT(i);
            mClkFreqLUT[BIT(i)] = clkLimits;
        }
    }

#if LWOS_IS_WINDOWS
    LwRmPtr    pLwRm;
    LW2080_CTRL_PERF_GET_PSTATES_INFO_PARAMS getAllPStatesInfoParams;
    LW2080_CTRL_PERF_GET_PSTATE_INFO_PARAMS getPStateInfoParams;
    LW2080_CTRL_PERF_CLK_DOM_INFO* pClkInfoList = NULL;
    LW2080_CTRL_PERF_VOLT_DOM_INFO *pVoltInfoList = NULL;
    LwU32 ClkListIdx = 0;
    LwU32 VoltListIdx = 0;
    vector<UINT08> vClkDomBuff;
    vector<UINT08> vVoltDomBuff;

    // Get all pstates info
    CHECK_RC(pLwRm->Control(m_hSubDev,
                           LW2080_CTRL_CMD_PERF_GET_PSTATES_INFO,
                           &getAllPStatesInfoParams,
                           sizeof(getAllPStatesInfoParams)));

    // Get the max stable frequencies of each clock (P0 freq)
    // Setup the parameters for querying details of P0
    getPStateInfoParams.pstate = LW2080_CTRL_PERF_PSTATES_P0;

    getPStateInfoParams.perfClkDomInfoListSize =
        Utility::CountBits(getAllPStatesInfoParams.perfClkDomains);

    getPStateInfoParams.perfVoltDomInfoListSize =
        Utility::CountBits(getAllPStatesInfoParams.perfVoltageDomains);

    vClkDomBuff.assign(sizeof(LW2080_CTRL_PERF_CLK_DOM_INFO) *
                              getPStateInfoParams.perfClkDomInfoListSize, 0);

    getPStateInfoParams.perfClkDomInfoList = LW_PTR_TO_LwP64(&vClkDomBuff[0]);

    vVoltDomBuff.assign(sizeof(LW2080_CTRL_PERF_VOLT_DOM_INFO) *
                               getPStateInfoParams.perfVoltDomInfoListSize, 0);

    getPStateInfoParams.perfVoltDomInfoList = LW_PTR_TO_LwP64(&vVoltDomBuff[0]);

    // set the Clock / voltage domain in each list
    for (UINT32 BitNum = 0; BitNum < 32; BitNum++)
    {
        UINT32 BitMask = 1<<BitNum;
        if (getAllPStatesInfoParams.perfClkDomains & BitMask)
        {
            pClkInfoList = (LW2080_CTRL_PERF_CLK_DOM_INFO*)(&vClkDomBuff[0]);
            pClkInfoList[ClkListIdx].domain = BitMask;
            ClkListIdx++;
        }
        if (getAllPStatesInfoParams.perfVoltageDomains & BitMask)
        {
            pVoltInfoList = (LW2080_CTRL_PERF_VOLT_DOM_INFO*)(&vVoltDomBuff[0]);
            pVoltInfoList[VoltListIdx].domain = BitMask;
            VoltListIdx++;
        }
    }

    // Get the details of the p-state
    CHECK_RC(pLwRm->Control(m_hSubDev,
                            LW2080_CTRL_CMD_PERF_GET_PSTATE_INFO,
                            &getPStateInfoParams,
                            sizeof(getPStateInfoParams)));

    pClkInfoList =
        (LW2080_CTRL_PERF_CLK_DOM_INFO*)(getPStateInfoParams.perfClkDomInfoList);
    // Set the freq limits
    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Upper limits for all clk frequencies based on P0\n");
    for (LwU32 i = 0; i < getPStateInfoParams.perfClkDomInfoListSize; ++i)
    {
        Printf(Tee::PriHigh, "  Name : %13s, Mask : 0x%08x, Max Freq : %7d KHz\n",
               m_ClkUtil.GetClkDomainName(pClkInfoList[i].domain),
               (UINT32)(pClkInfoList[i].domain), (int)(pClkInfoList[i].freq));

        mClkFreqLUT[pClkInfoList[i].domain].minOperationalFreqKHz = GetXtalFreqKHz();
        mClkFreqLUT[pClkInfoList[i].domain].maxOperationalFreqKHz = pClkInfoList[i].freq;
    }

#endif
    Printf(Tee::PriHigh,"OneSource Frequency : %d KHZ\n",
           GetOneSrcFreqKHz());

    Printf(Tee::PriHigh,"Xtal Frequency : %d KHz\n",
           GetXtalFreqKHz());

    // Cache the original clock freqs
    CHECK_RC(GetProgrammableClocks(vOrigClkInfo));
    vSetClkInfo = vOrigClkInfo;

    CHECK_RC(TestRootClocks(vSetClkInfo));
    CHECK_RC(GetProgrammableClocks(vSetClkInfo));

    CHECK_RC(TestDerivativeClocks(vSetClkInfo));
    CHECK_RC(GetProgrammableClocks(vSetClkInfo));

    CHECK_RC(TestCoreClocks(vSetClkInfo));

    return rc;
}

//!
//!@brief Tests the Root clks (clks without a dedicated PLL)
//!
//! It regresses the path of root clocks by switching the clk source from XTAL
//! to SPPLLs and vice-versa. It restores the original frequency of the root
//! clocks after completion.
//!
//!@param vClkInfo Vector contianing the list of clocks with their current freq
//!                from which to test only the root clocks
//!
//!@return OK if the tests passed, specific RC if failed.
//!----------------------------------------------------------------------------
RC ClockUtilGF100::TestRootClocks(vector<LW2080_CTRL_CLK_INFO>& vClkInfo)
{
    RC rc;
    vector<LW2080_CTRL_CLK_INFO> vNewClks;
    map<LwU32, LwU32> mOrigClkFreq;
    vector<LW2080_CTRL_CLK_INFO>::iterator it;

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Set the new frequency for all the Root clocks\n");
    Printf(Tee::PriHigh, "******************************************************************\n");

    it = vClkInfo.begin();
    while (it != vClkInfo.end())
    {
        // If not a Root Clock , dont change.
        if (!clkIsRootClock(it->clkDomain))
        {
            it = vClkInfo.erase(it);
            if (it == vClkInfo.end())
            {
                break;
            }
            continue;
        }

        // Store the initial frequency of the root clk before changing it
        mOrigClkFreq[it->clkDomain] = it->actualFreq;

        it->flags = 0;
        it++;
    }

    // Bail if there is not even one clk to be set
    if (!vClkInfo.size())
    {
        Printf(Tee::PriHigh,"No Root clocks found...exiting\n");
        return rc;
    }

#if !LWOS_IS_WINDOWS

    Printf(Tee::PriHigh, "Set all the Root clocks to the XTAL Input\n");
    Printf(Tee::PriHigh, "\n");

    for (it = vClkInfo.begin(); it != vClkInfo.end(); it++)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(it->clkDomain);

        //
        // First Set all the root clocks to Xtal
        // 27Mhz , this freq can be reached through OneSrc Clocks hence,
        // bound to switch the source to Xtal if not yet.
        //
        it->targetFreq = GetXtalFreqKHz();
        it->clkSource  = LW2080_CTRL_CLK_SOURCE_DEFAULT;

        Printf(Tee::PriHigh, "Programming %s to Frequency %d\n",
               domainName, it->targetFreq);
    }

    CHECK_RC(ProgramAndVerifyClockFreq(vClkInfo, vNewClks));

    Printf(Tee::PriHigh, "\n");
    // Verify the Clock Source..
    for (it = vNewClks.begin(); it != vNewClks.end(); it++)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(it->clkDomain);

        if (it->clkSource == LW2080_CTRL_CLK_SOURCE_XTAL)
        {
            Printf(Tee::PriHigh, "Successfully switched the source for %s to XTAL \n", domainName);
        }
        else
        {
            Printf(Tee::PriHigh, "Failed to switch the source for %s to XTAL \n", domainName);
            rc = RC::LWRM_ERROR;
            return rc;
        }
    }

#endif

    // Setting the source back to One Src Clocks.
    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Set the new Source for all the Root clocks as OneSrc(SPPLL)\n");

    for (it = vClkInfo.begin(); it != vClkInfo.end(); it++)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(it->clkDomain);

        //
        // Programming to a frequency which is a perfect divisor of
        // One Src Clock freq( = 1620 MHz) should switch the clocks to ALT path.
        // Use the last freq set by VBIOS as the starting ref freq.
        // Set it to the closest Alt path frequency from the present state.
        //
        clkSelectTargetFreq(LW_FALSE, it->clkDomain, mOrigClkFreq[it->clkDomain], it->targetFreq);

        Printf(Tee::PriHigh, "Programming %s to Frequency %d\n", domainName, (int)it->targetFreq);
    }

    Printf(Tee::PriHigh, "\n");
    CHECK_RC(ProgramAndVerifyClockFreq(vClkInfo, vNewClks));

    Printf(Tee::PriHigh, "\n");
    // Verify the Clock Source..
    for (it = vNewClks.begin(); it != vNewClks.end(); it++)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(it->clkDomain);

        if (clkIsOneSrc(it->clkSource))
        {
            Printf(Tee::PriHigh, "Successfully switched the source for %s to OneSrc \n", domainName);
        }
        else
        {
            Printf(Tee::PriHigh, "Failed to switch the source for %s to OneSrc \n", domainName);
            rc = RC::LWRM_ERROR;
            return rc;
        }
    }

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Restoring original frequencies for Root clocks\n");

    for (it = vClkInfo.begin(); it != vClkInfo.end(); it++)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(it->clkDomain);
        it->targetFreq = mOrigClkFreq[it->clkDomain];
        Printf(Tee::PriHigh, "Programming %s to Frequency %d\n",
               domainName, it->targetFreq);
    }

    Printf(Tee::PriHigh, "\n");
    CHECK_RC(ProgramAndVerifyClockFreq(vClkInfo, vNewClks));

    return rc;
}

//!
//!@brief Regress the clk path for all sys-derivative clks
//!
//! Regress the path for the SYS-Derived Clocks as follows:-
//! 1) Switch to ALT Path.
//! 2) While being in ALT Path switch to some other frequency.
//! 3) Switch to Ref path .
//! 4) Switch to some other frequency while being in Ref path.
//! 5) Restore all the clocks to their original frequencies.
//! 6) Perform a div sel check both for ref and bypass path for derivative clks
//!
//!@param vClkInfo Vector contianing the list of clocks with their current freq
//!                from which to test only the sys-derived clocks
//!
//!@return OK if the tests passed, specific RC if failed.
//!----------------------------------------------------------------------------
RC ClockUtilGF100::TestDerivativeClocks(vector<LW2080_CTRL_CLK_INFO>& vClkInfo)
{
    RC rc;
    vector<LW2080_CTRL_CLK_INFO> vNewClks;
    map<LwU32, LwU32> mOrigClkFreq;
    vector<LW2080_CTRL_CLK_INFO>::iterator it;
    LwU32 sysPllFreqKHz = 0;

    CHECK_RC(GetClkSrcFreqKHz(LW2080_CTRL_CLK_SOURCE_SYSPLL, sysPllFreqKHz));

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Set the new frequency for all the Derived clocks\n");
    Printf(Tee::PriHigh, "******************************************************************\n");

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Current freq of SysPll : %d KHz\n", sysPllFreqKHz);

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Switch all the SYS-Derived clocks to closest ALT Path\n");

    it = vClkInfo.begin();
    while (it != vClkInfo.end())
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(it->clkDomain);

        // If not a Derived Clock , dont change.
        if (!clkIsDerivativeClock(it->clkDomain))
        {
            it = vClkInfo.erase(it);
            if (it == vClkInfo.end())
            {
                break;
            }
            continue;
        }

        // Store the initial frequency of the derived clk before changing it
        mOrigClkFreq[it->clkDomain] = it->actualFreq;

        it->flags = 0;

        //
        // Programming to a frequency which is a perfect divisor of
        // One Src Clock freq( = 1620 MHz) should switch the clocks to ALT path.
        // Use the last freq set by VBIOS as the strating ref freq.
        // Set it to the closest Alt path frequency from the present state.
        //
        clkSelectTargetFreq(LW_FALSE, it->clkDomain, it->actualFreq, it->targetFreq);

        Printf(Tee::PriHigh, "Programming %s to Frequency %d\n",
               domainName, it->targetFreq);
        it++;
    }

    // Bail if there is not even one clk to be set
    if (!vClkInfo.size())
    {
        Printf(Tee::PriHigh,"No Derivative clocks found...exiting\n");
        return rc;
    }
    CHECK_RC(ProgramAndVerifyClockFreq(vClkInfo, vNewClks));

    // Verify the Clock Source..
    CHECK_RC(VerifyClockSrc(vNewClks, LW_FALSE));

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Switch all the SYS-Derived Clocks to some other freq while in  ALT Path\n");

    for (LwU32 i = 0; i < vClkInfo.size(); ++i)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(vClkInfo[i].clkDomain);

        //
        // Programming to a frequency which is a perfect divisor of
        // One Src Clock freq( = 1620 MHz) should switch the clocks to ALT path.
        // Set it to the closest Alt path frequency from the present state.
        //
        clkSelectTargetFreq(LW_FALSE, vClkInfo[i].clkDomain, vNewClks[i].actualFreq, vClkInfo[i].targetFreq);

        Printf(Tee::PriHigh, "Programming %s to Frequency %d\n", domainName, vClkInfo[i].targetFreq);
    }

    Printf(Tee::PriHigh, "\n");
    CHECK_RC(ProgramAndVerifyClockFreq(vClkInfo, vNewClks));

    Printf(Tee::PriHigh, "\n");
    // Verify the Clock Source..
    CHECK_RC(VerifyClockSrc(vNewClks, LW_FALSE));

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Switch all the SYS-Derived Clocks to REF Path\n");
    for (LwU32 i = 0; i < vClkInfo.size(); ++i)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(vClkInfo[i].clkDomain);

        // Force REF path
        vClkInfo[i].clkSource = LW2080_CTRL_CLK_SOURCE_DEFAULT;
        vClkInfo[i].flags = 0;
        vClkInfo[i].flags = FLD_SET_DRF(2080, _CTRL_CLK_INFO_FLAGS, _FORCE_PLL,
                                _ENABLE, vClkInfo[i].flags);

        //
        // Programming to a frequency much between two divisors of
        // OneSrc Freq should switch it to Ref path.
        //
        vClkInfo[i].targetFreq = sysPllFreqKHz / 4;

        Printf(Tee::PriHigh, "Programming %s to Frequency %d\n", domainName, vClkInfo[i].targetFreq);
    }

    Printf(Tee::PriHigh, "\n");
    CHECK_RC(ProgramAndVerifyClockFreq(vClkInfo, vNewClks));

    Printf(Tee::PriHigh, "\n");
    // Verify the Clock Source..
    CHECK_RC(VerifyClockSrc(vNewClks, LW_TRUE));

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Switch all the SYS-Derived Clocks to some other freq while still being in REF Path\n");

    for (LwU32 i = 0; i < vClkInfo.size(); ++i)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(vClkInfo[i].clkDomain);

        // Force REF path
        vClkInfo[i].clkSource = LW2080_CTRL_CLK_SOURCE_DEFAULT;
        vClkInfo[i].flags = 0;
        vClkInfo[i].flags = FLD_SET_DRF(2080, _CTRL_CLK_INFO_FLAGS, _FORCE_PLL,
                                _ENABLE, vClkInfo[i].flags);

        //
        // Programming to a frequency much between two divisors of
        // OneSrc Freq should switch it to Ref path.
        //
        vClkInfo[i].targetFreq = sysPllFreqKHz / 5;

        Printf(Tee::PriHigh, "Programming %s to Frequency %d\n", domainName, vClkInfo[i].targetFreq);
    }

    Printf(Tee::PriHigh, "\n");
    CHECK_RC(ProgramAndVerifyClockFreq(vClkInfo, vNewClks));

    Printf(Tee::PriHigh, "\n");
    // Verify the Clock Source..
    CHECK_RC(VerifyClockSrc(vNewClks, LW_TRUE));

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Switch all the SYS-Derived clocks  back to ALT Path\n");

    for (LwU32 i = 0; i < vClkInfo.size(); ++i)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(vClkInfo[i].clkDomain);
        vClkInfo[i].flags = 0;

        //
        // Programming to a frequency which is a perfect divisor of
        // One Src Clock freq( = 1620 MHz) should switch the clocks to ALT path.
        // Set it to the closest Alt path frequency from the present state.
        //
        clkSelectTargetFreq(LW_FALSE, vClkInfo[i].clkDomain, vNewClks[i].actualFreq, vClkInfo[i].targetFreq);

        Printf(Tee::PriHigh, "Programming %s to Frequency %d\n", domainName, vClkInfo[i].targetFreq);
    }

    Printf(Tee::PriHigh, "\n");
    CHECK_RC(ProgramAndVerifyClockFreq(vClkInfo, vNewClks));

    Printf(Tee::PriHigh, "\n");
    // Verify the Clock Source..
    CHECK_RC(VerifyClockSrc(vNewClks, LW_FALSE));

    Printf(Tee::PriHigh, "*********************** DIV VERIF START ******************************************************\n");

    // Alt path freq not to exceed SysPll freq
    LwU32 minBypassDiv = static_cast<LwU32>(ceil(GetOneSrcFreqKHz() * 2.0 /
                                                 max<LwU32>(1, sysPllFreqKHz)));

    for (LwU32 index = 3; index < 63 ; index++)
    {
        if (index < minBypassDiv || ((index & 0x01) && (index > 33)))
            continue;

        Printf(Tee::PriHigh, "\n");
        Printf(Tee::PriHigh, "Performing DIV_SEL check on ALT Path\n");
        Printf(Tee::PriHigh, "Switch all the SYS-Derived Clocks to OneSrc freq while in ALT Path\n");

        for (LwU32 i = 0; i < vClkInfo.size(); ++i)
        {
            const char * const domainName = m_ClkUtil.GetClkDomainName(vClkInfo[i].clkDomain);
            vClkInfo[i].flags = 0;

            Printf(Tee::PriHigh, "ALT PATH divVal2X to be verified for clock  %s is  %d\n", domainName, (int)index);

            vClkInfo[i].targetFreq = (GetOneSrcFreqKHz() * 2) / index;

            Printf(Tee::PriHigh, "Programming %s to Frequency %d\n", domainName,vClkInfo[i].targetFreq);
        }

        Printf(Tee::PriHigh, "\n");
        CHECK_RC(ProgramAndVerifyClockFreq(vClkInfo, vNewClks));

        Printf(Tee::PriHigh, "\n");

        // Verify the Clock Source..
        CHECK_RC(VerifyClockSrc(vNewClks, LW_FALSE));
    }

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Verify the Div Value for SYS-Derived Clocks in REF Path\n");

    for (LwU32 index = 4; index < 63 ; index+=2)
    {
        if ((index & 0x01) && (index > 33))
            continue;

        Printf(Tee::PriHigh, "\n");
        Printf(Tee::PriHigh, "Performing DIV_SEL check on REF PAth\n");
        Printf(Tee::PriHigh, "Switch all the SYS-Derived Clocks to some freq in REF Path\n");

        for (LwU32 i = 0; i < vClkInfo.size(); ++i)
        {
            const char * const domainName = m_ClkUtil.GetClkDomainName(vClkInfo[i].clkDomain);
            vClkInfo[i].clkSource = LW2080_CTRL_CLK_SOURCE_DEFAULT;
            vClkInfo[i].flags = 0;
            vClkInfo[i].flags = FLD_SET_DRF(2080, _CTRL_CLK_INFO_FLAGS, _FORCE_PLL,
                                                           _ENABLE, vClkInfo[i].flags);

            Printf(Tee::PriHigh, "REF PATH divVal2X to be verified for clock %s is  %d\n", domainName, (int)index);

            vClkInfo[i].targetFreq = (sysPllFreqKHz * 2) / index;

            Printf(Tee::PriHigh, "Programming %s to Frequency %d\n", domainName,vClkInfo[i].targetFreq);
        }

        Printf(Tee::PriHigh, "\n");
        CHECK_RC(ProgramAndVerifyClockFreq(vClkInfo, vNewClks));

        Printf(Tee::PriHigh, "\n");

        // Verify the Clock Source..
        CHECK_RC(VerifyClockSrc(vNewClks, LW_TRUE));
    }

    Printf(Tee::PriHigh, "*********************** DIV VERIF END ******************************************************\n");

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Restoring the original frequency for the SYS-Derived clocks\n");

    for (it = vClkInfo.begin(); it != vClkInfo.end(); it++)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(it->clkDomain);
        it->targetFreq = mOrigClkFreq[it->clkDomain];
        it->flags = 0;

        Printf(Tee::PriHigh, "Programming %s to Frequency %d\n",
               domainName, it->targetFreq);
    }

    Printf(Tee::PriHigh, "\n");
    CHECK_RC(ProgramAndVerifyClockFreq(vClkInfo, vNewClks));

    return rc;
}

//!
//!@brief Regresses the path of core clocks
//!
//! Regress the paths for Non-Root and Non-Sys Derived Clks as follows:-
//! 1) Switch to ALT Path.
//! 2) While being in ALT Path switch to some other frequency.
//! 3) Switch to Ref path .
//! 4) Switch to some other frequency while being in Ref path.
//! 5) Restore all the cloks to their original frequencies.
//!
//!@param vClkInfo Vector contianing the list of clocks with their current freq
//!                from which to test only the core clocks
//!
//!@return OK if the tests passed, specific RC if failed.
//!----------------------------------------------------------------------------
RC ClockUtilGF100::TestCoreClocks(vector<LW2080_CTRL_CLK_INFO>& vClkInfo)
{
    RC rc;
    vector<LW2080_CTRL_CLK_INFO> vNewClks;
    map<LwU32, LwU32> mOrigClkFreq;
    vector<LW2080_CTRL_CLK_INFO>::iterator it;

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Set the new frequency for all the Core clocks\n");
    Printf(Tee::PriHigh, "******************************************************************\n");

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Switch all the Core clocks to ALT Path\n");

    it = vClkInfo.begin();
    while (it != vClkInfo.end())
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(it->clkDomain);

        // If not a Core Clock , dont change.
        if (clkIsRootClock(it->clkDomain) ||
            clkIsDerivativeClock(it->clkDomain))
        {
            it = vClkInfo.erase(it);
            if (it == vClkInfo.end())
            {
                break;
            }
            continue;
        }

        // Store the initial frequency of the core clk before changing it
        mOrigClkFreq[it->clkDomain] = it->actualFreq;

        // Clear the source/flags override.
        it->flags = 0;
        it->clkSource = LW2080_CTRL_CLK_SOURCE_DEFAULT;

        // Force bypass
        it->flags = FLD_SET_DRF(2080, _CTRL_CLK_INFO_FLAGS, _FORCE_BYPASS,
                                      _ENABLE, it->flags);

        //
        // The frequency in the bypass path should not exceed the upper limit
        // provided such a limit exists
        //
        if (LW2080_CTRL_CLK_DOMAIN_MCLK != it->clkDomain &&
            mClkFreqLUT[it->clkDomain].maxOperationalFreqKHz)
        {
            LwU32 minGenClkBypassDiv = (LwU32)ceil((LwF64)(GetOneSrcFreqKHz() * 2) /
                (LwF64)mClkFreqLUT[it->clkDomain].maxOperationalFreqKHz);

            it->targetFreq = (GetOneSrcFreqKHz() * 2) / ++minGenClkBypassDiv;
        }
        else
        {
            //
            // Programming to a frequency which is a perfect divisor of
            // One Src Clock freq( = 1620 MHz) should switch the clocks to ALT path.
            // Set it to the closest Alt path frequency from the present state.
            //

            clkSelectTargetFreq(LW_FALSE, it->clkDomain, it->actualFreq / 4,
                                it->targetFreq);
        }

        Printf(Tee::PriHigh, "Programming %s to Frequency %d\n",
               domainName, it->targetFreq);
        it++;
    }

    // Bail if there is not even one clk to be set
    if (!vClkInfo.size())
    {
        Printf(Tee::PriHigh,"No Core clocks found...exiting\n");
        return rc;
    }

    Printf(Tee::PriHigh, "\n");
    CHECK_RC(ProgramAndVerifyClockFreq(vClkInfo, vNewClks));

    Printf(Tee::PriHigh, "\n");
    // Verify the Clock Source..
    CHECK_RC(VerifyClockSrc(vNewClks, LW_FALSE));

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Switch all the Core Clocks to some other freq while in ALT Path\n");

    for (LwU32 i = 0; i < vClkInfo.size(); ++i)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(vClkInfo[i].clkDomain);

        //
        // The frequency in the bypass path should not exceed the upper limit
        // provided such a limit exists
        //
        if (LW2080_CTRL_CLK_DOMAIN_MCLK != vClkInfo[i].clkDomain &&
            mClkFreqLUT[vClkInfo[i].clkDomain].maxOperationalFreqKHz)
        {
            LwU32 minGenClkBypassDiv = (LwU32)ceil((LwF64)(GetOneSrcFreqKHz() * 2) /
                (LwF64)mClkFreqLUT[vClkInfo[i].clkDomain].maxOperationalFreqKHz);

            vClkInfo[i].targetFreq = (GetOneSrcFreqKHz() * 2) / ++minGenClkBypassDiv;
        }
        else
        {
            //
            // Programming to a frequency which is a perfect divisor of
            // One Src Clock freq( = 1620 MHz) should switch the clocks to ALT path.
            // Set it to the closest Alt path frequency from the present state.
            //

            clkSelectTargetFreq(LW_FALSE, vClkInfo[i].clkDomain, vNewClks[i].actualFreq, vClkInfo[i].targetFreq);
        }

        Printf(Tee::PriHigh, "Programming %s to Frequency %d\n", domainName, (int)vClkInfo[i].targetFreq);
    }

    Printf(Tee::PriHigh, "\n");
    CHECK_RC(ProgramAndVerifyClockFreq(vClkInfo, vNewClks));

    Printf(Tee::PriHigh, "\n");
    // Verify the Clock Source..
    CHECK_RC(VerifyClockSrc(vNewClks, LW_FALSE));

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Switch all the Core Clocks to REF Path\n");

    for (LwU32 i = 0; i < vClkInfo.size(); ++i)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(vClkInfo[i].clkDomain);

        //
        // Programming to a frequency much between two divisors of
        // OneSrc Freq should switch it to Ref path.
        // Set it to the closest Ref path frequency from the present state.
        // Clear the FORCE_BYPASS flag previously set.
        //
        vClkInfo[i].clkSource = LW2080_CTRL_CLK_SOURCE_DEFAULT;
        vClkInfo[i].flags = FLD_SET_DRF(2080, _CTRL_CLK_INFO_FLAGS, _FORCE_BYPASS,
                                        _DISABLE, vClkInfo[i].flags);

        clkSelectTargetFreq(LW_TRUE, vClkInfo[i].clkDomain, vNewClks[i].actualFreq, vClkInfo[i].targetFreq);

        Printf(Tee::PriHigh, "Programming %s to Frequency %d\n", domainName, vClkInfo[i].targetFreq);
    }

    Printf(Tee::PriHigh, "\n");
    CHECK_RC(ProgramAndVerifyClockFreq(vClkInfo, vNewClks));

    Printf(Tee::PriHigh, "\n");
    // Verify the Clock Source..
    for (it = vNewClks.begin(); it != vNewClks.end(); it++)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(it->clkDomain);
        LwU32 ramType = GetRamType();

        // For GDDR5, MCLK is not expected to be accurate when below 500MHz.
        if ((LW2080_CTRL_CLK_DOMAIN_MCLK == it->clkDomain) &&
            ((LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR5  == ramType) ||
             (LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR5X == ramType) ||
             (LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR6  == ramType) ||
             (LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR6X == ramType)))
        {
            Printf(Tee::PriHigh, "%d: No need to check for path on %s\n", __LINE__, domainName);
        }
        else if ((clkIsOneSrc(it->clkSource) ||
                 (it->clkSource == LW2080_CTRL_CLK_SOURCE_XTAL))
                     &&
                 //
                 // Clocks 2.0 denotes that the dedicated PLL is used via its flags as opposed
                 // to the clock source. This condition also holds true for Clocks 1.0 since it
                 // zeroes out flags.
                 //
                 !FLD_TEST_DRF(2080, _CTRL_CLK_INFO_FLAGS, _PATH, _PLL, it->flags))
        {
            Printf(Tee::PriHigh, "%d: Failed to switch %s to REF Path\n", __LINE__, domainName);
            rc = RC::LWRM_ERROR;
            return rc;
        }
        else
        {
            Printf(Tee::PriHigh, "%d: Successfully switched %s to REF Path\n", __LINE__, domainName);
        }
    }

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Switch all the Core Clocks to some other freq while still being in REF Path\n");

    for (LwU32 i = 0; i < vClkInfo.size(); ++i)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(vClkInfo[i].clkDomain);

        // Force REF path
        vClkInfo[i].clkSource = LW2080_CTRL_CLK_SOURCE_DEFAULT;
        vClkInfo[i].flags = 0;
        vClkInfo[i].flags = FLD_SET_DRF(2080, _CTRL_CLK_INFO_FLAGS, _FORCE_PLL,
                                _ENABLE, vClkInfo[i].flags);

        //
        // Programming to a frequency much between two divisors of
        // OneSrc Freq should switch it to Ref path.
        // Set it to the closest Ref path frequency from the present state.
        //
        clkSelectTargetFreq(LW_TRUE, vClkInfo[i].clkDomain, vNewClks[i].actualFreq,vClkInfo[i].targetFreq);

        Printf(Tee::PriHigh, "Programming %s to Frequency %d\n", domainName, vClkInfo[i].targetFreq);
    }

    Printf(Tee::PriHigh, "\n");
    CHECK_RC(ProgramAndVerifyClockFreq(vClkInfo, vNewClks));

    Printf(Tee::PriHigh, "\n");
    // Verify the Clock Source..
    CHECK_RC(VerifyClockSrc(vNewClks, LW_TRUE));

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Switch all the Core clocks back to ALT Path\n");

    for (LwU32 i = 0; i < vClkInfo.size(); ++i)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(vClkInfo[i].clkDomain);

        vClkInfo[i].flags = 0;
        vClkInfo[i].flags = FLD_SET_DRF(2080, _CTRL_CLK_INFO_FLAGS, _FORCE_BYPASS,
                                        _ENABLE, vClkInfo[i].flags);
        vClkInfo[i].clkSource = LW2080_CTRL_CLK_SOURCE_DEFAULT;

        //
        // The frequency in the bypass path should not exceed the upper limit
        // provided such a limit exists
        //
        if (LW2080_CTRL_CLK_DOMAIN_MCLK != vClkInfo[i].clkDomain &&
            mClkFreqLUT[vClkInfo[i].clkDomain].maxOperationalFreqKHz)
        {
            LwU32 minGenClkBypassDiv = (LwU32)ceil((LwF64)(GetOneSrcFreqKHz() * 2) /
                (LwF64)mClkFreqLUT[vClkInfo[i].clkDomain].maxOperationalFreqKHz);

            vClkInfo[i].targetFreq = (GetOneSrcFreqKHz() * 2) / ++minGenClkBypassDiv;
        }
        else
        {
            //
            // Programming to a frequency which is a perfect divisor of
            // One Src Clock freq( = 1620 MHz) should switch the clocks to ALT path.
            // Set it to the closest Alt path frequency from the present state.
            //
            clkSelectTargetFreq(LW_FALSE, vClkInfo[i].clkDomain, vNewClks[i].actualFreq, vClkInfo[i].targetFreq);
        }

        Printf(Tee::PriHigh, "Programming %s to Frequency %d\n", domainName, vClkInfo[i].targetFreq);
    }
    Printf(Tee::PriHigh, "\n");
    CHECK_RC(ProgramAndVerifyClockFreq(vClkInfo, vNewClks));

    Printf(Tee::PriHigh, "\n");
    // Verify the Clock Source..
    CHECK_RC(VerifyClockSrc(vNewClks, LW_FALSE));

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Restoring the original frequency for the Core clocks\n");

    // Restore the old values
    for (it = vClkInfo.begin(); it != vClkInfo.end(); it++)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(it->clkDomain);

        // Clear previous flags if set.
        it->flags = 0;
        it->targetFreq = mOrigClkFreq[it->clkDomain];
        Printf(Tee::PriHigh, "Programming %s to Frequency %d\n",
               domainName, it->targetFreq);
    }

    Printf(Tee::PriHigh, "\n");
    CHECK_RC(ProgramAndVerifyClockFreq(vClkInfo, vNewClks));

    return rc;
}
