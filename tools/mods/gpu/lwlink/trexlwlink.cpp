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

#include "trexlwlink.h"
#include "core/include/mgrmgr.h"
#include "device/interface/lwlink/lwltrex.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_devif.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr_impl.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/include/testdevice.h"

using namespace LwLinkDevIf;

namespace
{
    const UINT32 BYTES_PER_FLIT = 16;
    const UINT32 BYTES_PER_FLIT_NCISOC = 32;
};

//-----------------------------------------------------------------------------
/* virtual */ RC TrexLwLink::DoGetDetectedEndpointInfo(vector<EndpointInfo> *pEndpointInfo)
{
    MASSERT(pEndpointInfo);
    pEndpointInfo->clear();
    pEndpointInfo->resize(GetMaxLinks());
    return RC::OK;
}

//-----------------------------------------------------------------------------
/* virtual */ void TrexLwLink::DoGetFlitInfo
(
    UINT32 transferWidth
   ,HwType hwType
   ,bool bSysmem
   ,FLOAT64 *pReadDataFlits
   ,FLOAT64 *pWriteDataFlits
   ,UINT32 *pTotalReadFlits
   ,UINT32 *pTotalWriteFlits
) const
{
    MASSERT(pReadDataFlits);
    MASSERT(pWriteDataFlits);
    MASSERT(pTotalReadFlits);
    MASSERT(pTotalWriteFlits);

    const UINT32 bytesPerFlit = m_bUseNcisocFlits ? BYTES_PER_FLIT_NCISOC : BYTES_PER_FLIT;
    // Read and write data flits are always the same
    *pReadDataFlits = static_cast<FLOAT64>(transferWidth) / bytesPerFlit;
    *pWriteDataFlits = *pReadDataFlits;


    if (!m_bUseNcisocFlits)
    {
        *pTotalReadFlits = max(transferWidth / bytesPerFlit, 1U);
        *pTotalWriteFlits = max(transferWidth / bytesPerFlit, 1U);

#if LWCFG(GLOBAL_LWSWITCH_IMPL_LS10)
        if (m_SwitchDevId == Device::LS10)
        {
            *pTotalWriteFlits += 1;  // AE FLIT
            *pTotalReadFlits  += 1;  // AE FLIT
        }
#endif

        // When writing and the data amount is smaller than a flit, then a byte
        // enable flit is generated
        if (transferWidth < bytesPerFlit)
        {
            *pTotalWriteFlits = (*pTotalWriteFlits + 1);
        }
    }
    else
    {
        // For NCISOC traffic when transfer width is less than the number of bytes
        // in a TLC flit, set the total flits since for NCISOC in this case the
        // data is part of the header flit
        if (transferWidth <= BYTES_PER_FLIT)
        {
            *pTotalReadFlits = 0;
            *pTotalWriteFlits = 0;
        }
        else
        {
            *pTotalReadFlits = max(transferWidth / bytesPerFlit, 1U);
            *pTotalWriteFlits = max(transferWidth / bytesPerFlit, 1U);
        }
    }

}

//-----------------------------------------------------------------------------
UINT32 TrexLwLink::DoGetLineRateMbps(UINT32 linkId) const
{
    if (!m_LinkSwitchDevs.count(linkId))
        return 0;

    return m_LinkSwitchDevs.at(linkId)->GetInterface<LwLink>()->GetLineRateMbps(linkId);
}

UINT32 TrexLwLink::DoGetMaxLinkDataRateKiBps(UINT32 linkId) const
{
    if (!m_LinkSwitchDevs.count(linkId))
        return 0;

    if (m_bUseNcisocFlits)
    {
        FLOAT64 switchClk = 1333333.3; // TODO: Need to read the switch clock from pDevice
        return static_cast<UINT32>((BYTES_PER_FLIT_NCISOC * switchClk) * 1000.0 / 1024.0);
    }

    return m_LinkSwitchDevs.at(linkId)->GetInterface<LwLink>()->GetMaxLinkDataRateKiBps(linkId);
}

//------------------------------------------------------------------------------
UINT32 TrexLwLink::DoGetMaxPerLinkPerDirBwKiBps(UINT32 lineRateMbps) const
{
    if (m_LinkSwitchDevs.empty())
        return 0;

    if (m_bUseNcisocFlits)
        return GetMaxLinkDataRateKiBps(m_LinkSwitchDevs.begin()->first);

    return SimLwLink::DoGetMaxPerLinkPerDirBwKiBps(lineRateMbps);
}

UINT32 TrexLwLink::DoGetSublinkWidth(UINT32 linkId) const
{
    if (!m_LinkSwitchDevs.count(linkId))
        return 0;

    return m_LinkSwitchDevs.at(linkId)->GetInterface<LwLink>()->GetSublinkWidth(linkId);
}

bool TrexLwLink::DoHasCollapsedResponses() const
{
    if (m_LinkSwitchDevs.empty())
        return false;

    return m_LinkSwitchDevs.begin()->second->GetInterface<LwLink>()->HasCollapsedResponses();
}

//-----------------------------------------------------------------------------
RC TrexLwLink::DoInitialize(LibDevHandles handles)
{
    RC rc;

    auto pFMLibIf = TopologyManager::GetFMLibIf();
    MASSERT(pFMLibIf);

    UINT16 devIdx = 0;
    GetDevice()->GetId().GetPciDBDF(nullptr, nullptr, &devIdx, nullptr);
    m_MaxLinks = pFMLibIf->GetGpuMaxLwLinks(devIdx);

    CHECK_RC(SimLwLink::DoInitialize(handles));

    return rc;
}

//-----------------------------------------------------------------------------
RC TrexLwLink::DoSetupTopology(const vector<EndpointInfo> &endpointInfo, UINT32 topologyId)
{
    RC rc;

    CHECK_RC(SimLwLink::DoSetupTopology(endpointInfo, topologyId));

    bool bStaticDataInit = false;
    for (UINT32 linkId = 0; linkId < GetMaxLinks(); linkId++)
    {
        EndpointInfo ep;
        if (RC::OK == GetRemoteEndpoint(linkId, &ep))
        {
            TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
            for (UINT32 idx = 0; idx < pTestDeviceMgr->NumDevices(); idx++)
            {
                TestDevicePtr pDevice;
                if (RC::OK != pTestDeviceMgr->GetDevice(idx, &pDevice))
                    break;

                if (pDevice->GetId() == ep.id)
                {
                    m_LinkSwitchDevs[linkId] = pDevice;
                    if (!bStaticDataInit)
                    {
                        auto * pTrex = pDevice->GetInterface<LwLinkTrex>();
                        m_bUseNcisocFlits = ((pTrex->GetTrexSrc() == LwLinkTrex::SRC_EGRESS) &&
                                             (pTrex->GetTrexDst() == LwLinkTrex::DST_INGRESS));
                        m_SwitchDevId = pDevice->GetDeviceId();
                        bStaticDataInit = true;                         
                    }
                }
            }
        }
    }
    return rc;
}

RC TrexLwLink::DoShutdown()
{
    RC rc;

    CHECK_RC(SimLwLink::DoShutdown());
    m_LinkSwitchDevs.clear();
    return rc;
}
