/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011,2013,2016-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_DmiMemAcc.cpp
//! \brief Ensure that DMI_MEMACC works for all kinds of iso traffic from display to FB
//!
#include "gpu/tests/rmtest.h"
#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/utility.h"
#include "core/include/jscript.h"
#include <string> // Only use <> for built-in C++ stuff
#include "gpu/display/evo_disp.h"
#include "gpu/tests/rm/utility/crccomparison.h"
#include "gpu/utility/surf2d.h"
#include "core/include/platform.h"
#include "disp/v02_00/dev_disp.h"
#include "core/include/xp.h"
#include "core/include/memcheck.h"

typedef struct
{
    GpuSubdevice *pSubdev;
    UINT32 RegOffset;
    UINT32 FieldDrfMask;
    UINT32 FieldDrfShift;
    UINT32 FieldValue;
} RegFieldValue_PollArgs;

class DmiMemAccTest : public RmTest
{
public:
    DmiMemAccTest();
    virtual ~DmiMemAccTest();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(pixelclock, UINT32); //Config pixel clock through cmdline
    SETGET_PROP(rastersize, string); //Config raster size through cmdline

private:
    const string DISP_INFRA_IMAGES_DIR;
    const string coreChnImageFile;
    const string baseChnImageFile;
    const string ovlyChnImageFile;
    const string lwrsChnImageFile;
    UINT32       CompCRCForBlank;
    UINT32       PriCRCForBlank;
    bool         CRCsForBlankInited;
    UINT32       m_pixelclock;
    string       m_rastersize;

    RC GetCRCValuesAndDump
    (
        DisplayID               Display,
        EvoCRCSettings::CHANNEL CntlChannel,
        UINT32                  &CompCRC,
        UINT32                  &PriCRC,
        string                  CrcFileName,
        bool                    CreateNewFile
    );
    RC VerifyDisp();

    RC AllocSetLUTAndGamma(DisplayID Display,
                           EvoLutSettings *&pLutSettings);

    RC FreeLUT(DisplayID Display,
               EvoLutSettings *pLutSettings);

    RC FlipDmiMemAccAndCrc(DisplayID displayId,
                           EvoCRCSettings::CHANNEL channel,
                           string crcfileName);

    RC Write_DMI_MEMACC(DisplayID Display,
                        bool bEnableImage);

};

//! \brief Constructor for DmiMemAccTest
//!
//! Set a name for the test
//!
//! \sa Setup
//------------------------------------------------------------------------------
DmiMemAccTest::DmiMemAccTest() :
        DISP_INFRA_IMAGES_DIR("dispinfradata/images/"),
        coreChnImageFile(DISP_INFRA_IMAGES_DIR + "coreimage640x480.PNG"),
        baseChnImageFile(DISP_INFRA_IMAGES_DIR + "baseimage640x480.PNG"),
        ovlyChnImageFile(DISP_INFRA_IMAGES_DIR + "ovlyimage640x480.PNG"),
        lwrsChnImageFile(DISP_INFRA_IMAGES_DIR + "lwrsorimage32x32.PNG"),
        CompCRCForBlank(0),
        PriCRCForBlank(0),
        CRCsForBlankInited(false)
{
    SetName("DmiMemAccTest");
    m_pixelclock = 0;
    m_rastersize = "";
}

//! \brief Destructor for DmiMemAccTest
//!
//! Does nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
DmiMemAccTest::~DmiMemAccTest()
{

}

//! \brief IsTestSupported()
//!
//! Check whether the hardware supports DMI_MEMACC functionality
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string DmiMemAccTest::IsTestSupported()
{
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
RC DmiMemAccTest::Setup()
{
    RC rc;

    CHECK_RC(InitFromJs());

    return rc;
}

//! \brief Tests DMI_MEMACC for all kinds of iso traffic from display to FB
//!
//! Central point where all tests are run.
//!
//! \return OK if test passes, appropriate err code if it fails.
//------------------------------------------------------------------------------
RC DmiMemAccTest::Run()
{
   RC rc = OK;

   CHECK_RC(VerifyDisp());

   return rc;
}

RC DmiMemAccTest::FlipDmiMemAccAndCrc(DisplayID displayId, EvoCRCSettings::CHANNEL channel, string crcfileName)
{
    RC      rc;
    UINT32  CompCRC[3];
    UINT32  PriCRC[3];

    CHECK_RC(GetCRCValuesAndDump(displayId,
                                 channel,
                                 CompCRC[0],
                                 PriCRC[0],
                                 crcfileName,
                                 true));

    // Hit DMI_MEMACC to blank and generate CRC again
    CHECK_RC(Write_DMI_MEMACC(displayId, false));
    CHECK_RC(GetCRCValuesAndDump(displayId,
                                 channel,
                                 CompCRC[1],
                                 PriCRC[1],
                                 crcfileName,
                                 false));

    // Hit DMI_MEMACC to unblank and generate CRC again. Should match the first CRC
    CHECK_RC(Write_DMI_MEMACC(displayId, true));
    CHECK_RC(GetCRCValuesAndDump(displayId,
                                 channel,
                                 CompCRC[2],
                                 PriCRC[2],
                                 crcfileName,
                                 false));

    // Basic sanity check on CRCs. 1'st and 3'rd CRCs must match and 2'nd one must mismatch
    if (CompCRC[0] != CompCRC[2])
    {
        Printf(Tee::PriHigh, "Compositor CRC before applying DMI_MEMACC (0x%X) must match the one after clearing DMI_MEMACC (0x%X)\n", CompCRC[0], CompCRC[2]);
        rc = RC::GOLDEN_VALUE_MISCOMPARE;
    }
    if (PriCRC[0] != PriCRC[2])
    {
        Printf(Tee::PriHigh, "Primary CRC before applying DMI_MEMACC (0x%X) must match the one after clearing DMI_MEMACC (0x%X)\n", PriCRC[0], PriCRC[2]);
        rc = RC::GOLDEN_VALUE_MISCOMPARE;
    }
    if (CompCRC[0] == CompCRC[1])
    {
        Printf(Tee::PriHigh, "Compositor CRC before applying DMI_MEMACC (0x%X) must NOT match the one after applying DMI_MEMACC (0x%X)\n", CompCRC[0], CompCRC[1]);
        rc = RC::GOLDEN_VALUE_MISCOMPARE;
    }
    if (PriCRC[0] == PriCRC[1])
    {
        Printf(Tee::PriHigh, "Primary CRC before applying DMI_MEMACC (0x%X) must NOT match the one after applying DMI_MEMACC (0x%X)\n", PriCRC[0], PriCRC[1]);
        rc = RC::GOLDEN_VALUE_MISCOMPARE;
    }

    if (CRCsForBlankInited == false)
    {
        CompCRCForBlank    = CompCRC[1];
        PriCRCForBlank     = PriCRC[1];
        CRCsForBlankInited = true;
    }
    else
    {
        if (CompCRCForBlank != CompCRC[1])
        {
            // Not treating this as error yet since infra is using pink as
            // default base color and hw seems to be
            // 1. Applying LUT to it (this will cause CRC change)
            // 2. Not using default base color for full screen overlay case.
            Printf(Tee::PriHigh, "First captured compositor CRC for blank (0x%X) doesn't match current (0x%X). WILL BE TREATED AS ERROR IN FUTURE\n", CompCRCForBlank, CompCRC[1]);
            // rc = RC::CRC_VALUES_NOT_UNIQUE;
        }
        if (PriCRCForBlank  != PriCRC[1])
        {
            // Not treating this as error yet since infra is using pink as
            // default base color and hw seems to be
            // 1. Applying LUT to it (this will cause CRC change)
            // 2. Not using default base color for full screen overlay case.
            Printf(Tee::PriHigh, "First captured primary CRC for blank (0x%X) doesn't match current (0x%X). WILL BE TREATED AS ERROR IN FUTURE\n", PriCRCForBlank, PriCRC[1]);
            // rc = RC::CRC_VALUES_NOT_UNIQUE;
        }
    }

    return rc;
}

// Test includes
// 1. Initial ModeSet
// 2. Generate CrcValues
// 3. Select custom params like Pixel clock  Settings etc and CommitSettings
// 4. Generate Crc again
RC DmiMemAccTest::VerifyDisp()
{
    RC rc;
    Display *pDisplay = GetDisplay();

    DisplayIDs Displays;
    UINT32 BaseWidth   = 640;
    UINT32 BaseHeight  = 480;

    UINT32 OvlyWidth = 640;
    UINT32 OvlyHeight = 480;

    UINT32 LwrsorWidth = 32;
    UINT32 LwrsorHeight = 32;

    UINT32 Depth       = 32;
    UINT32 RefreshRate = 60;

    Surface2D          CoreImage;
    Surface2D          BaseImage;
    Surface2D          LwrsorImage;
    Surface2D          OverlayImage;

    char               crcFileNameSuffix[16];
    bool               useSmallRaster = false;
    UINT32      OvlyCnt;

    // Intializing Display HW
    CHECK_RC(pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        Platform::EscapeWrite("Disp_AllOrs_EnableImageOutput",0,4,1);
    }

    CHECK_RC(pDisplay->GetDetected(&Displays));
    Printf(Tee::PriHigh, "All Detected displays = \n");
    pDisplay->PrintDisplayIds(Tee::PriHigh, Displays);

    DisplayID SelectedDisplay = Displays[0];

    /*  a. Core channel only
        b. Core with overlay
        i. Core with overlay, cursor and LUT
        c. Core with cursor

        d. Base channel only
        f. Base with overlay
        h. Base with overlay and cursor
        j. Base with overlay, cursor and LUT
        e. Base with cursor

        k. One of core or base with full screen overlay cursor and LUT

    */
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
        BaseWidth    = 160;
        BaseHeight   = 120;
        OvlyWidth    = 160;
        OvlyHeight   = 120;
        LwrsorWidth  = 32;
        LwrsorHeight = 32;
        RefreshRate  = 60;
    }
    else
    {
        BaseWidth    = 640;
        BaseHeight   = 480;
        OvlyWidth    = 640;
        OvlyHeight   = 480;
        LwrsorWidth  = 32;
        LwrsorHeight = 32;
        RefreshRate  = 60;
    }
    sprintf(crcFileNameSuffix, "-%dx%d.xml", BaseWidth, BaseHeight);

    if (m_pixelclock)
    {
        EvoCoreChnContext *pCoreDispContext = NULL;
        pCoreDispContext = (EvoCoreChnContext *)pDisplay->GetEvoDisplay()->
                             GetEvoDisplayChnContext(SelectedDisplay, Display::CORE_CHANNEL_ID);
        EvoCoreDisplaySettings &LwstomSettings = pCoreDispContext->DispLwstomSettings;

        Printf(Tee::PriHigh, "In %s() Using PixelClock = %d \n", __FUNCTION__, m_pixelclock);

        LwstomSettings.HeadSettings[0].PixelClkSettings.PixelClkFreqHz = m_pixelclock;
        LwstomSettings.HeadSettings[0].PixelClkSettings.Adj1000Div1001 = false;
        LwstomSettings.HeadSettings[0].PixelClkSettings.ClkMode = EvoPixelClockSettings::CLK_LWSTOM;
        LwstomSettings.HeadSettings[0].PixelClkSettings.NotDriver = false;
        LwstomSettings.HeadSettings[0].PixelClkSettings.Dirty = true;
        LwstomSettings.HeadSettings[0].Dirty = true;
    }

    Printf(Tee::PriHigh,"Setup: Core only\n");
    CHECK_RC(pDisplay->SetupChannelImage(SelectedDisplay,
                                         BaseWidth,
                                         BaseHeight,
                                         Depth,
                                         Display::CORE_CHANNEL_ID,
                                         &CoreImage,
                                         coreChnImageFile));

    CHECK_RC(pDisplay->SetMode(SelectedDisplay,
                               BaseWidth,
                               BaseHeight,
                               Depth,
                               RefreshRate));

    Printf(Tee::PriHigh,"CRC: Core only\n");
    CHECK_RC(FlipDmiMemAccAndCrc(SelectedDisplay, EvoCRCSettings::CORE, string("DMI_MEMACC-core") + crcFileNameSuffix));

    // b. Core with overlay
    vector<UINT64>testKeyMaskValues;
    vector<UINT64>testKeyColorValues;
    testKeyMaskValues.push_back(0x1F00);
    testKeyColorValues.push_back(0x1F00);
    testKeyMaskValues.push_back(0x10F00);
    testKeyColorValues.push_back(0x10F00);
    testKeyMaskValues.push_back(0x10F0F0);
    testKeyColorValues.push_back(0x10F0F0);

    EvoOvlyChnContext *pOvlyDispContext = NULL;
    pOvlyDispContext = (EvoOvlyChnContext *)pDisplay->GetEvoDisplay()->
                        GetEvoDisplayChnContext(SelectedDisplay, Display::OVERLAY_CHANNEL_ID);
    EvoOverlaySettings &OverlaySettings = pOvlyDispContext->OverlaySettings;

    CHECK_RC(pDisplay->SetupChannelImage(SelectedDisplay,
                                         OvlyWidth,
                                         OvlyHeight,
                                         Depth,
                                         Display::OVERLAY_CHANNEL_ID,
                                         &OverlayImage,
                                         ovlyChnImageFile));
    OverlaySettings.PointInX = 0;
    OverlaySettings.PointInY = 0;
    OverlaySettings.SizeInWidth = OverlayImage.GetWidth();
    OverlaySettings.SizeInHeight = OverlayImage.GetHeight();
    OverlaySettings.SizeOutWidth = OverlayImage.GetWidth();

    for(OvlyCnt =0; OvlyCnt < testKeyMaskValues.size(); OvlyCnt++)
    {
        OverlaySettings.OverlayKeyColor = testKeyColorValues[OvlyCnt];
        OverlaySettings.OverlayKeyMask  = testKeyMaskValues[OvlyCnt];
        OverlaySettings.Dirty  = true;

        Printf(Tee::PriHigh,"Setup: Core + Overlay \n");
        CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                    &OverlayImage,
                                    Display::OVERLAY_CHANNEL_ID));

        Printf(Tee::PriHigh,"CRC: Core + Overlay\n");
        CHECK_RC(FlipDmiMemAccAndCrc(SelectedDisplay, EvoCRCSettings::CORE, string("DMI_MEMACC-core-ovly") + crcFileNameSuffix));

        if (OvlyCnt != testKeyMaskValues.size())
        {
            // Free Overlay
            Printf(Tee::PriHigh,"Destroy: Overlay\n");
            CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                    NULL,
                                    Display::OVERLAY_CHANNEL_ID));

        }
    }
    OverlaySettings.Dirty  = false;
    //  Core with overlay and cursor
    Printf(Tee::PriHigh,"Setup: Core + Overlay + Cursor\n");
    CHECK_RC(pDisplay->SetupChannelImage(SelectedDisplay,
                                         LwrsorWidth,
                                         LwrsorHeight,
                                         Depth,
                                         Display::LWRSOR_IMM_CHANNEL_ID,
                                         &LwrsorImage,
                                         lwrsChnImageFile));

    CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                &LwrsorImage,
                                Display::LWRSOR_IMM_CHANNEL_ID));

    Printf(Tee::PriHigh,"CRC: Core + Overlay + Cursor \n");
    CHECK_RC(FlipDmiMemAccAndCrc(SelectedDisplay, EvoCRCSettings::CORE, string("DMI_MEMACC-core-ovly-lwrs") + crcFileNameSuffix));

    Printf(Tee::PriHigh,"Setup: Core + Overlay + Cursor + LUT\n");

    EvoLutSettings *pLutSettings1;
    CHECK_RC(AllocSetLUTAndGamma(SelectedDisplay, pLutSettings1));

    Printf(Tee::PriHigh,"CRC: Core + Overlay + Cursor + LUT\n");
    CHECK_RC(FlipDmiMemAccAndCrc(SelectedDisplay, EvoCRCSettings::CORE, string("DMI_MEMACC-core-ovly-lwrs-LUT") + crcFileNameSuffix));

    // Free Overlay
    Printf(Tee::PriHigh,"Destroy: Overlay\n");
    CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                NULL,
                                Display::OVERLAY_CHANNEL_ID));

    if(OverlayImage.GetMemHandle() != 0)
        OverlayImage.Free();

    // Windowed Overlay that is half the original (full screen) size
    Printf(Tee::PriHigh,"Setup: Core + Windowed Overlay + Cursor + LUT\n");
    CHECK_RC(pDisplay->SetupChannelImage(SelectedDisplay,
                                         OvlyWidth / 2,
                                         OvlyHeight / 2,
                                         Depth,
                                         Display::OVERLAY_CHANNEL_ID,
                                         &OverlayImage,
                                         ovlyChnImageFile));

    CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                &OverlayImage,
                                Display::OVERLAY_CHANNEL_ID));

    Printf(Tee::PriHigh,"CRC: Core + Windowed Overlay + Cursor + LUT\n");
    CHECK_RC(FlipDmiMemAccAndCrc(SelectedDisplay, EvoCRCSettings::CORE, string("DMI_MEMACC-core-windowedovly-lwrs-LUT") + crcFileNameSuffix));

    // Free Windowed Overlay
    Printf(Tee::PriHigh,"Destroy: Windowed Overlay\n");
    CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                NULL,
                                Display::OVERLAY_CHANNEL_ID));

    if (OverlayImage.GetMemHandle() != 0)
        OverlayImage.Free();

    Printf(Tee::PriHigh,"CRC: Core + Cursor + LUT\n");
    CHECK_RC(FlipDmiMemAccAndCrc(SelectedDisplay, EvoCRCSettings::CORE, string("DMI_MEMACC-core-lwrs-LUT") + crcFileNameSuffix));

    // Free Cursor
    Printf(Tee::PriHigh,"Destroy: Cursor\n");
    CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                NULL,
                                Display::LWRSOR_IMM_CHANNEL_ID));

    if(LwrsorImage.GetMemHandle() != 0)
        LwrsorImage.Free();

    Printf(Tee::PriHigh,"CRC: Core + LUT\n");
    CHECK_RC(FlipDmiMemAccAndCrc(SelectedDisplay, EvoCRCSettings::CORE, string("DMI_MEMACC-core-LUT") + crcFileNameSuffix));

    // Free LUT
    CHECK_RC(FreeLUT(SelectedDisplay, pLutSettings1));

    Printf(Tee::PriHigh,"Setup: Base only\n");
    CHECK_RC(pDisplay->SetupChannelImage(SelectedDisplay,
                                         BaseWidth,
                                         BaseHeight,
                                         Depth,
                                         Display::BASE_CHANNEL_ID,
                                         &BaseImage,
                                         baseChnImageFile));

    CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                &BaseImage,
                                Display::BASE_CHANNEL_ID));

    Printf(Tee::PriHigh,"CRC: Base only\n");
    CHECK_RC(FlipDmiMemAccAndCrc(SelectedDisplay, EvoCRCSettings::BASE, string("DMI_MEMACC-base") + crcFileNameSuffix));

    Printf(Tee::PriHigh,"Setup: Base + Overlay\n");
    CHECK_RC(pDisplay->SetupChannelImage(SelectedDisplay,
                                         OvlyWidth,
                                         OvlyHeight,
                                         Depth,
                                         Display::OVERLAY_CHANNEL_ID,
                                         &OverlayImage,
                                         ovlyChnImageFile));

    OverlaySettings.PointInX = 0;
    OverlaySettings.PointInY = 0;
    OverlaySettings.SizeInWidth = OverlayImage.GetWidth();
    OverlaySettings.SizeInHeight = OverlayImage.GetHeight();
    OverlaySettings.SizeOutWidth = OverlayImage.GetWidth();

    for(OvlyCnt =0; OvlyCnt < testKeyMaskValues.size(); OvlyCnt++)
    {
        OverlaySettings.OverlayKeyColor = testKeyColorValues[OvlyCnt];
        OverlaySettings.OverlayKeyMask  = testKeyMaskValues[OvlyCnt];
        OverlaySettings.Dirty  = true;

        Printf(Tee::PriHigh,"Setup: Core + Overlay \n");
        CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                    &OverlayImage,
                                    Display::OVERLAY_CHANNEL_ID));

        Printf(Tee::PriHigh,"CRC: Base + Overlay\n");
        CHECK_RC(FlipDmiMemAccAndCrc(SelectedDisplay, EvoCRCSettings::CORE, string("DMI_MEMACC-core-ovly") + crcFileNameSuffix));

        if (OvlyCnt != testKeyMaskValues.size())
        {
            // Free Overlay
            Printf(Tee::PriHigh,"Destroy: Overlay\n");
            CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                        NULL,
                                        Display::OVERLAY_CHANNEL_ID));

        }
    }
    OverlaySettings.Dirty  = false;
    // Base with overlay and cursor
    Printf(Tee::PriHigh,"Setup: Base + Overlay + Cursor\n");
    CHECK_RC(pDisplay->SetupChannelImage(SelectedDisplay,
                                         LwrsorWidth,
                                         LwrsorHeight,
                                         Depth,
                                         Display::LWRSOR_IMM_CHANNEL_ID,
                                         &LwrsorImage,
                                         lwrsChnImageFile));
    CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                &LwrsorImage,
                                Display::LWRSOR_IMM_CHANNEL_ID));

    Printf(Tee::PriHigh,"CRC: Base + Overlay + Cursor\n");
    CHECK_RC(FlipDmiMemAccAndCrc(SelectedDisplay, EvoCRCSettings::BASE, string("DMI_MEMACC-base-ovly-lwrs") + crcFileNameSuffix));

    Printf(Tee::PriHigh,"Setup: Base + Overlay + Cursor + LUT\n");
    EvoLutSettings *pLutSettings2;
    CHECK_RC(AllocSetLUTAndGamma(SelectedDisplay, pLutSettings2));

    Printf(Tee::PriHigh,"CRC: Base + Overlay + Cursor + LUT\n");
    CHECK_RC(FlipDmiMemAccAndCrc(SelectedDisplay, EvoCRCSettings::BASE, string("DMI_MEMACC-base-ovly-lwrs-LUT") + crcFileNameSuffix));

    // Free Overlay Image
    CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                NULL,
                                Display::OVERLAY_CHANNEL_ID));

    if (OverlayImage.GetMemHandle() != 0)
        OverlayImage.Free();

    // Windowed Overlay that is half the original (full screen) size
    Printf(Tee::PriHigh,"Setup: Base + Windowed Overlay + Cursor + LUT\n");
    CHECK_RC(pDisplay->SetupChannelImage(SelectedDisplay,
                                         OvlyWidth / 2,
                                         OvlyHeight / 2,
                                         Depth,
                                         Display::OVERLAY_CHANNEL_ID,
                                         &OverlayImage,
                                         ovlyChnImageFile));

    CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                &OverlayImage,
                                Display::OVERLAY_CHANNEL_ID));

    Printf(Tee::PriHigh,"CRC: Base + Windowed Overlay + Cursor + LUT\n");
    CHECK_RC(FlipDmiMemAccAndCrc(SelectedDisplay, EvoCRCSettings::CORE, string("DMI_MEMACC-base-windowedovly-lwrs-LUT") + crcFileNameSuffix));

    // Free Windowed Overlay
    Printf(Tee::PriHigh,"Destroy: Windowed Overlay\n");
    CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                NULL,
                                Display::OVERLAY_CHANNEL_ID));

    if (OverlayImage.GetMemHandle() != 0)
        OverlayImage.Free();

    Printf(Tee::PriHigh,"CRC: Base + Cursor + LUT\n");
    CHECK_RC(FlipDmiMemAccAndCrc(SelectedDisplay, EvoCRCSettings::BASE, string("DMI_MEMACC-base-lwrs-LUT") + crcFileNameSuffix));

    // Free LwrsorImage
    CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                NULL,
                                Display::LWRSOR_IMM_CHANNEL_ID));
    if (LwrsorImage.GetMemHandle() != 0)
        LwrsorImage.Free();

    Printf(Tee::PriHigh,"CRC: Base + LUT\n");
    CHECK_RC(FlipDmiMemAccAndCrc(SelectedDisplay, EvoCRCSettings::BASE, string("DMI_MEMACC-base-LUT") + crcFileNameSuffix));

    // free LUT
    CHECK_RC(FreeLUT(SelectedDisplay, pLutSettings2));

    // here all the Images allocated if any will be Freed ..
    Printf(Tee::PriHigh,"DetachDisplay\n");
    CHECK_RC_CLEANUP(pDisplay->DetachDisplay(DisplayIDs(1, SelectedDisplay)));

Cleanup:
    if(BaseImage.GetMemHandle() != 0)
        BaseImage.Free();
    if(CoreImage.GetMemHandle() != 0)
        CoreImage.Free();
    if(OverlayImage.GetMemHandle() != 0)
        OverlayImage.Free();
    if(LwrsorImage.GetMemHandle() != 0)
        LwrsorImage.Free();

    return rc;
}

RC DmiMemAccTest::GetCRCValuesAndDump
(
    DisplayID               Display,
    EvoCRCSettings::CHANNEL CntlChannel,
    UINT32                  &CompCRC,
    UINT32                  &PriCRC,
    string                  CrcFileName,
    bool                    CreateNewFile
)
{
    RC              rc;
    CrcComparison   crcCompObj;
    UINT32          SecCRC;

    EvoDisplay *pEvoDisplay = GetDisplay()->GetEvoDisplay();

    EvoCRCSettings CrcStgs;
    CrcStgs.ControlChannel = CntlChannel;
    CrcStgs.CrcPrimaryOutput = EvoCRCSettings::CRC_RG; // Test doesn't care about the OR type

    CHECK_RC(pEvoDisplay->SetupCrcBuffer(Display, &CrcStgs));
    CHECK_RC(pEvoDisplay->GetCrcValues(Display, &CrcStgs, &CompCRC, &PriCRC, &SecCRC));
    CHECK_RC(crcCompObj.DumpCrcToXml(GetDisplay()->GetOwningDevice(), CrcFileName.c_str(), &CrcStgs, CreateNewFile));

    return rc;
}

static bool CheckRegFieldValueForDesiredValue(void *pParam)
{
    MASSERT(pParam);
    RegFieldValue_PollArgs *pPollArgs = (RegFieldValue_PollArgs *)pParam;

    UINT32 data32 = pPollArgs->pSubdev->RegRd32(pPollArgs->RegOffset);
    UINT32 fieldVal = ((data32 >> pPollArgs->FieldDrfShift) & pPollArgs->FieldDrfMask);

    return (fieldVal == pPollArgs->FieldValue);
}

RC DmiMemAccTest::Write_DMI_MEMACC(DisplayID Display, bool bEnableImage)
{
    RC                      rc = OK;
    UINT32                  data32 = 0;
    RegFieldValue_PollArgs  PollArgs;
    UINT32                  TimeoutMs = 100;

    // for getting Head
    UINT32 Head = Display::ILWALID_HEAD;
    CHECK_RC(GetDisplay()->GetEvoDisplay()->GetHeadByDisplayID(Display, &Head));

    // Getting Subdevice
    GpuSubdevice *pSubdev =  GetDisplay()->GetOwningDisplaySubdevice();

    if (Head > 1)
    {
        Printf(Tee::PriHigh, "Write_DMI_MEMACC: Bad Head %u\n", Head);
        return OK;
    }

    // Write  LW_PDISP_DMI_MEMACC
    data32 = pSubdev->RegRd32(LW_PDISP_DMI_MEMACC);
    if (!bEnableImage)
    {
        data32 = FLD_IDX_SET_DRF_DEF(_PDISP, _DMI_MEMACC, _HEAD_REQUEST, Head, _STOP, data32);
    }
    else
    {
        data32 = FLD_IDX_SET_DRF_DEF(_PDISP, _DMI_MEMACC, _HEAD_REQUEST, Head, _FETCH, data32);
    }
    data32 = FLD_IDX_SET_DRF_DEF(_PDISP, _DMI_MEMACC, _HEAD_SETTING_NEW, Head, _TRIGGER, data32);
    pSubdev->RegWr32(LW_PDISP_DMI_MEMACC, data32);

    PollArgs.pSubdev       = pSubdev;
    PollArgs.RegOffset     = LW_PDISP_DMI_MEMACC;
    PollArgs.FieldDrfMask  = DRF_MASK(LW_PDISP_DMI_MEMACC_HEAD_SETTING_NEW(Head));
    PollArgs.FieldDrfShift = DRF_SHIFT(LW_PDISP_DMI_MEMACC_HEAD_SETTING_NEW(Head));
    PollArgs.FieldValue    = LW_PDISP_DMI_MEMACC_HEAD_SETTING_NEW_DONE;
    CHECK_RC(POLLWRAP_HW(&CheckRegFieldValueForDesiredValue, &PollArgs, TimeoutMs));

    return rc;
}

RC DmiMemAccTest::AllocSetLUTAndGamma(DisplayID Display,
                                      EvoLutSettings *&pLutSettings)
{
    RC rc;
    vector<UINT32>TestLut;

    UINT32 index = 0;

    pLutSettings = new EvoLutSettings();

    EvoDisplay *pEvoDisplay = GetDisplay()->GetEvoDisplay();

    //Set LUT Pallette
    for ( index = 0; index < 256; index++)
    {
        TestLut.push_back( ((UINT32)U8_TO_LUT(index)) << 16 | U8_TO_LUT(index) );  // Green, Red
        TestLut.push_back( (UINT32)U8_TO_LUT(index) );  // Blue
    }

    //257th value = 256th value
    TestLut.push_back( (((UINT32)U8_TO_LUT(index-1)) << 16) | U8_TO_LUT(index-1) );  // Green, Red
    TestLut.push_back( (UINT32)U8_TO_LUT(index-1) );  // Blue

    CHECK_RC(pEvoDisplay->SetupLUTBuffer(Display,
                                         pLutSettings,
                                         &TestLut[0],
                                         (UINT32)TestLut.size()));

    //SetLUT After Base Is Active
    CHECK_RC(pEvoDisplay->SetLUT(Display,
                                 pLutSettings));

    // SET CUSTOM GAMMA
    // 0 -10 .. is the values
    CHECK_RC(pEvoDisplay->SetGamma(Display, pLutSettings, 2, 0, 0));

    return rc;
}

RC DmiMemAccTest::FreeLUT(DisplayID Display,
                          EvoLutSettings *pLutSettings)
{
    RC rc;
    EvoLutSettings LutSettings;
    LutSettings.Dirty = false;
    LutSettings.pLutBuffer = NULL;
    CHECK_RC(GetDisplay()->GetEvoDisplay()->SetLUT(Display,
                                 &LutSettings));
    delete pLutSettings;
    pLutSettings = NULL;

    return rc;
}

//! \brief Cleanup
//!
//! Does nothing
//------------------------------------------------------------------------------
RC DmiMemAccTest::Cleanup()
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
JS_CLASS_INHERIT(DmiMemAccTest, RmTest,
    "DmiMemAccTest test\n");

CLASS_PROP_READWRITE(DmiMemAccTest, pixelclock, UINT32,
                     "Desired pixel clock in Hz");
CLASS_PROP_READWRITE(DmiMemAccTest, rastersize, string,
                     "Desired raster size (small/large)");

