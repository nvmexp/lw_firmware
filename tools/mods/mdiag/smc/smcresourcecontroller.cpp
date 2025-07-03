/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "smcresourcecontroller.h"
#include "gpupartition.h"
#include "smcengine.h"
#include "mdiag/sysspec.h"
#include "gpu/include/gpudev.h"
#include "mdiag/lwgpu.h"
#include "core/include/platform.h"
#include "mdiag/advschd/policymn.h"
#include "core/include/registry.h"
#include "mdiag/vgpu_migration/vgpu_migration.h"
#include "smcparser.h"
#include "mdiag/utl/utl.h"
#include "ctrl/ctrlc637.h"
#include "class/clc637.h"
#include <sstream>
#include <algorithm>

DEFINE_MSG_GROUP(Mdiag, Gpu, SMC, false);
#define SMCID() __MSGID__(Mdiag, Gpu, SMC)

namespace
{
    //FIXME: FModel is adding support for MODS_FUSE_STATUS_OPT_SYS_PIPE_DATA
    //       After it is ready, we will use AmpereFs::SyspipeMask
    UINT32 SyspipeMask(GpuSubdevice* pSubDev)
    {
        LwRmPtr pLwRm;
        UINT32 syspipeMask = 0;
        LW2080_CTRL_GPU_GET_PHYS_SYS_PIPE_IDS_PARAMS params = {0};
        params.swizzId = LW2080_CTRL_GPU_PARTITION_ID_ILWALID;
        RC rc = pLwRm->ControlBySubdevice(pSubDev,
                                          LW2080_CTRL_CMD_GPU_GET_PHYS_SYS_PIPE_IDS,
                                          &params, sizeof(params));
        if (rc != OK)
        {
            ErrPrintf(SMCID(), "%s : fails to ilwoke LW2080_CTRL_CMD_GPU_GET_PHYS_SYS_PIPE_IDS, RC: %d\n",
                   __FUNCTION__, rc.Get());
            MASSERT(!"Fail to get syspipe floorsweeping mask!");
        }

        // Create syspipe mask as further usage warrants it
        for (UINT32 i = 0; i < params.physSysPipeCount; i++)
        {
            syspipeMask |= BIT(params.physSysPipeId[i]);
        }

        DebugPrintf(SMCID(), "%s : syspipeMask is 0x%x\n", __FUNCTION__, syspipeMask);

        return syspipeMask;
    }
}

SmcResourceController::SmcResourceController
(
    LWGpuResource * pGpuResource,
    ArgReader * params
):
    m_pGpuResource(pGpuResource),
    m_Reader(params),
    m_EnableDynamicTpcFs(false)
{
    MASSERT(m_pGpuResource != nullptr);
    m_SysPipeMask = ~0U;
    m_GpcMask = ~0U;
}

SmcResourceController::~SmcResourceController()
{
    RC rc;
    // Acquire mutex for enabling/disabling SMC to ensure that SMC is not
    // enabled/disabled until SMC sensitive regs are read in a monitor
    // iteration
    Tasker::MutexHolder mh(m_pGpuResource->GetGpuSubdevice()->GetSmcEnabledMutex());
    // free HW resource
    if (Platform::IsPhysFunMode() || Platform::IsDefaultMode())
    {
        // Free Exelwtion Partitions
        if (OK != (rc = AllocOrFreeSmcEngines(m_ActiveGpuPartitions, false)))
        {
            ErrPrintf(SMCID(), "Error (%s) while freeing SmcEngine.\n",
                rc.Message());
            MASSERT(0);
        }
        // Free MemoryPartition RM clients
        if (OK != (rc = m_pGpuResource->FreeRmClients()))
        {
            ErrPrintf(SMCID(), "Error (%s) while freeing Memory Partitions' RM clients.\n",
                rc.Message());
            MASSERT(0);
        }
        // Free MemoryPartitions
        if (OK != (rc = FreeGpuPartitions(m_ActiveGpuPartitions)))
        {
            ErrPrintf(SMCID(), "Error (%s) while freeing Memory Partitions.\n",
                rc.Message());
            MASSERT(0);
        }
    }
    else
    {
    	if (OK != (rc = AllocOrFreeSmcEngines(m_ActiveGpuPartitions, false)))
        {
            ErrPrintf(SMCID(), "Error (%s) while freeing SmcEngine.\n",
                rc.Message());
            MASSERT(0);
        }
        if (OK != (rc = m_pGpuResource->FreeRmClients()))
        {
            ErrPrintf(SMCID(), "Error (%s) while freeing Memory Partitions' RM clients.\n",
                rc.Message());
            MASSERT(0);
        }
    }

    if (m_pProfilingLwRm)
    {
        MASSERT(m_ProfilingHandle != 0);
        m_pProfilingLwRm->Free(m_ProfilingHandle);
        if (OK != (rc = m_pGpuResource->GetGpuDevice()->Free(m_pProfilingLwRm)))
        {
            ErrPrintf(SMCID(), 
                "Freeing SMC Profiling RM client failed (%s).\n",
                rc.Message());
            MASSERT(0);
        }
    }

    for (auto it = m_ActiveGpuPartitions.begin();
            it != m_ActiveGpuPartitions.end(); )
    {
        // free Mods holded resource
        // free Smc Engine resource
        delete (*it);
        it = m_ActiveGpuPartitions.erase(it);
    }

    if (Platform::IsPhysFunMode() || Platform::IsDefaultMode())
    {
        SetGpuPartitionMode(SmcResourceController::PartitionMode::LEGACY);
        m_pGpuResource->GetGpuSubdevice()->DisableSMC();
    }

    if (m_EnableDynamicTpcFs)
    {
        GpuDevice * pGpuDevice = m_pGpuResource->GetGpuDevice();

        // If there are some partition mode switch happened in UTL side,
        // When SMC back to the legacy, Mods need to do resetGpu + RecoveryRM
        pGpuDevice->Reset(LW0080_CTRL_BIF_RESET_FLAGS_TYPE_SW_RESET);
        pGpuDevice->RecoverFromReset();
        m_EnableDynamicTpcFs = false;
    }
}

RC SmcResourceController::Init()
{
    RC rc;

    m_pSmcParser = make_unique<SmcParser>(this);
    // Cache GR0's MMU Engine Id Start before partitioning is done (needed for Access Counters)
    CHECK_RC(MDiagUtils::CacheMMUEngineIdStart(m_pGpuResource->GetGpuSubdevice()));

    UINT32 dynamicTpcFsEnabled = 0;
    if (OK == Registry::Read("ResourceManager", "RMDebugSMCDynamicTpcFs", &dynamicTpcFsEnabled))
    {
        m_EnableDynamicTpcFs = (0 != dynamicTpcFsEnabled);
    }

    InfoPrintf(SMCID(), "Dynamic TPC Floorsweeping %s\n", m_EnableDynamicTpcFs ? "enabled" : "disabled");

    // Acquire mutex for enabling/disabling SMC to ensure that SMC is not
    // enabled/disabled until SMC sensitive regs are read in a monitor
    // iteration
    Tasker::MutexHolder mh(m_pGpuResource->GetGpuSubdevice()->GetSmcEnabledMutex());
    if (Platform::IsVirtFunMode())
    {
        if (m_Reader->ParamPresent("-smc_partitioning") ||
                m_Reader->ParamPresent("-smc_partitioning_sys0_only") ||
                m_Reader->ParamPresent("-smc_mem"))
        {
            ErrPrintf(SMCID(), "Please remove -smc_partitioning/-smc_mem/-smc_partitioning_sys0_only at VF mods.\n");
            ErrPrintf(SMCID(), "Only PF can have this ability to allocate smc resource.\n");
            return RC::SOFTWARE_ERROR;
        }
        CHECK_RC(PartitioningVfGpu());
    }
    else
    {
        // Do not allow old SMC APIs from Hopper onwards
        if ((m_pGpuResource->GetGpuDevice()->GetFamily() >= GpuDevice::Hopper) &&
                (m_Reader->ParamPresent("-smc_partitioning") || 
                m_Reader->ParamPresent("-smc_partitioning_sys0_only")))
        {
            ErrPrintf(SMCID(), "-smc_partitioning, -smc_partitioning_sys0_only and -smc_engine_label are not supported from Hopper onwards\n");
            ErrPrintf(SMCID(), "Please use -smc_mem, -smc_eng, -smc_eng_name instead.\n");
            ErrPrintf(SMCID(), "Details about the new SMC args can be found at: https://confluence.lwpu.com/pages/viewpage.action?pageId=249627380\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        // Do not enable MAX PERF mode if dynamic TPC FS is set
        // It is the test writer's resposibility to do it from UTL script
        if (!m_EnableDynamicTpcFs)
        {
            SetGpuPartitionMode(SmcResourceController::PartitionMode::MAX_PERF);
            m_pGpuResource->GetGpuSubdevice()->EnableSMC();
        }
        m_SysPipeMask = SyspipeMask(m_pGpuResource->GetGpuSubdevice());
        m_GpcMask = m_pGpuResource->GetGpuSubdevice()->GetFsImpl()->GpcMask();
        CHECK_RC(PartitioningGPU());
    }

    if (Platform::IsPhysFunMode() || Platform::IsDefaultMode())
    {
        //FIXME: If it's a VF MODS, when it starts, GPU is already in SMC mode.
        //       We will enable SMC in AmpereGpuSubdevice ctor and disable it in dctor
        //       After we finalize elw variable to indicate SRIOV + SMC is active, we will fix it
        // In case the user specifies all Partitons as invalid then MODS will not allocate
        // any partition, thus HW and RM will still be in legacy mode
        // In such cases, do not enable SMC mode in MODS as well
        if (m_ActiveGpuPartitions.size() == 0)
        {
            m_pGpuResource->GetGpuSubdevice()->DisableSMC();
        }
    }

    return rc;
}

RC SmcResourceController::PartitioningVfGpu()
{
    RC rc;
    MASSERT(Platform::IsVirtFunMode());

    string partitionName = Xp::GetElw("SRIOV_SMC_PART_NAME");

    // Partition names will be sent by PF only when -smc_mem is used
    if (!partitionName.empty())
    {
        m_IsSmcMemApi = true;
    }
    GpuPartition * pGpuPartition = new GpuPartition("", m_pGpuResource, partitionName);
    m_ActiveGpuPartitions.push_back(pGpuPartition);

    vector<string> sysPipes;
    CHECK_RC(Utility::Tokenizer(Xp::GetElw("SRIOV_SMC_SYS"), "sys", &sysPipes));
    vector<SmcResourceController::SYS_PIPE> partitionAllSysPipes;
    for (auto sys : sysPipes)
    {
        partitionAllSysPipes.push_back(static_cast<SmcResourceController::SYS_PIPE>(std::atoi(sys.c_str())));
    }

    pGpuPartition->SetAllSyspipes(partitionAllSysPipes);

    vector<string> smcEngineNames;
    CHECK_RC(Utility::Tokenizer(Xp::GetElw("SRIOV_SMC_SMCENGINE_NAMES"), " ", &smcEngineNames));

    for (UINT32 i = 0; i < LW2080_CTRL_GPU_MAX_SMC_IDS; ++i)
    {
        UINT32 gpcCount = pGpuPartition->GetSmcEngineGpcCount(i);
        if (gpcCount != 0)
        {
            SmcResourceController::SYS_PIPE sys = pGpuPartition->GetSmcEngineSysPipe(i); // i + base sys pipe or sys pipe list
            SmcEngine * pSmcEngine = new SmcEngine(
                    gpcCount,
                    sys,
                    pGpuPartition,
                    // SmcEngine names will be sent by PF only when -smc_mem is used
                    IsSmcMemApi() ? smcEngineNames[i] : "");
            pGpuPartition->AddSmcEngine(pSmcEngine);
        }
    }

    CHECK_RC(AllocGpuPartitions(m_ActiveGpuPartitions));
    CHECK_RC(m_pGpuResource->AllocRmClients());

    for (auto pGpuPartition : m_ActiveGpuPartitions)
    {
        LWC637_CTRL_EXEC_PARTITIONS_GET_PARAMS params = { };
        LwRm* pLwRm = m_pGpuResource->GetLwRmPtr(pGpuPartition);
        CHECK_RC(pLwRm->Control(m_pGpuResource->GetGpuPartitionHandle(pGpuPartition),
            LWC637_CTRL_CMD_EXEC_PARTITIONS_GET,
            &params,
            sizeof(params)));

        MASSERT(pGpuPartition->GetActiveSmcEngines().size() <= params.execPartCount);

        UINT32 exelwtionPartitionIdx = 0;
        for (auto pSmcEngine : pGpuPartition->GetActiveSmcEngines())
        {
            pSmcEngine->SetExelwtionPartitionId(params.execPartId[exelwtionPartitionIdx]);
            exelwtionPartitionIdx++;
        }
    }
    CHECK_RC(AllocOrFreeSmcEngines(m_ActiveGpuPartitions, true));

    return rc;
}

RC SmcResourceController::PartitioningGPU()
{
    RC rc;

    if (m_Reader->ParamPresent("-smc_mem"))
    {
        m_SmcConfiguration = m_Reader->ParamStr("-smc_mem");
        m_IsSmcMemApi = true;
        CHECK_RC(ConfigureSmc(m_SmcConfiguration));
    }
    else if (m_Reader->ParamPresent("-smc_partitioning"))
    {
        m_SmcConfiguration = m_Reader->ParamStr("-smc_partitioning");
        CHECK_RC(ConfigureSmc(m_SmcConfiguration));
    }
    else if (m_Reader->ParamPresent("-smc_partitioning_sys0_only"))
    {
        // Create one swizzid 0 Gpu Partition and one syspipe 0 Smc Engine
        CHECK_RC(CreateSys0OnlyPartition());
    }
    else
    {
        ErrPrintf(SMCID(), "You haven't specified -smc_partitioning or -smc_partitioning_sys0_only or -smc_mem\n");
        ErrPrintf(SMCID(), "Please double check the command line.\n");
        MASSERT(0);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    return rc;
}

RC SmcResourceController::ConfigureSmc(string configure)
{
    MASSERT(Platform::IsPhysFunMode() || Platform::IsDefaultMode());

    m_pSmcParser->ParseSmcCmd(configure);
    return CreateSMCPartitioning();
}

GpuPartition* SmcResourceController::CreateOnePartition
(
    string partitionStr,
    UINT32 gpcCount,
    UINT32 ilwalidGpcCount,
    SmcResourceController::PartitionFlag partitionFlag,
    bool isValid,
    string partitionName
)
{
    auto pGpuPartition = std::make_unique<GpuPartition>(partitionStr, gpcCount,
            ilwalidGpcCount, partitionFlag, GetGpuResource(), isValid, partitionName);

    // When this function is called, smc engine has not been added to gpu partition.
    // GpuPartitionExist() always return true.
    // It's meaningless to do gpu partition check here.
    // Just ignore here.

    DebugPrintf(SMCID(), "%s: New GPU Partition (Name: %s) object created.\n",
            __FUNCTION__, pGpuPartition->GetName().c_str());

    return pGpuPartition.release();
}

SmcEngine* SmcResourceController::CreateOneSMCEngine
(
    SmcResourceController::SYS_PIPE sys,
    UINT32 gpcCount,
    GpuPartition* pGpuPartition,
    string smcEngName
)
{
    auto pSmcEngine = std::make_unique<SmcEngine>(gpcCount, sys, pGpuPartition,
                        smcEngName);

    // Delete equal smc engine
    if (pGpuPartition->SmcEngineExists(pSmcEngine.get()))
    {
        pGpuPartition = nullptr;
        DebugPrintf(SMCID(), "%s: Ignore existing SMC Engine (Name: %s).\n", 
            __FUNCTION__, pSmcEngine->GetName().c_str());
    }
    else
    {
        DebugPrintf(SMCID(), "%s: New SMC Engine (Name: %s) object created.\n", 
            __FUNCTION__, pSmcEngine->GetName().c_str());
    }

    return pSmcEngine.release();
}

void SmcResourceController::AddGpuPartition
(
    GpuPartition* pGpuPartition
)
{
    m_ActiveGpuPartitions.push_back(pGpuPartition);
    InfoPrintf(SMCID(), "%s: Add Gpu Partition.\n", __FUNCTION__);
}

RC SmcResourceController::CreateSys0OnlyPartition()
{
    RC rc;
    MASSERT(Platform::IsPhysFunMode() || Platform::IsDefaultMode());

    vector<GpuPartition *> newAddedGpuPartitions;
    GpuPartition * pGpuPartition = new GpuPartition("", m_pGpuResource, "",
            true/*useAllGpc*/);
    SmcEngine * pSmcEngine = new SmcEngine(0, SmcResourceController::SYS0,
            pGpuPartition, "", true/*useAllGpc*/);
    pGpuPartition->AddSmcEngine(pSmcEngine);
    newAddedGpuPartitions.push_back(pGpuPartition);

    CHECK_RC(AllocGpuPartitions(newAddedGpuPartitions));
    m_ActiveGpuPartitions.insert(m_ActiveGpuPartitions.end(),
            newAddedGpuPartitions.begin(), newAddedGpuPartitions.end());
    CHECK_RC(m_pGpuResource->AllocRmClients());
    CHECK_RC(AllocOrFreeSmcEngines(newAddedGpuPartitions, true));

    return rc;
}

bool SmcResourceController::GpuPartitionExists
(
    GpuPartition * pNewAddedGpuPartition
)
{
    // Note: Will not exist the case that move the inactive Gpu partition
    // to active Gpu partition
    for (const auto & it : m_ActiveGpuPartitions)
    {
        if (*it == *pNewAddedGpuPartition)
        {
            return true;
        }
    }

    return false;
}

GpuPartition * SmcResourceController::GetGpuPartition(UINT32 swizzId)
{
    for (const auto & partition : m_ActiveGpuPartitions)
    {
        if (partition->GetSwizzId() == swizzId)
        {
            return partition;
        }
    }

    WarnPrintf(SMCID(), "%s: Can't get the gpu partition which swizzId = %d.\n", 
        __FUNCTION__, swizzId);
    return nullptr;
}

SmcResourceController::SYS_PIPE SmcResourceController::Colwert2SysPipe(const string & str)
{
    const char * format = "sys%d";
    UINT32 value;

    if (1 == sscanf(str.c_str(), format, &value))
    {
        return static_cast<SmcResourceController::SYS_PIPE>(SmcResourceController::SYS_PIPE::SYS0 +
            value);
    }

    ErrPrintf(SMCID(), "%s: Unsupported syspipe string %s.\n", __FUNCTION__, str.c_str());

    return SmcResourceController::SYS_PIPE::NOT_SUPPORT;
}

string SmcResourceController::ColwertSysPipe2Str(SmcResourceController::SYS_PIPE sys)
{
    if (sys == SmcResourceController::SYS_PIPE::NOT_SUPPORT)
    {
        ErrPrintf(SMCID(), "%s: Unsupported syspipe enum.\n", __FUNCTION__);
        return "";
    }

    UINT32 value = sys - SmcResourceController::SYS_PIPE::SYS0;
    return "sys"+to_string(value);
}

RC SmcResourceController::AllocOrFreeSmcEngines
(
    vector<GpuPartition *> & gpupartition,
    bool isAlloc
)
{
    RC rc;

    for (const auto & pGpuPartition : gpupartition)
    {
        if (isAlloc)
        {
            CHECK_RC(pGpuPartition->AllocAllSmcEngines());
        }
        else
        {
            CHECK_RC(pGpuPartition->FreeAllSmcEngines());
        }
    }

    return OK;
}

SmcResourceController::PartitionFlag SmcResourceController::CheckAndGetPartitionFlag
(
    SmcResourceController::PartitionFlag requestedPartitionFlag,
    UINT32 requestedPartitionCount
)
{
    SmcResourceController::PartitionFlag actualPartitionFlag = 
        SmcResourceController::PartitionFlag::INVALID; 
    SmcResourceController::PartitionFlag partitionFlagSubType = 
        m_pSmcParser->GetPartitionFlagSubType(requestedPartitionFlag);
    vector<SmcResourceController::PartitionFlag> partitionFlagList { requestedPartitionFlag };

    // If this particular partitionFlag has a subtype (HALF -> MINI_HALF) then
    // add it to the list of partitionFlags to retrieve #AvailablePartition
    // The subtypes is added at the back so that we check for the 
    // requestedPartitionFlag first
    if (partitionFlagSubType != SmcResourceController::PartitionFlag::INVALID)
    {
        partitionFlagList.push_back(partitionFlagSubType);
    }
    
    // Map of PartitionFlag : #Available Partitions
    auto mapPartitionFlagCount = GetAvailablePartitionTypeCount(partitionFlagList);

    // For the requestedPartitionFlag and its subtypes check if #Requested is 
    // more than #Available. Choose the first one which matches this condition.
    // Use partitionFlagList here so that the search is in-order
    for (auto partitionFlag : partitionFlagList)
    {
        if (mapPartitionFlagCount[partitionFlag] >= requestedPartitionCount)
        {
            actualPartitionFlag = partitionFlag;
            break;
        }
    }

    // If there are not enough partitions of requestedPartitionFlag as well as 
    // its subtype, then assert and print the availability for debugging
    if (actualPartitionFlag == SmcResourceController::PartitionFlag::INVALID)
    {
        auto mapAllPartitionFlagCount = GetAvailablePartitionTypeCount(vector<SmcResourceController::PartitionFlag>());
        ErrPrintf(SMCID(), "Alloc partitionType %s and subtypes failed. #Partitions (%d) requested\n",
            m_pSmcParser->GetPartitionStrFromFlag(requestedPartitionFlag).c_str(), 
            requestedPartitionCount);
        ErrPrintf(SMCID(), "Available partitions count:\n");
        for (auto const & mapEntry : mapAllPartitionFlagCount)
        {
            ErrPrintf(SMCID(), "%s : %d\n", 
                m_pSmcParser->GetPartitionStrFromFlag(mapEntry.first).c_str(),
                mapEntry.second);
        }

        MASSERT(0);
    }

    // If we selected a different partitionType than requestedPartitionFlag
    if (requestedPartitionFlag != actualPartitionFlag)
    {
        InfoPrintf(SMCID(), "Requested partition type %s but will allocate %s due to availability\n",
            m_pSmcParser->GetPartitionStrFromFlag(requestedPartitionFlag).c_str(),
            m_pSmcParser->GetPartitionStrFromFlag(actualPartitionFlag).c_str());
    }

    return actualPartitionFlag;
}

RC SmcResourceController::AllocPartitionsRMCtrlCall
(
    LW2080_CTRL_GPU_SET_PARTITIONS_PARAMS* params,
    vector<GpuPartition*>& allocGpuPartitions
)
{
    RC rc;
    LwRmPtr pLwRm;
    GpuDevice * pGpuDev = m_pGpuResource->GetGpuDevice();
    // SMC and SLI can't work together
    MASSERT(pGpuDev->GetNumSubdevices() == 1);
    params->partitionCount = static_cast<UINT32>(allocGpuPartitions.size());

    GpuSubdevice * pGpusubdevice = pGpuDev->GetSubdevice(0);
    rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(pGpusubdevice),
                        LW2080_CTRL_CMD_GPU_SET_PARTITIONS,
                        params,
                        sizeof(*params));
    if (rc != OK)
    {
        ErrPrintf(SMCID(), "Alloc Partition failed: Error %s \n",
                    rc.Message());
        MASSERT(0);
        return rc;
    }
    
    for (UINT32 idx = 0; idx < allocGpuPartitions.size(); ++idx)
    {
        allocGpuPartitions[idx]->SetSwizzId(params->partitionInfo[idx].swizzId);
        // For -smc_mem API GpuPartition's GpcCount is invalid at this point
        // Update GpcCount for -smc_partitioning_sys0_only as well
        // Updating it here
        if (IsSmcMemApi() ||
            ((allocGpuPartitions.size() == 1) && (allocGpuPartitions[0]->UseAllGpcs())))
        {
            allocGpuPartitions[idx]->UpdateGpuPartitionInfo();
        }
    }

    return rc;
}

// hw side alloc or free resource
RC SmcResourceController::AllocGpuPartitions
(
    vector<GpuPartition *> & gpuPartitions
)
{
    RC rc;
    // List of GpuPartitions to be actually allocated or freed
    // We skip the right most invalid GpuPartitions since it has no meaning to occupy the resource and free it later
    vector<GpuPartition*> allocGpuPartitions;
    if (gpuPartitions.size() == 0)
    {
        return rc;
    }

    if (Platform::IsVirtFunMode())
    {
        if (Utl::HasInstance())
        {
            for (auto pGpuPartition : gpuPartitions)
            {
                Utl::Instance()->AddGpuPartition(m_pGpuResource, pGpuPartition);
            }
        }
        return rc;
    }

    LW2080_CTRL_GPU_SET_PARTITIONS_PARAMS params = {0};

    // Handle -smc_partitioning_sys0_only
    if ((gpuPartitions.size() == 1) && (gpuPartitions[0]->UseAllGpcs()))
    {
        Printf(SMCID(), "Only one Swizzid 0 Gpu Partition.\n");
        SmcResourceController::PartitionFlag partitionFlag = PartitionFlag::FULL;
        params.partitionInfo[0].partitionFlag = partitionFlag;
        params.partitionInfo[0].bValid = true;
        allocGpuPartitions.push_back(gpuPartitions[0]);
        // For -smc_partitioning_sys0_only cmdline arg, partitionFlag is set here
        gpuPartitions[0]->SetPartitionFlag(partitionFlag);
        CHECK_RC(AllocPartitionsRMCtrlCall(&params, allocGpuPartitions));
    }
    else
    {
        // If we are allocating do an optimization here:
        // Skip allocating right mods invalid GpuPartitions
        // Example {(2)sys0}{x}{(1)sys3}{xx}{xx} Skip the last two partitons {xx} and {xx} allocation
        for (auto it = gpuPartitions.rbegin(); it != gpuPartitions.rend(); ++it)
        {
            if ((*it)->IsValid())
            {
                // Found the last valid partition
                allocGpuPartitions.insert(allocGpuPartitions.begin(), gpuPartitions.begin(), it.base());
                break;
            }
        }

        // If there was no valid partition
        if (allocGpuPartitions.size() == 0)
        {
            WarnPrintf(SMCID(), "%s: All the partitions are invalid- not allocating any\n", __FUNCTION__);
            return rc;
        }

        // For the -smc_mem API use the partition flag directly
        if (IsSmcMemApi())
        {
            // Following the algorithm here:
            // https://confluence.lwpu.com/pages/viewpage.action?pageId=249627380#NEWSMCpartitioningAPI(HopperOnwards)-PartitionAllocationAlgorithm

            // Separate out the partitions based on partitionFlags
            map<SmcResourceController::PartitionFlag, vector<GpuPartition*>> mapPartitionFlagObjects;
            for (auto pGpuPartition : allocGpuPartitions)
            {
                MASSERT(pGpuPartition->GetPartitionFlag() != SmcResourceController::PartitionFlag::INVALID);
                mapPartitionFlagObjects[pGpuPartition->GetPartitionFlag()].push_back(pGpuPartition);
            }

            // For each partitionFlagType, check the availability of itself 
            // or its subtype and then allocate all partitions of that type 
            // together
            for (auto const & mapEntry : mapPartitionFlagObjects)
            {
                params = {0};
                SmcResourceController::PartitionFlag partitionFlag = 
                    CheckAndGetPartitionFlag(mapEntry.first, 
                        static_cast<UINT32>(mapEntry.second.size()));

                for (UINT32 idx = 0; idx < mapEntry.second.size(); idx++)
                {
                    // In case partitionFlag changed, change it in the 
                    // GpuPartition object as well
                    mapEntry.second[idx]->SetPartitionFlag(partitionFlag);
                    params.partitionInfo[idx].partitionFlag = partitionFlag;
                    params.partitionInfo[idx].bValid = true;
                }
                CHECK_RC(AllocPartitionsRMCtrlCall(&params, mapPartitionFlagObjects[partitionFlag]));
            }
        }
        // For -smc_partitioning API derive the Partition Flag based on the GpcCount
        else
        {
            UINT32 idx = 0;
            for (auto pGpuPartition : allocGpuPartitions)
            {
                MASSERT(pGpuPartition->GetGpcCount() != ILWALID_GPC_COUNT);
                SmcResourceController::PartitionFlag partitionFlag = m_pSmcParser->GetPartitionFlagFromGpcCount(pGpuPartition->GetGpcCount());
                if (partitionFlag == SmcResourceController::PartitionFlag::INVALID)
                {
                    ErrPrintf(SMCID(), "%s: Not a valid GPU partition size."
                                "GPC number = %u partition config str = %s.\n",
                                __FUNCTION__, pGpuPartition->GetGpcCount(), pGpuPartition->GetConfigStr().c_str());
                    MASSERT(0);
                }
                params.partitionInfo[idx].partitionFlag = partitionFlag;
                params.partitionInfo[idx].bValid = true;
                // For -smc_partitioning cmdline arg, partitionFlag is set here
                pGpuPartition->SetPartitionFlag(partitionFlag);
                idx++;
            }
            CHECK_RC(AllocPartitionsRMCtrlCall(&params, allocGpuPartitions));
        }
    }

    // handle case [xxxx]
    vector <GpuPartition *> ilwalidGpuPartitions;
    for (auto pGpuPartition : allocGpuPartitions)
    {
        if (!pGpuPartition->IsValid())
        {
            ilwalidGpuPartitions.push_back(pGpuPartition);
        }
        else
        {
            if (Utl::HasInstance())
            {
                Utl::Instance()->AddGpuPartition(m_pGpuResource, pGpuPartition);
            }
        }
    }

    FreeGpuPartitions(ilwalidGpuPartitions);
    return rc;
}

RC SmcResourceController::FreeGpuPartitions
(
    vector<GpuPartition *> & gpuPartitions
)
{
    RC rc;
    LwRmPtr pLwRm;
    if (Platform::IsVirtFunMode() || gpuPartitions.size() == 0)
    {
        return rc;
    }

    LW2080_CTRL_GPU_SET_PARTITIONS_PARAMS params = {0};

    for (UINT32 idx = 0; idx < gpuPartitions.size(); ++idx)
    {
        GpuPartition* pGpuPartition = gpuPartitions[idx];
        params.partitionInfo[idx].bValid = false;
        params.partitionInfo[idx].swizzId = pGpuPartition->GetSwizzId();
    }
    
    GpuDevice * pGpuDev = m_pGpuResource->GetGpuDevice();
    // Skip the right-most invalid gpupartition
    params.partitionCount = static_cast<UINT32>(gpuPartitions.size());

    GpuSubdevice * pGpusubdevice = pGpuDev->GetSubdevice(0);
    rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(pGpusubdevice),
                        LW2080_CTRL_CMD_GPU_SET_PARTITIONS,
                        &params,
                        sizeof(params));
    if (rc != OK)
    {
        ErrPrintf(SMCID(), "Free Partition failed: Error %s \n",
                    rc.Message());
        MASSERT(0);
    }

    return rc;
}

SmcEngine* SmcResourceController::GetSmcEngine(const string & str)
{
    for (auto & gpupartition : m_ActiveGpuPartitions)
    {
        SmcEngine * pSmcEngine;
        if (IsSmcMemApi())
        {
            pSmcEngine = gpupartition->GetSmcEngineByName(str);
        }
        else
        {
            pSmcEngine = gpupartition->GetSmcEngine(Colwert2SysPipe(str));
        }
        if (pSmcEngine != nullptr)
        {
            return pSmcEngine;
        }
    }

    return nullptr;
}

RC SmcResourceController::CreateSMCPartitioning()
{
    RC rc;

    vector<SmcParser::PartitionConfigData> partitionConfigDatas =
        m_pSmcParser->GetPartitionConfigDataVec();

    vector<GpuPartition *> newAddedGpuPartitions;
    vector<GpuPartition *> removeGpuPartitions;
    vector<pair<GpuPartition*, vector<SmcParser::SmcEngineConfigData>>> allPartitionsSmcConfigData;
    for (auto & partitionConfigData : partitionConfigDatas)
    {
        // Create gpu partition
        UINT32 gpcCount = partitionConfigData.m_GpcCount;

        auto pGpuPartition = CreateOnePartition(partitionConfigData.m_PartitionStr, gpcCount,
                partitionConfigData.m_IlwalidGpcCount, partitionConfigData.m_PartitionFlag, 
                partitionConfigData.m_IsValid, partitionConfigData.m_PartitionName);
        if (!pGpuPartition)
        {
            ErrPrintf(SMCID(), "%s: meet error when create gpu partition with gpu partition str %s.\n",
                    __FUNCTION__, partitionConfigData.m_PartitionStr.c_str());
            MASSERT(0);
        }

        // Store SmcConfig data for the partition
        if (pGpuPartition->IsValid() && partitionConfigData.m_SmcEngineConfigDatas.size() > 0)
        {
            allPartitionsSmcConfigData.push_back(make_pair(pGpuPartition, partitionConfigData.m_SmcEngineConfigDatas));
        }

        if (GetMDiagvGpuMigration()->IsEnabled())
        {
            MDiagVmScope::GetSmcResources()->AddGpuPartition(pGpuPartition);
        }

        // Delete equal gpu partition
        if (GpuPartitionExists(pGpuPartition))
        {
            pGpuPartition = nullptr;
            InfoPrintf(SMCID(), "%s: Ignore existing gpu partition.\n",
                    __FUNCTION__);
        }

        // Add this gpu partition to newAddedGpuPartitions
        if (pGpuPartition)
        {
            newAddedGpuPartitions.push_back(pGpuPartition);
        }
    }

    // Alloc the partitions
    CHECK_RC(AllocGpuPartitions(newAddedGpuPartitions));

    for (auto & gpuPartition : newAddedGpuPartitions)
    {
        if (gpuPartition->IsValid())
        {
            m_ActiveGpuPartitions.push_back(gpuPartition);
        }
        else
        {
            removeGpuPartitions.push_back(gpuPartition);
        }
    }

    // Alloc RM clients (needed for querying patititon's syspipe from RM)
    CHECK_RC(m_pGpuResource->AllocRmClients());

    // Query and set Partition's syspipes- do this before creating SmcEngines
    // since SmcEngine creation checks for valid syspipes in the partition
    for (auto pGpuPartition : m_ActiveGpuPartitions)
    {
        pGpuPartition->SetSyspipes();
    }

    // Create SmcEngines
    for (auto const & partitionSmcConfigData : allPartitionsSmcConfigData)
    {
        GpuPartition* pGpuPartition = partitionSmcConfigData.first;
        vector<SmcParser::SmcEngineConfigData> smcEngineConfigDatas = partitionSmcConfigData.second;
        UINT32 sysIdx = 0;

        for (const auto & smcEngineConfigData : smcEngineConfigDatas)
        {
            if (smcEngineConfigData.m_IsValid)
            {
                // If -smc_eng API is used then the user can skip providing
                // GpcCount for SmcEngine (telling MODS to use all Gpcs of
                // the partition)
                UINT32 gpcCount = smcEngineConfigData.m_GpcCount;
                if (smcEngineConfigData.m_UsePartitionAllGpcs)
                {
                    MASSERT(gpcCount == ILWALID_GPC_COUNT);
    
                    // The gpcCount returned by LW2080_CTRL_CMD_GPU_DESCRIBE_PARTITIONS 
                    // is the maximum gpcCount of the partition in the chip. 
                    // For example for 222 config, it will return 8 GPCs for FULL partition. 
                    // This number is used by the old API to determine the FLAG to be sent 
                    // with CreatePartition ctrl call. 
                    // Specifically to distinguish between {(2)sys0-xxxxxx} and {(2)sys0}.
                    // But the same gpcCount cannot be used for the new API when the user 
                    // does not specify gpcCount for an SmcEngine thus wanting to use all 
                    // GPCs within a partition.
                    // For such cases call UpdateGpuPartitionInfo here as well to set the 
                    // right number of GPCs within the partition and use that for SmcEngine.
                    pGpuPartition->UpdateGpuPartitionInfo();
                    gpcCount = pGpuPartition->GetGpcCount();
                }

                // If -smc_eng API is used then the user will not provide
                // syspipe for SmcEngine
                SmcResourceController::SYS_PIPE sys = smcEngineConfigData.m_Sys;
                if (sys == SYS_PIPE::NOT_SUPPORT)
                {
                    if (sysIdx >= pGpuPartition->GetAllSyspipes().size())
                    {
                        ErrPrintf(SMCID(), "Memory Partition (%s) has %d SysPipes but more SmcEngines are being requested\n",
                            pGpuPartition->GetName().c_str(), pGpuPartition->GetAllSyspipes().size());
                        return RC::BAD_COMMAND_LINE_ARGUMENT;
                    }
                    sys = (pGpuPartition->GetAllSyspipes())[sysIdx++];
                }

                SmcEngine* pSmcEngine = CreateOneSMCEngine(sys,
                        gpcCount, pGpuPartition,
                        smcEngineConfigData.m_SmcEngineName);
                if (pSmcEngine)
                {
                    pGpuPartition->AddSmcEngine(pSmcEngine);
                }
            }
        }
    }

    // Alloc SmcEngines
    CHECK_RC(AllocOrFreeSmcEngines(newAddedGpuPartitions, true));

    for (auto & pGpuPartition : removeGpuPartitions)
    {
        delete pGpuPartition;
        pGpuPartition = nullptr;
    }

    return rc;
}

void SmcResourceController::FreeGpuPartition(vector<GpuPartition *> retiredGpuPartitions)
{
    for (auto & gpuPartition : retiredGpuPartitions)
    {
        m_ActiveGpuPartitions.erase(remove(m_ActiveGpuPartitions.begin(), m_ActiveGpuPartitions.end(), gpuPartition),
                m_ActiveGpuPartitions.end());
        delete gpuPartition;
        gpuPartition = nullptr;
    }
}

RC SmcResourceController::SetGpuPartitionMode(SmcResourceController::PartitionMode  partitionMode)
{
    RC rc;

    LwRm * pLwRm = LwRmPtr().Get();
    GpuSubdevice * pGpuSubdevice = m_pGpuResource->GetGpuSubdevice();
    LW2080_CTRL_GPU_SET_PARTITIONING_MODE_PARAMS Params = {};
    Params.partitioningMode = static_cast<UINT32>(partitionMode);

    CHECK_RC(pLwRm->ControlBySubdevice(
                pGpuSubdevice,
                LW2080_CTRL_CMD_GPU_SET_PARTITIONING_MODE,
                &Params, sizeof(Params)));

    InfoPrintf(SMCID(), "Switch the Partition Mode to %s.\n", GetGpuPartitionModeName(partitionMode));
    return rc;
}

const char * SmcResourceController::GetGpuPartitionModeName(SmcResourceController::PartitionMode partitionMode)
{
    switch (partitionMode)
    {
        case SmcResourceController::PartitionMode::LEGACY:
            return "LEGACY MODE";
        case SmcResourceController::PartitionMode::MAX_PERF:
            return "MAX_PERF MODE";
        case SmcResourceController::PartitionMode::REDUCED_PERF:
            return "FAST_RECONFIG MODE";
    }

    return "Not Supported Mode";
}

map<SmcResourceController::PartitionFlag, UINT32> SmcResourceController::GetAvailablePartitionTypeCount
(
    vector<SmcResourceController::PartitionFlag> partitionFlagList
)
{
    // If no PartitionType specified then return data for all PartitionTypes
    if (partitionFlagList.empty())
    {
        partitionFlagList = m_pSmcParser->GetAllPartitionFlags();
    }

    LwRmPtr pLwRm;
    map<SmcResourceController::PartitionFlag, UINT32> partitionTypeCountMap;
    RC rc;
    GpuDevice *pGpuDev = m_pGpuResource->GetGpuDevice();
    GpuSubdevice *pGpusubdevice = pGpuDev->GetSubdevice(0);

    for (auto partitionFlag : partitionFlagList)
    {
        LW2080_CTRL_GPU_GET_PARTITION_CAPACITY_PARAMS params = { 0 };
        params.partitionFlag = partitionFlag;

        rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(pGpusubdevice),
                            LW2080_CTRL_CMD_GPU_GET_PARTITION_CAPACITY,
                            &params,
                            sizeof(params));

        if (rc != OK)
        {
            ErrPrintf(SMCID(), "%s: Get Partition Capacity RM ctrl call failed: Error %s \n",
                        __FUNCTION__, rc.Message());
            MASSERT(0);
        }

        partitionTypeCountMap[partitionFlag] = params.partitionCount;
    }

    return partitionTypeCountMap;
}

void SmcResourceController::PrintSmcInfo()
{
    if (Utl::HasInstance())
    {
        vector<vector<string>> partitionData;
        vector<vector<string>> smcEngineData;
        partitionData.push_back({"Name", "SwizzId", "PartitionType", "GpcCount", "SysPipes", "ConfigStr"});
        smcEngineData.push_back({"Name", "PartitionName", "GpcCount", "SysPipe", "GpcMask", "ExecPartitionId"});
        for (auto const & gpuPartition : m_ActiveGpuPartitions)
        {
            vector<string> onePartitionData;
            onePartitionData.push_back(gpuPartition->GetName());
            onePartitionData.push_back(Utility::StrPrintf("%d", gpuPartition->GetSwizzId()));
            onePartitionData.push_back(m_pSmcParser->GetPartitionStrFromFlag(gpuPartition->GetPartitionFlag()));
            onePartitionData.push_back(Utility::StrPrintf("%d", gpuPartition->GetGpcCount()));
            stringstream allSysPipes;
            for (auto sysPipe : gpuPartition->GetAllSyspipes())
            {
                allSysPipes << "SYS" << sysPipe << " ";
            }
            onePartitionData.push_back(allSysPipes.str());
            onePartitionData.push_back(gpuPartition->GetConfigStr());
            partitionData.push_back(move(onePartitionData));

            for (auto const & smcEngine : gpuPartition->GetActiveSmcEngines())
            {
                vector<string> oneSmcEngineData;
                oneSmcEngineData.push_back(smcEngine->GetName());
                oneSmcEngineData.push_back(gpuPartition->GetName());
                oneSmcEngineData.push_back(Utility::StrPrintf("%d", smcEngine->GetGpcCount()));
                oneSmcEngineData.push_back(Utility::StrPrintf("SYS%d", smcEngine->GetSysPipe()));
                oneSmcEngineData.push_back(Utility::StrPrintf("0x%08x", smcEngine->GetGpcMask()));
                oneSmcEngineData.push_back(Utility::StrPrintf("%d", smcEngine->GetExelwtionPartitionId()));
                smcEngineData.push_back(move(oneSmcEngineData));
            }
        }
        Utl::Instance()->TriggerTablePrintEvent("SMC Partitions Data:", move(partitionData), false);
        Utl::Instance()->TriggerTablePrintEvent("SMC Engines Data:", move(smcEngineData), false);

    }
    else
    {
        SmcPrintInfo::Instance()->Print();
    }
}

const map<SmcPrintInfo::COL_ID, string> SmcPrintInfo::m_ColumnName =
{
    {COL_SYS_PIPE, "SysPipe"},
    {COL_GPC_NUM, "GpcNumber"},
    {COL_SWIZZID, "SwizzId"},
    {COL_PARTITION_STR_PATTERN, "Gpu_Partition_Command_Line"},
    {COL_GPU_DEVICE_ID, "Gpu_Device_Id"},
    {COL_GPC_MASK, "GpcMask"}
};

SmcPrintInfo * SmcPrintInfo::m_pInstance = nullptr;

SmcPrintInfo * SmcPrintInfo::Instance()
{
    if (m_pInstance == nullptr)
    {
        m_pInstance = new SmcPrintInfo();
    }

    return m_pInstance;
}

SmcPrintInfo::SmcPrintInfo() :
    m_MaxEntryLengths(COL_NUM)
{
    InitHeader();
}

void SmcPrintInfo::InitHeader()
{
    Entry header;

    for (const auto & col : m_ColumnName)
    {
        header.m_Columns[col.first] = col.second;
        m_MaxEntryLengths[col.first] = static_cast<UINT32>(col.second.size());
    }

    m_Entries.push_back(std::move(header));
}

void SmcPrintInfo::AddSmcEngine(SmcEngine * pSmcEngine)
{
    m_Entries.emplace_back();

    SetSysPipe(pSmcEngine->GetSysPipe());
    SetGpcNum(pSmcEngine->GetGpcCount());
    SetSwizzId(pSmcEngine);
    SetStrPattern(pSmcEngine);
    SetGpuDeviceId(pSmcEngine);
    SetGpcMask(pSmcEngine->GetGpcMask());
}

void SmcPrintInfo::SetSysPipe(SmcResourceController::SYS_PIPE sys)
{
    auto & line = m_Entries.back();
    string sysPipe = Utility::StrPrintf("SYS%d", sys);
    line.m_Columns[COL_SYS_PIPE] = sysPipe;
    UpdateMaxLength(COL_SYS_PIPE, static_cast<UINT32>(sysPipe.size()));
}

void SmcPrintInfo::SetGpcNum(const UINT32 gpcNum)
{
    auto & line = m_Entries.back();
    string gpcCount = Utility::StrPrintf("%d", gpcNum);
    line.m_Columns[COL_GPC_NUM] = gpcCount;
    UpdateMaxLength(COL_GPC_NUM, static_cast<UINT32>(gpcCount.size()));
}

void SmcPrintInfo::SetSwizzId(const SmcEngine * pSmcEngine)
{
    auto & line = m_Entries.back();
    GpuPartition * pGpuPartition = pSmcEngine->GetGpuPartition();
    UINT32 swizzId = pGpuPartition->GetSwizzId();
    string swizzIdStr = Utility::StrPrintf("%d", swizzId);
    line.m_Columns[COL_SWIZZID] = swizzIdStr;
    UpdateMaxLength(COL_SWIZZID, static_cast<UINT32>(swizzIdStr.size()));
}

void SmcPrintInfo::SetStrPattern(const SmcEngine * pSmcEngine)
{
    auto & line = m_Entries.back();
    GpuPartition * pGpuPartition = pSmcEngine->GetGpuPartition();
    string pattern = pGpuPartition->GetConfigStr();
    line.m_Columns[COL_PARTITION_STR_PATTERN] = pattern;
    UpdateMaxLength(COL_PARTITION_STR_PATTERN, static_cast<UINT32>(pattern.size()));
}

void SmcPrintInfo::SetGpuDeviceId(const SmcEngine * pSmcEngine)
{
    auto & line = m_Entries.back();
    GpuPartition * pGpuPartition = pSmcEngine->GetGpuPartition();
    GpuDevice * pGpuDevice = pGpuPartition->GetLWGpuResource()->GetGpuDevice();
    UINT32 Id = pGpuDevice->DeviceId();
    string IdStr = Utility::StrPrintf("%d", Id);
    line.m_Columns[COL_GPU_DEVICE_ID] = IdStr;
    UpdateMaxLength(COL_GPU_DEVICE_ID, static_cast<UINT32>(IdStr.size()));
}

void SmcPrintInfo::SetGpcMask(const UINT32 gpcMask)
{
    auto & line = m_Entries.back();
    string gpcMaskStr = Utility::StrPrintf("0x%08x", gpcMask);
    line.m_Columns[COL_GPC_MASK] = gpcMaskStr;
    UpdateMaxLength(COL_GPC_MASK, static_cast<UINT32>(gpcMaskStr.size()));
}

void SmcPrintInfo::UpdateMaxLength(COL_ID colID, UINT32 length)
{
    m_MaxEntryLengths[colID] = max(m_MaxEntryLengths[colID], length);
}

void SmcPrintInfo::Print()
{
    InfoPrintf(SMCID(), "\n");
    InfoPrintf(SMCID(), "Alloc GpuPartitions:\n");
        char lineFormat[8];
    char entryStr[m_MaxEntryLength];
    for (auto & entry : m_Entries)
    {
        string line;
        for (int i = COL_SYS_PIPE; i < COL_NUM; ++i)
        {
            snprintf(lineFormat, 8, "%%%ds", m_MaxEntryLengths[i] + 1);
            snprintf(entryStr, m_MaxEntryLength, lineFormat, entry.m_Columns[i].c_str());
            line += entryStr;
        }
        InfoPrintf(SMCID(), "%s\n", line.c_str());
    }

    InfoPrintf(SMCID(), "\n");
}

LwRm* SmcResourceController::GetProfilingLwRm()
{
    if (m_pProfilingLwRm == nullptr)
    {
        MASSERT(m_ProfilingHandle == 0);
        RC rc;

        m_pProfilingLwRm = LwRmPtr::GetFreeClient();
        MASSERT(m_pProfilingLwRm);

        if (OK != (rc = m_pGpuResource->GetGpuDevice()->Alloc(m_pProfilingLwRm)))
        {
            ErrPrintf(SMCID(), 
                "Allocation of SMC Profling RM client failed (%s)\n",
                rc.Message());
            MASSERT(0);
        }

        LWC637_ALLOCATION_PARAMETERS params = { };
        params.swizzId = LWC637_DEVICE_PROFILING_SWIZZID;

        if (OK != 
            (rc = m_pProfilingLwRm->Alloc(
                m_pProfilingLwRm->GetSubdeviceHandle(m_pGpuResource->GetGpuSubdevice()),
                &m_ProfilingHandle, 
                AMPERE_SMC_PARTITION_REF, 
                &params)))
        {
            ErrPrintf(SMCID(), 
                "Subscription of SMC Profiling RM client to Profiling SwizzId failed (%s)\n",
                rc.Message());
            MASSERT(0);
        }
        if (OK != 
            (rc = m_pGpuResource->GetGpuDevice()->AllocSingleClientStuff(
                m_pProfilingLwRm, 
                m_pGpuResource->GetGpuSubdevice())))
        {
            ErrPrintf(SMCID(), 
                "Allocation of SMC Profiling RM client's objects failed (%s)\n",
                rc.Message());
            MASSERT(0);
        }
    }
    MASSERT(m_ProfilingHandle != 0);
    return m_pProfilingLwRm;
}
