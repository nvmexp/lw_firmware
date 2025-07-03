/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef __INCLUDED_LWL_TOPOLOGY_IMPL_H__
#define __INCLUDED_LWL_TOPOLOGY_IMPL_H__

#include "core/include/rc.h"
#include "gpu/utility/lwlink/lwlinkdev.h"
#include "lwl_devif.h"
#include <string>
#include <vector>
#include <memory>

class GlobalFabricManager;
class LocalFabricManagerControl;

namespace LwLinkDevIf
{
    //--------------------------------------------------------------------------
    //! \brief Topology manager implementation base class.  Implements generic
    //! functionality used by all implementations.
    class TopologyManagerImpl
    {
    public:
        TopologyManagerImpl(string name, TopologyMatch tm, string topoFile);
        virtual ~TopologyManagerImpl() { }

        // Pure virtual funcitons implemented by concrete implementations
        virtual LinkConfig GetDefaultLinkConfig() = 0;
        virtual TestDevicePtr GetDevice
        (
            TestDevice::Type devType
           ,UINT32           topoId
        ) = 0;
        virtual RC GetDevices(vector<TestDevicePtr>* pDevices) = 0;
        virtual RC GetExpectedLinkMask(TestDevicePtr pDevice, UINT64 *pLinkMask) = 0;
        virtual UINT32 GetMinForcedDevices(TestDevice::Type devType) const = 0;
        virtual RC Initialize() = 0;
        virtual RC Setup(const vector<TestDevicePtr> &devices) = 0;
        virtual RC SetupPostDiscovery(const vector<TestDevicePtr> &devices) = 0;
        virtual RC SetupPostTraining(const vector<TestDevicePtr> &devices) = 0;
        virtual bool ShouldUse() const = 0;

        virtual RC DiscoverConnections(const vector<TestDevicePtr> &devices) = 0;
        virtual RC TrainLinks(const vector<TestDevicePtr> &devices, bool * pbAllLinksTrained) = 0;
        virtual RC ReinitFailedLinks() = 0;
        virtual RC ShutdownLinks(const vector<TestDevicePtr> &devices) = 0;
        virtual RC ResetTopology(const vector<TestDevicePtr> &devices) = 0;

        string GetName() { return m_Name; }
        RC GetRoutes(vector<LwLinkRoutePtr> *pRoutes);
        RC GetRoutesOnDevice(TestDevicePtr pDevice, vector<LwLinkRoutePtr> *pRoutes);
        void Print(const vector<TestDevicePtr> &devices, Tee::Priority  pri);
        virtual void SetTopologyMatch(TopologyMatch topo) { m_TopologyMatch = topo; }
        virtual void SetTopologyFile(string topologyFile) { m_TopologyFile = topologyFile; }
        virtual void SetTopologyFlags(UINT32 flags) { m_TopologyFlags = flags; }
        virtual void SetHwDetectMethod(HwDetectMethod hwd) { m_HwDetectMethod = hwd; }
        virtual RC Shutdown();

    protected:
        class ConnectionSet
        {
        private:
            typedef vector<LwLinkConnectionPtr> Container;
        public:
            typedef Container::value_type value_type;
            typedef Container::iterator iterator;
            typedef Container::const_iterator const_iterator;
            typedef Container::size_type size_type;

            iterator begin() { return m_TheSet.begin(); }
            const_iterator begin() const { return m_TheSet.begin(); }
            iterator end() { return m_TheSet.end(); }
            const_iterator end() const { return m_TheSet.end(); }

            const_iterator cbegin() const { return m_TheSet.begin(); }
            const_iterator cend() const { return m_TheSet.end(); }

            void clear() { m_TheSet.clear(); }

            iterator find(const LwLinkConnectionPtr key)
            {
                return find_if(begin(), end(), [key](value_type v) { return *key == *v; });
            }

            const_iterator find(const LwLinkConnectionPtr key) const
            {
                return find_if(cbegin(), cend(), [key](value_type v) { return *key == *v; });
            }

            size_type count(const LwLinkConnectionPtr key) const
            {
                if (end() != find(key)) return 1;
                return 0;
            }

            pair<iterator, bool> insert(const LwLinkConnectionPtr key)
            {
                auto it = find(key);
                if (end() == it)
                {
                    m_TheSet.push_back(key);
                    return make_pair(end() - 1, true);
                }
                else
                {
                    return make_pair(it, false);
                }
            }
        private:
            Container m_TheSet;
        };

        LwLinkConnectionPtr AddConnection(LwLinkConnectionPtr pCon);
        RC AddPathToRoute(const LwLinkPath &path);
        void ConsolidatePaths();
        void CreateUnusedConnections(const vector<TestDevicePtr> &devices);
        ConnectionSet &GetConnections() { return m_Connections; }
        TopologyMatch GetTopologyMatch() const { return m_TopologyMatch; }
        HwDetectMethod GetHwDetectMethod() const { return m_HwDetectMethod; }
        string GetTopologyFile() const { return m_TopologyFile; }
        UINT32 GetTopologyFlags() const { return m_TopologyFlags; }
        RC GetDeviceById
        (
            const vector<TestDevicePtr> &devices
           ,TestDevice::Id               deviceId
           ,TestDevicePtr               *ppDevice
        ) const;
    private:
        RC BringupLinks(const vector<TestDevicePtr> &devices);
        RC GetRouteWithEndpoints
        (
            LwLinkConnection::EndpointDevices endpoints
           ,LwLinkRoutePtr                   *ppRoute
        );

        string m_Name;

        //! Set of connections between lwlink devices
        ConnectionSet m_Connections;

        //! Array of routes between lwlink endpoint devices
        vector<LwLinkRoutePtr> m_Routes;

        TopologyMatch  m_TopologyMatch;   //!< Topology Match Config
        string         m_TopologyFile;    //!< Topology file to use
        HwDetectMethod m_HwDetectMethod;
        UINT32         m_TopologyFlags;
    };
    typedef shared_ptr<TopologyManagerImpl> TopologyManagerImplPtr;

    namespace TopologyManagerImplFactory
    {
        //----------------------------------------------------------------------
        //! \brief Factory namespace for creating various types of topology
        //! manager implementations.  Topology manager implementations register
        //! themselves during static construction time
        typedef TopologyManagerImpl * (*FactoryFunc)(string,
                                                     TopologyMatch,
                                                     string);

        //----------------------------------------------------------------------
        //! \brief Class that is used to register a topology manager
        //! implementation creation function with the factory.  Each type of
        //! topology manager implementation should define an instance
        //! of this class in order to register with the factory
        class FactoryFuncRegister
        {
            public:
                FactoryFuncRegister(UINT32 order, string name, FactoryFunc f);
                ~FactoryFuncRegister() { }
        };

        //----------------------------------------------------------------------
        //! \brief Schwartz counter class to ensure factory data is initialized
        //! during static cconstruction time prior to any factory functions
        //! being registered
        class Initializer
        {
            public:
                Initializer();
                ~Initializer();
        };

        void CreateAll
        (
            TopologyMatch                    tm
           ,const string &                   topoFile
           ,vector<TopologyManagerImplPtr> * pTopoMgrs
        );
    }
};

//! Schwartz counter instance needs to be defined in header so that all users of
//! the factory ensure factory data is created first
static LwLinkDevIf::TopologyManagerImplFactory::Initializer s_LwLinkTopolgyMgrFactoryInit;

#endif
