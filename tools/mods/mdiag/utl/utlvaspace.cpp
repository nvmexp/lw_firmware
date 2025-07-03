/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "utlvaspace.h"
#include "utlutil.h"
#include "utlgpu.h"
#include "utltest.h"
#include "mdiag/lwgpu.h"
#include "core/include/lwrm.h"

vector<unique_ptr<UtlVaSpace>> UtlVaSpace::s_VaSpaces;

void UtlVaSpace::Register(py::module module)
{
    py::class_<UtlVaSpace> vaSpace(module, "VaSpace");
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("VaSpace.Create", "name", true, "name of vaspace");
    kwargs->RegisterKwarg("VaSpace.Create", "gpu", false, "gpu of vaspace, default to first GPU");
    kwargs->RegisterKwarg("VaSpace.Create", "test", false, "the test vaspace belongs to");
    kwargs->RegisterKwarg("VaSpace.Create", "index", false, "index of vaspace, should be one of VaSpace.Index");
    kwargs->RegisterKwarg("VaSpace.Create", "base", false, "base address of vaspace, must use together with size");
    kwargs->RegisterKwarg("VaSpace.Create", "size", false, "size of vaspace, must use together with base");
    
    vaSpace.def_static("Create", &UtlVaSpace::CreatePy,
        UTL_DOCSTRING("VaSpace.Create", "This function creates a VaSpace object that can be used to allocate surfaces."),
        py::return_value_policy::reference);

    vaSpace.def_property_readonly("vaSpaceId", &UtlVaSpace::GetVaSpaceId,
        "A read-only integer representing the id of the vaSpace.");

    vaSpace.def_property_readonly("isAtsEnabled", &UtlVaSpace::IsAtsEnabled,
        UTL_DOCSTRING("VaSpace.IsAtsEnabled", "This function returns true if ATS is enabled."));

    py::enum_<VaSpace::Index>(vaSpace, "Index")
        .value("FLA", VaSpace::Index::FLA)
        .value("NEW", VaSpace::Index::NEW);
}

UtlVaSpace::UtlVaSpace
(
    VaSpace * pVaSpace,
    UtlTest * pTest,
    UtlGpu * pGpu
) : 
    m_pVaSpace(pVaSpace),
    m_pTest(pTest),
    m_pGpu(pGpu)
{

}

UtlVaSpace* UtlVaSpace::Create
(
    VaSpace* pVaSpace,
    UtlTest * pTest,
    UtlGpu * pGpu
)
{
    if (GetVaSpace(pVaSpace->GetHandle(), pTest) == nullptr)
    {
        unique_ptr<UtlVaSpace> utlVaSpace(new UtlVaSpace(pVaSpace, pTest, pGpu));
        s_VaSpaces.push_back(std::move(utlVaSpace));
        return s_VaSpaces.back().get();
    }
    else
    {
        // Do nothing
        return nullptr;
    }
}

UtlVaSpace* UtlVaSpace::CreatePy(py::kwargs kwargs)
{
    string name = UtlUtility::GetRequiredKwarg<string>(kwargs, "name", "VaSpace.Create");
    // If user doesn't specify a GPU, use GPU 0 by default.
    UtlGpu* gpu = UtlGpu::GetGpus()[0];
    UtlUtility::GetOptionalKwarg<UtlGpu*>(&gpu, kwargs, "gpu", "VaSpace.Create");
    // If user doesn't specify a test, use nullptr by default;
    UtlTest* test = nullptr;
    UtlUtility::GetOptionalKwarg<UtlTest*>(&test, kwargs, "test", "VaSpace.Create");
    // nullptr means to use global.
    UINT32 testId = test ? test->GetTestId() : LWGpuResource::TEST_ID_GLOBAL;

    shared_ptr<VaSpace> vaSpace = gpu->GetGpuResource()->GetVaSpaceManager(LwRmPtr().Get())
        ->CreateResourceObject(testId, name);
    // Default is NEW
    VaSpace::Index index = VaSpace::Index::NEW;
    UtlUtility::GetOptionalKwarg<VaSpace::Index>(&index, kwargs, 
        "index", "VaSpace.Create");
    vaSpace->SetIndex(index);

    // Prepare base/size only for FLA, error out for others.
    UINT64 base = 0;
    UINT64 size = 0;
    if (UtlUtility::GetOptionalKwarg<UINT64>(&base, kwargs, "base", "VaSpace.Create") &&
        UtlUtility::GetOptionalKwarg<UINT64>(&size, kwargs, "size", "VaSpace.Create"))
    {
        if (index == VaSpace::Index::FLA)
        {
            vaSpace->SetRange(base, size);
        }
        else
        {
            UtlUtility::PyError("Only FLA VASpace support base/size settings now!\n");
        }
    }
    else if (UtlUtility::GetOptionalKwarg<UINT64>(&base, kwargs, "base", "VaSpace.Create") ||
             UtlUtility::GetOptionalKwarg<UINT64>(&size, kwargs, "size", "VaSpace.Create"))
    {
        UtlUtility::PyError("Base and size must use together!\n");
    }
    else
    {
        if (index == VaSpace::Index::FLA)
        {
            UtlUtility::PyError("You must assign base and size for FLA Virtual Address Space.!\n");
        }
    }
    
    return Create(vaSpace.get(), test, gpu);
}

UtlVaSpace * UtlVaSpace::GetVaSpace
(
    LwRm::Handle hVaSpace,
    UtlTest * pTest
)
{
    for (auto && vaSpace : s_VaSpaces)
    {
        // For the default vaspace, register each one for the test
        if ((vaSpace->GetHandle() == 0 &&
                hVaSpace == vaSpace->GetHandle()) &&
                vaSpace->GetTest() == pTest)
        {
            return vaSpace.get();
        }
        else if (vaSpace->GetHandle() == hVaSpace)
        {
            return vaSpace.get();   
        }
    }

    return nullptr;
}

void UtlVaSpace::FreeVaSpace(UtlTest * pTest, LwRm * pLwRm)
{
    for (auto it = s_VaSpaces.begin(); it != s_VaSpaces.end(); )
    {
        if ((*it)->GetTest() == pTest  &&
                (*it)->GetLwRm() == pLwRm)
        {
           it = s_VaSpaces.erase(it);
        }
        else
        {
            it++;
        }
    }
}

UINT32 UtlVaSpace::GetVaSpaceId() const
{
    RC rc;
    LW0080_CTRL_DMA_ADV_SCHED_GET_VA_CAPS_PARAMS params = {0};
    params.hVASpace = GetHandle();

    {
        UtlGil::Release gil;
        rc = GetLwRm()->Control(GetLwRm()->GetDeviceHandle(m_pGpu->GetGpudevice()),
            LW0080_CTRL_CMD_DMA_ADV_SCHED_GET_VA_CAPS, &params, sizeof (params));
    }

    if (rc != OK)
    {
        UtlUtility::PyError("VaSpace.vaSpaceId failed");
    }

    return params.vaSpaceId;
}
