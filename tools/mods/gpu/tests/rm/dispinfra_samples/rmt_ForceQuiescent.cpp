/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_ForceQuiescent.cpp
//! \brief Ensure that Force Quiescent semantics work properply for supporting GPUs.
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "lwmisc.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/utility.h"
#include "gpu/display/evo_disp.h"
#include "gpu/tests/rm/utility/crccomparison.h"
#include "gpu/utility/surf2d.h"
#include "disp/v02_02/dev_disp.h"
#include "ctrl/ctrl5070.h"
#include "class/cl9170.h"     // LW9170_DISPLAY
#include "class/cl917c.h"     // LW917C_BASE_CHANNEL_DMA
#include "class/cl917c.h"     // LW917C_BASE_CHANNEL_DMA
#include "class/cl917e.h"     // LW917E_OVERLAY_CHANNEL_DMA

class ForceQuiescentTest : public RmTest
{
public:
    ForceQuiescentTest();
    virtual ~ForceQuiescentTest();
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
    UINT32       m_pixelclock;
    string       m_rastersize;

    RC VerifyDisp();
    RC QuiesceSatelliteChannels(DisplayID Display);

    RC GetCRCValuesAndDump
    (
        DisplayID               Display,
        EvoCRCSettings::CHANNEL CntlChannel,
        UINT32                  &CompCRC,
        UINT32                  &PriCRC,
        string                  CrcFileName,
        bool                    CreateNewFile
    );
};

static GpuSubdevice    *subDev;

//! \brief Constructor for ForceQuiescentTest
//!
//! Set a name for the test
//!
//! \sa Setup
//------------------------------------------------------------------------------
ForceQuiescentTest::ForceQuiescentTest() :
        DISP_INFRA_IMAGES_DIR("dispinfradata/images/"),
        coreChnImageFile(DISP_INFRA_IMAGES_DIR + "coreimage640x480.PNG"),
        baseChnImageFile(DISP_INFRA_IMAGES_DIR + "baseimage640x480.PNG"),
        ovlyChnImageFile(DISP_INFRA_IMAGES_DIR + "ovlyimage640x480.PNG")
{
    SetName("ForceQuiescentTest");
    m_pixelclock = 0;
    m_rastersize = "";
}

//! \brief Destructor for ForceQuiescentTest
//!
//! Does nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
ForceQuiescentTest::~ForceQuiescentTest()
{
}

//! \brief IsTestSupported()
//!
//! Determine of this test is supported on the current setup
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string ForceQuiescentTest::IsTestSupported()
{
    Display *pDisplay = GetDisplay();
    if( pDisplay->GetDisplayClassFamily() == Display::EVO )
    {
        if (pDisplay->GetClass() >= LW9170_DISPLAY)
        {
            return RUN_RMTEST_TRUE;
        }
    }
    return "Required Display class not supported on current platform";
}

//! \brief Setup
//!
//! Just JS settings
//
//! \return RC -> OK if everything is allocated,
//------------------------------------------------------------------------------
RC ForceQuiescentTest::Setup()
{
    RC rc;
    CHECK_RC(InitFromJs());
    return rc;
}

//! \brief Run
//!
//! Central point where all tests are run.
//!
//! \return OK if test passes, appropriate err code if it fails.
//------------------------------------------------------------------------------
RC ForceQuiescentTest::Run()
{
   RC rc = OK;
   CHECK_RC(VerifyDisp());
   return rc;
}

//! \brief VerifyDisp
//!
//! \return OK if test passes, appropriate err code if it fails.
//------------------------------------------------------------------------------
RC ForceQuiescentTest::VerifyDisp()
{
    RC rc;
    Display *pDisplay = GetDisplay();

    DisplayIDs Displays;

    UINT32 BaseWidth;
    UINT32 BaseHeight;

    UINT32 OvlyWidth;
    UINT32 OvlyHeight;

    UINT32 Depth = 32;
    UINT32 RefreshRate = 60;

    Surface2D CoreImage;
    Surface2D BaseImage;
    Surface2D OverlayImage;

    char crcFileNameSuffix[16];
    bool useSmallRaster = false;

    UINT32  CompCRC[4];
    UINT32  PriCRC[4];

    subDev = GetBoundGpuSubdevice();
    EvoDisplay *pEvoDisplay = pDisplay->GetEvoDisplay();

    UINT32 head = 0;
    UINT32 regValue = 0;

    // Initialize Display HW
    CHECK_RC(pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        Platform::EscapeWrite("Disp_AllOrs_EnableImageOutput",0,4,1);
    }

    CHECK_RC(pDisplay->GetDetected(&Displays));
    Printf(Tee::PriHigh, "All Detected displays = \n");
    pDisplay->PrintDisplayIds(Tee::PriHigh, Displays);

    DisplayID SelectedDisplay = Displays[0];

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
        BaseWidth    = 640;
        BaseHeight   = 480;
        OvlyWidth    = 640;
        OvlyHeight   = 480;
        RefreshRate  = 60;
        RefreshRate  = 60;
    }
    else
    {
        BaseWidth    = 800;
        BaseHeight   = 600;
        OvlyWidth    = 800;
        OvlyHeight   = 600;
        RefreshRate  = 60;
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

    // Step1: Modeset in core
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

    // Step2: Get CORE image CRC values
    Printf(Tee::PriHigh,"CRC: Core only\n");
    CHECK_RC(GetCRCValuesAndDump(SelectedDisplay,
                                 EvoCRCSettings::CORE,
                                 CompCRC[0],
                                 PriCRC[0],
                                 string("ForceQuiescentTest-core") + crcFileNameSuffix,
                                 true));

    // Step3: Set up base image
    Printf(Tee::PriHigh,"Setup: Core + Base\n");
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

    // Step4: Set up overlay image.
    Printf(Tee::PriHigh,"Setup: Core + Base + Overlay\n");
        CHECK_RC(pDisplay->SetupChannelImage(SelectedDisplay,
                                         OvlyWidth,
                                         OvlyHeight,
                                         Depth,
                                         Display::OVERLAY_CHANNEL_ID,
                                         &OverlayImage,
                                         ovlyChnImageFile));

    CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                &OverlayImage,
                                Display::OVERLAY_CHANNEL_ID));

    // Step5: Get Base image CRC values
    Printf(Tee::PriHigh,"CRC: Core + Base + Overlay\n");
    CHECK_RC(GetCRCValuesAndDump(SelectedDisplay,
                                 EvoCRCSettings::BASE,
                                 CompCRC[1],
                                 PriCRC[1],
                                 string("ForceQuiescentTest-core-base-ovly") + crcFileNameSuffix,
                                 true));

    // Step6: Force quiesce base and overlay
    CHECK_RC(QuiesceSatelliteChannels(SelectedDisplay));

    pEvoDisplay->AllowDebugModeOnDisplayChannels(true);

    // Step7: Get CORE image CRC values
    CHECK_RC(GetCRCValuesAndDump(SelectedDisplay,
                                 EvoCRCSettings::CORE,
                                 CompCRC[2],
                                 PriCRC[2],
                                 string("ForceQuiescentTest-core-post-quiesce") + crcFileNameSuffix,
                                 true));

    if (CompCRC[0] != CompCRC[2])
    {
        Printf(Tee::PriHigh, "Compositor CRC after modeset in core (0x%X) doesn't match the one after quiesceSatelliteChannels (0x%X)\n", CompCRC[0], CompCRC[2]);
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }

    if (PriCRC[0] != PriCRC[2])
    {
        Printf(Tee::PriHigh, "Primary CRC after modeset in core (0x%X) doesn't match the one after quiesceSatelliteChannels (0x%X)\n", PriCRC[0], PriCRC[2]);
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }

    // Step8,9: Change CORE timings and clear debug mode.
    BaseWidth  = (BaseWidth == 800) ? 640 : 800;
    BaseHeight = (BaseHeight == 600) ? 480 : 600;

    // This will clear the debug mode alongwith ForceQuiescence
    CHECK_RC(pDisplay->SetMode(SelectedDisplay,
                               BaseWidth,
                               BaseHeight,
                               Depth,
                               RefreshRate));

    pEvoDisplay->AllowDebugModeOnDisplayChannels(false);

    // Enable Quiescence and debug mode again
    CHECK_RC(QuiesceSatelliteChannels(SelectedDisplay));

    pEvoDisplay->AllowDebugModeOnDisplayChannels(true);

    BaseWidth  = (BaseWidth == 640) ? 800 : 640;
    BaseHeight = (BaseHeight == 480) ? 600 : 480;

    // Set the initial timings as in step1, this will again disable debug mode and ForceQuiescence.
    CHECK_RC(pDisplay->SetMode(SelectedDisplay,
                               BaseWidth,
                               BaseHeight,
                               Depth,
                               RefreshRate));

    pEvoDisplay->AllowDebugModeOnDisplayChannels(false);

    // Enable FQ by writing into the 21st bit for both base and overlay
    CHECK_RC(pEvoDisplay->GetHeadByDisplayID(SelectedDisplay, &head));
    regValue = subDev->RegRd32(LW_PDISP_CHNCTL_BASE(head));
    regValue = FLD_SET_DRF(_PDISP, _CHNCTL_BASE, _FORCE_QUIESCENT, _ENABLE, regValue);
    subDev->RegWr32(LW_PDISP_CHNCTL_BASE(head), regValue);

    regValue = subDev->RegRd32(LW_PDISP_CHNCTL_OVLY(head));
    regValue = FLD_SET_DRF(_PDISP, _CHNCTL_OVLY, _FORCE_QUIESCENT, _ENABLE, regValue);
    subDev->RegWr32(LW_PDISP_CHNCTL_OVLY(head), regValue);

    // Step10: Set BASE to NULL
    CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                NULL,
                                Display::BASE_CHANNEL_ID));

    if(BaseImage.GetMemHandle() != 0)
        BaseImage.Free();

    // Step11: Set OVERLAY to NULL.
    CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                NULL,
                                Display::OVERLAY_CHANNEL_ID));

    if(OverlayImage.GetMemHandle() != 0)
        OverlayImage.Free();

    // Step12: Un-force quiescence BASE and OVERLAY.
    // Disable FQ by writing into the 21st bit for both base and overlay; it will actually
    // get disabled after an update is sent
    CHECK_RC(pEvoDisplay->GetHeadByDisplayID(SelectedDisplay, &head));
    regValue = subDev->RegRd32(LW_PDISP_CHNCTL_BASE(head));
    regValue = FLD_SET_DRF(_PDISP, _CHNCTL_BASE, _FORCE_QUIESCENT, _DISABLE, regValue);
    subDev->RegWr32(LW_PDISP_CHNCTL_BASE(head), regValue);

    regValue = subDev->RegRd32(LW_PDISP_CHNCTL_OVLY(head));
    regValue = FLD_SET_DRF(_PDISP, _CHNCTL_OVLY, _FORCE_QUIESCENT, _DISABLE, regValue);
    subDev->RegWr32(LW_PDISP_CHNCTL_OVLY(head), regValue);

    //
    // On gk104/7, when force_quiescent is disabled, it will take effect on the next frame.
    // On gk110, the state will persist until an update is processed in the satellite channel.
    // So, this should be verified different for GK10X chips.
    //
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

    CHECK_RC(pDisplay->SetupChannelImage(SelectedDisplay,
                                         BaseWidth,
                                         BaseHeight,
                                         Depth,
                                         Display::OVERLAY_CHANNEL_ID,
                                         &OverlayImage,
                                         ovlyChnImageFile));

    CHECK_RC(pDisplay->SetImage(SelectedDisplay,
                                &OverlayImage,
                                Display::OVERLAY_CHANNEL_ID));

    // Step13: BASE crc again should match the crc from step 5
    CHECK_RC(GetCRCValuesAndDump(SelectedDisplay,
                                 EvoCRCSettings::BASE,
                                 CompCRC[3],
                                 PriCRC[3],
                                 string("ForceQuiescentTest-base-ovly-quiesce-cleared") + crcFileNameSuffix,
                                 true));

    if (CompCRC[1] != CompCRC[3])
    {
        Printf(Tee::PriHigh, "Compositor CRC after image setup in base/ovly (0x%X) doesn't match the one after clear force quiescent (0x%X)\n", CompCRC[1], CompCRC[3]);
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }

    if (PriCRC[1] != PriCRC[3])
    {
        Printf(Tee::PriHigh, "Primary CRC after image setup in base/ovly (0x%X) doesn't match the one after clear force quiescent (0x%X)\n", PriCRC[1], PriCRC[3]);
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }

    // here all the Images allocated if any will be Freed ..
    Printf(Tee::PriHigh,"DetachDisplay\n");
    CHECK_RC_CLEANUP(pDisplay->DetachDisplay(DisplayIDs(1, SelectedDisplay)));

Cleanup:
    if(CoreImage.GetMemHandle() != 0)
        CoreImage.Free();
    if(BaseImage.GetMemHandle() != 0)
        BaseImage.Free();
    if(OverlayImage.GetMemHandle() != 0)
        OverlayImage.Free();

    return rc;
}

//! \brief QuiesceSatelliteChannels
//!
//! Sends base and overlay channel into Quiescent state.
//------------------------------------------------------------------------------
RC ForceQuiescentTest::QuiesceSatelliteChannels
(
    DisplayID Display
)
{
    RC              rc;
    LW5070_CTRL_IDLE_CHANNEL_PARAMS idleChannelParams;
    LwU32 i;
    UINT32 head;
    LwRmPtr pLwRm;
    LwRm::Handle hDisplayHandle;
    EvoDisplay *pEvoDisplay = GetDisplay()->GetEvoDisplay();
    UINT32 activeSubdeviceIndex = 0;    // active subdevice for control call

    CHECK_RC(pEvoDisplay->GetHeadByDisplayID(Display, &head));
    CHECK_RC(pEvoDisplay->GetEvoDispObject(&hDisplayHandle));

    ////RtlZeroMemory(&idleChannelParams, sizeof(idleChannelParams));
    memset(&idleChannelParams, 0, sizeof(idleChannelParams));

    // Need to specify correct subdeviceIndex in SLI mode.
    ////idleChannelParams.base.subdeviceIndex     = m_masterGpu[head];
    idleChannelParams.base.subdeviceIndex = LwV32(activeSubdeviceIndex);
    idleChannelParams.channelInstance         = head;

    idleChannelParams.desiredChannelStateMask = LW5070_CTRL_IDLE_CHANNEL_STATE_QUIESCENT2;
    idleChannelParams.accelerators            = LW5070_CTRL_IDLE_CHANNEL_ACCL_NONE;

    // Loops through base(0) and overlay(1) channel.
    for (i = 0; i < 2; i++)
    {
        if (i == 0)
        {
            //idleChannelParams.channelClass = getBaseDma(head).getClassInUse();
            //idleChannelParams.channelClass = pEvoDisplay->GetClass();
            idleChannelParams.channelClass = LW917C_BASE_CHANNEL_DMA;
        }
        else
        {
            idleChannelParams.channelClass = LW917E_OVERLAY_CHANNEL_DMA;
        }

        rc = pLwRm->Control(hDisplayHandle,
                          LW5070_CTRL_CMD_IDLE_CHANNEL,
                          (void*)&idleChannelParams,
                          sizeof(idleChannelParams));

        if (rc != OK)
        {
            return rc;
        }
    }

    return rc;
}

RC ForceQuiescentTest::GetCRCValuesAndDump
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

//! \brief Cleanup
//!
//! Does nothing
//------------------------------------------------------------------------------
RC ForceQuiescentTest::Cleanup()
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
JS_CLASS_INHERIT(ForceQuiescentTest, RmTest,
    "ForceQuiescentTest test\n");

CLASS_PROP_READWRITE(ForceQuiescentTest, pixelclock, UINT32,
                     "Desired pixel clock in Hz");
CLASS_PROP_READWRITE(ForceQuiescentTest, rastersize, string,
                     "Desired raster size (small/large)");

