/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "core/include/rc.h"
#include "mdiag/sysspec.h"
#include "mdiag/smc/smcresourcecontroller.h"

#include "utlgpu.h"
#include "utlutil.h"
#include "utlsmcengine.h"
#include "utlgpupartition.h"
#include "utl.h"
#include "mdiag/lwgpu.h"
#include "gpu/include/gpudev.h"
#include "utlfbinfo.h"
#include "utlevent.h"
#include "utlmemctrl.h"

map<GpuPartition*, unique_ptr<UtlGpuPartition>> UtlGpuPartition::s_GpuPartitions;

void UtlGpuPartition::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("GpuPartition.CreateSmcEngine", "sysPipe", false, "Sys pipe owned by the SmcEngine created; must be in range 0..7"); //made optional for now, has to be depricated from Hopper onwards
    kwargs->RegisterKwarg("GpuPartition.CreateSmcEngine", "gpcCount", true, "Number of gpc resources owned by the SmcEgine created");
    kwargs->RegisterKwarg("GpuPartition.CreateSmcEngine", "smcEngineName", false, "Name of SmcEngine to be created");
    kwargs->RegisterKwarg("GpuPartition.RemoveSmcEngine", "sysPipe", false, "sys pipe owned by the SmcEngine to be removed; must be in range 0..7");
    kwargs->RegisterKwarg("GpuPartition.RemoveSmcEngine", "smcEngineName", false, "Name of the SmcEngine to be removed");
    kwargs->RegisterKwarg("GpuPartition.GetSmcEngine", "sysPipe", false, "sys pipe of the SmcEngine to be retrieved; must be in range 0..7");
    kwargs->RegisterKwarg("GpuPartition.GetSmcEngine", "smcEngineName", false, "Name of the SmcEngine to be retrieved");
    kwargs->RegisterKwarg("GpuPartition.GetMemCtrl", "memCtrlType", true, "MemCtrlType enum (FBMEM, MMIO_PRIV or MMIO_USER) indicating memory type");
    kwargs->RegisterKwarg("GpuPartition.GetMemCtrl", "address", false, "start address of the memory region. For FBMEM it is the GPU Partition PA.");
    kwargs->RegisterKwarg("GpuPartition.GetMemCtrl", "memSize", false, "size of the memory region");

    py::class_<UtlGpuPartition> gpuPartition(module, "GpuPartition",
        "The GpuPartition class is used for objects that represent Gpu Partition. Part of GpuPartition objects are created when UTL initializes SmcResource, which happens before any UTL scripts run. Part of GpuPartition objects are created by GPU when UTL scripts run.");
    gpuPartition.def("CreateSmcEngine", &UtlGpuPartition::CreateSmcEnginePy,
        UTL_DOCSTRING("GpuPartition.CreateSmcEngine", "This function will create an SmcEngine within this partition."),
        py::return_value_policy::reference);
    gpuPartition.def("RemoveSmcEngine", &UtlGpuPartition::RemoveSmcEngine,
        UTL_DOCSTRING("GpuPartition.RemoveSmcEngine", "This function will remove the SmcEngine within this partition."));
    gpuPartition.def("GetSmcEngine", &UtlGpuPartition::GetSmcEnginePy,
        UTL_DOCSTRING("GpuPartition.GetSmcEngine", "This function will return the SmcEngine object."),
        py::return_value_policy::reference);
    gpuPartition.def("GetSysPipes", &UtlGpuPartition::GetSysPipes,
        "This function returns all the syspipe indexes (both used/unused by SmcEngines) of a GpuPartition.");
    gpuPartition.def("GetGpcCount", &UtlGpuPartition::GetGpcCount,
        UTL_DOCSTRING("GpuPartition.GetGpcCount", "This function will return the GPC Count value"));
    gpuPartition.def("GetSwizId", &UtlGpuPartition::GetSwizzId,
        UTL_DOCSTRING("GpuPartition.GetSwizId", "This function will return the SwizzleID of this partition"));
    gpuPartition.def("GetName", &UtlGpuPartition::GetName,
	UTL_DOCSTRING("GpuPartition.GetName", "This function will return the name of the GPU partition"));
    gpuPartition.def("GetFbInfo", &UtlGpuPartition::GetFbInfo,
        UTL_DOCSTRING("GpuPartition.GetFbInfo", "This function returns the FB Info object for the GpuPartition. The returned object can then be used to retrieve various FB related info (see utl.FbInfo class)."),
        py::return_value_policy::reference);
    gpuPartition.def("GetSmcEngines", &UtlGpuPartition::GetActiveSmcEnginesPy,
        UTL_DOCSTRING("GpuPartition.GetSmcEngines", "This function returns a list of all the SmcEngines within the partition"));
    gpuPartition.def("GetMemCtrl", &UtlGpuPartition::GetMemCtrl,
        UTL_DOCSTRING("GpuPartition.GetMemCtrl", "This function returns the utl.MemCtrl object for the memory type specified."),
        py::return_value_policy::reference);
}

UtlGpuPartition* UtlGpuPartition::Create
(
    UINT32 gpcCount, 
    GpuPartition * pGpuPartition,
    UtlGpu * pUtlGpu
)
{
    UtlGpuPartition* returnPtr;
    if (s_GpuPartitions.count(pGpuPartition) == 0)
    {
        unique_ptr<UtlGpuPartition> utlGpuPartition(
            new UtlGpuPartition(gpcCount, pGpuPartition, pUtlGpu));
        returnPtr = utlGpuPartition.get();
        s_GpuPartitions[pGpuPartition] = move(utlGpuPartition);
    }
    else
    {
        returnPtr = s_GpuPartitions[pGpuPartition].get();
    }

    return returnPtr;
}

UtlGpuPartition::UtlGpuPartition
(
    UINT32 gpcCount, 
    GpuPartition * pGpuPartition,
    UtlGpu * pUtlGpu
)
    : m_AvailableGpcCount(gpcCount),
      m_pGpuPartition(pGpuPartition),
      m_pUtlGpu(pUtlGpu)
{
}

void UtlGpuPartition::FreeGpuPartitions()
{
    s_GpuPartitions.clear();
}

void UtlGpuPartition::FreeGpuPartition(GpuPartition* pGpuPartition)
{
    if (s_GpuPartitions.find(pGpuPartition) != s_GpuPartitions.end())
    {
        s_GpuPartitions.erase(pGpuPartition);
    }
}

// This function can be called from a UTL script to create
// a UTLSmcEngine.
//
UtlSmcEngine * UtlGpuPartition::CreateSmcEnginePy
(
    py::kwargs kwargs
)
{
    if (Platform::IsVirtFunMode())
    {
        UtlUtility::PyError("Dynamical creating SMC engine in existed GpuPartition is forbidden in virtual funtion mode.");
    }

    // Parse arguments from Python script
    UINT32 sysPipeNum = ~0U;
    string engName;
    SmcResourceController::SYS_PIPE sysPipe = SmcResourceController::SYS_PIPE::NOT_SUPPORT;
    bool hasSysPipe = UtlUtility::GetOptionalKwarg<UINT32>(&sysPipeNum, kwargs, "sysPipe",
        "GpuPartition.CreateSmcEngine");
    UINT32 gpcCount = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "gpcCount",
        "GpuPartition.CreateSmcEngine");
    bool hasEngName = UtlUtility::GetOptionalKwarg<string>(&engName, kwargs, "smcEngineName",
        "GpuPartition.CreateSmcEngine");

    if (m_AvailableGpcCount < gpcCount)
    {
        UtlUtility::PyError("GpuPartition.CreateSmcEngine- Only %d GPC[s] available for partition (swizid %d) but SmcEngine (SYS%d) requested with %d GPC[s].",
            m_AvailableGpcCount, GetSwizzId(), sysPipeNum, gpcCount);
    }

    if (GetUtlGpu()->FetchSmcResourceController()->IsSmcMemApi())
    {
        if (hasSysPipe)
        {
            UtlUtility::PyError("CreateSmcEngine error: sysPipe cannot be used along with -smc_eng_mem");
        }
        sysPipe = m_pGpuPartition->GetAvailableSyspipes().front();
    }
    else
    {
        if (hasEngName)
        {
            UtlUtility::PyError("CreateSmcEngine error: engName cannot be used along with -smc_engine_label");
        }
        if (sysPipeNum >= 0 && sysPipeNum <= 7)
        {
            sysPipe = static_cast<SmcResourceController::SYS_PIPE>(SmcResourceController::SYS_PIPE::SYS0 + sysPipeNum);
        }
        else
        {
            UtlUtility::PyError("Illegal sysPipe value %d; must be in range 0..7", sysPipeNum);
        }
    }

    UtlGil::Release gil;

    // Fetch SmcResourceController
    SmcResourceController * pSmcResourceMgr = GetUtlGpu()->FetchSmcResourceController();
    MASSERT(pSmcResourceMgr != nullptr);

    // Create SmcEngine through SmcResourceController
    SmcEngine * pSmcEngine = pSmcResourceMgr->CreateOneSMCEngine(sysPipe, gpcCount, m_pGpuPartition, engName);
    if (!pSmcEngine)
    {
        UtlUtility::PyError("Create smc engine fails.");
    }

    // Add Smc Engine
    m_pGpuPartition->AddSmcEngine(pSmcEngine);

    // Alloc SmcEngine
    RC rc = m_pGpuPartition->AllocAllSmcEngines();
    UtlUtility::PyErrorRC(rc, "Alloc smc engine fails.");

    pSmcResourceMgr->PrintSmcInfo();

    return UtlSmcEngine::GetFromSmcEnginePtr(pSmcEngine);
}

UtlSmcEngine * UtlGpuPartition::CreateSmcEngine
(
    SmcEngine * pSmcEngine
)
{
    UtlSmcEngine * pUtlSmcEngine = UtlSmcEngine::Create(this, pSmcEngine);
    SmcResourceController * pSmcResourceMgr = GetUtlGpu()->FetchSmcResourceController();
    MASSERT(pSmcResourceMgr != nullptr);
    LWGpuResource * pLWGpuResource = pSmcResourceMgr->GetGpuResource();
    pLWGpuResource->AllocSmcRegHal(pSmcEngine);

    m_ActiveUtlSmcEngines.push_back(pUtlSmcEngine);
    m_AvailableGpcCount -= pSmcEngine->GetGpcCount();
    return pUtlSmcEngine;
}

// This function can be called from a UTL script to obtain
// a UTL SmcEngine.
//
UtlSmcEngine * UtlGpuPartition::GetSmcEnginePy
(
    py::kwargs kwargs
)
{
    UINT32 tmp = ~0U;
    string engName;
    // Parse arguments from Python script
    bool hasSysPipe = UtlUtility::GetOptionalKwarg<UINT32>(&tmp, kwargs, "sysPipe", "GpuPartition.GetSmcEngine");
    bool hasEngName = UtlUtility::GetOptionalKwarg<string>(&engName, kwargs, "smcEngineName", "GpuPartition.GetSmcEngine");
    SmcResourceController::SYS_PIPE sys = SmcResourceController::SYS_PIPE::NOT_SUPPORT;
    
    if (GetUtlGpu()->FetchSmcResourceController()->IsSmcMemApi())
    {
        if (hasSysPipe)
        {
            UtlUtility::PyError("GetSmcEngine error: sysPipe cannot be mentioned along with -smc_eng_mem");
        }
        UtlGil::Release gil;

        return (GetSmcEngineByName(engName));
    }
    else
    {
        if (hasEngName)
        {
            UtlUtility::PyError("GetSmcEngine error: engName cannot be mentioned along with sys pipe value");
        }
        if (tmp >= 0 && tmp <= 7)
        {
            sys = static_cast<SmcResourceController::SYS_PIPE>(SmcResourceController::SYS_PIPE::SYS0 + tmp);
        }
        else
        {
            UtlUtility::PyError("Illegal sysPipe value %d; must be in range 0..7", tmp);
        }
        UtlGil::Release gil;

        return GetSmcEngine(sys);
    }
}

UtlSmcEngine * UtlGpuPartition::GetSmcEngine
(
    SmcResourceController::SYS_PIPE syspipe
)
{
    for (auto & smcEngine : m_ActiveUtlSmcEngines)
    {
        if (smcEngine->GetSysPipe() == syspipe)
        {
            return smcEngine;
        }
    }

    UtlUtility::PyError("SmcEngine with sysPipe value %d does not exist.",
        syspipe - SmcResourceController::SYS_PIPE::SYS0);
    return nullptr;
}

UtlSmcEngine * UtlGpuPartition::GetSmcEngineByName
(
    string engName
)
{
    for (auto & smcEngine : m_ActiveUtlSmcEngines)
    {
        if (smcEngine->GetName() == engName)
        {
            return smcEngine;
        }
    }

    return nullptr;
}

vector<UINT32> UtlGpuPartition::GetSysPipes() const
{
    vector<SmcResourceController::SYS_PIPE> sysPipes = 
        m_pGpuPartition->GetAllSyspipes();
    vector<UINT32> syspipeIndexes;
    for (const auto& syspipe : sysPipes)
    {
        syspipeIndexes.push_back(UINT32(syspipe));
    }

    return syspipeIndexes;
}

void UtlGpuPartition::RemoveSmcEngine
(
    py::kwargs kwargs
)
{
    UtlSmcEngine * pUtlSmcEngine = GetSmcEnginePy(kwargs);
    
    UtlGil::Release gil;

    SmcResourceController * pSmcResourceMgr = GetUtlGpu()->FetchSmcResourceController();
    MASSERT(pSmcResourceMgr != nullptr);
    LWGpuResource * pLWGpuResource = pSmcResourceMgr->GetGpuResource();
    pLWGpuResource->FreeSmcRegHal(pUtlSmcEngine->GetSmcEngineRawPtr());

    // Free related RmEvent before free LwRm
    LwRm* pLwRm = pLWGpuResource->GetLwRmPtr(pUtlSmcEngine->GetSmcEngineRawPtr());
    RmEventData::FreeLwRmEventData(pLwRm);

    UINT32 syspipe = pUtlSmcEngine->GetSysPipe(); 
    vector<SmcEngine *> smcEngines;
    smcEngines.push_back(pUtlSmcEngine->GetSmcEngineRawPtr());
    RC rc = m_pGpuPartition->FreeSmcEngines(smcEngines);
    UtlUtility::PyErrorRC(rc, "Remove SmcEngine with syspipe = %d failed.",
        syspipe);
    m_AvailableGpcCount += pUtlSmcEngine->GetSmcEngineRawPtr()->GetGpcCount();
    m_ActiveUtlSmcEngines.erase(
        remove(m_ActiveUtlSmcEngines.begin(), m_ActiveUtlSmcEngines.end(), pUtlSmcEngine),
        m_ActiveUtlSmcEngines.end());
    UtlSmcEngine::FreeSmcEngine(pUtlSmcEngine->GetSmcEngineRawPtr());
    m_pGpuPartition->RemoveSmcEngines(smcEngines);
    pSmcResourceMgr->PrintSmcInfo();
}

void UtlGpuPartition::FreeSmcEngines()
{
    vector<GpuPartition *> removeGpuPartitions;
    removeGpuPartitions.push_back(GetGpuPartitionRawPtr());
    RC rc;

    if (GetGpuPartitionRawPtr()->GetActiveSmcEngines().size() > 0)
    {
        SmcResourceController * pSmcResourceMgr = GetUtlGpu()->FetchSmcResourceController();
        rc = pSmcResourceMgr->AllocOrFreeSmcEngines(removeGpuPartitions, false);
        UtlUtility::PyErrorRC(rc, "Free All SmcEngine in specified GpuPartition swizzId = %d failed.", GetSwizzId());
    }

    for (auto &pUtlSmcEngine : GetActiveSmcEngines())
    {
        UtlSmcEngine::FreeSmcEngine(pUtlSmcEngine->GetSmcEngineRawPtr());
    }

    m_ActiveUtlSmcEngines.clear();
}

UtlGpuPartition* UtlGpuPartition::GetFromGpuPartitionPtr(GpuPartition* pGpuPartition)
{
    if (s_GpuPartitions.count(pGpuPartition) > 0)
    {
        return s_GpuPartitions[pGpuPartition].get();
    }

    return nullptr;
}

UtlFbInfo* UtlGpuPartition::GetFbInfo()
{
    if (m_pUtlFbInfo == nullptr)
    {
        LwRm* pLwRm = GetUtlGpu()->GetGpuResource()->GetLwRmPtr(GetGpuPartitionRawPtr());
        GpuSubdevice* pGpuSubdev = GetUtlGpu()->GetGpuSubdevice();
        m_pUtlFbInfo = make_unique<UtlFbInfo>(pGpuSubdev, pLwRm, this);
    }

    return m_pUtlFbInfo.get();
}

py::list UtlGpuPartition::GetActiveSmcEnginesPy()
{
    return UtlUtility::ColwertPtrVectorToPyList(m_ActiveUtlSmcEngines);
}

UtlMemCtrl* UtlGpuPartition::GetMemCtrl(py::kwargs kwargs)
{
     auto memCtrlType = UtlUtility::GetRequiredKwarg<UtlMemCtrl::MemCtrlType>(
         kwargs, "memCtrlType", "GpuPartition.GetMemCtrl");
 
     UINT64 address = UtlMemCtrl::INVALID;
     UINT64 memSize = UtlMemCtrl::INVALID;
 
    if (memCtrlType == UtlMemCtrl::MemCtrlType::FBMEM)
    {
        if (!UtlUtility::GetOptionalKwarg<UINT64>(&address, kwargs, "address", "GpuPartition.GetMemCtrl") ||
            !UtlUtility::GetOptionalKwarg<UINT64>(&memSize, kwargs, "memSize", "GpuPartition.GetMemCtrl"))
        {
            UtlUtility::PyError("GpuPartition.GetMemCtrl requires address and memSize arguments when memCtrlType = FBMEM.");
        }
    }

    UtlGil::Release gil;

    return UtlMemCtrl::GetMemCtrl(
        memCtrlType, 
        m_pUtlGpu, 
        GetUtlGpu()->GetGpuResource()->GetLwRmPtr(GetGpuPartitionRawPtr()), 
        address, 
        memSize);
}
