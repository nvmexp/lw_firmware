/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2015,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//!
//! \file rmt_simpleModeset.cpp
//! \brief Sample test to demonstrate use of SetMode() API.
//!.\It does a modeset on all the detected displays. After each modeset
//!.\completion, the test detaches that display and performs the same
//! \sequence on the other detected displays.
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/display.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "core/include/xp.h"
#include "core/include/memcheck.h"

class SimpleModeSetTest : public RmTest
{
public:
    SimpleModeSetTest();
    virtual ~SimpleModeSetTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();

    RC SetModeOnAllDetectedDisplays();

    virtual RC Cleanup();

    SETGET_PROP(useFakeDisplays, bool);

private:
    Display         *m_pDisplay;
    DisplayIDs      fakedDisplays;

    bool   m_useFakeDisplays;

};

//! \brief SimpleModeSetTest constructor
//!
//! Doesn't do much except SetName.
//!
//! \sa Setup
//------------------------------------------------------------------------------
SimpleModeSetTest::SimpleModeSetTest()
{
    SetName("SimpleModeSetTest");
    m_pDisplay = nullptr;
    m_useFakeDisplays = false;
}

//! \brief SimpleModeSetTest destructor
//!
//! Doesn't do much.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
SimpleModeSetTest::~SimpleModeSetTest()
{

}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string SimpleModeSetTest::IsTestSupported()
{
    Display *pDisplay = GetDisplay();

    if ((pDisplay->GetDisplayClassFamily() == Display::EVO) ||
        (pDisplay->GetDisplayClassFamily() == Display::LWDISPLAY))
    {
        return RUN_RMTEST_TRUE;
    }

    return "EVO Display class is not supported on current platform";
}

//! \brief Just doing Init from Js, nothing else.
//!
//! \return if InitFromJs passed, else corresponding rc
//------------------------------------------------------------------------------
RC SimpleModeSetTest::Setup()
{
    RC rc;

    m_pDisplay = GetDisplay();

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    return rc;
}

//! \brief Run the test
//!
//! Call the SetMode() API with some default values
//!
//! \return OK if the SetMode call succeeded, else corresponding rc
//------------------------------------------------------------------------------
RC SimpleModeSetTest::Run()
{
    RC rc;

    CHECK_RC(m_pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    CHECK_RC(SetModeOnAllDetectedDisplays());

    return rc;
}

RC SimpleModeSetTest::SetModeOnAllDetectedDisplays()
{
    RC rc;
    DisplayIDs Detected;
    DisplayIDs Supported;
    DisplayIDs DisplaysToUse;

    // Stress Starts here
    CHECK_RC(m_pDisplay->GetConnectors(&Supported));
    Printf(Tee::PriHigh, "All supported displays = \n");
    m_pDisplay->PrintDisplayIds(Tee::PriHigh, Supported);

    CHECK_RC(m_pDisplay->GetDetected(&Detected));
    Printf(Tee::PriHigh, "Detected displays  = \n");
    m_pDisplay->PrintDisplayIds(Tee::PriHigh, Detected);

    UINT32 Width,Height,Depth,RefreshRate;

    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        // This is for Fmodel and RTL which are slow in producing frames
        Width    = 160;
        Height   = 120;
        Depth = 32;
        RefreshRate  = 60;
    }
    else
    {
        Width    = 800;
        Height   = 600;
        Depth = 32;
        RefreshRate  = 60;
    }

    if (m_useFakeDisplays)
    {
        CHECK_RC(DTIUtils::EDIDUtils::FakeDisplay(m_pDisplay, "SINGLE_TMDS_A", Supported, &DisplaysToUse));
    }
    else
    {
        DisplaysToUse = Detected;
    }

    for(UINT32 i =0 ; i < DisplaysToUse.size() ; ++i)
    {
        Printf(Tee::PriHigh, "============================================\n");
        Printf(Tee::PriHigh, " SetMode # %d  on displayId 0x%08x\n",i,UINT32(DisplaysToUse[i]));
        Printf(Tee::PriHigh, "============================================\n");

        rc = m_pDisplay->SetMode(DisplaysToUse[i],Width, Height, Depth, RefreshRate);

        if (OK != rc)
        {
            Printf(Tee::PriHigh, "In function %s, SetMode#%d on displayId 0x%08x failed with args "
                  "width = %u, height = %u, depth = %u, RR = %u",
                  __FUNCTION__, i, UINT32(DisplaysToUse[i]), Width, Height, Depth ,RefreshRate);
        }

        rc = m_pDisplay->DetachDisplay(DisplayIDs(1, DisplaysToUse[i]));

        if (OK != rc)
        {
            Printf(Tee::PriHigh, "In function %s, DetachDisplay on displayId 0x%08x failed!",
                  __FUNCTION__, UINT32(DisplaysToUse[i]));
        }

    }

    return rc;
}

//! \brief Cleanup, does nothing for this simple test
//------------------------------------------------------------------------------
RC SimpleModeSetTest::Cleanup()
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
JS_CLASS_INHERIT(SimpleModeSetTest, RmTest,
    "Simple test to demonstrate usage of SetMode() API");

CLASS_PROP_READWRITE(SimpleModeSetTest, useFakeDisplays, bool,
                     "run on only supported faked displays, default = 0");
