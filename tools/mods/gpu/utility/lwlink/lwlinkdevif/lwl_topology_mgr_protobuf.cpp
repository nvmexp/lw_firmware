/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwl_topology_mgr_protobuf.h"

#include "core/include/mgrmgr.h"
#include "core/include/mle_protobuf.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "device/interface/lwlink.h"
#include "device/interface/lwlink/lwlcci.h"
#include "gpu/include/testdevicemgr.h"
#include "lwl_devif.h"
#include "lwl_devif_mgr.h"
#include "lwl_topology_mgr.h"

#if LWCFG(GLOBAL_LWSWITCH_IMPL_LR10)
    #include "gpu/utility/lwlink/lwlinkdevif/lwl_trex_dev.h"
#endif

#include <algorithm>
#include <fstream>
#include <istream>
#include <streambuf>

namespace
{
    UINT32 GetRemoteEndpoints(LwLinkConnection::EndpointIds e)
    {
        return e.second;
    }

    // Create lambdas that can be used to take slices out of the device vector for all devices
    // matching a particular type
    auto s_LbDeviceTypeLambda([](const TestDevicePtr & pDev, const TestDevice::Type d) { return pDev->GetType() < d; }); //$
    auto s_UbDeviceTypeLambda([](const TestDevice::Type d, const TestDevicePtr & pDev) { return pDev->GetType() > d; }); //$
    using ProtoMismatchReason = Mle::LwlinkMismatch::MismatchReason;
    Mle::LwlinkMismatch::DeviceType ToProtoLwlinkDeviceType(TestDevice::Type deviceType)
    {
        using ProtoType = Mle::LwlinkMismatch::DeviceType;
        switch (deviceType)
        {
            case TestDevice::Type::TYPE_LWIDIA_GPU:              return ProtoType::gpu;
            case TestDevice::Type::TYPE_LWIDIA_LWSWITCH:         return ProtoType::lwswitch;
            case TestDevice::Type::TYPE_TREX:                    return ProtoType::trex;
            case TestDevice::Type::TYPE_UNKNOWN:                 return ProtoType::none;
            default:
                MASSERT(!"Unsupported Test Device type");
                return ProtoType::none;
        }
    }
}

namespace LwLinkDevIf
{
    //--------------------------------------------------------------------------
    //! \brief Create a topology manager protobuf implementation
    //!
    //! \return Pointer to new IBM NPU device interface
    TopologyManagerImpl * CreateTopologyManagerProtobuf
    (
        string          name
       ,TopologyMatch   tm
       ,string          topoFile
    )
    {
        return new TopologyManagerProtobuf(name, tm, topoFile);
    }

    //! Register the Protobuf topology mananger implementation with the factory
    TopologyManagerImplFactory::FactoryFuncRegister
        m_TopologyMgrProtobuf(0, "TopologyManagerProtobuf", CreateTopologyManagerProtobuf);
};

//------------------------------------------------------------------------------
//! \brief Constructor
//!
LwLinkDevIf::TopologyManagerProtobuf::TopologyManagerProtobuf
(
    string name
   ,TopologyMatch tm
   ,string topoFile
)
 : TopologyManagerImpl(name, tm, topoFile)
  ,m_bInitialized(false)
{
}

//------------------------------------------------------------------------------
// TopologyManagerProtobuf Private Implementation
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! \brief Get the device of the specified type with the specified id within the
//! topology specification
//!
//! \param devType : Device type to get
//! \param topoId  : ID within the topology specification
//!
//! \return TestDevicePtr of the device
//!
/* virtual */ TestDevicePtr LwLinkDevIf::TopologyManagerProtobuf::GetDevice
(
    TestDevice::Type devType
   ,UINT32           topoId
)
{
    if (m_TopologyMap.find(devType) == m_TopologyMap.end())
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "Topolgy map not found for device type %d\n", devType);
        return TestDevicePtr();
    }
    TopoIdToDevice &idToDev = m_TopologyMap[devType].get<topo_id_index>();
    TopoIdToDevice::iterator ppDev = idToDev.find(topoId);

    return (ppDev == idToDev.end()) ? TestDevicePtr() : ppDev->first;
}

//-----------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::TopologyManagerProtobuf::GetDevices
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
            if (TrexFlagSet()
                && pDevice->GetType() == TestDevice::TYPE_LWIDIA_GPU)
            {
                // Skip GPU devices when using TREX
                // Both cannot exist in the same topology simultaneously
                // And TREXs take the place of the GPUs
                continue;
            }
            pDevices->push_back(pDevice);
        }
    }

#if LWCFG(GLOBAL_LWSWITCH_IMPL_LR10)
    if (TrexFlagSet())
    {
        // Create TREX device equivalents for the GPUs in the topology
        auto pFMLibIf = TopologyManager::GetFMLibIf();
        UINT32 numGpus = pFMLibIf->GetNumGpus();
        for (UINT32 i = 0; i < numGpus; i++)
        {
            UINT32 physicalId = ~0U;
            CHECK_RC(pFMLibIf->GetGpuPhysicalId(i, &physicalId));
            TestDevicePtr pTrexDev(new TrexDev(TestDevice::Id(0, TestDevice::Id::TREX_PCI_BUS, physicalId, 0)));
            pTrexDev->Initialize();
            pDevices->push_back(pTrexDev);
        }
    }
#endif

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Extract the endpoint information from the topology file for the
//! specified device
//!
//! \param devType        : Device to get endpoint info for
//! \param pEndpointInfo  : Pointer to returned endpoint info
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::TopologyManagerProtobuf::GetEndpointInfo
(
    TestDevicePtr                 pDev
   ,vector<LwLink::EndpointInfo> *pEndpointInfo
)
{
    MASSERT(pEndpointInfo);

    pEndpointInfo->clear();
    pEndpointInfo->resize(pDev->GetInterface<LwLink>()->GetMaxLinks());

    UINT32 topoId = 0;
    if (RC::OK != GetTopologyId(pDev, &topoId))
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "Device %s not found in topology specification, skipping\n",
               pDev->GetName().c_str());
        return OK;
    }

    // Ebridge devices not supported by topology specification
    if (pDev->GetType() == TestDevice::TYPE_EBRIDGE)
        return OK;

    RC rc;

    // Detect an initial topology
    vector<LwLink::EndpointInfo> detectedInfo;
    CHECK_RC(pDev->GetInterface<LwLink>()->GetDetectedEndpointInfo(&detectedInfo));

    const bool bMatchMin = (GetTopologyMatch() == TM_MIN);

    for (UINT32 port = 0; port < pEndpointInfo->size(); port++)
    {
        if (bMatchMin &&
            detectedInfo[port].devType == TestDevice::TYPE_UNKNOWN &&
            !TrexFlagSet())
        {
            // Skip links which don't have a detected connection and aren't a TREX port
            continue;
        }

        FMLibInterface::PortInfo portInfo = {};
        portInfo.type = pDev->GetType();
        portInfo.nodeId = 0;
        portInfo.physicalId = topoId;
        portInfo.portNum = port;

        if (portInfo.type == TestDevice::TYPE_TREX)
            portInfo.type = TestDevice::TYPE_LWIDIA_GPU;

        FMLibInterface::PortInfo remInfo = {};
        CHECK_RC(TopologyManager::GetFMLibIf()->GetRemotePortInfo(portInfo, &remInfo));

        if (remInfo.type == TestDevice::TYPE_UNKNOWN)
            continue;

        LwLink::EndpointInfo& epInfo = (*pEndpointInfo)[port];
        epInfo.devType = remInfo.type;
        epInfo.linkNumber = remInfo.portNum;

        TestDevicePtr pRemDev = GetDevice(remInfo.type, remInfo.physicalId);
        if (pRemDev)
        {
            epInfo.devType = pRemDev->GetType(); // override to TREX if detected
            epInfo.id      = pRemDev->GetId();
            CHECK_RC(pRemDev->GetInterface<LwLink>()->GetPciInfo(remInfo.portNum,
                                                                 &epInfo.pciDomain,
                                                                 &epInfo.pciBus,
                                                                 &epInfo.pciDevice,
                                                                 &epInfo.pciFunction));
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManagerProtobuf::GetExpectedLinkMask
(
    TestDevicePtr pDevice,
    UINT64 *pLinkMask
)
{
    MASSERT(pLinkMask);
    *pLinkMask = 0x0LLU;
    RC rc;

    vector<LwLink::EndpointInfo> epInfo;
    CHECK_RC(GetEndpointInfo(pDevice, &epInfo));
    for (UINT32 i = 0; i < epInfo.size(); i++)
    {
        if (epInfo[i].devType == TestDevice::TYPE_UNKNOWN)
            continue;

        *pLinkMask |= (1LLU << i);
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Get the minimum number of devices of the specified type
//!
//! \param devType   : Device type to get the minimum number of devices on
//!
//! \return Device count
//!
/* virtual */ UINT32 LwLinkDevIf::TopologyManagerProtobuf::GetMinForcedDevices
(
    TestDevice::Type devType
) const
{
    if (GetTopologyMatch() == TM_DEFAULT || GetTopologyMatch() == TM_MIN)
        return 0;

    switch (devType)
    {
        case TestDevice::TYPE_EBRIDGE:
            break;
        case TestDevice::TYPE_LWIDIA_LWSWITCH:
            return TopologyManager::GetFMLibIf()->GetNumLwSwitches();
        case TestDevice::TYPE_LWIDIA_GPU:
            return TopologyManager::GetFMLibIf()->GetNumGpus();
        default:
            break;

    }
    return 0;
}

//------------------------------------------------------------------------------
//! \brief Get the topology ID from the topology file for the specified device
//!
//! \param pDev    : Pointer to lwlink device to get the topology file ID for
//! \param pTopoId : Pointer to returned topology ID
//!
//! \return OK if the device was found, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::TopologyManagerProtobuf::GetTopologyId
(
    TestDevicePtr pDev
   ,UINT32       *pTopoId
)
{
    TestDevice::Type devType = pDev->GetType();
    if (devType == TestDevice::TYPE_TREX)
        devType = TestDevice::TYPE_LWIDIA_GPU;

    if (m_TopologyMap.find(devType) == m_TopologyMap.end())
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "Topolgy map not found for device type %d\n",
               pDev->GetType());
        return RC::DEVICE_NOT_FOUND;
    }

    DeviceToTopoId &devToId = m_TopologyMap[devType].get<dev_index>();
    DeviceToTopoId::iterator pTopoIdIt = devToId.find(pDev);
    if (pTopoIdIt == devToId.end())
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "Device %s not found in topology specification\n",
               pDev->GetName().c_str());
        return RC::DEVICE_NOT_FOUND;
    }

    if (pTopoId)
        *pTopoId = pTopoIdIt->second;
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Initialize the implementation
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::TopologyManagerProtobuf::Initialize()
{
    if (m_bInitialized)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : Already initialized, skipping.\n",
               __FUNCTION__);
        return RC::OK;
    }

    m_bInitialized = true;

    return RC::OK;
}

//------------------------------------------------------------------------------
//! \brief Setup the topology manager implementation
//!
//! \param devices            : Vector of discovered devices
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::TopologyManagerProtobuf::Setup
(
    const vector<TestDevicePtr> &devices
)
{
    RC rc;

    if (!ShouldUse())
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Protobuf topology manager should not be used with auto "
               "detected configurations!\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(CheckDeviceCount(devices, TestDevice::TYPE_LWIDIA_GPU));
    CHECK_RC(CheckDeviceCount(devices, TestDevice::TYPE_LWIDIA_LWSWITCH));

    CHECK_RC(MapDevicesToTopologyTrivial(devices));
    CHECK_RC(MapDevicesToTopologyPciInfo(devices));

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::TopologyManagerProtobuf::SetupPostDiscovery
(
    const vector<TestDevicePtr> &devices
)
{
    return RC::OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::TopologyManagerProtobuf::SetupPostTraining
(
    const vector<TestDevicePtr> &devices
)
{
    RC rc;

    CHECK_RC(TopologyManager::GetFMLibIf()->FinishInitialize());

    CHECK_RC(MapDevicesToTopologyPciInfo(devices));
    CHECK_RC(MapDevicesToTopologyForced(devices));

    rc = ValidateTopologyIds(devices);
    if (RC::OK != rc)
    {
        Print(devices, Tee::PriNormal);
        CHECK_RC(rc);
    }

    vector<LwLink::FabricAperture> allFas;
    CHECK_RC(SetupFabricApertures(&allFas));

    for (const auto& pDevice : devices)
    {
        UINT32 topologyId;
        rc = GetTopologyId(pDevice, &topologyId);
        if (rc == RC::DEVICE_NOT_FOUND)
        {
            Printf(Tee::PriLow, GetTeeModule()->GetCode(),
                   "%s : LwLink device %s not found in topology, skipping topology setup!\n",
                   __FUNCTION__, pDevice->GetName().c_str());
            rc.Clear();
            continue;
        }
        CHECK_RC(rc);

        vector<LwLink::EndpointInfo> endpointInfo;
        CHECK_RC(GetEndpointInfo(pDevice, &endpointInfo));

        auto * pLwLink = pDevice->GetInterface<LwLink>();
        CHECK_RC(pLwLink->SetupTopology(endpointInfo, topologyId));

        // The current link training procedure is guaranteed to generate error flags on cable
        // controllers and flags are clear on read.  Read and discard all error flags after
        // training
        if (pLwLink->SupportsInterface<LwLinkCableController>())
        {
            LwLinkCableController::CCErrorFlags errorFlags;
            CHECK_RC(pLwLink->GetInterface<LwLinkCableController>()->GetErrorFlags(&errorFlags));
        }
    }

    EndpointMap srcEndpointMap;
    CHECK_RC(CreateSourceEndpointMap(devices, &srcEndpointMap));

    for (auto const & pSrcDev : devices)
    {
        if (!pSrcDev->IsEndpoint())
            continue;

        for (auto const & fa : allFas)
        {
            CHECK_RC(ConstructRoutes(devices,
                                     pSrcDev,
                                     srcEndpointMap[pSrcDev],
                                     fa.first,
                                     LwLink::DT_REQUEST));
            CHECK_RC(ConstructRoutes(devices,
                                     pSrcDev,
                                     srcEndpointMap[pSrcDev],
                                     fa.first,
                                     LwLink::DT_RESPONSE));
        }
    }
    CreateUnusedConnections(devices);
    ConsolidatePaths();

    return rc;
}

//-----------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManagerProtobuf::DiscoverConnections(const vector<TestDevicePtr> &devices)
{
    RC rc;

    for (auto pLwrDev : devices)
    {
        UINT32 topoId = 0;
        if (GetTopologyId(pLwrDev, &topoId) == OK)
        {
            CHECK_RC(pLwrDev->GetInterface<LwLink>()->ConfigurePorts(topoId));
        }
    }

    auto pFMLibIf = TopologyManager::GetFMLibIf();
    CHECK_RC(pFMLibIf->InitializeAllLwLinks());
    CHECK_RC(pFMLibIf->DiscoverAllLwLinks());

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManagerProtobuf::TrainLinks(const vector<TestDevicePtr> &devices, bool * pbAllLinksTrained)
{
    MASSERT(pbAllLinksTrained);

    RC rc;

    bool offBug = false;
    for (auto pDev : devices)
    {
        if (pDev->HasBug(2700543))
        {
            offBug = true;
            break;
        }
    }

    if (!offBug)
    {
        CHECK_RC(TopologyManager::GetFMLibIf()->TrainLwLinkConns(FMLibInterface::LWLINK_TRAIN_OFF_TO_SAFE));
    }
    CHECK_RC(TopologyManager::GetFMLibIf()->TrainLwLinkConns(FMLibInterface::LWLINK_TRAIN_SAFE_TO_HIGH));

    CHECK_RC(TopologyManager::GetFMLibIf()->ValidateLwLinksTrained(pbAllLinksTrained));
    return rc;
}

//-----------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManagerProtobuf::ReinitFailedLinks()
{
    return TopologyManager::GetFMLibIf()->ReinitFailedLinks();
}

//-----------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManagerProtobuf::ShutdownLinks(const vector<TestDevicePtr> &devices)
{
    RC rc;

    for (auto pDev : devices)
        if (pDev->HasBug(2700543))
            return RC::OK;

    CHECK_RC(TopologyManager::GetFMLibIf()->TrainLwLinkConns(FMLibInterface::LWLINK_TRAIN_TO_OFF));

    return rc;
}

//-----------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManagerProtobuf::ResetTopology(const vector<TestDevicePtr> &devices)
{
    StickyRC rc;

    for (auto pDev : devices)
    {
        if (pDev->IsEmulation())
            continue;

        if (pDev->HasBug(3464731))
            continue;

        rc = pDev->GetInterface<LwLink>()->ResetTopology();
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Return whether this topology manager implementation should be used
//!
//! \return True if this topology manager implementation should be used
//!
/* virtual */ bool  LwLinkDevIf::TopologyManagerProtobuf::ShouldUse() const
{

    // Only use the protobuf implementation if necessary.  If the
    // topology has been forced to auto detect or it can be automatically
    // detected, then do so
    if ((GetTopologyMatch() == TM_DEFAULT) && (GetTopologyFile() == ""))
    {
        return false;
    }

    TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
    MASSERT(pTestDeviceMgr);
    if (pTestDeviceMgr->NumDevicesType(TestDevice::TYPE_LWIDIA_LWSWITCH) == 0)
    {
        return false;
    }

    if (!TopologyManager::GetFMLibIf())
        return false;

    return true;
}

//------------------------------------------------------------------------------
//! \brief Shutdown all contained LwLink Devices
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC LwLinkDevIf::TopologyManagerProtobuf::Shutdown()
{
    StickyRC rc = LwLinkDevIf::TopologyManagerImpl::Shutdown();

    m_TopologyMap.clear();

    m_bInitialized = false;
    return rc;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManagerProtobuf::AssignDeviceToTopology
(
    TestDevicePtr  pDev
   ,UINT32         topologyIndex
   ,bool         * pbTopologyMismatch
)
{
    RC rc;
    UINT32 foundTopoId;

    if (OK != GetTopologyId(pDev, &foundTopoId))
    {
        TestDevice::Type devType = pDev->GetType();
        if (devType == TestDevice::TYPE_TREX)
            devType = TestDevice::TYPE_LWIDIA_GPU;

        DeviceToTopologyIdMiEntry miEntry(pDev, topologyIndex);
        m_TopologyMap[devType].push_back(miEntry);
    }
    else if (foundTopoId != topologyIndex)
    {
        // If the topology ID has already been assigned, then ensure that this would
        // simply reassing the same index.  If this would assign a different index
        // then it is an error if topology is not being forced.  For forced topologies
        // allow the mismatch to happen (without assignment), print a message and
        // keep track that the topology was mismatched so it can be further reported
        // later and any forced devices assigned.
        const bool bForcedTopology = (GetTopologyMatch() == TM_FILE || GetTopologyMatch() == TM_MIN);
        const Tee::Priority pri = bForcedTopology ? Tee::PriWarn : Tee::PriError;
        Printf(pri, GetTeeModule()->GetCode(),
               "%s : %s already assigned topology id %u!\n",
               __FUNCTION__, pDev->GetName().c_str(), foundTopoId);
        *pbTopologyMismatch = true;
        if (!bForcedTopology)
        {
            SCOPED_DEV_INST(pDev.get());
            ByteStream bs;
            auto entry = Mle::Entry::lwlink_mismatch(&bs);
            entry.lwlink_topo_mismatch()
                 .reason(ProtoMismatchReason::multiple_topo_ids_assigned);
            entry.Finish();
            Tee::PrintMle(&bs);
            return RC::LWLINK_TOPOLOGY_MISMATCH;
        }
    }
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManagerProtobuf::AssignUniqueDeviceToToplogy
(
    const vector<TestDevicePtr> &devices
   ,TestDevice::Type             devType
)
{
    RC rc;
    auto pFMLibIf = TopologyManager::GetFMLibIf();

    UINT32 topoCount = 0;
    UINT32 topoId = 0;
    switch (devType)
    {
        case TestDevice::TYPE_LWIDIA_GPU:
            topoCount = pFMLibIf->GetNumGpus();
            if (topoCount > 0)
                CHECK_RC(pFMLibIf->GetGpuPhysicalId(0, &topoId));
            break;
        case TestDevice::TYPE_LWIDIA_LWSWITCH:
            topoCount = pFMLibIf->GetNumLwSwitches();
            if (topoCount > 0)
                CHECK_RC(pFMLibIf->GetLwSwitchPhysicalId(0, &topoId));
            break;
        default:
            Printf(Tee::PriError, GetTeeModule()->GetCode(),
                   "%s : Invalid device type %d\n", __FUNCTION__, devType);
            return RC::SOFTWARE_ERROR;
    }

    if (topoCount != 1)
        return RC::OK;

    auto ppDevBegin = lower_bound(devices.begin(), devices.end(), devType, s_LbDeviceTypeLambda); //$
    auto ppDevEnd   = upper_bound(devices.begin(), devices.end(), devType, s_UbDeviceTypeLambda); //$

    // Dont try to reassigning a unique device if it was already assigned (it would only
    // have one possible assignment in any case)
    if ((distance(ppDevBegin, ppDevEnd) == 1) &&
        (RC::OK != GetTopologyId(*ppDevBegin, nullptr)))
    {
        bool bTopologyMismatch = false;
        CHECK_RC(AssignDeviceToTopology(*ppDevBegin, topoId, &bTopologyMismatch));

        if (bTopologyMismatch)
            return RC::LWLINK_TOPOLOGY_MISMATCH;
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManagerProtobuf::CheckDeviceCount
(
    const vector<TestDevicePtr> &devices
   ,TestDevice::Type             devType
)
{
    if (TrexFlagSet() && devType == TestDevice::TYPE_LWIDIA_GPU)
        devType = TestDevice::TYPE_TREX;

    UINT32 devCount = 0;
    for (const auto & lwrDev : devices)
    {
        if (lwrDev->GetType() == devType)
            devCount++;
    }

    string typeStr;
    UINT32 topoCount;
    switch (devType)
    {
        case TestDevice::TYPE_LWIDIA_GPU:
        case TestDevice::TYPE_TREX:
            typeStr = "GPU";
            topoCount = TopologyManager::GetFMLibIf()->GetNumGpus();
            break;
        case TestDevice::TYPE_LWIDIA_LWSWITCH:
            typeStr = "LWSWITCH";
            topoCount = TopologyManager::GetFMLibIf()->GetNumLwSwitches();
            break;
        default:
            Printf(Tee::PriError, GetTeeModule()->GetCode(),
                   "%s : Invalid device type %d\n", __FUNCTION__, devType);
            return RC::SOFTWARE_ERROR;
    }

    if (devCount != topoCount)
    {
        const bool bForced = (GetTopologyMatch() == TM_FILE || GetTopologyMatch() == TM_MIN);
        Printf(!bForced ? Tee::PriError : Tee::PriWarn,
               GetTeeModule()->GetCode(),
               "%s : %s device count mismatch, found %u, topology file requires %d\n",
               __FUNCTION__, typeStr.c_str(), devCount, topoCount);
        if (!bForced)
            return RC::BAD_PARAMETER;
    }
    return OK;
}

//------------------------------------------------------------------------------
bool LwLinkDevIf::TopologyManagerProtobuf::AllDevicesMapped()
{
    return AllDevicesMapped(TestDevice::TYPE_LWIDIA_GPU) &&
           AllDevicesMapped(TestDevice::TYPE_LWIDIA_LWSWITCH);
}

//------------------------------------------------------------------------------
bool LwLinkDevIf::TopologyManagerProtobuf::AllDevicesMapped(TestDevice::Type type)
{
    const size_t assignedDevs = m_TopologyMap[type].size();

    switch (type)
    {
        case TestDevice::TYPE_LWIDIA_GPU:
            return assignedDevs == static_cast<size_t>(TopologyManager::GetFMLibIf()->GetNumGpus());
        case TestDevice::TYPE_LWIDIA_LWSWITCH:
            return assignedDevs == static_cast<size_t>(TopologyManager::GetFMLibIf()->GetNumLwSwitches());
        default:
            return false;
    }
}

//------------------------------------------------------------------------------
//! \brief Construct all routes in the system starting at the source device
//! targeting the specific fabric address for the data type
//!
//! \param devices       : Vector of devices in the system
//! \param pSrcDev       : Starting device for the routes
//! \param srcEndpoints  : Map of discovered endpoints for the source device
//! \param fabricAddress : Fabric base address of the traffic for the routes
//! \param dataType      : Type of data for the routes
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::TopologyManagerProtobuf::ConstructRoutes
(
    const vector<TestDevicePtr> &devices
   ,TestDevicePtr                pSrcDev
   ,const EndpointMapEntry      &srcEndpoints
   ,UINT64                       fabricBase
   ,LwLink::DataType             dataType
)
{
    RC rc;

    // Create the fabric address and data type specific connections
    vector<LwLinkConnectionPtr> srcConnections;
    CHECK_RC(CreateSourceConnections(pSrcDev,
                                     srcEndpoints,
                                     fabricBase,
                                     dataType,
                                     &srcConnections));

    for (auto const &pCon : srcConnections)
    {
        UINT64 srcLinkMask = pCon->GetLocalLinkMask(pSrcDev);

        LwLinkPath path(fabricBase, dataType);
        if (!path.AddEntry(pCon, pSrcDev, srcLinkMask))
        {
            Printf(Tee::PriError, GetTeeModule()->GetCode(),
                   "%s : Couldnt add initial path entry!", __FUNCTION__);
            return RC::SOFTWARE_ERROR;
        }

        TestDevicePtr pRemoteDev = pCon->GetRemoteDevice(pSrcDev);
        vector<LwLinkConnection::EndpointIds> epIds = pCon->GetLinks(pSrcDev);
        vector<UINT32> remEndpoints(epIds.size());
        transform(epIds.begin(), epIds.end(), remEndpoints.begin(), GetRemoteEndpoints);
        CHECK_RC(DoConstructRoutes(devices,
                                   pRemoteDev,
                                   remEndpoints,
                                   path));
    }
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Create all the connections on a source device. The connection list may be empty
//! if the source device cannot communicate with the fabric base / data type
//!
//! \param pSrcDev      : Source device to create connections on
//! \param srcEndpoints : Map of discovered endpoints for the source device
//! \param fabricBase   : Fabric base address of the traffic for the connections
//! \param dataType     : Type of data for the connections
//! \param pConns       : vector of discovered connections.
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::TopologyManagerProtobuf::CreateSourceConnections
(
    TestDevicePtr           pSrcDev
   ,const EndpointMapEntry &srcEndpoints
   ,UINT64                  fabricBase
   ,LwLink::DataType        dataType
   ,vector<LwLinkConnectionPtr>* pConns
)
{
    MASSERT(pConns);
    RC rc;

    // Iterate over the discovered srcEndpoints which are a map of remote device
    // and vector of endpoints that are connected to the remote device
    for (auto const & lwrDevEndpoint : srcEndpoints)
    {
        // Each set of endpoints could potentially be broken up into multiple
        // seperate endpoint vectors depending upon how the endpoints are
        // routed through the bridge device
        //
        // Each entry in this vector contains a list of endpoints that share
        // identical internal routing on the bridge device
        vector<vector<LwLinkConnection::EndpointIds>> connectionEndpoints;

        // Iterate through all the endpoints between the source device and
        // remote device
        for (auto const & lwrEndpoint : lwrDevEndpoint.second)
        {
            // lwrDevEndpoint.first is the remote device (i.e. bridge device)
            // lwrEndpoint.second is the link number on the remote device
            //
            // Determine where the input link on the remote (bridge) device will
            // be routed to based on the fabric base and data type

            set<UINT32> lwrOutputLinks;
            CHECK_RC(lwrDevEndpoint.first->GetInterface<LwLink>()->GetOutputLinks(lwrEndpoint.second,
                                                                                  fabricBase,
                                                                                  dataType,
                                                                                  &lwrOutputLinks));

            bool bEndpointAdded = false;

            // Group endpoints by routability.  Iterate through all lwrrently
            // discovered endpoint vectors and if the current output links are
            // identical an entry in the existing endpoint vector, then add the
            // current endpoint to that group of endpoints that can be routed as
            // a unit
            for (auto & foundEndpoints : connectionEndpoints)
            {
                set<UINT32> foundOutputLinks;
                CHECK_RC(lwrDevEndpoint.first->GetInterface<LwLink>()->GetOutputLinks(foundEndpoints[0].second,
                                                                                      fabricBase,
                                                                                      dataType,
                                                                                      &foundOutputLinks));

                if (lwrOutputLinks == foundOutputLinks)
                {
                    foundEndpoints.push_back(lwrEndpoint);
                    bEndpointAdded = true;
                    break;
                }
            }

            // If the endpoint wasnt added, then create a new vector group of
            // routable endpoints containing only the new endpoint
            if (!bEndpointAdded)
            {
                connectionEndpoints.push_back({ lwrEndpoint });
            }
        }

        // For all discovered sets of endpoints that are routable as a unit,
        // create a connection
        for (auto lwrEndpoints : connectionEndpoints)
        {
            LwLinkConnectionPtr newCon(new LwLinkConnection(pSrcDev,
                                                            lwrDevEndpoint.first,
                                                            lwrEndpoints));
            pConns->push_back(AddConnection(newCon));
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Create a map of source/remote devices and their endpoints
//!
//! \param devices         : Vector of devices in the system
//! \param pSrcEndpointMap : Returned 2D map of source devices to their remote
//!                          devices and list of endpoints
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::TopologyManagerProtobuf::CreateSourceEndpointMap
(
    const vector<TestDevicePtr> &devices
   ,EndpointMap                 *pSrcEndpointMap
) const
{
    RC rc;

    for (auto const & pSwitchDev : devices)
    {
        // Protobuf topology determination is switch centric
        if (pSwitchDev->GetType() != TestDevice::TYPE_LWIDIA_LWSWITCH)
            continue;

        auto pLwLink = pSwitchDev->GetInterface<LwLink>();
        for (UINT32 lwrLink = 0; lwrLink < pLwLink->GetMaxLinks(); lwrLink++)
        {
            LwLinkConnection::EndpointIds linkEndpoints(LwLink::ILWALID_LINK_ID,
                                                        lwrLink);

            // If the link is active fill in the remote endpoint info
            if (pLwLink->IsLinkActive(lwrLink))
            {
                TestDevicePtr pRemoteDev;
                LwLink::EndpointInfo endInfo;

                CHECK_RC(pLwLink->GetRemoteEndpoint(lwrLink, &endInfo));
                CHECK_RC(GetDeviceById(devices, endInfo.id, &pRemoteDev));

                // This is only a source map so non source links i.e. trunk
                // connections can be skipped
                if (!pRemoteDev->IsEndpoint())
                    continue;

                CHECK_RC(pRemoteDev->GetInterface<LwLink>()->GetLinkId(endInfo, &linkEndpoints.first));

                // If the remote device doesnt already have an entry in the
                // source endpoint map then add a new entry
                if (!pSrcEndpointMap->count(pRemoteDev))
                {
                    EndpointMapEntry newEntry;
                    newEntry[pSwitchDev] = { linkEndpoints };
                    pSrcEndpointMap->emplace(pRemoteDev, newEntry);
                }
                else
                {
                    EndpointMapEntry &mapEntry = pSrcEndpointMap->at(pRemoteDev);
                    // If there is already an entry in the remote map but not
                    // one connected to the current switch, then add a new entry
                    if (!mapEntry.count(pSwitchDev))
                    {
                        mapEntry[pSwitchDev] = { linkEndpoints };
                    }
                    else
                    {
                        // Otherwise add to an existing entry
                        mapEntry[pSwitchDev].push_back(linkEndpoints);
                    }
                }
            }
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Relwrsively contruct the routes starting with the specified path and
//! attempting to add the source device and source links to the path
//!
//! \param devices : Vector of devices in the system
//! \param pSrcDev : Source device for the route
//!
//! \return OK if successful, not OK otherwise
//!
RC  LwLinkDevIf::TopologyManagerProtobuf::DoConstructRoutes
(
    const vector<TestDevicePtr> &devices
   ,TestDevicePtr                pSrcDev
   ,vector<UINT32>               srcLinks
   ,LwLinkPath                   path
)
{
    MASSERT(!pSrcDev->IsEndpoint());

    RC rc;

    auto pLwLink = pSrcDev->GetInterface<LwLink>();

    // Iterate over the source links into the device, determine where each route
    // to internally and create the necessary LwLinkConnections for the outputs
    for (auto const  &srcLink : srcLinks)
    {
        // Get the
        set<UINT32> outputLinks;
        CHECK_RC(pSrcDev->GetInterface<LwLink>()->GetOutputLinks(srcLink,
                                                                 path.GetFabricBase(),
                                                                 path.GetDataType(),
                                                                 &outputLinks));

        // Each lwlink device can have multiple arrays of endpoint Ids connected
        // Each array of endpoint ids represents a gang
        map<TestDevicePtr, vector<vector<LwLinkConnection::EndpointIds>>> remCons;
        for (auto const &lwrLink : outputLinks)
        {
            LwLinkConnection::EndpointIds linkEndpoints(lwrLink,
                                                        LwLink::ILWALID_LINK_ID);

            // If the link is active fill in the remote link information and
            // store the endpoints
            if (pLwLink->IsLinkActive(lwrLink))
            {
                TestDevicePtr pRemoteDev;
                LwLink::EndpointInfo endInfo;

                CHECK_RC(pLwLink->GetRemoteEndpoint(lwrLink, &endInfo));
                CHECK_RC(GetDeviceById(devices, endInfo.id, &pRemoteDev));

                CHECK_RC(pRemoteDev->GetInterface<LwLink>()->GetLinkId(endInfo, &linkEndpoints.second));

                // If this is the first connection to the remote device create a new entry for
                // it with the current endpoints
                if (!remCons.count(pRemoteDev))
                {
                    remCons[pRemoteDev] = { { linkEndpoints } };
                }
                else if (pRemoteDev->IsEndpoint())
                {
                    // Otherwise if this is an endpoint, then simply add the current endpoints
                    // to the single list (all connections to an endpoint type device can be
                    // treated as a gang
                    remCons[pRemoteDev][0].push_back(linkEndpoints);
                }
                else
                {
                    // Otherwise the remote device is a switch type device.  For a switch
                    // type remote device only group incoming links together if they are
                    // routed identically within the switch
                    set<UINT32> newRemoteOutputLinks;
                    CHECK_RC(pRemoteDev->GetInterface<LwLink>()->GetOutputLinks(linkEndpoints.second,
                                                                                path.GetFabricBase(),
                                                                                path.GetDataType(),
                                                                                &newRemoteOutputLinks));
                    bool bAddedEndpoint = false;
                    for (auto & lwrEndpoints : remCons[pRemoteDev])
                    {
                        set<UINT32> lwrRemoteOutputLinks;
                        CHECK_RC(pRemoteDev->GetInterface<LwLink>()->GetOutputLinks(lwrEndpoints[0].second,
                                                                                    path.GetFabricBase(),
                                                                                    path.GetDataType(),
                                                                                    &lwrRemoteOutputLinks));

                        // If the current incoming endpoint is routed the same as an existing
                        // set of incoming endpoints, then add the current endpoint to an
                        // existing array
                        if (newRemoteOutputLinks == lwrRemoteOutputLinks)
                        {
                            lwrEndpoints.push_back(linkEndpoints);
                            bAddedEndpoint = true;
                            break;
                        }
                    }

                    // Otherwise the current endpoint couldnt be added to an existing array so
                    // create a new array for the current endpoint
                    if (!bAddedEndpoint)
                    {
                        remCons[pRemoteDev].emplace_back(1, linkEndpoints);
                    }
                }
            }
        }

        // Iterate over the discovered connections on the output side of the
        // device and create a connection for each, add it to the current path
        // and if the remote device is an endpoint add the complete path to a
        // route, otherwise relwrsively call this function until it terminates
        // in an endpoint
        for (auto const & lwrConSet : remCons)
        {
            for (auto const & lwrCon : lwrConSet.second)
            {
                // Create and add a new connection
                LwLinkConnectionPtr pNewCon(new LwLinkConnection(pSrcDev,
                                                                 lwrConSet.first,
                                                                 lwrCon));
                pNewCon = AddConnection(pNewCon);

                // Create a copy of the current path so that it isnt modified
                LwLinkPath lwrPath = path;

                // Create the source link mask explicitly here rather than using
                // pNewCon->GetLinkMask() since it is explicitly known which links
                // to use in the case of a loopout link
                UINT64 srcLinkMask = 0ULL;
                for (auto const & lwrEp : lwrCon)
                    srcLinkMask |= (1ULL << lwrEp.first);

                // The connection must be able to be added
                if (!lwrPath.AddEntry(pNewCon, pSrcDev, srcLinkMask))
                {
                    Printf(Tee::PriError, GetTeeModule()->GetCode(),
                           "%s : Couldnt add new path entry!\n", __FUNCTION__);
                    return RC::SOFTWARE_ERROR;
                }

                // Either terminate the path (if at an endpoint) or continue to
                // add devices
                if (lwrConSet.first->IsEndpoint())
                {
                    bool bPathValid;
                    CHECK_RC(lwrPath.Validate(&bPathValid));
                    if (bPathValid)
                    {
                        CHECK_RC(AddPathToRoute(lwrPath));
                    }
                }
                else
                {
                    vector<UINT32> remEndpoints(lwrCon.size());
                    transform(lwrCon.begin(),
                              lwrCon.end(),
                              remEndpoints.begin(),
                              GetRemoteEndpoints);
                    CHECK_RC(DoConstructRoutes(devices, lwrConSet.first, remEndpoints, lwrPath));
                }
            }
        }
    }
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManagerProtobuf::MapDevicesToTopologyTrivial
(
    const vector<TestDevicePtr> &devices
)
{
    if (AllDevicesMapped())
        return RC::OK;

    RC rc;
    CHECK_RC(AssignUniqueDeviceToToplogy(devices, TestDevice::TYPE_LWIDIA_GPU));
    CHECK_RC(AssignUniqueDeviceToToplogy(devices, TestDevice::TYPE_LWIDIA_LWSWITCH));

    // Assign TREX Devices
    UINT32 trexDevs = std::count_if(devices.begin(), devices.end(), [](auto dev){ return dev->GetType() == TestDevice::TYPE_TREX; });
    if (trexDevs > 0)
    {
        for (size_t ii = 0; ii < devices.size(); ii++)
        {
            if (devices[ii]->GetType() != TestDevice::TYPE_TREX)
                continue;

            UINT16 deviceNum = _UINT16_MAX;
            devices[ii]->GetId().GetPciDBDF(nullptr, nullptr, &deviceNum, nullptr);

            bool bTopologyMismatch = false;
            CHECK_RC(AssignDeviceToTopology(devices[ii], deviceNum, &bTopologyMismatch));

            if (bTopologyMismatch)
                return RC::LWLINK_TOPOLOGY_MISMATCH;
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManagerProtobuf::MapDevicesToTopologyForced
(
    const vector<TestDevicePtr> &devices
)
{
    RC rc;
    auto pFMLibIf = TopologyManager::GetFMLibIf();

    if (AllDevicesMapped())
        return RC::OK;

    if (GetTopologyMatch() != TM_FILE && GetTopologyMatch() != TM_MIN)
        return RC::OK;

    const bool bMatchMin = (GetTopologyMatch() == TM_MIN);
    const Tee::Priority pri = (bMatchMin) ? Tee::PriWarn : Tee::PriError;

    UINT32 numGpus = pFMLibIf->GetNumGpus();
    for (UINT32 gpuIdx = 0; gpuIdx < numGpus; gpuIdx++)
    {
        UINT32 physicalId = ~0U;
        CHECK_RC(pFMLibIf->GetGpuPhysicalId(gpuIdx, &physicalId));

        TopoIdToDevice &idToDev = m_TopologyMap[TestDevice::TYPE_LWIDIA_GPU].get<topo_id_index>();

        if (idToDev.find(physicalId) == idToDev.end())
        {
            bool bFoundDev = false;
            DeviceToTopoId &devToId = m_TopologyMap[TestDevice::TYPE_LWIDIA_GPU].get<dev_index>();

            // Loop through all devices until a GPU device is found that is not
            // already in the topology map and assign it to the ID without
            // an entry
            for (size_t lwrGpu = 0; (lwrGpu < devices.size()) && !bFoundDev; lwrGpu++)
            {
                if (devices[lwrGpu]->GetType() != TestDevice::TYPE_LWIDIA_GPU)
                    continue;

                if (devToId.find(devices[lwrGpu]) == devToId.end())
                {
                    DeviceToTopologyIdMiEntry miEntry(devices[lwrGpu], physicalId);
                    m_TopologyMap[TestDevice::TYPE_LWIDIA_GPU].push_back(miEntry);

                    Printf(Tee::PriLow, GetTeeModule()->GetCode(),
                           "%s : Forcing GPU topology ID %u to %s\n",
                                       __FUNCTION__,
                                       physicalId,
                                       devices[lwrGpu]->GetName().c_str());

                    bFoundDev = true;
                }
            }

            // For forced configurations, all entries in the topology map must
            // have a device assigned
            if (!bFoundDev)
            {
                Printf(pri, GetTeeModule()->GetCode(),
                       "%s : Unable to assign device to GPU topology ID %d!\n",
                       __FUNCTION__, physicalId);
                if (!bMatchMin)
                    return RC::DEVICE_NOT_FOUND;
            }
        }
    }

    UINT32 numSwitches = pFMLibIf->GetNumLwSwitches();
    for (UINT32 switchIdx = 0; switchIdx < numSwitches; switchIdx++)
    {
        UINT32 physicalId = ~0U;
        CHECK_RC(pFMLibIf->GetLwSwitchPhysicalId(switchIdx, &physicalId));

        TopoIdToDevice &idToDev =
            m_TopologyMap[TestDevice::TYPE_LWIDIA_LWSWITCH].get<topo_id_index>();

        if (idToDev.find(physicalId) == idToDev.end())
        {
            bool bFoundDev = false;
            DeviceToTopoId &devToId =
                m_TopologyMap[TestDevice::TYPE_LWIDIA_LWSWITCH].get<dev_index>();

            // Loop through all devices until an LwSwitch device is found that is not
            // already in the topology map and assign it to the ID without
            // an entry
            for (size_t lwrSwitch = 0; (lwrSwitch < devices.size()) && !bFoundDev; lwrSwitch++)
            {
                if (devices[lwrSwitch]->GetType() != TestDevice::TYPE_LWIDIA_LWSWITCH)
                    continue;

                if (devToId.find(devices[lwrSwitch]) == devToId.end())
                {
                    DeviceToTopologyIdMiEntry miEntry(devices[lwrSwitch], physicalId);
                    m_TopologyMap[TestDevice::TYPE_LWIDIA_LWSWITCH].push_back(miEntry);

                    Printf(Tee::PriLow, GetTeeModule()->GetCode(),
                           "%s : Forcing Switch topology ID %u to %s\n",
                                       __FUNCTION__,
                                       static_cast<UINT32>(physicalId),
                                       devices[lwrSwitch]->GetName().c_str());

                    bFoundDev = true;
                }
            }

            // For forced configurations, all entries in the topology map must
            // have a device assigned
            if (!bFoundDev)
            {
                Printf(pri, GetTeeModule()->GetCode(),
                       "%s : Unable to assign device to LwSwitch topology ID %d!\n",
                       __FUNCTION__, physicalId);
                if (!bMatchMin)
                    return RC::DEVICE_NOT_FOUND;
            }
        }
    }

    return RC::OK;
}

RC LwLinkDevIf::TopologyManagerProtobuf::MapDevicesToTopologyPciInfo
(
    const vector<TestDevicePtr> &devices
)
{
    RC rc;

    if (AllDevicesMapped())
        return RC::OK;

    auto pFMLibIf = TopologyManager::GetFMLibIf();

    UINT32 numGpus = pFMLibIf->GetNumGpus();
    for (UINT32 i = 0; i < numGpus; i++)
    {
        UINT32 physicalId = ~0U;
        if (RC::OK != pFMLibIf->GetGpuPhysicalId(i, &physicalId))
            continue;

        TestDevice::Id gpuId;
        if (RC::OK != pFMLibIf->GetGpuId(physicalId, &gpuId))
            continue;

        TestDevicePtr pDev;
        if (RC::OK == GetDeviceById(devices, gpuId, &pDev))
        {
            bool bTopologyMismatch = false;
            CHECK_RC(AssignDeviceToTopology(pDev, physicalId, &bTopologyMismatch));

            if (bTopologyMismatch)
                return RC::LWLINK_TOPOLOGY_MISMATCH;
        }
    }

    UINT32 numSwitches = pFMLibIf->GetNumLwSwitches();
    for (UINT32 i = 0; i < numSwitches; i++)
    {
        UINT32 physicalId = ~0U;
        if (RC::OK != pFMLibIf->GetLwSwitchPhysicalId(i, &physicalId))
            continue;

        TestDevice::Id switchId;
        if (RC::OK != pFMLibIf->GetLwSwitchId(physicalId, &switchId))
            continue;

        TestDevicePtr pDev;
        if (RC::OK == GetDeviceById(devices, switchId, &pDev))
        {
            bool bTopologyMismatch = false;
            CHECK_RC(AssignDeviceToTopology(pDev, physicalId, &bTopologyMismatch));

            if (bTopologyMismatch)
                return RC::LWLINK_TOPOLOGY_MISMATCH;
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManagerProtobuf::DoSwitchPortsMatch
(
    TestDevicePtr                        pDev
   ,const vector<TestDevicePtr>        & devices
   ,bool                               * pbSwitchMatches
)
{
    RC rc;

    const bool bForcedTopology = (GetTopologyMatch() == TM_FILE || GetTopologyMatch() == TM_MIN);
    auto pLwLink = pDev->GetInterface<LwLink>();

    vector<LwLink::EndpointInfo> detectedEpInfo;
    CHECK_RC(pLwLink->GetDetectedEndpointInfo(&detectedEpInfo));

    vector<LwLink::EndpointInfo> topoEpInfo;
    CHECK_RC(GetEndpointInfo(pDev, &topoEpInfo));

    MASSERT(detectedEpInfo.size() == topoEpInfo.size());

    // Validate that all ports on the switch with the specified ID match the
    // endpoint information in the provided vector
    *pbSwitchMatches = true;
    for (UINT32 lwrLink = 0; lwrLink < detectedEpInfo.size(); lwrLink++)
    {
        const LwLink::EndpointInfo& detectedEp = detectedEpInfo[lwrLink];
        const LwLink::EndpointInfo& topoEp = topoEpInfo[lwrLink];

        TestDevicePtr pTopoRemoteDev;
        if (topoEp.id != TestDevice::Id())
        {
            rc = GetDeviceById(devices, topoEp.id, &pTopoRemoteDev);
        }
        if (rc != RC::OK)
        {
            Printf(Tee::PriError, GetTeeModule()->GetCode(),
                   "%s : %s unable to find remote device ID %s"
                   " at port %u\n",
                   __FUNCTION__,
                   pDev->GetName().c_str(),
                   topoEp.id.GetString().c_str(),
                   lwrLink);
            if (!bForcedTopology)
            {
                SCOPED_DEV_INST(pDev.get());
                UINT16 domain = 0;
                UINT16 bus = 0;
                UINT16 dev = 0;
                UINT16 func = 0;
                detectedEp.id.GetPciDBDF(&domain, &bus, &dev, &func);
                const UINT64 pciDBDF = (static_cast<UINT64>(domain) << 48) |
                                       (static_cast<UINT64>(bus)    << 32) |
                                       (static_cast<UINT64>(dev)    << 16) |
                                       (static_cast<UINT64>(func));
                ByteStream bs;
                auto entry = Mle::Entry::lwlink_mismatch(&bs);
                entry.lwlink_topo_mismatch()
                     .reason(ProtoMismatchReason::peer_device_not_found)
                     .lwlink_id(lwrLink)
                     .peer_pci_dbdf(pciDBDF);
                entry.Finish();
                Tee::PrintMle(&bs);
            }
            return rc;
        }

        if (topoEp.devType == TestDevice::TYPE_TREX)
            continue;

        TestDevicePtr pDetectedRemoteDev;
        if (detectedEp.id != TestDevice::Id())
        {
            rc = GetDeviceById(devices, detectedEp.id, &pDetectedRemoteDev);
        }
        if (rc != RC::OK)
        {
            Printf(Tee::PriError, GetTeeModule()->GetCode(),
                   "%s : %s unable to find remote device ID %s"
                   " at port %u\n",
                   __FUNCTION__,
                   pDev->GetName().c_str(),
                   detectedEp.id.GetString().c_str(),
                   lwrLink);
            return rc;
        }

        if (detectedEp.devType != topoEp.devType
            || detectedEp.linkNumber != topoEp.linkNumber
            || pDetectedRemoteDev != pTopoRemoteDev)
        {
            *pbSwitchMatches = false;

            string mismatchStr = Utility::StrPrintf("%s : %s mismatch at port %u\n",
                                                    __FUNCTION__,
                                                    pDev->GetName().c_str(),
                                                    lwrLink);

            if (detectedEp.devType == TestDevice::TYPE_UNKNOWN)
                mismatchStr += "    Detected no connection\n";
            else
                mismatchStr += Utility::StrPrintf("    Detected remote type %s, remote port %u",
                                           TestDevice::TypeToString(detectedEp.devType).c_str(),
                                           detectedEp.linkNumber);

            if (pDetectedRemoteDev)
                mismatchStr += ", remote device " + pDetectedRemoteDev->GetName();
            mismatchStr += "\n";

            if (topoEp.devType == TestDevice::TYPE_UNKNOWN)
                mismatchStr += "    Expected no connection\n";
            else
                mismatchStr += Utility::StrPrintf("    Expected remote type %s, remote port %u",
                                           TestDevice::TypeToString(topoEp.devType).c_str(),
                                           topoEp.linkNumber);
            if (pTopoRemoteDev)
                mismatchStr += ", remote device " + pTopoRemoteDev->GetName();
            mismatchStr += "\n";

            vector<LwLinkCableController::GradingValues> gradingVals;
            if (detectedEp.devType == TestDevice::TYPE_UNKNOWN
                && topoEp.devType != TestDevice::TYPE_UNKNOWN
                && pLwLink->SupportsInterface<LwLinkCableController>())
            {
                auto pLwlCC = pLwLink->GetInterface<LwLinkCableController>();
                const UINT32 cciId = pLwlCC->GetCciIdFromLink(lwrLink);
                if (cciId != LwLinkCableController::ILWALID_CCI_ID)
                {
                    CHECK_RC(pLwlCC->GetGradingValues(cciId, &gradingVals));
                    mismatchStr +=
                        LwLinkCableController::LinkGradingValuesToString(lwrLink, gradingVals);
                }
            }

            Printf(Tee::PriError, GetTeeModule()->GetCode(), "%s", mismatchStr.c_str());
            if (!bForcedTopology)
            {
                SCOPED_DEV_INST(pDev.get());
                ByteStream bs;
                auto entry = Mle::Entry::lwlink_mismatch(&bs);

                {
                    auto portMismatch = entry.lwswitch_port_mismatch();
                    portMismatch.reason(ProtoMismatchReason::port_mismatch)
                                .lwlink_id(lwrLink)
                                .detected_peer(ToProtoLwlinkDeviceType(detectedEp.devType))
                                .expected_peer(ToProtoLwlinkDeviceType(topoEp.devType));

                    for (auto const & lwrGrading : gradingVals)
                    {
                        if ((lwrGrading.linkId == lwrLink) && (lwrGrading.laneMask != 0))
                        {
                            const INT32 firstLane =
                                Utility::BitScanForward(lwrGrading.laneMask);
                            const INT32 lastLane =
                                Utility::BitScanReverse(lwrGrading.laneMask);
                            for (INT32 lane = firstLane; lane <= lastLane; ++lane)
                            {
                                portMismatch.cc_lane_gradings()
                                            .lane_id(lane)
                                            .rx_init(lwrGrading.rxInit[lane])
                                            .tx_init(lwrGrading.txInit[lane])
                                            .rx_maint(lwrGrading.rxMaint[lane])
                                            .tx_maint(lwrGrading.txMaint[lane]);
                            }
                        }
                    }
                }

                entry.Finish();
                Tee::PrintMle(&bs);
            }

            if (bForcedTopology)
                return RC::OK;
        }
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
//! \brief Setup all the fabric apertures for all devices
//!
//! \param pAllFas : Returned vector of all discovered fabric apertures
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::TopologyManagerProtobuf::SetupFabricApertures
(
    vector<LwLink::FabricAperture> *pAllFas
)
{
    RC rc;

    const bool bMatchMin = (GetTopologyMatch() == TM_MIN);

    TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
    TestDevicePtr pSwitch;
    CHECK_RC(pTestDeviceMgr->GetDevice(TestDevice::TYPE_LWIDIA_LWSWITCH, 0, &pSwitch));

    auto pFMLibIf = TopologyManager::GetFMLibIf();

    // Find the fabric apertures for all GPU devices
    UINT32 numGpus = pFMLibIf->GetNumGpus();
    for (UINT32 gpuIdx = 0; gpuIdx < numGpus; gpuIdx++)
    {
        UINT32 physicalId = ~0U;
        CHECK_RC(pFMLibIf->GetGpuPhysicalId(gpuIdx, &physicalId));

        TestDevicePtr pDev = GetDevice(TestDevice::TYPE_LWIDIA_GPU, physicalId);

        if (!pDev && bMatchMin)
            continue;

        MASSERT(pDev);
        vector<LwLink::FabricAperture> fas;
        const UINT32 fabricRegionBits = pSwitch->GetInterface<LwLink>()->GetFabricRegionBits();
        const UINT64 fabricRegionSize = (1ULL << fabricRegionBits);

        if (pSwitch->GetDeviceId() == Device::SVNP01)
        {
            UINT64 numApertures = 0;
            CHECK_RC(pFMLibIf->GetFabricAddressRange(physicalId, &numApertures));
            numApertures >>= fabricRegionBits;

            UINT64 lwrBase = 0;
            CHECK_RC(pFMLibIf->GetFabricAddressBase(physicalId, &lwrBase));
            for (UINT32 lwrAperture = 0;
                  lwrAperture < numApertures;
                  lwrAperture++, lwrBase += fabricRegionSize)
            {
                fas.push_back(LwLink::FabricAperture(lwrBase, fabricRegionSize));
            }
        }
        else
        {
            UINT64 numApertures = 0;
            bool   bUseFla = true;

            // Prefer FLA over GPA for routing in MODS
            rc = pFMLibIf->GetFlaAddressRange(physicalId, &numApertures);
            if (rc == RC::ILWALID_OBJECT_PROPERTY)
            {
                rc.Clear();
                CHECK_RC(pFMLibIf->GetGpaAddressRange(physicalId, &numApertures));
                bUseFla = false;
            }
            else if (rc != RC::OK)
            {
                return rc;
            }

            numApertures >>= fabricRegionBits;

            UINT64 lwrBase = 0;
            if (bUseFla)
            {
                CHECK_RC(pFMLibIf->GetFlaAddressBase(physicalId, &lwrBase));
            }
            else
            {
                CHECK_RC(pFMLibIf->GetGpaAddressBase(physicalId, &lwrBase));
            }
            for (UINT32 lwrAperture = 0;
                  lwrAperture < numApertures;
                  lwrAperture++, lwrBase += fabricRegionSize)
            {
                fas.push_back(LwLink::FabricAperture(lwrBase, fabricRegionSize));
            }
        }

        CHECK_RC(pDev->GetInterface<LwLink>()->SetFabricApertures(fas));

        pAllFas->insert(pAllFas->end(), fas.begin(), fas.end());
    }

    return rc;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManagerProtobuf::ValidateTopologyIds
(
    const vector<TestDevicePtr> &devices
)
{
    const bool bForcedTopology = (GetTopologyMatch() == TM_FILE || GetTopologyMatch() == TM_MIN);
    const Tee::Priority pri = bForcedTopology ? Tee::PriWarn : Tee::PriError;
    StickyRC validRc;
    RC rc;

    // Ensure that all devices have been assigned a topology ID and all topology
    // ids have been assigned a device

    for (auto const & lwrDev : devices)
    {
        UINT32 topoId;
        if (OK != GetTopologyId(lwrDev, &topoId))
        {
            Printf(pri, GetTeeModule()->GetCode(),
                   "%s : %s not assigned to topology\n",
                   __FUNCTION__, lwrDev->GetName().c_str());
            SCOPED_DEV_INST(lwrDev.get());
            ByteStream bs;
            auto entry = Mle::Entry::lwlink_mismatch(&bs);
            entry.lwlink_topo_mismatch()
                 .reason(ProtoMismatchReason::device_not_assigned_topo_id);
            entry.Finish();
            Tee::PrintMle(&bs);
            validRc = RC::LWLINK_TOPOLOGY_MISMATCH;
        }
    }

    auto pFMLibIf = TopologyManager::GetFMLibIf();

    UINT32 numGpus = pFMLibIf->GetNumGpus();
    for (UINT32 gpuIdx = 0; gpuIdx < numGpus; gpuIdx++)
    {
        UINT32 physicalId = ~0U;
        CHECK_RC(pFMLibIf->GetGpuPhysicalId(gpuIdx, &physicalId));

        TestDevicePtr pDev = GetDevice(TestDevice::TYPE_LWIDIA_GPU, physicalId);
        if (!pDev)
        {
            Printf(pri, GetTeeModule()->GetCode(),
                   "%s : GPU with topology id %d not assigned a matching lwlink device\n",
                   __FUNCTION__, physicalId);
            ByteStream bs;
            auto entry = Mle::Entry::lwlink_mismatch(&bs);
            entry.lwlink_topo_mismatch()
                 .reason(ProtoMismatchReason::unmapped_topology_ids_found)
                 .unmapped_device_type(ToProtoLwlinkDeviceType(TestDevice::TYPE_LWIDIA_GPU))
                 .topology_id(gpuIdx);
            entry.Finish();
            Tee::PrintMle(&bs);
            validRc = RC::LWLINK_TOPOLOGY_MISMATCH;
        }
    }

    UINT32 numSwitches = pFMLibIf->GetNumLwSwitches();
    for (UINT32 switchIdx = 0; switchIdx < numSwitches; switchIdx++)
    {
        UINT32 physicalId = ~0U;
        CHECK_RC(pFMLibIf->GetLwSwitchPhysicalId(switchIdx, &physicalId));

        TestDevicePtr pDev = GetDevice(TestDevice::TYPE_LWIDIA_LWSWITCH, physicalId);
        if (!pDev)
        {
            Printf(pri, GetTeeModule()->GetCode(),
                   "%s : LwSwitch with topology id %d not assigned a matching lwlink device\n",
                   __FUNCTION__, physicalId);
            ByteStream bs;
            auto entry = Mle::Entry::lwlink_mismatch(&bs);
            entry.lwlink_topo_mismatch()
                 .reason(ProtoMismatchReason::unmapped_topology_ids_found)
                 .unmapped_device_type(ToProtoLwlinkDeviceType(TestDevice::TYPE_LWIDIA_LWSWITCH))
                 .topology_id(switchIdx);
            entry.Finish();
            Tee::PrintMle(&bs);
            validRc = RC::LWLINK_TOPOLOGY_MISMATCH;
        }
        else
        {
            bool bSwitchMatches = false;
            CHECK_RC(DoSwitchPortsMatch(pDev, devices, &bSwitchMatches));

            if (!bSwitchMatches)
            {
                Printf(pri, GetTeeModule()->GetCode(),
                       "%s : Switch %s assigned topology ID %u but does not match!\n",
                       __FUNCTION__,
                       pDev->GetName().c_str(),
                       physicalId);
                validRc = RC::LWLINK_TOPOLOGY_MISMATCH;
            }
        }
    }

    if ((validRc != OK) && bForcedTopology)
    {
        Printf(pri, GetTeeModule()->GetCode(),
               "%s : Ignoring topology mismatches since topology is forced\n",
               __FUNCTION__);
        validRc.Clear();
    }
    return validRc;
}

//-----------------------------------------------------------------------------
bool LwLinkDevIf::TopologyManagerProtobuf::TrexFlagSet() const
{
    return !!(GetTopologyFlags() & TF_TREX);
}
