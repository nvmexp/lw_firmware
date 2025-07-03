/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file clkbase.cpp
//! \brief Actual implementation of base clk hal
//!

#include "clkbase.h"
#include "core/include/memcheck.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"

ClockUtil::ClockUtil(LwU32 hClient, LwU32 hDev, LwU32 hSubDev): RMTHalModule(hClient, hDev, hSubDev)
{
    m_ProgClkDomains = 0;
    m_AllClkDomains = 0;
}

ClockUtil::~ClockUtil()
{
}

ClockUtil * ClockUtil::CreateModule(LwU32 hClient, GpuSubdevice* pSubdev)
{
    LwU32 hDev = LwRmPtr()->GetDeviceHandle(pSubdev->GetParentDevice());
    LwU32 hSubDev = LwRmPtr()->GetSubdeviceHandle(pSubdev);

    return new ClockUtilGK104(hClient, hDev, hSubDev);
}

//!
//! @brief Stub Function
//!
//! @param clkDomain
//!
//! @return LW_FALSE
//!----------------------------------------------------------------------------
LwBool ClockUtil::clkIsRootClock(LwU32 clkDomain)
{
    return LW_FALSE;
}

//!
//! @brief Stub Function
//!
//! @param clkDomain
//!
//! @return LW_FALSE
//!----------------------------------------------------------------------------
LwBool ClockUtil::clkIsDerivativeClock(LwU32 clkDomain)
{
    return LW_FALSE;
}

//!
//! @brief Checks if the given clkSrc is SPPLL0/1
//!
//! @param clkSrc
//!
//! @return Returns true if clkSrc is SPPLL0/1
//!----------------------------------------------------------------------------
LwBool ClockUtil::clkIsOneSrc(LwU32 clkSrc)
{
    if ((LW2080_CTRL_CLK_SOURCE_SPPLL0 == clkSrc) ||
        (LW2080_CTRL_CLK_SOURCE_SPPLL1 == clkSrc))
    {
        return LW_TRUE;
    }
    return LW_FALSE;
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
RC ClockUtil::ProgramAndVerifyClockFreq
(
    const vector<LW2080_CTRL_CLK_INFO>& vSetClkInfo,
    vector<LW2080_CTRL_CLK_INFO>& vNewClkInfo
)
{
    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_CLK_SET_INFO_PARAMS m_setInfoParams;

    // hook up clk info table
    m_setInfoParams.flags =
        LW2080_CTRL_CLK_SET_INFO_FLAGS_WHEN_IMMEDIATE;
    m_setInfoParams.clkInfoListSize = static_cast<LwU32>(vSetClkInfo.size());
    m_setInfoParams.clkInfoList = LW_PTR_TO_LwP64(&vSetClkInfo[0]);

    Printf(Tee::PriHigh, "Programming clocks to the target frequencies\n");

    // Set new clocks frequency
    CHECK_RC(pLwRm->Control(m_hSubDev,
                            LW2080_CTRL_CMD_CLK_SET_INFO,
                            &m_setInfoParams,
                            sizeof (m_setInfoParams)));

    CHECK_RC(VerifyClockFreq(vSetClkInfo, vNewClkInfo));

    return rc;
}

//!
//! @brief Verifies if the current clk frequencies are matching the target
//!        frequencies of the given clk domains
//!
//! @param vSetClkInfo Vector containing target frequencies
//! @param vNewClkInfo Vector containing current frequencies of all clocks
//!
//! @return OK if the verification passed, specific RC if failed
//!----------------------------------------------------------------------------
RC ClockUtil::VerifyClockFreq
(
    const vector<LW2080_CTRL_CLK_INFO>& vSetClkInfo,
    vector<LW2080_CTRL_CLK_INFO>& vNewClkInfo
)
{
    RC rc;
    LwU32 tolerance100X = 0;

    // Get the current frequencies of the clks we just programmed
    vNewClkInfo = vSetClkInfo;
    CHECK_RC(GetClocks(vNewClkInfo));

    for (LwU32 i = 0;i < vSetClkInfo.size(); ++i)
    {
        LwU32 actualClkFreq, expClkFreq;

        MASSERT(vSetClkInfo[i].clkDomain == vNewClkInfo[i].clkDomain);

        tolerance100X = GetFreqTolerance100X(vSetClkInfo[i]);

        Printf(Tee::PriHigh, "Clk Name : %13s,    ",
               m_ClkUtil.GetClkDomainName(vSetClkInfo[i].clkDomain));

        Printf(Tee::PriHigh,"Source : %10s,    ",
               m_ClkUtil.GetClkSourceName(vNewClkInfo[i].clkSource));

        expClkFreq = vSetClkInfo[i].targetFreq;
        Printf(Tee::PriHigh, "Expected Freq : %7d KHz, ",
               (UINT32)(expClkFreq));

        actualClkFreq = vNewClkInfo[i].actualFreq;
        Printf(Tee::PriHigh, "Actual Freq (SW) : %7d KHz, ",
               (UINT32)(actualClkFreq));

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
//!@brief Verifies that the given clks have been switched to the given clk path
//!
//!@param vClkInfo Vector containing the clk information
//!@param bIsTargetPathVCO True if the clks had to be switched to REF path
//!
//!@return OK if all the clks have switched as per target path, error otherwise
//!----------------------------------------------------------------------------
RC ClockUtil::VerifyClockSrc
(
    const vector<LW2080_CTRL_CLK_INFO>& vClkInfo,
    LwBool bIsTargetPathVCO
)
{
    RC rc;

    // Verify the Clock Source..
    for (LwU32 i = 0; i < vClkInfo.size(); ++i)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(vClkInfo[i].clkDomain);
        LwBool bIsLwrrentPathByp =
            ((clkIsOneSrc(vClkInfo[i].clkSource) ||
             (vClkInfo[i].clkSource == LW2080_CTRL_CLK_SOURCE_XTAL))
                &&
             //
             // Clocks 2.0 denotes that the dedicated PLL is used via its flags as opposed
             // to the clock source. This condition also holds true for Clocks 1.0 since it
             // zeroes out flags.
             //
             !FLD_TEST_DRF(2080, _CTRL_CLK_INFO_FLAGS, _PATH, _PLL, vClkInfo[i].flags));

        if (bIsTargetPathVCO ^ bIsLwrrentPathByp)
        {
            Printf(Tee::PriHigh, "Successfully programmed %s to %s Path\n",
                   domainName, (bIsTargetPathVCO ? "REF" : "ALT"));
        }
        else
        {
            Printf(Tee::PriHigh, "Failed to program %s to %s Path\n",
                   domainName, (bIsTargetPathVCO ? "REF" : "ALT"));
            rc = RC::LWRM_ERROR;
            return rc;
        }
    }
    return rc;
}

//!
//!@brief Verifies that the given clks have been switched to the given clk src
//!
//!@param vClkInfo Vector containing the clk information
//!@param clkSrc Clock Src to verify
//!
//!@return OK if all the clks have the given clkSrc as source, error otherwise
//!----------------------------------------------------------------------------
RC ClockUtil::VerifyClockSrc
(
    const vector<LW2080_CTRL_CLK_INFO>& vClkInfo,
    LwU32 clkSrc
)
{
    RC rc;

    // Verify the Clock Source..
    for (LwU32 i = 0; i < vClkInfo.size(); ++i)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(vClkInfo[i].clkDomain);
        const char * const sourceName = m_ClkUtil.GetClkSourceName(vClkInfo[i].clkSource);
        const char * const tgtSrcName = m_ClkUtil.GetClkSourceName(clkSrc);

        if (vClkInfo[i].clkSource == clkSrc)
        {
            Printf(Tee::PriHigh, "Verified : %s is sourced from %s\n",
                   domainName, sourceName);
        }
        else
        {
            Printf(Tee::PriHigh, "Verification FAILED :%s is sourced from %s"
                   " instead of %s\n", domainName, sourceName, tgtSrcName);
            rc = RC::LWRM_ERROR;
            return rc;
        }
    }
    return rc;
}

//!
//! @brief Used to callwlate the target freq of a given clk domain depending on
//!        its current clk frequency and target clk path
//!
//! @param bSwitchToVCOPath True if target freq is intended to be a VCO freq
//! @param clkDomain Domain for which this is to be callwlated
//! @param refFreqKHz Ref freq, usually the current freq of the clk
//! @param targetFreqKHz Reference to the target freq.
//!
//! @return OK if all went well, specific RC error otherwise
//!----------------------------------------------------------------------------
RC ClockUtil::clkSelectTargetFreq
(
    LwBool bSwitchToVCOPath,
    LwU32  clkDomain,
    LwU32  refFreqKHz,
    LwU32& targetFreqKHz
)
{
    //
    // Stub function - each arch will have to implement this
    // as the logic for choosing the target freq would vary accordingly.
    //
    return OK;
}

//!
//! @brief Returns the tolerance allowable while programming the given clk domain
//!
//! @param ClkInfo Given Clkdomains
//!
//! @return The % tolerance allowable , multipled by 100
//!----------------------------------------------------------------------------
LwU32 ClockUtil::GetFreqTolerance100X(const LW2080_CTRL_CLK_INFO ClkInfo)
{
    MASSERT(ClkInfo.clkDomain != LW2080_CTRL_CLK_DOMAIN_UNDEFINED);

    // Allow up to 15% tolerance on paths
    return 1500;
}

//!
//! @brief Used to get the mask of programmable clk domains
//!
//! @return Mask of programmable clk domains available
//!----------------------------------------------------------------------------
LwU32 ClockUtil::GetProgrammableDomains()
{
    if (!m_ProgClkDomains)
    {
        RC rc;
        LwRmPtr pLwRm;
        LW2080_CTRL_CLK_GET_DOMAINS_PARAMS getDomainsParams;

        // Get all Programmable Clock Domains.
        getDomainsParams.clkDomainsType = LW2080_CTRL_CLK_DOMAINS_TYPE_PROGRAMMABALE_ONLY;

        // get programable clk domains
        CHECK_RC(pLwRm->Control(m_hSubDev,
            LW2080_CTRL_CMD_CLK_GET_DOMAINS,
            &getDomainsParams, sizeof (getDomainsParams)));

        m_ProgClkDomains = getDomainsParams.clkDomains;

        // Programmable domains should not be empty!
        MASSERT(m_ProgClkDomains);
    }

    return m_ProgClkDomains;
}

//!
//! @brief Used to get the number of programmable clk domains
//!
//! @return Number of programmable clk domains
//!----------------------------------------------------------------------------
LwU32 ClockUtil::GetNumProgrammableDomains()
{
    return Utility::CountBits(GetProgrammableDomains());
}

//!
//! @brief Used to get the mask of all clk domains
//!
//! @return Mask of all clk domains available
//!----------------------------------------------------------------------------
LwU32 ClockUtil::GetAllDomains()
{
    if (!m_AllClkDomains)
    {
        RC rc;
        LwRmPtr pLwRm;
        LW2080_CTRL_CLK_GET_DOMAINS_PARAMS getDomainsParams;

        // Get all Programmable Clock Domains.
        getDomainsParams.clkDomainsType = LW2080_CTRL_CLK_DOMAINS_TYPE_ALL;

        // get programable clk domains
        CHECK_RC(pLwRm->Control(m_hSubDev,
            LW2080_CTRL_CMD_CLK_GET_DOMAINS,
            &getDomainsParams, sizeof (getDomainsParams)));

        m_AllClkDomains = getDomainsParams.clkDomains;

        // Clk domains should not be empty!
        MASSERT(m_AllClkDomains);
    }

    return m_AllClkDomains;
}

//!
//! @brief Used to get the number of clk domains
//!
//! @return Number of clk domains available
//!----------------------------------------------------------------------------
LwU32 ClockUtil::GetNumDomains()
{
    return Utility::CountBits(GetAllDomains());
}

//!
//! @brief Prints detailed information about given clk domains
//!
//! @param clkDomains Mask of clk domains whose information is to be printed
//!
//! @return OK if all went well, specific RC error otherwise
//!----------------------------------------------------------------------------
RC ClockUtil::PrintClocks(const LwU32 clkDomains)
{
    RC rc;
    vector<LW2080_CTRL_CLK_INFO> vClkInfo;

    CHECK_RC(GetClocks(clkDomains, vClkInfo));

    for (LwU32 i = 0; i < vClkInfo.size(); ++i)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(vClkInfo[i].clkDomain);
        const char * const sourceName = m_ClkUtil.GetClkSourceName(vClkInfo[i].clkSource);

        Printf(Tee::PriHigh,"Clock Domain is %s .\n", domainName);
        Printf(Tee::PriHigh,"Clock Source is %s. \n", sourceName);
        Printf(Tee::PriHigh,"Actual Frequency is  %d \n", (int)vClkInfo[i].actualFreq);
        Printf(Tee::PriHigh,"Target Frequency is  %d \n", (int)vClkInfo[i].targetFreq);
        Printf(Tee::PriHigh,"Flag Value is  %d \n", (LwU32)vClkInfo[i].flags);

        Printf(Tee::PriHigh,"\n");
    }
    return rc;
}

//!
//! @brief Used to get the current state of the given clk domains
//!
//! @param vClkInfo Vector containing the clk domains to be queried. The current
//!                 frequency of each domain is returned in this same vector.
//!
//! @return OK if the query succeeded, specific RC error otherwise
//!----------------------------------------------------------------------------
RC ClockUtil::GetClocks
(
    vector<LW2080_CTRL_CLK_INFO>& vClkInfo
)
{
    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_CLK_GET_INFO_PARAMS getInfoParams;

    if (!vClkInfo.size())
    {
        // Has to have at least 1 clk domain !
        return RC::BAD_PARAMETER;
    }

    getInfoParams.flags = 0;
    getInfoParams.clkInfoListSize = static_cast<LwU32>(vClkInfo.size());
    getInfoParams.clkInfoList =  LW_PTR_TO_LwP64(&vClkInfo[0]);

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Reading the set frequency for given clocks\n");
    Printf(Tee::PriHigh, "\n");

    // get New clocks
    CHECK_RC(pLwRm->Control(m_hSubDev,
                            LW2080_CTRL_CMD_CLK_GET_INFO,
                            &getInfoParams,
                            sizeof (getInfoParams)));
    return rc;
}

//!
//! @brief Used to get the current state of the given clk domains
//!
//! @param clkDomains Mask of clk domains for which freq is to be queried
//! @param vClkInfo Vector in which information about the given clks is returned
//!
//! @return OK if the query succeeded, specific RC error otherwise
//!----------------------------------------------------------------------------
RC ClockUtil::GetClocks
(
    const LwU32 clkDomains,
    vector<LW2080_CTRL_CLK_INFO>& vClkInfo
)
{
    RC rc;

    vClkInfo.clear();

    for (LwU32 i = 0 ; i < 32; ++i)
    {
        if (BIT(i) & clkDomains)
        {
            LW2080_CTRL_CLK_INFO clkInfo = {0};
            clkInfo.clkDomain = BIT(i);
            vClkInfo.push_back(clkInfo);
        }
    }

    CHECK_RC(GetClocks(vClkInfo));

    return rc;
}

//!
//! @brief Gets the effective state of the given clock domains. "Effective"
//!        means that if any slowdowns are in effect for a clock domain, the
//!        slowed-down frequency will be returned instead of the programmed
//!        frequency.
//!
//! @param vExtendedClkInfo     Vector containing the clk domains to be
//!                             queried. The effective frequency of each
//!                             domain is returned in this same vector.
//!
//! @return OK if the query succeeded, specific RC error otherwise
//!----------------------------------------------------------------------------
RC ClockUtil::GetEffectiveClocks
(
    vector<LW2080_CTRL_CLK_EXTENDED_INFO>& vExtendedClkInfo
)
{
    LW2080_CTRL_CLK_GET_EXTENDED_INFO_PARAMS getExtendedInfoParams;
    LwRmPtr pLwRm;
    LwU32   i;
    RC      rc;

    // Must give at least one clock domain.
    if (!vExtendedClkInfo.size())
    {
        return RC::BAD_PARAMETER;
    }

    // Setup the parameters for the control call.
    getExtendedInfoParams.flags = 0;
    getExtendedInfoParams.numClkInfos = static_cast<LwU32>(vExtendedClkInfo.size());
    MASSERT(getExtendedInfoParams.numClkInfos <
            LW2080_CTRL_CLK_MAX_EXTENDED_INFOS);
    for (i=0; i<getExtendedInfoParams.numClkInfos; i++)
    {
        getExtendedInfoParams.clkInfos[i] = vExtendedClkInfo[i];
    }

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Reading the effective frequency for given clocks\n");
    Printf(Tee::PriHigh, "\n");

    // Get the effective clock state.
    CHECK_RC(pLwRm->Control(m_hSubDev,
                            LW2080_CTRL_CMD_CLK_GET_EXTENDED_INFO,
                            &getExtendedInfoParams,
                            sizeof (getExtendedInfoParams)));

    for (i=0; i<getExtendedInfoParams.numClkInfos; i++)
    {
        vExtendedClkInfo[i] = getExtendedInfoParams.clkInfos[i];
    }

    return rc;
}

//!
//! @brief Gets the effective state of the given clock domains. "Effective"
//!        means that if any slowdowns are in effect for a clock domain, the
//!        slowed-down frequency will be returned instead of the programmed
//!        frequency.
//!
//! @param clkDomains           Bitmask of clk domains to be queried.
//! @param vExtendedClkInfo     Vector that will return the effective frequency
//!                             of each queried domain.
//!
//! @return OK if the query succeeded, specific RC error otherwise
//!----------------------------------------------------------------------------
RC ClockUtil::GetEffectiveClocks
(
    const LwU32 clkDomains,
    vector<LW2080_CTRL_CLK_EXTENDED_INFO>& vExtendedClkInfo
)
{
    LwU32 i;
    RC    rc;

    vExtendedClkInfo.clear();

    for (i = 0; i < LW2080_CTRL_CLK_MAX_EXTENDED_INFOS; i++)
    {
        if (BIT(i) & clkDomains)
        {
            LW2080_CTRL_CLK_EXTENDED_INFO extendedClkInfo;
            memset(&extendedClkInfo, 0, sizeof(extendedClkInfo));
            extendedClkInfo.clkInfo.clkDomain = BIT(i);
            vExtendedClkInfo.push_back(extendedClkInfo);
        }
    }

    CHECK_RC(GetEffectiveClocks(vExtendedClkInfo));

    return rc;
}

//!
//! @brief Return information about all clk domains
//!
//! @param vClkInfo Vector contianing the information about all available clks
//!
//! @return OK if the query succeeded, specific RC error otherwise
//!----------------------------------------------------------------------------
RC ClockUtil::GetAllClocks(vector<LW2080_CTRL_CLK_INFO>& vClkInfo)
{
    RC rc;

    CHECK_RC(GetClocks(GetAllDomains(), vClkInfo));

    return rc;
}

//!
//! @brief Return information about programmable clk domains
//!
//! @param vClkInfo Vector contianing the information about all programmable clks
//!
//! @return OK if the query succeeded, specific RC error otherwise
//!----------------------------------------------------------------------------
RC ClockUtil::GetProgrammableClocks(vector<LW2080_CTRL_CLK_INFO>& vClkInfo)
{
    RC rc;

    CHECK_RC(GetClocks(GetProgrammableDomains(), vClkInfo));

    return rc;
}

//!
//! @brief Return the current freq of the given clkSrc
//!
//! @param clkSrc The clkSrc whose freq is to be queried
//! @param clkSrcFreqKHz The current freq of the given clksrc is returned here
//!
//! @return OK if the query succeeded, specific RC error otherwise
//!----------------------------------------------------------------------------
RC ClockUtil::GetClkSrcFreqKHz(LwU32 clkSrc, LwU32& clkSrcFreqKHz)
{
    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_CLK_GET_SRC_FREQ_KHZ_V2_PARAMS getSrcFreqParams = { };
    LW2080_CTRL_CLK_INFO *clkInfo = getSrcFreqParams.clkInfoList;

    clkInfo[0].clkSource = clkSrc;
    getSrcFreqParams.clkInfoListSize = 1;

    // get New clocks
    CHECK_RC(pLwRm->Control(m_hSubDev,
                            LW2080_CTRL_CMD_CLK_GET_SRC_FREQ_KHZ_V2,
                            &getSrcFreqParams,
                            sizeof (getSrcFreqParams)));

    clkSrcFreqKHz = clkInfo[0].actualFreq;
    return rc;
}

//!
//! @brief Stub function for clock path test, which is architecture specific
//!
//! @return OK
//!----------------------------------------------------------------------------
RC ClockUtil::ClockPathTest()
{
    return OK;
}

//!
//! @brief Stub function for clock sanity test
//!
//! @return OK
//!----------------------------------------------------------------------------
RC ClockUtil::ClockSanityTest()
{
    return OK;
}

//!
//! @brief Stub function for clock ndiv slowdown test
//!
//! @return OK
//!----------------------------------------------------------------------------
RC ClockUtil::ClockNDivSlowdownTest()
{
    return OK;
}

//!
//!@brief Retrieves operating limits and attributes of the given PLL.
//!
//!@return Specific RC if failure.
//!        OK otherwise.
//!----------------------------------------------------------------------------
RC ClockUtil::clkGetPllInfo
(
    LwU32 pll,
    LW2080_CTRL_CLK_PLL_INFO &pllInfo
)
{
    LW2080_CTRL_CLK_GET_PLL_INFO_PARAMS params;
    LW2080_CTRL_CLK_PLL_INFO           *pPllInfo;
    LwRmPtr pLwRm;
    RC      rc;

    // Retrieve operating limits and attributes of the given PLL.
    params.flags = 0;
    params.pllInfo.pll = pll;
    CHECK_RC(pLwRm->Control(m_hSubDev,
                            LW2080_CTRL_CMD_CLK_GET_PLL_INFO,
                            &params,
                            sizeof(params)));

    // Print out the retrieved information.
    pPllInfo = &params.pllInfo;
    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "LW2080_CTRL_CLK_SOURCE_<PLL> : %d    \n", pPllInfo->pll       );
    Printf(Tee::PriHigh, "* MilwcoFreq                 : %d KHz\n", pPllInfo->milwcoFreq);
    Printf(Tee::PriHigh, "* MaxVcoFreq                 : %d KHz\n", pPllInfo->maxVcoFreq);
    Printf(Tee::PriHigh, "* Current NDIV               : %d    \n", pPllInfo->ndiv      );
    Printf(Tee::PriHigh, "* Current MDIV               : %d    \n", pPllInfo->mdiv      );
    Printf(Tee::PriHigh, "* Current PLDIV              : %d    \n", pPllInfo->pldiv     );
    Printf(Tee::PriHigh, "*                                    \n"                      );

    if (pPllInfo->bNdivSliding)
    {
        Printf(Tee::PriHigh, "* Supports NDIV Sliding      : YES\n"                   );
        Printf(Tee::PriHigh, "* Current  NDIV_LO           : %d \n", pPllInfo->ndivLo );
        Printf(Tee::PriHigh, "* Current  NDIV_MID          : %d \n", pPllInfo->ndivMid);
    }
    else
    {
        Printf(Tee::PriHigh, "* Supports NDIV Sliding      : NO\n");
    }
    Printf(Tee::PriHigh, "* \n");

    // Return the PLL info.
    pllInfo = *pPllInfo;

    return OK;
}

