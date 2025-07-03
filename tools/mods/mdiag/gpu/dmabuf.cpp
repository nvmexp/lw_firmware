/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2018,2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "mdiag/utils/surfutil.h"
#include "core/include/massert.h"
#include "core/include/lwrm.h"
#include "lwmisc.h"
#include "core/include/gpu.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "core/include/cmdline.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "lwos.h"
#include "gpu/include/gpumgr.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/gpuutils.h"

#include <map>

//----------------------------------------------------------------------------
RC SetPteKind(MdiagSurf& Surf, const string& pteKindName, GpuDevice* pGpuDev)
{
    if ((pteKindName != "") && (pGpuDev != 0))
    {
        UINT32 PteKind = 0;
        if (!pGpuDev->GetSubdevice(0)->GetPteKindFromName(pteKindName.c_str(), &PteKind))
        {
            ErrPrintf("Invalid PTE kind name: %s\n", pteKindName.c_str());
            return RC::SOFTWARE_ERROR;
        }
        Surf.SetPteKind(PteKind);
    }
    return OK;
}

//----------------------------------------------------------------------------
RC ParseDmaBufferArgs(Surface2D& Surf, const ArgReader *params, const char *MemTypeName, string* PteKindName, bool* NeedPeerMapping)
{
    StickyRC rc = OK;

    if (Surf.GetLocation() == Memory::Optimal)
        Surf.SetLocation(Memory::Fb);
    _DMA_TARGET target = LocationToTarget(Surf.GetLocation());
    target = GetLocationFromParams(params, MemTypeName, target);
    if (target == _DMA_TARGET_P2P)
    {
        target = _DMA_TARGET_VIDEO;
        if (NeedPeerMapping)
            *NeedPeerMapping = true;
    }
    else if (NeedPeerMapping)
        *NeedPeerMapping = false;
    Surf.SetLocation(TargetToLocation(target));
    _PAGE_LAYOUT Layout = GetPageLayoutFromParams(params, MemTypeName);

    string ArgName = Utility::StrPrintf("-split_%s", MemTypeName);
    if (params->ParamPresent(ArgName.c_str()) > 0)
    {
        Surf.SetSplit(true);
    }

    ArgName = Utility::StrPrintf("-split_loc_%s", MemTypeName);
    if (params->ParamPresent(ArgName.c_str()) > 0)
    {
        target = (_DMA_TARGET)params->ParamUnsigned(ArgName.c_str(), target);
        Surf.SetSplitLocation(TargetToLocation(target));
    }

    ArgName = Utility::StrPrintf("-pte_kind_%s", MemTypeName);
    if (params->ParamPresent(ArgName.c_str()) && (PteKindName != 0))
    {
        // Don't care about PTE kind on amodel
        if (Platform::GetSimulationMode() != Platform::Amodel)
        {
            *PteKindName = params->ParamStr(ArgName.c_str());
        }
    }

    ArgName = Utility::StrPrintf("-split_pte_kind_%s", MemTypeName);
    if (params->ParamPresent(ArgName.c_str()))
    {
        WarnPrintf("Argument %s not yet supported\n", ArgName.c_str());
    }

    ArgName = Utility::StrPrintf("-part_stride_%s", MemTypeName);
    if (params->ParamPresent(ArgName.c_str()))
        Surf.SetPartStride(params->ParamUnsigned(ArgName.c_str()));

    ArgName = Utility::StrPrintf("-ctxdma_attrs_%s", MemTypeName);
    if (params->ParamPresent("-ctxdma_attrs") ||
        params->ParamPresent(ArgName.c_str()))
    {
        Surf.SetMemAttrsInCtxDma(true);
    }

    ArgName = Utility::StrPrintf("-privileged_%s", MemTypeName);
    if (params->ParamPresent("-privileged") ||
        params->ParamPresent(ArgName.c_str()))
    {
        Surf.SetPriv(true);
    }

    ArgName = Utility::StrPrintf("-va_reverse_%s", MemTypeName);
    if (params->ParamPresent("-va_reverse") ||
        params->ParamPresent(ArgName.c_str()))
    {
        Surf.SetVaReverse(true);
    }

    ArgName = Utility::StrPrintf("-pa_reverse_%s", MemTypeName);
    if (params->ParamPresent("-pa_reverse") ||
        params->ParamPresent(ArgName.c_str()))
    {
        Surf.SetPaReverse(true);
    }

    ArgName = Utility::StrPrintf("-max_coalesce_%s", MemTypeName);
    if (params->ParamPresent(ArgName.c_str()))
        Surf.SetMaxCoalesce(params->ParamUnsigned(ArgName.c_str()));
    else if (params->ParamPresent("-max_coalesce"))
        Surf.SetMaxCoalesce(params->ParamUnsigned("-mem_model"));

    ArgName = Utility::StrPrintf("-phys_align_%s", MemTypeName);

    if (params->ParamPresent(ArgName.c_str()))
    {
        Surf.SetAlignment(params->ParamUnsigned(ArgName.c_str()));
    }

    ArgName = Utility::StrPrintf("-offset_%s", MemTypeName);

    if (params->ParamPresent(ArgName.c_str()))
    {
        Surf.SetExtraAllocSize(params->ParamUnsigned64(ArgName.c_str()));
    }

    ArgName = Utility::StrPrintf("-adjust_%s", MemTypeName);

    if (params->ParamPresent(ArgName.c_str()))
    {
        Surf.SetHiddenAllocSize(params->ParamUnsigned64(ArgName.c_str()));
    }

    ArgName = Utility::StrPrintf("-limit_%s", MemTypeName);

    if (params->ParamPresent(ArgName.c_str()))
    {
        Surf.SetLimit(params->ParamSigned(ArgName.c_str()));
    }

    SetPageLayout(Surf, Layout, (params->ParamPresent("-force_cont")>0));

    ArgName = Utility::StrPrintf("-zbc_mode_%s", MemTypeName);

    if (params->ParamPresent(ArgName.c_str()))
    {
        if (params->ParamUnsigned(ArgName.c_str()) > 0)
        {
            Surf.SetZbcMode(Surface2D::ZbcOn);
        }
        else
        {
            Surf.SetZbcMode(Surface2D::ZbcOff);
        }
    }
    else if (params->ParamPresent("-zbc_mode"))
    {
        if (params->ParamUnsigned("-zbc_mode") > 0)
        {
            Surf.SetZbcMode(Surface2D::ZbcOn);
        }
        else
        {
            Surf.SetZbcMode(Surface2D::ZbcOff);
        }
    }

    ArgName = Utility::StrPrintf("-gpu_cache_mode_%s", MemTypeName);

    if (params->ParamPresent(ArgName.c_str()))
    {
        if (params->ParamUnsigned(ArgName.c_str()) > 0)
        {
            Surf.SetGpuCacheMode(Surface2D::GpuCacheOn);
        }
        else
        {
            Surf.SetGpuCacheMode(Surface2D::GpuCacheOff);
        }
    }
    else if (params->ParamPresent("-gpu_cache_mode"))
    {
        if (params->ParamUnsigned("-gpu_cache_mode") > 0)
        {
            Surf.SetGpuCacheMode(Surface2D::GpuCacheOn);
        }
        else
        {
            Surf.SetGpuCacheMode(Surface2D::GpuCacheOff);
        }
    }

    ArgName = Utility::StrPrintf("-gpu_p2p_cache_mode_%s", MemTypeName);

    if (params->ParamPresent(ArgName.c_str()))
    {
        if (params->ParamUnsigned(ArgName.c_str()) > 0)
        {
            Surf.SetP2PGpuCacheMode(Surface2D::GpuCacheOn);
        }
        else
        {
            Surf.SetP2PGpuCacheMode(Surface2D::GpuCacheOff);
        }
    }
    else if (params->ParamPresent("-gpu_p2p_cache_mode"))
    {
        if (params->ParamUnsigned("-gpu_p2p_cache_mode") > 0)
        {
            Surf.SetP2PGpuCacheMode(Surface2D::GpuCacheOn);
        }
        else
        {
            Surf.SetP2PGpuCacheMode(Surface2D::GpuCacheOff);
        }
    }

    ArgName = Utility::StrPrintf("-split_gpu_cache_mode_%s", MemTypeName);

    if (params->ParamPresent(ArgName.c_str()))
    {
        if (params->ParamUnsigned(ArgName.c_str()) > 0)
        {
            Surf.SetSplitGpuCacheMode(Surface2D::GpuCacheOn);
        }
        else
        {
            Surf.SetSplitGpuCacheMode(Surface2D::GpuCacheOff);
        }
    }
    else if (params->ParamPresent("-split_gpu_cache_mode"))
    {
        if (params->ParamUnsigned("-split_gpu_cache_mode") > 0)
        {
            Surf.SetSplitGpuCacheMode(Surface2D::GpuCacheOn);
        }
        else
        {
            Surf.SetSplitGpuCacheMode(Surface2D::GpuCacheOff);
        }
    }

    ArgName = Utility::StrPrintf("-virt_addr_hint_%s", MemTypeName);

    if (params->ParamPresent(ArgName.c_str()))
    {
        Surf.SetGpuVirtAddrHint(params->ParamUnsigned64(ArgName.c_str()));
    }
    else
    {
        ArgName = Utility::StrPrintf("-virt_addr_range_%s", MemTypeName);

        if (params->ParamPresent(ArgName.c_str()))
        {
            Surf.SetGpuVirtAddrHint(
                params->ParamNUnsigned64(ArgName.c_str(), 0));
            Surf.SetGpuVirtAddrHintMax(
                params->ParamNUnsigned64(ArgName.c_str(), 1));
        }
    }

    ArgName = Utility::StrPrintf("-phys_addr_hint_%s", MemTypeName);

    if (params->ParamPresent(ArgName.c_str()))
    {
        Surf.SetGpuPhysAddrHint(params->ParamUnsigned64(ArgName.c_str()));
    }
    else
    {
        ArgName = Utility::StrPrintf("-phys_addr_range_%s", MemTypeName);

        if (params->ParamPresent(ArgName.c_str()))
        {
            Surf.SetGpuPhysAddrHint(
                params->ParamNUnsigned64(ArgName.c_str(), 0));
            Surf.SetGpuPhysAddrHintMax(
                params->ParamNUnsigned64(ArgName.c_str(), 1));
        }
        else if (params->ParamPresent("-phys_addr_range"))
        {
            Surf.SetGpuPhysAddrHint(
                params->ParamNUnsigned64("-phys_addr_range", 0));
            Surf.SetGpuPhysAddrHintMax(
                params->ParamNUnsigned64("-phys_addr_range", 1));
        }
    }

    ArgName = Utility::StrPrintf("-map_mode_%s", MemTypeName);

    if (params->ParamPresent(ArgName.c_str()) > 0)
    {
        if (params->ParamUnsigned(ArgName.c_str()) > 0)
        {
            Surf.SetMemoryMappingMode(Surface2D::MapReflected);
        }
        else
        {
            Surf.SetMemoryMappingMode(Surface2D::MapDirect);
        }
    }
    else if (params->ParamPresent("-map_mode") > 0)
    {
        if (params->ParamUnsigned("-map_mode") > 0)
        {
            Surf.SetMemoryMappingMode(Surface2D::MapReflected);
        }
        else
        {
            Surf.SetMemoryMappingMode(Surface2D::MapDirect);
        }
    }

    ArgName = Utility::StrPrintf("-upr_%s", MemTypeName);

    if (params->ParamPresent(ArgName.c_str()))
    {
        string value = params->ParamStr(ArgName.c_str());

        if (value == "ON")
        {
            Surf.SetAllocInUpr(true);
        }
        else if (value == "OFF")
        {
            Surf.SetAllocInUpr(false);
        }
        else
        {
            ErrPrintf("Unrecognized UPR value %s for %s argument.\n",
                value.c_str(), ArgName.c_str());

            rc = RC::ILWALID_FILE_FORMAT;
        }
    }

    ArgName = Utility::StrPrintf("-vpr_%s", MemTypeName);

    if (params->ParamPresent(ArgName.c_str()))
    {
        string value = params->ParamStr(ArgName.c_str());

        if (value == "ON")
        {
            Surf.SetVideoProtected(true);
        }
        else if (value == "OFF")
        {
            Surf.SetVideoProtected(false);
        }
        else
        {
            ErrPrintf("Unrecognized VPR value %s for %s argument.\n",
                value.c_str(), ArgName.c_str());

            rc = RC::ILWALID_FILE_FORMAT;
        }
    }

    ArgName = Utility::StrPrintf("-acr1_%s", MemTypeName);

    if (params->ParamPresent(ArgName.c_str()))
    {
        string value = params->ParamStr(ArgName.c_str());

        if (value == "ON")
        {
            Surf.SetAcrRegion1(true);
        }
        else if (value == "OFF")
        {
            Surf.SetAcrRegion1(false);
        }
        else
        {
            ErrPrintf("Unrecognized ACR1 value %s for %s argument.\n",
                value.c_str(), ArgName.c_str());

            rc = RC::ILWALID_FILE_FORMAT;
        }
    }

    ArgName = Utility::StrPrintf("-acr2_%s", MemTypeName);

    if (params->ParamPresent(ArgName.c_str()))
    {
        string value = params->ParamStr(ArgName.c_str());

        if (value == "ON")
        {
            Surf.SetAcrRegion2(true);
        }
        else if (value == "OFF")
        {
            Surf.SetAcrRegion2(false);
        }
        else
        {
            ErrPrintf("Unrecognized ACR2 value %s for %s argument.\n",
                value.c_str(), ArgName.c_str());

            rc = RC::ILWALID_FILE_FORMAT;
        }
    }

    ArgName = Utility::StrPrintf("-fb_speed_%s", MemTypeName);

    if (params->ParamPresent(ArgName.c_str()))
    {
        string value = params->ParamStr(ArgName.c_str());

        if (value == "FAST")
        {
            Surf.SetFbSpeed(Surface2D::FbSpeedFast);
        }
        else if (value == "SLOW")
        {
            Surf.SetFbSpeed(Surface2D::FbSpeedSlow);
        }
        else if (value == "DEFAULT")
        {
            Surf.SetFbSpeed(Surface2D::FbSpeedDefault);
        }
        else
        {
            ErrPrintf("Unrecognized value %s for argument %s.  "
                "Valid values are FAST, SLOW, and DEFAULT.\n",
                value.c_str(), ArgName.c_str());
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    return rc;
}

//----------------------------------------------------------------------------
RC ParseDmaBufferArgs(MdiagSurf& Surf, const ArgReader *params, const char *MemTypeName, string* PteKindName, bool* NeedPeerMapping)
{
    RC rc;

    ParseDmaBufferArgs(*Surf.GetSurf2D(), params, MemTypeName, PteKindName,
        NeedPeerMapping);

    string ArgName = Utility::StrPrintf("-ats_map_%s", MemTypeName);

    if (params->ParamPresent(ArgName.c_str()))
    {
        Surf.SetAtsMapped();
    }

    ArgName = Utility::StrPrintf("-no_gmmu_map_%s", MemTypeName);

    if (params->ParamPresent(ArgName.c_str()))
    {
        Surf.SetNoGmmuMap();
    }

    ArgName = Utility::StrPrintf("-ats_page_size_%s", MemTypeName);

    if (params->ParamPresent(ArgName.c_str()))
    {
        Surf.SetAtsPageSize(params->ParamUnsigned(ArgName.c_str()));
    }

    return rc;
}

//----------------------------------------------------------------------------
_DMA_TARGET GetLocationFromParams(const ArgReader *params,
                                   const char *MemTypeName,
                                   _DMA_TARGET DefaultLoc)
{
    string ArgName = Utility::StrPrintf("-loc_%s", MemTypeName);
    if (params->ParamPresent("-loc") > 0)
        DefaultLoc = (_DMA_TARGET)params->ParamUnsigned("-loc", DefaultLoc);

    return (_DMA_TARGET)params->ParamUnsigned(ArgName.c_str(), DefaultLoc);
}

//----------------------------------------------------------------------------
_PAGE_LAYOUT GetPageLayoutFromParams(const ArgReader *params,
                                      const char *MemTypeName)
{
    _PAGE_LAYOUT Layout;
    UINT32 PageSize;

    if (MemTypeName)
    {
        string ArgName = Utility::StrPrintf("-ctx_dma_type_%s", MemTypeName);
        if (params->ParamPresent(ArgName.c_str()))
            Layout = (_PAGE_LAYOUT)params->ParamUnsigned(ArgName.c_str());
        else
            Layout = (_PAGE_LAYOUT)params->ParamUnsigned("-ctx_dma_type",
                PAGE_VIRTUAL);
    }
    else
    {
        Layout = (_PAGE_LAYOUT)params->ParamUnsigned("-ctx_dma_type",
            PAGE_VIRTUAL);
    }

    if (MemTypeName)
    {
        string ArgName = Utility::StrPrintf("-page_size_%s", MemTypeName);
        if (params->ParamPresent(ArgName.c_str()))
            PageSize = params->ParamUnsigned(ArgName.c_str());
        else
            PageSize = params->ParamUnsigned("-page_size", 0);
    }
    else
    {
        PageSize = params->ParamUnsigned("-page_size", 0);
    }

    // Allow page size overrides only for virtual context DMAs
    // Yuck... we should make page size and context DMA type orthogonal.
    if (Layout == PAGE_VIRTUAL)
    {
        switch (PageSize)
        {
            case 0:
                break;
            case 4:
                Layout = PAGE_VIRTUAL_4KB;
                break;
            case 64:
            case 128:
                Layout = PAGE_VIRTUAL_64KB;
                break;
            case 2048:
                Layout = PAGE_VIRTUAL_2MB;
                break;
            case 524288:
                Layout = PAGE_VIRTUAL_512MB;
                break;
            default:
                MASSERT(0);
        }
    }

    return Layout;
}

void PrintDmaBufferParams(MdiagSurf& Surf)
{
    return PrintDmaBufferParams(*Surf.GetSurf2D());
}

void PrintDmaBufferParams(Surface2D& Surf)
{
    DebugPrintf("DmaBuffer::PrintParams ptr= %p\n", &Surf);
    DebugPrintf("  m_Size= 0x%llx\n", Surf.GetSize());
     InfoPrintf("  m_hMem= 0x%08x\n", Surf.GetMemHandle());
    DebugPrintf("  m_hCtxDma= 0x%08x\n", Surf.GetCtxDmaHandle());
    DebugPrintf("  m_hVirtMem= 0x%08x\n", Surf.GetVirtMemHandle());
    DebugPrintf("  m_VidMemOffset= 0x%08x\n", Surf.GetVidMemOffset());
    DebugPrintf("  m_Offset= 0x%llx\n", Surf.GetCtxDmaOffsetGpu()+Surf.GetExtraAllocSize());
    DebugPrintf("  m_Location= '%s'\n",
                GetMemoryLocationName(Surf.GetLocation()));
    DebugPrintf("  IsPagingAndFb= %d:\n",
                Surf.GetMemHandle() &&
                (Surf.GetLocation() == Memory::Fb) &&
                (Surf.GetAddressModel() == Memory::Paging));
    DebugPrintf("  SLI and hybrid related data structs:\n");

    GpuDevMgr *pDevMgr = ((GpuDevMgr*)DevMgrMgr::d_GraphDevMgr);
    GpuDevice *pGpuDevice;
    GpuDevice *pLocalDevice = Surf.GetGpuDev();
    bool       bIsRemote;

    for (pGpuDevice = pDevMgr->GetFirstGpuDevice(); (pGpuDevice != NULL);
         pGpuDevice = pDevMgr->GetNextGpuDevice(pGpuDevice))
    {
        bIsRemote = (pLocalDevice != pGpuDevice);

        DebugPrintf("%s device %d\n", bIsRemote ? "local" : "remote",
                    pGpuDevice->GetDeviceInst());
        DebugPrintf("  m_PeerIsMapped= %d\n", Surf.IsPeerMapped(pGpuDevice));

        if (Surf.IsPeerMapped(pGpuDevice))
        {
            if (bIsRemote)
            {
                DebugPrintf("  m_hCtxDmaGpuPeer= 0x%08X\n",
                            Surf.GetCtxDmaHandleGpuPeer(pGpuDevice));
                for (UINT32 remSD = 0;
                     remSD < pGpuDevice->GetNumSubdevices(); remSD++)
                {
                    DebugPrintf("  remote subdev %d/%d\n",
                           remSD, pGpuDevice->GetNumSubdevices()-1);

                    for (UINT32 locSD = 0;
                          locSD < pLocalDevice->GetNumSubdevices(); locSD++)
                    {
                        DebugPrintf("    local subdev %d/%d\n",
                               locSD, pLocalDevice->GetNumSubdevices()-1);
                        DebugPrintf("      m_CtxDmaOffsetGpuPeer= 0x%llx\n",
                                    Surf.GetCtxDmaOffsetGpuPeer(locSD, pGpuDevice, remSD));
                        DebugPrintf("      m_GpuVirtAddrPeer= 0x%llx\n",
                                    Surf.GetGpuVirtAddrPeer(locSD, pGpuDevice, remSD));
                        DebugPrintf("      m_hVirtMemGpuPeer= 0x%x\n",
                                    Surf.GetVirtMemHandleGpuPeer(locSD, pGpuDevice, remSD));
                    }
                }
            }
            else
            {
                for (UINT32 subdev = 0;
                      subdev < pGpuDevice->GetNumSubdevices();
                      subdev++)
                {
                    DebugPrintf("  subdev %d/%d\n",
                             subdev, pGpuDevice->GetNumSubdevices()-1);
                    DebugPrintf("    m_CtxDmaOffsetGpuPeer= 0x%llx\n",
                             Surf.GetCtxDmaOffsetGpuPeer(subdev, pGpuDevice, 0));
                    DebugPrintf("    m_GpuVirtAddrPeer= 0x%llx\n",
                             Surf.GetGpuVirtAddrPeer(subdev, pGpuDevice, 0));
                    DebugPrintf("    m_hVirtMemGpuPeer= 0x%x\n",
                             Surf.GetVirtMemHandleGpuPeer(subdev, pGpuDevice, 0));
                }
            }
        }
        else
        {
            DebugPrintf("  No SLI or hybrid related params.\n");
        }
    }

    const Surface::GpuObjectMappings& gpuObjMap = Surf.GetGpuObjectMappings();
    DebugPrintf("  GPU object mapping data structs:\n");
    DebugPrintf("  GPU object mapping Count= %d\n", gpuObjMap.size());
    if (!gpuObjMap.empty())
    {
        Surface::GpuObjectMappings::const_iterator iter;
        UINT32 mapEntry;

        for (iter = gpuObjMap.begin(), mapEntry = 0;
             iter != gpuObjMap.end(); iter++, mapEntry++)
        {
            DebugPrintf("mapping %d/%d\n", mapEntry, gpuObjMap.size());

            DebugPrintf("  GPU object handle= 0x%x\n", iter->first);
            DebugPrintf("  GpuObjectOffset= 0x%llx\n", iter->second);
        }
    }
    else
    {
        DebugPrintf("  No GPU object mapping params.\n");
    }
}

_DMA_TARGET LocationToTarget(Memory::Location location)
{
    switch (location)
    {
        case Memory::Coherent:
            return _DMA_TARGET_COHERENT;
        case Memory::NonCoherent:
            return _DMA_TARGET_NONCOHERENT;
        case Memory::Fb:
            return _DMA_TARGET_VIDEO;
        default:
            return _DMA_TARGET(location);
    }
}

void SetPageLayout(Surface2D& Surf, _PAGE_LAYOUT Layout, bool ForceCont)
{
    bool contig =  Surf.GetPhysContig() || ForceCont;

    switch (Layout)
    {
        case PAGE_PHYSICAL:
            Surf.SetPhysContig(true);
            Surf.SetPageSize(0);
            break;
        case PAGE_VIRTUAL:
            Surf.SetPhysContig(contig);
            break;
        case PAGE_VIRTUAL_4KB:
            Surf.SetPhysContig(contig);
            Surf.SetPageSize(4);
            break;
        case PAGE_VIRTUAL_64KB:
            Surf.SetPhysContig(contig);
            Surf.SetPageSize(64);
            break;
        case PAGE_VIRTUAL_2MB:
            Surf.SetPhysContig(contig);
            Surf.SetPageSize(2048);
            break;
        case PAGE_VIRTUAL_512MB:
            Surf.SetPhysContig(contig);
            Surf.SetPageSize(524288);
            break;
        default:
            MASSERT(!"Invalid page layout");
            break;
    }
}

void SetPageLayout(MdiagSurf& Surf, _PAGE_LAYOUT Layout, bool ForceCont)
{
    SetPageLayout(*(Surf.GetSurf2D()), Layout, ForceCont);
}

_PAGE_LAYOUT GetPageLayout(const MdiagSurf& Surf)
{
    switch (Surf.GetPageSize())
    {
        case 0:
            return Surf.GetPhysContig() ? PAGE_PHYSICAL : PAGE_VIRTUAL;
        case 4:
            return PAGE_VIRTUAL_4KB;
        case 64:
            return PAGE_VIRTUAL_64KB;
        case 2048:
            return PAGE_VIRTUAL_2MB;
        case 524288:
            return PAGE_VIRTUAL_512MB;
        default:
            MASSERT(!"Invalid PageSize");
            return PAGE_VIRTUAL;
    }
}
