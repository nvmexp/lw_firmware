/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Display CRC checker test.

#include "core/include/platform.h"
#include "core/include/utility.h"
#include "gpu/display/lwdisplay/lw_disp.h"
#include "gpu/display/lwdisplay/lw_disp_crc_handler_c3.h"
#include "gpu/display/lwdisplay/lw_disp_utils.h"
#include "disp_crcchecker.h"
#include "lwMultiMon.h"
#include "Lwcm.h"

//------------------------------------------------------------------------------
//global flag which sets if test is run in parallel mode using -RunParallel flag
//SetMode() checks this flag before selecting displays.
//------------------------------------------------------------------------------
static bool s_ParallelFlag = false;

//------------------------------------------------------------------------------
// DisplayHAL: Display abstraction
//------------------------------------------------------------------------------
DisplayHAL::DisplayHAL
(
    Display *pDisplay,
    const GpuTestConfiguration& testConfig,
    UINT32 maxPixelClockHz,
    bool useRasterFromEdid
)
    : m_pDisplay(pDisplay)
    , m_TestConfig(testConfig)
    , m_MaxPixelClockHz(maxPixelClockHz)
    , m_UseRasterFromEdid(useRasterFromEdid)
{

}

RC DisplayHAL::GetHead(const DisplayID& displayID, UINT32 *pHead)
{
    RC rc;
    DisplayIDs displayIDsSingle;
    std::vector<UINT32> headsForSingleDisplayID;
    displayIDsSingle.push_back(displayID);

    CHECK_RC(m_pDisplay->GetHeadsForMultiDisplay(displayIDsSingle, &headsForSingleDisplayID));

    *pHead = headsForSingleDisplayID[0];

    return rc;
}

void DisplayHAL::SetDelayKnobs(const DisplayHAL::DelayKnobs& delayKnobs)
{
    m_DelayKnobs = delayKnobs;
}

const DisplayHAL::DelayKnobs& DisplayHAL::GetDelayKnobs() const
{
    return m_DelayKnobs;
}

#if !defined(TEGRA_MODS)
bool DisplayHAL::FillRasterSettings
(
    const Display::Mode& resolutionInfo,
    EvoRasterSettings *rasterSettings
)
{
    UINT32 scaledWidth = resolutionInfo.width;
    UINT32 scaledHeight = resolutionInfo.height;

    // Values in the raster were determined by experiments so they work
    // both for DACs and SORs in various modes.
    // They should also have plenty of vblank margin across the full
    // range of pclks used, so that there are no problems callwlating
    // the DMI duration value.
    UINT32 pclk = (resolutionInfo.width+200)
                  * (resolutionInfo.height+150)
                  * resolutionInfo.refreshRate;

    *rasterSettings = EvoRasterSettings(
                       scaledWidth+200,  101, scaledWidth+100+49, 149,
                       scaledHeight+150,  50, scaledHeight+50+50, 100,
                       1, 0,
                       pclk,
                       0, 0,
                       false);
    rasterSettings->ReorderBankWidthSize = 0; // disable reorder bank width

    // Apply max pixel clock limit
    if (m_MaxPixelClockHz != 0)
    {
        if (rasterSettings->PixelClockHz > m_MaxPixelClockHz)
            return false;
    }
    return true;
}
#endif

//------------------------------------------------------------------------------
// LegacyDisplayHAL : Abstract implementation of DisplayHAL for Display arch which
// uses UINT32/Select()
//------------------------------------------------------------------------------
LegacyDisplayHAL::LegacyDisplayHAL
(
    Display *pDisplay,
    const GpuTestConfiguration& testConfig,
    const DelayKnobs& delayKnobs,
    UINT32 maxPixelClockHz,
    bool useRasterFromEdid
): DisplayHAL(pDisplay, testConfig, maxPixelClockHz, useRasterFromEdid)
{

}

RC LegacyDisplayHAL::Setup()
{
    StickyRC rc;

    const UINT32 selectedDisplays = m_pDisplay->Selected();
    const INT32 lowBit = Utility::BitScanForward(selectedDisplays);
    m_OriginalDisplayID = (lowBit >= 0) ? BIT(lowBit) : 0;

    CHECK_RC(CacheColorTranscoding());

    rc = m_pDisplay->DetachAllDisplays();

    // Force RGB colorspace. Otherwise, HDMI will get different crcs than
    //  other protocols for certain resolutions (eg 1080p, 2160p).
    m_pDisplay->SetHdmiColorOverride(Display::COLORSPACE_RGB);

    return OK;
}

RC LegacyDisplayHAL::SetMode
(
    const DisplayID& displayID,
    const Display::Mode& resolutionInfo,
    UINT32 head
)
{
    StickyRC rc;

    // Avoid calling into Select if the display ID has not changed
    // This forces a new modeset even if modeset optimization is enabled
    if (!s_ParallelFlag)
    {
        if (displayID != m_pDisplay->Selected())
            CHECK_RC(m_pDisplay->Select(displayID));
    }
    CHECK_RC(UpdateRasterSettings(resolutionInfo));

    if (IsColorTranscodingSupported())
    {
        // Disable underreplication in display pipe
        CHECK_RC(m_pDisplay->SelectColorTranscoding(m_pDisplay->Selected(),
                Display::ZeroFill));
    }

    Display::FilterTaps filterTaps = Display::FilterTapsNone;
    Display::ColorCompression colorCompression = Display::ColorCompressionNone;

    rc = m_pDisplay->SetMode(displayID,
                               resolutionInfo.width,
                               resolutionInfo.height,
                               resolutionInfo.depth,
                               resolutionInfo.refreshRate,
                               filterTaps,
                               colorCompression,
                               LW_CFGEX_GET_FLATPANEL_INFO_SCALER_DEFAULT, // scaler mode
                               (VIDEO_INFOFRAME_CTRL*)0,
                               (AUDIO_INFOFRAME_CTRL *)0);

    Platform::Delay(m_DelayKnobs.postSetmodeDelay);

    return rc;
}

RC LegacyDisplayHAL::Cleanup()
{
    StickyRC rc;

    if (m_pDisplay)
    {
        rc = m_pDisplay->DetachAllDisplays();
        m_pDisplay->SetHdmiColorOverride(INFOFRAME_CTRL_DONTCARE);

        if (m_OriginalDisplayID)
        {
            rc = m_pDisplay->Select(m_OriginalDisplayID);
            rc = m_pDisplay->SetTimings(NULL);

            rc = DisplayImage(m_OriginalDisplayID, nullptr, E1850CrcChecker::BoardType::UNKNOWN);

            rc = RestoreColorTranscoding();

            Display::Mode mode(m_TestConfig.DisplayWidth(),
                               m_TestConfig.DisplayHeight(),
                               m_TestConfig.DisplayDepth(),
                               m_TestConfig.RefreshRate());
            rc = SetMode(m_OriginalDisplayID, mode, 0);
        }
    }

    return rc;
}

RC LegacyDisplayHAL::DisplayImage
(
    const DisplayID& displayID,
    Surface2D *image,
    E1850CrcChecker::BoardType boardType
)
{
    return m_pDisplay->SetImage(displayID, image);
}

RC LegacyDisplayHAL::CacheColorTranscoding()
{
    if (IsColorTranscodingSupported())
    {
        // Cache the color transcoding mode
        m_OrigColorTranscoding = m_pDisplay->GetColorTranscoding(m_pDisplay->Selected());
        if (m_OrigColorTranscoding == Display::IlwalidMode)
            return RC::SOFTWARE_ERROR;
    }
    return OK;
}

RC LegacyDisplayHAL::RestoreColorTranscoding()
{
    if (IsColorTranscodingSupported())
    {
        // Re-enable underreplication in display pipe
        return m_pDisplay->SelectColorTranscoding(m_pDisplay->Selected(),
                         m_OrigColorTranscoding);
    }

    return OK;
}

//------------------------------------------------------------------------------
// TegraDisplayHAL : Concrete implementation of DisplayHAL for TegraDisplay
//------------------------------------------------------------------------------
TegraDisplayHAL::TegraDisplayHAL
(
    Display *pDisplay,
    const GpuTestConfiguration& testConfig,
    const DelayKnobs& delayKnobs,
    bool enableHdcpInAndroid
)
    : LegacyDisplayHAL(pDisplay, testConfig, delayKnobs, 0, false)
    , m_EnableHdcpInAndroid(enableHdcpInAndroid)
{
    if (Xp::GetOperatingSystem() == Xp::OS_ANDROID && !m_EnableHdcpInAndroid)
    {
        // We no longer need to wait for HDCP negotiation for 8 secs
        // However HDCP driver will still try negotiating once, so wait for 1 sec
        // + 1 sec guard band
        DelayKnobs filledDelayKnobs = delayKnobs;
        if (TESTARG_UNSET == delayKnobs.postSetupDelay)
            filledDelayKnobs.postSetupDelay = 2000000;
        if (TESTARG_UNSET == delayKnobs.postCleanupDelay)
            filledDelayKnobs.postCleanupDelay = 2000000;
        if (TESTARG_UNSET == delayKnobs.postSetmodeDelay)
            filledDelayKnobs.postSetmodeDelay = 2000000;
        SetDelayKnobs(filledDelayKnobs);
    }
}

RC TegraDisplayHAL::GetHead(const DisplayID& displayID, UINT32 *pHead)
{
    // Todo: When reusing this method for multiple displayIDs use GetConfigs
    return m_pDisplay->GetHead(displayID.Get(), pHead);
}

RC TegraDisplayHAL::Setup()
{
    StickyRC rc;
    if (Xp::GetOperatingSystem() == Xp::OS_ANDROID)
    {
        TegraDisplay *pTegraDisp = m_pDisplay->GetTegraDisplay();
        CHECK_RC(pTegraDisp->SetHdmiPowerToggle(1));
        if (!m_EnableHdcpInAndroid)
        {
            m_SavedMaxHdcpRetries = pTegraDisp->GetHdcpMaxRetries();
            if (m_SavedMaxHdcpRetries == TegraDisplay::ILWALID_HDCP_MAX_RETRIES)
            {
                Printf(Tee::PriError,
                       "Unable to read HDCP max retries from kernel sysfs node!");
                return RC::SOFTWARE_ERROR;
            }

            //Bug 200156208: will enable this function when kernel supporting it
            //CHECK_RC(pTegraDisp->SetHdcpMaxRetries(0));
        }
    }

    rc = LegacyDisplayHAL::Setup();

    // In spite of WAR to bug 1572683 in Foster, kernel HDCP driver
    // will attempt key negotition in the background, causing MODS to
    // hang. Apply the post modeset delay as well
    if (Xp::GetOperatingSystem() == Xp::OS_ANDROID)
    {
        Platform::Delay(m_DelayKnobs.postSetupDelay);
    }

    return rc;
}

RC TegraDisplayHAL::Cleanup()
{
    StickyRC rc;
    if (Xp::GetOperatingSystem() == Xp::OS_ANDROID)
    {
        TegraDisplay *pTegraDisp = m_pDisplay->GetTegraDisplay();
        if (!m_EnableHdcpInAndroid)
        {
            //Bug 200156208: will enable this function when kernel supporting it
            //rc = pTegraDisp->SetHdcpMaxRetries(m_SavedMaxHdcpRetries);
        }
        CHECK_RC(pTegraDisp->SetHdmiPowerToggle(0));
    }

    rc = LegacyDisplayHAL::Cleanup();

    // In spite of WAR to bug 1572683 in Foster, kernel HDCP driver
    // will attempt key negotition in the background, causing MODS to
    // hang. Apply the post modeset delay as well
    if (Xp::GetOperatingSystem() == Xp::OS_ANDROID)
    {
        Platform::Delay(m_DelayKnobs.postCleanupDelay);
    }

    return rc;
}

bool TegraDisplayHAL::IsColorTranscodingSupported()
{
    return false;
}

RC TegraDisplayHAL::UpdateRasterSettings(const Display::Mode& resolutionInfo)
{
    return OK;
}

#if !defined(TEGRA_MODS)
//------------------------------------------------------------------------------
// EvoDisplayHAL : Concrete implementation of DisplayHAL for EvoDisplay
//------------------------------------------------------------------------------
EvoDisplayHAL::EvoDisplayHAL
(
    Display *pDisplay,
    const GpuTestConfiguration& testConfig,
    const DelayKnobs& delayKnobs,
    UINT32 maxPixelClockHz,
    bool useRasterFromEdid
): LegacyDisplayHAL(pDisplay, testConfig, delayKnobs, maxPixelClockHz, useRasterFromEdid)
{
    DelayKnobs filledDelayKnobs = delayKnobs;
    if (TESTARG_UNSET == delayKnobs.postSetmodeDelay)
        filledDelayKnobs.postSetmodeDelay = 2000000;

    SetDelayKnobs(filledDelayKnobs);
}

bool EvoDisplayHAL::IsColorTranscodingSupported()
{
    return true;
}

RC EvoDisplayHAL::UpdateRasterSettings(const Display::Mode& resolutionInfo)
{
    RC rc;
    if (!m_UseRasterFromEdid)
    {
        EvoRasterSettings ers;
        if (FillRasterSettings(resolutionInfo, &ers))
            CHECK_RC(m_pDisplay->SetTimings(&ers));
    }
    return OK;

}

//------------------------------------------------------------------------------
// LwDisplayHAL : Concrete implementation of DisplayHAL for LwDisplay
//------------------------------------------------------------------------------
LwDisplayHAL::LwDisplayHAL
(
    Display *pDisplay,
    const GpuTestConfiguration& testConfig,
    const DelayKnobs& delayKnobs,
    UINT32 maxPixelClockHz,
    bool useRasterFromEdid,
    bool enableInternalCrcCapture,
    Tee::Priority printPriority
)
: DisplayHAL(pDisplay, testConfig, maxPixelClockHz, useRasterFromEdid)
, m_EnableInternalCrcCapture(enableInternalCrcCapture)
, m_PrintPriority(printPriority)
{
    m_pDisplay = static_cast<LwDisplay *>(pDisplay);

    DelayKnobs filledDelayKnobs = delayKnobs;
    if (TESTARG_UNSET == delayKnobs.postSetmodeDelay)
    {
        if (m_pDisplay->GetOwningDisplaySubdevice()->
            HasFeature(Device::GPUSUB_SUPPORTS_HDR_DISPLAY))
        {
            filledDelayKnobs.postSetmodeDelay = 2600000;
        }
        else
        {
            filledDelayKnobs.postSetmodeDelay = 2000000;
        }
    }

    SetDelayKnobs(filledDelayKnobs);
}

RC LwDisplayHAL::Setup()
{
    RC rc;

    m_pLwDisplay = static_cast<LwDisplay *>(m_pDisplay);
    if (m_pLwDisplay->GetDisplayClassFamily() != Display::LWDISPLAY)
        return RC::SOFTWARE_ERROR;

    m_OrgDefaultActiveCrcRasterOnly = m_pLwDisplay->GetDefaultActiveCrcRasterOnly();
    m_pLwDisplay->SetDefaultActiveCrcRasterOnly(false);

    if (m_EnableInternalCrcCapture)
    {
        m_pCrcHandler = make_unique<CrcHandler>(m_pLwDisplay, "DispCRCChecker", m_PrintPriority);
        CHECK_RC(m_pCrcHandler->Setup(m_pLwDisplay->m_UsableHeadsMask));
    }

    return rc;
}

RC LwDisplayHAL::SetMode
(
    const DisplayID& displayID,
    const Display::Mode& resolutionInfo,
    UINT32 head
)
{
    RC rc;
    CHECK_RC(m_pLwDisplay->SetModeWithSingleWindow(displayID, resolutionInfo, head));
    Platform::Delay(m_DelayKnobs.postSetmodeDelay);
    return rc;
}

RC LwDisplayHAL::DetachDisplay(const DisplayID& displayID)
{
    RC rc;

    DisplayIDs displaysToDetach(1, displayID);

    CHECK_RC(m_pDisplay->DetachDisplay(displaysToDetach));

    CHECK_RC(m_pLwDisplay->DetachAllWindows());

    return rc;
}

RC LwDisplayHAL::Cleanup()
{
    StickyRC rc;

    if (m_pLwDisplay)
    {
        m_pLwDisplay->SetDefaultActiveCrcRasterOnly(m_OrgDefaultActiveCrcRasterOnly);

        rc = m_pLwDisplay->ResetEntireState();
    }

    if (m_EnableInternalCrcCapture)
    {
        rc = m_pCrcHandler->Cleanup();
    }

    return rc;
}

RC LwDisplayHAL::DisplayImage
(
    const DisplayID& displayID,
    Surface2D *image,
    E1850CrcChecker::BoardType boardType
)
{
    RC rc;

    if (m_EnableInternalCrcCapture && image)
    {
        m_pLwDisplay->SetUpdateMode(Display::ManualUpdate);

        UINT32 head;
        CHECK_RC(GetHead(displayID, &head));
        CHECK_RC(StartCrcCapture(displayID, head, boardType));
    }

    CHECK_RC(m_pLwDisplay->DisplayImage(displayID, image));

    if (m_EnableInternalCrcCapture && image)
    {
        CHECK_RC(m_pLwDisplay->SendInterlockedUpdates(
                LwDisplay::UPDATE_CORE,
                m_pLwDisplay->m_AllocWinInstMask,
                m_pLwDisplay->m_AllocWinInstMask,
                0,
                0,
                LwDisplay::INTERLOCK_CHANNELS,
                LwDisplay::WAIT_FOR_NOTIFIER));

        CHECK_RC(CompleteCrcCapture());
        m_pLwDisplay->SetUpdateMode(Display::AutoUpdate);
    }

    return rc;
}

RC LwDisplayHAL::StartCrcCapture
(
    const DisplayID& displayID,
    HeadIndex headIndex,
    E1850CrcChecker::BoardType boardType
)
{
    RC rc;

    CrcHandler::ActiveHeadDescriptions activeHeadDescriptions;
    CrcHandler::ActiveHeadDescription activeHeadDescription;

    CHECK_RC(GetCrcProtocol(boardType, &activeHeadDescription.crcProtocol));
    activeHeadDescription.seed = 0x1234; // Todo????

    UINT32 windowMask = 0;
    CHECK_RC(m_pLwDisplay->GetWindowsMaskAssociatedDisplay(displayID, &windowMask));
    activeHeadDescription.windowMask = windowMask;

    activeHeadDescriptions[headIndex] = activeHeadDescription;

    return m_pCrcHandler->QueueCrcCaptureStart(activeHeadDescriptions);
}

RC LwDisplayHAL::GetCrcProtocol
(
    E1850CrcChecker::BoardType boardType,
    CrcHandler::CRCProtocol *pProtocol
) const
{
    // See E1850CrcChecker::ColwertBoardTypeToString
    switch (boardType)
    {
        case E1850CrcChecker::BoardType::DVI:
            *pProtocol = CrcHandler::CRC_TMDS_A;
            break;
        case E1850CrcChecker::BoardType::HDMI_1_4_1080p:
            *pProtocol = CrcHandler::CRC_TMDS_A;
            break;
        case E1850CrcChecker::BoardType::DP_1_1a:
            *pProtocol = CrcHandler::CRC_DP_SST;
            break;
        case E1850CrcChecker::BoardType::HDMI_1_4_2160p:
            *pProtocol = CrcHandler::CRC_TMDS_A;
            break;
        case E1850CrcChecker::BoardType::HDMI_1_4_2160p_HDCP:
            *pProtocol = CrcHandler::CRC_TMDS_A;
            break;
        default:
            return RC::UNKNOWN_BOARD_DESCRIPTION;
    }

    return OK;
}

RC LwDisplayHAL::CompleteCrcCapture() const
{
    RC rc;

    CHECK_RC(m_pCrcHandler->Stop());

    CHECK_RC(m_pCrcHandler->PollMultiCrcCompletion());

    CHECK_RC(m_pCrcHandler->ReadCRCs());

    CHECK_RC(m_pCrcHandler->DumpCRCs(m_PrintPriority));

    CHECK_RC(m_pCrcHandler->ParseCapturedCRCs());

    CHECK_RC(m_pCrcHandler->DumpParsedCRCs(m_PrintPriority));

    return rc;
}

#endif

//----------------------------------------------------------------------------
// Script interface.

JS_CLASS_INHERIT(DisplayCrcChecker, GpuTest,
                 "Display CRC checker test.");

CLASS_PROP_READWRITE(DisplayCrcChecker, DisplaysToTest, UINT32,
                     "Require DisplayCrcChecker to run on these display(s)");
CLASS_PROP_READWRITE(DisplayCrcChecker, SkipDisplayMask, UINT32,
                     "Do not run DisplayCrcChecker on these display masks");
CLASS_PROP_READWRITE(DisplayCrcChecker, MaxPixelClockHz, UINT32,
                     "Skip SetModes with Pixel Clock higher than specified - useful when DP is forced to work with lowered bandwidth");
CLASS_PROP_READWRITE(DisplayCrcChecker, ContinueOnCrcError, bool,
                     "bool: continue running if an incorrect CRC was captured");
CLASS_PROP_READWRITE(DisplayCrcChecker, AllowedMismatchedCrcCount, UINT32,
                     "Ignore these many mismatched CRCs");
CLASS_PROP_READWRITE(DisplayCrcChecker, EnableDMA, bool,
                     "bool: enable DMA transfers");
CLASS_PROP_READWRITE(DisplayCrcChecker, DumpCrcCheckerRegisters, bool,
                     "bool: Dump the CRC checker device register state during the test in verbose mode");
CLASS_PROP_READWRITE(DisplayCrcChecker, VsyncPollTimeout, FLOAT64,
                     "Specify how long MODS must poll the CRC checker for an incoming video signal");
CLASS_PROP_READWRITE(DisplayCrcChecker, NumOfFrames, UINT32,
                     "Specify how many frames MODS must test on the CRC checker device");
CLASS_PROP_READWRITE(DisplayCrcChecker, TestPatternFileName, string,
                     "string: File name of file containing PNG test pattern descriptions");
CLASS_PROP_READWRITE(DisplayCrcChecker, TestPatternTableName, string,
                     "string: Name of table containing PNG test pattern descriptions");
CLASS_PROP_READWRITE(DisplayCrcChecker, DumpPngPattern, bool,
                     "bool: Dump the surface being used for CRC test as PNG image. (debug feature)");
CLASS_PROP_READWRITE(DisplayCrcChecker, InnerLoopModesetDelayUs, UINT32,
                     "Specify how long MODS must wait after the modeset in the CRC test inner loop");
CLASS_PROP_READWRITE(DisplayCrcChecker, OuterLoopModesetDelayUs, UINT32,
                     "Specify how long MODS must wait after the modeset in the CRC test outer loop");
CLASS_PROP_READWRITE(DisplayCrcChecker, UseRasterFromEDID, bool,
                     "bool: If true, use the raster from EDID else the default raster from the test");
CLASS_PROP_READWRITE(DisplayCrcChecker, LimitVisibleBwFromEDID, bool,
                     "bool: If true, use the max resolution from EDID to limit visible bandwidth of stress modes");
CLASS_PROP_READWRITE(DisplayCrcChecker, EnableHdcpInAndroid, bool,
                     "bool: If true, HDCP support in Android driver is enabled");
CLASS_PROP_READWRITE(DisplayCrcChecker, EnableInternalCrcCapture, bool,
                     "bool: If true, internal CRC capture is enabled");
CLASS_PROP_READWRITE(DisplayCrcChecker, IOTConfigFilePath, string,
                     "string: File name of file containing IOT Config descriptions");
CLASS_PROP_READWRITE(DisplayCrcChecker, PostHPDDelayMs, UINT32,
                     "Post HPD detect delay in milliseconds");
CLASS_PROP_READWRITE(DisplayCrcChecker, RunParallel, bool,
                     "bool: If true, test displays images on all heads simultaneously and compute external CRCs");

DisplayCrcChecker::DisplayCrcChecker()
{
    SetName("DisplayCrcChecker");
    m_pTestConfig = GetTestConfiguration();

    ModeDescription basicMode;
    m_StressModes.push_back(basicMode);

    m_CrcCheckerDeviceList.clear();
}

RC DisplayCrcChecker::Setup()
{
    RC rc;

    m_pGpuSubdevice = GetBoundGpuSubdevice();
    CHECK_RC(GpuTest::Setup());
    // Reserve a Display object, disable UI, set default graphics mode.
    CHECK_RC(GpuTest::AllocDisplay());

    m_pDisplay = GetDisplay();

    m_OrigIMP = m_pDisplay->SetIsModePossiblePriority(Tee::PriLow);

    CHECK_RC(InitProperties());

    // Claim p-states on the subdevice we run on to prevent
    // other tests from changing them
    CHECK_RC(m_PStateOwner.ClaimPStates(m_pGpuSubdevice));

    CHECK_RC(SetupSurfaceFill());

    CHECK_RC(SetupDisplayHAL());

    if (m_IOTConfigFilePath.empty())
    {
        CHECK_RC(FillCrcCheckerDeviceList());
    }
    else
    {
        if (!m_TestPatternFileName.empty() || !m_TestPatternTableName.empty())
        {
            Printf(Tee::PriError, "TestPatternFileName and "
                "TestPatternTableName are not allowed in IOT mode.\n");
            return RC::PARAMETER_MISMATCH;
        }
    }

    return OK;
}

RC DisplayCrcChecker::SetupSurfaceFill()
{
    if (m_EnableDMA && !GetBoundGpuSubdevice()->IsDFPGA())
    {
        m_pSurfaceFill = make_unique<SurfaceFillDMA>(true);
        Printf(GetVerbosePrintPri(), "DispCRC checker will use DMA for displaying the image\n");
    }
    else
    {
        m_pSurfaceFill =  make_unique<SurfaceFillFB>(true);
        Printf(GetVerbosePrintPri(), "DispCRC checker will not use DMA for displaying the image\n");
    }
    return m_pSurfaceFill->Setup(this, m_pTestConfig);
}

RC DisplayCrcChecker::SetupDisplayHAL()
{
    DisplayHAL::DelayKnobs delayKnobs(m_OuterLoopModesetDelayUs, m_OuterLoopModesetDelayUs,
        m_InnerLoopModesetDelayUs);

    if (m_pGpuSubdevice->IsSOC())
    {
        m_pDisplayHAL = make_unique<TegraDisplayHAL>(m_pDisplay, m_TestConfig, delayKnobs,
                                                     m_EnableHdcpInAndroid);
    }
#if !defined(TEGRA_MODS)
    else
    {
        if (m_pDisplay->GetDisplayClassFamily() == Display::EVO)
        {
            m_pDisplayHAL = make_unique<EvoDisplayHAL>(m_pDisplay, m_TestConfig, delayKnobs,
                                                       m_MaxPixelClockHz, m_UseRasterFromEDID);
        }
        else if (m_pDisplay->GetDisplayClassFamily() == Display::LWDISPLAY)
        {
            m_pDisplayHAL = make_unique<LwDisplayHAL>(m_pDisplay, m_TestConfig, delayKnobs,
                                                      m_MaxPixelClockHz, m_UseRasterFromEDID,
                                                      m_EnableInternalCrcCapture,
                                                      GetVerbosePrintPri());
        }
        else
        {
            return RC::SOFTWARE_ERROR;
        }
    }
#endif
    return m_pDisplayHAL->Setup();
}

RC DisplayCrcChecker::FillCrcCheckerDeviceList()
{
    // Check if there are any CRC checker devices connected to the GPU
    UINT32 deviceMask = 0;
    m_CrcCheckerDeviceList.clear();

    m_pDisplay->FindAndCreateCrcCheckerDevices(&m_CrcCheckerDeviceList,
        m_pDisplayHAL->GetDelayKnobs().postSetmodeDelay);

    for (auto it = m_CrcCheckerDeviceList.begin(); it != m_CrcCheckerDeviceList.end(); )
    {
        if (m_SkipDisplayMask & (*it)->GetDisplayId())
        {
            it = m_CrcCheckerDeviceList.erase(it);
        }
        else
        {
            deviceMask |= (*it)->GetDisplayId();
            ++it;
        }
    }
    if (m_CrcCheckerDeviceList.empty())
    {
        Printf(Tee::PriError,
               "Unable to detect any CRC checker devices !\n"
               "Please check the physical connections to the panel, HDMI DDC I2C settings, etc.\n");
        return RC::DEVICE_NOT_FOUND;
    }
    if (m_DisplaysToTest && (m_DisplaysToTest != deviceMask))
    {
        Printf(Tee::PriError,
               "Unable to detect all specified CRC checker devices !\n"
               "Please check the physical connections to the panel, HDMI DDC I2C settings, etc\n"
               "for the following diplays: 0x%x\n",
                m_DisplaysToTest ^ deviceMask);
        return RC::DEVICE_NOT_FOUND;
    }

    return OK;
}

RC DisplayCrcChecker::PrepareBackground(UINT32 stressModeIdx, ColorUtils::Format format)
{
    RC rc;
    const ModeDescription &stressMode = m_StressModes[stressModeIdx];

    Display::Mode resolutionMode(stressMode.width, stressMode.height,
        stressMode.depth, stressMode.refreshRate);

    SurfaceFill::SurfaceId surfaceId = static_cast<SurfaceFill::SurfaceId> (stressModeIdx);
    CHECK_RC(m_pSurfaceFill->PrepareSurface(surfaceId, resolutionMode, format));
    CHECK_RC(m_pSurfaceFill->FillPattern(surfaceId, stressMode.testPattern));
    CHECK_RC(m_pSurfaceFill->CopySurface(surfaceId));

    return rc;
}

RC DisplayCrcChecker::DumpImage(UINT32 stressModeIdx, Surface2D *image)
{
    const ModeDescription &stressMode = m_StressModes[stressModeIdx];

    // Dump the surface into a PNG file
    string pngFileNameTowrite = Utility::StrPrintf(
                                  "test46_mode%dx%d@%dHz_%s",
                                  stressMode.width,
                                  stressMode.height,
                                  stressMode.refreshRate,
                                  stressMode.testPattern.c_str());

    return image->WritePng(pngFileNameTowrite.c_str(), 0);
}

RC DisplayCrcChecker::ResetCrcCheckerDevices()
{
    RC rc;

    // Resetting E1850 panels for each stress test wastes test time
    // (especially for DP panels we have a 1.5s reset delay)
    for (const auto& pCrcCheckerDevice : m_CrcCheckerDeviceList)
    {
        // Reset device
        Printf(GetVerbosePrintPri(),
               "Resetting CRC checker device on display 0x%08x, I2C port = %d\n",
               pCrcCheckerDevice->GetDisplayId(),
               pCrcCheckerDevice->GetI2cPort());

        // Bug 1755215
        // The DP display head should be selected(enabled) before reset DP-crc checker,
        // otherwise DP driver will cause kernel panic when use DP AUX protocol to
        // read/write crc checker register.
        if (Xp::GetOperatingSystem() == Xp::OS_ANDROID && m_pGpuSubdevice->IsSOC())
        {
            if (pCrcCheckerDevice->GetDisplayId() != m_pDisplay->Selected())
            {
                CHECK_RC(m_pDisplay->Select(pCrcCheckerDevice->GetDisplayId()));
            }
            CHECK_RC(m_pDisplay->SetMode(1024,768,32,60));
            Platform::SleepUS(m_pDisplayHAL->GetDelayKnobs().postSetmodeDelay);
        }
        CHECK_RC(pCrcCheckerDevice->Reset());
    }
    return rc;
}

RC DisplayCrcChecker::IsVisibleBandwidthAllowed
(
    DisplayID displayID,
    const ModeDescription& stressMode,
    bool *pIsAllowed
)
{
    RC rc;
    *pIsAllowed = true;

    UINT32 maxWidth = 0, maxHeight = 0, maxRefreshRate = 0;
    CHECK_RC(m_pDisplay->GetMaxResolution(
        displayID,
        &maxWidth,
        &maxHeight,
        &maxRefreshRate));

    UINT32 visibleBandwidth = stressMode.width *
                  stressMode.height * stressMode.refreshRate;
    UINT32 maxVisibleBandwidth = maxWidth * maxHeight * maxRefreshRate;
    if (visibleBandwidth > maxVisibleBandwidth)
    {
       Printf(GetVerbosePrintPri(),
              "Skipping stress mode due to max visible bandwidth in EDID ...\n%s\n",
               stressMode.GetStringDescription().c_str());
        *pIsAllowed = false;
    }

    return rc;
}

RC DisplayCrcChecker::PrintCrcExperimentInfo
(
    Tee::Priority printPriority,
    CrcCheckerDevice* const & pCrcCheckerDevice,
    const CrcCheckerDevice::BoardInfo& brdInfo,
    const ModeDescription& stressMode
)
{
    RC rc;

    Printf(printPriority,
        "\nStarting CRC experiment on display 0x%08x, I2C port = %d\n",
        pCrcCheckerDevice->GetDisplayId(), pCrcCheckerDevice->GetI2cPort());

    Printf(printPriority, "\nCRC device firmare version: %d.%d\n",
        pCrcCheckerDevice->GetMajorVersion(),
        pCrcCheckerDevice->GetMinorVersion());

    Printf(printPriority, "CRC board type: %d (%s)\n",
        static_cast<int>(brdInfo.boardType), brdInfo.boardName.c_str());

    Printf(printPriority, "Running stress mode ...\n%s\n",
            stressMode.GetStringDescription().c_str());

    if (m_DumpCrcCheckerRegisters)
    {
        CHECK_RC(DumpCrcDeviceRegisters(pCrcCheckerDevice, "Before sending video frames",
            printPriority));
    }
    return OK;
}

RC DisplayCrcChecker::PrintDebugInfoPostSetMode
(
    Tee::Priority printPriority,
    CrcCheckerDevice* const & pCrcCheckerDevice
)
{
    Printf(GetVerbosePrintPri(),
        "Completed CRC experiment on display 0x%08x, I2C port = %d\n",
        pCrcCheckerDevice->GetDisplayId(),
        pCrcCheckerDevice->GetI2cPort());

    return OK;
}

RC DisplayCrcChecker::DumpCrcDeviceRegisters
(
    CrcCheckerDevice *pCrcChecker,
    const char *prefixMsg,
    Tee::Priority printPriority
)
{
    MASSERT(pCrcChecker);
    if (!pCrcChecker)
        return RC::SOFTWARE_ERROR;

    Printf(printPriority, "Display 0x%08x [I2C port: %d] %s\n"
               "*** CRC checker device register dump ***\n",
               pCrcChecker->GetDisplayId(),
               pCrcChecker->GetI2cPort(), prefixMsg ? prefixMsg : "");

    return pCrcChecker->DumpDeviceRegisters(printPriority);
}

RC DisplayCrcChecker::Run()
{
    RC rc;

    if (m_RunParallel)
    {
        CHECK_RC(RunDisplayCRCInParallel());
    }
    else if (!m_IOTConfigFilePath.empty())
    {
        CHECK_RC(RunIotMode());
    }
    else
    {
        CHECK_RC(RunNonIotMode());
    }

    return rc;
}

RC DisplayCrcChecker::RunIotMode()
{
    RC rc;

#if !defined(ANDROID)
    unique_ptr<IotConfigParser> pIotConfigParser = make_unique<IotConfigParser>();
    VerbosePrintf("\nLoading %s\n", m_IOTConfigFilePath.c_str());
    CHECK_RC(pIotConfigParser->Load(m_IOTConfigFilePath));

    IotConfigurationExt iotConfig;
    VerbosePrintf("Parsing ...%s\n", m_IOTConfigFilePath.c_str());
    CHECK_RC(pIotConfigParser->Parse(&iotConfig));

    m_pHPDGpio = make_unique<LwDispUtils::HPDGpio>(
        GetBoundRmClient(),
        GetBoundGpuSubdevice(),
        GetBoundGpuDevice()->GetDisplay()->GetLwDisplay(),
        iotConfig.hpdDebugPrintPriority);

    IotConfigurationExt::StressModesInfo stressModesInfo = iotConfig.stressModesInfo;
    IotSuperSwitch::PortMap portMap = iotConfig.portMap;
    Tee::Priority printPriority = iotConfig.printPriority;
    UINT32 timeoutMs = iotConfig.pollTimeoutMs;

    unique_ptr<IotSuperSwitch> pIotSuperSwitch;
    switch (iotConfig.superSwitchVersion)
    {
    case 1:
        VerbosePrintf("\nSuperSwitch version is %d\n", iotConfig.superSwitchVersion);
        pIotSuperSwitch = make_unique<IotSuperSwitch1>(std::move(iotConfig));
        break;
    case 2:
        VerbosePrintf("\nSuperSwitch version is %d\n", iotConfig.superSwitchVersion);
        pIotSuperSwitch = make_unique<IotSuperSwitch2>(std::move(iotConfig));
        break;
    default:
        Printf(Tee::PriError, "Unsupported SuperSwitch version\n");
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    CHECK_RC(pIotSuperSwitch->Initialize());
    CHECK_RC(m_pHPDGpio->FindHPDGPIOs());

    CHECK_RC(pIotSuperSwitch->PerformHandshake());

    if (iotConfig.superSwitchVersion > 1)
    {
        CHECK_RC(ReadAllLwstomParams(pIotSuperSwitch.get()));
    }

    CHECK_RC(pIotSuperSwitch->DisconnectAllInputs());

    string response;

    UINT32 directlyConnectedDisplayIDMask = 0;
    for (const auto& portInfo : stressModesInfo)
    {
        const string& portStr = portMap[portInfo.first];

        if (portInfo.second.empty())
        {
            directlyConnectedDisplayIDMask |= portInfo.first.Get();
            continue;
        }
        VerbosePrintf("\n=====================================================\n");
        VerbosePrintf("Setting up SuperSwitch routing for DisplayID 0x%x\n\n",
            portInfo.first.Get());

        // Ideally switching to unused port should disconnect other ports
        // but it doesn't work. So, disconnect individually.
        CHECK_RC(pIotSuperSwitch->DisconnectInput(portStr));

        // wait till HPD status is Disconnected OR timeout
        CHECK_RC(WaitForConnectionAndReadResponse(pIotSuperSwitch.get(),
            printPriority, portInfo.first, timeoutMs, false));

        CHECK_RC(pIotSuperSwitch->ConnectInput(portStr));

        // wait till HPD status is Connected OR timeout
        CHECK_RC(WaitForConnectionAndReadResponse(pIotSuperSwitch.get(),
            printPriority, portInfo.first, timeoutMs, true));

        // Skip displayIDs not under test and run CRC checker test
        m_SkipDisplayMask = ~portInfo.first.Get();

        CHECK_RC(UpdateStressModesForIotTesting(portInfo.first, portInfo.second));

        CHECK_RC(FillCrcCheckerDeviceList());

        CHECK_RC(RunDisplayCRCInSequential());

        CHECK_RC(pIotSuperSwitch->DisconnectInput(portStr));

        CHECK_RC(WaitForConnectionAndReadResponse(pIotSuperSwitch.get(),
            printPriority, portInfo.first, timeoutMs, false));

        VerbosePrintf("=====================================================\n");
    }
#endif

    return rc;
}

#if !defined(ANDROID)

RC DisplayCrcChecker::ReadAllLwstomParams(IotSuperSwitch *pIotSuperSwitch)
{
    RC rc;

    vector<string> params {"rd-dm", "rd-hw", "rd-fw", "rd-sn"};

    VerbosePrintf("\nReading params\n");
    for (const auto& param : params)
    {
        string response;
        CHECK_RC(pIotSuperSwitch->ReadLwstomParams(param, &response));
        VerbosePrintf("\t%s : %s\n", param.c_str(), response.c_str());
    }

    return rc;
}

RC DisplayCrcChecker::UpdateStressModesForIotTesting
(
    const DisplayID &displayID,
    const IotConfigurationExt::StressModes& stressModes
)
{
    RC rc;

    ClearStressModes();

    for (const auto& stressMode : stressModes)
    {
        AddStressMode(stressMode);
    }

    return rc;
}

struct PollHpdGpio_Args
{
    LwDispUtils::HPDGpio *HpdGpio;
    DisplayID displayID;
    bool IsConnectionExpected;
};

static bool GpuPollRegValue
(
   void * Args
)
{
    PollHpdGpio_Args * pArgs = static_cast<PollHpdGpio_Args*>(Args);

    bool isConnected = false;
    pArgs->HpdGpio->IsPanelConnected(pArgs->displayID, &isConnected);

    return (isConnected == pArgs->IsConnectionExpected);
}

RC DisplayCrcChecker::WaitForConnectionAndReadResponse
(
    IotSuperSwitch *pIotSuperSwitch,
    Tee::Priority hpdDebugPrintPriority,
    const DisplayID& displayID,
    UINT32 timeoutMs,
    bool isConnectionExpected
)
{
    StickyRC rc;
    string response;

    rc = WaitForConnection(hpdDebugPrintPriority, displayID, timeoutMs, isConnectionExpected);

    // ReadResponse from SuperSwitch
    rc = pIotSuperSwitch->ReadResponse(&response);

    if (isConnectionExpected)
    {
        Tasker::Sleep(m_PostHPDDelayMs);
    }
        
    return rc;
}

RC DisplayCrcChecker::WaitForConnection
(
    Tee::Priority hpdDebugPrintPriority,
    const DisplayID& displayID,
    UINT32 timeoutMs,
    bool isConnectionExpected
)
{
    RC rc;

    PollHpdGpio_Args Args =
    {
        m_pHPDGpio.get(),
        displayID,
        isConnectionExpected
    };


    bool isConnected = false;
    CHECK_RC(m_pHPDGpio->IsPanelConnected(displayID, &isConnected));
    Printf(hpdDebugPrintPriority, "Initial state for 0x%x: Panel %s\n", displayID.Get(),
        isConnected ? "Connected" : "Disconnected");

    const UINT64 waitStartMs = Xp::GetWallTimeMS();
    Printf(hpdDebugPrintPriority, "Wait for %s\n",
        isConnectionExpected ? "connection" : "disconnection");
    rc = POLLWRAP_HW(GpuPollRegValue, &Args, timeoutMs);

    CHECK_RC(m_pHPDGpio->IsPanelConnected(displayID, &isConnected));
    Printf(hpdDebugPrintPriority, "Final state for 0x%x: Panel %s\n", displayID.Get(),
        isConnected ? "Connected" : "Disconnected");
    Printf(hpdDebugPrintPriority, "WaitForConnection Finished: Poll Time %llu ms\n",
        Xp::GetWallTimeMS() - waitStartMs);

    if (isConnected != isConnectionExpected || rc != RC::OK)
    {
        Printf(Tee::PriError, "WaitForConnection failed. Connection expectation not fulfilled\n");
        return RC::HW_STATUS_ERROR;
    }

    return rc;
}

#endif

RC DisplayCrcChecker::RunNonIotMode()
{
    RC rc;

    CHECK_RC(RunDisplayCRCInSequential());

    return rc;
}

RC DisplayCrcChecker::RunDisplayCRCInSequential()
{
    RC rc;
    // CRC test algorithm
    // for each CRC checker device
    // {
    //    Reset device
    // }
    //
    // for each stress mode
    // {
    //    for each CRC checker device
    //    {
    //       Draw image
    //       for each inner loop
    //       {
    //          Clear stale CRCs
    //          Configure CRC test
    //          Start CRC test
    //          Collect CRCs
    //          Compare CRCs
    //       }
    //    }
    // }

    CHECK_RC(ResetCrcCheckerDevices());
    for (UINT32 stressModeIdx = 0; stressModeIdx < m_StressModes.size(); stressModeIdx++)
    {
        const ModeDescription& stressMode = m_StressModes[stressModeIdx];

        for (const auto& pCrcCheckerDevice : m_CrcCheckerDeviceList)
        {
            if (m_LimitVisibleBwFromEDID)
            {
                bool isVisibleBandwidthAllowed = true;
                CHECK_RC(IsVisibleBandwidthAllowed(pCrcCheckerDevice->GetDisplayId(),
                    stressMode, &isVisibleBandwidthAllowed));
                if (!isVisibleBandwidthAllowed)
                {
                    continue;
                }
            }

            CrcCheckerDevice::BoardInfo brdInfo;
            pCrcCheckerDevice->GetBoardInfo(&brdInfo);

            CHECK_RC(PrintCrcExperimentInfo(GetVerbosePrintPri(), pCrcCheckerDevice.get(),
                brdInfo, stressMode));

            ColorUtils::Format bgFormat = ColorUtils::A8R8G8B8;
            CHECK_RC(PrepareBackground(stressModeIdx, bgFormat));

            CHECK_RC(PerformSetMode(pCrcCheckerDevice->GetDisplayId(), stressMode));

            Surface2D *image = m_pSurfaceFill->GetImage(stressModeIdx);
            CHECK_RC(m_pDisplayHAL->DisplayImage(pCrcCheckerDevice->GetDisplayId(),
                image, brdInfo.boardType));

            if (m_DumpPngPattern)
            {
                CHECK_RC(DumpImage(stressModeIdx, image));
            }

            CHECK_RC(RunAllInnerLoops(stressMode, pCrcCheckerDevice.get()));

            // Make sure that the next SetMode will occur without any image
            CHECK_RC(m_pDisplayHAL->DisplayImage(pCrcCheckerDevice->GetDisplayId(),
                nullptr, brdInfo.boardType));

            CHECK_RC(m_pDisplayHAL->DetachDisplay(pCrcCheckerDevice->GetDisplayId()));

            CHECK_RC(PrintDebugInfoPostSetMode(GetVerbosePrintPri(), pCrcCheckerDevice.get()));
        }
    }
    CHECK_RC(m_ContinuationRC);

    if (!m_WasAnyStressModeTested)
    {
        Printf(Tee::PriError,
               "\nDisplayCrcChecker test skipped since no stress mode was tested!\n");
        return RC::NO_TESTS_RUN;
    }

    return rc;

}

RC DisplayCrcChecker::RunDisplayCRCInParallel()
{
    RC rc;
    // CRC test algorithm
    // for each CRC checker device
    // {
    //    Reset device
    // }
    //
    // for each stress mode
    // {
    //    for each CRC checker device
    //    {
    //       Draw image
    //    }
    //    for each CRC checker device
    //    {
    //       for each inner loop
    //       {
    //          Clear stale CRCs
    //          Configure CRC test
    //          Start CRC test
    //          Collect CRCs
    //          Compare CRCs
    //       }
    //    }
    // }

    UINT32 displayMaskToTest = 0;
    for (const auto& pCrcCheckerDevice : m_CrcCheckerDeviceList)
    {
        displayMaskToTest |= pCrcCheckerDevice->GetDisplayId();
    }

    CHECK_RC(ResetCrcCheckerDevices());
    s_ParallelFlag = true;

    if (displayMaskToTest != m_pDisplay->Selected())
        CHECK_RC(m_pDisplay->Select(displayMaskToTest));

    for (UINT32 stressModeIdx = 0; stressModeIdx < m_StressModes.size(); stressModeIdx++)
    {
        const ModeDescription& stressMode = m_StressModes[stressModeIdx];
        for (const auto& pCrcCheckerDevice : m_CrcCheckerDeviceList)
        {
            if (m_LimitVisibleBwFromEDID)
            {
                bool isVisibleBandwidthAllowed = true;
                CHECK_RC(IsVisibleBandwidthAllowed(pCrcCheckerDevice->GetDisplayId(),
                    stressMode, &isVisibleBandwidthAllowed));
                if (!isVisibleBandwidthAllowed)
                {
                    continue;
                }
            }

            CrcCheckerDevice::BoardInfo brdInfo;
            pCrcCheckerDevice->GetBoardInfo(&brdInfo);

            CHECK_RC(PrintCrcExperimentInfo(GetVerbosePrintPri(), pCrcCheckerDevice.get(),
                brdInfo, stressMode));

            ColorUtils::Format bgFormat = ColorUtils::A8R8G8B8;
            CHECK_RC(PrepareBackground(stressModeIdx, bgFormat));

            CHECK_RC(PerformSetMode(pCrcCheckerDevice->GetDisplayId(), stressMode));

            Surface2D *image = m_pSurfaceFill->GetImage(stressModeIdx);
            CHECK_RC(m_pDisplayHAL->DisplayImage(pCrcCheckerDevice->GetDisplayId(),
                image, brdInfo.boardType));

            if (m_DumpPngPattern)
            {
                CHECK_RC(DumpImage(stressModeIdx, image));
            }

        }

        for (const auto& pCrcCheckerDevice : m_CrcCheckerDeviceList)
        {
            CHECK_RC(RunAllInnerLoops(stressMode, pCrcCheckerDevice.get()));
        }

        for (const auto& pCrcCheckerDevice : m_CrcCheckerDeviceList)
        {
            CrcCheckerDevice::BoardInfo brdInfo;
            pCrcCheckerDevice->GetBoardInfo(&brdInfo);
            // Make sure that the next SetMode will occur without any image
            CHECK_RC(m_pDisplayHAL->DisplayImage(pCrcCheckerDevice->GetDisplayId(),
                nullptr, brdInfo.boardType));

            CHECK_RC(m_pDisplayHAL->DetachDisplay(pCrcCheckerDevice->GetDisplayId()));

            CHECK_RC(PrintDebugInfoPostSetMode(GetVerbosePrintPri(), pCrcCheckerDevice.get()));

        }
    }
    CHECK_RC(m_ContinuationRC);
    s_ParallelFlag = false;

    if (!m_WasAnyStressModeTested)
    {
        Printf(Tee::PriError,
            "\nDisplayCrcChecker test skipped since no stress mode was tested!\n");
        return RC::NO_TESTS_RUN;
    }

    return rc;

}

RC DisplayCrcChecker::PerformSetMode(const DisplayID& displayID, const ModeDescription& stressMode)
{
    StickyRC rc;

    UINT32 head;
    CHECK_RC(m_pDisplayHAL->GetHead(displayID, &head));

    rc = m_pDisplayHAL->SetMode(displayID, stressMode, head);
    if (rc == RC::MODE_IS_NOT_POSSIBLE)
    {
        Printf(GetVerbosePrintPri(), "Skipping stress mode ...\n%s\n",
            stressMode.GetStringDescription().c_str());
        return OK;
    }
    return rc;
}

RC DisplayCrcChecker::RunAllInnerLoops
(
    const ModeDescription& stressMode,
    CrcCheckerDevice* const & pCrcCheckerDevice
)
{
    RC rc;

    CrcCheckerDevice::CrcCheckerConfig crcConfig;
    crcConfig.NumOfFrames = m_NumOfFrames;
    crcConfig.PixelDepth = stressMode.depth;
    crcConfig.ExpectedCrc = stressMode.expectedCrc;
    crcConfig.RefreshRate = stressMode.refreshRate;

    // Inner loop starts here
    for (UINT32 loop = 0; loop < m_pTestConfig->Loops(); loop++)
    {
        if (m_pTestConfig->Loops() > 1)
        {
            Printf(GetVerbosePrintPri(), "DisplayCrcChecker loop %d of %d\n",
                    (loop + 1), m_pTestConfig->Loops());
        }
        rc = RunInnerLoop(pCrcCheckerDevice, &crcConfig, stressMode);
        if (rc == RC::GOLDEN_VALUE_MISCOMPARE)
        {
            if (m_ContinueOnCrcError)
            {
                m_ContinuationRC = rc;
                continue;
            }
        }
        else if (rc == RC::MODE_IS_NOT_POSSIBLE)
        {
            continue;
        }
        CHECK_RC(rc);
    }

    return rc;
}

RC DisplayCrcChecker::RunInnerLoop
(
    CrcCheckerDevice *pCrcChecker,
    CrcCheckerDevice::CrcCheckerConfig *pConfig,
    const ModeDescription& stressMode
)
{
    RC rc;
    MASSERT(pCrcChecker);
    MASSERT(pConfig);

    // Clean up previous loop state
    CHECK_RC(pCrcChecker->ClearStaleCrcs());
    CHECK_RC(pCrcChecker->ConfigureCrcExperiment(pConfig));

    CHECK_RC(IsModePossibleForCrcChecker(pCrcChecker, stressMode));

    if (m_DumpCrcCheckerRegisters)
    {
        CHECK_RC(DumpCrcDeviceRegisters(pCrcChecker, "Before CRC checker experiment",
            GetVerbosePrintPri()));
    }

    // Start the CRC test
    StickyRC rc_crc = pCrcChecker->RunCrcExperiment();
    if (rc_crc != OK)
    {
        CHECK_RC(DumpCrcDeviceRegisters(pCrcChecker, "Failed CRC checker experiment",
            GetVerbosePrintPri()));
        CHECK_RC(rc_crc);
    }

    // Collect and validate the captured Crcs
    vector <UINT32> crcsCollected;
    CHECK_RC(pCrcChecker->GetCrcs(&crcsCollected));

    CHECK_RC(ValidateCapturedCrcCount(pCrcChecker, pConfig, crcsCollected));

    vector<UINT32> failedCrcIndices;
    rc_crc = ParseCapturedCrcs(crcsCollected, stressMode.expectedCrc, &failedCrcIndices);
    rc_crc = PrintParseResult(pCrcChecker, crcsCollected,
        stressMode.expectedCrc, failedCrcIndices);
    CHECK_RC(rc_crc);

    return rc;
}

RC DisplayCrcChecker::IsModePossibleForCrcChecker
(
    CrcCheckerDevice* pCrcChecker,
    const ModeDescription& stressMode
)
{
    // Only proceed if the display receiver chip can see the incoming VSYNC
    if (!pCrcChecker->IsVideoSignalDetected(m_VsyncPollTimeout))
    {
        // IMP may allow a mode, but the receiver chip may not
        // support certain display modes. In which case we should
        // treat this as "MODE_IS_NOT_POSSIBLE"
        Printf(Tee::PriWarn, "CRC checker not responding, bailing ...\n%s\n",
                stressMode.GetStringDescription().c_str());
        return RC::MODE_IS_NOT_POSSIBLE;
    }
    return OK;
}

RC DisplayCrcChecker::ValidateCapturedCrcCount
(
    CrcCheckerDevice *pCrcChecker,
    CrcCheckerDevice::CrcCheckerConfig* pConfig,
    const vector<UINT32>& crcsCollected
)
{
    RC rc;

    // Sanity check if the right no. of CRCs were captured
    if (crcsCollected.size() != pConfig->NumOfFrames)
    {
        Printf(Tee::PriError,
                "Crcs captured do not match no. of test frames, Received = %d, Expected = %d\n",
                static_cast<UINT32>(crcsCollected.size()), pConfig->NumOfFrames);

        Printf(Tee::PriError, "\n*** CRC checker device register dump ***\n");
        CHECK_RC(pCrcChecker->DumpDeviceRegisters(Tee::PriError));

        return RC::CRC_CAPTURE_FAILED;
    }

    return rc;
}

RC DisplayCrcChecker::ParseCapturedCrcs
(
    const vector<UINT32>& crcsCollected,
    UINT32 expectedCrc,
    vector<UINT32> *pFailedCrcIndices
)
{
    RC rc;

    UINT32 mismatchedCrcCount = 0;

    // Check the CRCs for discontinuity
    UINT32 crcIdx;
    for (crcIdx = 0; crcIdx < crcsCollected.size(); crcIdx++)
    {
        if (expectedCrc != crcsCollected[crcIdx])
        {
            // WAR for Bug 200698136
            pFailedCrcIndices->push_back(crcIdx);
            ++mismatchedCrcCount;
            if (mismatchedCrcCount > m_AllowedMismatchedCrcCount)
            {
                // Parse till end
                rc = RC::GOLDEN_VALUE_MISCOMPARE;
            }
        }
    }

    m_WasAnyStressModeTested = true;

    if (rc != RC::OK)
    {
        Printf(Tee::PriError, "\n*** Allowed Mismatched Crc Count %u "
            "exceeded threshold value***\n", mismatchedCrcCount);
    }

    return rc;
}

RC DisplayCrcChecker::PrintParseResult
(
    CrcCheckerDevice *pCrcChecker,
    const vector<UINT32>& crcsCollected,
    UINT32 expectedCrc,
    const vector<UINT32>& failedCrcIndices
)
{
    RC rc;

    bool isGoldenMismatch = failedCrcIndices.size() > m_AllowedMismatchedCrcCount;

    Printf(isGoldenMismatch ? Tee::PriError : Tee::PriNormal,
        "\nCaptured %u correct CRCs = 0x%08x\n",
        static_cast<UINT32>(crcsCollected.size() - failedCrcIndices.size()), expectedCrc);

    if (failedCrcIndices.size())
    {
        Tee::Priority prio = isGoldenMismatch ? Tee::PriError : Tee::PriWarn;

        Printf(prio, "\nCaptured %u incorrect CRCs\n",
            static_cast<UINT32>(failedCrcIndices.size()));

        for (UINT32 idx = failedCrcIndices.front(); idx < crcsCollected.size(); idx++)
        {
            if (expectedCrc != crcsCollected[idx])
                Printf (prio, "    CRC [%d] = 0x%08x\n", idx, crcsCollected[idx]);
        }

        if (isGoldenMismatch)
        {
            CHECK_RC(pCrcChecker->DumpDeviceRegisters(Tee::PriError));
        }
    }
    return rc;
}

RC DisplayCrcChecker::Cleanup()
{
    StickyRC rc;

    if (m_pDisplay)
        m_pDisplay->SetIsModePossiblePriority(m_OrigIMP);

    if (m_pDisplayHAL)
        rc = m_pDisplayHAL->Cleanup();

    m_CrcCheckerDeviceList.clear();

    if (m_pSurfaceFill)
        rc = m_pSurfaceFill->Cleanup();

    m_PStateOwner.ReleasePStates();

    rc = GpuTest::Cleanup();

    return rc;
}

bool DisplayCrcChecker::IsSupported()
{
    GpuDevice * pGpuDev = GetBoundGpuDevice();
    Display *pDisplay = GetDisplay();

    if ((pDisplay->GetDisplayClassFamily() != Display::EVO) &&
        (pDisplay->GetDisplayClassFamily() != Display::TEGRA_ANDROID) &&
        (pDisplay->GetDisplayClassFamily() != Display::LWDISPLAY))
    {
        // Need one of: EVO display, CheetAh display, CheetAh Android display, LWDisplay
        return false;
    }

    if (GetBoundGpuSubdevice()->GetPerf()->IsOwned())
    {
        Printf(Tee::PriNormal,
               "Skipping %s because another test is holding pstates\n",
               GetName().c_str());
        return false;
    }

    if (pGpuDev->GetNumSubdevices() > 1)
    {
        return false;
    }

    if (!m_IOTConfigFilePath.empty() && pDisplay->GetDisplayClassFamily() != Display::LWDISPLAY)
    {
        Printf(Tee::PriNormal, "IOT mode is only supported on LwDisplay\n");
        return false;
    }

    return true;
}

//----------------------------------------------------------------------------
// Initialize the properties.

RC DisplayCrcChecker::InitProperties()
{
    if (m_DisplaysToTest)
        m_SkipDisplayMask = ~m_DisplaysToTest;

    m_VsyncPollTimeout = m_pTestConfig->TimeoutMs();

    if (m_pTestConfig->DisableCrt())
          Printf(Tee::PriLow, "NOTE: ignoring TestConfiguration.DisableCrt.\n");

    return OK;
}

//----------------------------------------------------------------------------
void DisplayCrcChecker::ClearStressModes()
{
    m_StressModes.clear();
}

//----------------------------------------------------------------------------
void DisplayCrcChecker::AddStressMode(const ModeDescription& mode)
{
    m_StressModes.push_back(mode);
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(DisplayCrcChecker,
                  SetStressModes,
                  1,
                  "Set the stress modes that DisplayCrcChecker will use")
{
    JavaScriptPtr pJs;

    // Check the arguments.
    JsArray modes;
    if
    (
            (NumArguments != 1)
            || (OK != pJs->FromJsval(pArguments[0], &modes))
    )
    {
        JS_ReportError(pContext,
                       "Usage: DisplayCrcChecker.SetStressModes([modes])");
        return JS_FALSE;
    }

    DisplayCrcChecker * pDisplayCrcChecker;
    if ((pDisplayCrcChecker = JS_GET_PRIVATE(DisplayCrcChecker,
                                             pContext,
                                             pObject,
                                             "DisplayCrcChecker")) != 0)
    {
        pDisplayCrcChecker->ClearStressModes();
        for (UINT32 i = 0; i < modes.size(); i++)
        {
            JsArray jsa;
            ModeDescription tmpMode;
            if (
               (OK != pJs->FromJsval(modes[i], &jsa)) ||
               (jsa.size() != 6) ||
               (OK != pJs->FromJsval(jsa[0], &tmpMode.width)) ||
               (OK != pJs->FromJsval(jsa[1], &tmpMode.height)) ||
               (OK != pJs->FromJsval(jsa[2], &tmpMode.depth)) ||
               (OK != pJs->FromJsval(jsa[3], &tmpMode.refreshRate)) ||
               (OK != pJs->FromJsval(jsa[4], &tmpMode.expectedCrc)) ||
               (OK != pJs->FromJsval(jsa[5], &tmpMode.testPattern))
               )
            {
                JS_ReportError(pContext,
                            "Usage: DisplayCrcChecker.SetStressModes([modes])");
                return JS_FALSE;
            }

            pDisplayCrcChecker->AddStressMode(tmpMode);
        }

        return JS_TRUE;
    }

    return JS_FALSE;
}
