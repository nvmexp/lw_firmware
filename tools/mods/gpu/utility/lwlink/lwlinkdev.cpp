/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwlinkdev.h"

#include "core/include/gpu.h"
#include "core/include/jscript.h"
#include "core/include/pci.h"
#include "core/include/platform.h"
#include "core/include/script.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "ctrl/ctrl2080.h"
#include "device/interface/lwlink.h"
#include "device/interface/lwlink/lwlcci.h"
#include "device/interface/lwlink/lwltrex.h"
#include "device/interface/pcie.h"
#include "gpu/include/testdevice.h"
#include "gpu/lwlink/lwlinkimpl.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_devif_mgr.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr.h"

#if defined(INCLUDE_LWLINK)
#include "gpu/fuse/js_fuse.h"
#include "gpu/reghal/js_reghal.h"
#endif

#include <memory>

namespace
{
    // Transformation function for swapping endpoints to make them relative
    // to a specified lwlink device via the STL transform function
    LwLinkConnection::EndpointIds SwapEndpoints(LwLinkConnection::EndpointIds e)
    {
        LwLinkConnection::EndpointIds swappedEndpoints(e.second, e.first);
        return swappedEndpoints;
    }
    // Transformation function for normalizing endpoints used to make loopout
    // endpoint links always be presented in the same way via the STL transform function
    LwLinkConnection::EndpointIds NormalizeLoopEndpoints(LwLinkConnection::EndpointIds e)
    {
        if (e.second < e.first)
            return { e.second, e.first };
        return e;
    }

    // Sorting functions for endpoints Ids within a connection
    bool SortByLocalLink
    (
        LwLinkConnection::EndpointIds eid1,
        LwLinkConnection::EndpointIds eid2
    )
    {
        return eid1.first < eid2.first;
    }
    bool SortByRemoteLink
    (
        LwLinkConnection::EndpointIds eid1,
        LwLinkConnection::EndpointIds eid2
    )
    {
        return eid1.second < eid2.second;
    }

    // Sorting functions for endpoints Ids within a connection
    bool SortPaths
    (
        const LwLinkPath & path1,
        const LwLinkPath & path2
    )
    {
        LwLinkConnection::EndpointDevices epDevs = path1.GetEndpointDevices();

        TestDevicePtr pBaseDev = epDevs.first;
        if (epDevs.second->GetId() < epDevs.first->GetId())
            pBaseDev = epDevs.second;

        return path1.GetLocalEndpointLinkMask(pBaseDev) <
               path2.GetLocalEndpointLinkMask(pBaseDev);
    }

    TeeModule s_ModsLwLinkModule("ModsLwLink");
}

//------------------------------------------------------------------------------
// LwLinkConnection Implementation
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! \brief Constructor
//!
LwLinkConnection::LwLinkConnection
(
     TestDevicePtr pDev1
    ,TestDevicePtr pDev2
    ,const vector<EndpointIds> &endpoints
)
 :  m_pDev1(pDev1)
   ,m_pDev2(pDev2)
   ,m_Endpoints(endpoints)
{
    sort(m_Endpoints.begin(), m_Endpoints.end(), SortByLocalLink);
}

//------------------------------------------------------------------------------
//! \brief Get the maximum data bandwith on the connection from the perspective of
//! the local device
//!
//! \param pLocDev     : Local device for getting bandwidth
//!
//! \return Maximum bandwidth for the connection in KiBps
UINT32 LwLinkConnection::GetMaxDataBwKiBps(TestDevicePtr pLocDev) const
{
    MASSERT((pLocDev == m_pDev1) || (pLocDev == m_pDev2));
    TestDevicePtr pRemDev = (pLocDev == m_pDev1) ? m_pDev2 : m_pDev1;
    auto pLocLwLink = pLocDev->GetInterface<LwLink>();
    auto pRemLwLink = pRemDev->GetInterface<LwLink>();
    auto eIds = GetLocalLinks(pLocDev);
    return (min)(pLocLwLink->GetMaxLinkDataRateKiBps(eIds[0].first),
                 pRemLwLink->GetMaxLinkDataRateKiBps(eIds[0].second)) * GetWidth();
}

//-----------------------------------------------------------------------------
pair<TestDevicePtr, TestDevicePtr> LwLinkConnection::GetDevices()
{
    return pair<TestDevicePtr, TestDevicePtr>(m_pDev1, m_pDev2);
}

//------------------------------------------------------------------------------
//! \brief Get any endpoint devices that are present on the connection
//!
//! \return Endpoint devices that are present in the connection.
LwLinkConnection::EndpointDevices LwLinkConnection::GetEndpointDevices()
{
    EndpointDevices devs;

    if (m_pDev1->IsEndpoint())
    {
        devs.first = m_pDev1;
        if (m_pDev2->IsEndpoint())
            devs.second = m_pDev2;
    }
    else if (m_pDev2->IsEndpoint())
        devs.first = m_pDev2;
    return devs;
}

//------------------------------------------------------------------------------
//! \brief Get the link mask on the connection for the specified device
//!
//! \param pLocDev : Device to get the link mask on
//!
//! \return Link mask on the connection for the specified device
UINT64 LwLinkConnection::GetLocalLinkMask(TestDevicePtr pLocDev)
{
    return GetLinkMask(pLocDev, false);
}

//------------------------------------------------------------------------------
//! \brief Get a string representing the connected links on the specified device
//!
//! \param pLocDev : Device to get the link string on
//!
//! \return Link string on the connection for the specified device
string LwLinkConnection::GetLocalLinkString(TestDevicePtr pLocDev, TestDevicePtr pSortDev)
{
    return GetLinkString(pLocDev, false, pSortDev);
}

//------------------------------------------------------------------------------
//! \brief Get a list of endpoints representing the links present on this
//! connection from the perspective of the specified local device
//!
//! \param pLocDev : Local device for returning the endpoints relative to
//!
//! \return List of endpoints from the perspective of the local device.  Since
//! endpoints are pairs of link IDs, this means that the first member of a
//! pair will be the link ID on the specified local device and the second member
//! of a pair will be the link ID on the remote device.  If the specified local
//! device is not a member of this connection, then endpoints will be empty
//!
vector<LwLinkConnection::EndpointIds> LwLinkConnection::GetLinks(TestDevicePtr pLocDev) const
{
    if (pLocDev == m_pDev1)
        return m_Endpoints;

    if (pLocDev != m_pDev2)
    {
        vector<EndpointIds> emptyVec;
        return emptyVec;
    }

    vector<EndpointIds> swappedVector(m_Endpoints.size());
    transform(m_Endpoints.begin(), m_Endpoints.end(), swappedVector.begin(),
              SwapEndpoints);
    return swappedVector;
}

//------------------------------------------------------------------------------
vector<LwLinkConnection::EndpointIds> LwLinkConnection::GetLocalLinks(TestDevicePtr pLocDev) const
{
    if (!UsesDevice(pLocDev))
        return vector<EndpointIds>();

    if (pLocDev == m_pDev1)
        return GetLinks(pLocDev);

    return GetLinks(m_pDev2);
}

//------------------------------------------------------------------------------
//! \brief Get the maximum bandwith on the connection from the perspective of
//! the local device
//!
//! \param pLocDev     : Local device for getting bandwidth
//!
//! \return Maximum bandwidth for the connection in KiBps
UINT32 LwLinkConnection::GetMaxBwKiBps(TestDevicePtr pLocDev) const
{
    MASSERT((pLocDev == m_pDev1) || (pLocDev == m_pDev2));
    TestDevicePtr pRemDev = (pLocDev == m_pDev1) ? m_pDev2 : m_pDev1;
    auto pLocLwLink = pLocDev->GetInterface<LwLink>();
    auto pRemLwLink = pRemDev->GetInterface<LwLink>();
    UINT32 locMaxBw, remMaxBw;
    vector<LwLinkConnection::EndpointIds> eids = GetLinks(pLocDev);

    // All links that share the same devices as endpoints run at the same speed
    UINT32 lineRateMbps = (min)(pLocLwLink->GetLineRateMbps(eids[0].first),
                                pRemLwLink->GetLineRateMbps(eids[0].second));
    UINT32 maxBw = (min)(pLocLwLink->GetMaxPerLinkPerDirBwKiBps(lineRateMbps),
                         pRemLwLink->GetMaxPerLinkPerDirBwKiBps(lineRateMbps)) * GetWidth();

    locMaxBw = pLocLwLink->GetMaxTotalBwKiBps(lineRateMbps);
    remMaxBw = pRemLwLink->GetMaxTotalBwKiBps(lineRateMbps);

    return (min)(maxBw, (min)(locMaxBw, remMaxBw));
}

//------------------------------------------------------------------------------
//! \brief Get the remote device for this connection given the local device
//!
//! \param pLocDev : Local device
//!
//! \return Remote device pointer based on the specified local device.  If the
//! local device is not part of this connection, the returned shared_ptr will
//! contain null
//!
TestDevicePtr LwLinkConnection::GetRemoteDevice(TestDevicePtr pLocDev) const
{
    if (!UsesDevice(pLocDev))
        return TestDevicePtr();

    return (pLocDev == m_pDev1) ? m_pDev2 : m_pDev1;
}

//------------------------------------------------------------------------------
//! \brief Get the link mask on the opposite end of the conneciton from the
//!        specified device
//!
//! \param pLocDev : Device to get the opposite end link mask
//!
//! \return Link mask on the opposite end of the connection from the
//!         specified device
UINT64 LwLinkConnection::GetRemoteLinkMask(TestDevicePtr pLocDev)
{
    return GetLinkMask(GetRemoteDevice(pLocDev), HasLoopout());
}

//------------------------------------------------------------------------------
vector<LwLinkConnection::EndpointIds> LwLinkConnection::GetRemoteLinks(TestDevicePtr pLocDev) const
{
    if (!UsesDevice(pLocDev))
        return vector<EndpointIds>();

    // If there is no loopout connection then either all links are loopback to the same port
    // or connected to a different device, in either case getting the links on the remote
    // device is the correct thing to do
    if (!HasLoopout())
        return GetLinks(GetRemoteDevice(pLocDev));

    // Otherwise, for loopout connections, swap the endpoints so the remote links are
    // retrieved
    vector<EndpointIds> swappedVector(m_Endpoints.size());
    transform(m_Endpoints.begin(), m_Endpoints.end(), swappedVector.begin(), SwapEndpoints);

    return swappedVector;
}

//------------------------------------------------------------------------------
//! \brief Get a string representing the connected links on the opposite end of
//!        the connection from the specified device
//!
//! \param pLocDev : Device to get the opposite end link string on
//!
//! \return Link string on the opposite end of the connection from the
//!         specified device
string LwLinkConnection::GetRemoteLinkString(TestDevicePtr pLocDev, TestDevicePtr pSortDev)
{
    return GetLinkString(GetRemoteDevice(pLocDev), HasLoopout(), pSortDev);
}

//------------------------------------------------------------------------------
//! \brief Get a single string representing the connection
//!
//! \param pLocDev : Local device to get the string relative to
//! \param prefix  : Prefix for the string
//!
//! \return Single string representing the connection
//!
string LwLinkConnection::GetString(TestDevicePtr pLocDev, string prefix) const
{
    string singleStr = "";
    if (!pLocDev)
    {
        pLocDev = m_pDev1;
        singleStr = Utility::StrPrintf("Connection on %s : \n", pLocDev->GetName().c_str());
    }

    vector<string> connStrings;
    GetStrings(pLocDev, &connStrings);

    sort(connStrings.begin(), connStrings.end());
    for (size_t ii = 0; ii < connStrings.size(); ii++)
        singleStr += prefix + connStrings[ii] + "\n";

    return singleStr;
}

//------------------------------------------------------------------------------
size_t LwLinkConnection::GetWidth() const
{
    // For GPU devices, when loopout links are present both ends of the link
    // contribute to the width since they will both always
    // be active. Since connections are grouped by endpoint pairs when a loopout
    // connection exists all pairs of endpoints must either be loopback or
    // loopout
    if (!HasLoopout())
        return m_Endpoints.size();

    size_t width = 0;
    for (auto const & lwrEp : m_Endpoints)
    {
        width += (lwrEp.first == lwrEp.second) ? 1 : 2;
    }
    return width;
}

//------------------------------------------------------------------------------
//! \brief Append strings representing the connections relative to
//! the specified local device to the provided string vector
//!
//! \param pLocDev      : Local device
//! \param pConnStrings : Vector to append strings to
//!
void LwLinkConnection::GetStrings
(
     TestDevicePtr pLocDev
    ,vector<string> *pConnStrings
) const
{
    TestDevicePtr pRemDev = GetRemoteDevice(pLocDev);
    vector<LwLinkConnection::EndpointIds> endpoints = GetLinks(pLocDev);

    for (size_t ii = 0; ii < endpoints.size(); ii++)
    {
        // shared_ptr has a bool() operator and does the appropriate colwersion
        if (!pRemDev)
        {
            pConnStrings->push_back(Utility::StrPrintf("%u : Not Connected",
                                                       endpoints[ii].first));
        }
        else
        {
            string conStr;
            conStr = Utility::StrPrintf("%u : %s, Link ", endpoints[ii].first,
                                        pRemDev->GetName().c_str());
            conStr += Utility::StrPrintf("%u", endpoints[ii].second);
            pConnStrings->push_back(conStr);
        }
    }
}

//------------------------------------------------------------------------------
//! \brief Return whether the connection is active
//!
//! \return true if the connection is active, false otherwise
bool LwLinkConnection::IsActive() const
{
    return ((m_pDev1.get() != nullptr) && (m_pDev2.get() != nullptr));
}

//------------------------------------------------------------------------------
//! \brief Return whether the connection contains an endpoint device for traffic
//!
//! \return true if the connection contains an endpoint device for traffic,
//! false otherwise
bool LwLinkConnection::IsEndpoint() const
{
    return (IsActive() &&
            (m_pDev1->IsEndpoint() || m_pDev2->IsEndpoint()));
}

//------------------------------------------------------------------------------
bool LwLinkConnection::IsLoop() const
{
    return (IsActive() && (m_pDev1 == m_pDev2));
}

//------------------------------------------------------------------------------
bool LwLinkConnection::IsSysmem() const
{
    return (IsActive() &&
            (m_pDev1->IsSysmem() || m_pDev2->IsSysmem()));
}

//------------------------------------------------------------------------------
//! \brief Return whether the connection is a loopout (same device differtent
//!        links)
//!
//! \return true if the connection is loopout, false otherwise
bool LwLinkConnection::HasLoopout() const
{
    if (m_pDev1 != m_pDev2)
        return false;

    for (auto lwrEp : m_Endpoints)
    {
        if (lwrEp.first != lwrEp.second)
            return true;
    }
    return false;
}

//------------------------------------------------------------------------------
//! \brief Return whether this connection uses the specified device
//!
//! \param pDev : Device to check for use
//!
//! \return true if specified device is used in this connection, false otherwise
bool LwLinkConnection::UsesDevice(TestDevicePtr pDev) const
{
    return ((pDev == m_pDev1) || (pDev == m_pDev2));
}

//------------------------------------------------------------------------------
//! \brief Compare connections for equality
//!
//! \param rhs : Connection to compare
//!
//! \return true if specified connections are equal, false otherwise
bool LwLinkConnection::operator==(const LwLinkConnection &rhs) const
{
    vector<EndpointIds> rhsEndpoints = rhs.m_Endpoints;
    vector<EndpointIds> lhsEndpoints = m_Endpoints;

    if ((m_pDev1 != rhs.m_pDev1 && m_pDev1 != rhs.m_pDev2) ||
        (m_pDev2 != rhs.m_pDev1 && m_pDev2 != rhs.m_pDev2))
    {
        return false;
    }

    // If the first devices are equal, then the connections were created relative
    // to the same device, so simply check the remaining for equality
    if (rhs.m_pDev1 == m_pDev1)
    {
        if (rhs.m_pDev2 != m_pDev2)
            return false;
    }
    else if (rhs.m_pDev1 == m_pDev2)
    {
        if (rhs.m_pDev2 != m_pDev1)
            return false;

        // If the rhs first device and the lhs second device are equal, then the connections
        // were created relative to opposite ends of the link so swap the endpoint
        // sense for checking endpoint equality
        transform(lhsEndpoints.begin(), lhsEndpoints.end(), lhsEndpoints.begin(), SwapEndpoints);
    }

    // If this is a loop connection then normalize how endpoints are stored
    // (endpoint.first should always be the lower numbered endpoint).  This can
    // only be done on loop connections because endpoint.first corresponds to
    // m_pDev1 and endpoint.second corresponds to m_pDev2 so sorting first/second
    // on a non-loop connection would actually alter what is being connected
    if (m_pDev1 == m_pDev2)
    {
        transform(lhsEndpoints.begin(), lhsEndpoints.end(), lhsEndpoints.begin(),
                  NormalizeLoopEndpoints);
        transform(rhsEndpoints.begin(), rhsEndpoints.end(), rhsEndpoints.begin(),
                  NormalizeLoopEndpoints);
    }

    sort(lhsEndpoints.begin(), lhsEndpoints.end(), SortByLocalLink);
    sort(rhsEndpoints.begin(), rhsEndpoints.end(), SortByLocalLink);
    return (rhsEndpoints == lhsEndpoints);
}

//------------------------------------------------------------------------------
//! \brief Order connections within a STL class
//!
//! \param rhs : Connection to compare
//!
//! \return true if this connection is less than the rhs, false otherwise
bool LwLinkConnection::operator<(const LwLinkConnection &rhs) const
{
    vector<EndpointIds> rhsEndpoints = rhs.m_Endpoints;
    vector<EndpointIds> lhsEndpoints = m_Endpoints;
    TestDevicePtr rhsDev1 = rhs.m_pDev1;
    TestDevicePtr rhsDev2 = rhs.m_pDev2;

    // If the connections were created relative to opposite ends of the link
    // swap the endpoint sense for checking ordering and reorder the rhs devices
    // for checking
    //
    // Do not swap links on loopout/loopout connections
    if ((rhs.m_pDev1 != rhs.m_pDev2) &&
        ((rhs.m_pDev1 == m_pDev2) || (rhs.m_pDev2 == m_pDev1)))
    {
        rhsDev1 = rhs.m_pDev2;
        rhsDev2 = rhs.m_pDev1;
        transform(rhsEndpoints.begin(), rhsEndpoints.end(), rhsEndpoints.begin(), SwapEndpoints);
    }

    // Order by device 1 null pointer first
    if ((m_pDev1 == nullptr) && (rhsDev1 != nullptr))
        return true;
    else if ((m_pDev1 != nullptr) && (rhsDev1 == nullptr))
         return false;

    // Order by device 2 null pointer second
    if ((m_pDev2 == nullptr) && (rhsDev2 != nullptr))
        return true;
    else if ((m_pDev2 != nullptr) && (rhsDev2 == nullptr))
         return false;

    // At this point both device ones are either null or both are not null
    // order by device IDs of device 1
    if (m_pDev1)
    {
        if (m_pDev1->GetId() < rhsDev1->GetId())
            return true;
        else if (m_pDev1->GetId() > rhsDev1->GetId())
            return false;
    }

    // At this point both device twos are either null or both are not null and
    // the device IDs of device 1 are equal, sort by device 2 device IDs
    if (m_pDev2)
    {
        if (m_pDev2->GetId() < rhsDev2->GetId())
            return true;
        else if (m_pDev2->GetId() > rhsDev2->GetId())
            return false;
    }

    // At this point both device twos are either null or both are not null and
    // the device IDs of device 1 and device 2 are both equal.  Sort by the
    // number of endpoints first
    if (lhsEndpoints.size() < rhsEndpoints.size())
        return true;
    else if (lhsEndpoints.size() > rhsEndpoints.size())
        return false;

    // If this is a loop connection then normalize how endpoints are stored
    // (endpoint.first should always be the lower numbered endpoint).  This can
    // only be done on loop connections because endpoint.first corresponds to
    // m_pDev1 and endpoint.second corresponds to m_pDev2 so sorting first/second
    // on a non-loop connection would actually alter what is being connected
    if (m_pDev1 == m_pDev2)
    {
        transform(lhsEndpoints.begin(), lhsEndpoints.end(), lhsEndpoints.begin(),
                  NormalizeLoopEndpoints);
        transform(rhsEndpoints.begin(), rhsEndpoints.end(), rhsEndpoints.begin(),
                  NormalizeLoopEndpoints);
    }
    sort(lhsEndpoints.begin(), lhsEndpoints.end(), SortByLocalLink);
    sort(rhsEndpoints.begin(), rhsEndpoints.end(), SortByLocalLink);

    // Both device 1 and device 2 are the same and contain the same number of
    // endpoints.  Order by the first endpoint in the link
    for (size_t ii = 0; ii < m_Endpoints.size(); ii++)
    {
        if (lhsEndpoints[ii].first < rhsEndpoints[ii].first)
            return true;
        else if (lhsEndpoints[ii].first > rhsEndpoints[ii].first)
            return false;
    }

    return false;
}

//------------------------------------------------------------------------------
// LwLinkConnection Private Implementation
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! \brief Get the link mask on the connection for the specified device
//!
//! \param pDev                 : Device to get the link mask relative to
//! \param bForceSecondEndpoint : True to force use of the second endpoint in the
//!                               endpoint vector
//!
//! \return Link mask on the connection for the specified device
UINT64 LwLinkConnection::GetLinkMask(TestDevicePtr pLocDev, bool bForceSecondEndpoint)
{
    if (!UsesDevice(pLocDev))
        return 0;

    const bool bUseFirst = (pLocDev == m_pDev1) && !bForceSecondEndpoint;

    UINT64 linkMask = 0ULL;
    for (size_t ii = 0; ii < m_Endpoints.size(); ii++)
    {
        linkMask |= bUseFirst ? (1ULL << m_Endpoints[ii].first) :
                                (1ULL << m_Endpoints[ii].second);
    }
    return linkMask;
}

//------------------------------------------------------------------------------
//! \brief Get a string representing the links for the connection on the
//! provided local device.  The link numbers will be sorted by the local device
//! unless a different sort device is specified
//!
//! \param pLocDev               : Device for generating the link string
//! \param bForceSecondEndpoint  : True to use the second endpoint from the endpoint
//!                                pair when creating the string
//! \param pSortDev              : Device that the link numbers should be sorted based on
//!
//! \return String representing the links on the local device
//!
string LwLinkConnection::GetLinkString
(
    TestDevicePtr pLocDev
   ,bool bForceSecondEndpoint
   ,TestDevicePtr pSortDev
)
{
    MASSERT(UsesDevice(pLocDev));

    vector<EndpointIds> eIds = GetLinks(pLocDev);
    sort(eIds.begin(), eIds.end(),
         (!pSortDev || (pLocDev != pSortDev)) ? SortByLocalLink : SortByRemoteLink);

    string linkStr = "";
    if (eIds.size() > 1)
    {
        linkStr += "(";
        vector<EndpointIds>::iterator pEndpoint = eIds.begin();
        for (; pEndpoint != eIds.end(); pEndpoint++)
        {
            if (pEndpoint != eIds.begin())
            {
                linkStr += ",";
            }
            linkStr += Utility::StrPrintf("%u", !bForceSecondEndpoint ? pEndpoint->first :
                                                                        pEndpoint->second);
        }
        linkStr += ")";
    }
    else
    {
        linkStr += Utility::StrPrintf("%u", !bForceSecondEndpoint ? eIds[0].first :
                                                                    eIds[0].second);
    }

    return linkStr;
}

//------------------------------------------------------------------------------
// LwLinkPath Public Implementation
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! \brief Equality operator for PathEntry to make usage easier and enable STL
//!
//! \param rhs : PathEntry to check for equality against
//!
//! \return true if equal, false otherwise
bool LwLinkPath::PathEntry::operator==(const PathEntry &rhs) const
{
    return ((pSrcDev == rhs.pSrcDev) &&
            (*pCon == *rhs.pCon) &&
            (bRemote == rhs.bRemote));
}

//------------------------------------------------------------------------------
//! \brief Non-euality operator for PathEntry to make usage easier
//!
//! \param rhs : PathEntry to check for non-equality against
//!
//! \return true if not equal, false otherwise
bool LwLinkPath::PathEntry::operator!=(const PathEntry &rhs) const
{
    return ((pSrcDev != rhs.pSrcDev) ||
            (*pCon != *rhs.pCon) ||
            (bRemote != rhs.bRemote));
}

//------------------------------------------------------------------------------
bool LwLinkPath::PathEntry::operator<(const PathEntry &rhs) const
{
    if (*pCon < *rhs.pCon)
        return true;
    if (pSrcDev->GetId() < rhs.pSrcDev->GetId())
        return true;
    if (bRemote == rhs.bRemote)
        return false;
    return bRemote;
}

//------------------------------------------------------------------------------
//! \brief Reverse the sense of the path entry to be from the perspective of the
//! remote device
void LwLinkPath::PathEntry::Reverse()
{
    pSrcDev = pCon->GetRemoteDevice(pSrcDev);
    if (pCon->HasLoopout())
        bRemote = !bRemote;
}

//------------------------------------------------------------------------------
//! \brief Default construtor for iterating over path entries.  Used to create
//! an iterator that can be used to check for the end of an iteration
LwLinkPath::PathEntryIterator::PathEntryIterator()
   : m_bReverse(false)
    ,m_LwrIndex(0)
    ,m_pPathEntries(nullptr)
{
}

//------------------------------------------------------------------------------
//! \brief Construtor for iterating over path entries.  Creates an iterator that
//! either iterates either forward or reverse on the path based on the parameter
//!
//! \param pPathEntries : Path entries to iterate over
//! \param bReverse     : true to iterate from end to beginning of the path
LwLinkPath::PathEntryIterator::PathEntryIterator
(
    const vector<PathEntry> *pPathEntries
   ,bool bReverse
)
   : m_bReverse(bReverse)
    ,m_pPathEntries(pPathEntries)
{
    m_LwrIndex = m_bReverse ? static_cast<int>(pPathEntries->size() - 1) : 0;
    m_LwrValue = pPathEntries->at(m_LwrIndex);
    if (m_bReverse)
        m_LwrValue.Reverse();
};

//------------------------------------------------------------------------------
//! \brief Function called to increment the iterator (required implementation
//! of iterator_facade
void LwLinkPath::PathEntryIterator::increment()
{
    const INT32 endIdx = m_bReverse ? -1 : static_cast<INT32>(m_pPathEntries->size());
    const INT32 inc    = m_bReverse ? -1 : 1;

    // If already at the end do nothing
    if (m_LwrIndex == endIdx)
        return;

    // Incrementing when already at the end of iteration sets m_pPathEntries
    // to null which effectively terminates all iteration and will cause equal()
    // to return true for the default constructor (used for checking iteration
    // endpoint).
    //
    // Also any further calls to increment after at the end do not change any
    // state
    m_LwrIndex += inc;
    if (m_LwrIndex == endIdx)
    {
        m_pPathEntries = nullptr;
        m_LwrValue = PathEntry();
        return;
    }
    m_LwrValue = m_pPathEntries->at(m_LwrIndex);
    if (m_bReverse)
        m_LwrValue.Reverse();
}

//------------------------------------------------------------------------------
//! \brief Check a different iterator for equality
//!
//! \param other : PathEntryIterator to check for equality against
//!
//! \return true if equal, false otherwise
bool LwLinkPath::PathEntryIterator::equal(PathEntryIterator const& other) const
{
    // Only check the data pointer for equality (allows the default constructor
    // to be used to create a "ending" iterator
    return this->m_pPathEntries == other.m_pPathEntries;
}

//------------------------------------------------------------------------------
//! \brief Add a entry to the path
//!
//! \param pCon        : Connection to add to the path
//! \param srcDev      : Source device for traffic on the connection
//! \param srcLinkMask : Link mask on the source device
//!
//! \return true if the connection was added, false if it was not
bool LwLinkPath::AddEntry(LwLinkConnectionPtr pCon, TestDevicePtr pSrcDev, UINT64 srcLinkMask)
{
    PathEntry pathEntry = { pCon, pSrcDev, false };
    TestDevicePtr pRemoteDev = pCon->GetRemoteDevice(pSrcDev);

    // Coming from an endpoint, this better be the first in the chain, coming
    // from a non-endpoint, it better not be the first in the chain
    MASSERT(pSrcDev->IsEndpoint() == m_PathEntries.empty());

    // Paths through bridges require fabric addresses
    //
    // Paths should only be created with a single destination fabric address
    // when a path is added to a route if multiple fabric addresses share
    // exactly the same path they will be consolidated
    MASSERT(pSrcDev->IsEndpoint() || (m_FabricBases.size() == 1));

    if (pCon->HasLoopout())
        pathEntry.bRemote = (srcLinkMask != pCon->GetLocalLinkMask(pSrcDev));

    for (auto const &lwrPe : m_PathEntries)
    {
        if (lwrPe == pathEntry)
        {
            Printf(Tee::PriLow, s_ModsLwLinkModule.GetCode(),
                   "%s : Path already contains new entry, discarding!\n",
                   __FUNCTION__);
            return false;
        }
    }

    // Add the current connection to the path
    m_PathEntries.push_back(pathEntry);

    return true;
}

//------------------------------------------------------------------------------
//! \brief Add all the specified fabric bases to the list of used bases
//!
//! \param fabricBases : Fabric base addresses to add to the used list
//!
void LwLinkPath::AddFabricBases(const set<UINT64> &fabricBases)
{
    m_FabricBases.insert(fabricBases.begin(), fabricBases.end());
}

//------------------------------------------------------------------------------
//! \brief Add the specified data type as covered by this path
//!
//! \param dt : Data type to add
//!
void LwLinkPath::AddDataType(LwLink::DataType dt)
{
    m_DataType = static_cast<LwLink::DataType>(m_DataType | dt);
}

//------------------------------------------------------------------------------
//! \brief Check if the connections used in the provided path are identical to
//! the connections in this path
//!
//! \param pCon   : Connection to add to the path
//! \param srcDev : Source device for traffic on the connection
//!
//! \return true if the connection was added, false if it was not
bool LwLinkPath::ConnectionsMatch(const LwLinkPath &path) const
{
    // Trivial check : Paths must be the same size
    if (path.m_PathEntries.size() != m_PathEntries.size())
        return false;

    // Trivial check : The first connection in this path must match either the
    // first or last connection of the provided path
    if ((*path.m_PathEntries.front().pCon != *m_PathEntries.front().pCon) &&
        (*path.m_PathEntries.back().pCon != *m_PathEntries.front().pCon))
    {
        return false;
    }

    // Iterate through all path entries in both paths starting from the same
    // device and check if all connections are the same
    PathEntryIterator pThisPathEntry = PathEntryBegin(m_PathEntries.front().pSrcDev);
    PathEntryIterator pThatPathEntry = path.PathEntryBegin(m_PathEntries.front().pSrcDev);
    PathEntryIterator pThisEnd = PathEntryIterator();
    while (pThisPathEntry != pThisEnd)
    {
        if (*pThisPathEntry != *pThatPathEntry)
            return false;
        ++pThisPathEntry;
        ++pThatPathEntry;
    }
    return true;
}

//------------------------------------------------------------------------------
//! \brief Get the data bandwidth on the path
//!
//! \return Data bandwidth on the path
UINT32 LwLinkPath::GetMaxDataBwKiBps() const
{
    UINT32 pathBwKiBps = 0;

    PathEntry pe = m_PathEntries.front();
    pathBwKiBps = m_PathEntries[0].pCon->GetMaxDataBwKiBps(pe.pSrcDev);
    for (size_t lwrEntry = 1; lwrEntry < m_PathEntries.size(); lwrEntry++)
    {
        pe = m_PathEntries[lwrEntry];
        pathBwKiBps = (min)(pathBwKiBps, pe.pCon->GetMaxDataBwKiBps(pe.pSrcDev));
    }
    return pathBwKiBps;
}

//------------------------------------------------------------------------------
//! \brief Get the endpoint devices on the path
//!
//! \return Endpoint devices on the path
LwLinkConnection::EndpointDevices LwLinkPath::GetEndpointDevices() const
{
    LwLinkConnection::EndpointDevices epDevs(m_PathEntries.front().pSrcDev, TestDevicePtr());
    epDevs.second = m_PathEntries.back().pCon->GetRemoteDevice(m_PathEntries.back().pSrcDev);
    return epDevs;
}

//------------------------------------------------------------------------------
//! \brief Get the links used on each of the endpoint devices of the path
//! relative to the specified device
//!
//! \param pDev : Device to get endpoint links relative to
//!
//! \return vector of endpoint ids.  The first element of the link pair will be
//! a link on the device passed as a parameter, the second element of the link
//! pair will be a link on the endpoint device at the other end of the path
vector<LwLinkConnection::EndpointIds> LwLinkPath::GetEndpointLinks(TestDevicePtr pDev) const
{
    vector<LwLinkConnection::EndpointIds> epIds;
    vector<LwLinkConnection::EndpointIds> ep1Ids;
    vector<LwLinkConnection::EndpointIds> ep2Ids;

    MASSERT(pDev->IsEndpoint() && UsesDevice(pDev));

    // Trivial paths cannot have a switch device and are a direct connection between
    // endpoints so simply return the endpoints from the connection the connection
    if (m_PathEntries.size() == 1)
        return m_PathEntries.front().pCon->GetLinks(pDev);

    // Get the links on the first end of the path
    ep1Ids = m_PathEntries.front().pCon->GetLinks(m_PathEntries.front().pSrcDev);

    // Get the links on the far end of the path
    LwLinkConnectionPtr pCon = m_PathEntries.back().pCon;
    ep2Ids = pCon->GetLinks(m_PathEntries.back().pSrcDev);

    const size_t maxEpSize = max(ep1Ids.size(), ep2Ids.size());
    for (size_t lwrEp = 0; lwrEp < maxEpSize; lwrEp++)
    {
        // Construct the complete endpoint array
        const UINT32 firstEp = (lwrEp >= ep1Ids.size()) ? LwLink::ILWALID_LINK_ID :
                                                          ep1Ids[lwrEp].first;
        const UINT32 secondEp = (lwrEp >= ep2Ids.size()) ? LwLink::ILWALID_LINK_ID :
                                                           ep2Ids[lwrEp].second;
        if (pDev != m_PathEntries.front().pSrcDev)
            epIds.push_back(LwLinkConnection::EndpointIds(secondEp, firstEp));
        else
            epIds.push_back(LwLinkConnection::EndpointIds(firstEp, secondEp));
    }

    sort(epIds.begin(), epIds.end(), SortByLocalLink);

    return epIds;
}

//------------------------------------------------------------------------------
//! \brief Get the fabric base for the path (there must be exactly one)
//!
//! \return Fabric base of the path
UINT64 LwLinkPath::GetFabricBase() const
{
    MASSERT(m_FabricBases.size() == 1);
    return *m_FabricBases.begin();
}

//------------------------------------------------------------------------------
//! \brief Get the link mask on the specifed endpoint
//!
//! \param pLocDev : Endpoint device to get the mask on
//!
//! \return Link mask on the specified endpoint
UINT64 LwLinkPath::GetLocalEndpointLinkMask(TestDevicePtr pLocDev) const
{
    return GetEndpointLinkMask(pLocDev, false);
}

//------------------------------------------------------------------------------
//! \brief Get the maximum bandwidth on the path
//!
//! \return Maximum bandwidth on the path
UINT32 LwLinkPath::GetMaxBwKiBps() const
{
    UINT32 pathBwKiBps = 0;

    PathEntry pe = m_PathEntries.front();
    pathBwKiBps = m_PathEntries[0].pCon->GetMaxBwKiBps(pe.pSrcDev);
    for (size_t lwrEntry = 1; lwrEntry < m_PathEntries.size(); lwrEntry++)
    {
        pe = m_PathEntries[lwrEntry];
        pathBwKiBps = (min)(pathBwKiBps, pe.pCon->GetMaxBwKiBps(pe.pSrcDev));
    }
    return pathBwKiBps;
}

//------------------------------------------------------------------------------
//! \brief Get the link mask on opposite endpoint from specified
//!
//! \param pLocDev : Endpoint device to get remote link mask for
//!
//! \return Link mask on the remote endpoint endpoint
UINT64 LwLinkPath::GetRemoteEndpointLinkMask(TestDevicePtr pLocDev) const
{
    return GetEndpointLinkMask(pLocDev, true);
}

//------------------------------------------------------------------------------
//! \brief Get the width of the path which is limited by the smallest connection
//! width
//!
//! \return Width of the path
size_t LwLinkPath::GetWidth() const
{
    size_t pathWidth = 0;
    PathEntry pe = m_PathEntries.front();
    pathWidth = m_PathEntries[0].pCon->GetWidth();
    for (size_t lwrEntry = 1; lwrEntry < m_PathEntries.size(); lwrEntry++)
    {
        pe = m_PathEntries[lwrEntry];
        pathWidth = (min)(pathWidth, pe.pCon->GetWidth());
    }
    return pathWidth;
}

//------------------------------------------------------------------------------
//! \brief Get an iterator for iterating over the path entries
//!
//! \return Width of the path
LwLinkPath::PathEntryIterator LwLinkPath::PathEntryBegin(TestDevicePtr pFromEndpoint) const
{
    return PathEntryIterator(&m_PathEntries,
                             (pFromEndpoint != m_PathEntries.front().pSrcDev));
}

//------------------------------------------------------------------------------
//! \brief Print the specified path
//!
//! \param pri             : Priority to print the path at
//! \param fromEndpoint    : Endpoint to start printing from
//! \param firstLinePrefix : Prefix for the first line of the path print
//! \param prefix          : Prefix for second and subsequent lines of the path
//!                          print or all lines if firstLinePrefix is empty
void  LwLinkPath::Print
(
    Tee::Priority pri,
    TestDevicePtr fromEndpoint,
    const string &firstLinePrefix,
    const string &prefix
) const
{
    MASSERT(fromEndpoint->IsEndpoint() && UsesDevice(fromEndpoint));

    string lwrPrefix = (firstLinePrefix == "") ? prefix : firstLinePrefix;

    string pathStr;
    if (!m_FabricBases.empty())
    {
        lwrPrefix = prefix;
        pathStr   = lwrPrefix;
        size_t lwrBaseNum = 0;
        for (auto lwrBase : m_FabricBases)
        {
            pathStr += Utility::StrPrintf("0x%012llx :", lwrBase);
            if (lwrBaseNum != (m_FabricBases.size() - 1))
                pathStr += "\n" + lwrPrefix;
            else
                pathStr += " ";
            lwrBaseNum++;
        }
        lwrPrefix += "                 ";
    }
    else
    {
        pathStr   = lwrPrefix;
        lwrPrefix = prefix;
    }

    PathEntryIterator pPe = PathEntryBegin(fromEndpoint);
    PathEntryIterator peEnd = PathEntryIterator();
    for (; pPe != peEnd; ++pPe)
    {
        TestDevicePtr pLwrDev = pPe->pSrcDev;
        pathStr += pLwrDev->GetName() + ", Link";
        pathStr += pPe->pCon->GetWidth() > 1 ? "s " : " ";

        // Ensure that the link string is on the current device side of the
        // connection for loopout connections my ignoring any masks containing
        // bits not in the source mask
        string linkString;
        if (pPe->bRemote)
            linkString = pPe->pCon->GetRemoteLinkString(pLwrDev, pLwrDev);
        else
            linkString = pPe->pCon->GetLocalLinkString(pLwrDev, pLwrDev);

        pathStr += linkString + "\n" + lwrPrefix;

        TestDevicePtr pRemDev = pPe->pCon->GetRemoteDevice(pLwrDev);
        pathStr += pRemDev->GetName() + ", Link";
        pathStr += pPe->pCon->GetWidth() > 1 ? "s " : " ";

        // Ensure that the link string is on the opposite device side of the
        // connection for loopout connections my ignoring any masks containing
        // bits in the source mask
        if (pPe->bRemote)
            linkString = pPe->pCon->GetLocalLinkString(pLwrDev, pLwrDev);
        else
            linkString = pPe->pCon->GetRemoteLinkString(pLwrDev, pLwrDev);

        pathStr += linkString + "\n";

        if (!pRemDev->IsEndpoint())
            pathStr += lwrPrefix;
    }

    Printf(pri, s_ModsLwLinkModule.GetCode(), "%s\n", pathStr.c_str());
}

//------------------------------------------------------------------------------
//! \brief Check if the path uses the specified connection anywhere in the path
//!
//! \param pConnection : Connection to check for use
//!
//! \return true if the connection is used, false otherwise
bool LwLinkPath::UsesConnection(LwLinkConnectionPtr pConnection) const
{
    for (size_t lwrEntry = 0; lwrEntry < m_PathEntries.size(); lwrEntry++)
    {
        if (m_PathEntries[lwrEntry].pCon == pConnection)
            return true;
    }
    return false;
}

//------------------------------------------------------------------------------
//! \brief Check if the path uses the specified device anywhere in the path
//!
//! \param pDevice : Device to check for use
//!
//! \return true if the device is used, false otherwise
bool LwLinkPath::UsesDevice(TestDevicePtr pDevice) const
{
    for (size_t lwrEntry = 0; lwrEntry < m_PathEntries.size(); lwrEntry++)
    {
        if (m_PathEntries[lwrEntry].pCon->UsesDevice(pDevice))
            return true;
    }
    return false;
}

//------------------------------------------------------------------------------
//! \brief Check if the path uses all the specified fabric bases already
//!
//! \param fabricBases : Fabric bases to check for use
//!
//! \return true if all fabric bases are used, false otherwise
bool LwLinkPath::UsesFabricBases(const set<UINT64> &fabricBases) const
{
    if (fabricBases.empty())
        return m_FabricBases.empty();

    for (auto const &base : fabricBases)
    {
        if (!m_FabricBases.count(base))
            return false;
    }
    return true;
}

//------------------------------------------------------------------------------
//! \brief Check if the path is valid
//!
//! \param pbValid : Pointer to returned indicator if the path is valid
//!
//! \return OK if checking validity succeeds, false otherwise
RC LwLinkPath::Validate(bool *pbValid) const
{
    MASSERT(pbValid);
    *pbValid = false;

    // Get the terminating endpoints from the path being added
    LwLinkConnection::EndpointDevices endpoints = GetEndpointDevices();

    // Both endpoints must exist
    if (!endpoints.first || !endpoints.second)
    {
        Printf(Tee::PriError, s_ModsLwLinkModule.GetCode(),
               "%s : Path does not have 2 endpoints!\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    // Both endpoints must really be endpoints
    if (!endpoints.first->IsEndpoint() || !endpoints.second->IsEndpoint())
    {
        Printf(Tee::PriError, s_ModsLwLinkModule.GetCode(),
               "%s : Path does not begin and end with an endpoint device!\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    if ((m_PathEntries.size() == 1) && !m_FabricBases.empty())
    {
        Printf(Tee::PriError, s_ModsLwLinkModule.GetCode(),
               "%s : Direct connect paths cannot have fabric base addresses specified!\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    *pbValid = true;
    return OK;
}

//------------------------------------------------------------------------------
// LwLinkRoute Private Implementation
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! \brief Get the link mask on the specifed endpoint
//!
//! \param pDev    : Endpoint device to get the mask on
//! \param bRemote : Use remote endpoints for a loopout connection
//!
//! \return Link mask on the specified endpoint
UINT64 LwLinkPath::GetEndpointLinkMask(TestDevicePtr pDev, bool bRemote) const
{
    MASSERT(pDev->IsEndpoint() && UsesDevice(pDev));
    LwLinkConnection::EndpointDevices epDevs = GetEndpointDevices();

    if (epDevs.first == epDevs.second)
    {
        if (bRemote)
            return m_PathEntries.back().pCon->GetRemoteLinkMask(epDevs.first);
        else
            return m_PathEntries.back().pCon->GetLocalLinkMask(epDevs.first);
    }

    if ((pDev == epDevs.first) != bRemote)
    {
        return m_PathEntries.front().pCon->GetLocalLinkMask(epDevs.first);
    }
    return m_PathEntries.back().pCon->GetLocalLinkMask(epDevs.second);
}

//------------------------------------------------------------------------------
// LwLinkRoute Public Implementation
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! \brief Constructor
//!
LwLinkRoute::LwLinkRoute(LwLinkConnection::EndpointDevices endpoints)
 : m_EndpointDevs(endpoints)
{
}

//------------------------------------------------------------------------------
void LwLinkRoute::ConsolidatePathsByDataType()
{
    // Keep track of elements that need to be erased due to consolidation
    vector<bool> eraseElements(m_Paths.size(), false);

    // At this point paths will all contain a single data type and also have been
    // consolidated with fabric base addresses if possible.
    for (size_t modifyIndex = 0; modifyIndex < m_Paths.size(); modifyIndex++)
    {
        if (eraseElements[modifyIndex])
            continue;

        LwLinkPath &modifyPath = m_Paths[modifyIndex];
        for (size_t lwrIdx = modifyIndex + 1; lwrIdx < m_Paths.size(); lwrIdx++)
        {
            if (eraseElements[lwrIdx])
                continue;

            // If the endpoint devices are the same (i.e. paths are in the same
            // direction), the paths use the same connections, and the paths use
            // the same fabric bases then the paths can have their data types
            // combined
            const LwLinkPath & lwrPath = m_Paths[lwrIdx];
            if ((modifyPath.GetEndpointDevices() == lwrPath.GetEndpointDevices()) &&
                modifyPath.ConnectionsMatch(lwrPath) &&
                (modifyPath.GetFabricBases() == lwrPath.GetFabricBases()))
            {
                modifyPath.AddDataType(lwrPath.GetDataType());
                eraseElements[lwrIdx] = true;
            }
        }
    }

    // Erase any elemnts from the path list that were consolidated
    UINT32 lwrIndex = 0;
    auto pLwrPath = m_Paths.begin();
    while (pLwrPath != m_Paths.end())
    {
        if (eraseElements[lwrIndex])
            pLwrPath = m_Paths.erase(pLwrPath);
        else
            pLwrPath++;
        lwrIndex++;
    }
    sort(m_Paths.begin(), m_Paths.end(), SortPaths);
}

//------------------------------------------------------------------------------
//! \brief Add a path to the route
//!
//! \param path : Path to add to the route
//!
//! \return OK if adding the path succeeds, not OK otherwise
RC LwLinkRoute::AddPath(LwLinkPath path)
{
    if (path.GetFabricBases().empty())
        return AddDirectPath(path);

    return AddFabricPath(path);
}

//------------------------------------------------------------------------------
//! \brief Check if all paths in the route are bidirectional
//!
//! \return true if all paths in the route are bidirectional
//!
bool LwLinkRoute::AllPathsBidirectional() const
{
    for (size_t ii = 0; ii < m_Paths.size(); ii++)
    {
        if (!m_Paths[ii].IsBidirectional())
            return false;
    }
    return true;
}

//------------------------------------------------------------------------------
//! \brief Get the data bandwidth on the route relative to the specified
//! device
//!
//! \param pDev : Endpoint device to get bandwidth relative to
//! \param dt   : Data type to get data bandwidth for
//!
//! \return Data bandwidth on the route
UINT32 LwLinkRoute::GetMaxDataBwKiBps(TestDevicePtr pDev, LwLink::DataType dt) const
{
    MASSERT(pDev);
    MASSERT(pDev->IsEndpoint());
    MASSERT((pDev == m_EndpointDevs.first) || (pDev == m_EndpointDevs.second));

    map<UINT64, UINT32> maxDataBwPerMask;
    PathIterator pPath = PathsBegin(pDev, dt);
    PathIterator pathEnd = PathsEnd(pDev, dt);
    for (; pPath != pathEnd; ++pPath)
    {
        const UINT64 linkMask = pPath->GetLocalEndpointLinkMask(pDev);
        const UINT32 pathMaxDataBwKiBps = pPath->GetMaxDataBwKiBps();;
        if ((maxDataBwPerMask.find(linkMask) == maxDataBwPerMask.end()) ||
            (pathMaxDataBwKiBps > maxDataBwPerMask[linkMask]))
        {
            maxDataBwPerMask[linkMask] = pathMaxDataBwKiBps;
        }
    }

    // TODO : Note that this assumes that multiple paths will not share an
    // intermediate connection to the point where there are more source
    // connections than the width of the shared connection.  For instance
    // 3 paths that are single links at the endpoint device that share a
    // trunk connection that is only 2 links wide.
    MASSERT(!AnyConnectionsOversubscribed(pDev, dt));

    UINT32 totalBwKiBps = 0;
    for (auto const & lwrMaskBw : maxDataBwPerMask)
        totalBwKiBps += lwrMaskBw.second;

    return totalBwKiBps;
}

//------------------------------------------------------------------------------
//! \brief Get the links used on each of the endpoint devices of the route
//! relative to the specified device
//!
//! \param pDev : Device to get endpoint links relative to
//! \param dt   : Data type to get the endpoint links for
//!
//! \return vector of endpoint ids.  The first element of the link pair will be
//! a link on the device passed as a parameter, the second element of the link
//! pair will be a link on the endpoint device at the other end of the path
vector<LwLinkConnection::EndpointIds> LwLinkRoute::GetEndpointLinks
(
    TestDevicePtr    pDev
   ,LwLink::DataType dt
) const
{
    MASSERT(pDev);
    MASSERT(pDev->IsEndpoint());
    MASSERT((pDev == m_EndpointDevs.first) || (pDev == m_EndpointDevs.second));
    MASSERT(!m_Paths.empty());

    // For direct connect routes (which are guaranteed to only have one path entry
    // without fabric bases) simply return the links from the first path
    if (m_Paths[0].GetFabricBases().empty())
        return m_Paths[0].GetEndpointLinks(pDev);

    PathIterator pPath = PathsBegin(pDev, dt);
    PathIterator pathEnd = PathsEnd(pDev, dt);
    set<UINT32> firstEps;
    set<UINT32> secondEps;
    for (; pPath != pathEnd; ++pPath)
    {
        vector<LwLinkConnection::EndpointIds> pathEpIds = pPath->GetEndpointLinks(pDev);
        for (auto const & lwrPathEp : pathEpIds)
        {
            if (lwrPathEp.first != LwLink::ILWALID_LINK_ID)
                firstEps.insert(lwrPathEp.first);
            if (lwrPathEp.second != LwLink::ILWALID_LINK_ID)
                secondEps.insert(lwrPathEp.second);
        }
    }

    vector<LwLinkConnection::EndpointIds> eIds(max(firstEps.size(), secondEps.size()),
                                      { LwLink::ILWALID_LINK_ID, LwLink::ILWALID_LINK_ID });

    UINT32 lwrIdx = 0;
    for (auto const & lwrFirstEp : firstEps)
    {
        eIds[lwrIdx].first = lwrFirstEp;
        lwrIdx++;
    }
    lwrIdx = 0;
    for (auto const & lwrSecondEp : secondEps)
    {
        eIds[lwrIdx].second = lwrSecondEp;
        lwrIdx++;
    }

    return eIds;
}

//------------------------------------------------------------------------------
//! \brief Get the maximum bandwidth on the route relative to the specified
//! device
//!
//! \param pDev : Endpoint device to get bandwidth relative to
//! \param dt   : Data type to get maximum bandwidth for
//!
//! \return Maximum bandwidth on the route
UINT32 LwLinkRoute::GetMaxBwKiBps(TestDevicePtr pDev, LwLink::DataType dt) const
{
    MASSERT(pDev);
    MASSERT(pDev->IsEndpoint());
    MASSERT((pDev == m_EndpointDevs.first) || (pDev == m_EndpointDevs.second));

    map<UINT64, FLOAT64> maxBwPerMask;
    PathIterator pPath = PathsBegin(pDev, dt);
    PathIterator pathEnd = PathsEnd(pDev, dt);
    for (; pPath != pathEnd; ++pPath)
    {
        const UINT64 linkMask = pPath->GetLocalEndpointLinkMask(pDev);
        const UINT32 pathMaxBwKiBps = pPath->GetMaxBwKiBps();;
        if ((maxBwPerMask.find(linkMask) == maxBwPerMask.end()) ||
            (pathMaxBwKiBps > maxBwPerMask[linkMask]))
        {
            maxBwPerMask[linkMask] = pathMaxBwKiBps;
        }
    }

    // TODO : Note that this assumes that multiple paths will not share an
    // intermediate connection to the point where there are more source
    // connections than the width of the shared connection.  For instance
    // 3 paths that are single links at the endpoint device that share a
    // trunk connection that is only 2 links wide.
    MASSERT(!AnyConnectionsOversubscribed(pDev, dt));

    UINT32 totalBwKiBps = 0;
    for (auto const & lwrMaskBw : maxBwPerMask)
        totalBwKiBps += lwrMaskBw.second;

    return totalBwKiBps;
}

//------------------------------------------------------------------------------
//! \brief Get the maximum length of all paths that use the specified device as
//! an endpoint
//!
//! \param pDev : Endpoint device to get maximum length relative to
//! \param dt   : Data type to get length for
//!
//! \return Maximum length of all paths with the specified device as an endpoint
size_t LwLinkRoute::GetMaxLength(TestDevicePtr pDev, LwLink::DataType dt) const
{
    MASSERT(pDev);
    MASSERT(pDev->IsEndpoint());
    MASSERT((pDev == m_EndpointDevs.first) || (pDev == m_EndpointDevs.second));

    size_t maxLength = 0;
    PathIterator pPath = PathsBegin(pDev, dt);
    PathIterator pathEnd = PathsEnd(pDev, dt);
    for (; pPath != pathEnd; ++pPath)
    {
        maxLength = (max)(maxLength, pPath->GetLength());
    }
    return maxLength;
}

//------------------------------------------------------------------------------
//! \brief Get the remote endpoint device
//!
//! \param pLocEndpoint : Local endpoint to get the remote endpoint for
//!
//! \return Remote endpoint device
TestDevicePtr LwLinkRoute::GetRemoteEndpoint(TestDevicePtr pLocEndpoint) const
{
    MASSERT(pLocEndpoint);
    MASSERT(pLocEndpoint->IsEndpoint());
    MASSERT((pLocEndpoint == m_EndpointDevs.first) || (pLocEndpoint == m_EndpointDevs.second));

    if (m_EndpointDevs.first == pLocEndpoint)
        return m_EndpointDevs.second;
    return m_EndpointDevs.first;
}

//------------------------------------------------------------------------------
//! \brief Get a single string representing the route
//!
//! \param pLocDev : Local device to get the string relative to
//! \param prefix  : Prefix for the string
//!
//! \return Single string representing the route
//!
string LwLinkRoute::GetString(TestDevicePtr pLocDev, LwLink::DataType dt, string prefix) const
{
    MASSERT(pLocDev);
    MASSERT(pLocDev->IsEndpoint());
    MASSERT((pLocDev == m_EndpointDevs.first) || (pLocDev == m_EndpointDevs.second));

    TestDevicePtr pRemDev = GetRemoteEndpoint(pLocDev);
    return prefix + Utility::StrPrintf("%s Link(s) %s to %s Link(s) %s\n",
                                       pLocDev->GetName().c_str(),
                                       GetEndpointLinkString(pLocDev, pLocDev, false, dt).c_str(),
                                       pRemDev->GetName().c_str(),
                                       GetEndpointLinkString(pRemDev, pLocDev, true, dt).c_str());
}

//------------------------------------------------------------------------------
//! \brief Get the width of the route with the specified device as the starting
//! endpoint
//!
//! \param pDev : Endpoint device to get width relative to
//! \param dt   : Data type to get width for
//!
//! \return Total width of the route
size_t LwLinkRoute::GetWidth(TestDevicePtr pDev, LwLink::DataType dt) const
{
    MASSERT(pDev);

    set<UINT64> scannedMasks;
    size_t totalWidth = 0;
    PathIterator pPath = PathsBegin(pDev, dt);
    PathIterator pathEnd = PathsEnd(pDev, dt);
    for (; pPath != pathEnd; ++pPath)
    {
        const UINT64 linkMask = pPath->GetLocalEndpointLinkMask(pDev);
        if (scannedMasks.count(linkMask))
            continue;

        totalWidth += pPath->GetWidth();
        scannedMasks.insert(linkMask);
    }

    return totalWidth;
}

//------------------------------------------------------------------------------
//! \brief Get the width of the route at the specified endpoint
//!
//! \param pDev : Endpoint device to get width at
//!
//! \return Width at the endpoinnt
size_t LwLinkRoute::GetWidthAtEndpoint(TestDevicePtr pDev) const
{
    PathIterator pPath = PathsBegin(pDev, LwLink::DT_ALL);
    PathIterator pathEnd = PathsEnd(pDev, LwLink::DT_ALL);
    UINT64 linkMask = 0;
    for (; pPath != pathEnd; ++pPath)
    {
        linkMask |= pPath->GetLocalEndpointLinkMask(pDev);

        // For a path where the local/remote device are the same, need to add in
        // the remote endpoint link mask to account for loopout links
        auto endpointDevs = pPath->GetEndpointDevices();
        if (endpointDevs.first == endpointDevs.second)
            linkMask |= pPath->GetRemoteEndpointLinkMask(pDev);
    }
    return Utility::CountBits(linkMask);
}

//------------------------------------------------------------------------------
//! \brief Return true if the route is a looped route (either loopback or
//! loopout)
//!
//! \return True if the route is looped
bool LwLinkRoute::IsLoop() const
{
    return m_EndpointDevs.first == m_EndpointDevs.second;
}

//------------------------------------------------------------------------------
//! \brief Return true if the route is a P2P route (traffic between 2 GPUs)
//!
//! \return True if the route is P2P
bool LwLinkRoute::IsP2P() const
{
    return ((m_EndpointDevs.first->GetType() == TestDevice::TYPE_LWIDIA_GPU ||
             m_EndpointDevs.first->GetType() == TestDevice::TYPE_TREX) &&
            (m_EndpointDevs.second->GetType() == TestDevice::TYPE_LWIDIA_GPU ||
             m_EndpointDevs.second->GetType() == TestDevice::TYPE_TREX) &&
            !m_EndpointDevs.first->IsSOC() && !m_EndpointDevs.second->IsSOC());
}

//------------------------------------------------------------------------------
//! \brief Return true if the route is a Sysmem route
//!
//! \return True if the route is Sysmem
bool LwLinkRoute::IsSysmem() const
{
    return !IsP2P();
}

//------------------------------------------------------------------------------
//! \brief Print the route at the specified priority
//!
//! \param pri : Priority to print the route at
//!
void LwLinkRoute::Print(Tee::Priority pri) const
{
    const bool bBiDir = AllPathsBidirectional();
    const bool bLoopback = m_EndpointDevs.second == m_EndpointDevs.first;
    const bool bUsesFabricAddrs = !m_Paths[0].GetFabricBases().empty();
    bool bDataFlowIdentical = AllDataFlowIdentical(m_EndpointDevs.first);
    const string tableHeader =
        Utility::StrPrintf("    %s\n"
                           "    ---------------------------------------------------------------\n",
                           bUsesFabricAddrs ? "   Fabric Base : Path" : "PathNum : Path");

    Printf(pri, s_ModsLwLinkModule.GetCode(),
           "%s%sRoute from %s to %s%s:\n%s",
           bUsesFabricAddrs ? "Request " : "",
           (bUsesFabricAddrs && bDataFlowIdentical) ? "and Response " : "",
           m_EndpointDevs.first->GetName().c_str(),
           m_EndpointDevs.second->GetName().c_str(),
           bLoopback ? "(Loopback)" : bBiDir ? "(Bidirectional)" : "",
           tableHeader.c_str());

    PrintPaths(pri, m_EndpointDevs.first, LwLink::DT_REQUEST, bUsesFabricAddrs);
    if (!bDataFlowIdentical)
    {
        Printf(pri, s_ModsLwLinkModule.GetCode(),
               "Response Route from %s to %s%s:\n%s",
               m_EndpointDevs.first->GetName().c_str(),
               m_EndpointDevs.second->GetName().c_str(),
               bLoopback ? "(Loopback)" : bBiDir ? "(Bidirectional)" : "",
               tableHeader.c_str());
        PrintPaths(pri, m_EndpointDevs.first, LwLink::DT_RESPONSE, bUsesFabricAddrs);
    }

    if (bLoopback || bBiDir)
        return;

    bDataFlowIdentical = AllDataFlowIdentical(m_EndpointDevs.second);
    Printf(pri, s_ModsLwLinkModule.GetCode(),
           "%s%sRoute from %s to %s:\n%s",
           bUsesFabricAddrs ? "Request " : "",
           (bUsesFabricAddrs && bDataFlowIdentical) ? "and Response " : "",
           m_EndpointDevs.second->GetName().c_str(),
           m_EndpointDevs.first->GetName().c_str(),
           tableHeader.c_str());

    PrintPaths(pri, m_EndpointDevs.second, LwLink::DT_REQUEST, bUsesFabricAddrs);

    if (!bDataFlowIdentical)
    {
        Printf(pri, s_ModsLwLinkModule.GetCode(),
               "Response Route from %s to %s:\n%s",
               m_EndpointDevs.second->GetName().c_str(),
               m_EndpointDevs.first->GetName().c_str(),
               tableHeader.c_str());
        PrintPaths(pri, m_EndpointDevs.second, LwLink::DT_RESPONSE, bUsesFabricAddrs);
    }

}

//------------------------------------------------------------------------------
//! \brief Check if any path in the route uses the specified connection
//!
//! \param pConnection : Connection to check if the route uses
//!
//! \return true if the route uses the connection, false otherwise
//!
bool LwLinkRoute::UsesConnection(LwLinkConnectionPtr pConnection) const
{
    for (size_t ii = 0; ii < m_Paths.size(); ii++)
    {
        if (m_Paths[ii].UsesConnection(pConnection))
        {
            return true;
        }
    }
    return false;
}

//------------------------------------------------------------------------------
bool LwLinkRoute::UsesConnection
(
    TestDevicePtr       pFromEndpoint,
    LwLinkConnectionPtr pConnection,
    LwLink::DataType    dt
) const
{
    PathIterator pPath = PathsBegin(pFromEndpoint, dt);
    PathIterator pEnd = PathsEnd(pFromEndpoint, dt);

    for (; pPath != pEnd; ++pPath)
    {
        if (pPath->UsesConnection(pConnection))
            return true;
    }
    return false;
}

//------------------------------------------------------------------------------
//! \brief Check if any path in the route uses the specified device
//!
//! \param pDevice : Device to check if the route uses
//!
//! \return true if the route uses the device, false otherwise
//!
bool LwLinkRoute::UsesDevice(TestDevicePtr pDevice) const
{
    for (size_t ii = 0; ii < m_Paths.size(); ii++)
    {
        if (m_Paths[ii].UsesDevice(pDevice))
            return true;
    }
    return false;
}

//------------------------------------------------------------------------------
//! \brief Check if the route connects the specified endpoints
//!
//! \param endpoints : Endpoints to check if the route connects
//!
//! \return true if the route connects the two endpoints, false otherwise
//!
bool LwLinkRoute::UsesEndpoints(LwLinkConnection::EndpointDevices endpoints) const
{
    if (m_EndpointDevs == endpoints)
        return true;
    LwLinkConnection::EndpointDevices swappedEndpoints(endpoints.second,
                                                       endpoints.first);
    if (m_EndpointDevs == swappedEndpoints)
        return true;
    return false;
}

//------------------------------------------------------------------------------
// LwLinkRoute Private Implementation
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! \brief Add a fabric based path to the route
//!
//! \param path : Path to add to the route
//!
//! \return OK if adding the path succeeds, not OK otherwise
RC LwLinkRoute::AddFabricPath(LwLinkPath path)
{
    MASSERT(path.GetFabricBases().size() == 1);

    // Ensure that for loopout/loopback connections both masks are retrieved by
    // retrieving the remote mask
    const UINT64 pathEp1LinkMask =
        path.GetLocalEndpointLinkMask(m_EndpointDevs.first);
    UINT64 pathEp2LinkMask =
        path.GetLocalEndpointLinkMask(m_EndpointDevs.second);
    if (m_EndpointDevs.first == m_EndpointDevs.second)
        pathEp2LinkMask = path.GetRemoteEndpointLinkMask(m_EndpointDevs.second);

    UINT64 fabricBase = *path.GetFabricBases().begin();
    for (auto & lwrPath : m_Paths)
    {
        // Consolidate by fabric address first for maximum speed and data type
        // later.  Trying to do both at once is not possible since request
        // and response paths are added seperately and can be different
        if (lwrPath.GetDataType() != path.GetDataType())
            continue;

        const bool bPathDirectionsMatch =
            (lwrPath.GetEndpointDevices() == path.GetEndpointDevices());

        if (lwrPath.ConnectionsMatch(path) && bPathDirectionsMatch)
        {
            if (lwrPath.GetFabricBases().count(fabricBase))
            {
                Printf(Tee::PriLow, s_ModsLwLinkModule.GetCode(),
                       "%s : Duplicate path found, skipping!\n", __FUNCTION__);
            }
            else
            {
                lwrPath.AddFabricBases(path.GetFabricBases());
            }
            return OK;
        }

        // Ensure that for loopout/loopback connections both masks are retrieved by
        // retrieving the remote mask
        const UINT64 lwrEp1LinkMask =
            lwrPath.GetLocalEndpointLinkMask(m_EndpointDevs.first);
        UINT64 lwrEp2LinkMask =
            lwrPath.GetLocalEndpointLinkMask(m_EndpointDevs.second);
        if (m_EndpointDevs.first == m_EndpointDevs.second)
            lwrEp2LinkMask = lwrPath.GetRemoteEndpointLinkMask(m_EndpointDevs.second);
        if ((lwrEp1LinkMask == pathEp1LinkMask) &&
            (lwrEp2LinkMask == pathEp2LinkMask))
        {
            // If the endpoint links are identical, the path directions match,
            // fabric bases are identical, and the data type has already been
            // accounted for then the path is identical but has different
            // intermediate steps
            if (bPathDirectionsMatch &&
                (lwrPath.GetDataType() & path.GetDataType()))
            {
                // Identical endpoint IDs with different intermediate steps are
                // valid for fabric base type paths
                Printf(Tee::PriLow,
                       s_ModsLwLinkModule.GetCode(),
                       "%s : Multiple paths found in the same direction between "
                       "the same links!\n",
                       __FUNCTION__);

            }
        }
        else if ((lwrEp1LinkMask & pathEp1LinkMask) ||
                 (lwrEp2LinkMask & pathEp2LinkMask))
        {
            Printf(Tee::PriLow, s_ModsLwLinkModule.GetCode(),
                   "%s : Multiple paths found with non-equal overlapping "
                   "endpoint masks!\n",
                   __FUNCTION__);
        }
    }
    m_Paths.push_back(path);
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Add a direct connect path to the route
//!
//! \param path : Path to add to the route
//!
//! \return OK if adding the path succeeds, not OK otherwise
RC LwLinkRoute::AddDirectPath(LwLinkPath path)
{
    MASSERT(path.GetFabricBases().size() == 0);

    if (m_Paths.empty())
    {
        m_Paths.push_back(path);
        return OK;
    }

    if (m_Paths[0].ConnectionsMatch(path))
    {
        if (m_Paths[0].IsBidirectional() ||
            (m_Paths[0].GetEndpointDevices() == path.GetEndpointDevices()))
        {
            Printf(Tee::PriLow, s_ModsLwLinkModule.GetCode(),
                   "%s : Duplicate path found, skipping!\n", __FUNCTION__);
        }
        else
        {
            // Otherwise, the path is not bidirectional and the path directions
            // do not match, simply flag the path as bidirectional
            m_Paths[0].SetBidirectional();
        }
        return OK;
    }

    Printf(Tee::PriError, s_ModsLwLinkModule.GetCode(),
           "%s : Directly connected routes can only have a single Path!\n",
           __FUNCTION__);

    LwLinkConnection::EndpointDevices existingEps =  m_Paths[0].GetEndpointDevices();
    LwLinkConnection::EndpointDevices newEps =  path.GetEndpointDevices();
    TestDevicePtr fromEndpoint = newEps.first;
    if (!m_Paths[0].IsBidirectional() && (newEps.first != existingEps.first))
        fromEndpoint = existingEps.first;

    Printf(Tee::PriError, s_ModsLwLinkModule.GetCode(),
           "  Route from %s to %s\n",
           newEps.first->GetName().c_str(),
           newEps.second->GetName().c_str());

    Printf(Tee::PriError, "  Existing Path:\n");
    m_Paths[0].Print(Tee::PriError, fromEndpoint, "", "    ");
    Printf(Tee::PriError, "  New Path:\n");
    path.Print(Tee::PriError, newEps.first, "", "    ");
    return RC::SOFTWARE_ERROR;
}

//------------------------------------------------------------------------------
//! \brief Check if data flow for requests and responses is identical for all
//! paths in the route
//!
//! \param pFromEndpoint : Endpoint to check for identical data flow on
//!
//! \return true if data flow for requests and responses is identical for all
//! paths in the route
//!
bool LwLinkRoute::AllDataFlowIdentical(TestDevicePtr pFromEndpoint) const
{
    PathIterator pPath = PathsBegin(pFromEndpoint, LwLink::DT_ALL);
    PathIterator pEnd = PathsEnd(pFromEndpoint, LwLink::DT_ALL);

    for (; pPath != pEnd; ++pPath)
    {
        if (pPath->GetDataType() != LwLink::DT_ALL)
            return false;
    }
    return true;
}

//------------------------------------------------------------------------------
// Determine if any connections in the entire route have more traffic routed through
// them than they can handle
bool LwLinkRoute::AnyConnectionsOversubscribed(TestDevicePtr pDev, LwLink::DataType dt) const
{
    struct ConnectionUsage
    {
        LwLinkConnectionPtr pCon;
        size_t              widthUsed;
    };
    vector<ConnectionUsage> perConnectionUsage;

    PathIterator pPath = PathsBegin(pDev, dt);
    PathIterator pathEnd = PathsEnd(pDev, dt);
    for (; pPath != pathEnd; ++pPath)
    {
        LwLinkPath::PathEntryIterator pPathEntry = pPath->PathEntryBegin(pDev);
        LwLinkPath::PathEntryIterator pPathEnd = LwLinkPath::PathEntryIterator();
        vector<LwLinkConnectionPtr> pathConnections;
        while (pPathEntry != pPathEnd)
        {
            // If the route is a loopback route, ensure that we only count each connection
            // once per path otherwise it would count both out and back and the connection
            // would always be oversubscribed
            if (IsLoop())
            {
                auto findConScanned = [&] (LwLinkConnectionPtr & con) -> bool {
                    return con == pPathEntry->pCon;
                };
                auto pConScanned =
                    find_if(pathConnections.begin(), pathConnections.end(), findConScanned);
                if (pConScanned != pathConnections.end())
                {
                    pPathEntry++;
                    continue;
                }
                pathConnections.push_back(pPathEntry->pCon);
            }

            auto findCon = [&] (const ConnectionUsage & lw) -> bool {
                return lw.pCon == pPathEntry->pCon;
            };
            auto pConUsage =
                find_if(perConnectionUsage.begin(), perConnectionUsage.end(), findCon);
            if (pConUsage != perConnectionUsage.end())
            {
                pConUsage->widthUsed += pPath->GetWidth();
                if (pConUsage->widthUsed > pConUsage->pCon->GetWidth())
                {
                    Printf(Tee::PriWarn,
                           "Connection %s is oversubscribed\n",
                           pConUsage->pCon->GetString(pConUsage->pCon->GetDevices().first, "").c_str()); //$
                    return true;
                }
            }
            else
            {
                perConnectionUsage.push_back({ pPathEntry->pCon, pPath->GetWidth() });
            }
            pPathEntry++;
        }
    }
    return false;
}

//------------------------------------------------------------------------------
//! \brief Get a string representing the links for the connection on the
//! provided local device.  The link numbers will be sorted by the local device
//! unless a different sort device is specified
//!
//! \param pEndpoint : Endpoint device for generating the link string
//! \param pSortDev  : Device that the link numbers should be sorted based on
//! \param bRemote   : Get the remote links if it is loopout
//!
//! \return String representing the links on the endpoint device
//!
string LwLinkRoute::GetEndpointLinkString
(
    TestDevicePtr pEndpoint
   ,TestDevicePtr pSortDev
   ,bool bRemote
   ,LwLink::DataType dt
) const
{
    vector<LwLinkConnection::EndpointIds> eIds = GetEndpointLinks(pEndpoint, dt);
    sort(eIds.begin(), eIds.end(),
         (!pSortDev || (pEndpoint != pSortDev)) ? SortByLocalLink : SortByRemoteLink);

    // For loopout connections (same device but different endpoints) simply
    // getting the mask relative to a device is not enough since there are 2
    // different masks on the same device.  Specify whether to use the first or
    //second endpoints for loopout connections
    const bool bUseFirst = !m_Paths.front().PathEntryBegin(pEndpoint)->pCon->HasLoopout() ||
                           (m_Paths.front().PathEntryBegin(pEndpoint)->pCon->HasLoopout() &&
                            !bRemote);
    string linkStr = "";
    if (eIds.size() > 1)
    {
        linkStr += "(";

        string comma = "";
        for (auto const & lwrEpId : eIds)
        {
            UINT32 lwrLink = bUseFirst ? lwrEpId.first : lwrEpId.second;
            if (lwrLink == LwLink::ILWALID_LINK_ID)
                continue;

            linkStr += comma;
            linkStr += Utility::StrPrintf("%u", lwrLink);
            comma = ",";
        }
        linkStr += ")";
    }
    else
    {
        linkStr += Utility::StrPrintf("%u", bUseFirst ? eIds[0].first : eIds[0].second);
    }

    return linkStr;
}

//------------------------------------------------------------------------------
//! \brief Print the paths in the route
//!
//! \param pri              : Priority to print the path at
//! \param fromDev          : Device to print the path relative to
//! \param dt               : Data type to print the path for
//! \param bUsesFabricAddrs : True if the path uses fabric addresses
//!
void LwLinkRoute::PrintPaths
(
    Tee::Priority    pri
   ,TestDevicePtr     fromDev
   ,LwLink::DataType dt
   ,bool             bUsesFabricAddrs
) const
{
    PathIterator pPath = PathsBegin(fromDev, dt);
    PathIterator pathEnd = PathsEnd(fromDev, dt);
    UINT32 pathNum = 0;
    for (; pPath != pathEnd; ++pPath)
    {
        pPath->Print(pri,
                     fromDev,
                     bUsesFabricAddrs ? "    " : Utility::StrPrintf("         %2u : ", pathNum),
                     bUsesFabricAddrs ? "    " : "              ");
        pathNum++;
    }

}

//------------------------------------------------------------------------------
// LwLinkErrorFlags Implementation
//------------------------------------------------------------------------------

namespace LwLinkErrorFlags
{
    map<UINT32, LwLinkErrorFlags::LwLinkFlagBit> s_FlagIdxToLwlFlag =
    {
        { LW2080_CTRL_LWLINK_TL_ERRLOG_IDX_RXDLDATAPARITYERR  , LWL_ERR_FLAG_RX_DL_DATA_PARITY  },
        { LW2080_CTRL_LWLINK_TL_ERRLOG_IDX_RXDLCTRLPARITYERR  , LWL_ERR_FLAG_RX_DL_CTRL_PARITY  },
        { LW2080_CTRL_LWLINK_TL_ERRLOG_IDX_RXPROTOCOLERR      , LWL_ERR_FLAG_RX_PROTOCOL        },
        { LW2080_CTRL_LWLINK_TL_ERRLOG_IDX_RXOVERFLOWERR      , LWL_ERR_FLAG_RX_OVERFLOW        },
        { LW2080_CTRL_LWLINK_TL_ERRLOG_IDX_RXRAMDATAPARITYERR , LWL_ERR_FLAG_RX_RAM_DATA_PARITY },
        { LW2080_CTRL_LWLINK_TL_ERRLOG_IDX_RXRAMHDRPARITYERR  , LWL_ERR_FLAG_RX_RAM_HDR_PARITY  },
        { LW2080_CTRL_LWLINK_TL_ERRLOG_IDX_RXRESPERR          , LWL_ERR_FLAG_RX_RESP            },
        { LW2080_CTRL_LWLINK_TL_ERRLOG_IDX_RXPOISONERR        , LWL_ERR_FLAG_RX_POISON          },
        { LW2080_CTRL_LWLINK_TL_ERRLOG_IDX_TXRAMDATAPARITYERR , LWL_ERR_FLAG_TX_RAM_DATA_PARITY },
        { LW2080_CTRL_LWLINK_TL_ERRLOG_IDX_TXRAMHDRPARITYERR  , LWL_ERR_FLAG_TX_RAM_HDR_PARITY  },
        { LW2080_CTRL_LWLINK_TL_ERRLOG_IDX_DLFLOWPARITYERR    , LWL_ERR_FLAG_DL_FLOW_PARITY     },
        { LW2080_CTRL_LWLINK_TL_ERRLOG_IDX_DLHDRPARITYERR     , LWL_ERR_FLAG_DL_HDR_PARITY      },
        { LW2080_CTRL_LWLINK_TL_ERRLOG_IDX_TXCREDITERR        , LWL_ERR_FLAG_TX_CREDIT          }
    };

    map<LwLinkErrorFlags::LwLinkFlagBit, string> s_FlagBitToString =
    {
        { LWL_ERR_FLAG_RX_DL_DATA_PARITY  , "Receive DL Data Parity Error"     },
        { LWL_ERR_FLAG_RX_DL_CTRL_PARITY  , "Receive DL Control Parity Error"  },
        { LWL_ERR_FLAG_RX_PROTOCOL        , "Receive Protocol Error"           },
        { LWL_ERR_FLAG_RX_OVERFLOW        , "Receive Overflow Error"           },
        { LWL_ERR_FLAG_RX_RAM_DATA_PARITY , "Receive RAM Data Parity Error"    },
        { LWL_ERR_FLAG_RX_RAM_HDR_PARITY  , "Receive RAM Header Parity Error"  },
        { LWL_ERR_FLAG_RX_RESP            , "Receive Response Error"           },
        { LWL_ERR_FLAG_RX_POISON          , "Receive Poison Error"             },
        { LWL_ERR_FLAG_TX_RAM_DATA_PARITY , "Transmit RAM Data Parity Error"   },
        { LWL_ERR_FLAG_TX_RAM_HDR_PARITY  , "Transmit RAM Header Parity Error" },
        { LWL_ERR_FLAG_DL_FLOW_PARITY     , "DL Flow Parity Error"             },
        { LWL_ERR_FLAG_DL_HDR_PARITY      , "DL Header Parity Error"           },
        { LWL_ERR_FLAG_TX_CREDIT          , "Transmit Credit Error"            },
        { LWL_ERR_FLAG_EXCESS_DL          , "Excess DL Errors"                 }
    };
}

//------------------------------------------------------------------------------
//! \brief Get the error flag bit associated with a flag index
//!
//! \param flagIdx : Flag index to get the error flag bit for
//!
//! \return Error flag bit associated with the counter bit, LWL_NUM_ERR_FLAGS if
//!         there is no matching error flag bit
LwLinkErrorFlags::LwLinkFlagBit LwLinkErrorFlags::GetErrorFlagBit(UINT32 flagIdx)
{
    if (s_FlagIdxToLwlFlag.find(flagIdx) == s_FlagIdxToLwlFlag.end())
    {
        Printf(Tee::PriError, "%s : Unknown flag index 0x%08x\n", __FUNCTION__, flagIdx);
        return LWL_NUM_ERR_FLAGS;
    }
    return s_FlagIdxToLwlFlag[flagIdx];
}

//------------------------------------------------------------------------------
//! \brief Get a string representation for the specified flag bit
//!
//! \param flagBit    : Flag bit to get the string for
//!
//! \return String describing the flag bit
string LwLinkErrorFlags::GetFlagString(const LwLinkFlagBit flagBit)
{
    if (s_FlagBitToString.find(flagBit) == s_FlagBitToString.end())
        return "Unknown Error Flag";

    return s_FlagBitToString[flagBit];
}

//------------------------------------------------------------------------------
// JS Interface
//-----------------------------------------------------------------------------

//------------------------------------------------------------------------------
// LwLinkDev constants
//-----------------------------------------------------------------------------
JS_CLASS(LwLinkDevConst);
static SObject LwLinkDevConst_Object
(
    "LwLinkDevConst",
    LwLinkDevConstClass,
    0,
    0,
    "LwLinkDevConst JS Object"
);

#define DEFINE_LWL_DEV( DevIdStart, DevIdEnd, ChipId, LwId, Constant, \
                        HwrefDir, DispHwrefDir, ...)                  \
                        PROP_CONST( LwLinkDevConst, LwId, Device::LwId );
#define DEFINE_OBS_LWL_DEV(...) // don't care
#include "core/include/lwlinklist.h"

PROP_CONST(LwLinkDevConst, DOMAIN_AFS,         static_cast<UINT32>(RegHalDomain::AFS));
PROP_CONST(LwLinkDevConst, DOMAIN_AFS_PERFMON, static_cast<UINT32>(RegHalDomain::AFS_PERFMON));
PROP_CONST(LwLinkDevConst, DOMAIN_CLKS,        static_cast<UINT32>(RegHalDomain::CLKS));
PROP_CONST(LwLinkDevConst, DOMAIN_DLPL,        static_cast<UINT32>(RegHalDomain::DLPL));
PROP_CONST(LwLinkDevConst, DOMAIN_FUSE,        static_cast<UINT32>(RegHalDomain::FUSE));
PROP_CONST(LwLinkDevConst, DOMAIN_GLUE,        static_cast<UINT32>(RegHalDomain::GLUE));
PROP_CONST(LwLinkDevConst, DOMAIN_IOCTRL,      static_cast<UINT32>(RegHalDomain::IOCTRL));
PROP_CONST(LwLinkDevConst, DOMAIN_JTAG,        static_cast<UINT32>(RegHalDomain::JTAG));
PROP_CONST(LwLinkDevConst, DOMAIN_MINION,      static_cast<UINT32>(RegHalDomain::MINION));
PROP_CONST(LwLinkDevConst, DOMAIN_NPG,         static_cast<UINT32>(RegHalDomain::NPG));
PROP_CONST(LwLinkDevConst, DOMAIN_NPORT,       static_cast<UINT32>(RegHalDomain::NPORT));
PROP_CONST(LwLinkDevConst, DOMAIN_NTL,         static_cast<UINT32>(RegHalDomain::NTL));
PROP_CONST(LwLinkDevConst, DOMAIN_LWLDL,       static_cast<UINT32>(RegHalDomain::LWLDL));
PROP_CONST(LwLinkDevConst, DOMAIN_LWLIPT,      static_cast<UINT32>(RegHalDomain::LWLIPT));
PROP_CONST(LwLinkDevConst, DOMAIN_LWLIPT_LNK,  static_cast<UINT32>(RegHalDomain::LWLIPT_LNK));
PROP_CONST(LwLinkDevConst, DOMAIN_LWLPLL,      static_cast<UINT32>(RegHalDomain::LWLPLL));
PROP_CONST(LwLinkDevConst, DOMAIN_LWLTLC,      static_cast<UINT32>(RegHalDomain::LWLTLC));
PROP_CONST(LwLinkDevConst, DOMAIN_PHY,         static_cast<UINT32>(RegHalDomain::PHY));
PROP_CONST(LwLinkDevConst, DOMAIN_PMGR,        static_cast<UINT32>(RegHalDomain::PMGR));
PROP_CONST(LwLinkDevConst, DOMAIN_RAW,         static_cast<UINT32>(RegHalDomain::RAW));
PROP_CONST(LwLinkDevConst, DOMAIN_SAW,         static_cast<UINT32>(RegHalDomain::SAW));
PROP_CONST(LwLinkDevConst, DOMAIN_SIOCTRL,     static_cast<UINT32>(RegHalDomain::SIOCTRL));
PROP_CONST(LwLinkDevConst, DOMAIN_SWX,         static_cast<UINT32>(RegHalDomain::SWX));
PROP_CONST(LwLinkDevConst, DOMAIN_SWX_PERFMON, static_cast<UINT32>(RegHalDomain::SWX_PERFMON));
PROP_CONST(LwLinkDevConst, DOMAIN_XP3G,        static_cast<UINT32>(RegHalDomain::XP3G));
PROP_CONST(LwLinkDevConst, DOMAIN_XVE,         static_cast<UINT32>(RegHalDomain::XVE));
PROP_CONST(LwLinkDevConst, DOMAIN_RUNLIST,     static_cast<UINT32>(RegHalDomain::RUNLIST));
PROP_CONST(LwLinkDevConst, DOMAIN_CHRAM,       static_cast<UINT32>(RegHalDomain::CHRAM));
PROP_CONST(LwLinkDevConst, DOMAIN_PCIE,        static_cast<UINT32>(RegHalDomain::PCIE));
PROP_CONST(LwLinkDevConst, DOMAIN_XTL,         static_cast<UINT32>(RegHalDomain::XTL));
PROP_CONST(LwLinkDevConst, DOMAIN_XPL,         static_cast<UINT32>(RegHalDomain::XPL));
PROP_CONST(LwLinkDevConst, LWL_RX_CRC_FLIT_ID,   LwLink::ErrorCounts::LWL_RX_CRC_FLIT_ID);
PROP_CONST(LwLinkDevConst, LWL_RX_CRC_LANE0_ID,  LwLink::ErrorCounts::LWL_RX_CRC_LANE0_ID);
PROP_CONST(LwLinkDevConst, LWL_RX_CRC_LANE1_ID,  LwLink::ErrorCounts::LWL_RX_CRC_LANE1_ID);
PROP_CONST(LwLinkDevConst, LWL_RX_CRC_LANE2_ID,  LwLink::ErrorCounts::LWL_RX_CRC_LANE2_ID);
PROP_CONST(LwLinkDevConst, LWL_RX_CRC_LANE3_ID,  LwLink::ErrorCounts::LWL_RX_CRC_LANE3_ID);
PROP_CONST(LwLinkDevConst, LWL_RX_CRC_LANE4_ID,  LwLink::ErrorCounts::LWL_RX_CRC_LANE4_ID);
PROP_CONST(LwLinkDevConst, LWL_RX_CRC_LANE5_ID,  LwLink::ErrorCounts::LWL_RX_CRC_LANE5_ID);
PROP_CONST(LwLinkDevConst, LWL_RX_CRC_LANE6_ID,  LwLink::ErrorCounts::LWL_RX_CRC_LANE6_ID);
PROP_CONST(LwLinkDevConst, LWL_RX_CRC_LANE7_ID,  LwLink::ErrorCounts::LWL_RX_CRC_LANE7_ID);
PROP_CONST(LwLinkDevConst, LWL_TX_REPLAY_ID,     LwLink::ErrorCounts::LWL_TX_REPLAY_ID);
PROP_CONST(LwLinkDevConst, LWL_RX_REPLAY_ID,     LwLink::ErrorCounts::LWL_RX_REPLAY_ID);
PROP_CONST(LwLinkDevConst, LWL_TX_RECOVERY_ID,   LwLink::ErrorCounts::LWL_TX_RECOVERY_ID);
PROP_CONST(LwLinkDevConst, LWL_SW_RECOVERY_ID,   LwLink::ErrorCounts::LWL_SW_RECOVERY_ID);
PROP_CONST(LwLinkDevConst, LWL_UPHY_REFRESH_FAIL_ID,     LwLink::ErrorCounts::LWL_UPHY_REFRESH_FAIL_ID);
PROP_CONST(LwLinkDevConst, LWL_UPHY_REFRESH_PASS_ID,     LwLink::ErrorCounts::LWL_UPHY_REFRESH_PASS_ID);
PROP_CONST(LwLinkDevConst, LWL_PRE_FEC_ID,               LwLink::ErrorCounts::LWL_PRE_FEC_ID);
PROP_CONST(LwLinkDevConst, LWL_FEC_CORRECTIONS_LANE0_ID, LwLink::ErrorCounts::LWL_FEC_CORRECTIONS_LANE0_ID);
PROP_CONST(LwLinkDevConst, LWL_FEC_CORRECTIONS_LANE1_ID, LwLink::ErrorCounts::LWL_FEC_CORRECTIONS_LANE1_ID);
PROP_CONST(LwLinkDevConst, LWL_FEC_CORRECTIONS_LANE2_ID, LwLink::ErrorCounts::LWL_FEC_CORRECTIONS_LANE2_ID);
PROP_CONST(LwLinkDevConst, LWL_FEC_CORRECTIONS_LANE3_ID, LwLink::ErrorCounts::LWL_FEC_CORRECTIONS_LANE3_ID);
PROP_CONST(LwLinkDevConst, LWL_NUM_ERR_COUNTERS, LwLink::ErrorCounts::LWL_NUM_ERR_COUNTERS);

PROP_CONST(LwLinkDevConst, LWL_ERR_FLAG_RX_DL_DATA_PARITY,  LwLinkErrorFlags::LWL_ERR_FLAG_RX_DL_DATA_PARITY);  //$
PROP_CONST(LwLinkDevConst, LWL_ERR_FLAG_RX_DL_CTRL_PARITY,  LwLinkErrorFlags::LWL_ERR_FLAG_RX_DL_CTRL_PARITY);  //$
PROP_CONST(LwLinkDevConst, LWL_ERR_FLAG_RX_PROTOCOL,        LwLinkErrorFlags::LWL_ERR_FLAG_RX_PROTOCOL);        //$
PROP_CONST(LwLinkDevConst, LWL_ERR_FLAG_RX_OVERFLOW,        LwLinkErrorFlags::LWL_ERR_FLAG_RX_OVERFLOW);        //$
PROP_CONST(LwLinkDevConst, LWL_ERR_FLAG_RX_RAM_DATA_PARITY, LwLinkErrorFlags::LWL_ERR_FLAG_RX_RAM_DATA_PARITY); //$
PROP_CONST(LwLinkDevConst, LWL_ERR_FLAG_RX_RAM_HDR_PARITY,  LwLinkErrorFlags::LWL_ERR_FLAG_RX_RAM_HDR_PARITY);  //$
PROP_CONST(LwLinkDevConst, LWL_ERR_FLAG_RX_RESP,            LwLinkErrorFlags::LWL_ERR_FLAG_RX_RESP);            //$
PROP_CONST(LwLinkDevConst, LWL_ERR_FLAG_RX_POISON,          LwLinkErrorFlags::LWL_ERR_FLAG_RX_POISON);          //$
PROP_CONST(LwLinkDevConst, LWL_ERR_FLAG_TX_RAM_DATA_PARITY, LwLinkErrorFlags::LWL_ERR_FLAG_TX_RAM_DATA_PARITY); //$
PROP_CONST(LwLinkDevConst, LWL_ERR_FLAG_TX_RAM_HDR_PARITY,  LwLinkErrorFlags::LWL_ERR_FLAG_TX_RAM_HDR_PARITY);  //$
PROP_CONST(LwLinkDevConst, LWL_ERR_FLAG_DL_FLOW_PARITY,     LwLinkErrorFlags::LWL_ERR_FLAG_DL_FLOW_PARITY);     //$
PROP_CONST(LwLinkDevConst, LWL_ERR_FLAG_DL_HDR_PARITY,      LwLinkErrorFlags::LWL_ERR_FLAG_DL_HDR_PARITY);      //$
PROP_CONST(LwLinkDevConst, LWL_ERR_FLAG_TX_CREDIT,          LwLinkErrorFlags::LWL_ERR_FLAG_TX_CREDIT);          //$
PROP_CONST(LwLinkDevConst, LWL_ERR_FLAG_EXCESS_DL,          LwLinkErrorFlags::LWL_ERR_FLAG_EXCESS_DL);          //$
PROP_CONST(LwLinkDevConst, LWL_NUM_ERR_FLAGS,               LwLinkErrorFlags::LWL_NUM_ERR_FLAGS);               //$

// Properties used for LwLink reporting
PROP_CONST(LwLinkDevConst, SLS_OFF,         LwLink::SLS_OFF);
PROP_CONST(LwLinkDevConst, SLS_SAFE_MODE,   LwLink::SLS_SAFE_MODE);
PROP_CONST(LwLinkDevConst, SLS_TRAINING,    LwLink::SLS_TRAINING);
PROP_CONST(LwLinkDevConst, SLS_SINGLE_LANE, LwLink::SLS_SINGLE_LANE);
PROP_CONST(LwLinkDevConst, SLS_HIGH_SPEED,  LwLink::SLS_HIGH_SPEED);
PROP_CONST(LwLinkDevConst, SLS_ILWALID,     LwLink::SLS_ILWALID);

PROP_CONST(LwLinkDevConst, TM_DEFAULT, LwLinkDevIf::TM_DEFAULT);
PROP_CONST(LwLinkDevConst, TM_MIN,     LwLinkDevIf::TM_MIN);
PROP_CONST(LwLinkDevConst, TM_FILE,    LwLinkDevIf::TM_FILE);

PROP_CONST(LwLinkDevConst, HWD_NONE,            LwLinkDevIf::HWD_NONE);
PROP_CONST(LwLinkDevConst, HWD_EXPLORER_NORMAL, LwLinkDevIf::HWD_EXPLORER_NORMAL);
PROP_CONST(LwLinkDevConst, HWD_EXPLORER_ILWERT, LwLinkDevIf::HWD_EXPLORER_ILWERT);

PROP_CONST(LwLinkDevConst, LC_NONE,   LwLinkDevIf::LC_NONE);
PROP_CONST(LwLinkDevConst, LC_MANUAL, LwLinkDevIf::LC_MANUAL);
PROP_CONST(LwLinkDevConst, LC_AUTO,   LwLinkDevIf::LC_AUTO);

PROP_CONST(LwLinkDevConst, TF_TREX, LwLinkDevIf::TF_TREX);
PROP_CONST(LwLinkDevConst, TF_DISABLE_AUTO_MAPPING, LwLinkDevIf::TF_DISABLE_AUTO_MAPPING);

PROP_CONST(LwLinkDevConst, TYPE_EBRIDGE,         TestDevice::TYPE_EBRIDGE);
PROP_CONST(LwLinkDevConst, TYPE_IBM_NPU,         TestDevice::TYPE_IBM_NPU);
PROP_CONST(LwLinkDevConst, TYPE_LWIDIA_GPU,      TestDevice::TYPE_LWIDIA_GPU);
PROP_CONST(LwLinkDevConst, TYPE_LWIDIA_LWSWITCH, TestDevice::TYPE_LWIDIA_LWSWITCH);
PROP_CONST(LwLinkDevConst, TYPE_TEGRA_MFG,       TestDevice::TYPE_TEGRA_MFG);
PROP_CONST(LwLinkDevConst, TYPE_UNKNOWN,         TestDevice::TYPE_UNKNOWN);

PROP_CONST(LwLinkDevConst, ILWALID_TOPOLOGY_ID, ~0U);

PROP_CONST(LwLinkDevConst, ILWALID_LANE,     LwLink::ErrorCounts::LWLINK_ILWALID_LANE);

PROP_CONST(LwLinkDevConst, SP_RESTORE_PHY_PARAMS,              static_cast<UINT08>(LwLink::SystemParameter::RESTORE_PHY_PARAMS));
PROP_CONST(LwLinkDevConst, SP_SL_ENABLE,                       static_cast<UINT08>(LwLink::SystemParameter::SL_ENABLE));
PROP_CONST(LwLinkDevConst, SP_L2_ENABLE,                       static_cast<UINT08>(LwLink::SystemParameter::L2_ENABLE));
PROP_CONST(LwLinkDevConst, SP_LINE_RATE,                       static_cast<UINT08>(LwLink::SystemParameter::LINE_RATE));
PROP_CONST(LwLinkDevConst, SP_LINE_CODE_MODE,                  static_cast<UINT08>(LwLink::SystemParameter::LINE_CODE_MODE));
PROP_CONST(LwLinkDevConst, SP_LINK_DISABLE,                    static_cast<UINT08>(LwLink::SystemParameter::LINK_DISABLE));
PROP_CONST(LwLinkDevConst, SP_TXTRAIN_OPT_ALGO,                static_cast<UINT08>(LwLink::SystemParameter::TXTRAIN_OPT_ALGO));
PROP_CONST(LwLinkDevConst, SP_BLOCK_CODE_MODE,                 static_cast<UINT08>(LwLink::SystemParameter::BLOCK_CODE_MODE));
PROP_CONST(LwLinkDevConst, SP_REF_CLOCK_MODE,                  static_cast<UINT08>(LwLink::SystemParameter::REF_CLOCK_MODE));
PROP_CONST(LwLinkDevConst, SP_LINK_INIT_DISABLE,               static_cast<UINT08>(LwLink::SystemParameter::LINK_INIT_DISABLE));
PROP_CONST(LwLinkDevConst, SP_ALI_ENABLE,                      static_cast<UINT08>(LwLink::SystemParameter::ALI_ENABLE));
PROP_CONST(LwLinkDevConst, SP_TXTRAIN_MIN_TRAIN_TIME_MANTISSA, static_cast<UINT08>(LwLink::SystemParameter::TXTRAIN_MIN_TRAIN_TIME_MANTISSA));
PROP_CONST(LwLinkDevConst, SP_TXTRAIN_MIN_TRAIN_TIME_EXPONENT, static_cast<UINT08>(LwLink::SystemParameter::TXTRAIN_MIN_TRAIN_TIME_EXPONENT));
PROP_CONST(LwLinkDevConst, SP_L1_MIN_RECAL_TIME_MANTISSA,      static_cast<UINT08>(LwLink::SystemParameter::L1_MIN_RECAL_TIME_MANTISSA));
PROP_CONST(LwLinkDevConst, SP_L1_MIN_RECAL_TIME_EXPONENT,      static_cast<UINT08>(LwLink::SystemParameter::L1_MIN_RECAL_TIME_EXPONENT));
PROP_CONST(LwLinkDevConst, SP_L1_MAX_RECAL_PERIOD_MANTISSA,    static_cast<UINT08>(LwLink::SystemParameter::L1_MAX_RECAL_PERIOD_MANTISSA));
PROP_CONST(LwLinkDevConst, SP_L1_MAX_RECAL_PERIOD_EXPONENT,    static_cast<UINT08>(LwLink::SystemParameter::L1_MAX_RECAL_PERIOD_EXPONENT));
PROP_CONST(LwLinkDevConst, SP_DISABLE_UPHY_UCODE_LOAD,         static_cast<UINT08>(LwLink::SystemParameter::DISABLE_UPHY_UCODE_LOAD));
PROP_CONST(LwLinkDevConst, SP_CHANNEL_TYPE,                    static_cast<UINT08>(LwLink::SystemParameter::CHANNEL_TYPE));
PROP_CONST(LwLinkDevConst, SP_L1_ENABLE,                       static_cast<UINT08>(LwLink::SystemParameter::L1_ENABLE));

PROP_CONST(LwLinkDevConst, LR_16_0_GBPS,  static_cast<UINT08>(LwLink::SystemLineRate::GBPS_16_0));
PROP_CONST(LwLinkDevConst, LR_20_0_GBPS,  static_cast<UINT08>(LwLink::SystemLineRate::GBPS_20_0));
PROP_CONST(LwLinkDevConst, LR_25_0_GBPS,  static_cast<UINT08>(LwLink::SystemLineRate::GBPS_25_0));
PROP_CONST(LwLinkDevConst, LR_25_7_GBPS,  static_cast<UINT08>(LwLink::SystemLineRate::GBPS_25_7));
PROP_CONST(LwLinkDevConst, LR_32_0_GBPS,  static_cast<UINT08>(LwLink::SystemLineRate::GBPS_32_0));
PROP_CONST(LwLinkDevConst, LR_40_0_GBPS,  static_cast<UINT08>(LwLink::SystemLineRate::GBPS_40_0));
PROP_CONST(LwLinkDevConst, LR_50_0_GBPS,  static_cast<UINT08>(LwLink::SystemLineRate::GBPS_50_0));
PROP_CONST(LwLinkDevConst, LR_53_1_GBPS,  static_cast<UINT08>(LwLink::SystemLineRate::GBPS_53_1));
PROP_CONST(LwLinkDevConst, LR_28_1_GBPS,  static_cast<UINT08>(LwLink::SystemLineRate::GBPS_28_1));
PROP_CONST(LwLinkDevConst, LR_100_0_GBPS, static_cast<UINT08>(LwLink::SystemLineRate::GBPS_100_0));
PROP_CONST(LwLinkDevConst, LR_106_2_GBPS, static_cast<UINT08>(LwLink::SystemLineRate::GBPS_106_2));

PROP_CONST(LwLinkDevConst, LC_NRZ,         static_cast<UINT08>(LwLink::SystemLineCode::NRZ));
PROP_CONST(LwLinkDevConst, LC_NRZ_128B130, static_cast<UINT08>(LwLink::SystemLineCode::NRZ_128B130));
PROP_CONST(LwLinkDevConst, LC_PAM4,        static_cast<UINT08>(LwLink::SystemLineCode::PAM4));

PROP_CONST(LwLinkDevConst, TTA_A0_SINGLE_PRESET,           static_cast<UINT08>(LwLink::SystemTxTrainAlg::A0_SINGLE_PRESET));
PROP_CONST(LwLinkDevConst, TTA_A1_PRESET_ARRAY,            static_cast<UINT08>(LwLink::SystemTxTrainAlg::A1_PRESET_ARRAY));
PROP_CONST(LwLinkDevConst, TTA_A2_FINE_GRAINED_EXHAUSTIVE, static_cast<UINT08>(LwLink::SystemTxTrainAlg::A2_FINE_GRAINED_EXHAUSTIVE));
PROP_CONST(LwLinkDevConst, TTA_A4_FOM_CENTROID,            static_cast<UINT08>(LwLink::SystemTxTrainAlg::A4_FOM_CENTROID));

PROP_CONST(LwLinkDevConst, BC_OFF,   static_cast<UINT08>(LwLink::SystemBlockCodeMode::OFF));
PROP_CONST(LwLinkDevConst, BC_ECC88, static_cast<UINT08>(LwLink::SystemBlockCodeMode::ECC88));
PROP_CONST(LwLinkDevConst, BC_ECC96, static_cast<UINT08>(LwLink::SystemBlockCodeMode::ECC96));

PROP_CONST(LwLinkDevConst, RC_COMMON,           static_cast<UINT08>(LwLink::SystemRefClockMode::COMMON));
PROP_CONST(LwLinkDevConst, RC_RESERVED,         static_cast<UINT08>(LwLink::SystemRefClockMode::RESERVED));
PROP_CONST(LwLinkDevConst, RC_NON_COMMON_NO_SS, static_cast<UINT08>(LwLink::SystemRefClockMode::NON_COMMON_NO_SS));
PROP_CONST(LwLinkDevConst, RC_NON_COMMON_SS,    static_cast<UINT08>(LwLink::SystemRefClockMode::NON_COMMON_SS));

PROP_CONST(LwLinkDevConst, CT_GENERIC,         static_cast<UINT08>(LwLink::SystemChannelType::GENERIC));
PROP_CONST(LwLinkDevConst, CT_ACTIVE_0,        static_cast<UINT08>(LwLink::SystemChannelType::ACTIVE_0));
PROP_CONST(LwLinkDevConst, CT_ACTIVE_1,        static_cast<UINT08>(LwLink::SystemChannelType::ACTIVE_1));
PROP_CONST(LwLinkDevConst, CT_ACTIVE_CABLE_0,  static_cast<UINT08>(LwLink::SystemChannelType::ACTIVE_CABLE_0));
PROP_CONST(LwLinkDevConst, CT_ACTIVE_CABLE_1,  static_cast<UINT08>(LwLink::SystemChannelType::ACTIVE_CABLE_1));
PROP_CONST(LwLinkDevConst, CT_OPTICAL_CABLE_0, static_cast<UINT08>(LwLink::SystemChannelType::OPTICAL_CABLE_0));
PROP_CONST(LwLinkDevConst, CT_OPTICAL_CABLE_1, static_cast<UINT08>(LwLink::SystemChannelType::OPTICAL_CABLE_1));
PROP_CONST(LwLinkDevConst, CT_DIRECT_0,        static_cast<UINT08>(LwLink::SystemChannelType::DIRECT_0));

PROP_CONST(LwLinkDevConst, DR_TEST,           LwLink::DR_TEST);
PROP_CONST(LwLinkDevConst, DR_TEST_ERROR,     LwLink::DR_TEST_ERROR);
PROP_CONST(LwLinkDevConst, DR_ERROR,          LwLink::DR_ERROR);
PROP_CONST(LwLinkDevConst, DR_POST_DISCOVERY, LwLink::DR_POST_DISCOVERY);
PROP_CONST(LwLinkDevConst, DR_PRE_TRAINING,   LwLink::DR_PRE_TRAINING);
PROP_CONST(LwLinkDevConst, DR_POST_TRAINING,  LwLink::DR_POST_TRAINING);
PROP_CONST(LwLinkDevConst, DR_POST_SHUTDOWN,  LwLink::DR_POST_SHUTDOWN);

PROP_CONST(LwLinkDevConst, TREX_DATA_ALL_ZEROS, LwLinkTrex::DP_ALL_ZEROS);
PROP_CONST(LwLinkDevConst, TREX_DATA_ALL_ONES,  LwLinkTrex::DP_ALL_ONES);
PROP_CONST(LwLinkDevConst, TREX_DATA_0_F,       LwLinkTrex::DP_0_F);
PROP_CONST(LwLinkDevConst, TREX_DATA_A_5,       LwLinkTrex::DP_A_5);

PROP_CONST(LwLinkDevConst, TREX_SRC_EGRESS,  LwLinkTrex::SRC_EGRESS);
PROP_CONST(LwLinkDevConst, TREX_SRC_RX,      LwLinkTrex::SRC_RX);
PROP_CONST(LwLinkDevConst, TREX_DST_INGRESS, LwLinkTrex::DST_INGRESS);
PROP_CONST(LwLinkDevConst, TREX_DST_TX,      LwLinkTrex::DST_TX);
        
PROP_CONST(LwLinkDevConst, CCI_ILWALID_LANE, LwLinkCableController::CCI_ILWALID);

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(LwLinkDevConst,
                  ErrorIndexToString,
                  1,
                  "Get the string associated with an error index")
{
    STEST_HEADER(1, 1, "Usage: LwLinkDevConst.ErrorIndexToString(ErrIndex)\n");
    STEST_ARG(0, UINT32, nErrIdx);

    LwLink::ErrorCounts::ErrId errIdx = static_cast<LwLink::ErrorCounts::ErrId>(nErrIdx);
    if (pJavaScript->ToJsval(LwLink::ErrorCounts::GetName(errIdx), pReturlwalue) == OK)
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(LwLinkDevConst,
                  ErrorIndexToJsonTag,
                  1,
                  "Get the JSON tag associated with an error index")
{
    STEST_HEADER(1, 1, "Usage: LwLinkDevConst.ErrorIndexToJsonTag(ErrIndex)\n");
    STEST_ARG(0, UINT32, nErrIdx);

    LwLink::ErrorCounts::ErrId errIdx = static_cast<LwLink::ErrorCounts::ErrId>(nErrIdx);
    if (pJavaScript->ToJsval(LwLink::ErrorCounts::GetJsonTag(errIdx), pReturlwalue) == OK)
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(LwLinkDevConst,
                  ErrorIndexToLane,
                  1,
                  "Get the lane associated with an error index")
{
    STEST_HEADER(1, 1, "Usage: LwLinkDevConst.ErrorIndexToLane(ErrIndex)\n");
    STEST_ARG(0, UINT32, nErrIdx);

    LwLink::ErrorCounts::ErrId errIdx = static_cast<LwLink::ErrorCounts::ErrId>(nErrIdx);
    if (pJavaScript->ToJsval(LwLink::ErrorCounts::GetLane(errIdx), pReturlwalue) == OK)
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(LwLinkDevConst,
                  ErrorIndexIsInternalOnly,
                  1,
                  "Check if the error index is for internal use only")
{
    STEST_HEADER(1, 1, "Usage: LwLinkDevConst.ErrorIndexIsInternalOnly(ErrIndex)\n");
    STEST_ARG(0, UINT32, nErrIdx);

    LwLink::ErrorCounts::ErrId errIdx = static_cast<LwLink::ErrorCounts::ErrId>(nErrIdx);
    if (pJavaScript->ToJsval(LwLink::ErrorCounts::IsInternalOnly(errIdx), pReturlwalue) == OK)
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(LwLinkDevConst,
                  ErrorIndexIsCountOnly,
                  1,
                  "Check if the error index should always use count thresholds")
{
    STEST_HEADER(1, 1, "Usage: LwLinkDevConst.ErrorIndexIsCountOnly(ErrIndex)\n");
    STEST_ARG(0, UINT32, nErrIdx);

    LwLink::ErrorCounts::ErrId errIdx = static_cast<LwLink::ErrorCounts::ErrId>(nErrIdx);
    if (pJavaScript->ToJsval(LwLink::ErrorCounts::IsCountOnly(errIdx), pReturlwalue) == RC::OK)
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(LwLinkDevConst,
                  ErrorIndexIsThreshold,
                  1,
                  "Check if the error index has a threshold")
{
    STEST_HEADER(1, 1, "Usage: LwLinkDevConst.ErrorIndexIsThreshold(ErrIndex)\n");
    STEST_ARG(0, UINT32, nErrIdx);

    LwLink::ErrorCounts::ErrId errIdx = static_cast<LwLink::ErrorCounts::ErrId>(nErrIdx);
    if (pJavaScript->ToJsval(LwLink::ErrorCounts::IsThreshold(errIdx), pReturlwalue) == OK)
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(LwLinkDevConst,
                  ErrorFlagBitToString,
                  1,
                  "Get the string associated with an error flag bit")
{
    STEST_HEADER(1, 1, "Usage: LwLinkDevConst.ErrorFlagBitToString(ErrFlagBit)\n");
    STEST_ARG(0, UINT32, nErrFlagBit);

    LwLinkErrorFlags::LwLinkFlagBit errFlagBit =
        static_cast<LwLinkErrorFlags::LwLinkFlagBit>(nErrFlagBit);
    if (pJavaScript->ToJsval(LwLinkErrorFlags::GetFlagString(errFlagBit), pReturlwalue) == OK)
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(LwLinkDevConst,
                  LinkStateToString,
                  1,
                  "Get the string associated with a link state")
{
    STEST_HEADER(1, 1, "Usage: LwLinkDevConst.LinkStateToString(state)\n");
    STEST_ARG(0, UINT32, nLinkState);

    LwLink::LinkState linkState = static_cast<LwLink::LinkState>(nLinkState);
    if (pJavaScript->ToJsval(LwLink::LinkStateToString(linkState), pReturlwalue) == RC::OK)
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(LwLinkDevConst,
                  SubLinkStateToString,
                  1,
                  "Get the string associated with a sublink state")
{
    STEST_HEADER(1, 1, "Usage: LwLinkDevConst.SubLinkStateToString(state)\n");
    STEST_ARG(0, UINT32, nSublinkState);

    LwLink::SubLinkState subLinkState = static_cast<LwLink::SubLinkState>(nSublinkState);
    if (pJavaScript->ToJsval(LwLink::SubLinkStateToString(subLinkState), pReturlwalue) == RC::OK)
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}


//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(LwLinkDevConst,
                  SysParamStr,
                  1,
                  "Get the string associated with lwlink system parameter")
{
    STEST_HEADER(1, 1, "Usage: LwLinkDevConst.SysParamStr(sysParam)\n");
    STEST_ARG(0, UINT08, nSysParam);

    LwLink::SystemParameter sysParam = static_cast<LwLink::SystemParameter>(nSysParam);
    if (pJavaScript->ToJsval(LwLink::SystemParameterString(sysParam), pReturlwalue) == RC::OK)
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}
