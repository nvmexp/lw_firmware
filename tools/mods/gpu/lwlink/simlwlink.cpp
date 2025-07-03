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

#include "core/include/tee.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_devif.h"
#include "gpu/utility/lwlink/lwlinkdev.h"
#include "simlwlink.h"
#include "ctrl/ctrl2080.h"

using namespace LwLinkDevIf;

namespace
{
    bool SortByFabricBase
    (
        const LwLink::FabricAperture ap1,
        const LwLink::FabricAperture ap2
    )
    {
        return ap1.first < ap2.first;
    }
}

//------------------------------------------------------------------------------
//! \brief Clear the error counts for the specified link id
//!
//! \param linkId  : Link id to clear counts on
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC SimLwLink::DoClearHwErrorCounts(UINT32 linkId)
{
    RC rc;

    if (!IsLinkValid(linkId))
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Unknown linkId %u on LwLink device %s\n",
               __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

//-----------------------------------------------------------------------------
void SimLwLink::DoGetEomDefaultSettings
(
    UINT32 link,
    EomSettings* pSettings
) const
{
    MASSERT(!"EOM not supported on sim devices");
    MASSERT(pSettings);
    pSettings->numErrors = 0;
    pSettings->numBlocks = 0;
}

//------------------------------------------------------------------------------
//! \brief Get the current HW LwLink error counters for the specified link
//!
//! \param linkId       : Link ID to get errors on
//! \param pErrorCounts : Pointer to returned error counts
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC SimLwLink::DoGetErrorCounts(UINT32 linkId, LwLink::ErrorCounts * pErrorCounts)
{
    MASSERT(pErrorCounts);
    if (!IsLinkValid(linkId))
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Unknown linkId %u on LwLink device %s\n",
               __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    for (INT32 lwrErr = 0; lwrErr < LwLink::ErrorCounts::LWL_NUM_ERR_COUNTERS; lwrErr++)
    {
        LwLink::ErrorCounts::ErrId errIdx;
        errIdx = static_cast<LwLink::ErrorCounts::ErrId>(lwrErr);
        pErrorCounts->SetCount(errIdx, 0, false);
    }
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Get the current HW LwLink error flags
//!
//! \param pErrorFlags : Pointer to returned error flags
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC SimLwLink::DoGetErrorFlags(LwLink::ErrorFlags * pErrorFlags)
{
    MASSERT(pErrorFlags);
    pErrorFlags->linkErrorFlags.clear();
    pErrorFlags->linkGroupErrorFlags.clear();
    return OK;
}

//------------------------------------------------------------------------------
RC SimLwLink::DoGetFabricApertures(vector<FabricAperture> *pFas)
{
    MASSERT(pFas);
    *pFas = m_FabricApertures;
    return (m_FabricApertures.empty()) ? RC::SOFTWARE_ERROR : OK;
}

//-----------------------------------------------------------------------------
/* virtual */ void SimLwLink::DoGetFlitInfo
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
    //TODO
    MASSERT(pReadDataFlits);
    MASSERT(pWriteDataFlits);
    MASSERT(pTotalReadFlits);
    MASSERT(pTotalWriteFlits);
    *pReadDataFlits = 0;
    *pWriteDataFlits = 0;
    *pTotalReadFlits = 0;
    *pTotalWriteFlits = 0;
}

//------------------------------------------------------------------------------
//! \brief Get Link ID associated with the specified endpoint
//!
//! \param endpointInfo : Endpoint info to get link ID for
//! \param pLinkId      : Pointer to returned Link ID
//!
//! \return OK if successful, not OK otherwise
RC SimLwLink::DoGetLinkId(const EndpointInfo &endpointInfo, UINT32 *pLinkId) const
{
    if (GetDevice()->GetId() != endpointInfo.id)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Endpoint not present on %s\n", __FUNCTION__,
               GetDevice()->GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }
    *pLinkId = endpointInfo.linkNumber;
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Get information about the LwLinks on the associated subdevice
//!
//! \param pLinkInfo  : Pointer to returned link info
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC SimLwLink::DoGetLinkInfo(vector<LinkInfo> *pLinkInfo)
{
    RC rc;
    pLinkInfo->clear();
    for (UINT32 lwrLink = 0; lwrLink < GetMaxLinks(); lwrLink++)
        pLinkInfo->emplace_back(true, true, 3, 50000, 133, 24414062, 4, false);
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Get link status for all links
//!
//! \param pLinkStatus  : Pointer to returned per-link status
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC SimLwLink::DoGetLinkStatus(vector<LinkStatus> *pLinkStatus)
{
    MASSERT(pLinkStatus);
    RC rc;
    pLinkStatus->clear();
    for (size_t ii = 0; ii < GetMaxLinks(); ii++)
        pLinkStatus->push_back({LS_ACTIVE,
                                SLS_HIGH_SPEED,
                                SLS_HIGH_SPEED});
    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC SimLwLink::DoRegRd
(
    UINT32       linkId,
    RegHalDomain domain,
    UINT32       offset,
    UINT64*      pData
)
{
    *pData = 0;
    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC SimLwLink::DoRegWr
(
    UINT32       linkId,
    RegHalDomain domain,
    UINT32       offset,
    UINT64       data
)
{
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Setup the fabric aperture for this device
//!
//! \param fa : Fabric aperture to set
//!
//! \return OK if successful, not OK otherwise
RC SimLwLink::DoSetFabricApertures(const vector<FabricAperture> &fa)
{
    m_FabricApertures = fa;

    //Ensure contiguous fabric addresses of the coorect size
    sort(m_FabricApertures.begin(), m_FabricApertures.end(), SortByFabricBase);

    static const UINT64 FABRIC_ADDR_ALIGN = (1ULL << 35) - 1;
    if (m_FabricApertures[0].first & FABRIC_ADDR_ALIGN)
    {
        Printf(Tee::PriError, "%s : Fabric addresses must be 32GB aligned!\n", __FUNCTION__);
        return RC::BAD_PARAMETER;
    }
    for (UINT32 ii = 1; ii < m_FabricApertures.size(); ii++)
    {
        const UINT64 expectedFa = m_FabricApertures[ii - 1].first +
                                  m_FabricApertures[ii - 1].second;
        if (m_FabricApertures[ii].first != expectedFa)
        {
            Printf(Tee::PriError, "%s : Fabric addresses are not contiguous!\n", __FUNCTION__);
            return RC::BAD_PARAMETER;
        }
    }
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Setup topology related functionality on the device
//!
//! \param endpointInfo : Per link endpoint info for the device
//! \param topologyId   : Id in the topology file for the device
//!
//! \return OK if successful, not OK otherwise
RC SimLwLink::DoSetupTopology
(
    const vector<EndpointInfo> &endpointInfo
   ,UINT32                                topologyId
)
{
    RC rc;

    Printf(Tee::PriLow, GetTeeModule()->GetCode(),
           "%s : Setting up topology on %s\n",
           __FUNCTION__, GetDevice()->GetName().c_str());

    SetEndpointInfo(endpointInfo);

    m_TopologyId = topologyId;

    return rc;
}
