/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file disp_crcchecker.h
 * @brief
 */

#pragma once

#ifndef INCLUDED_DISP_CRCCHECKER_H
#define INCLUDED_DISP_CRCCHECKER_H

#include <stdio.h>
#include <vector>
#include "core/include/display.h"
#include "gpu/display/lwdisplay/iot_config_parser.h"
#include "gpu/display/lwdisplay/iot_superswitch.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/dmawrap.h"
#include "gpu/utility/surf2d.h"
#include "gpu/utility/surffill.h"
#include "gputest.h"

#if !defined(TEGRA_MODS)
#include "gpu/display/evo_disp.h" // EvoRasterSettings
#endif

#include "gpu/display/tegra_disp.h" // HDCP max retries
#include "gpu/display/crc_checker_dev.h" // CrcCheckerDevice

using namespace SurfaceUtils;
using ModeDescription = Display::StressMode;

class DisplayHAL
{
public:
    struct DelayKnobs
    {
        UINT32 postSetupDelay = 0;
        UINT32 postCleanupDelay = 0;
        UINT32 postSetmodeDelay = 0;

        DelayKnobs(UINT32 postSetupDelay, UINT32 postCleanupDelay, UINT32 postSetmodeDelay)
            : postSetupDelay(postSetupDelay)
            , postCleanupDelay(postCleanupDelay)
            , postSetmodeDelay(postSetmodeDelay)
        {

        }
        DelayKnobs() = default;
    };

    DisplayHAL
    (
        Display *pDisplay,
        const GpuTestConfiguration& testConfig,
        UINT32 maxPixelClockHz,
        bool useRasterFromEdid
    );
    virtual ~DisplayHAL() = default;

    virtual RC GetHead(const DisplayID& displayID, UINT32 *pHead);

    virtual RC Setup() = 0;
    virtual RC SetMode
    (
        const DisplayID& displayID,
        const Display::Mode& resolutionInfo,
        UINT32 head
    ) = 0;
    virtual RC DetachDisplay
    (
        const DisplayID& displayID
    ) { return OK; }
    virtual RC Cleanup() = 0;
    virtual RC DisplayImage
    (
        const DisplayID& displayID,
        Surface2D *image,
        E1850CrcChecker::BoardType boardType
    ) = 0;

    const DelayKnobs& GetDelayKnobs() const;
protected:
    Display *m_pDisplay;
    const GpuTestConfiguration& m_TestConfig;
    UINT32 m_MaxPixelClockHz = 0;
    bool m_UseRasterFromEdid = false;
    DelayKnobs m_DelayKnobs;

    void SetDelayKnobs(const DelayKnobs& delayKnobs);
#if !defined(TEGRA_MODS)
    bool FillRasterSettings
    (
        const Display::Mode& resolutionInfo,
        EvoRasterSettings *rasterSettings
    );
#endif
};

class LegacyDisplayHAL : public DisplayHAL
{
public:
    LegacyDisplayHAL
    (
        Display *pDisplay,
        const GpuTestConfiguration& testConfig,
        const DelayKnobs& delayKnobs,
        UINT32 maxPixelClockHz,
        bool useRasterFromEdid
    );

protected:
    RC Setup() override;
    RC Cleanup() override;

private:
    DisplayID m_OriginalDisplayID;
    Display::ColorTranscodeMode m_OrigColorTranscoding = Display::IlwalidMode;

    RC SetMode
    (
        const DisplayID& displayID,
        const Display::Mode& resolutionInfo,
        UINT32 head
    ) override;
    RC DisplayImage
    (
        const DisplayID& displayID,
        Surface2D *image,
        E1850CrcChecker::BoardType boardType
    ) override;

    virtual bool IsColorTranscodingSupported() = 0;
    virtual RC UpdateRasterSettings(const Display::Mode& resolutionInfo) = 0;

    RC CacheColorTranscoding();
    RC RestoreColorTranscoding();
};

class TegraDisplayHAL : public LegacyDisplayHAL
{
public:
    TegraDisplayHAL
    (
        Display *pDisplay,
        const GpuTestConfiguration& testConfig,
        const DelayKnobs& delayKnobs,
        bool enableHdcpInAndroid
    );

private:
    bool m_EnableHdcpInAndroid = false;
    UINT32 m_SavedMaxHdcpRetries = 0;

    RC GetHead(const DisplayID& displayID, UINT32 *pHead) override;

    RC Setup() override;
    RC Cleanup() override;

    bool IsColorTranscodingSupported() override;
    RC UpdateRasterSettings(const Display::Mode& resolutionInfo) override;
};

#if !defined(TEGRA_MODS)
class EvoDisplayHAL : public LegacyDisplayHAL
{
public:
    EvoDisplayHAL
    (
        Display *pDisplay,
        const GpuTestConfiguration& testConfig,
        const DelayKnobs& delayKnobs,
        UINT32 maxPixelClockHz,
        bool useRasterFromEdid
    );

private:
    bool IsColorTranscodingSupported() override;
    RC UpdateRasterSettings(const Display::Mode& resolutionInfo) override;
};

class CrcHandler;
class LwDisplayHAL : public DisplayHAL
{
public:
    LwDisplayHAL
    (
        Display *pDisplay,
        const GpuTestConfiguration& testConfig,
        const DelayKnobs& delayKnobs,
        UINT32 maxPixelClockHz,
        bool useRasterFromEdid,
        bool enableInternalCrcCapture,
        Tee::Priority printPriority
    );

private:
    LwDisplay *m_pLwDisplay = nullptr;
    bool m_EnableInternalCrcCapture = false;
    Tee::Priority m_PrintPriority = Tee::PriLow;
    unique_ptr<CrcHandler> m_pCrcHandler;
    map<DisplayID, LwDispWindowSettings> m_UsedWindowMap;
    bool m_OrgDefaultActiveCrcRasterOnly = true;

    RC Setup() override;
    RC SetMode
    (
        const DisplayID& displayID,
        const Display::Mode& resolutionInfo,
        UINT32 head
    ) override;
    RC DetachDisplay
    (
        const DisplayID& displayID
    ) override;
    RC Cleanup() override;
    RC DisplayImage
    (
        const DisplayID& displayID,
        Surface2D *image,
        E1850CrcChecker::BoardType
        boardType
    ) override;

    RC StartCrcCapture
    (
        const DisplayID& displayID,
        HeadIndex headIndex,
        E1850CrcChecker::BoardType boardType
    );
    RC GetCrcProtocol
    (
        E1850CrcChecker::BoardType boardType,
        CrcHandler::CRCProtocol *pProtocol
    ) const;
    RC CompleteCrcCapture() const;
};
#endif

static const UINT32 TESTARG_UNSET = 0xFFFFFFFF;

//------------------------------------------------------------------------------
// Display CRC checker class definition
//------------------------------------------------------------------------------
class DisplayCrcChecker : public GpuTest
{
private:
    Display *m_pDisplay = nullptr;
    unique_ptr<SurfaceFill> m_pSurfaceFill;
    unique_ptr<DisplayHAL> m_pDisplayHAL;
    Tee::Priority m_OrigIMP = Tee::PriNone;

    StickyRC m_ContinuationRC;
    bool     m_WasAnyStressModeTested = false;

    vector<ModeDescription> m_StressModes;
    vector<unique_ptr<CrcCheckerDevice>> m_CrcCheckerDeviceList;

    bool   m_ContinueOnCrcError = false;
    UINT32 m_AllowedMismatchedCrcCount = 0;
    bool   m_EnableDMA = true;

    // Dump the CRC registers in verbose mode
    bool m_DumpCrcCheckerRegisters = false;

    // Require DisplayCrcChecker to run on these displays
    UINT32 m_DisplaysToTest = 0;

    // Don't run DisplayCrcChecker on these displays
    UINT32 m_SkipDisplayMask = 0;

    // Limit test to this pixel clock limits
    UINT32 m_MaxPixelClockHz = 0;

    // Timeout for polling on VSYNC
    FLOAT64 m_VsyncPollTimeout = 1000;

    // Number of frames to be tested
    UINT32 m_NumOfFrames = 100;

    // For PNG test pattern support
    string m_TestPatternFileName;
    string m_TestPatternTableName;
    bool m_DumpPngPattern = false;

    // IOT Config file with DisplayIDs
    string m_IOTConfigFilePath;

    // To run the test in parallel
    bool m_RunParallel = false;

    // Test knob to control delay after modeset in the inner test loop i.e. RunCrcTest
    UINT32 m_InnerLoopModesetDelayUs = TESTARG_UNSET;

    // Test knob to control delay after modeset in the outer test loop i.e. Setup, Run
    UINT32 m_OuterLoopModesetDelayUs = TESTARG_UNSET;

    // Test knob to select raster from EDID / default raster from test 46
    bool m_UseRasterFromEDID = false;

    // Test knob to limit DP by the max resolution in the EDID
    bool m_LimitVisibleBwFromEDID = false;

    // Test knob to disable HDCP negotiation support (CheetAh + Android only)
    bool m_EnableHdcpInAndroid = false;

    // Test knob to enable internal CRC capture (LwDisplay only - Volta onwards)
    bool m_EnableInternalCrcCapture = false;

    // Test knob to control delay after hot plug is detected
    UINT32 m_PostHPDDelayMs = 1000;

    // Used to prevent other tests from changing p-states during this test
    PStateOwner m_PStateOwner;

    // Test configuration context
    GpuTestConfiguration *m_pTestConfig = nullptr;

    GpuSubdevice *m_pGpuSubdevice = nullptr;

    unique_ptr<LwDispUtils::HPDGpio> m_pHPDGpio;

    RC RunIotMode();
    RC ReadAllLwstomParams(IotSuperSwitch *pIotSuperSwitch);
    RC UpdateStressModesForIotTesting
    (
        const DisplayID &displayID,
        const IotConfigurationExt::StressModes& stressModes
    );
    RC WaitForConnectionAndReadResponse
    (
        IotSuperSwitch *pIotSuperSwitch,
        Tee::Priority hpdDebugPrintPriority,
        const DisplayID& displayID,
        UINT32 timeoutMs,
        bool isConnectionExpected
    );
    RC WaitForConnection
    (
        Tee::Priority hpdDebugPrintPriority,
        const DisplayID& displayID,
        UINT32 timeoutMs,
        bool isConnectionExpected
    );

    RC RunNonIotMode();

    RC FillCrcCheckerDeviceList();

    RC PrepareBackground(UINT32 stressModeIdx, ColorUtils::Format format);

    RC DumpImage(UINT32 stressModeIdx, Surface2D *image);

    // Initialize the test properties.
    RC InitProperties();

    RC SetupSurfaceFill();
    RC SetupDisplayHAL();

    RC ResetCrcCheckerDevices();

    RC IsVisibleBandwidthAllowed
    (
        DisplayID displayID,
        const ModeDescription& stressMode,
        bool *pIsAllowed
    );

    RC DumpCrcDeviceRegisters
    (
        CrcCheckerDevice *pCrcChecker,
        const char *prefixMsg,
        Tee::Priority
    );

    RC PerformSetMode(const DisplayID& displayID, const ModeDescription& stressMode);

    // Wrapper for CRC test loop - based on test configuration.
    RC RunAllInnerLoops
    (
        const ModeDescription& stressMode,
        CrcCheckerDevice* const & pCrcCheckerDevice
    );

    // Single loop of CRC test
    RC RunInnerLoop
    (
        CrcCheckerDevice *pCrcChecker,
        CrcCheckerDevice::CrcCheckerConfig *pConfig,
        const ModeDescription& stressMode
    );

    RC PrintCrcExperimentInfo
    (
        Tee::Priority printPriority,
        CrcCheckerDevice* const & pCrcCheckerDevice,
        const CrcCheckerDevice::BoardInfo& brdInfo,
        const ModeDescription& stressMode
    );

    RC PrintDebugInfoPostSetMode
    (
        Tee::Priority printPriority,
        CrcCheckerDevice* const & pCrcCheckerDevice
    );

    RC IsModePossibleForCrcChecker
    (
        CrcCheckerDevice* pCrcChecker,
        const ModeDescription& stressMode
    );

    RC ValidateCapturedCrcCount
    (
        CrcCheckerDevice *pCrcChecker,
        CrcCheckerDevice::CrcCheckerConfig* pConfig,
        const vector<UINT32>& crcsCollected
    );

    RC ParseCapturedCrcs
    (
        const vector<UINT32>& crcsCollected,
        UINT32 expectedCrc,
        vector<UINT32> *pFailedCrcIndices
    );

    RC PrintParseResult
    (
        CrcCheckerDevice *pCrcChecker,
        const vector<UINT32>& crcsCollected,
        UINT32 expectedCrc,
        const vector<UINT32>& failedCrcIndices
    );

    RC RunDisplayCRCInParallel();
    RC RunDisplayCRCInSequential();

public:
    DisplayCrcChecker();

    RC Setup() override;
    RC Run() override;
    RC Cleanup() override;

    bool IsSupported() override;

    void AddStressMode(const ModeDescription& mode);
    void ClearStressModes();

    SETGET_PROP(DisplaysToTest, UINT32);
    SETGET_PROP(SkipDisplayMask, UINT32);
    SETGET_PROP(MaxPixelClockHz, UINT32);
    SETGET_PROP(ContinueOnCrcError, bool);
    SETGET_PROP(AllowedMismatchedCrcCount, UINT32);
    SETGET_PROP(EnableDMA, bool);
    SETGET_PROP(DumpCrcCheckerRegisters, bool);
    SETGET_PROP(VsyncPollTimeout, FLOAT64);
    SETGET_PROP(NumOfFrames, UINT32);
    SETGET_PROP(TestPatternFileName, string);
    SETGET_PROP(TestPatternTableName, string);
    SETGET_PROP(DumpPngPattern, bool);
    SETGET_PROP(InnerLoopModesetDelayUs, UINT32);
    SETGET_PROP(OuterLoopModesetDelayUs, UINT32);
    SETGET_PROP(UseRasterFromEDID, bool);
    SETGET_PROP(LimitVisibleBwFromEDID, bool);
    SETGET_PROP(EnableHdcpInAndroid, bool);
    SETGET_PROP(EnableInternalCrcCapture, bool);
    SETGET_PROP(PostHPDDelayMs, UINT32);
    SETGET_PROP(IOTConfigFilePath, string);
    SETGET_PROP(RunParallel, bool);
};

#endif // INCLUDED_DISP_CRCCHECKER_H
