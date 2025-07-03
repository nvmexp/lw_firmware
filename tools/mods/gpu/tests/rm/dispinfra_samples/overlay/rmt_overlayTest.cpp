/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2009 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//!
//! \file rmt_overlayTest.cpp
//! \brief Sample test to demonstrate use of Overlay APIs.
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/display.h"
#include "lwos.h"
#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include "gpu/display/evo_disp.h"
#include "gpu/display/evo_chns.h"
#include "core/include/platform.h"
#include "core/include/xp.h"
#include "core/include/memcheck.h"

class OverlayTest : public RmTest
{
public:
    OverlayTest();
    virtual ~OverlayTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
};

//! \brief OverlayTest constructor
//!
//! Doesn't do much except SetName.
//!
//! \sa Setup
//------------------------------------------------------------------------------
OverlayTest::OverlayTest()
{
    SetName("OverlayTest");
}

//! \brief OverlayTest destructor
//!
//! Doesn't do much.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
OverlayTest::~OverlayTest()
{

}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment. Restricting this
//! test to Win32Display Class for now, i.e. only in Win32 MODS. This test is
//! DisplayID based, and overlay APIs in EvoDisplay class i.e. on WinMfgMODS
//! lwrrently do not have DisplayID support. That support will be added soon,
//! and this check for WIN32DISP class can be removed then.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string OverlayTest::IsTestSupported()
{
    Display *pDisplay = GetDisplay();

    if (pDisplay->GetDisplayClassFamily() == Display::WIN32DISP || pDisplay->GetDisplayClassFamily() == Display::EVO )
        return RUN_RMTEST_TRUE;
    return "EVO and WIN32DISP Dispaly classes are not supported on current platform";
}

//! \brief Just doing Init from Js, nothing else.
//!
//! \return if InitFromJs passed, else corresponding rc
//------------------------------------------------------------------------------
RC OverlayTest::Setup()
{
    RC rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    return rc;
}

//! \brief Run the test
//!
//! Start overlay, move overlay and free overlay.
//!
//! \return OK if the overlay APIs succeeded, corresponding RC otherwise
//------------------------------------------------------------------------------
RC OverlayTest::Run()
{
    RC rc;
    Display *pDisplay = GetDisplay();

    DisplayIDs selectedDisplays;
    DisplayID overlayDisplay;

    // Used for Intializing Display HW
    if (pDisplay->GetDisplayClassFamily() == Display::EVO)
    {
        CHECK_RC(pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));
    }
    // First see if some displays are selected
    CHECK_RC(pDisplay->Selected(&selectedDisplays));

    //
    // if no displays are selected, lets get all the connected displays,
    // then and start overlay on one of them.
    //
    if (selectedDisplays.size() == 0)
    {
        Printf(Tee::PriHigh, "No displays selected lwrrently, checking all connected displays\n");

        DisplayIDs allConnectedDisplays;
        CHECK_RC(pDisplay->GetDetected(&allConnectedDisplays));

        if (allConnectedDisplays.size() == 0)
        {
            Printf(Tee::PriHigh, "Strange.., no displays detected\n");
            return RC::LWRM_ILWALID_STATE;
        }

        //
        // some displays are present, lets pick one of them,
        // and start overlay on that.
        //
        overlayDisplay = allConnectedDisplays[0];
    }
    else
    {
       overlayDisplay = selectedDisplays[0];
    }

    UINT32 Width,Height,Depth,RefreshRate;

    UINT32 posX         = 0;
    UINT32 posY         = 0;

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

    Surface2D OvlyImage;
    Surface2D CoreImage;
    string OvlyImageName = "dispinfradata/images/ovlyimage640x480.PNG";
    string CoreImageName = "dispinfradata/images/coreimage640x480.PNG";

    if(pDisplay->GetDisplayClassFamily() == Display::WIN32DISP)
    {
        // Start overlay on this display. SetOverlayParameters() call starts overlay
        CHECK_RC(pDisplay->SetOverlayParameters(overlayDisplay,
                                                  posX,
                                                  posY,
                                                  Width,
                                                  Height,
                                                  0,
                                                  0,
                                                  0,
                                                  0));

        // lets render the overlay 100 times in a loop
        for(UINT32 loopCount = 0; loopCount < 100; loopCount++)
        {
            //
            // If an overlay is already running on a display, calling
            // SetOverlayParameters() will flip that overlay again.
            //
            CHECK_RC_CLEANUP(pDisplay->SetOverlayParameters(overlayDisplay,
                                                              0,
                                                              0,
                                                              0,
                                                              0,
                                                              0,
                                                              0,
                                                              0,
                                                              0));
         }
    }
    else if(pDisplay->GetDisplayClassFamily() == Display::EVO)
    {
        ///**************************************************************************/
        ///**************Set Image on Overlay Channel For EVO Class *****************/
        ///**************************************************************************/
        // normal Modeset
        CHECK_RC(pDisplay->SetupChannelImage(overlayDisplay,
                                     Width,
                                     Height,
                                     32,
                                     Display::CORE_CHANNEL_ID,
                                     &CoreImage,
                                     CoreImageName));

        CHECK_RC(pDisplay->SetMode(overlayDisplay,Width,Height,Depth,RefreshRate));

        CHECK_RC(pDisplay->SetupChannelImage(overlayDisplay,
                                     Width,
                                     Height,
                                     32,
                                     Display::OVERLAY_CHANNEL_ID,
                                     &OvlyImage,
                                     OvlyImageName));

        Printf(Tee::PriHigh,"Creating a New OverlayImage\n");
        CHECK_RC(pDisplay->SetImage(overlayDisplay,
                                    &OvlyImage,
                                    Display::OVERLAY_CHANNEL_ID));
    }

    /**************************************************************************/
    /*********************** Now move this overlay ****************************/
    /**************************************************************************/
    CHECK_RC_CLEANUP(pDisplay->SetOverlayPos(overlayDisplay,
                                               posX + 100,
                                               posY + 100));
    // Free Overlay
    Printf(Tee::PriHigh,"Destroy: Overlay\n");
    CHECK_RC(pDisplay->SetImage(overlayDisplay,
                                NULL,
                                Display::OVERLAY_CHANNEL_ID));

Cleanup:

    CHECK_RC(pDisplay->DetachDisplay(DisplayIDs(1,overlayDisplay)));
    if(OvlyImage.GetMemHandle() != 0)
    {
       OvlyImage.Free();
    }

    return rc;
}

//! \brief Cleanup, does nothing for this simple test
//------------------------------------------------------------------------------
RC OverlayTest::Cleanup()
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
JS_CLASS_INHERIT(OverlayTest, RmTest,
    "Test to demonstrate use of overlay APIs");

