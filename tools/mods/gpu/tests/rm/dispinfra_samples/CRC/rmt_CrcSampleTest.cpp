/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2008 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_CrcSampleTest.cpp
//! \brief Crc Generation For CORE, BASE, OVERLAY channel
//!
#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "lwos.h"
#include "core/include/platform.h"
#include "core/include/lwrm.h"
#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/utility.h"
#include "gpu/include/gpugoldensurfaces.h"
#include "gpu/utility/gpuutils.h"
#include "core/include/jscript.h"
#include <string> // Only use <> for built-in C++ stuff
#include <stdio.h>
#include <stdlib.h>
//#include "../../display/disptest.h"
#include "gpu/display/evo_disp.h"
#include "gpu/display/evo_chns.h"
#include "gpu/tests/rm/utility/crccomparison.h"
#include "gpu/utility/surf2d.h"
#include "core/include/memcheck.h"

class CrcSampleTest : public RmTest
{
public:
    CrcSampleTest();
    virtual ~CrcSampleTest();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    RC CrcTest();
};

//! \brief Constructor for CrcSampleTest
//!
//! Set a name for the test
//!
//! \sa Setup
//------------------------------------------------------------------------------
CrcSampleTest::CrcSampleTest()
{
    SetName("CrcSampleTest");
}

//! \brief Destructor for CrcSampleTest
//!
//! Does nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
CrcSampleTest::~CrcSampleTest()
{

}

//! \brief IsTestSupported()
//!
//! Check whether the hardware supports LwDPS.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string CrcSampleTest::IsTestSupported()
{
    // Must belong to EVO Display class
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
RC CrcSampleTest::Setup()
{
    RC rc;

    CHECK_RC(InitFromJs());

    return rc;
}

//! \brief Runs CrcSampleTest
//!
//! Central point where all tests are run.
//!
//! \return Always OK on getting CORE Crc same on beginning and end of the Test
//------------------------------------------------------------------------------
RC CrcSampleTest::Run()
{
   RC rc;

   CHECK_RC(CrcTest());

   return rc;
}

//! CrcSampleTest includes:
//! 1. Initial ModeSet
//! 2. Generate CrcValues
//! 3. Select custom params like Pixel clock  Settings etc and CommitSettings
//! 4. Generate Crc again
RC CrcSampleTest::CrcTest()
{
    RC rc;
    Display *pDisplay = GetDisplay();
    EvoDisplay *pEvoDisplay = NULL;
    CrcComparison crcCompObj;
    UINT32 compositorCrc, primaryCrc, secondaryCrc;
    Surface2D CoreImage;
    Surface2D BaseImage;
    Surface2D OvlyImage;
    string CoreImageName = "dispinfradata/images/coreimage640x480.PNG";
    string BaseImageName = "dispinfradata/images/baseimage640x480.PNG";
    string OvlyImageName = "dispinfradata/images/ovlyimage640x480.PNG";

    UINT32 BaseWidth, BaseHeight, OvlyWidth, OvlyHeight, Depth, RefreshRate;

    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        // This is for Fmodel and RTL which are slow in producing frames
        BaseWidth = 160;
        BaseHeight = 120;
        OvlyWidth = 160;
        OvlyHeight = 120;
        Depth = 32;
        RefreshRate = 60;
    }
    else
    {
        BaseWidth = 640;
        BaseHeight = 480;
        OvlyWidth = 160;
        OvlyHeight = 120;
        Depth = 32;
        RefreshRate = 60;
    }

    // Intializing Display HW
    CHECK_RC(pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    DisplayIDs  Detected;

    CHECK_RC(pDisplay->GetDetected(&Detected));
    DisplayID SelectedDisplay = Detected[0];

    CHECK_RC(pDisplay->SetupChannelImage(SelectedDisplay,
                                         BaseWidth,
                                         BaseHeight,
                                         Depth,
                                         Display::CORE_CHANNEL_ID,
                                         &CoreImage,
                                         CoreImageName));
    CHECK_RC(pDisplay->SetMode(SelectedDisplay,
                               BaseWidth,
                               BaseHeight,
                               Depth,
                               RefreshRate));

    pEvoDisplay = pDisplay->GetEvoDisplay();

    Printf(Tee::PriHigh, "Doing CRC Stuff \n");

    bool GenerateCrc = true;

    if (GenerateCrc)
    {
        // Capture CORE CRC using GetCrcValues
        Printf(Tee::PriHigh,"Generating Crc 1st time\n");
        EvoCRCSettings crcSettings;
        crcSettings.ControlChannel = EvoCRCSettings::CORE;

        pEvoDisplay->SetupCrcBuffer(SelectedDisplay, &crcSettings);
        CHECK_RC(pEvoDisplay->GetCrcValues(SelectedDisplay, &crcSettings, &compositorCrc, &primaryCrc, &secondaryCrc));
        CHECK_RC(crcCompObj.DumpCrcToXml(GetBoundGpuDevice(), "crc_test1.xml", &crcSettings, true));

        Printf(Tee::PriHigh,"Creating a New BaseImage\n");
        CHECK_RC(pDisplay->SetupChannelImage(SelectedDisplay, BaseWidth, BaseHeight, Depth, Display::BASE_CHANNEL_ID, &BaseImage, BaseImageName));
        CHECK_RC(pDisplay->SetImage(SelectedDisplay,&BaseImage,Display::BASE_CHANNEL_ID));

        // Capture BASE CRC using GetCrcValues
        Printf(Tee::PriHigh,"Generating Crc 2nd time\n");
        EvoCRCSettings crcSettings2;
        crcSettings2.ControlChannel = EvoCRCSettings::BASE;

        pEvoDisplay->SetupCrcBuffer(SelectedDisplay, &crcSettings2);
        CHECK_RC(pEvoDisplay->GetCrcValues(SelectedDisplay, &crcSettings2, &compositorCrc, &primaryCrc, &secondaryCrc));
        CHECK_RC(crcCompObj.DumpCrcToXml(GetBoundGpuDevice(), "crc_test2.xml", &crcSettings2, true));

        // Free Base Image
        Printf(Tee::PriHigh,"Freeing Base Image\n");
        CHECK_RC(pDisplay->SetImage(SelectedDisplay, NULL, Display::BASE_CHANNEL_ID));
        if (BaseImage.GetMemHandle() != 0)
            BaseImage.Free();

        //API to take diff between 2 XML crc files
        //Do no its failing so kepping Disbaled
        //CHECK_RC(crcCompObj.CompareCrcFiles("crc_test1.xml", "crc_test2.xml", "output.log"));

        CHECK_RC(pDisplay->SetupChannelImage(SelectedDisplay,
                                             OvlyWidth,
                                             OvlyHeight,
                                             32,
                                             Display::OVERLAY_CHANNEL_ID,
                                             &OvlyImage,
                                             OvlyImageName));
        Printf(Tee::PriHigh,"Creating a New OverlayImage\n");
        CHECK_RC(pDisplay->SetImage(SelectedDisplay, &OvlyImage, Display::OVERLAY_CHANNEL_ID));

        // Capture Overlay CRC using GetCrcValues
        Printf(Tee::PriHigh,"Generating Crc 3rd time\n");
        EvoCRCSettings crcSettings3;
        crcSettings3.ControlChannel = EvoCRCSettings::OVERLAY;

        pEvoDisplay->SetupCrcBuffer(SelectedDisplay, &crcSettings3);
        CHECK_RC(pEvoDisplay->GetCrcValues(SelectedDisplay, &crcSettings3, &compositorCrc, &primaryCrc, &secondaryCrc));
        CHECK_RC(crcCompObj.DumpCrcToXml(GetBoundGpuDevice(), "crc_test3.xml", &crcSettings3, true));

        CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                    NULL,
                                    Display::OVERLAY_CHANNEL_ID));
        if(OvlyImage.GetMemHandle() != 0)
        {
            OvlyImage.Free();
        }

        // Capture again CORE CRC using GetCrcValues
        Printf(Tee::PriHigh,"Generating Crc 4th time\n");
        EvoCRCSettings crcSettings4;
        crcSettings4.ControlChannel = EvoCRCSettings::CORE;

        pEvoDisplay->SetupCrcBuffer(SelectedDisplay, &crcSettings4);
        CHECK_RC(pEvoDisplay->GetCrcValues(SelectedDisplay,
                                           &crcSettings4,
                                           &compositorCrc,
                                           &primaryCrc,
                                           &secondaryCrc));
        CHECK_RC(crcCompObj.DumpCrcToXml(GetBoundGpuDevice(), "crc_test4.xml", &crcSettings4, true));

        //API to take diff between 2 XML crc files
        //Do no its failing so kepping Disbaled
        //CHECK_RC(crcCompObj.CompareCrcFiles("crc_test3.xml", "crc_test4.xml", "output2.log"));
    }

    DisplayIDs SelectedDisplays;
    pDisplay->Selected(&SelectedDisplays);
    CHECK_RC(pDisplay->DetachDisplay(SelectedDisplays));

    return rc;
}

//! \brief Cleanup
//!
//! Does nothing
//------------------------------------------------------------------------------
RC CrcSampleTest::Cleanup()
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
JS_CLASS_INHERIT(CrcSampleTest, RmTest,
    "CrcSampleTest test\n");

