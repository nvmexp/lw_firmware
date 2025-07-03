/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_SURFUTIL_H
#define INCLUDED_SURFUTIL_H

#include "surf_types.h"
#include "SurfaceType.h"

class ArgReader;
class LWGpuChannel;
class GpuDevice;
class LWGpuResource;
class MdiagSurf;
class SmcEngine;
class DmaReader;

namespace SurfaceUtils
{
    class MappingSurfaceReader;
    class MappingSurfaceWriter;
    class MappingSurfaceReadWriter;
    class DmaSurfaceReader;
}

enum Compression
{
    COMPR_REQUIRED = 1,
    COMPR_ANY,
    COMPR_NONE
};

enum ZLwllEnum
{
    ZLWLL_REQUIRED = 1,
    ZLWLL_ANY,
    ZLWLL_RESERVED,  // Placeholder for deprecated SHARED
    ZLWLL_NONE,
    ZLWLL_CTXSW_SEPERATE_BUFFER,
    ZLWLL_CTXSW_NOCTXSW
};

_DMA_TARGET LocationToTarget(Memory::Location location);
_DMA_TARGET GetLocationFromParams(const ArgReader *params,
                                  const char *MemTypeName,
                                  _DMA_TARGET DefaultLoc);
_PAGE_LAYOUT GetPageLayoutFromParams(const ArgReader *params,
                                     const char *MemTypeName);
void SetPageLayout(Surface2D& Surf, _PAGE_LAYOUT Layout, bool ForceContinue = false);
void SetPageLayout(MdiagSurf& Surf, _PAGE_LAYOUT Layout, bool ForceContinue = false);

_PAGE_LAYOUT GetPageLayout(const MdiagSurf& Surf);
RC ParseDmaBufferArgs(Surface2D& Surf, const ArgReader *params, const char *MemTypeName, string* PteKindName, bool* NeedPeerMapping);
RC ParseDmaBufferArgs(MdiagSurf& Surf, const ArgReader *params, const char *MemTypeName, string* PteKindName, bool* NeedPeerMapping);
RC SetPteKind(MdiagSurf& Surf, const string& PteKindName, GpuDevice* pGpuDev);
//! Print DmaBuffer-compatible surface parameters.
void PrintDmaBufferParams(Surface2D& Surf);
void PrintDmaBufferParams(MdiagSurf& Surf);

// Macros to simplify defining all of the various parameters that need to exist
// per memory space
#define BUFF_LOC_PARAMS_P2P(name, description) \
    { "-loc" name, "u", (ParamDecl::PARAM_ENFORCE_RANGE | \
                         ParamDecl::GROUP_START), _DMA_TARGET_VIDEO, _DMA_TARGET_P2P, "Location for " description }, \
    { "-vid" name,  (const char*)_DMA_TARGET_VIDEO, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED, \
      0, 0, description " in video memory" }, \
    { "-coh" name,  (const char*)_DMA_TARGET_COHERENT, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED, \
      0, 0, description " in coherent system memory" }, \
    { "-ncoh" name, (const char*)_DMA_TARGET_NONCOHERENT, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED, \
      0, 0, description " in noncoherent system memory" },\
    { "-p2p" name, (const char*)_DMA_TARGET_P2P, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED, \
      0, 0, description " in peer memory" }

#define BUFF_LOC_PARAMS_NOP2P(name, description) \
    { "-loc" name, "u", (ParamDecl::PARAM_ENFORCE_RANGE | \
                         ParamDecl::GROUP_START), _DMA_TARGET_VIDEO, _DMA_TARGET_NONCOHERENT, "Location for " description }, \
    { "-vid" name,  (const char*)_DMA_TARGET_VIDEO, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED, \
      0, 0, description " in video memory" }, \
    { "-coh" name,  (const char*)_DMA_TARGET_COHERENT, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED, \
      0, 0, description " in coherent system memory" }, \
    { "-ncoh" name, (const char*)_DMA_TARGET_NONCOHERENT, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED, \
      0, 0, description " in noncoherent system memory" }

#define MISC_LOC_PARAMS(name, description) \
    { "-loc", "u", (ParamDecl::PARAM_ENFORCE_RANGE | ParamDecl::GROUP_START), _DMA_TARGET_VIDEO, _DMA_TARGET_NONCOHERENT, \
                   "Default location for all data"}, \
    { "-vid",  (const char*)_DMA_TARGET_VIDEO, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED, 0, 0, \
                   "Put all data in video memory" }, \
    { "-coh",  (const char*)_DMA_TARGET_COHERENT, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED, 0, 0, \
                   "Put all data in coherent system memory" }, \
    { "-ncoh", (const char*)_DMA_TARGET_NONCOHERENT, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED, 0, 0, \
                   "Put all data in noncoherent system memory" }, \
    { "-split_loc" name, "u", (ParamDecl::PARAM_ENFORCE_RANGE | \
                         ParamDecl::GROUP_START), _DMA_TARGET_VIDEO, _DMA_TARGET_NONCOHERENT, "Split location for " description }, \
    SIMPLE_PARAM("-split" name, "split the surface across two apertures for " description), \
    SIMPLE_PARAM("-inst_in_sys", "Put instance memory in system memory")

#define MEMORY_LOC_PARAMS(name, description) \
    BUFF_LOC_PARAMS_P2P(name, description), \
    MISC_LOC_PARAMS(name, description)

#define MEMORY_LOC_PARAMS_NOP2P(name, description) \
    BUFF_LOC_PARAMS_NOP2P(name, description), \
    MISC_LOC_PARAMS(name, description)

#define MEMORY_ATTR_PARAMS(name, description) \
    UNSIGNED_PARAM("-page_size" name, "override the page size for " description), \
    \
    { "-ctx_dma_type" name, "u", (ParamDecl::PARAM_ENFORCE_RANGE | \
                                  ParamDecl::GROUP_START), PAGE_PHYSICAL, PAGE_VIRTUAL, "context DMA type for " description }, \
    { "-phys_ctx_dma" name,    (const char *)PAGE_PHYSICAL, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED, \
      0, 0, "use physical context DMA for " description }, \
    { "-virtual_ctx_dma" name, (const char *)PAGE_VIRTUAL, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED, \
      0, 0, "use virtual context DMA for " description }, \
    \
    STRING_PARAM("-pte_kind" name, "override the page kind for " description " (e.g. specify GENERIC_8BX2 or another kind name)"), \
    STRING_PARAM("-split_pte_kind" name, "override the page kind for split " description " (e.g. specify GENERIC_8BX2 or another kind name)"), \
    UNSIGNED_PARAM("-part_stride" name, "override the partition stride for " description), \
    \
    SIMPLE_PARAM("-ctxdma_attrs" name, "put attributes in context DMA for " description), \
    SIMPLE_PARAM("-privileged" name, "allow only privileged access to " description), \
    \
    { "-mem_model" name, "u", (ParamDecl::PARAM_ENFORCE_RANGE | \
                                ParamDecl::GROUP_START), Memory::Paging, Memory::Paging, "memory model for " description }, \
    { "-paging" name,       (const char *)Memory::Paging, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED, \
      0, 0, "use paging memory model for " description }, \
    \
    SIMPLE_PARAM("-va_reverse" name, "allocate virtual address space from the top down for " description), \
    SIMPLE_PARAM("-pa_reverse" name, "allocate physical address space from the top down for " description), \
    UNSIGNED_PARAM("-p2ploopback" name, "force surface into peer memory" description), \
    UNSIGNED_PARAM("-max_coalesce" name, "maximum number of coalesced PTEs for " description), \
    \
    UNSIGNED_PARAM("-phys_align" name, "align memory allocation to the specified # of bytes for " description), \
    UNSIGNED64_PARAM("-offset" name, "offsets the starting CPU virtual address relative to the starting GPU virtual address for " description), \
    UNSIGNED64_PARAM("-adjust" name, "offsets the starting GPU virtual address relative to the base of the memory allocation for " description), \
    SIGNED_PARAM("-limit" name, "set the context DMA limit to the specified # of bytes for " description), \
    UNSIGNED_PARAM("-suballoc_align" name, "align suballocations within the " description " surface to this # bytes"), \
    SIMPLE_PARAM("-sli_p2ploopback", "use loopback for peer buffers"), \
    { "-zbc_mode" name, "u", \
        (ParamDecl::PARAM_ENFORCE_RANGE | ParamDecl::GROUP_START), 0, 1, \
        "ZBC compression mode for " description }, \
    { "-zbc_off" name, "0", ParamDecl::GROUP_MEMBER, 0, 0, \
        "don't use ZBC compression for " description }, \
    { "-zbc_on" name, "1", ParamDecl::GROUP_MEMBER, 0, 0, \
        "use ZBC compression for " description }, \
    { "-zbc" name, "1", ParamDecl::GROUP_MEMBER, 0, 0, \
        "use ZBC compression for " description }, \
    UNSIGNED64_PARAM("-virt_addr_hint" name, "desired GPU virtual address of the memory allocated for " description), \
    UNSIGNED64_PARAM("-phys_addr_hint" name, "desired GPU physical address of the memory allocated for " description), \
    { "-virt_addr_range" name, "LL", 0, 0, 0, \
      "desired GPU virtual address range of the memory allocated for " \
      description }, \
    { "-phys_addr_range" name, "LL", 0, 0, 0, \
      "desired GPU physical address range of the memory allocated for " \
      description }, \
    { "-gpu_cache_mode" name, "u", \
        (ParamDecl::PARAM_ENFORCE_RANGE | ParamDecl::GROUP_START), 0, 1, \
        "use or don't use GPU cache for " description }, \
    { "-gpu_cache_off" name, "0", ParamDecl::GROUP_MEMBER, 0, 0, \
        "don't use GPU cache for " description }, \
    { "-gpu_cache_on" name, "1", ParamDecl::GROUP_MEMBER, 0, 0, \
        "use GPU cache for " description }, \
    { "-sysmem_nolwolatile" name, "1", ParamDecl::GROUP_MEMBER, 0, 0, \
        "use non-volatile memory for " description }, \
    { "-gpu_p2p_cache_mode" name, "u", \
        (ParamDecl::PARAM_ENFORCE_RANGE | ParamDecl::GROUP_START), 0, 1, \
        "use or don't use GPU cache for peer memory " description }, \
    { "-gpu_p2p_cache_off" name, "0", ParamDecl::GROUP_MEMBER, 0, 0, \
        "don't use GPU cache for peer memory " description }, \
    { "-gpu_p2p_cache_on" name, "1", ParamDecl::GROUP_MEMBER, 0, 0, \
        "use GPU cache for peer memory " description }, \
    { "-split_gpu_cache_mode" name, "u", \
        (ParamDecl::PARAM_ENFORCE_RANGE | ParamDecl::GROUP_START), 0, 1, \
        "use or don't use GPU cache for split memory of " description }, \
    { "-split_gpu_cache_off" name, "0", ParamDecl::GROUP_MEMBER, 0, 0, \
        "don't use GPU cache for split memory of " description }, \
    { "-split_gpu_cache_on" name, "1", ParamDecl::GROUP_MEMBER, 0, 0, \
        "use GPU cache for split memory of " description }, \
    { "-map_mode" name, "u", \
        (ParamDecl::PARAM_ENFORCE_RANGE | ParamDecl::GROUP_START), 0, 1, \
        "Memory mapping mode for " description }, \
    { "-map_direct" name, "0", ParamDecl::GROUP_MEMBER, 0, 0, \
        "Memory mapping mode for " description }, \
    { "-map_reflected" name, "1", ParamDecl::GROUP_MEMBER, 0, 0, \
        "Memory mapping mode for " description }, \
    UNSIGNED_PARAM("-padding" name, "number of bytes to add to the combined memory allocation of " description ), \
    STRING_PARAM("-upr" name, "sets the UPR status for the memory allocation of " description ), \
    STRING_PARAM("-vpr" name, "sets the VPR status for the memory allocation of " description ), \
    STRING_PARAM("-acr1" name, "sets the ACR status [first of two regions] for the memory allocation of " description ), \
    STRING_PARAM("-acr2" name, "sets the ACR status [second of two regions] for the memory allocation of " description ), \
    STRING_PARAM("-fb_speed" name, "allocate to SLOW or FAST FB memory for " description ), \
    SIMPLE_PARAM("-ats_map" name, "create an ATS mapping after the allocation of " description ), \
    SIMPLE_PARAM("-no_gmmu_map" name, "remove the GMMU mapping after the allocation of " description ), \
    UNSIGNED_PARAM("-ats_page_size" name, "set the ATS page size for the allocation of " description )

#define MEMORY_SPACE_PARAMS(name, description) \
    MEMORY_LOC_PARAMS(name, description), \
    MEMORY_ATTR_PARAMS(name, description)

#define MEMORY_SPACE_PARAMS_NOP2P(name, description) \
    MEMORY_LOC_PARAMS_NOP2P(name, description), \
    MEMORY_ATTR_PARAMS(name, description)

inline Memory::Location TargetToLocation(_DMA_TARGET target)
{
    switch (target)
    {
        case _DMA_TARGET_COHERENT:
            return Memory::Coherent;
        case _DMA_TARGET_NONCOHERENT:
            return Memory::NonCoherent;
        case _DMA_TARGET_VIDEO:
            return Memory::Fb;
        default:
            return Memory::Location(target);
    }
}

const char* GetTypeName(SurfaceType type);
bool IsColorSurface(const string& name);
void PrintParams(const MdiagSurf* pSurface);
bool IsSuitableForPeerMemory(const MdiagSurf* pSurface);
RC DumpRawSurfaceMemory(LwRm* pLwRm, SmcEngine* pSmcEngine, 
                        MdiagSurf *surface, UINT64 offset, UINT32 size, string name, bool isCrc,
                        LWGpuResource *lwgpu, LWGpuChannel *channel, const ArgReader *params,
                        std::vector<UINT08> *pData = NULL);
namespace SurfaceUtils
{
    unique_ptr<MappingSurfaceReader> CreateMappingSurfaceReader(const MdiagSurf& surface);
    unique_ptr<MappingSurfaceWriter> CreateMappingSurfaceWriter(MdiagSurf* pSurface);
    unique_ptr<MappingSurfaceReadWriter> CreateMappingSurfaceReadWriter(MdiagSurf* pSurface);
    unique_ptr<DmaSurfaceReader> CreateDmaSurfaceReader(const MdiagSurf& surface, DmaReader* pDmaReader);
}

class SurfacePteModifier
{
public:
    SurfacePteModifier(MdiagSurf *pSurface, UINT32 targetPteKind)
        : m_pSurface(pSurface), m_OriPteKind(pSurface->GetPteKind()), m_TargetPteKind(targetPteKind)
    {
    }

    RC ChangeTo()
    {
        RC rc;

        CHECK_RC(m_pSurface->ChangePteKind(m_TargetPteKind));

        return rc;
    }

    RC Restore()
    {
        RC rc;

        CHECK_RC(m_pSurface->ChangePteKind(m_OriPteKind));

        return rc;
    }

private:
    MdiagSurf *m_pSurface;
    UINT32 m_OriPteKind;
    UINT32 m_TargetPteKind;
};

class SurfaceProtAttrModifier
{
public:
    SurfaceProtAttrModifier(MdiagSurf *pSurface, Memory::Protect protect)
        : m_pSurface(pSurface), m_OriProtAttr(pSurface->GetProtect()), m_TargetProtAttr(protect)
    {
    }

    RC ChangeTo()
    {
        RC rc;

        m_pSurface->SetProtect(m_TargetProtAttr);
        CHECK_RC(m_pSurface->ReMapPhysMemory());

        return rc;
    }

    RC Restore()
    {
        RC rc;

        m_pSurface->SetProtect(m_OriProtAttr);
        CHECK_RC(m_pSurface->ReMapPhysMemory());

        return rc;
    }

private:
    MdiagSurf *m_pSurface;
    Memory::Protect m_OriProtAttr;
    Memory::Protect m_TargetProtAttr;
};

#endif // INCLUDED_SURFUTIL_H
