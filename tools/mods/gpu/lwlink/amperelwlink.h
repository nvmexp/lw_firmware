/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "turinglwlink.h"
#include "device/interface/lwlink/lwlfla.h"

class AmpereLwLink :
    public TuringLwLink
   ,public LwLinkFla
{
protected:
    AmpereLwLink() = default;

    void SetLinkInfo
    (
        UINT32                                      linkId,
        const LW2080_CTRL_LWLINK_LINK_STATUS_INFO & rmLinkInfo,
        LinkInfo *                                  pLinkInfo
    ) const override;

    void LoadLwLinkCaps() override;

    RC DoClearHwErrorCounts(UINT32 linkId) override;
    RC DoGetErrorCounts(UINT32 linkId, LwLink::ErrorCounts * pErrorCounts) override;
    RC DoGetLineCode(UINT32 linkId, SystemLineCode *pLineCode) override;
    RC DoGetBlockCodeMode(UINT32 linkId, SystemBlockCodeMode *pBlockCodeMode) override;
    RC DoInitialize(LibDevHandles handles) override;
    bool DoIsLinkAcCoupled(UINT32 linkId) const override;
    RC GetEomConfigValue
    (
        FomMode mode,
        UINT32 numErrors,
        UINT32 numBlocks,
        UINT32 *pConfigVal
    ) const override;
private:
    RC DoEnablePerLaneErrorCounts(UINT32 linkId, bool bEnable) override;
    FLOAT64 DoGetDefaultErrorThreshold
    (
        LwLink::ErrorCounts::ErrId errId,
        bool bRateErrors
    ) const;
    void DoGetEomDefaultSettings
    (
        UINT32 link,
        EomSettings* pSettings
    ) const override;
    RC DoGetEomStatus
    (
        FomMode mode,
        UINT32             link,
        UINT32             numErrors,
        UINT32             numBlocks,
        FLOAT64            timeoutMs,
        vector<INT32>    * status
    ) override;
    RC DoGetErrorFlags(ErrorFlags * pErrorFlags) override;
    RC DoGetFomStatus(vector<PerLaneFomData> * pFomData) override;
    UINT32 DoGetLinkDataRateKiBps(UINT32 linkId) const override;
    UINT32 DoGetLinksPerGroup() const override;
    UINT32 DoGetMaxLinkDataRateKiBps(UINT32 linkId) const override;
    UINT32 DoGetMaxLinkGroups() const override;
    FLOAT64 DoGetMaxObservableBwPct(LwLink::TransferType tt) override;
    SystemLineRate DoGetSystemLineRate(UINT32 linkId) const override;
    RC DoGetXbarBandwidth
    (
        bool bWriting,
        bool bReading,
        bool bWrittenTo,
        bool bReadFrom,
        UINT32 *pXbarBwKiBps
    ) override;
    bool DoHasCollapsedResponses() const override;
    bool DoHasNonPostedWrites() const override { return true; }
    RC DoIsPerLaneEccEnabled(UINT32 linkId, bool *pbPerLaneEccEnabled) const override;
    RC DoPerLaneErrorCountsEnabled(UINT32 linkId, bool *pbPerLaneEnabled) override;
    void DoSetCeCopyWidth(CeTarget target, CeWidth width) override;
    RC DoSetCollapsedResponses(UINT32 mask) override;
    RC DoSetSystemParameter(UINT64 linkMask, SystemParameter parm, SystemParamValue val) override;
    bool DoSupportsFomMode(FomMode mode) const override;

    // LwLinkPowerState
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

    // LwLinkPowerStateCounters
    RC DoClearLPCounts(UINT32 linkId) override;
    RC DoGetLPEntryOrExitCount
    (
        UINT32 linkId,
        bool entry,
        UINT32 *pCount,
        bool *pbOverflow
    ) override;

    // LwLinkSleepState
    bool DoIsSleepStateSupported() const { return false; }

    // LwLinkFla
    bool DoGetFlaCapable() override;
    bool DoGetFlaEnabled() override;

    // LwLinkUphy
    Version DoGetVersion() const override { return UphyIf::Version::V50; }
    bool DoIsUphySupported() const override { return true; }
    RC DoWritePadLaneReg(UINT32 slinkId, UINT32 lane, UINT16 addr, UINT16 data) override;
    RC DoReadPllReg(UINT32 ioctl, UINT16 addr, UINT16 * pData) override;

    UINT32 GetDomainBase(RegHalDomain domain, UINT32 linkId) override;
    UINT32 GetErrorCounterControlMask(bool bIncludePerLane) override;
    bool HasLPCountOverflowed(bool bEntry, UINT32 lpCount) const override;

    UINT32         m_SavedPerLaneEnable   = ~0U;      //!< Saved value of per-lane enable
    UINT32         m_SavedReplayThresh    = ~0U;      //!< Saved value of the replay threshold
                                                      //!< (MODS needs to change the threshold
                                                      //!< for testing)
    UINT32         m_SavedLinkTimeout     = ~0U;      //!< Saved value of the link timeout
                                                      //!<(MODS needs to change the timeout
                                                      //!< for testing)
    UINT32         m_SavedErrCountCtrl    = ~0U;
    bool           m_bFlaCapable          = true;
    bool           m_bFlaCapableInit      = false;
    bool           m_bFlaEnabled          = true;
    UINT64         m_FlaBase              = 0ULL;
    UINT64         m_FlaSize              = 0ULL;
    static const ErrorFlagDefinition s_ErrorFlags;
};
