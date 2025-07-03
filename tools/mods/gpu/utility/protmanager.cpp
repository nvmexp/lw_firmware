/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/protmanager.h"
#include "core/include/rc.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "core/include/js_uint64.h"
#include "gpu/include/gpudev.h"
#include "gpu/js_gpu.h"
#include "gpu/utility/sec2rtossub.h"
#include "cheetah/include/devid.h"
#include "t12x/t124/dev_armc.h"
#include "class/cle3f1.h" // TEGRA_VASPACE_A
#include "ctrl/ctrl0080/ctrl0080fb.h"

//--------------------------------------------------------------------------
//! \brief Constructor
//!
ProtectedRegionManager::ProtectedRegionManager
(
    GpuDevice *gpuDevice
)
    :m_GpuDevice(gpuDevice)
{
}

//--------------------------------------------------------------------------
//! \brief Destructor
//!
ProtectedRegionManager::~ProtectedRegionManager()
{
    FreeRegions();
}

//------------------------------------------------------------------------------
//! \brief Allocate all of the regions
//!
RC ProtectedRegionManager::AllocRegions
(
    UINT64 vprSize,
    UINT64 vprAlignment,
    UINT64 acr1Size,
    UINT64 acr1Alignment,
    UINT64 acr2Size,
    UINT64 acr2Alignment
)
{
    RC rc;
    JavaScriptPtr pJs;

    CHECK_RC(m_GpuDevice->AllocDevices());

    m_VprRegion.Name = "VPR";
    m_VprRegion.Size = vprSize;
    m_VprRegion.Alignment = vprAlignment;

    PHYSADDR dummyAddr;
    UINT64 dummySize;
    RC tempRC = Platform::GetVPR(&dummyAddr, &dummySize);

    // If GetVPR is unsupported, that means the current platform
    // doesn't support MODS creating a VPR region (e.g. silicon).
    if (RC::UNSUPPORTED_FUNCTION == tempRC)
    {
        m_VprRegion.ReadExisting = true;
    }

    if (!m_VprRegion.ReadExisting && (vprSize > 0))
    {
        UINT32 vprStartAddrMb = 0xffffffff;
        CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_VprStartAddrMb,
            &vprStartAddrMb));
        if (vprStartAddrMb != 0xffffffff)
        {
            m_VprRegion.BasePhysAddr =
                static_cast<PHYSADDR>(vprStartAddrMb) << 20;
            m_VprRegion.FixedAddr = true;
        }
        else
        {
            LwRmPtr pLwRm;
            LW0080_CTRL_FB_GET_COMPBIT_STORE_INFO_PARAMS params = {};

            CHECK_RC(pLwRm->Control(
                    pLwRm->GetDeviceHandle(m_GpuDevice),
                    LW0080_CTRL_CMD_FB_GET_COMPBIT_STORE_INFO,
                    &params, sizeof(params)));

            // For Turing and later devices, there is a HW register bit
            // (LW_PFB_PRI_MMU_CBC_MAX_SAFE) that will be set to FALSE if the
            // VPR region overlaps the maximum possible CBC backing store
            // region.  RM will assert if it detects this condition.
            // For such devices, RM will set the privRegionStartOffset parameter
            // of the previous control call to an address that is safe to
            // start the VPR region so that it won't overlap (see bug 2773583).
            if (params.privRegionStartOffset != 0)
            {
                m_VprRegion.BasePhysAddr = ALIGN_UP(
                    params.privRegionStartOffset,
                    m_VprRegion.Alignment);
                m_VprRegion.FixedAddr = true;
            }
        }

        CHECK_RC_MSG(AllocRegion(&m_VprRegion),
            "Error: failed to allocate VPR region.\n");
    }

    m_Acr1Region.Name = "ACR1";
    m_Acr1Region.Size = acr1Size;
    m_Acr1Region.Alignment = acr1Alignment;

    JSContext *pContext;
    CHECK_RC(pJs->GetContext(&pContext));
    JSObject* pAcr1StartAddr = nullptr;
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_Acr1StartAddr, &pAcr1StartAddr));
    JSClass* pJsClass = JS_GetClass(pAcr1StartAddr);

    // If the property is of type UINT64, then the user specified a fixed
    // starting address for the ACR1 region.
    if ((pJsClass->flags & JSCLASS_HAS_PRIVATE) &&
        strcmp(pJsClass->name, "UINT64") == 0)
    {
        JsUINT64* pJsUINT64 = static_cast<JsUINT64*>(
            JS_GetPrivate(pContext, pAcr1StartAddr));
        m_Acr1Region.BasePhysAddr = pJsUINT64->GetValue();

        if (m_Acr1Region.BasePhysAddr % m_Acr1Region.Alignment != 0)
        {
            Printf(Tee::PriError,
                "Error: -acr1_start_addr value 0x%llx is not aligned to 0x%llx.\n",
                m_Acr1Region.BasePhysAddr,
                m_Acr1Region.Alignment);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        m_Acr1Region.FixedAddr = true;
    }

    CHECK_RC_MSG(AllocRegion(&m_Acr1Region),
        "Error: failed to allocate ACR1 region.\n");

    m_Acr2Region.Name = "ACR2";
    m_Acr2Region.Size = acr2Size;
    m_Acr2Region.Alignment = acr2Alignment;

    JSObject* pAcr2StartAddr = nullptr;
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_Acr2StartAddr, &pAcr2StartAddr));
    pJsClass = JS_GetClass(pAcr2StartAddr);

    // If the property is of type UINT64, then the user specified a fixed
    // starting address for the ACR2 region.
    if ((pJsClass->flags & JSCLASS_HAS_PRIVATE) &&
        strcmp(pJsClass->name, "UINT64") == 0)
    {
        JsUINT64* pJsUINT64 = static_cast<JsUINT64*>(
            JS_GetPrivate(pContext, pAcr2StartAddr));
        m_Acr2Region.BasePhysAddr = pJsUINT64->GetValue();

        if (m_Acr2Region.BasePhysAddr % m_Acr2Region.Alignment != 0)
        {
            Printf(Tee::PriError,
                "Error: -acr2_start_addr value 0x%llx is not aligned to 0x%llx.\n",
                m_Acr2Region.BasePhysAddr,
                m_Acr2Region.Alignment);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        m_Acr2Region.FixedAddr = true;
    }

    CHECK_RC_MSG(AllocRegion(&m_Acr2Region),
        "Error: failed to allocate ACR2 region.\n");

    CHECK_RC(AllolwprRegions());

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Allocate the UPR regions.
//!
RC ProtectedRegionManager::AllolwprRegions()
{
    RC rc;
    JavaScriptPtr pJs;
    JSContext *pContext;
    JSClass* pJsClass;

    CHECK_RC(pJs->GetContext(&pContext));
    JSObject* pUprObj = nullptr;
    CHECK_RC(pJs->GetProperty(
            Gpu_Object,
            Gpu_UprStartAddrList,
            &pUprObj));
    pJsClass = JS_GetClass(pUprObj);

    // Only create UPR regions if there were command-line arguments specifying
    // that they should be created.  If no arguments were specified, then the
    // Gpu_UprStartAddrList object would still be set to zero and the class
    // would be "Number".
    if (strcmp(pJsClass->name, "Array") == 0)
    {
        vector<PHYSADDR> uprStartAddrList;
        CHECK_RC(pJs->GetProperty(
            Gpu_Object,
            Gpu_UprStartAddrList,
            &uprStartAddrList));

        vector<UINT64> uprSizeList;
        CHECK_RC(pJs->GetProperty(
            Gpu_Object,
            Gpu_UprSizeList,
            &uprSizeList));

        MASSERT(uprStartAddrList.size() == uprSizeList.size());

        // Unprotected memory regions are aligned with VMMU segments.
        LwRmPtr pLwRm;
        LW2080_CTRL_GPU_GET_VMMU_SEGMENT_SIZE_PARAMS params = {};
        CHECK_RC(pLwRm->ControlBySubdevice(
            m_GpuDevice->GetSubdevice(0),
            LW2080_CTRL_CMD_GPU_GET_VMMU_SEGMENT_SIZE,
            &params,
            sizeof(params)));
        UINT64 alignment = params.vmmuSegmentSize;

        // It is possible for the VMMU segment size control call to return zero,
        // even if the actual VMMU segment size is non-zero (for example,
        // if the request was made from a VF in SRIOV mode).  In that case,
        // align to the largest VMMU segment size possible so that the
        // UPR regions are guaranteed to work with any of the possible
        // VMMU segment size values.
        if (alignment == 0)
        {
            RegHal& regs = m_GpuDevice->GetSubdevice(0)->Regs();

            // Lwrrently all GPUs that support UPR have a maximum VMMU segment
            // size of 512MB.  If GPUs with a different maximum VMMU segment
            // size are added in the future, those values will need to be
            // checked here as well (in descending size order).
            if (regs.IsSupported(MODS_PFB_PRI_MMU_VMMU_CFG0_SEGMENT_SIZE_512MB))
            {
                alignment = 512UL * 1024UL * 1024UL;
            }
            else
            {
                MASSERT(!"UPR requested on device with unrecognized VMMU segment size definitions");
            }
        }

        for (UINT32 i = 0; i < uprStartAddrList.size(); ++i)
        {
            PHYSADDR startAddr = uprStartAddrList[i];
            UINT64 size = uprSizeList[i];
            unique_ptr<RegionInfo> region = make_unique<RegionInfo>();

            region->Name = "UPR";
            region->Size = size;
            region->Alignment = alignment;

            if (startAddr != 0xffffffffffffffffull)
            {
                region->BasePhysAddr = startAddr;
                region->FixedAddr = true;
            }

            CHECK_RC_MSG(AllocRegion(region.get()),
                "Error: failed to allocate UPR region.\n");

            m_UprRegions.push_back(std::move(region));
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Allocate the VPR region
//!
RC ProtectedRegionManager::AllocRegion(RegionInfo *region)
{
    RC rc;
    if (region->Size == 0) return rc;

    region->Size = ALIGN_UP(region->Size, region->Alignment);

    GpuSubdevice *gpuSubdevice = m_GpuDevice->GetSubdevice(0);
    if (gpuSubdevice->IsFbBroken() ||
        gpuSubdevice->IsSOC() ||
        gpuSubdevice->HasFeature(Device::GPUSUB_IS_STANDALONE_TEGRA_GPU))
    {
        CHECK_RC(AllocRegionInSysmem(region));
    }
    else
    {
        CHECK_RC(AllocRegionIlwidmem(region));
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Allocate a region in sysmem
//!
RC ProtectedRegionManager::AllocRegionInSysmem(RegionInfo *region)
{
    RC rc;

    //bug 2041372 obtain a valid memdesc for soc tests.
    GpuSubdevice *gpuSubdevice = m_GpuDevice->GetSubdevice(0);
    if (gpuSubdevice->IsSOC() && !Platform::UsesLwgpuDriver())
    {
        PHYSADDR physAddr;
        UINT64 size;
        void *pMemDesc;
        CHECK_RC(Platform::GetVPR(&physAddr, &size, &pMemDesc));

        region->BasePhysAddr = physAddr;
        region->NextPhysAddr = region->BasePhysAddr;
        region->MemDesc = pMemDesc;
        region->MemDescFromCarveout = true;
    }
    else
    {
        CHECK_RC(Platform::AllocPagesAligned(
                 region->Size,
                 &region->MemDesc,
                 region->Alignment,
                 64,
                 Memory::WB,
                 m_GpuDevice->GetDeviceInst()));

        MASSERT(region->MemDesc);

        region->BasePhysAddr = Platform::GetPhysicalAddress(region->MemDesc, 0);
        region->NextPhysAddr = region->BasePhysAddr;
    }

    Printf(Tee::PriDebug,
        "Allocated %s region in sysmem; size 0x%llx, physical address 0x%llx.\n",
        region->Name.c_str(),
        region->Size,
        region->BasePhysAddr);

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Allocate a region in vidmem
//!
RC ProtectedRegionManager::AllocRegionIlwidmem(RegionInfo *region)
{
    RC rc;
    MASSERT(region->Handle == 0);

    LwRmPtr lwRm;
    UINT32 attr = DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM);
    UINT32 attr2 = 0;
    UINT64 offset = 0;
    UINT32 flags = 0;

    if (region->FixedAddr)
    {
        offset = region->BasePhysAddr;
        flags |= LWOS32_ALLOC_FLAGS_FIXED_ADDRESS_ALLOCATE;
    }

    CHECK_RC(lwRm->VidHeapAllocSizeEx(
                 LWOS32_TYPE_IMAGE, // type
                 flags,
                 &attr,
                 &attr2,
                 region->Size,
                 region->Alignment,
                 0, // format
                 0, // coverage
                 0, // stride
                 0, // ctag offset
                 &(region->Handle),
                 &offset,
                 0, // limit
                 0, // return free
                 0, // return total
                 0, // width
                 0, // height
                 m_GpuDevice,
                 0, // range begin
                 0, // range end
                 0));

    // Get the starting physical address of the allocation.
    LW0041_CTRL_GET_SURFACE_PHYS_ATTR_PARAMS params = {0};
    params.mmuContext = TEGRA_VASPACE_A;

    CHECK_RC(lwRm->Control(region->Handle,
        LW0041_CTRL_CMD_GET_SURFACE_PHYS_ATTR,
        &params,
        sizeof(params)));

    region->BasePhysAddr = ALIGN_UP(params.memOffset, region->Alignment);
    region->NextPhysAddr = region->BasePhysAddr;
    region->Size -= region->BasePhysAddr - params.memOffset;
    region->Size = ALIGN_DOWN(region->Size, region->Alignment);

    Printf(Tee::PriDebug,
        "Allocated %s region in vidmem; size 0x%llx, physical address 0x%llx.\n",
        region->Name.c_str(),
        region->Size,
        region->BasePhysAddr);

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Free all memory allocated to protected regions.
//!
RC ProtectedRegionManager::FreeRegions()
{
    RC rc;
    vector<LwRm::Handle>::iterator iter;
    LwRmPtr lwRm;

    for (iter = m_AllocatedHandles.begin();
         iter != m_AllocatedHandles.end();
         ++iter)
    {
        lwRm->FreeOsDescriptor(m_GpuDevice, *iter);
    }

    m_AllocatedHandles.clear();

    GpuSubdevice *gpuSubdevice = m_GpuDevice->GetSubdevice(0);

    // Reset the VPR registers.  This is needed to prevent RM from issuing
    // a CBC backingstore error.
    if (IsVprRegionAllocated())
    {
        CHECK_RC_MSG(gpuSubdevice->RestoreVprRegisters(),
            "Error: could not restore VPR registers.\n");
    }

    m_VprRegion.Free();
    m_Acr1Region.Free();
    m_Acr2Region.Free();

    for (auto& uprRegion : m_UprRegions)
    {
        uprRegion->Free();
    }
    m_UprRegions.clear();

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Free the memory allocated to one protected region.
//!
void ProtectedRegionManager::RegionInfo::Free()
{
    LwRmPtr lwRm;

    if (Handle != 0)
    {
        lwRm->Free(Handle);
        Handle = 0;
    }
    if (MemDesc && !MemDescFromCarveout)
    {
        Platform::FreePages(MemDesc);
    }
}

//------------------------------------------------------------------------------
//! \brief Initialize registers for all protected regions
//!
RC ProtectedRegionManager::InitSubdevice
(
    GpuSubdevice *gpuSubdevice
)
{
    RC rc;

    if (IsVprRegionAllocated())
    {
        CHECK_RC_MSG(gpuSubdevice->SetVprRegisters(
            m_VprRegion.Size,
            m_VprRegion.BasePhysAddr),
            "Error: VPR is unsupported by this GPU.\n");
    }
    else if (m_VprRegion.ReadExisting)
    {
        rc = gpuSubdevice->ReadVprRegisters(
            &m_VprRegion.Size,
            &m_VprRegion.BasePhysAddr);

        // If there are no VPR registers to read, just skip.
        if (RC::UNSUPPORTED_FUNCTION == rc)
        {
            rc.Clear();
        }
        else if (OK != rc)
        {
            Printf(Tee::PriError, "Couldn't read VPR region registers.\n");
            return rc;
        }
    }

    if (IsAcr1RegionAllocated() || IsAcr2RegionAllocated())
    {
        CHECK_RC_MSG(gpuSubdevice->SetAcrRegisters(
            m_Acr1Region.Size, m_Acr1Region.BasePhysAddr,
            m_Acr2Region.Size, m_Acr2Region.BasePhysAddr),
            "Error: ACR is unsupported by this GPU.");
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Allocate a portion of the VPR memory
//!
RC ProtectedRegionManager::AllocVprMem
(
    UINT64 size,
    UINT32 type,
    UINT32 flags,
    UINT32 attr,
    UINT32 attr2,
    LwRm::Handle *handle,
    PHYSADDR *physAddr
)
{
    RC rc;

    CHECK_RC(AllocFromRegion(&m_VprRegion, size, type, flags, attr, attr2,
        handle, physAddr));

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Reserve a portion of the VPR memory
//!
//! This function mainly differs from AllocVprMem in that it will not call
//! VidHeapOsDescriptor to create a memory handle.  This function is primarily
//! used by RM, which will create its own internal memory descriptor.
//
RC ProtectedRegionManager::ReserveVprMem
(
    UINT64 size,
    UINT64 *physAddr,
    void **sysMemDesc,
    UINT64 *sysMemOffset
)
{
    RC rc;
    CHECK_RC(ReserveFromRegion(&m_VprRegion, size, physAddr));

    if (m_VprRegion.InSysmem())
    {
        *sysMemDesc = m_VprRegion.MemDesc;
        *sysMemOffset = *physAddr - m_VprRegion.BasePhysAddr;
    }
    else
    {
        *sysMemDesc = nullptr;
        *sysMemOffset = 0;
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Allocate a portion of the ACR2 memory
//!
RC ProtectedRegionManager::AllocAcr1Mem
(
    UINT64 size,
    UINT32 type,
    UINT32 flags,
    UINT32 attr,
    UINT32 attr2,
    LwRm::Handle *handle,
    PHYSADDR *physAddr
)
{
    RC rc;

    CHECK_RC(AllocFromRegion(&m_Acr1Region, size, type, flags, attr, attr2,
        handle, physAddr));

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Allocate a portion of the ACR2 memory
//!
RC ProtectedRegionManager::AllocAcr2Mem
(
    UINT64 size,
    UINT32 type,
    UINT32 flags,
    UINT32 attr,
    UINT32 attr2,
    LwRm::Handle *handle,
    PHYSADDR *physAddr
)
{
    RC rc;

    CHECK_RC(AllocFromRegion(&m_Acr2Region, size, type, flags, attr, attr2,
        handle, physAddr));

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Allocate a portion of the UPR memory
//!
RC ProtectedRegionManager::AllolwprMem
(
    UINT64 size,
    UINT32 type,
    UINT32 flags,
    UINT32 attr,
    UINT32 attr2,
    LwRm::Handle *handle,
    PHYSADDR *physAddr
)
{
    RC rc;

    for (auto& region : m_UprRegions)
    {
        if (size <= region->UnusedSize())
        {
            rc = AllocFromRegion(
                region.get(),
                size,
                type,
                flags,
                attr,
                attr2,
                handle,
                physAddr);

            return rc;
        }
    }

    Printf(Tee::PriError, "out of UPR memory.\n");
    rc = RC::LWRM_INSUFFICIENT_RESOURCES;

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Allocate a portion of a protected region.
//!
RC ProtectedRegionManager::AllocFromRegion
(
    RegionInfo *region,
    UINT64 size,
    UINT32 type,
    UINT32 flags,
    UINT32 attr,
    UINT32 attr2,
    LwRm::Handle *handle,
    PHYSADDR *physAddr
)
{
    RC rc;
    MASSERT(handle);
    MASSERT(physAddr);

    if (region->Size == 0)
    {
        Printf(Tee::PriError, "Error: surface assigned to the %s region,"
            " but that region does not exist.\n",
            region->Name.c_str());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    // Remove the VPR and ACR flags because the descriptor should not be
    // allocated in those regions.
    flags &= ~LWOS32_ALLOC_FLAGS_PROTECTED;
    flags &= ~LWOS32_ALLOC_FLAGS_WPR1;
    flags &= ~LWOS32_ALLOC_FLAGS_WPR2;

    if (LWOS32_ATTR_COMPR_NONE != DRF_VAL(OS32, _ATTR, _COMPR, attr))
    {
        Printf(Tee::PriWarn, "Warning: compression not allowed for surfaces in VPR; ignorning.\n");
        attr = FLD_SET_DRF(OS32, _ATTR, _COMPR, _NONE, attr);
    }

    attr = FLD_SET_DRF(OS32, _ATTR, _PHYSICALITY, _CONTIGUOUS, attr);

    UINT64 pageSize;

    if (region->InSysmem())
    {
        attr = FLD_SET_DRF(OS32, _ATTR, _LOCATION, _PCI, attr);
        attr = FLD_SET_DRF(OS32, _ATTR, _COHERENCY, _CACHED, attr);

        pageSize = Platform::GetPageSize();
        attr = FLD_SET_DRF(OS32, _ATTR, _PAGE_SIZE, _4KB, attr);
    }
    else
    {
        attr = FLD_SET_DRF(OS32, _ATTR, _LOCATION, _VIDMEM, attr);

        // Find a pagesize that's at least bigPageSize, but preferably
        // no bigger than "size".  This module is called from the RM
        // before GpuDevice::Initialize, so we need to call
        // GetSupportedPageSizes first.
        CHECK_RC(m_GpuDevice->GetSupportedPageSizes(nullptr));
        pageSize = m_GpuDevice->GetMaxPageSize(0, size);
        pageSize = max(pageSize, m_GpuDevice->GetBigPageSize());
        switch (pageSize)
        {
            case GpuDevice::PAGESIZE_SMALL:
                attr = FLD_SET_DRF(OS32, _ATTR, _PAGE_SIZE, _4KB, attr);
                break;
            case GpuDevice::PAGESIZE_BIG1:
            case GpuDevice::PAGESIZE_BIG2:
                attr = FLD_SET_DRF(OS32, _ATTR, _PAGE_SIZE, _BIG, attr);
                break;
            case GpuDevice::PAGESIZE_HUGE:
                attr = FLD_SET_DRF(OS32, _ATTR, _PAGE_SIZE, _HUGE, attr);
                attr2 = FLD_SET_DRF(OS32, _ATTR2, _PAGE_SIZE_HUGE, _2MB,
                                    attr2);
                break;
            case GpuDevice::PAGESIZE_512MB:
                attr = FLD_SET_DRF(OS32, _ATTR, _PAGE_SIZE, _HUGE, attr);
                attr2 = FLD_SET_DRF(OS32, _ATTR2, _PAGE_SIZE_HUGE, _512MB,
                                    attr2);
                break;
            default:
                MASSERT(!"Unknown page size");
                Printf(Tee::PriError, "Unknown page size 0x%08llx\n", pageSize);
                return RC::SOFTWARE_ERROR;
        }
    }

    // Grab the next free set of pages in the pre-allocated region.

    LwRmPtr pLwRm;
    *physAddr = ALIGN_UP(region->NextPhysAddr, pageSize);

    region->NextPhysAddr = ALIGN_UP(*physAddr + size, pageSize);

    if (region->NextPhysAddr > region->BasePhysAddr + region->Size)
    {
        Printf(Tee::PriError, "Error: out of %s memory.\n",
            region->Name.c_str());
        return RC::LWRM_INSUFFICIENT_RESOURCES;
    }

    // Create a memory handle to represent this suballocation of
    // physical memory from the protected region.
    CHECK_RC(pLwRm->VidHeapOsDescriptor(type, flags, attr, attr2, physAddr,
        size, m_GpuDevice, handle));

    Printf(Tee::PriDebug,
        "Allocated 0x%llx bytes of %s memory at physical address 0x%llx\n",
        size,
        region->Name.c_str(),
        *physAddr);

    m_AllocatedHandles.push_back(*handle);

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Reserve a portion of a protected region.
//!
//! This function mainly differs from AllocFromRegion in that it will not call
//! VidHeapOsDescriptor to create a memory handle.  This function is primarily
//! used by RM, which will create its own internal memory descriptor.
//
RC ProtectedRegionManager::ReserveFromRegion
(
    RegionInfo *region,
    UINT64 size,
    PHYSADDR *physAddr
)
{
    RC rc;
    MASSERT(physAddr);

    if (region->Size == 0)
    {
        Printf(Tee::PriError, "Error: surface assigned to the %s region,"
            " but that region does not exist.\n",
            region->Name.c_str());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    UINT64 pageSize;

    if (region->InSysmem())
    {
        pageSize = Platform::GetPageSize();
    }
    else
    {
        // Find a pagesize that's at least bigPageSize, but preferably
        // no bigger than "size".  This module is called from the RM
        // before GpuDevice::Initialize, so we need to call
        // GetSupportedPageSizes first.
        CHECK_RC(m_GpuDevice->GetSupportedPageSizes(nullptr));
        pageSize = m_GpuDevice->GetMaxPageSize(0, size);
        pageSize = max(pageSize, m_GpuDevice->GetBigPageSize());
    }

    // Grab the next free set of pages in the pre-allocated region.

    *physAddr = ALIGN_UP(region->NextPhysAddr, pageSize);

    region->NextPhysAddr = ALIGN_UP(*physAddr + size, pageSize);

    if (region->NextPhysAddr > region->BasePhysAddr + region->Size)
    {
        Printf(Tee::PriError, "Error: out of %s memory.\n",
            region->Name.c_str());
        return RC::LWRM_INSUFFICIENT_RESOURCES;
    }

    Printf(Tee::PriDebug,
        "Reserved 0x%llx bytes of %s memory at physical address 0x%llx\n",
        size,
        region->Name.c_str(),
        *physAddr);

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Return a list of UPR regions
//
vector<std::pair<PHYSADDR, UINT64>> ProtectedRegionManager::GetUprRegionList()
{
    vector<std::pair<PHYSADDR, UINT64>> regionList;

    for (auto& uprRegion : m_UprRegions)
    {
        regionList.push_back(
            std::make_pair(uprRegion->BasePhysAddr, uprRegion->Size));
    }

    return regionList;
}
