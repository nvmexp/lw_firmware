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

#ifndef __INCLUDED_LWL_TOPOLOGY_AUTO_H__
#define __INCLUDED_LWL_TOPOLOGY_AUTO_H__

#include "core/include/rc.h"
#include "gpu/utility/lwlink/lwlinkdev.h"
#include "lwl_devif.h"
#include "lwl_topology_mgr_impl.h"
#include <string>
#include <vector>

namespace LwLinkDevIf
{
    //--------------------------------------------------------------------------
    //! \brief Topology manager implementation for toplogies that can be auto
    //! detected.
    class TopologyManagerAuto : public TopologyManagerImpl
    {
    public:
        TopologyManagerAuto(string name, TopologyMatch tm, string topoFile) :
            TopologyManagerImpl(name, tm, topoFile) { }
        virtual ~TopologyManagerAuto() { }
    private:
        // Make all public virtual functions from the base class private
        // in order to forbid external access to this class except through
        // the base class pointer
        virtual LinkConfig GetDefaultLinkConfig() { return LC_AUTO; }
        virtual TestDevicePtr GetDevice
        (
            TestDevice::Type devType
           ,UINT32           topoId
        );
        virtual RC GetDevices(vector<TestDevicePtr>* pDevices);
        virtual RC GetExpectedLinkMask(TestDevicePtr pDevice, UINT64 *pLinkMask);
        virtual UINT32 GetMinForcedDevices(TestDevice::Type devType) const;
        virtual RC Initialize() { return OK; }
        RC Setup(const vector<TestDevicePtr> &devices) override;
        RC SetupPostDiscovery(const vector<TestDevicePtr> &devices) override;
        RC SetupPostTraining(const vector<TestDevicePtr> &devices) override;
        virtual bool ShouldUse() const;

        RC DiscoverConnections(const vector<TestDevicePtr> &devices) override
            { return RC::OK; }
        RC TrainLinks(const vector<TestDevicePtr> &devices, bool * pbAllLinksTrained) override;
        RC ReinitFailedLinks() override
            { return RC::UNSUPPORTED_FUNCTION; }
        RC ShutdownLinks(const vector<TestDevicePtr> &devices) override;
        RC ResetTopology(const vector<TestDevicePtr> &devices) override
            { return RC::OK; }

        RC ConstructRoutes(TestDevicePtr srcDev);
        RC CreateConnections(const vector<TestDevicePtr> &devices);
    };
};

#endif

