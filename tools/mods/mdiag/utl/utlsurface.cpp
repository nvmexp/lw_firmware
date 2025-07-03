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

#include "core/include/utility.h"

#include "mdiag/sysspec.h"
#include "mdiag/utils/buffinfo.h"

#include "utl.h"
#include "utlchannel.h"
#include "utlgpu.h"
#include "utlsurface.h"
#include "utlusertest.h"
#include "utlutil.h"
#include "utltest.h"
#include "mdiag/lwgpu.h"
#include "mdiag/resource/lwgpu/verif/SurfBufInfo.h"
#include "utlgpuverif.h"
#include "utlmmu.h"
#include "utlvaspace.h"
#include "utlsmcengine.h"

using namespace UtlMmu;

map<MdiagSurf*, unique_ptr<UtlSurface>> UtlSurface::s_Surfaces;
map<UtlTest*, map<string, UtlSurface*>> UtlSurface::s_ScriptSurfaces;

void UtlSurface::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("Surface.Create", "name", true, "name of surface");
    kwargs->RegisterKwarg("Surface.Create", "userTest", false, "user test associated with this surface");
    kwargs->RegisterKwarg("Surface.Create", "keepCpuMapping", false, "if True, functions that create temporary CPU mappings (e.g., Surface.ReadData and Surface.WriteData) will not remove those mappings when finished, increasing performance for tests that read and write to this surface frequently.  Defaults to False.");
    kwargs->RegisterKwarg("Surface.CreateFromSurface", "name", true, "The name of new surface to create.");
    kwargs->RegisterKwarg("Surface.CreateFromSurface", "source", true, "The source surface of the new surface.");
    kwargs->RegisterKwarg("Surface.Allocate", "gpu", false, "target GPU; default to 0 if not given");
    kwargs->RegisterKwarg("Surface.Allocate", "disableLocationOverride", false, "boolean to disable location overrides (set by e.g. -force_fb, -force_coh, etc.) for this surface");
    kwargs->RegisterKwarg("Surface.Allocate", "smcEngine", false, "smcEngine associated with this surface");
    kwargs->RegisterKwarg("Surface.WriteData", "data", true, "an array of bytes to write to memory");
    kwargs->RegisterKwarg("Surface.WriteData", "offset", false, "the offset into the memory to begin writing (defaults to zero)");
    kwargs->RegisterKwarg("Surface.WriteData", "size", false, "the number of bytes to write to memory (defaults to the size of the data array)");
    kwargs->RegisterKwarg("Surface.ReadData", "offset", false, "this value means the surface will be read at a separared offset");
    kwargs->RegisterKwarg("Surface.ReadData", "size", false, "the number of bytes that will be read from the surface");
    kwargs->RegisterKwarg("Surface.ReadData", "format", false, "this value means how to read this surface. Supported two way to read the data"
            " back. 1. Read the same formate back as it is showned in FB. 2. Read the data back as Pitch. (default is 1)");
    kwargs->RegisterKwarg("Surface.ReadData", "channel", false, "this channel will be used to read this surface."
            " (default is the first same vaspace channel as surface)");
    kwargs->RegisterKwarg("Surface.GetVirtualAddress", "type", false, "the type of virtual address to retrieve. Must be a member of the Surface.VirtualAddressType enum. Defaults to VirtualAddressType.GmmuLocal.");
    kwargs->RegisterKwarg("Surface.GetVirtualAddress", "accessingGpu", false, "the GPU that will be accessing this surface remotely via the returned peer virtual address. This argument is required when type = VirtualAddressType.GmmuPeer and is invalid otherwise.");
    kwargs->RegisterKwarg("Surface.GetPhysicalAddress", "offset", false, "virtual offset");
    kwargs->RegisterKwarg("Surface.GetPhysicalAddress", "type", false, "the type of physical address to retrieve. Must be a member of the Surface.PhysicalAddressType enum. Defaults to PhysicalAddressType.Gpu.");
    kwargs->RegisterKwarg("Surface.SetSpecialization", "spec", true, "Specialization, enum type, value could be one of "
            "Default, VirtOnly, PhysOnly, MapOnly, VirtPhysNoMap.");
    kwargs->RegisterKwarg("Surface.SetVaSpace", "vaSpace", true, "The virtual address space the surface belongs to");
    kwargs->RegisterKwarg("Surface.SetFlaImport", "flaSurface", true, "The the fla surface that's going to be accessed");
    kwargs->RegisterKwarg("Surface.CallwlateCrc", "preCrcValue", false, "previous crc value which can help to callwlate the chunk crc");
    kwargs->RegisterKwarg("Surface.CallwlateCrc", "channel", false, "this channel will be used to read this surface."
            " (default is the first same vaspace channel as surface)");
    kwargs->RegisterKwarg("Surface.GetCrcKey", "channel", false, "this channel will be used to read this surface."
            " (default is the first same vaspace channel as surface)");
    kwargs->RegisterKwarg("Surface.DumpImageToFile", "channel", false, "this channel will be used to read this surface."
            " (default is the first same vaspace channel as surface)");
    kwargs->RegisterKwarg("Surface.DumpImageToFile", "fileName", true, "this file will be used to save image output.");
    kwargs->RegisterKwarg("Surface.MapVirtToPhys", "virtAlloc", true, "The virtual allocation surface to be mapped.");
    kwargs->RegisterKwarg("Surface.MapVirtToPhys", "physAlloc", true, "The physical allocation surface to map to.");
    kwargs->RegisterKwarg("Surface.MapVirtToPhys", "virtOffset", true, "The offset into virtual allocation surface to map to.");
    kwargs->RegisterKwarg("Surface.MapVirtToPhys", "physOffset", true, "The offset into physical allocation surface to map to.");
    kwargs->RegisterKwarg("Surface.MapVirtToPhys", "gpu", false, "The target GPU device on which to map to.");
    kwargs->RegisterKwarg("Surface.SetSurfaceViewParent", "parent", true, "The parent UtlSurface object.");
    kwargs->RegisterKwarg("Surface.SetSurfaceViewParent", "addressOffset", true, "The surface view address offset into DMA buffer.");
    kwargs->RegisterKwarg("Surface.GetEntryLists", "offset", true, "The interested entry's offset which want to query from this surface.");
    kwargs->RegisterKwarg("Surface.GetEntryLists", "level", true, "The interested entry's level which want to query from this surface.");
    kwargs->RegisterKwarg("Surface.GetEntryLists", "size", true, "The interested entry's size which want to query from this surface.");
    kwargs->RegisterKwarg("Surface.GetEntryLists", "subLevelIndex", false, "The interested entry's sub level index which want to query from this surface."
                        "Default value is default. Only required for the PDE0.");
    kwargs->RegisterKwarg("Surface.CreatePeerMapping", "accessingGpu", true, "the GPU that will be accessing this surface remotely using the created peer virtual address.");

    kwargs->RegisterKwarg("Surface.GetPteKind", "dataType", false, "an enum representing the data type returned from this function.  Possible values are Str, Num.  Default is Str");
    kwargs->RegisterKwarg("Surface.GetPteKind", "gpu", false, "the gpu that the surface will be allocated on.  This parameter is only needed if \"STR\" data type is used, the surface is not yet allocated, and the test is running in a hybrid multi GPU system.  Defaults to GPU 0.");
    kwargs->RegisterKwarg("Surface.SetPteKind", "kindStr", false, "a string representing the PTE kind to use when allocating the surface.  This string will be colwerted into a number based on the GPU.  One of kindStr or kindNum must be passed to SetPteKind.");
    kwargs->RegisterKwarg("Surface.SetPteKind", "kindNum", false, "a number representing the PTE kind to use when allocating the surface.  One of kindNum or kindStr must be passed to SetPteKind.");
    kwargs->RegisterKwarg("Surface.SetPteKind", "gpu", false, "the gpu that the surface will be allocated on.  This parameter is only needed if kindStr is used, and if the test is running in a hybrid multi GPU system.  Defaults to GPU 0.");

    py::class_<UtlSurface> surface(module, "Surface");
    surface.def_static("Create", &UtlSurface::Create,
        UTL_DOCSTRING("Surface.Create", "This function creates a Surface object that can be used to allocate memory."),
        py::return_value_policy::reference);
    surface.def_static("CreateFromSurface", &UtlSurface::CreateFromSurface,
        UTL_DOCSTRING("Surface.CreateFromSurface", "This function creates a Surface object from an existent one."),
        py::return_value_policy::reference);
    surface.def("Allocate", &UtlSurface::Allocate,
        UTL_DOCSTRING("Surface.Allocate", "This function allocates the memory for the surface based on previously set properties."));
    surface.def("Free", &UtlSurface::Free,
        UTL_DOCSTRING("Surface.Free", "This function frees the memory for the surface."));
    surface.def("MapCpuAddress", &UtlSurface::MapPy,
        UTL_DOCSTRING("Surface.MapCpuAddress", "This function creates a CPU BAR1 mapping for the surface. "));
    surface.def("UnmapCpuAddress", &UtlSurface::UnmapPy,
        UTL_DOCSTRING("Surface.UnmapCpuAddress", "This function frees the CPU BAR1 mapping for the surface."));
    surface.def("MapVirtToPhys", &UtlSurface::MapVirtToPhys,
        UTL_DOCSTRING("Surface.MapVirtToPhys", "This function maps a virtual alloc to a physical alloc."));
    surface.def("SetSurfaceViewParent", &UtlSurface::SetSurfaceViewParent,
        UTL_DOCSTRING("Surface.SetSurfaceViewParent", "This function set parent surface and DMA buffer offset of the surface view."));
    surface.def("WriteData", &UtlSurface::WriteData,
        UTL_DOCSTRING("Surface.WriteData", "This function writes data to the Surface memory via the CPU (BAR1)."));
    surface.def("ReadData", &UtlSurface::ReadData,
        UTL_DOCSTRING("Surface.ReadData", "This function reads data from the Surface memory via the CPU (BAR1)."),
        py::return_value_policy::reference);
    surface.def("GetVirtualAddress", &UtlSurface::GetVirtualAddress,
        UTL_DOCSTRING("Surface.GetVirtualAddress", "This function returns the GPU virtual address of the Surface."));
    surface.def("GetPhysicalAddress", &UtlSurface::GetPhysicalAddress,
        UTL_DOCSTRING("Surface.GetPhysicalAddress", "This function returns the physical address of the Surface at the given offset within the surface."));
    surface.def("IsAllocated", &UtlSurface::IsAllocated,
        "This function returns True if the Surface has already been allocated.");
    surface.def("SetChannel", &UtlSurface::SetChannel,
        "This function will associate the surface with the specified channel.");
    surface.def("GetPageSize", &UtlSurface::GetPageSize,
        "This function will return the page size of the Surface in bytes.");
    surface.def("GetEntryLists", &UtlSurface::GetEntryListsPy,
        UTL_DOCSTRING("Surface.GetEntryLists", "Get given entry lists according to the input information"));
    surface.def("SetVaSpace", &UtlSurface::SetVaSpacePy,
        UTL_DOCSTRING("Surface.SetVaSpace", "Set vaspace of a surface"));
    surface.def("GetVaSpace", &UtlSurface::GetVaSpacePy,
        UTL_DOCSTRING("Surface.GetVaSpace", "Get corresponding vaspace"),
        py::return_value_policy::reference);
    surface.def("GetGpu", &UtlSurface::GetGpu,
        UTL_DOCSTRING("Surface.GetGpu", "Get the gpu associated with the surface"),
        py::return_value_policy::reference);
    surface.def("SetFlaImport", &UtlSurface::SetFlaImportPy,
        UTL_DOCSTRING("Surface.SetFlaImport", "Set the FLA surface that can be accessed by this surface"));
    surface.def("CreatePeerMapping", &UtlSurface::CreatePeerMappingPy,
        UTL_DOCSTRING("Surface.CreatePeerMapping", "This function creates a peer mapping so that this surface can be accessed remotely by the specified GPU. This function returns the peer virtual address of the mapping, and that same address can also be accessed with the Surface.GetVirtualAddress function. The surface must already be allocated when Surface.CreatePeerMapping is called."));

    py::enum_<AAMode>(surface, "AAMode")
        .value("AA_1X1", AA_1X1)
        .value("AA_2X1", AA_2X1)
        .value("AA_2X1_D3D", AA_2X1_D3D)
        .value("AA_2X2", AA_2X2)
        .value("AA_4X2", AA_4X2)
        .value("AA_4X2_D3D", AA_4X2_D3D)
        .value("AA_4X4", AA_4X4)
        .value("AA_2X2_VC_4", AA_2X2_VC_4)
        .value("AA_2X2_VC_12", AA_2X2_VC_12)
        .value("AA_4X2_VC_8", AA_4X2_VC_8)
        .value("AA_4X2_VC_24", AA_4X2_VC_24);

    py::enum_<Surface2D::Layout>(surface, "Layout")
        // PITCH in trace.hdr
        .value("Pitch", Surface2D::Pitch)
        // SWIZZLED in trace.hdr
        .value("Swizzled", Surface2D::Swizzled)
        // BLOCKLINEAR in trace.hdr
        .value("BlockLinear", Surface2D::BlockLinear)
        // placeholder
        .value("Tiled", Surface2D::Tiled);

    py::enum_<AttrCompr>(surface, "Compr")
        // NONE in trace.hdr
        .value("NoneVal", ComprNone)
        // REQUIRED in trace.hdr
        .value("Required", ComprRequired)
        // ANY in trace.hdr
        .value("Any", ComprAny);

    py::enum_<AttrZLwll>(surface, "ZLwll")
        // NONE in trace.hdr
        .value("NoneVal", ZLwllNone)
        // REQUIRED in trace.hdr
        .value("Required", ZLwllRequired)
        // ANY in trace.hdr
        .value("Any", ZLwllAny);

    py::enum_<AttrType>(surface, "Type")
        // IMAGE in trace.hdr
        .value("Image", TypeImage)
        // DEPTH in trace.hdr
        .value("Depth", TypeDepth)
        // TEXTURE in trace.hdr
        .value("Texture", TypeTexture)
        // VIDEO in trace.hdr
        .value("Video", TypeVideo);

    py::enum_<Memory::Protect>(surface, "Protect")
        // Defalut
        .value("Default", Memory::ProtectDefault)
        // READ_ONLY in trace.hdr
        .value("ReadOnly", Memory::Readable)
        // WRITE_ONLY in trace.hdr
        .value("WriteOnly", Memory::Writeable)
        // Exelwtable
        .value("Exelwtable", Memory::Exelwtable)
        // READ_WRITE in trace.hdr
        .value("ReadWrite", Memory::ReadWrite);

    py::enum_<Toggle>(surface, "Toggle")
        // Defalut
        .value("Default", ToggleDefault)
        // OFF in trace.hdr
        .value("Off", ToggleOff)
        // ON in trace.hdr
        .value("On", ToggleOn);

    py::enum_<Surface2D::MemoryMappingMode>(surface, "MapMode")
        // Default
        .value("Default", Surface2D::MapDefault)
        // DIRECT in trace.hdr
        .value("Direct", Surface2D::MapDirect)
        // REFLECTED in trace.hdr
        .value("Reflected", Surface2D::MapReflected);

    py::enum_<Surface2D::Specialization>(surface, "Specialization")
        // Default
        .value("Default", Surface2D::NoSpecialization)
        .value("VirtOnly", Surface2D::VirtualOnly)
        .value("PhysOnly", Surface2D::PhysicalOnly)
        .value("MapOnly", Surface2D::MapOnly)
        .value("VirtPhysNoMap", Surface2D::VirtualPhysicalNoMap);

    py::enum_<VirtualAddressType>(surface, "VirtualAddressType")
        .value("GmmuLocal", GmmuLocal)
        .value("GmmuPeer", GmmuPeer);

    py::enum_<PhysicalAddressType>(surface, "PhysicalAddressType")
        .value("Cpu", Cpu)
        .value("Gpu", Gpu);

    py::enum_<PteKindDataType>(surface, "PteKindDataType")
        .value("Str", PteKindDataType::Str)
        .value("Num", PteKindDataType::Num);

    surface.def("CallwlateCrc", &UtlSurface::CallwlateCrc,
        UTL_DOCSTRING("Surface.CallwlateCrc",
            "This function will to be callwlating a CRC value based on the current contents"
            " of the surface at the time the function is called. User should make sure there"
            " are no other this surface write when you call this function."));
    surface.def("GetCrcKey", &UtlSurface::GetCrcKey,
        UTL_DOCSTRING("Surface.GetCrcKey", "This function will return the crc key of the Surface."));
    surface.def("DumpImageToFile", &UtlSurface::DumpImageToFile,
        UTL_DOCSTRING("Surface.DumpImageToFile", "This function will dump the image to specified file."));

    /******Surface property********/
    surface.def_property("displayable",
        &UtlSurface::GetDisplayable, &UtlSurface::SetDisplayable);
    surface.def_property("upr",
        &UtlSurface::GetUpr, &UtlSurface::SetUpr);
    surface.def_property("access",
        &UtlSurface::GetAccess, &UtlSurface::SetAccess);
    surface.def_property("aperture",
        &UtlSurface::GetAperture, &UtlSurface::SetAperture,
        "A string that represents the memory type for this surface.  Possible values include \"VIDEO\", \"COHERENT_SYSMEM\", and \"NONCOHERENT_SYSMEM\".  For Surface objects created with Surface.Create, the value defaults to \"VIDEO\".");
    surface.def_property("height",
        &UtlSurface::GetHeight, &UtlSurface::SetHeight,
        "An integer that represents the height of the surface.  Defaults to zero.");
    surface.def_property_readonly("name", &UtlSurface::GetName,
        "A read-only string representing the name of the surface.");
    surface.def_property("physContig",
        &UtlSurface::GetPhysContig,
        &UtlSurface::SetPhysContig);
    surface.def_property("size",
        &UtlSurface::GetSize, &UtlSurface::SetSize,
        "An integer that represents the total size of the surface.  Defaults to zero.");
    surface.def_property("fixedVirtAddress",
        &UtlSurface::GetFixedVirtualAddress,
        &UtlSurface::SetFixedVirtualAddress,
        "If the surface is expected to be allocated to a fixed virtual address, this member will be an integer representing that address.  Otherwise this member will be None, indicating no fixed virtual address constraint.  This member is only writable before the surface is allocated.  Writing to this member will set the virtualAddressRange member to None.");
    surface.def_property("virtAddressRange",
        &UtlSurface::GetVirtualAddressRange,
        &UtlSurface::SetVirtualAddressRange,
        "If the surface is expected to be allocated within a range of virtual addresses, this member will be a tuple of integers of size two, representing a virtual address range.  Otherwise, the member will be None, indicating no virtual address range constraint.  In the case of a tuple, the first member is the minimum address and the second member is the maximum address.  This member is only writable before the surface is allocated.  Writing to this member will set the fixedVirtAddress member to None.");
    surface.def_property("fixedPhysAddress",
        &UtlSurface::GetFixedPhysicalAddress,
        &UtlSurface::SetFixedPhysicalAddress,
        "If the surface is expected to be allocated to a fixed physical address, this member will be an integer representing that address.  Otherwise this member will be None, indicating no fixed physical address constraint.  This member is only writable before the surface is allocated.  Writing to this member will set the physicalAddressRange to None.");
    surface.def_property("physAddressRange",
        &UtlSurface::GetPhysicalAddressRange,
        &UtlSurface::SetPhysicalAddressRange,
        "If the surface is expected to be allocated within a range of physical addresses, this member will be a tuple of integers of size two, representing a physical address range.  Otherwise, the member will be None, indicating no physical address range constraint.  In the case of a tuple, the first member is the minimum address and the second member is the maximum address.  This member is only writable before the surface is allocated.  Writing to this member will set the fixedPhysicalAddress member to None.");
    surface.def_property("width",
        &UtlSurface::GetWidth, &UtlSurface::SetWidth,
        "An integer that represents the width of the surface.  Defaults to zero.");
    surface.def_property("colorFormat",
        &UtlSurface::GetColorFormat,
        &UtlSurface::SetColorFormat,
        "A string that represents the color format of the surface. Defaults to 'Y8'.");
    surface.def_property("aaMode",
        &UtlSurface::GetAAMode,
        &UtlSurface::SetAAMode,
        "An enum that represents the sample mode (AA mode) of the surface. Defaults to AA_1X1.");
    surface.def_property("layout",
        &UtlSurface::GetLayout,
        &UtlSurface::SetLayout,
        "An enum that represents the layout of the surface. Defaults to Pitch.");
    surface.def_property("compression",
        &UtlSurface::GetCompression,
        &UtlSurface::SetCompression,
        "An enum that represents the compression of the surface. Defaults to NoneVal.");
    surface.def_property("zlwll",
        &UtlSurface::GetZLwll,
        &UtlSurface::SetZLwll,
        "An enum that represents the zlwll of the surface. Defaults to NoneVal.");
    surface.def_property("type",
        &UtlSurface::GetType,
        &UtlSurface::SetType,
        "An enum that represents the type of the surface. Defaults to Image.");
    surface.def_property("shaderProtect",
        &UtlSurface::GetShaderProtect,
        &UtlSurface::SetShaderProtect,
        "An enum that represents the shaderProtect of the surface. Defaults to ReadWrite.");
    surface.def_property("priv",
        &UtlSurface::GetPriv,
        &UtlSurface::SetPriv,
        "A bool that represents the privileged of the surface. Defaults to True.");
    surface.def_property("gpuCache",
        &UtlSurface::GetGpuCacheMode,
        &UtlSurface::SetGpuCacheMode,
        "A toggle enum that represents the GPU cahce mode of the surface. Defaults to On.");
    surface.def_property("p2pGpuCache",
        &UtlSurface::GetP2PGpuCacheMode,
        &UtlSurface::SetP2PGpuCacheMode,
        "A toggle enum that represents the P2P GPU cahce mode of the surface. Defaults to On.");
    surface.def_property("zbc",
        &UtlSurface::GetZbcMode,
        &UtlSurface::SetZbcMode,
        "A string that represents the ZBC mode of the surface. Defaults to On.");
    surface.def_property("depth",
        &UtlSurface::GetDepth,
        &UtlSurface::SetDepth,
        "An integer that represents the depth of the surface. Defaults to zero.");
    surface.def_property("pitch",
        &UtlSurface::GetPitch,
        &UtlSurface::SetPitch,
        "An integer that represents the pitch of the surface. Defaults to zero.");
    surface.def_property("blockWidth",
        &UtlSurface::GetLogBlockWidth,
        &UtlSurface::SetLogBlockWidth,
        "An integer that represents the block width of the surface. Defaults to zero.");
    surface.def_property("blockHeight",
        &UtlSurface::GetLogBlockHeight,
        &UtlSurface::SetLogBlockHeight,
        "An integer that represents the block height of the surface. Defaults to zero.");
    surface.def_property("blockDepth",
        &UtlSurface::GetLogBlockDepth,
        &UtlSurface::SetLogBlockDepth,
        "An integer that represents the block depth of the surface. Defaults to zero.");
    surface.def_property("arraySize",
        &UtlSurface::GetArraySize,
        &UtlSurface::SetArraySize,
        "An integer that represents the array size of the surface. Defaults to zero.");
    surface.def_property("mipLevel",
        &UtlSurface::GetMipLevels,
        &UtlSurface::SetMipLevels,
        "An integer that represents the miplevels of the surface. Defaults to zero.");
    surface.def_property("border",
        &UtlSurface::GetBorder,
        &UtlSurface::SetBorder,
        "An integer that represents the border of the surface. Defaults to zero.");
    surface.def_property("lwbemap",
        &UtlSurface::GetLwbemap,
        &UtlSurface::SetLwbemap,
        "A bool that represents the lwbemap of the surface. Defaults to False.");
    surface.def_property("physAlignment",
        &UtlSurface::GetAlignment,
        &UtlSurface::SetAlignment,
        "An integer that represents the physical alignment of the surface. Defaults to zero.");
    surface.def_property("virtAlignment",
        &UtlSurface::GetVirtAlignment,
        &UtlSurface::SetVirtAlignment,
        "An integer that represents the virtual alignment of the surface. Defaults to zero.");
    surface.def_property("attrOverride",
        &UtlSurface::GetAttrOverride,
        &UtlSurface::ConfigFromAttr,
        "An integer that represents the attr override of the surface. Defaults to zero.");
    surface.def_property("attr2Override",
        &UtlSurface::GetAttr2Override,
        &UtlSurface::ConfigFromAttr2,
        "An integer that represents the attr2 override of the surface. Defaults to zero.");
    // TYPE_OVERRIDE not yet added, owned by TraceModule
    surface.def_property("loopback",
        &UtlSurface::GetLoopback,
        &UtlSurface::SetLoopback,
        "A bool that represents if the primary GPU mapping of this surface should use P2P loopback mode");
    surface.def_property("skedReflected",
        &UtlSurface::GetSkedReflected,
        &UtlSurface::SetSkedReflected,
        "A bool that represents the sked-reflected of the surface. Defaults to False.");
    // PAGE_SIZE not yet added, not GpuDev in UtlSurface
    surface.def_property("mapMode",
        &UtlSurface::GetMemoryMappingMode,
        &UtlSurface::SetMemoryMappingMode,
        "An enum that represents the map mode of the surface. Defaults to Direct.");
    // VPR not yet added, owned by TraceModule
    surface.def_property("tileWidthInGobs",
        &UtlSurface::GetTileWidthInGobs,
        &UtlSurface::SetTileWidthInGobs,
        "An integer that represents the tile width in GOBs of the surface. Defaults to zero.");
    surface.def_property("atsMap",
        &UtlSurface::GetAtsMapped,
        &UtlSurface::SetAtsMapped,
        "A bool that represents the ATS-maped of the surface. Defaults to False.");
    surface.def_property("noGmmuMap",
        &UtlSurface::GetNoGmmuMap,
        &UtlSurface::SetNoGmmuMap,
        "A bool that represents the no-GMMU-map of the surface. Defaults to False.");
    surface.def_property("atsPageSize",
        &UtlSurface::GetAtsPageSize,
        &UtlSurface::SetAtsPageSize,
        "An integer that represents the ATS page size of the surface. Defaults to zero.");
    surface.def_property("pageSize",
        &UtlSurface::GetPageSize,
        &UtlSurface::SetPageSize,
        "An integer that represents the page size of the surface in bytes.");
    surface.def_property("htex",
        &UtlSurface::GetIsHtex,
        &UtlSurface::SetIsHtex,
        "A bool that represents if is HTex of the surface. Defaults to False.");
    surface.def("SetSpecialization", &UtlSurface::SetSpecialization,
        UTL_DOCSTRING("Surface.SetSpecialization", "This function sets a Specialization (in enum type) to a UTL surface."));
    surface.def("SetIsSurfaceView", &UtlSurface::SetIsSurfaceView,
        UTL_DOCSTRING("Surface.SetIsSurfaceView", "This function sets a UTL surface as a surface view."));
    surface.def_property_readonly("virtOnly", &UtlSurface::IsVirtualOnly,
        "A read-only bool representing if the surface specialization is virtual alloc only.");
    surface.def_property_readonly("physOnly", &UtlSurface::IsPhysicalOnly,
        "A read-only bool representing if the surface specialization is physical alloc only.");
    surface.def_property_readonly("mapOnly", &UtlSurface::IsMapOnly,
        "A read-only bool representing if the surface specialization is map only without virt or phys alloc.");
    surface.def_property_readonly("hasVirt", &UtlSurface::HasVirtual,
        "A read-only bool representing if the surface has virtual alloc.");
    surface.def_property_readonly("hasPhys", &UtlSurface::HasPhysical,
        "A read-only bool representing if the surface has physical alloc.");
    surface.def_property_readonly("hasMap", &UtlSurface::HasMap,
        "A read-only bool representing if the surface has mapping.");

    surface.def("GetPteKind", &UtlSurface::GetPteKind,
        UTL_DOCSTRING("Surface.GetPteKind", "This function returns the PTE kind that is used when the surface is allocated."));
    surface.def("SetPteKind", &UtlSurface::SetPteKind,
        UTL_DOCSTRING("Surface.SetPteKind", "This function assigns the PTE kind that should be used when the surface is allocated.  (Normally RM chooses the PTE kind based on the surface properties.)  This function may not be called after the surface is allocated."));

    py::enum_<FORMAT>(surface, "FORMAT")
        .value("RAW", RAW)
        .value("PITCH", PITCH);
}

// This constructor should only be called from UtlSurface::Create.
//
UtlSurface::UtlSurface(MdiagSurf* mdiagSurface)
{
    m_Surface = mdiagSurface;
}

UtlSurface::~UtlSurface()
{
    m_Surface = nullptr;
    m_OwnedSurface.reset(nullptr);
}

void UtlSurface::FreeSurfaces()
{
    s_ScriptSurfaces.clear();
    s_Surfaces.clear();
}

// Free the UtlSurface object for the passed MdiagSurf object
//
void UtlSurface::FreeSurface(MdiagSurf* mdiagSurface)
{
    s_Surfaces.erase(mdiagSurface);
}

// This function can be called from a UTL script to create a surface.
//
UtlSurface* UtlSurface::Create(py::kwargs kwargs)
{
    return DoCreate(kwargs, nullptr);
}

UtlSurface* UtlSurface::DoCreate(py::kwargs kwargs, CreateFunction createFunction)
{
    UtlUtility::RequirePhase(Utl::InitEventPhase | Utl::RunPhase,
        "Surface.Create");

    const string& name = UtlUtility::GetRequiredKwarg<string>(kwargs, "name",
        "Surface.Create");

    // If a userTest parameter is passed in the kwargs, the surface is
    // associated with the given user test.  The UtlSurface::s_Surfaces member
    // will still own the UtlSurface object on the C++ side, but the user test
    // will own the Surface object on the Python side.
    UtlUserTest* userTest = nullptr;
    py::object userTestObj;
    UtlTest* utlTest = Utl::Instance()->GetTestFromScriptId();

    if (UtlUtility::GetOptionalKwarg<py::object>(&userTestObj, kwargs, "userTest", "Surface.Create"))
    {
        userTest = UtlUserTest::GetTestFromPythonObject(userTestObj);

        if (userTest == nullptr)
        {
            UtlUtility::PyError("invalid userTest argument.");
        }
        else if (userTest->GetSurface(name) != nullptr)
        {
            UtlUtility::PyError(
                "userTest object already has surface with name %s.",
                name.c_str());
        }
    }
    else
    {
        auto iter = s_ScriptSurfaces.find(utlTest);
        if ((iter != s_ScriptSurfaces.end()) &&
            (*iter).second.find(name) != (*iter).second.end())
        {
            UtlUtility::PyError("surface with name '%s' already exists.",
                name.c_str());
        }
    }

    bool keepCpuMapping = false;
    UtlUtility::GetOptionalKwarg<bool>(&keepCpuMapping, kwargs,
        "keepCpuMapping", "Surface.Create");

    unique_ptr<MdiagSurf> mdiagSurface = make_unique<MdiagSurf>();
    mdiagSurface->SetName(name);
    mdiagSurface->SetProtect(Memory::Readable);

    // Default to a 1 byte-per-pixel format.
    mdiagSurface->SetColorFormat(ColorUtils::Y8);

    UtlSurface* utlSurface = GetFromMdiagSurf(mdiagSurface.get(), createFunction);
    utlSurface->m_OwnedSurface = move(mdiagSurface);
    utlSurface->m_KeepCpuMapping = keepCpuMapping;

    if (userTest != nullptr)
    {
        userTest->AddSurface(name, utlSurface);
        UtlTest::GetFromTestPtr(userTest)->AddSurface(utlSurface);
    }
    else
    {
        UtlTest* utlTest = Utl::Instance()->GetTestFromScriptId();
        auto iter = s_ScriptSurfaces.find(utlTest);
        if (iter == s_ScriptSurfaces.end())
        {
            iter = s_ScriptSurfaces.emplace(
                make_pair(utlTest, map<string, UtlSurface*>())).first;
        }
        (*iter).second.emplace(make_pair(name, utlSurface));
    }

    return utlSurface;
}

// This function can be called from a UTL script to
// create a surface from an existent one.
//
UtlSurface* UtlSurface::CreateFromSurface(py::kwargs kwargs)
{
    UtlUtility::RequirePhase(Utl::InitEventPhase | Utl::RunPhase,
        "Surface.CreateFromSurface");

    const string& name = UtlUtility::GetRequiredKwarg<string>(kwargs, "name",
        "Surface.CreateFromSurface");
    UtlSurface* pSrc = UtlUtility::GetRequiredKwarg<UtlSurface*>(kwargs, "source",
        "Surface.CreateFromSurface");

    UtlSurface* pSurf = UtlSurface::Create(kwargs);


    UtlGil::Release gil;

    if (pSrc->IsAllocated())
    {
        UtlUtility::PyError("Can't create a new surface from an allocated source surface %s.",
            pSrc->GetName().c_str());
    }
    *pSurf->m_Surface = *pSrc->m_Surface;
    pSurf->m_Surface->SetName(name);

    return pSurf;
}

UtlSurface* UtlSurface::GetFromMdiagSurf(MdiagSurf* mdiagSurface)
{
    return GetFromMdiagSurf(mdiagSurface, nullptr);
}

UtlSurface* UtlSurface::GetFromMdiagSurf(MdiagSurf* mdiagSurface, CreateFunction createFunction)
{
    if (s_Surfaces.count(mdiagSurface) == 0)
    {
        unique_ptr<UtlSurface> utlSurface;
        if (createFunction != nullptr)
        {
            utlSurface = createFunction(mdiagSurface);
        }
        else
        {
            utlSurface.reset(new UtlSurface(mdiagSurface));
        }

        s_Surfaces[mdiagSurface] = move(utlSurface);
    }

    return s_Surfaces[mdiagSurface].get();
}

// This function can be called from a UTL script to allocate a surface
// that was instantiated from a UTL script.
//
void UtlSurface::Allocate(py::kwargs kwargs)
{
    UtlGpu* gpu = nullptr;
    if (!UtlUtility::GetOptionalKwarg<UtlGpu*>(&gpu, kwargs, "gpu", "Surface.Allocate"))
    {
        // If the user doesn't specify a GPU, use GPU 0 by default.
        gpu = UtlGpu::GetGpus()[0];
    }

    bool disableLocationOverride = false;

    INT32 oldLocationOverride = Surface2D::NO_LOCATION_OVERRIDE;
    UtlUtility::GetOptionalKwarg<bool>(&disableLocationOverride, kwargs, "disableLocationOverride", "Surface.Allocate");

    UtlSmcEngine* pUtlSmcEngine = nullptr;
    UtlUtility::GetOptionalKwarg<UtlSmcEngine*>(&pUtlSmcEngine, kwargs,
        "smcEngine", "Surface.Allocate");

    // If the user didn't specify an SMC engine and this surface belongs to
    // a test, use the test's SMC engine if it has one.
    if ((pUtlSmcEngine == nullptr) && (GetTest() != nullptr))
    {
        pUtlSmcEngine = GetTest()->GetSmcEngine();
    }

    if (pUtlSmcEngine)
    {
        m_LwRm = gpu->GetGpuResource()->GetLwRmPtr(pUtlSmcEngine->GetSmcEngineRawPtr());
    }

    UtlGil::Release gil;

    if (disableLocationOverride)
    {
        oldLocationOverride = Surface2D::GetLocationOverride();
        Surface2D::SetLocationOverride(Surface2D::NO_LOCATION_OVERRIDE);
    }

    RC rc = DoAllocate(m_Surface, gpu->GetGpudevice());

    if (disableLocationOverride)
    {
        Surface2D::SetLocationOverride(oldLocationOverride);
    }

    UtlUtility::PyErrorRC(rc, "Surface.Allocate failed");

    // Print the allocation info to mods.log.
    BuffInfo buffInfo;
    buffInfo.SetDmaBuff(m_Surface->GetName(), *m_Surface);
    buffInfo.SetSizeBL(*m_Surface);
    buffInfo.SetSizePix(*m_Surface);
    buffInfo.Print("UTL Surface.Allocate", gpu->GetGpudevice());
}

RC UtlSurface::DoAllocate(MdiagSurf* pMdiagSurf, GpuDevice *pGpuDevice)
{
    return pMdiagSurf->Alloc(pGpuDevice, GetLwRm());
}

void UtlSurface::Free()
{
    UtlGil::Release gil;

    m_Surface->Free();
}

// This function can be called from a UTL script to write data to the surface
// via the CPU.
//
void UtlSurface::WriteData(py::kwargs kwargs)
{
    vector<UINT08> data = UtlUtility::GetRequiredKwarg<vector<UINT08>>(kwargs,
        "data", "Surface.WriteData");

    UINT64 offset = 0ULL;
    UtlUtility::GetOptionalKwarg<UINT64>(&offset, kwargs, "offset", "Surface.WriteData");

    UINT64 size = data.size();
    UtlUtility::GetOptionalKwarg<UINT64>(&size, kwargs, "size", "Surface.WriteData");

    if (size > data.size())
    {
        string errorMsg = Utility::StrPrintf(
            "size parameter is larger than data parameter list size (0x%llx > 0x%zx).",
            size, data.size());
        UtlUtility::PyError(errorMsg.c_str());
    }
    else if (offset + size > m_Surface->GetSize())
    {
        string errorMsg = Utility::StrPrintf(
            "offset 0x%llx plus size 0x%llx is larger than surface size 0x%llx.",
            offset, size, m_Surface->GetSize());
        UtlUtility::PyError(errorMsg.c_str());
    }

    UtlGil::Release gil;

    RC rc = Map();
    UtlUtility::PyErrorRC(rc, "Surface.WriteData map failed");
    Platform::VirtualWr((UINT08 *)m_Surface->GetAddress() + offset, &data[0],
        size);
    rc = Platform::FlushCpuWriteCombineBuffer();
    UtlUtility::PyErrorRC(rc, "Surface.WriteData flush failed");
    Unmap();
}

// This function can be called from a UTL script to read data from the surface
// via the CPU.
//
vector<UINT08> UtlSurface::ReadData(py::kwargs kwargs)
{
    UINT64 offset = 0ULL;
    UtlUtility::GetOptionalKwarg<UINT64>(&offset, kwargs, "offset", "Surface.ReadData");

    UINT64 size = m_Surface->GetSize();
    UtlUtility::GetOptionalKwarg<UINT64>(&size, kwargs, "size", "Surface.ReadData");

    if (offset + size > m_Surface->GetSize())
    {
        UtlUtility::PyError("offset 0x%llx plus size 0x%llx is larger than surface size 0x%llx.",
                offset, size, m_Surface->GetSize());
    }

    UtlSurface::FORMAT format = UtlSurface::RAW;
    UtlUtility::GetOptionalKwarg<UtlSurface::FORMAT>(&format, kwargs, "format", "Surface.ReadData");

    UtlChannel * pChannel = nullptr;
    UtlUtility::GetOptionalKwarg<UtlChannel *>(&pChannel, kwargs, "channel",
        "Surface.ReadData");

    UtlGil::Release gil;

    vector<UINT08> data(size);

    switch (format)
    {
        case UtlSurface::RAW:
            ReadRawData(offset, size, &data[0]);
            break;
        case UtlSurface::PITCH:
            ReadPitchData(offset, size, &data[0], pChannel);
            break;
        default:
            MASSERT(!"UnSupported format to read surface data.\n");
    }

    return data;
}

// This function can be called from a UTL script to get a virtual address
// of the specified type (lwrrently GMMU local and GMMU peer are supported).
//
UINT64 UtlSurface::GetVirtualAddress(py::kwargs kwargs) const
{
    UINT64 virtualAddress = ~0x0ULL;

    VirtualAddressType type = GmmuLocal;
    UtlUtility::GetOptionalKwarg<VirtualAddressType>(&type, kwargs,
        "type", "Surface.GetVirtualAddress");

    UtlGpu* accessingGpu = nullptr;
    UtlUtility::GetOptionalKwarg<UtlGpu*>(&accessingGpu, kwargs,
        "accessingGpu", "Surface.GetVirtualAddress");

    UtlGil::Release gil;

    switch (type)
    {
        case GmmuLocal:
            if (accessingGpu != nullptr)
            {
                UtlUtility::PyError("accessingGpu is not a valid argument when type = VirtualAddressType::GmmuLocal");
            }

            virtualAddress = m_Surface->GetCtxDmaOffsetGpu();
            break;

        case GmmuPeer:
            if (accessingGpu == nullptr)
            {
                UtlUtility::PyError("Surface.GetVirtualAddress requires a valid accessingGpu argument when type = VirtualAddressType::GmmuPeer");
            }

            virtualAddress = GetGmmuPeerVirtualAddress(accessingGpu);
            break;

        default:
            UtlUtility::PyError("unrecognized type passed to function Surface.GetVirtualAddress");
    }

    return virtualAddress;
}

UINT64 UtlSurface::GetGmmuPeerVirtualAddress(UtlGpu* accessingGpu) const
{
    UINT32 hostSubdevInst =
        m_Surface->GetGpuDev()->GetSubdevice(0)->GetSubdeviceInst();
    GpuDevice* accessingGpuDevice = accessingGpu->GetGpudevice();
    UINT32 accessingSubdevInst =
        accessingGpuDevice->GetSubdevice(0)->GetSubdeviceInst();

    return m_Surface->GetCtxDmaOffsetGpuPeer(
        hostSubdevInst,
        accessingGpuDevice,
        accessingSubdevInst);
}

UINT64 UtlSurface::GetPhysicalAddress(py::kwargs kwargs)
{
    UINT64 offset = 0ULL;
    UtlUtility::GetOptionalKwarg<UINT64>(&offset, kwargs, "offset", "Surface.GetPhysicalAddress");

    PhysicalAddressType type = Gpu;
    UtlUtility::GetOptionalKwarg<PhysicalAddressType>(&type, kwargs,
        "type", "Surface.GetPhysicalAddress");

    UtlGil::Release gil;

    UINT64 physAddress = ~0ULL;
    RC rc;
    switch (type)
    {
        case Cpu:
            if (m_Surface->GetLocation() != Memory::Fb)
            {
                UtlUtility::PyError("Cannot get BAR1 PA of sysmem surface");
            }
            if (!m_Surface->IsMapped())
            {
                UtlUtility::PyError("Cannot get BAR1 PA of unmapped surface. Please call Surface.Map() prior to Surface.GetPhysicalAddress.");
            }
            physAddress = reinterpret_cast<UINT64>(m_Surface->GetPhysAddress());
            break;

        case Gpu:
            rc = m_Surface->GetPhysAddress(offset, &physAddress);
            break;

        default:
            UtlUtility::PyError("unrecognized type passed to function Surface.GetPhysicalAddress");
    }

    UtlUtility::PyErrorRC(rc, "Surface.GetPhysicalAddress failed.");

    return physAddress;
}

// This function can be called from a UTL script to determine if the surface
// has been allocated.
//
bool UtlSurface::IsAllocated() const
{
    if (IsVirtualOnly())
    {
        return (m_Surface->GetVirtMemHandle() != 0);
    }

    return (m_Surface->GetMemHandle() != 0);
}

void UtlSurface::CheckIfAllocated(const char* prop) const
{
    MASSERT(prop);
    if (IsAllocated())
    {
        UtlUtility::PyError("Can't set %s on a surface after it has been allocated.",
            prop);
    }
}

namespace
{
    template <typename Type>
    class ToggleType
    {
    private:
        typedef Type ModeType;

    public:
        static ModeType GetMode(const UtlSurface::Toggle tog);
        static UtlSurface::Toggle GetToggle(const ModeType mode);
    };

    template <>
    struct ToggleType<Surface2D::GpuCacheMode>
    {
        typedef Surface2D::GpuCacheMode ModeType;

        static UtlSurface::Toggle GetToggle(const ModeType mode)
        {
            switch (mode)
            {
            case Surface2D::GpuCacheOn:
                return UtlSurface::ToggleOn;
            case Surface2D::GpuCacheOff:
                return UtlSurface::ToggleOff;
            default:
                break;
            }
            return UtlSurface::ToggleDefault;
        }

        static ModeType GetMode(const UtlSurface::Toggle tog)
        {
            static ModeType modeList[] =
            {
                Surface2D::GpuCacheDefault,
                Surface2D::GpuCacheOff,
                Surface2D::GpuCacheOn
            };
            return modeList[tog];
        }
    };

    template <>
    struct ToggleType<Surface2D::ZbcMode>
    {
        typedef Surface2D::ZbcMode ModeType;

        static UtlSurface::Toggle GetToggle(const ModeType mode)
        {
            switch (mode)
            {
            case Surface2D::ZbcOn:
                return UtlSurface::ToggleOn;
            case Surface2D::ZbcOff:
                return UtlSurface::ToggleOff;
            default:
                break;
            }
            return UtlSurface::ToggleDefault;
        }

        static ModeType GetMode(const UtlSurface::Toggle tog)
        {
            static ModeType modeList[] =
            {
                Surface2D::ZbcDefault,
                Surface2D::ZbcOff,
                Surface2D::ZbcOn
            };
            return modeList[tog];
        }
    };

    void UnsupportedPropValue(const char* propName, const string& propVal)
    {
        UtlUtility::PyError("Unsupported surface %s property value %s.",
            propName,
            propVal.c_str());
    }

    void UnsupportedPropValue(const char* propName, UINT32 propVal)
    {
        UnsupportedPropValue(propName, std::to_string(propVal).c_str());
    }
}

// This function can be called from a UTL script to associate the surface
// with the specified channel.  This allows the channel to access the surface
// because the surface will be allocated with the same RM client and
// virtual address space as the channel.
//
void UtlSurface::SetChannel(UtlChannel* channel)
{
    if (IsAllocated())
    {
        UtlUtility::PyError("can't set channel on a surface after it has been allocated.");
    }

    if (m_LwRm)
    {
        MASSERT(m_LwRm == channel->GetLwRmPtr());
    }
    else
    {
        m_LwRm = channel->GetLwRmPtr();
    }
    m_Surface->SetGpuVASpace(channel->GetVaSpaceHandle());
    m_pChannel = channel;
}

// For global surface which channel is set by
// UtlSurface.SetChannel
// For per trace sruface some channel bind
// information will be specified in the hdr
UtlChannel * UtlSurface::GetChannel()
{
    if (m_pChannel == nullptr)
    {
        if (GetTest() != nullptr)
        {
            LWGpuChannel * pChannel = m_Surface->GetChannel();
            m_pChannel = UtlChannel::GetFromChannelPtr(pChannel);
        }
    }

    return m_pChannel;
}

UINT64 UtlSurface::GetPageSize() const
{
    UtlGil::Release gil;

    UINT32 pageSize = m_Surface->GetActualPageSizeKB();

    if (pageSize == ~0U)
    {
        UtlUtility::PyError("Surface.GetPageSize failed.");
    }

    return static_cast<UINT64>(pageSize) * 1024ULL;
}

void UtlSurface::SetPageSize(UINT64 pageSize)
{
    UtlGil::NoYield gil;

    m_Surface->SetPageSize(static_cast<UINT32>(pageSize / 1024ULL));
}

// The following set of functions are used as property access functions
// for the UTL Surface object.
void UtlSurface::SetDisplayable(bool displayable)
{
    if (IsAllocated())
    {
        UtlUtility::PyError("can't set displayable property on a surface after it has been allocated.");
    }

    m_Surface->SetDisplayable(displayable);
}

void UtlSurface::SetUpr(bool bAllocInUpr)
{
    if (IsAllocated())
    {
        UtlUtility::PyError("can't set upr property on a surface after it has been allocated.");
    }

    m_Surface->SetAllocInUpr(bAllocInUpr);
}

string UtlSurface::GetAccess() const
{
    switch (m_Surface->GetProtect())
    {
        case Memory::Readable:
            return "READ_ONLY";
        case Memory::Writeable:
            return "WRITE_ONLY";
        case Memory::ReadWrite:
            return "READ_WRITE";
        default:
            return "UNKNOWN";
    }
}

void UtlSurface::SetAccess(const string& access)
{
    if (IsAllocated())
    {
        UtlUtility::PyError("can't set access on a surface after it has been allocated.");
    }

    if (access == "READ_ONLY")
    {
        m_Surface->SetProtect(Memory::Readable);
    }
    else if (access == "WRITE_ONLY")
    {
        m_Surface->SetProtect(Memory::Writeable);
    }
    else if (access == "READ_WRITE")
    {
        m_Surface->SetProtect(Memory::ReadWrite);
    }
    else
    {
        UtlUtility::PyError("unrecognized Surface.access value %s.\n",
            access.c_str());
    }
}

string UtlSurface::GetAperture() const
{
    switch (m_Surface->GetLocation())
    {
        case Memory::Fb:
            return "VIDEO";
        case Memory::Coherent:
        case Memory::CachedNonCoherent:
            return "COHERENT_SYSMEM";
        case Memory::NonCoherent:
            return "NONCOHERENT_SYSMEM";
        case Memory::Optimal:
            return "OPTIMAL";
        default:
            return "UNKNOWN";
    }
}

void UtlSurface::SetAperture(const string& aperture)
{
    if (IsAllocated())
    {
        UtlUtility::PyError("can't set aperture on a surface after it has been allocated.");
    }

    // PK: Keeping the same strings that ALLOC_SURFACE uses for now.
    // Tempted to change "VIDEO" to "FB".

    if (aperture == "VIDEO")
    {
        m_Surface->SetLocation(Memory::Fb);
    }
    else if (aperture == "COHERENT_SYSMEM")
    {
        m_Surface->SetLocation(Memory::Coherent);
    }
    else if (aperture == "NONCOHERENT_SYSMEM")
    {
        m_Surface->SetLocation(Memory::NonCoherent);
    }
    else
    {
        UtlUtility::PyError("Surface.Location: unrecognized aperture %s.\n",
            aperture.c_str());
    }
}

void UtlSurface::SetHeight(UINT32 height)
{
    if (IsAllocated())
    {
        UtlUtility::PyError("can't set height on a surface after it has been allocated.");
    }
    m_Surface->SetHeight(height);
}

py::object UtlSurface::GetFixedVirtualAddress() const
{
    if (m_Surface->HasFixedVirtAddr())
    {
        return py::cast(m_Surface->GetFixedVirtAddr());
    }

    return py::none();
}

void UtlSurface::SetFixedVirtualAddress(py::object address)
{
    CheckIfAllocated("fixedVirtAddress");

    if (address == py::none())
    {
        m_Surface->SetFixedVirtAddr(0);
    }
    else if (py::isinstance<py::int_>(address))
    {
        m_Surface->SetFixedVirtAddr(address.cast<UINT64>());
    }
    else
    {
        UtlUtility::PyError("fixedVirtAddress must be assigned to an integer value.");
    }
}

py::object UtlSurface::GetVirtualAddressRange() const
{
    if (m_Surface->HasVirtAddrRange())
    {
        return py::make_tuple(m_Surface->GetVirtAddrRangeMin(),
            m_Surface->GetVirtAddrRangeMax());
    }

    return py::none();
}

void UtlSurface::SetVirtualAddressRange(py::object range)
{
    CheckIfAllocated("virtAddressRange");

    if (range == py::none())
    {
        m_Surface->SetVirtAddrRange(0, 0);
    }
    else if (py::isinstance<py::tuple>(range))
    {
        py::tuple addressRange = range.cast<py::tuple>();

        if (addressRange.size() != 2)
        {
            UtlUtility::PyError("a tuple assigned to virtAddressRange must contain two integer values.");
        }

        UINT64 minAddr = addressRange[0].cast<UINT64>();
        UINT64 maxAddr = addressRange[1].cast<UINT64>();

        if (minAddr > maxAddr)
        {
            UtlUtility::PyError("virtAddressRange minimum value is greater than the maximum value.");
        }

        m_Surface->SetVirtAddrRange(minAddr, maxAddr);
    }
    else
    {
        UtlUtility::PyError("virtAddressRange must be set to a tuple containing two integer values.");
    }
}

py::object UtlSurface::GetFixedPhysicalAddress() const
{
    if (m_Surface->HasFixedPhysAddr())
    {
        return py::cast(m_Surface->GetFixedPhysAddr());
    }

    return py::none();
}

void UtlSurface::SetFixedPhysicalAddress(py::object address)
{
    CheckIfAllocated("fixedPhysAddress");

    if (address == py::none())
    {
        m_Surface->SetFixedPhysAddr(0);
    }
    else if (py::isinstance<py::int_>(address))
    {
        m_Surface->SetFixedPhysAddr(address.cast<UINT64>());
    }
    else
    {
        UtlUtility::PyError("fixedPhysAddress must be assigned to an integer value.");
    }
}

py::object UtlSurface::GetPhysicalAddressRange() const
{
    if (m_Surface->HasPhysAddrRange())
    {
        return py::make_tuple(m_Surface->GetPhysAddrRangeMin(),
            m_Surface->GetPhysAddrRangeMax());
    }

    return py::none();
}

void UtlSurface::SetPhysicalAddressRange(py::object range)
{
    CheckIfAllocated("physAddressRange");

    if (range == py::none())
    {
        m_Surface->SetPhysAddrRange(0, 0);
    }
    else if (py::isinstance<py::tuple>(range))
    {
        py::tuple addressRange = range.cast<py::tuple>();

        if (addressRange.size() != 2)
        {
            UtlUtility::PyError("a tuple assigned to physAddressRange must contain two integer values.");
        }

        UINT64 minAddr = addressRange[0].cast<UINT64>();
        UINT64 maxAddr = addressRange[1].cast<UINT64>();

        if (minAddr > maxAddr)
        {
            UtlUtility::PyError("physAddressRange minimum value is greater than the maximum value.");
        }

        m_Surface->SetPhysAddrRange(minAddr, maxAddr);
    }
    else
    {
        UtlUtility::PyError("physAddressRange must be set to a tuple containing two integer values.");
    }
}

void UtlSurface::SetSize(UINT64 size)
{
    if (IsAllocated())
    {
        UtlUtility::PyError("can't set size on a surface after it has been allocated.");
    }
    DoSetSize(size);
}

void UtlSurface::DoSetSize(UINT64 size)
{
    m_Surface->SetArrayPitch(size);
}

void UtlSurface::SetPhysContig(bool physContig)
{
    if (IsAllocated())
    {
        UtlUtility::PyError("can't set physContig on a surface after it has been allocated.");
    }
    m_Surface->SetPhysContig(physContig);
}

void UtlSurface::SetWidth(UINT32 width)
{
    if (IsAllocated())
    {
        UtlUtility::PyError("can't set width on a surface after it has been allocated.");
    }
    m_Surface->SetWidth(width);
}

string UtlSurface::GetColorFormat() const
{
    return ColorUtils::FormatToString(m_Surface->GetColorFormat());
}

void UtlSurface::SetColorFormat(string formatString)
{
    CheckIfAllocated("colorFormat");

    ColorUtils::Format format = ColorUtils::StringToFormat(formatString);

    if (format == ColorUtils::LWFMT_NONE)
    {
        UtlUtility::PyError("%s is not a valid color format",
            formatString.c_str());
    }

    m_Surface->SetColorFormat(format);
}

UtlSurface::AAMode UtlSurface::GetAAMode() const
{
    const Surface2D::AAMode mode = m_Surface->GetAAMode();
    switch (mode)
    {
    case Surface2D::AA_1:
        return AA_1X1;
    case Surface2D::AA_2:
        if (m_Surface->IsD3DSwizzled())
        {
            return AA_2X1_D3D;
        }
        return AA_2X1;
    case Surface2D::AA_4_Rotated:
        return AA_2X2;
    case Surface2D::AA_8:
        if (m_Surface->IsD3DSwizzled())
        {
            return AA_4X2_D3D;
        }
        return AA_4X2;
    case Surface2D::AA_16:
        return AA_4X4;
    case Surface2D::AA_4_Virtual_8:
        return AA_2X2_VC_4;
    case Surface2D::AA_4_Virtual_16:
        return AA_2X2_VC_12;
    case Surface2D::AA_8_Virtual_16:
        return AA_4X2_VC_8;
    case Surface2D::AA_8_Virtual_32:
        return AA_4X2_VC_24;
    default:
        UnsupportedPropValue("AA mode", mode);
        break;
    }
    return AA_1X1;
}

void UtlSurface::SetAAMode(const AAMode propVal)
{
    CheckIfAllocated("AA mode");
    switch(propVal)
    {
    case AA_1X1:
        m_Surface->SetAAMode(Surface2D::AA_1);
        break;
    case AA_2X1:
        m_Surface->SetAAMode(Surface2D::AA_2);
        break;
    case AA_2X1_D3D:
        m_Surface->SetAAMode(Surface2D::AA_2);
        m_Surface->SetD3DSwizzled(true);
        break;
    case AA_2X2:
        m_Surface->SetAAMode(Surface2D::AA_4_Rotated);
        break;
    case AA_4X2:
        m_Surface->SetAAMode(Surface2D::AA_8);
        break;
    case AA_4X4:
        m_Surface->SetAAMode(Surface2D::AA_16);
        break;
    case AA_4X2_D3D:
        m_Surface->SetAAMode(Surface2D::AA_8);
        m_Surface->SetD3DSwizzled(true);
        break;
    case AA_2X2_VC_4:
        m_Surface->SetAAMode(Surface2D::AA_4_Virtual_8);
        break;
    case AA_2X2_VC_12:
        m_Surface->SetAAMode(Surface2D::AA_4_Virtual_16);
        break;
    case AA_4X2_VC_8:
        m_Surface->SetAAMode(Surface2D::AA_8_Virtual_16);
        break;
    case AA_4X2_VC_24:
        m_Surface->SetAAMode(Surface2D::AA_8_Virtual_32);
        break;
    default:
        UnsupportedPropValue("SAMPLE_MODE", propVal);
        break;
    }
}

Surface2D::Layout UtlSurface::GetLayout() const
{
    Surface2D::Layout propVal = m_Surface->GetLayout();
    if (Surface2D::Swizzled == propVal)
    {
        MASSERT(!"SWIZZLED surface layout should no longer be used anywhere");
    }
    else if (!(Surface2D::BlockLinear == propVal || Surface2D::Pitch == propVal))
    {
        UnsupportedPropValue("layout value", propVal);
    }
    return propVal;
}

void UtlSurface::SetLayout(const Surface2D::Layout propVal)
{
    CheckIfAllocated("layout");
    if (Surface2D::Swizzled == propVal)
    {
        MASSERT(!"SWIZZLED surface layout should no longer be used anywhere");
    }
    else if (!(Surface2D::BlockLinear == propVal || Surface2D::Pitch == propVal))
    {
        UnsupportedPropValue("LAYOUT", propVal);
    }
    m_Surface->SetLayout(propVal);
}

UtlSurface::AttrCompr UtlSurface::GetCompression() const
{
    if (!m_Surface->GetCompressed())
    {
        return ComprNone;
    }
    const UINT32 flag = m_Surface->GetCompressedFlag();
    if (!(ComprRequired == flag ||
        ComprAny == flag))
    {
        UnsupportedPropValue("compression flag", flag);
        return ComprNone;
    }
    return static_cast<AttrCompr>(flag);
}

void UtlSurface::SetCompression(const AttrCompr propVal)
{
    CheckIfAllocated("compression");
    if (ComprNone == propVal)
    {
        m_Surface->SetCompressed(false);
    }
    else
    {
        m_Surface->SetCompressed(true);
    }
    m_Surface->SetCompressedFlag(propVal);
}

UtlSurface::AttrZLwll UtlSurface::GetZLwll() const
{
    if (Platform::GetSimulationMode() != Platform::Amodel)
    {
        const UINT32 flag = m_Surface->GetZLwllFlag();
        switch (flag)
        {
        case ZLwllNone:
        case ZLwllAny:
        case ZLwllRequired:
            return static_cast<AttrZLwll>(flag);
        default:
            UnsupportedPropValue("ZLWLL flag", flag);
            break;
        }
    }
    else
    {
        UtlUtility::PyError("Surface ZLWLL not supported in Amodel.");
    }
    return ZLwllNone;
}

void UtlSurface::SetZLwll(const AttrZLwll propVal)
{
    if (Platform::GetSimulationMode() != Platform::Amodel)
    {
        CheckIfAllocated("zlwll");
        m_Surface->SetZLwllFlag(propVal);
    }
    else
    {
        UtlUtility::PyError("Surface ZLWLL not supported in Amodel.");
    }
}

UtlSurface::AttrType UtlSurface::GetType() const
{
    const UINT32 type = m_Surface->GetType();
    switch (type)
    {
    case TypeImage:
    case TypeDepth:
    case TypeTexture:
    case TypeVideo:
        return static_cast<AttrType>(type);
    default:
        UnsupportedPropValue("type", type);
        break;
    }
    return TypeImage;
}

void UtlSurface::SetType(const AttrType propVal)
{
    CheckIfAllocated("type");
    m_Surface->SetType(propVal);
}

Memory::Protect UtlSurface::GetShaderProtect() const
{
    const Memory::Protect propVal = m_Surface->GetShaderProtect();
    switch (propVal)
    {
    case Memory::Readable:
    case Memory::Writeable:
    case Memory::ReadWrite:
    case Memory::ProtectDefault:
        return propVal;
    default:
        UnsupportedPropValue("shader protect", propVal);
        break;
    }
    return Memory::ReadWrite;
}

void UtlSurface::SetShaderProtect(const Memory::Protect propVal)
{
    CheckIfAllocated("shader protect");
    switch (propVal)
    {
    case Memory::Readable:
    case Memory::Writeable:
    case Memory::ReadWrite:
        m_Surface->SetShaderProtect(propVal);
        break;
    default:
        UnsupportedPropValue("SHADER_ACCESS", propVal);
    }
}

void UtlSurface::SetPriv(bool propVal)
{
    CheckIfAllocated("privileged");
    m_Surface->SetPriv(propVal);
}

UtlSurface::Toggle UtlSurface::GetGpuCacheMode() const
{
    return ToggleType<Surface2D::GpuCacheMode>::GetToggle(m_Surface->GetGpuCacheMode());
}

void UtlSurface::SetGpuCacheMode(const Toggle propVal)
{
    CheckIfAllocated("GPU cache mode");
    Surface2D::GpuCacheMode mode =
        ToggleType<Surface2D::GpuCacheMode>::GetMode(propVal);
    if (Surface2D::GpuCacheDefault == mode)
    {
        UnsupportedPropValue("GPU_CACHE", propVal);
    }
    m_Surface->SetGpuCacheMode(mode);
}

UtlSurface::Toggle UtlSurface::GetP2PGpuCacheMode() const
{
    return ToggleType<Surface2D::GpuCacheMode>::GetToggle(m_Surface->GetP2PGpuCacheMode());
}

void UtlSurface::SetP2PGpuCacheMode(const Toggle propVal)
{
    CheckIfAllocated("P2P GPU cache mode");
    Surface2D::GpuCacheMode mode =
        ToggleType<Surface2D::GpuCacheMode>::GetMode(propVal);
    if (Surface2D::GpuCacheDefault == mode)
    {
        UnsupportedPropValue("P2P_GPU_CACHE", propVal);
    }
    m_Surface->SetP2PGpuCacheMode(mode);
}

UtlSurface::Toggle UtlSurface::GetZbcMode() const
{
    return ToggleType<Surface2D::ZbcMode>::GetToggle(m_Surface->GetZbcMode());
}

void UtlSurface::SetZbcMode(const Toggle propVal)
{
    CheckIfAllocated("ZBC mode");
    Surface2D::ZbcMode mode =
        ToggleType<Surface2D::ZbcMode>::GetMode(propVal);
    if (Surface2D::ZbcDefault == mode)
    {
        UnsupportedPropValue("ZBC", propVal);
    }
    m_Surface->SetZbcMode(mode);
}

void UtlSurface::SetDepth(UINT32 propVal)
{
    CheckIfAllocated("depth");
    m_Surface->SetDepth(propVal);
}

void UtlSurface::SetPitch(UINT32 propVal)
{
    CheckIfAllocated("pitch");
    m_Surface->SetPitch(propVal);
}

void UtlSurface::SetLogBlockWidth(UINT32 propVal)
{
    CheckIfAllocated("block width");
    m_Surface->SetLogBlockWidth(propVal);
}

void UtlSurface::SetLogBlockHeight(UINT32 propVal)
{
    CheckIfAllocated("block height");
    m_Surface->SetLogBlockHeight(propVal);
}

void UtlSurface::SetLogBlockDepth(UINT32 propVal)
{
    CheckIfAllocated("block depth");
    m_Surface->SetLogBlockDepth(propVal);
}

void UtlSurface::SetArraySize(UINT32 propVal)
{
    CheckIfAllocated("array size");
    m_Surface->SetArraySize(propVal);
}

void UtlSurface::SetMipLevels(UINT32 propVal)
{
    CheckIfAllocated("miplevels");
    m_Surface->SetMipLevels(propVal);
}

void UtlSurface::SetBorder(UINT32 propVal)
{
    CheckIfAllocated("border");
    m_Surface->SetBorder(propVal);
}

void UtlSurface::SetLwbemap(bool propVal)
{
    CheckIfAllocated("lwbemap");
    if (propVal)
    {
        m_Surface->SetDimensions(6);
    }
    else
    {
        m_Surface->SetDimensions(1);
    }
}

void UtlSurface::SetAlignment(UINT64 propVal)
{
    CheckIfAllocated("physical alignment");
    m_Surface->SetAlignment(propVal);
}

void UtlSurface::SetVirtAlignment(UINT64 propVal)
{
    CheckIfAllocated("virtual alignment");
    m_Surface->SetVirtAlignment(propVal);
}

UINT32 UtlSurface::GetAttrOverride() const
{
    UtlUtility::PyError("Surface get attr override not implemented.");
    return 0;
}

void UtlSurface::ConfigFromAttr(UINT32 propVal)
{
    CheckIfAllocated("attr override");
    m_Surface->ConfigFromAttr(propVal);
}

UINT32 UtlSurface::GetAttr2Override() const
{
    UtlUtility::PyError("Surface get attr2 override not implemented.");
    return 0;
}

void UtlSurface::ConfigFromAttr2(UINT32 propVal)
{
    CheckIfAllocated("attr2 override");
    m_Surface->ConfigFromAttr2(propVal);
}

void UtlSurface::SetSkedReflected(bool propVal)
{
    CheckIfAllocated("sked-reflected");
    m_Surface->SetSkedReflected(propVal);
}

void UtlSurface::SetMemoryMappingMode(const Surface2D::MemoryMappingMode propVal)
{
    CheckIfAllocated("map mode");
    switch (propVal)
    {
    case Surface2D::MapDirect:
    case Surface2D::MapReflected:
        m_Surface->SetMemoryMappingMode(propVal);
        break;
    default:
        UnsupportedPropValue("MAP_MODE", propVal);
        break;
    }
}

void UtlSurface::SetTileWidthInGobs(UINT32 propVal)
{
    CheckIfAllocated("tile width in GOBs");
    m_Surface->SetTileWidthInGobs(propVal);
}

void UtlSurface::SetAtsMapped(bool propVal)
{
    CheckIfAllocated("ATS-mapped");
    if (propVal)
    {
        m_Surface->SetAtsMapped();
    }
    // else do nothing to ignore
}

bool UtlSurface::GetNoGmmuMap() const
{
    UtlUtility::PyError("Surface get no-GMMU-map not implemented.");
    return false;
}

void UtlSurface::SetNoGmmuMap(bool propVal)
{
    CheckIfAllocated("no-GMMU-map");
    if (propVal)
    {
        m_Surface->SetNoGmmuMap();
    }
    // else do nothing to ignore
}

void UtlSurface::SetAtsPageSize(UINT32 propVal)
{
    CheckIfAllocated("ATS page size");
    m_Surface->SetAtsPageSize(propVal);
}

void UtlSurface::SetIsHtex(bool propVal)
{
    CheckIfAllocated("is-HTex");
    m_Surface->GetSurf2D()->SetIsHtex(propVal);
}

void UtlSurface::SetSpecialization(py::kwargs kwargs)
{
    CheckIfAllocated("Specialization");
    Surface2D::Specialization spec =
        UtlUtility::GetRequiredKwarg<Surface2D::Specialization>(kwargs,
        "spec", "Surface.SetSpecialization");

    UtlGil::Release gil;

    m_Surface->SetSpecialization(spec);
}

void UtlSurface::SetIsSurfaceView()
{
    CheckIfAllocated("SurfaceView");
    m_Surface->SetIsSurfaceView();
}

LwRm* UtlSurface::GetLwRm() const
{
    return m_LwRm != nullptr ? m_LwRm : m_Surface->GetLwRmPtr();
}

LwRm::Handle UtlSurface::GetMemHandle() const
{
    return m_Surface->GetMemHandle(GetLwRm());
}

UtlGpuVerif * UtlSurface::GetGpuVerif(UtlChannel * pChannel)
{
    UtlTest * pTest = GetTest();

    // If the user didn't pass a channel to the relevant CRC function,
    // use the channel bound to the surface.
    if (pChannel == nullptr)
    {
        pChannel = m_pChannel;
    }

    // If there's no test associated with this surface, try to get one
    // from the channel.
    if ((pTest == nullptr) && (pChannel != nullptr))
    {
        for (auto test : UtlTest::GetTests())
        {
            auto channelIter = find(test->GetChannels().begin(),
                test->GetChannels().end(), pChannel);

            if (channelIter != test->GetChannels().end())
            {
                pTest = test;
                break;
            }
        }

        if (pTest == nullptr)
        {
            UtlUtility::PyError(
                "surface channel must belong to a utl.Test object");
        }
    }

    // If no channel was specified or bound to the surface, get one from
    // the test that matches the surface's VA space.
    else if ((pChannel == nullptr) && (pTest != nullptr))
    {
        if (GetChannel())
        {
            pChannel = GetChannel();
        }
        else
        {
            for (auto channel : pTest->GetChannels())
            {
                if (channel->GetVaSpaceHandle() == GetVaSpaceHandle())
                {
                    pChannel = channel;
                    DebugPrintf("User don't specify channel, UTL choose the default"
                            " vaspace channel as given surface.\n");
                    break;
                }
            }
        }

        if (pChannel == nullptr)
        {
            UtlUtility::PyError(
                    "Missing avaliable channel.");
        }
    }
    else if ((pChannel == nullptr) && (pTest == nullptr))
    {
        UtlUtility::PyError(
            "this function requires either a channel argument, or that surface %s is bound to a channel",
            GetName().c_str());
    }

    if (m_pGpuVerif == nullptr)
        m_pGpuVerif = UtlGpuVerif::GetGpuVerif(pTest->GetRawPtr(),
                GetVaSpace(), pChannel->GetLwGpuChannel());

    return m_pGpuVerif;
}

string UtlSurface::GetCrcKey(py::kwargs kwargs)
{
    UtlChannel * pChannel = nullptr;
    UtlUtility::GetOptionalKwarg<UtlChannel *>(&pChannel, kwargs, "channel",
        "Surface.GetCrcKey");

    UtlGpuVerif * pGpuVerif = GetGpuVerif(pChannel);
    if (pGpuVerif == nullptr)
    {
        MASSERT(!"Get GpuVerif failure.");
    }

    return pGpuVerif->GetCrcKey(m_Surface);
}

UINT32 UtlSurface::CallwlateCrc(py::kwargs kwargs)
{
    UINT32 preCrcValue = 0;
    UtlUtility::GetOptionalKwarg<UINT32>(
            &preCrcValue, kwargs, "preCrcValue", "Surface.CallwlateCrc");

    UtlChannel * pChannel = nullptr;
    UtlUtility::GetOptionalKwarg<UtlChannel *>(&pChannel, kwargs, "channel",
        "Surface.CallwlateCrc");

    UtlGpuVerif * pGpuVerif = GetGpuVerif(pChannel);
    if (pGpuVerif == nullptr)
    {
        MASSERT(!"Get GpuVerif failure.");
    }

    // Don't support FIlE surface
    UtlGil::Release gil;
    UINT32 crcValue = pGpuVerif->CallwlateCrc(
            m_Surface, preCrcValue);
    return crcValue;
}

void UtlSurface::ReadRawData
(
    UINT64 offset,
    UINT64 size,
    void * data
)
{
    RC rc = Map();
    UtlUtility::PyErrorRC(rc, "Surface.ReadData map failed");
    Platform::VirtualRd((UINT08 *)m_Surface->GetAddress() + offset, data,
        size);
    Unmap();
}

void UtlSurface::ReadPitchData
(
    UINT64 offset,
    UINT64 size,
    void * data,
    UtlChannel * pChannel
)
{
    vector<UINT08> wholeData;

    UtlGpuVerif * pGpuVerif = GetGpuVerif(pChannel);
    if (pGpuVerif == nullptr)
    {
        MASSERT(!"Get GpuVerif failure.");
    }

    pGpuVerif->ReadSurface(&wholeData, m_Surface);

    memcpy(data, &wholeData[0] + offset, size);
}

void UtlSurface::DumpImageToFile(py::kwargs kwargs)
{
    const string& fileName = UtlUtility::GetRequiredKwarg<string>(kwargs, "fileName",
        "Surface.DumpImageToFile");
    UtlChannel * pChannel = nullptr;
    UtlUtility::GetOptionalKwarg<UtlChannel *>(&pChannel, kwargs, "channel",
        "Surface.DumpImageToFile");

    UtlGpuVerif * pGpuVerif = GetGpuVerif(pChannel);
    if (pGpuVerif == nullptr)
    {
        MASSERT(!"Get GpuVerif failure.");
    }

    RC rc = pGpuVerif->DumpImageToFile(m_Surface, fileName);
    UtlUtility::PyErrorRC(rc, "surface dump image to file failed");
}

RC UtlSurface::Map()
{
    RC rc;
    m_CpuMapCount++;

    if (!m_Surface->IsMapped())
    {
        rc = m_Surface->Map();
    }

    return rc;
}

void UtlSurface::MapPy()
{
    UtlGil::Release gil;

    RC rc = Map();
    UtlUtility::PyErrorRC(rc, "Surface.Map failed");
}

void UtlSurface::Unmap()
{
    // Only unmap when there are a matching number of Map() calls and
    // the surface was not created with keepCpuMapping = True
    m_CpuMapCount = std::max(0, m_CpuMapCount-1);
    if (!m_KeepCpuMapping && m_CpuMapCount == 0 && m_Surface->IsMapped())
    {
        m_Surface->Unmap();
    }
}

void UtlSurface::UnmapPy()
{
    UtlGil::Release gil;

    Unmap();
}

void UtlSurface::MapVirtToPhys(py::kwargs kwargs)
{
    UtlSurface* pVirt =
        UtlUtility::GetRequiredKwarg<UtlSurface*>(kwargs,
        "virtAlloc", "Surface.MapVirtToPhys");
    UtlSurface* pPhys =
        UtlUtility::GetRequiredKwarg<UtlSurface*>(kwargs,
        "physAlloc", "Surface.MapVirtToPhys");
    UINT64 virtOff =
        UtlUtility::GetRequiredKwarg<UINT64>(kwargs,
        "virtOffset", "Surface.MapVirtToPhys");
    UINT64 physOff =
        UtlUtility::GetRequiredKwarg<UINT64>(kwargs,
        "physOffset", "Surface.MapVirtToPhys");

    UtlGpu* pGpu = nullptr;
    if (!UtlUtility::GetOptionalKwarg<UtlGpu*>(&pGpu, kwargs, "gpu", "Surface.MapVirtToPhys"))
    {
        // If the user doesn't specify a GPU, use GPU 0 by default.
        pGpu = UtlGpu::GetGpus()[0];
    }

    UtlGil::Release gil;

    RC rc;
    rc = m_Surface->MapVirtToPhys(pGpu->GetGpudevice(),
        pVirt->m_Surface,
        pPhys->m_Surface,
        virtOff,
        physOff,
        GetLwRm());
    UtlUtility::PyErrorRC(rc, "Surface.MapVirtToPhys failed to map");
}

void UtlSurface::SetSurfaceViewParent(py::kwargs kwargs)
{
    UtlSurface* pParent = UtlUtility::GetRequiredKwarg<UtlSurface*>(kwargs, "parent",
        "Surface.SetSurfaceViewParent");
    MASSERT(pParent);
    MdiagSurf* pSurf = pParent->m_Surface;
    MASSERT(pSurf && pSurf->IsAllocated());
    UINT64 addrOff = UtlUtility::GetRequiredKwarg<UINT64>(kwargs, "addressOffset",
        "Surface.SetSurfaceViewParent");

    UtlGil::Release gil;

    if (!m_Surface->IsSurfaceView())
    {
        UtlUtility::PyError("Surface %s is not a surface view, can't set its parent surface.",
            GetName().c_str());
    }
    m_Surface->SetSurfaceViewParent(pSurf, addrOff);
}

RC UtlSurface::ExploreMmuLevelTree()
{
    if (m_pMmuLevelTree)
    {
        // has been explored
        // PTE/PDE will not be freed/allocated
        // So mmu level info should not change
        // Note: PTE/PDE content may be modified
        return OK;
    }

    RC rc;
    GpuDevice *pGpuDevice = m_Surface->GetGpuDev();
    LwRm::Handle hVASpace = m_Surface->GetVASpaceHandle(Surface2D::DefaultVASpace);
    UINT64 virtAddress = m_Surface->GetCtxDmaOffsetGpu();

    MmuLevelTreeManager * pMmuLevelMgr = MmuLevelTreeManager::Instance();
    m_pMmuLevelTree = pMmuLevelMgr->GetMmuLevelTree(pGpuDevice, m_Surface, virtAddress, hVASpace, true);
    CHECK_RC(m_pMmuLevelTree->ExploreMmuLevels());

    return rc;
}

RC UtlSurface::GetMmuLevel
(
    MmuLevelTree::LevelIndex index,
    MmuLevel** ppMmuLevel,
    MmuLevelTree * pMmuLevelTree
)
{
    RC rc;

    MASSERT(pMmuLevelTree);

    CHECK_RC(pMmuLevelTree->GetMmuLevel(index, ppMmuLevel));
    return rc;
}

UtlMmuSegment * UtlSurface::GetUtlMmuSegment
(
    MmuLevelSegment * pSegment,
    UtlMmu::SUB_LEVEL_INDEX subLevelIdx = SUB_LEVEL_INDEX::DEFAULT
)
{
    if (m_UtlMmuSegments.find(pSegment) == m_UtlMmuSegments.end())
    {
        m_UtlMmuSegments[pSegment] = unique_ptr<UtlMmuSegment>(new UtlMmuSegment(this,
                                                         pSegment, subLevelIdx));
    }

    return m_UtlMmuSegments[pSegment].get();
}

py::list UtlSurface::GetEntryListsPy
(
    py::kwargs kwargs
)
{
    // input data
    UINT64 offset = UtlUtility::GetRequiredKwarg<UINT64>(kwargs, "offset",
                                "Surface.GetEntryLists");
    UINT32 size  = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "size",
                                "Surface.GetEntryLists");
    UtlMmu::LEVEL level = UtlUtility::GetRequiredKwarg<UtlMmu::LEVEL>(kwargs, "level",
                                            "Surface.GetEntryLists");
    UtlMmu::SUB_LEVEL_INDEX subLevelIdx = SUB_LEVEL_INDEX::DEFAULT;
    if (level == UtlMmu::LEVEL_PDE0)
    {
        if (!UtlUtility::GetOptionalKwarg<SUB_LEVEL_INDEX>(&subLevelIdx, kwargs, "subLevelIndex",
                                            "Surface.GetEntryLists"))
        {
            UtlUtility::PyError("For Level == PDE0, user need to specify the subLevel index.");
        }
    }

    UtlGil::Release gil;

    if (!m_Surface->HasMap())
    {
        UtlUtility::PyError("Fail to query entry list since surface is not gmmu mapped.");
    }

    if (ExploreMmuLevelTree() != OK)
    {
        UtlUtility::PyError("Explore MMU level tree error.");
    }

    vector<UtlMmuEntry*> entryList =
        GetEntryLists(level, subLevelIdx, offset, size);

    return UtlUtility::ColwertPtrVectorToPyList(entryList);
}

vector<UtlMmuEntry *> UtlSurface::GetEntryLists
(
    UtlMmu::LEVEL level,
    UtlMmu::SUB_LEVEL_INDEX subLevelIdx,
    UINT64 offset,
    UINT32 size
)
{
    MmuLevel* pMmuLevel = NULL;
    vector<UtlMmuEntry * > entryLists;

    if (OK != GetMmuLevel((MmuLevelTree::LevelIndex)level, &pMmuLevel,
                m_pMmuLevelTree))
    {
        UtlUtility::PyError("Failed to get MMU level.");
    }

#if LWCFG(GLOBAL_ARCH_HOPPER)
    if (pMmuLevel->GetGmmuFmt()->version != GMMU_FMT_VERSION_3)
    {
        UtlUtility::PyError("UTL only support v3 mmu modification. Please check your test.");
    }
#endif

    for (UINT64 batchOffset = offset; batchOffset < offset + size; /*nop*/)
    {
        MmuLevelSegment *pSegment = NULL;
        if (!pMmuLevel ||
                (pMmuLevel->GetMmuLevelSegment(batchOffset, &pSegment) != OK))
        {
            UtlUtility::PyError("Can't get the MMU level segment successfully.");
        }

        UINT64 batchSize = offset + size - batchOffset;

        UtlMmuSegment * pUtlMmuSegment = GetUtlMmuSegment(pSegment, subLevelIdx);
        vector<UtlMmuEntry * > partEntryLists = pUtlMmuSegment->GetEntryLists(offset, &batchSize);
        entryLists.insert(entryLists.end(), partEntryLists.begin(), partEntryLists.end());

        // update the batchOffset according to the real batchSize in current segment
        batchOffset += batchSize;
    }

    // ownership in Mods side
    return entryLists;
}


LwRm::Handle UtlSurface::GetVASpaceHandle() const
{
    UINT32 vaSpaceClass = 0;  // default
    Surface2D::VASpace vaSpace = m_Surface->GetVASpace();
    if (vaSpace == Surface2D::GPUVASpace)
    {
        vaSpaceClass = FERMI_VASPACE_A;
    }
    switch (vaSpaceClass)
    {
        case 0:                          // default va space
        case FERMI_VASPACE_A:            // gmmu va space
            return 0;
        default:
            MASSERT(!"Invalid memory mapping type");
    } 
 
    return 0;
}

void UtlSurface::SetVaSpacePy(py::kwargs kwargs)
{
    LwRm::Handle hVaSpace = 0;
    UtlVaSpace * vaSpace = UtlUtility::GetRequiredKwarg<UtlVaSpace*>(kwargs, 
        "vaSpace", "Surface.SetVaSpace");
    if (vaSpace != nullptr)
    {
        hVaSpace = vaSpace->GetHandle();
    }
    else
    {
        //TODO: Support VA Space created by core MODS.
        UtlUtility::PyError("Didn't find VA Space handle, auto get VA space is not support");
    }

    m_Surface->SetGpuVASpace(hVaSpace);
}

UtlVaSpace * UtlSurface::GetVaSpacePy()
{
    return UtlVaSpace::GetVaSpace(GetVaSpaceHandle(), m_pTest);
}

UtlGpu* UtlSurface::GetGpu() const
{
    UtlGpu* gpu = nullptr;
    if (IsAllocated())
    {
        MASSERT(m_Surface->GetGpuDev()->GetNumSubdevices() == 1);
        gpu = UtlGpu::GetFromGpuSubdevicePtr(m_Surface->GetGpuSubdev(0));
    }

    return gpu;
}

void UtlSurface::SetLoopback(bool loopback)
{
    CheckIfAllocated("loopback");
    m_Surface->SetLoopBack(loopback);
}

UINT64 UtlSurface::CreatePeerMappingPy(py::kwargs kwargs)
{
    if (!IsAllocated())
    {
        UtlUtility::PyError("can't call CreatePeerMapping on a surface that has not yet been allocated.");
    }

    UtlGpu* accessingGpu = UtlUtility::GetRequiredKwarg<UtlGpu*>(kwargs,
        "accessingGpu", "Surface.CreatePeerMapping");

    if (accessingGpu == nullptr)
    {
        UtlUtility::PyError("invalid accessingGpu passed to CreatePeerMapping.");
    }

    UtlGil::Release gil;

    RC rc;

    // MapPeer will assert if both the surface's GPU and the accessing GPU are
    // the same, so use MapLoopback instead for that case.
    if (accessingGpu->GetGpudevice() == m_Surface->GetGpuDev())
    {
        rc = m_Surface->MapLoopback();
    }
    else
    {
        rc = m_Surface->MapPeer(accessingGpu->GetGpudevice());
    }

    if (OK != rc)
    {
        UtlUtility::PyError("CreatePeerMapping failed to create a peer mapping.");
    }

    return GetGmmuPeerVirtualAddress(accessingGpu);
}

py::object UtlSurface::GetPteKind(py::kwargs kwargs) const
{

    PteKindDataType dataType = PteKindDataType::Str;
    UtlUtility::GetOptionalKwarg<PteKindDataType>(&dataType, kwargs, "dataType",
        "Surface.GetPteKind");

    UtlGpu* gpu = nullptr;
    if (UtlUtility::GetOptionalKwarg<UtlGpu*>(&gpu, kwargs, "gpu",
        "Surface.GetPteKind"))
    {
        if (IsAllocated() && (gpu != GetGpu()))
        {
            UtlUtility::PyError("gpu parameter (device %d) does not match gpu in which surface is allocated (device %d)",
                gpu->GetGpudevice()->GetDeviceInst(), m_Surface->GetGpuDev()->GetDeviceInst());

        }
    }
    else
    {
        if (IsAllocated())
        {
            gpu = GetGpu();
        }
        else
        {
            gpu = UtlGpu::GetGpus()[0];
        }
        MASSERT(gpu != nullptr);
    }

    const UINT32 kindNum = m_Surface->GetPteKind();
    py::object kind;

    if (dataType == PteKindDataType::Str)
    {
        const char* kindStr = gpu->GetGpuSubdevice()->GetPteKindName(kindNum);
        kind = py::cast(kindStr);
    }
    else if (dataType == PteKindDataType::Num)
    {
        kind = py::cast(kindNum);
    }
    else
    {
        UtlUtility::PyError("Parameter '%d' is not a recognized PTE kind dataType.",
            dataType);
    }

    return kind;
}

void UtlSurface::SetPteKind(py::kwargs kwargs)
{
    if (IsAllocated())
    {
        UtlUtility::PyError("can't call SetPteKind on a surface after it has been allocated.");
    }

    UtlGpu* gpu = nullptr;
    if (!UtlUtility::GetOptionalKwarg<UtlGpu*>(&gpu, kwargs, "gpu",
        "Surface.SetPteKind"))
    {
        // If the user doesn't specify a GPU, use GPU 0 by default.
        gpu = UtlGpu::GetGpus()[0];
    }

    string kindStr;
    bool hasKindStr = UtlUtility::GetOptionalKwarg<string>(&kindStr, kwargs,
        "kindStr", "Surface.SetPteKind");
    UINT32 kindNum = 0x0;
    bool hasKindNum = UtlUtility::GetOptionalKwarg<UINT32>(&kindNum, kwargs,
        "kindNum", "Surface.SetPteKind");

    if (!hasKindStr && !hasKindNum)
    {
        UtlUtility::PyError("SetPteKind requires one of the following parameters: kindStr or kindNum");
    }
    else if (hasKindStr && hasKindNum)
    {
        UtlUtility::PyError("only one of the parameters kindStr or kindNum may be passed to SetPteKind.");
    }
    else if (hasKindStr)
    {
        if (!gpu->GetGpuSubdevice()->GetPteKindFromName(kindStr.c_str(), &kindNum))
        {
            UtlUtility::PyError("kind parameter '%s' is not a recognized PTE kind string.",
                kindStr.c_str());
        }

    }
    m_Surface->SetPteKind(kindNum);
}

void UtlSurface::SetFlaImportPy(py::kwargs kwargs)
{
    UtlSurface* flaSurf = UtlUtility::GetRequiredKwarg<UtlSurface*>(kwargs,
        "flaSurface", "Surface.SetFlaImport");

    MdiagSurf* flaExportSurface = flaSurf->GetRawPtr();
    MASSERT(flaExportSurface != nullptr);

    if ((m_Surface->SetFlaImportMem(flaExportSurface->GetCtxDmaOffsetGpu(),
            flaExportSurface->GetSize(), flaExportSurface)) != OK)
    {
        UtlUtility::PyError("SetFlaImport failed to SetFlaImportMem.");
    }
}

void UtlSurface::FreeMmuLevels()
{
    m_UtlMmuSegments.clear();
    m_pMmuLevelTree = nullptr;
}
