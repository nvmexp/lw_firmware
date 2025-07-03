/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "amperelwlink.h"
#include "core/include/tasker.h"
#include "device/interface/lwlink/lwliobist.h"
#include "device/interface/lwlink/lwlrecal.h"

class HopperLwLink : public AmpereLwLink
                   , public LwLinkIobist
                   , public LwLinkRecal
{
protected:
    HopperLwLink() = default;

    // Base implementation
    RC DoClearErrorCounts(UINT32 linkId) override;
    RC DoEnablePerLaneErrorCounts(UINT32 linkId, bool bEnable)
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoGetBlockCodeMode(UINT32 linkId, SystemBlockCodeMode *pBlockCodeMode) override;
    FLOAT64 DoGetDefaultErrorThreshold
    (
        LwLink::ErrorCounts::ErrId errId,
        bool bRateErrors
    ) const;
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
    RC DoGetErrorCounts(UINT32 linkId, LwLink::ErrorCounts * pErrorCounts) override;
    RC DoGetLineCode(UINT32 linkId, SystemLineCode *pLineCode) override;
    RC DoInitialize(LibDevHandles handles) override;
    RC DoPerLaneErrorCountsEnabled (UINT32 linkId, bool *pbPerLaneEnabled) override;
    bool DoSupportsErrorCaching() const override
        { return true; }
    bool DoSupportsFomMode(FomMode mode) const override;

    RC GetEomConfigValue
    (
        FomMode mode,
        UINT32 numErrors,
        UINT32 numBlocks,
        UINT32 *pConfigVal
    ) const override;
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

    // LwLinkIobist implementation
    void DoGetIobistErrorFlags
    (
        UINT64 linkMask,
        set<LwLinkIobist::ErrorFlag> * pErrorFlags
    ) const;
    void DoSetIobistInitiator(UINT64 linkMask, bool bInitiator) override;
    RC DoSetIobistTime(UINT64 linkMask, IobistTime iobistTime) override;
    void DoSetIobistType(UINT64 linkMask, IobistType iobistType) override;

private:
     UINT32 GetErrorCounterControlMask(bool bIncludePerLane) override;

    // Do nothing, this will prevent anything from being saved therefore
    // during shutdown nothing will be restored (hopper does not support the
    // TXLANECRC register
    RC SavePerLaneCrcSetting() override { return RC::OK; }

    Tasker::Mutex       m_ErrorCounterMutex = Tasker::NoMutex();
    vector<ErrorCounts> m_CachedErrorCounts;
};
