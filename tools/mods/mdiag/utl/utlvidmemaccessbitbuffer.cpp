/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/utility.h"
#include "gpu/include/gpudev.h"
#include "mdiag/sysspec.h"
#include "class/clc763.h"
#include "ctrl/ctrlc763.h"

#include "utlvidmemaccessbitbuffer.h"
#include "utl.h"
#include "utlutil.h"

namespace
{
    const UINT32 PacketSize = 512;
}

void UtlVidmemAccessBitBuffer::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("VidmemAccessBitBuffer.EnableLogging", "granularity", true, "granularity of the buffer");
    kwargs->RegisterKwarg("VidmemAccessBitBuffer.EnableLogging", "trackMode", true, "track mode of the buffer");
    kwargs->RegisterKwarg("VidmemAccessBitBuffer.EnableLogging", "disableMode", true, "disable mode of the buffer");
    kwargs->RegisterKwarg("VidmemAccessBitBuffer.EnableLogging", "mmuType", true, "mmu type of the buffer");
    kwargs->RegisterKwarg("VidmemAccessBitBuffer.EnableLogging", "startAddress", true, "start address of the buffer");

    py::class_<UtlVidmemAccessBitBuffer, UtlSurface> buffer(module, "VidmemAccessBitBuffer");
    buffer.def_static("Create", &UtlVidmemAccessBitBuffer::Create,
        UTL_DOCSTRING("VidmemAccessBitBuffer.Create",
            "This function creates a VidmemAccessBitBuffer object."),
        py::return_value_policy::reference);
    buffer.def("EnableLogging", &UtlVidmemAccessBitBuffer::EnableLogging,
        UTL_DOCSTRING("VidmemAccessBitBuffer.EnableLogging",
            "This function enables logging with attributes for the dump request."));
    buffer.def("DisableLogging", &UtlVidmemAccessBitBuffer::DisableLogging,
        UTL_DOCSTRING("VidmemAccessBitBuffer.DisableLogging",
            "This function disables logging."));
    buffer.def("Dump", &UtlVidmemAccessBitBuffer::Dump,
        UTL_DOCSTRING("VidmemAccessBitBuffer.Dump",
            "This function dumps data to the buffer."));
    buffer.def("PutOffset", &UtlVidmemAccessBitBuffer::PutOffset,
        UTL_DOCSTRING("VidmemAccessBitBuffer.PutOffset",
            "This function returns the put offset of the buffer."));

    py::enum_<LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_GRANULARITY> granularity(buffer, "Granularity",
        "This enum specifies the granularity of the memory to be tracked.");
    granularity.value("_64kb", LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_GRANULARITY_64KB);
    granularity.value("_128kb", LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_GRANULARITY_128KB);
    granularity.value("_256kb", LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_GRANULARITY_256KB);
    granularity.value("_512kb", LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_GRANULARITY_512KB);
    granularity.value("_1mb", LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_GRANULARITY_1MB);
    granularity.value("_2mb", LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_GRANULARITY_2MB);
    granularity.value("_4mb", LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_GRANULARITY_4MB);
    granularity.value("_8mb", LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_GRANULARITY_8MB);
    granularity.value("_16mb", LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_GRANULARITY_16MB);
    granularity.value("_32mb", LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_GRANULARITY_32MB);
    granularity.value("_64mb", LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_GRANULARITY_64MB);
    granularity.value("_128mb", LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_GRANULARITY_128MB);
    granularity.value("_256mb", LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_GRANULARITY_256MB);
    granularity.value("_512mb", LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_GRANULARITY_512MB);
    granularity.value("_1gb", LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_GRANULARITY_1GB);
    granularity.value("_2gb", LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_GRANULARITY_2GB);

    py::enum_<LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_TRACK_MODE> trackMode(buffer, "TrackMode",
        "This enum indicates whether to track access or dirty bits.");
    trackMode.value("Access", LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_TRACK_MODE_ACCESS);
    trackMode.value("Dirty", LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_TRACK_MODE_DIRTY);

    py::enum_<LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_DISABLE_MODE> disableMode(buffer, "DisableMode",
        "This enum indicates whether to set or clear bits when logging is disabled.");
    disableMode.value("Clear", LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_DISABLE_MODE_CLEAR);
    disableMode.value("Set", LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_DISABLE_MODE_SET);

    py::enum_<LW_VIDMEM_ACCESS_BIT_BUFFER_MMU_TYPE> mmuType(buffer, "MmuType",
        "This enum indicates which mmu component to track.");
    mmuType.value("Hubmmu", LW_VIDMEM_ACCESS_BIT_BUFFER_HUBMMU);
    mmuType.value("Gpcmmu", LW_VIDMEM_ACCESS_BIT_BUFFER_GPCMMU);
    mmuType.value("Hshubmmu", LW_VIDMEM_ACCESS_BIT_BUFFER_HSHUBMMU);
    mmuType.value("Default", LW_VIDMEM_ACCESS_BIT_BUFFER_DEFAULT);
}

// This constructor should only be called from UtlVidmemAccessBitBuffer::Create.
//
UtlVidmemAccessBitBuffer::UtlVidmemAccessBitBuffer(MdiagSurf* pMdiagSurf)
    : UtlSurface(pMdiagSurf)
{
}

// This function can be called from a UTL script to create a Vidmem Access Bit
// buffer.
//
UtlVidmemAccessBitBuffer* UtlVidmemAccessBitBuffer::Create(py::kwargs kwargs)
{
    return static_cast<UtlVidmemAccessBitBuffer*>(
        UtlSurface::DoCreate(kwargs,
            [](MdiagSurf* pMdiagSurf)
            {
                return unique_ptr<UtlSurface>(
                    new UtlVidmemAccessBitBuffer(pMdiagSurf));
            }
        )
    );
}

void UtlVidmemAccessBitBuffer::EnableLogging(py::kwargs kwargs)
{
    if (!IsAllocated())
    {
        UtlUtility::PyError("Can't enable logging for the buffer before it has been allocated.");
    }

    vector<LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_GRANULARITY> granularity;
    py::object pyGranularity = UtlUtility::GetRequiredKwarg<py::object>(kwargs,
            "granularity", "VidmemAccessBitBuffer.EnableLogging");
    UtlUtility::CastPyObjectToListOrSingleElement(pyGranularity, &granularity);
    const LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_TRACK_MODE trackMode =
        UtlUtility::GetRequiredKwarg<LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_TRACK_MODE>(kwargs,
            "trackMode", "VidmemAccessBitBuffer.EnableLogging");
    const LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_DISABLE_MODE disableMode =
        UtlUtility::GetRequiredKwarg<LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_DISABLE_MODE>(kwargs,
            "disableMode", "VidmemAccessBitBuffer.EnableLogging");
    const LW_VIDMEM_ACCESS_BIT_BUFFER_MMU_TYPE mmuType =
        UtlUtility::GetRequiredKwarg<LW_VIDMEM_ACCESS_BIT_BUFFER_MMU_TYPE>(kwargs,
            "mmuType", "VidmemAccessBitBuffer.EnableLogging");
    vector<UINT64> startAddress;
    py::object pyStartAddress = UtlUtility::GetRequiredKwarg<py::object>(kwargs,
            "startAddress", "VidmemAccessBitBuffer.EnableLogging");
    UtlUtility::CastPyObjectToListOrSingleElement(pyStartAddress, &startAddress);

    if ((granularity.size() != startAddress.size()) ||
        granularity.empty() ||
        (granularity.size() > LW_VIDMEM_ACCESS_BIT_BUFFER_NUM_CHECKERS))
    {
        UtlUtility::PyError("invalid granularity size(%d) or startAddress size(%d), they are expected to be equal and in range 1 to %d", 
            granularity.size(), startAddress.size(), LW_VIDMEM_ACCESS_BIT_BUFFER_NUM_CHECKERS);
    }

    LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_ENABLE_LOGGING_PARAMS params = {};
    for (UINT32 i = 0; i < granularity.size(); i++)
    {
        params.granularity[i] = granularity[i];
        params.startAddress[i] = startAddress[i];
    }
    params.rangeCount = granularity.size();
    params.trackMode = trackMode;
    params.disableMode = disableMode;
#ifdef LW_VERIF_FEATURES
    params.mmuType = mmuType;
#else
    WarnPrintf("VidmemAccessBitBuffer.EnableLogging mmuType=%d not set.\n", mmuType);
#endif

    UtlGil::Release gil;

    RC rc = GetLwRm()->Control(
        GetMemHandle(),
        LWC763_CTRL_CMD_VIDMEM_ACCESS_BIT_ENABLE_LOGGING,
        &params, sizeof (params));
    UtlUtility::PyErrorRC(rc, "VidmemAccessBitBuffer.EnableLogging failed");
}

void UtlVidmemAccessBitBuffer::DisableLogging()
{
    if (!IsAllocated())
    {
        UtlUtility::PyError("Can't disable logging for the buffer before it has been allocated.");
    }

    UtlGil::Release gil;

    RC rc = GetLwRm()->Control(
        GetMemHandle(),
        LWC763_CTRL_CMD_VIDMEM_ACCESS_BIT_DISABLE_LOGGING, nullptr, 0);
    UtlUtility::PyErrorRC(rc, "VidmemAccessBitBuffer.DisableLogging failed");
}

void UtlVidmemAccessBitBuffer::Dump()
{
    LWC763_CTRL_VIDMEM_ACCESS_BIT_BUFFER_DUMP_PARAMS params = {};
    if (!IsAllocated())
    {
        UtlUtility::PyError("Can't dump to the buffer before it has been allocated.");
    }

    UtlGil::Release gil;

    RC rc = GetLwRm()->Control(
        GetMemHandle(),
        LWC763_CTRL_CMD_VIDMEM_ACCESS_BIT_DUMP, &params, sizeof(params));
    UtlUtility::PyErrorRC(rc, "VidmemAccessBitBuffer.Dump failed");
}

UINT32 UtlVidmemAccessBitBuffer::PutOffset()
{
    if (!IsAllocated())
    {
        UtlUtility::PyError("Can't read put offset for the buffer before it has been allocated.");
    }

    LWC763_CTRL_VIDMEM_ACCESS_BIT_PUT_OFFSET_PARAMS params = {0};

    UtlGil::Release gil;

    RC rc = GetLwRm()->Control(
        GetMemHandle(),
        LWC763_CTRL_CMD_VIDMEM_ACCESS_BIT_PUT_OFFSET,
        &params, sizeof (params));
    UtlUtility::PyErrorRC(rc, "VidmemAccessBitBuffer.PutOffset failed");

    return params.vidmemAccessBitPutOffset;
}

RC UtlVidmemAccessBitBuffer::DoAllocate(MdiagSurf* pMdiagSurf, GpuDevice *pGpuDevice)
{
    LwRm::Handle hMemory = 0;

    LW_VIDMEM_ACCESS_BIT_ALLOCATION_PARAMS params = {0};

    MASSERT((UtlSurface::GetSize() % PacketSize) == 0);
    params.noOfEntries = UtlSurface::GetSize() / PacketSize;

    switch (pMdiagSurf->GetLocation())
    {
        case Memory::Fb:
            params.addrSpace = LW_VIDMEM_ACCESS_BIT_BUFFER_ADDR_SPACE_VID;
            break;
        case Memory::Coherent:
        case Memory::CachedNonCoherent:
            params.addrSpace = LW_VIDMEM_ACCESS_BIT_BUFFER_ADDR_SPACE_COH;
            break;
        case Memory::NonCoherent:
            params.addrSpace = LW_VIDMEM_ACCESS_BIT_BUFFER_ADDR_SPACE_NCOH;
            break;
        case Memory::Optimal:
            params.addrSpace = LW_VIDMEM_ACCESS_BIT_BUFFER_ADDR_SPACE_DEFAULT;
            break;
        default:
            MASSERT(0);
    }

    RC rc;
    CHECK_RC(GetLwRm()->Alloc(GetLwRm()->GetSubdeviceHandle(pGpuDevice->GetSubdevice(0)),
            &hMemory,
            MMU_VIDMEM_ACCESS_BIT_BUFFER,
            &params));
    pMdiagSurf->SetExternalPhysMem(hMemory, 0);
    pMdiagSurf->SetForceCpuMappable(true);
    return UtlSurface::DoAllocate(pMdiagSurf, pGpuDevice);
}

void UtlVidmemAccessBitBuffer::DoSetSize(UINT64 size)
{
    if ((size % PacketSize) != 0)
    {
        UtlUtility::PyError("size must aligned to %u bytes.", PacketSize);
    }
    UtlSurface::DoSetSize(size);
}
