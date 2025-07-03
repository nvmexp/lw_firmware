
/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2022 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "limerocklwlink.h"
#include "device/interface/lwlink/lwliobist.h"
#include "device/interface/lwlink/lwlmulticast.h"
#include "device/interface/lwlink/lwltrex.h"
#include "device/interface/lwlink/lwlrecal.h"

class LagunaLwLink : public LimerockLwLink
                   , public LwLinkMulticast
                   , public LwLinkIobist
                   , public LwLinkRecal
{
protected:
    LagunaLwLink() = default;

    // Base implementation
    RC DoEnablePerLaneErrorCounts(UINT32 linkId, bool bEnable)
        { return RC::UNSUPPORTED_FUNCTION; }
    FLOAT64 DoGetDefaultErrorThreshold(LwLink::ErrorCounts::ErrId errId, bool bRateErrors) const;
    RC DoGetLineCode(UINT32 linkId, SystemLineCode *pLineCode) override;
    RC DoGetBlockCodeMode(UINT32 linkId, SystemBlockCodeMode *pBlockCodeMode) override;
    void DoGetEomDefaultSettings(UINT32 link, EomSettings* pSettings) const override;
    RC DoGetEomStatus
    (
        FomMode mode
       ,UINT32 link
       ,UINT32 numErrors
       ,UINT32 numBlocks
       ,FLOAT64 timeoutMs
       ,vector<INT32>* status
    ) override;
    UINT32 DoGetFabricRegionBits() const override { return 39; }
    UINT32 DoGetMaxLinks() const override { return 64; }
    UINT64 DoGetMaxLinkMask() const override { return 0xffffffffffffffffULL; }
    RC DoPerLaneErrorCountsEnabled (UINT32 linkId, bool *pbPerLaneEnabled) override;
    RC DoSetCollapsedResponses(UINT32 mask) override;
    RC DoSetCompressedResponses(bool bEnable) override { return RC::OK; }

    RC InitializeTrex(UINT32 linkId) override;
    virtual RC GetTrexReductionOpEncode(LwLinkTrex::ReductionOp op, UINT32* pCode) const;
    RC GetEomConfigValue
    (
        FomMode mode,
        UINT32 numErrors,
        UINT32 numBlocks,
        UINT32 *pConfigVal
    ) const override;
    UINT32 GetErrorCounterControlMask(bool bIncludePerLane) override;

private:
    // LwLinkPowerStateCounters implementation
    RC DoClearLPCounts(UINT32 linkId);
    RC DoGetLPEntryOrExitCount
    (
        UINT32 linkId,
        bool entry,
        UINT32 *pCount,
        bool *pbOverflow
    );

    // LwLinkRecal implementation
    UINT32 DoGetRecalStartTimeUs(UINT32 linkId) const override;
    UINT32 DoGetRecalWakeTimeUs(UINT32 linkId) const override;
    UINT32 DoGetMinRecalTimeUs(UINT32 linkId) const override;

    // LwlPowerState implementation
    RC DoGetLinkPowerStateStatus
    (
        UINT32 linkId,
        LwLinkPowerState::LinkPowerStateStatus *pLinkPowerStatus
    ) const override;
    RC DoRequestPowerState
    (
        UINT32 linkId
      , LwLinkPowerState::SubLinkPowerState state
      , bool bHwControlled
    ) override;
    UINT32 DoGetLowPowerEntryTimeMs(UINT32 linkId, LwLink::SubLinkDir dir) const override;
    bool DoSupportsLpHwToggle() const override { return true; }
    RC DoStartPowerStateToggle(UINT32 linkId, UINT32 countIn, UINT32 countOut) override;

    // UPHY implementation
    Version DoGetVersion() const override { return UphyIf::Version::V61; }
    UINT32 DoGetMaxRegBlocks(RegBlock regBlock) const override;
    RC DoIsRegBlockActive(RegBlock regBlock, UINT32 blockIdx, bool *pbActive) override;
    RC DoWritePadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 data) override;
    RC DoReadPllReg(UINT32 ioctl, UINT16 addr, UINT16 * pData) override;

    // TREX implementation
    RC DoCreateRamEntry
    (
        LwLinkTrex::RamEntryType type,
        LwLinkTrex::ReductionOp op,
        UINT32 route,
        UINT32 dataLen,
        RamEntry* pEntry
    ) override;
    bool DoIsTrexDone(UINT32 linkId) override;
    RC DoSetLfsrEnabled(UINT32 linkId, bool bEnabled) override;
    RC DoSetTrexRun(bool bStart) override;
    RC DoSetQueueRepeatCount(UINT32 linkId, UINT32 repeatCount) override;
    RC DoWriteTrexRam
    (
        UINT32 linkId,
        LwLinkTrex::TrexRam ram,
        const vector<LwLinkTrex::RamEntry>& entries
    ) override;

    // LwLinkMulticast implementation
    RC DoGetResidencyCounts(UINT32 linkId, UINT32 mcId, UINT64* pMcCount, UINT64* pRedCount) override;
    RC DoGetResponseCheckCounts(UINT32 linkId, UINT64* pPassCount, UINT64* pFailCount) override;
    RC DoResetResponseCheckCounts(UINT32 linkId) override;
    RC DoStartResidencyTimers(UINT32 linkId) override;
    RC DoStopResidencyTimers(UINT32 linkId) override;

    // LwLinkIobist implementation
    void DoGetIobistErrorFlags
    (
        UINT64 linkMask,
        set<LwLinkIobist::ErrorFlag> * pErrorFlags
    ) const override;
    void DoSetIobistInitiator(UINT64 linkMask, bool bInitiator) override;
    RC DoSetIobistTime(UINT64 linkMask, IobistTime iobistTime) override;
    void DoSetIobistType(UINT64 linkMask, IobistType iobistType) override;

private:
    // Do nothing, this will prevent anything from being saved therefore
    // during shutdown nothing will be restored (hopper does not support the
    // TXLANECRC register
    RC SavePerLaneCrcSetting() override { return RC::OK; }
};
