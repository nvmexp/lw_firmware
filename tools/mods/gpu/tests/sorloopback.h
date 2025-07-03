/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Display SOR loopback test.
// Based on //arch/traces/non3d/iso/disp/class010X/core/disp_core_sor_loopback_vaio/test.js
//          //arch/traces/ip/display/3.0/branch_trace/full_chip/sor_loopback/src/2.0/test#10.cpp

#pragma once

#include <bitset>
#include "gputest.h"
#include "core/include/display.h"
#include "gpu/display/lwdisplay/lw_disp.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/loopbacktester.h"

using BitSet16 = bitset<16>;

class DisplaySor
{
public:
    DisplaySor(GpuSubdevice *pSubdev, Display *pDisplay);

    virtual ~DisplaySor() = default;

    virtual RC Setup
    (
        Tee::Priority printPriority,
        Display::Mode cleanupMode
    ) = 0;

    virtual RC Cleanup() = 0;

    virtual void SetMaximizePclk(bool val) {}

    virtual UINT32 GetRequiredLoopsCount() { return 1; }

    virtual RC ConfigureForLoop(UINT32 index) { return OK; }

    virtual bool IsTestRequiredForDisplay(UINT32 loopIndex, DisplayID displayID) { return true; }

    virtual void GetTMDSLinkSupport
    (
        DisplayID displayID,
        bool *pLinkA,
        bool *pLinkB
    );

    virtual bool IsLinkUPhy(Display::Encoder::Links link) { return false; }

    virtual RC AssignSOR
    (
        DisplayID displayID,
        UINT32 requestedSor,
        UINT32 *pAssignedSor,
        UINT32 *pOrType,
        UINT32 *pProtocol
    );

    virtual RC PerformPassingSetMode
    (
        DisplayID displayID,
        Display::Mode mode,
        UINT32 assignedSor,
        UINT32 protocol,
        UINT32 requestedPixelClockHz,
        UINT32 *pPassingPixelClockHz
    ) = 0;

    virtual RC PostSetModeCleanup(const DisplayID& displayID, UINT32 protocol) = 0;

    virtual RC DoWARs
    (
        UINT32 sorNumber,
        bool isProtocolTMDS,
        bool isLinkATestNeeded,
        bool isLinkBTestNeeded
    ){ return OK; }

    virtual void SaveRegisters() {}
    virtual void RestoreRegisters() {}

    SETGET_PROP(FrlMode, bool);

    virtual RC RegisterCallbackArgs(LoopbackTester* pLoopbacktester);

protected:
    GpuSubdevice *m_pSubdev = nullptr;
    Display *m_pDisplay = nullptr;
    Tee::Priority m_VerbosePrintPriority = Tee::PriLow;
    Tee::Priority m_OriginalIMPPrintPriority = Tee::PriLow;
    Display::Mode m_CleanupMode;
    bool m_FrlMode = false;

    INT32 VerbosePrintf(const char* format, ...) const;

    virtual RC FillRasterSettings
    (
        RasterSettings *pRasterSettings,
        UINT32 width,
        UINT32 height,
        UINT32 pclk
    );
};
class SorLoopback : public GpuTest
{
public:
    SorLoopback();

    RC Setup() override;

    RC Run() override;

    RC Cleanup() override;

    bool IsSupported() override;

    // Helper API to poll registers
    RC PollRegValue
    (
        UINT32 regAddr,
        UINT32 expectedData,
        UINT32 dataMask,
        FLOAT64 timeoutMs
    );

    RC PollPairRegValue
    (
        UINT32 regAddr,
        UINT64 expectedData,
        UINT64 dataWidth,
        FLOAT64 timeoutMs
    );

    SETGET_PROP(TMDSPixelClock, UINT32);
    SETGET_PROP(LVDSPixelClock, UINT32);
    SETGET_PROP(PllLoadAdj, UINT32);
    SETGET_PROP(PllLoadAdjOverride, bool);
    SETGET_PROP(DsiVpllLockDelay, UINT32);
    SETGET_PROP(DsiVpllLockDelayOverride, bool);
    SETGET_PROP(ForcedPatternIdx, UINT32);
    SETGET_PROP(ForcePattern, bool);
    SETGET_PROP(CRCSleepMS, UINT32);
    SETGET_PROP(UseSmallRaster, bool);
    SETGET_PROP(WaitForEDATA, bool);
    SETGET_PROP(IgnoreCrcMiscompares, bool);
    SETGET_PROP(SkipDisplayMask, UINT32);
    SETGET_PROP(SleepMSOnError, UINT32);
    SETGET_PROP(PrintErrorDetails, bool);
    SETGET_PROP(PrintModifiedCRC, bool);
    SETGET_PROP(EnableDisplayRegistersDump, bool);
    SETGET_PROP(EnableDisplayRegistersDumpOnCrcMiscompare, bool);
    SETGET_PROP(NumRetries, UINT32);
    SETGET_PROP(InitialZeroPatternMS, UINT32);
    SETGET_PROP(FinalZeroPatternMS, UINT32);
    SETGET_PROP(TestOnlySor, UINT32);
    SETGET_PROP(LVDSA, bool);
    SETGET_PROP(LVDSB, bool);
    SETGET_PROP(EnableSplitSOR, bool);
    SETGET_PROP(SkipSplitSORMask, UINT32);
    SETGET_PROP(SkipDisplayMaskBySplitSOR, string);
    SETGET_PROP(SkipLVDS, bool);
    SETGET_PROP(SkipUPhy, bool);
    SETGET_PROP(SkipLegacyTmds, bool);
    SETGET_PROP(SkipFrl, bool);
    SETGET_PROP(TimeoutMs, FLOAT64);
    SETGET_PROP(IobistLoopCount, UINT32);
    SETGET_PROP(IobistCaptureBufferForSecLink, bool);
    SETGET_PROP(FrlMode, bool);

private:
    Display *m_pDisplay = nullptr;

    GpuSubdevice *m_pSubdev = nullptr;
    bool m_FrlMode = false;

    std::unique_ptr<DisplaySor> m_pDispSor;
    std::unique_ptr<LoopbackTester> m_pLoopBackTesterLegacy;
    std::unique_ptr<LoopbackTester> m_pLoopBackTesterUPhy;
    std::unique_ptr<LoopbackTester> m_pLoopBackTesterIobist;

    Display::Mode m_Mode;
    Display::Mode m_CleanupMode;
    FLOAT64 m_TimeoutMs = 0;

    UINT32 m_NumSorsTested = 0;
    UINT32 m_VirtualDisplayMask = 0;

    //---------------------------------- Test args - start ---------------------------------------
    // Pixel clock in Hz
    UINT32 m_TMDSPixelClock = 0;
    UINT32 m_LVDSPixelClock = 0;

    // LW_PDISP_SOR_PLL1_LOADADJ:
    UINT32 m_PllLoadAdj = 0;
    bool m_PllLoadAdjOverride = false;

    // Indicates the PLL lock delay override in LW_PDISP_DSI_VPLL
    UINT32 m_DsiVpllLockDelay = 0;
    bool m_DsiVpllLockDelayOverride = false;

    UINT32 m_ForcedPatternIdx = 0;
    bool m_ForcePattern = false;

    // Sleep in MS before reading CRC values from registers
    UINT32 m_CRCSleepMS = 1;

    // Use smaller raster in SetMode so it exelwtes fast on fmodel
    bool m_UseSmallRaster = false;

    // Wait for expected values in LW_PDISP_SOR_EDATA_* registers
    // Set to "false" so this test can be sanity checked on fmodel
    bool m_WaitForEDATA = true;

    // Don't report CRC miscompare error so this test can be
    // sanity checked on fmodel on all supported connectors
    bool m_IgnoreCrcMiscompares = false;

    // Don't run SorLoopback on these displays
    UINT32 m_SkipDisplayMask = 0;

    // Sleep in MS after determining an error to support scope shots, etc...
    UINT32 m_SleepMSOnError = 0;

    // Print detailed info on error at PriNormal level
    bool m_PrintErrorDetails = false;
    bool m_PrintModifiedCRC = false;

    bool m_EnableDisplayRegistersDump = false;
    bool m_EnableDisplayRegistersDumpOnCrcMiscompare = false;

    // Retry reading loopback registers
    UINT32 m_NumRetries = 0;

    // Sleep in MS with zero output before sending the patterns to support scope shots, etc...
    UINT32 m_InitialZeroPatternMS = 0;

    // Sleep in MS with zero output after sending the patterns to support scope shots, etc...
    UINT32 m_FinalZeroPatternMS = 0;

    // Limit test to selected SOR
    UINT32 m_TestOnlySor = 0xFFFFFFFF;

    // Select which sublinks to test on LVDS outputs
    bool m_LVDSA = true;
    bool m_LVDSB = true;

    // Default split SOR config before SorLoopback starts
    bool m_EnableSplitSOR = false;

    // Don't run SorLoopback on these split SOR combinations
    UINT32 m_SkipSplitSORMask = 0;

    // Vector of skip display mask and split SOR config index
    string m_SkipDisplayMaskBySplitSOR;

    bool m_SkipLVDS = false;

    bool m_SkipUPhy = true;

    bool m_SkipLegacyTmds = false;
    bool m_SkipFrl = false;

    // Number of cycles to run during IOBIST test
    UINT32 m_IobistLoopCount = 0;

    // Select the sublink for which the capture buffer should hold the value
    UINT32 m_IobistCaptureBufferForSecLink = false;
    //---------------------------------- Test args - end -----------------------------------------

    RC InitProperties();

    RC RunForAllProtocols(UINT32 loopIndex);
    RC RunOnAllConnectors(UINT32 loopIndex, bool frlMode);

    RC IsTestRequiredForDisplay
    (
        UINT32 loopIndex,
        DisplayID displayID,
        bool *pIsRequired
    );

    RC IsSupportedSignalType(DisplayID displayID, bool *pIsSupported);

    UINT32 GetMaxSORCountPerDisplayID();

    RC DetermineValidPixelClockHz
    (
        UINT32 protocol,
        const Display::Encoder encoderInfo,
        UINT32 *pixelClockHz
    );

    RC RunDisplay
    (
        const DisplayID& displayID,
        UINT32 assignedSor,
        UINT32 protocol
    );

    RC RunDisplayWithMappedProtocol
    (
        Display::Encoder::Links link,
        DisplayID displayID,
        UINT32 assignedSor,
        UINT32 mappedProtocol,
        UPHY_LANES lane,
        UINT32 pixelClockHz
    );

    RC CleanSORExcludeMask(UINT32 displayID);

    RC ReassignSorForDualLink(const DisplayID &displayID, UINT32 sorIdx);

    LoopbackTester* GetLoopbackTester(Display::Encoder::Links links);

    RC SetSorLoopbackTestArgs(LoopbackTester::SorLoopbackTestArgs* pSorLoopbackTestArgs);

};

class LwDisplaySor : public DisplaySor
{
public:
    LwDisplaySor(GpuSubdevice *pSubdev, Display *pDisplay);

    ~LwDisplaySor() = default;

    RC Setup
    (
        Tee::Priority printPriority,
        Display::Mode cleanupMode
    ) override;

    RC Cleanup() override;

    void SetMaximizePclk(bool val) override { m_MaximizePclk = val; }

    bool IsLinkUPhy(Display::Encoder::Links link) override;

    RC PerformPassingSetMode
    (
        DisplayID displayID,
        Display::Mode mode,
        UINT32 assignedSor,
        UINT32 protocol,
        UINT32 requestedPixelClockHz,
        UINT32 *pPassingPixelClockHz
    ) override;

    RC PostSetModeCleanup(const DisplayID& displayID, UINT32 protocol) override;

private:
    LwDisplay *m_pLwDisplay = nullptr;
    LwDispCoreChnContext *m_pCoreContext = nullptr;
    bool m_MaximizePclk = true;

    map<UINT32, unique_ptr<LwDispLutSettings>> m_OLutSettings;

    RC SetupOLut
    (
        const HeadIndex head,
        LwDispHeadSettings *pHeadSettings
    );

    RC UpdatePixelClockForModePossible
    (
        HeadIndex headIndex,
        Display::Mode mode,
        LwDispHeadSettings *pHeadSettings
    );

    RC FillRasterSettings
    (
        RasterSettings *pRasterSettings,
        UINT32 width,
        UINT32 height,
        UINT32 pclk
    ) override;

    RC SendUpdates();
};

class EvoDisplaySor : public DisplaySor
{
public:
    std::vector<UINT32> m_RegAddSorBasedRegLookup;

    EvoDisplaySor
    (
        GpuSubdevice *pSubdev,
        Display *pDisplay,
        SorLoopback *pParent
    );

    ~EvoDisplaySor() = default;

    RC Setup
    (
        Tee::Priority printPriority,
        Display::Mode cleanupMode
    ) override;

    RC Cleanup() override;

    UINT32 GetRequiredLoopsCount() override;

    RC ConfigureForLoop(UINT32 index) override;

    bool IsTestRequiredForDisplay(UINT32 loopIndex, DisplayID displayID) override;

    // Check which TMDS links does the display under test support in split
    // SOR mode. Assume in default split SOR mode they are always supported
    void GetTMDSLinkSupport
    (
        DisplayID displayID,
        bool *pLinkA,
        bool *pLinkB
    ) override;

    RC AssignSOR
    (
        DisplayID displayID,
        UINT32 requestedSor,
        UINT32 *pAssignedSor,
        UINT32 *pOrType,
        UINT32 *pProtocol
    ) override;

    RC PerformPassingSetMode
    (
        DisplayID displayID,
        Display::Mode mode,
        UINT32 assignedSor,
        UINT32 protocol,
        UINT32 requestedPixelClockHz,
        UINT32 *pPassingPixelClockHz
    ) override;

    RC PostSetModeCleanup(const DisplayID& displayID, UINT32 protocol) override;

    RC DoWARs
    (
        UINT32 sorNumber,
        bool isProtocolTMDS,
        bool isLinkATestNeeded,
        bool isLinkBTestNeeded
    ) override;

    void SaveRegisters() override;
    void RestoreRegisters() override;
    RC RegisterCallbackArgs(LoopbackTester* pLoopbacktester) override;
private:
    UINT32 m_OriginalDisplay = 0;
    Display::SplitSorReg m_DefaultSplitSorReg;
    EvoRasterSettings m_ers;
    SorLoopback *m_pParent = nullptr;

    UINT32 m_SkipSplitSORMask = 0;
    map<UINT32, UINT32> m_SkipDisplayMaskBySplitSORList;
    bool m_TestSplitSors = false;
    Display::SplitSorConfig m_SplitSorConfig;
    UINT32 m_NumSplitSorCombos = 0;

    vector<UINT08> m_CachedEdid;
    UINT32 m_SavedDsiVpllValue = 0;

    enum TmdsLinkType
    {
        TMDS_LINK_A,
        TMDS_LINK_B
    };

    // API to check if a display is TMDS single link capable in split SOR
    bool IsTmdsSingleLinkCapable(UINT32 displayID, TmdsLinkType LinkType);

    // Parser function to map split SOR config index toi skip display mask
    RC ParseSkipDisplayMaskBySplitSOR();

    // Helper API to indicate if a display ID is skipped in a split SOR mode
    bool IsDisplayIdSkipped(UINT32 displayID);

    RC SetMode
    (
        UINT32 width,
        UINT32 height,
        UINT32 depth,
        UINT32 refreshRate,
        UINT32 sorIndex
    );

    RC RetrySetMode
    (
        UINT32 width,
        UINT32 height,
        UINT32 depth,
        UINT32 refreshRate,
        GpuSubdevice *pSubdev,
        UINT32 *updatedPClk
    );

    RC Select(UINT32 displayID);

    RC SetPixelClock(UINT32 pclk);

    RC SendORSettings(UINT32 sorIndex, UINT32 protocol);

    RC SetTimings(UINT32 hActive, UINT32 vActive, UINT32 pclk);

    RC SendUpdates();

    RC CacheEdid(DisplayID displayID, UINT32 protocol);

    RC RestoreEdid(DisplayID displayID, UINT32 protocol);

    RC DisableDPModeForced
    (
        UINT32 sorNumber,
        bool TestLinkA,
        bool TestLinkB
    );
};
