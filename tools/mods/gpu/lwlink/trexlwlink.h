/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "simlwlink.h"
#include "gpu/include/testdevice.h"

//--------------------------------------------------------------------
//! \brief Implementation of LwLink used for trex devices
//!
class TrexLwLink
    : public SimLwLink
{
protected:
    TrexLwLink() {}
    virtual ~TrexLwLink() {};

    RC DoGetDetectedEndpointInfo(vector<EndpointInfo> *pEndpointInfo) override;
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
    UINT32 DoGetLineRateMbps(UINT32 linkId) const override;
    UINT32 DoGetMaxLinks() const override { return m_MaxLinks; }
    UINT32 DoGetMaxLinkDataRateKiBps(UINT32 linkId) const override;
    UINT32 DoGetMaxPerLinkPerDirBwKiBps(UINT32 lineRateMbps) const override;
    FLOAT64 DoGetMaxObservableBwPct(LwLink::TransferType tt) override { return 1.0; }
    UINT32 DoGetSublinkWidth(UINT32 linkId) const override;
    RC DoInitialize(LibDevHandles handles) override;
    bool DoHasCollapsedResponses() const override;
    bool DoHasNonPostedWrites() const override { return true; }
    RC DoSetupTopology(const vector<EndpointInfo> &endpointInfo, UINT32 topologyId) override;
    RC DoShutdown() override;
    RC DoWaitForLinkInit() override { return RC::OK; }

private:
    UINT32 m_MaxLinks = 0;
    bool   m_bUseNcisocFlits = false;
    map<UINT32, TestDevicePtr> m_LinkSwitchDevs;
    Device::LwDeviceId m_SwitchDevId = Device::ILWALID_DEVICE;
};
