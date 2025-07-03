/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// TCONStress : Test dedicated to stressing LWSR TCONs (bug 1637020) that covers
//              many iterations dedicated to Mutex unlocking/locing, Crash Sync
//              & Burst with Flicker measurements (CR-100), NLT

#include "class/cl0004.h"
#include "device/meter/colorimetryresearch.h"
#include "dpu/dpuifscanoutlogging.h"
#include "gpu/utility/gpurectfill.h"
#include "gpu/include/gpusbdev.h"
#include "gputest.h"
#include "gputestc.h"
#include "gpu/display/modeset_utils.h"
#include "gpu/display/tcon_device.h"
#include "core/include/modsedid.h"
#include "lw_ref_dev_sr.h"
#include "core/include/utility.h"

class TCONStress : public GpuTest
{
public:
    TCONStress();

    bool IsSupported();
    RC Setup();
    RC Run();
    RC Cleanup();
    void PrintJsProperties(Tee::Priority pri);

    SETGET_PROP(DelayBeforeMeasurementMs,      UINT32);
    SETGET_PROP(EnableColorToggle,             bool);
    SETGET_PROP(TestMask,                      UINT32);
    SETGET_PROP(MutexIterations,               UINT32);
    SETGET_PROP(FlickerMaximumSearchFrequency, FLOAT64);
    SETGET_PROP(FlickerLowPassFilterFrequency, FLOAT64);
    SETGET_PROP(FlickerFilterOrder,            UINT32);
    SETGET_PROP(FlickerIterations,             UINT32);
    SETGET_PROP(NLTMaximumSearchFrequency,     FLOAT64);
    SETGET_PROP(NLTLowPassFilterFrequency,     FLOAT64);
    SETGET_PROP(NLTFilterOrder,                UINT32);
    SETGET_PROP(NLTIterations,                 UINT32);
    SETGET_PROP(LatencyIntervalMs,             FLOAT64);
    SETGET_PROP(LatencyToleranceMs,            FLOAT64);
    SETGET_PROP(LatencyPrintLog,               bool);
    SETGET_PROP(LatencyIterations,             UINT32);

private:
    GpuTestConfiguration          *m_pTestConfig;
    Display                       *m_pDisplay;
    TCONDevice                     m_TCONDevice;
    UINT32                         m_OrigDisplays;

    Surface2D                      m_DisplaySurface;
    GpuRectFill                    m_GpuRectFill;

    ColorimetryResearchInstruments m_CRInstrument;
    UINT32                         m_CRInstrumentIdx;

    UINT32                         m_DelayBeforeMeasurementMs;

    bool                           m_EnableColorToggle;

    UINT32                         m_FineWaitUS;

    enum
    {
        SUBTEST_MUTEX   = 1,
        SUBTEST_FLICKER = 2,
        SUBTEST_NLT     = 4,
        SUBTEST_LATENCY = 8,
    };
    UINT32                         m_TestMask;

    UINT32                         m_MutexIterations;

    FLOAT64                        m_FlickerMaximumSearchFrequency;
    FLOAT64                        m_FlickerLowPassFilterFrequency;
    UINT32                         m_FlickerFilterOrder;
    UINT32                         m_FlickerStressColor;
    UINT32                         m_FlickerIterations;

    FLOAT64                        m_NLTMaximumSearchFrequency;
    FLOAT64                        m_NLTLowPassFilterFrequency;
    UINT32                         m_NLTFilterOrder;
    UINT32                         m_NLTStressColor;
    FLOAT64                        m_NLTMinimalLuminanceCdM2;
    FLOAT64                        m_NLTLuminanceToleranceCdM2;
    UINT32                         m_NLTIterations;

    UINT32                         m_LatencyStressColor;
    FLOAT64                        m_LatencyIntervalAdjustmentMs;
    FLOAT64                        m_LatencyIntervalMs;
    FLOAT64                        m_LatencyToleranceMs;
    bool                           m_LatencyPrintLog;
    UINT32                         m_LatencyIterations;

    EvoRasterSettings              m_NativeRaster;
    struct TestMode
    {
        UINT32  VTotal;
        FLOAT64 RefreshRate;
    };
    vector<TestMode>               m_TestModes;

    RC MutexStress();
    RC FlickerStress();
    RC FlickerStressCrashSyncLoops();
    RC NLTStress();
    RC LatencyStress();
    RC LatencyStressCrashSyncLoops();

    RC SetModeAndImage(EvoRasterSettings *pEvoRasterSettings);
    bool IsRegisterDisabled(UINT32 address);
    RC FreeLoggerBuffer();
};

TCONStress::TCONStress()
: m_pDisplay(nullptr)
, m_OrigDisplays(0)
, m_CRInstrumentIdx(0)
, m_DelayBeforeMeasurementMs(100)
, m_EnableColorToggle(false)
, m_FineWaitUS(2000)
, m_TestMask(~0)
, m_MutexIterations(3)
, m_FlickerMaximumSearchFrequency(50.0)
, m_FlickerLowPassFilterFrequency(50.0)
, m_FlickerFilterOrder(4)
, m_FlickerStressColor(0x808080)
, m_FlickerIterations(3)
, m_NLTMaximumSearchFrequency(90.0)
, m_NLTLowPassFilterFrequency(0.0)
, m_NLTFilterOrder(1)
, m_NLTStressColor(0xFFFFFF)
, m_NLTMinimalLuminanceCdM2(50)
, m_NLTLuminanceToleranceCdM2(10)
, m_NLTIterations(3)
, m_LatencyStressColor(0x808080)
, m_LatencyIntervalAdjustmentMs(1.0)
, m_LatencyIntervalMs(0.0)
, m_LatencyToleranceMs(1.0)
, m_LatencyPrintLog(false)
, m_LatencyIterations(100)
{
    SetName("TCONStress");

    m_pTestConfig = GetTestConfiguration();
}

bool TCONStress::IsSupported()
{
    if (GetBoundGpuSubdevice()->IsSOC())
    {
        Printf(Tee::PriLow, "Skipping TCONStress on SOC GPU\n");
        return false;
    }

    return true;
}

RC TCONStress::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::Setup());

    if (m_TestMask == 0)
    {
        Printf(Tee::PriNormal,
            "TCONStress error: TestMask=0 - at least one test has to be enabled.\n");
        return RC::ILWALID_ARGUMENT;
    }

    CHECK_RC(GpuTest::AllocDisplay());
    m_pDisplay = GetDisplay();
    m_OrigDisplays = m_pDisplay->Selected();

    CHECK_RC(m_GpuRectFill.Initialize(m_pTestConfig));

    UINT32 numCRInstuments = 0;
    if ((OK != m_CRInstrument.FindInstruments(&numCRInstuments)) ||
        (numCRInstuments == 0))
    {
        Printf(Tee::PriNormal,
            "TCONStress error: Photometric instrument not available - some"
            " subtests will fail.\n");
    }

    return rc;
}

RC TCONStress::Run()
{
    RC rc;
    StickyRC subtestsRc;
    bool stopOnError = GetGoldelwalues()->GetStopOnError();
    UINT32 panelID = m_OrigDisplays;
    Edid *modsEdid = m_pDisplay->GetEdidMember();

    Utility::CleanupOnReturnArg<Display, void, bool>
        RestoreUsePersistentClass073Handle(m_pDisplay,
            &Display::SetUsePersistentClass073Handle,
            m_pDisplay->GetUsePersistentClass073Handle());

    // The persistent handle is needed for VRR / crash sync setup.
    // RM deletes VRR resources when the handle is closed.
    // Can't use a locally allocated handle as the aux read/writes
    // also need it inside of Display:: and there can be only one
    // allocated at a time.
    m_pDisplay->SetUsePersistentClass073Handle(true);

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
    m_TestModes.clear();
    TestMode nativeTestMode = { m_NativeRaster.RasterHeight,
        m_NativeRaster.CallwlateRefreshRate() };
    m_TestModes.push_back(nativeTestMode);

    CHECK_RC(m_TCONDevice.Alloc(m_pDisplay, panelID));

    CHECK_RC(m_TCONDevice.UnlockBasicRegisters());

    if (m_TestMask & SUBTEST_MUTEX)
    {
        subtestsRc = MutexStress();
        if (stopOnError)
            CHECK_RC(subtestsRc);
    }

    CHECK_RC(m_TCONDevice.UnlockMutex());

    CHECK_RC(m_TCONDevice.InitPanel());

    m_DisplaySurface.SetWidth(m_NativeRaster.ActiveX);
    m_DisplaySurface.SetHeight(m_NativeRaster.ActiveY);
    m_DisplaySurface.SetColorFormat(ColorUtils::ColorDepthToFormat(32));
    m_DisplaySurface.SetLayout(Surface2D::BlockLinear);
    m_DisplaySurface.SetDisplayable(true);
    CHECK_RC(m_DisplaySurface.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_GpuRectFill.SetSurface(&m_DisplaySurface));

    if (m_TestMask & SUBTEST_FLICKER)
    {
        subtestsRc = FlickerStress();
        if (stopOnError)
            CHECK_RC(subtestsRc);
    }

    if (m_TestMask & SUBTEST_NLT)
    {
        subtestsRc = NLTStress();
        if (stopOnError)
            CHECK_RC(subtestsRc);
    }

    if (m_TestMask & SUBTEST_LATENCY)
    {
        subtestsRc = LatencyStress();
        if (stopOnError)
            CHECK_RC(subtestsRc);
    }

    return subtestsRc;
}

RC TCONStress::MutexStress()
{
    RC rc;
    Tee::Priority pri = GetVerbosePrintPri();

    Printf(pri, "Starting Mutex Stress\n");

    for (UINT32 loopNum = 1; loopNum <= m_MutexIterations; loopNum++)
    {
        Printf(pri, "Mutex loop idx = %d\n", loopNum);
        rc = m_TCONDevice.LockMutex();
        if (rc != OK)
        {
            Printf(Tee::PriNormal,
                "Error in MutexStress: Unable to lock mutex at loop %d\n",
                loopNum);
            return rc;
        }
        if (!IsRegisterDisabled(LW_SR_SR_CAPABILITIES0))
        {
            Printf(Tee::PriNormal,
                "Error in MutexStress: Expected not to read SR Capabilities"
                " register at loop %d\n",
                loopNum);
            return RC::UNEXPECTED_RESULT;
        }

        rc = m_TCONDevice.UnlockMutex();
        if (rc != OK)
        {
            Printf(Tee::PriNormal,
                "Error in MutexStress: Unable to unlock mutex at loop %d\n",
                loopNum);
            return rc;
        }
        if (IsRegisterDisabled(LW_SR_SR_CAPABILITIES0))
        {
            Printf(Tee::PriNormal,
                "Error in MutexStress: Expected to read SR Capabilities"
                " register at loop %d\n",
                loopNum);
            return RC::UNEXPECTED_RESULT;
        }
    }

    return OK;
}

RC TCONStress::FlickerStress()
{
    RC rc;
    Tee::Priority pri = GetVerbosePrintPri();

    Printf(pri, "Starting Flicker Stress\n");

    CHECK_RC(m_GpuRectFill.Fill(0,
                                0,
                                m_DisplaySurface.GetWidth(),
                                m_DisplaySurface.GetHeight(),
                                m_FlickerStressColor,
                                true));

    CHECK_RC(SetModeAndImage(&m_NativeRaster));

    // Give time to the display to sync:
    Tasker::Sleep(m_DelayBeforeMeasurementMs);

    FLOAT64 baseFlickerFrequency = 0;
    FLOAT64 baseFlickerLevel = 0;
    FLOAT64 basefma = 0;
    CHECK_RC_MSG(m_CRInstrument.MeasureFlicker(m_CRInstrumentIdx,
        m_FlickerMaximumSearchFrequency, m_FlickerLowPassFilterFrequency,
        m_FlickerFilterOrder,
        &baseFlickerFrequency, &baseFlickerLevel, &basefma),
        "TCONStress error: failed to measure base flicker\n");

    Printf(pri, "   Base Flicker Frequency = %.1f Hz\n",
        baseFlickerFrequency);
    Printf(pri, "   Base Flicker Level     = %.2f dB\n",
        baseFlickerLevel);
    Printf(pri, "   Base FMA               = %.2f %%\n",
        basefma);

    Utility::CleanupOnReturn<TCONDevice, RC>
        exitSR(&m_TCONDevice, &TCONDevice::ExitSelfRefresh);

    CHECK_RC(m_TCONDevice.SelectSREntryMethod());
    CHECK_RC(m_TCONDevice.SetupResync(m_TCONDevice.GetSREntryLatency()));
    CHECK_RC(m_TCONDevice.CacheFrame(nullptr)); // Enables the SR mode at the same time

    EvoRasterSettings burstRaster;
    const UINT32 MINIMUM_BURST_PIXEL_CLOCK = 60000000;
    m_TCONDevice.CallwlateBurstRaster(&m_NativeRaster,
        MINIMUM_BURST_PIXEL_CLOCK, &burstRaster);

    Display::MaxPclkQueryInput maxPclkQuery = {m_OrigDisplays,
                                               &burstRaster};
    vector<Display::MaxPclkQueryInput> maxPclkQueryVector(1, maxPclkQuery);
    CHECK_RC(m_pDisplay->SetRasterForMaxPclkPossible(&maxPclkQueryVector, 0));
    if (burstRaster.PixelClockHz < m_TCONDevice.GetMaxPixelClockHz())
    {
        Printf(Tee::PriNormal,
            "Warning: Max burst pixel clock (%.1f MHz) not tested due to the "
            "source being limited to %.1f MHz\n",
            m_TCONDevice.GetMaxPixelClockHz() / 1e6, burstRaster.PixelClockHz / 1e6);
    }
    else
    {
        burstRaster.PixelClockHz = m_TCONDevice.GetMaxPixelClockHz();
    }

    CHECK_RC(SetModeAndImage(&burstRaster));

    CHECK_RC(m_TCONDevice.WriteBurstTimingRegisters(&burstRaster));

    CHECK_RC(m_TCONDevice.EnableBufferedBurstRefreshMode());

    CHECK_RC(m_TCONDevice.CacheFrame(nullptr));

    CHECK_RC(m_TCONDevice.EnableVrr());

    CHECK_RC(FlickerStressCrashSyncLoops());

    CHECK_RC(m_TCONDevice.DisableBufferedBurstRefreshMode());

    CHECK_RC(m_TCONDevice.DisableVrr());

    CHECK_RC(SetModeAndImage(&m_NativeRaster));

    CHECK_RC(m_TCONDevice.ExitSelfRefresh());

    Tasker::Sleep(m_DelayBeforeMeasurementMs);
    FLOAT64 stressedFlickerFrequency = 0;
    FLOAT64 stressedFlickerLevel = 0;
    FLOAT64 stressedFMA = 0;
    CHECK_RC_MSG(m_CRInstrument.MeasureFlicker(m_CRInstrumentIdx,
        m_FlickerMaximumSearchFrequency, m_FlickerLowPassFilterFrequency,
        m_FlickerFilterOrder,
        &stressedFlickerFrequency, &stressedFlickerLevel, &stressedFMA),
        "TCONStress error: failed to measure stressed flicker\n");

    Printf(pri, "   Stressed Flicker Frequency = %.1f Hz\n",
        stressedFlickerFrequency);
    Printf(pri, "   Stressed Flicker Level     = %.2f dB\n",
        stressedFlickerLevel);
    Printf(pri, "   Stressed FMA               = %.2f %%\n",
        stressedFMA);

    if (stressedFlickerLevel > -45.0)
    {
        Printf(Tee::PriNormal,
            "Error: Excessive Stress Flicker level = %.2f dB.\n",
            stressedFlickerLevel);
        return RC::UNEXPECTED_RESULT;
    }

    return OK;
}

// Assumes that the burst mode is already enabled:
RC TCONStress::FlickerStressCrashSyncLoops()
{
    RC rc;
    Tee::Priority pri = GetVerbosePrintPri();

    FLOAT64 shortDelayMS = 1000.0/m_TestModes[0].RefreshRate;

    FLOAT64 longDelayMS;
    if (m_TestModes.size() < 2)
    {
        Printf(Tee::PriNormal,
            "Warning: Flicker stress will use long_delay = 2*short_delay due"
            " to lack of proper DTD#s\n");
        longDelayMS = 2*shortDelayMS;
    }
    else
    {
        longDelayMS = 1000.0/m_TestModes.back().RefreshRate;
    }
    Printf(pri, "Flicker stress uses short_delay = %.2f ms\n", shortDelayMS);
    Printf(pri, "Flicker stress uses long_delay  = %.2f ms\n", longDelayMS);

    LW0073_CTRL_SYSTEM_GET_VRR_CONFIG_PARAMS vrrConfig = {0};
    vrrConfig.subDeviceInstance = GetBoundGpuSubdevice()->GetSubdeviceInst();
    CHECK_RC_MSG(m_pDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_GET_VRR_CONFIG,
        &vrrConfig, sizeof(vrrConfig)), "Unable to get VRR config\n");

    // The stall takes effect after a "flip" - SetImage
    CHECK_RC(m_pDisplay->SetStallLock(m_OrigDisplays, true, vrrConfig.frameLockPin));

    CHECK_RC(m_pDisplay->SetImage(&m_DisplaySurface));

    Printf(pri, "Starting crash sync loops:\n");

    LW0073_CTRL_SYSTEM_FORCE_VRR_RELEASE_PARAMS params = {0};
    params.subDeviceInstance = GetBoundGpuSubdevice()->GetSubdeviceInst();
    params.waitForFlipMs = 0; // ASAP

    struct DelaySequence
    {
        const UINT32 NumDelays;
        const bool LongDelay[4];
    };
    const DelaySequence delaySequences[] =
    {
        { 2, { false, true } }
       ,{ 3, { false, false, true } }
       ,{ 3, { false, true,  true } }
       ,{ 4, { false, false, false, true } }
    };
    const UINT32 numDelaySequences = sizeof(delaySequences)/sizeof(delaySequences[0]);

    UINT64 startTimeUS = Xp::GetWallTimeUS();

    for (UINT32 loopNum = 1;
         loopNum <= m_FlickerIterations;
         loopNum++)
    {
        Printf(pri, "Flicker loop idx = %d\n", loopNum);
        for (UINT32 sequenceIdx = 0; sequenceIdx < numDelaySequences; sequenceIdx++)
        {
            const DelaySequence *delaySequence = &delaySequences[sequenceIdx];
            for (UINT32 delayIdx = 0; delayIdx < delaySequence->NumDelays; delayIdx++)
            {
                UINT64 lwrrentTimeUS = Xp::GetWallTimeUS();
                FLOAT64 timeToSleepMS = startTimeUS/1000.0 - lwrrentTimeUS/1000.0 +
                    (delaySequence->LongDelay[delayIdx] ? longDelayMS : shortDelayMS);
                if (timeToSleepMS > 0.0)
                {
                     // SleepUS is more reliable than Tasker::Sleep
                    Utility::SleepUS(static_cast<UINT32>(1000*timeToSleepMS));
                }
                startTimeUS += static_cast<UINT64>(1000 *
                    (delaySequence->LongDelay[delayIdx] ? longDelayMS : shortDelayMS));

                CHECK_RC_MSG(m_pDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_FORCE_VRR_RELEASE,
                    &params, sizeof(params)), "Unable to perform VRR release\n");
            }
        }
    }

    CHECK_RC(m_pDisplay->SetStallLock(m_OrigDisplays, false, 0));
    CHECK_RC(m_pDisplay->SetImage(&m_DisplaySurface));

    CHECK_RC(m_TCONDevice.CacheFrame(nullptr));

    return OK;
}

struct RestoreGPIOParams
{
    GpuSubdevice *pGpuSub;
    UINT32 gpioIdx;
    bool orgBLGPIOvalue;
};
static void RestoreGPIO(RestoreGPIOParams params)
{
    params.pGpuSub->GpioSetOutput(params.gpioIdx, params.orgBLGPIOvalue);
}

RC TCONStress::NLTStress()
{
    RC rc;
    Tee::Priority pri = GetVerbosePrintPri();
    GpuSubdevice * pGpuSub = GetBoundGpuSubdevice();

    Printf(pri, "Starting NLT Stress\n");

    CHECK_RC(m_GpuRectFill.Fill(0,
                                0,
                                m_DisplaySurface.GetWidth(),
                                m_DisplaySurface.GetHeight(),
                                m_NLTStressColor,
                                true));

    CHECK_RC(SetModeAndImage(&m_NativeRaster));

    LW0073_CTRL_DFP_GET_LCD_GPIO_PIN_NUM_PARAMS blGPIO = {};
    blGPIO.subDeviceInstance = pGpuSub->GetSubdeviceInst();
    blGPIO.displayId = m_OrigDisplays;
    blGPIO.funcType = LW0073_CTRL_CMD_DFP_LCD_GPIO_FUNC_TYPE_LCD_BACKLIGHT;
    CHECK_RC_MSG(m_pDisplay->RmControl(LW0073_CTRL_CMD_DFP_GET_LCD_GPIO_PIN_NUM,
        &blGPIO, sizeof(blGPIO)), "Unable to get Backlight Enable GPIO index\n");

    RestoreGPIOParams restoreGPIOParams = { pGpuSub, blGPIO.lcdGpioPinNum,
        pGpuSub->GpioGetOutput(blGPIO.lcdGpioPinNum) };
    Utility::CleanupOnReturnArg<void, void, RestoreGPIOParams>
        restoreBL(RestoreGPIO, restoreGPIOParams);

    FLOAT64 luminanceSyncFrequency = 0.0; // It will be determined automatically.
    FLOAT64 luminanceBaseCdM2 = 0;

    CHECK_RC(m_CRInstrument.MeasureLuminance(m_CRInstrumentIdx,
        &luminanceSyncFrequency, &luminanceBaseCdM2));
    Printf(pri, "   Base luminance = %.2f cd/m2\n", luminanceBaseCdM2);
    if (luminanceBaseCdM2 < m_NLTMinimalLuminanceCdM2)
    {
        Printf(Tee::PriNormal,
            "Error in NLT Stress: Measured base luminance is too low = %.2f cd/m2\n",
            luminanceBaseCdM2);
        return RC::UNEXPECTED_RESULT;
    }

    CHECK_RC(m_TCONDevice.SelectSREntryMethod());
    CHECK_RC(m_TCONDevice.SetupResync(m_TCONDevice.GetSREntryLatency()));

    Utility::CleanupOnReturn<TCONDevice, RC>
        exitSR(&m_TCONDevice, &TCONDevice::ExitSelfRefresh);

    for (UINT32 loopNum = 1;
         loopNum <= m_NLTIterations;
         loopNum++)
    {
        Printf(pri, "NLT loop idx = %d\n", loopNum);

        CHECK_RC_MSG(m_CRInstrument.MeasureFlickerStart(m_CRInstrumentIdx),
            "TCONStress error: Failed to start measure flicker at SR entry in NLT Stress\n");
        CHECK_RC(m_TCONDevice.CacheFrame(nullptr)); // Enables the SR mode at the same time
        FLOAT64 srEntryFlickerFrequency = 0;
        FLOAT64 srEntryFlickerLevel = 0;
        FLOAT64 srEntryFMA = 0;
        CHECK_RC_MSG(m_CRInstrument.MeasureFlicker(m_CRInstrumentIdx,
            m_NLTMaximumSearchFrequency, m_NLTLowPassFilterFrequency,
            m_NLTFilterOrder,
            &srEntryFlickerFrequency, &srEntryFlickerLevel, &srEntryFMA),
            "TCONStress error: Failed to measure flicker at SR entry in NLT Stress\n");
        Printf(pri, "   SR Entry Flicker Frequency = %.1f Hz\n",
            srEntryFlickerFrequency);
        Printf(pri, "   SR Entry Flicker Level     = %.2f dB\n",
            srEntryFlickerLevel);
        Printf(pri, "   SR Entry FMA               = %.2f %%\n",
            srEntryFMA);

        CHECK_RC(m_pDisplay->DetachAllDisplays());

        pGpuSub->GpioSetOutput(blGPIO.lcdGpioPinNum, false);
        if (pGpuSub->GpioGetOutput(blGPIO.lcdGpioPinNum) != false)
        {
            Printf(Tee::PriNormal, "Error: Unable to set Backlight Enable GPIO to 0\n");
            return RC::UNEXPECTED_RESULT;
        }

        FLOAT64 luminanceCdM2 = 0;
        CHECK_RC(m_CRInstrument.MeasureLuminance(m_CRInstrumentIdx,
            &luminanceSyncFrequency, &luminanceCdM2));
        Printf(pri, "   BL_EN at 0 luminance = %.2f cd/m2\n", luminanceCdM2);

        pGpuSub->GpioSetOutput(blGPIO.lcdGpioPinNum, true);

        CHECK_RC(SetModeAndImage(&m_NativeRaster));

        CHECK_RC_MSG(m_CRInstrument.MeasureFlickerStart(m_CRInstrumentIdx),
            "TCONStress error: Failed to start measure flicker at SR exit in NLT Stress\n");
        CHECK_RC(m_TCONDevice.ExitSelfRefresh());
        FLOAT64 srExitFlickerFrequency = 0;
        FLOAT64 srExitFlickerLevel = 0;
        FLOAT64 srExitFMA = 0;
        CHECK_RC_MSG(m_CRInstrument.MeasureFlicker(m_CRInstrumentIdx,
            m_NLTMaximumSearchFrequency, m_NLTLowPassFilterFrequency,
            m_NLTFilterOrder,
            &srExitFlickerFrequency, &srExitFlickerLevel, &srExitFMA),
            "TCONStress error: Failed to measure flicker at SR exit in NLT Stress\n");
        Printf(pri, "   SR Exit Flicker Frequency = %.1f Hz\n",
            srExitFlickerFrequency);
        Printf(pri, "   SR Exit Flicker Level     = %.2f dB\n",
            srExitFlickerLevel);
        Printf(pri, "   SR Exit FMA               = %.2f %%\n",
            srExitFMA);

        if (srEntryFlickerLevel > -45.0)
        {
            Printf(Tee::PriNormal,
                "Error: NLT Stress excessive SR Entry Flicker level = %.2f dB.\n",
                srEntryFlickerLevel);
            return RC::UNEXPECTED_RESULT;
        }

        if (fabs(luminanceBaseCdM2 - luminanceCdM2) > m_NLTLuminanceToleranceCdM2)
        {
            Printf(Tee::PriNormal,
                "Error: Measured luminance with BL_EN at 0 = %.2f cd/m2 differs"
                " too much from the base = %.2f cd/m2\n", luminanceCdM2, luminanceBaseCdM2);
            return RC::UNEXPECTED_RESULT;
        }

        if (srExitFlickerLevel > -45.0)
        {
            Printf(Tee::PriNormal,
                "Error: NLT Stress excessive SR Exit Flicker level = %.2f dB.\n",
                srExitFlickerLevel);
            return RC::UNEXPECTED_RESULT;
        }
    }

    return OK;
}

RC TCONStress::LatencyStress()
{
    StickyRC rc;
    Tee::Priority pri = GetVerbosePrintPri();

    Printf(pri, "Starting Latency Stress\n");

    if (m_LatencyIterations < 10)
    {
        Printf(Tee::PriNormal, "Error: Latency subtest requires at least 10"
            " iterations, but it is configured for %d\n", m_LatencyIterations);
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(m_GpuRectFill.Fill(0,
                                0,
                                m_DisplaySurface.GetWidth(),
                                m_DisplaySurface.GetHeight(),
                                m_LatencyStressColor,
                                true));

    CHECK_RC(SetModeAndImage(&m_NativeRaster));

    Utility::CleanupOnReturn<TCONDevice, RC>
        exitSR(&m_TCONDevice, &TCONDevice::ExitSelfRefresh);

    CHECK_RC(m_TCONDevice.SelectSREntryMethod());
    CHECK_RC(m_TCONDevice.SetupResync(m_TCONDevice.GetSREntryLatency()));
    CHECK_RC(m_TCONDevice.CacheFrame(nullptr)); // Enables the SR mode at the same time

    EvoRasterSettings burstRaster;
    const UINT32 MINIMUM_BURST_PIXEL_CLOCK = 60000000;
    m_TCONDevice.CallwlateBurstRaster(&m_NativeRaster,
        MINIMUM_BURST_PIXEL_CLOCK, &burstRaster);

    Display::MaxPclkQueryInput maxPclkQuery = {m_OrigDisplays,
                                               &burstRaster};
    vector<Display::MaxPclkQueryInput> maxPclkQueryVector(1, maxPclkQuery);
    CHECK_RC(m_pDisplay->SetRasterForMaxPclkPossible(&maxPclkQueryVector, 0));
    if (burstRaster.PixelClockHz < m_TCONDevice.GetMaxPixelClockHz())
    {
        Printf(Tee::PriNormal,
            "Error: Latency Stress requires burst pclk of %.1f MHz, but only"
            "%.1f MHz is possible.\n",
            m_TCONDevice.GetMaxPixelClockHz() / 1e6, burstRaster.PixelClockHz / 1e6);
        return RC::MODE_IS_NOT_POSSIBLE;
    }

    burstRaster.PixelClockHz = m_TCONDevice.GetMaxPixelClockHz();

    CHECK_RC(SetModeAndImage(&burstRaster));

    CHECK_RC(m_TCONDevice.WriteBurstTimingRegisters(&burstRaster));

    CHECK_RC(m_TCONDevice.EnableBufferedBurstRefreshMode());

    CHECK_RC(m_TCONDevice.CacheFrame(nullptr));

    CHECK_RC(m_TCONDevice.EnableVrr());

    rc = LatencyStressCrashSyncLoops();

    rc = m_TCONDevice.DisableVrr();

    rc = m_TCONDevice.DisableBufferedBurstRefreshMode();

    rc = SetModeAndImage(&m_NativeRaster);

    return rc;
}

RC TCONStress::LatencyStressCrashSyncLoops()
{
    RC rc;
    Tee::Priority pri = GetVerbosePrintPri();

    LW2080_CTRL_TIMER_GET_REGISTER_OFFSET_PARAMS timerRegisterOffset;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(m_pDisplay->GetOwningDisplaySubdevice(),
        LW2080_CTRL_CMD_TIMER_GET_REGISTER_OFFSET,
        &timerRegisterOffset, sizeof(timerRegisterOffset)));

    UINT32 timerTime0Offset = timerRegisterOffset.tmr_offset +
        offsetof(Lw01TimerMap, PTimerTime0);

    UINT64 releaseIntervalUS = static_cast<UINT64>(1000*m_LatencyIntervalMs);
    if (releaseIntervalUS == 0)
    {
        FLOAT64 refreshIntervalUS = 1000000.0/m_TestModes.back().RefreshRate;

        releaseIntervalUS = static_cast<UINT64>(refreshIntervalUS +
            1000*m_LatencyIntervalAdjustmentMs);
    }

    Printf(pri, "Latency stress uses release interval = %llu us\n", releaseIntervalUS);

    LW0073_CTRL_SYSTEM_GET_VRR_CONFIG_PARAMS vrrConfig = {0};
    vrrConfig.subDeviceInstance = GetBoundGpuSubdevice()->GetSubdeviceInst();
    CHECK_RC_MSG(m_pDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_GET_VRR_CONFIG,
        &vrrConfig, sizeof(vrrConfig)), "Unable to get VRR config\n");

    // The stall takes effect after a "flip" - SetImage
    CHECK_RC(m_pDisplay->SetStallLock(m_OrigDisplays, true, vrrConfig.frameLockPin));

    CHECK_RC(m_pDisplay->SetImage(&m_DisplaySurface));

    Printf(pri, "Starting crash sync loops:\n");

    LW0073_CTRL_SYSTEM_FORCE_VRR_RELEASE_PARAMS vrrRelease = {0};
    vrrRelease.subDeviceInstance = GetBoundGpuSubdevice()->GetSubdeviceInst();
    vrrRelease.waitForFlipMs = 0; // ASAP

    LW0073_CTRL_CMD_SYSTEM_SCANOUT_LOGGING_PARAMS scanoutLogging = {0};
    scanoutLogging.subDeviceInstance = GetBoundGpuSubdevice()->GetSubdeviceInst();
    scanoutLogging.displayId =  m_OrigDisplays;
    scanoutLogging.cmd = LW0073_CTRL_SYSTEM_SCANOUT_LOGGING_ENABLE;
    scanoutLogging.size = m_LatencyIterations + 1; // The first entry captures logging
                                                   // start and not the actual event
    scanoutLogging.verticalScanline = 1;
    scanoutLogging.bFreeBuffer = LW_FALSE;
    scanoutLogging.bUseRasterLineIntr = LW_FALSE;
    scanoutLogging.scanoutLogFlag = 0;
    scanoutLogging.loggingAddr = 0;
    CHECK_RC_MSG(m_pDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_SCANOUT_LOGGING,
        &scanoutLogging, sizeof(scanoutLogging)), "Error starting the logger\n");

    Utility::CleanupOnReturn<TCONStress, RC>
        freeLoggerBuffer(this, &TCONStress::FreeLoggerBuffer);

    // Bug 1653639:
    // Give some arbitrary amount of time for the logger to actually start
    Tasker::Sleep(300);

    UINT64 releaseReferenceTimeUS = Xp::GetWallTimeUS();

    bool colorToggle = false;

    vector<UINT32> releaseStartGPUTimes;
    releaseStartGPUTimes.resize(m_LatencyIterations);

    for (UINT32 iterationNum = 0;
         iterationNum < m_LatencyIterations;
         iterationNum++)
    {
        Printf(pri, "Latency iteration idx = %d\n", iterationNum);

        if (m_EnableColorToggle)
        {
            CHECK_RC(m_GpuRectFill.Fill(0,
                                        0,
                                        m_DisplaySurface.GetWidth(),
                                        m_DisplaySurface.GetHeight(),
                                        colorToggle ? 0x00ff00 : 0xff0000,
                                        true));
            colorToggle = !colorToggle;
        }

        UINT64 lwrrentTimeUS = Xp::GetWallTimeUS();

        // Even though Utility::SleepUS is much more precise than Tasker::Sleep
        // experimental measurements have shown the SleepUS can still take
        // longer up to 1.5 ms then requested. To solve this finish the Sleep
        // m_FineWaitUS earlier and spend the remaining time in CPU busy wait
        // inside of Utility::Delay.
        INT64 timeToSleepUS = releaseReferenceTimeUS - lwrrentTimeUS +
            releaseIntervalUS - m_FineWaitUS;
        if (timeToSleepUS > 0)
        {
            Utility::SleepUS(static_cast<UINT32>(timeToSleepUS));
        }

        lwrrentTimeUS = Xp::GetWallTimeUS();
        INT64 remainingToReleaseUS = releaseReferenceTimeUS - lwrrentTimeUS +
            releaseIntervalUS;

        if (remainingToReleaseUS > 0)
        {
            Utility::Delay(static_cast<UINT32>(remainingToReleaseUS));
        }

        releaseStartGPUTimes[iterationNum] =
            GetBoundGpuSubdevice()->RegRd32PGSafe(timerTime0Offset);

        // Even when the release is not started at the exactly expected time
        // schedule the next release after the requested interval from now.
        // Otherwise the time between releases might become too short (above
        // available  burst bandwidth to scanout a single frame) if time
        // compensation were to be used.
        releaseReferenceTimeUS = Xp::GetWallTimeUS();

        CHECK_RC_MSG(m_pDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_FORCE_VRR_RELEASE,
            &vrrRelease, sizeof(vrrRelease)), "Unable to perform VRR release\n");
    }

    // Make sure the logger captures the latest release:
    Tasker::Sleep(4.0*releaseIntervalUS/1000.0);

    scanoutLogging.bFreeBuffer = LW_FALSE;
    scanoutLogging.cmd = LW0073_CTRL_SYSTEM_SCANOUT_LOGGING_DISABLE;
    CHECK_RC_MSG(m_pDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_SCANOUT_LOGGING,
        &scanoutLogging, sizeof(scanoutLogging)), "Error stopping the logger\n");

    // The same as bug 1653639:
    // The ctrl call with _DISABLE above is not waiting for the DPU cmd
    // to be exelwted.
    Tasker::Sleep(200);

    vector<SCANOUTLOGGING> scanoutLog;
    scanoutLog.resize(m_LatencyIterations + 1);

    scanoutLogging.loggingAddr = LW_PTR_TO_LwP64(&scanoutLog[0]);
    scanoutLogging.cmd = LW0073_CTRL_SYSTEM_SCANOUT_LOGGING_FETCH;
    CHECK_RC_MSG(m_pDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_SCANOUT_LOGGING,
        &scanoutLogging, sizeof(scanoutLogging)), "Error retrieving logger data\n");

    Tee::Priority priLog = pri;
    if (m_LatencyPrintLog)
        priLog = Tee::PriNormal;

    bool reportError = false;

    Printf(priLog,
        "Release idx | Release diff (ms) | Scanout diff (ms) | Scanout diff - Release diff (ms)\n");

    map<INT32, UINT32> scanoutMinusReleaseDistribution;

    for (UINT32 idx = 1; idx < m_LatencyIterations; idx++)
    {
        UINT32 releaseStartDiff = releaseStartGPUTimes[idx] - releaseStartGPUTimes[idx-1];
        INT64 scanoutDiff = scanoutLog[idx+1].timeStamp - scanoutLog[idx].timeStamp;
        FLOAT64 scanoutMinusReleaseMs = (scanoutDiff - releaseStartDiff)/1000000.0;
        INT32 scanoutMinusReleaseTenthMs = static_cast<INT32>(10*scanoutMinusReleaseMs);

        if (scanoutMinusReleaseDistribution.find(scanoutMinusReleaseTenthMs) ==
            scanoutMinusReleaseDistribution.end())
        {
            scanoutMinusReleaseDistribution[scanoutMinusReleaseTenthMs] = 1;
        }
        else
        {
            scanoutMinusReleaseDistribution[scanoutMinusReleaseTenthMs]++;
        }

        string logLine = Utility::StrPrintf(
            "%11d   %17.3f   %17.3f   %+32.3f",
            idx,
            releaseStartDiff/1000000.0,
            scanoutDiff/1000000.0,
            scanoutMinusReleaseMs);

        if (fabs(scanoutMinusReleaseMs) > m_LatencyToleranceMs)
        {
            reportError = true;
            logLine += Utility::StrPrintf(
                "   Error: scanout time beyond tolerance = +/- %.1f ms",
                m_LatencyToleranceMs);
        }
        Printf(priLog, "%s\n", logLine.c_str());
    }

    Tee::Priority priDistribution = priLog;
    if (reportError)
    {
        priDistribution = Tee::PriNormal;
    }

    Printf(priDistribution,
        "Measured scanout to release differences (tolerance = +/- %.1f ms):\n",
        m_LatencyToleranceMs
        );

    for (std::map<INT32, UINT32>::const_iterator iter = scanoutMinusReleaseDistribution.begin();
         iter != scanoutMinusReleaseDistribution.end();
         iter++)
    {
        INT32 timeTenthMS = iter->first;
        Printf(priDistribution, "[%+3.1f ms] = %d\n",
            timeTenthMS/10.0, iter->second);
    }

    CHECK_RC(m_pDisplay->SetStallLock(m_OrigDisplays, false, 0));
    CHECK_RC(m_pDisplay->SetImage(&m_DisplaySurface));

    CHECK_RC(m_TCONDevice.CacheFrame(nullptr));

    return reportError ? RC::UNEXPECTED_RESULT : OK;
}

RC TCONStress::SetModeAndImage(EvoRasterSettings *pEvoRasterSettings)
{
    RC rc;
    const UINT32 panelID = m_OrigDisplays;

    // This is needed to prevent exelwting a SetMode while some
    // previous "SetImage" is still active. Such situation
    // is now signalled with SOFTWARE_ERROR during SetMode.
    CHECK_RC(m_pDisplay->SetImage(static_cast<Surface2D*>(nullptr)));

    CHECK_RC(m_pDisplay->SetTimings(pEvoRasterSettings));

    // This is needed to avoid internal display pipe rounding which results
    // in pixel values 254 and 255 to come out the same on the output
    CHECK_RC(m_pDisplay->SelectColorTranscoding(panelID, Display::ZeroFill));

    m_pDisplay->SetPendingSetModeChange();

    CHECK_RC(m_pDisplay->SetMode(
        pEvoRasterSettings->ActiveX,
        pEvoRasterSettings->ActiveY,
        32,
        UINT32(0.5 + pEvoRasterSettings->CallwlateRefreshRate())));

    CHECK_RC(m_pDisplay->SetImage(&m_DisplaySurface));

    CHECK_RC(m_TCONDevice.UnlockBasicRegisters());
    CHECK_RC(m_TCONDevice.UnlockMutex());

    Tasker::Sleep(100);

    return OK;
}

bool TCONStress::IsRegisterDisabled(UINT32 address)
{
    UINT08 readValue = 0xFF;
    if (OK == m_TCONDevice.AuxRead(address, &readValue, sizeof(readValue)))
    {
        if (readValue == 0)
            return true;
        else
            return false;
    }
    return true;
}

RC TCONStress::FreeLoggerBuffer()
{
    RC rc;
    LW0073_CTRL_CMD_SYSTEM_SCANOUT_LOGGING_PARAMS scanoutLogging;
    memset(&scanoutLogging, 0, sizeof(scanoutLogging));
    scanoutLogging.subDeviceInstance = GetBoundGpuSubdevice()->GetSubdeviceInst();
    scanoutLogging.displayId =  m_OrigDisplays;
    scanoutLogging.cmd = LW0073_CTRL_SYSTEM_SCANOUT_LOGGING_DISABLE;
    scanoutLogging.bFreeBuffer = LW_TRUE;
    CHECK_RC_MSG(m_pDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_SCANOUT_LOGGING,
        &scanoutLogging, sizeof(scanoutLogging)), "Error freeing logger buffer\n");
    return OK;
}

RC TCONStress::Cleanup()
{
    StickyRC rc;

    if (m_pDisplay != nullptr)
    {
        rc = m_pDisplay->SetImage((Surface2D*)nullptr);
        m_DisplaySurface.Free();
        rc = m_pDisplay->Select(m_OrigDisplays);
        m_pDisplay->SetTimings(NULL);
        m_pDisplay->SetPendingSetModeChange();
        m_pDisplay = nullptr;
    }
    m_TCONDevice.Free();
    rc = m_GpuRectFill.Cleanup();
    rc = GpuTest::Cleanup();
    return rc;
}

void TCONStress::PrintJsProperties(Tee::Priority pri)
{
    Printf(pri, "%s Js Properties:\n", GetName().c_str());

    Printf(pri, "    DelayBeforeMeasurementMs:      %d\n", m_DelayBeforeMeasurementMs);
    Printf(pri, "    EnableColorToggle:             %s\n", m_EnableColorToggle ? "true" : "false");
    Printf(pri, "    TestMask:                      0x%x\n", m_TestMask);
    Printf(pri, "    MutexIterations:               %d\n", m_MutexIterations);
    Printf(pri, "    FlickerMaximumSearchFrequency: %f\n", m_FlickerMaximumSearchFrequency);
    Printf(pri, "    FlickerLowPassFilterFrequency: %f\n", m_FlickerLowPassFilterFrequency);
    Printf(pri, "    FlickerFilterOrder:            %d\n", m_FlickerFilterOrder);
    Printf(pri, "    FlickerIterations:             %d\n", m_FlickerIterations);
    Printf(pri, "    NLTMaximumSearchFrequency:     %f\n", m_NLTMaximumSearchFrequency);
    Printf(pri, "    NLTLowPassFilterFrequency:     %f\n", m_NLTLowPassFilterFrequency);
    Printf(pri, "    NLTFilterOrder:                %d\n", m_NLTFilterOrder);
    Printf(pri, "    NLTIterations:                 %d\n", m_NLTIterations);
    Printf(pri, "    LatencyIntervalMs:             %f\n", m_LatencyIntervalMs);
    Printf(pri, "    LatencyToleranceMs:            %f\n", m_LatencyToleranceMs);
    Printf(pri, "    LatencyPrintLog:               %s\n", m_LatencyPrintLog ? "true" : "false");
    Printf(pri, "    LatencyIterations:             %d\n", m_LatencyIterations);

    GpuTest::PrintJsProperties(pri);
}

JS_CLASS_INHERIT(TCONStress, GpuTest, "TCONStress");

CLASS_PROP_READWRITE(TCONStress, DelayBeforeMeasurementMs, UINT32,
        "Delay in miliseconds after display setup but before measurements");
CLASS_PROP_READWRITE(TCONStress, EnableColorToggle, bool,
        "Debug feature: enable color toggling between flips");
CLASS_PROP_READWRITE(TCONStress, TestMask, UINT32,
        "Bitmask of enabled tests");
CLASS_PROP_READWRITE(TCONStress, MutexIterations, UINT32,
        "Number of mutex stress loops");
CLASS_PROP_READWRITE(TCONStress, FlickerMaximumSearchFrequency, FLOAT64,
        "Maximum measured flicker frequency in flicker stress test");
CLASS_PROP_READWRITE(TCONStress, FlickerLowPassFilterFrequency, FLOAT64,
        "Frequency of low pass filter applied before flicker callwlations "
        "(when set to 0 the filter is disabled) in flicker stress test");
CLASS_PROP_READWRITE(TCONStress, FlickerFilterOrder, UINT32,
        "Filter order used in flicker stress test");
CLASS_PROP_READWRITE(TCONStress, FlickerIterations, UINT32,
        "Number of flicker stress loops");
CLASS_PROP_READWRITE(TCONStress, NLTMaximumSearchFrequency, FLOAT64,
        "Maximum measured flicker frequency in NLT stress test");
CLASS_PROP_READWRITE(TCONStress, NLTLowPassFilterFrequency, FLOAT64,
        "Frequency of low pass filter applied before flicker callwlations "
        "(when set to 0 the filter is disabled) in NLT stress test");
CLASS_PROP_READWRITE(TCONStress, NLTFilterOrder, UINT32,
        "Filter order used in NLT stress test");
CLASS_PROP_READWRITE(TCONStress, NLTIterations, UINT32,
        "Number of NLT stress loops");
CLASS_PROP_READWRITE(TCONStress, LatencyIntervalMs, FLOAT64,
        "Override of time in miliseconds between frame releases in the Latency stress");
CLASS_PROP_READWRITE(TCONStress, LatencyToleranceMs, FLOAT64,
        "Time in miliseconds of tolerance betweeen frame releases vs between actual scanouts");
CLASS_PROP_READWRITE(TCONStress, LatencyPrintLog, bool,
        "Print log of all releases in the Latency stress");
CLASS_PROP_READWRITE(TCONStress, LatencyIterations, UINT32,
        "Number of Latency stress iterations");
