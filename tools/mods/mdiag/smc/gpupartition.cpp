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

#include "mdiag/smc/gpupartition.h"
#include "mdiag/smc/smcengine.h"
#include "mdiag/sysspec.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpudev.h"
#include "ctrl/ctrl2080/ctrl2080gpu.h"
#include "gpu/include/gpumgr.h"
#include "mdiag/smc/smcresourcecontroller.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/utils.h"
#include "mdiag/vgpu_migration/vgpu_migration.h"
#include "mdiag/advschd/policymn.h"
#include "mdiag/utl/utl.h"
#include "mdiag/utils/mdiag_xml.h"
#include "ctrl/ctrlc637.h"

class PmSmcEngine;
#include <algorithm>
#include <sstream>
#include <regex>

DECLARE_MSG_GROUP(Mdiag, Gpu, SMC)
#define SMCID() __MSGID__(Mdiag, Gpu, SMC)

UINT32 GpuPartition::s_InternalNameIdx = 0;
vector<string> GpuPartition::s_GpuPartitionNames;

// Checks and asserts if the user-provided name for GpuPartition matches the
// MODS internal format for GpuPartitions
// If no name is provided by the user then set up the name to MODS internal
// name for debugging purposes
void GpuPartition::SetCheckName()
{
    if (m_Name.size() == 0)
    {
        m_Name = Utility::StrPrintf("%s%d", MODS_PARTITION_NAME, s_InternalNameIdx++);
    }
    else
    {
        // VFs will use the same name for GpuPartition as PF objects
        // If PF GpuPartition object uses MODS internal name then VF will create
        // a GpuPartition object with MODS internal name- hence this check is
        // not needed
        if (!Platform::IsVirtFunMode())
        {
            string partitionRegex = MODS_PARTITION_NAME;
            partitionRegex += "[0-9]+";
            if (std::regex_match(m_Name, std::regex(partitionRegex)))
            {
                ErrPrintf(SMCID(), "%s: Partition %s in -smc_mem uses MODS internal name %s, which is not allowed\n",
                            __FUNCTION__, m_ConfigStr.c_str(), m_Name.c_str());
                MASSERT(0);
            }
        }
    }
    AddGpuPartitionName(m_Name);
}

GpuPartition::GpuPartition
(
    const string & configStr,
    LWGpuResource * pGpuResource,
    const string & partitionName,
    bool bUseAllGpcs
) :
    m_SwizzId(0),
    m_CeCount(0),
    m_LwDecCount(0),
    m_LwEncCount(0),
    m_LwJpgCount(0),
    m_OfaCount(0),
    m_GpcCount(0),
    m_IlwalidGpcCount(0),
    m_PartitionFlag(SmcResourceController::PartitionFlag::INVALID),
    m_ConfigStr(configStr),
    m_pGpuResource(pGpuResource),
    m_IsValid(true),
    m_bUseAllGpcs(bUseAllGpcs),
    m_SmcEngineCount(0),
    m_Name(partitionName)
{
    MASSERT(m_pGpuResource != nullptr);
    SetCheckName();
}

GpuPartition::GpuPartition
(
    const string & configStr,
    UINT32 gpcCount,
    UINT32 ilwalidGpcCount,
    SmcResourceController::PartitionFlag partitionFlag,
    LWGpuResource * pGpuResource,
    bool isValid,
    const string & partitionName
) :
    m_SwizzId(0),
    m_CeCount(0),
    m_LwDecCount(0),
    m_LwEncCount(0),
    m_LwJpgCount(0),
    m_OfaCount(0),
    m_GpcCount(gpcCount),
    m_IlwalidGpcCount(ilwalidGpcCount),
    m_PartitionFlag(partitionFlag),
    m_ConfigStr(configStr),
    m_pGpuResource(pGpuResource),
    m_IsValid(isValid),
    m_bUseAllGpcs(false),
    m_SmcEngineCount(0),
    m_Name(partitionName)
{
    MASSERT(m_pGpuResource != nullptr);
    SetCheckName();
}

GpuPartition::~GpuPartition()
{
    RemoveSmcEngines(m_ActiveSmcEngines);
    RemoveGpuPartitionName(m_Name);
}

bool GpuPartition::operator ==(const GpuPartition & gpuPartition)
{
    vector<SmcEngine *> activeSmcEngines = const_cast<GpuPartition &>(gpuPartition).GetActiveSmcEngines();
    if (activeSmcEngines.size() != m_ActiveSmcEngines.size())
    {
        return false;
    }

    for (const auto & smcEngine : m_ActiveSmcEngines)
    {
        for (const auto & newSmcEngine : activeSmcEngines)
        {
            if (*smcEngine == *newSmcEngine)
            {
                break;
            }
        }
        return false;
    }

    if (m_Name != gpuPartition.GetName())
    {
        return false;
    }

    return true;
}

RC GpuPartition::AllocSmcEngines(vector<SmcEngine *> & smcEngines)
{
    RC rc;
    if (!IsValid() || smcEngines.size() == 0)
    {
        return rc;
    }

    if (Platform::IsVirtFunMode())
    {
        for (auto & smcEngine : smcEngines)
        {
            smcEngine->SetAlloc();
            CHECK_RC(m_pGpuResource->AllocRmClient(smcEngine));
            if (Utl::HasInstance())
            {
                Utl::Instance()->AddSmcEngine(this, smcEngine);
            }
        }
        return rc;
    }

    GpuDevice *pGpuDev = m_pGpuResource->GetGpuDevice();
    SmcPrintInfo *pSmcPrintInfo = SmcPrintInfo::Instance();
    LwRm *pLwRm = m_pGpuResource->GetLwRmPtr(m_SwizzId);

    LWC637_CTRL_EXEC_PARTITIONS_CREATE_PARAMS params = { };

    CHECK_RC(GetNonGrEngineCount());
    // Handle -smc_partitioning_sys0_only
    if ((m_ActiveSmcEngines.size() == 1) && (m_ActiveSmcEngines[0]->UseAllGpcs()))
    {
        params.execPartInfo[params.execPartCount].gpcCount = GetGpcCount();
        params.execPartInfo[params.execPartCount].ceCount = m_CeCount;
        params.execPartInfo[params.execPartCount].lwDecCount = m_LwDecCount;
        params.execPartInfo[params.execPartCount].lwEncCount = m_LwEncCount;
        params.execPartInfo[params.execPartCount].lwJpgCount = m_LwJpgCount;
        params.execPartInfo[params.execPartCount].ofaCount = m_OfaCount;
        params.execPartInfo[params.execPartCount].sharedEngFlag =
            LWC637_CTRL_EXEC_PARTITIONS_SHARED_FLAG_CE |
            LWC637_CTRL_EXEC_PARTITIONS_SHARED_FLAG_LWDEC |
            LWC637_CTRL_EXEC_PARTITIONS_SHARED_FLAG_LWENC |
            LWC637_CTRL_EXEC_PARTITIONS_SHARED_FLAG_LWJPG |
            LWC637_CTRL_EXEC_PARTITIONS_SHARED_FLAG_OFA;
        params.execPartCount++;
    }
    else
    {
        for (auto & smcEngine : smcEngines)
        {
            if (smcEngine->IsAlloc())
            {
                // If smcEngine is already allocated, skip it.
                continue;
            }

            MASSERT(GetGpcCount() >= smcEngine->GetGpcCount());
            params.execPartInfo[params.execPartCount].gpcCount = smcEngine->GetGpcCount();
            params.execPartInfo[params.execPartCount].ceCount = m_CeCount;
            params.execPartInfo[params.execPartCount].lwDecCount = m_LwDecCount;
            params.execPartInfo[params.execPartCount].lwEncCount = m_LwEncCount;
            params.execPartInfo[params.execPartCount].lwJpgCount = m_LwJpgCount;
            params.execPartInfo[params.execPartCount].ofaCount = m_OfaCount;
            params.execPartInfo[params.execPartCount].sharedEngFlag =
                LWC637_CTRL_EXEC_PARTITIONS_SHARED_FLAG_CE |
                LWC637_CTRL_EXEC_PARTITIONS_SHARED_FLAG_LWDEC |
                LWC637_CTRL_EXEC_PARTITIONS_SHARED_FLAG_LWENC |
                LWC637_CTRL_EXEC_PARTITIONS_SHARED_FLAG_LWJPG |
                LWC637_CTRL_EXEC_PARTITIONS_SHARED_FLAG_OFA;
            params.execPartCount++;
        }
    }

    // SMC is not compatible with SLI
    MASSERT(pGpuDev->GetNumSubdevices() == 1);
    rc = pLwRm->Control(m_pGpuResource->GetGpuPartitionHandle(this),
            LWC637_CTRL_CMD_EXEC_PARTITIONS_CREATE,
            &params,
            sizeof(params));

    if (rc != OK)
    {
        ErrPrintf(SMCID(), "Allocation of SmcEngine failed: Error %s \n",
            rc.Message());
        MASSERT(0);
        return rc;
    }

    UINT32 exelwtionPartitionIdx = 0;
    // Update status of smcEngine, especially those allocated this time.
    for (auto & smcEngine : smcEngines)
    {
        if (!smcEngine->IsAlloc())
        {
            smcEngine->SetExelwtionPartitionId(params.execPartId[exelwtionPartitionIdx]);
            exelwtionPartitionIdx++;
            smcEngine->SetAlloc();
            CHECK_RC(m_pGpuResource->AllocRmClient(smcEngine));
            if (Utl::HasInstance())
            {
                Utl::Instance()->AddSmcEngine(this, smcEngine);
            }
        }
        LW2080_CTRL_GR_GET_PHYS_GPC_MASK_PARAMS physGpcParams = {0};
        physGpcParams.physSyspipeId = smcEngine->GetSysPipe();
        CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(pGpuDev->GetSubdevice(0)),
                            LW2080_CTRL_CMD_GR_GET_PHYS_GPC_MASK,
                            &physGpcParams,
                            sizeof(physGpcParams)));
        smcEngine->SetGpcMask(physGpcParams.gpcMask);
        if (gXML)
        {
            XD->XMLStartStdInline("<SysPipeGpcMask");
            XD->outputAttribute("sysPipe", physGpcParams.physSyspipeId);
            string gpcMaskStr = Utility::StrPrintf("0x%08x", physGpcParams.gpcMask);
            XD->outputAttribute("gpcMask", gpcMaskStr.c_str());
            XD->XMLEndLwrrent();
        }
        // Add to the print table
        pSmcPrintInfo->AddSmcEngine(smcEngine);
        // Remove the SysPipe from m_AvailableSyspipes
        m_AvailableSyspipes.erase(
            remove(m_AvailableSyspipes.begin(), m_AvailableSyspipes.end(), smcEngine->GetSysPipe()),
            m_AvailableSyspipes.end());
    }

    return rc;
}

RC GpuPartition::FreeSmcEngines(vector<SmcEngine *> & smcEngines)
{
    RC rc;
    if (!IsValid() || smcEngines.size() == 0)
    {
        return rc;
    }

    if (Platform::IsVirtFunMode())
    {
        for (const auto & smcEngine : smcEngines)
        {
            CHECK_RC(m_pGpuResource->FreeRmClient(smcEngine));
        }
        return rc;
    }

    LwRm *pLwRm = m_pGpuResource->GetLwRmPtr(m_SwizzId);

    LWC637_CTRL_EXEC_PARTITIONS_DELETE_PARAMS params = { };

    // Remove policy manager side resource to inactiveSmcEngines
    if (PolicyManager::HasInstance())
    {
        PolicyManager *pPolicyMgr = PolicyManager::Instance();
        for (auto pSmcEngine : smcEngines)
        {
            CHECK_RC(pPolicyMgr->RemoveSmcEngine(m_pGpuResource->GetLwRmPtr(pSmcEngine)));
        }
    }

    for (const auto & smcEngine : smcEngines)
    {
        CHECK_RC(m_pGpuResource->FreeRmClient(smcEngine));
        params.execPartId[params.execPartCount] = smcEngine->GetExelwtionPartitionId();
        params.execPartCount++;
    }

    rc = pLwRm->Control(m_pGpuResource->GetGpuPartitionHandle(this),
            LWC637_CTRL_CMD_EXEC_PARTITIONS_DELETE,
            &params,
            sizeof(params));
    if (rc != OK)
    {
        ErrPrintf(SMCID(), "Freeing of SmcEngine failed: Error %s \n",
                    rc.Message());
        MASSERT(0);
        return rc;
    }

    for (auto & smcEngine : smcEngines)
    {
        m_AvailableSyspipes.push_back(smcEngine->GetSysPipe());
    }

    return rc;
}

RC GpuPartition::AllocAllSmcEngines()
{
    RC rc;

    CHECK_RC(AllocSmcEngines(m_ActiveSmcEngines));
    CHECK_RC(UpdateGpuPartitionInfo());
    if (Utl::HasInstance() && IsValid())
    {
        Utl::Instance()->TriggerSmcPartitionCreatedEvent(this);
    }

    return rc;
}

RC GpuPartition::FreeAllSmcEngines()
{
    RC rc;

    // Free SmcRegHal before the SmcEngine is removed
    for (auto & smcEngine : m_ActiveSmcEngines)
    {
        m_pGpuResource->FreeSmcRegHal(smcEngine);
    }

    CHECK_RC(FreeSmcEngines(m_ActiveSmcEngines));
    RemoveSmcEngines(m_ActiveSmcEngines);
    return rc;
}

UINT32 GpuPartition::GetGpcCount()
{
    // After reconfigurate the GpuPartition, need to update the gpc count
    if (m_GpcCount != 0)
        return m_GpcCount;

    for(const auto & it : m_ActiveSmcEngines)
    {
        m_GpcCount += it->GetGpcCount();
    }

    m_GpcCount += m_IlwalidGpcCount;

    return m_GpcCount;
}

SmcEngine * GpuPartition::GetSmcEngine(UINT32 smcEngineId)
{
    for(const auto & it : m_ActiveSmcEngines)
    {
        if (it->GetSmcEngineId() == smcEngineId)
        {
            return it;
        }
    }

    DebugPrintf(SMCID(), "%s: Can't find matched smc Engine which Id = %d.\n",
                __FUNCTION__, smcEngineId);
    return nullptr;
}

SmcEngine * GpuPartition::GetSmcEngine(SmcResourceController::SYS_PIPE sys)
{
    for (const auto & it : m_ActiveSmcEngines)
    {
        if (it->GetSysPipe() == sys)
        {
            return it;
        }
    }

    DebugPrintf(SMCID(), "%s: Can't find matched sys pipe which sys_pipe = SYS%d.\n",
                __FUNCTION__, sys);
    return nullptr;
}

SmcEngine * GpuPartition::GetSmcEngineByName(const string& name)
{
    for (const auto & it : m_ActiveSmcEngines)
    {
        if (it->GetName() == name)
        {
            return it;
        }
    }
    DebugPrintf(SMCID(), "%s: Can't find smc engine with name = %s.\n",
                __FUNCTION__, name.c_str());
    return nullptr;
}

void GpuPartition::AddSmcEngine(SmcEngine * pSmcEngine)
{
    if (GetSmcEngine(pSmcEngine->GetSysPipe()) == nullptr)
    {
        m_ActiveSmcEngines.push_back(pSmcEngine);
        UpdateSmcEngineId(pSmcEngine);
        InfoPrintf(SMCID(), "%s: Add smc Engine which sys_pipe = SYS%d, GPC count = %s\n",
                __FUNCTION__, pSmcEngine->GetSysPipe(),
                pSmcEngine->GetGpcCount() == 0 ? "not specified" : to_string(pSmcEngine->GetGpcCount()).c_str());
    }
    else
    {
        InfoPrintf(SMCID(), "%s: Add an existed smc Engine which sys_pipe = SYS%d, GPC count = %s\n",
                __FUNCTION__, pSmcEngine->GetSysPipe(),
                pSmcEngine->GetGpcCount() == 0 ? "not specified" : to_string(pSmcEngine->GetGpcCount()).c_str());
    }
    if (GetMDiagvGpuMigration()->IsEnabled())
    {
        MDiagVmScope::GetSmcResources()->AddSmcEngine(pSmcEngine);
    }
}

void GpuPartition::RemoveSmcEngines(vector<SmcEngine *> retiredSmcEngines)
{
    for (auto & smcEngine : retiredSmcEngines)
    {
        m_ActiveSmcEngines.erase(remove(m_ActiveSmcEngines.begin(), m_ActiveSmcEngines.end(), smcEngine),
                m_ActiveSmcEngines.end());
        InfoPrintf(SMCID(), "%s: Removed SmcEngine on SYS%d with GPC count = %d\n",
                __FUNCTION__, smcEngine->GetSysPipe(), smcEngine->GetGpcCount());
        delete smcEngine;
        smcEngine = nullptr;
    }
}

RC GpuPartition::UpdateGpuPartitionInfo()
{
    RC rc;
    unsigned int idx;

    if (!IsValid())
        return rc;

    LW2080_CTRL_GPU_GET_PARTITIONS_PARAMS params = {0};
    params.bGetAllPartitionInfo = true;

    // Might use RM client created for each partition
    LwRmPtr pLwRm;
    GpuDevice * pGpuDev = m_pGpuResource->GetGpuDevice();
    GpuSubdevice * pGpusubdevice = pGpuDev->GetSubdevice(0);
    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(pGpusubdevice),
             LW2080_CTRL_CMD_GPU_GET_PARTITIONS,
             &params,
             sizeof(params)));

    // Check if only 1 partition information is returned
    if (Platform::IsVirtFunMode())
    {
        if (params.validPartitionCount != 1)
        {
            ErrPrintf(SMCID(), "%s: RM returns %d GPU partition for VF SMC MODS. It should be 1!\n",
                __FUNCTION__, params.validPartitionCount);
            m_IsValid = false;
            return RC::SOFTWARE_ERROR;
        }
        m_SwizzId = params.queryPartitionInfo[0].swizzId;
    }

    for (idx = 0; idx < params.validPartitionCount; ++idx)
    {
        if (params.queryPartitionInfo[idx].bValid &&
            (params.queryPartitionInfo[idx].swizzId) == m_SwizzId)
        {
            break;
        }
    }

    if (idx >= params.validPartitionCount)
    {
        ErrPrintf(SMCID(), "%s: GPU partition which swizid is %u is not allocated successfully because RM reports it's invalid!\n",
                __FUNCTION__, m_SwizzId);
        m_IsValid = false;
        return RC::SOFTWARE_ERROR;
    }

    m_IsValid = true;
    m_GpcCount = params.queryPartitionInfo[idx].gpcCount;

    DebugPrintf(SMCID(), "%s: GpuPartition (swizid %d) has %d GPC[s]\n",
            __FUNCTION__, m_SwizzId, m_GpcCount);


    for (auto & smcEngine : m_ActiveSmcEngines)
    {
        int smcId = smcEngine->GetSmcEngineId();
        if (params.queryPartitionInfo[idx].veidsPerGr[smcId] == 0)
        {
            ErrPrintf(SMCID(), "%s: GpuPartition (swizid %u) could not be allocated because RM reports available 0 VEIDs in SmcEngine %u\n",
                        __FUNCTION__, m_SwizzId, params.queryPartitionInfo[idx].veidsPerGr[smcId], smcId);
        }
        smcEngine->SetVeidCount(params.queryPartitionInfo[idx].veidsPerGr[smcId]);
    }

    return rc;
}

RC GpuPartition::GetNonGrEngineCount()
{
    MASSERT(Platform::IsPhysFunMode() || Platform::IsDefaultMode());

    RC rc;
    LW2080_CTRL_GPU_GET_PARTITIONS_PARAMS params = { };

    LwRm *pLwRm = m_pGpuResource->GetLwRmPtr(m_SwizzId);
    MASSERT(pLwRm);

    GpuDevice* pGpuDev = m_pGpuResource->GetGpuDevice();
    GpuSubdevice* pGpusubdevice = pGpuDev->GetSubdevice(0);
    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(pGpusubdevice),
        LW2080_CTRL_CMD_GPU_GET_PARTITIONS,
        &params,
        sizeof(params)));

    MASSERT(params.queryPartitionInfo[0].bValid);

    m_CeCount = params.queryPartitionInfo[0].ceCount;
    m_LwDecCount = params.queryPartitionInfo[0].lwDecCount;
    m_LwEncCount = params.queryPartitionInfo[0].lwEncCount;
    m_LwJpgCount = params.queryPartitionInfo[0].lwJpgCount;
    m_OfaCount = params.queryPartitionInfo[0].lwOfaCount;

    return rc;
}

UINT32 GpuPartition::GetSmcEngineGpcCount(UINT32 smcEngineId)
{
    SmcEngine * pSmcEngine = GetSmcEngine(smcEngineId);
    if (pSmcEngine != nullptr)
    {
        return pSmcEngine->GetGpcCount();
    }

    if (Platform::IsVirtFunMode())
    {
        LW2080_CTRL_GPU_GET_PARTITIONS_PARAMS params = {0};
        unsigned int idx;
        params.bGetAllPartitionInfo = true;

        // Might use RM client created for each partition
        LwRmPtr pLwRm;
        GpuDevice * pGpuDev = m_pGpuResource->GetGpuDevice();
        GpuSubdevice * pGpusubdevice = pGpuDev->GetSubdevice(0);
        RC rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(pGpusubdevice),
                    LW2080_CTRL_CMD_GPU_GET_PARTITIONS,
                    &params,
                    sizeof(params));
        if (rc != OK)
        {
            ErrPrintf(SMCID(), "Can't get the Gpu Partition Information correctly.\n");
            return 0;
        }

        for (idx = 0; idx < params.validPartitionCount; ++idx)
        {
            if (params.queryPartitionInfo[idx].bValid &&
                (params.queryPartitionInfo[idx].swizzId) == m_SwizzId)
            {
                break;
            }
        }

        if (idx >= params.validPartitionCount)
        {
            ErrPrintf(SMCID(), "Can't get the Gpu Partition Information correctly.\n");
            return 0;
        }

        return params.queryPartitionInfo[idx].gpcsPerGr[smcEngineId];
    }

    return 0;
}

SmcResourceController::SYS_PIPE GpuPartition::GetSmcEngineSysPipe(UINT32 smcEngineId)
{
    SmcEngine * pSmcEngine = GetSmcEngine(smcEngineId);
    if (pSmcEngine != nullptr)
    {
        return pSmcEngine->GetSysPipe();
    }

    if (Platform::IsVirtFunMode())
    {
        if (smcEngineId < m_AllSyspipes.size())
        {
            SmcResourceController::SYS_PIPE syspipe = m_AllSyspipes[smcEngineId];
            return syspipe;
        }
    }

    return SmcResourceController::NOT_SUPPORT;
}

// Get SmcEngine for the absolute GR engineId
SmcEngine* GpuPartition::GetSmcEngineByType(UINT32 engineId)
{
    UINT32 smcEngineId = ~0U;
    if (MDiagUtils::IsGraphicsEngine(engineId))
    {
        smcEngineId =  MDiagUtils::GetGrEngineOffset(engineId);
    }
    else
    {
        MASSERT(!"Cannot get SmcEngineOffset for non-Graphics engine\n");
        return nullptr;
    }

    return GetSmcEngine(smcEngineId);
}

void GpuPartition::SetAllSyspipes(vector<SmcResourceController::SYS_PIPE> syspipes)
{
    m_AllSyspipes.swap(syspipes);
    m_AvailableSyspipes = m_AllSyspipes;

    auto& __OUTER_FUNCTION__ = __FUNCTION__;
    for_each(m_AllSyspipes.begin(), m_AllSyspipes.end(),
             [this, &__OUTER_FUNCTION__](SmcResourceController::SYS_PIPE syspipe)
             {
                DebugPrintf(SMCID(), "%s: Add sys%u into GPU partition %s\n",
                            __OUTER_FUNCTION__, syspipe, this->m_Name.c_str());
             });
}

void GpuPartition::UpdateSmcEngineId(SmcEngine * pSmcEngine)
{
    for (UINT32 i = 0; i < m_AllSyspipes.size(); i++)
    {
        if (pSmcEngine->GetSysPipe() == m_AllSyspipes[i])
        {
            pSmcEngine->SetSmcEngineId(i);
            break;
        }
    }
    if (pSmcEngine->GetSmcEngineId() == SmcEngine::m_UknownSmcId)
    {
        PrintPartitionErrorMessage(pSmcEngine->GetSysPipe());
        MASSERT(0);
    }

}

bool GpuPartition::SmcEngineExists
(
    SmcEngine * pNewAddedSmcEngine
)
{
    for (const auto & pSmcEngine : m_ActiveSmcEngines)
    {
        if (*pSmcEngine == *pNewAddedSmcEngine)
        {
            return true;
        }
    }

    return false;
}

void GpuPartition::SanityCheck(LwRm* pLwRm, GpuSubdevice* pSubdev) const
{
    // PF will perform the sanity check
    // RM will always return SYS0 for VFs else migration will be broken
    // Skip Sanity check for -smc_mem as well since user does not have the 
    // ability to specify syspipe with that arg
    if (Platform::IsVirtFunMode() ||
        GetSmcResourceController()->IsSmcMemApi())
    {
        return;
    }

    vector<SmcResourceController::SYS_PIPE> allSyspipes;

    LW2080_CTRL_GPU_GET_PHYS_SYS_PIPE_IDS_PARAMS params = {0};

    params.swizzId = GetSwizzId();
    if (pLwRm->LwRm::ControlBySubdevice(pSubdev,
                                        LW2080_CTRL_CMD_GPU_GET_PHYS_SYS_PIPE_IDS,
                                        &params, sizeof(params)) != OK)
    {
        ErrPrintf(SMCID(), "%s: LW2080_CTRL_CMD_GPU_GET_PHYS_SYS_PIPE_IDS failed for the partition with swizid=%d\n",
                    __FUNCTION__, GetSwizzId());
        MASSERT(0);
    }

    for (UINT32 i = 0; i < params.physSysPipeCount; i++)
    {
        allSyspipes.push_back(static_cast<SmcResourceController::SYS_PIPE>(params.physSysPipeId[i]));
    }

    // Check each SmcEngines' SysPipe
    for (auto const & pSmcEngine : GetActiveSmcEngines())
    {
        UINT32 smcEngineId = pSmcEngine->GetSmcEngineId();
        MASSERT(smcEngineId < allSyspipes.size());

        if (pSmcEngine->GetSysPipe() != allSyspipes[smcEngineId])
        {
            PrintPartitionErrorMessage(pSmcEngine->GetSysPipe());
            MASSERT(0);
        }
    }

    // Check Partition's Stored Available SysPipe
    for (auto sysPipe : m_AllSyspipes)
    {
        if (find(allSyspipes.begin(), allSyspipes.end(), sysPipe) ==
            allSyspipes.end())
        {
            PrintPartitionErrorMessage(sysPipe);
            MASSERT(0);
        }
    }
}

SmcResourceController* GpuPartition::GetSmcResourceController() const
{
    return m_pGpuResource->GetSmcResourceController();
}

void GpuPartition::PrintPartitionErrorMessage(SmcResourceController::SYS_PIPE sysPipe) const
{
    stringstream allSysPipes;

    for (auto sysPipe : m_AllSyspipes)
    {
        allSysPipes << " SYS" << sysPipe;
    }

    ErrPrintf(SMCID(), "Expected one of%s in the requested partition config %s, which has been allocated as swizid %d\n",
                allSysPipes.str().c_str(), m_ConfigStr.c_str(), GetSwizzId());
    ErrPrintf(SMCID(), "Requested SmcEngine on SysPipe %d does not match any of the available syspipes of the allocated partition\n",
                sysPipe);
    ErrPrintf(SMCID(), "Please check -smc_partitioning/-smc_mem and -floorsweep arguments\n");
    ErrPrintf(SMCID(), "-smc_partitioning/-smc_mem: %s\n", GetSmcResourceController()->GetSmcConfigStr().c_str());

    // GPC mask and Syspipe mask are not populated for VF
    if (!Platform::IsVirtFunMode())
    {
        ErrPrintf(SMCID(), "GPU SysPipeMask0x%x GPU GpcMask:0x%x\n",
                    GetSmcResourceController()->GetSysPipeMask(), GetSmcResourceController()->GetGpcMask());
    }
}

void GpuPartition::SetSyspipes()
{
    LwRm* pLwRm = m_pGpuResource->GetLwRmPtr(this);
    MASSERT(pLwRm);
    LW2080_CTRL_GPU_GET_PHYS_SYS_PIPE_IDS_PARAMS params = {0};
    params.swizzId = m_SwizzId;
    RC rc = pLwRm->ControlBySubdevice(m_pGpuResource->GetGpuSubdevice(),
                                      LW2080_CTRL_CMD_GPU_GET_PHYS_SYS_PIPE_IDS,
                                      &params, sizeof(params));
    if (rc != OK)
    {
        ErrPrintf(SMCID(), "%s : Failed to ilwoke LW2080_CTRL_CMD_GPU_GET_PHYS_SYS_PIPE_IDS for swizzid %d, RC: %d\n",
                    __FUNCTION__, m_SwizzId, rc.Get());
        MASSERT(0);
    }

    stringstream partitionSysPipes;
    vector<SmcResourceController::SYS_PIPE> allocatedSyspipes;
    for (UINT32 i = 0; i < params.physSysPipeCount; i++)
    {
        allocatedSyspipes.push_back(static_cast<SmcResourceController::SYS_PIPE>(params.physSysPipeId[i]));
        partitionSysPipes << " SYS" << params.physSysPipeId[i];
    }

    DebugPrintf(SMCID(), "%s : SwizzId %d available syspipes -%s\n",
        __FUNCTION__, m_SwizzId, partitionSysPipes.str().c_str());

    SetAllSyspipes(allocatedSyspipes);
}

void GpuPartition::AddGpuPartitionName(string addGpuPartitionName)
{
    auto pos = find(s_GpuPartitionNames.begin(), s_GpuPartitionNames.end(), addGpuPartitionName);
    if (pos != s_GpuPartitionNames.end())
    {
        ErrPrintf(SMCID(), "%s: GpuPartition's name %s already used previously\n",
                    __FUNCTION__, addGpuPartitionName.c_str());
        MASSERT(0);
    }
    else
    {
        s_GpuPartitionNames.push_back(move(addGpuPartitionName));
    }
}

void GpuPartition::RemoveGpuPartitionName(string removeGpuPartitionName)
{
    auto pos = find(s_GpuPartitionNames.begin(), s_GpuPartitionNames.end(), removeGpuPartitionName);
    if (pos != s_GpuPartitionNames.end())
    {
        s_GpuPartitionNames.erase(pos);
    }
    else
    {
        ErrPrintf(SMCID(), "%s: GpuPartition (%s) being removed not found in the names list \n",
                    __FUNCTION__, removeGpuPartitionName.c_str());
        MASSERT(0);
    }
}
