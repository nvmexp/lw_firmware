/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file pascallwlink.h -- Concrete implemetation for pascal version of lwlink

#pragma once

#ifndef INCLUDED_PASCALLWLINK_H
#define INCLUDED_PASCALLWLINK_H

#include "core/include/lwrm.h"
#include "core/include/tasker.h"
#include "gpu/lwlink/lwlinkimpl.h"
#include "ctrl/ctrl2080/ctrl2080lwlink.h"

#include <map>

class GpuSubdevice;

//--------------------------------------------------------------------
//! \brief Pascal implementation used for GP100+
//!
class PascalLwLink : public LwLinkImpl
{
protected:
    PascalLwLink() = default;
    virtual ~PascalLwLink() {}

    RC DoClearHwErrorCounts(UINT32 linkId) override;
    RC DoDetectTopology() override;
    RC DoEnablePerLaneErrorCounts(UINT32 linkId, bool bEnable) override;
    CeWidth DoGetCeCopyWidth(CeTarget target) const override;
    RC DoGetDetectedEndpointInfo(vector<EndpointInfo> *pEndpointInfo) override;
    RC DoGetDiscoveredLinkMask(UINT64 *pLinkMask) override;
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
    UINT32 DoGetLinksPerGroup() const override;
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
    bool DoHasNonPostedWrites() const override { return false; }
    RC DoInitialize(LibDevHandles handles) override;
    RC DoInitializePostDiscovery() override;
    bool DoIsSupported(LibDevHandles handles) const override;
    RC DoPerLaneErrorCountsEnabled(UINT32 linkId, bool *pbPerLaneEnabled) override;
    RC DoRegRd(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT64 *pData) override;
    RC DoRegWr(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT64 data) override;
    RC DoRegWrBroadcast(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT64 data) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoReleaseCounters() override;
    RC DoReserveCounters() override;
    RC DoResetTopology() override;
    void DoSetCeCopyWidth(CeTarget target, CeWidth width) override;
    RC DoSetCollapsedResponses(UINT32 mask) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoSetFabricApertures(const vector<FabricAperture> &fas) override;
    RC DoSetupTopology(const vector<EndpointInfo> &endpointInfo, UINT32 topologyId) override;
    RC DoShutdown() override;
    bool DoSupportsFomMode(FomMode mode) const override;

protected:
    static const UINT32 ILWALID_DOMAIN_BASE = ~0U;
    GpuSubdevice * GetGpuSubdevice();
    const GpuSubdevice * GetGpuSubdevice() const;
    LwRm::Handle GetProfilerHandle() const { return m_hProfiler; }

    virtual void SetLinkInfo
    (
        UINT32                                      linkId,
        const LW2080_CTRL_LWLINK_LINK_STATUS_INFO & rmLinkInfo,
        LinkInfo *                                  pLinkInfo
    ) const;
    class ReserveLwLinkCounters
    {
        public:
            explicit ReserveLwLinkCounters(LwLink *pLwLink) : m_pLwLink(pLwLink) { };
            ~ReserveLwLinkCounters() { Release(); }
            RC Release();
            RC Reserve();
        private:
            // non-copyable
            ReserveLwLinkCounters(const ReserveLwLinkCounters&);
            ReserveLwLinkCounters& operator=(const ReserveLwLinkCounters&);

            LwLink   * m_pLwLink            = nullptr;
            bool       m_bCountersReserved  = false;
    };

    virtual void LoadLwLinkCaps();
    bool AreLwLinkCapsLoaded() { return m_bLwCapsLoaded; }
    void SetMaxLinks(UINT32 maxLinks) { m_MaxLinks = maxLinks; }
private:
    virtual RC SavePerLaneCrcSetting();
    RC WriteLwLinkCfgData(UINT32 addr, UINT32 data, UINT32 link, UINT32 lane);
    virtual UINT32 GetDomainBase(RegHalDomain domain, UINT32 linkId);

    UINT32         m_MaxLinks             = 0;        //!< Maximum number of links on the device
    bool           m_bLwCapsLoaded        = false;    //!< True if lwlink caps have been loaded
    RC             m_LoadLwLinkCapsRc;                //!< RC value from LoadLwLinkCaps
    bool           m_bSupported           = true;     //!< True if lwlink is supported
    vector<FabricAperture> m_FabricApertures;         //!< Apertures in the fabric that the
                                                      //!< Device uses
    UINT32         m_TopologyId = ~0U;                //!< ID of this device in the topology file, note
                                                      //!< that topology ids are per device type
    UINT32         m_SavedPerLaneEnable   = ~0U;      //!< Saved value of per-lane enable
    bool           m_bFbpBwWarningPrinted = false;    //!< Whether the warning for invalid
                                                      //!< FBPs has been printed
    LwRm::Handle   m_hProfiler            = 0;        //!< Handle for GPU Profiler object
    vector<EndpointInfo> m_CachedDetectedEndpointInfo; //!< This information shouldn't change
                                                      //!< during exelwtion so only retrieve it once
    Tasker::Mutex                   m_pCounterMutex = Tasker::NoMutex();
    UINT32                          m_CountersReservedCount = 0;
};

#endif
