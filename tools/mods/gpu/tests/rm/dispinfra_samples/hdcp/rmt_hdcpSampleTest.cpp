/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2009,2014,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//!
//! \file rmt_hdcpSampleTest.cpp
//! \brief Sample test to demonstrate use of HDCP related MODS APIs.
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/display.h"
#include "core/include/memcheck.h"

class HdcpSampleTest : public RmTest
{
public:
    HdcpSampleTest();
    virtual ~HdcpSampleTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    Display*        m_pDisplay;
    DisplayID       m_DisplayToTest;

};

//! \brief HdcpSampleTest constructor
//!
//! Initalize private member variables to default values.
//!
//! \sa Setup
//------------------------------------------------------------------------------
HdcpSampleTest::HdcpSampleTest()
{
    SetName("HdcpSampleTest");
    m_pDisplay = nullptr;
    m_DisplayToTest = 0;
}

//! \brief HdcpSampleTest destructor
//!
//! Doesn't do much.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
HdcpSampleTest::~HdcpSampleTest()
{

}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string HdcpSampleTest::IsTestSupported()
{
    //
    // Lets first find out if any of the connected displays
    // support HDCP and also if the GPU supports HDCP
    //
    RC          rc;
    DisplayIDs  allConnectedDisplays;
    // Get all connected displays

    m_pDisplay = GetDisplay();
    CHECK_RC_CLEANUP(m_pDisplay->GetDetected(&allConnectedDisplays));

    if (allConnectedDisplays.size() == 0)
    {
        return "Strange.., no displays detected";
    }

    bool isDisplayCapable, isGpuCapable;
    for (UINT32 loopCount = 0; loopCount < allConnectedDisplays.size(); ++loopCount)
    {
        CHECK_RC_CLEANUP(m_pDisplay->GetHDCPInfo(allConnectedDisplays[loopCount], NULL,
                            &isDisplayCapable, &isGpuCapable, NULL, NULL, true));

        // if GPU itself is not capable, no need to go further
        if (!isGpuCapable)
        {
            return "GPU not capable of HDCP";
        }

        //
        // if this display is capable, save it and break from here. We got at least
        // one display which is HDCP capable, we'll run our tests on this display.
        //
        if (isDisplayCapable)
        {
            m_DisplayToTest = allConnectedDisplays[loopCount];
            break;
        }
    }

    // if we couldn't find any HDCP capable displays, print msg and return false
    if (m_DisplayToTest == 0)
    {
        return "No displays found which are capable of HDCP";
    }

    return RUN_RMTEST_TRUE;

Cleanup:
    // We failed somewhere, print the RC we got and return false.
    Printf(Tee::PriHigh, "Function %s failed, rc = %s", __FUNCTION__, rc.Message());
    return rc.Message();
}

//! \brief Just doing Init from Js, nothing else.
//!
//! \return if InitFromJs passed, else corresponding rc
//------------------------------------------------------------------------------
RC HdcpSampleTest::Setup()
{
    RC rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    m_pDisplay = GetDisplay();

    return rc;
}

//! \brief Run the test
//!
//! Enable HDCP on a HDCP enabled display, and get RM's callwlated value of Kp.
//!
//! \return OK if the HDCP APIs succeeded, else corresponding rc
//------------------------------------------------------------------------------
RC HdcpSampleTest::Run()
{
    RC rc;

    if (m_DisplayToTest == 0)
    {
        // This should never happen
        Printf(Tee::PriHigh, "We're in %s function, no HDCP capable displays saved "
            "IsTestSupported() should've saved at least one HDCP capable display\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    UINT64 cN   = 0x0867530908675309LL;  // Arbitrary integer
    UINT64 cKsv = 0xfffff;               // Arbitrary number with 20 1's

    // Enable HDCP on HDCP capable display
    CHECK_RC(m_pDisplay->RenegotiateHDCP(m_DisplayToTest, cN, cKsv, 0));

    // Now get HDCP data to verify Kp = Kp'
    UINT32 linkCount, numAps;
    UINT64 cS;
    vector <UINT64> status, aN, aKsv, bKsv, bKsvList, dKsv, kP;

    CHECK_RC(m_pDisplay->GetHDCPLinkParams(m_DisplayToTest, cN, cKsv, &linkCount, &numAps, &cS,
                            &status, &aN, &aKsv, &bKsv, &dKsv, &bKsvList, &kP));

    //
    // At this point, the test has all the info needed to apply HDCP algorithm
    // and callwlate Kp. It can then match with the Kp value obtained from RM
    // and declare PASS/FAIL
    //

    return rc;
}

//! \brief Cleanup, does nothing for this simple test
//------------------------------------------------------------------------------
RC HdcpSampleTest::Cleanup()
{
    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(HdcpSampleTest, RmTest,
    "Simple test to demonstrate usage of HDCP related APIs in MODS");

