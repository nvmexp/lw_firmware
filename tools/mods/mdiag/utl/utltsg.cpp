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

#include "class/cla06c.h"
#include "gpu/include/gpusbdev.h"
#include "mdiag/smc/gpupartition.h"
#include "mdiag/lwgpu.h"

#include "utl.h"
#include "utlgpu.h"
#include "utltsg.h"
#include "utlusertest.h"
#include "utlutil.h"
#include "utlchannel.h"
#include "utlengine.h"
#include "utlgpupartition.h"

map<LWGpuTsg*, unique_ptr<UtlTsg>> UtlTsg::s_Tsgs;
map<UtlTest*, map<string, UtlTsg*>> UtlTsg::s_ScriptTsgs;

void UtlTsg::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("Tsg.Create", "name", true, "name of tsg");
    kwargs->RegisterKwarg("Tsg.Create", "engine", false, "engine associated with this TSG; this argument is required for Ampere and later and is ignored otherwise");
    kwargs->RegisterKwarg("Tsg.Create", "gpu", false, "target GPU; default to GPU0 if not given");
    kwargs->RegisterKwarg("Tsg.Create", "userTest", false, "user test associated with this TSG");
    kwargs->RegisterKwarg("Tsg.GetSubCtx", "veid", true, "Virtual Engine ID");

    py::class_<UtlTsg> tsg(module, "Tsg", "The Tsg class is used for objects that represent Tsg.");
    tsg.def_property_readonly("name", &UtlTsg::GetName);
    tsg.def_property_readonly("isShared", &UtlTsg::IsShared);
    tsg.def_static("Create", &UtlTsg::CreatePy,
        UTL_DOCSTRING("Tsg.Create", "This function creates a TSG."),
        py::return_value_policy::reference);
    tsg.def("GetChannels", &UtlTsg::GetChannelsPy,
        "This function returns a list of channels within the tsg.");
    tsg.def("GetSubCtx", &UtlTsg::GetSubCtx,
        UTL_DOCSTRING("Tsg.GetSubCtx", "This function returns the subctx corresponding to the veid."),
        py::return_value_policy::reference);
}

UtlTsg* UtlTsg::GetFromTsgPtr(LWGpuTsg* lwgpuTsg)
{
    if (s_Tsgs.count(lwgpuTsg) == 0)
    {
        unique_ptr<UtlTsg> utlTsg(new UtlTsg(lwgpuTsg));
        s_Tsgs[lwgpuTsg] = move(utlTsg);
    }

    return s_Tsgs[lwgpuTsg].get();
}

// Called from the Utl class to free the UtlTsg objects
//
void UtlTsg::FreeTsgs()
{
    s_ScriptTsgs.clear();
    s_Tsgs.clear();
}

// This constructor should only be called from UtlTsg::Create.
//
UtlTsg::UtlTsg
(
    LWGpuTsg *lwgpuTsg
) :
    m_LWGpuTsg(lwgpuTsg)
{
    MASSERT(m_LWGpuTsg != nullptr);
}

UtlTsg::~UtlTsg()
{
    if (m_OwnedTsg.get() != nullptr)
    {
        m_OwnedTsg->RelRef();
        m_OwnedTsg.reset(nullptr);
    }
}

void UtlTsg::AddChannel(UtlChannel * pChannel)
{
    if (find(m_Channels.begin(), m_Channels.end(), pChannel) == m_Channels.end())
    {
        // Don't own the ownship about the UtlChannel, not all channel will have tsg
        m_Channels.push_back(pChannel);
    }
}

// This function can be called from Python to create a UTL TSG object.
UtlTsg* UtlTsg::CreatePy(py::kwargs kwargs)
{
    UtlUtility::RequirePhase(Utl::InitEventPhase | Utl::RunPhase,
        "Tsg.Create");

    string name = UtlUtility::GetRequiredKwarg<string>(kwargs, "name",
        "Tsg.Create");

    UtlGpu* gpu = nullptr;
    if (!UtlUtility::GetOptionalKwarg<UtlGpu*>(&gpu, kwargs, "gpu", "Tsg.Create"))
    {
        // If the user doesn't specify a GPU, use GPU 0 by default.
        gpu = UtlGpu::GetGpus()[0];
        MASSERT(gpu != nullptr);
    }
    else if (gpu == nullptr)
    {
        UtlUtility::PyError("invalid gpu passed to Tsg.Create.");
    }

    UtlEngine* engine = nullptr;
    if (gpu->GetGpuSubdevice()->HasFeature(Device::GPUSUB_HOST_REQUIRES_ENGINE_ID))
    {
        if (!UtlUtility::GetOptionalKwarg<UtlEngine*>(&engine, kwargs, "engine", "Tsg.Create"))
        {
            UtlUtility::PyError("For Ampere and later, engine is required when creating a TSG");
        }
    }

    // If a userTest parameter is passed in the kwargs, the TSG is
    // associated with the given user test.  The UtlTsg::s_Tsgs member will
    // still own the UtlTsg object on the C++ side, but the user test will
    // own the Tsg object on the Python side.
    UtlUserTest* userTest = nullptr;
    py::object userTestObj;
    UtlTest* utlTest = Utl::Instance()->GetTestFromScriptId();
    if (UtlUtility::GetOptionalKwarg<py::object>(&userTestObj, kwargs, "userTest", "Tsg.Create"))
    {
        userTest = UtlUserTest::GetTestFromPythonObject(userTestObj);

        if (userTest == nullptr)
        {
            UtlUtility::PyError("invalid userTest argument.");
        }
        else if (userTest->GetTsg(name) != nullptr)
        {
            UtlUtility::PyError(
                "userTest object already has Tsg with name %s.",
                name.c_str());
        }
    }
    else
    {
        auto iter = s_ScriptTsgs.find(utlTest);
        if ((iter != s_ScriptTsgs.end()) &&
            (*iter).second.find(name) != (*iter).second.end())
        {
            UtlUtility::PyError("TSG with name '%s' already exists.",
                name.c_str());
        }
    }

    UtlGil::Release gil;

    LWGpuResource* lwgpu = gpu->GetGpuResource();
    const ArgReader *params = userTest ? userTest->GetParam() :
        Utl::Instance()->GetArgReader();

    UINT32 engineId = LW2080_ENGINE_TYPE_NULL;
    if (engine)
    {
        if (engine->IsGrCE())
        {
            engineId = LW2080_ENGINE_TYPE_GR0;
        }
        else
        {
            engineId = engine->GetEngineId();
        }
    }

    SmcEngine* pSmcEngine = nullptr;
    LwRm* pLwRm = engine ? engine->GetLwRmPtr() : LwRmPtr().Get();

    // In SMC mode, UtlEngine objects are created per MemoryPartition but 
    // the GR TSGs need to be created using SmcEngine RM client and 
    // engineId = GR0 and non-GR TSGs will be created using SmcEngine0's
    // RM client and MemoryPartition local engineId since they are shared
    // among SmcEngines
    if (engine && engine->GetGpuPartition())
    {
        GpuPartition* pGpuPartition = engine->GetGpuPartition()->GetGpuPartitionRawPtr();
        if (MDiagUtils::IsGraphicsEngine(engineId))
        {
            pSmcEngine = pGpuPartition->GetSmcEngineByType(engine->GetEngineId());
            engineId = LW2080_ENGINE_TYPE_GR0;
        }
        else
        {
            pSmcEngine = pGpuPartition->GetSmcEngine(0);
        }
        pLwRm = lwgpu->GetLwRmPtr(pSmcEngine);
    }

    unique_ptr<LWGpuTsg> mdiagTsg(LWGpuTsg::Factory(
        name,
        lwgpu,
        params,
        KEPLER_CHANNEL_GROUP_A,  /* hwClass */
        0,  /* hVASpace */
        false,  /* isShared */
        pLwRm,
        engineId,
        pSmcEngine
    ));

    UtlTsg* utlTsg = GetFromTsgPtr(mdiagTsg.get());
    utlTsg->m_OwnedTsg = move(mdiagTsg);
    utlTsg->m_OwnedTsg->AddRef();

    if (userTest != nullptr)
    {
        userTest->AddTsg(name, utlTsg);
    }
    else
    {
        auto iter = s_ScriptTsgs.find(utlTest);
        if (iter == s_ScriptTsgs.end())
        {
            iter = s_ScriptTsgs.emplace(
                make_pair(utlTest, map<string, UtlTsg*>())).first;
        }
        (*iter).second.emplace(make_pair(name, utlTsg));
    }

    return utlTsg;
}

vector<UtlTsg*> UtlTsg::GetEngineTsgs(UINT32 engineId)
{
    vector<UtlTsg*> tsgList;

    for (const auto& keyValue : s_Tsgs)
    {
        if (keyValue.second->GetEngineId() == engineId)
        {
            tsgList.push_back(keyValue.second.get());
        }
    }
    return tsgList;
}

UtlSubCtx* UtlTsg::GetSubCtx(py::kwargs kwargs)
{
    UINT32 veid = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "veid", "Tsg.GetSubCtx");
    for (auto &channel : m_Channels)
    {
        SubCtx* pSubCtx = channel->GetSubCtx()->GetRawPtr();
        if (pSubCtx && pSubCtx->GetVeid() == veid)
        {
            return channel->GetSubCtx();
        }
    }

    // Tsg will always have the veid 0 even if it doesn't specify the subctx
    WarnPrintf("No available subctx in the tsg %s.\n", GetName().c_str());
    return nullptr;
}

// Free the UtlTsg object for the passed LWGpuTsg object
//
void UtlTsg::FreeTsg(LWGpuTsg* lwgpuTsg)
{
    s_Tsgs.erase(lwgpuTsg);
}

py::list UtlTsg::GetChannelsPy()
{
    return UtlUtility::ColwertPtrVectorToPyList(m_Channels);
} 
