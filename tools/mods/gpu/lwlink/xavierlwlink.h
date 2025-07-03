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

#pragma once

#include "device/interface/lwlink/lwlpowerstatecounters.h"
#include "gpu/lwlink/lwlinkimpl.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_libif.h"

//--------------------------------------------------------------------
//! \brief Xavier base implementation used for T194
//!
class XavierLwLink
  : public LwLinkImpl
  , public LwLinkPowerStateCounters
{
protected:
    XavierLwLink() { }
    virtual ~XavierLwLink() {}

    bool DoIsSupported(LibDevHandles handles) const override;
    RC DoShutdown() override;
    Device::LibDeviceHandle GetLibHandle() const { return m_LibHandle; }
private:
    RC SavePerLaneCrcSetting();

    RC DoClearHwErrorCounts(UINT32 linkId) override;
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
    RC DoGetLinkId(const EndpointInfo &endpointInfo, UINT32 *pLinkId) const override;
    RC DoGetLinkInfo(vector<LinkInfo> *pLinkInfo) override;
    RC DoGetLinkStatus(vector<LinkStatus> *pLinkStatus) override;
    UINT32 DoGetMaxLinks() const override;
    FLOAT64 DoGetMaxObservableBwPct(LwLink::TransferType tt) override;
    FLOAT64 DoGetMaxTransferEfficiency() const override;
    RC DoGetOutputLinks
    (
        UINT32 inputLink
       ,UINT64 fabricBase
       ,DataType dt
       ,set<UINT32>* pPorts
    ) const override
        { return RC::UNSUPPORTED_FUNCTION; }
    UINT32 DoGetTopologyId() const override { return m_TopologyId; }
    RC DoInitialize(LibDevHandles handles) override;
    RC DoPerLaneErrorCountsEnabled(UINT32 linkId, bool *pbPerLaneEnabled) override;
    RC DoRegRd(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT64 *pData) override;
    RC DoRegWr(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT64 data) override;
    RC DoRegWrBroadcast(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT64 data) override
        { return RC::UNSUPPORTED_FUNCTION; }
    void DoSetCeCopyWidth(CeTarget target, CeWidth width) override;
    RC DoSetCollapsedResponses(UINT32 mask) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoSetFabricApertures(const vector<FabricAperture> &fas) override;
    RC DoSetupTopology(const vector<EndpointInfo> &endpointInfo, UINT32 topologyId) override;
    bool DoSupportsFomMode(FomMode mode) const override;
    RC DoWaitForLinkInit() override;

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

    LwLinkDevIf::LibIfPtr GetLwLinkLibIf();
    void LoadLwLinkCaps(Device::LibDeviceHandle handle);
    RC GetDomainPtr(RegHalDomain domain, UINT08 **ppRegs);

    RC ChangeLinkState
    (
        SubLinkState fromState
       ,SubLinkState toState
       ,const EndpointInfo &locEp
       ,const EndpointInfo &remEp
    ) override;

    virtual LwLinkDevIf::LibIfPtr GetTegraLwLinkLibIf() = 0;
    virtual RC GetRegPtr(void **ppRegs) = 0;

    vector<FabricAperture> m_FabricApertures; //!< Apertures in the fabric that the
                                                         //!< Device uses
    UINT32 m_TopologyId           = ~0U; //!< ID of this device in the topology file, note
                                         //!< that topology ids are per device type
    UINT32 m_MaxLinks             = 0;
    bool   m_bLwCapsLoaded        = false;
    RC     m_LoadLwLinkCapsRc     = RC::OK; //!< RC value from LoadLwLinkCaps
    bool   m_bSupported           = true;
    bool   m_bConnDetectSupported = false;
    UINT32 m_ThroughputCountersMask     = 0;
    UINT32 m_bThroughputCountersRunning = false;
    UINT32 m_SavedPerLaneEnable         = ~0U;  //!< Saved value of per-lane enable
    void * m_pRegs                      = nullptr;
    Device::LibDeviceHandle m_LibHandle = Device::ILWALID_LIB_HANDLE; //!< Handle to the library
};
