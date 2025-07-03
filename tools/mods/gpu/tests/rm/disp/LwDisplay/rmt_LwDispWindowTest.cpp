/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2017, 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//!
//! \file rmt_LwDispWindowTest
//!       This test calls lwdisplay library APIs to verify window(s) on head(s)

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "gpu/display/lwdisplay/lw_disp.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "hwref/disp/v03_00/dev_disp.h" // LW_PDISP_FE_DEBUG_CTL
#include "lwmisc.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/display/modeset_utils.h"
using namespace std;

class LwDispWindowTest : public RmTest
{
public:
    LwDispWindowTest();
    virtual ~LwDispWindowTest();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(head, UINT32);
    SETGET_PROP(windows, string);

private:
    Display *m_pDisplay;
    UINT32 m_head;
    string m_windows;
};

LwDispWindowTest::LwDispWindowTest()
{
    m_pDisplay = GetDisplay();
    SetName("LwDispWindowTest");
    m_head = 0;
    m_windows = "0,1";
}

//! \brief LwDispBringupPass1 destructor
//!
//! does  nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
LwDispWindowTest::~LwDispWindowTest()
{
}

//! \brief IsSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string LwDispWindowTest::IsTestSupported()
{
    if (m_pDisplay->GetDisplayClassFamily() != Display::LWDISPLAY)
    {
            return "The test is supported only on LwDisplay class!";
    }

    return RUN_RMTEST_TRUE;
}

//! \setup Initialise internal state from JS
//!
//! Initial state setup based on the JS setting
//------------------------------------------------------------------------------
RC LwDispWindowTest::Setup()
{
    RC rc = OK;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    return rc;
}

RC LwDispWindowTest::Run()
{
    RC rc = OK;
    LwDisplay *pLwDisplay = m_pDisplay->GetLwDisplay();
    vector<WindowIndex> hWin;
    DisplayIDs workingSet;
    DisplayIDs DisplaysToUse;
    DisplayID  setModeDisplay;
    UINT32 width, height, refreshRate;
    vector <LwDispWindowSettings> winSettings;
    vector<string> windowNumString;
    UINT32 windowNum;

    // allocate core channel
    CHECK_RC(m_pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        // This is for Fmodel and RTL which are slow in producing frames
        width    = 320;
        height   = 320;
        refreshRate  = 60;
    }
    else
    {
        width    = 800;
        height   = 600;
        refreshRate  = 60;
    }

    //
    // As of now, we just have SINGLE_TMDS_A support on fmodel and this is always
    // forcefully detected on emu/fmodel so we dont need to fake the TMDS display.
    //
    CHECK_RC(m_pDisplay->GetDetected(&DisplaysToUse));
    Printf(Tee::PriHigh, "Detected displays  = \n");
    m_pDisplay->PrintDisplayIds(Tee::PriHigh, DisplaysToUse);

    if (DisplaysToUse.size() == 0)
    {
        Printf(Tee::PriHigh, "There is no display detected!\n");
        return RC::SOFTWARE_ERROR;
    }

    for (UINT32 i = 0; i < DisplaysToUse.size(); i++)
    {
        if (DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay, DisplaysToUse[i], "SINGLE_TMDS_A"))
        {
            //
            // if the edid is not valid for disp id then SetLwstomEdid.
            // This is needed on emulation / simulation / silicon (when kvm used)
            //
            if (! (DTIUtils::EDIDUtils::IsValidEdid(m_pDisplay, DisplaysToUse[i]) &&
                   DTIUtils::EDIDUtils::EdidHasResolutions(m_pDisplay, DisplaysToUse[i]))
               )
            {
                if((rc = DTIUtils::EDIDUtils::SetLwstomEdid(m_pDisplay, DisplaysToUse[i], true)) != OK)
                {
                    Printf(Tee::PriLow,"SetLwstomEdid failed for displayID = 0x%X \n",
                        (UINT32)DisplaysToUse[i]);
                    return rc;
                }
            }
            workingSet.push_back(DisplaysToUse[i]);
            break;
        }
    }

    if (workingSet.size() == 0)
    {
        Printf(Tee::PriHigh, "There is no TMDS display detected! Check VBIOS...\n");
        return RC::SOFTWARE_ERROR;
    }

    setModeDisplay = workingSet[0];

    windowNumString = DTIUtils::VerifUtils::Tokenize(m_windows,",");
    hWin.resize(windowNumString.size());
    winSettings.resize(windowNumString.size());

    for (UINT32 i = 0; i < windowNumString.size(); i++)
    {
        // Allocate the window channel for the given window instance
        windowNum = std::atoi(windowNumString[i].c_str());
        CHECK_RC(pLwDisplay->AllocateWinAndWinImm(&hWin[i], m_head, windowNum));

        // Allocate default surface to the given window
        winSettings[i].winIndex = hWin[i];
        CHECK_RC(pLwDisplay->SetupChannelImage(setModeDisplay, &winSettings[i]));
    }

    // SetMode on the display with the given windows
    CHECK_RC(pLwDisplay->SetMode(setModeDisplay, width, height, 32, refreshRate, m_head));

    for (UINT32 i = 0; i < windowNumString.size(); i++)
    {
        // Destroy created windows.
        CHECK_RC(pLwDisplay->DestroyWinAndWinImm(hWin[i]));
    }

    return rc;
}

//! \brief Cleanup, does nothing for this simple test
//------------------------------------------------------------------------------
RC LwDispWindowTest::Cleanup()
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
JS_CLASS_INHERIT(LwDispWindowTest, RmTest,
    "Simple test to demonstrate usage of SetMode() API");

CLASS_PROP_READWRITE(LwDispWindowTest, head, UINT32,
                     "head number to be used in the test");
CLASS_PROP_READWRITE(LwDispWindowTest, windows, string,
                     "window(s) to be attached to the given head");
