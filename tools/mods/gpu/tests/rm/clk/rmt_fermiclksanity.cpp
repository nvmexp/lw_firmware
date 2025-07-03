/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2004-2013,2018-2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//!
//! \file rmt_fermiclksanity.cpp
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
#include "ctrl/ctrl2080/ctrl2080perf.h"
#include "core/include/utility.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/rm/utility/clk/clkbase.h"
#include "core/include/mrmtestinfra.h"
#include "core/include/memcheck.h"

/* Syncpoints sequence
 *
 * FERMICLOCKSANITYTEST_SP_SETUP_START
 *     Do Setup
 * FERMICLOCKSANITYTEST_SP_SETUP_STOP
 *
 * FERMICLOCKSANITYTEST_SP_RUN_START
 *     Run clksanity test
 * FERMICLOCKSANITYTEST_SP_RUN_STOP
 */

#define AltPathTolerance100X  1000
#define RefPathTolerance100X  1000

class FermiClockSanityTest: public RmTest
{
public:
    FermiClockSanityTest();
    virtual ~FermiClockSanityTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    RmtClockUtil m_ClkUtil;          // Instantiate Clk Util

    //! CLK Domains variables
    LW2080_CTRL_CLK_INFO *m_pClkInfos;
    LW2080_CTRL_CLK_INFO *m_pSetClkInfos;
    LW2080_CTRL_CLK_INFO *m_restorGetInfo;

    //! FB Info variables
    ClockUtil *pClkHal;

    //
    //! Test functions
    //
    RC BasicSanityClockTest();

};

//! \brief ClockTest constructor.
//!
//! Initializes some basic variables as default values in the resepctive tests
//! \sa Setup
//-----------------------------------------------------------------------------
FermiClockSanityTest::FermiClockSanityTest()
{
    SetName("ClockTest");
    m_pClkInfos = NULL;
    m_pSetClkInfos = NULL;
    m_restorGetInfo = NULL;
    pClkHal = NULL;

// Quick and dirty hack: If Class906fTest is in the background, keep it there until we're done.
    Class906fTest::runControl= Class906fTest::ContinueRunning;
}

//! \brief ClockTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
FermiClockSanityTest::~FermiClockSanityTest()
{
    return;
}

//! \brief IsTestSupported Function.
//!
//! The test is supported on HW
//! \sa Setup
//-----------------------------------------------------------------------------
string FermiClockSanityTest::IsTestSupported()
{
    Gpu::LwDeviceId chipName = GetBoundGpuSubdevice()->DeviceId();

    if (!IsKEPLERorBetter(chipName))
    {
        Printf(Tee::PriError, "This Clock Sanity Test is only supported for Kepler+\n");
        return "Supported only on Kepler+ chips";
    }

    if (IsGP107orBetter(chipName))
    {
        return "Support for this test has been deprecated from GP107+ in favor of -testname UniversalClockTest -ctp sanity";
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
//! \sa SetupPerfTest
//! \sa Run
//-----------------------------------------------------------------------------
RC FermiClockSanityTest::Setup()
{
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle m_hClient;

    SYNC_POINT(FERMICLOCKSANITYTEST_SP_SETUP_START);
    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    m_hClient = pLwRm->GetClientHandle();

    pClkHal= ClockUtil::CreateModule(m_hClient, GetBoundGpuSubdevice());
    if (!pClkHal)
    {
        Printf(Tee::PriHigh,
            "%d: Clock hal creation failure",__LINE__);
        rc = RC::DEVICE_NOT_FOUND;
    }

    SYNC_POINT(FERMICLOCKSANITYTEST_SP_SETUP_STOP);
    return rc;
}

//! \brief Run Function.
//!
//! Run function is the primary function which exelwtes the primary sub-test i.e.
//! BasicSanityClockTest.
//! \return OK if the tests passed, specific RC if failed
//! \sa BasicSanityClockTest
//! \sa PerfTableTest
//-----------------------------------------------------------------------------
RC FermiClockSanityTest::Run()
{
    RC rc;
    SYNC_POINT(FERMICLOCKSANITYTEST_SP_RUN_START);
#ifdef INCLUDE_MDIAG
    UINT32 i, numLoops = 1;
    bool TestsRunOnMRMI = MrmCppInfra::TestsRunningOnMRMI();
    if (TestsRunOnMRMI)
    {
        CHECK_RC(MrmCppInfra::SetupDoneWaitingForOthers("RM"));
        numLoops = MrmCppInfra::GetTestLoopsArg();
        if (!numLoops)
        {
            numLoops = 1;
        }
    }
    for (i=0; i<numLoops; i++)
    {
#endif

        CHECK_RC(BasicSanityClockTest());

#ifdef INCLUDE_MDIAG
    }
    if (TestsRunOnMRMI)
    {
        CHECK_RC(MrmCppInfra::TestDoneWaitingForOthers("RM"));
    }
#endif

    SYNC_POINT(FERMICLOCKSANITYTEST_SP_RUN_STOP);
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
RC FermiClockSanityTest::Cleanup()
{
    RC rc;
    rc = OK;

    delete []m_pClkInfos;
    delete []m_pSetClkInfos;
    delete []m_restorGetInfo;
    delete pClkHal;
    TEST_COMPLETE("FermiClockSanityTest");
    return rc;
}

//-----------------------------------------------------------------------------
// Private Member Functions
//-----------------------------------------------------------------------------

//! \brief Test gets current clocks state and verifies if we can program them.
//!
//! The test basically does a Sanity test on all Fermi Clock states.
//! This also verifies for all the clocks if they are programmable or not.
//! It sets a predetermined frequency and then verifies if it has been
//! set or not.
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------------
RC FermiClockSanityTest::BasicSanityClockTest()
{
    RC rc;

    Printf(Tee::PriHigh, " In BasicSanityClockTest\n");

    Printf(Tee::PriHigh, " Dumping all domains\n");
    m_ClkUtil.DumpDomains(pClkHal->GetAllDomains());

    Printf(Tee::PriHigh,"Present Clock State for %u domains:\n",
           pClkHal->GetNumDomains());

    CHECK_RC(pClkHal->PrintClocks(pClkHal->GetAllDomains()));

    Printf(Tee::PriHigh, " Dumping All Programmable Clock Domains\n");
    m_ClkUtil.DumpDomains(pClkHal->GetProgrammableDomains());

    CHECK_RC(pClkHal->ClockSanityTest());

    Printf(Tee::PriHigh, " BasicSanityClockTest Done  %d\n",
        __LINE__);

    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ FermiClockSanityTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(FermiClockSanityTest, RmTest,
                 "TODORMT - Write a good short description here");
