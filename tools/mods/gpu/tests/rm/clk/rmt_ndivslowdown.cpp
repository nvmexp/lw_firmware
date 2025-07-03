/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2013,2016-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_ndivslowdown.cpp
//! \brief Wrapper for the NDIV slowdown clock test.
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include <string>           // Only use <> for built-in C++ stuff

#include "class/cla097.h"   // KEPLER_A
#include "gpu/include/gralloc.h"

#include "gpu/tests/rm/utility/clk/clkbase.h"
#include "core/include/memcheck.h"

class NDivSlowdownTest : public RmTest
{
public:
    NDivSlowdownTest();
    virtual ~NDivSlowdownTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    ClockUtil     *pClkHal;
    ThreeDAlloc   m_3dAlloc;
};

//! \brief NDivSlowdownTest constructor.
//!
//! Initializes member fields.
//!
//! \sa Setup
//------------------------------------------------------------------------------
NDivSlowdownTest::NDivSlowdownTest()
{
    SetName("NDivSlowdownTest");
    pClkHal = NULL;
    m_3dAlloc.SetOldestSupportedClass(KEPLER_A);
}

//! \brief NDivSlowdownTest destructor.
//!
//! Lwrrently does nothing.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
NDivSlowdownTest::~NDivSlowdownTest()
{
   return;
}

//! \brief Checks whether the test can be run.
//!
//! This test is only supported on KEPLER and later GPUs.
//!
//! \return RUN_RMTEST_TRUE if the test can be run in the current environment,
//!         skip reason otherwise
//------------------------------------------------------------------------------
string NDivSlowdownTest::IsTestSupported()
{
    if (!m_3dAlloc.IsSupported(GetBoundGpuDevice()))
    {
        return "Test is supported only for KEPLER and later.";
    }

    //
    // There is no point in running this test in fmodel because, in general,
    // NDIV Slowdown/Sliding is not enabled on the VBIOSes used in fmodel.
    // If the MODS infrastructure allowed us to call into RM here, we would
    // do so (instead of checking for fmodel) and return negative if this
    // feature is disabled.
    //
    if (Platform::GetSimulationMode() == Platform::Fmodel)
    {
        return "Test is not supported on F-Model.";
    }

    return RUN_RMTEST_TRUE;
}

//! \brief Test setup.
//!
//! Create the clock object used to test with.
//
//! \return DEVICE_NOT_FOUND if unable to create the clock object.
//!         OK otherwise.
//------------------------------------------------------------------------------
RC NDivSlowdownTest::Setup()
{
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle hClient;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    hClient = pLwRm->GetClientHandle();

    // Create the clock object.
    pClkHal = ClockUtil::CreateModule(hClient, GetBoundGpuSubdevice());
    if (!pClkHal)
    {
        Printf(Tee::PriHigh,
            "%d: Clock hal creation failure\n",__LINE__);
        rc = RC::DEVICE_NOT_FOUND;
    }

    return rc;
}

//! \brief Runs the test.
//!
//! Wrapper for the NDIV slowdown clock test.
//!
//! \return A specific RC error is returned if the test fails.
//          OK otherwise.
//------------------------------------------------------------------------------
RC NDivSlowdownTest::Run()
{
    vector<LW2080_CTRL_CLK_INFO> vClkInfo;
    RC rc;

    Printf(Tee::PriHigh, "Test start!\n");

    // Runs the test.
    CHECK_RC(pClkHal->ClockNDivSlowdownTest());

    Printf(Tee::PriHigh, "Test end!\n");

    return rc;
}

//! \brief Test cleanup.
//!
//! Destroy the clock object used to test with.
//
//! \return OK.
//------------------------------------------------------------------------------
RC NDivSlowdownTest::Cleanup()
{
    if (pClkHal)
        delete pClkHal;
    return OK;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(NDivSlowdownTest, RmTest,
    "Wrapper for the NDIV slowdown clock test.");

