/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// PanelCheck : Test dedicated to LWSR panels (bug 1547024) that covers
//              communication as well as quality measurements with external
//              sensors (Flicker / Colorimeter / Photometric and IR thermometer)

#ifndef BOOST_NO_FELW_H
#define BOOST_NO_FELW_H
#endif
#include <boost/math/special_functions/round.hpp>
#include "core/include/display.h"
#include "gpu/display/tcon_device.h"
#include "gpu/utility/gpurectfill.h"
#include "gpu/include/gpusbdev.h"
#include "gputest.h"
#include "gputestc.h"
#include "core/include/modsedid.h"
#include "lw_ref_dev_sr.h"
#include "core/include/utility.h"
#include "device/irthermometer/extech42570.h"
#include "device/meter/colorimetryresearch.h"
#include "gpu/display/modeset_utils.h"
#include "gpu/js/fpk_comm.h"
#include "core/include/version.h"
#include "core/include/registry.h"
#include "lwRmReg.h"

using boost::math::round;

class PanelCheck : public GpuTest
{
public:
    PanelCheck();

    bool IsSupported();
    RC Setup();
    RC Run();
    RC Cleanup();
    void PrintJsProperties(Tee::Priority pri);

    SETGET_PROP(NoThermometer,                       bool);
    SETGET_PROP(PrintSubtestRC,                      bool);
    SETGET_PROP(DelayBeforeMeasurementMs,            UINT32);
    SETGET_PROP(MaximumSearchFrequency,              FLOAT64);
    SETGET_PROP(LowPassFilterFrequency,              FLOAT64);
    SETGET_PROP(FilterOrder,                         UINT32);
    SETGET_PROP(SkipFlickerCheck,                    bool);
    SETGET_PROP(SkipSelfRefreshCheck,                bool);
    SETGET_PROP(SkipGC6Testing,                      bool);
    SETGET_PROP(OnlyGrayGamma,                       bool);
    SETGET_PROP(FullRangeGamma,                      bool);
    SETGET_PROP(GammaToleranceAtLowerRefreshCdM2,    FLOAT64);
    SETGET_PROP(GammaToleranceAtLowerRefreshPercent, FLOAT64);
    SETGET_PROP(ShowGamma,                           bool);
    SETGET_PROP(OnlyEdidModes,                       bool);
    SETGET_PROP(SelfRefreshCheckGC6Iterations,       UINT32);
    SETGET_PROP(BrightnessCheckGC6Iterations,        UINT32);

private:
    GpuTestConfiguration  *m_pTestConfig;
    Display               *m_pDisplay;
    TCONDevice             m_TCONDevice;
    UINT32                 m_OrigDisplays;
    GCxImpl               *m_pGCx;

    Surface2D              m_DisplaySurface;

    GpuRectFill            m_GpuRectFill;

    ColorimetryResearchInstruments m_CRInstrument;
    UINT32                         m_CRInstrumentIdx;

    Extech42570                    m_IRThermometer;
    UINT32                         m_IRThermometerIdx;
    bool                           m_NoThermometer;

    bool                           m_PrintSubtestRC;
    UINT32                         m_DelayBeforeMeasurementMs;
    FLOAT64                        m_MaximumSearchFrequency;
    FLOAT64                        m_LowPassFilterFrequency;
    UINT32                         m_FilterOrder;
    bool                           m_SkipFlickerCheck;
    bool                           m_SkipSelfRefreshCheck; // TODO: Temporary WAR for Bug 200201572
    bool                           m_SkipGC6Testing;       // TODO: Temporary WAR for Bug 200201572
    FLOAT64                        m_FlickerCheckMinimumLuminanceCdM2;
    FLOAT64                        m_FlickerCheckMaximumLuminanceModeDeltaCdM2;
    FLOAT64                        m_FlickerCheckMaximumLuminanceModeDeltaPercent;
    FLOAT64                        m_CIE1931CoordinatesToleranceRequired;
    FLOAT64                        m_CIE1931CoordinatesToleranceCompetitive;
    bool                           m_OnlyGrayGamma;
    bool                           m_FullRangeGamma;
    FLOAT64                        m_ExpectedGamma;
    FLOAT64                        m_RequiredGammaTolarance;
    FLOAT64                        m_CompetitiveGammaTolerance;
    FLOAT64                        m_GammaToleranceAtLowerRefreshCdM2;
    FLOAT64                        m_GammaToleranceAtLowerRefreshPercent;
    UINT32                         m_NumBorderGammaMeasurements;
    FLOAT64                        m_LuminanceMeasurementToleranceCdM2;
    FLOAT64                        m_MinimumRelativeLuminance;
    bool                           m_ShowGamma;
    FLOAT64                        m_MinimalLuminanceCdM2;
    FLOAT64                        m_MaxNumFramesForCrashSynlwpdate;
    FLOAT64                        m_MaxNumFramesForCrashSync;
    FLOAT64                        m_MaxNumFramesForBurstUpdate;
    FLOAT64                        m_DimBrightBacklightTolerance;
    FLOAT64                        m_GC6BacklightTolerancePercent;
    bool                           m_OnlyEdidModes;
    UINT32                         m_SelfRefreshCheckGC6Iterations;
    UINT32                         m_BrightnessCheckGC6Iterations;
    UINT32                         m_MaxGC6NLTTransitionUS;

    EvoRasterSettings              m_NativeRaster;
    struct TestMode
    {
        UINT32  VTotal;
        FLOAT64 RefreshRate;
        bool reducedMode; // This mode reduces the number of colors tested in GammaCheck
        bool relaxedMode; // This mode relaxes the error thresholds in all subtests
    };
    vector<TestMode>               m_TestModes;

    struct ColorResponsePoint
    {
        UINT32  PixelValue;
        FLOAT32 ExpectedCIE1931x;
        FLOAT32 ExpectedCIE1931y;
        string  ColorName;
    };
    vector<ColorResponsePoint>     m_ColorResponsePoints;

    RC FlickerCheck();
    RC ColorCheck();
    RC GammaCheck();
    RC SelfRefreshCheck();
    RC EdidCheck(const LWT_EDID_INFO &lwtei);
    RC RegisterCheck();
    RC CapabilitiesCheck();
    RC BrightnessCheck();

    RC SetModeAndImage(EvoRasterSettings *pEvoRasterSettings);
    bool IsRegisterDisabled(UINT32 address);
    RC AverageLuminance
    (
        UINT32 numMeasurements,
        FLOAT64 *manualSyncFrequency,
        FLOAT64 *averageLuminanceCdM2
    );
    RC SetBacklightBrightnessAndMeasureLuminance
    (
        FLOAT32 backlightBrightnessPercent,
        FLOAT64 *manualSyncFrequency,
        FLOAT64 *averageLuminanceCdM2
    );
    FLOAT64 CallwlateBorderLuminance
    (
        FLOAT64 minLuminance,
        FLOAT64 relativeLuminance,
        UINT32 pixelLevel,
        FLOAT64 borderGamma,
        FLOAT64 expectedIdealLuminance,
        bool callwlateMin
    );
    RC CheckMinimalLuminance();
    RC MeasureCrashSyncDelays
    (
        FLOAT64 panelRefreshRate,
        EvoRasterSettings *burstRaster
    );
    RC RestoreStallLock();
    RC RestoreSlaveLock();
    RC MeasureBurstDelays
    (
        FLOAT64 panelRefreshRate,
        EvoRasterSettings *burstRaster
    );
    RC CheckNLTTransition();
};

PanelCheck::PanelCheck()
: m_pDisplay(nullptr)
, m_OrigDisplays(0)
, m_pGCx(nullptr)
, m_CRInstrumentIdx(0)
, m_IRThermometerIdx(0)
, m_NoThermometer(false)
, m_PrintSubtestRC(true)
, m_DelayBeforeMeasurementMs(50)
, m_MaximumSearchFrequency(90.0)
, m_LowPassFilterFrequency(0.0)
, m_FilterOrder(1)
, m_SkipFlickerCheck(false)
, m_SkipSelfRefreshCheck(false)
, m_SkipGC6Testing(false)
, m_FlickerCheckMinimumLuminanceCdM2(10.0)
, m_FlickerCheckMaximumLuminanceModeDeltaCdM2(5.0)
, m_FlickerCheckMaximumLuminanceModeDeltaPercent(5.0)
, m_CIE1931CoordinatesToleranceRequired(0.04)
, m_CIE1931CoordinatesToleranceCompetitive(0.03)
, m_OnlyGrayGamma(false)
, m_FullRangeGamma(false)
, m_ExpectedGamma(2.2)
, m_RequiredGammaTolarance(0.35)
, m_CompetitiveGammaTolerance(0.25)
, m_GammaToleranceAtLowerRefreshCdM2(1.0)
, m_GammaToleranceAtLowerRefreshPercent(1.0)
, m_NumBorderGammaMeasurements(3)
, m_LuminanceMeasurementToleranceCdM2(0.05)
, m_MinimumRelativeLuminance(8.0)
, m_ShowGamma(false)
, m_MinimalLuminanceCdM2(10.0)
, m_MaxNumFramesForCrashSynlwpdate(2.0)
, m_MaxNumFramesForCrashSync(0.5)
, m_MaxNumFramesForBurstUpdate(3.0)
, m_DimBrightBacklightTolerance(0.4)
, m_GC6BacklightTolerancePercent(1.0)
, m_OnlyEdidModes(false)
, m_SelfRefreshCheckGC6Iterations(3)
, m_BrightnessCheckGC6Iterations(3)
, m_MaxGC6NLTTransitionUS(500) // 500us from Bug 1821853
{
    SetName("PanelCheck");

    m_pTestConfig = GetTestConfiguration();
}

bool PanelCheck::IsSupported()
{
    if (GetBoundGpuSubdevice()->IsSOC())
    {
        Printf(Tee::PriLow, "Skipping PanelCheck on SOC GPU\n");
        return false;
    }

    return true;
}

RC PanelCheck::Setup()
{
    RC rc;

    m_pGCx = GetBoundGpuSubdevice()->GetGCxImpl();
    if (m_pGCx != nullptr)
    {
        if (!m_pGCx->IsGc6Supported(m_pGCx->GetGc6Pstate()))
        {
            Printf(Tee::PriNormal, "GC6 is not supported at current pstate.\n");
            m_pGCx = nullptr;
        }
        else
        {
            if (OK != m_pGCx->InitGc6(m_pTestConfig->TimeoutMs()))
                m_pGCx = nullptr;
        }
    }
    if (m_pGCx == nullptr)
    {
        Printf(Tee::PriNormal, "PanelCheck is unable to use GC6 - some subtests will fail.\n");
    }
    else
    {
        // LWSR system require GC6 exit post processing task
        m_pGCx->EnableGC6ExitPostProcessing(true);
    }

    CHECK_RC(GpuTest::Setup());

    CHECK_RC(GpuTest::AllocDisplay());
    m_pDisplay = GetDisplay();
    m_OrigDisplays = m_pDisplay->Selected();

    CHECK_RC(m_GpuRectFill.Initialize(m_pTestConfig));

    UINT32 numCRInstuments = 0;
    if ((OK != m_CRInstrument.FindInstruments(&numCRInstuments)) ||
        (numCRInstuments == 0))
    {
        Printf(Tee::PriNormal,
            "PanelCheck error: Photometric instrument not available - some subtests will fail.\n");
    }

    UINT32 numIRThermometers = 0;
    if (!m_NoThermometer &&
        ((OK != m_IRThermometer.FindInstruments(&numIRThermometers)) ||
        (numIRThermometers == 0)))
    {
        Printf(Tee::PriNormal,
            "PanelCheck is unable to find an IR thermometer - some subtests will fail.\n");
    }

    if (m_TestConfig.Verbose())
        m_PrintSubtestRC = true;

    if (!m_SkipGC6Testing)
    {
        UINT32 data = 0x0;
        if (OK != Registry::Read("ResourceManager", LW_REG_STR_RM_GC6_STATS, &data)
            || !FLD_TEST_DRF(_REG_STR_RM, _GC6_STATS, _ENABLED, _YES, data))
        {
            Printf(Tee::PriError, "PanelCheck must have \"-rmkey RMGC6Stats 0x1\" for GC6 testing.\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    return rc;
}

RC PanelCheck::Run()
{
    RC rc;
    StickyRC subtestsRc;
    bool stopOnError = GetGoldelwalues()->GetStopOnError();
    UINT32 panelID = m_OrigDisplays;
    Edid *modsEdid = m_pDisplay->GetEdidMember();

    UINT32 nativeWidth = 0;
    UINT32 nativeHeight = 0;
    UINT32 nativeRefresh = 0;

    Utility::CleanupOnReturnArg<Display, void, bool>
        RestoreUsePersistentClass073Handle(m_pDisplay,
            &Display::SetUsePersistentClass073Handle,
            m_pDisplay->GetUsePersistentClass073Handle());

    Tee::Priority subtestRCPrintPri = (m_PrintSubtestRC) ? Tee::PriNormal : Tee::PriLow;

    // Variadic macro would work better here, but we still have a compiler that doesn't support them
#define RunSubtestArg(subtestName, arglist)\
    {\
        Printf(subtestRCPrintPri, "Subtest: %-23s begin\n", #subtestName);\
        RC subtestName##RC = subtestName arglist;\
        ErrorMap subtestName##ErrorMap(subtestName##RC);\
        Printf(subtestRCPrintPri, "Subtest: %-23s returned %012d (%s)\n",\
               #subtestName, subtestName##RC.Get(), subtestName##ErrorMap.Message());\
        subtestsRc = subtestName##RC;\
        if (stopOnError)\
            CHECK_RC(subtestsRc);\
    }
#define RunSubtest(subtestName) RunSubtestArg(subtestName, ())
#define RunConditionalSubtest(condition, subtestName)\
    if (condition)\
    {\
        RunSubtest(subtestName);\
    }\
    else\
    {\
        Printf(subtestRCPrintPri, "Subtest: %-23s skipped\n", #subtestName);\
    }

    // The persistent handle is needed for VRR / crash sync setup.
    // RM deletes VRR resources when the handle is closed.
    // Can't use a locally allocated handle as the aux read/writes
    // also need it inside of Display:: and there can be only one
    // allocated at a time.
    m_pDisplay->SetUsePersistentClass073Handle(true);

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

    RunSubtestArg(EdidCheck, (lwtei));

    CHECK_RC(m_TCONDevice.Alloc(m_pDisplay, panelID));

    CHECK_RC(m_TCONDevice.UnlockBasicRegisters());

    RunSubtest(RegisterCheck);

    CHECK_RC(m_TCONDevice.InitPanel());

    LWT_TIMING lwtsr = {0};
    CHECK_RC(m_TCONDevice.InitSelfRefreshTiming(lwtei, &lwtsr));

    m_DisplaySurface.SetWidth(m_NativeRaster.ActiveX);
    m_DisplaySurface.SetHeight(m_NativeRaster.ActiveY);
    m_DisplaySurface.SetColorFormat(ColorUtils::ColorDepthToFormat(32));
    m_DisplaySurface.SetLayout(Surface2D::BlockLinear);
    m_DisplaySurface.SetDisplayable(true);
    CHECK_RC(m_DisplaySurface.Alloc(GetBoundGpuDevice()));

    CHECK_RC(m_GpuRectFill.SetSurface(&m_DisplaySurface));

    // GammaCheck and ColorCheck should be performed before the other subtests
    // to get confidence in the photometric measurements:
    RunSubtest(ColorCheck);

    RunSubtest(GammaCheck);

    RunConditionalSubtest(!m_SkipFlickerCheck, FlickerCheck);

    RunConditionalSubtest(!m_SkipSelfRefreshCheck, SelfRefreshCheck);

    RunSubtest(BrightnessCheck);

    return subtestsRc;
}

RC PanelCheck::SetModeAndImage(EvoRasterSettings *pEvoRasterSettings)
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

    Tasker::Sleep(100);

    return OK;
}

RC PanelCheck::FlickerCheck()
{
    Printf(Tee::PriLow, "Starting Flicker Check\n");

    RC rc;
    const UINT32 panelID = m_OrigDisplays;
    bool outOfBounds = false;

    CHECK_RC(m_GpuRectFill.Fill(0,
                                0,
                                m_DisplaySurface.GetWidth(),
                                m_DisplaySurface.GetHeight(),
                                0x808080,
                                true));

    CHECK_RC(SetModeAndImage(&m_NativeRaster));

    FLOAT64 nativeLuminance = 99999999.0;

    for (UINT32 modeIdx = 0; modeIdx < m_TestModes.size(); modeIdx++)
    {
        Printf(Tee::PriNormal, "Testing refresh rate = %.1f Hz\n",
            m_TestModes[modeIdx].RefreshRate);
        CHECK_RC(m_pDisplay->SetBackendVTotal(panelID, m_TestModes[modeIdx].VTotal));

        // Give time to the display to sync:
        Tasker::Sleep(m_DelayBeforeMeasurementMs);

        if (m_NoThermometer)
        {
            Printf(Tee::PriNormal, "   Panel Temperature = N/A\n");
        }
        else
        {
            FLOAT64 tempDegC = 0;
            CHECK_RC(m_IRThermometer.MeasureTemperature(m_IRThermometerIdx, &tempDegC));
            Printf(Tee::PriNormal, "   Panel Temperature = %.1f C\n", tempDegC);
        }

        FLOAT64 luminanceCdM2 = 0;
        CHECK_RC(m_CRInstrument.MeasureLuminance(m_CRInstrumentIdx, nullptr, &luminanceCdM2));
        Printf(Tee::PriNormal, "   Luminance         = %.2f cd/m2\n", luminanceCdM2);

        if (modeIdx == 0)
        {
            if (luminanceCdM2 < m_FlickerCheckMinimumLuminanceCdM2)
            {
                Printf(Tee::PriNormal,
                    "Error: Measured luminance of the panel in the Flicker subtest is too low.\n");
                return RC::UNEXPECTED_RESULT;
            }
            nativeLuminance = luminanceCdM2;
        }
        else
        {
            FLOAT64 diff = fabs(nativeLuminance - luminanceCdM2);
            FLOAT64 refreshRate = boost::math::round(m_TestModes[modeIdx].RefreshRate);
            if ((refreshRate < 30.0
                    && diff > (nativeLuminance * m_FlickerCheckMaximumLuminanceModeDeltaPercent))
                || (refreshRate >= 30.0
                        && diff > m_FlickerCheckMaximumLuminanceModeDeltaCdM2))
            {
                Printf(Tee::PriNormal,
                    "Error: Measured luminance of the panel in the Flicker subtest stretch mode is too low.\n");
                return RC::UNEXPECTED_RESULT;
            }
        }

        FLOAT64 flickerFrequency = 0;
        FLOAT64 flickerLevel = 0;
        FLOAT64 fma = 0;
        CHECK_RC(m_CRInstrument.MeasureFlicker(m_CRInstrumentIdx,
            m_MaximumSearchFrequency, m_LowPassFilterFrequency,
            m_FilterOrder,
            &flickerFrequency, &flickerLevel, &fma));

        Printf(Tee::PriNormal, "   Flicker Frequency = %.1f Hz\n",
            flickerFrequency);
        Printf(Tee::PriNormal, "   Flicker Level     = %.2f dB\n",
            flickerLevel);

        FLOAT64 refreshRate = round(m_TestModes[modeIdx].RefreshRate);
        if (m_TestModes[modeIdx].relaxedMode)
        {
            if (flickerLevel > -35.0)
            {
                outOfBounds = true;
                Printf(Tee::PriNormal,
                    "Error: Flicker level out of required spec (<= -35 dB) "
                    "for below flicker free range\n");
            }
        }
        else if (refreshRate < 35.0 && refreshRate >= 30.0)
        {
            if (flickerLevel > -43.0)
            {
                outOfBounds = true;
                Printf(Tee::PriNormal,
                    "Error: Flicker level out of required spec (<= -43 dB) "
                    "for refresh rate = [30,35) Hz\n");
            }
        }
        else
        {
            if (flickerLevel > -45.0)
            {
                outOfBounds = true;
                Printf(Tee::PriNormal,
                    "Error: Flicker level out of required spec (<= -45 dB) "
                    "for refresh rate >= 35 Hz\n");
            }
        }
    }

    if (outOfBounds)
        return RC::EXCEEDED_EXPECTED_THRESHOLD;

    return rc;
}

RC PanelCheck::AverageLuminance
(
    UINT32 numMeasurements,
    FLOAT64 *manualSyncFrequency,
    FLOAT64 *averageLuminanceCdM2
)
{
    RC rc;

    *averageLuminanceCdM2 = 0;
    for (UINT32 measIdx = 0; measIdx < numMeasurements; measIdx++)
    {
        FLOAT64 luminanceCdM2 = 0;
        CHECK_RC(m_CRInstrument.MeasureLuminance(m_CRInstrumentIdx,
            manualSyncFrequency, &luminanceCdM2));
        *averageLuminanceCdM2 += luminanceCdM2;
    }
    *averageLuminanceCdM2 /= numMeasurements;

    return OK;
}

FLOAT64 PanelCheck::CallwlateBorderLuminance
(
    FLOAT64 minLuminance,
    FLOAT64 relativeLuminance,
    UINT32 pixelLevel,
    FLOAT64 borderGamma,
    FLOAT64 expectedIdealLuminance,
    bool callwlateMin
)
{
    FLOAT64 borderLuminance = minLuminance + relativeLuminance *
        pow(pixelLevel/255.0, borderGamma);

    if (callwlateMin)
    {
        if ((expectedIdealLuminance - borderLuminance) < m_LuminanceMeasurementToleranceCdM2)
            borderLuminance = expectedIdealLuminance - m_LuminanceMeasurementToleranceCdM2;
    }
    else
    {
        if ((borderLuminance - expectedIdealLuminance) < m_LuminanceMeasurementToleranceCdM2)
            borderLuminance = expectedIdealLuminance + m_LuminanceMeasurementToleranceCdM2;
    }

    return borderLuminance;
}

RC PanelCheck::ColorCheck()
{
    Printf(Tee::PriLow, "Starting Color Check\n");
    RC rc;
    bool outOfBounds = false;

    CHECK_RC(SetModeAndImage(&m_NativeRaster));

    const UINT32 panelID = m_OrigDisplays;

    for (UINT32 modeIdx = 0; modeIdx < m_TestModes.size(); modeIdx++)
    {
        Printf(Tee::PriNormal, "Testing refresh rate = %.1f Hz\n", m_TestModes[modeIdx].RefreshRate);
        CHECK_RC(m_pDisplay->SetBackendVTotal(panelID, m_TestModes[modeIdx].VTotal));

        FLOAT64 xyMeasurementSyncFrequency = 0.0; // It will be determined automatically.
                                                  // Needs to be reset to 0 for every mode.

        for (UINT32 colorIdx = 0; colorIdx < m_ColorResponsePoints.size(); colorIdx++)
        {
            CHECK_RC(m_GpuRectFill.Fill(0,
                                        0,
                                        m_DisplaySurface.GetWidth(),
                                        m_DisplaySurface.GetHeight(),
                                        m_ColorResponsePoints[colorIdx].PixelValue,
                                        true));

            Tasker::Sleep(m_DelayBeforeMeasurementMs);

            FLOAT32 measuredx = 0;
            FLOAT32 measuredy = 0;
            CHECK_RC(m_CRInstrument.MeasureCIE1931xy(m_CRInstrumentIdx,
                &xyMeasurementSyncFrequency, &measuredx, &measuredy));

            FLOAT64 deltax = measuredx - m_ColorResponsePoints[colorIdx].ExpectedCIE1931x;
            FLOAT64 deltay = measuredy - m_ColorResponsePoints[colorIdx].ExpectedCIE1931y;
            FLOAT64 sumOfSquares = deltax*deltax + deltay*deltay;
            FLOAT64 distance = 0.0;
            if (sumOfSquares > 0.0)
                distance = sqrt(sumOfSquares);

            // Refresh rates <30Hz just need to be checked for sanity and for any catastrophic failures
            FLOAT64 multiplier = (m_TestModes[modeIdx].RefreshRate < 30) ? 3.0 : 1.0;

            if (distance > m_CIE1931CoordinatesToleranceRequired * multiplier)
            {
                outOfBounds = true;
                Printf(Tee::PriNormal,
                       "Error: Measured chromaticity coordinates (x=%.3f, y=%.3f) "
                       "for color %s are at a too far distance (%.3f > %.3f) "
                       "from expected (x=%.3f, y=%.3f)\n",
                       measuredx,
                       measuredy,
                       m_ColorResponsePoints[colorIdx].ColorName.c_str(),
                       distance,
                       m_CIE1931CoordinatesToleranceRequired,
                       m_ColorResponsePoints[colorIdx].ExpectedCIE1931x,
                       m_ColorResponsePoints[colorIdx].ExpectedCIE1931y);
            }
            else if (distance > m_CIE1931CoordinatesToleranceCompetitive * multiplier)
            {
                Printf(Tee::PriNormal,
                       "Warning: Measured chromaticity coordinates (x=%.3f, y=%.3f) "
                       "for color %s are outside of the competitive distance (%.3f > %.3f) "
                       "from expected (x=%.3f, y=%.3f)\n",
                       measuredx,
                       measuredy,
                       m_ColorResponsePoints[colorIdx].ColorName.c_str(),
                       distance,
                       m_CIE1931CoordinatesToleranceCompetitive,
                       m_ColorResponsePoints[colorIdx].ExpectedCIE1931x,
                       m_ColorResponsePoints[colorIdx].ExpectedCIE1931y);
            }
        }
    }

    if (outOfBounds)
        return RC::EXCEEDED_EXPECTED_THRESHOLD;

    return rc;
}

static UINT32 PixelLevelMask2PixelValue(UINT32 pixelLevel, UINT32 pixelMask)
{
    return ((pixelLevel<<16) + (pixelLevel<<8) + pixelLevel) & pixelMask;
}

RC PanelCheck::GammaCheck()
{
    Printf(Tee::PriLow, "Starting Gamma Check\n");
    RC rc;
    bool outOfBounds = false;

    CHECK_RC(SetModeAndImage(&m_NativeRaster));

    const UINT32 panelID = m_OrigDisplays;

    map<UINT32, FLOAT64> firstModeLuminances;

    for (UINT32 modeIdx = 0; modeIdx < m_TestModes.size(); modeIdx++)
    {
        Printf(Tee::PriNormal, "Testing refresh rate = %.1f Hz\n", m_TestModes[modeIdx].RefreshRate);
        CHECK_RC(m_pDisplay->SetBackendVTotal(panelID, m_TestModes[modeIdx].VTotal));

        FLOAT64 luminanceSyncFrequency = 0.0; // It will be determined automatically.
                                              // Needs to be reset to 0 for every mode.

        const UINT32 pixelMask[] = { 0xFFFFFF, 0x0000FF, 0x00FF00, 0xFF0000};
        size_t numPixelMasks = NUMELEMS(pixelMask);
        if (m_OnlyGrayGamma || m_TestModes[modeIdx].reducedMode)
            numPixelMasks = 1;

        set<UINT32> limitedPixelIdxes;
        if (!m_FullRangeGamma || m_TestModes[modeIdx].reducedMode)
        {
            limitedPixelIdxes.insert(0);
            limitedPixelIdxes.insert(16);
            limitedPixelIdxes.insert(32);
            limitedPixelIdxes.insert(64);
            limitedPixelIdxes.insert(128);
            limitedPixelIdxes.insert(192);
            limitedPixelIdxes.insert(255);
        }

        for (UINT32 pixelMaskIdx = 0; pixelMaskIdx < numPixelMasks; pixelMaskIdx++)
        {
            map<UINT32, FLOAT64> luminances;

            // Start with the highest level so that the sync frequency is correctly determined:
            for (INT32 pixelValueIdx = 255; pixelValueIdx >= 0; pixelValueIdx--)
            {
                if (limitedPixelIdxes.size() &&
                    (limitedPixelIdxes.find(pixelValueIdx) == limitedPixelIdxes.end()))
                    continue;

                UINT32 numMeasurements = 1;
                if ((pixelValueIdx == 0) || (pixelValueIdx == 255))
                {
                    // The end points will be used to callwlate all the points
                    // in between so measure them carefully:
                    numMeasurements = m_NumBorderGammaMeasurements;
                }

                UINT32 pixelValue = PixelLevelMask2PixelValue(pixelValueIdx,
                    pixelMask[pixelMaskIdx]);

                CHECK_RC(m_GpuRectFill.Fill(0,
                                            0,
                                            m_DisplaySurface.GetWidth(),
                                            m_DisplaySurface.GetHeight(),
                                            pixelValue,
                                            true));

                Tasker::Sleep(m_DelayBeforeMeasurementMs);

                FLOAT64 averagedLuminanceCdM2 = 0;
                CHECK_RC(AverageLuminance(numMeasurements, &luminanceSyncFrequency,
                    &averagedLuminanceCdM2));
                luminances[pixelValueIdx] = averagedLuminanceCdM2;

                if (modeIdx == 0)
                {
                    firstModeLuminances[pixelValue] = averagedLuminanceCdM2;
                }
            }

            if (luminances.find(0) == luminances.end())
            {
                Printf(Tee::PriNormal, "Error: Missing luminance measurement of pixel level 0.\n");
                return RC::SOFTWARE_ERROR;
            }
            if (luminances.find(255) == luminances.end())
            {
                Printf(Tee::PriNormal, "Error: Missing luminance measurement of pixel level 255.\n");
                return RC::SOFTWARE_ERROR;
            }

            FLOAT64 minLuminance = luminances[0];
            FLOAT64 maxLuminance = luminances[255];
            FLOAT64 relativeLuminance = maxLuminance - minLuminance;
            if (relativeLuminance < m_MinimumRelativeLuminance)
            {
                Printf(Tee::PriNormal,
                    "Error: Too small luminance delta between 0 and 255 pixel values.\n");
                return RC::UNEXPECTED_RESULT;
            }

            Tee::Priority tablePriority = m_ShowGamma ? Tee::PriNormal : Tee::PriLow;

            const char gammaTableDescription[] =
                "   Pixel value | MeasLum |   rMin   cMin Expected Ideal   cMax   rMax\n";
            Printf(tablePriority, gammaTableDescription);
            bool desciptionPrintedAtNormal = tablePriority >= Tee::PriNormal;

            for (map<UINT32, FLOAT64>::const_iterator luminancesIter = luminances.begin();
                 luminancesIter != luminances.end() && !m_TestModes[modeIdx].relaxedMode;
                 luminancesIter++)
            {
                UINT32 pixelLevel = luminancesIter->first;
                FLOAT64 luminance = luminancesIter->second;

                FLOAT64 expectedIdeal = minLuminance + relativeLuminance *
                    pow(pixelLevel/255.0, m_ExpectedGamma);

                FLOAT64 requiredLuminanceMin = CallwlateBorderLuminance(minLuminance,
                    relativeLuminance, pixelLevel,
                    m_ExpectedGamma + m_RequiredGammaTolarance, expectedIdeal, true);

                FLOAT64 competitiveLuminanceMin = CallwlateBorderLuminance(minLuminance,
                    relativeLuminance, pixelLevel,
                    m_ExpectedGamma + m_CompetitiveGammaTolerance, expectedIdeal, true);

                FLOAT64 requiredLuminanceMax = CallwlateBorderLuminance(minLuminance,
                    relativeLuminance, pixelLevel,
                    m_ExpectedGamma - m_RequiredGammaTolarance, expectedIdeal, false);

                FLOAT64 competitiveLuminanceMax = CallwlateBorderLuminance(minLuminance,
                    relativeLuminance, pixelLevel,
                    m_ExpectedGamma - m_CompetitiveGammaTolerance, expectedIdeal, false);

                bool outsideRequired = (luminance < requiredLuminanceMin) ||
                                       (luminance > requiredLuminanceMax);
                bool outsideCompetitive = (luminance < competitiveLuminanceMin) ||
                                          (luminance > competitiveLuminanceMax);

                Tee::Priority entryPriority = tablePriority;

                string comments = "\n";
                if (outsideRequired)
                {
                    comments = "   WARNING: gamma level outside of required range!\n";
                    entryPriority = Tee::PriNormal;
                }
                else if (outsideCompetitive)
                {
                    comments = "   WARNING: gamma level outside of comeptitive range!\n";
                    entryPriority = Tee::PriNormal;
                }
                if (!desciptionPrintedAtNormal && (outsideRequired || outsideCompetitive))
                {
                    Printf(Tee::PriNormal, gammaTableDescription);
                    desciptionPrintedAtNormal = true;
                }

                Printf(entryPriority,
                    "      0x%06x |  %6.2f | %6.2f %6.2f     %6.2f     %6.2f %6.2f%s",
                    PixelLevelMask2PixelValue(pixelLevel, pixelMask[pixelMaskIdx]),
                    luminance,
                    requiredLuminanceMin,
                    competitiveLuminanceMin,
                    expectedIdeal,
                    competitiveLuminanceMax,
                    requiredLuminanceMax,
                    comments.c_str());
            }

            if (modeIdx != 0)
            {
                for (map<UINT32, FLOAT64>::const_iterator luminancesIter = luminances.begin();
                     luminancesIter != luminances.end();
                     luminancesIter++)
                {
                    UINT32 pixelValue = PixelLevelMask2PixelValue(luminancesIter->first,
                        pixelMask[pixelMaskIdx]);

                    FLOAT64 luminance = luminancesIter->second;
                    FLOAT64 firstModeLuminance = firstModeLuminances[pixelValue];

                    if (fabs(firstModeLuminance - luminance) >
                        (m_GammaToleranceAtLowerRefreshCdM2 +
                         (firstModeLuminance * (m_GammaToleranceAtLowerRefreshPercent/100.0))))
                    {
                        outOfBounds = true;
                        Printf(Tee::PriNormal,
                            "Error: Measured luminance = %.2f cd/m2 for pixel value = 0x%06x"
                            " at refresh rate %.1f Hz is too far from luminance = %.2f cd/m2"
                            " measured at refresh rate %.1f Hz\n",
                            luminance,
                            pixelValue,
                            m_TestModes[modeIdx].RefreshRate,
                            firstModeLuminance,
                            m_TestModes[0].RefreshRate);
                    }
                }
            }
        }
    }

    if (outOfBounds)
        return RC::EXCEEDED_EXPECTED_THRESHOLD;

    return OK;
}

RC PanelCheck::CheckMinimalLuminance()
{
    RC rc;
    FLOAT64 luminanceCdM2 = 0;
    CHECK_RC(m_CRInstrument.MeasureLuminance(m_CRInstrumentIdx, nullptr, &luminanceCdM2));
    if (luminanceCdM2 < m_MinimalLuminanceCdM2)
    {
        Printf(Tee::PriNormal,
            "Error: Measured luminance %.1f Cd/M2 of the panel is too low.\n",
            luminanceCdM2);
        return RC::UNEXPECTED_RESULT;
    }
    return OK;
}

struct MaxUpdateTimes
{
    UINT32 MaxUpdateTimeMs;
    UINT32 MaxSyncTimeMs;
};

void static ProcessUpdateTime
(
    UINT64 UpdateStartTime,
    UINT64 CachingStartTime,
    UINT64 UpdateEndTime,
    MaxUpdateTimes *pMaxUpdateTimes
)
{
    UINT32 updateTime = UINT32(UpdateEndTime - UpdateStartTime);
    UINT32 syncTime = 999; // If for some reason the caching start time was
                           // not captured (CachingStartTime == 0) then return
                           // a high syncTime value to fail any checks made on
                           // that value (they should be in range of 2 * 16ms).
    if (CachingStartTime != 0)
    {
        syncTime = UINT32(CachingStartTime - UpdateStartTime);
    }

    Printf(Tee::PriLow, "Update time = %d ms, sync time = %d ms\n",
        updateTime, syncTime);

    if (updateTime > pMaxUpdateTimes->MaxUpdateTimeMs)
        pMaxUpdateTimes->MaxUpdateTimeMs = updateTime;
    if (syncTime > pMaxUpdateTimes->MaxSyncTimeMs)
        pMaxUpdateTimes->MaxSyncTimeMs = syncTime;
}

RC PanelCheck::RestoreStallLock()
{
    RC rc;

    CHECK_RC(m_pDisplay->SetStallLock(m_OrigDisplays, false, 0));
    CHECK_RC(m_pDisplay->SetImage(&m_DisplaySurface));

    return rc;
}

RC PanelCheck::RestoreSlaveLock()
{
    RC rc;
    CHECK_RC(m_pDisplay->SetHeadSetControlSlaveLock(m_OrigDisplays, false, 0));
    return rc;
}

// Assumes that the burst mode is already enabled:
RC PanelCheck::MeasureCrashSyncDelays
(
    FLOAT64 panelRefreshRate,
    EvoRasterSettings *burstRaster
)
{
    RC rc;
    StickyRC lumRC;

    LW0073_CTRL_SYSTEM_GET_VRR_CONFIG_PARAMS vrrConfig = {0};
    vrrConfig.subDeviceInstance = GetBoundGpuSubdevice()->GetSubdeviceInst();
    CHECK_RC_MSG(m_pDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_GET_VRR_CONFIG,
        &vrrConfig, sizeof(vrrConfig)), "Unable to get VRR config\n");

    // SetModeAndImage() includes DetachAllDisplays()
    CHECK_RC(SetModeAndImage(burstRaster));
    // The stall takes affect after a "flip" - SetImage
    CHECK_RC(m_pDisplay->SetStallLock(m_OrigDisplays, true, vrrConfig.frameLockPin));
    Utility::CleanupOnReturn<PanelCheck, RC>
        restoreStallLock(this, &PanelCheck::RestoreStallLock);

    CHECK_RC(m_pDisplay->SetImage(&m_DisplaySurface));

    Printf(Tee::PriLow, "Starting crash sync update time measurements:\n");

    MaxUpdateTimes maxUpdateTimes = {0, 0};

    const UINT32 NUM_CRASH_SYNC_TIME_MEASUREMENTS = 30;

    for (UINT32 measurementIdx = 0;
         measurementIdx < NUM_CRASH_SYNC_TIME_MEASUREMENTS;
         measurementIdx++)
    {
        CHECK_RC(m_GpuRectFill.Fill(0,
                                    0,
                                    m_DisplaySurface.GetWidth(),
                                    m_DisplaySurface.GetHeight(),
                                    (measurementIdx & 1) ? 0xffff00 : 0xff00ff,
                                    true));

        CHECK_RC(m_TCONDevice.WaitForNewSRFrame());

        UINT64 startTime = Xp::GetWallTimeMS();

        LW0073_CTRL_SYSTEM_FORCE_VRR_RELEASE_PARAMS params = {0};
        params.subDeviceInstance = GetBoundGpuSubdevice()->GetSubdeviceInst();
        params.waitForFlipMs = 0; // ASAP
        // This RM Ctrl will write DPCD registers 0x340, 0x366
        CHECK_RC_MSG(m_pDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_FORCE_VRR_RELEASE,
            &params, sizeof(params)), "Unable to perform VRR release\n");

        //
        // Somehow RM write DPCD register 0x340, 0x366 late than we check
        // SR_STATUS.  So, wait a while, then check SR_STATUS.
        //
        Tasker::Sleep(5.0);

        UINT64 cachingStartedTimeStampMs = 0;
        CHECK_RC(m_TCONDevice.WaitForCrashSyncSRState(&cachingStartedTimeStampMs));

        UINT64 endTime = Xp::GetWallTimeMS();

        if (measurementIdx == 0)
        {
            lumRC = CheckMinimalLuminance();
        }

        ProcessUpdateTime(startTime, cachingStartedTimeStampMs, endTime, &maxUpdateTimes);
    }
    Printf(Tee::PriLow, "Max entire update time = %d ms, max sync time = %d ms.\n",
        maxUpdateTimes.MaxUpdateTimeMs, maxUpdateTimes.MaxSyncTimeMs);

    restoreStallLock.Cancel();
    CHECK_RC(RestoreStallLock());

    UINT32 maxAllowedCrashSynlwpdateTimeMs =
        UINT32(m_MaxNumFramesForCrashSynlwpdate * 1000.0 / panelRefreshRate);
    if (maxUpdateTimes.MaxUpdateTimeMs > maxAllowedCrashSynlwpdateTimeMs)
    {
        Printf(Tee::PriNormal,
            "Error: Max measured crash sync update = %d ms larger than allowed = %d ms.\n",
            maxUpdateTimes.MaxUpdateTimeMs, maxAllowedCrashSynlwpdateTimeMs);
        return RC::UNEXPECTED_RESULT;
    }

    UINT32 maxAllowedCrashSyncTimeMs =
        UINT32(m_MaxNumFramesForCrashSync * 1000.0 / panelRefreshRate);
    if (maxUpdateTimes.MaxSyncTimeMs > maxAllowedCrashSyncTimeMs)
    {
        Printf(Tee::PriNormal,
            "Error: Max measured crash sync time = %d ms larger than the allowed = %d ms.\n",
            maxUpdateTimes.MaxSyncTimeMs, maxAllowedCrashSyncTimeMs);
        return RC::UNEXPECTED_RESULT;
    }

    return lumRC;
}

RC PanelCheck::MeasureBurstDelays
(
    FLOAT64 panelRefreshRate,
    EvoRasterSettings *burstRaster
)
{
    RC rc;
    MaxUpdateTimes maxUpdateTimes = {0, 0};

    Printf(Tee::PriLow, "Starting burst update time measurements:\n");

    const UINT32 NUM_BURST_UPDATE_MEASUREMENTS = 30;

    for (UINT32 measurementIdx = 0;
         measurementIdx < NUM_BURST_UPDATE_MEASUREMENTS;
         measurementIdx++)
    {
        CHECK_RC(m_GpuRectFill.Fill(0,
                                    0,
                                    m_DisplaySurface.GetWidth(),
                                    m_DisplaySurface.GetHeight(),
                                    (measurementIdx&1) ? 0x00ff00 : 0x00ffff,
                                    true));

        CHECK_RC(m_TCONDevice.WaitForNewSRFrame());

        UINT64 cachingStartedTimeStampMs = 0;
        UINT64 startTime = Xp::GetWallTimeMS();
        CHECK_RC(m_TCONDevice.CacheBurstFrame(&cachingStartedTimeStampMs));
        UINT64 endTime = Xp::GetWallTimeMS();

        ProcessUpdateTime(startTime, cachingStartedTimeStampMs, endTime, &maxUpdateTimes);
    }
    Printf(Tee::PriLow, "Max entire update time = %d ms, max sync time = %d ms.\n",
        maxUpdateTimes.MaxUpdateTimeMs, maxUpdateTimes.MaxSyncTimeMs);

    UINT32 maxAllowedBurstUpdateTimeMs =
        UINT32(m_MaxNumFramesForBurstUpdate * 1000.0 / panelRefreshRate);
    if (maxUpdateTimes.MaxUpdateTimeMs > maxAllowedBurstUpdateTimeMs)
    {
        Printf(Tee::PriNormal,
            "Error: Max measured burst update = %d ms larger than the allowed = %d ms.\n",
            maxUpdateTimes.MaxUpdateTimeMs, maxAllowedBurstUpdateTimeMs);
        return RC::UNEXPECTED_RESULT;
    }

    return CheckMinimalLuminance();
}

RC PanelCheck::SelfRefreshCheck()
{
    Printf(Tee::PriLow, "Starting Self Refresh Check\n");
    Tee::Priority pri = GetVerbosePrintPri();
    RC rc;

    CHECK_RC(m_GpuRectFill.Fill(0,
                                0,
                                m_DisplaySurface.GetWidth(),
                                m_DisplaySurface.GetHeight(),
                                0xff0000,
                                true));

    CHECK_RC(SetModeAndImage(&m_NativeRaster));

    StickyRC lumRC = CheckMinimalLuminance();

    Printf(pri, "Enter Sparse mode\n");
    CHECK_RC(m_TCONDevice.CacheFrame(nullptr)); // Enables the SR mode at the same time
    Utility::CleanupOnReturn<TCONDevice, RC>
        exitSR(&m_TCONDevice, &TCONDevice::ExitSelfRefresh);

    CHECK_RC(m_GpuRectFill.Fill(0,
                                0,
                                m_DisplaySurface.GetWidth(),
                                m_DisplaySurface.GetHeight(),
                                0x000000,
                                true));

    lumRC = CheckMinimalLuminance();

    CHECK_RC(m_GpuRectFill.Fill(0,
                                0,
                                m_DisplaySurface.GetWidth(),
                                m_DisplaySurface.GetHeight(),
                                0x0000ff,
                                true));

    //
    // The Detach is needed because the TCON powers-down its receiver sometime
    // after entering SR. The burst SetMode below performs link training again.
    //
    CHECK_RC(m_pDisplay->DetachAllDisplays());

    Printf(pri, "Change GC-SRC pixel clock\n");
    //
    // TODO: We may need to remove CallwlateBurstRaster() because TCON DPCD
    // register already report the max pixel clock which panel can support.
    // And we directly use the value from DPCD register in MODS209.
    //
    EvoRasterSettings burstRaster;
    const UINT32 MINIMUM_BURST_PIXEL_CLOCK = 60000000;
    CHECK_RC(m_TCONDevice.CallwlateBurstRaster(&m_NativeRaster, MINIMUM_BURST_PIXEL_CLOCK, &burstRaster));

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

    // When in burst mode, the panel will be refreshing at its lowest refresh rate
    FLOAT64 panelRefreshRate = m_TestModes[m_TestModes.size()-1].RefreshRate;

    Printf(pri, "Enable Frame lock to slave lock\n");
    LW0073_CTRL_SYSTEM_GET_VRR_CONFIG_PARAMS vrrConfig = {0};
    vrrConfig.subDeviceInstance = GetBoundGpuSubdevice()->GetSubdeviceInst();
    CHECK_RC_MSG(m_pDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_GET_VRR_CONFIG,
        &vrrConfig, sizeof(vrrConfig)), "Unable to get VRR config\n");
    CHECK_RC(m_pDisplay->SetHeadSetControlSlaveLock(m_OrigDisplays, true, vrrConfig.frameLockPin));
    Utility::CleanupOnReturn<PanelCheck, RC>
        restoreSlaveLock(this, &PanelCheck::RestoreSlaveLock);

    CHECK_RC(SetModeAndImage(&burstRaster));
    Utility::CleanupOnReturnArg<PanelCheck, RC, EvoRasterSettings*>
        resetMode(this, &PanelCheck::SetModeAndImage, &m_NativeRaster);

    Printf(pri, "Enter Burst mode\n");
    CHECK_RC(m_TCONDevice.WriteBurstTimingRegisters(&burstRaster));

    CHECK_RC(m_TCONDevice.EnableBufferedBurstRefreshMode());
    Utility::CleanupOnReturn<TCONDevice, RC>
        disableBurst(&m_TCONDevice, &TCONDevice::DisableBufferedBurstRefreshMode);
    Printf(pri, "Set resync delay = 1 for non-VRR mode\n");
    const UINT08 RESYNC_DELAY_BURST_1 = 1;
    CHECK_RC(m_TCONDevice.SetupResync(RESYNC_DELAY_BURST_1));

    lumRC = CheckMinimalLuminance();

    //
    // The Detach and SetMode are needed because the TCON powers-down
    // its receiver sometime after entering SR. SetMode performs link training.
    // SetModeAndImage() includes DetachAllDisplays()
    //
    CHECK_RC(SetModeAndImage(&burstRaster));

    Printf(pri, "Burst single frame update\n");
    CHECK_RC(m_TCONDevice.CacheBurstFrame(nullptr));

    lumRC = CheckMinimalLuminance();

    //
    // The Detach and SetMode are needed because the TCON powers-down
    // its receiver sometime after entering SR. SetMode performs link training.
    // SetModeAndImage() includes DetachAllDisplays()
    //
    CHECK_RC(SetModeAndImage(&burstRaster));

    Printf(pri, "Measure Burst Delay\n");
    CHECK_RC(MeasureBurstDelays(panelRefreshRate, &burstRaster));

    Printf(pri, "Disable Frame lock to slave lock\n");
    restoreSlaveLock.Cancel();
    CHECK_RC(RestoreSlaveLock());

    Printf(pri, "Set resync delay = 0 for enable VRR\n");
    const UINT08 RESYNC_DELAY_BURST_0 = 0;
    CHECK_RC(m_TCONDevice.SetupResync(RESYNC_DELAY_BURST_0));

    Printf(pri, "Enable Vrr\n");
    CHECK_RC(m_TCONDevice.EnableVrr());
    Utility::CleanupOnReturn<TCONDevice, RC>
        disableVrr(&m_TCONDevice, &TCONDevice::DisableVrr);

    Printf(pri, "Measure Crash Sync Delay\n");
    CHECK_RC(MeasureCrashSyncDelays(panelRefreshRate, &burstRaster));

    Printf(pri, "Exit Burst mode (disable burst mode)\n");
    resetMode.Cancel();
    CHECK_RC(SetModeAndImage(&m_NativeRaster));
    disableBurst.Cancel();
    CHECK_RC(m_TCONDevice.DisableBufferedBurstRefreshMode());

    Printf(pri, "Disable Vrr\n");
    disableVrr.Cancel();
    CHECK_RC(m_TCONDevice.DisableVrr());

    lumRC = CheckMinimalLuminance();

    CHECK_RC(m_GpuRectFill.Fill(0,
                                0,
                                m_DisplaySurface.GetWidth(),
                                m_DisplaySurface.GetHeight(),
                                0x00ff00,
                                true));

    Printf(pri, "GC6 loop test\n");

    if (m_SkipGC6Testing)
    {
        Printf(Tee::PriNormal, "Skipping GC6 Testing\n");
    }
    else
    {
        for (UINT32 loop = 0; loop < m_SelfRefreshCheckGC6Iterations; loop++)
        {
            CHECK_RC(m_pDisplay->DetachAllDisplays());
            Printf(pri, "Enter GC6\n");
            CHECK_RC(m_pGCx->DoEnterGc6(GC_CMN_WAKEUP_EVENT_gc6gpu, 0));

            lumRC = CheckMinimalLuminance();

            Printf(pri, "Exit GC6\n");
            CHECK_RC(m_pGCx->DoExitGc6(0, false));

            CHECK_RC(m_pDisplay->SetHeadSetControlSlaveLock(m_OrigDisplays, true, vrrConfig.frameLockPin));
            restoreSlaveLock.Activate();

            CHECK_RC(SetModeAndImage(&m_NativeRaster));

            Printf(pri, "Exit Sparse mode for GC6-exit\n");
            CHECK_RC(m_TCONDevice.ExitSelfRefresh());

            Printf(pri, "Enter Sparse mode for GC6-enter\n");
            CHECK_RC(m_TCONDevice.SelectSREntryMethod());
            CHECK_RC(m_TCONDevice.CacheFrame(nullptr)); // Enables the SR mode at the same time
        }

        CHECK_RC(CheckNLTTransition());
    }

    CHECK_RC(SetModeAndImage(&m_NativeRaster));

    exitSR.Cancel();
    CHECK_RC(m_TCONDevice.ExitSelfRefresh());

    restoreSlaveLock.Cancel();
    CHECK_RC(RestoreSlaveLock());

    lumRC = CheckMinimalLuminance();

    CHECK_RC(lumRC);

    return rc;
}

RC PanelCheck::CheckNLTTransition()
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
        sizeof(params)
    ));

    if (params.list[0].param.flag != LW2080_CTRL_LPWR_FEATURE_PARAMETER_FLAG_SUCCEED_YES)
    {
        Printf(Tee::PriError, "Failed to get max NLT time!\n");
        return RC::LWRM_ERROR;
    }

    UINT32 nltMaxUS = params.list[LW2080_CTRL_LPWR_GC6_NLT_MAX_US].param.val;

    Printf(Tee::PriNormal, "GC6 NLT Transition = %dus\n", nltMaxUS);

    if (nltMaxUS == 0)
    {
        return RC::SOFTWARE_ERROR;
    }

    if (nltMaxUS > m_MaxGC6NLTTransitionUS)
    {
        // Just print a warning for now. Once the spec is fully defined
        // it will probably become an error.
        Printf(Tee::PriWarn, "GC6 NLT Transition exceeds %dus!\n",
               m_MaxGC6NLTTransitionUS);
    }

    return OK;
}

static bool IsThisVStretch
(
    const LWT_TIMING *tBase,
    const LWT_TIMING *tStretch,
    bool *wrongRefreshOrder
)
{
    if (tBase->pclk != tStretch->pclk)
        return false;

    if (tBase->HVisible != tStretch->HVisible)
        return false;
    if (tBase->HBorder != tStretch->HBorder)
        return false;
    if (tBase->HFrontPorch != tStretch->HFrontPorch)
        return false;
    if (tBase->HSyncWidth != tStretch->HSyncWidth)
        return false;
    if (tBase->HTotal != tStretch->HTotal)
        return false;
    if (tBase->HSyncPol != tStretch->HSyncPol)
        return false;

    if (tBase->VVisible != tStretch->VVisible)
        return false;
    if (tBase->VBorder != tStretch->VBorder)
        return false;
    if (tBase->VSyncWidth != tStretch->VSyncWidth)
        return false;
    if (tBase->VSyncPol != tStretch->VSyncPol)
        return false;

    if (tBase->interlaced != tStretch->interlaced)
        return false;

    if (tBase->VTotal >= tStretch->VTotal)
    {
        *wrongRefreshOrder = true;
        return false;
    }

    return true;
}

static void DetectVStretchDTDs
(
    const LWT_EDID_INFO &lwtei,
    vector<UINT32> *stretchIdxs,
    bool *wrongRefreshOrder
)
{
    stretchIdxs->clear();
    *wrongRefreshOrder = false;

    UINT32 baseIdx = 0;
    for (UINT32 timingIdx = 1; timingIdx < lwtei.total_timings; timingIdx++)
    {
        if (IsThisVStretch(&lwtei.timing[baseIdx], &lwtei.timing[timingIdx],
            wrongRefreshOrder))
        {
            stretchIdxs->push_back(timingIdx);
            baseIdx = timingIdx;
        }
    }
}

RC PanelCheck::EdidCheck(const LWT_EDID_INFO &lwtei)
{
    Printf(Tee::PriLow, "Starting EDID Check\n");
    RC rc;
    bool outOfSpec = false;
    m_TestModes.resize(1); // The first entry should be the native mode

    Printf(Tee::PriNormal, "    $ Panel Manuf. ID  : %#x\n", lwtei.manuf_id);
    Printf(Tee::PriNormal, "    $ Panel Product ID : %#x\n", lwtei.product_id);
    Printf(Tee::PriNormal, "    $ Panel Version    : %#x\n", lwtei.version);
    // $ is used as a uniqifier to make grepping easier and more reliable

    if (lwtei.total_timings < 2)
    {
        Printf(Tee::PriNormal,
            "Error: Expected at least two DTDs in EDID, but found %d.\n",
            (UINT32)lwtei.total_timings);
        outOfSpec = true;
    }

    vector<UINT32> stretchIdxs;
    bool wrongRefreshOrder = false;
    DetectVStretchDTDs(lwtei, &stretchIdxs, &wrongRefreshOrder);
    if (stretchIdxs.size() == 0)
    {
        Printf(Tee::PriNormal,
            "Error: Expected at least one V-Stretched DTD in EDID, but found "
            "none.\n");
        outOfSpec = true;
    }

    bool lowestRefreshAlreadyPresent = false;
    for (size_t stretchModeIdx = 0; stretchModeIdx < stretchIdxs.size(); stretchModeIdx++)
    {
        EvoRasterSettings stretchRaster;
        CHECK_RC(ModesetUtils::ColwertLwtTiming2EvoRasterSettings(
            lwtei.timing[stretchIdxs[stretchModeIdx]], &stretchRaster));
        TestMode stretchTestMode = {stretchRaster.RasterHeight,
                                    stretchRaster.CallwlateRefreshRate(),
                                    false, false};
        // Bug 1713505:
        if (m_OnlyEdidModes && stretchTestMode.RefreshRate <= 30.0)
        {
            Printf(Tee::PriLow,
                "PanelCheck: Refresh rate = %.1f Hz (VTotal = %d) capped to 30 Hz\n",
                stretchTestMode.RefreshRate,
                stretchTestMode.VTotal);

            if (lowestRefreshAlreadyPresent)
                continue;

            stretchTestMode.VTotal = static_cast<UINT32>(stretchRaster.PixelClockHz /
                (30.0 * stretchRaster.RasterWidth));
            stretchTestMode.RefreshRate = 30.0;
            lowestRefreshAlreadyPresent = true;
        }

        if (round(stretchTestMode.RefreshRate) < 30.0)
        {
            stretchTestMode.reducedMode = true;
            stretchTestMode.relaxedMode = true;
        }

        m_TestModes.push_back(stretchTestMode);
    }

    // The values 2.0 and 2.4 come from the LWSR spec item P10, Refresh Requirements.
    const FLOAT64 flickerFreeMinRefeshRate = round(m_TestModes[0].RefreshRate / 2.0);
    const FLOAT64 tconLogicMinRefreshRate  = round(m_TestModes[0].RefreshRate / 2.4);

    const FLOAT64 refreshRateInterval = 5.0;
    FLOAT64 refreshRate = round(m_TestModes[0].RefreshRate) - refreshRateInterval;
    for (vector<TestMode>::iterator testModeItr = m_TestModes.begin()+1;//start at second test mode
         refreshRate >= tconLogicMinRefreshRate && !m_OnlyEdidModes; testModeItr++)
    {
        if (testModeItr != m_TestModes.end()
            && refreshRate <= round(testModeItr->RefreshRate))
        {
            if (refreshRate == round(testModeItr->RefreshRate))
                refreshRate -= refreshRateInterval;
            continue;
        }

        vector<TestMode>::iterator prevMode = testModeItr - 1;
        UINT32 vtotal = static_cast<UINT32>((prevMode->VTotal * prevMode->RefreshRate)
                                            / refreshRate);
        TestMode testMode = {vtotal, refreshRate, true,
                             (round(refreshRate) < flickerFreeMinRefeshRate)};
        // Below the flicker free minimum refresh rate, relax the error thresholds

        // Bug 1713505:
        if (round(testMode.RefreshRate) == 30.0)
        {
            testMode.reducedMode = false;
            testMode.relaxedMode = false;
        }

        testModeItr = m_TestModes.insert(testModeItr, testMode);
        refreshRate -= refreshRateInterval;
    }

    if (wrongRefreshOrder)
    {
        Printf(Tee::PriNormal,
            "Error: Expected V-Stretch DTDs to be from highest to lowest "
            "refresh rate (or DTDs are identical)\n");
        outOfSpec = true;
    }

    if (lwtei.version < 0x104)
    {
        Printf(Tee::PriNormal, "Error: Expected EDID at version at least 1.4.\n");
        outOfSpec = true;
    }

    if (lwtei.input.isDigital)
    {
        if (lwtei.input.u.digital.bpc == 0)
        {
            Printf(Tee::PriNormal, "Error: Undefined Color Bit Depth in EDID.\n");
            outOfSpec = true;
        }
        if (lwtei.input.u.digital.video_interface == 0)
        {
            Printf(Tee::PriNormal, "Error: Digital Interface is not defined in EDID.\n");
            outOfSpec = true;
        }
    }
    else
    {
        Printf(Tee::PriNormal, "Error: Expected \"Digital\" Video Signal Interface in EDID.\n");
        outOfSpec = true;
    }

    m_ExpectedGamma = lwtei.gamma/100.0;

    m_ColorResponsePoints.clear();
    ColorResponsePoint red = { 0xFF0000, lwtei.cc_red_x/1024.0f, lwtei.cc_red_y/1024.0f, "red" };
    m_ColorResponsePoints.push_back(red);
    ColorResponsePoint green = { 0x00FF00, lwtei.cc_green_x/1024.0f, lwtei.cc_green_y/1024.0f, "green" };
    m_ColorResponsePoints.push_back(green);
    ColorResponsePoint blue = { 0x0000FF, lwtei.cc_blue_x/1024.0f, lwtei.cc_blue_y/1024.0f, "blue" };
    m_ColorResponsePoints.push_back(blue);
    ColorResponsePoint white = { 0xFFFFFF, lwtei.cc_white_x/1024.0f, lwtei.cc_white_y/1024.0f, "white" };
    m_ColorResponsePoints.push_back(white);

    if (outOfSpec)
        return RC::ILWALID_EDID;

    return OK;
}

bool PanelCheck::IsRegisterDisabled(UINT32 address)
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

RC PanelCheck::RegisterCheck()
{
    Printf(Tee::PriLow, "Starting Register Check\n");

    Tee::Priority pri = GetVerbosePrintPri();
    RC rc;

    UINT32 vdid = 0;
    if (OK != m_TCONDevice.AuxRead(LW_SR_SRC_ID0, &vdid, sizeof(vdid)))
    {
        Printf(Tee::PriNormal, "Error: Unable to read panel Vendor/Device IDs\n");
        return RC::UNEXPECTED_RESULT;
    }

    UINT16 did = 0;
    UINT08 vid = 0;
    UINT08 version = 0;
    CHECK_RC(m_TCONDevice.AuxRead(LW_SR_SRC_ID0, &did, sizeof(did)));
    CHECK_RC(m_TCONDevice.AuxRead(LW_SR_SRC_ID2, &vid, sizeof(vid)));
    CHECK_RC(m_TCONDevice.AuxRead(LW_SR_SRC_ID3, &version, sizeof(version)));
    Printf(Tee::PriNormal, "    $ TCON DID : %#x\n", did);
    Printf(Tee::PriNormal, "    $ TCON VID : %#x\n", vid);
    Printf(Tee::PriNormal, "    $ TCON Ver : %#x\n", version);
    // $ is used as a uniqifier to make grepping easier and more reliable

    if (!IsRegisterDisabled(LW_SR_SR_CAPABILITIES0))
    {
        // LWSR registers are already unlocked:
        CHECK_RC(m_TCONDevice.LockMutex());

        if (!IsRegisterDisabled(LW_SR_SR_CAPABILITIES0))
        {
            Printf(Tee::PriNormal, "Error: Expected not to read SR Capabilities register\n");
            return RC::UNEXPECTED_RESULT;
        }
    }

    CHECK_RC(m_TCONDevice.UnlockMutex());

    if (IsRegisterDisabled(LW_SR_SR_CAPABILITIES0))
    {
        Printf(Tee::PriNormal, "Error: Expected to read SR Capabilities register\n");
        return RC::UNEXPECTED_RESULT;
    }

    Printf(pri, "Unlock Mutex successfully!\n");
    return OK;
}

RC PanelCheck::SetBacklightBrightnessAndMeasureLuminance
(
    FLOAT32 backlightBrightnessPercent,
    FLOAT64 *manualSyncFrequency,
    FLOAT64 *averageLuminanceCdM2
)
{
    RC rc;

    CHECK_RC(m_TCONDevice.SetBacklightBrightness(backlightBrightnessPercent));

    const UINT32 numMeasurements = 3;
    CHECK_RC(AverageLuminance(numMeasurements, manualSyncFrequency, averageLuminanceCdM2));

    return OK;
}

RC PanelCheck::BrightnessCheck()
{
    Printf(Tee::PriLow, "Starting Brightness Check\n");

    Tee::Priority pri = GetVerbosePrintPri();
    RC rc;
    FLOAT32 orgBacklightPercent;
    CHECK_RC(m_TCONDevice.GetBacklightBrightness(&orgBacklightPercent));

    Utility::CleanupOnReturnArg<TCONDevice, RC, FLOAT32>
        RestoreBacklight(&m_TCONDevice,
            &TCONDevice::SetBacklightBrightness,
            orgBacklightPercent);

    CHECK_RC(m_GpuRectFill.Fill(0,
                                0,
                                m_DisplaySurface.GetWidth(),
                                m_DisplaySurface.GetHeight(),
                                0x808080,
                                true));
    CHECK_RC(SetModeAndImage(&m_NativeRaster));
    FLOAT64 luminanceSyncFrequency = 0.0; // It will be determined automatically.
                                          // Needs to be reset to 0 for every mode.

    // Start with brightness=100 so that the sync frequency is correctly determined:
    FLOAT64 luminanceCdM2at100 = 0;
    CHECK_RC(SetBacklightBrightnessAndMeasureLuminance(100.0f,
        &luminanceSyncFrequency, &luminanceCdM2at100));
    Printf(Tee::PriLow, "   Luminance at 100%% backlight        = %5.1f cd/m2\n",
        luminanceCdM2at100);

    FLOAT64 luminanceCdM2at10 = 0;
    CHECK_RC(SetBacklightBrightnessAndMeasureLuminance(10.0f,
        &luminanceSyncFrequency, &luminanceCdM2at10));
    Printf(Tee::PriLow, "   Luminance at  10%% backlight        = %5.1f cd/m2\n",
        luminanceCdM2at10);

    CHECK_RC(m_TCONDevice.SetBacklightBrightness(100.0f));

    if ((m_DimBrightBacklightTolerance * luminanceCdM2at100) < luminanceCdM2at10)
    {
        Printf(Tee::PriNormal,
            "Error: Measured panel luminance at 100%% backlight = %5.1f cd/m2 "
            "is not sufficiently higher then at 10%% backlight = %5.1f cd/m2\n",
            luminanceCdM2at100, luminanceCdM2at10);
        return RC::UNEXPECTED_RESULT;
    }

    if (m_SkipGC6Testing)
    {
        Printf(Tee::PriNormal, "Skipping GC6 Testing\n");
        return rc;
    }

    CHECK_RC(m_TCONDevice.CacheFrame(nullptr)); // Enables the SR mode at the same time
    Utility::CleanupOnReturn<TCONDevice, RC>
        exitSR(&m_TCONDevice, &TCONDevice::ExitSelfRefresh);

    Printf(pri, "Enable Frame lock to slave lock\n");
    LW0073_CTRL_SYSTEM_GET_VRR_CONFIG_PARAMS vrrConfig = {};
    vrrConfig.subDeviceInstance = GetBoundGpuSubdevice()->GetSubdeviceInst();
    CHECK_RC_MSG(m_pDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_GET_VRR_CONFIG,
        &vrrConfig, sizeof(vrrConfig)), "Unable to get VRR config\n");
    CHECK_RC(m_pDisplay->SetHeadSetControlSlaveLock(m_OrigDisplays, true, vrrConfig.frameLockPin));
    Utility::CleanupOnReturn<PanelCheck, RC>
    restoreSlaveLock(this, &PanelCheck::RestoreSlaveLock);

    Utility::CleanupOnReturnArg<PanelCheck, RC, EvoRasterSettings*>
        setNativeRaster(this, &PanelCheck::SetModeAndImage, &m_NativeRaster);

    for (UINT32 loop = 0; loop < m_BrightnessCheckGC6Iterations; loop++)
    {
        CHECK_RC(m_pGCx->DoEnterGc6(GC_CMN_WAKEUP_EVENT_gc6gpu, 0));

        const UINT32 numMeasurements = 3;
        FLOAT64 luminanceCdM2atGC6 = 0;
        luminanceSyncFrequency = 0.0; // Just in case panel has changed the refresh

        // Don't do CHECK_RC here so that ExitGc6 is not skipped on measurement error:
        RC lumRc = AverageLuminance(numMeasurements, &luminanceSyncFrequency, &luminanceCdM2atGC6);

        CHECK_RC(m_pGCx->DoExitGc6(0, false));

        CHECK_RC(lumRc);
        Printf(Tee::PriLow, "   Luminance at 100%% backlight in GC6 = %5.1f cd/m2\n",
            luminanceCdM2at100);

        if (fabs(luminanceCdM2at100 - luminanceCdM2atGC6) >
            ((m_GC6BacklightTolerancePercent/100.0) * luminanceCdM2at100))
        {
            Printf(Tee::PriNormal,
                "Error: Measured panel luminance in GC6 = %3.1f cd/m2 "
                "is too far from when not in GC6 = %3.1f cd/m2\n",
                luminanceCdM2atGC6, luminanceCdM2at100);
            return RC::UNEXPECTED_RESULT;
        }
    }

    CHECK_RC(CheckNLTTransition());

    setNativeRaster.Cancel();
    CHECK_RC(SetModeAndImage(&m_NativeRaster));

    exitSR.Cancel();
    CHECK_RC(m_TCONDevice.ExitSelfRefresh());

    return OK;
}

RC PanelCheck::Cleanup()
{
    StickyRC rc;

    if (m_pGCx != nullptr)
    {
        m_pGCx->Shutdown();
        m_pGCx = nullptr;
    }

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

void PanelCheck::PrintJsProperties(Tee::Priority pri)
{
    Printf(pri, "%s Js Properties:\n", GetName().c_str());

    if (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
    {
        Printf(pri, "    NoThermometer:                       %s\n", m_NoThermometer ? "true" : "false");
        Printf(pri, "    SkipFlickerCheck:                    %s\n", m_SkipFlickerCheck ? "true" : "false");
    }
    Printf(pri, "    SkipSelfRefreshCheck                 %s\n", m_SkipSelfRefreshCheck ? "true" : "false");
    Printf(pri, "    SkipGC6Testing                       %s\n", m_SkipGC6Testing ? "true" : "false");
    Printf(pri, "    PrintSubtestRC:                      %s\n", m_PrintSubtestRC ? "true" : "false");
    Printf(pri, "    DelayBeforeMeasurementMs:            %d\n", m_DelayBeforeMeasurementMs);
    Printf(pri, "    MaximumSearchFrequency:              %f\n", m_MaximumSearchFrequency);
    Printf(pri, "    LowPassFilterFrequency:              %f\n", m_LowPassFilterFrequency);
    Printf(pri, "    FilterOrder:                         %d\n", m_FilterOrder);
    Printf(pri, "    OnlyGrayGamma:                       %s\n", m_OnlyGrayGamma ? "true" : "false");
    Printf(pri, "    FullRangeGamma:                      %s\n", m_FullRangeGamma ? "true" : "false");
    Printf(pri, "    GammaToleranceAtLowerRefreshCdM2:    %f\n", m_GammaToleranceAtLowerRefreshCdM2);
    Printf(pri, "    GammaToleranceAtLowerRefreshPercent: %f\n", m_GammaToleranceAtLowerRefreshPercent);
    Printf(pri, "    ShowGamma:                           %s\n", m_ShowGamma ? "true" : "false");
    Printf(pri, "    OnlyEdidModes:                       %s\n", m_OnlyEdidModes ? "true" : "false");
    Printf(pri, "    SelfRefreshCheckGC6Iterations:       %d\n", m_SelfRefreshCheckGC6Iterations);
    Printf(pri, "    BrightnessCheckGC6Iterations:        %d\n", m_BrightnessCheckGC6Iterations);

    GpuTest::PrintJsProperties(pri);
}

JS_CLASS_INHERIT(PanelCheck, GpuTest, "PanelCheck");

CLASS_PROP_READWRITE_FULL(PanelCheck, NoThermometer, bool,
         "The IR Thermometer is known missing, so suppress warnings/errors of it being missing.",
         MODS_INTERNAL_ONLY | SPROP_VALID_TEST_ARG, 0);
CLASS_PROP_READWRITE(PanelCheck, PrintSubtestRC, bool,
         "Print the RC from each of the individual subtests. Defaults to true.");
CLASS_PROP_READWRITE(PanelCheck, DelayBeforeMeasurementMs, UINT32,
        "Delay in miliseconds after display setup but before measurements");
CLASS_PROP_READWRITE(PanelCheck, MaximumSearchFrequency, FLOAT64,
        "Maximum measured flicker frequency");
CLASS_PROP_READWRITE(PanelCheck, LowPassFilterFrequency, FLOAT64,
        "Frequency of low pass filter applied before flicker callwlations "
        "(when set to 0 the filter is disabled)");
CLASS_PROP_READWRITE(PanelCheck, FilterOrder, UINT32,
        "Filter order used in flicker callwlations");
CLASS_PROP_READWRITE_FULL(PanelCheck, SkipFlickerCheck, bool,
        "Do not run FlickerCheck.", MODS_INTERNAL_ONLY | SPROP_VALID_TEST_ARG, 0);
CLASS_PROP_READWRITE(PanelCheck, SkipSelfRefreshCheck, bool,
        "Do not run SelfRefreshCheck");
CLASS_PROP_READWRITE(PanelCheck, SkipGC6Testing, bool,
        "Skip the GC6 portion of SelfRefreshCheck and BrightnessCheck");
CLASS_PROP_READWRITE(PanelCheck, OnlyGrayGamma, bool,
        "Don't measure R G B separately during the Gamma Check, only Gray");
CLASS_PROP_READWRITE(PanelCheck, FullRangeGamma, bool,
        "Measure all levels during the Gamma Check instead of just a few ones");
CLASS_PROP_READWRITE(PanelCheck, GammaToleranceAtLowerRefreshCdM2, FLOAT64,
        "Tolerance in cd/m^2 between gamma measurements at different refresh rates");
CLASS_PROP_READWRITE(PanelCheck, GammaToleranceAtLowerRefreshPercent, FLOAT64,
        "Additional tolerance in percent between gamma measurements at different refresh rates");
CLASS_PROP_READWRITE(PanelCheck, ShowGamma, bool,
        "Show tables with gamma measurement results");
CLASS_PROP_READWRITE(PanelCheck, OnlyEdidModes, bool,
        "Only run test modes with refresh rates defined in the EDID");
CLASS_PROP_READWRITE(PanelCheck, SelfRefreshCheckGC6Iterations, UINT32,
        "Number of loops to enter/exit GC6 during the SelfRefreshCheck subtest");
CLASS_PROP_READWRITE(PanelCheck, BrightnessCheckGC6Iterations, UINT32,
        "Number of loops to enter/exit GC6 during the BrightnessCheck subtest");
