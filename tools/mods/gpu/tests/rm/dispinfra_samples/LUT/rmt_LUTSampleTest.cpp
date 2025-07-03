/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2010 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_LUTSampleTest.cpp
//! \brief SetLUT functionality test
//!
#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/utility.h"
#include "core/include/jscript.h"
#include <string> // Only use <> for built-in C++ stuff
#include "lwos.h"
#include "gpu/display/evo_disp.h"
#include "gpu/display/evo_chns.h"
#include "core/include/platform.h"
#include "core/include/xp.h"
#include "core/include/memcheck.h"

class LUTSampleTest : public RmTest
{
public:
    LUTSampleTest();

    virtual ~LUTSampleTest();
    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    RC TestLUTandGamma();

};

//! \brief Constructor for LUTSampleTest
//!
//! Set a name for the test
//!
//! \sa Setup
//------------------------------------------------------------------------------
LUTSampleTest::LUTSampleTest()
{
    SetName("LUTSampleTest");
}

//! \brief Destructor for LUTSampleTest
//!
//! Does nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
LUTSampleTest::~LUTSampleTest()
{

}

//! \brief IsTestSupported()
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string LUTSampleTest::IsTestSupported()
{
    // Must support raster extension
    Display *pDisplay = GetDisplay();
    if( pDisplay->GetDisplayClassFamily() == Display::EVO )
        return RUN_RMTEST_TRUE;
    return "EVO Display class is not supported on current platform";
}

//! \brief Setup
//!
//! Just JS settings
//
//! \return RC -> OK if everything is allocated,
//------------------------------------------------------------------------------
RC LUTSampleTest::Setup()
{
    RC rc;

    CHECK_RC(InitFromJs());

    return rc;
}

//! \brief Ilwoke TestLUTandGamma Method
//!
//! Central point where all tests are run.
//!
//! \return Always OK since we have only one test and it's a visual test.
//------------------------------------------------------------------------------
RC LUTSampleTest::Run()
{
    RC rc;

    TestLUTandGamma();

    return rc;
}

//! \brief Cleanup
//!
//! Does nothing
//------------------------------------------------------------------------------
RC LUTSampleTest::Cleanup()
{
    return OK;
}

RC LUTSampleTest::TestLUTandGamma()
{
    RC rc;
    UINT32             BaseWidth;
    UINT32             BaseHeight;
    UINT32             OvlyWidth;
    UINT32             OvlyHeight;
    UINT32             LwrsorWidth;
    UINT32             LwrsorHeight;
    UINT32             SetmodeWidth;
    UINT32             SetmodeHeight;
    UINT32             SetmodeDepth;
    UINT32             SetmodeRefreshRate;

    LwU16              j;
    Surface2D          CoreImage;
    Surface2D          BaseImage;
    Surface2D          LwrsorImage;
    Surface2D          OverlayImage;
    vector<UINT32>TestLut;
    Display *pDisplay = GetDisplay();

    string BaseImageName = "dispinfradata/images/baseimage640x480.PNG";
    string OvlyImageName = "dispinfradata/images/ovlyimage640x480.PNG";
    string LwrsorImageName = "dispinfradata/images/lwrsorimage32x32.PNG";
    string CoreImageName = "dispinfradata/images/coreimage640x480.PNG";
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        // This is for Fmodel and RTL which are slow in producing frames
        BaseWidth    = 160;
        BaseHeight   = 120;
        OvlyWidth = BaseWidth;
        OvlyHeight = BaseHeight;
        LwrsorWidth = 32;
        LwrsorHeight = 32;
        SetmodeWidth  = BaseWidth;
        SetmodeHeight = BaseHeight;
        SetmodeDepth  = 32;
        SetmodeRefreshRate = 60;
    }

    else
    {
        BaseWidth  = 800;
        BaseHeight = 600;
        OvlyWidth  = 300;
        OvlyHeight = 250;
        LwrsorWidth  = 32;
        LwrsorHeight = 32;
        SetmodeWidth  = 800;
        SetmodeHeight = 600;
        SetmodeDepth  = 32;
        SetmodeRefreshRate = 60;

    }

    DisplayIDs Detected;

    CHECK_RC(pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));
    CHECK_RC(pDisplay->GetDetected(&Detected));
    DisplayID SelectedDisplay = Detected[0];

    CHECK_RC(pDisplay->SetupChannelImage(SelectedDisplay, SetmodeWidth, SetmodeHeight, SetmodeDepth, Display::CORE_CHANNEL_ID, &CoreImage, CoreImageName));
    CHECK_RC(pDisplay->SetMode(SelectedDisplay,SetmodeWidth, SetmodeHeight, SetmodeDepth, SetmodeRefreshRate));

    EvoDisplay *pEvoDisplay = pDisplay->GetEvoDisplay();

    Printf(Tee::PriHigh,"CREATE OVERLAY IMAGE\n");
    CHECK_RC(pDisplay->SetupChannelImage(SelectedDisplay,
                                         OvlyWidth,
                                         OvlyHeight,
                                         SetmodeDepth,
                                         Display::OVERLAY_CHANNEL_ID,
                                         &OverlayImage,
                                         OvlyImageName));
    CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                &OverlayImage,
                                Display::OVERLAY_CHANNEL_ID));

    Printf(Tee::PriHigh,"CREATE CURSOR IMAGE \n");
    CHECK_RC(pDisplay->SetupChannelImage(SelectedDisplay,
                                         LwrsorWidth,
                                         LwrsorHeight,
                                         SetmodeDepth,
                                         Display::LWRSOR_IMM_CHANNEL_ID,
                                         &LwrsorImage,
                                         LwrsorImageName));

    CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                &LwrsorImage,
                                Display::LWRSOR_IMM_CHANNEL_ID));

    //*************** Set LUT Pallette ***************
    //Pallette can be configured by the end user
    //Creating a sample LUT pallette
    for (j = 0; j < 256; j++)
    {
        TestLut.push_back( ((UINT32)U8_TO_LUT(j)) << 16 | U8_TO_LUT(j) );  // Green, Red
        TestLut.push_back( (UINT32)U8_TO_LUT(j) );  // Blue
    }

    //257th value = 256th value
    TestLut.push_back( (((UINT32)U8_TO_LUT(j-1)) << 16) | U8_TO_LUT(j-1) );  // Green, Red
    TestLut.push_back( (UINT32)U8_TO_LUT(j-1) );  // Blue

    EvoLutSettings LutSettings;
    //SetupLUTBuffer creates LUT buffer and initialises it with the user specified LUT Pallette
    //This should always be the first method ilwoked for any variable of type EvoLutSettings
    CHECK_RC(pEvoDisplay->SetupLUTBuffer(SelectedDisplay, &LutSettings, &TestLut[0],(UINT32)TestLut.size()));

    //SetLUT Only After Base Is Active
    //SetLUT binds LUTBuffer to LUT Context DMA
    //SetLUT must be preceeded by SetupLUTBuffer
    //Once this is ilwoked then next time onwards just ilwoking
    //SetupLUTBuffer will update LUT.
    pEvoDisplay->SetLUT(SelectedDisplay, &LutSettings);

    Printf(Tee::PriHigh,"CREATE OVERLAY IMAGE\n");
    CHECK_RC(pDisplay->SetupChannelImage(SelectedDisplay,
                                         OvlyWidth,
                                         OvlyHeight,
                                         SetmodeDepth,
                                         Display::OVERLAY_CHANNEL_ID,
                                         &OverlayImage,
                                         OvlyImageName));
    CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                &OverlayImage,
                                Display::OVERLAY_CHANNEL_ID));

    Printf(Tee::PriHigh,"CREATE CURSOR IMAGE \n");
    CHECK_RC(pDisplay->SetupChannelImage(SelectedDisplay,
                                         LwrsorWidth,
                                         LwrsorHeight,
                                         SetmodeDepth,
                                         Display::LWRSOR_IMM_CHANNEL_ID,
                                         &LwrsorImage,
                                         LwrsorImageName));

    CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                &LwrsorImage,
                                Display::LWRSOR_IMM_CHANNEL_ID));

    // Change gamma settings For each R, G, B, color independently
    // Changing Gamma For RED
    for(int gamma=0;gamma < 4; gamma++)
    {
        CHECK_RC(pEvoDisplay->SetGamma(SelectedDisplay, &LutSettings, gamma, 0, 0));
    }

    //Reset Pallete to remove RED channel changes
    CHECK_RC(pEvoDisplay->SetupLUTBuffer(SelectedDisplay, &LutSettings, &TestLut[0],
        (UINT32)TestLut.size()));
    //Changing Gamma For Green
    for(int gamma=0;gamma < 4; gamma++)
    {
        CHECK_RC(pEvoDisplay->SetGamma(SelectedDisplay, &LutSettings, 0, gamma, 0));
    }

    //Reset Pallete to remove GREEN channel changes
    CHECK_RC(pEvoDisplay->SetupLUTBuffer(SelectedDisplay, &LutSettings, &TestLut[0],
        (UINT32)TestLut.size()));
    //Changing Gamma For Blue
    for(int gamma=0;gamma < 4; gamma++)
    {
        CHECK_RC(pEvoDisplay->SetGamma(SelectedDisplay, &LutSettings, 0, 0, gamma));
    }

    //Reset Pallete to original pallete to remove BLUE channel changes
    CHECK_RC(pEvoDisplay->SetupLUTBuffer(SelectedDisplay, &LutSettings, &TestLut[0],
        (UINT32)TestLut.size()));

    //Free LUT
    //To Remove LUT use this method
    //Use this only if neccessary.
    //This is provided for flexible control to end user over LUT usage.
    //However Infra will remove LUT on shutdown So this can be ignored.
    //Usage: Set LutSettings.Dirty = false; and then ilwoke SetLUT.

    LutSettings.Dirty = false;
    LutSettings.pLutBuffer=NULL;
    pEvoDisplay->SetLUT(SelectedDisplay, &LutSettings);

    // Free Cursor
    CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                NULL,
                                Display::LWRSOR_IMM_CHANNEL_ID));

    if(LwrsorImage.GetMemHandle() != 0)
        LwrsorImage.Free();

    // Free Overlay
    CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                NULL,
                                Display::OVERLAY_CHANNEL_ID));

    if(OverlayImage.GetMemHandle() != 0)
        OverlayImage.Free();

    //Now demonstrating SetLUT on Base Surface
    Printf(Tee::PriHigh,"CREATE BASE IMAGE \n");
    CHECK_RC(pDisplay->SetupChannelImage(SelectedDisplay,
                                         BaseWidth,
                                         BaseHeight,
                                         SetmodeDepth,
                                         Display::BASE_CHANNEL_ID,
                                         &BaseImage,
                                         BaseImageName));

    CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                &BaseImage,
                                Display::BASE_CHANNEL_ID));

    Printf(Tee::PriHigh,"CREATE CURSOR IMAGE \n");
    CHECK_RC(pDisplay->SetupChannelImage(SelectedDisplay,
                                         LwrsorWidth,
                                         LwrsorHeight,
                                         SetmodeDepth,
                                         Display::LWRSOR_IMM_CHANNEL_ID,
                                         &LwrsorImage,
                                         LwrsorImageName));

    CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                &LwrsorImage,
                                Display::LWRSOR_IMM_CHANNEL_ID));

    EvoLutSettings LutSettings2;
    CHECK_RC(pEvoDisplay->SetupLUTBuffer(SelectedDisplay, &LutSettings2, &TestLut[0],(UINT32)TestLut.size()));

    pEvoDisplay->SetLUT(SelectedDisplay, &LutSettings2);

    // Free LUT
    LutSettings2.Dirty = false;
    LutSettings2.pLutBuffer=NULL;
    pEvoDisplay->SetLUT(SelectedDisplay, &LutSettings2);

    // Free the Cursor Image
    Printf(Tee::PriHigh,"Freeing Cursor Image\n");
    CHECK_RC(pDisplay->SetImage(SelectedDisplay, NULL, Display::LWRSOR_IMM_CHANNEL_ID));

    // Free Base Image
    Printf(Tee::PriHigh,"Freeing Base Image\n");
    CHECK_RC(pDisplay->SetImage(SelectedDisplay, NULL, Display::BASE_CHANNEL_ID));

    if (LwrsorImage.GetMemHandle() != 0)
        LwrsorImage.Free();
    if (BaseImage.GetMemHandle() != 0)
        BaseImage.Free();

    DisplayIDs SelectedDisplays;
    pDisplay->Selected(&SelectedDisplays);
    CHECK_RC(pDisplay->DetachDisplay(SelectedDisplays));

    return rc;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(LUTSampleTest, RmTest,
    "LUT Sample test\n");

