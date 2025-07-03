/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2013,2015-2017,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_lwdps.cpp
//! \brief LwDPS functionality test
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
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include "core/include/xp.h"
//#include "../../display/disptest.h"
#include "gpu/display/evo_disp.h"
#include "gpu/display/evo_chns.h"
#include "gpu/tests/rm/utility/crccomparison.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "core/include/memcheck.h"
#include "ctrl/ctrl0073.h"

using namespace DTIUtils;

class TestDisp : public RmTest
{
public:
    TestDisp();

    virtual ~TestDisp();
    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    RC LwstomModeSetTest();

    RC RandomDraw();
    RC TestDPOnSIM();
    DisplayID FindDPDisplayID();
    RC SetupImage
    (
        string ImageName,
        UINT32 ImageWidth,
        UINT32 ImageHeight,
        Surface2D *pImage
    );

    RC GetEdidBufFromFile(string path,UINT08 *&edidData, UINT32 *edidSize);
    int gethex(char ch1);
    void FindNReplace(string &content,string findstr, string repstr);
    RC ApplyLwstomTimings();
    RC HeadBasedSetMode();
    RC PlayWithOverlay();
    RC CoreBaseSwitch();

private:

     GpuTestConfiguration    m_TestConfig;
     bool  m_LwstomModeset;

     Channel         *pDmaCopyChannel;

};

//! \brief Constructor for TestDisp
//!
//! Set a name for the test
//!
//! \sa Setup
//------------------------------------------------------------------------------
TestDisp::TestDisp()
{
    SetName("TestDisp");
}

//! \brief Destructor for TestDisp
//!
//! Does nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
TestDisp::~TestDisp()
{

}

//! \brief IsTestSupported()
//!
//! Check whether the hardware supports LwDPS.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string TestDisp::IsTestSupported()
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
RC TestDisp::Setup()
{
    RC rc;

    CHECK_RC(InitFromJs());

    return rc;
}

//! \brief Runs each LwDPS test
//!
//! Central point where all tests are run.
//!
//! \return Always OK since we have only one test and it's a visual test.
//------------------------------------------------------------------------------
RC TestDisp::Run()
{
   RC rc;

   m_LwstomModeset = false;

// CHECK_RC(RandomDraw());
//
// CHECK_RC(ApplyLwstomTimings());
//
// CHECK_RC(HeadBasedSetMode());
//
// CHECK_RC(TestDPOnSIM());

   CHECK_RC(CoreBaseSwitch());
   return rc;
}

RC TestDisp::LwstomModeSetTest()
{
    // Test includes
    //1. LwstomModeset + Initial ModeSet ... Like
    // 1. Default Modset.
    // 2. Select the all other Default params and Pixel clock  Settings are need to be changed n do modeset on Core.
    // 3. After Initial ModeSet with some Custom params.

    RC rc;
    Display *pDisplay = GetDisplay();
    DisplayIDs Displays;
    UINT32 Head = 0;
    EvoCoreChnContext *pCoreDispContext = NULL;
    EvoDisplayChnContext *pCoreContext = NULL;
    EvoDisplay *pEvoDisplay = NULL;
    CrcComparison crcCompObj;
    UINT32 compositorCrc, primaryCrc, secondaryCrc;

    // Used for Intializing Display HW
    // this is must ...

    CHECK_RC(pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    // normal Modeset
    CHECK_RC(pDisplay->GetDetected(&Displays));

    pDisplay->SetMode(Displays,640,480,32,60);

    pEvoDisplay = pDisplay->GetEvoDisplay();

    Printf(Tee::PriHigh, "Doing CRC Stuff \n");

    CHECK_RC_MSG(pEvoDisplay->GetHead(Displays[0], &Head),
                            "Failed to get head.\n");

    bool DontDoStuff = false;

    if (DontDoStuff)
    {
        EvoCRCSettings crcSettings;
        crcSettings.ControlChannel = EvoCRCSettings::CORE;

        pEvoDisplay->SetupCrcBuffer(Displays[0], &crcSettings);
        CHECK_RC(pEvoDisplay->GetCrcValues(Displays[0], &crcSettings, &compositorCrc, &primaryCrc, &secondaryCrc));
        CHECK_RC(crcCompObj.DumpCrcToXml(GetBoundGpuDevice(), "crc_test1.xml", &crcSettings, true));

        pCoreContext = pEvoDisplay->GetEvoDisplayChnContext(Displays[0], Display::CORE_CHANNEL_ID);
        pCoreDispContext = (EvoCoreChnContext *)pCoreContext;
        EvoCoreDisplaySettings &LwstomSettings = pCoreDispContext->DispLwstomSettings;

        LwstomSettings.HeadSettings[0].Reset();
        LwstomSettings.HeadSettings[0].PixelClkSettings.PixelClkFreqHz = 4000 * 10000;
        LwstomSettings.HeadSettings[0].PixelClkSettings.Adj1000Div1001 = false;
        LwstomSettings.HeadSettings[0].PixelClkSettings.ClkMode = EvoPixelClockSettings::CLK_LWSTOM;
        LwstomSettings.HeadSettings[0].PixelClkSettings.Dirty = true;
        LwstomSettings.HeadSettings[0].Dirty = true;
        pCoreDispContext->CommitSettings();

        // Another way to capture CRC using GetCrcValues
        EvoCRCSettings crcSettings2;
        crcSettings2.ControlChannel = EvoCRCSettings::CORE;

        pEvoDisplay->SetupCrcBuffer(Displays[0], &crcSettings2);
        CHECK_RC(pEvoDisplay->GetCrcValues(Displays[0], &crcSettings2, &compositorCrc, &primaryCrc, &secondaryCrc));
        CHECK_RC(crcCompObj.DumpCrcToXml(GetBoundGpuDevice(), "crc_test2.xml", &crcSettings2, true));

        // API to take diff between 2 XML crc files
        CHECK_RC(crcCompObj.CompareCrcFiles("crc_test1.xml", "crc_test2.xml", "output.log"));

    }

    bool bAlreadyDid = true;

    /************************************************************************/
    /******************Custom modeset ***************************************/
    /***********************************************************************/
    // Example ModeSet using Only changing Pixel Clock Settings on Desired Display
    //Custom modeset
    if(!bAlreadyDid)
    {
        pCoreContext =
             pEvoDisplay->GetEvoDisplayChnContext(Displays[0], Display::CORE_CHANNEL_ID);

        pCoreDispContext = (EvoCoreChnContext *)(pCoreContext);

        EvoCoreDisplaySettings &LwstomSettings = pCoreDispContext->DispLwstomSettings;

        for(UINT32 i = 0; i < 2 ; i++)
        {
            LwstomSettings.HeadSettings[i].Reset();
        }
        LwstomSettings.HeadSettings[Head].PixelClkSettings.PixelClkFreqHz = 4000 * 10000;
        LwstomSettings.HeadSettings[Head].PixelClkSettings.Adj1000Div1001 = false;
        LwstomSettings.HeadSettings[Head].PixelClkSettings.ClkMode = EvoPixelClockSettings::CLK_LWSTOM;
        LwstomSettings.HeadSettings[Head].PixelClkSettings.Dirty = true;

        LwstomSettings.HeadSettings[Head].Dirty = true;

        pCoreDispContext->CommitSettings();
    }

    /**************************************************************************/
    /************** Free Display resources **********************************/
    /**************************************************************************/

    /* Freeing automatic if we explictly free then Allocated Display Resources will get freed or else frame work will
       Take care of that */

    //pEvoDisplay->FreeDispResources();
    return rc;
}

//! \brief Cleanup
//!
//! Does nothing
//------------------------------------------------------------------------------
RC TestDisp::Cleanup()
{
    if (pDmaCopyChannel)
    {
        // Free the channel and its objects.
        m_TestConfig.FreeChannel(pDmaCopyChannel);
        pDmaCopyChannel = 0;
    }

    return OK;
}

RC TestDisp::SetupImage
(
    string ImageName,
    UINT32 ImageWidth,
    UINT32 ImageHeight,
    Surface2D *pImage
)
{
    RC rc;
    UINT32 Depth = 32;
    if (pImage->GetMemHandle() == 0)
    {
        pImage->SetWidth(ImageWidth);
        pImage->SetHeight(ImageHeight);
        pImage->SetColorFormat(ColorUtils::ColorDepthToFormat(Depth));
        pImage->SetDisplayable(true);
        pImage->SetLocation (Memory::Fb);
        pImage->SetAddressModel(Memory::Paging);
        pImage->SetType(LWOS32_TYPE_IMAGE);
        CHECK_RC(pImage->Alloc(GetBoundGpuDevice()));
     }

    if (pImage->GetMemHandle() != 0)
        if (pImage->ReadPng(ImageName.c_str(), 0) != OK)
            pImage->Fill(0x00004000, 0);

    return rc;
}

RC TestDisp::RandomDraw()
{
    RC rc;

    Surface2D CoreImage[2];
    Surface2D BaseImage[2];
    Surface2D OvlyImage[2];
    Surface2D LwrsorImage[2];

    DisplayIDs Detected;
    Display *pDisplay = GetDisplay();

    CHECK_RC(pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    CHECK_RC(pDisplay->GetDetected(&Detected));
    Printf(Tee::PriHigh, "Detected displays  = \n");
    pDisplay->PrintDisplayIds(Tee::PriHigh, Detected);

    UINT32  BaseWidth  = 160;
    UINT32  BaseHeight = 120;
    UINT32  Depth =32;
    UINT32  RefreshRate =60;
    UINT32 OvlyWidth = 160;
    UINT32 OvlyHeight = 120;
    UINT32 LwrsorWidth = 32;
    UINT32 LwrsorHeight = 32;
    string BaseImageName = "baseimage";
    string OvlyImageName = "ovlyimage";
    string LwrsorImageName = "lwrsorimage";
    bool DetachDisplays = true;

    // Find out what is Usable Head Count
    const UINT32 numHeads = Utility::CountBits(pDisplay->GetUsableHeadsMask());

    // Writing for only for 2 heads .. juts a test

    for(UINT32 i =0 ; i < Detected.size() ; ++i)
    {
        Printf(Tee::PriHigh, "============================================\n");
        Printf(Tee::PriHigh, " SetMode # %d  on displayId 0x%08x\n",i,UINT32(Detected[i]));
        Printf(Tee::PriHigh, "============================================\n");

        DisplayID SelectedDisplay = Detected[i];

        if(i >= numHeads)
        {
            Printf(Tee::PriNormal," all heads are busy \n");
            break;
        }
        CHECK_RC(pDisplay->SetupChannelImage(SelectedDisplay, BaseWidth, BaseHeight, 32, Display::CORE_CHANNEL_ID, &CoreImage[i], "beau.PNG"));
        CHECK_RC(pDisplay->SetMode(SelectedDisplay, BaseWidth, BaseHeight, Depth, RefreshRate));

        if (OK != rc)
        {
            Printf(Tee::PriHigh, "In function %s, SetMode#%d on displayId 0x%08x failed with args "
                "width = %u, height = %u, depth = %u, RR = %u",
                __FUNCTION__, i, UINT32(SelectedDisplay), BaseWidth, BaseHeight, Depth ,RefreshRate);

        }

        string NewImageName = BaseImageName + ".PNG";

        CHECK_RC(pDisplay->SetupChannelImage(SelectedDisplay, BaseWidth, BaseHeight, 32, Display::BASE_CHANNEL_ID, &BaseImage[i], NewImageName));

        Printf(Tee::PriHigh,"Creating a New BaseImage\n");
        CHECK_RC(pDisplay->SetImage(SelectedDisplay, &BaseImage[i], Display::BASE_CHANNEL_ID));

        NewImageName = OvlyImageName + ".PNG";

        CHECK_RC(pDisplay->SetupChannelImage(SelectedDisplay, OvlyWidth, OvlyHeight, 32, Display::OVERLAY_CHANNEL_ID, &OvlyImage[i], NewImageName));

        Printf(Tee::PriHigh,"Creating a New OverlayImage\n");
        CHECK_RC(pDisplay->SetImage(SelectedDisplay, &OvlyImage[i], Display::OVERLAY_CHANNEL_ID));

        NewImageName = LwrsorImageName + ".PNG";

        CHECK_RC(pDisplay->SetupChannelImage(SelectedDisplay, LwrsorWidth, LwrsorHeight, 32, Display::LWRSOR_IMM_CHANNEL_ID, &LwrsorImage[i], NewImageName));

        Printf(Tee::PriHigh,"Creating a New LwrsorImage\n");
        CHECK_RC(pDisplay->SetImage(SelectedDisplay, &LwrsorImage[i], Display::LWRSOR_IMM_CHANNEL_ID));

    }

    for(UINT32 i =0 ; i < Detected.size() ; ++i)
    {
        char num[2];
        DisplayID SelectedDisplay = Detected[i];

        if(i >= numHeads)
        {
            Printf(Tee::PriNormal," all heads are busy \n");
            break;
        }

        for(UINT32 j = 1 ; j < 4 ; j ++)
        {
            sprintf(num,"%d", j);
            string NewImageName= BaseImageName + string(num);
            NewImageName += ".PNG";

            Printf(Tee::PriHigh,"Copying the Image : %s  to BaseSurface\n",NewImageName.c_str());
            CHECK_RC(BaseImage[i].ReadPng(NewImageName.c_str(),0));
            CHECK_RC(pDisplay->SetImage(SelectedDisplay, &BaseImage[i], Display::BASE_CHANNEL_ID));

            NewImageName = OvlyImageName + string(num);
            NewImageName+= ".PNG";

            Printf(Tee::PriHigh,"Copying the Image : %s to OverlaySurface\n",NewImageName.c_str());
            CHECK_RC(OvlyImage[i].ReadPng(NewImageName.c_str(),0));
            CHECK_RC(pDisplay->SetImage(SelectedDisplay, &OvlyImage[i], Display::OVERLAY_CHANNEL_ID));

            NewImageName = LwrsorImageName + string(num);
            NewImageName+= ".PNG";

            Printf(Tee::PriHigh,"Copying the Image : %s  to LwrsorSurface\n",NewImageName.c_str());
            CHECK_RC(LwrsorImage[i].ReadPng(NewImageName.c_str(),0));
            CHECK_RC(pDisplay->SetImage(SelectedDisplay, &LwrsorImage[i], Display::LWRSOR_IMM_CHANNEL_ID));

            // Move the Cursor and Ovly API's
            UINT32 x = 100+ (5 * i);
            UINT32 y = 100+ (5 * i);

            Printf(Tee::PriHigh,"Moving the Overlay to (x,y):(%d,%d) \n",x,y);
            CHECK_RC(pDisplay->SetOverlayPos(SelectedDisplay,x,y));

            Printf(Tee::PriHigh," Moving the Cursor  to (x,y):(%d,%d) \n",x,y);
            CHECK_RC(pDisplay->SetLwrsorPos(SelectedDisplay,x,y));
        }
    }

    if(DetachDisplays)
    {
        // Now Detach Displays
        DisplayIDs ActiveDisplays;
        pDisplay->Selected(&ActiveDisplays);
        CHECK_RC(pDisplay->DetachDisplay(ActiveDisplays));
    }
    else
    {
        // Free in Normal way ..
        for(UINT32 i = 0 ; i < Detected.size() ; i++)
        {
            DisplayID SelectedDisplay = Detected[i];

            if(i >= numHeads)
            {
                Printf(Tee::PriNormal," all heads are busy \n");
                break;
            }

            // Free the Cursor Image
            Printf(Tee::PriHigh,"Freeing Cursor Image\n");
            CHECK_RC(pDisplay->SetImage(SelectedDisplay, NULL, Display::LWRSOR_IMM_CHANNEL_ID));

            // Free Overlay Image
            Printf(Tee::PriHigh,"Freeing Overlay Image \n");
            CHECK_RC(pDisplay->SetImage(SelectedDisplay, NULL, Display::OVERLAY_CHANNEL_ID));

            // Free Base Image
            Printf(Tee::PriHigh,"Freeing Base Image\n");
            CHECK_RC(pDisplay->SetImage(SelectedDisplay, NULL, Display::BASE_CHANNEL_ID));
        }
    }

   // now Free the Image Surfaces .. chekc them but
    for (UINT32 i = 0; i < numHeads ;i ++)
    {
        if (CoreImage[i].GetMemHandle() != 0)
            CoreImage[i].Free();
        if (BaseImage[i].GetMemHandle() != 0)
            BaseImage[i].Free();
        if (LwrsorImage[i].GetMemHandle() != 0)
            LwrsorImage[i].Free();
        if (OvlyImage[i].GetMemHandle() != 0)
            OvlyImage[i].Free();
    }

    return rc;
}

RC TestDisp::ApplyLwstomTimings()
{

    RC rc;
    UINT32 Width = 160;
    UINT32 Height = 120;
    UINT32 Depth = 32;
    UINT32 RefreshRate = 60;

    Surface2D CoreImage;
    DisplayIDs Detected;

    EvoCoreChnContext *pCoreDispContext = NULL;

    Display *pDisplay = GetDisplay();

    CHECK_RC(pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));
    CHECK_RC(pDisplay->GetDetected(&Detected));

    pCoreDispContext = (EvoCoreChnContext *)pDisplay->GetEvoDisplay()->
                            GetEvoDisplayChnContext(Detected[0], Display::CORE_CHANNEL_ID);
    EvoCoreDisplaySettings &LwstomSettings = pCoreDispContext->DispLwstomSettings;
    LwstomSettings.HeadSettings[0].Reset();
    EvoRasterSettings &LwstomRaster = LwstomSettings.HeadSettings[0].RasterSettings;

    LwstomRaster.RasterWidth  = Width + 26;
    LwstomRaster.ActiveX      = Width;
    LwstomRaster.SyncEndX     = 15;
    LwstomRaster.BlankStartX  = LwstomRaster.RasterWidth - 5;
    LwstomRaster.BlankEndX    = LwstomRaster.SyncEndX + 4;
    LwstomRaster.PolarityX    = false;
    LwstomRaster.RasterHeight = Height + 30;
    LwstomRaster.ActiveY      = Height;
    LwstomRaster.SyncEndY     = 0;
    LwstomRaster.BlankStartY  = LwstomRaster.RasterHeight - 2;
    LwstomRaster.BlankEndY    = LwstomRaster.SyncEndY + 24;
    LwstomRaster.Blank2StartY = 1;
    LwstomRaster.Blank2EndY   = 0;
    LwstomRaster.PolarityY    = false;
    LwstomRaster.Interlaced   = false;

    LwstomRaster.Dirty = true;

    // 135 MHZ
    LwstomSettings.HeadSettings[0].PixelClkSettings.PixelClkFreqHz = 135000000;
    LwstomSettings.HeadSettings[0].PixelClkSettings.Adj1000Div1001 = false;
    LwstomSettings.HeadSettings[0].PixelClkSettings.ClkMode = EvoPixelClockSettings::CLK_LWSTOM;
    LwstomSettings.HeadSettings[0].PixelClkSettings.NotDriver = false;
    LwstomSettings.HeadSettings[0].PixelClkSettings.Dirty = true;
    LwstomSettings.HeadSettings[0].Dirty = true;

    CHECK_RC(pDisplay->SetupChannelImage(Detected[0], Width, Height, Depth, Display::CORE_CHANNEL_ID, &CoreImage, "beau.PNG"));
    CHECK_RC(pDisplay->SetMode(Detected[0], Width, Height, Depth, RefreshRate));

    if(CoreImage.GetMemHandle() != 0)
    {
        CoreImage.Free();
    }

    DisplayIDs ActiveDisplays;
    CHECK_RC(pDisplay->Selected(&ActiveDisplays));
    CHECK_RC(pDisplay->DetachDisplay(ActiveDisplays));

    return rc;

}

RC TestDisp::TestDPOnSIM()
{
    RC rc;
    DisplayIDs supportedDisplays;
    Display *pDisplay = GetDisplay();
    CHECK_RC(pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    CHECK_RC(pDisplay->GetConnectors(&supportedDisplays));
    Printf(Tee::PriHigh, "All supported displays = \n");
    pDisplay->PrintDisplayIds(Tee::PriHigh, supportedDisplays);

    UINT08 *rawEdid = NULL;
    UINT32 edidSize = 0;
    Surface2D CoreImage;
    DisplayID DPDisplay = FindDPDisplayID();
    DisplayIDs FakeDisplays;

    string edidFile = "edids/edid_DP_HP.txt";   // found in MODS_RUNSPACE

//  // we need data in form of HEX Dump
//  CHECK_RC(Utility::AllocateLoadFile(edidFile.c_str(),
//                            rawEdid,
//                            &edidSize,
//                            Utility::HEX));

    CHECK_RC(GetEdidBufFromFile(edidFile, rawEdid, &edidSize));

    Printf(Tee::PriHigh, "Trying to fake display 0x%x\n", (UINT32) DPDisplay);

    // Call AttachFakeDevice() to fake this display
    CHECK_RC_CLEANUP(pDisplay->AttachFakeDevice(DPDisplay , rawEdid, edidSize));
    Printf(Tee::PriHigh, "Successfully faked display 0x%x\n", (UINT32) DPDisplay);

     CHECK_RC(pDisplay->SetupChannelImage(DPDisplay, 160, 120, 32, Display::CORE_CHANNEL_ID, &CoreImage, "beau.PNG"));
    CHECK_RC(pDisplay->SetMode(DPDisplay, 160, 120, 32, 60));

    // Display is faked successfully here, the user can do what he wants, setmode etc
    // Now remove the faked device

    FakeDisplays.push_back(DPDisplay);
    pDisplay->DetachAndRemoveFakeDevices();

Cleanup:
    if(CoreImage.GetMemHandle() != 0)
        CoreImage.Free();

    if(rawEdid)
    {
        delete rawEdid;
        rawEdid = NULL;
    }

    return rc;
}

int TestDisp:: gethex(char ch1)
{
    if (ch1 >= 'A' && ch1 <= 'F')
        return ch1 - 'A' + 10;
    if (ch1 >= 'a' && ch1 <= 'f')
        return ch1 - 'a' + 10;
    return ch1 - '0';
}

//! \brief this function gets Edid buf from given file
//!
//------------------------------------------------------------------------------
RC TestDisp::GetEdidBufFromFile(string path,UINT08 *&edidData, UINT32 *edidSize)
{

    RC rc;

    string edidbuf= "" ,tmpEdidBuf= "";

    ifstream *pEdidFile= new ifstream(path.c_str());

    if (!(pEdidFile->is_open()))
    {
        Printf(Tee::PriHigh,"Cannot open EDID file %s",
               path.c_str());
        rc = RC::CANNOT_OPEN_FILE;
        pEdidFile->close();
        delete pEdidFile;
        return rc;
    }

    size_t begin = pEdidFile->tellg();
    pEdidFile->seekg (0, ios::end);
    size_t end = pEdidFile->tellg();

    edidData  = new UINT08[end-begin];

    pEdidFile->seekg (0, ios::beg);

    while (getline(*pEdidFile, tmpEdidBuf))
    {
        edidbuf += tmpEdidBuf;
    }

    //
    //replace the EDID charecters with the ""
    //
    FindNReplace(edidbuf," ", "");
    FindNReplace(edidbuf,"\t", "");
    FindNReplace(edidbuf,"\n", "");
    FindNReplace(edidbuf,"\r", "");

    Printf(Tee::PriHigh," \n Edid Data Starts \n ");
    UINT32 i= 0,j = 0;
    for (i = 0,j = 0; i < edidbuf.size() ; i += 2)
    {
        edidData[j] = (UINT08)(gethex(edidbuf[i])* 16 + gethex(edidbuf[i + 1]));
        Printf(Tee::PriHigh,"0x%x,",edidData[j]);
        j++;
    }

    Printf(Tee::PriHigh," \n Edid data Completed \n");

    //fill the param Edid Size
    *edidSize = j;

    pEdidFile->close();
    delete pEdidFile;
    pEdidFile = NULL;

    return rc;
}

void TestDisp::FindNReplace(string &content,string findstr, string repstr)
{
    size_t j = 0;
    while (true)
    {
        j = content.find(findstr,j);
        if (j == string::npos)
        {
            break;
        }
        content.replace(j,findstr.size(),repstr);
    }
    return;
}

DisplayID TestDisp::FindDPDisplayID()
{
    RC rc;
    Display *pDisplay = GetDisplay();
    // Ask RM which OR should be used (and protocol) for this Display
    for (UINT32 i = 0 ; i < 32 ; i++ )
    {

        DisplayID FindDPDisplay = 1 << i;
        LW0073_CTRL_SPECIFIC_OR_GET_INFO_PARAMS params;
        memset(&params, 0, sizeof(params));
        params.subDeviceInstance = pDisplay->GetOwningDisplaySubdevice()->GetSubdeviceInst();
        params.displayId = FindDPDisplay;
        if (pDisplay->RmControl(LW0073_CTRL_CMD_SPECIFIC_OR_GET_INFO,
                                &params, sizeof(params)) != OK)
        {
            continue;
        }

        if ((params.type == LW0073_CTRL_SPECIFIC_OR_TYPE_SOR) &&
            (params.protocol == LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DP_A ||
             params.protocol == LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DP_B))
        {
            return FindDPDisplay;
        }
    }
    return 0;
}

RC TestDisp::HeadBasedSetMode()
{
    RC rc;

    Display *pDisplay = GetDisplay();

    CHECK_RC(pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    DisplayIDs Detected;
    CHECK_RC(pDisplay->GetDetected(&Detected));

    DisplayIDs ActiveDisplays;

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
        Width    = 640;
        Height   = 480;
        Depth = 32;
        RefreshRate  = 60;
    }

    bool case1 = true;
    if(case1)
    {
        DisplayID SetModeDisplay;
        for(UINT32 i = 0; i  < Detected.size(); i++)
        {
            SetModeDisplay = Detected[i];

            // Find out what is Usable Head Count
            UINT32 UsableHeadCount = Utility::CountBits(pDisplay->GetUsableHeadsMask());

            for( UINT32 head = 0; head < UsableHeadCount; head++)
            {
                Surface2D CoreImage;
                CHECK_RC(pDisplay->SetupChannelImage(SetModeDisplay,
                    Width,
                    Height,
                    Depth,
                    Display::CORE_CHANNEL_ID,
                    &CoreImage, "beau.PNG",0/*color*/,
                    head));

                CHECK_RC(pDisplay->SetMode(SetModeDisplay,
                    Width,
                    Height,
                    Depth,
                    RefreshRate,
                    head));

                CHECK_RC(pDisplay->DetachDisplay(DisplayIDs(1, SetModeDisplay)));

                if(CoreImage.GetMemHandle() != 0)
                    CoreImage.Free();
            }
        }
    }

    CHECK_RC(pDisplay->Selected(&ActiveDisplays));
    if(!ActiveDisplays.empty())
    {
        CHECK_RC(pDisplay->DetachDisplay(ActiveDisplays));
    }
    ActiveDisplays.clear();

    bool case2 = true;
    if(case2)
    {
        //// Second case like DoModeSet Upto Maximum Heads
        for(UINT32 i = 0; i  < Detected.size(); i++)
        {
            Surface2D CoreImage;

            rc = pDisplay->SetupChannelImage(Detected[i],
                Width,
                Height,
                Depth,
                Display::CORE_CHANNEL_ID,
                &CoreImage, "beau.PNG",0/*color*/);
            if(rc != OK)
            {
                if(rc == RC::ILWALID_HEAD)
                {
                    DisplayIDs ActiveDisplays;
                    CHECK_RC(pDisplay->Selected(&ActiveDisplays));
                    if(!ActiveDisplays.empty())
                    {
                        CHECK_RC(pDisplay->DetachDisplay(DisplayIDs(1, Detected[i-1])));
                        i --;
                    }
                    continue;
                }
            }

            rc = pDisplay->SetMode(Detected[i],
                Width,
                Height,
                Depth,
                RefreshRate
                );

            if( rc != OK)
            {
                if( rc == RC::DISPLAY_NO_FREE_HEAD)
                {
                    CHECK_RC(pDisplay->DetachDisplay(DisplayIDs(1, Detected[i-1])));
                    i --;
                }
            }

            if(CoreImage.GetMemHandle() != 0)
                CoreImage.Free();

        }
    }

    CHECK_RC(pDisplay->Selected(&ActiveDisplays));
    if(!ActiveDisplays.empty())
    {
        CHECK_RC(pDisplay->DetachDisplay(ActiveDisplays));
    }
    ActiveDisplays.clear();

    bool case3 = true;
    if(case3)
    {
        //// back to Back modesets .. need to resolve this issue ..
        for(UINT32 i = 0; i  < Detected.size(); i++)
        {
            rc = pDisplay->SetMode(Detected[i],
                Width,
                Height,
                Depth,
                RefreshRate
                );

            if( rc != OK)
            {
                if( rc == RC::DISPLAY_NO_FREE_HEAD)
                {
                    CHECK_RC(pDisplay->DetachDisplay(DisplayIDs(1, Detected[i-1])));
                    i --;
                }
                else
                {
                    CHECK_RC(rc);
                }
            }

            if (Platform::GetSimulationMode() == Platform::Hardware)
            {
                rc = pDisplay->SetMode(Detected[i],
                                       640,
                                       480,
                                       32,
                                       60);

                rc = pDisplay->SetMode(Detected[i],
                                       800,
                                       600,
                                       32,
                                       60);
            }

        }
    }

    CHECK_RC(pDisplay->Selected(&ActiveDisplays));
    if(!ActiveDisplays.empty())
    {
        CHECK_RC(pDisplay->DetachDisplay(ActiveDisplays));
    }

    return rc;
}

RC TestDisp::CoreBaseSwitch()
{
    RC rc;

    Display *pDisplay = GetDisplay();
    CHECK_RC(pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    DisplayIDs Detected;
    CHECK_RC(pDisplay->GetDetected(&Detected));
    UINT32 Width,Height,Depth,RefreshRate;

    Width    = 800;
    Height   = 600;
    Depth = 32;
    RefreshRate  = 60;

    ImageUtils imgArr1;
    imgArr1 = ImageUtils::SelectImage(800, 600);

    DisplayID SetModeDisplay = Detected[0];

    // Setmode
    Surface2D CoreImage;
    CHECK_RC(pDisplay->SetupChannelImage(SetModeDisplay,
        Width,
        Height,
        Depth,
        Display::CORE_CHANNEL_ID,
        &CoreImage,imgArr1.GetImageName()));

    CHECK_RC(pDisplay->SetMode(SetModeDisplay,
        Width,
        Height,
        Depth,
        RefreshRate));

    // Set BaseNormal
    Surface2D BaseImage;
    ImageUtils imgArr2;
    UINT32 baseWidth  = 640;
    UINT32 baseHeight = 480;
    UINT32 baseDepth = 32;
    imgArr2 = ImageUtils::SelectImage(baseWidth, baseHeight);

    CHECK_RC(pDisplay->SetupChannelImage(SetModeDisplay,
        baseWidth,
        baseHeight,
        baseDepth,
        Display::BASE_CHANNEL_ID,
        &BaseImage,imgArr2.GetImageName()));

    CHECK_RC(pDisplay->SetImage(SetModeDisplay, &BaseImage, Display::BASE_CHANNEL_ID));

    // Free base Normal
    CHECK_RC(pDisplay->SetImage(SetModeDisplay, NULL, Display::BASE_CHANNEL_ID));

    // Set Base Block Linear
    Surface2D BaseBLImage;
    BaseBLImage.SetLayout(Surface2D::BlockLinear);
    CHECK_RC(pDisplay->SetupChannelImage(SetModeDisplay,
        baseWidth,
        baseHeight,
        baseDepth,
        Display::BASE_CHANNEL_ID,
        &BaseBLImage,
        "",
        0));

    CHECK_RC(pDisplay->SetImage(SetModeDisplay, &BaseBLImage, Display::BASE_CHANNEL_ID));

    Width = 1024;
    Height = 768;

    // Again SetMode this time what will be shown..
    CHECK_RC(pDisplay->SetMode(SetModeDisplay,
        Width,
        Height,
        Depth,
        RefreshRate));

     CHECK_RC(pDisplay->SetImage(SetModeDisplay, &BaseImage, Display::BASE_CHANNEL_ID));

    Surface2D CoreImage2;
    UINT32 CoreWidth = 1280;
    UINT32 CoreHeight = 1024;

    imgArr1 = ImageUtils::SelectImage(CoreWidth, CoreHeight);
    CHECK_RC(pDisplay->SetupChannelImage(SetModeDisplay,
        CoreWidth,
        CoreHeight,
        baseDepth,
        Display::CORE_CHANNEL_ID,
        &CoreImage2,
        imgArr1.GetImageName()));

    CHECK_RC(pDisplay->SetImage(SetModeDisplay, &CoreImage2, Display::CORE_CHANNEL_ID));

    CHECK_RC(pDisplay->SetImage(SetModeDisplay, &BaseImage, Display::BASE_CHANNEL_ID));

    CHECK_RC(pDisplay->SetImage(SetModeDisplay, &CoreImage2, Display::CORE_CHANNEL_ID));

    CHECK_RC(pDisplay->SetImage(SetModeDisplay, &BaseImage, Display::BASE_CHANNEL_ID));

    return rc;
}
RC TestDisp::PlayWithOverlay()
{
  RC rc;

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
JS_CLASS_INHERIT(TestDisp, RmTest,
    "LwDPS 1.0 test\n");

