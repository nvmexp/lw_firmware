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

#include "utlmemctrl.h"
#include "utlutil.h"
#include "mdiag/sysspec.h"
#include "class/clc661.h"
#include "gpu/include/gpudev.h"
#include "core/include/refparse.h"

map<UtlMemCtrl::ResourceKey, UtlMemCtrl*> UtlMemCtrl::s_MMIOMemCtrlMap;
vector<unique_ptr<UtlMemCtrl>> UtlMemCtrl::s_MemCtrls;

void UtlMemCtrl::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("MemCtrl.Write", "offset", true, "offset to which data has to be written.");
    kwargs->RegisterKwarg("MemCtrl.Write", "data", true, "32 bit data to be written.");
    kwargs->RegisterKwarg("MemCtrl.Read", "offset", true, "offset from which data has to be read.");
    
    py::class_<UtlMemCtrl> memCtrl(module, "MemCtrl",
        "Interface for memory objects. It is lwrrently supported for allocating and reading to MMIO pages. Users can get the memory controller object using Gpu.GetMemCtrl(memCtrlType).");
    memCtrl.def("Alloc", &UtlMemCtrl::AllocPy,
        UTL_DOCSTRING("MemCtrl.Alloc", "This function allocates the memory object of the memory controller."));
    memCtrl.def("Free", &UtlMemCtrl::FreePy,
        UTL_DOCSTRING("MemCtrl.Free", "This function frees the memory object of the memory controller."));
    memCtrl.def("GetGpuVA", &UtlMemCtrl::GetGpuVAPy,
        UTL_DOCSTRING("MemCtrl.GetGpuVA", "This function returns the GPU VA of the memory controller."));
    memCtrl.def("Write", &UtlMemCtrl::WritePy,
        UTL_DOCSTRING("MemCtrl.Write", "This function writes the specified 32 bit data to the specified offset within the memory object."));
    memCtrl.def("Read", &UtlMemCtrl::ReadPy,
        UTL_DOCSTRING("MemCtrl.Read", "This function returns the 32 bit data from the specified offset within the memory object."));

    py::enum_<UtlMemCtrl::MemCtrlType>(memCtrl, "MemCtrlType")
        .value("MMIO_USER", UtlMemCtrl::MemCtrlType::MMIO_USER)
        .value("MMIO_PRIV", UtlMemCtrl::MemCtrlType::MMIO_PRIV)
        .value("FBMEM", UtlMemCtrl::MemCtrlType::FBMEM);
}

void UtlMemCtrl::FreeMemCtrls()
{
    s_MMIOMemCtrlMap.clear();
    s_MemCtrls.clear();
}

UtlMemCtrl::UtlMemCtrl(UtlGpu* pUtlGpu, LwRm* pLwRm, string name) :
    m_pUtlGpu(pUtlGpu),
    m_pLwRm(pLwRm),
    m_Handle(0),
    m_CtxDma(0),
    m_CpuPointer(nullptr),
    m_GpuVA(~0U),
    m_Name(name)
{
}

/* static */ UtlMemCtrl* UtlMemCtrl::GetMemCtrl
(
    MemCtrlType memCtrlType, 
    UtlGpu* pUtlGpu,
    LwRm* pLwRm,
    UINT64 address,
    UINT64 memSize
)
{
    if ((memCtrlType == MemCtrlType::MMIO_PRIV) ||
        (memCtrlType == MemCtrlType::MMIO_USER))
    {
        ResourceKey key(pUtlGpu, pLwRm, memCtrlType);

        if (s_MMIOMemCtrlMap.find(key) == s_MMIOMemCtrlMap.end())
        {
            unique_ptr<UtlMemCtrl> pMemCtrl;
            if (memCtrlType == MemCtrlType::MMIO_PRIV)
            {
                pMemCtrl = make_unique<UtlMmioPriv>(pUtlGpu, pLwRm);
            }
            else 
            {
                pMemCtrl = make_unique<UtlMmioUser>(pUtlGpu, pLwRm);
            }
            s_MMIOMemCtrlMap[key] = pMemCtrl.get();
            s_MemCtrls.push_back(move(pMemCtrl));
        }
        return s_MMIOMemCtrlMap[key];
    }
    else if (memCtrlType == MemCtrlType::FBMEM)
    {
        MASSERT(address != INVALID && memSize != INVALID);
        s_MemCtrls.push_back(make_unique<UtlFbMem>(pUtlGpu, pLwRm, address, memSize));
        return s_MemCtrls.back().get();
    }
    else
    {
        UtlUtility::PyError("Unsupported MemCtrlType to UtlMemCtrl::GetMemCtrl().");
    }

    return nullptr;
}

UtlMemCtrl::~UtlMemCtrl()
{
    Free();
    m_pLwRm = nullptr;
    m_pUtlGpu = nullptr;

    return;
}

void UtlMemCtrl::AllocPy()
{
    UtlGil::Release gil;
    if (m_Handle == 0)
    {
        Alloc();
    }
}

void UtlMemCtrl::FreePy()
{
    UtlGil::Release gil;
    Free();
}

void UtlMemCtrl::Free()
{
    if (m_GpuVA != ~0U)
    {
        m_pLwRm->UnmapMemoryDma(m_CtxDma, m_Handle, 0, m_GpuVA, GetGpuDevice());
    }

    if (m_CpuPointer)
    {
        m_pLwRm->UnmapMemory(m_Handle, m_CpuPointer, 0, GetGpuSubdevice());
        m_CpuPointer = nullptr;   
    }

    if (m_Handle)
    {
        MASSERT(m_pLwRm);
        m_pLwRm->Free(m_Handle);
        m_Handle = 0;
    }
}

void UtlMemCtrl::WritePy(py::kwargs kwargs)
{
    if (m_CpuPointer == nullptr)
    {
        UtlUtility::PyError(
            "Cannot write to %s memory. It has not been allocated/mapped.",
            GetName().c_str());
    }

    UINT32 offset = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "offset", "MemCtrl.Write");
    UINT32 data = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "data", "MemCtrl.Write");

    auto address = reinterpret_cast<uintptr_t>(static_cast<UINT08*>(m_CpuPointer) + offset);
    MEM_WR32(address, data);

    return;
}

UINT32 UtlMemCtrl::ReadPy(py::kwargs kwargs)
{
    if (m_CpuPointer == nullptr)
    {
        UtlUtility::PyError(
            "Cannot read from %s memory. It has not been allocated/mapped.",
            GetName().c_str());
    }

    UINT32 offset = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "offset", "MemCtrl.Read");

    auto address = reinterpret_cast<uintptr_t>(static_cast<UINT08*>(m_CpuPointer) + offset);
    return MEM_RD32(address);
}

UINT64 UtlMemCtrl::GetGpuVAPy()
{
    return m_GpuVA;
}

UtlMmioPriv::UtlMmioPriv(UtlGpu* pUtlGpu, LwRm* pLwRm) :
    UtlMemCtrl(pUtlGpu, pLwRm, "MMIO Priv")
{
}

void UtlMmioPriv::Alloc()
{
    MASSERT(m_pLwRm);
    MASSERT(m_pUtlGpu);

    RC rc;
    LW_HOPPER_USERMODE_A_PARAMS params;
    params.bBar1Mapping = true;
    params.bPriv = true;

    const RefManual *pRefManual = GetGpuSubdevice()->GetRefManual();
    const RefManualRegisterGroup *pVirtualFunctionPriv = 
        pRefManual->FindGroup("LW_VIRTUAL_FUNCTION_PRIV");
    UINT64 privSize = pVirtualFunctionPriv->GetRangeHi() - 
        pVirtualFunctionPriv->GetRangeLo() + 1;

    rc = m_pLwRm->Alloc(m_pLwRm->GetSubdeviceHandle(GetGpuSubdevice()), &m_Handle, 
            HOPPER_USERMODE_A, &params);
    UtlUtility::PyErrorRC(rc, "MMIO Priv page allocation failed.");

    rc = m_pLwRm->MapMemory(m_Handle, 0, privSize, &m_CpuPointer, 0,
            GetGpuSubdevice());
    UtlUtility::PyErrorRC(rc, "MMIO Priv page map failed.");

    m_CtxDma = GetGpuDevice()->GetMemSpaceCtxDma(Memory::Fb, false, m_pLwRm);
    rc = m_pLwRm->MapMemoryDma(m_CtxDma, m_Handle, 0, privSize, 0, 
            &m_GpuVA, GetGpuDevice()); 
    UtlUtility::PyErrorRC(rc, "MMIO Priv page GPU map failed.");
}

UtlMmioUser::UtlMmioUser(UtlGpu* pUtlGpu, LwRm* pLwRm) :
    UtlMemCtrl(pUtlGpu, pLwRm, "MMIO User")
{
}

void UtlMmioUser::Alloc()
{
    MASSERT(m_pLwRm);
    MASSERT(m_pUtlGpu);

    RC rc;
    LW_HOPPER_USERMODE_A_PARAMS params;
    params.bBar1Mapping = true;
    params.bPriv = false;

    rc = m_pLwRm->Alloc(m_pLwRm->GetSubdeviceHandle(GetGpuSubdevice()), &m_Handle, 
            HOPPER_USERMODE_A, &params);
    UtlUtility::PyErrorRC(rc, "MMIO User page allocation failed.");

    rc = m_pLwRm->MapMemory(m_Handle, 0, DRF_SIZE(LWC661), &m_CpuPointer, 0,
            GetGpuSubdevice());
    UtlUtility::PyErrorRC(rc, "MMIO User page map failed.");

    m_CtxDma = GetGpuDevice()->GetMemSpaceCtxDma(Memory::Fb, false, m_pLwRm);
    rc = m_pLwRm->MapMemoryDma(m_CtxDma, m_Handle, 0, DRF_SIZE(LWC661), 0, 
            &m_GpuVA, GetGpuDevice());     
    UtlUtility::PyErrorRC(rc, "MMIO User page GPU map failed.");
}

UtlFbMem::UtlFbMem(UtlGpu* pUtlGpu, LwRm* pLwRm, UINT64 gpuPA, UINT64 memSize) :
    UtlMemCtrl(pUtlGpu, pLwRm, "FBMEM"),
    m_GpuPA(gpuPA),
    m_MemSize(memSize)
{
    MASSERT(m_GpuPA != INVALID && m_MemSize != INVALID);
}

UtlFbMem::~UtlFbMem()
{
    Free();
    m_GpuPA = INVALID;
    m_MemSize = INVALID;
}

void UtlFbMem::Alloc()
{
    RC rc;

    LW0080_CTRL_FB_CREATE_FB_SEGMENT_PARAMS params = { };
    params.VidOffset = m_GpuPA;
    params.Offset = params.VidOffset;
    params.ValidLength = m_MemSize;
    params.Length = params.ValidLength;
    params.AllocHintHandle = 0;
    params.hClient = m_pLwRm->GetClientHandle();
    params.hDevice = m_pLwRm->GetDeviceHandle(GetGpuDevice());

    params.Flags = DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _FIXED_OFFSET, _NO) |
        DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _MAP_CPUVA, _YES) |
        DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _CONTIGUOUS, _TRUE) |
        DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _PAGE_SIZE, _4K) |
        DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _KIND_PROVIDED, _YES);
    params.kind = GetGpuSubdevice()->Regs().LookupValue(MODS_MMU_PTE_KIND_GENERIC_MEMORY);
    params.subDeviceIDMask = 1; // Just enable subdevice 0

    rc = m_pLwRm->CreateFbSegment(GetGpuDevice(), &params);

    UtlUtility::PyErrorRC(rc, 
        "CreateFbSegment failed for FbMem with address=0x%llx and size=0x%llx.",
        m_GpuPA, m_MemSize);

    m_Handle = params.hMemory;
    m_CpuPointer = reinterpret_cast<void*>(params.ppCpuAddress[0]);
}

void UtlFbMem::Free()
{
    RC rc;

    if (m_Handle != 0)
    {
        LW0080_CTRL_FB_DESTROY_FB_SEGMENT_PARAMS params = { };
        params.hMemory = m_Handle;
        rc = m_pLwRm->DestroyFbSegment(GetGpuDevice(), &params);

        UtlUtility::PyErrorRC(rc, 
            "DestroyFbSegment failed for FbMem with address=0x%llx and size=0x%llx.",
            m_GpuPA, m_MemSize);
    
        m_CpuPointer = nullptr;
        m_Handle = 0;
    }
}
