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

#include "lwldt_test_route.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "device/interface/lwlink/lwlpowerstatecounters.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/lwlink/lwlinkimpl.h"

#include <cmath>
#include <algorithm>

namespace
{
    bool SortByLocalLink
    (
        LwLinkConnection::EndpointIds eid1,
        LwLinkConnection::EndpointIds eid2
    )
    {
        return eid1.first < eid2.first;
    }
}

using namespace LwLinkDataTestHelper;

//------------------------------------------------------------------------------
//! \brief Constructor
//!
TestRoute::TestRoute
(
     TestDevicePtr pLocDev
    ,LwLinkRoutePtr pRoute
    ,Tee::Priority pri
) : m_pRoute(pRoute)
   ,m_pLocLwLinkDev(pLocDev)
   ,m_TransferType(TT_ILWALID)
   ,m_PrintPri(pri)
{
}

//------------------------------------------------------------------------------
//! \brief Validate the link state on all connections in the tested route
//! (i.e. all links should be in HS mode)
//!
//! \return OK if all links are in HS mode, not OK otherwise
RC TestRoute::CheckLinkState()
{
    LwLinkRoute::PathIterator pPath = m_pRoute->PathsBegin(m_pLocLwLinkDev, LwLink::DT_ALL);
    LwLinkRoute::PathIterator pathEnd = m_pRoute->PathsEnd(m_pLocLwLinkDev, LwLink::DT_ALL);

    StickyRC rc;
    UINT32 pathIdx = 0;
    for (; pPath != pathEnd; ++pPath, pathIdx++)
    {
        LwLinkPath::PathEntryIterator pPe = pPath->PathEntryBegin(m_pLocLwLinkDev);
        LwLinkPath::PathEntryIterator peEnd = LwLinkPath::PathEntryIterator();
        UINT32 depth = 0;
        for (; pPe != peEnd; ++pPe, depth++)
        {
            vector<LwLink::LinkStatus> ls;
            CHECK_RC(pPe->pSrcDev->GetInterface<LwLink>()->GetLinkStatus(&ls));

            string s = Utility::StrPrintf("Link status on Path %u, Depth %u, %s\n",
                                          pathIdx,
                                          depth,
                                          pPe->pSrcDev->GetName().c_str());

            TestDevicePtr pRemoteDev = pPe->pCon->GetRemoteDevice(pPe->pSrcDev);
            s += "---------------------------------------------------\n";
            s += Utility::StrPrintf("  Remote Device  = %s\n",
                                    pRemoteDev->GetName().c_str());
            vector<LwLinkConnection::EndpointIds> eids = pPe->pCon->GetLinks(pPe->pSrcDev);
            for (size_t ii = 0; ii < eids.size(); ii++)
            {
                MASSERT(eids[ii].first < ls.size());
                LwLinkPowerState::LinkPowerStateStatus lpws;
                if (
                    pPe->pSrcDev->SupportsInterface<LwLinkPowerState>() &&
                    LwLink::LS_ILWALID != ls[eids[ii].first].linkState
                )
                {
                    auto powerState = pPe->pSrcDev->GetInterface<LwLinkPowerState>();
                    CHECK_RC(powerState->GetLinkPowerStateStatus(eids[ii].first, &lpws));
                }
                s +=
                    Utility::StrPrintf("  Link %2u        = %s (RX : (%s, %s), TX : (%s, %s), "
                                       "Remote Link %u\n",
                        eids[ii].first,
                        LwLink::LinkStateToString(ls[eids[ii].first].linkState).c_str(),
                        LwLink::SubLinkStateToString(ls[eids[ii].first].rxSubLinkState).c_str(),
                        LwLinkPowerState::SubLinkPowerStateToString(lpws.rxSubLinkLwrrentPowerState).c_str(), //$
                        LwLink::SubLinkStateToString(ls[eids[ii].first].txSubLinkState).c_str(),
                        LwLinkPowerState::SubLinkPowerStateToString(lpws.txSubLinkLwrrentPowerState).c_str(), //$
                        eids[ii].second);

                // High speed and single lane are both acceptable since single lane indicates
                // that the link trained to HS mode but went back to single lane due to no
                // traffic
                const bool validLS = ls[eids[ii].first].linkState == LwLink::LS_ACTIVE ||
                                     ls[eids[ii].first].linkState == LwLink::LS_SLEEP;
                const bool validRxSLS = ls[eids[ii].first].rxSubLinkState == LwLink::SLS_HIGH_SPEED ||
                                        ls[eids[ii].first].rxSubLinkState == LwLink::SLS_SINGLE_LANE ||
                                        (ls[eids[ii].first].rxSubLinkState == LwLink::SLS_OFF &&
                                         ls[eids[ii].first].linkState == LwLink::LS_SLEEP);
                const bool validTxSLS = ls[eids[ii].first].txSubLinkState == LwLink::SLS_HIGH_SPEED ||
                                        ls[eids[ii].first].txSubLinkState == LwLink::SLS_SINGLE_LANE ||
                                        (ls[eids[ii].first].txSubLinkState == LwLink::SLS_OFF &&
                                         ls[eids[ii].first].linkState == LwLink::LS_SLEEP);
                if (!validLS || !validRxSLS || !validTxSLS)
                {
                    rc = RC::LWLINK_BUS_ERROR;
                }
            }

            if (OK != rc)
                s += "******* Invalid link state found! *******\n";

            Printf((OK != rc) ? Tee::PriError : m_PrintPri, "%s\n", s.c_str());
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Get a formatted string representing the LwLinks in the route for
//! use as part of a filename
//!
//! \return Filename string representing the LwLinks in the route
string TestRoute::GetFilenameString()
{
    set<UINT32> links;
    vector<LwLinkConnection::EndpointIds> eIds;
    eIds = m_pRoute->GetEndpointLinks(m_pLocLwLinkDev, LwLink::DT_REQUEST);

    string s;
    GpuSubdevice* pLocSubdev = m_pLocLwLinkDev->GetInterface<GpuSubdevice>();
    if (pLocSubdev)
    {
        s = Utility::StrPrintf("G%u_%u",
                               pLocSubdev->GetParentDevice()->GetDeviceInst(),
                               pLocSubdev->GetSubdeviceInst());
    }
    else
    {
        s = Utility::StrPrintf("D%u",
                               m_pLocLwLinkDev->DevInst());
    }

    string locLinkStr = "L";
    string remLinkStr = "L";
    for (auto const & lwrEp : eIds)
    {
        if (lwrEp.first != LwLink::ILWALID_LINK_ID)
            locLinkStr += Utility::StrPrintf("%u", lwrEp.first);
        if (lwrEp.second != LwLink::ILWALID_LINK_ID)
            remLinkStr += Utility::StrPrintf("%u", lwrEp.second);
    }
    s += "_" + locLinkStr;
    if ((m_TransferType & TT_ALL) == TT_LOOPBACK)
        return s;

    if (m_TransferType & TT_P2P)
    {
        GpuSubdevice* pRemSubdev = m_pRemLwLinkDev->GetInterface<GpuSubdevice>();
        if (pLocSubdev)
        {
            s += Utility::StrPrintf("G%u_%u",
                                    pRemSubdev->GetParentDevice()->GetDeviceInst(),
                                    pRemSubdev->GetSubdeviceInst());
        }
        else
        {
            s += Utility::StrPrintf("D%u",
                                    m_pRemLwLinkDev->DevInst());
        }
    }
    else if (m_TransferType & TT_SYSMEM)
    {
        s += "S";
    }
    if (!(m_TransferType & TT_LOOPBACK))
        s += "_" + remLinkStr;
    return s;
}

//------------------------------------------------------------------------------
//! \brief Get a formatted string representing the LwLinks in the route
//!
//! \param bRemote         : true to get the link string for the remote side
//!
//! \return String representing the LwLinks in the route
string TestRoute::GetLinkString(bool bRemote)
{
    vector<UINT32> links;
    vector<LwLinkConnection::EndpointIds> eIds;
    eIds =  m_pRoute->GetEndpointLinks(m_pLocLwLinkDev, LwLink::DT_REQUEST);

    // Sort the endpoint ids by the local link
    sort(eIds.begin(), eIds.end(), SortByLocalLink);
    for (auto const & lwrEp : eIds)
    {
        UINT32 linkId = bRemote ? lwrEp.second : lwrEp.first;
        if (linkId == LwLink::ILWALID_LINK_ID)
            continue;
        links.push_back(linkId);
    }

    string linkStr = "";
    if (links.size() > 1)
    {
        linkStr += "(";
        vector<UINT32>::iterator pLink = links.begin();
        for (; pLink != links.end(); pLink++)
        {
            if (pLink != links.begin())
                linkStr += ",";
            linkStr += Utility::StrPrintf("%u", *pLink);
        }
        linkStr += ")";
    }
    else
    {
        linkStr += Utility::StrPrintf("%u", *(links.begin()));
    }

    return linkStr;
}

UINT64 TestRoute::GetLinkMask(bool bRemote)
{
    UINT64 linkMask = 0;
    vector<LwLinkConnection::EndpointIds> eIds;
    eIds = m_pRoute->GetEndpointLinks(m_pLocLwLinkDev, LwLink::DT_REQUEST);
    for (auto const &lwrEp : eIds)
    {
        linkMask |= 1ULL << (bRemote ? lwrEp.second : lwrEp.first);
    }
    return linkMask;
}

namespace
{
    string GetDevString(TestDevicePtr pDev)
    {
        switch (pDev->GetType())
        {
        case TestDevice::TYPE_LWIDIA_GPU:
            return Utility::StrPrintf("Gpu(%u, %u)",
                                      pDev->GetInterface<GpuSubdevice>()->GetParentDevice()->GetDeviceInst(),
                                      pDev->GetInterface<GpuSubdevice>()->GetSubdeviceInst());
        case TestDevice::TYPE_TREX:
        {
            UINT16 dev = 0;
            pDev->GetId().GetPciDBDF(nullptr, nullptr, &dev, nullptr);
            return Utility::StrPrintf("Trex(%u)", dev);
        }
        default:
            return "Unknown";
        }
    }
}

//------------------------------------------------------------------------------
//! \brief Get a formatted string representing the local device of the
//! route
//!
//! \return String representing the local device of the route
string TestRoute::GetLocalDevString()
{
    return GetDevString(m_pLocLwLinkDev);
}

//------------------------------------------------------------------------------
//! \brief Get the maximum possible bandwidth on this route
//!
//! \return Maximum possible bandwidth on this route
UINT32 TestRoute::GetMaxBandwidthKiBps()
{
    auto pLwLink = m_pLocLwLinkDev->GetInterface<LwLink>();
    return static_cast<UINT32>(m_pRoute->GetMaxBwKiBps(m_pLocLwLinkDev, LwLink::DT_REQUEST) *
                               pLwLink->GetMaxTransferEfficiency());
}

//------------------------------------------------------------------------------
//! \brief Get the maximum observable bandwidth percentage
//!
//! \return Maximum percent of SOL bandwidth that can be observed
FLOAT64 TestRoute::GetMaxObservableBwPct(LwLink::TransferType tt) const
{
    return m_pLocLwLinkDev->GetInterface<LwLink>()->GetMaxObservableBwPct(tt);
}

//------------------------------------------------------------------------------
//! \brief Get the raw bandwidth of the route
//!
//! \return Raw bandwidth of the route
UINT32 TestRoute::GetRawBandwidthKiBps()
{
    return m_pRoute->GetMaxBwKiBps(m_pLocLwLinkDev, LwLink::DT_REQUEST);
}

//------------------------------------------------------------------------------
//! \brief Get a formatted string representing the remote device of the
//! route
//!
//! \return String representing the remote device of the route
string TestRoute::GetRemoteDevString()
{
    string remDevStr = "Sysmem";
    TestDevicePtr pRemDev;
    if (m_TransferType & TT_P2P)
        pRemDev = m_pRemLwLinkDev;
    else if (m_TransferType & TT_LOOPBACK)
        pRemDev = m_pLocLwLinkDev;
    if (pRemDev)
    {
        remDevStr = GetDevString(pRemDev);
    }

    return remDevStr;
}

//------------------------------------------------------------------------------
//! \brief Get the local GpuSubdevice on this route
//!
//! \return Local GpuSubdevice on this route
GpuSubdevice* TestRoute::GetLocalSubdevice()
{
    return m_pLocLwLinkDev->GetInterface<GpuSubdevice>();
}

//------------------------------------------------------------------------------
//! \brief Get the remote GpuSubdevice on this route
//!
//! \return Remote GpuSubdevice on this route
GpuSubdevice* TestRoute::GetRemoteSubdevice()
{
    if (!m_pRemLwLinkDev)
        return nullptr;

    return m_pRemLwLinkDev->GetInterface<GpuSubdevice>();
}

//------------------------------------------------------------------------------
//! \brief Get the remote testdevice on this route
//!
//! \return Remote testdevice on this route
TestDevicePtr TestRoute::GetRemoteLwLinkDev()
{
    return m_pRoute->GetRemoteEndpoint(m_pLocLwLinkDev);
}

//------------------------------------------------------------------------------
//! \brief Get a formatted string representing the route
//!
//! \param prefix          : prefix for the route string
//!
//! \return String representing the route
string TestRoute::GetString(string prefix)
{
    string s;
    s = prefix + GetLocalDevString() + ", Link";
    s += (m_pRoute->GetWidth(m_pLocLwLinkDev, LwLink::DT_REQUEST) > 1) ? "s " : " ";
    s += GetLinkString(false) + " : " + GetRemoteDevString() + ", Link";
    s += (m_pRoute->GetWidth(m_pLocLwLinkDev, LwLink::DT_REQUEST) > 1) ? "s " : " ";
    s += GetLinkString(true);
    return s;
}

//------------------------------------------------------------------------------
UINT32 TestRoute::GetSublinkWidth() const
{
    auto eIds =  m_pRoute->GetEndpointLinks(m_pLocLwLinkDev, LwLink::DT_REQUEST);

    // Should never happen otherwise a route could not have been created
    MASSERT(!eIds.empty());

    // All links ilwolved in a route should contain the same number of sublinks
    // (i.e. no mixing of lwlink versions between connections in a route)
    return m_pLocLwLinkDev->GetInterface<LwLink>()->GetSublinkWidth(eIds[0].first);
}

//------------------------------------------------------------------------------
//! \brief Get the width of the test route
//!
//! \return Width of the test route
UINT32 TestRoute::GetWidth()
{
    return m_pRoute->GetWidth(m_pLocLwLinkDev, LwLink::DT_REQUEST);
}

//------------------------------------------------------------------------------
//! \brief Initialize the test route
//!
//! \return OK if successful, not OK otherwise
RC TestRoute::Initialize()
{
    RC rc;

    if (m_pRoute->IsLoop())
        m_TransferType = TT_LOOPBACK;
    else if (m_pRoute->IsSysmem())
        m_TransferType = TT_SYSMEM;
    else if (m_pRoute->IsP2P())
        m_TransferType = TT_P2P;
    else
    {
        Printf(Tee::PriError,
               "Unknown route type on route :\n%s",
               m_pRoute->GetString(m_pLocLwLinkDev, LwLink::DT_REQUEST, "    ").c_str());
        return RC::SOFTWARE_ERROR;
    }

    TestDevicePtr pRemoteDev = m_pRoute->GetRemoteEndpoint(m_pLocLwLinkDev);
    if (m_TransferType == TT_P2P)
    {
        m_pRemLwLinkDev = pRemoteDev;
    }

    return rc;
}
