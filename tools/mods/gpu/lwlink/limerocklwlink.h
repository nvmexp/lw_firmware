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

#pragma once

#include "device/interface/lwlink/lwltrex.h"
#include "device/interface/lwlink/lwlcci.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_fabricmanager_libif.h"
#include "lwswitchlwlink.h"

class LimerockLwLink : public LwSwitchLwLink,
                       public LwLinkTrex,
                       public LwLinkCableController
{
protected:
    LimerockLwLink() = default;
    virtual ~LimerockLwLink() = default;

    RC DoAssertBufferReady(UINT32 linkId, bool ready) override;
    RC DoConfigurePorts(UINT32 topologyId) override;
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
        FomMode mode
       ,UINT32 link
       ,UINT32 numErrors
       ,UINT32 numBlocks
       ,FLOAT64 timeoutMs
       ,vector<INT32>* status
    ) override;
    RC DoGetErrorCounts(UINT32 linkId, LwLink::ErrorCounts * pErrorCounts) override;
    UINT32 DoGetFabricRegionBits() const override { return 36; }
    RC DoGetFomStatus(vector<PerLaneFomData> * pFomData) override;
    UINT32 DoGetMaxLinks() const override { return 36; }
    RC DoGetOutputLinks
    (
        UINT32 inputLink
       ,UINT64 fabricBase
       ,DataType dt
       ,set<UINT32>* pPorts
    ) const override;
    UINT32 DoGetLinkDataRateKiBps(UINT32 linkId) const override;
    RC DoGetLinkInfo(vector<LinkInfo> *pLinkInfo) override;
    UINT32 DoGetMaxLinkDataRateKiBps(UINT32 linkId) const override;
    FLOAT64 DoGetMaxObservableBwPct(LwLink::TransferType tt) override { return 1.0; }
    UINT32 DoGetMaxPerLinkPerDirBwKiBps(UINT32 lineRateMbps) const;
    UINT32 DoGetMaxTotalBwKiBps(UINT32 lineRateMbps) const;
    SystemLineRate DoGetSystemLineRate(UINT32 linkId) const override;
    bool DoHasCollapsedResponses() const override;
    RC DoInitialize(const vector<LwLinkDevIf::LibInterface::LibDeviceHandle> &handles) override;
    RC DoInitializePostDiscovery() override;
    bool DoIsLinkAcCoupled(UINT32 linkId) const override;
    bool DoIsLinkPam4(UINT32 linkId) const override;
    RC DoIsPerLaneEccEnabled(UINT32 linkId, bool *pbPerLaneEccEnabled) const override;
    RC DoPerLaneErrorCountsEnabled(UINT32 linkId, bool *pbPerLaneEnabled) override;
    RC DoResetTopology() override;
    RC DoSetCollapsedResponses(UINT32 mask) override;
    RC DoSetCompressedResponses(bool bEnable) override;
    RC DoSetDbiEnable(bool bEnable) override;
    bool DoSupportsFomMode(FomMode mode) const override;
    RC DoWaitForIdle(FLOAT64 timeoutMs) override;

    RC ChangeLinkState
    (
        SubLinkState fromState,
        SubLinkState toState,
        const vector<pair<EndpointInfo, EndpointInfo>>& epPairs
    ) override;
    UINT32 GetErrorCounterControlMask(bool bIncludePerLane) override;

    LwLinkTrex::DataPattern GetTrexPattern() const { return m_TrexPattern; }

    virtual RC InitializeTrex(UINT32 linkId);
    virtual RC GetTrexDataLenEncode(UINT32 dataLen, UINT32* pCode) const;
    virtual RC GetEomConfigValue
    (
        FomMode mode,
        UINT32 numErrors,
        UINT32 numBlocks,
        UINT32 *pConfigVal
    ) const;

protected: // Interface Implementations
    // LwLinkPowerStateCounters
    RC DoClearLPCounts(UINT32 linkId);
    RC DoGetLPEntryOrExitCount
    (
        UINT32 linkId,
        bool entry,
        UINT32 *pCount,
        bool *pbOverflow
    );

    // LwLinkPowerState
    bool LwlPowerStateIsSupported() const override { return true; }
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

    // LwLinkTrex
    RC DoCreateRamEntry
    (
        LwLinkTrex::RamEntryType type,
        LwLinkTrex::ReductionOp op,
        UINT32 route,
        UINT32 dataLen,
        RamEntry* pEntry
    ) override;
    RC DoGetPacketCounts(UINT32 linkId, UINT64* pReqCount, UINT64* pRspCount) override;
    UINT32 DoGetRamDepth() const override;
    LwLinkTrex::NcisocDst DoGetTrexDst() const override { return m_TrexDst; }
    LwLinkTrex::NcisocSrc DoGetTrexSrc() const override { return m_TrexSrc; }
    bool DoIsTrexDone(UINT32 linkId) override;
    RC DoSetLfsrEnabled(UINT32 linkId, bool bEnabled) override { return RC::UNSUPPORTED_FUNCTION; }
    RC DoSetRamWeight(UINT32 linkId, LwLinkTrex::TrexRam ram, UINT32 weight) override;
    RC DoSetTrexDataPattern(LwLinkTrex::DataPattern pattern) override { m_TrexPattern = pattern; return RC::OK; }
    RC DoSetTrexDst(LwLinkTrex::NcisocDst dst) override { m_TrexDst = dst; return RC::OK; }
    RC DoSetTrexRun(bool bStart) override;
    RC DoSetTrexSrc(LwLinkTrex::NcisocSrc src) override { m_TrexSrc = src; return RC::OK; }
    RC DoSetQueueRepeatCount(UINT32 linkId, UINT32 repeatCount) override;
    RC DoSetSystemParameter(UINT64 linkMask, SystemParameter parm, SystemParamValue val) override;
    RC DoWriteTrexRam
    (
        UINT32 linkId,
        LwLinkTrex::TrexRam ram,
        const vector<LwLinkTrex::RamEntry>& entries
    ) override;

    // LwLinkUphy
    Version DoGetVersion() const override { return UphyIf::Version::V50; }
    RC DoWritePadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 data) override;
    RC DoReadPllReg(UINT32 ioctl, UINT16 addr, UINT16 * pData) override;

    // LwLinkCableController
    UINT64 DoGetCCBiosLinkMask() const override { return m_CableControllerBiosLinkMask; }
    UINT32 DoGetCCCount() const override;
    UINT32 DoGetCciIdFromLink(UINT32 linkId) const override;
    RC DoGetCCTempC(UINT32 cciId, FLOAT32 *pTemp) const override;
    RC DoGetCCErrorFlags(CCErrorFlags * pErrorFlags) const override;
    const CCFirmwareInfo & DoGetCCFirmwareInfo(UINT32 cciId, CCFirmwareImage image) const override;
    RC DoGetCCGradingValues(UINT32 cciId, vector<GradingValues> * pGradingValues) const override;
    UINT32 DoGetCCHostLaneCount(UINT32 cciId) const override;
    string DoGetCCHwRevision(UINT32 cciId) const override;
    RC DoGetCarrierFruEepromData(vector<CarrierFruInfo>* carrierInfo) override;
    CCInitStatus DoGetCCInitStatus(UINT32 cciId) const override;
    UINT64 DoGetCCLinkMask(UINT32 cciId) const override;
    CCModuleIdentifier DoGetCCModuleIdentifier(UINT32 cciId) const override;
    UINT32 DoGetCCModuleLaneCount(UINT32 cciId) const override;
    CCModuleMediaType DoGetCCModuleMediaType(UINT32 cciId) const override;
    RC DoGetCCModuleState(UINT32 cciId, ModuleState * pStateValue) const override;
    string DoGetCCPartNumber(UINT32 cciId) const override;
    UINT32 DoGetCCPhysicalId(UINT32 cciId) const override;
    string DoGetCCSerialNumber(UINT32 cciId) const override;
    RC DoGetCCVoltage(UINT32 cciId, UINT16 * pVoltageMv) const override;
    const CarrierFruInfos & DoGetCarrierFruInfo() const override { return m_CarrierInfo; }
    RC InitializeCableController() override;

    RC DoGetLineCode(UINT32 linkId, SystemLineCode *pLineCode) override;
    RC DoGetBlockCodeMode(UINT32 linkId, SystemBlockCodeMode *pBlockCodeMode) override;

private:
    RC GetErrorCountsDirect(UINT32 linkId, LwLink::ErrorCounts * pErrorCounts);
    RC GetPerPortCarrierFruEepromData(TestDevice* pDev, UINT08 portNum, UINT32 portAddr,
                                      CarrierFruInfo* carrierFruInfo);
    string GetCarrierBoardFruVer(vector<UINT32>& epromData);
    string GetCarrierBoardPartNum(vector<UINT32>& epromData);
    string GetCarrierBoardHwRev(vector<UINT32>& epromData);
    string GetCarrierBoardSerial(vector<UINT32>& epromData);
    string GetCarrierBoardProductName(vector<UINT32>& epromData);
    string GetCarrierBoardMfgName(vector<UINT32>& epromData);
    string GetCarrierBoardMfgDate(vector<UINT32>& epromData);

    FLOAT64 GetLinkDataRateEfficiency(UINT32 linkId) const override;

    UINT64 m_UphyRegsLinkMask = 0ULL;
    LwLinkTrex::DataPattern m_TrexPattern = LwLinkTrex::DP_0_F;
    LwLinkTrex::NcisocDst m_TrexDst = LwLinkTrex::DST_TX;
    LwLinkTrex::NcisocSrc m_TrexSrc = LwLinkTrex::SRC_EGRESS;

    // LwLinkCableController data
    struct CableControllerInfo
    {
        CCInitStatus              initStatus       = CCInitStatus::Unknown;
        UINT08                    hostLaneCount    = 0;
        UINT08                    moduleLaneCount  = 0;
        CCModuleMediaType         moduleMediaType  = CCModuleMediaType::Unknown;
        CCModuleIdentifier        moduleIdentifier = CCModuleIdentifier::Unknown;
        UINT64                    linkMask         = 0;
        UINT32                    physicalId       = ~0U;
        CCFirmwareInfo            factoryFwInfo;
        CCFirmwareInfo            imageAFwInfo;
        CCFirmwareInfo            imageBFwInfo;
        string                    serialNumber;
        string                    partNumber;
        string                    hwRev;
    };
    vector<CableControllerInfo> m_CableControllerInfo;
    UINT64                      m_CableControllerBiosLinkMask = 0;
    const CableControllerInfo * GetCableControllerInfo(UINT32 linkId) const;
    CarrierFruInfos             m_CarrierInfo;
};
