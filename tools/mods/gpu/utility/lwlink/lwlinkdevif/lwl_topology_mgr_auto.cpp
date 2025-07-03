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

#include "lwl_topology_mgr_auto.h"

#include "core/include/mgrmgr.h"
#include "core/include/rc.h"
#include "core/include/xp.h"
#include "device/interface/lwlink.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/utility/lwlink/lwlinkdev.h"
#include "lwl_devif_mgr.h"
#include <string>
#include <vector>

namespace LwLinkDevIf
{
    //--------------------------------------------------------------------------
    //! \brief Create an IBM NPU device interface
    //!
    //! \return Pointer to new IBM NPU device interface
    TopologyManagerImpl * CreateTopologyManagerAuto
    (
        string          name
       ,TopologyMatch   tm
       ,string          topoFile
    )
    {
        return new TopologyManagerAuto(name, tm, topoFile);
    }

    //! Register the automatic topology manager with the factory.  This should
    //! always be created so that the order parameter causes it to be the last
    //! type of topology manager created
    TopologyManagerImplFactory::FactoryFuncRegister
        m_TopologyMgrAuto(2, "TopologyManagerAuto", CreateTopologyManagerAuto);
};

//------------------------------------------------------------------------------
//! \brief Get the device of the specified type with the specified id within the
//! topology specification
//!
//! \param devType : Device type to get
//! \param topoId  : ID within the topology specification
//!
//! \return TestDevicePtr of the device
//!
/* virtual */ TestDevicePtr LwLinkDevIf::TopologyManagerAuto::GetDevice
(
    TestDevice::Type devType
   ,UINT32           topoId
)
{
    return TestDevicePtr();
}

//-----------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::TopologyManagerAuto::GetDevices
(
    vector<TestDevicePtr>* pDevices
)
{
    MASSERT(pDevices);
    RC rc;

    pDevices->clear();

    TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
    for (UINT32 idx = 0; idx < pTestDeviceMgr->NumDevices(); idx++)
    {
        TestDevicePtr pDevice;
        CHECK_RC(pTestDeviceMgr->GetDevice(idx, &pDevice));
        if (pDevice->SupportsInterface<LwLink>())
        {
            pDevices->push_back(pDevice);
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::TopologyManagerAuto::GetExpectedLinkMask
(
    TestDevicePtr pDevice,
    UINT64 *pLinkMask
)
{
    return pDevice->GetInterface<LwLink>()->GetDiscoveredLinkMask(pLinkMask);
}

//------------------------------------------------------------------------------
//! \brief Get the minimum number of devices of the specified type
//!
//! \param devType : Device type to get the minimum number of devices on
//!
//! \return Minimum number of devices
//!
/* virtual */ UINT32  LwLinkDevIf::TopologyManagerAuto::GetMinForcedDevices
(
    TestDevice::Type devType
) const
{
    if ((GetTopologyMatch() == TM_DEFAULT) &&
        (Xp::GetOperatingSystem() != Xp::OS_WINSIM) &&
        (Xp::GetOperatingSystem() != Xp::OS_LINUXSIM))
    {
        return 0;
    }

    // For forced configs, use a minimum number of ebridges
    static const UINT32 MIN_FORCED_EBRIDGES = 1;
    static const UINT32 MIN_FORCED_NPUS     = 2;
    if (devType == TestDevice::TYPE_EBRIDGE)
        return MIN_FORCED_EBRIDGES;
    if (devType == TestDevice::TYPE_IBM_NPU)
        return MIN_FORCED_NPUS;

    // No other devices are forced with auto detected topology, the devices
    // must actually be there
    return 0;
}

//------------------------------------------------------------------------------
//! \brief Setup the topology manager implementation
//!
//! \param devices            : Vector of discovered devices
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::TopologyManagerAuto::Setup
(
    const vector<TestDevicePtr> &devices
)
{
    // The auto topology manager by definition relies on discovery being complete
    // in order to do processing
    return RC::OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::TopologyManagerAuto::SetupPostDiscovery
(
    const vector<TestDevicePtr> &devices
)
{
    RC rc;

    for (const auto& pDevice : devices)
    {
        CHECK_RC(pDevice->GetInterface<LwLink>()->DetectTopology());
    }

    CHECK_RC(CreateConnections(devices));

    for (auto const & pLwrLwLinkDev : devices)
    {
        if (!pLwrLwLinkDev->IsEndpoint())
        {
            Printf(Tee::PriError,
                   "%s : Auto configuration not supported with non endpoint devices!\n",
                   __FUNCTION__);
            return RC::SOFTWARE_ERROR;
        }

        CHECK_RC(ConstructRoutes(pLwrLwLinkDev));
    }

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::TopologyManagerAuto::SetupPostTraining
(
    const vector<TestDevicePtr> &devices
)
{
    return RC::OK;
}

//------------------------------------------------------------------------------
//! \brief Return whether this topology manager implementation should be used
//!
//! \return True if this topology manager implementation should be used
//!
/* virtual */ bool  LwLinkDevIf::TopologyManagerAuto::ShouldUse() const
{
    if ((GetTopologyMatch() == TM_FILE) ||
        ((GetTopologyMatch() == TM_DEFAULT) && (GetTopologyFile() != "")))
    {
        return false;
    }
    return true;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManagerAuto::TrainLinks
(
    const vector<TestDevicePtr> &devices,
    bool * pbAllLinksTrained
)
{
    RC rc;


    for (auto & pTestDev : devices)
    {
        if (!pTestDev->SupportsInterface<LwLink>())
            continue;

        auto *pLwLink = pTestDev->GetInterface<LwLink>();
        vector<LwLink::EndpointInfo> epInfo;
        CHECK_RC(pLwLink->GetDetectedEndpointInfo(&epInfo));

        for (size_t lwrLink = 0; lwrLink < epInfo.size(); lwrLink++)
        {
            if (epInfo[lwrLink].devType == Device::TYPE_UNKNOWN)
                continue;
            CHECK_RC(pLwLink->SetLinkState(static_cast<UINT32>(lwrLink), LwLink::SLS_HIGH_SPEED));
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManagerAuto::ShutdownLinks(const vector<TestDevicePtr> &devices)
{
    RC rc;

    for (auto & pTestDev : devices)
    {
        if (!pTestDev->SupportsInterface<LwLink>())
            continue;

        auto *pLwLink = pTestDev->GetInterface<LwLink>();
        vector<LwLink::EndpointInfo> epInfo;
        CHECK_RC(pLwLink->GetDetectedEndpointInfo(&epInfo));

        for (size_t lwrLink = 0; lwrLink < epInfo.size(); lwrLink++)
        {
            if (epInfo[lwrLink].devType == Device::TYPE_UNKNOWN)
                continue;
            CHECK_RC(pLwLink->SetLinkState(static_cast<UINT32>(lwrLink), LwLink::SLS_OFF));
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Construct all the routes in the system
//!
//! \param srcDev : Device to start constructing routes from
//!
//! \return OK if successful, not OK on error
RC LwLinkDevIf::TopologyManagerAuto::ConstructRoutes(TestDevicePtr srcDev)
{
    RC rc;
    ConnectionSet connections = GetConnections();

    // All paths in an auto detected topology are simple paths (i.e. no
    // lwlink bridge devices are present)
    for (auto const & pCon : connections)
    {
        if (!pCon->UsesDevice(srcDev) || !pCon->IsActive())
            continue;

        LwLinkConnection::EndpointDevices epDevs = pCon->GetEndpointDevices();
        if ((epDevs.first == nullptr) || (epDevs.second == nullptr))
        {
            Printf(Tee::PriError,
                   "%s : Connections must be between endpoint devices only!",
                   __FUNCTION__);
            return RC::SOFTWARE_ERROR;
        }

        LwLinkPath path(LwLink::DT_ALL);
        UINT32 srcLinkMask = pCon->GetLocalLinkMask(srcDev);
        if (!path.AddEntry(pCon, srcDev, srcLinkMask))
        {
            Printf(Tee::PriError,
                   "%s : Failed to add entry to path!",
                   __FUNCTION__);
            return RC::SOFTWARE_ERROR;
        }

        bool bPathValid = false;
        CHECK_RC(path.Validate(&bPathValid));
        if (bPathValid)
        {
            CHECK_RC(AddPathToRoute(path));
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Create all lwlink connections
//!
//! \return OK if successful, not OK otherwise
RC LwLinkDevIf::TopologyManagerAuto::CreateConnections(const vector<TestDevicePtr> &devices)
{
    RC rc;

    set<TestDevice::Id> scannedIds;
    for (auto const & pLwrLwLinkDev : devices)
    {
        if (pLwrLwLinkDev->GetType() != TestDevice::TYPE_LWIDIA_GPU)
            continue;

        auto pLwrLwLink = pLwrLwLinkDev->GetInterface<LwLink>();
        typedef map<TestDevicePtr, vector<LwLinkConnection::EndpointIds> > ConnMap;
        set<UINT32> loopoutSkipLinks;
        ConnMap lwrCons;
        for (UINT32 lwrLink = 0; lwrLink < pLwrLwLink->GetMaxLinks(); lwrLink++)
        {
            LwLinkConnection::EndpointIds linkEndpoints(lwrLink,
                                                        LwLink::ILWALID_LINK_ID);
            TestDevicePtr pRemoteDev;

            // Dont add the same connection twice with reversed endpoints if the
            // connection is a loopout connection
            if (loopoutSkipLinks.find(lwrLink) != loopoutSkipLinks.end())
                continue;

            // If the link is active fill in the remote link ID
            if (pLwrLwLink->IsLinkActive(lwrLink))
            {
                LwLink::EndpointInfo endInfo;
                CHECK_RC(pLwrLwLink->GetRemoteEndpoint(lwrLink, &endInfo));

                // If the remote device was already scanned, skip this link
                // since it was already added when the remote device was scanned
                if (scannedIds.find(endInfo.id) != scannedIds.end())
                    continue;

                CHECK_RC(GetDeviceById(devices, endInfo.id, &pRemoteDev));
                CHECK_RC(pRemoteDev->GetInterface<LwLink>()->GetLinkId(endInfo, &linkEndpoints.second));

                if (pLwrLwLinkDev == pRemoteDev)
                    loopoutSkipLinks.insert(linkEndpoints.second);
            }

            ConnMap::iterator pEndpoints = lwrCons.find(pRemoteDev);
            if (pEndpoints != lwrCons.end())
            {
                pEndpoints->second.push_back(linkEndpoints);
            }
            else
            {
                vector<LwLinkConnection::EndpointIds> newEndpoints(1, linkEndpoints);
                lwrCons[pRemoteDev] = newEndpoints;
            }
        }

        // Create the proper new connections and save them
        for (auto const & lwrCon : lwrCons)
        {
            LwLinkConnectionPtr pNewCon(new LwLinkConnection(pLwrLwLinkDev,
                                                             lwrCon.first,
                                                             lwrCon.second));
            AddConnection(pNewCon);
        }

        // Add the current device ID to the "scanned" list to avoid duplicate
        // entries when we scan the other side of the link
        scannedIds.insert(pLwrLwLinkDev->GetId());
    }

    CreateUnusedConnections(devices);

    return rc;
}
