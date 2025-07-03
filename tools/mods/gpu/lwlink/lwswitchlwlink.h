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

//! \file lwswitchlwlink.h -- Concrete implemetation for lwswitch version of lwlink

#pragma once

#include "core/include/lwrm.h"
#include "core/include/tasker.h"
#include "device/interface/lwlink/lwlinkuphy.h"
#include "device/interface/lwlink/lwllatencybincounters.h"
#include "device/interface/lwlink/lwlpowerstatecounters.h"
#include "gpu/lwlink/lwlinkimpl.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_fabricmanager_libif.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_libif.h"
#include <map>

//--------------------------------------------------------------------
//! \brief LwSwitch implementation used for Willow+
//!
class LwSwitchLwLink
    : public LwLinkImpl
    , public LwLinkPowerStateCounters
    , public LwLinkLatencyBinCounters
    , public LwLinkUphy
{
protected:
    LwSwitchLwLink() = default;
    virtual ~LwSwitchLwLink() {}

    RC DoClearErrorCounts(UINT32 linkId) override;
    RC DoClearHwErrorCounts(UINT32 linkId) override;
    RC DoConfigurePorts(UINT32 topologyId) override;
    RC DoDetectTopology() override;
    RC DoEnablePerLaneErrorCounts(UINT32 linkId, bool bEnable) override;
    CeWidth DoGetCeCopyWidth(CeTarget target) const override;
    RC DoGetDetectedEndpointInfo(vector<EndpointInfo> *pEndpointInfo) override;
    RC DoGetDiscoveredLinkMask(UINT64 *pLinkMask) override
        { *pLinkMask = GetMaxLinkMask(); return RC::OK; }
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
    RC DoGetErrorFlags(ErrorFlags * pErrorFlags) override;
    UINT32 DoGetFabricRegionBits() const override { return 34; }
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
    string DoGetLinkGroupName() const override { return "LWLW"; }
    RC DoGetLinkId(const EndpointInfo &endpointInfo, UINT32 *pLinkId) const override;
    RC DoGetLinkInfo(vector<LinkInfo> *pLinkInfo) override;
    RC DoGetLinkStatus(vector<LinkStatus> *pLinkStatus) override;
    UINT32 DoGetMaxLinkGroups() const override { return 9; } // Willow has 9 pairs of links
    UINT32 DoGetMaxLinks() const override { return 18; }
    FLOAT64 DoGetMaxObservableBwPct(LwLink::TransferType tt) override;
    FLOAT64 DoGetMaxTransferEfficiency() const override;
    RC DoGetOutputLinks
    (
        UINT32 inputLink
       ,UINT64 fabricBase
       ,DataType dt
       ,set<UINT32>* pPorts
    ) const override;
    RC DoGetPciInfo
    (
        UINT32  linkId,
        UINT32 *pDomain,
        UINT32 *pBus,
        UINT32 *pDev,
        UINT32 *pFunc
    ) override;
    UINT32 DoGetTopologyId() const override { return m_TopologyId; }
    bool DoHasNonPostedWrites() const override { return true; }
    RC DoInitialize(LibDevHandles handles) override;
    RC DoInitializePostDiscovery() override;
    bool DoIsLinkAcCoupled(UINT32 linkId) const override;
    bool DoIsSupported(LibDevHandles handles) const override
        { MASSERT(handles.size() == 0); return true; }
    RC DoPerLaneErrorCountsEnabled(UINT32 linkId, bool *pbPerLaneEnabled) override;
    RC DoRegRd(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT64 *pData) override;
    RC DoRegWr(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT64 data) override;
    RC DoRegWrBroadcast(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT64 data) override;
    RC DoResetTopology() override;
    void DoSetCeCopyWidth(CeTarget target, CeWidth width) override;
    RC DoSetCollapsedResponses(UINT32 mask) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoSetFabricApertures(const vector<FabricAperture> &fas) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoSetNearEndLoopbackMode(UINT64 linkMask, NearEndLoopMode loopMode);
    RC DoSetupTopology(const vector<EndpointInfo> &endpointInfo, UINT32 topologyId) override;
    RC DoShutdown() override;
    bool DoSupportsFomMode(FomMode mode) const override;
    bool DoSupportsErrorCaching() const override
        { return true; }

    ErrorCounts::ErrId ErrorCounterBitToId(UINT32 counterBit) override;
    UINT32 GetErrorCounterControlMask(bool bIncludePerLane) override;
    void SetTopologyId(UINT32 topologyId) { m_TopologyId = topologyId; }

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
    UINT32 DoGetLowPowerEntryTimeMs(UINT32 linkId, LwLink::SubLinkDir dir) const override;

    // LwLinkLatencyBinCounters Implementation
    RC DoSetupLatencyBins(const LatencyBins &latencyBins) const override;
    RC DoClearLatencyBinCounts() const override;
    RC DoGetLatencyBinCounts(vector<vector<LatencyCounts>> *pLatencyCounts) const override;

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

    // LwLinkUphy
    UINT32 DoGetActiveLaneMask(RegBlock regBlock) const override;
    UINT32 DoGetMaxLanes(RegBlock regBlock) const override;
    UINT32 DoGetMaxRegBlocks(RegBlock regBlock) const override;
    bool DoGetRegLogEnabled() const override { return m_bUphyRegLogEnabled; }
    UINT64 DoGetRegLogRegBlockMask(RegBlock regBlock) override;
    Version DoGetVersion() const override { return UphyIf::Version::V30; }
    RC DoIsRegBlockActive(RegBlock regBlock, UINT32 blockIdx, bool *pbActive) override;
    bool DoIsUphySupported() const override { return true; }
    RC DoReadPadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 * pData) override;
    RC DoWritePadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 data) override;
    RC DoReadPllReg(UINT32 ioctl, UINT16 addr, UINT16 * pData) override;
    RC DoSetRegLogLinkMask(UINT64 linkMask) override;
    void DoSetRegLogEnabled(bool bEnabled) override { m_bUphyRegLogEnabled = bEnabled; }

    virtual FLOAT64 GetLinkDataRateEfficiency(UINT32 linkId) const;

protected:
    Device::LibDeviceHandle GetLibHandle() const { return m_LibHandle; } // $
    void SetLibHandle(Device::LibDeviceHandle handle) { m_LibHandle = handle; } // $

    ErrorCounts * GetCachedErrorCounts(UINT32 linkId);
    void * GetErrorCountMutex() { return m_ErrorCounterMutex.get(); }

    UINT32 m_SavedReplayThresh    = ~0U;
    UINT32 m_SavedSL1ErrCountCtrl = ~0U;

private:
    UINT32 GetDomainId(RegHalDomain domain);
    virtual RC SavePerLaneCrcSetting();

    Device::LibDeviceHandle m_LibHandle = Device::ILWALID_LIB_HANDLE; //!< Handle to the library
    vector<EndpointInfo> m_EndpointInfo;          //!< Endpoint info for each link on
                                                             //!< the device
    UINT32 m_TopologyId = ~0U; //!< ID of this device in the topology file, note
                               //!< that topology ids are per device type

    UINT32 m_SavedPerLaneEnable   = ~0U;  //!< Saved value of per-lane enable
    vector<EndpointInfo> m_CachedDetectedEndpointInfo; //!< This information shouldn't change
                                                      //!< during exelwtion so only retrieve it once
    UINT64 m_UphyRegsLinkMask      = 0ULL;
    UINT32 m_UphyRegsIoctlMask     = ~0U;
    bool   m_bUphyRegLogEnabled    = false;

    Tasker::Mutex       m_ErrorCounterMutex = Tasker::NoMutex();
    vector<ErrorCounts> m_CachedErrorCounts;
};
