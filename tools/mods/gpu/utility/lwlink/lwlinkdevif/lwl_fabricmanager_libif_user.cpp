/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwl_lwlink_libif_user.h"
#include "lwl_fabricmanager_libif_user.h"
#include "lwl_topology_mgr_impl.h"
#include "lwl_devif.h"
#include "lwl_devif_fact.h"
#include "lwl_topology_mgr.h"
#include "core/include/xp.h"
#include "core/include/utility.h"

#include "topology.pb.h"
#include "fabricmanager.pb.h"
#include "GlobalFabricManager.h"
#include "LocalFabricManager.h"

using namespace LwLinkDevIf;

namespace
{
    //--------------------------------------------------------------------------
    //! \brief Create a FM user library interface
    //!
    //! \return Pointer to new FM user library interface
    LwLinkDevIf::FMLibIfPtr CreateFMLibIfUser()
    {
        return make_shared<LwLinkDevIf::FMLibIfUser>();
    }

    //! Register the FM user library with the factory
    LwLinkDevIf::Factory::FMLibIfFactoryFuncRegister
        m_FMLibIfUserFact("FMLibInterface", CreateFMLibIfUser);

    //------------------------------------------------------------------------------
    RC FMIntReturnToRC(FMIntReturn_t ret)
    {
        switch (ret)
        {
            case FM_INT_ST_OK:
                return RC::OK;
            case FM_INT_ST_BADPARAM:
                return RC::ILWALID_ARGUMENT;
            default:
                Printf(Tee::PriError,
                    "Could not translate FM error code %d to a MODS error code\n",
                    ret);
                return RC::SOFTWARE_ERROR;
        }
    }
};

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::FMLibIfUser::InitializeLocalFM()
{
    if (m_bInitialized)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
                "%s : Local FabricManager core library already initialized\n",
               __FUNCTION__);
        return RC::OK;
    }

    RC rc;

    string topologyFile = TopologyManager::GetTopologyFile();
    // TODO : If topology file is "" find it within the filesystem from the
    // actual location
    if (topologyFile.empty())
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : Topology file not specified, skipping FM init.\n",
               __FUNCTION__);
        m_bInitialized = true;
        return RC::OK;
    }

    try
    {
        // Check if custom socket path is present
        string socketPathStr = TopologyManager::GetSocketPath();
        if (socketPathStr.empty())
        {
            // Create a unique socket path based on PID and update
            socketPathStr = Utility::StrPrintf("mods-fm-%d", Xp::GetPid());
        }
        char socketPath[32];
        strncpy(socketPath, socketPathStr.c_str(), sizeof(socketPath));

        LocalFmArgs_t lfm = {};
        lfm.fabricMode = 0;
        lfm.bindInterfaceIp = "127.0.0.1";
        lfm.fmStartingTcpPort = 16000;
        lfm.domainSocketPath = socketPath;
        lfm.continueWithFailures = false;
        lfm.abortLwdaJobsOnFmExit = 0; // disable RC channel recovery due to FMSession crash in RM. Ref http://lwbugs/3198694
        lfm.switchHeartbeatTimeout = 10000;
        lfm.simMode = false;           // indicates whether FM is running in FSF/Emulation environment.
#if LWCFG(GLOBAL_LWSWITCH_IMPL_LS10)
        lfm.disableLwlinkAli = 1;      // For now, by default ALI is disabled
#endif

        m_pLFM.reset(new LocalFabricManagerControl(&lfm));
    }
    catch (const exception& e)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Exception in Local Fabric Manager during initialization! : \"%s\"\n",
               __FUNCTION__, e.what());
        return RC::SOFTWARE_ERROR;
    }

    m_bInitialized = true;
    return rc;
}

/* virtual */ RC LwLinkDevIf::FMLibIfUser::InitializeGlobalFM()
{
    if (m_bInitializedGFM)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
                "%s : Global FabricManager core library already initialized\n",
               __FUNCTION__);
        return RC::OK;
    }

    RC rc;

    string topologyFile = TopologyManager::GetTopologyFile();
    // TODO : If topology file is "" find it within the filesystem from the
    // actual location
    if (topologyFile.empty())
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : Topology file not specified, skipping FM init.\n",
               __FUNCTION__);
        m_bInitializedGFM = true;
        return RC::OK;
    }

    try
    {
        // Check if custom socket path is present
        string socketPathStr = TopologyManager::GetSocketPath();
        if (socketPathStr.empty())
        {
            // Create a unique socket path based on PID and update
            socketPathStr = Utility::StrPrintf("mods-fm-%d", Xp::GetPid());
        }
        char socketPath[32];
        strncpy(socketPath, socketPathStr.c_str(), sizeof(socketPath));

        GlobalFmArgs_t gfm = {};
        gfm.fabricMode = 0;
        gfm.fmStartingTcpPort = 16000;
        gfm.fabricModeRestart = 0; // default case
        gfm.stagedInit = true; // Allows us to ilwoke LwLink discovery/training at our leisure
        gfm.domainSocketPath = socketPath;
        gfm.stateFileName = "/tmp/fabricmanager.state";
        gfm.fmLibCmdBindInterface = "127.0.0.1";
        gfm.fmLibCmdSockPath = "";
        gfm.fmLibPortNumber = 6666;
        gfm.fmBindInterfaceIp = "127.0.0.1";
        gfm.continueWithFailures = true;
        gfm.accessLinkFailureMode = 0;
        gfm.trunkLinkFailureMode = 0;
        gfm.lwswitchFailureMode = 0;
        gfm.enableTopologyValidation = true;
        gfm.gfmWaitTimeout = 10; // default time GFM waits for LFMs to come up (in seconds)
        gfm.topologyFilePath = strdup(topologyFile.c_str());
        gfm.fabricPartitionFileName = NULL;
        gfm.disableDegradedMode = true;
        gfm.disablePruning = !(TopologyManager::GetTopologyMatch() == TM_MIN);
        gfm.simMode = false;    // indicates whether FM is running in FSF/Emulation environment.
        gfm.fmLWlinkRetrainCount = -1;
#if LWCFG(GLOBAL_LWSWITCH_IMPL_LS10)
        gfm.disableLwlinkAli = 1;     // For now, by default ALI is disabled
#endif

        m_pGFM.reset(new GlobalFabricManager(&gfm));
    }
    catch (const exception& e)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Exception in Global Fabric Manager during initialization! : \"%s\"\n",
               __FUNCTION__, e.what());
        return RC::SOFTWARE_ERROR;
    }

    m_bInitializedGFM = true;
    return RC::OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::FMLibIfUser::Shutdown()
{
    RC rc;

    if (!m_bInitialized)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
                "%s : FabricManager core library not initialized, skipping shutdown\n",
               __FUNCTION__);
        return RC::OK;
    }

    try
    {
        if (m_bInitializedGFM)
        {
            m_pGFM.reset();
        }
        m_pLFM.reset();
    }
    catch (const exception& e)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "TopologyManagerProtobuf : Exception in Fabric Manager during shutdown! : \"%s\"\n", e.what());
        rc = RC::SOFTWARE_ERROR;
    }

    m_bInitialized = false;
    m_bInitializedGFM = false;
    return rc;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::FMLibIfUser::InitializeAllLwLinks()
{
    try
    {
        m_pGFM->initializeAllLWLinks();
    }
    catch (const exception& e)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Exception in Fabric Manager during LwLink Initialization! : \"%s\"\n",
               __FUNCTION__, e.what());
        return RC::SOFTWARE_ERROR;
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::FMLibIfUser::DiscoverAllLwLinks()
{
    try
    {
        m_pGFM->discoverAllLWLinks();
    }
    catch (const exception& e)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Exception in Fabric Manager during LwLink Discovery! : \"%s\"\n",
               __FUNCTION__, e.what());
        return RC::SOFTWARE_ERROR;
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::FMLibIfUser::TrainLwLinkConns(LwLinkTrainType type)
{
    FMLWLinkTrainType fmType;
    switch (type)
    {
        case FMLibInterface::LWLINK_TRAIN_OFF_TO_SAFE:
            fmType = FMLWLinkTrainType::LWLINK_TRAIN_OFF_TO_SAFE;
            break;
        case FMLibInterface::LWLINK_TRAIN_SAFE_TO_HIGH:
            fmType = FMLWLinkTrainType::LWLINK_TRAIN_SAFE_TO_HIGH;
            break;
        case FMLibInterface::LWLINK_TRAIN_TO_OFF:
            fmType = FMLWLinkTrainType::LWLINK_TRAIN_TO_OFF;
            break;
        case FMLibInterface::LWLINK_TRAIN_HIGH_TO_SAFE:
            fmType = FMLWLinkTrainType::LWLINK_TRAIN_HIGH_TO_SAFE;
            break;
        case FMLibInterface::LWLINK_TRAIN_SAFE_TO_OFF:
            fmType = FMLWLinkTrainType::LWLINK_TRAIN_SAFE_TO_OFF;
            break;
        default:
            Printf(Tee::PriError, "%s : Invalid LwLinkTrainType\n", __FUNCTION__);
            return RC::ILWALID_ARGUMENT;
    }

    try
    {
        m_pGFM->trainLWLinkConns(fmType);
    }
    catch (const exception& e)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Exception in Fabric Manager during LwLink Training! : \"%s\"\n",
               __FUNCTION__, e.what());
        return RC::SOFTWARE_ERROR;
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::FMLibIfUser::ValidateLwLinksTrained(bool *pbAllLinksTrained)
{
    MASSERT(pbAllLinksTrained);

    try
    {
        m_pFailedLinks = make_unique<GlobalFMLWLinkConnRepo>();
        *pbAllLinksTrained = m_pGFM->validateLWLinkConns(*m_pFailedLinks);
    }
    catch (const exception& e)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "Not all expected lwlink connections were trained! : \"%s\"\n",
               e.what());
        *pbAllLinksTrained = false;
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::FMLibIfUser::ReinitFailedLinks()
{
#if LWCFG(GLOBAL_FEATURE_RID72837_KT_MULTINODE)
    // Nothing to do
    if (!m_pFailedLinks ||
        (m_pFailedLinks->getIntraConnections().empty() &&
         m_pFailedLinks->getInterConnections().empty()))
    {
        return RC::OK;
    }

    try
    {
        m_pGFM->reinitLWLinks(*m_pFailedLinks);
    }
    catch (const exception& e)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Exception in Fabric Manager when retraining links! : \"%s\"\n",
               __FUNCTION__, e.what());
        return RC::SOFTWARE_ERROR;
    }
    return RC::OK;
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::FMLibIfUser::FinishInitialize()
{
    try
    {
        m_pGFM->finishInitialization();
    }
    catch (const exception& e)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Exception in Fabric Manager during Fabric Init! : \"%s\"\n",
               __FUNCTION__, e.what());
        return RC::SOFTWARE_ERROR;
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
UINT32 LwLinkDevIf::FMLibIfUser::GetNumGpus() const
{
    return m_pGFM->mpParser->gpuCfg.size();
}

//------------------------------------------------------------------------------
UINT32 LwLinkDevIf::FMLibIfUser::GetGpuMaxLwLinks(UINT32 physicalId) const
{
    UINT32 maxLink = 0;

    const TopologyLWLinkConnList& connList = m_pGFM->mpParser->lwLinkConnMap[0];
    for (const TopologyLWLinkConn& conn : connList)
    {
        if (conn.connType != ACCESS_PORT_GPU)
            continue;

        if (conn.farEnd.nodeId == 0 &&
            conn.farEnd.lwswitchOrGpuId == physicalId)
        {
            maxLink = max(maxLink, conn.farEnd.portIndex + 1);
        }
    }

    return maxLink;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::FMLibIfUser::GetGpuPhysicalId(UINT32 index, UINT32* pId) const
{
    MASSERT(pId);
    *pId = ~0U;

    if (index >= GetNumGpus())
    {
        Printf(Tee::PriError, "Invalid FM GPU Index = %d\n", index);
        return RC::ILWALID_ARGUMENT;
    }

    UINT32 gpuIdx = 0;
    for (const auto& gpuPair : m_pGFM->mpParser->gpuCfg)
    {
        if (gpuIdx == index)
        {
            *pId = gpuPair.second->gpuphysicalid();
            return RC::OK;
        }

        gpuIdx++;
    }

    return RC::DEVICE_NOT_FOUND;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::FMLibIfUser::GetGpuId(UINT32 physicalId, TestDevice::Id* pId) const
{
    MASSERT(pId);

    uint32_t enumIndex = 0;
    if (!m_pGFM->getGpuEnumIndex(0, physicalId, enumIndex))
        return RC::DEVICE_NOT_FOUND;

    FMPciInfo_t pciInfo;
    if (!m_pGFM->getGpuPciBdf(0, enumIndex, pciInfo))
        return RC::DEVICE_NOT_FOUND;

    pId->SetPciDBDF(pciInfo.domain,
                    pciInfo.bus,
                    pciInfo.device,
                    pciInfo.function);

    return RC::OK;
}

//------------------------------------------------------------------------------
UINT32 LwLinkDevIf::FMLibIfUser::GetNumLwSwitches() const
{
    return m_pGFM->mpParser->lwswitchCfg.size();
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::FMLibIfUser::GetLwSwitchPhysicalId(UINT32 index, UINT32* pId) const
{
    MASSERT(pId);
    *pId = ~0U;

    if (index >= GetNumLwSwitches())
    {
        Printf(Tee::PriError, "Invalid FM LwSwitch Index = %d\n", index);
        return RC::ILWALID_ARGUMENT;
    }

    UINT32 switchIdx = 0;
    for (const auto& gpuPair : m_pGFM->mpParser->lwswitchCfg)
    {
        if (switchIdx == index)
        {
            *pId = gpuPair.second->switchphysicalid();
            return RC::OK;
        }

        switchIdx++;
    }

    return RC::DEVICE_NOT_FOUND;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::FMLibIfUser::GetLwSwitchId(UINT32 physicalId, TestDevice::Id* pId) const
{
    MASSERT(pId);

    FMPciInfo_t pciInfo;
    if (!m_pGFM->getLWSwitchPciBdf(0, physicalId, pciInfo))
        return RC::DEVICE_NOT_FOUND;

    pId->SetPciDBDF(pciInfo.domain,
                    pciInfo.bus,
                    pciInfo.device,
                    pciInfo.function);

    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::FMLibIfUser::GetFabricAddressBase(UINT32 physicalId, UINT64* pBase) const
{
    GpuKeyType gpuKey = {};
    gpuKey.nodeId = 0;
    gpuKey.physicalId = physicalId;

    auto& gpuCfg = m_pGFM->mpParser->gpuCfg;
    if (gpuCfg.find(gpuKey) == gpuCfg.end())
        return RC::DEVICE_NOT_FOUND;

    lwswitch::gpuInfo* pGpuInfo = gpuCfg.at(gpuKey);

    *pBase = pGpuInfo->fabricaddressbase();

    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::FMLibIfUser::GetFabricAddressRange(UINT32 physicalId, UINT64* pRange) const
{
    GpuKeyType gpuKey = {};
    gpuKey.nodeId = 0;
    gpuKey.physicalId = physicalId;

    auto& gpuCfg = m_pGFM->mpParser->gpuCfg;
    if (gpuCfg.find(gpuKey) == gpuCfg.end())
        return RC::DEVICE_NOT_FOUND;

    lwswitch::gpuInfo* pGpuInfo = gpuCfg.at(gpuKey);

    *pRange = pGpuInfo->fabricaddressrange();

    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::FMLibIfUser::GetGpaAddressBase(UINT32 physicalId, UINT64* pBase) const
{
    GpuKeyType gpuKey = {};
    gpuKey.nodeId = 0;
    gpuKey.physicalId = physicalId;

    auto& gpuCfg = m_pGFM->mpParser->gpuCfg;
    if (gpuCfg.find(gpuKey) == gpuCfg.end())
        return RC::DEVICE_NOT_FOUND;

    lwswitch::gpuInfo* pGpuInfo = gpuCfg.at(gpuKey);

    if (!pGpuInfo->has_gpaaddressbase())
        return RC::ILWALID_OBJECT_PROPERTY;

    *pBase = pGpuInfo->gpaaddressbase();

    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::FMLibIfUser::GetGpaAddressRange(UINT32 physicalId, UINT64* pRange) const
{
    GpuKeyType gpuKey = {};
    gpuKey.nodeId = 0;
    gpuKey.physicalId = physicalId;

    auto& gpuCfg = m_pGFM->mpParser->gpuCfg;
    if (gpuCfg.find(gpuKey) == gpuCfg.end())
        return RC::DEVICE_NOT_FOUND;

    lwswitch::gpuInfo* pGpuInfo = gpuCfg.at(gpuKey);

    if (!pGpuInfo->has_gpaaddressrange())
        return RC::ILWALID_OBJECT_PROPERTY;

    *pRange = pGpuInfo->gpaaddressrange();

    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::FMLibIfUser::GetFlaAddressBase(UINT32 physicalId, UINT64* pBase) const
{
    GpuKeyType gpuKey = {};
    gpuKey.nodeId = 0;
    gpuKey.physicalId = physicalId;

    auto& gpuCfg = m_pGFM->mpParser->gpuCfg;
    if (gpuCfg.find(gpuKey) == gpuCfg.end())
        return RC::DEVICE_NOT_FOUND;

    lwswitch::gpuInfo* pGpuInfo = gpuCfg.at(gpuKey);

    if (!pGpuInfo->has_flaaddressbase())
        return RC::ILWALID_OBJECT_PROPERTY;

    *pBase = pGpuInfo->flaaddressbase();

    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::FMLibIfUser::GetFlaAddressRange(UINT32 physicalId, UINT64* pRange) const
{
    GpuKeyType gpuKey = {};
    gpuKey.nodeId = 0;
    gpuKey.physicalId = physicalId;

    auto& gpuCfg = m_pGFM->mpParser->gpuCfg;
    if (gpuCfg.find(gpuKey) == gpuCfg.end())
        return RC::DEVICE_NOT_FOUND;

    lwswitch::gpuInfo* pGpuInfo = gpuCfg.at(gpuKey);

    if (!pGpuInfo->has_flaaddressrange())
        return RC::ILWALID_OBJECT_PROPERTY;

    *pRange = pGpuInfo->flaaddressrange();

    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::FMLibIfUser::GetRequestorLinkId(const PortInfo& portInfo, UINT32* pLinkId)
{
    MASSERT(pLinkId);
    RC rc;
    *pLinkId = ~0U;

    PortKeyType portKey;
    if (portInfo.type == TestDevice::TYPE_LWIDIA_LWSWITCH)
    {
        portKey.nodeId = portInfo.nodeId;
        portKey.physicalId = portInfo.physicalId;
        portKey.portIndex = portInfo.portNum;
    }
    else if (portInfo.type == TestDevice::TYPE_LWIDIA_GPU)
    {
        // First get info for the connected switch port
        PortInfo remInfo;
        CHECK_RC(GetRemotePortInfo(portInfo, &remInfo));
        portKey.nodeId = remInfo.nodeId;
        portKey.physicalId = remInfo.physicalId;
        portKey.portIndex = remInfo.portNum;
    }
    else
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Invalid portInfo type\n",
               __FUNCTION__);
        return RC::ILWALID_ARGUMENT;
    }

    auto portKeyItr = m_pGFM->mpParser->portInfo.find(portKey);
    if (portKeyItr == m_pGFM->mpParser->portInfo.end())
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Cannot find switchPortInfo for lwswitch %d port %d\n",
               __FUNCTION__, portKey.physicalId, portKey.portIndex);
        return RC::BAD_PARAMETER;
    }

    lwswitch::switchPortInfo* switchPortInfo = portKeyItr->second;

    *pLinkId = static_cast<UINT32>(switchPortInfo->config().requesterlinkid());

    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::FMLibIfUser::GetRemotePortInfo(const PortInfo& portInfo, PortInfo* pRemInfo)
{
    MASSERT(pRemInfo);
    RC rc;

    *pRemInfo = {};
    pRemInfo->type = TestDevice::TYPE_UNKNOWN;

    const TopologyLWLinkConnList& connList = m_pGFM->mpParser->lwLinkConnMap.at(portInfo.nodeId);

    if (portInfo.type == TestDevice::TYPE_LWIDIA_GPU)
    {
        for (const TopologyLWLinkConn& conn : connList)
        {
            if (conn.connType != ACCESS_PORT_GPU)
                continue;

            if (conn.farEnd.nodeId == portInfo.nodeId &&
                conn.farEnd.lwswitchOrGpuId == portInfo.physicalId &&
                conn.farEnd.portIndex == portInfo.portNum)
            {
                pRemInfo->type = TestDevice::TYPE_LWIDIA_LWSWITCH;
                pRemInfo->nodeId = conn.localEnd.nodeId;
                pRemInfo->physicalId = conn.localEnd.lwswitchOrGpuId;
                pRemInfo->portNum = conn.localEnd.portIndex;
                return RC::OK;
            }
        }
    }
    else if (portInfo.type == TestDevice::TYPE_LWIDIA_LWSWITCH)
    {
        for (const TopologyLWLinkConn& conn : connList)
        {
            if (conn.localEnd.nodeId == portInfo.nodeId &&
                conn.localEnd.lwswitchOrGpuId == portInfo.physicalId &&
                conn.localEnd.portIndex == portInfo.portNum)
            {
                pRemInfo->type = (conn.connType == ACCESS_PORT_GPU) ?
                                 TestDevice::TYPE_LWIDIA_GPU :
                                 TestDevice::TYPE_LWIDIA_LWSWITCH;
                pRemInfo->nodeId = conn.farEnd.nodeId;
                pRemInfo->physicalId = conn.farEnd.lwswitchOrGpuId;
                pRemInfo->portNum = conn.farEnd.portIndex;
                return RC::OK;
            }
            if (conn.connType == TRUNK_PORT_SWITCH &&
                conn.farEnd.nodeId == portInfo.nodeId &&
                conn.farEnd.lwswitchOrGpuId == portInfo.physicalId &&
                conn.farEnd.portIndex == portInfo.portNum)
            {
                pRemInfo->type = TestDevice::TYPE_LWIDIA_LWSWITCH;
                pRemInfo->nodeId = conn.localEnd.nodeId;
                pRemInfo->physicalId = conn.localEnd.lwswitchOrGpuId;
                pRemInfo->portNum = conn.localEnd.portIndex;
                return RC::OK;
            }
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::FMLibIfUser::AllocMulticastGroup(UINT32* pGroupId)
{
    MASSERT(pGroupId);
    RC rc;
    FMIntReturn_t ret = FM_INT_ST_OK;

    *pGroupId = ~0U;

#if LWCFG(GLOBAL_LWSWITCH_IMPL_LS10)
    ret = m_pGFM->mpMcastMgr->allocateMulticastGroup(ILWALID_FABRIC_PARTITION_ID, *pGroupId);
#else
    ret = FM_INT_ST_GENERIC_ERROR;
#endif
    CHECK_RC_MSG(FMIntReturnToRC(ret), "Could not allocate multicast group\n");

    return rc;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::FMLibIfUser::FreeMulticastGroup(UINT32 groupId)
{
    RC rc;
    FMIntReturn_t ret = FM_INT_ST_OK;

#if LWCFG(GLOBAL_LWSWITCH_IMPL_LS10)
    ret = m_pGFM->mpMcastMgr->freeMulticastGroup(ILWALID_FABRIC_PARTITION_ID, groupId);
#else
    ret = FM_INT_ST_GENERIC_ERROR;
#endif
    CHECK_RC_MSG(FMIntReturnToRC(ret), "Could not free multicast group %d\n", groupId);

    return rc;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::FMLibIfUser::SetMulticastGroup
(
    UINT32 groupId,
    bool reflectiveMode,
    bool excludeSelf,
    const set<TestDevicePtr>& gpus
)
{
    RC rc;
    FMIntReturn_t ret = FM_INT_ST_OK;

    set<GpuKeyType> fmGpus;
    for (const TestDevicePtr& pGpu : gpus)
    {
        GpuKeyType gpuKey = {};
        gpuKey.nodeId = 0;
        gpuKey.physicalId = pGpu->GetInterface<LwLink>()->GetTopologyId();
        auto ret = fmGpus.insert(gpuKey);
        if (!ret.second)
        {
            Printf(Tee::PriError, "GPUs passed to Multicast Group are not unique!\n");
            return RC::ILWALID_ARGUMENT;
        }
    }

#if LWCFG(GLOBAL_LWSWITCH_IMPL_LS10)
    ret = m_pGFM->mpMcastMgr->setMulticastGroup(ILWALID_FABRIC_PARTITION_ID,
                                                groupId,
                                                reflectiveMode,
                                                excludeSelf,
                                                fmGpus);
#else
    ret = FM_INT_ST_GENERIC_ERROR;
#endif
    CHECK_RC_MSG(FMIntReturnToRC(ret), "Could not set multicast group %d\n", groupId);

    return rc;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::FMLibIfUser::GetMulticastGroupBaseAddress(UINT32 groupId, UINT64* pBase)
{
    MASSERT(pBase);
    RC rc;
    FMIntReturn_t ret = FM_INT_ST_OK;

    uint64_t base = 0;

#if LWCFG(GLOBAL_LWSWITCH_IMPL_LS10)
    ret = m_pGFM->mpMcastMgr->getMulticastGroupBaseAddress(groupId, base);
#else
    ret = FM_INT_ST_GENERIC_ERROR;
#endif
    CHECK_RC_MSG(FMIntReturnToRC(ret), "Could not get multicast group %d base address\n", groupId);

    *pBase = static_cast<UINT64>(base);

    return rc;
}

namespace
{
    const UINT32 ACCESS_VALID = 0x1;
    const UINT32 PORT_SHIFT   = 4;

    //------------------------------------------------------------------------------
    RC ParseVcValid
    (
        UINT32 vcValid,
        UINT32 portOffset,
        set<UINT32>* pPorts
    )
    {
        UINT32 lwrPort = 0;

        // The low order bit in each nibble indicates if a port is a valid target
        while (vcValid)
        {
            if (vcValid & ACCESS_VALID)
                pPorts->insert(lwrPort + portOffset);

            lwrPort++;
            vcValid >>= PORT_SHIFT;
        }
        return RC::OK;
    }
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::FMLibIfUser::GetIngressRequestPorts
(
    const PortInfo& portInfo,
    UINT32 fabricIndex,
    set<UINT32>* pPorts
)
{
    MASSERT(pPorts);
    RC rc;

    pPorts->clear();
    FMFabricParser* pParser = m_pGFM->mpParser;

    ReqTableKeyType reqKey = {};
    reqKey.nodeId = portInfo.nodeId;
    reqKey.portIndex = portInfo.portNum;
    reqKey.physicalId = portInfo.physicalId;
    reqKey.index = fabricIndex;

    auto reqItr = pParser->reqEntry.find(reqKey);
    if (reqItr == pParser->reqEntry.end())
        return RC::OK;

    ::ingressRequestTable* pReqTable = reqItr->second;
    if (pReqTable->has_vcmodevalid7_0())
    {
        CHECK_RC(ParseVcValid(pReqTable->vcmodevalid7_0(), 0, pPorts));
    }
    if (pReqTable->has_vcmodevalid15_8())
    {
        CHECK_RC(ParseVcValid(pReqTable->vcmodevalid15_8(), 8, pPorts));
    }
    if (pReqTable->has_vcmodevalid17_16())
    {
        CHECK_RC(ParseVcValid(pReqTable->vcmodevalid17_16(), 16, pPorts));
    }

    return rc;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::FMLibIfUser::GetIngressResponsePorts
(
    const PortInfo& portInfo,
    UINT64 fabricBase,
    set<UINT32>* pPorts
)
{
    MASSERT(pPorts);
    RC rc;

    pPorts->clear();
    FMFabricParser* pParser = m_pGFM->mpParser;

    auto gpuItr = find_if(pParser->gpuCfg.begin(), pParser->gpuCfg.end(), [&fabricBase](auto itr) -> bool
    {
        const lwswitch::gpuInfo* pGpuInfo = itr.second;
        UINT64 fabricAddressBase = pGpuInfo->fabricaddressbase();
        UINT64 fabricAddressRange = pGpuInfo->fabricaddressrange();
        return (fabricBase >= fabricAddressBase &&
                fabricBase < fabricAddressBase + fabricAddressRange);
    });
    if (gpuItr == pParser->gpuCfg.end())
    {
        Printf(Tee::PriError, "%s : Cannot find GPU. Invalid fabricBase = %llu\n", __FUNCTION__, fabricBase);
        return RC::BAD_PARAMETER;
    }

    GpuKeyType& gpuKey = const_cast<GpuKeyType&>(gpuItr->first);
    map<SwitchKeyType, uint64_t> connectedSwitches;
    pParser->getSwitchPortMaskToGpu(gpuKey, connectedSwitches);

    for (auto switchItr : connectedSwitches)
    {
        const SwitchKeyType& switchKey = switchItr.first;
        UINT64 switchMask = switchItr.second;

        PortKeyType portKey = {};
        portKey.nodeId = switchKey.nodeId;
        portKey.physicalId = switchKey.physicalId;

        for (INT32 port = Utility::BitScanForward64(switchMask);
             port != -1;
             port = Utility::BitScanForward64(switchMask, port+1))
        {
            portKey.portIndex = port;

            auto portItr = pParser->portInfo.find(portKey);
            if (portItr == pParser->portInfo.end())
                continue;

            lwswitch::switchPortInfo* pInfo = portItr->second;
            if (!pInfo->has_config())
            {
                Printf(Tee::PriError, "%s : Invalid switchPortInfo entry. Config not present.\n", __FUNCTION__);
                return RC::BAD_PARAMETER;
            }

            const ::switchPortConfig& config = pInfo->config();
            if (!config.has_requesterlinkid())
            {
                Printf(Tee::PriError, "%s : Invalid switchPortConfig entry. RequesterLinkId not present.\n", __FUNCTION__);
                return RC::BAD_PARAMETER;
            }

            UINT32 requesterLinkId = config.requesterlinkid();

            RespTableKeyType respKey = {};
            respKey.nodeId = portInfo.nodeId;
            respKey.portIndex = portInfo.portNum;
            respKey.physicalId = portInfo.physicalId;
            respKey.index = requesterLinkId;

            auto respItr = pParser->respEntry.find(respKey);
            if (respItr == pParser->respEntry.end())
                continue;

            ::ingressResponseTable* pRespTable = respItr->second;
            if (pRespTable->has_vcmodevalid7_0())
            {
                CHECK_RC(ParseVcValid(pRespTable->vcmodevalid7_0(), 0, pPorts));
            }
            if (pRespTable->has_vcmodevalid15_8())
            {
                CHECK_RC(ParseVcValid(pRespTable->vcmodevalid15_8(), 8, pPorts));
            }
            if (pRespTable->has_vcmodevalid17_16())
            {
                CHECK_RC(ParseVcValid(pRespTable->vcmodevalid17_16(), 16, pPorts));
            }
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::FMLibIfUser::GetRidPorts
(
    const PortInfo& portInfo,
    UINT32 fabricIndex,
    set<UINT32>* pPorts
)
{
    MASSERT(pPorts);
    RC rc;

    pPorts->clear();
    FMFabricParser* pParser = m_pGFM->mpParser;

    // Use the first matching RMAP entry
    // They should all be identical
    auto rmapItr = find_if(pParser->rmapEntry.begin(), pParser->rmapEntry.end(), [&fabricIndex](auto itr) -> bool
    {
        return itr.first.index == fabricIndex;
    });
    if (rmapItr == pParser->rmapEntry.end())
    {
#if LWCFG(GLOBAL_LWSWITCH_IMPL_LS10)
        rmapItr = find_if(pParser->rmapExtBEntry.begin(), pParser->rmapEntry.end(), [&fabricIndex](auto itr) -> bool
        {
            return itr.first.index == fabricIndex;
        });

        if (rmapItr == pParser->rmapExtBEntry.end())
#endif
        {
            Printf(Tee::PriError, "%s : Cannot find RMAP entry for fabricIndex = %d\n", __FUNCTION__, fabricIndex);
            return RC::BAD_PARAMETER;
        }
    }

    ::rmapPolicyEntry* pRmapEntry = rmapItr->second;
    if (!pRmapEntry->has_targetid())
    {
        Printf(Tee::PriError, "%s : Invalid RMAP entry. TargetId not present.\n", __FUNCTION__);
        return RC::BAD_PARAMETER;
    }
    const UINT32 targetId = pRmapEntry->targetid();

    RidTableKeyType ridKey = {};
    ridKey.nodeId = portInfo.nodeId;
    ridKey.portIndex = portInfo.portNum;
    ridKey.physicalId = portInfo.physicalId;
    ridKey.index = targetId;

    auto ridItr = pParser->ridEntry.find(ridKey);
    if (ridItr == pParser->ridEntry.end())
        return RC::OK;

    ::ridRouteEntry* pRidEntry = ridItr->second;
    for (int idx = 0; idx < pRidEntry->portlist_size(); idx++)
    {
        const routePortList& portList = pRidEntry->portlist(idx);
        if (!portList.has_portindex())
        {
            Printf(Tee::PriError, "%s : Invalid Rid PortList Entry!\n", __FUNCTION__);
            return RC::BAD_PARAMETER;
        }
        UINT32 portIndex = static_cast<UINT32>(portList.portindex());
        pPorts->insert(portIndex);
    }

    return rc;
}
