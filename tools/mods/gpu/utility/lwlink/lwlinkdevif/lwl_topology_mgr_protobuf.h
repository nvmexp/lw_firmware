/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef __INCLUDED_LWL_TOPOLOGY_PROTOBUF_H__
#define __INCLUDED_LWL_TOPOLOGY_PROTOBUF_H__

#include "core/include/rc.h"
#include "gpu/utility/lwlink/lwlinkdev.h"
#include "lwl_topology_mgr_impl.h"
#include <string>
#include <vector>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>

namespace LwLinkDevIf
{
    //--------------------------------------------------------------------------
    //! \brief Topology manager implementation for toplogies that are specified
    //! via a protobuf file
    class TopologyManagerProtobuf : public TopologyManagerImpl
    {
    public:
        TopologyManagerProtobuf(string name, TopologyMatch tm, string topoFile);
        virtual ~TopologyManagerProtobuf() { }

    protected:
        struct SwitchTopologyMatch
        {
            set<UINT32> topologyIdMatches;
            bool        bRemoteDevicesAssigned = false;
            bool        bMatched               = false;
        };

        // Make all public virtual functions from the base class private
        // in order to forbid external access to this class except through
        // the base class pointer
        LinkConfig GetDefaultLinkConfig() override { return LC_MANUAL; }
        RC GetDevices(vector<TestDevicePtr>* pDevices) override;
        TestDevicePtr GetDevice(TestDevice::Type devType, UINT32 topoId) override;
        RC GetExpectedLinkMask(TestDevicePtr pDevice, UINT64 *pLinkMask) override;
        UINT32 GetMinForcedDevices(TestDevice::Type devType) const override;
        RC Initialize() override;
        RC Setup(const vector<TestDevicePtr> &devices) override;
        RC SetupPostDiscovery(const vector<TestDevicePtr> &devices) override;
        RC SetupPostTraining(const vector<TestDevicePtr> &devices) override;
        bool ShouldUse() const override;
        RC Shutdown() override;

    protected:
        // A 2 dimensional map indexed by where map[dev1][dev2] contains a vector
        // of endpoint IDs to indicate the endpoints between device 1 and device 2
        typedef map<TestDevicePtr, vector<LwLinkConnection::EndpointIds>> EndpointMapEntry;
        typedef map<TestDevicePtr, EndpointMapEntry> EndpointMap;

        RC  ConstructRoutes
        (
            const vector<TestDevicePtr> &devices
           ,TestDevicePtr                pSrcDev
           ,const EndpointMapEntry      &srcEndpoints
           ,UINT64                       fabricBase
           ,LwLink::DataType             dt
        );
        RC SetupFabricApertures(vector<LwLink::FabricAperture> *pAllFas);

    private:
        // Private functions
        RC GetTopologyId(TestDevicePtr pDev, UINT32 *pTopoId);
        RC GetEndpointInfo
        (
            TestDevicePtr                 pDev
           ,vector<LwLink::EndpointInfo> *pEndpointInfo
        );

        RC DiscoverConnections(const vector<TestDevicePtr> &devices) override;
        RC TrainLinks(const vector<TestDevicePtr> &devices, bool * pbAllLinksTrained) override;
        RC ReinitFailedLinks() override;
        RC ShutdownLinks(const vector<TestDevicePtr> &devices) override;
        RC ResetTopology(const vector<TestDevicePtr> &devices) override;

        bool AllDevicesMapped();
        bool AllDevicesMapped(TestDevice::Type type);
        RC AssignDeviceToTopology
        (
            TestDevicePtr  pDev
           ,UINT32         topologyIndex
           ,bool         * pbTopologyMismatch
        );
        RC AssignUniqueDeviceToToplogy
        (
            const vector<TestDevicePtr> &devices
           ,TestDevice::Type             devType
        );
        RC CheckDeviceCount(const vector<TestDevicePtr> &devices, TestDevice::Type devType);

        RC CreateSourceConnections
        (
            TestDevicePtr           pSrcDev
           ,const EndpointMapEntry &srcEndpoints
           ,UINT64                  fabricBase
           ,LwLink::DataType        dt
           ,vector<LwLinkConnectionPtr> * pConns
        );
        RC CreateSourceEndpointMap
        (
            const vector<TestDevicePtr> &devices
           ,EndpointMap                 *pSrcEndpointMap
        ) const;
        RC  DoConstructRoutes
        (
            const vector<TestDevicePtr> &devices
           ,TestDevicePtr                pSrcDev
           ,vector<UINT32>               srcLinks
           ,LwLinkPath                   path
        );
        RC DoSwitchPortsMatch
        (
            TestDevicePtr                        pDev
           ,const vector<TestDevicePtr>        & devices
           ,bool                               * pbSwitchMatches
        );

        RC MapDevicesToTopologyTrivial(const vector<TestDevicePtr> &devices);
        RC MapDevicesToTopologyForced(const vector<TestDevicePtr> &devices);
        RC MapDevicesToTopologyPciInfo(const vector<TestDevicePtr> &devices);

        RC ValidateTopologyIds(const vector<TestDevicePtr> &devices);

        bool TrexFlagSet() const;

        // Keys to use with the boost multi index container
        struct dev_index { };
        struct topo_id_index { };

        // The multi index container is used to map either TestDevicePtr to a topology
        // ID or a topology ID to a TestDevicePtr
        typedef pair<TestDevicePtr, UINT32> DeviceToTopologyIdMiEntry;
        typedef boost::multi_index::multi_index_container<
            DeviceToTopologyIdMiEntry
          , boost::multi_index::indexed_by<
                boost::multi_index::sequenced<> // list like index just in case for going through
                                                // all records effectively and populating
              , boost::multi_index::ordered_unique <
                    boost::multi_index::tag<dev_index>
                  , boost::multi_index::member<
                        DeviceToTopologyIdMiEntry
                      , TestDevicePtr
                      , &DeviceToTopologyIdMiEntry::first
                      >
                  >
              , boost::multi_index::ordered_unique <
                      boost::multi_index::tag<topo_id_index>
                    , boost::multi_index::member<
                        DeviceToTopologyIdMiEntry
                      , UINT32
                      , &DeviceToTopologyIdMiEntry::second
                      >
                  >
              >
          > DeviceToTopologyIdMi;
        typedef DeviceToTopologyIdMi::index<topo_id_index>::type TopoIdToDevice;
        typedef DeviceToTopologyIdMi::index<dev_index>::type DeviceToTopoId;

        // Requester link IDs are unique and there is one per link per device
        // (for endpoint devices only)
        typedef map<UINT32, TestDevicePtr> RequesterLinkIdMap;

        // There needs to be one multi-instance per device type since topology IDs
        // are not unique across all device times
        typedef map<TestDevice::Type, DeviceToTopologyIdMi> TopologyIdMap;

        //! Map of multi-instance maps used to connect devices to their
        //!associated IDs within the topology
        TopologyIdMap      m_TopologyMap;
        bool               m_bInitialized;    //!< True if initialized
    };
};

#endif

