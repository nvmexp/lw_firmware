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

//! \file simlwlink.h -- Concrete implemetation for a simulated version of lwlink

#pragma once

#ifndef INCLUDED_SIMLWLINK_H
#define INCLUDED_SIMLWLINK_H

#include "gpu/utility/lwlink/lwlinkdevif/lwl_libif.h"
#include "gpu/lwlink/lwlinkimpl.h"
#include <map>

//--------------------------------------------------------------------
//! \brief Implementation of LwLink used for simulated devices
//!
class SimLwLink
    : public LwLinkImpl
{
protected:
    SimLwLink() {}
    virtual ~SimLwLink() {};

    RC DoClearHwErrorCounts(UINT32 linkId) override;
    RC DoDetectTopology() override { return OK; }
    RC DoEnablePerLaneErrorCounts(UINT32 linkId, bool bEnable) override
        { return OK; }
    CeWidth DoGetCeCopyWidth(CeTarget target) const override
        { return CEW_256_BYTE; }
    RC DoGetDetectedEndpointInfo(vector<EndpointInfo> *pEndpointInfo) override
        { return RC::UNSUPPORTED_FUNCTION; }
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
    ) override { return RC::UNSUPPORTED_FUNCTION; }
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
    UINT32 DoGetLinksPerGroup() const override { return 4; }
    UINT32 DoGetMaxLinkGroups() const override { return 3; }
    UINT32 DoGetMaxLinks() const override { return 12; }
    FLOAT64 DoGetMaxObservableBwPct(LwLink::TransferType tt) override { return 0.0; }
    FLOAT64 DoGetMaxTransferEfficiency() const override { return 0.0; }
    RC DoGetOutputLinks
    (
        UINT32 inputLink
       ,UINT64 fabricBase
       ,DataType dt
       ,set<UINT32>* pPorts
    ) const override
        { return RC::UNSUPPORTED_FUNCTION; }
    bool DoIsSupported(LibDevHandles handles) const override
        { MASSERT(handles.size() == 0); return true; }
    UINT32 DoGetTopologyId() const override { return m_TopologyId; }
    bool DoHasNonPostedWrites() const override { return false; }
    RC DoPerLaneErrorCountsEnabled(UINT32 linkId, bool *pbPerLaneEnabled) override
        { return OK; }
    RC DoRegRd(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT64 *pData) override;
    RC DoRegWr(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT64 data) override;
    RC DoRegWrBroadcast(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT64 data) override
        { return RC::UNSUPPORTED_FUNCTION; }
    void DoSetCeCopyWidth(CeTarget target, CeWidth width) override {}
    RC DoSetCollapsedResponses(UINT32 mask) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoSetFabricApertures(const vector<FabricAperture> &fas) override;
    RC DoSetupTopology
    (
        const vector<EndpointInfo> &endpointInfo
       ,UINT32 topologyId
    ) override;
    RC DoShutdown() override { return OK; }
    bool DoSupportsFomMode(FomMode mode) const override { return false; }

private:
    vector<EndpointInfo> m_EndpointInfo; //!< Endpoint info for each link on
                                         //!< the device
    vector<FabricAperture> m_FabricApertures; //!< Apertures in the fabric that the
                                              //!< Device uses
    UINT32 m_TopologyId = ~0U; //!< device ID of this device in the topology file, note that
                               //!<topology ids are per device type
};

#endif
