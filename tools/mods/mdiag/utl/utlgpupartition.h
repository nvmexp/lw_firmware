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

#ifndef INCLUDED_UTLGPUPARTITION_H
#define INCLUDED_UTLGPUPARTITION_H

#include "core/include/types.h"
#include "mdiag/smc/smcresourcecontroller.h"
#include "mdiag/smc/gpupartition.h"
#include "utlpython.h"

class GpuPartition;
class UtlSmcEngine;
class UtlGpu;
class UtlFbInfo;
class UtlMemCtrl;

// This class represents the C++ backing for the UTL Python GpuPartition object.
// This class is a wrapper around GpuPartition.
//
class UtlGpuPartition
{
public:
    static void Register(py::module module);
    
    static UtlGpuPartition* Create(UINT32 gpcCount, GpuPartition* m_pGpuPartition, UtlGpu* pUtlGpu);
    static UtlGpuPartition* GetFromGpuPartitionPtr(GpuPartition* pGpuPartition);
    static void FreeGpuPartitions();
    static void FreeGpuPartition(GpuPartition* pGpuPartition);

    UINT32 GetGpcCount() const { return m_pGpuPartition->GetGpcCount();  }

    UtlSmcEngine * CreateSmcEngine(SmcEngine * pSmcEngine);
    UtlSmcEngine * GetSmcEngine(SmcResourceController::SYS_PIPE syspipe);
    UtlSmcEngine * GetSmcEngineByName(string engName);
    
    // the version to handle argument from python script
    UtlSmcEngine * CreateSmcEnginePy(py::kwargs kwargs);
    UtlSmcEngine * GetSmcEnginePy(py::kwargs kwargs);
    
    py::list GetActiveSmcEnginesPy();
    const vector<UtlSmcEngine*>& GetActiveSmcEngines()
        { return m_ActiveUtlSmcEngines; }
    UtlGpu * GetUtlGpu() { return m_pUtlGpu; }
    GpuPartition * GetGpuPartitionRawPtr() { return m_pGpuPartition; }

    vector<UINT32> GetSysPipes() const;
    UINT32 GetSwizzId() const { return m_pGpuPartition->GetSwizzId(); }
    string GetName() const { return m_pGpuPartition->GetName(); }
    void RemoveSmcEngine(py::kwargs kwargs);
    void FreeSmcEngines();
    UtlMemCtrl* GetMemCtrl(py::kwargs kwargs);

    UtlFbInfo* GetFbInfo();

    // Make sure the compiler doesn't implement default versions
    // of these constructors and assignment operators.  This is so we can
    // catch errors with incorrect references on the Python side.
    UtlGpuPartition() = delete;
    UtlGpuPartition(UtlGpuPartition&) = delete;
    UtlGpuPartition &operator=(UtlGpuPartition&) = delete;

private:
    UtlGpuPartition(UINT32 gpcCount, GpuPartition * m_pGpuPartition, UtlGpu * pUtlGpu);
    static map<GpuPartition*, unique_ptr<UtlGpuPartition>> s_GpuPartitions;
    UINT32 m_AvailableGpcCount;
    vector<UtlSmcEngine *> m_ActiveUtlSmcEngines;
    GpuPartition * m_pGpuPartition;
    UtlGpu * m_pUtlGpu;
    unique_ptr<UtlFbInfo> m_pUtlFbInfo;
};

#endif
