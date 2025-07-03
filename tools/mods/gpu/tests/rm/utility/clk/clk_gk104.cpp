/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2013,2016-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file clk_gk104.cpp
//! \brief Implementation of GK104 clk hal
//!
#include "clkbase.h"
#include "core/include/memcheck.h"

ClockUtilGK104::ClockUtilGK104(LwU32 hClient, LwU32 hDev, LwU32 hSubDev)
    :ClockUtilGF117(hClient, hDev, hSubDev)
{

}

//!
//! @brief Gets mask of all programmable domains
//!
//! @return Mask of all programmable domains
//!----------------------------------------------------------------------------
LwU32 ClockUtilGK104::GetProgrammableDomains()
{
    if (!m_ProgClkDomains)
    {
        m_ProgClkDomains = ClockUtil::GetProgrammableDomains();

        // Programmable domains should not be empty!
        MASSERT(m_ProgClkDomains);
    }

    return m_ProgClkDomains;
}

//!
//! @brief Identify if a clock is a root clock
//!
//! @param[in]  clkDomain
//! @param[out] None

//! @returns    TRUE if the clock is a root clock
//-----------------------------------------------------------------------------
LwBool ClockUtilGK104::clkIsRootClock(LwU32 clkDomain)
{
    if ((LW2080_CTRL_CLK_DOMAIN_DISPCLK == clkDomain)  ||
        (LW2080_CTRL_CLK_DOMAIN_UTILSCLK == clkDomain) ||
        (LW2080_CTRL_CLK_DOMAIN_HOSTCLK == clkDomain)  ||
        (LW2080_CTRL_CLK_DOMAIN_HUB2CLK == clkDomain)  ||
        (LW2080_CTRL_CLK_DOMAIN_MSDCLK == clkDomain)   ||
        (LW2080_CTRL_CLK_DOMAIN_PWRCLK == clkDomain))
        return LW_TRUE;
    return LW_FALSE;
}

//!
//! @brief Identify if a clock is a derivative clock
//!   For Kepler, there are no derivative clocks
//!
//! @param[in]  clkDomain
//! @param[out] None

//! @returns    TRUE if the clock is a derivative clock
//-----------------------------------------------------------------------------
LwBool ClockUtilGK104::clkIsDerivativeClock(LwU32 clkDomain)
{
    return LW_FALSE;
}

//!
//!@brief Spelwlatively set the given PLL to the given frequency.
//!
//!@return Specific RC if failure.
//!        OK otherwise.
//!----------------------------------------------------------------------------
RC ClockUtilGK104::clkSetPllSpelwlatively
(
    LwU32 pll,
    LwU32 targetFreq,
    LwBool bAnyMdiv,
    LW2080_CTRL_CLK_PLL_INFO &clkPllInfo
)
{
    LW2080_CTRL_CLK_SET_PLL_SPELWLATIVELY_PARAMS clkSetPllSpelwlativelyParams;
    LwRmPtr pLwRm;
    RC      rc;

    // Spelwlatively set the given PLL to the given frequency.
    clkSetPllSpelwlativelyParams.flags       = 0;
    clkSetPllSpelwlativelyParams.targetFreq  = targetFreq;
    clkSetPllSpelwlativelyParams.pllInfo.pll = pll;

    if (bAnyMdiv)
    {
        clkSetPllSpelwlativelyParams.flags =
            FLD_SET_DRF(2080,
                        _CTRL_CLK_SET_PLL_SPELWLATIVELY_FLAGS,
                        _MDIV_RANGE,
                        _FULL,
                        clkSetPllSpelwlativelyParams.flags);
    }
    else
    {
        clkSetPllSpelwlativelyParams.flags =
            FLD_SET_DRF(2080,
                        _CTRL_CLK_SET_PLL_SPELWLATIVELY_FLAGS,
                        _MDIV_RANGE,
                        _DEFAULT,
                        clkSetPllSpelwlativelyParams.flags);
    }

    CHECK_RC(
        pLwRm->Control(m_hSubDev,
                       LW2080_CTRL_CMD_CLK_SET_PLL_SPELWLATIVELY,
                      &clkSetPllSpelwlativelyParams,
                       sizeof(clkSetPllSpelwlativelyParams)));

    // Return the spelwlative PLL info.
    clkPllInfo = clkSetPllSpelwlativelyParams.pllInfo;

    return OK;
}

//!
//!@brief Forces/unforces NDIV slowdown on the given clock domain.
//!
//!@return Specific RC if failure.
//!        OK otherwise.
//!----------------------------------------------------------------------------
RC ClockUtilGK104::clkForceNDivSlowdown
(
    LwU32  clkDomain,
    LwBool bForceSlowdown
)
{
    LW2080_CTRL_CLK_TEST_NDIV_SLOWDOWN_PARAMS params;
    vector<LW2080_CTRL_CLK_INFO> vClkInfo;
    LwRmPtr pLwRm;
    RC rc;
    const char *const domainName = m_ClkUtil.GetClkDomainName(clkDomain);

    if (bForceSlowdown)
    {
        Printf(Tee::PriHigh, "Force NDIV slowdown on %s domain:\n", domainName);
    }
    else
    {
        Printf(Tee::PriHigh, "Unforce NDIV slowdown on %s domain:\n", domainName);
    }

    params.action = DRF_DEF(2080,
                            _CTRL_CLK_TEST_NDIV_SLOWDOWN_ACTION,
                            _VAL,
                            _FORCE_SLOWDOWN);

    if (bForceSlowdown)
    {
        // Engage slowdown.
        params.data = LW_TRUE;
    }
    else
    {
        // Disengage slowdown.
        params.data = LW_FALSE;
    }

    // Test NDIV slowdown on the given clock domain.
    params.clkDomain = clkDomain;
    CHECK_RC(pLwRm->Control(m_hSubDev,
                            LW2080_CTRL_CMD_CLK_TEST_NDIV_SLOWDOWN,
                            &params,
                            sizeof(params)));

    //
    // Verify result by comparing actual and measured slowed/un-slowed
    // frequencies.
    //
    if (bForceSlowdown)
    {
        CHECK_RC(clkVerifySlowdownFreq(clkDomain));
    }
    else
    {
        // When slowdown is disengaged, verify the ramped frequency.
        CHECK_RC(GetClocks(clkDomain, vClkInfo));
        MASSERT(vClkInfo.size() == 1);
        CHECK_RC(VerifyClockFreq(vClkInfo, vClkInfo));
    }

    return OK;
}

//!
//!@brief Prevents/allows NDIV ramp-up on the given clock domain during a clock
//!       switch.
//!
//!@return Specific RC if failure.
//!        OK otherwise.
//!----------------------------------------------------------------------------
RC ClockUtilGK104::clkPreventNDivRampUp
(
    LwU32  clkDomain,
    LwBool bPreventRampup
)
{
    LW2080_CTRL_CLK_TEST_NDIV_SLOWDOWN_PARAMS params;
    vector<LW2080_CTRL_CLK_INFO> vClkInfo;
    LwRmPtr pLwRm;
    RC rc;

    params.action = DRF_DEF(2080,
                            _CTRL_CLK_TEST_NDIV_SLOWDOWN_ACTION,
                            _VAL,
                            _PREVENT_RAMPUP);

    if (bPreventRampup)
    {
        // Prevent ramp-up.
        params.data = LW_TRUE;
    }
    else
    {
        // Allow ramp-up.
        params.data = LW_FALSE;
    }

    // Test NDIV slowdown on the given clock domain.
    params.clkDomain = clkDomain;
    CHECK_RC(pLwRm->Control(m_hSubDev,
                            LW2080_CTRL_CMD_CLK_TEST_NDIV_SLOWDOWN,
                            &params,
                            sizeof(params)));

    return OK;
}

//!
//!@brief Verifies the slowed down frequency on the given clock domain.
//!
//!@return Specific RC if failure.
//!        OK otherwise.
//!----------------------------------------------------------------------------
RC ClockUtilGK104::clkVerifySlowdownFreq
(
    LwU32 clkDomain
)
{
    LW2080_CTRL_CLK_MEASURE_FREQ_PARAMS     clkCounterFreqParams;
    vector<LW2080_CTRL_CLK_EXTENDED_INFO>   vExtendedClkInfo;
    vector<LW2080_CTRL_CLK_INFO>            vClkInfo;
    LwU32                                   actualSlowdownFreq;
    LwU32                                   expectedSlowdownFreq;
    LwRmPtr                                 pLwRm;
    RC                                      rc;
    double                                  fPercentDiff;

    CHECK_RC(GetEffectiveClocks(clkDomain, vExtendedClkInfo));
    MASSERT(vExtendedClkInfo.size() == 1);
    expectedSlowdownFreq = vExtendedClkInfo[0].effectiveFreq;

    if (Platform::GetSimulationMode() != Platform::Fmodel)
    {
        Printf(Tee::PriHigh, "Clk Name : %13s,    ",
               m_ClkUtil.GetClkDomainName(vExtendedClkInfo[0].clkInfo.clkDomain));

        Printf(Tee::PriHigh, "Source : %10s,    ",
               m_ClkUtil.GetClkSourceName(vExtendedClkInfo[0].clkInfo.clkSource));

        Printf(Tee::PriHigh, "Nominal Freq : %7d KHz, ",
               (UINT32)vExtendedClkInfo[0].clkInfo.actualFreq);

        Printf(Tee::PriHigh, "Expected Slowdown Freq : %7d KHz, ",
               (UINT32)expectedSlowdownFreq);

        // Get the HW clock counter frequency for the clock domain.
        clkCounterFreqParams.clkDomain = vExtendedClkInfo[0].clkInfo.clkDomain;
        CHECK_RC(pLwRm->Control(m_hSubDev,
                                LW2080_CTRL_CMD_CLK_MEASURE_FREQ,
                                &clkCounterFreqParams,
                                sizeof(clkCounterFreqParams)));

        actualSlowdownFreq = clkCounterFreqParams.freqKHz;
        Printf(Tee::PriHigh, "Actual Slowdown Freq (HW Clock Counter) : %7d KHz, ",
               (LwU32)actualSlowdownFreq);

        //
        // The expected slowdown frequency (which is slightly rounded) should
        // be within 1% of each other.
        //
        fPercentDiff
            = ((double)actualSlowdownFreq - (double)expectedSlowdownFreq) * 100.0 /
               (double)expectedSlowdownFreq;

        Printf(Tee::PriHigh, "Difference : %+5.1f %%\n\n", fPercentDiff);
        MASSERT(abs(fPercentDiff) <= 1.0);
    }
    else
    {
        //
        // Since FMODEL does not support NDIV slowdown, fall-back to
        // verifying the frequency w/o slowdown.
        //
        Printf(Tee::PriHigh,
               "\n"
               "*****************************************************\n"
               "NOTE: FMODEL does not support NDIV Slowdown!         \n"
               "      Thus, verifying non-slowed frequency.          \n"
               "*****************************************************\n");
        CHECK_RC(GetClocks(clkDomain, vClkInfo));
        MASSERT(vClkInfo.size() == 1);
        CHECK_RC(VerifyClockFreq(vClkInfo, vClkInfo));
        Printf(Tee::PriHigh, "\n");
    }

    return OK;
}

//!
//!@brief Program the given clock domain and verify the slowed down frequency.
//        Then deassert slowdown and verify the ramped-up frequency.
//!
//!@return Specific RC if any test failed.
//!        OK otherwise.
//!----------------------------------------------------------------------------
RC ClockUtilGK104::clkProgramAndVerifySlowdownFreq
(
    LW2080_CTRL_CLK_INFO &clkInfo,
    LwBool bForceVCO,
    LwBool bForceBypass,
    LwBool bAnyMdiv
)
{
    LW2080_CTRL_CLK_SET_INFO_PARAMS setInfoParams;
    LwRmPtr pLwRm;
    RC rc;
    const char *const domainName = m_ClkUtil.GetClkDomainName(clkInfo.clkDomain);

    // Set the flag to force VCO or Bypass path if necessary.
    MASSERT(!(bForceVCO && bForceBypass));
    clkInfo.clkSource = LW2080_CTRL_CLK_SOURCE_DEFAULT;
    if (bForceVCO)
    {
        Printf(Tee::PriHigh, "Forcing VCO Path...\n");
        clkInfo.flags = FLD_SET_DRF(2080,
                                    _CTRL_CLK_INFO_FLAGS,
                                    _FORCE_PLL,
                                    _ENABLE,
                                    clkInfo.flags);
        clkInfo.flags = FLD_SET_DRF(2080,
                                    _CTRL_CLK_INFO_FLAGS,
                                    _FORCE_BYPASS,
                                    _DISABLE,
                                    clkInfo.flags);
    }
    else if (bForceBypass)
    {
        Printf(Tee::PriHigh, "Forcing Bypass Path...\n");
        clkInfo.flags = FLD_SET_DRF(2080,
                                    _CTRL_CLK_INFO_FLAGS,
                                    _FORCE_PLL,
                                    _DISABLE,
                                    clkInfo.flags);
        clkInfo.flags = FLD_SET_DRF(2080,
                                    _CTRL_CLK_INFO_FLAGS,
                                    _FORCE_BYPASS,
                                    _ENABLE,
                                    clkInfo.flags);
    }
    else
    {
        clkInfo.flags = FLD_SET_DRF(2080,
                                    _CTRL_CLK_INFO_FLAGS,
                                    _FORCE_PLL,
                                    _DISABLE,
                                    clkInfo.flags);
        clkInfo.flags = FLD_SET_DRF(2080,
                                    _CTRL_CLK_INFO_FLAGS,
                                    _FORCE_BYPASS,
                                    _DISABLE,
                                    clkInfo.flags);
    }

    // Set flags to control the range of MDIV.
    if (bAnyMdiv)
    {
        clkInfo.flags = FLD_SET_DRF(2080,
                                    _CTRL_CLK_INFO_FLAGS,
                                    _MDIV_RANGE,
                                    _FULL,
                                    clkInfo.flags);
    }
    else
    {
        clkInfo.flags = FLD_SET_DRF(2080,
                                    _CTRL_CLK_INFO_FLAGS,
                                    _MDIV_RANGE,
                                    _DEFAULT,
                                    clkInfo.flags);
    }

    //
    // Set the flag to prevent ramp-up during a clock switch. This is so that
    // on further clock switches, GPC2CLK will stay at its slowed-down
    // frequency after the switch instead of ramping-back up.
    //
    Printf(Tee::PriHigh,
           "Programming %s to frequency %d and leaving forced at NDIV slowdown.\n",
           domainName,
           clkInfo.targetFreq);
    CHECK_RC(clkPreventNDivRampUp(clkInfo.clkDomain, LW_TRUE));

    setInfoParams.flags           = LW2080_CTRL_CLK_SET_INFO_FLAGS_WHEN_IMMEDIATE;
    setInfoParams.clkInfoListSize = 1;
    setInfoParams.clkInfoList     = LW_PTR_TO_LwP64(&clkInfo);

    // Program the given clock domain.
    CHECK_RC(pLwRm->Control(m_hSubDev,
                            LW2080_CTRL_CMD_CLK_SET_INFO,
                            &setInfoParams,
                            sizeof(setInfoParams)));

    CHECK_RC(
        clkVerifySlowdownFreq(clkInfo.clkDomain));

     // Unforce slowdown.
    CHECK_RC(clkForceNDivSlowdown(clkInfo.clkDomain, LW_FALSE));

    return OK;
}

//!
//!@brief Tests NDIV slowdown.
//!
//!@return Specific RC if any test failed.
//!        OK otherwise.
//!----------------------------------------------------------------------------
RC ClockUtilGK104::ClockNDivSlowdownTest()
{
    LW2080_CTRL_CLK_PLL_INFO                     clkPllInfoLwrr;
    LW2080_CTRL_CLK_PLL_INFO                     clkPllInfoNext;
    vector<LW2080_CTRL_CLK_INFO>                 vClkInfo;
    LW2080_CTRL_CLK_INFO                        *pClkInfo;
    LwU32                                        vcoFreq;
    RC                                           rc;
    LwRmPtr                                      pLwRm;
    LW2080_CTRL_THERMAL_SYSTEM_EXELWTE_V2_PARAMS thermalCommands;

    ///////////////////////////////////////////////////////////////////////////
    // SETUP
    ///////////////////////////////////////////////////////////////////////////

    // Prints the expected and measured GPC2CLK frequency.
    Printf(Tee::PriHigh, "Clock State for LW2080_CTRL_CLK_DOMAIN_GPC2CLK domain:\n");
    CHECK_RC(PrintClocks(LW2080_CTRL_CLK_DOMAIN_GPC2CLK));

    // Retrieve operating limits and attributes of the GPCPLL.
    CHECK_RC(
        clkGetPllInfo(LW2080_CTRL_CLK_SOURCE_GPCPLL, clkPllInfoLwrr));

    //
    // In general, P-State-enabled VBIOSes also enable NDIV Slowdown/Sliding.
    // If you have encountered this message, you may have found a counter example.
    // For details, please see bug 1044306 (GM107 emulation).
    //
    if (!clkPllInfoLwrr.bNdivSliding)
    {
        Printf(Tee::PriHigh, "NDIV Slowdown/Sliding not supported on "
                             "LW2080_CTRL_CLK_DOMAIN_GPC2CLK domain.\n"
                             "You may need to modify the VBIOS.\n\n");
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    // Disable other slowdowns that would affect testing of NDIV Slowdown.
    memset(&thermalCommands, 0 , sizeof(LW2080_CTRL_THERMAL_SYSTEM_EXELWTE_PARAMS));
    thermalCommands.clientAPIVersion  = THERMAL_SYSTEM_API_VER;
    thermalCommands.clientAPIRevision = THERMAL_SYSTEM_API_REV;
    thermalCommands.clientInstructionSizeOf =
        sizeof(LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION);
    thermalCommands.exelwteFlags =
        LW2080_CTRL_THERMAL_SYSTEM_EXELWTE_FLAGS_IGNORE_FAIL;
    thermalCommands.instructionListSize = 1;
    thermalCommands.instructionList[0].opcode = LW2080_CTRL_THERMAL_SYSTEM_SET_SLOWDOWN_STATE_OPCODE;
    thermalCommands.instructionList[0].operands.setThermalSlowdownState.slowdownState = LW2080_CTRL_THERMAL_SLOWDOWN_DISABLED_ALL;
    CHECK_RC(pLwRm->Control(m_hSubDev,
                            LW2080_CTRL_CMD_THERMAL_SYSTEM_EXELWTE_V2,
                            &thermalCommands,
                            sizeof (thermalCommands)));
    if (thermalCommands.successfulInstructions != 1)
    {
        return RC::LWRM_ERROR;
    }

    ///////////////////////////////////////////////////////////////////////////
    // TEST 1 - Basic force/unforce of NDIV slowdown on current clock state.
    ///////////////////////////////////////////////////////////////////////////

    // Force slowdown.
    CHECK_RC(clkForceNDivSlowdown(LW2080_CTRL_CLK_DOMAIN_GPC2CLK, LW_TRUE));

    // Unforce slowdown.
    CHECK_RC(clkForceNDivSlowdown(LW2080_CTRL_CLK_DOMAIN_GPC2CLK, LW_FALSE));

    ///////////////////////////////////////////////////////////////////////////
    // TEST 2 - Verify NDIV slowdown during clock switch to a VCO frequency.
    ///////////////////////////////////////////////////////////////////////////

    // Get information about the GPC2CLK domain.
    CHECK_RC(GetClocks(LW2080_CTRL_CLK_DOMAIN_GPC2CLK, vClkInfo));
    MASSERT(vClkInfo.size() == 1);
    pClkInfo = &vClkInfo[0];

    // Retrieve operating limits and attributes of the GPCPLL.
    CHECK_RC(
        clkGetPllInfo(LW2080_CTRL_CLK_SOURCE_GPCPLL, clkPllInfoLwrr));

    //
    // Choose a VCO frequency (in KHz) between the min and max VCO frequencies
    // of GPCPLL.
    //
    pClkInfo->targetFreq =
        clkPllInfoLwrr.milwcoFreq +
        ((clkPllInfoLwrr.maxVcoFreq - clkPllInfoLwrr.milwcoFreq) / 2);

    //
    // Switch to the VCO path.
    //
    CHECK_RC(clkProgramAndVerifySlowdownFreq(vClkInfo[0], LW_TRUE, LW_FALSE, LW_FALSE));

    ///////////////////////////////////////////////////////////////////////////
    // TEST 3 - Verify NDIV slowdown during a VCO to VCO switch.
    ///////////////////////////////////////////////////////////////////////////

    // Get information about the GPC2CLK domain.
    CHECK_RC(GetClocks(LW2080_CTRL_CLK_DOMAIN_GPC2CLK, vClkInfo));
    MASSERT(vClkInfo.size() == 1);
    pClkInfo = &vClkInfo[0];

    // Retrieve operating limits and attributes of the GPCPLL.
    CHECK_RC(
        clkGetPllInfo(LW2080_CTRL_CLK_SOURCE_GPCPLL, clkPllInfoLwrr));

    //
    // Spelwlate another VCO frequency with a different NDIV_LO value. This
    // is to make sure the clocks HW state machine can deal with NDIV_LO
    // changing while thermal slowdown is engaged.
    //
    for (vcoFreq = clkPllInfoLwrr.milwcoFreq;
         vcoFreq < clkPllInfoLwrr.maxVcoFreq;
         vcoFreq += 10)
    {
        CHECK_RC(
            clkSetPllSpelwlatively(
                LW2080_CTRL_CLK_SOURCE_GPCPLL, vcoFreq, LW_TRUE, clkPllInfoNext));

        if (clkPllInfoLwrr.ndivLo != clkPllInfoNext.ndivLo)
        {
            break;
        }
    }

    MASSERT(clkPllInfoLwrr.maxVcoFreq == clkPllInfoNext.maxVcoFreq);
    if (vcoFreq >= clkPllInfoLwrr.maxVcoFreq)
    {
        return RC::LWRM_ERROR;
    }
    else
    {
        pClkInfo->targetFreq = vcoFreq;
    }

    //
    // Switch to the VCO path.
    //
    CHECK_RC(clkProgramAndVerifySlowdownFreq(vClkInfo[0], LW_TRUE, LW_FALSE, LW_TRUE));

    ///////////////////////////////////////////////////////////////////////////
    // TEST 4 - Verify NDIV slowdown during a VCO to Bypass switch.
    ///////////////////////////////////////////////////////////////////////////

    // Get information about the GPC2CLK domain.
    CHECK_RC(GetClocks(LW2080_CTRL_CLK_DOMAIN_GPC2CLK, vClkInfo));
    MASSERT(vClkInfo.size() == 1);
    pClkInfo = &vClkInfo[0];

    // Retrieve operating limits and attributes of the GPCPLL.
    CHECK_RC(
        clkGetPllInfo(LW2080_CTRL_CLK_SOURCE_GPCPLL, clkPllInfoLwrr));

    //
    // Choose the max OSM frequency as that is a frequency the Bypass path is
    // definitely capable of hitting.
    //
    pClkInfo->targetFreq = GetOneSrcFreqKHz();

    //
    // Switch to the Bypass path.
    //
    CHECK_RC(clkProgramAndVerifySlowdownFreq(vClkInfo[0], LW_FALSE, LW_TRUE, LW_FALSE));

    ///////////////////////////////////////////////////////////////////////////
    // TEST 5 - Verify NDIV slowdown DOES NOT happen during a Bypass to Bypass
    // switch.
    ///////////////////////////////////////////////////////////////////////////

    // Get information about the GPC2CLK domain.
    CHECK_RC(GetClocks(LW2080_CTRL_CLK_DOMAIN_GPC2CLK, vClkInfo));
    MASSERT(vClkInfo.size() == 1);
    pClkInfo = &vClkInfo[0];

    //
    // Choose the max OSM frequency divided by 2 as that is another frequency
    // the Bypass path is definitely capable of hitting.
    //
    pClkInfo->targetFreq = GetOneSrcFreqKHz() / 2;

    // Set the flag to force the Bypass path.
    Printf(Tee::PriHigh, "Forcing Bypass Path...\n");
    pClkInfo->clkSource = LW2080_CTRL_CLK_SOURCE_DEFAULT;
    pClkInfo->flags = FLD_SET_DRF(2080,
                                  _CTRL_CLK_INFO_FLAGS,
                                  _FORCE_PLL,
                                  _DISABLE,
                                  pClkInfo->flags);

    pClkInfo->flags = FLD_SET_DRF(2080,
                                  _CTRL_CLK_INFO_FLAGS,
                                  _FORCE_BYPASS,
                                  _ENABLE,
                                  pClkInfo->flags);

    //
    // Setting the flag to prevent ramp-up during a clock switch should not
    // matter since no slowdown is expected.
    //
    CHECK_RC(clkPreventNDivRampUp(LW2080_CTRL_CLK_DOMAIN_GPC2CLK, LW_TRUE));

    //
    // Switch to the Bypass path.
    //
    // NOTE: No slowdown is expected thus ProgramAndVerifyClockFreq() is used
    // below.
    //
    Printf(Tee::PriHigh,
           "Programming %s to frequency %d.\n",
           m_ClkUtil.GetClkDomainName(pClkInfo->clkDomain),
           pClkInfo->targetFreq);
    CHECK_RC(ProgramAndVerifyClockFreq(vClkInfo, vClkInfo));

    ///////////////////////////////////////////////////////////////////////////
    // TEST 6 - Verify NDIV slowdown during a Bypass to VCO switch.
    ///////////////////////////////////////////////////////////////////////////

    // Get information about the GPC2CLK domain.
    CHECK_RC(GetClocks(LW2080_CTRL_CLK_DOMAIN_GPC2CLK, vClkInfo));
    MASSERT(vClkInfo.size() == 1);
    pClkInfo = &vClkInfo[0];

    // Retrieve operating limits and attributes of the GPCPLL.
    CHECK_RC(
        clkGetPllInfo(LW2080_CTRL_CLK_SOURCE_GPCPLL, clkPllInfoLwrr));

    //
    // Choose a VCO frequency (in KHz) between the min and max VCO frequencies
    // of GPCPLL.
    //
    pClkInfo->targetFreq =
        clkPllInfoLwrr.milwcoFreq +
        ((clkPllInfoLwrr.maxVcoFreq - clkPllInfoLwrr.milwcoFreq) / 2);

    //
    // Switch to the VCO path.
    //
    CHECK_RC(clkProgramAndVerifySlowdownFreq(vClkInfo[0], LW_TRUE, LW_FALSE, LW_FALSE));

    ///////////////////////////////////////////////////////////////////////////
    // TEST 7 - Verify NDIV slowdown during a VCO to VCO switch with only
    //          NDIV changing (MDIV and PDIV stay constant).
    ///////////////////////////////////////////////////////////////////////////

    // Get information about the GPC2CLK domain.
    CHECK_RC(GetClocks(LW2080_CTRL_CLK_DOMAIN_GPC2CLK, vClkInfo));
    MASSERT(vClkInfo.size() == 1);
    pClkInfo = &vClkInfo[0];

    // Retrieve operating limits and attributes of the GPCPLL.
    CHECK_RC(
        clkGetPllInfo(LW2080_CTRL_CLK_SOURCE_GPCPLL, clkPllInfoLwrr));

    //
    // Spelwlate another VCO frequency with a different NDIV and NDIV_MID value
    // (but same NDIV_LO value due to MDIV staying constant). This is to make
    // sure the clocks HW state machine can deal with NDIV changing while
    // thermal slowdown is engaged.
    //
    for (vcoFreq = clkPllInfoLwrr.milwcoFreq;
         vcoFreq < clkPllInfoLwrr.maxVcoFreq;
         vcoFreq += 1000)
    {
        CHECK_RC(
            clkSetPllSpelwlatively(
                LW2080_CTRL_CLK_SOURCE_GPCPLL, vcoFreq, LW_FALSE, clkPllInfoNext));

        if (clkPllInfoLwrr.mdiv    == clkPllInfoNext.mdiv  &&
            clkPllInfoLwrr.pldiv   == clkPllInfoNext.pldiv &&
            clkPllInfoLwrr.ndiv    != clkPllInfoNext.ndiv  &&
            clkPllInfoLwrr.ndivMid != clkPllInfoNext.ndivMid)
        {
            MASSERT(clkPllInfoLwrr.ndivLo == clkPllInfoNext.ndivLo);
            break;
        }
    }

    MASSERT(clkPllInfoLwrr.maxVcoFreq == clkPllInfoNext.maxVcoFreq);
    if (vcoFreq >= clkPllInfoLwrr.maxVcoFreq)
    {
        return RC::LWRM_ERROR;
    }
    else
    {
        pClkInfo->targetFreq = vcoFreq;
    }

    //
    // Switch to the VCO path.
    //
    CHECK_RC(clkProgramAndVerifySlowdownFreq(vClkInfo[0], LW_TRUE, LW_FALSE, LW_FALSE));

    ///////////////////////////////////////////////////////////////////////////
    // TEST 8 - Verify NDIV slowdown during a VCO to VCO switch with only
    //          PDIV changing (MDIV and NDIV stay constant).
    ///////////////////////////////////////////////////////////////////////////

    // Get information about the GPC2CLK domain.
    CHECK_RC(GetClocks(LW2080_CTRL_CLK_DOMAIN_GPC2CLK, vClkInfo));
    MASSERT(vClkInfo.size() == 1);
    pClkInfo = &vClkInfo[0];

    // Retrieve operating limits and attributes of the GPCPLL.
    CHECK_RC(
        clkGetPllInfo(LW2080_CTRL_CLK_SOURCE_GPCPLL, clkPllInfoLwrr));

    //
    // Spelwlate another VCO frequency with a different PDIV value. This is to
    // make sure the clocks HW state machine can deal with PDIV changing while
    // thermal slowdown is engaged.
    //
    for (vcoFreq = clkPllInfoLwrr.milwcoFreq / 2;
         vcoFreq < clkPllInfoLwrr.maxVcoFreq;
         vcoFreq += 1000)
    {
        CHECK_RC(
            clkSetPllSpelwlatively(
                LW2080_CTRL_CLK_SOURCE_GPCPLL, vcoFreq, LW_FALSE, clkPllInfoNext));

        if (clkPllInfoLwrr.mdiv  == clkPllInfoNext.mdiv &&
            clkPllInfoLwrr.ndiv  == clkPllInfoNext.ndiv &&
            clkPllInfoLwrr.pldiv != clkPllInfoNext.pldiv)
        {
            MASSERT(clkPllInfoLwrr.ndivLo == clkPllInfoNext.ndivLo);
            break;
        }
    }

    MASSERT(clkPllInfoLwrr.maxVcoFreq == clkPllInfoNext.maxVcoFreq);
    if (vcoFreq >= clkPllInfoLwrr.maxVcoFreq)
    {
        return RC::LWRM_ERROR;
    }
    else
    {
        pClkInfo->targetFreq = vcoFreq;
    }

    //
    // Switch to the VCO path.
    //
    CHECK_RC(clkProgramAndVerifySlowdownFreq(vClkInfo[0], LW_TRUE, LW_FALSE, LW_FALSE));

    ///////////////////////////////////////////////////////////////////////////
    // TEST 9 - Verify NDIV slowdown during a VCO to VCO switch with only NDIV
    //          and PDIV changing (MDIV stays constant).
    ///////////////////////////////////////////////////////////////////////////

    // Get information about the GPC2CLK domain.
    CHECK_RC(GetClocks(LW2080_CTRL_CLK_DOMAIN_GPC2CLK, vClkInfo));
    MASSERT(vClkInfo.size() == 1);
    pClkInfo = &vClkInfo[0];

    // Retrieve operating limits and attributes of the GPCPLL.
    CHECK_RC(
        clkGetPllInfo(LW2080_CTRL_CLK_SOURCE_GPCPLL, clkPllInfoLwrr));

    //
    // Spelwlate another VCO frequency with a different PDIV value. This is to
    // make sure the clocks HW state machine can deal with NDIV and PDIV changing
    // while thermal slowdown is engaged.
    //
    for (vcoFreq = clkPllInfoLwrr.milwcoFreq / 2;
         vcoFreq < clkPllInfoLwrr.maxVcoFreq;
         vcoFreq += 1000)
    {
        CHECK_RC(
            clkSetPllSpelwlatively(
                LW2080_CTRL_CLK_SOURCE_GPCPLL, vcoFreq, LW_FALSE, clkPllInfoNext));

        if (clkPllInfoLwrr.mdiv  == clkPllInfoNext.mdiv  &&
            clkPllInfoLwrr.ndiv  != clkPllInfoNext.ndiv &&
            clkPllInfoLwrr.pldiv != clkPllInfoNext.pldiv)
        {
            MASSERT(clkPllInfoLwrr.ndivLo == clkPllInfoNext.ndivLo);
            break;
        }
    }

    MASSERT(clkPllInfoLwrr.maxVcoFreq == clkPllInfoNext.maxVcoFreq);
    if (vcoFreq >= clkPllInfoLwrr.maxVcoFreq)
    {
        return RC::LWRM_ERROR;
    }
    else
    {
        pClkInfo->targetFreq = vcoFreq;
    }

    //
    // Switch to the VCO path.
    //
    CHECK_RC(clkProgramAndVerifySlowdownFreq(vClkInfo[0], LW_TRUE, LW_FALSE, LW_FALSE));

    ///////////////////////////////////////////////////////////////////////////
    // CLEANUP
    ///////////////////////////////////////////////////////////////////////////

    // Retrieve operating limits and attributes of the GPCPLL.
    CHECK_RC(
        clkGetPllInfo(LW2080_CTRL_CLK_SOURCE_GPCPLL, clkPllInfoLwrr));

    // Set the flag to re-allow ramp-up during a clock switch.
    CHECK_RC(clkPreventNDivRampUp(LW2080_CTRL_CLK_DOMAIN_GPC2CLK, LW_FALSE));

    // Re-enable other slowdowns.
    memset(&thermalCommands, 0 , sizeof(LW2080_CTRL_THERMAL_SYSTEM_EXELWTE_PARAMS));
    thermalCommands.clientAPIVersion  = THERMAL_SYSTEM_API_VER;
    thermalCommands.clientAPIRevision = THERMAL_SYSTEM_API_REV;
    thermalCommands.clientInstructionSizeOf =
        sizeof(LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION);
    thermalCommands.exelwteFlags =
        LW2080_CTRL_THERMAL_SYSTEM_EXELWTE_FLAGS_IGNORE_FAIL;
    thermalCommands.instructionListSize = 1;
    thermalCommands.instructionList[0].opcode = LW2080_CTRL_THERMAL_SYSTEM_SET_SLOWDOWN_STATE_OPCODE;
    thermalCommands.instructionList[0].operands.setThermalSlowdownState.slowdownState = LW2080_CTRL_THERMAL_SLOWDOWN_ENABLED;
    CHECK_RC(pLwRm->Control(m_hSubDev,
                            LW2080_CTRL_CMD_THERMAL_SYSTEM_EXELWTE_V2,
                            &thermalCommands,
                            sizeof (thermalCommands)));
    if (thermalCommands.successfulInstructions != 1)
    {
        return RC::LWRM_ERROR;
    }

    return OK;
}

