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
//! \file rmt_fermiclkpath.cpp
//! \RM-Test to perform a clock path check.
//! This test is primarily written to verify the clock paths for :-
//! 1) Sys and Sys-Derived Clocks(Sys2Clk, Hub2Clk, LegClk & MsdClk).
//! 2) Root Clocks(UtilsClk, PwrClk & DispClk).
//! 3) Non-Root and Non-Sys-Derived Clocks.(Gpc2Clk, LtcClk & Xbar2Clk).
//! N.B.: All Frequency callwlations are here in units of KHz.
//! FERMITODO:CLK:
//!     1) Enhance the test for clock counter verification once they
//!     are implemented fully on HW.
//!     2) Presently the base refrence freq to start with is same as the VBIOS frequency.
//!        Change it some other fixed pattern dependent on Arch and not on board.
//!     3) The current code genrates automatic frequencies(Ref/Alt) only for Non-Sys Derived clocks.
//!        Implement a similar mechnaism for SYS-Derived clocks.
//!     4) Print the clock programming states in tabular Format for better comprehension of results.
//!

#include <cmath>
#include "gpu/tests/rmtest.h"
#include "gpu/tests/rmt_cl906f.h"
#include "core/include/jscript.h"
#include "gpu/tests/gputestc.h"
#include "core/include/display.h"
#include "core/include/platform.h"
#include "gpu/utility/rmclkutil.h"
#include "lwRmApi.h"
#include "lwmisc.h"
#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl2080/ctrl2080perf.h"
#include "core/include/utility.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/rm/utility/clk/clkbase.h"
#include "core/include/mrmtestinfra.h"
#include "core/include/memcheck.h"

/* Syncpoints sequence
 *
 * FERMICLOCKPATHTEST_SP_SETUP_START
 *     Do Setup
 * FERMICLOCKPATHTEST_SP_SETUP_STOP
 *
 * FERMICLOCKPATHTEST_SP_RUN_START
 *     Run clkpath test
 * FERMICLOCKPATHTEST_SP_RUN_STOP
 */

class FermiClockPathTest: public RmTest
{
public:
    FermiClockPathTest();
    virtual ~FermiClockPathTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    RmtClockUtil m_ClkUtil;          // Instantiate Clk Util
    LwRm::Handle m_hBasicClient;
    LwRm::Handle m_hBasicSubDev;

    ClockUtil               *pClkHal;

#if LWOS_IS_WINDOWS
    LW2080_CTRL_PERF_GET_PSTATES_INFO_PARAMS m_getAllPStatesInfoParams;
#endif

    //
    //! Setup and Test functions
    //
    RC BasicClockPathTest();
};

//! \brief ClockTest constructor.
//!
//! Initializes some basic variables as default values in the resepctive tests
//! \sa Setup
//-----------------------------------------------------------------------------
FermiClockPathTest::FermiClockPathTest()
{
    SetName("FermiClockPathTest");
    m_hBasicClient = 0;
    m_hBasicSubDev = 0;
    pClkHal = nullptr;

// Quick and dirty hack: If Class906fTest is in the background, keep it there until we're done.
    Class906fTest::runControl= Class906fTest::ContinueRunning;
}

//! \brief ClockTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
FermiClockPathTest::~FermiClockPathTest()
{
     return;
}

//! \brief IsSupported Function.
//!
//! The test is supported on HW
//! \sa Setup
//-----------------------------------------------------------------------------
string FermiClockPathTest::IsTestSupported()
{
    LwRmPtr    pLwRm;
    Gpu::LwDeviceId chipName = GetBoundGpuSubdevice()->DeviceId();

    if (!IsKEPLERorBetter(chipName))
    {
        return "This Clock Path Test is only supported for Kepler+";
    }

    if (IsGP107orBetter(chipName))
    {
        return "Support for this test has been deprecated from GP107+ in favor of -testname UniversalClockTest -ctp path";
    }

    m_hBasicSubDev = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    m_hBasicClient = pLwRm->GetClientHandle();

#if LWOS_IS_WINDOWS
    // Get P-States info
    pLwRm->Control(m_hBasicSubDev,
                   LW2080_CTRL_CMD_PERF_GET_PSTATES_INFO,
                   &m_getAllPStatesInfoParams,
                   sizeof(m_getAllPStatesInfoParams));

    if (!(m_getAllPStatesInfoParams.pstates & LW2080_CTRL_PERF_PSTATES_P0))
    {
        return "P-state P0 must be defined for the test to work.";
    }
#endif

    return RUN_RMTEST_TRUE;
}

//! \brief Setup Function.
//!
//! For basic test and Perf table test this setup function allocates the
//! required memory. Calls the sub functions to do appropriate allocations.
//!
//! \return OK if the allocations success, specific RC if failed
//! \sa SetupClockPathTest
//! \sa SetupPerfTest
//! \sa Run
//-----------------------------------------------------------------------------
RC FermiClockPathTest::Setup()
{
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle hClient;
    SYNC_POINT(FERMICLOCKPATHTEST_SP_SETUP_START);

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    hClient = pLwRm->GetClientHandle();

    pClkHal= ClockUtil::CreateModule(hClient, GetBoundGpuSubdevice());
    if (!pClkHal)
    {
        Printf(Tee::PriHigh,
            "%d: Clock hal creation failure\n",__LINE__);
        rc = RC::DEVICE_NOT_FOUND;
    }
    SYNC_POINT(FERMICLOCKPATHTEST_SP_SETUP_STOP);
    return rc;
}

//! \brief Run Function.
//!
//! Run function is the primary function which exelwtes the primary sub-test i.e.
//! BasicClockPathTest.
//! \return OK if the tests passed, specific RC if failed
//! \sa BasicClockPathTest
//! \sa PerfTableTest
//-----------------------------------------------------------------------------
RC FermiClockPathTest::Run()
{
    RC rc;
    SYNC_POINT(FERMICLOCKPATHTEST_SP_RUN_START);

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

        CHECK_RC(BasicClockPathTest());

#ifdef INCLUDE_MDIAG
    }
    if (TestsRunOnMRMI)
    {
        CHECK_RC(MrmCppInfra::TestDoneWaitingForOthers("RM"));
    }
#endif

    SYNC_POINT(FERMICLOCKPATHTEST_SP_RUN_STOP);
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
RC FermiClockPathTest::Cleanup()
{
    delete pClkHal;
    TEST_COMPLETE("FermiClockPathTest");
    return OK;
}

//-----------------------------------------------------------------------------
// Private Member Functions
//-----------------------------------------------------------------------------

//! \brief BasicClockPathTest function.
//!
//! The test verifies the capability of a clock to switch between its possible
//! paths i.e. ALT and REF(if any!) and the capability to switch between different
//! clock sources while being on that path.
//! This also verifies for all the clocks if they are programmable or not.
//! It sets a predetermined frequency and then verifies if it has been
//! set or not as per the desired path and source.
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------------
RC FermiClockPathTest::BasicClockPathTest()
{
    RC rc;

    Printf(Tee::PriHigh, " In BasicClockPathTest\n");

    Printf(Tee::PriHigh, " Dumping Domains\n");
    m_ClkUtil.DumpDomains(pClkHal->GetAllDomains());

    Printf(Tee::PriHigh,"Present Clock State for %u domains:\n",
           pClkHal->GetNumDomains());

    CHECK_RC(pClkHal->PrintClocks(pClkHal->GetAllDomains()));

    Printf(Tee::PriHigh, " Dumping All Programmable Clock Domains\n");
    m_ClkUtil.DumpDomains(pClkHal->GetProgrammableDomains());

    CHECK_RC(pClkHal->ClockPathTest());

    Printf(Tee::PriHigh, " BasicClockPathTest Done  %d\n",
                        __LINE__);

    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ FermiClockPathTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(FermiClockPathTest, RmTest,
    "TODORMT - Write a good short description here");
