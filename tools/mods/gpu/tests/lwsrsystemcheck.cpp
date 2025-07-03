/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
/**
* @file   lwsrsystemcheck.cpp
* @brief  LWSR system check test
*
*/
#include <cmath>
#include "class/cl0004.h"
#include "core/include/cpu.h"
#include "core/include/display.h"
#include "core/include/modsedid.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "ctrl/ctrl0073.h"
#include "device/meter/colorimetryresearch.h"
#include "dpu/dpuifscanoutlogging.h"
#include "gpu/display/modeset_utils.h"
#include "gpu/display/lwdisplay/lw_disp.h"
#include "gpu/display/tcon_device.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/js/fpk_comm.h"
#include "gpu/utility/gpurectfill.h"
#include "gputest.h"
#include "nbci.h"
#include "lwRmApi.h"
#include "rmpbicmdif.h"

class LWSRDisplay
{
public:
    LWSRDisplay(Display *pDisplay);
    virtual ~LWSRDisplay() = default;

    enum SetModeState
    {
        NoSetMode,
        NativeSetMode,
        BurstSetMode
    };
    virtual RC InitialSetup() = 0;
    virtual RC Cleanup() = 0;
    virtual RC SetMode() = 0;
    virtual RC SetImage() = 0;
    virtual RC SetModeAndImage(RasterSettings *pRasterSettings, SetModeState setModeState) = 0;
    virtual RC SetControlSlaveLockPin(bool enable, UINT32 pin) = 0;
    virtual RC SetModeWithLockPin(bool enable, UINT32 pin) = 0;
    virtual RC DisableDisplay() = 0;
    virtual RC SetStallLock(bool enable, UINT32 pin) = 0;
    auto GetPanelID() { return m_PanelID; }
    void SetDispSurface(Surface2D *pSurf) { m_pDisplaySurface = pSurf; }
protected:
    UINT32 m_PanelID = 0;
    Display *m_pDisplay = nullptr;
    Tee::Priority m_Pri = Tee::PriLow;
    UINT32 m_NativeWidth = 0;
    UINT32 m_NativeHeight = 0;
    UINT32 m_NativeRR = 0;
    UINT32 m_NativeDepth = 0;
    Surface2D *m_pDisplaySurface = nullptr;
    SetModeState m_LwrrentSetModeState = NoSetMode;
};

class LWSREvoDisplay : public LWSRDisplay
{
public:
    LWSREvoDisplay(Display *pDisplay);
    virtual ~LWSREvoDisplay() = default;
    RC InitialSetup() override;
    RC Cleanup() override;
    RC SetMode() override;
    RC SetImage() override;
    RC SetModeAndImage(RasterSettings *pRasterSettings, SetModeState setModeState) override;
    RC SetControlSlaveLockPin(bool enable, UINT32 pin) override;
    RC SetModeWithLockPin(bool enable, UINT32 pin) override;
    RC DisableDisplay() override;
    RC SetStallLock(bool enable, UINT32 pin) override;
};

class LWSRLWDisplay : public LWSRDisplay
{
public:
    LWSRLWDisplay(Display *pDisplay);
    virtual ~LWSRLWDisplay() = default;
    RC InitialSetup() override;
    RC Cleanup() override;
    RC SetMode() override;
    RC SetImage() override;
    RC SetModeAndImage(RasterSettings *pRasterSettings, SetModeState setModeState) override;
    RC SetControlSlaveLockPin(bool enable, UINT32 pin) override;
    RC SetModeWithLockPin(bool enable, UINT32 pin) override;
    RC SetControlSlaveLockPinCBArgs(void * pCbArgs);
    RC SetDesktopColorLW();
    RC SetRasterSettingsCB(void *cbArgs);
    RC DisableDisplay() override;
    RC SetStallLock(bool enable, UINT32 pin) override;

    struct FrameData
    {
        bool enable;
        UINT32 pin;
    };

private:
    LwDisplay *m_pLwDisplay = nullptr;
    LwDispCoreChnContext *m_pCoreContext = nullptr;
    HeadIndex m_HeadIdx = Display::ILWALID_HEAD;
    RC SetModeWithCB(std::function<RC(void *)> cbFunc, void *cbArgs);
    RC SendUpdates();
};

LWSRDisplay::LWSRDisplay(Display *pDisplay) : m_pDisplay(pDisplay) {}

LWSREvoDisplay::LWSREvoDisplay(Display *pDisplay)
: LWSRDisplay(pDisplay)
{
    m_PanelID = pDisplay->Selected();
}

RC LWSREvoDisplay::InitialSetup()
{
    return OK;
}

RC LWSREvoDisplay::Cleanup()
{
    StickyRC rc;
    rc = SetModeAndImage(nullptr, NoSetMode);
    return rc;
}

RC LWSREvoDisplay::SetMode()
{
    RC rc;
    CHECK_RC(m_pDisplay->SetMode(m_NativeWidth, m_NativeHeight, m_NativeDepth, m_NativeRR));
    return rc;
}

RC LWSREvoDisplay::SetImage()
{
    RC rc;
    CHECK_RC(m_pDisplay->SetImage(m_pDisplaySurface));
    return rc;
}

RC LWSREvoDisplay::SetModeAndImage(RasterSettings *pRasterSettings, SetModeState setModeState)
{
    RC rc;

    auto pEvoRasterSettings = static_cast<EvoRasterSettings*>(pRasterSettings);
    // This is needed to prevent exelwting a SetMode while some
    // previous "SetImage" is still active. Such situation
    // is now signalled with SOFTWARE_ERROR during SetMode.
    CHECK_RC(m_pDisplay->SetImage(static_cast<Surface2D*>(nullptr)));

    CHECK_RC(m_pDisplay->SetTimings(pEvoRasterSettings));

    // This is needed to avoid internal display pipe rounding which results
    // in pixel values 254 and 255 to come out the same on the output
    CHECK_RC(m_pDisplay->SelectColorTranscoding(m_PanelID, Display::ZeroFill));

    m_pDisplay->SetPendingSetModeChange();

    if (pEvoRasterSettings != nullptr)
    {
        CHECK_RC(m_pDisplay->SetMode(
            pEvoRasterSettings->ActiveX,
            pEvoRasterSettings->ActiveY,
            32,
            UINT32(0.5 + pEvoRasterSettings->CallwlateRefreshRate())));

        CHECK_RC(m_pDisplay->SetImage(m_pDisplaySurface));
    }

    Tasker::Sleep(100);

    return rc;
}

RC LWSREvoDisplay::DisableDisplay()
{
    RC rc;
    CHECK_RC(m_pDisplay->GetMode(m_PanelID,
        &m_NativeWidth, &m_NativeHeight, &m_NativeDepth, &m_NativeRR));
    CHECK_RC(m_pDisplay->Disable());
    return OK;
}

RC LWSREvoDisplay::SetControlSlaveLockPin(bool enable, UINT32 pin)
{
    RC rc;
    CHECK_RC(m_pDisplay->SetHeadSetControlSlaveLock(m_PanelID, enable, pin));
    return rc;
}

RC LWSREvoDisplay::SetModeWithLockPin(bool enable, UINT32 pin)
{
    RC rc;

    CHECK_RC(SetControlSlaveLockPin(enable, pin));
    CHECK_RC(SetMode());

    return rc;
}

RC LWSREvoDisplay::SetStallLock(bool enable, UINT32 pin)
{
    RC rc;
    CHECK_RC(m_pDisplay->SetStallLock(m_PanelID, enable, pin));
    return rc;
}

LWSRLWDisplay::LWSRLWDisplay(Display *pDisplay)
: LWSRDisplay(pDisplay)
{
    m_PanelID = pDisplay->GetPrimaryDisplay();
    m_pLwDisplay = static_cast<LwDisplay *>(pDisplay);
}

RC LWSRLWDisplay::InitialSetup()
{
    RC rc;

    LwDispChnContext *pLwDispChnContext = nullptr;
    CHECK_RC(m_pLwDisplay->
           GetLwDisplayChnContext(Display::CORE_CHANNEL_ID,
                &pLwDispChnContext,
                LwDisplay::ILWALID_WINDOW_INSTANCE,
                Display::ILWALID_HEAD));

    if (pLwDispChnContext == nullptr)
    {
        return RC::SOFTWARE_ERROR;
    }

    m_pCoreContext = static_cast<LwDispCoreChnContext *>(pLwDispChnContext);
    CHECK_RC(m_pDisplay->GetEdidMember()->GetDdNativeRes(m_PanelID,
        &m_NativeWidth, &m_NativeHeight, &m_NativeRR));
    m_NativeDepth = 32;

    UINT32 headMask = 0;
    CHECK_RC(m_pLwDisplay->GetHeadMask(m_PanelID, &headMask));
    // Find first usable head
    m_HeadIdx = Utility::BitScanForward(headMask);

    // Need to perform single modeset as AllocDisplay has disabled the
    // display through DeactivateComposition mode
    CHECK_RC(SetMode());
    return rc;
}

RC LWSRLWDisplay::Cleanup()
{
    StickyRC rc;
    rc = m_pLwDisplay->ResetEntireState();
    return rc;
}

RC LWSRLWDisplay::SetMode()
{
    RC rc;
    CHECK_RC(SetModeWithCB(nullptr, nullptr));
    m_LwrrentSetModeState = NativeSetMode;
    return rc;
}

RC LWSRLWDisplay::SetImage()
{
    return OK;
}

RC LWSRLWDisplay::SetModeWithCB(std::function<RC(void *)> cbFunc, void *cbArgs)
{
    RC rc;
    const Display::Mode resolutionInfo(m_NativeWidth, m_NativeHeight, m_NativeDepth, m_NativeRR);
    CHECK_RC(m_pLwDisplay->SetModeWithSingleWindow(
        m_PanelID, resolutionInfo, m_HeadIdx, cbFunc, cbArgs));
    return rc;
}

RC LWSRLWDisplay::SetModeAndImage(RasterSettings *pRasterSettings, SetModeState setModeState)
{
    RC rc;

    if (m_LwrrentSetModeState != setModeState)
    {
        CHECK_RC(SetModeWithCB(std::bind(
            &LWSRLWDisplay::SetRasterSettingsCB, this, std::placeholders::_1),
            pRasterSettings));
        m_LwrrentSetModeState = setModeState;
    }

    CHECK_RC(m_pLwDisplay->DisplayImage(m_PanelID, m_pDisplaySurface, nullptr));
    return rc;
}

RC LWSRLWDisplay::SetRasterSettingsCB(void *pCbArgs)
{
    auto pRasterSettings = reinterpret_cast<LwDispRasterSettings *>(pCbArgs);
    auto &headSettings = m_pCoreContext->DispLwstomSettings.HeadSettings[m_HeadIdx];
    headSettings.rasterSettings = *pRasterSettings;
    headSettings.rasterSettings.Dirty = true;
    headSettings.pixelClkSettings.pixelClkFreqHz = pRasterSettings->PixelClockHz;
    headSettings.pixelClkSettings.dirty = true;
    return OK;
}

RC LWSRLWDisplay::DisableDisplay()
{
    RC rc;
    CHECK_RC(m_pLwDisplay->DetachDisplay(DisplayIDs(1, m_PanelID)));
    CHECK_RC(m_pLwDisplay->DetachAllWindows());
    m_LwrrentSetModeState = NoSetMode;
    return rc;
}

RC LWSRLWDisplay::SetControlSlaveLockPin(bool enable, UINT32 pin)
{
    RC rc;
    LwDispControlSettings *pControlSettings =
        &m_pCoreContext->DispLwstomSettings.HeadSettings[m_HeadIdx].controlSettings;

    if (enable)
    {
        pControlSettings->slaveLockMode = LwDispControlSettings::FRAME_LOCK;
        pControlSettings->slaveLockPin.lockPilwalue =
            static_cast<LwDispControlSettings::LockPilwalue>(pin);
        pControlSettings->slaveLockPin.internalScanLockPilwalue =
            static_cast<LwDispControlSettings::InternalScanLockPilwalue>(pin);
        pControlSettings->slaveLockout = 4;
    }
    else
    {
        pControlSettings->slaveLockMode = LwDispControlSettings::NO_LOCK;
        pControlSettings->slaveLockPin.lockPilwalue =
            LwDispControlSettings::LOCK_PIN_VALUE_NONE;
    }

    pControlSettings->slaveLockPin.lockPinType = LwDispControlSettings::LOCK_PIN_TYPE_PIN;
    pControlSettings->dirty = true;

    return rc;
}

RC LWSRLWDisplay::SetControlSlaveLockPinCBArgs(void * pCbArgs)
{
   RC rc;

   LWSRLWDisplay::FrameData *pFrameData =
                   reinterpret_cast<LWSRLWDisplay::FrameData *>(pCbArgs);
   CHECK_RC(SetControlSlaveLockPin(pFrameData->enable, pFrameData->pin));
   CHECK_RC(SetDesktopColorLW());

   return rc;
}

RC LWSRLWDisplay::SetDesktopColorLW()
{

    LwDispHeadSettings *pHeadSettings =
        &m_pCoreContext->DispLwstomSettings.HeadSettings[m_HeadIdx];

    // Show the official LWPU green when no window covers the desktop:
    pHeadSettings->miscSettings.desktopColor.Red = 118;
    pHeadSettings->miscSettings.desktopColor.Green = 185;
    pHeadSettings->miscSettings.desktopColor.Blue = 0;
    pHeadSettings->miscSettings.dirty = true;

    return OK;
}

RC LWSRLWDisplay::SetModeWithLockPin(bool enable, UINT32 pin)
{
    RC rc;

    if (enable)
    {
        FrameData frameData;
        frameData.enable = enable;
        frameData.pin = pin;
        CHECK_RC(SetModeWithCB(std::bind(
            &LWSRLWDisplay::SetControlSlaveLockPinCBArgs,
            this, std::placeholders::_1),
            &frameData));
    }
    else 
    { 
        // Change as per Bug 200453780
        // In case of disable slave lock with FL, instead of calling a fresh 
        // setmode with complete disable of display, only sending the
        // controlsettings with InterlockedUpdates 
        CHECK_RC(SetControlSlaveLockPin(enable, pin));
        CHECK_RC(SendUpdates());
    }
    return rc;
}

RC LWSRLWDisplay::SetStallLock(bool enable, UINT32 pin)
{
    RC rc;
    CHECK_RC(m_pLwDisplay->SetStallLock(m_PanelID, enable, pin));
    CHECK_RC(SendUpdates());
    return rc;
}

RC LWSRLWDisplay::SendUpdates()
{
    return m_pLwDisplay->SendInterlockedUpdates
        (
            LwDisplay::UPDATE_CORE,
            0,
            0,
            0,
            0,
            LwDisplay::DONT_INTERLOCK_CHANNELS,
            LwDisplay::WAIT_FOR_NOTIFIER
        );
}

class LWSRSystemCheck : public GpuTest
{
public:
    LWSRSystemCheck();

    bool IsSupported();
    RC Setup();
    RC Run();
    RC Cleanup();

    SETGET_PROP(SkipPhotometricTests, bool);
    SETGET_PROP(SkipSmiTrapTest, bool);
    SETGET_PROP(SkipFramelockLatency, bool);
    SETGET_PROP(SkipMSHybridCheck, bool);
    SETGET_PROP(PrintSubtestRC, bool);
    SETGET_PROP(GC5Mode, UINT32);
    SETGET_PROP(FramelockFrameLatency, FLOAT64);
    SETGET_PROP(FramelockLatencyIterations, UINT32);
    SETGET_PROP(JTGCXBacklightTolerance, FLOAT64);
    SETGET_PROP(JTGCXBacklightVariance, FLOAT64);
    SETGET_PROP(SelfRefreshGetTimeDelayMS, FLOAT64);
    SETGET_PROP(JTGC6EnterLatencyMS, UINT32);
    SETGET_PROP(JTGC6ExitLatencyMS, UINT32);
    SETGET_PROP(JTGC5EnterLatencyMS, UINT32);
    SETGET_PROP(JTGC5ExitLatencyMS, UINT32);
    SETGET_PROP(MaxSbiosInLatencyMS, UINT32);
    SETGET_PROP(MaxSbiosOutLatencyMS, UINT32);

private:
    RC CallACPI(UINT32 func, UINT32 subFunc, UINT32 *rtlwalue, UINT16 * size);
    // Verify Power control function
    RC CheckGCxSequence();
    //Verify _PRT
    RC VerifyInterruptSteering();
    RC ReadJTCapabilities();
    RC VerifyJTCapabilities();
    RC VerifyJTSubfunctions();
    RC GetBrightness(UINT32 *level);
    RC SetBrightness(UINT32 level);
    RC VerifyBrightness();
    RC VerifyNBCIInterface();

    RC VerifySmiTrap();
    RC VerifyFramelockLatency();
    RC FramelockLatencyLoops
    (
        FLOAT64 numFramesAllowed,
        FLOAT64 panelRefreshRate,
        bool bCrashSyncEnabled
    );
    RC FreeLoggerBuffer();
    RC VerifyJTGCXBacklight();
    RC MeasureGCXBacklight(FLOAT64 baseline);
    RC VerifyJTGCXLatency();
    RC CheckGC6andNLTStats();

    GpuTestConfiguration *m_pTestConfig;
    GCxImpl              *m_pGCx;
    Surface2D             m_DisplaySurface;
    GpuRectFill           m_GpuRectFill;
    PStateOwner           m_PStateOwner;      //!< need to claim the pstates when doing GCx
    TCONDevice            m_TCONDevice;
    UINT32                m_frameLockPin;
    ColorimetryResearchInstruments m_CRInstrument;
    Xp::JTMgr            *m_pJT;
    UINT32                m_CRInstrumentIdx;
    RasterSettings        m_NativeRaster;
    LWT_TIMING            m_lwtsr;
    std::unique_ptr<LWSRDisplay> m_pSRDisplay;

    bool                  m_SkipPhotometricTests;
    bool                  m_SkipSmiTrapTest;
    bool                  m_SkipFramelockLatency;
    bool                  m_SkipMSHybridCheck;
    bool                  m_PrintSubtestRC;
    UINT32                m_GC5Mode;

    FLOAT64               m_DimBrightBacklightTolerance;

    FLOAT64               m_FramelockFrameLatency;
    UINT32                m_FramelockLatencyIterations;

    FLOAT64               m_JTGCXBacklightTolerance;
    FLOAT64               m_JTGCXBacklightVariance;
    FLOAT64               m_SelfRefreshGetTimeDelayMS;

    UINT32                m_JTGC6EnterLatencyMS;
    UINT32                m_JTGC6ExitLatencyMS;
    UINT32                m_JTGC5EnterLatencyMS;
    UINT32                m_JTGC5ExitLatencyMS;
    const UINT32          m_MaxGC6NLTLatencyUS = 500;
    UINT32                m_MaxSbiosInLatencyMS = 3;
    UINT32                m_MaxSbiosOutLatencyMS = 20;
    UINT32                m_WakeupEvent = 0x0;

    struct
    {
        bool    JTEnabled;
        bool    LWSREnabled;
        bool    MSHybridEnabled;
        UINT32  RootPortControl;
    } m_JTCapabilities;

    RC SetupNativeRaster();

    Perf::PerfPoint m_OrigPerfPoint;
    bool            m_RestorePerfPoint;
    RC MaximizeDispClk();
    std::unique_ptr<LWSRDisplay> CreateDisplayClass(Display *pDisplay);

    static const UINT32 VGA_REGISTER        = 0x3CA;
    static const UINT32 STATUS_REGISTER     = 0x80;
    static const UINT32 SMI_READY           = 0xAA;
    static const UINT32 ENABLE_SMI_VALUE    = 0xBA;
    static const UINT32 ENABLE_SMI_REGISTER = 0xB2;
};

LWSRSystemCheck::LWSRSystemCheck()
: m_pGCx(nullptr)
, m_pJT(nullptr)
, m_CRInstrumentIdx(0)
, m_SkipPhotometricTests(false)
, m_SkipSmiTrapTest(false)
, m_SkipFramelockLatency(false)
, m_SkipMSHybridCheck(false)
, m_PrintSubtestRC(true)
, m_GC5Mode(GCxImpl::gc5DI_OS)
, m_DimBrightBacklightTolerance(0.4)
, m_FramelockFrameLatency(2.0)
, m_FramelockLatencyIterations(10)
, m_JTGCXBacklightTolerance(40.0)
, m_JTGCXBacklightVariance(1.5)
, m_SelfRefreshGetTimeDelayMS(1.5)
, m_JTGC6EnterLatencyMS(30)
, m_JTGC6ExitLatencyMS(60)
, m_JTGC5EnterLatencyMS(40)
, m_JTGC5ExitLatencyMS(100)
, m_RestorePerfPoint(false)
{
    SetName("LWSRSystemCheck");
    m_pTestConfig = GetTestConfiguration();
    memset(&m_JTCapabilities, 0, sizeof(m_JTCapabilities));
}

bool LWSRSystemCheck::IsSupported()
{
    if (GetBoundGpuSubdevice()->IsSOC())
    {
        Printf(Tee::PriLow, "Skipping LWSRSystemCheck on SOC GPU\n");
        return false;
    }

    m_pGCx = GetBoundGpuSubdevice()->GetGCxImpl();
    if (m_pGCx == nullptr)
    {
        Printf(Tee::PriNormal, "LWSRSystemCheck is unable to use GCX\n");
        return false;
    }

    m_pGCx->SetVerbosePrintPriority(GetVerbosePrintPri());
    m_pGCx->SetBoundRmClient(GetBoundRmClient());
    m_pGCx->SetGc5Mode(static_cast<GCxImpl::gc5Mode>(m_GC5Mode));
    if (GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_GC5) &&
            !m_pGCx->IsGc5Supported(Perf::ILWALID_PSTATE))
    {
        Printf(Tee::PriNormal, "GC5 is not supported at current pstate.\n");
        return false;
    }
    if (!m_pGCx->IsGc6Supported(m_pGCx->GetGc6Pstate()))
    {
        Printf(Tee::PriNormal, "GC6 is not supported at current pstate.\n");
        return false;
    }

    return true;
}

RC LWSRSystemCheck::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::Setup());
    CHECK_RC(m_PStateOwner.ClaimPStates(GetBoundGpuSubdevice()));
    CHECK_RC(GpuTest::AllocDisplay());

    m_pGCx->SetVerbosePrintPriority(GetVerbosePrintPri());

    if (!m_pJT)
    {
        m_pJT = Xp::GetJTMgr(GetBoundGpuSubdevice()->GetGpuInst());
    }

    MASSERT(m_pJT);
    if (!m_pJT)
    {
        Printf(Tee::PriHigh, "JT is unsupported\n");
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    m_pGCx = GetBoundGpuSubdevice()->GetGCxImpl();
    m_pGCx->SetBoundRmClient(GetBoundRmClient());
    m_pGCx->SetSkipSwFuseCheck(true);
    if (GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_GC5))
    {
        CHECK_RC(m_pGCx->InitGc5());
    }
    CHECK_RC(m_pGCx->InitGc6(m_pTestConfig->TimeoutMs()));
    // LWSR systems require GC6 exit post processing task due to deferred
    // perf state load added by RM as an optimization for exit latency
    m_pGCx->EnableGC6ExitPostProcessing(true);

    // Use RTD3 events on Turing+
    m_WakeupEvent =
        GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_RTD3) ?
        GC_CMN_WAKEUP_EVENT_gc6rtd3_cpu : GC_CMN_WAKEUP_EVENT_gc6gpu;

    CHECK_RC(m_GpuRectFill.Initialize(m_pTestConfig));

    UINT32 numCRInstuments = 0;
    if (!m_SkipPhotometricTests &&
        (OK != m_CRInstrument.FindInstruments(&numCRInstuments) ||
        numCRInstuments == 0))
    {
        Printf(Tee::PriLow,
            "LWSRSystemCheck error: Photometric instruments not detected - some subtests will fail.\n");
    }

    Display* pDisplay = GetDisplay();
    m_pSRDisplay = CreateDisplayClass(pDisplay);
    MASSERT(m_pSRDisplay);
    auto panelID = m_pSRDisplay->GetPanelID();
    CHECK_RC(m_TCONDevice.Alloc(pDisplay, panelID));
    CHECK_RC(m_TCONDevice.UnlockBasicRegisters());
    CHECK_RC(m_TCONDevice.UnlockMutex());
    CHECK_RC(m_TCONDevice.InitPanel());

    Edid *modsEdid = pDisplay->GetEdidMember();
    LWT_EDID_INFO lwtei = {0};
    if (modsEdid->GetLwtEdidInfo(panelID, &lwtei) != OK)
    {
        Printf(Tee::PriError, "Unable to get EDID info of the panel\n");
        return RC::ILWALID_EDID;
    }
    CHECK_RC(m_TCONDevice.InitSelfRefreshTiming(lwtei, &m_lwtsr));

    LW0073_CTRL_SYSTEM_GET_VRR_CONFIG_PARAMS vrrConfig = {0};
    vrrConfig.subDeviceInstance = GetBoundGpuSubdevice()->GetSubdeviceInst();
    CHECK_RC_MSG(pDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_GET_VRR_CONFIG,
                                     &vrrConfig,
                                     sizeof(vrrConfig)),
                                     "Unable to get VRR config\n");
    m_frameLockPin = vrrConfig.frameLockPin;

    Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();
    CHECK_RC(pPerf->GetLwrrentPerfPoint(&m_OrigPerfPoint));

    if (m_TestConfig.Verbose())
        m_PrintSubtestRC = true;

    CHECK_RC(m_pSRDisplay->InitialSetup());
    CHECK_RC(SetupNativeRaster());
    return rc;
}

RC LWSRSystemCheck::SetupNativeRaster()
{
    RC rc;
    Display* pDisplay = GetDisplay();
    const UINT32 panelID = m_pSRDisplay->GetPanelID();
    Edid *modsEdid = pDisplay->GetEdidMember();

    UINT32 nativeWidth = 0;
    UINT32 nativeHeight = 0;
    UINT32 nativeRefresh = 0;
    CHECK_RC(modsEdid->GetDdNativeRes(panelID, &nativeWidth,
        &nativeHeight, &nativeRefresh));

    LWT_EDID_INFO lwtei = {0};
    LWT_TIMING lwtt = {0};

    if ((modsEdid->GetLwtEdidInfo(panelID, &lwtei) != OK) ||
        (LwTiming_GetEdidTiming(nativeWidth, nativeHeight, nativeRefresh, 0,
            &lwtei, &lwtt) != LWT_STATUS_SUCCESS))
    {
        Printf(Tee::PriNormal,
            "Error: Unable to determine native resolution of the panel\n");
        return RC::ILWALID_EDID;
    }

    CHECK_RC(ModesetUtils::ColwertLwtTiming2EvoRasterSettings(lwtt, &m_NativeRaster));

    m_DisplaySurface.SetWidth(m_NativeRaster.ActiveX);
    m_DisplaySurface.SetHeight(m_NativeRaster.ActiveY);
    m_DisplaySurface.SetColorFormat(ColorUtils::ColorDepthToFormat(32));
    m_DisplaySurface.SetLayout(Surface2D::BlockLinear);
    m_DisplaySurface.SetDisplayable(true);
    CHECK_RC(m_DisplaySurface.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_GpuRectFill.SetSurface(&m_DisplaySurface));

    m_pSRDisplay->SetDispSurface(&m_DisplaySurface);

    return rc;
}

RC LWSRSystemCheck::Run()
{
    RC rc;
    StickyRC subtestRc;
    bool stopOnError = GetGoldelwalues()->GetStopOnError();
    Display* pDisplay = GetDisplay();

    Utility::CleanupOnReturnArg<Display, void, bool>
        RestoreUsePersistentClass073Handle(pDisplay,
            &Display::SetUsePersistentClass073Handle,
            pDisplay->GetUsePersistentClass073Handle());

    // The persistent handle is needed for VRR / crash sync setup.
    // RM deletes VRR resources when the handle is closed.
    // Can't use a locally allocated handle as the aux read/writes
    // also need it inside of Display:: and there can be only one
    // allocated at a time.
    pDisplay->SetUsePersistentClass073Handle(true);

    // It is required for VerifyFramelockLatency, update this as well when enabling it.
    //CHECK_RC(MaximizeDispClk());

    Tee::Priority subtestRCPrintPri = (m_PrintSubtestRC) ? Tee::PriNormal : Tee::PriLow;

#define RunSubtest(subtestName)\
    {\
        Printf(subtestRCPrintPri, "Subtest: %-23s begin\n", #subtestName);\
        RC subtestName##RC = subtestName();\
        ErrorMap subtestName##ErrorMap(subtestName##RC);\
        Printf(subtestRCPrintPri, "Subtest: %-23s returned %012d (%s)\n",\
               #subtestName, subtestName##RC.Get(), subtestName##ErrorMap.Message());\
        subtestRc = subtestName##RC;\
        if (stopOnError)\
            CHECK_RC(subtestRc);\
    }
#define RunConditionalSubtest(condition, subtestName)\
    if (condition)\
    {\
        RunSubtest(subtestName);\
    }\
    else\
    {\
        Printf(subtestRCPrintPri, "Subtest: %-23s skipped\n", #subtestName);\
    }

    RunSubtest(VerifyJTSubfunctions);

    RunSubtest(VerifyJTCapabilities);

    RunSubtest(CheckGCxSequence);

    //RunSubtest(VerifyNBCIInterface);

    RunConditionalSubtest(!m_SkipPhotometricTests, VerifyBrightness);

    //RunConditionalSubtest(!m_SkipSmiTrapTest, VerifySmiTrap);

    //
    // Force PState from lowest (P8) to P0 because LWSR Burst mode needs the
    // highest clock.
    //
    Perf::PerfPoint oldPerfPt;
    Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();
    CHECK_RC(pPerf->GetLwrrentPerfPoint(&oldPerfPt));
    CHECK_RC(pPerf->ForcePState(0, LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR));
    RunConditionalSubtest(!m_SkipFramelockLatency, VerifyFramelockLatency);
    CHECK_RC(pPerf->SetPerfPoint(oldPerfPt));

    RunConditionalSubtest(!m_SkipPhotometricTests, VerifyJTGCXBacklight);

    RunSubtest(VerifyJTGCXLatency);

    return subtestRc;
}

RC LWSRSystemCheck::Cleanup()
{
    StickyRC rc;
    if (m_pGCx != nullptr)
    {
        m_pGCx->Shutdown();
        m_pGCx = nullptr;
    }

    rc = m_pSRDisplay->Cleanup();

    if (m_RestorePerfPoint)
    {
        Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();
        rc = pPerf->SetPerfPoint(m_OrigPerfPoint);
    }

    m_TCONDevice.Free();
    m_DisplaySurface.Free();
    rc = m_GpuRectFill.Cleanup();
    m_PStateOwner.ReleasePStates();
    rc = GpuTest::Cleanup();

    return rc;
}

std::unique_ptr<LWSRDisplay> LWSRSystemCheck::CreateDisplayClass(Display *pDisplay)
{
    switch (pDisplay->GetDisplayClassFamily())
    {
        case Display::LWDISPLAY:
            return make_unique<LWSRLWDisplay>(pDisplay);
        case Display::EVO:
            return make_unique<LWSREvoDisplay>(pDisplay);
        default:
            return nullptr;
    }
}

RC LWSRSystemCheck::MaximizeDispClk()
{
    RC rc;

    Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();
    Perf::PerfPoint lwrrPP;
    CHECK_RC(pPerf->GetLwrrentPerfPoint(&lwrrPP));

    UINT32 pstateNum = 0;
    CHECK_RC(pPerf->GetLwrrentPState(&pstateNum));
    Perf::ClkRange clkRange = {};
    CHECK_RC(pPerf->GetClockRange(pstateNum, Gpu::ClkDisp, &clkRange));

    UINT64 maxDispClkHz = static_cast<UINT64>(clkRange.MaxKHz) * 1000L;

    // Maximize the display clock first so that we can properly callwlate the max pixel clock.
    // Otherwise the starting display clock might be too low for any pixel clock.

    m_RestorePerfPoint = true;
    lwrrPP.Clks[Gpu::ClkDisp].FreqHz = maxDispClkHz;
    CHECK_RC(pPerf->SetPerfPoint(lwrrPP));

    EvoRasterSettings burstRaster;
    const UINT32 MINIMUM_BURST_PIXEL_CLOCK = 60000000;
    m_TCONDevice.CallwlateBurstRaster(&m_NativeRaster,
        MINIMUM_BURST_PIXEL_CLOCK, &burstRaster);

    Display* pDisplay = GetDisplay();
    Display::MaxPclkQueryInput maxPclkQuery = {pDisplay->Selected(),
                                               &burstRaster};
    vector<Display::MaxPclkQueryInput> maxPclkQueryVector(1, maxPclkQuery);
    CHECK_RC(pDisplay->SetRasterForMaxPclkPossible(&maxPclkQueryVector, 0));

    UINT64 maxPixelClkHz = static_cast<UINT64>(burstRaster.PixelClockHz);

    if (maxPixelClkHz < maxDispClkHz)
    {
        maxDispClkHz = maxPixelClkHz;
        lwrrPP.Clks[Gpu::ClkDisp].FreqHz = maxDispClkHz;
        CHECK_RC(pPerf->SetPerfPoint(lwrrPP));
    }

    return rc;
}

// not implemented in aml, will be updated later
RC LWSRSystemCheck::VerifyInterruptSteering()
{
    return OK;
}

// Get current backlight brightness level
RC LWSRSystemCheck::GetBrightness(UINT32 *level)
{
    RC rc;
    LW0073_CTRL_SPECIFIC_BACKLIGHT_BRIGHTNESS_PARAMS params = {0};
    params.subDeviceInstance = GetBoundGpuSubdevice()->GetSubdeviceInst();
    params.displayId = GetDisplay()->Selected();
    CHECK_RC(GetDisplay()->RmControl(
                LW0073_CTRL_CMD_SPECIFIC_GET_BACKLIGHT_BRIGHTNESS,
                &params,
                sizeof(params)));
    *level = params.brightness;
    return rc;
}

// Set backlight brightness level
RC LWSRSystemCheck::SetBrightness(UINT32 level)
{
    RC rc;
    LW0073_CTRL_SPECIFIC_BACKLIGHT_BRIGHTNESS_PARAMS params = {0};
    params.subDeviceInstance = GetBoundGpuSubdevice()->GetSubdeviceInst();
    params.displayId = GetDisplay()->Selected();
    params.brightness = level;
    CHECK_RC(GetDisplay()->RmControl(
                LW0073_CTRL_CMD_SPECIFIC_SET_BACKLIGHT_BRIGHTNESS,
                &params,
                sizeof(params)));
    return rc;
}

RC LWSRSystemCheck::CallACPI(UINT32 func, UINT32 subFunc, UINT32 *rtlwalue, UINT16 * size)
{
    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();

    return Platform::CallACPIGeneric(
                    pSubdev->GetGpuInst(),
                    func,
                    (void*)(size_t)subFunc,
                    rtlwalue,
                    size);
}

RC LWSRSystemCheck::VerifyJTSubfunctions()
{
    RC rc;
    UINT32 rtlwalue = 0;
    UINT16 size = sizeof(rtlwalue);

    CHECK_RC(CallACPI(
                MODS_DRV_CALL_ACPI_GENERIC_DSM_JT,
                0, // JT_FUNC_SUPPORT
                &rtlwalue, &size));
    /*
       Minimum required JT subfunctions as per JT spec
        Subfunction(Bit)                Required
        JT_FUNC_SUPPORT(0)              (Y)
        JT_FUNC_CAPS(1)                 (Y)
        JT_FUNC_POLICYSELECT(2)         (N) 0=not required
        JT_FUNC_POWERCONTROL(3)         (Y)
        JT_FUNC_PLATPOLICY(4)           (Y) 1=not required //check
        JT_FUNC_DISPLAYSTATUS(5)        (N) optional
        JT_FUNC_MDTL(6)                 (N) optional
    */
    UINT32 supportedMask =
            (1 << 0) | // JT_FUNC_SUPPORT
            (1 << JT_FUNC_CAPS) |
            (1 << JT_FUNC_POWERCONTROL) |
            (1 << JT_FUNC_PLATPOLICY);

    if ((rtlwalue & supportedMask) != supportedMask)
    {
        Printf(Tee::PriHigh, "Required JT subfunctions not present\n");
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

RC LWSRSystemCheck::ReadJTCapabilities()
{
    RC rc;
    UINT32 rtlwalue = 0;
    UINT16 size = sizeof(rtlwalue);

    CHECK_RC(CallACPI(
                MODS_DRV_CALL_ACPI_GENERIC_DSM_JT,
                JT_FUNC_CAPS,
                &rtlwalue, &size));

    m_JTCapabilities.JTEnabled = FLD_TEST_DRF(_JT, _FUNC_CAPS,_JT_ENABLED, _TRUE, rtlwalue);
    m_JTCapabilities.LWSREnabled = FLD_TEST_DRF(_JT, _FUNC_CAPS,_LWSR_ENABLED, _TRUE, rtlwalue);
    m_JTCapabilities.MSHybridEnabled = FLD_TEST_DRF(_JT, _FUNC_CAPS,_MSHYB_ENABLED, _TRUE, rtlwalue);
    m_JTCapabilities.RootPortControl = DRF_VAL(_JT, _FUNC_CAPS, _RPC, rtlwalue);
    return rc;
}

RC LWSRSystemCheck::VerifyJTCapabilities()
{
    StickyRC rc;
    CHECK_RC(ReadJTCapabilities());

    if (!m_JTCapabilities.JTEnabled)
    {
        Printf(Tee::PriHigh, "JT is not enabled\n");
        rc = RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    if (!m_JTCapabilities.LWSREnabled)
    {
        Printf(Tee::PriHigh, "LWSR is not enabled\n");
        rc = RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    if (m_JTCapabilities.MSHybridEnabled && !m_SkipMSHybridCheck)
    {
        Printf(Tee::PriHigh, "MSHybrid is not supported\n");
        rc = RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    if (m_JTCapabilities.RootPortControl != LW_JT_FUNC_CAPS_RPC_FINEGRAIN)
    {
        Printf(Tee::PriHigh,
                "Does not supports fine grain root port control PLON, PRPC\n");
        rc = RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    return rc;
}

// This function perform GCx sequence while putting panel into
// self refresh mode and verify NLT & GC6 stats
RC LWSRSystemCheck::CheckGCxSequence()
{
    RC rc;
    Tee::Priority pri = GetVerbosePrintPri();

    Printf(pri, "Enter SR Sparse mode\n");
    // Put panel in self refresh sparse mode
    CHECK_RC(m_TCONDevice.CacheFrame(nullptr));

    Printf(pri, "Disable Display output\n");
    // Need to update for native resolution of lwsr panel
    CHECK_RC(m_pSRDisplay->DisableDisplay());

    // Atleast 10 gc6 cycles are recommended to get better RM GC6 stats
    // in CheckGC6andNLTStats()
    const UINT32 gc6Loops = 10;
    for (UINT32 idx = 0; idx < gc6Loops; ++idx)
    {
        Printf(pri, "Loop %u: Enter GC6\n", idx);
        // Perform DBGS entry sequence and EBPG exit sequence
        CHECK_RC(m_pGCx->DoEnterGc6(m_WakeupEvent, 0)); // EGIS
        Printf(pri, "Loop %u: Exit GC6\n", idx);
        CHECK_RC(m_pGCx->DoExitGc6(0, false));                       // XGXS
    }

    Printf(pri, "SetMode for enable slave lock with FL\n");
    CHECK_RC(m_pSRDisplay->SetModeWithLockPin(true, m_frameLockPin));
    // Exit lwsr self refresh
    Printf(pri, "Exit SR Sparse mode\n");
    CHECK_RC(m_TCONDevice.ExitSelfRefresh());

    Printf(pri, "SetMode for disable slave lock with FL\n");
    CHECK_RC(m_pSRDisplay->SetModeWithLockPin(false, 0));

    VerbosePrintf("GC6 and NLT stats after CheckGCxSequence subtest:\n");
    CHECK_RC(CheckGC6andNLTStats());
    return rc;
}

// NBCI: NoteBook Common Interface
RC LWSRSystemCheck::VerifyNBCIInterface()
{
    StickyRC rc;
    UINT32 rtlwalue = 0;
    UINT16 size = sizeof(rtlwalue);
    CHECK_RC(CallACPI(
                MODS_DRV_CALL_ACPI_GENERIC_DSM_NBCI,
                LW_NBCI_FUNC_SUPPORT,
                &rtlwalue, &size));
    Printf(Tee::PriLow, "NBCI caps 0x%x\n", rtlwalue);

    // These are the only 2 nbci subfunctions that are implemented
    // in aml as of now
    UINT32 supportedMask =
            (1 << LW_NBCI_FUNC_SUPPORT) |
            (1 << LW_NBCI_FUNC_GETBACKLIGHT);

    if ((rtlwalue & supportedMask) != supportedMask)
    {
        Printf(Tee::PriError, "Required NBCI subfunctions are not present\n");
        return RC::SOFTWARE_ERROR;
    }

    rtlwalue = 0;
    size = sizeof(rtlwalue);
    rc = CallACPI(
            MODS_DRV_CALL_ACPI_GENERIC_DSM_NBCI,
            LW_NBCI_FUNC_GETBACKLIGHT,
            &rtlwalue, &size);
    // Mostly this will fail as it requires package support.
    // Skipping this error until package support is added in mods.
    if (rc != OK)
    {
        VerbosePrintf("GetBacklight nbci acpi call is not supported, Skipping.\n");
        rc.Clear();
    }
    return rc;
}

// Verify the backlight brightness of the panel
RC LWSRSystemCheck::VerifyBrightness()
{
    RC rc;
    Tee::Priority pri = GetVerbosePrintPri();
    FLOAT64 luminanceSyncFrequency = 0.0; // It will be determined automatically.

    UINT32 origLevel = 0;
    CHECK_RC(GetBrightness(&origLevel));
    Utility::CleanupOnReturnArg<LWSRSystemCheck, RC, UINT32>
        restoreBrightness(this, &LWSRSystemCheck::SetBrightness, origLevel);

    Printf(pri, "Enter SR Sparse mode\n");
    // Put panel in self refresh sparse mode
    CHECK_RC(m_TCONDevice.CacheFrame(nullptr));
    Utility::CleanupOnReturn<TCONDevice, RC>
        exitSR(&m_TCONDevice, &TCONDevice::ExitSelfRefresh);
    Printf(pri, "Disable Display output\n");
    // Disable display output since panel is in self refresh mode
    CHECK_RC(m_pSRDisplay->DisableDisplay());

    Printf(pri, "Set Brightness 100\n");
    CHECK_RC(SetBrightness(100));
    FLOAT64 luminanceCdM2at100 = 0;
    CHECK_RC(m_CRInstrument.MeasureLuminance(m_CRInstrumentIdx,
        &luminanceSyncFrequency, &luminanceCdM2at100));

    Printf(pri, "Set Brightness 10\n");
    CHECK_RC(SetBrightness(10));
    FLOAT64 luminanceCdM2at10 = 0;
    CHECK_RC(m_CRInstrument.MeasureLuminance(m_CRInstrumentIdx,
        &luminanceSyncFrequency, &luminanceCdM2at10));

    if ((m_DimBrightBacklightTolerance * luminanceCdM2at100) < luminanceCdM2at10)
    {
        Printf(Tee::PriNormal,
            "Error: Measured panel luminance at 100%% backlight = %5.1f cd/m2 "
            "is not sufficiently higher then at 10%% backlight = %5.1f cd/m2\n",
            luminanceCdM2at100, luminanceCdM2at10);
        return RC::UNEXPECTED_RESULT;
    }

    exitSR.Cancel();
    CHECK_RC(m_pSRDisplay->SetControlSlaveLockPin(true, m_frameLockPin));
    Printf(pri, "SetMode for enable slave lock with FL\n");
    CHECK_RC(m_pSRDisplay->SetMode());
    Printf(pri, "Exit SR Sparse mode\n");
    // Exit lwsr self refresh
    CHECK_RC(m_TCONDevice.ExitSelfRefresh());

    CHECK_RC(m_pSRDisplay->SetControlSlaveLockPin(false, 0));
    Printf(pri, "SetMode for disable slave lock with FL\n");
    CHECK_RC(m_pSRDisplay->SetMode());
    return rc;
}

RC LWSRSystemCheck::VerifySmiTrap()
{
    RC rc;
    Display* pDisplay = GetDisplay();
    Tee::Priority pri = GetVerbosePrintPri();
    Printf(pri, "Begin SmiTrap test\n");

    // TODO: Uncomment when PMU Modeset is implemented
    //LW208F_CTRL_BIF_PBI_WRITE_COMMAND_PARAMS writeBifPbiParam = {};
    //LwRm::Handle subDevDiag = GetBoundGpuSubdevice()->GetSubdeviceDiag();
    //LwRmPtr  pLwRm;

    CHECK_RC(Platform::PioWrite08(ENABLE_SMI_REGISTER, ENABLE_SMI_VALUE));

    UINT32 smiStatus = 0;
    CHECK_RC(Platform::PioRead32(STATUS_REGISTER, &smiStatus));
    if (smiStatus != SMI_READY)
    {
        Printf(Tee::PriHigh, "Error: Port 0x%x = 0x%x. Should be 0x%x\n", STATUS_REGISTER, smiStatus, SMI_READY);
        return RC::UNEXPECTED_RESULT;
    }

    Printf(pri, "Enter SR Sparse mode\n");
    // Enable Self-Refresh Sparse Mode
    // Enables the SR mode at the same time
    CHECK_RC(m_TCONDevice.CacheFrame(nullptr));
    Utility::CleanupOnReturn<TCONDevice, RC>
            exitSR(&m_TCONDevice, &TCONDevice::ExitSelfRefresh);
    Printf(pri, "Disable Display output\n");
    // Disable display output since panel is in self refresh mode
    UINT32 wd, ht, dt, rf;
    CHECK_RC(pDisplay->GetMode(pDisplay->Selected(), &wd, &ht, &dt, &rf));
    CHECK_RC(pDisplay->Disable());

    Printf(pri, "Enter GC6\n");
    CHECK_RC(m_pGCx->DoEnterGc6(m_WakeupEvent, 0));

    // Trigger the SMI Trap
    UINT08 data = 0;
    Printf(pri, "Read VGA_REGISTER\n");
    CHECK_RC(Platform::PioRead08(VGA_REGISTER, &data));
    if (data != 0)
    {
        Printf(Tee::PriError, "Exit GC6 by UNEXPECTED_RESULT, VGA_REG=0x%x\n", data);
        CHECK_RC(m_pGCx->DoExitGc6(0, false));
        return RC::UNEXPECTED_RESULT;
    }

    // TODO: Uncomment when PMU Modeset is implemented
    //Printf(pri, "Trigger PMU Modeset\n");
    //writeBifPbiParam.cmdFuncId = LW_PBI_COMMAND_FUNC_ID_TRIGGER_PMU_MODESET;
    //CHECK_RC(LwRmControl(pLwRm->GetClientHandle(), subDevDiag,
    //                     LW208F_CTRL_CMD_BIF_PBI_WRITE_COMMAND,
    //                     (void*)&writeBifPbiParam,
    //                     sizeof (LW208F_CTRL_BIF_PBI_WRITE_COMMAND_PARAMS)));
    //
    //if(writeBifPbiParam.status != LW_PBI_COMMAND_STATUS_SUCCESS)
    //{
    //    Printf(Tee::PriHigh,"ERROR: PMU Modeset failed. status = %d \n ", writeBifPbiParam.status);
    //    return RC::UNEXPECTED_RESULT;
    //}

    Printf(pri, "Exit GC6\n");
    CHECK_RC(m_pGCx->DoExitGc6(0, false));

    exitSR.Cancel();
    CHECK_RC(pDisplay->SetHeadSetControlSlaveLock(pDisplay->Selected(),
                                                  true,
                                                  m_frameLockPin));
    Printf(pri, "SetMode for enable slave lock with FL\n");
    CHECK_RC(pDisplay->SetMode(wd, ht, dt, rf));
    Printf(pri, "Exit SR Sparse mode\n");
    // Exit lwsr self refresh
    CHECK_RC(m_TCONDevice.ExitSelfRefresh());

    CHECK_RC(pDisplay->SetHeadSetControlSlaveLock(pDisplay->Selected(),
                                                  false,
                                                  0));
    Printf(pri, "SetMode for disable slave lock with FL\n");
    CHECK_RC(pDisplay->SetMode(wd, ht, dt, rf));

    return rc;
}

RC LWSRSystemCheck::VerifyFramelockLatency()
{
    StickyRC rc;
    Tee::Priority pri = GetVerbosePrintPri();
    Display* pDisplay = GetDisplay();

    Printf(pri, "Starting Framelock latency test\n");

    if (m_FramelockLatencyIterations < 10)
    {
        Printf(Tee::PriNormal, "Error: Framelock subtest requires at least 10"
            " iterations, but it is configured for %d\n", m_FramelockLatencyIterations);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    CHECK_RC(m_GpuRectFill.Fill(0,
                                0,
                                m_DisplaySurface.GetWidth(),
                                m_DisplaySurface.GetHeight(),
                                0x808080,
                                true));

    CHECK_RC(m_pSRDisplay->SetModeAndImage(&m_NativeRaster, LWSRDisplay::NativeSetMode));

    Printf(pri, "Enter SR sparse mode\n");
    // Enables the SR mode at the same time
    CHECK_RC(m_TCONDevice.CacheFrame(nullptr));
    Utility::CleanupOnReturn<TCONDevice, RC>
        exitSR(&m_TCONDevice, &TCONDevice::ExitSelfRefresh);
    Printf(pri, "Disable Display output\n");
    // Disable display output since panel is in self refresh mode
    CHECK_RC(m_pSRDisplay->DisableDisplay());

    FLOAT64 panelRefreshRateInSR = (FLOAT64)(m_lwtsr.etc.rr);

    //
    // We should set burst pclk by obtained from TCON directly since panel is
    // already in the self-refresh mode.
    //
    RasterSettings burstRaster;
    m_TCONDevice.CallwlateBurstRaster(&m_NativeRaster,
                                      m_TCONDevice.GetMaxPixelClockHz(),
                                      &burstRaster);

    bool ignoreDPLibIMP = pDisplay->GetIgnoreDPLibIMP();
    pDisplay->SetIgnoreDPLibIMP(true);
    Utility::CleanupOnReturnArg<Display, void, bool>
        resetDPLibIMPMode(pDisplay, &Display::SetIgnoreDPLibIMP, ignoreDPLibIMP);

    // It is required to skip DP library IMP call here(in windows we don't check it)
    // otherwise it fails on most of the FHD panels. See bug 200258833
    Printf(pri, "Set Raster for Max Pclk: SetModeAndImage(&burstRaster)\n");
    CHECK_RC(m_pSRDisplay->SetModeAndImage(&burstRaster, LWSRDisplay::BurstSetMode));

    resetDPLibIMPMode.Cancel();
    pDisplay->SetIgnoreDPLibIMP(ignoreDPLibIMP);

    auto resetMode = true;
    DEFER
    {
        if (resetMode)
        {
            m_pSRDisplay->SetModeAndImage(&m_NativeRaster, LWSRDisplay::NativeSetMode);
        }
    };

    Printf(pri, "Write Burst Timing Regs\n");
    CHECK_RC(m_TCONDevice.WriteBurstTimingRegisters(&burstRaster));

    Printf(pri, "Enable Burst Mode\n");
    CHECK_RC(m_TCONDevice.EnableBufferedBurstRefreshMode());
    Utility::CleanupOnReturn<TCONDevice, RC>
        disableBurst(&m_TCONDevice, &TCONDevice::DisableBufferedBurstRefreshMode);
    Printf(pri, "Set Resync FL and Resync delay = 0\n");
    CHECK_RC(m_TCONDevice.SetupResync(0));

    Printf(pri, "Burst single frame update\n");
    CHECK_RC(m_TCONDevice.CacheBurstFrame(nullptr));

    Printf(pri, "Enable VRR\n");
    bool enableCrashSync = false;
    CHECK_RC(m_TCONDevice.EnableVrr(enableCrashSync));
    Utility::CleanupOnReturn<TCONDevice, RC>
        disableVrr(&m_TCONDevice, &TCONDevice::DisableVrr);

    Printf(pri, "Starting framelock latency loops without Crash Sync. Max frame latency = %.1f frame%s\n",
                m_FramelockFrameLatency, (m_FramelockFrameLatency == 1.0)? "" : "s");
    CHECK_RC(FramelockLatencyLoops(m_FramelockFrameLatency, panelRefreshRateInSR, false));

    Printf(pri, "Disable VRR\n");
    disableVrr.Cancel();
    CHECK_RC(m_TCONDevice.DisableVrr());

    Printf(pri, "Enable VRR\n");
    enableCrashSync = true;
    FLOAT64 crashSyncFrameLatency = m_FramelockFrameLatency / 2.0;
    CHECK_RC(m_TCONDevice.EnableVrr(enableCrashSync));
    disableVrr.Activate();

    Printf(pri, "Starting framelock latency loops with Crash Sync. Max frame latency = %.1f frame%s\n",
            crashSyncFrameLatency, (crashSyncFrameLatency == 1.0)? "" : "s");
    CHECK_RC(FramelockLatencyLoops(crashSyncFrameLatency, panelRefreshRateInSR, true));

    Printf(pri, "Disable VRR\n");
    disableVrr.Cancel();
    CHECK_RC(m_TCONDevice.DisableVrr());

    Printf(pri, "Disable Burst mode\n");
    disableBurst.Cancel();
    CHECK_RC(m_TCONDevice.DisableBufferedBurstRefreshMode());

    Printf(pri, "SetModeAndImage(&m_NativeRaster)\n");
    resetMode = false;
    CHECK_RC(m_pSRDisplay->SetModeAndImage(&m_NativeRaster, LWSRDisplay::NativeSetMode));

    exitSR.Cancel();

    Printf(pri, "SetMode for enable slave lock with FL\n");
    CHECK_RC(m_pSRDisplay->SetModeWithLockPin(true, m_frameLockPin));
    // Exit lwsr self refresh
    Printf(pri, "Exit SR Sparse mode\n");
    CHECK_RC(m_TCONDevice.ExitSelfRefresh());

    Printf(pri, "SetMode for disable slave lock with FL\n");
    CHECK_RC(m_pSRDisplay->SetModeWithLockPin(false, 0));

    return rc;
}

RC LWSRSystemCheck::FramelockLatencyLoops
(
    FLOAT64 numFramesAllowed,
    FLOAT64 panelRefreshRate,
    bool bCrashSyncEnabled
)
{
    RC rc;
    Display* pDisplay = GetDisplay();
    const UINT32 panelID = m_pSRDisplay->GetPanelID();
    Tee::Priority pri = GetVerbosePrintPri();

    Printf(pri, "Enable Stall lock with FL\n");
    // The stall takes effect after a "flip" - SetImage
    CHECK_RC(m_pSRDisplay->SetStallLock(true, m_frameLockPin));

    Printf(pri, "SetImage\n");
    CHECK_RC(m_pSRDisplay->SetImage());

    LW0073_CTRL_SYSTEM_FORCE_VRR_RELEASE_PARAMS vrrRelease = {0};
    vrrRelease.subDeviceInstance = GetBoundGpuSubdevice()->GetSubdeviceInst();
    vrrRelease.waitForFlipMs = 0; // ASAP

    LW0073_CTRL_CMD_SYSTEM_SCANOUT_LOGGING_PARAMS scanoutLogging = {0};
    scanoutLogging.subDeviceInstance = GetBoundGpuSubdevice()->GetSubdeviceInst();
    scanoutLogging.displayId =  panelID;
    scanoutLogging.cmd = LW0073_CTRL_SYSTEM_SCANOUT_LOGGING_ENABLE;
    scanoutLogging.size = m_FramelockLatencyIterations + 1; // The first entry captures logging
                                                            // start and not the actual event
    scanoutLogging.verticalScanline = 1;
    scanoutLogging.bFreeBuffer = LW_FALSE;
    scanoutLogging.bUseRasterLineIntr = LW_FALSE;
    scanoutLogging.scanoutLogFlag = 0;
    scanoutLogging.loggingAddr = 0;
    CHECK_RC_MSG(pDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_SCANOUT_LOGGING,
        &scanoutLogging, sizeof(scanoutLogging)), "Error starting the logger\n");

    Utility::CleanupOnReturn<LWSRSystemCheck, RC>
        freeLoggerBuffer(this, &LWSRSystemCheck::FreeLoggerBuffer);

    // Bug 1653639:
    // Give some arbitrary amount of time for the logger to actually start
    Tasker::Sleep(300.0);

    vector<UINT64> releaseStartGPUTimes;
    releaseStartGPUTimes.resize(m_FramelockLatencyIterations);

    for (UINT32 iterationNum = 0;
         iterationNum < m_FramelockLatencyIterations;
         iterationNum++)
    {
        Printf(pri, "Framelock latency iteration idx = %d\n", iterationNum);

        CHECK_RC(m_GpuRectFill.Fill(0,
                                    0,
                                    m_DisplaySurface.GetWidth(),
                                    m_DisplaySurface.GetHeight(),
                                    (iterationNum & 1) ? 0x00ff00 : 0xff0000,
                                    true));

        CHECK_RC(m_TCONDevice.WaitForNewSRFrame());

        LW2080_CTRL_TIMER_GET_TIME_PARAMS getTime = {};
        CHECK_RC_MSG(LwRmPtr()->ControlBySubdevice(pDisplay->GetOwningDisplaySubdevice(),
                                                   LW2080_CTRL_CMD_TIMER_GET_TIME,
                                                   &getTime, sizeof(getTime)),
                                                   "Error getting the GPU time\n");
        releaseStartGPUTimes[iterationNum] = getTime.time_nsec;

        // This RM Ctrl will write DPCD registers 0x340, 0x366
        CHECK_RC_MSG(pDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_FORCE_VRR_RELEASE,
            &vrrRelease, sizeof(vrrRelease)), "Unable to perform VRR release\n");

        Tasker::Sleep(200.0);
    }

    scanoutLogging.bFreeBuffer = LW_FALSE;
    scanoutLogging.cmd = LW0073_CTRL_SYSTEM_SCANOUT_LOGGING_DISABLE;
    CHECK_RC_MSG(pDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_SCANOUT_LOGGING,
        &scanoutLogging, sizeof(scanoutLogging)), "Error stopping the logger\n");

    // The same as bug 1653639:
    // The ctrl call with _DISABLE above is not waiting for the DPU cmd
    // to be exelwted.
    Tasker::Sleep(200.0);

    vector<SCANOUTLOGGING> scanoutLog;
    scanoutLog.resize(m_FramelockLatencyIterations + 1);

    scanoutLogging.loggingAddr = LW_PTR_TO_LwP64(&scanoutLog[0]);
    scanoutLogging.cmd = LW0073_CTRL_SYSTEM_SCANOUT_LOGGING_FETCH;
    CHECK_RC_MSG(pDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_SCANOUT_LOGGING,
        &scanoutLogging, sizeof(scanoutLogging)), "Error retrieving logger data\n");

    bool reportError = false;

    FLOAT64 maxAllowedUpdateTimeMs = 0;
    if (bCrashSyncEnabled == false)
    {
        //
        // Without Crash Sync request, GPU is testing the normal burst single
        // frame update.  Since TCON may receive SR-Update request at the
        // beginning of a frame, then TCON will toggle framelock signal at the
        // beginning of next frame.  So, the max allowed update time should be
        // (2 * panel self refresh rate).
        //
        maxAllowedUpdateTimeMs = numFramesAllowed * 1000.0 / panelRefreshRate;
    }
    else
    {
        //
        // With Crash Sync request, TCON is reset during Vertical Front Porch
        // period and cache frame.  But if TCON receive Crash Sync request at
        // VSync, VBP, Vactive of a frame, then TCON will be reset and toggle
        // framelock signal at the end of Vactive.  So, the max allowed update
        // time should be ((VSync+VBP+Vactive)/Vtotal)*(1 * panel self refresh
        // rate)+(1 * panel self refresh rate)
        //
        maxAllowedUpdateTimeMs = ((FLOAT64)(m_lwtsr.VTotal - m_lwtsr.VFrontPorch) /
            (FLOAT64)m_lwtsr.VTotal) * (1000.0 / panelRefreshRate) +
            (numFramesAllowed * 1000.0 / panelRefreshRate);
    }
    for (UINT32 idx = 0; idx < m_FramelockLatencyIterations; idx++)
    {
        // The first entry in the scanout log is just when logging started
        if (scanoutLog[idx+1].timeStamp == 0)
        {
            Printf(Tee::PriHigh, "Error: Iteration %d, invalid scanout timestamp = 0\n", idx);
            reportError = true;
            break;
        }
        if (releaseStartGPUTimes[idx] > scanoutLog[idx+1].timeStamp)
        {
            Printf(Tee::PriHigh, "Error: Iteration %d, invalid scanout timestamp: can't be before release\n", idx);
            reportError = true;
            break;
        }

        const UINT64 diff = scanoutLog[idx+1].timeStamp - releaseStartGPUTimes[idx];
        const FLOAT64 diffMs = diff / 1000000.0;
        if (diffMs > maxAllowedUpdateTimeMs)
        {
            Printf(Tee::PriHigh, "Error: Iteration %d, latency = %.2fms > max latency = %.2fms\n",
                   idx, diffMs, maxAllowedUpdateTimeMs);
            reportError = true;
        }
        else
            Printf(pri, "Iteration %d, latency = %.2fms\n", idx, diffMs);
    }

    CHECK_RC(m_pSRDisplay->SetStallLock(false, 0));
    CHECK_RC(m_pSRDisplay->SetImage());

    Printf(pri, "Burst single frame update after disabled Stall lock\n");
    CHECK_RC(m_TCONDevice.CacheBurstFrame(nullptr));

    return reportError ? RC::UNEXPECTED_RESULT : OK;
}

RC LWSRSystemCheck::VerifyJTGCXBacklight()
{
    RC rc;
    Tee::Priority pri = GetVerbosePrintPri();

    Printf(pri, "Begin JTGCX backlight test\n");

    CHECK_RC(m_pSRDisplay->DisableDisplay());

    FLOAT64 darkBaseline = 0.0;
    CHECK_RC(m_CRInstrument.MeasureLuminance(m_CRInstrumentIdx, nullptr, &darkBaseline));
    Printf(pri, "Measuring GCX without self-refresh\n");
    CHECK_RC(MeasureGCXBacklight(darkBaseline));

    CHECK_RC(m_pSRDisplay->SetMode());

    CHECK_RC(m_GpuRectFill.Fill(0,
                                0,
                                m_DisplaySurface.GetWidth(),
                                m_DisplaySurface.GetHeight(),
                                0x808080,
                                true));

    CHECK_RC(m_pSRDisplay->SetModeAndImage(&m_NativeRaster, LWSRDisplay::NativeSetMode));

    FLOAT64 illuminatedBaseline = 0.0;
    CHECK_RC(m_CRInstrument.MeasureLuminance(m_CRInstrumentIdx, nullptr, &illuminatedBaseline));
    // Now get luminances while in Self-refresh
    Printf(pri, "Measuring GCX with self-refresh\n");

    Printf(pri, "Enter SR Sparse\n");
    // Enable Self-Refresh Sparse Mode
    // Enables the SR mode at the same time
    CHECK_RC(m_TCONDevice.CacheFrame(nullptr));
    Utility::CleanupOnReturn<TCONDevice, RC>
        exitSR(&m_TCONDevice, &TCONDevice::ExitSelfRefresh);
    Printf(pri, "Disable Display output\n");
    // Disable display output since panel is in self refresh mode
    CHECK_RC(m_pSRDisplay->DisableDisplay());

    Printf(pri, "MeasureGCXBacklight()\n");
    CHECK_RC(MeasureGCXBacklight(illuminatedBaseline));

    FLOAT64 luminanceDifference = illuminatedBaseline - darkBaseline;
    if (luminanceDifference < m_JTGCXBacklightTolerance)
    {
        Printf(Tee::PriHigh,
            "Error: Measured panel luminance while illuminated = %.1f cd/m2 is not at least"
            " JTGCXBacklightTolerance = %.1f cd/m2 greater than while dark = %.1f cd/m2\n",
            illuminatedBaseline, m_JTGCXBacklightTolerance, darkBaseline);
        return RC::UNEXPECTED_RESULT;
    }

    exitSR.Cancel();
    Printf(pri, "SetMode for enable slave lock with FL\n");
    CHECK_RC(m_pSRDisplay->SetModeWithLockPin(true, m_frameLockPin));
    Printf(pri, "Exit SR Sparse mode\n");
    // Exit lwsr self refresh
    CHECK_RC(m_TCONDevice.ExitSelfRefresh());

    Printf(pri, "SetMode for disable slave lock with FL\n");
    CHECK_RC(m_pSRDisplay->SetModeWithLockPin(false, 0));
    return rc;
}

RC LWSRSystemCheck::MeasureGCXBacklight(FLOAT64 baseline)
{
    RC rc;
    Tee::Priority pri = GetVerbosePrintPri();

    Printf(pri, "Baseline Luminance = %.1f cd/m2, Max expected variance = %.1f cd/m2\n",
                baseline, m_JTGCXBacklightVariance);

    Printf(pri, "Enter GC6\n");
    CHECK_RC(m_pGCx->DoEnterGc6(m_WakeupEvent, 0));

    FLOAT64 gc6Lum = 0.0;
    RC lumRc = m_CRInstrument.MeasureLuminance(m_CRInstrumentIdx, nullptr, &gc6Lum);
    Printf(pri, "GC6 Luminance = %.1f cd/m2\n", gc6Lum);

    Printf(pri, "Exit GC6\n");
    CHECK_RC(m_pGCx->DoExitGc6(0, false));
    CHECK_RC(lumRc);

    FLOAT64 luminanceAfterGC6Exit = 0.0;
    CHECK_RC(m_CRInstrument.MeasureLuminance(m_CRInstrumentIdx, nullptr, &luminanceAfterGC6Exit));

    FLOAT64 luminanceGC6Variance = fabs(gc6Lum - baseline);
    if (luminanceGC6Variance > m_JTGCXBacklightVariance)
    {
        Printf(Tee::PriHigh,
            "Error: Measured panel luminance in GC6 = %.1f cd/m2, baseline = %.1f cd/m2, variance = %.1f cd/m2"
            " exceeds the m_JTGCXBacklightVariance = %.1f cd/m2\n",
            gc6Lum, baseline, luminanceGC6Variance, m_JTGCXBacklightVariance);
        return RC::UNEXPECTED_RESULT;
    }
    FLOAT64 luminanceGC6ExitVariance = fabs(luminanceAfterGC6Exit - baseline);
    if (luminanceGC6ExitVariance > m_JTGCXBacklightVariance)
    {
        Printf(Tee::PriHigh,
            "Error: Measured panel luminance after exiting GC6 = %.1f cd/m2, baseline = %.1f cd/m2, variance = %.1f cd/m2"
            " exceeds the m_JTGCXBacklightVariance = %.1f cd/m2\n",
            luminanceAfterGC6Exit, baseline, luminanceGC6ExitVariance, m_JTGCXBacklightVariance);
        return RC::UNEXPECTED_RESULT;
    }

    if (!GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_GC5))
    {
        Printf(Tee::PriLow, "Skipping MeasureGC5Backlight because GC5 is not supported\n");
        return rc;
    }

    Printf(pri, "Enter GC5\n");
    CHECK_RC(m_pGCx->DoEnterGc5(8, GC_CMN_WAKEUP_EVENT_gc5pex, 0));

    FLOAT64 gc5Lum = 0.0;
    lumRc = m_CRInstrument.MeasureLuminance(m_CRInstrumentIdx, nullptr, &gc5Lum);
    Printf(pri, "GC5 Luminance = %.1f cd/m2\n", gc5Lum);

    Printf(pri, "Exit GC5\n");
    CHECK_RC(m_pGCx->DoExitGc5(0, false));
    CHECK_RC(lumRc);

    FLOAT64 luminanceAfterGC5Exit = 0.0;
    CHECK_RC(m_CRInstrument.MeasureLuminance(m_CRInstrumentIdx, nullptr, &luminanceAfterGC5Exit));

    FLOAT64 luminanceGC5Variance = fabs(gc5Lum - baseline);
    if (luminanceGC5Variance > m_JTGCXBacklightVariance)
    {
        Printf(Tee::PriHigh,
            "Error: Measured panel luminance in GC5 = %.1f cd/m2, baseline = %.1f cd/m2, variance = %.1f cd/m2"
            " exceeds the m_JTGCXBacklightVariance = %.1f cd/m2\n",
            gc5Lum, baseline, luminanceGC5Variance, m_JTGCXBacklightVariance);
        return RC::UNEXPECTED_RESULT;
    }
    FLOAT64 luminanceGC5ExitVariance = fabs(luminanceAfterGC5Exit - baseline);
    if (luminanceGC5ExitVariance > m_JTGCXBacklightVariance)
    {
        Printf(Tee::PriHigh,
            "Error: Measured panel luminance after exiting GC5 = %.1f cd/m2, baseline = %.1f cd/m2, variance = %.1f cd/m2"
            " exceeds the m_JTGCXBacklightVariance = %.1f cd/m2\n",
            luminanceAfterGC5Exit, baseline, luminanceGC5ExitVariance, m_JTGCXBacklightVariance);
        return RC::UNEXPECTED_RESULT;
    }

    return rc;
}

RC LWSRSystemCheck::VerifyJTGCXLatency()
{
    RC rc;
    Tee::Priority pri = GetVerbosePrintPri();
    Printf(pri, "Begin JTGCX latency test\n");

    Printf(pri, "Testing JT/GC6\n");
    UINT64 srEnterStartTimeUs = 0;
    UINT64 srEnterEndTimeUs = 0;
    UINT64 srExitStartTimeUs = 0;
    UINT64 srExitEndTimeUs = 0;
    {
        Printf(pri, "Enter SR sparse mode\n");
        srEnterStartTimeUs = Xp::GetWallTimeUS();
        // Enable Self-Refresh Sparse Mode
        // Enables the SR mode at the same time
        CHECK_RC(m_TCONDevice.CacheFrame(nullptr));
        srEnterEndTimeUs = Xp::GetWallTimeUS();
        Utility::CleanupOnReturn<TCONDevice, RC>
            exitSR(&m_TCONDevice, &TCONDevice::ExitSelfRefresh);
        Printf(pri, "Disable Display output\n");
        // Disable display output since panel is in self refresh mode
        CHECK_RC(m_pSRDisplay->DisableDisplay());

        Printf(pri, "Enter GC6\n");
        CHECK_RC(m_pGCx->DoEnterGc6(m_WakeupEvent, 0));
        Printf(pri, "Exit GC6\n");
        CHECK_RC(m_pGCx->DoExitGc6(0, false));

        exitSR.Cancel();
        Printf(pri, "SetMode for enable slave lock with FL\n");
        CHECK_RC(m_pSRDisplay->SetModeWithLockPin(true, m_frameLockPin));
        Printf(pri, "Exit SR sparse mode\n");
        srExitStartTimeUs = Xp::GetWallTimeUS();
        // Exit lwsr self refresh
        CHECK_RC(m_TCONDevice.ExitSelfRefresh());
        srExitEndTimeUs = Xp::GetWallTimeUS();
    }

    // Clean up frame lock setting
    Printf(pri, "SetMode for disable slave lock with FL\n");
    CHECK_RC(m_pSRDisplay->SetModeWithLockPin(false, 0));

    //
    // Callwlate max latency of SR-Enter
    // TCON may receviced a SR-Entry request at the beginning of a frame,
    // then TCON will cache next frame and entered SR mode at the end of next
    // frame.  So, the max latency of SR-Enter should be
    // (2 * panel refresh rate before entering SR).
    //
    FLOAT64 SREnterLatencyMS = (2 * 1000.0 / (m_NativeRaster.CallwlateRefreshRate()));
    SREnterLatencyMS = SREnterLatencyMS + m_SelfRefreshGetTimeDelayMS;
    //
    // Callwlate max latency of SR-Exit
    // TCON may receviced a SR-Exit request at the beginning of a SR frame
    // and because GPU HW limitation so we set resync delay = 1 frame,
    // then TCON will exit SR mode at the end of next frame.
    // So, the max latency of SR-Exit should be (2 * panel self refresh rate).
    //
    FLOAT64 SRExitLatencyMS = (2 * 1000.0 / (m_lwtsr.etc.rr));
    SRExitLatencyMS = SRExitLatencyMS + m_SelfRefreshGetTimeDelayMS;

    FLOAT64 enterSR = (FLOAT64)(srEnterEndTimeUs - srEnterStartTimeUs) / 1000.0;
    FLOAT64 exitSR  = (FLOAT64)(srExitEndTimeUs - srExitStartTimeUs) / 1000.0;
    UINT32 GC6entryLatency = m_pGCx->GetEnterTimeMs();
    UINT32 GC6exitLatency = m_pGCx->GetExitTimeMs();
    Printf(pri, "SR entry latency: %.2fms; SREnterLatencyMS: %.2fms\n", enterSR, SREnterLatencyMS);
    Printf(pri, "SR exit latency: %.2fms; SRExitLatencyMS: %.2fms\n", exitSR, SRExitLatencyMS);
    Printf(pri, "GC6 entry latency: %ums\n", GC6entryLatency);
    Printf(pri, "GC6 exit latency: %ums\n", GC6exitLatency);

    bool bIsExceeded = false;
    // As per HW team, we shouldn't fail if SR enter/exit latency get exceeded.
    // Since we get different latencies in almost all different panel, it fails most
    // of the time, skip it as test 209 is more of system level test.
    if (enterSR > SREnterLatencyMS)
    {
        Printf(Tee::PriHigh, "GC6 SR enter latency %.2fms > tolerance %.2fms\n", enterSR, SREnterLatencyMS);
    }
    if (exitSR > SRExitLatencyMS)
    {
        Printf(Tee::PriHigh, "GC6 SR exit latency %.2fms > tolerance %.2fms\n", exitSR, SRExitLatencyMS);
    }
    if (GC6entryLatency > m_JTGC6EnterLatencyMS)
    {
        Printf(Tee::PriHigh, "GC6 enter latency %ums > tolerance %ums\n", GC6entryLatency, m_JTGC6EnterLatencyMS);
        bIsExceeded = true;
    }
    if (GC6exitLatency > m_JTGC6ExitLatencyMS)
    {
        Printf(Tee::PriHigh, "GC6 exit latency %ums > tolerance %ums\n", GC6exitLatency, m_JTGC6ExitLatencyMS);
        bIsExceeded = true;
    }
    if (bIsExceeded == true)
    {
        return RC::UNEXPECTED_RESULT;
    }

    if (!GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_GC5))
    {
        Printf(Tee::PriLow, "Skipping JTGC5Latency because GC5 is not supported\n");
        return rc;
    }

    Printf(pri, "Testing JT/GC5\n");
    srEnterStartTimeUs = 0;
    srEnterEndTimeUs = 0;
    srExitStartTimeUs = 0;
    srExitEndTimeUs = 0;
    {
        Printf(pri, "debug info: Enter SR sparse mode\n");
        srEnterStartTimeUs = Xp::GetWallTimeUS();
        // Enable Self-Refresh Sparse Mode
        // Enables the SR mode at the same time
        CHECK_RC(m_TCONDevice.CacheFrame(nullptr));
        srEnterEndTimeUs = Xp::GetWallTimeUS();
        Utility::CleanupOnReturn<TCONDevice, RC>
            exitSR(&m_TCONDevice, &TCONDevice::ExitSelfRefresh);
        Printf(pri, "Disable Display output\n");
        // Disable display output since panel is in self refresh mode
        CHECK_RC(m_pSRDisplay->DisableDisplay());

        Printf(pri, "Enter GC5\n");
        CHECK_RC(m_pGCx->DoEnterGc5(8, GC_CMN_WAKEUP_EVENT_gc5pex, 0));
        Printf(pri, "Exit GC5\n");
        CHECK_RC(m_pGCx->DoExitGc5(0, false));

        exitSR.Cancel();
        Printf(pri, "SetMode for enable slave lock with FL\n");
        CHECK_RC(m_pSRDisplay->SetModeWithLockPin(true, m_frameLockPin));
        Printf(pri, "Exit SR sparse mode\n");
        srExitStartTimeUs = Xp::GetWallTimeUS();
        // Exit lwsr self refresh
        CHECK_RC(m_TCONDevice.ExitSelfRefresh());
        srExitEndTimeUs = Xp::GetWallTimeUS();
    }

    // Clean up frame lock setting
    Printf(pri, "SetMode for disable slave lock with FL\n");
    CHECK_RC(m_pSRDisplay->SetModeWithLockPin(false, 0));

    enterSR = (FLOAT64)(srEnterEndTimeUs - srEnterStartTimeUs) / 1000.0;
    exitSR = (FLOAT64)(srExitEndTimeUs - srExitStartTimeUs) / 1000.0;
    UINT32 GC5entryLatency = m_pGCx->GetEnterTimeMs();
    UINT32 GC5exitLatency = m_pGCx->GetExitTimeMs();

    bIsExceeded = false;
    if (enterSR > SREnterLatencyMS)
    {
        Printf(Tee::PriHigh, "GC5 SR enter latency %.2fms > tolerance %.2fms\n", enterSR, SREnterLatencyMS);
        bIsExceeded = true;
    }
    if (exitSR > SRExitLatencyMS)
    {
        Printf(Tee::PriHigh, "GC5 SR exit latency %.2fms > tolerance %.2fms\n", exitSR, SRExitLatencyMS);
        bIsExceeded = true;
    }
    if (GC5entryLatency > m_JTGC5EnterLatencyMS)
    {
        Printf(Tee::PriHigh, "JT/GC5 enter latency %ums > tolerance %ums\n", GC5entryLatency, m_JTGC5EnterLatencyMS);
        bIsExceeded = true;
    }
    if (GC5exitLatency > m_JTGC5ExitLatencyMS)
    {
        Printf(Tee::PriHigh, "JT/GC5 exit latency %ums > tolerance %ums\n", GC5exitLatency, m_JTGC5ExitLatencyMS);
        bIsExceeded = true;
    }
    if (bIsExceeded == true)
    {
        return RC::UNEXPECTED_RESULT;
    }

    return rc;
}

RC LWSRSystemCheck::FreeLoggerBuffer()
{
    RC rc;
    Display* pDisplay = GetDisplay();
    const UINT32 panelID = pDisplay->Selected();
    LW0073_CTRL_CMD_SYSTEM_SCANOUT_LOGGING_PARAMS scanoutLogging;
    memset(&scanoutLogging, 0, sizeof(scanoutLogging));
    scanoutLogging.subDeviceInstance = GetBoundGpuSubdevice()->GetSubdeviceInst();
    scanoutLogging.displayId =  panelID;
    scanoutLogging.cmd = LW0073_CTRL_SYSTEM_SCANOUT_LOGGING_DISABLE;
    scanoutLogging.bFreeBuffer = LW_TRUE;
    CHECK_RC_MSG(pDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_SCANOUT_LOGGING,
        &scanoutLogging, sizeof(scanoutLogging)), "Error freeing logger buffer\n");
    return OK;
}

RC LWSRSystemCheck::CheckGC6andNLTStats()
{
    RC rc;

    LW2080_CTRL_LPWR_FEATURE_PARAMETER_GET_PARAMS params = {};
    params.listSize = 1;
    params.list[0].feature.id    = LW2080_CTRL_LPWR_FEATURE_ID_GC6;
    params.list[0].param.id      = LW2080_CTRL_LPWR_PARAMETER_ID_GC6_GET_STATS;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        GetBoundGpuSubdevice(),
        LW2080_CTRL_CMD_LPWR_FEATURE_PARAMETER_GET,
        &params,
        sizeof(params)));

    if (params.list[0].param.flag != LW2080_CTRL_LPWR_FEATURE_PARAMETER_FLAG_SUCCEED_YES)
    {
        Printf(Tee::PriError, "Failed to get GC6 latencies!\n");
        return RC::LWRM_ERROR;
    }

    UINT32 nltMaxUS      = params.list[LW2080_CTRL_LPWR_GC6_NLT_MAX_US].param.val;
    UINT32 sbiosInMaxUS  = params.list[LW2080_CTRL_LPWR_GC6_POWER_OFF_MAX_US].param.val;
    UINT32 sbiosOutMaxUS = params.list[LW2080_CTRL_LPWR_GC6_POWER_ON_MAX_US].param.val;

    VerbosePrintf("GC6   NLT latency(us) = %u\n", nltMaxUS);
    VerbosePrintf("SBIOS In  latency(us) = %u\n", sbiosInMaxUS);
    VerbosePrintf("SBIOS Out latency(us) = %u\n", sbiosOutMaxUS);

    bool bIsExceeded = false;
    if (nltMaxUS > m_MaxGC6NLTLatencyUS)
    {
        Printf(Tee::PriError, "GC6 NLT latency %uus exceeds threshold %uus!\n",
                nltMaxUS, m_MaxGC6NLTLatencyUS);
        bIsExceeded = true;
    }
    if (sbiosInMaxUS > m_MaxSbiosInLatencyMS * 1000)
    {
        Printf(Tee::PriError, "SBIOS In latency %.2fms exceeds threshold %ums\n",
                static_cast<float>(sbiosInMaxUS) / 1000, m_MaxSbiosInLatencyMS);
        bIsExceeded = true;
    }
    if (sbiosOutMaxUS > m_MaxSbiosOutLatencyMS * 1000)
    {
        Printf(Tee::PriError, "SBIOS Out latency %.2fms exceeds threshold %ums\n",
                static_cast<float>(sbiosOutMaxUS) / 1000, m_MaxSbiosOutLatencyMS);
        bIsExceeded = true;
    }

    if (bIsExceeded)
    {
        return RC::UNEXPECTED_RESULT;
    }

    return OK;
}

JS_CLASS_INHERIT(LWSRSystemCheck, GpuTest, "LWSRSystemCheck");
CLASS_PROP_READWRITE(LWSRSystemCheck, SkipPhotometricTests, bool,
                     "Skip the subtests that require the use of the CR100 photometer");
CLASS_PROP_READWRITE(LWSRSystemCheck, SkipSmiTrapTest, bool,
                     "Skip the SMI trap subtest");
CLASS_PROP_READWRITE(LWSRSystemCheck, SkipFramelockLatency, bool,
                     "Skip the Framelock latency subtest");
CLASS_PROP_READWRITE(LWSRSystemCheck, SkipMSHybridCheck, bool,
                     "Skip MSHybrid check when verifying JT caps");
CLASS_PROP_READWRITE(LWSRSystemCheck, PrintSubtestRC, bool,
                     "Print the RC from each of the individual subtests. Defaults to true.");
CLASS_PROP_READWRITE(LWSRSystemCheck, GC5Mode, UINT32,
                     "DeepIdle mode to test. 1=DI-OS or 2=DI-SSC. (default=DI-OS)");
CLASS_PROP_READWRITE(LWSRSystemCheck, FramelockFrameLatency, FLOAT64,
                     "Within how many frames the framelock pin needs to be toggled");
CLASS_PROP_READWRITE(LWSRSystemCheck, FramelockLatencyIterations, UINT32,
                     "How many iterations to test the framelock latency");
CLASS_PROP_READWRITE(LWSRSystemCheck, JTGCXBacklightTolerance, FLOAT64,
                     "At least how much brighter the panel must be while illuminated compared to while dark. In cd/m2.");
CLASS_PROP_READWRITE(LWSRSystemCheck, JTGCXBacklightVariance, FLOAT64,
                     "How much the backlight luminance can vary from outside to inside self-refresh. In cd/m2.");
CLASS_PROP_READWRITE(LWSRSystemCheck, SelfRefreshGetTimeDelayMS, FLOAT64,
                     "Millisecond delay for Xp::GetWallTimeUS() for SelfRefresh measure");
CLASS_PROP_READWRITE(LWSRSystemCheck, JTGC6EnterLatencyMS, UINT32,
                     "Millisecond latency to enter JT/GC6");
CLASS_PROP_READWRITE(LWSRSystemCheck, JTGC6ExitLatencyMS, UINT32,
                     "Millisecond latency to exit JT/GC6");
CLASS_PROP_READWRITE(LWSRSystemCheck, JTGC5EnterLatencyMS, UINT32,
                     "Millisecond latency to enter JT/GC5");
CLASS_PROP_READWRITE(LWSRSystemCheck, JTGC5ExitLatencyMS, UINT32,
                     "Millisecond latency to exit JT/GC5");
CLASS_PROP_READWRITE(LWSRSystemCheck, MaxSbiosInLatencyMS, UINT32,
                     "Millisecond latency to GC6 enter sbios call");
CLASS_PROP_READWRITE(LWSRSystemCheck, MaxSbiosOutLatencyMS, UINT32,
                     "Millisecond latency to GC6 exit sbios call");
