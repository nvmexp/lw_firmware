/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2009, 2018,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_ColorFormatN64bitTest.cpp
//! \brief Test to check all the color formats supported by our graphics card..
//! 1) This test loops through all the combinations of colorformats on "Core + Overlay + Diff cursor size" and
//! all the combinations of colorformats on "Base + Overlay + Diff cursor size"
//! 2) Tests 64 bit SetMode on Core , Base and 64 bit SetImage on Overlay.
//! 3) This test also tests the different Cursor sizes 32x32, 64x64, 128x128, 256x256
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
#include "gpu/tests/rm/utility/dtiutils.h"
#include "core/include/memcheck.h"

class ColorFormatN64bitTest : public RmTest
{
public:
    ColorFormatN64bitTest();
    virtual ~ColorFormatN64bitTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(onlyConnectedDisplays,bool);
    SETGET_PROP(protocol, string);   //Config protocol through cmdline
    SETGET_PROP(rastersize, string); //Config raster size through cmdline
    SETGET_PROP(bpc, string);        //Config bpc (bits per color component) through cmdline

private:

    bool        m_onlyConnectedDisplays;
    string      m_protocol;
    string      m_rastersize;
    string      m_bpc;

    RC TestAllSurfaceForColorFormat(DisplayID setModeDisplay,
                                UINT32 Width,
                                UINT32 Height,
                                UINT32 Depth,
                                UINT32 RefreshRate,
                                vector<ColorUtils::Format> colorFormats,
                                vector<ColorUtils::Format> ovlyColorFormats);
};

//! \brief ColorFormatN64bitTest constructor
//!
//! Doesn't do much except SetName and initialising the member variables with the default values.
//!
//! \sa Setup
//------------------------------------------------------------------------------
ColorFormatN64bitTest::ColorFormatN64bitTest()
{
    SetName("ColorFormatN64bitTest");
    m_onlyConnectedDisplays = false;
    m_protocol = "CRT,TV,LVDS_LWSTOM,SINGLE_TMDS_A,SINGLE_TMDS_B,DUAL_TMDS,EXT_TMDS_ENC";
    m_rastersize = "";
    m_bpc = "8";
}

//! \brief ColorFormatN64bitTest destructor
//!
//! Doesn't do much.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
ColorFormatN64bitTest::~ColorFormatN64bitTest()
{

}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment. Restricting this
//! test to EvoDisplay Class for now. Lwrrently all Kepler Color formats isnt supported on Win32Display.
//! That support will be added soon, and this check for EvoDisplay class can be removed then.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string ColorFormatN64bitTest::IsTestSupported()
{
    Display *pDisplay = GetDisplay();

    if (pDisplay->GetDisplayClassFamily() == Display::EVO )
        return RUN_RMTEST_TRUE;
    return "EVO Display classes are not supported on current platform";
}

//! \brief Just doing Init from Js, nothing else.
//!
//! \return if InitFromJs passed, else corresponding rc
//------------------------------------------------------------------------------
RC ColorFormatN64bitTest::Setup()
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
RC ColorFormatN64bitTest::Run()
{
    RC rc;
    Display            *pDisplay = GetDisplay();
    DisplayIDs          selectedDisplays;
    DisplayID           setModeDisplay;

    vector<ColorUtils::Format> coreColorFormat8Bit;
    coreColorFormat8Bit.push_back(ColorUtils::A8R8G8B8);
    coreColorFormat8Bit.push_back(ColorUtils::A8B8G8R8);

    vector<ColorUtils::Format> coreColorFormat10Bit;
    coreColorFormat10Bit.push_back(ColorUtils::A2B10G10R10);
    coreColorFormat10Bit.push_back(ColorUtils::A2R10G10B10);
    coreColorFormat10Bit.push_back(ColorUtils::X2BL10GL10RL10_XRBIAS);
    coreColorFormat10Bit.push_back(ColorUtils::X2BL10GL10RL10_XVYCC);

    vector<ColorUtils::Format> coreColorFormat16Bit;
    coreColorFormat16Bit.push_back(ColorUtils::R16_G16_B16_A16);
    coreColorFormat16Bit.push_back(ColorUtils::R16_G16_B16_A16_LWBIAS);

    vector<ColorUtils::Format> ovlyColorFormat8Bit;
    ovlyColorFormat8Bit.push_back(ColorUtils::A8R8G8B8);

    vector<ColorUtils::Format> ovlyColorFormat10Bit;
    ovlyColorFormat10Bit.push_back(ColorUtils::A2B10G10R10);
    ovlyColorFormat10Bit.push_back(ColorUtils::A2R10G10B10);
    ovlyColorFormat10Bit.push_back(ColorUtils::X2BL10GL10RL10_XRBIAS);

    vector<ColorUtils::Format> ovlyColorFormat16Bit;
    ovlyColorFormat16Bit.push_back(ColorUtils::R16_G16_B16_A16);
    ovlyColorFormat16Bit.push_back(ColorUtils::R16_G16_B16_A16_LWBIAS);

    // Used for Intializing Display HW
    CHECK_RC(pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    // get all the connected displays
    DisplayIDs Detected;
    CHECK_RC(pDisplay->GetDetected(&Detected));
    Printf(Tee::PriHigh, "All Detected displays = \n");
    pDisplay->PrintDisplayIds(Tee::PriHigh, Detected);

    DisplayIDs workingSet;

    //Only the protocol passed by the user must be used
    for (UINT32 i=0; i < Detected.size(); i++)
    {
        if(DTIUtils::VerifUtils::IsSupportedProtocol(pDisplay,
                                                  Detected[i],
                                                  m_protocol))
        {
            //
            // if the edid is not valid for disp id then SetLwstomEdid.
            // This is needed on emulation / simulation / silicon (when kvm used)
            //
            if (! (DTIUtils::EDIDUtils::IsValidEdid(pDisplay, Detected[i]) &&
                   DTIUtils::EDIDUtils::EdidHasResolutions(pDisplay, Detected[i]))
               )
            {
                if((rc = DTIUtils::EDIDUtils::SetLwstomEdid(pDisplay, Detected[i], true)) != OK)
                {
                    Printf(Tee::PriLow,"SetLwstomEdid failed for displayID = 0x%X \n",
                        (UINT32)Detected[i]);
                    return rc;
                }
            }
            workingSet.push_back(Detected[i]);
        }
    }

    Printf(Tee::PriHigh, "\n Detected displays with protocol %s \n",m_protocol.c_str());
    pDisplay->PrintDisplayIds(Tee::PriHigh, workingSet);

    //If !m_onlyConnectedDisplays then consider fake displays too
    if(!m_onlyConnectedDisplays)
    {
        //get all the available displays
        DisplayIDs supported;
        CHECK_RC(pDisplay->GetConnectors(&supported));

        DisplayIDs displaysToFake;
        for(UINT32 loopCount = 0; loopCount < supported.size(); loopCount++)
        {
            if (!pDisplay->IsDispAvailInList(supported[loopCount], Detected) &&
                DTIUtils::VerifUtils::IsSupportedProtocol(pDisplay, supported[loopCount], m_protocol))
            {
                displaysToFake.push_back(supported[loopCount]);
            }
        }

        for(UINT32 loopCount = 0; loopCount < displaysToFake.size(); loopCount++)
        {
            CHECK_RC(DTIUtils::EDIDUtils::SetLwstomEdid(pDisplay,displaysToFake[loopCount], false));
            workingSet.push_back(displaysToFake[loopCount]);
        }

    }

    //get the no of displays to work with
    UINT32 numDisps = static_cast<UINT32>(workingSet.size());

    if (numDisps == 0)
    {
        Printf(Tee::PriHigh, "No displays detected, test aborting... \n");
        return RC::SOFTWARE_ERROR;
    }

    UINT32 width;
    UINT32 height;
    UINT32 depth;
    UINT32 refreshRate;
    bool   useSmallRaster = false;

    if (m_rastersize.compare("") == 0)
    {
        useSmallRaster = (Platform::GetSimulationMode() != Platform::Hardware);
    }
    else
    {
        useSmallRaster = (m_rastersize.compare("small") == 0);
        if (!useSmallRaster && m_rastersize.compare("large") != 0)
        {
            // Raster size param must be either "small" or "large".
            Printf(Tee::PriHigh, "%s: Bad raster size argument: \"%s\". Treating as \"large\"\n", __FUNCTION__, m_rastersize.c_str());
        }
    }

    if (useSmallRaster)
    {
        // This is for Fmodel and RTL which are slow in producing frames
        width    = 160;
        height   = 120;
        depth    = 32;
        refreshRate  = 60;
    }
    else
    {
        width    = 640;
        height   = 480;
        depth    = 32;
        refreshRate = 60;
    }

    bool b8bitMonitorConnected = false;
    bool b10bitMonitorConnected = false;
    bool b16bitMonitorConnected = false;

    if ( m_bpc.compare("") == 0 )
    {
        Printf(Tee::PriHigh, "%s: Bpc argument cannot be NULL. Treating as \"8 bit bpc\"\n", __FUNCTION__);
        m_bpc = "8";
    }

    vector<string> bpcNeeded = DTIUtils::VerifUtils::Tokenize(m_bpc, ",");

     for(UINT32 i=0; i < bpcNeeded.size(); i++)
     {
        if ( m_bpc.compare("8") == 0 )
            b8bitMonitorConnected = true;

        if ( m_bpc.compare("10") == 0 )
            b10bitMonitorConnected = true;

        if ( m_bpc.compare("16") == 0 )
            b16bitMonitorConnected = true;
     }

    for(UINT32 disp = 0 ; disp < numDisps ; disp++)
    {
        setModeDisplay = workingSet[disp];

        if(b8bitMonitorConnected)
        {
            Printf(Tee::PriHigh, "%s: Testing \"8 bit bpc\" color Formats\n", __FUNCTION__);
            CHECK_RC(TestAllSurfaceForColorFormat(setModeDisplay,
                width,
                height,
                depth,
                refreshRate,
                coreColorFormat8Bit,
                ovlyColorFormat8Bit));
        }

        if(b10bitMonitorConnected)
        {
            Printf(Tee::PriHigh, "%s: Testing \"10 bit bpc\" color Formats\n", __FUNCTION__);
            CHECK_RC(TestAllSurfaceForColorFormat(setModeDisplay,
                width,
                height,
                depth,
                refreshRate,
                coreColorFormat10Bit,
                ovlyColorFormat10Bit));
        }

        if(b16bitMonitorConnected)
        {
 
            Printf(Tee::PriHigh, "%s: Testing \"16 bit bpc\" color Formats\n", __FUNCTION__);
            CHECK_RC(TestAllSurfaceForColorFormat(setModeDisplay,
                width,
                height,
                64,         // 64 bit SetMode Depth for 16 bit bpc
                refreshRate,
                coreColorFormat16Bit,
                ovlyColorFormat16Bit));
        }
    }

    return rc;
}

RC ColorFormatN64bitTest::TestAllSurfaceForColorFormat(DisplayID setModeDisplay,
                                                       UINT32 Width,
                                                       UINT32 Height,
                                                       UINT32 Depth,
                                                       UINT32 RefreshRate,
                                                       vector<ColorUtils::Format> coreColorFormats,
                                                       vector<ColorUtils::Format> ovlyColorFormats)
{
    RC rc;
    Surface2D OvlyImage;
    Surface2D CoreImage;
    Surface2D BaseImage;
    Surface2D LwrsorImage;
    string OvlyImageName = "dispinfradata/images/ovlyimage640x480.PNG";
    string CoreImageName = "dispinfradata/images/coreimage640x480.PNG";
    string BaseImageName = "dispinfradata/images/baseimage640x480.PNG";
    string LwrsorImageName = "dispinfradata/images/image32x32.png";
    char lwrsorImgName[256];
    UINT32              posX         = 0;
    UINT32              posY         = 0;
    Display *pDisplay = GetDisplay();

    // Creating Cursor Image
    UINT32 LwrsorWidth[]  = {32, 64, 128, 256};
    UINT32 LwrsorHeight[] = {32, 64, 128, 256};
    UINT32 lwrX = 80;
    UINT32 lwrY = 80;

    for(UINT32 coreColorFormatIndex = 0; coreColorFormatIndex < coreColorFormats.size(); coreColorFormatIndex++)
    {
        // 32/64 bit depth Modeset
        CHECK_RC(pDisplay->SetupChannelImage(setModeDisplay,
            Width,
            Height,
            Depth,
            Display::CORE_CHANNEL_ID,
            &CoreImage,
            CoreImageName,
            0,
            Display::ILWALID_HEAD,
            (EvoColor)0,//EvoColor BaseColor
            (EvoColor)0,//EvoColor OverscanColor,
            Surface2D::Pitch,
            coreColorFormats[coreColorFormatIndex]));

        Printf(Tee::PriHigh, "%s: || Doing SetMode on Core Surface | DisplayId = %X | Width = %d | Height = %d | Depth = %d | Refresh Rate = %d | color Format = %d  ||\n",
                             __FUNCTION__, (UINT32)setModeDisplay, Width, Height, Depth, RefreshRate, coreColorFormats[coreColorFormatIndex]);
        CHECK_RC(pDisplay->SetMode(setModeDisplay,Width,Height,Depth,RefreshRate));

        for(UINT32 ovlyColorFormatIndex = 0; ovlyColorFormatIndex < ovlyColorFormats.size(); ovlyColorFormatIndex++)
        {
            Printf(Tee::PriHigh, "%s: || Creating Overlay Image  | DisplayId = %X | Width = %d | Height = %d | Depth = %d  | color Format = %d  ||\n",
                             __FUNCTION__, (UINT32)setModeDisplay, Width, Height, Depth, ovlyColorFormats[ovlyColorFormatIndex]);
            CHECK_RC(pDisplay->SetupChannelImage(setModeDisplay,
                Width,
                Height,
                Depth,
                Display::OVERLAY_CHANNEL_ID,
                &OvlyImage,
                OvlyImageName,
                0,
                Display::ILWALID_HEAD,
                (EvoColor)0,//EvoColor BaseColor
                (EvoColor)0,//EvoColor OverscanColor,
                Surface2D::Pitch,
                ovlyColorFormats[ovlyColorFormatIndex]));
            CHECK_RC(pDisplay->SetImage(setModeDisplay,
                &OvlyImage,
                Display::OVERLAY_CHANNEL_ID));

            // For GK104 and above all 4 cursor sizes are valid..
            // Lower chips (no longer supported) only 32 bit and 64 bit are valid
            UINT32 ValidLwrsorSize = 4;

            for(UINT32 lwrsorDimension = 0 ; lwrsorDimension < ValidLwrsorSize; lwrsorDimension++)
            {
                sprintf(lwrsorImgName,"dispinfradata/images/image%dx%d.png",
                    LwrsorWidth[lwrsorDimension],
                    LwrsorHeight[lwrsorDimension]);
                LwrsorImageName = string(lwrsorImgName);

                Printf(Tee::PriHigh, "%s: || Creating Cursor Image | DisplayId = %X | Width = %d | Height = %d | Depth = %d | \n",
                             __FUNCTION__, (UINT32)setModeDisplay, LwrsorWidth[lwrsorDimension], LwrsorHeight[lwrsorDimension], 32 );
                CHECK_RC(pDisplay->SetupChannelImage(setModeDisplay,
                    LwrsorWidth[lwrsorDimension],
                    LwrsorHeight[lwrsorDimension],
                    32,             // valid depth for cursor now is 32 bit only.. NO 64 bit support for cursor
                    Display::LWRSOR_IMM_CHANNEL_ID,
                    &LwrsorImage,
                    LwrsorImageName));
                CHECK_RC(pDisplay->SetImage(setModeDisplay,
                    &LwrsorImage,
                    Display::LWRSOR_IMM_CHANNEL_ID));

                Printf(Tee::PriHigh," Moving the Cursor on | DisplayId = %X | to (x,y):(%d,%d) \n",(UINT32)setModeDisplay, lwrX, lwrY);
                CHECK_RC(pDisplay->SetLwrsorPos(setModeDisplay,lwrX,lwrY));

                // Free the Cursor Image
                Printf(Tee::PriHigh,"Freeing Cursor Image on | DisplayId = %X | \n",(UINT32)setModeDisplay);
                CHECK_RC(pDisplay->SetImage(setModeDisplay, NULL, Display::LWRSOR_IMM_CHANNEL_ID));

                if(LwrsorImage.GetMemHandle() != 0)
                {
                    LwrsorImage.Free();
                }
            }
            /**************************************************************************/
            /*********************** Now move this overlay ****************************/
            /**************************************************************************/
            Printf(Tee::PriHigh," Moving the Overlay on | DisplayId = %X | to (x,y):(%d,%d) \n", (UINT32)setModeDisplay, posX + 100, posY + 100);
            CHECK_RC_CLEANUP(pDisplay->SetOverlayPos(setModeDisplay,
                posX + 100,
                posY + 100));
            // Free Overlay
            Printf(Tee::PriHigh,"Destroy: Overlay on | DisplayId = %X |\n", (UINT32)setModeDisplay);
            CHECK_RC(pDisplay->SetImage(setModeDisplay,
                NULL,
                Display::OVERLAY_CHANNEL_ID));
            if(OvlyImage.GetMemHandle() != 0)
            {
                OvlyImage.Free();
            }
        }

        Printf(Tee::PriHigh, "%s: || Doing SetMode on Base Surface | DisplayId = %X | Width = %d | Height = %d | Depth = %d | Refresh Rate = %d | color Format = %d  ||\n",
                             __FUNCTION__, (UINT32)setModeDisplay, Width, Height, Depth, RefreshRate, coreColorFormats[coreColorFormatIndex]);
        CHECK_RC(pDisplay->SetupChannelImage(setModeDisplay,
            Width,
            Height,
            Depth,
            Display::BASE_CHANNEL_ID,
            &BaseImage,
            BaseImageName,
            0,
            Display::ILWALID_HEAD,
            (EvoColor)0,//EvoColor BaseColor
            (EvoColor)0,//EvoColor OverscanColor,
            Surface2D::Pitch,
            coreColorFormats[coreColorFormatIndex]));

        CHECK_RC(pDisplay->SetImage(setModeDisplay,
            &BaseImage,
            Display::BASE_CHANNEL_ID));

        for(UINT32 ovlyColorFormatIndex = 0; ovlyColorFormatIndex < ovlyColorFormats.size(); ovlyColorFormatIndex++)
        {
            Printf(Tee::PriHigh, "%s: || Creating Overlay Image  | DisplayId = %X | Width = %d | Height = %d | Depth = %d  | color Format = %d  ||\n",
                             __FUNCTION__, (UINT32)setModeDisplay, Width, Height, Depth, ovlyColorFormats[ovlyColorFormatIndex]);
            CHECK_RC(pDisplay->SetupChannelImage(setModeDisplay,
                Width,
                Height,
                Depth,
                Display::OVERLAY_CHANNEL_ID,
                &OvlyImage,
                OvlyImageName,
                0,
                Display::ILWALID_HEAD,
                (EvoColor)0,//EvoColor BaseColor
                (EvoColor)0,//EvoColor OverscanColor,
                Surface2D::Pitch,
                ovlyColorFormats[ovlyColorFormatIndex]));
            CHECK_RC(pDisplay->SetImage(setModeDisplay,
                &OvlyImage,
                Display::OVERLAY_CHANNEL_ID));

            // For GK104 and above all 4 cursor sizes are valid..
            // For lower chips (not supported) only 32 bit and 64 bit are valid
            UINT32 ValidLwrsorSize = 4;

            for(UINT32 lwrsorDimension = 0 ; lwrsorDimension < ValidLwrsorSize; lwrsorDimension++)
            {
                sprintf(lwrsorImgName,"dispinfradata/images/image%dx%d.png",
                    LwrsorWidth[lwrsorDimension],
                    LwrsorHeight[lwrsorDimension]);
                LwrsorImageName = string(lwrsorImgName);

                Printf(Tee::PriHigh, "%s: || Creating Cursor Image | DisplayId = %X | Width = %d | Height = %d | Depth = %d | \n",
                             __FUNCTION__, (UINT32)setModeDisplay, LwrsorWidth[lwrsorDimension], LwrsorHeight[lwrsorDimension], 32 );
                CHECK_RC(pDisplay->SetupChannelImage(setModeDisplay,
                    LwrsorWidth[lwrsorDimension],
                    LwrsorHeight[lwrsorDimension],
                    32,
                    Display::LWRSOR_IMM_CHANNEL_ID,
                    &LwrsorImage,
                    LwrsorImageName));
                CHECK_RC(pDisplay->SetImage(setModeDisplay,
                    &LwrsorImage,
                    Display::LWRSOR_IMM_CHANNEL_ID));

                Printf(Tee::PriHigh," Moving the Cursor on | DisplayId = %X | to (x,y):(%d,%d) \n",(UINT32)setModeDisplay, lwrX, lwrY);
                CHECK_RC(pDisplay->SetLwrsorPos(setModeDisplay,lwrX,lwrY));

                // Free the Cursor Image
                Printf(Tee::PriHigh,"Freeing Cursor Image on | DisplayId = %X | \n",(UINT32)setModeDisplay);
                CHECK_RC(pDisplay->SetImage(setModeDisplay, NULL, Display::LWRSOR_IMM_CHANNEL_ID));

                if(LwrsorImage.GetMemHandle() != 0)
                {
                    LwrsorImage.Free();
                }
            }
            /**************************************************************************/
            /*********************** Now move this overlay ****************************/
            /**************************************************************************/
            Printf(Tee::PriHigh," Moving the Overlay | DisplayId = %X | to (x,y):(%d,%d) \n", (UINT32)setModeDisplay, posX + 100, posY + 100);
            CHECK_RC_CLEANUP(pDisplay->SetOverlayPos(setModeDisplay,
                posX + 100,
                posY + 100));
            // Free Overlay
            Printf(Tee::PriHigh,"Destroy: Overlay on | DisplayId = %X |\n",(UINT32)setModeDisplay);
            CHECK_RC(pDisplay->SetImage(setModeDisplay,
                NULL,
                Display::OVERLAY_CHANNEL_ID));
            if(OvlyImage.GetMemHandle() != 0)
            {
                OvlyImage.Free();
            }
        }

        Printf(Tee::PriHigh,"Detaching DisplayId = %X \n", (UINT32)setModeDisplay);
        CHECK_RC(pDisplay->DetachDisplay(DisplayIDs(1,setModeDisplay)));

        if(LwrsorImage.GetMemHandle() != 0)
        {
            LwrsorImage.Free();
        }
        if(OvlyImage.GetMemHandle() != 0)
        {
            OvlyImage.Free();
        }
        if(BaseImage.GetMemHandle() != 0)
        {
            BaseImage.Free();
        }
        if(CoreImage.GetMemHandle() != 0)
        {
            CoreImage.Free();
        }
    }
Cleanup:

    return rc;
}

//! \brief Cleanup, does nothing for this simple test
//------------------------------------------------------------------------------
RC ColorFormatN64bitTest::Cleanup()
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
JS_CLASS_INHERIT(ColorFormatN64bitTest, RmTest,
    "Test to demonstrate use of color format and 64 bit SetMode APIs");

CLASS_PROP_READWRITE(ColorFormatN64bitTest,onlyConnectedDisplays,bool,
                     "run on only connected displays, default = 0");
CLASS_PROP_READWRITE(ColorFormatN64bitTest, protocol, string,
                     "OR protocol to use for the test");
CLASS_PROP_READWRITE(ColorFormatN64bitTest, rastersize, string,
                     "Desired raster size (small/large)");
CLASS_PROP_READWRITE(ColorFormatN64bitTest, bpc, string,
                     "Setup bpc (bits per color component). Valid values for lWPU card = 8,10,16");

