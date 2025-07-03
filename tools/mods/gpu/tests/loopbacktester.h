/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
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
#include "gpu/display/lwdisplay/lw_disp_utils.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"

using BitSet16 = bitset<16>;


enum class UPHY_LANES
{
    NONE,
    ZeroOne,
    TwoThree
};

class LoopbackTester
{
public:
    struct SorLoopbackTestArgs
    {
        // Pixel clock in Hz
        UINT32 tmdsPixelClock = 0;
        UINT32 lvdsPixelClock = 0;

        // LW_PDISP_SOR_PLL1_LOADADJ:
        UINT32 pllLoadAdj = 0;
        bool pllLoadAdjOverride = false;

        // Indicates the PLL lock delay override in LW_PDISP_DSI_VPLL
        UINT32 dsiVpllLockDelay = 0;
        bool dsiVpllLockDelayOverride = false;

        UINT32 forcedPatternIdx = 0;
        bool forcePattern = false;

        // Sleep in MS before reading CRC values from registers
        UINT32 crcSleepMS = 1;

        // Use smaller raster in SetMode so it exelwtes fast on fmodel
        bool useSmallRaster = false;

        // Wait for expected values in LW_PDISP_SOR_EDATA_* registers
        // Set to "false" so this test can be sanity checked on fmodel
        bool waitForEDATA = true;

        // Don't report CRC miscompare error so this test can be
        // sanity checked on fmodel on all supported connectors
        bool ignoreCrcMiscompares = false;

        // Don't run SorLoopback on these displays
        UINT32 skipDisplayMask = 0;

        // Sleep in MS after determining an error to support scope shots, etc...
        UINT32 sleepMSOnError = 0;

        // Print detailed info on error at PriNormal level
        bool printErrorDetails = false;
        bool printModifiedCRC = false;

        bool enableDisplayRegistersDump = false;
        bool enableDisplayRegistersDumpOnCrcMiscompare = false;

        // Retry reading loopback registers
        UINT32 numRetries = 0;

        // Sleep in MS with zero output before sending the patterns to support scope shots, etc...
        UINT32 initialZeroPatternMS = 0;

        // Sleep in MS with zero output after sending the patterns to support scope shots, etc...
        UINT32 finalZeroPatternMS = 0;

        // Limit test to selected SOR
        UINT32 testOnlySor = 0xFFFFFFFF;

        // Select which sublinks to test on LVDS outputs
        bool lvdsA = true;
        bool lvdsB = true;

        // Default split SOR config before SorLoopback starts
        bool enableSplitSOR = false;

        // Don't run SorLoopback on these split SOR combinations
        UINT32 skipSplitSORMask = 0;

        // Vector of skip display mask and split SOR config index
        string skipDisplayMaskBySplitSOR;

        bool skipLVDS = false;

        bool skipUPhy = true;

        bool skipLegacyTmds = false;
        bool skipFrl = false;

        // Number of cycles to run during IOBIST test
        UINT32 iobistLoopCount = 0;

        // Select the sublink for which the capture buffer should hold the value
        bool iobistCaptureBufferForSecLink = false;

        FLOAT64 timeoutMs = 5000.0f;
    };

    LoopbackTester
    (
        GpuSubdevice *pSubdev,
        Display *display,
        const SorLoopbackTestArgs& sorLoopbackTestArgs,
        Tee::Priority printPriority
    );

    virtual ~LoopbackTester() = default;

    virtual RC RunLoopbackTest
    (
        const DisplayID &displayID,
        UINT32 sorNumber,
        UINT32 protocol,
        Display::Encoder::Links link,
        UPHY_LANES lane
    );

    SETGET_PROP(FrlMode, bool);

    using DoWarsCB = std::function<RC(UINT32, bool, bool, bool)>;
    RC RegisterDoWars(DoWarsCB cb);

    using IsTestRequiredForDisplayCB = std::function<bool(UINT32, DisplayID)>;
    RC RegisterIsTestRequiredForDisplay(IsTestRequiredForDisplayCB cb);

    RC IsTestRequiredForDisplay
    (
        UINT32 loopIndex,
        const DisplayID& displayID,
        bool *pIsRequired
    );

protected:
    GpuSubdevice *m_pSubdev = nullptr;
    Display *m_pDisplay = nullptr;
    SorLoopbackTestArgs m_SorLoopbackTestArgs;
    bool m_FrlMode = false;
    std::unique_ptr<LwDispUtils::LwDispRegHal> m_LwDispRegHal;
    RegHal *m_pRegHal;

    struct PollRegValue_Args
    {
        UINT32 RegAdr;
        UINT32 ExpectedData;
        UINT32 DataMask;
        GpuSubdevice *Subdev;
    };

    static const UINT32 NUM_PATTERNS = 6;
    static const UINT64 DebugData[NUM_PATTERNS];

    INT32 VerbosePrintf(const char* format, ...) const;
    Tee::Priority GetVerbosePrintPri() const { return m_PrintPriority; };

    virtual RC EnableSORDebug(UINT32 sorNumber, UPHY_LANES lane);
    virtual RC DisableSORDebug(UINT32 sorNumber);

    UINT64 RegPairRd64(UINT32 offset);
    void RegPairWr64(UINT32 regAddr, UINT64 data);

    virtual RC SaveRegisters(UINT32 sorNumber);
    virtual RC RestoreRegisters(UINT32 sorNumber);

    void GenerateCRC(UINT32 dataIn, UINT32 *crcOut0, UINT32 *crcOut1);

    void DumpSorInfoOnCrcMismatch(UINT32 sorNumber);

    RC GetLinksUnderTestInfo
    (
        UINT32 sorNumber,
        UINT32 protocol,
        bool *pIsProtocolTMDS,
        bool *pIsLinkATestNeeded,
        bool *pIsLinkBTestNeeded
    );

    RC PollPairRegValue
    (
        UINT32 regAddr,
        UINT64 expectedData,
        UINT64 availableBits,
        FLOAT64 timeoutMs
    );

    RC PollRegValue
    (
        UINT32 regAddr,
        UINT32 expectedData,
        UINT32 dataMask,
        FLOAT64 timeoutMs
    );

    static bool GpuPollRegValue
    (
       void * Args
    );

private:
    const Tee::Priority m_PrintPriority = Tee::PriSecret;

    DoWarsCB m_DoWarsCB;
    IsTestRequiredForDisplayCB m_IsTestRequiredForDisplayCB;

    // Save state of the registers to restore them after the test is done:
    UINT32 m_SaveSorTrig = 0;
    UINT32 m_SaveSorTest = 0;
    UINT64 m_SaveSorDebugA = 0;
    UINT64 m_SaveSorDebugB = 0;
    UINT32 m_SaveSorLvds = 0;
    UINT32 m_SaveLoadAdjA = 0;
    UINT32 m_SaveLoadAdjB = 0;

    RC EnableLVDS(UINT32 sorNumber);

    RC WaitForNormalMode(UINT32 sorNumber);

    RC ProgramLoadPulsePositionAdjust(UINT32 sorNumber, UINT32 loadAdj0, UINT32 loadAdj1);
    vector<ModsGpuRegField> GetLoadAdjRegsFields(RegHal &regs);

    RC RunLoopbackTestInner
    (
        UINT32 sorNumber,
        bool isProtocolTMDS,
        bool isLinkATestNeeded,
        bool isLinkBTestNeeded,
        UPHY_LANES lane
    );

    virtual RC WritePatternAndAwaitEData
    (
        UINT32 sorNumber,
        UINT32 PatternIdx,
        bool isProtocolTMDS,
        bool isLinkATestNeeded,
        bool isLinkBTestNeeded,
        UPHY_LANES lane
    )
    {
        return RC::OK;
    }

    virtual RC MatchCrcs
    (
        UINT32 sorNumber,
        UINT32 PatternIdx,
        bool isProtocolTMDS,
        bool isLinkATestNeeded,
        bool isLinkBTestNeeded,
        UPHY_LANES lane
    )
    {
        return RC::OK;
    }

    virtual RC RecalcEncodedDataAndCrcs(UINT32 sorNumber)
    { 
        return RC::OK;
    }

    virtual void UpdateCRC() { }

    void CRCOperations(BitSet16 *cIn, BitSet16 *dIn, BitSet16 *cOut);

    RC IsSupportedSignalType
    (
        const DisplayID& displayID,
        bool *pIsSupported
    );

};

class LegacyLoopbackTester : public LoopbackTester
{
public:
    LegacyLoopbackTester
    (
        GpuSubdevice *pSubdev,
        Display *display,
        const SorLoopbackTestArgs& sorLoopbackTestArgs,
        Tee::Priority printPriority
    );

private:
    // These are recallwlated
    // Todo: Is this initialization required? Kept from legacy code as of now.
    UINT64 EncodedDataA[NUM_PATTERNS] =
        {0x0a8771b803, 0x0200400810, 0x0fc7710020, 0xaabf7ff803, 0xf83e0f83e0, 0x5555555555};
    UINT64 EncodedDataB[NUM_PATTERNS] =
        {0x0a8771b803, 0x0200400810, 0x0fc7710020, 0xaabf7ff803, 0xf83e0f83e0, 0x5555555555};
    UINT64 CrcsA[NUM_PATTERNS] =
        {0x71f5c4d38f86, 0xbadb39b6c05a, 0xc11fd5a91cdb, 0x1e4212659efc, 0x1b5275594fbc, 0x0f21dfcedfce};
    UINT64 CrcsB[NUM_PATTERNS] =
        {0x71f5c4d38f86, 0xbadb39b6c05a, 0xc11fd5a91cdb, 0x1e4212659efc, 0x1b5275594fbc, 0x0f21dfcedfce};

    static const UINT64 DebugDataLVDS[NUM_PATTERNS];
    static const UINT64 EncodedDataLVDS[NUM_PATTERNS];
    static const UINT64 CrcsLVDS[];

    RC RecalcEncodedDataAndCrcs(UINT32 sorNumber) override;

    void UpdateCRC() override;

    RC WritePatternAndAwaitEData
    (
        UINT32 sorNumber,
        UINT32 PatternIdx,
        bool isProtocolTMDS,
        bool isLinkATestNeeded,
        bool isLinkBTestNeeded,
        UPHY_LANES lane
    ) override;

    RC MatchCrcs
    (
        UINT32 sorNumber,
        UINT32 PatternIdx,
        bool isProtocolTMDS,
        bool isLinkATestNeeded,
        bool isLinkBTestNeeded,
        UPHY_LANES lane
    ) override;
};

class UPhyLoopbackTester : public LoopbackTester
{
public:
    UPhyLoopbackTester
    (
        GpuSubdevice *pSubdev,
        Display *display,
        const SorLoopbackTestArgs& sorLoopbackTestArgs,
        Tee::Priority printPriority
    );

private:
    // These are recallwlated
    UINT64 EncodedDataUPhy01[NUM_PATTERNS] = {0};
    UINT64 EncodedDataUPhy23[NUM_PATTERNS] = {0};
    UINT64 CrcsUPhy01[NUM_PATTERNS] = {0};
    UINT64 CrcsUPhy23[NUM_PATTERNS] = {0};

    UINT32 m_SavedLinkControl = 0;
    UINT32 m_SavedRxAligner = 0;
    UINT64 m_SavedDriveLwrrent = 0;
    const UINT32 m_txCtrlSequenceDelayMs = 1;

    RC SaveRegisters(UINT32 sorNumber) override;
    RC RestoreRegisters(UINT32 sorNumber) override;

    RC EnableSORDebug(UINT32 sorNumber, UPHY_LANES lane) override;
    RC PreEnableSORDebug(const UINT32 sorNumber, const UPHY_LANES lane);
    RC RxLanePowerUpHdmi(const UINT32 sorNumber);
    RC PostEnableSORDebug(UINT32 sorNumber);
    RC DisableSORDebug(UINT32 sorNumber) override;
    RC RxLanePowerDownHdmi(const UINT32 sorNumber);

    RC RecalcEncodedDataAndCrcs(UINT32 sorNumber) override;

    void UpdateCRC() override;

    RC WritePatternAndAwaitEData
    (
        UINT32 sorNumber,
        UINT32 PatternIdx,
        bool isProtocolTMDS,
        bool isLinkATestNeeded,
        bool isLinkBTestNeeded,
        UPHY_LANES lane
    ) override;
    RC PreWritePatternSetup(UINT32 sorNumber);

    RC MatchCrcs
    (
        UINT32 sorNumber,
        UINT32 PatternIdx,
        bool isProtocolTMDS,
        bool isLinkATestNeeded,
        bool isLinkBTestNeeded,
        UPHY_LANES lane
    ) override;
};

class IobistLoopbackTester : public LoopbackTester
{
public:
    IobistLoopbackTester
    (
        GpuSubdevice *pSubdev,
        Display *pDisplay,
        const SorLoopbackTestArgs& sorLoopbackTestArgs,
        Tee::Priority printPriority
    );

    RC RunLoopbackTest
    (
        const DisplayID &displayID,
        UINT32 sorNumber,
        UINT32 protocol,
        Display::Encoder::Links link,
        UPHY_LANES lane
    ) override;

private:
    RC ValidateIobistLoopCount();
    RC RunIobistSequence
    (
        UINT32 sorNumber,
        UINT32 patternIdx,
        INT32 priPadlinkNum,
        INT32 secPadlinkNum
    );
    RC EnableSORDebug(UINT32 sorNumber);
    RC RunSequenceOnEachSublink(UINT32 sorNumber, INT32 priPadlinkNum, INT32 secPadlinkNum);
    RC ConfigureSubLink
    (
        UINT32 sorNumber,
        INT32 padlinkNum,
        UINT32 sorSublink
    );
    RC RunSequenceOnEachLaneForSublink
    (
        UINT32 sorNumber,
        UINT32 sorSublink,
        INT32 priPadlinkNum,
        INT32 secPadlinkNum
    );
    RC SelectPRBSPattern(UINT32 sorNumber, UINT32 sorSublink);
    RC CleanupRegisters(UINT32 sorNumber);

    RC WriteIobistReg
    (
        ModsGpuRegAddress regAddr,
        UINT32 sorNumber,
        UINT32 data
    );

    RC ReadIobistReg
    (
        ModsGpuRegAddress regAddr,
        UINT32 sorNumber,
        UINT32 *pData
    );

    RC EnableIobistClock(UINT32 sorNumber);
    RC EnableIobistOutput(UINT32 sorNumber);
    RC ConfigureIobist(UINT32 sorNumber);
    RC SelectSorSublink(UINT32 sorNumber, INT32 priPadlinkNum, INT32 secPadlinkNum);
    RC ConnectSorSublinkToPAD(UINT32 sorNumber, INT32 padlinkNum, UINT32 sorSublinkToConfig);
    RC AllowCapture(UINT32 sorNumber);
    RC EnableAfifoAndLoopback(UINT32 sorNumber);
    RC SelectSpecifiedLane(UINT32 sorNumber, UINT32 lane);
    RC StopCapture(UINT32 sorNumber);
    RC StartTest(UINT32 sorNumber);
    RC StartDPV(UINT32 sorNumber);
    RC CheckTestResults
    (
        UINT32 sorNumber,
        UINT32 lane,
        UINT32 sorSublink,
        INT32 priPadlinkNum,
        INT32 secPadlinkNum
    );
    RC SetSeed(UINT32 sorNumber, UINT32 sorSublink);
    RC LoadSeed(UINT32 sorNumber);
    RC SelectPRBSMode(UINT32 sorNumber, UINT32 sorSublink);
};
