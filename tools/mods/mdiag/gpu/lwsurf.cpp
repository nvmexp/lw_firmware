/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <queue>
#include "mdiag/utils/surfutil.h"
#include "mdiag/sysspec.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpumgr.h"
#include "core/include/mgrmgr.h"
#include "core/include/utility.h"
#include "gpu/utility/surfrdwr.h"
#include "core/include/chiplibtracecapture.h"
#include "mdiag/resource/lwgpu/dmard.h"
#include "mdiag/utils/utils.h"
#include "mdiag/lwgpu.h"
#include "core/include/tar.h"
#include "gpu/utility/surfrdwr.h"
#include "gpu/utility/scopereg.h"
#include "mdiag/resource/lwgpu/dmasurfr.h"

#include "fermi/gf100/dev_bus.h"
#include "fermi/gf100/dev_ram.h"

const char *DmaTargetName[] =
{
    "video memory",
    "coherent system memory",
    "noncoherent system memory",
    "optimal location",
    NULL
};

const char* GetTypeName(SurfaceType type)
{
    const char *name;

    switch (type)
    {
        case SURFACE_TYPE_COLORA:
            name = "ColorA";
            break;
        case SURFACE_TYPE_COLORB:
            name = "ColorB";
            break;
        case SURFACE_TYPE_COLORC:
            name = "ColorC";
            break;
        case SURFACE_TYPE_COLORD:
            name = "ColorD";
            break;
        case SURFACE_TYPE_COLORE:
            name = "ColorE";
            break;
        case SURFACE_TYPE_COLORF:
            name = "ColorF";
            break;
        case SURFACE_TYPE_COLORG:
            name = "ColorG";
            break;
        case SURFACE_TYPE_COLORH:
            name = "ColorH";
            break;
        case SURFACE_TYPE_Z:
            name = "Zeta";
            break;
        case SURFACE_TYPE_VERTEX_BUFFER:
            name = "Vertex Buffer";
            break;
        case SURFACE_TYPE_TEXTURE:
            name = "Texture";
            break;
        case SURFACE_TYPE_CLIPID:
            name = "ClipID";
            break;
        case SURFACE_TYPE_ZLWLL:
            name = "ZLwll";
            break;
        case SURFACE_TYPE_INDEX_BUFFER:
            name = "Index Buffer";
            break;
        case SURFACE_TYPE_SEMAPHORE:
            name = "Semaphore";
            break;
        case SURFACE_TYPE_NOTIFIER:
            name = "Notifier";
            break;
        case SURFACE_TYPE_UNKNOWN:
            name = "Unknown";
            break;
        default:
            name = "Who knows what surface";
            break;
    }

    return name;
}

bool IsColorSurface(const string& name)
{
    return (name.size() == 6) && (name.substr(0,5) == "Color");
}

bool IsSuitableForPeerMemory(const MdiagSurf* pSurface)
{
    // Check prerequisites.
    if (!pSurface->GetMemHandle())
    {
        //DebugPrintf("%s: !GetMemHandle\n", __FUNCTION__);
        return false;
    }
    // I don't think a split surface can be peer-ed.
    if (pSurface->GetSplit())
    {
        //DebugPrintf("%s: GetSplit\n", __FUNCTION__);
        return false;
    }
    if (pSurface->GetLocation() != Memory::Fb)
    {
        //DebugPrintf("%s: GetLocation\n", __FUNCTION__);
        return false;
    }
    return true;
}

void PrintParams(const MdiagSurf* pSurface)
{
    DebugPrintf("LWSurfaceImpl::PrintParams ptr= %p, name= '%s'\n",
                pSurface, pSurface->GetName().c_str());
    DebugPrintf("  m_Size= 0x%08X\n", pSurface->GetAllocSize());
    DebugPrintf("  m_hMem= 0x%08X\n", pSurface->GetMemHandle());
    DebugPrintf("  m_hCtxDma= 0x%08X\n", pSurface->GetCtxDmaHandle());
    DebugPrintf("  m_hVirtMem= 0x%08X\n", pSurface->GetVirtMemHandle());
    //DebugPrintf("  m_VirtMemMapped= %d\n", m_VirtMemMapped);
    DebugPrintf("  m_CtxDmaOffsetGpu= 0x%llX\n",
                pSurface->GetCtxDmaOffsetGpu());
    DebugPrintf("  GetSplit= %d\n", pSurface->GetSplit());
    DebugPrintf("  IsSuitableForPeerMemory= %d\n",
                IsSuitableForPeerMemory(pSurface));
    DebugPrintf("  m_Location= '%s'\n",
                DmaTargetName[pSurface->GetLocation()]);
    DebugPrintf("  m_Layout= '%s'\n",
                (pSurface->GetLayout() == Surface::Pitch) ? "Pitch" :
                (pSurface->GetLayout() == Surface::BlockLinear) ? "BlockLinear" :
                 "Invalid Layout");
    DebugPrintf("  depth= %d\n", pSurface->GetBytesPerPixel());
    DebugPrintf("SLI and hybrid related data structs:\n");

    GpuDevMgr *pDevMgr = ((GpuDevMgr*)DevMgrMgr::d_GraphDevMgr);
    GpuDevice *pGpuDevice;
    GpuDevice *pLocalDevice = pSurface->GetGpuDev();
    bool       bIsRemote;

    for (pGpuDevice = pDevMgr->GetFirstGpuDevice(); (pGpuDevice != NULL);
         pGpuDevice = pDevMgr->GetNextGpuDevice(pGpuDevice))
    {
        bIsRemote = (pLocalDevice != pGpuDevice);

        DebugPrintf("%s device %d\n", bIsRemote ? "local" : "remote",
                    pGpuDevice->GetDeviceInst());
        DebugPrintf("  m_PeerIsMapped= %d\n", pSurface->IsPeerMapped(pGpuDevice));
        DebugPrintf("  m_MapShared= %d\n", pSurface->IsMapShared(pGpuDevice));

        if (pSurface->IsPeerMapped(pGpuDevice))
        {
            if (bIsRemote)
            {
                DebugPrintf("  Hybrid Peer Mapping\n");
                DebugPrintf("  m_hCtxDmaGpuPeer= 0x%08X\n",
                            pSurface->GetCtxDmaHandleGpuPeer(pGpuDevice));
                for (UINT32 remoteSubdev = 0;
                      remoteSubdev < pGpuDevice->GetNumSubdevices();
                      remoteSubdev++)
                {
                    DebugPrintf("  remote subdev %d/%d\n",
                             remoteSubdev, pGpuDevice->GetNumSubdevices()-1);
                    for (UINT32 localSubdev = 0;
                          localSubdev < pLocalDevice->GetNumSubdevices();
                          localSubdev++)
                    {
                        DebugPrintf("    local subdev %d/%d\n",
                                 localSubdev, pLocalDevice->GetNumSubdevices()-1);
                        DebugPrintf("      m_CtxDmaOffsetGpuPeer= 0x%llx\n",
                                 pSurface->GetCtxDmaOffsetGpuPeer(localSubdev,
                                                                  pGpuDevice,
                                                                  remoteSubdev));
                        DebugPrintf("      m_GpuVirtAddrPeer= 0x%llx\n",
                                 pSurface->GetGpuVirtAddrPeer(localSubdev,
                                                              pGpuDevice,
                                                              remoteSubdev));
                        DebugPrintf("      m_hVirtMemGpuPeer= 0x%x\n",
                                 pSurface->GetVirtMemHandleGpuPeer(localSubdev,
                                                                   pGpuDevice,
                                                                   remoteSubdev));
                    }
                }
            }
            else
            {
                DebugPrintf("  %s Peer Mapping\n",
                          (pGpuDevice->GetNumSubdevices() > 1) ?
                                  "SLI" : "Loopback");
                for (UINT32 subdev = 0;
                      subdev < pGpuDevice->GetNumSubdevices();
                      subdev++)
                {
                    DebugPrintf("  subdev %d/%d\n",
                             subdev, pGpuDevice->GetNumSubdevices()-1);
                    DebugPrintf("    m_CtxDmaOffsetGpuPeer= 0x%llx\n",
                             pSurface->GetCtxDmaOffsetGpuPeer(subdev));
                    DebugPrintf("    m_GpuVirtAddrPeer= 0x%llx\n",
                             pSurface->GetGpuVirtAddrPeer(subdev));
                    DebugPrintf("    m_hVirtMemGpuPeer= 0x%x\n",
                             pSurface->GetVirtMemHandleGpuPeer(subdev));
                }
            }
        }
        else if (pSurface->IsMapShared(pGpuDevice))
        {
            DebugPrintf("  Shared System Memory Mapping\n");
            DebugPrintf("  m_hCtxDmaGpuShared= 0x%08X\n",
                        pSurface->GetCtxDmaHandleGpuShared(pGpuDevice));
            DebugPrintf("  m_CtxDmaOffsetGpuShared= 0x%llx\n",
                      pSurface->GetCtxDmaOffsetGpuShared(pGpuDevice));
            DebugPrintf("  m_GpuVirtAddrShared= 0x%llx\n",
                     pSurface->GetGpuVirtAddrShared(pGpuDevice));
            DebugPrintf("  m_hVirtMemGpuShared= 0x%x\n",
                     pSurface->GetVirtMemHandleGpuShared(pGpuDevice));
        }
        else
        {
            DebugPrintf("  No SLI or hybrid params.\n");
        }
    }
}

AAmodetype GetAAModeFromSurface(MdiagSurf *surface)
{
    AAmodetype mode = AAMODE_1X1;

    switch (surface->GetAAMode())
    {
        case Surface2D::AA_1:
            mode = AAMODE_1X1;
            break;

        case Surface2D::AA_2:
            if (surface->IsD3DSwizzled())
            {
                mode = AAMODE_2X1_D3D;
            }
            else
            {
                mode = AAMODE_2X1;
            }
            break;

        case Surface2D::AA_4:
        case Surface2D::AA_4_Rotated:
            mode = AAMODE_2X2;
            break;

        case Surface2D::AA_8:
            if (surface->IsD3DSwizzled())
            {
                mode = AAMODE_4X2_D3D;
            }
            else
            {
                mode = AAMODE_4X2;
            }
            break;

        case Surface2D::AA_16:
            mode = AAMODE_4X4;
            break;

        case Surface2D::AA_4_Virtual_8:
            mode = AAMODE_2X2_VC_4;
            break;

        case Surface2D::AA_4_Virtual_16:
            mode = AAMODE_2X2_VC_12;
            break;

        case Surface2D::AA_8_Virtual_16:
            mode = AAMODE_4X2_VC_8;
            break;

        case Surface2D::AA_8_Virtual_32:
            mode = AAMODE_4X2_VC_24;
            break;

        default:
            // Ignore all other AA modes.
            break;
    }

    return mode;
}

void SetSurfaceAAMode(MdiagSurf *surface, AAmodetype mode)
{
    switch (mode)
    {
        case AAMODE_1X1:
            surface->SetAAMode(Surface::AA_1);
            break;

        case AAMODE_2X1:
            surface->SetAAMode(Surface::AA_2);
            break;

        case AAMODE_2X1_D3D:
            surface->SetAAMode(Surface::AA_2);
            surface->SetD3DSwizzled(true);
            break;

        case AAMODE_2X2:
            surface->SetAAMode(Surface::AA_4_Rotated);
            break;

        case AAMODE_4X2:
            surface->SetAAMode(Surface::AA_8);
            break;

        case AAMODE_4X2_D3D:
            surface->SetAAMode(Surface::AA_8);
            surface->SetD3DSwizzled(true);
            break;

        case AAMODE_2X2_VC_4:
            surface->SetAAMode(Surface::AA_4_Virtual_8);
            break;

        case AAMODE_2X2_VC_12:
            surface->SetAAMode(Surface::AA_4_Virtual_16);
            break;

        case AAMODE_4X2_VC_8:
            surface->SetAAMode(Surface::AA_8_Virtual_16);
            break;

        case AAMODE_4X2_VC_24:
            surface->SetAAMode(Surface::AA_8_Virtual_32);
            break;

        case AAMODE_4X4:
            surface->SetAAMode(Surface::AA_16);
            break;

        default:
            // An invalid AA will be caught elsewhere so ignore here.
            break;
    }
}

bool GetAAModeFromName(const char* aaname, Surface2D::AAMode* aamode)
{
    if (!strcmp(aaname, "1X1"))
    {
        *aamode = Surface2D::AA_1;
    }
    else if (!strcmp(aaname, "2X1") || !strcmp(aaname, "2X1_D3D"))
    {
        *aamode = Surface2D::AA_2;
    }
    else if (!strcmp(aaname, "2X2"))
    {
        *aamode = Surface2D::AA_4;
    }
    else if (!strcmp(aaname, "4X2") || !strcmp(aaname, "4X2_D3D"))
    {
        *aamode = Surface2D::AA_8;
    }
    else if (!strcmp(aaname, "4X4"))
    {
        *aamode = Surface2D::AA_16;
    }
    else if (!strcmp(aaname, "2X2_VC_4"))
    {
        *aamode = Surface2D::AA_4_Virtual_8;
    }
    else if (!strcmp(aaname, "2X2_VC_12"))
    {
        *aamode = Surface2D::AA_4_Virtual_16;
    }
    else if (!strcmp(aaname, "4X2_VC_8"))
    {
        *aamode = Surface2D::AA_8_Virtual_16;
    }
    else if (!strcmp(aaname, "4X2_VC_24"))
    {
        *aamode = Surface2D::AA_8_Virtual_32;
    }
    else
    {
        ErrPrintf("Unrecognized SAMPLE_MODE %s\n", aaname);
        return false;
    }
    return true;
}

// If the user specified a TIR mode via the command-line, then
// that TIR mode overrides the AA mode of the Z buffer.
// This function determines what the new AA mode should be
// based on the TIR mode.  If no TIR mode was specified, then
// the original AA mode of the Z buffer should be used.
//
AAmodetype GetTirAAMode(const ArgReader *params, AAmodetype aaMode)
{
    AAmodetype newAAMode = aaMode;

    if (params->ParamPresent("-tirrzmode"))
    {
        UINT32 tirMode = params->ParamUnsigned("-tirrzmode");

        switch (tirMode)
        {
            case 0:    // 1X1
                newAAMode = AAMODE_1X1;
                break;

            case 1:    // 2X1_D3D
                newAAMode = AAMODE_2X1_D3D;
                break;

            case 2:    // 4X2_D3D
                newAAMode = AAMODE_4X2_D3D;
                break;

            case 3:    // 2X2
                newAAMode = AAMODE_2X2;
                break;

            case 4:    // 4X4
                newAAMode = AAMODE_4X4;
                break;

            default:
                MASSERT(!"Illegal -tirrzmode value");
                break;
        }
    }

    return newAAMode;
}

static bool PollFlush(void *pVoidPollArgs)
{
    LWGpuResource *pGpuRes = (LWGpuResource *)pVoidPollArgs;

    if (pGpuRes->GetGpuDevice()->GetFamily() >= GpuDevice::Maxwell)
    {
        //LW_PLTCG_LTC0_LTSS_TSTG_CMGMT_1
        return (0x70010300 == pGpuRes->RegRd32(0x001402a4));
    }
    else
    {
        //LW_PLTCG_LTC0_LTSS_TSTG_CMGMT_1
        return (0x70010300 == pGpuRes->RegRd32(0x00140914));
    }
}

RC FlushEverything(LWGpuResource *pGpuRes, UINT32 subdev, LwRm* pLwRm)
{
    RC rc;
    bool wasDisabled = false;
    GpuSubdevice *gpuSubdevice = pGpuRes->GetGpuSubdevice(subdev);

    // The flush needs to be part of the replay, so make sure chiplib trace dumping is enabled.
    if (!ChiplibTraceDumper::GetPtr()->IsChiplibTraceDumpEnabled())
    {
        ChiplibTraceDumper::GetPtr()->EnableChiplibTraceDump();
        wasDisabled = true;
    }

    MASSERT(gpuSubdevice != 0);

    {
        ChiplibOpScope newScope("FlushEverything old", NON_IRQ, ChiplibOpScope::SCOPE_COMMON, NULL);
        CHECK_RC(gpuSubdevice->FbFlush(0x15000));
        CHECK_RC(gpuSubdevice->IlwalidateL2Cache(0));
        CHECK_RC(Platform::FlushCpuWriteCombineBuffer());
    }

    {
        ChiplibOpScope newScope("FlushEverything new", NON_IRQ, ChiplibOpScope::SCOPE_COMMON, NULL);

        if (gpuSubdevice->GetParentDevice()->GetFamily() >= GpuDevice::Maxwell)
        {
            //LW_PLTCG_LTCS_LTSS_TSTG_CMGMT_1
            pGpuRes->RegWr32( 0x17e2a4, 0x70010301 );
        }
        else
        {
            //LW_PLTCG_LTCS_LTSS_TSTG_CMGMT_1
            pGpuRes->RegWr32( 0x17e914, 0x70010301 );
        }
        CHECK_RC(POLLWRAP(PollFlush, pGpuRes, 10000));

        LW0080_CTRL_DMA_FLUSH_PARAMS dmaParams;
        memset(&dmaParams, 0, sizeof(dmaParams));
        dmaParams.targetUnit = DRF_DEF(0080_CTRL_DMA_FLUSH, _TARGET_UNIT, _FB, _ENABLE);
        dmaParams.targetUnit |= DRF_DEF(0080_CTRL_DMA_FLUSH, _TARGET_UNIT, _L2, _ENABLE);

        CHECK_RC(pLwRm->Control(
            pLwRm->GetDeviceHandle(gpuSubdevice->GetParentDevice()),
            LW0080_CTRL_CMD_DMA_FLUSH,
            &dmaParams,
            sizeof(dmaParams)));
    }

    if (wasDisabled)
    {
        ChiplibTraceDumper::GetPtr()->DisableChiplibTraceDump();
    }

    return rc;
}

RC AlignDumpToPage(GpuSubdevice *gpuSubdevice, UINT64 *physAddress, UINT32 *size, UINT32 pageSizeBytes)
{
    // For some devices the PRAMIN physical address space can be DRAM bank swizzled.
    // This swizzling is always within a page, so if we align the physical address
    // and the size to the nearest page boundary, we don't have to deal with the swizzling.
    //
    // The check for not-equal Maxwell is crude as the swizzling isn't necessarily
    // consistent across the family.  However, since we're lwrrently testing only on
    // GK208, this check will work for now.  When this code goes
    // back to chips_a, a better check will be needed here.
    //
    // Well, guess what, a better check is still needed and the checks below
    // don't make any sense, esp. that they only cover some random CheetAh variants.
    bool isSomeTegra = false;
#if LWCFG(GLOBAL_CHIP_T194)
    if (gpuSubdevice->GetParentDevice()->DeviceId() == Gpu::T194)
    {
        isSomeTegra = true;
    }
#endif
#if LWCFG(GLOBAL_CHIP_T234)
    if (gpuSubdevice->GetParentDevice()->DeviceId() == Gpu::T234)
    {
        isSomeTegra = true;
    }
#endif
    if (!isSomeTegra)
    {
        MASSERT(pageSizeBytes != 0);
        MASSERT((pageSizeBytes & (pageSizeBytes - 1)) == 0);
        UINT32 mask = pageSizeBytes - 1;

        // Align the physical address down to the nearest big page size.
        *size += (*physAddress) & mask;
        *physAddress = (*physAddress) & ~((UINT64)mask);

        // Align the size up to the nearest big page size.
        *size = (*size + mask) & ~mask;
    }

    return OK;
}

RC DumpMemoryViaBar1Phys
(
    LWGpuResource *pGpuRes,
    MdiagSurf *surface,
    UINT64 physAddress,
    UINT32 size,
    std::vector<UINT08>* pData
)
{
    RC rc;
    GpuSubdevice *gpuSubdevice = surface->GetGpuSubdev(0);
    MASSERT(gpuSubdevice != 0);

    ChiplibOpScope newScope("DumpMemoryViaBar1Phys", NON_IRQ, ChiplibOpScope::SCOPE_COMMON, NULL);

    auto scopedBar1BlockReg = gpuSubdevice->EnableScopedBar1PhysMemAccess();
    pData->reserve(size);

    UINT64 address = physAddress + gpuSubdevice->GetPhysFbBase();
    void *pMem = 0;

    Platform::MapDeviceMemory(&pMem, gpuSubdevice->GetPhysFbBase(),
        gpuSubdevice->FbApertureSize(), Memory::UC, Memory::ReadWrite);
    Platform::PhysRd(address, &(*pData)[0], size);
    Platform::UnMapDeviceMemory(pMem);

    return rc;
}

RC DumpMemoryViaPramin
(
    LWGpuResource *pGpuRes,
    MdiagSurf *surface,
    UINT64 physAddress,
    UINT32 size,
    std::vector<UINT08>* pData
)
{
    RC rc;
    GpuSubdevice *gpuSubdevice = surface->GetGpuSubdev(0);
    MASSERT(gpuSubdevice != 0);
    MASSERT(gpuSubdevice == pGpuRes->GetGpuSubdevice());

    ChiplibOpScope newScope("DumpMemoryViaPramin", NON_IRQ, ChiplibOpScope::SCOPE_COMMON, NULL);

    CHECK_RC(MDiagUtils::PraminPhysRdRaw(gpuSubdevice, physAddress, surface->GetLocation(), size, pData));

    return rc;
}

RC DumpMemoryViaPteChange
(
    LwRm* pLwRm,
    SmcEngine* pSmcEngine,
    MdiagSurf *surface,
    UINT64 bufferOffset,
    UINT32 size,
    LWGpuResource* pGpuRes,
    LWGpuChannel* channel,
    const ArgReader* params,
    std::vector<UINT08>* pData
)
{
    RC rc;

    ChiplibOpScope newScope("DumpMemoryViaPteChange", NON_IRQ, ChiplibOpScope::SCOPE_COMMON, NULL);

    pData->resize(size);

    UINT32 offset = 0;
    queue<UINT32> savedKinds;

    while (offset < surface->GetAllocSize())
    {
        LW0080_CTRL_DMA_GET_PTE_INFO_PARAMS getParams = { 0 };
        getParams.gpuAddr = surface->GetCtxDmaOffsetGpu() + offset;

        CHECK_RC(pLwRm->ControlByDevice(
                     surface->GetGpuDev(), LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                     &getParams, sizeof(getParams)));

        LW0080_CTRL_DMA_SET_PTE_INFO_PARAMS setParams = {0};
        setParams.gpuAddr = getParams.gpuAddr;

        for (UINT32 ii = 0; ii < LW0080_CTRL_DMA_GET_PTE_INFO_PTE_BLOCKS; ++ii)
        {
             setParams.pteBlocks[ii] = getParams.pteBlocks[ii];
             savedKinds.push(getParams.pteBlocks[ii].kind);
             if (setParams.pteBlocks[ii].pageSize != 0 &&
                 FLD_TEST_DRF(0080_CTRL_DMA_PTE_INFO_PARAMS, _FLAGS, _VALID,
                     _TRUE, setParams.pteBlocks[ii].pteFlags))
             {
                 setParams.pteBlocks[ii].kind = 0xfd;
             }
        }

        CHECK_RC(pLwRm->ControlByDevice(surface->GetGpuDev(),
                     LW0080_CTRL_CMD_DMA_SET_PTE_INFO,
                     &setParams, sizeof(setParams)));

        offset += surface->GetActualPageSizeKB() * 1024;
    }

    DmaReader* reader = DmaReader::CreateDmaReader
    (
        DmaReader::CE,
        DmaReader::NOTIFY,
        pGpuRes,
        channel,
        size,
        params,
        pLwRm,
        pSmcEngine,
        0/*subdev*/
    );

    reader->AllocateNotifier(GetPageLayoutFromParams(params, NULL));

    reader->AllocateDstBuffer(GetPageLayoutFromParams(params, NULL), size, _DMA_TARGET_COHERENT);

    reader->AllocateObject();

    reader->ReadLine(surface->GetCtxDmaHandle(),
        surface->GetCtxDmaOffsetGpu() + surface->GetExtraAllocSize() + bufferOffset,
        0, 1, size, 0, surface->GetSurf2D());

    Platform::VirtualRd((void *)reader->GetBuffer(), &(*pData)[0], size);

    // restore pte kind
    offset = 0;

    while (offset < surface->GetAllocSize())
    {
        LW0080_CTRL_DMA_GET_PTE_INFO_PARAMS getParams = { 0 };
        getParams.gpuAddr = surface->GetCtxDmaOffsetGpu() + offset;

        CHECK_RC(pLwRm->ControlByDevice(
                     surface->GetGpuDev(), LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                     &getParams, sizeof(getParams)));

        LW0080_CTRL_DMA_SET_PTE_INFO_PARAMS setParams = {0};
        setParams.gpuAddr = getParams.gpuAddr;

        for (UINT32 ii = 0; ii < LW0080_CTRL_DMA_GET_PTE_INFO_PTE_BLOCKS; ++ii)
        {
             setParams.pteBlocks[ii] = getParams.pteBlocks[ii];
             MASSERT(savedKinds.size() > 0);
             setParams.pteBlocks[ii].kind = savedKinds.front();
             savedKinds.pop();
        }

        CHECK_RC(pLwRm->ControlByDevice(surface->GetGpuDev(),
                     LW0080_CTRL_CMD_DMA_SET_PTE_INFO,
                     &setParams, sizeof(setParams)));

        offset += surface->GetActualPageSizeKB() * 1024;
    }

    MASSERT(savedKinds.size() == 0);

    return rc;
}

RC DumpMemoryViaDirectPhysRd
(
    MdiagSurf *surface,
    UINT64 physAddress,
    UINT32 size,
    std::vector<UINT08>* pData
)
{
    RC rc;
    ChiplibOpScope newScope("DumpMemoryViaDirectPhysRd", NON_IRQ, ChiplibOpScope::SCOPE_COMMON, NULL);

    pData->resize(size);

    void *pMem = 0;
    Platform::MapDeviceMemory(&pMem, physAddress, size, Memory::UC, Memory::ReadWrite);
    Platform::PhysRd(physAddress, &(*pData)[0], size);
    Platform::UnMapDeviceMemory(pMem);

    return rc;
}

RC DumpRawSurfaceMemory
(
    LwRm* pLwRm,
    SmcEngine* pSmcEngine,
    MdiagSurf *surface,
    UINT64 offset,
    UINT32 size,
    string name,
    bool isCrc,
    LWGpuResource *pGpuRes,
    LWGpuChannel *channel,
    const ArgReader *params,
    std::vector<UINT08> *pData
)
{
    RC rc;

    bool useArchive = false;
    bool createArchive = false;
    string archiveName;
    const UINT32 subdev = 0;

    GpuSubdevice *gpuSubdevice = surface->GetGpuSubdev(subdev);
    MASSERT(gpuSubdevice != 0);

    string dumpFilename;

    if (isCrc)
    {
        dumpFilename = name + ".chiplib_dump.raw";
    }
    else
    {
        dumpFilename = name + ".chiplib_init.raw";
    }

    string gpuName = Gpu::DeviceIdToString(gpuSubdevice->DeviceId());
    string archiveDumpFilename;

    if (params->ParamPresent("-backdoor_archive"))
    {
        useArchive = true;
        UINT32 mode = params->ParamUnsigned("-backdoor_archive");

        switch (mode)
        {
            case 0:
                createArchive = true;
                break;

            case 1:
                // Nothing to do here.
                break;

            default:
                ErrPrintf("unrecognized -backdoor_archive mode '%u'\n", mode);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        archiveName = params->ParamStr("-i");
        archiveName = UtilStrTranslate(archiveName, DIR_SEPARATOR_CHAR2, DIR_SEPARATOR_CHAR);

        size_t pos;
        pos = archiveName.rfind("test.hdr");
        if (pos == string::npos)
        {
            ErrPrintf("trace file path %s does not end with test.hdr\n", archiveName.c_str());
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        archiveName.replace(pos, 8, "backdoor.tgz");

        if (params->ParamPresent("-backdoor_archive_id"))
        {
            archiveDumpFilename = Utility::StrPrintf("%s/%s/%s",
                gpuName.c_str(),
                params->ParamStr("-backdoor_archive_id"),
                dumpFilename.c_str());
        }
        else
        {
            ErrPrintf("-backdoor_archive_id required when using either -backdoor_archive_create or -backdoor_archive_read\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    UINT32 dumpMode = params->ParamUnsigned("-raw_memory_dump_mode", 0);
    UINT64 physAddress = 0;
    UINT32 realSize = size;

    CHECK_RC(surface->GetPhysAddress(offset, &physAddress));

    // Both the BAR1 phys and the PRAMIN methods can be affected
    // by DRAM bank swizzling depending on the GPU.
    // Page alignment needs to be done to account for DRAM bank swizzling.
    if ((dumpMode == DUMP_RAW_VIA_BAR1_PHYS) ||
        (dumpMode == DUMP_RAW_VIA_PRAMIN))
    {
        CHECK_RC(AlignDumpToPage(gpuSubdevice, &physAddress, &realSize, surface->GetActualPageSizeKB()*1024));
    }

    if ((params->ParamPresent("-raw_memory_dump_mode") == 0) && Platform::IsTegra())
    {
        // For SOC, it's faster to use cpu to read sysmem directly
        dumpMode = DUMP_RAW_VIA_PHYS_READ;
    }
    else if ((dumpMode == DUMP_RAW_VIA_BAR1_PHYS) &&
        (physAddress + realSize >= 0x8000000))
    {
        // When mapping BAR1 to physical address space, only 128MB can be addressed.
        // If the surface is allocated beyond that, we need to switch to the PRAMIN mode.
        dumpMode = DUMP_RAW_VIA_PRAMIN;
    }

    std::vector<UINT08> data;

    if (pData == 0)
    {
        MASSERT(dumpMode != DUMP_RAW_CRC);
        pData = &data;
    }

    bool dumpMem = true;

    // If the archive is being used, but only for reading (not creating),
    // make sure the file is already in the archive.  If it is, dumping can be
    // skipped.  If the file isn't already in the archive, issue an error.
    if (useArchive && !createArchive)
    {
    }

    // This mode is for dumping the raw mem op only, no raw memory dumping.
    else if (dumpMode == DUMP_RAW_MEM_OP_ONLY)
    {
        dumpMem = false;
    }

    FlushEverything(pGpuRes, subdev, pLwRm);

    string outDirName;
    if (params->ParamPresent("-o") &&
        (ChiplibTraceDumper::GetPtr()->GetTestNum() > 1))
    {
        outDirName = params->ParamStr("-o", "");
        outDirName = Utility::StringReplace(outDirName,"/", "_") + "_";
    }
    string fullName = outDirName + dumpFilename;

    if (dumpMem)
    {
        bool wasEnabled = false;

        // Disable the chiplib trace dump as we don't want the mem dump to be part of the capture.
        if (ChiplibTraceDumper::GetPtr()->IsChiplibTraceDumpEnabled())
        {
            ChiplibTraceDumper::GetPtr()->DisableChiplibTraceDump();
            wasEnabled = true;
        }

        switch (dumpMode)
        {
            case DUMP_RAW_VIA_BAR1_PHYS:
                CHECK_RC(DumpMemoryViaBar1Phys(pGpuRes, surface, physAddress, realSize, pData));
                break;

            case DUMP_RAW_VIA_PRAMIN:
                CHECK_RC(DumpMemoryViaPramin(pGpuRes, surface, physAddress, realSize, pData));
                break;

            case DUMP_RAW_VIA_PTE_CHANGE:
                CHECK_RC(DumpMemoryViaPteChange(pLwRm, pSmcEngine, surface, offset, realSize, pGpuRes, channel, params, pData));
                break;

            case DUMP_RAW_VIA_PHYS_READ:
                if (!Platform::IsTegra())
                {
                    ErrPrintf("Mode 4(read sysmem directly) is only suitable for SOC '%u'.\n");
                    return RC::BAD_COMMAND_LINE_ARGUMENT;
                }

                CHECK_RC(DumpMemoryViaDirectPhysRd(surface, physAddress, realSize, pData));
                break;

            case DUMP_RAW_CRC:
                // Data is already in pData.
                break;

            default:
                ErrPrintf("Illegal raw memory dump mode '%u'.\n", dumpMode);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        if (!createArchive)
        {
            // TODO Support group mode when doing archive.
            dumpFilename = fullName;
        }

        FILE *dumpFp;
        CHECK_RC(Utility::OpenFile(dumpFilename.c_str(), &dumpFp, "w"));

        for (UINT32 i = 0; i < realSize; ++i)
        {
            fprintf(dumpFp, "%02x\n", (*pData)[i]);
        }

        fclose(dumpFp);

        if (createArchive)
        {
            ChiplibTraceDumper::GetPtr()->AddArchiveFile(dumpFilename, archiveDumpFilename);
        }

        // Restore the chiplib trace dump if necessary.
        if (wasEnabled)
        {
            ChiplibTraceDumper::GetPtr()->EnableChiplibTraceDump();
        }
    }

    if (ChiplibTraceDumper::GetPtr()->DumpChiplibTrace())
    {
        ChiplibOpBlock *block = ChiplibTraceDumper::GetPtr()->GetLwrrentChiplibOpBlock();

        const string aperture = (surface->GetLocation() == Memory::Fb) ? "FB" : "sysmem";

        if (isCrc)
        {
            dumpFilename = outDirName + name + ".chiplib_replay.raw";
            block->AddRawMemDumpOp(aperture, physAddress, realSize, dumpFilename);
        }
        else
        {
            dumpFilename = fullName;
            block->AddRawMemLoadOp(aperture, physAddress, realSize, dumpFilename);
        }
    }

    return rc;
}

// This function gets the data needed for the SET_ANTI_ALIAS_SAMPLE_POSITIONS
// methods.  The surface parameter is used to get the AA mode and the index
// parameter is used to determine which of the four methods to get data for.
//
UINT32 GetProgrammableSampleData(MdiagSurf *surface, UINT32 index,
    const ArgReader *params)
{
    // This is a table of programmable AA sample data indexed by AA mode
    // and method number (there are four programmable sample methods).
    static const UINT32 sampleData[12][4] = {
        { 0x88888888, 0x88888888, 0x88888888, 0x88888888 },  // AAMODE_1X1
        { 0x44cc44cc, 0x44cc44cc, 0x44cc44cc, 0x44cc44cc },  // AAMODE_2X1_D3D
        { 0x359db759, 0x1ffb71d3, 0x359db759, 0x1ffb71d3 },  // AAMODE_4X2_D3D
        { 0xcc44cc44, 0xcc44cc44, 0xcc44cc44, 0xcc44cc44 },  // AAMODE_2X1
        { 0x00000000, 0x00000000, 0x00000000, 0x00000000 },  // unused
        { 0xeaa26e26, 0xeaa26e26, 0xeaa26e26, 0xeaa26e26 },  // AAMODE_2X2
        { 0xb7d33571, 0x9dfb1f59, 0xb7d33571, 0x9dfb1f59 },  // AAMODE_4X2
        { 0x00000000, 0x00000000, 0x00000000, 0x00000000 },  // unused
        { 0x00000000, 0x00000000, 0x00000000, 0x00000000 },  // unused
        { 0x00000000, 0x00000000, 0x00000000, 0x00000000 },  // unused
        { 0x00000000, 0x00000000, 0x00000000, 0x00000000 },  // unused
        { 0x7ca55799, 0x3bbdda63, 0xc22418e6, 0x01fe4f80 },  // AAMODE_4X4
    };

    AAmodetype mode;

    mode = GetAAModeFromSurface(surface);

    // Modify the AA mode by the TIR mode if necessary.
    mode = GetTirAAMode(params, mode);

    MASSERT(mode < 12);
    MASSERT(index < 4);

    UINT32 data = sampleData[mode][index];
    MASSERT(data != 0);

    return data;
}

// This function gets the data needed for a FakeGL trace to do per-sample
// anti-aliasing via shader program.  The surface parameter is used to get
// the AA mode and the index parameter is used to determine which of the
// 16 samples the data is for.
//
UINT32 GetProgrammableSampleDataFloat(MdiagSurf *surface, UINT32 index,
    const ArgReader *params)
{
    // This is a table of programmable AA sample data indexed by AA mode
    // and sample index (there are 16 possible samples).
    static const UINT32 sampleDataFloat[12][16] =
    {
        { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },  // AAMODE_1X1
        { 0x04000400, 0xfc00fc00, 0x04000400, 0xfc00fc00, 0x04000400, 0xfc00fc00, 0x04000400, 0xfc00fc00, 0x04000400, 0xfc00fc00, 0x04000400, 0xfc00fc00, 0x04000400, 0xfc00fc00, 0x04000400, 0xfc00fc00 },  // AAMODE_2X1_D3D
        { 0xfd000100, 0x0300ff00, 0x01000500, 0xfb00fd00, 0x0500fb00, 0xff00f900, 0x07000300, 0xf9000700, 0xfd000100, 0x0300ff00, 0x01000500, 0xfb00fd00, 0x0500fb00, 0xff00f900, 0x07000300, 0xf9000700 },  // AAMODE_4X2_D3D
        { 0xfc00fc00, 0x04000400, 0xfc00fc00, 0x04000400, 0xfc00fc00, 0x04000400, 0xfc00fc00, 0x04000400, 0xfc00fc00, 0x04000400, 0xfc00fc00, 0x04000400, 0xfc00fc00, 0x04000400, 0xfc00fc00, 0x04000400 },  // AAMODE_2X1
        { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff },  // unused
        { 0xfa00fe00, 0xfe000600, 0x0200fa00, 0x06000200, 0xfa00fe00, 0xfe000600, 0x0200fa00, 0x06000200, 0xfa00fe00, 0xfe000600, 0x0200fa00, 0x06000200, 0xfa00fe00, 0xfe000600, 0x0200fa00, 0x06000200 },  // AAMODE_2X2
        { 0xff00f900, 0xfb00fd00, 0x0500fb00, 0x0300ff00, 0xfd000100, 0xf9000700, 0x07000300, 0x01000500, 0xff00f900, 0xfb00fd00, 0x0500fb00, 0x0300ff00, 0xfd000100, 0xf9000700, 0x07000300, 0x01000500 },  // AAMODE_4X2
        { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff },  // unused
        { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff },  // unused
        { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff },  // unused
        { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff },  // unused
        { 0x01000100, 0xfd00ff00, 0x0200fd00, 0xff000400, 0xfe00fb00, 0x05000200, 0x03000500, 0xfb000300, 0x0600fe00, 0xf9000000, 0xfa00fc00, 0x0400fa00, 0x0000f800, 0xfc000700, 0x07000600, 0xf800f900 },  // AAMODE_4X4
    };

    AAmodetype mode;

    mode = GetAAModeFromSurface(surface);

    // Modify the AA mode by the TIR mode if necessary.
    mode = GetTirAAMode(params, mode);

    MASSERT(mode < 12);
    MASSERT(index < 16);

    UINT32 data = sampleDataFloat[mode][index];
    MASSERT(data != 0xffffffff);

    return data;
}

namespace SurfaceUtils
{
    unique_ptr<MappingSurfaceReader> CreateMappingSurfaceReader(const MdiagSurf& surface)
    {
        auto* pMapSurface = surface.GetCpuMappableSurface();
        MASSERT(pMapSurface != nullptr);
        return make_unique<MappingSurfaceReader>(*(pMapSurface->GetSurf2D()));
    }

    unique_ptr<MappingSurfaceWriter> CreateMappingSurfaceWriter(MdiagSurf* pSurface)
    {
        MASSERT(pSurface != nullptr);
        auto* pMapSurface = pSurface->GetCpuMappableSurface();
        MASSERT(pMapSurface != nullptr);
        return make_unique<MappingSurfaceWriter>(*(pMapSurface->GetSurf2D()));
    }

    unique_ptr<MappingSurfaceReadWriter> CreateMappingSurfaceReadWriter(MdiagSurf* pSurface)
    {
        MASSERT(pSurface != nullptr);
        auto* pMapSurface = pSurface->GetCpuMappableSurface();
        MASSERT(pMapSurface != nullptr);
        return make_unique<MappingSurfaceReadWriter>(*(pMapSurface->GetSurf2D()));
    }

    unique_ptr<DmaSurfaceReader> CreateDmaSurfaceReader(const MdiagSurf& surface, DmaReader* pDmaReader)
    {
        MASSERT(pDmaReader != nullptr);
        return make_unique<DmaSurfaceReader>(*(surface.GetSurf2D()), *pDmaReader);
    }
}
