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

#include "utlgpuregctrl.h"
#include "utlsmcengine.h"
#include "utlgpupartition.h"
#include "core/include/massert.h"

map<SmcEngine*, unique_ptr<UtlSmcEngine>> UtlSmcEngine::s_SmcEngines;

void UtlSmcEngine::Register(py::module module)
{
    py::class_<UtlSmcEngine> smcEngine(module, "SmcEngine");
    smcEngine.def("GetRegCtrl", &UtlSmcEngine::GetRegCtrl,
        "This function returns the Register controller associated with the SmcEngine.",
        py::return_value_policy::take_ownership);
    smcEngine.def("GetSysPipe", &UtlSmcEngine::GetSysPipePy,
        "This function returns the SysPipe of the SmcEngine.");
    smcEngine.def("GetName", &UtlSmcEngine::GetName,
        "This function returns the name of the SmcEngine.");
    smcEngine.def("GetGpuPartition", &UtlSmcEngine::GetGpuPartition,
        "This function returns the GpuPartition for the SmcEngine.",
        py::return_value_policy::reference);
    smcEngine.def_property_readonly("gpcMask", &UtlSmcEngine::GetGpcMaskPy,
        "GPC mask of the SmcEngine.");
    smcEngine.def("__eq__", [] (UtlSmcEngine* o1, UtlSmcEngine* o2)
    {
        return o1 == o2;
    });
    smcEngine.def("__ne__", [] (UtlSmcEngine* o1, UtlSmcEngine* o2)
    {
        return o1 != o2;
    });
}

UtlSmcEngine* UtlSmcEngine::Create
(
    UtlGpuPartition * pUtlGpuPartition,
    SmcEngine * pSmcEngine
)
{
    MASSERT(s_SmcEngines.count(pSmcEngine) == 0);
    unique_ptr<UtlSmcEngine> utlSmcEngine(new UtlSmcEngine(pUtlGpuPartition, pSmcEngine));
    UtlSmcEngine* returnPtr = utlSmcEngine.get();
    s_SmcEngines[pSmcEngine] = move(utlSmcEngine);

    return returnPtr;
}

UtlSmcEngine::UtlSmcEngine
(
   UtlGpuPartition * pUtlGpuPartition,
   SmcEngine * pSmcEngine
) :
    m_pUtlGpuPartition(pUtlGpuPartition),
    m_pSmcEngine(pSmcEngine)
{
    MASSERT(m_pUtlGpuPartition);
    MASSERT(m_pSmcEngine);
}

void UtlSmcEngine::FreeSmcEngines()
{
    s_SmcEngines.clear();
}

void UtlSmcEngine::FreeSmcEngine(SmcEngine *pSmcEngine)
{
    if (s_SmcEngines.find(pSmcEngine) != s_SmcEngines.end())
    {
        s_SmcEngines.erase(pSmcEngine);
    }
}

UtlSmcEngine* UtlSmcEngine::GetFromSmcEnginePtr(SmcEngine* pSmcEngine)
{
    if (s_SmcEngines.count(pSmcEngine) > 0)
    {
        return s_SmcEngines[pSmcEngine].get();
    }

    return nullptr;
}

UtlGpuRegCtrl* UtlSmcEngine::GetRegCtrl()
{
    return new UtlGpuRegCtrl(m_pSmcEngine);
}

UINT32 UtlSmcEngine::GetSysPipePy()
{
    return static_cast<UINT32>(GetSysPipe());
}

UINT32 UtlSmcEngine::GetGpcMaskPy()
{
    return m_pSmcEngine->GetGpcMask();
}
