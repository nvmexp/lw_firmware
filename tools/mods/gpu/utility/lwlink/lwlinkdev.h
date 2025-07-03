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
#pragma once

#include "core/include/pci.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "device/interface/lwlink.h"
#include "gpu/include/testdevice.h"
#include "lwgputypes.h"

#include <boost/iterator/filter_iterator.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <map>
#include <memory>
#include <set>
#include <vector>

//------------------------------------------------------------------------------
//! \brief Abstraction for a LwLink connection
//!
//! Contains the LwLink devices at each end of the link as well as a list of
//! LwLink Ids for the endpoints at each side of the link
class LwLinkConnection
{
public:
    typedef pair<TestDevicePtr, TestDevicePtr> EndpointDevices;
    typedef pair<UINT32, UINT32> EndpointIds;

    LwLinkConnection
    (
         TestDevicePtr pDev1
        ,TestDevicePtr pDev2
        ,const vector<EndpointIds> &endpoints
    );
    ~LwLinkConnection() { }

    // Functions sorted alphabetically
    UINT32 GetMaxDataBwKiBps(TestDevicePtr pLocDev) const;
    pair<TestDevicePtr, TestDevicePtr> GetDevices();
    EndpointDevices GetEndpointDevices();
    vector<EndpointIds> GetLinks(TestDevicePtr pLocDev) const;
    vector<EndpointIds> GetLocalLinks(TestDevicePtr pLocDev) const;
    UINT64 GetLocalLinkMask(TestDevicePtr pDev);
    string GetLocalLinkString(TestDevicePtr pLocDev, TestDevicePtr pSortDev);
    UINT32 GetMaxBwKiBps(TestDevicePtr pLocDev) const;
    TestDevicePtr GetRemoteDevice(TestDevicePtr pLocDev) const;
    UINT64 GetRemoteLinkMask(TestDevicePtr pDev);
    vector<EndpointIds> GetRemoteLinks(TestDevicePtr pLocDev) const;
    string GetRemoteLinkString(TestDevicePtr pLocDev, TestDevicePtr pSortDev);
    string GetString(TestDevicePtr pLocDev, string prefix) const;
    void GetStrings(TestDevicePtr pLocDev, vector<string> *pConnStrings) const;
    size_t GetWidth() const;
    bool IsActive() const;
    bool IsEndpoint() const;
    bool IsLoop() const;
    bool IsSysmem() const;
    bool HasLoopout() const;
    bool UsesDevice(TestDevicePtr pDev) const;

    bool operator==(const LwLinkConnection &rhs) const;
    bool operator!=(const LwLinkConnection &rhs) const
    {
        return !(rhs == *this);
    }
    bool operator<(const LwLinkConnection &rhs) const;

private:
    UINT64 GetLinkMask(TestDevicePtr pLocDev, bool bForceSecondEndpoint);
    string GetLinkString(TestDevicePtr pLocDev, bool bForceSecondEndpoint, TestDevicePtr pSortDev);

    TestDevicePtr m_pDev1;  //!< Device at endpoint 1 of the link
    TestDevicePtr m_pDev2;  //!< Device at endpoint 2 of the link

    //! List of link ID pairs representing the LwLink ids at each side of the
    //! link.  For internal storage, the first element of the pair always refers
    //! to the link ID on device 1 and the second element of the pair always
    //! refers to device 2
    vector<EndpointIds> m_Endpoints;
};

//! Use boost shared pointers when passing around pointers to LwLinkConnection
//! objects to ensure that MODS can appropriately error when shutting down if
//! there are still external implementations holding references to
//! LwLinkConnections
typedef shared_ptr<LwLinkConnection> LwLinkConnectionPtr;

//------------------------------------------------------------------------------
//! \brief Abstraction for a LwLink path
//!
//! Contains all LwLink connections in a path
class LwLinkPath
{
public:
    LwLinkPath() : m_IsBidirectional(false) { }
    LwLinkPath(LwLink::DataType dt) :
        m_IsBidirectional(false), m_DataType(dt) { }
    LwLinkPath(UINT64 fb, LwLink::DataType dt) :
        m_IsBidirectional(false), m_DataType(dt) { m_FabricBases.insert(fb); }
    ~LwLinkPath() { }

    //! Structure describing an entry in the path
    struct PathEntry
    {
        LwLinkConnectionPtr pCon;    //!< Connection between 2 devices
        TestDevicePtr       pSrcDev; //!< Source device (some paths are not
                                     //!< bi-directional)
        bool                bRemote; //!< If this entry is loopout whether this
                                     //!< entry represents the local or remote
                                     //!< end of the loopout connection

        bool operator==(const PathEntry &rhs) const;
        bool operator!=(const PathEntry &rhs) const;
        bool operator<(const PathEntry &rhs) const;
        void Reverse();
    };

    //! Iterator for walking through a path in either a forward or reverse
    //! direction (reverse for bidirectional paths).  Walking in reverse starts
    //! from the end of the provided vector and will automatically reverse the
    //! data in the returned path entry so that the source device / link mask
    //! will always reflect the direction that the path is being walked
    class PathEntryIterator : public boost::iterator_facade<PathEntryIterator,
                                                            PathEntry,
                                                            boost::forward_traversal_tag,
                                                            const PathEntry &>
    {
        friend class boost::iterator_core_access;

        public:
            PathEntryIterator();
            PathEntryIterator(const vector<PathEntry> *pPathEntries, bool bReverse);
        private:
            void increment();
            bool equal(PathEntryIterator const& other) const;

            const PathEntry &dereference() const { return m_LwrValue; }

            bool      m_bReverse;  //!< true if the path is being walked in reverse order
            PathEntry m_LwrValue;  //!< Current path entry (returned on iterator dereference)
            INT32     m_LwrIndex;  //!< Current index
            const vector<PathEntry> *m_pPathEntries; //!< Constant pointer to path data
    };

    bool AddEntry(LwLinkConnectionPtr pCon, TestDevicePtr pSrcDev, UINT64 notFromMask);
    void AddFabricBases(const set<UINT64> &fabricBases);
    void AddDataType(LwLink::DataType dt);
    bool ConnectionsMatch(const LwLinkPath &path) const;
    UINT32 GetMaxDataBwKiBps() const;
    LwLink::DataType GetDataType() const { return m_DataType; }
    LwLinkConnection::EndpointDevices GetEndpointDevices() const;
    vector<LwLinkConnection::EndpointIds> GetEndpointLinks(TestDevicePtr pDev) const;
    UINT64 GetFabricBase() const;
    set<UINT64> GetFabricBases() const { return m_FabricBases; }
    size_t GetLength() const { return m_PathEntries.size(); }
    UINT64 GetLocalEndpointLinkMask(TestDevicePtr pDev) const;
    UINT32 GetMaxBwKiBps() const;
    UINT64 GetRemoteEndpointLinkMask(TestDevicePtr pDev) const;
    size_t GetWidth() const;
    bool IsBidirectional() const { return m_IsBidirectional; }
    PathEntryIterator PathEntryBegin(TestDevicePtr pFromEndpoint) const;
    void Print
    (
        Tee::Priority pri,
        TestDevicePtr  fromEndpoint,
        const string &firstLinePrefix,
        const string &prefix
    ) const;
    void SetBidirectional() { m_IsBidirectional = true; }
    bool UsesConnection(LwLinkConnectionPtr pConnection) const;
    bool UsesDevice(TestDevicePtr pDevice) const;
    bool UsesFabricBases(const set<UINT64> &fabricBases) const;
    RC Validate(bool *pbValid) const;

private:
    UINT64 GetEndpointLinkMask(TestDevicePtr pLocDev, bool bRemote) const;

    vector<PathEntry>   m_PathEntries;      //!< Entries in the path
    bool                m_IsBidirectional;  //!< True if the path is valid in both directions
    LwLink::DataType m_DataType;         //!< Data type for the path (request, response, all)

    //! (LwSwitch only) Fabric bases where this path is valid.  Each entry
    //! specifies the base of a region where this path is used
    set<UINT64> m_FabricBases;
};

//------------------------------------------------------------------------------
//! \brief Abstraction for a LwLink route
//!
//! Contains all LwLink paths in a route
class LwLinkRoute
{
public:
    struct IsPathUsed
    {
        TestDevicePtr    fromDev;
        LwLink::DataType dt;
        bool operator()(LwLinkPath p)
        {
            return ((p.GetEndpointDevices().first == fromDev) || p.IsBidirectional()) &&
                   (p.GetDataType() & dt);
        }
    };
    typedef boost::filter_iterator<IsPathUsed, vector<LwLinkPath>::const_iterator> PathIterator;

    LwLinkRoute(LwLinkConnection::EndpointDevices endpoints);
    ~LwLinkRoute() { }

    // Functions sorted alphabetically
    RC AddPath(LwLinkPath path);
    bool AllDataFlowIdentical(TestDevicePtr pFromEndpoint) const;
    bool AllPathsBidirectional() const;
    void ConsolidatePathsByDataType();
    UINT32 GetMaxDataBwKiBps(TestDevicePtr pLocDev, LwLink::DataType dt) const;
    LwLinkConnection::EndpointDevices GetEndpointDevices() const { return m_EndpointDevs; }
    vector<LwLinkConnection::EndpointIds> GetEndpointLinks
    (
        TestDevicePtr pDev
       ,LwLink::DataType dt
    ) const;
    UINT32 GetMaxBwKiBps(TestDevicePtr pDev, LwLink::DataType dt) const;
    size_t GetMaxLength(TestDevicePtr pDev, LwLink::DataType dt) const;
    TestDevicePtr GetRemoteEndpoint(TestDevicePtr pLocEndpoint) const;
    string GetString(TestDevicePtr pLocDev, LwLink::DataType dt, string prefix) const;
    size_t GetWidth(TestDevicePtr pDev, LwLink::DataType dt) const;
    size_t GetWidthAtEndpoint(TestDevicePtr pDev) const;
    bool IsLoop() const;
    bool IsP2P() const;
    bool IsSysmem() const;
    PathIterator PathsBegin(TestDevicePtr pFromEndpoint, LwLink::DataType dt) const
    {
        IsPathUsed ipu = { pFromEndpoint, dt };
        return PathIterator(ipu, m_Paths.begin(), m_Paths.end());
    }

    PathIterator PathsEnd(TestDevicePtr pFromEndpoint, LwLink::DataType dt) const
    {
        IsPathUsed ipu = { pFromEndpoint, dt };
        return PathIterator(ipu, m_Paths.end(), m_Paths.end());
    }
    void Print(Tee::Priority pri) const;
    bool UsesConnection(LwLinkConnectionPtr pConnection) const;
    bool UsesConnection
    (
        TestDevicePtr       pFromEndpoint,
        LwLinkConnectionPtr pConnection,
        LwLink::DataType    dt
    ) const;
    bool UsesDevice(TestDevicePtr pDevice) const;
    bool UsesEndpoints(LwLinkConnection::EndpointDevices endpoints) const;

private:
    RC AddDirectPath(LwLinkPath path);
    RC AddFabricPath(LwLinkPath path);

    bool AnyConnectionsOversubscribed(TestDevicePtr pDev, LwLink::DataType dt) const;
    string GetEndpointLinkString
    (
        TestDevicePtr    pEndpoint
       ,TestDevicePtr    pSortDev
       ,bool             bRemote
       ,LwLink::DataType dt
    ) const;
    void PrintPaths
    (
        Tee::Priority    pri
       ,TestDevicePtr    fromDev
       ,LwLink::DataType dt
       ,bool             bUsesFabricAddrs
    ) const;

    LwLinkConnection::EndpointDevices   m_EndpointDevs;
    vector<LwLinkPath> m_Paths;
};

//! Use boost shared pointers when passing around pointers to LwLinkRoute
//! objects to ensure that MODS can appropriately error when shutting down if
//! there are still external implementations holding references to
//! LwLinkRoutes
typedef shared_ptr<LwLinkRoute> LwLinkRoutePtr;

//------------------------------------------------------------------------------
//! \brief Abstraction containing error information for all links on a
//! particular LwLink device (both local and remote data for all active links)
//!
namespace LwLinkErrorFlags
{
    //! Enum describing the error flag bits
    enum LwLinkFlagBit
    {
         LWL_ERR_FLAG_RX_DL_DATA_PARITY
        ,LWL_ERR_FLAG_RX_DL_CTRL_PARITY
        ,LWL_ERR_FLAG_RX_PROTOCOL
        ,LWL_ERR_FLAG_RX_OVERFLOW
        ,LWL_ERR_FLAG_RX_RAM_DATA_PARITY
        ,LWL_ERR_FLAG_RX_RAM_HDR_PARITY
        ,LWL_ERR_FLAG_RX_RESP
        ,LWL_ERR_FLAG_RX_POISON
        ,LWL_ERR_FLAG_TX_RAM_DATA_PARITY
        ,LWL_ERR_FLAG_TX_RAM_HDR_PARITY
        ,LWL_ERR_FLAG_DL_FLOW_PARITY
        ,LWL_ERR_FLAG_DL_HDR_PARITY
        ,LWL_ERR_FLAG_TX_CREDIT
        ,LWL_ERR_FLAG_EXCESS_DL

        // Add new flag defines above this line
        ,LWL_NUM_ERR_FLAGS
    };

    LwLinkFlagBit GetErrorFlagBit(UINT32 flagIdx);
    string GetFlagString(const LwLinkFlagBit flagBit);
};
