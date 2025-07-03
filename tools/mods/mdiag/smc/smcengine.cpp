/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/massert.h"
#include "mdiag/smc/smcengine.h"
#include "mdiag/smc/smcresourcecontroller.h"
#include "mdiag/smc/gpupartition.h"
#include "mdiag/sysspec.h"
#include "lwmisc.h"
#include "core/include/utility.h"
#include <regex>

DECLARE_MSG_GROUP(Mdiag, Gpu, SMC)
#define SMCID() __MSGID__(Mdiag, Gpu, SMC)

const UINT32 SmcEngine::m_UknownSmcId;

SmcEngine* SmcEngine::s_pFakeSmcEngine = nullptr;
UINT32 SmcEngine::s_InternalNameIdx = 0;
vector<string> SmcEngine::s_SmcEngineNames;

// Checks and asserts if the user-provided name for SmcEngine matches the MODS 
// internal format for SmcEngines
// If no name is provided by the user then set up the name to MODS internal
// name for debugging purposes
void SmcEngine::SetCheckName()
{
    if (m_Name.size() == 0)
    {
        m_Name = Utility::StrPrintf("%s%d", MODS_SMCENG_NAME, s_InternalNameIdx++);
    }
    else 
    {
        // VFs will use the same name for SmcEngine as PF objects
        // If PF SmcEngine object uses MODS internal name then VF will create 
        // an SmcEngine object with MODS internal name- hence this check is
        // not needed
        if (!Platform::IsVirtFunMode())
        {
            string smcEngineRegex = MODS_SMCENG_NAME;
            smcEngineRegex += "[0-9]+";
            if (std::regex_match(m_Name, std::regex(smcEngineRegex)))
            {
                ErrPrintf(SMCID(), "%s: SmcEngine in -smc_eng uses MODS internal name %s, which is not allowed\n",
                            __FUNCTION__, m_Name.c_str());
                MASSERT(0);
            }
        }
    }

    AddSmcEngineName(m_Name);
}

SmcEngine::SmcEngine
(
    UINT32 gpcCount,
    SmcResourceController::SYS_PIPE sys,
    GpuPartition * pGpuPartition,
    string engName,
    bool bUseAllGpcs
):
    m_GpcCount(gpcCount),
    m_pGpuPartition(pGpuPartition),
    m_SysPipe(sys),
    m_SmcEngineId(m_UknownSmcId),
    m_VeidCount(0),
    m_IsAlloc(false),
    m_bUseAllGpcs(bUseAllGpcs),
    m_GpcMask(0),
    m_Name(engName),
    m_ExelwtionPartitionId(m_UknownSmcId)
{
    MASSERT(pGpuPartition != nullptr);
    if (bUseAllGpcs)
    {
        m_SmcEngineId = 0;
    }

    // Do not setup Name for FakeSmcEngine since it does not have a GpuPartition*
    if (m_GpcCount != 0)
    {
        SetCheckName();
    }
}

SmcEngine::~SmcEngine()
{
    // FakeSmcEngine does not have it's name setup
    if (m_Name.size() != 0)
    {
        RemoveSmcEngineName(m_Name);
    }
}

bool SmcEngine::operator == (const SmcEngine & smcEngine) const
{
    if (m_GpcCount == smcEngine.GetGpcCount() &&
            m_SysPipe == smcEngine.GetSysPipe() &&
            m_SmcEngineId == smcEngine.GetSmcEngineId() &&
            m_VeidCount == smcEngine.GetVeidCount() &&
            m_bUseAllGpcs == smcEngine.UseAllGpcs() &&
            m_Name == smcEngine.GetName() &&
            m_ExelwtionPartitionId == smcEngine.GetExelwtionPartitionId())
    {
        return true;
    }

    return false;
}

UINT32 SmcEngine::GetVeidCount() const
{
    return m_VeidCount;
}

void SmcEngine::SetVeidCount(UINT32 veidCount)
{
    DebugPrintf(SMCID(), "%s: GPU partition which swizid is %u SMC engine ID %u has VEID count %u.\n",
                __FUNCTION__, m_pGpuPartition->GetSwizzId(), m_SmcEngineId, veidCount);
    m_VeidCount = veidCount;
}

void SmcEngine::SetSmcEngineId(UINT32 smcEngineId)
{
    DebugPrintf(SMCID(), "%s: SMC Engine sys%u has local SMC ID %u.\n",
                __FUNCTION__, m_SysPipe, smcEngineId);
    m_SmcEngineId = smcEngineId;
}

string SmcEngine::GetInfoStr()
{
    if (this == GetFakeSmcEngine())
    {
        return "";
    }

    UINT32 swizzId = m_pGpuPartition->GetSwizzId();
    return  "(Swizzid=" + to_string(swizzId) + ", SmcEngineId=" + to_string(m_SmcEngineId) + ")";
}

SmcEngine* SmcEngine::GetFakeSmcEngine()
{
    if (s_pFakeSmcEngine == nullptr)
    {
        s_pFakeSmcEngine = new SmcEngine(0, SmcResourceController::NOT_SUPPORT, (GpuPartition*)(intptr_t)~0U, "");
    }

    return s_pFakeSmcEngine;
}

void SmcEngine::AddSmcEngineName(string addSmcEngineName)
{
    auto pos = find(s_SmcEngineNames.begin(), s_SmcEngineNames.end(), addSmcEngineName);
    if (pos != s_SmcEngineNames.end())
    {
        ErrPrintf(SMCID(), "%s: SmcEngine's name %s already used previously\n",
                    __FUNCTION__, addSmcEngineName.c_str());
        MASSERT(0);
    }
    else
    {
        s_SmcEngineNames.push_back(move(addSmcEngineName));
    }
}

void SmcEngine::RemoveSmcEngineName(string removeSmcEngineName)
{
    auto pos = find(s_SmcEngineNames.begin(), s_SmcEngineNames.end(), removeSmcEngineName);
    if (pos != s_SmcEngineNames.end())
    {
        s_SmcEngineNames.erase(pos);
    }
    else
    {
        ErrPrintf(SMCID(), "%s: SmcEngine (%s) being removed not found in the names list \n",
                    __FUNCTION__, removeSmcEngineName.c_str());
        MASSERT(0);
    }
}
