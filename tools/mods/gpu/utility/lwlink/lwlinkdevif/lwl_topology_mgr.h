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

#ifndef __INCLUDED_LWL_TOPOLOGY_MGR_H__
#define __INCLUDED_LWL_TOPOLOGY_MGR_H__

#include "core/include/rc.h"
#include "gpu/utility/lwlink/lwlinkdev.h"
#include "lwl_devif.h"
#include "lwl_fabricmanager_libif.h"
#include <string>
#include <memory>

namespace LwLinkDevIf
{
    // Topology manager implementation class.  Different system configurations
    // have different methods of detecting topologies
    class TopologyManagerImpl;
    typedef shared_ptr<TopologyManagerImpl> TopologyManagerImplPtr;

    //--------------------------------------------------------------------------
    //! \brief Topology manager used to manage topology detection and
    //! configuration.  Different topology manager implementations register
    //! themselves during static construction time
    namespace TopologyManager
    {
        TopologyManagerImplPtr GetImpl();
        RC GetExpectedLinkMask(TestDevicePtr pDevice, UINT64 *pLinkMask);
        UINT32 GetMinForcedDevices(TestDevice::Type devType);
        string GetPostDiscoveryDumpFunction();
        string GetPostTrainingDumpFunction();

        RC ClearTopologyFlag(TopologyFlag flag);
        RC GetDevices(vector<TestDevicePtr>* pDevices);
        FMLibIfPtr GetFMLibIf();
        bool GetIgnoreMissingCC();
        UINT32 GetLinkTrainAttempts();
        RC GetRoutes(vector<LwLinkRoutePtr> *pRoutes);
        RC GetRoutesOnDevice(TestDevicePtr pDevice, vector<LwLinkRoutePtr> *pRoutes);
        bool GetSkipLinkShutdownOnExit();
        bool GetSkipLinkShutdownOnInit();
        bool GetSkipTopologyDetect();
        bool GetSystemParameterPresent();
        string GetTopologyFile();
        string GetSocketPath();
        UINT32 GetTopologyFlags();
        TopologyMatch GetTopologyMatch();
        RC Initialize();
        RC Print(Tee::Priority pri);
        RC SetForcedLinkConfig(LinkConfig flc);
        RC SetHwDetectMethod(HwDetectMethod hwd);
        RC SetIgnoreMissingCC(bool bIgnoreMissingCC);
        RC SetLinkTrainAttempts(UINT32 linkTrainAttempts);
        void SetPostDiscoveryDumpFunction(string func);
        void SetPreTrainingDumpFunction(string func);
        void SetPostTrainingDumpFunction(string func);
        void SetPostShutdownDumpFunction(string func);
        RC SetSkipLinkShutdownOnExit(bool bSkipLinkShutdownOnExit);
        RC SetSkipLinkShutdownOnInit(bool bSkipLinkShutdownOnInit);
        RC SetSkipTopologyDetect(bool bSkipTopologyDetect);
        RC SetSocketPath(string socketPath);
        RC SetSystemParameterPresent(bool bPresent);
        RC SetTopologyFile(string topologyFile);
        RC SetTopologyFlag(TopologyFlag flag);
        RC SetTopologyMatch(TopologyMatch topo);
        RC Setup();
        RC Shutdown();
    };
};

#endif
