/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2009-2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//!
//! \file rmt_clksanity.cpp
//! \RM-Test to perform a sanity check.
//! This test is primarily written to verify the DevInit path VBIOS programming
//! of clocks and the initialization of the clocks done by HW itself after a
//! reset.Much of this test  is inspired from rmt_clktest.cpp.
// !
//! This Test performs the following Sanity checks:-
//  a) Verify that all clocks are driven by valid sources.
//! b) Verify none of the clocks are being driven on XTAL.
//! c) Verify that if a clock is running on a PLL , the PLL is enabled and running.
//! d) Verify that UTILSCLK is running at 108 MHz.
//! e) Verify if we can program the clocks to a different frequency or not(except UTILSCLK).
//!

#include "gpu/tests/rmtest.h"
#include "gpu/tests/rmt_cl906f.h"
#include "core/include/jscript.h"
#include "gpu/tests/gputestc.h"
#include "core/include/display.h"
#include "core/include/platform.h"
#include "gpu/utility/rmclkutil.h"
#include "lwRmApi.h"
#include "ctrl/ctrl2080.h"
#include "gpu/tests/rm/utility/clk/clkbase.h"
#include "core/include/utility.h"
#include "gpu/include/gpusbdev.h"
#include "rmpmucmdif.h"
#include "core/include/memcheck.h"

#define AltPathTolerance100X  1000
#define RefPathTolerance100X  1000

class ClockSanityTest: public RmTest
{
public:
    ClockSanityTest();
    virtual ~ClockSanityTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    RmtClockUtil m_ClkUtil;          // Instantiate Clk Util
    LwRm::Handle m_hBasicClient                                = 0;
    LwRm::Handle m_hBasicSubDev                                = 0;

    //! CLK Domains variables
    LW2080_CTRL_CLK_GET_DOMAINS_PARAMS m_getDomainsParams      = {};
    LW2080_CTRL_CLK_GET_INFO_PARAMS m_getInfoParams            = {};
    LW2080_CTRL_CLK_SET_INFO_PARAMS m_setInfoParams            = {};
    LW2080_CTRL_CLK_MEASURE_FREQ_PARAMS m_clkCounterFreqParams = {};
    LW2080_CTRL_CLK_INFO *m_pClkInfos                          = nullptr;
    LW2080_CTRL_CLK_INFO *m_pSetClkInfos                       = nullptr;
    LW2080_CTRL_CLK_INFO *m_restorGetInfo                      = nullptr;
    UINT32 m_numClkInfos                                       = 0;

    //
    //! Setup and Test functions
    //
    RC SetupClockSanityTest();
    RC BasicSanityClockTest();
    bool clkIsRootClock(UINT32 clkDomain);
    bool clkIsDerivedClock(UINT32 clkDomain);
    RC RestoreElpgEnableState();

    RC QueryPowerGatingParameter(UINT32 param,
                                 UINT32 paramExt,
                                 UINT32 *paramVal,
                                 bool set);

    // ! ELPG Save/Disable/Restore state
    bool m_elpgGraphicsSupportedState = false;
    bool m_elpgVideoSupportedState    = false;
    bool m_elpgVicSupportedState      = false;
    UINT32 m_elpgGraphicsEnabledState = 0;
    UINT32 m_elpgVideoEnabledState    = 0;
    UINT32 m_elpgVicEnabledState      = 0;
};

//! \brief ClockTest constructor.
//!
//! Initializes some basic variables as default values in the resepctive tests
//! \sa Setup
//-----------------------------------------------------------------------------
ClockSanityTest::ClockSanityTest()
{
    SetName("ClockTest");
    m_pClkInfos = NULL;
    m_pSetClkInfos = NULL;
    m_restorGetInfo = NULL;

    // Quick and dirty hack: If Class906fTest is in the background, keep it there until we're done.
    Class906fTest::runControl= Class906fTest::ContinueRunning;
}

//! \brief ClockTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
ClockSanityTest::~ClockSanityTest()
{
    // Quick and dirty hack: If Class906fTest is in the background, release it.
    Class906fTest::runControl= Class906fTest::StopRunning;
}

//! \brief IsSupported Function.
//!
//! The test is supported on HW
//! \sa Setup
//-----------------------------------------------------------------------------
string ClockSanityTest::IsTestSupported()
{
    if (Platform::GetSimulationMode() == Platform::Fmodel)
    {
        // Since we cannot model all the clock regsisters and their functionality on F-Model.
        return "Clock Sanity Test is not supported for F-Model due to incomplete register modelling";
    }


    return RUN_RMTEST_TRUE;
}

//! \brief Setup Function.
//!
//! For basic test and Perf table test this setup function allocates the
//! required memory. Calls the sub functions to do appropriate allocations.
//!
//! \return OK if the allocations success, specific RC if failed
//! \sa SetupClockSanityTest
//! \sa Run
//-----------------------------------------------------------------------------
RC ClockSanityTest::Setup()
{
    RC rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());
    CHECK_RC(SetupClockSanityTest());

    return rc;
}

//! \brief Run Function.
//!
//! Run function is the primary function which exelwtes the primary sub-test i.e.
//! BasicSanityClockTest.
//! \return OK if the tests passed, specific RC if failed
//! \sa BasicSanityClockTest
//-----------------------------------------------------------------------------
RC ClockSanityTest::Run()
{
    RC rc;
    CHECK_RC(BasicSanityClockTest());
    return rc;
}

//! \brief Cleanup Function.
//!
//! Cleanup all the allocated variables in the setup. Cleanup can be called
//! even in errorneous condition, in such case all the allocations should be
//! freed, this function takes care of such freeing up.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
RC ClockSanityTest::Cleanup()
{
    RC rc;
    rc = OK;

    delete []m_pClkInfos;
    delete []m_pSetClkInfos;
    delete []m_restorGetInfo;

    // Restore ELPG state if needed
    CHECK_RC(RestoreElpgEnableState());

    return rc;
}

//-----------------------------------------------------------------------------
// Private Member Functions
//-----------------------------------------------------------------------------

//! \brief SetupClockSanityTest Function.for basic test setup
//!
//! For basic test this setup function allocates the required memory.
//! To allocate memory for basic test it needs clock domains
//! information so the setup function also gets and dumps the clock domains
//! information.
//!
//! \return OK if the allocations success, specific RC if failed
//! \sa Setup
//-----------------------------------------------------------------------------
RC ClockSanityTest::SetupClockSanityTest()
{
    RC rc;
    LwRmPtr pLwRm;
    UINT32 iLoopVar;

    m_elpgGraphicsSupportedState = false;
    m_elpgVideoSupportedState = false;
    m_elpgVicSupportedState = false;

    m_hBasicSubDev = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    m_hBasicClient = pLwRm->GetClientHandle();

    // Get all exposable clock domains.
    m_getDomainsParams.clkDomainsType = LW2080_CTRL_CLK_DOMAINS_TYPE_ALL;

    // get clk domains
    CHECK_RC(pLwRm->Control(m_hBasicSubDev,
        LW2080_CTRL_CMD_CLK_GET_DOMAINS,
        &m_getDomainsParams, sizeof (m_getDomainsParams)));

    Printf(Tee::PriHigh, " Dumping All Domains\n");
    m_ClkUtil.DumpDomains(m_getDomainsParams.clkDomains);

    // Get the number of domains this GPU supports
    m_numClkInfos = 0;

    for (iLoopVar = 0; iLoopVar < 32; iLoopVar++)
    {
        if ((1 << iLoopVar) & m_getDomainsParams.clkDomains)
            m_numClkInfos++;
    }

    // Initilaize the ClkInfo Params.
    m_pClkInfos = new
        LW2080_CTRL_CLK_INFO[m_numClkInfos * sizeof (LW2080_CTRL_CLK_INFO)];

    if(m_pClkInfos == NULL)
    {
        Printf(Tee::PriHigh,
            "%d: Set Clock Info allocation failed,\n",__LINE__);
        rc = RC::CANNOT_ALLOCATE_MEMORY;
        return rc;
    }

    memset((void *)m_pClkInfos,
        0,
        m_numClkInfos * sizeof (LW2080_CTRL_CLK_INFO));

    m_pSetClkInfos = new
        LW2080_CTRL_CLK_INFO[m_numClkInfos * sizeof (LW2080_CTRL_CLK_INFO)];

    // Initilaize the SetClkInfo Params.
    if(m_pSetClkInfos == NULL)
    {
        Printf(Tee::PriHigh,
            "%d: Set Clock Info allocation failed\n",__LINE__);
        rc = RC::CANNOT_ALLOCATE_MEMORY;
        return rc;
    }
    memset((void *)m_pSetClkInfos,
        0,
        m_numClkInfos * sizeof(LW2080_CTRL_CLK_INFO));

    m_restorGetInfo = new
        LW2080_CTRL_CLK_INFO[m_numClkInfos * sizeof (LW2080_CTRL_CLK_INFO)];

    // Initilaize the restoreClkInfo Params.
    if(m_restorGetInfo == NULL)
    {
        Printf(Tee::PriHigh,
            "%d: Restore info buffer allocation failure\n",__LINE__);
        rc = RC::CANNOT_ALLOCATE_MEMORY;
        return rc;
    }
    memset((void *)m_restorGetInfo,
        0,
        m_numClkInfos * sizeof(LW2080_CTRL_CLK_INFO));

    return rc;
}

//! \brief Test gets current clocks state and verifies if we can program them.
//!
//! The test basically does a Sanity test on all Clock states.
//! This also verifies for all the clocks if they are programmable or not.
//! It sets a predetermined frequency and then verifies if it has been
//! set or not.
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------------
RC ClockSanityTest::BasicSanityClockTest()
{
    RC rc;
    UINT32 iLoopVar, tempVal;
    LwRmPtr pLwRm;
    UINT32 setLoopVar;
    const UINT32 incrVal = 50000; // 50 MHz
    UINT32 errorLog = 0;

    Printf(Tee::PriHigh, " In BasicSanityClockTest\n");

    // fill in clock info state
    tempVal = 0;
    for (iLoopVar = 0; iLoopVar < 32; iLoopVar++)
    {
        if (((1 << iLoopVar) & m_getDomainsParams.clkDomains) == 0)
            continue;

        m_pClkInfos[tempVal++].clkDomain = (1 << iLoopVar);
    }

    // hook up clk info table
    m_getInfoParams.flags = 0;
    m_getInfoParams.clkInfoListSize = m_numClkInfos;
    m_getInfoParams.clkInfoList = (LwP64)m_pClkInfos;

    // get clocks
    CHECK_RC(pLwRm->Control(m_hBasicSubDev,
        LW2080_CTRL_CMD_CLK_GET_INFO,
        &m_getInfoParams,
        sizeof (m_getInfoParams)));

    Printf(Tee::PriHigh,"Present Clock State\n");

    for (iLoopVar = 0; iLoopVar < m_numClkInfos; iLoopVar++)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(m_pClkInfos[iLoopVar].clkDomain);
        const char * const sourceName = m_ClkUtil.GetClkSourceName(m_pClkInfos[iLoopVar].clkSource);

        Printf(Tee::PriHigh,"Clock Domain is %s .\n", domainName);
        Printf(Tee::PriHigh,"Clock Source is %s %d. \n", sourceName, m_pClkInfos[iLoopVar].clkSource);
        Printf(Tee::PriHigh,"Actual Frequency is  %d \n", (int)m_pClkInfos[iLoopVar].actualFreq);
        Printf(Tee::PriHigh,"Target Frequency is  %d \n", (int)m_pClkInfos[iLoopVar].targetFreq);
        Printf(Tee::PriHigh,"Flag Value is 0x%x \n", (UINT32)m_pClkInfos[iLoopVar].flags);
        m_clkCounterFreqParams.clkDomain = m_pClkInfos[iLoopVar].clkDomain;
        // Get the Clock Counter Frequency for this clock.
        CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                        LW2080_CTRL_CMD_CLK_MEASURE_FREQ,
                        &m_clkCounterFreqParams,
                        sizeof (m_clkCounterFreqParams)));
        Printf(Tee::PriHigh, "Measured Clock Counter Frequency for %s Domain was %d\n", domainName, (int)m_clkCounterFreqParams.freqKHz );
        Printf(Tee::PriHigh,"\n");
    }

    errorLog = 0;

    // Perform the check on all the clocks.
    for (iLoopVar = 0; iLoopVar < m_numClkInfos; iLoopVar++)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(m_pClkInfos[iLoopVar].clkDomain);
        const char * const sourceName = m_ClkUtil.GetClkSourceName(m_pClkInfos[iLoopVar].clkSource);
        bool checkClk = true;

        //
        // Exclude PCLKfrom any such verification for Now.
        // Laterwe can enhance the test to verify that if
        // the head is active then the sourcefor PCLK should
        // not be XTALotherwise it should be XTAL as a sanity check.
        //
        for (UINT32 pi = 0; pi < LW2080_CTRL_CLK_DOMAIN_PCLK__SIZE_1; pi++)
        {
            if (LW2080_CTRL_CLK_DOMAIN_PCLK(pi) == m_pClkInfos[iLoopVar].clkDomain)
                checkClk = false;
        }

        if (checkClk == false)
            continue;

        if (m_pClkInfos[iLoopVar].clkDomain == LW2080_CTRL_CLK_DOMAIN_PWRCLK)
            continue;

        // Check 1: None of the clocks should be running on default source(No Source!)
        if (m_pClkInfos[iLoopVar].clkSource == LW2080_CTRL_CLK_SOURCE_DEFAULT)
        {
            Printf(Tee::PriHigh,"Invalid Clock Source \n");
            Printf(Tee::PriHigh,"ERROR: %s is running on No Source.\n",
                domainName);
            errorLog = 1;
        }

        //
        // Check 2: If a clock is running on any source(even a PLL),it should
        // be on and running. Otherwise the clock Actual Frequency will be zero.
        //
        if ((m_pClkInfos[iLoopVar].clkSource != LW2080_CTRL_CLK_SOURCE_DEFAULT) &&
            (m_pClkInfos[iLoopVar].actualFreq == 0))
        {
            Printf(Tee::PriHigh,"Invalid Clock Source Configuration \n");
            Printf(Tee::PriHigh,"ERROR: The following clock source is not running -> %s .\n",
                sourceName);
            errorLog = 1;
        }

        // Check 3: None of the clocks should be running on XTAL.
        if (m_pClkInfos[iLoopVar].clkSource == LW2080_CTRL_CLK_SOURCE_XTAL)
        {
            Printf(Tee::PriHigh,"Invalid Clock Tree Configuration \n");
            Printf(Tee::PriHigh,"ERROR: %s is still running on XTAL.\n",
                domainName);
            errorLog = 1;
        }
        // CHECK 4: UTILSCLK should never run a frequency other than 108MHz .
        if ((m_pClkInfos[iLoopVar].clkDomain == LW2080_CTRL_CLK_DOMAIN_UTILSCLK) &&
            (m_pClkInfos[iLoopVar].actualFreq != 108000))
        {
            Printf(Tee::PriHigh,"Invalid Configuration for UTILSCLK\n");
            Printf(Tee::PriHigh,"ERROR: UTILSCLK is running on Freq  -> %d \n", (int)m_pClkInfos[iLoopVar].actualFreq);
            errorLog = 1;
        }
    }
    if (errorLog == 1)
    {
        rc =  RC::LWRM_ERROR;
        return rc;
    }

    // Get all Programmable Clock Domains.
    m_getDomainsParams.clkDomainsType = LW2080_CTRL_CLK_DOMAINS_TYPE_PROGRAMMABALE_ONLY;

    // get programable clk domains
    CHECK_RC(pLwRm->Control(m_hBasicSubDev,
        LW2080_CTRL_CMD_CLK_GET_DOMAINS,
        &m_getDomainsParams, sizeof (m_getDomainsParams)));

    Printf(Tee::PriHigh, " Dumping All Programmable Clock Domains\n");
    m_ClkUtil.DumpDomains(m_getDomainsParams.clkDomains);

    // fill in programmable clock info state
    tempVal = 0;
    m_numClkInfos = 0;
    for (iLoopVar = 0; iLoopVar < 32; iLoopVar++)
    {
        if (((1 << iLoopVar) & m_getDomainsParams.clkDomains) == 0)
            continue;

        m_pClkInfos[tempVal++].clkDomain = (1 << iLoopVar);
        m_numClkInfos++;
    }

    // hook up clk info table
    m_getInfoParams.flags = 0;
    m_getInfoParams.clkInfoListSize = m_numClkInfos;
    m_getInfoParams.clkInfoList = (LwP64)m_pClkInfos;

    // get clocks
    CHECK_RC(pLwRm->Control(m_hBasicSubDev,
        LW2080_CTRL_CMD_CLK_GET_INFO,
        &m_getInfoParams,
        sizeof (m_getInfoParams)));

    // Store current clock state
    for (iLoopVar = 0; iLoopVar < m_numClkInfos; iLoopVar++)
    {
        memcpy(&m_restorGetInfo[iLoopVar],
            &m_pClkInfos[iLoopVar],
            sizeof(LW2080_CTRL_CLK_INFO));
    }

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Set the new frequency for all the clocks\n");

    setLoopVar = 0;
    for (iLoopVar = 0; iLoopVar < m_numClkInfos; iLoopVar++)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(m_pClkInfos[iLoopVar].clkDomain);
        const char * const sourceName = m_ClkUtil.GetClkSourceName(m_pClkInfos[iLoopVar].clkSource);

        m_pSetClkInfos[setLoopVar].clkDomain =
            m_pClkInfos[iLoopVar].clkDomain;
        m_pSetClkInfos[setLoopVar].clkSource =
            m_pClkInfos[iLoopVar].clkSource;
        m_pSetClkInfos[setLoopVar].flags =
            m_pClkInfos[iLoopVar].flags;
        m_pSetClkInfos[setLoopVar].actualFreq =
            m_pClkInfos[iLoopVar].actualFreq;

        // Increment the clock Frequency by some pre-decided value.
        if (clkIsRootClock(m_pSetClkInfos[setLoopVar].clkDomain) ||
            clkIsDerivedClock(m_pSetClkInfos[setLoopVar].clkDomain))
        {
                m_pSetClkInfos[setLoopVar].targetFreq =
                    m_pClkInfos[iLoopVar].targetFreq/2;
        }
        else
        {
                m_pSetClkInfos[setLoopVar].targetFreq =
                    m_pClkInfos[iLoopVar].targetFreq + incrVal;
         }
        Printf(Tee::PriHigh, "Programming %s with src %s to Frequency %d flag %d\n", domainName, sourceName, (int)m_pSetClkInfos[setLoopVar].targetFreq, m_pSetClkInfos[setLoopVar].flags );
        setLoopVar++;
    }

    Printf(Tee::PriHigh, "\n");

    // hook up clk info table
    m_setInfoParams.flags =
        LW2080_CTRL_CLK_SET_INFO_FLAGS_WHEN_IMMEDIATE;

    m_setInfoParams.clkInfoListSize = setLoopVar;

    m_setInfoParams.clkInfoList =
        (LwP64)m_pSetClkInfos;

    // Set new clocks frequency
    CHECK_RC(pLwRm->Control(m_hBasicSubDev,
        LW2080_CTRL_CMD_CLK_SET_INFO,
        &m_setInfoParams,
        sizeof (m_setInfoParams)));

    // hook up clk info table to get the back the recently set values
    m_getInfoParams.flags = 0;
    m_getInfoParams.clkInfoListSize = m_numClkInfos;
    m_getInfoParams.clkInfoList = (LwP64)m_pClkInfos;

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Reading the set frequency for all the clocks\n");

    // get New clocks
    CHECK_RC(pLwRm->Control(m_hBasicSubDev,
        LW2080_CTRL_CMD_CLK_GET_INFO,
        &m_getInfoParams,
        sizeof (m_getInfoParams)));

    setLoopVar = 0;

    // Verify the recently set new clock frequencies.
    for (iLoopVar = 0; iLoopVar < m_numClkInfos; iLoopVar++)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(m_pClkInfos[iLoopVar].clkDomain);
        const char * const domainNameNew = m_ClkUtil.GetClkDomainName(m_pSetClkInfos[setLoopVar].clkDomain);
        const char * const sourceName = m_ClkUtil.GetClkSourceName(m_pClkInfos[iLoopVar].clkSource);
        UINT32 rangePercent = 0;

        if (m_pSetClkInfos[setLoopVar].clkDomain != m_pClkInfos[iLoopVar].clkDomain)
        {
            Printf(Tee::PriHigh, "%s running at Frequency %d\n", domainName, (int)m_pClkInfos[iLoopVar].targetFreq);
            continue;
        }

        Printf(Tee::PriHigh, "%s is Programmed to Frequency %d driven by %s\n", domainName, (int)m_pClkInfos[iLoopVar].actualFreq, sourceName);
        Printf(Tee::PriHigh, "Expected Frequency for %s Domain was %d\n", domainNameNew, (int)m_pSetClkInfos[setLoopVar].targetFreq );

        m_clkCounterFreqParams.clkDomain = m_pClkInfos[iLoopVar].clkDomain;
        //Get the Clock Counter Frequency for this clock.
        CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                    LW2080_CTRL_CMD_CLK_MEASURE_FREQ,
                    &m_clkCounterFreqParams,
                    sizeof (m_clkCounterFreqParams)));
        Printf(Tee::PriHigh, "Measured Clock Counter Frequency for %s Domain was %d\n", domainName, (int)m_clkCounterFreqParams.freqKHz );

        if ((m_pClkInfos[iLoopVar].clkSource == LW2080_CTRL_CLK_SOURCE_XTAL) ||
            (m_pClkInfos[iLoopVar].clkSource == LW2080_CTRL_CLK_SOURCE_SPPLL0))
        {
            // Allowing 10 % tolerance on Alt path
            rangePercent = AltPathTolerance100X;
        }
        else
        {
            // Allowing 10 % tolerance on Ref Path
            rangePercent = RefPathTolerance100X;
        }

        if (!m_ClkUtil.clkIsFreqWithinRange(m_pSetClkInfos[setLoopVar].targetFreq,
            m_pClkInfos[iLoopVar].actualFreq,
            rangePercent))
        {
           Printf(Tee::PriHigh,
                "%d :Setting new target frequency Failed\n",
                __LINE__);
            rc = RC::LWRM_ERROR;
            return rc;
        }
        else
        {
            Printf(Tee::PriHigh,
                "Setting new target frequency Successful\n");
        }
        setLoopVar++;
    }

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "Restoring the original frequency for all the clocks\n");
    Printf(Tee::PriHigh, "\n");

    // Restore the old values
    setLoopVar = 0;

    for (iLoopVar = 0; iLoopVar < m_numClkInfos; iLoopVar++)
    {
        memcpy(&m_pSetClkInfos[setLoopVar],
            &m_restorGetInfo[iLoopVar],
            sizeof(LW2080_CTRL_CLK_INFO));
        setLoopVar++;
    }

    // hook up clk info table
    m_setInfoParams.flags =
        LW2080_CTRL_CLK_SET_INFO_FLAGS_WHEN_IMMEDIATE;

    m_setInfoParams.clkInfoListSize = setLoopVar;

    m_setInfoParams.clkInfoList =
        (LwP64)m_pSetClkInfos;

    // Set clocks
    CHECK_RC(pLwRm->Control(m_hBasicSubDev,
        LW2080_CTRL_CMD_CLK_SET_INFO,
        &m_setInfoParams,
        sizeof (m_setInfoParams)));

    // hook up clk info table to get the back the recently set values
    m_getInfoParams.flags = 0;
    m_getInfoParams.clkInfoListSize = m_numClkInfos;
    m_getInfoParams.clkInfoList = (LwP64)m_pClkInfos;

    // get New clocks
    CHECK_RC(pLwRm->Control(m_hBasicSubDev,
        LW2080_CTRL_CMD_CLK_GET_INFO,
        &m_getInfoParams,
        sizeof (m_getInfoParams)));

    setLoopVar = 0;
    for (iLoopVar = 0; iLoopVar < m_numClkInfos; iLoopVar++)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(m_pClkInfos[iLoopVar].clkDomain);
        const char * const domainNameNew = m_ClkUtil.GetClkDomainName(m_pSetClkInfos[setLoopVar].clkDomain);
        const char * const sourceName = m_ClkUtil.GetClkSourceName(m_pClkInfos[iLoopVar].clkSource);

        if (m_pSetClkInfos[setLoopVar].clkDomain != m_pClkInfos[iLoopVar].clkDomain)
        {
            Printf(Tee::PriHigh, "%s running at Frequency %d\n", domainName, (int)m_pClkInfos[iLoopVar].targetFreq);
            continue;
        }

        Printf(Tee::PriHigh, "%s is Programmed to Frequency %d driven by %s\n", domainName, (int)m_pClkInfos[iLoopVar].actualFreq, sourceName);
        Printf(Tee::PriHigh, "Expected Frequency for %s Domain was %d\n", domainNameNew, (int)m_pSetClkInfos[setLoopVar].targetFreq );

        m_clkCounterFreqParams.clkDomain = m_pClkInfos[iLoopVar].clkDomain;
        //Get the Clock Counter Frequency for this clock.
        CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                        LW2080_CTRL_CMD_CLK_MEASURE_FREQ,
                        &m_clkCounterFreqParams,
                        sizeof (m_clkCounterFreqParams)));
        Printf(Tee::PriHigh, "Measured Clock Counter Frequency for %s Domain was %d\n", domainName, (int)m_clkCounterFreqParams.freqKHz );

        if(m_pClkInfos[iLoopVar].targetFreq !=
            m_pSetClkInfos[setLoopVar].targetFreq )
        {
            Printf(Tee::PriHigh,
                "%d :Failure :Restoring target frequency\n",
                __LINE__);
            rc = RC::LWRM_ERROR;
            return rc;
        }
        else
        {

            Printf(Tee::PriHigh,
                "Restoring target freq Successful\n");
        }
        setLoopVar++;
    }

    Printf(Tee::PriHigh, "\n");

    Printf(Tee::PriHigh, " BasicSanityClockTest Done  %d\n",
        __LINE__);

    return rc;

}

// Helper functions for BasicSanityClockTest

//! \brief clkIsRootClock function.
//!
//! This helper function is used to determine if the given clock domain is for
//! a Root clock or not.Root clocks are those clocks that do not have a
//! dedicated VCO Path.They are only driven by an OSM2 Module.
//!
//! \return TRUE if a Root Clock, otherwise FALSE.
//------------------------------------------------------------------------------
bool ClockSanityTest::clkIsRootClock(UINT32 clkDomain)
{
    if ((clkDomain == LW2080_CTRL_CLK_DOMAIN_DISPCLK)  ||
        (clkDomain == LW2080_CTRL_CLK_DOMAIN_UTILSCLK) ||
        (clkDomain == LW2080_CTRL_CLK_DOMAIN_HOSTCLK)  ||
        (clkDomain == LW2080_CTRL_CLK_DOMAIN_PWRCLK))
        return true;
    else
        return false;
}

//! \brief clkIsDerivedClock function.
//!
//! This helper function is used to determine if the given clock domain is for
//! a Sys_derived clock or not.Sys-Derived clocks are those clocks that share a
//! common VCO in refrence Path. This common VCO belongs to SYSPLL.On alternate path
//! each of them have their own dedicated OSM1 module and LDIV muxes.
//!
//! \return TRUE if a Sys-Derived Clock, otherwise FALSE.
//------------------------------------------------------------------------------
bool ClockSanityTest::clkIsDerivedClock(UINT32 clkDomain)
{
    if ((clkDomain == LW2080_CTRL_CLK_DOMAIN_SYS2CLK)  ||
        (clkDomain == LW2080_CTRL_CLK_DOMAIN_HUB2CLK)  ||
        (clkDomain == LW2080_CTRL_CLK_DOMAIN_LEGCLK)  ||
        (clkDomain == LW2080_CTRL_CLK_DOMAIN_MSDCLK))
        return true;
    else
        return false;
}

//! \brief RestoreElpgEnableState Function.
//!
//! This function restores any supporting engine's elpg enable state.
//!
//! \return OK if the restore succeeds, specific RC if failed
//-----------------------------------------------------------------------------
RC ClockSanityTest::RestoreElpgEnableState()
{
    RC rc;

    rc = OK;

    Printf(Tee::PriHigh," Restoring previous ELPG enable state\n");

    // Restore GRAPHICS enabled state if supported
    if (m_elpgGraphicsSupportedState && (m_elpgGraphicsEnabledState != 0))
    {
        rc = QueryPowerGatingParameter(LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED,
                                       LW2080_CTRL_MC_POWERGATING_ENGINE_ID_GRAPHICS,
                                       &m_elpgGraphicsEnabledState, true);
    }

    // Restore VIC enabled state if supported
    if (m_elpgVicSupportedState && (m_elpgVicEnabledState != 0))
    {
        rc = QueryPowerGatingParameter(LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED,
                                       LW2080_CTRL_MC_POWERGATING_ENGINE_ID_GR_RG,
                                       &m_elpgVicEnabledState, true);
    }

    return rc;
}

//! \brief Query for a parameter if set=false, else Set the passed parameter
//!
//----------------------------------------------------------------------------
 RC ClockSanityTest::QueryPowerGatingParameter(UINT32 param, UINT32 paramExt, UINT32 *paramVal,  bool set)
{
    RC           rc;
    LwRmPtr      pLwRm;

    LW2080_CTRL_POWERGATING_PARAMETER powerGatingParam = {0};
    LW2080_CTRL_MC_SET_POWERGATING_PARAMETER_PARAMS setPowerGatingParam = {0};

    powerGatingParam.parameterExtended = paramExt;
    powerGatingParam.parameter = param;

    if(!set)
    {
        LW2080_CTRL_MC_QUERY_POWERGATING_PARAMETER_PARAMS queryPowerGatingParam = {0};

        queryPowerGatingParam.listSize = 1;
        queryPowerGatingParam.parameterList = (LwP64)&powerGatingParam;

        rc = pLwRm->ControlBySubdevice(GetBoundGpuSubdevice(),
                                       LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER,
                                       &queryPowerGatingParam,
                                       sizeof(queryPowerGatingParam));
        if (rc == RC::LWRM_NOT_SUPPORTED)
        {
            Printf(Tee::PriHigh,"ElpgTest::QueryPowerGatingParameter : Query not done\n");
            return rc;
        }
        else
        {
            *paramVal = powerGatingParam.parameterValue;
        }
    }
    else
    {

        powerGatingParam.parameterExtended = paramExt;
        powerGatingParam.parameter = param;
        powerGatingParam.parameterValue = *paramVal;
        setPowerGatingParam.listSize = 1;
        setPowerGatingParam.parameterList = (LwP64)&powerGatingParam;
        rc = pLwRm->ControlBySubdevice(GetBoundGpuSubdevice(),
                                           LW2080_CTRL_CMD_MC_SET_POWERGATING_PARAMETER,
                                           &setPowerGatingParam,
                                           sizeof(setPowerGatingParam));
         if (rc == RC::LWRM_NOT_SUPPORTED)
        {
            Printf(Tee::PriHigh,"ElpgTest::QueryPowerGatingParameter : Set not done\n");
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ ClockSanityTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(ClockSanityTest, RmTest,
                 "Generic Clock Sanity Test");
