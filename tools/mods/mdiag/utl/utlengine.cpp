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

#include "utlengine.h"
#include "mdiag/utils/utils.h"
#include "mdiag/utl/utlutil.h"
#include "mdiag/lwgpu.h"
#include "gpu/include/gpudev.h"
#include "utltsg.h"
#include "utlchannel.h"
#include "utlgpupartition.h"
#include "utlgpu.h"

vector<unique_ptr<UtlEngine> > UtlEngine::s_Engines;
map<UtlEngine::ResourceKey, vector<UtlEngine *> > UtlEngine::s_PythonEngines;

void UtlEngine::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("Engine.GetFirstSupportedClass", "classType", false, "An enum of type utl.Engine.ClassType which specifies the type of the class. This optional argument is to be required for all engines which support multiple class types (e.g. GRAPHICS engine).");
    kwargs->RegisterKwarg("Engine.IsMatchedEngine", "engineId", true, "Engine ID");
    kwargs->RegisterKwarg("Engine.IsMatchedEngine", "engineType", true, "Engine type");

    py::class_<UtlEngine> engine(module, "Engine");
    engine.def("Reset", &UtlEngine::Reset,
        "This function resets the engine.");
    engine.def_property_readonly("name", &UtlEngine::GetName,
        "This function returns the name of the engine.");
    engine.def("GetTsgs", &UtlEngine::GetTsgs,
        "This function will return the Tsgs associated with this engine.");
    engine.def("GetEngineId", &UtlEngine::GetEngineId,
        UTL_DOCSTRING("Engine.GetEngineId", "This function will return the Engine ID."));
    engine.def("GetFirstSupportedClass", &UtlEngine::GetFirstSupportedClass,
        UTL_DOCSTRING("Engine.GetFirstSupportedClass", "This function returns the first supported class for an engine. Most of the time this function will automatically know the type of the class to be retrieved based on the engine. But sometimes user will have to provide a classType, for engines which support multiple classTypes (current example is GR- which supports compute, graphics and copy classes)."));
    engine.def("GetSubdeviceEngineInfo", &UtlEngine::GetSubdeviceEngineInfo,
        UTL_DOCSTRING("Engine.GetSubdeviceEngineInfo","Gets the subdevice engine info."), py::return_value_policy::reference);
    engine.def_static("IsMatchedEngine", &UtlEngine::IsMatchedEnginePy,
        UTL_DOCSTRING("Engine.IsMatchedEnginePy", "Returns true if the engine id belongs to the mentioned engine type."));
    engine.def("IsGrCE", &UtlEngine::IsGrCEPy,
        UTL_DOCSTRING("Engine.IsGrCE", "Returns true if the engine is GrCE engine."));

    py::enum_<LWGpuClasses::Engine>(engine, "EngineName")
        .value("GRAPHICS", LWGpuClasses::Engine::GRAPHICS)
        .value("COPY", LWGpuClasses::Engine::COPY)
        .value("LWDEC", LWGpuClasses::Engine::LWDEC)
        .value("LWENC", LWGpuClasses::Engine::LWENC)
        .value("OFA", LWGpuClasses::Engine::OFA)
        .value("LWJPG", LWGpuClasses::Engine::LWJPG)
        .value("SEC", LWGpuClasses::Engine::SEC)
        .value("SW", LWGpuClasses::Engine::SW);

    py::enum_<LWGpuClasses::ClassType>(engine, "ClassType")
        .value("GRAPHICS", LWGpuClasses::ClassType::GRAPHICS)
        .value("COPY", LWGpuClasses::ClassType::COPY)
        .value("COMPUTE", LWGpuClasses::ClassType::COMPUTE);

    py::class_<UtlSubdeviceEngineInfo> subdeviceEngineInfo(module, "SubdeviceEngineInfo");
    subdeviceEngineInfo.def_readonly("startMmuEngineId", &UtlSubdeviceEngineInfo::m_StartMmuEngineId, "Start mmu engine id");
    subdeviceEngineInfo.def_readonly("maxSubCtx", &UtlSubdeviceEngineInfo::m_MaxSubCtx, "Max sub-context");
    subdeviceEngineInfo.def_readonly("subCtxAware", &UtlSubdeviceEngineInfo::m_bSubCtxAware, "Is sub-context aware");
}

UtlEngine::UtlEngine
(
    UINT32 engineId,
    LWGpuClasses::Engine engineType,
    UtlGpu * pGpu,
    UtlGpuPartition * pGpuPartition
) :
    m_EngineId(engineId),
    m_EngineType(engineType),
    m_pUtlGpuPartition(pGpuPartition),
    m_pGpu(pGpu)
{
}

void UtlEngine::FreeEngines()
{
    s_PythonEngines.clear();
    s_Engines.clear();
}

UtlEngine* UtlEngine::Create
(
    UINT32 engineId, 
    UtlGpu * pGpu, 
    UtlGpuPartition * pUtlGpuPartition
)
{
    auto engineType = LWGpuClasses::Engine::ENGINE_NUM;

    if (MDiagUtils::IsGraphicsEngine(engineId))
    {
        engineType = LWGpuClasses::Engine::GRAPHICS;
    }
    else if (MDiagUtils::IsCopyEngine(engineId))
    {
        engineType = LWGpuClasses::Engine::COPY;
    }
    else if (MDiagUtils::IsLwDecEngine(engineId))
    {
        engineType = LWGpuClasses::Engine::LWDEC;
    }
    else if (MDiagUtils::IsLwEncEngine(engineId))
    {
        engineType = LWGpuClasses::Engine::LWENC;
    }
    else if (MDiagUtils::IsOFAEngine(engineId))
    {
        engineType = LWGpuClasses::Engine::OFA;
    }
    else if (MDiagUtils::IsLWJPGEngine(engineId))
    {
        engineType = LWGpuClasses::Engine::LWJPG;
    }
    else if (MDiagUtils::IsSECEngine(engineId))
    {
        engineType = LWGpuClasses::Engine::SEC;
    }
    else if (MDiagUtils::IsSWEngine(engineId))
    {
        engineType = LWGpuClasses::Engine::SW;
    }
    else
    {
        UtlUtility::PyError("Not supported engine");
    }
       
    UtlEngine* pUtlEngine = UtlEngine::GetEngine(engineId, pGpu, pUtlGpuPartition);
    
    if (!pUtlEngine)
    {
        std::unique_ptr<UtlEngine> pEngine = 
            make_unique<UtlEngine>(engineId, engineType, pGpu, pUtlGpuPartition);
        ResourceKey key(engineType, pGpu, pUtlGpuPartition);
        s_PythonEngines[key].push_back(pEngine.get());
        pUtlEngine = pEngine.get();
        s_Engines.push_back(std::move(pEngine));
    }

    return pUtlEngine;
}

vector<UtlEngine *> * UtlEngine::GetEngines
(
    LWGpuClasses::Engine engineName, 
    UtlGpu * pGpu,
    UtlGpuPartition * pUtlGpuPartition
)
{
    ResourceKey key(engineName, pGpu, pUtlGpuPartition);
    return &s_PythonEngines[key];
}

bool UtlEngine::IsMatchedEnginePy(py::kwargs kwargs)
{
    UINT32 engineId = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "engineId", "Engine.IsMatchedEngine");
    LWGpuClasses::Engine engineType = UtlUtility::GetRequiredKwarg<LWGpuClasses::Engine>(kwargs, "engineType", "Engine.IsMatchedEngine");

    return MatchedEngine(engineType, engineId);
}

bool UtlEngine::MatchedEngine(LWGpuClasses::Engine engineName, UINT32 engineId)
{
    if (engineName == LWGpuClasses::Engine::COPY)
    {
        if (MDiagUtils::IsCopyEngine(engineId))
            return true;
    }
    else if (engineName == LWGpuClasses::Engine::GRAPHICS)
    {
        if (MDiagUtils::IsGraphicsEngine(engineId))
            return true;
    }
    else if (engineName == LWGpuClasses::Engine::LWDEC)
    {
        if (MDiagUtils::IsLwDecEngine(engineId))
            return true;
    }
    else if (engineName == LWGpuClasses::Engine::LWENC)
    {
        if (MDiagUtils::IsLwEncEngine(engineId))
            return true;
    }
    else if (engineName == LWGpuClasses::Engine::OFA)
    {
        if (MDiagUtils::IsOFAEngine(engineId))
            return true;
    }
    else if (engineName == LWGpuClasses::Engine::LWJPG)
    {
        if (MDiagUtils::IsLWJPGEngine(engineId))
            return true;
    }
    else if (engineName == LWGpuClasses::Engine::SEC)
    {
        if (MDiagUtils::IsSECEngine(engineId))
            return true;
    }
    else if (engineName == LWGpuClasses::Engine::SW)
    {
        if (MDiagUtils::IsSWEngine(engineId))
            return true;
    }

    return false;
}

bool UtlEngine::IsGrCEPy()
{
    UtlGil::Release gil;
    return IsGrCE();
}

bool UtlEngine::IsGrCE()
{
    if (!MDiagUtils::IsCopyEngine(m_EngineId) || m_pUtlGpuPartition)
    {
        return false;
    }

    UINT32 ceType = m_EngineId - LW2080_ENGINE_TYPE_COPY0;
    LwRm * pLwRm = GetLwRmPtr();
    GpuDevice* pGpu = nullptr;
    for (const auto & gpu : UtlGpu::GetGpus())
    {
        if (gpu->GetGpudevice() == GetGpu()->GetGpudevice())
        {
            pGpu = gpu->GetGpudevice();
        }
    }
    if (!pGpu)
    {
        UtlUtility::PyError("No UtlGpu with matching GpuDevice to UtlEngine");
    }

    LWGpuResource* pGpuResource = LWGpuResource::GetGpuByDeviceInstance(
        pGpu->GetDeviceInst());

    vector<UINT32> grCeGroup;
    if (pGpuResource->GetGrCopyEngineType(grCeGroup, pLwRm) != OK)
    {
        MASSERT(!"UtlEngine::IsGrCE: Failed to get GrCE group\n");
    }

    if (ceType < CEObjCreateParam::CE_INSTANCE_MAX)
    {
        const UINT32 ceEngineType = CEObjCreateParam::GetEngineTypeByCeType(ceType);
        for(size_t i = 0; i < grCeGroup.size(); i++)
        {
            if (ceEngineType == grCeGroup[i])
            {
                return true;
            }
        }
    }
    return false;
}

UtlSubdeviceEngineInfo* UtlEngine::GetSubdeviceEngineInfo()
{
    if (m_pSubdeviceEngineInfo != nullptr)
    {
        return m_pSubdeviceEngineInfo.get();
    }

    UtlGil::Release gil;
    UINT32 subdeviceId = m_pGpu->GetSubdeviceInst();

    LW2080_CTRL_GPU_GET_ENGINE_FAULT_INFO_PARAMS params = {};
    params.engineType = GetEngineId();
    GetLwRmPtr()->ControlBySubdevice(m_pGpu->GetGpuSubdevice(),
                LW2080_CTRL_CMD_GPU_GET_ENGINE_FAULT_INFO,
                (void *)&params,
                sizeof(params));
    
    m_pSubdeviceEngineInfo = make_unique<UtlSubdeviceEngineInfo>();
    m_pSubdeviceEngineInfo->m_StartMmuEngineId = params.mmuFaultId;
    m_pSubdeviceEngineInfo->m_bSubCtxAware = params.bSubcontextSupported;
    if (m_pSubdeviceEngineInfo->m_bSubCtxAware)
    {
        GpuSubdevice *pGpuSubdevice = m_pGpu->GetGpuSubdevice();
        if (!pGpuSubdevice->IsSMCEnabled())
        {
            LW2080_CTRL_FIFO_GET_INFO_PARAMS fifoParams = {};
            fifoParams.fifoInfoTblSize = 1;
            fifoParams.fifoInfoTbl[0].index = LW2080_CTRL_FIFO_INFO_INDEX_MAX_SUBCONTEXT_PER_GROUP;
            GetLwRmPtr()->ControlBySubdevice(pGpuSubdevice,
                        LW2080_CTRL_CMD_FIFO_GET_INFO,
                        (void *)&fifoParams,
                        sizeof(fifoParams));
            m_pSubdeviceEngineInfo->m_MaxSubCtx = fifoParams.fifoInfoTbl[0].data;
        }
        else
        {
            m_pSubdeviceEngineInfo->m_MaxSubCtx = m_pGpu->GetGpuResource()->GetMaxSubCtxForSmcEngine(GetLwRmPtr());
        }
    }

    DebugPrintf("Subdevice engine info: Subdevice id = %d, StartMmuEngineId = %d, MaxSubCtx = %d, SubCtxAware = %d\n",
            subdeviceId,
            m_pSubdeviceEngineInfo->m_StartMmuEngineId,
            m_pSubdeviceEngineInfo->m_MaxSubCtx,
            m_pSubdeviceEngineInfo->m_bSubCtxAware);
    return m_pSubdeviceEngineInfo.get();
}

UtlEngine * UtlEngine::GetEngine
(
    UINT32 engineId, 
    UtlGpu * pGpu,
    UtlGpuPartition * pGpuPartition
)
{
    for (const auto & engine : s_Engines)
    {
        if (engine->GetEngineId() == engineId &&
                engine->GetGpuPartition() == pGpuPartition &&
                engine->GetGpu() == pGpu)
        {
            return engine.get();
        }
    }

    return nullptr;
}

py::list UtlEngine::GetTsgs()
{
    vector<UtlTsg*> tsgList = UtlTsg::GetEngineTsgs(m_EngineId);
    return UtlUtility::ColwertPtrVectorToPyList(tsgList);
}

void UtlEngine::Reset()
{
    RC rc;

    UtlChannel * pChannel = UtlChannel::FindFirstEngineChannel(m_EngineId);

    rc = MDiagUtils::ResetEngine(pChannel->GetHandle(), 
                                 m_EngineId, 
                                 pChannel->GetLwRmPtr(),
                                 m_pGpu->GetGpudevice());

    UtlUtility::PyErrorRC(rc, "Can't reset engine successfully,");
}

const string UtlEngine::GetName()
{
    return  m_pGpu->GetGpudevice()->GetEngineName(m_EngineId);
}

LwRm * UtlEngine::GetLwRmPtr() const
{
    LwRm * pLwRm = LwRmPtr().Get();
    if (m_pUtlGpuPartition)
    {
        GpuPartition * pGpuPartition = m_pUtlGpuPartition->GetGpuPartitionRawPtr();
        LWGpuResource* pGpuResource = pGpuPartition->GetLWGpuResource();
        MASSERT(pGpuResource);
        if (MDiagUtils::IsGraphicsEngine(m_EngineId))
        {
            pLwRm = pGpuResource->GetLwRmPtr(pGpuPartition->GetSmcEngineByType(m_EngineId));
        }
        else
        {
            pLwRm = pGpuResource->GetLwRmPtr(pGpuPartition->GetSmcEngine(0));
        }
        MASSERT(pLwRm);
    }

    return pLwRm;
}

UINT32 UtlEngine::GetFirstSupportedClass(py::kwargs kwargs)
{
    UINT32 supportedClass;
    LWGpuClasses::ClassType classType = LWGpuClasses::ClassType::CLASS_TYPE_END;

    UtlUtility::GetOptionalKwarg<LWGpuClasses::ClassType>(
            &classType, kwargs, "classType", "Engine.GetFirstSupportedClass");
    RC rc = EngineClasses::GetFirstSupportedClass(
            GetLwRmPtr(), m_pGpu->GetGpudevice(), m_EngineType, &supportedClass, classType);
    
    UtlUtility::PyErrorRC(rc, 
        "UtlEngine.GetFirstSupportedClass() failed for engine %s\n",
        MDiagUtils::Eng2Str(m_EngineId).c_str());

    return supportedClass;
}
