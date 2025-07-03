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

//! \file ibmnpulwlink.h -- Concrete implemetation for ibmnpu version of lwlink

#pragma once

#include "core/include/lwrm.h"
#include "device/interface/lwlink/lwlpowerstatecounters.h"
#include "gpu/lwlink/lwlinkimpl.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_libif.h"
#include <map>

//--------------------------------------------------------------------
//! \brief IBM NPU implementation used for P8'+
//!
class IbmNpuLwLink
    : public LwLinkImpl
    , public LwLinkPowerStateCounters

{
protected:
    IbmNpuLwLink() = default;
    virtual ~IbmNpuLwLink() {}

    RC DoClearHwErrorCounts(UINT32 linkId) override;
    RC DoDetectTopology() override;
    RC DoEnablePerLaneErrorCounts(UINT32 linkId, bool bEnable) override;
    CeWidth DoGetCeCopyWidth(CeTarget target) const override;
    RC DoGetDetectedEndpointInfo(vector<EndpointInfo> *pEndpointInfo) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoGetDiscoveredLinkMask(UINT64 *pLinkMask) override
        { *pLinkMask = (1ULL << GetMaxLinks()) - 1; return RC::OK; }
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
    ) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoGetErrorCounts(UINT32 linkId, LwLink::ErrorCounts * pErrorCounts) override;
    RC DoGetErrorFlags(ErrorFlags * pErrorFlags) override;
    RC DoGetFabricApertures(vector<FabricAperture> *pFas) override;
    void DoGetFlitInfo
    (
        UINT32 transferWidth
       ,HwType hwType
       ,bool bSysmem
       ,FLOAT64 *pReadDataFlits
       ,FLOAT64 *pWriteDataFlits
       ,UINT32 *pTotalReadFlits
       ,UINT32 *pTotalWriteFlits
    ) const override;
    UINT32 DoGetLineRateMbps(UINT32 linkId) const override
        { return UNKNOWN_RATE; }
    UINT32 DoGetLinkClockRateMhz(UINT32 linkId) const override
        { return UNKNOWN_RATE; }
    RC DoGetLinkId(const EndpointInfo &endpointInfo, UINT32 *pLinkId) const override;
    RC DoGetLinkInfo(vector<LinkInfo> *pLinkInfo) override;
    RC DoGetLinkStatus(vector<LinkStatus> *pLinkStatus) override;
    UINT32 DoGetMaxLinks() const override;
    FLOAT64 DoGetMaxObservableBwPct(LwLink::TransferType tt) override;
    UINT32 DoGetMaxPerLinkPerDirBwKiBps(UINT32 lineRateMbps) const override;
    FLOAT64 DoGetMaxTransferEfficiency() const override;
    RC DoGetOutputLinks
    (
        UINT32 inputLink
       ,UINT64 fabricBase
       ,DataType dt
       ,set<UINT32>* pPorts
    ) const override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoGetPciInfo
    (
        UINT32  linkId,
        UINT32 *pDomain,
        UINT32 *pBus,
        UINT32 *pDev,
        UINT32 *pFunc
    ) override;
    UINT32 DoGetRefClkSpeedMhz(UINT32 linkId) const override
        { return UNKNOWN_RATE; }
    bool DoHasNonPostedWrites() const override { return false; }
    RC DoInitialize(LibDevHandles handles) override;
    bool DoIsLinkActive(UINT32 linkId) const override;
    bool DoIsLinkValid(UINT32 linkId) const override;
    bool DoIsSupported(LibDevHandles handles) const override
        { MASSERT(handles.size() == 0); return true; }
    UINT32 DoGetTopologyId() const override { return m_TopologyId; }
    RC DoPerLaneErrorCountsEnabled(UINT32 linkId, bool *pbPerLaneEnabled) override;
    RC DoRegRd(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT64 *pData) override;
    RC DoRegWr(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT64 data) override;
    RC DoRegWrBroadcast(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT64 data) override
        { return RC::UNSUPPORTED_FUNCTION; }
    void DoSetCeCopyWidth(CeTarget target, CeWidth width) override;
    RC DoSetCollapsedResponses(UINT32 mask) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoSetFabricApertures(const vector<FabricAperture> &fa) override;
    RC DoSetupTopology(const vector<EndpointInfo> &endpointInfo, UINT32 topologyId) override;
    RC DoShutdown() override;
    bool DoSupportsFomMode(FomMode mode) const override { return false; }

    virtual bool DomainRequiresByteSwap(RegHalDomain domain) const;
private: //Interface Implementations
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
    UINT32 DoGetLowPowerEntryTimeMs(UINT32 linkId, LwLink::SubLinkDir dir) const override
        { return 0; }

    // LwLinkPowerStateCounters
    bool LwlPowerStateCountersAreSupported() const override { return true; }
    RC DoClearLPCounts(UINT32 linkId) override;
    RC DoGetLPEntryOrExitCount
    (
        UINT32 linkId,
        bool entry,
        UINT32 *pCount,
        bool *pbOverflow
    ) override;

private:
    bool DomainRequiresBitSwap(RegHalDomain domain) const;
    UINT32 GetBarNumber(RegHalDomain domain) const;
    UINT32 GetDataSize(RegHalDomain domain) const;
    UINT32 GetDomainBase(RegHalDomain domain, UINT32 linkId) const;
    Device::LibDeviceHandle GetLibHandle(UINT32 linkId) const;
    UINT32 GetLibLinkId(UINT32 linkId) const;
    RC GetModsLinkId(UINT32 libLinkId, UINT32 *pModsLinkId) const;
    RC GetLibLinkNumber(UINT32 linkId, UINT32 *pLinkNum) const;
    RC GetVirtMappedAddress(UINT32 barNum, UINT32 linkId, void **ppAddr) const;
    RC SavePerLaneCrcSetting();

    //! Vector of (handle, linknumber) pairs used to communicate with the
    //! library interface
    vector<pair<Device::LibDeviceHandle, UINT32>> m_LibLinkInfo;
    UINT32                   m_TopologyId = ~0U; //!< ID of this device in the topology file, note
                                                 //!< that topology ids are per device type

    UINT32 m_SavedPerLaneEnable   = ~0U; //!< Saved value for per lane counter enable
    vector<FabricAperture> m_FabricApertures; //!< Apertures in the fabric that the
                                                         //!< Device uses
};

