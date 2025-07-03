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

#ifndef INCLUDED_UTLENGINE_H
#define INCLUDED_UTLENGINE_H

#include <memory>
#include <vector>
#include <map>
using namespace std;

#include "core/include/types.h"
#include "mdiag/utils/utils.h"
#include "mdiag/utils/lwgpu_classes.h"
#include "utlpython.h"
#include <tuple>

class UtlGpuPartition;
class UtlTsg;
class UtlChannel;
class UtlGpu;

// This struct hold the engine info specific to a subdevice.
struct UtlSubdeviceEngineInfo
{
    UINT32 m_StartMmuEngineId;
    UINT32 m_MaxSubCtx;
    bool m_bSubCtxAware;
};

// This class represents the abstract concept of engine object.
//
class UtlEngine
{
public:
    static void Register(py::module module);

    static UtlEngine* Create(UINT32 engineId, UtlGpu * pGpu, UtlGpuPartition * pUtlGpuPartition);
    static void FreeEngines();
    UtlEngine(UINT32 engineId, LWGpuClasses::Engine engineType, UtlGpu * pGpu, UtlGpuPartition * pGpuPartition);
   
    UINT32 GetEngineId() const { return m_EngineId; }
    UtlGpuPartition * GetGpuPartition() const { return m_pUtlGpuPartition; }
    UtlGpu * GetGpu() const { return m_pGpu; }
    bool IsGrCE();
    static UtlEngine * GetEngine(UINT32 engineId, UtlGpu * pGpu, UtlGpuPartition * pGpuPartition);
    UINT32 GetFirstSupportedClass(py::kwargs kwargs);

    UtlEngine() = delete;
    UtlEngine(UtlGpu&) = delete;
    UtlEngine& operator=(UtlEngine&) = delete;
    
    static bool MatchedEngine(LWGpuClasses::Engine engineName, UINT32 engineId);
    static bool IsMatchedEnginePy(py::kwargs kwargs);
    static vector<UtlEngine *> *GetEngines(LWGpuClasses::Engine engineName, 
            UtlGpu * pGpu, UtlGpuPartition * pGpuPartition);
    
    // Explore to python for call
    py::list GetTsgs();
    void Reset();
    const string GetName();
    bool IsGrCEPy();
    typedef std::tuple<LWGpuClasses::Engine, UtlGpu *, UtlGpuPartition *> ResourceKey;
    LwRm * GetLwRmPtr() const;

    UtlSubdeviceEngineInfo* GetSubdeviceEngineInfo();

private:
    UINT32 m_EngineId;
    LWGpuClasses::Engine m_EngineType;
    string m_EngineName;
    UtlGpuPartition * m_pUtlGpuPartition;
    UtlGpu * m_pGpu;
    // This vector will own the ownship about the engine objects
    static vector<unique_ptr<UtlEngine> > s_Engines;
    // This map is used for python 
    static map<ResourceKey, vector<UtlEngine *> > s_PythonEngines;

    unique_ptr<UtlSubdeviceEngineInfo> m_pSubdeviceEngineInfo;
};

#endif
