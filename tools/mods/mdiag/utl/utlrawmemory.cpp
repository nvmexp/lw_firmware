/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "mdiag/sysspec.h"

#include "utl.h"
#include "utlrawmemory.h"
#include "utlutil.h"

void UtlRawMemory::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("RawMemory.ReadData", "size", true, "number of bytes to read");
    kwargs->RegisterKwarg("RawMemory.ReadData", "offset", false, "offset in bytes; default 0");
    kwargs->RegisterKwarg("RawMemory.WriteData", "data", true, "data to write");
    kwargs->RegisterKwarg("RawMemory.WriteData", "offset", false, "offset in bytes; default 0");

    py::class_<UtlRawMemory> segment(module, "RawMemory");
    segment.def("Map", &UtlRawMemory::MapPy,
        UTL_DOCSTRING("RawMemory.Map", "This function creates a virtual mapping to the physical address."));
    segment.def("Unmap", &UtlRawMemory::UnmapPy,
        UTL_DOCSTRING("RawMemory.Unmap", "This function frees a previously allocated mapping."));
    segment.def("GetPhysAddress", &UtlRawMemory::GetPhysAddress,
        UTL_DOCSTRING("RawMemory.GetPhysAddress", "This function returns the physical address of the segment base."));
    segment.def("ReadData", &UtlRawMemory::ReadData,
        UTL_DOCSTRING("RawMemory.ReadData", "This function reads data from the mapped segment."));
    segment.def("WriteData", &UtlRawMemory::WriteData,
        UTL_DOCSTRING("RawMemory.WriteData", "This functions writes data to the mapped segment."));
    segment.def_property_readonly("aperture", &UtlRawMemory::GetAperture, 
        "Returns the aperture information for the mapped segment as an enum of type Utl.Mmu.Aperture."
        "Possible values are Utl.Mmu.APERTURE.FB, Utl.Mmu.APERTURE.COHERENT and Utl.Mmu.APERTURE.NONCOHERENT");
}

UtlRawMemory::UtlRawMemory(LwRm *pLwRm, GpuDevice* pGpuDev)
{
    m_pRawMemory = make_unique<MDiagUtils::RawMemory>(pLwRm, pGpuDev);
}

RC UtlRawMemory::Map()
{
    return m_pRawMemory->Map();
}

RC UtlRawMemory::Unmap()
{
    return m_pRawMemory->Unmap();
}

void UtlRawMemory::SetPhysAddress(PHYSADDR physAddress)
{
    MASSERT(nullptr != m_pRawMemory);
    m_pRawMemory->SetPhysAddress(physAddress);
}

void UtlRawMemory::MapPy()
{
    UtlGil::Release gil;

    RC rc = Map();
    UtlUtility::PyErrorRC(rc, "RawMemory.Map failed!");
}

void UtlRawMemory::UnmapPy()
{
    UtlGil::Release gil;

    RC rc = Unmap();
    UtlUtility::PyErrorRC(rc, "RawMemory.Unmap failed");
}

UINT64 UtlRawMemory::GetPhysAddress()
{
    UtlGil::Release gil;

    return (UINT64)m_pRawMemory->GetPhysAddress();
}

vector<UINT08> UtlRawMemory::ReadData(py::kwargs kwargs)
{
    UINT64 offset = 0ULL;
    UtlUtility::GetOptionalKwarg<UINT64>(&offset, kwargs,
        "offset", "RawMemory.ReadData");
    UINT64 size = UtlUtility::GetRequiredKwarg<UINT64>(kwargs,
        "size", "RawMemory.ReadData");

    UtlGil::Release gil;

    RC rc = Map();
    UtlUtility::PyErrorRC(rc, "Failed to read data!");

    vector<UINT08> data(size);
    Platform::VirtualRd((UINT08 *)m_pRawMemory->GetAddress() + offset, &data[0], size);

    return data;
}

void UtlRawMemory::WriteData(py::kwargs kwargs)
{
    UINT64 offset = 0ULL;
    UtlUtility::GetOptionalKwarg<UINT64>(&offset, kwargs,
        "offset", "RawMemory.WriteData");
    vector<UINT08> data = UtlUtility::GetRequiredKwarg<vector<UINT08>>(kwargs,
        "data", "RawMemory.WriteData");

    UtlGil::Release gil;

    RC rc = Map();
    UtlUtility::PyErrorRC(rc, "Failed to write data!");

    Platform::VirtualWr((UINT08 *)m_pRawMemory->GetAddress() + offset, &data[0], data.size());
}

void UtlRawMemory::SetAperture(UtlMmu::APERTURE aperture)
{
    m_Aperture = aperture;
    
    switch (aperture)
    {
        case UtlMmu::APERTURE::APERTURE_FB:
            m_pRawMemory->SetLocation(Memory::Fb);
            break;
        case UtlMmu::APERTURE::APERTURE_COHERENT:
            m_pRawMemory->SetLocation(Memory::Coherent);
            break;
        case UtlMmu::APERTURE::APERTURE_NONCOHERENT:
            m_pRawMemory->SetLocation(Memory::NonCoherent);
            break;
        default:
            MASSERT(!"UtlRawMemory.SetAperture: Aperture is unsupported/ not recognized.");
    }
}

UtlMmu::APERTURE UtlRawMemory::GetAperture() const
{ 
    return m_Aperture;
}
