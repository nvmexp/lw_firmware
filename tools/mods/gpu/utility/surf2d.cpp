/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  surf2d.cpp
 * @brief Abstraction representing a two-dimensional surface.
 */

#include "surf2d.h"
#include "blocklin.h"
#include "class/cl0071.h" // LW01_MEMORY_SYSTEM_OS_DESCRIPTOR
#include "class/cl00f8.h" // LW_MEMORY_FABRIC
#include "class/cl5070.h" // LW50_DISPLAY
#include "class/cl50a0.h" // LW50_MEMORY_VIRTUAL
#include "class/cl8270.h" // G82_DISPLAY
#include "class/cl90f1.h" // FERMI_VASPACE_A
#include "class/cle3f1.h" // TEGRA_VASPACE_A
#include "core/include/cpu.h"
#include "core/include/crc.h"
#include "core/include/deprecat.h"
#include "core/include/display.h"
#include "core/include/fileholder.h"
#include "core/include/imagefil.h"
#include "core/include/jscript.h"
#include "core/include/memcheck.h"
#include "core/include/mgrmgr.h"
#include "core/include/platform.h"
#include "core/include/script.h"
#include "core/include/utility.h"
#include "device/interface/lwlink/lwlfla.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/protmanager.h"
#include "gpu/js_gpudv.h"
#include "gpuutils.h"
#include "random.h"
#include "surffmt.h"
#include "surfrdwr.h"

#if defined(TEGRA_MODS)
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"
#endif

#include <stdio.h>
#include <math.h>
#include <set>
#include <limits.h>

// ----------------------------------------------------------------------------
// Surface allocation and usage looks like:
//
// --------------------
// |   Hidden Alloc   |
// |------------------|  <---------------------------------  (CtxDmaOffsetGpu)
// |   Extra Alloc    |                                   |
// |------------------|  <--------                        |
// |                  |          |                   GPU Mapped
// |   Surface Data   |  CPU Mapped via Map*              |
// |                  |          |                        |
// |------------------|  <---------------------------------
// |    CDE Padding   |
// |------------------|  <---------------------------------  (CtxDmaOffsetCde)
// |  CDE Horz Data   |                                   |
// |------------------|                      Gpu and CPU Mapped via MapCde
// |  CDE Vert Data   |                                   |
// --------------------  <---------------------------------
//
// ----------------------------------------------------------------------------
using Utility::AlignUp;
using Utility::AlignDown;

enum rgbComps { redComp, greenComp, blueComp,  NumComps };

namespace
{
    const UINT32 CdeAllocAlignment = 256;
    const UINT64 CdePadding = 20 << 10;

    bool g_DebugFillOnAlloc = false;
    UINT32 g_DebugFillWord = 0;
#ifdef SIM_BUILD
    map<string, INT32> g_IndividualLocationOverride;
#endif
}

/* static */ INT32 Surface2D::s_LocationOverride = Surface2D::NO_LOCATION_OVERRIDE;
/* static */ INT32 Surface2D::s_LocationOverrideSysmem = Surface2D::NO_LOCATION_OVERRIDE;

struct Surface2D::MapContext
{
    bool   Mapped;
    UINT32 MappedSubdev;
    UINT32 MappedPart;
    UINT64 MappedOffset;
    UINT64 MappedSize;
    LwRm  *MappedClient;
};

static UINT32 GetWriteValue(UINT32 BytesPerPixel, UINT32 NumWritten,
                                UINT32 PatternLength, UINT32* pPattern);

//-----------------------------------------------------------------------------
Surface2D::Surface2D()
{
    ClearMembers();
}

//-----------------------------------------------------------------------------
Surface2D::~Surface2D()
{
    if (!m_DmaBufferAlloc)
        Free();
}

//-----------------------------------------------------------------------------
static UINT32 FloorLog2(UINT32 n)
{
    int i = 1;
    while ((n >> i) > 0)
    {
        i++;
    }
    return i-1;
}

void Surface2D::SetExtSysMemFlags(UINT32 AllocFlags, UINT32 CtxDmaFlags)
{
    m_bUseExtSysMemFlags = true;
    m_ExtSysMemAllocFlags = AllocFlags;
    m_ExtSysMemCtxDmaFlags = CtxDmaFlags;
}

void Surface2D::SetPeerMappingFlags(UINT32 PeerMappingFlags)
{
    const UINT32 mask = DRF_SHIFTMASK(LWOS46_FLAGS_P2P);
    MASSERT((PeerMappingFlags & ~mask) == 0);
    m_bUsePeerMappingFlags = true;
    m_PeerMappingFlags = (PeerMappingFlags & mask);
}

void Surface2D::SetDefaultVidHeapAttr(UINT32 Attr)
{
    MASSERT(!m_DefClientData.m_hMem);
    m_VidHeapAttr = Attr;
}

void Surface2D::SetDefaultVidHeapAttr2(UINT32 Attr2)
{
    MASSERT(!m_DefClientData.m_hMem);
    m_VidHeapAttr2 = Attr2;
}

void Surface2D::SetCtxDmaHandleGpu(LwRm::Handle dmaHandle, LwRm *pLwRm)
{
    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    if (!pClientData)
        return;

    MASSERT(pClientData->m_hMem);
    pClientData->m_hCtxDmaGpu = dmaHandle;
}

void Surface2D::SetCtxDmaOffsetGpu(UINT64 dmaOffset, LwRm *pLwRm)
{
    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    if (!pClientData)
        return;

    MASSERT(pClientData->m_hMem);
    pClientData->m_CtxDmaOffsetGpu = dmaOffset;
}

//-----------------------------------------------------------------------------
RC Surface2D::Alloc(GpuDevice *gpudev, LwRm *pLwRm)
{
    RC rc;

    MASSERT(gpudev);
    m_pGpuDev = gpudev;

    // Shouldn't have allocated yet...
    if (m_DefClientData.m_hMem)
    {
       Printf(Tee::PriHigh, "Surface2D already allocated!\n"
              " Please free memory before allocating.\n");
       MASSERT(!"Generic assertion failure<Refer to previous message>.");
       return RC::SOFTWARE_ERROR;
    }

    if ((pLwRm != 0) && (pLwRm != m_pLwRm))
    {
        m_pLwRm = pLwRm;
    }

    GpuSubdevice* const pSubdev = GetGpuSubdev(0);

    const Memory::Location requestedLocation = m_Location;

    // For VPR surfaces, the surface must be in FB
    // The Video-Protected Region is managed by a special FB heap in RM
    if (m_bAllocInUpr || m_bVideoProtected  || m_bAcrRegion1 || m_bAcrRegion2)
    {
        m_Location = Memory::Fb;
        m_SplitLocation = Memory::Fb;
    }

    m_Location = GetActualLocation(m_Location, pSubdev);
    m_SplitLocation = GetActualLocation(m_SplitLocation, pSubdev);
    m_Location = CheckForIndividualLocationOverride(m_Location, pSubdev);
    m_SplitLocation = CheckForIndividualLocationOverride(m_SplitLocation,
        pSubdev);

    if (pSubdev->IsSOC() && m_RouteDispRM && m_Displayable &&
        m_Location == Memory::Coherent &&
        pSubdev->HasFeature(Device::GPUSUB_HAS_NONCOHERENT_SYSMEM))
    {
        m_Location = Memory::NonCoherent;
    }

    bool bSupportGpuCache = false;
    CHECK_RC(GpuUtility::GetGpuCacheExist(m_pGpuDev, &bSupportGpuCache, m_pLwRm));

    if (Platform::GetSimulationMode() == Platform::Amodel)
    {
        // According to bug 973546, XQ mentioned that
        // Amodel doesn't support GPU cache is harmless.
        // MODS should ignore this error on Amodel
        // and passes along the correct PTE kind.
        // Amodel itself doesn't care about this,
        // but other simulators build upon amodel,
        // such as perfsim, might very well model it.
        bSupportGpuCache = true;
    }

    // Egm support
    if (m_bEgmOn && HasPhysical())
    {
        CommitEgmAttr();
    }

    // Don't try to map surfaces to GPU if there is no GPU on CheetAh
    if (GetGpuSubdev(0)->IsFakeTegraGpu())
    {
        m_VASpace = TegraVASpace;
    }

    // Let RM to decide the cache for fb surfaces and coh/ncoh sysmem surfaces
    // that are with default cache attribute for sim mods
    const bool isSim = (Xp::GetOperatingSystem() == Xp::OS_LINUXSIM ||
                        Xp::GetOperatingSystem() == Xp::OS_WINSIM);
    if (isSim)
    {
        if (m_GpuCacheMode == GpuCacheDefault)
        {
            // Force GPU cache to be OFF for sysmem surfaces when it's location is Memory::NonCoherent, layout is Pitch and
            // when cache mode is GpuCacheDefault. If cache mode is mentioned externally, this condition will not be exelwted.
            if ((m_Location == Memory::NonCoherent) && (m_Layout == Pitch))
            {
                m_GpuCacheMode = GpuCacheOff;
                Printf(Tee::PriDebug, "Forcing GPU cache OFF for Surface %s.\n",
                    GetName().c_str());
            }
            else
            {
                //RM will decide the default cache value.
                Printf(Tee::PriDebug, "Surface %s use RM default cache value.\n",
                        GetName().c_str());
            }
        }
    }
    else
    {
        // If the regular FB surface was forced to non-coherent sysmem on SOC/iGPU,
        // make it GPU cacheable by default.
        // Note: if the surface is accessible outside of the GPU, e.g. is
        // displayable on CheetAh, the owner has to explicitly decompress
        // the surface (if compressed) and flush L2 with writeback of dirty.
        if ((m_Location != Memory::Fb)
            && ((requestedLocation == Memory::Fb) ||
                   (requestedLocation == Memory::Optimal))
            && (pSubdev->IsSOC() ||
                pSubdev->HasFeature(Device::GPUSUB_IS_STANDALONE_TEGRA_GPU))
            && (m_GpuCacheMode == GpuCacheDefault)
            && bSupportGpuCache
            && (m_VASpace != TegraVASpace)
            )
        {
            m_GpuCacheMode = GpuCacheOn;
        }

        // Force GPU cache to be off for sysmem surfaces on SOC/iGPU
        if ((pSubdev->IsSOC() ||
             pSubdev->HasFeature(Device::GPUSUB_IS_STANDALONE_TEGRA_GPU)) &&
            (m_GpuCacheMode == GpuCacheDefault) &&
            ( (requestedLocation == Memory::Coherent) ||
              (requestedLocation == Memory::NonCoherent)))
        {
            m_GpuCacheMode = GpuCacheOff;
        }

        // Force GPU cache to be off for non-GPU surfaces on CheetAh
        if (m_VASpace == TegraVASpace)
        {
            m_GpuCacheMode = GpuCacheOff;
        }
    }

    if (requestedLocation != m_Location)
    {
        Printf(Tee::PriDebug, "Location override, from %s to %s.\n",
               Memory::GetMemoryLocationName(requestedLocation),
               Memory::GetMemoryLocationName(GetLocation()));
    }

    if (m_AddressModel == Memory::OptimalAddress)
    {
        if (pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_PAGING))
            m_AddressModel = Memory::Paging;
        else
            m_AddressModel = Memory::Segmentation;
    }

    // Override layout with what was set with SetDefaultVidHeapAttr
    if (Pitch == m_Layout)
    {
        m_Layout = static_cast<Layout>(
            DRF_VAL(OS32, _ATTR, _FORMAT, m_VidHeapAttr));
    }

    if (Tiled == m_Layout)
    {
        m_Tiled = true;
    }

    // Sanity check
    MASSERT(!m_DefClientData.m_hVirtMem);
    MASSERT(!m_DefClientData.m_hTegraVirtMem);
    MASSERT(!m_DefClientData.m_hCtxDmaGpu);
    MASSERT(!m_DefClientData.m_hCtxDmaIso);
    MASSERT(!m_Address);

    // The address translation code in surface2d only supports up to
    //  4k x 4k for Swizzled layout. Pitch and Blocklinear layout surfaces
    //  have no translation so they may be arbitrarily large.
    // Zero size surfaces are not alowed
    if (((m_Width > 4096) || (m_Height > 4096) || (m_Width == 0) || (m_Height == 0))
        && (m_Layout == Swizzled))
    {
        Printf(Tee::PriHigh, "Surface2D cannot allocate!\n"
               " Width and Height must be between 1 and 4096 for Swizzled layout.\n");
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return RC::SOFTWARE_ERROR;
    }
    if (m_ColorFormat == ColorUtils::LWFMT_NONE)
    {
        Printf(Tee::PriHigh, "Surface2D cannot allocate!\n"
               " ColorFormat must not be ColorUtils::LWFMT_NONE.\n");
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return RC::SOFTWARE_ERROR;
    }

    // In order to utilize the CDE region, the GPU must run a shader to
    // copy the compbits from the global store to the CDE region of the
    // surface (the shader also performs data colwersion).  This shader
    // needs to treat the surface as pitch linear.  RM cannot create a
    // pitch linear GPU mapping onto a surface with a physical allocation
    // of block linear compressed and therefore when RM is used it is
    // necessary to use a completely seperate surface for storing CDE data
    if (m_bCdeAlloc && !Platform::UsesLwgpuDriver())
    {
        Printf(Tee::PriHigh, "Surface2D cannot allocate!\n"
               " CDE allocation only supported on SOC with linux kernel "
               "drivers.\n");
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return RC::SOFTWARE_ERROR;
    }

    if ((m_Width == 0) || (m_Height == 0))
    {
        if (m_ArrayPitch == 0)
        {
            Printf(Tee::PriHigh, "Surface2D cannot allocate!\n"
                   " (Width and Height) or ArrayPitch must be nonzero.\n");
            MASSERT(!"Generic assertion failure<Refer to previous message>.");
            return RC::SOFTWARE_ERROR;
        }

        if (m_bCdeAlloc)
        {
            Printf(Tee::PriHigh, "Surface2D cannot allocate!\n"
                   " Width and Height must be non-zero with CdeAlloc.\n");
            MASSERT(!"Generic assertion failure<Refer to previous message>.");
            return RC::SOFTWARE_ERROR;
        }
    }
    if ((m_Layout == Swizzled) && (m_Depth != 1))
    {
        Printf(Tee::PriHigh, "Surface2D cannot allocate!\n"
              " Depth must be 1 for Swizzled layout.\n");
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return RC::SOFTWARE_ERROR;
    }
    if ((m_Layout == Swizzled) && (m_ArraySize != 1))
    {
        Printf(Tee::PriHigh, "Surface2D cannot allocate!\n"
              " ArraySize must be 1 for Swizzled layout.\n");
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return RC::SOFTWARE_ERROR;
    }
    if ((m_Depth == 0) || (m_ArraySize == 0))
    {
        Printf(Tee::PriHigh, "Surface2D cannot allocate!\n"
              " Depth and ArraySize must be nonzero.\n");
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return RC::SOFTWARE_ERROR;
    }

    if ((m_AddressModel != Memory::Paging) && m_Split)
    {
        Printf(Tee::PriHigh, "Split surface rendering must be peformed using the paging memory model\n");
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    if (m_Split && ! m_pLwRm->IsClassSupported(LW50_MEMORY_VIRTUAL, GetGpuDev()))
    {
        Printf(Tee::PriHigh, "Split surface rendering requires support for virtual memory\n");
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    if (Tiled == m_Layout && m_VASpace != TegraVASpace)
    {
        Printf(Tee::PriHigh, "Tiled surfaces only supported on SOC\n");
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }
    if (m_Split)
    {
        if (m_HiddenAllocSize)
        {
            Printf(Tee::PriHigh, "Hidden allocation size not supported with split surfaces\n");
            MASSERT(!"Generic assertion failure<Refer to previous message>.");
            return RC::CANNOT_ALLOCATE_MEMORY;
        }
        if (m_ExtraAllocSize)
        {
            Printf(Tee::PriHigh, "Extra allocation size not supported with split surfaces\n");
            MASSERT(!"Generic assertion failure<Refer to previous message>.");
            return RC::CANNOT_ALLOCATE_MEMORY;
        }
        if (m_bCdeAlloc)
        {
            Printf(Tee::PriHigh, "CDE allocation not supported with split surfaces\n");
            MASSERT(!"Generic assertion failure<Refer to previous message>.");
            return RC::CANNOT_ALLOCATE_MEMORY;
        }
        if (m_VASpace == TegraVASpace || m_VASpace == GPUAndTegraVASpace)
        {
            Printf(Tee::PriHigh, "Split surfaces are not supported for SOC GPUs\n");
            return RC::CANNOT_ALLOCATE_MEMORY;
        }
    }

#ifdef LW_VERIF_FEATURES
    if (m_LoopBack)
    {
        if (m_Location != Memory::Fb)
        {
            Printf(Tee::PriError, "Loopback mode is supported only on vidmem surfaces\n");
            MASSERT(!"Generic assertion failure<Refer to previous message>.");
            return RC::CANNOT_ALLOCATE_MEMORY;
        }

        if (m_AddressModel != Memory::Paging)
        {
            Printf(Tee::PriError, "Loopback mode is supported only with paging\n");
            MASSERT(!"Generic assertion failure<Refer to previous message>.");
            return RC::CANNOT_ALLOCATE_MEMORY;
        }
    }
#endif

    // Check if CheetAh VA space is available
    if (m_VASpace == TegraVASpace || m_VASpace == GPUAndTegraVASpace)
    {
        if (!m_pGpuDev->GetVASpace(TEGRA_VASPACE_A, m_pLwRm) &&
            Platform::HasClientSideResman())
        {
            Printf(Tee::PriError, "CheetAh VA space is not available\n");
            MASSERT(!"Generic assertion failure<Refer to previous message>.");
            return RC::CANNOT_ALLOCATE_MEMORY;
        }
    }

    // Displayable surfaces on CheetAh with big GPU must also have
    // CheetAh VA space allocation.
    if (m_Displayable && (m_VASpace == GPUVASpace || m_VASpace == DefaultVASpace))
    {
        if (m_pGpuDev->GetVASpace(FERMI_VASPACE_A, m_pLwRm) &&
            m_pGpuDev->GetVASpace(TEGRA_VASPACE_A, m_pLwRm))
        {
            Printf(Tee::PriLow, "Forcing displayable surface to CheetAh VA space\n");
            m_VASpace = GPUAndTegraVASpace;
        }
    }

    if (m_Displayable && (m_Type == LWOS32_TYPE_IMAGE))
    {
        Printf(Tee::PriLow, "Overriding Surface2D type to make it displayable\n");
        m_Type = LWOS32_TYPE_PRIMARY;
    }

    if ((m_Layout == BlockLinear) && !pSubdev->HasFeature(Device::GPUSUB_HAS_BLOCK_LINEAR))
    {
        Printf(Tee::PriError, "Block linear memory is not supported\n");
        MASSERT(!"Block linear memory is not supported.");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    if (m_FLAPeerMapping && (GetLocation() == Memory::Fb))
    {
        if ((gpudev->GetNumSubdevices() > 1))
        {
            Printf(Tee::PriError, "FLA peer mapping not supported on SLI devices\n");
            MASSERT(!"FLA peer mapping not supported on SLI devices.");
            return RC::CANNOT_ALLOCATE_MEMORY;
        }

        if (GetPageSize() != 2048)
        {
            Printf(Tee::PriError,
                   "FLA peer mapping requires memory be allocated with 2MB pages\n");
            MASSERT(!"FLA peer mapping requires memory be allocated with 2MB pages");
            return RC::CANNOT_ALLOCATE_MEMORY;
        }
    }

    ComputeParams();

    m_VaVidHeapFlags = 0;
    m_PaVidHeapFlags = 0;
    m_VidHeapAlign = 0;
    m_VidHeapCoverage = 0;
    m_AllocSysMemFlags = 0;
    m_MapMemoryDmaFlags = 0;

    UINT32 sysmemMaxPageSizeKB =
        static_cast<UINT32>(Platform::GetPageSize()) >> 10;

    // In dGPU, don't allow big paged sysmem allocations at all.
    // In CheetAh, don't allow big paged sysmem allocations with carveout
    if ((m_Location != Memory::Fb) &&
        Platform::HasClientSideResman() &&
        m_PageSize > sysmemMaxPageSizeKB &&
        pSubdev->HasFeature(Device::GPUSUB_HAS_FB_HEAP))
    {
        Printf(Tee::PriWarn,
            "Forcing page size to %u KB for sysmem on dGPU or SOC GPU with carvout FB!\n",
            sysmemMaxPageSizeKB);
        m_PageSize = sysmemMaxPageSizeKB;
    }

    // For CheetAh-only surfaces set 4KB page size explicitly.
    // CheetAh-only surfaces are expected to only be mapped in SMMU, which only supports
    // small (4KB) pages.  At the moment, in practice rmapi_tegra maps these surfaces
    // in GMMU as well, but we never use them on the GPU.
    if (m_PageSize == 0 && m_VASpace == TegraVASpace)
    {
        m_PageSize = 4;
    }

    m_bUseVidHeapAlloc = true;

    if (m_bUseVidHeapAlloc || GetUseVirtMem())
    {
        CommitAlignment();

        CommitDepthAttr();
        CommitTiledAttr();
        SetCompressAttr(m_Location, &m_VidHeapAttr);
        CommitAASamplesAttr();
        CommitFormatAttr();
        CommitFormatOverrideAttr();
        CommitPageSizeAttr();
        CommitPhysicalityAttr();
        CommitComponentPackingAttr(HasC24Scanout());
        CommitZLwllAttr();

        if (GetGpuCacheMode() == GpuCacheOn)
        {
            if (!bSupportGpuCache)
            {
                Printf(Tee::PriHigh, "Surface2D cannot allocate: "
                    " no GPU cache exists\n");
                MASSERT(!"Generic assertion failure<Refer to previous message>.");
                return RC::SOFTWARE_ERROR;
            }
            else if (GetLocation() == Memory::Coherent && !pSubdev->IsSOC())
            {
                Printf(Tee::PriHigh, "Warning:  GPU caching used with coherent system memory\n");
            }
        }

        // If the GPU cache value hasn't been set to ON or OFF already
        // and the buffer is in framebuffer memory and the current GPU supports caching,
        // set the GPU cache mode to ON.  GpuCache is only supported on FB memory,
        // enabling caching for system memory will cause a reflected mapping when
        // mapped to the CPU which is forbidden
        //
        // SIM MODS lwrrently relies on reflected mappings so it must default
        // cache on for all but coherent memory
        else if ((GetGpuCacheMode() == GpuCacheDefault) && bSupportGpuCache &&
                 ((GetLocation() == Memory::Fb) ||
                  (isSim && (GetLocation() != Memory::Coherent))))
        {
            SetGpuCacheMode(GpuCacheOn);
        }

        if (GetP2PGpuCacheMode() == GpuCacheOn)
        {
            if (!bSupportGpuCache)
            {
                Printf(Tee::PriHigh, "Surface2D cannot allocate: "
                    " no GPU cache exists\n");
                MASSERT(!"Generic assertion failure<Refer to previous message>.");
                return RC::SOFTWARE_ERROR;
            }
            else if (GetLocation() == Memory::Coherent)
            {
                Printf(Tee::PriHigh, "Surface2D cannot allocate: "
                    " can't use GPU caching with coherent system memory\n");
                MASSERT(!"Generic assertion failure<Refer to previous message>.");
                return RC::SOFTWARE_ERROR;
            }
        }

        if (GetSplitGpuCacheMode() == GpuCacheOn)
        {
            if (!bSupportGpuCache)
            {
                Printf(Tee::PriHigh, "Surface2D cannot allocate: "
                    " no GPU cache exists\n");
                MASSERT(!"Generic assertion failure<Refer to previous message>.");
                return RC::SOFTWARE_ERROR;
            }
            else if ((GetSplitLocation() == Memory::Coherent ||
                      GetSplitLocation() == Memory::CachedNonCoherent)
                     && !pSubdev->IsSOC())
            {
                Printf(Tee::PriHigh, "Warning:  GPU caching used with coherent"
                                     " or cached system memory\n");
            }
        }

        CommitZbcAttr();
        CommitGpuCacheAttr();

        CHECK_RC(CommitFbSpeed());

        m_SplitVidHeapAttr = m_VidHeapAttr;
        m_SplitVidHeapAttr2 = m_VidHeapAttr2;

#ifdef LW_VERIF_FEATURES
        if (m_SplitPteKind >= 0)
        {
            m_SplitVidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _FORMAT_OVERRIDE, _ON,
                                             m_SplitVidHeapAttr);
        }
#endif

        SetCompressAttr(m_SplitLocation, &m_SplitVidHeapAttr);

        SetLocationAttr(m_Location, &m_VidHeapAttr);
        SetLocationAttr(m_SplitLocation, &m_SplitVidHeapAttr);

        CommitSplitGpuCacheAttr();
        CommitGpuSmmuMode();

        m_VidHeapCoverage |= GetComprCoverage();

        m_PaVidHeapFlags |= GetAlignmentFlags();
        m_PaVidHeapFlags |= GetTurboCipherFlags();
        m_PaVidHeapFlags |= GetScanoutFlags();
        m_VaVidHeapFlags = m_PaVidHeapFlags | GetVaReverseFlags(true);
        m_PaVidHeapFlags |= GetPaReverseFlags(true);
        m_PaVidHeapFlags |= GetVideoProtectedFlags();
        m_PaVidHeapFlags |= GetAcrRegion1Flags();
        m_PaVidHeapFlags |= GetAcrRegion2Flags();
    }

    if (HasPhysical())
    {
        if (m_bAllocInUpr || m_bVideoProtected || m_bAcrRegion1 || m_bAcrRegion2)
        {
            if (m_ExternalPhysMem != 0)
            {
                Printf(Tee::PriError, "Error: Can't allocate external memory as UPR/VPR/WPR.\n");
                return RC::ILWALID_ARGUMENT;
            }
            else if (((m_bAllocInUpr ? 1 : 0) +
                    (m_bVideoProtected ? 1 : 0) +
                    (m_bAcrRegion1 ? 1 : 0) +
                    (m_bAcrRegion2 ? 1 : 0)) > 1)
            {
                Printf(Tee::PriError, "Error: Can't allocate memory in multiple unprotected/protected regions (only one of UPR, VPR, ACR1, and ACR2.\n");
                return RC::ILWALID_ARGUMENT;
            }

            ProtectedRegionManager *protectedRegionManager;
            CHECK_RC(gpudev->GetProtectedRegionManager(&protectedRegionManager));
            MASSERT(protectedRegionManager);
            LwRm::Handle physMem = 0;
            PHYSADDR physAddr = 0;

            if (m_bAllocInUpr &&
                protectedRegionManager->IsUprRegionAllocated())
            {
                CHECK_RC(protectedRegionManager->AllolwprMem(m_Size,
                        m_Type,
                        m_PaVidHeapFlags,
                        m_VidHeapAttr,
                        m_VidHeapAttr2,
                        &physMem,
                        &physAddr));
            }

            if (m_bVideoProtected &&
                protectedRegionManager->IsVprRegionAllocated())
            {
                CHECK_RC(protectedRegionManager->AllocVprMem(m_Size,
                        m_Type,
                        m_PaVidHeapFlags,
                        m_VidHeapAttr,
                        m_VidHeapAttr2,
                        &physMem,
                        &physAddr));
            }

            if (m_bAcrRegion1 &&
                protectedRegionManager->IsAcr1RegionAllocated())
            {
                CHECK_RC(protectedRegionManager->AllocAcr1Mem(m_Size,
                        m_Type,
                        m_PaVidHeapFlags,
                        m_VidHeapAttr,
                        m_VidHeapAttr2,
                        &physMem,
                        &physAddr));
            }

            if (m_bAcrRegion2 &&
                protectedRegionManager->IsAcr2RegionAllocated())
            {
                CHECK_RC(protectedRegionManager->AllocAcr2Mem(m_Size,
                        m_Type,
                        m_PaVidHeapFlags,
                        m_VidHeapAttr,
                        m_VidHeapAttr2,
                        &physMem,
                        &physAddr));
            }

            if (physMem != 0)
            {
                SetExternalPhysMem(physMem, physAddr);
            }
        }

        // Allocate physical memory for the surface.
        if (m_ExternalPhysMem != 0)
        {
            m_DefClientData.m_hMem = m_ExternalPhysMem;
            m_SplitHalfSize = m_Size;
        }
        else
        {
            CHECK_RC(AllocPhysMemory());
        }
    }

    // Size is not set until physical memory is allocated so this check cannot be done
    // until afterwards.  By default FLA page size is 512MB, to use 512MB pages the allocation
    // must be a multiple of 512MB
    if (m_FLAPeerMapping && (GetLocation() == Memory::Fb) &&
        ((m_FLAPageSizeKb == 0) || (m_FLAPageSizeKb == 524288)))
    {
        // FLA requires that the physical allocation use 2MB pages, enforced above
        const UINT64 fullAllocSize = ALIGN_UP(GetAllocSize(), 2_MB);

        if (fullAllocSize % 512_MB)
        {
            // Only print a warning, the error will occur when FLA mapping is actually attempted
            // Note that this limitation is only in the MODS surface 2D class as we dont support
            // multiple peer mappings to the same surface.
            //
            // In the real world multiple mappings is how this would be handled.  E.g. for a 768MB
            // surface, 512MB could be mapped with a 512MB page and the remaining 256MB would be
            // mapped using 2MB FLA pages
            Printf(Tee::PriWarn,
                   "Using 512MB FLA pages requires the physical allocation be a multiple of 512MB,"
                   " FLA mapping will fail\n");           
        }
    }

    // Obtain Z-lwll region
    if (m_Type == LWOS32_TYPE_DEPTH && HasPhysical() &&
        (m_ExternalPhysMem == 0))
    {
        LW0041_CTRL_GET_SURFACE_ZLWLL_ID_PARAMS ZlwllIdParams = {0};
        CHECK_RC(m_pLwRm->Control(
            m_DefClientData.m_hMem,
            LW0041_CTRL_CMD_GET_SURFACE_ZLWLL_ID,
            &ZlwllIdParams, sizeof(ZlwllIdParams)));
        m_ZLwllRegion = ZlwllIdParams.zlwllId;
    }

    if (HasVirtual())
    {
        CHECK_RC(MapPhysMemory(m_pLwRm));
    }

    PrintParams();

    // Debug fill maps entire surface, so we detect whether the entire surface
    // can be mapped to avoid failing MODS in PVS.  This typically happens
    // when running GpuTrace test which has huge surfaces, which can't be
    // filled in one shot.  The debug fill lwrrently uses mapping to fill
    // the surface and requires the entire surface to be mapped.
    const auto CanMapWholeSurface = [&]()
    {
        // Sysmem surfaces that don't require BAR1 mapping
        // can always be mapped whole regardless of their size.
        // This is not really true on 32-bit systems, but we ignore that...
        if (m_Location != Memory::Fb &&
            m_Layout == Pitch &&
            ! m_Compressed)
        {
            return true;
        }

        // If a surface requires BAR1 mapping, see if the surface will fit
        // in BAR1.  We estimate that we can map surfaces up to 3/4th of BAR1
        // size.
        const auto allocSize = GetAllocSize();
        const auto bar1Size  = pSubdev->FbApertureSize();
        return allocSize * 4u <= bar1Size * 3u;
    };

    // As a nifty debugging feature, pre-fill the surface with a fill pattern
    // This pattern will differ between gpugen and gputest. Should a test not
    // fill its entire golden surface, a mismatch failure will be reported.
    if (g_DebugFillOnAlloc && CanMapWholeSurface() && HasMap())
    {
        for (UINT32 subidx = 0;
             subidx < gpudev->GetNumSubdevices(); ++subidx)
        {
            CHECK_RC(Fill(g_DebugFillWord, subidx));
        }
    }

    return OK;
}

//-----------------------------------------------------------------------------
RC Surface2D::AllocGhost(LwRm::Handle hMem, GpuDevice* pGpuDev, LwRm* pLwRm)
{
    MASSERT(pGpuDev);

    if (m_DefClientData.m_hMem)
    {
        Printf(Tee::PriHigh, "Surface2D already allocated!\n"
                             " Please free memory before allocating.\n");
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return RC::SOFTWARE_ERROR;
    }

    if ((pLwRm != 0) && (pLwRm != m_pLwRm))
    {
        m_pLwRm = pLwRm;
    }

    m_pGpuDev = pGpuDev;

    m_Location = GetActualLocation(m_Location, GetGpuSubdev(0));
    m_Location = CheckForIndividualLocationOverride(m_Location,
        GetGpuSubdev(0));

    GpuSubdevice* const pSubdev = GetGpuSubdev(0);

    if (m_AddressModel == Memory::OptimalAddress)
    {
        if (pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_PAGING))
            m_AddressModel = Memory::Paging;
        else
            m_AddressModel = Memory::Segmentation;
    }
    if (m_AddressModel != Memory::Paging)
    {
        Printf(Tee::PriHigh, "Segmentation memory model is not supported!\n");
        return RC::SOFTWARE_ERROR;
    }

    ComputeParams();

    m_GhostSurface = true;
    m_DefClientData.m_hMem = hMem;

    const UINT32 PagDmaFlags =
        GetAccessFlagsPag()
        | GetCacheSnoopFlagsPag()
        | GetShaderAccessFlags()
        | GetPageSizeFlagsPag();

    RC rc;
    CHECK_RC(MapPhysMemToGpu(PagDmaFlags, m_pLwRm));
    return OK;
}

//-----------------------------------------------------------------------------
//! Determine if explicit virtual allocation can and should be used.
//!
bool Surface2D::GetUseVirtMem() const
{
    return VirtMemAllowed() && VirtMemNeeded();
}

//-----------------------------------------------------------------------------
//! Returns VA space object handle for the default or primary VA space.
//! This is typically used if no VA space was specified for the surface
//! or if a single VA space was specified.
//!
LwRm::Handle Surface2D::GetPrimaryVASpaceHandle() const
{
    if ((Xp::GetOperatingSystem() == Xp::OS_WINDA) ||
        (Xp::GetOperatingSystem() == Xp::OS_LINDA))
    {
        // The handle is not really used and the VASPACE classes
        // are not supported by amodel.
        return 0;
    }

    switch (m_VASpace)
    {
        case DefaultVASpace:
        case GPUVASpace:
        case GPUAndTegraVASpace:
            MASSERT(m_pGpuDev->GetVASpace(FERMI_VASPACE_A, m_pLwRm));
            return m_DefClientData.m_hGpuVASpace;

        case TegraVASpace:
            return m_pGpuDev->GetVASpace(TEGRA_VASPACE_A, m_pLwRm);

        default:
            MASSERT(!"Unsupported VA space type or combination!");
            break;
    }
    return 0;
}

//-----------------------------------------------------------------------------
//! Returns VA space object handle for a particular VA space.
//! The function accepts only a single VA space as input.
//!
LwRm::Handle Surface2D::GetVASpaceHandle(VASpace vaSpace) const
{
    if (vaSpace == DefaultVASpace)
    {
        return GetPrimaryVASpaceHandle();
    }

    switch (vaSpace)
    {
        case GPUVASpace:
            MASSERT(m_pGpuDev->GetVASpace(FERMI_VASPACE_A, m_pLwRm));
            return m_DefClientData.m_hGpuVASpace;

        case TegraVASpace:
            return m_pGpuDev->GetVASpace(TEGRA_VASPACE_A, m_pLwRm);

        default:
            MASSERT(!"Unsupported VA space type or combination!");
            break;
    }
    return 0;
}

//-----------------------------------------------------------------------------
//! Determine if explicit virtual allocation is supported for this surface.
//!
bool Surface2D::VirtMemAllowed() const
{
    // Virtual allocation is only available in the paging model.
    if (m_AddressModel == Memory::Paging)
    {
        if (m_pLwRm->IsClassSupported(LW50_MEMORY_VIRTUAL, GetGpuDev()))
        {
            return true;
        }

        // Amodel supports virtual allocation but returns false for the
        // above class check.  However, Amodel can also be in a mode that
        // requires a surface's virtual address to match its physical address.
        // In this mode, the easiest way to guarantee that those addresses
        // will match is to let RM handle the virtual allocation via
        // the MapMemoryDma call (which means returning false in this
        // function).  There are cases where explicit virtual allocation is
        // still necessary, as handled below.
        else if (Platform::GetSimulationMode() == Platform::Amodel)
        {
            return true;
        }
    }

    if (m_DefClientData.m_hGpuVASpace)
    {
        MASSERT(!" MultiVA requires paging & VM support");
    }

    return false;
}

//-----------------------------------------------------------------------------
//! Determine if explicit virtual allocation is needed for this surface.
//!
bool Surface2D::VirtMemNeeded() const
{
    // No mappings of any kind mean no virtual memory is needed
    if (m_Specialization == PhysicalOnly)
        return false;

    const bool hasFermiVASpace = m_pGpuDev->GetVASpace(FERMI_VASPACE_A, m_pLwRm) != 0;
    const bool hasTegraVASpace = m_pGpuDev->GetVASpace(TEGRA_VASPACE_A, m_pLwRm) != 0;

    if (GpuPtr()->GetUseVirtualDma())
    {
        return true;
    }
    else if (m_Split)
    {
        return true;
    }
    else if (m_VirtAlignment || m_VidHeapAlign)
    {
        return true;
    }
    else if (m_Specialization != NoSpecialization)
    {
        return true;
    }
    else if (hasTegraVASpace && ! hasFermiVASpace)
    {
        return true;
    }
    else if (m_VASpace == TegraVASpace)
    {
        return true;
    }
    else if (m_IsSparse)
    {
        return true;
    }
    else if (m_DefClientData.m_hGpuVASpace)
    {
        return true;
    }
    else if (m_FLAPeerMapping)
    {
        return true;
    }

    // Normally virtual allocation should be used when there is a virtual
    // address range constraint, because the range can be passed directly
    // to RM.  However, there are some cirlwmstances under which RM can fail
    // virtual allocation with a certain address range, but will pass if
    // using MapMemoryDma with a specific address in the same range (this
    // has to do with alignment and the MapMemoryDma call having information
    // about the physical allocation).  The m_GpuVirtAddrHintUseVirtAlloc flag
    // can be set to false to allow for MapMemoryDma to be used if the only
    // reason for using an explicit virtual allocation was the presense of a
    // virtual address range.
    else if (((m_GpuVirtAddrHint > 0) || (m_GpuVirtAddrHintMax > 0)) &&
        m_GpuVirtAddrHintUseVirtAlloc)
    {
        return true;
    }

    return false;
}

//-----------------------------------------------------------------------------
bool Surface2D::GetUsePitchAlloc() const
{
    return (Pitch == m_Layout || Tiled == m_Layout) &&
        (0 != m_Pitch) &&
        (INT_MAX >= m_Pitch) &&
        !m_ForceSizeAlloc;
}

//-----------------------------------------------------------------------------
void Surface2D::CommitDepthAttr()
{
    // Skip if depth was already set with SetDefaultVidHeapAttr()
    if (DRF_VAL(OS32, _ATTR, _DEPTH, m_VidHeapAttr)
        != LWOS32_ATTR_DEPTH_UNKNOWN)
        return;

    switch (GetBitsPerPixel())
    {
        case 1:
        case 2:
        case 4:
        case 8:
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _DEPTH, _8,
                                        m_VidHeapAttr);
            break;
        case 16:
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _DEPTH, _16,
                                        m_VidHeapAttr);
            break;
        case 24:
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _DEPTH, _24,
                                        m_VidHeapAttr);
            break;
        case 32:
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _DEPTH, _32,
                                        m_VidHeapAttr);
            break;
        case 48:
        case 64:
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _DEPTH, _64,
                                        m_VidHeapAttr);
            break;
        case 128:
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _DEPTH, _128,
                                        m_VidHeapAttr);
            break;
        default:
            MASSERT(!"Invalid BitsPerPixel");
            break;
    }
}

//-----------------------------------------------------------------------------
void Surface2D::CommitTiledAttr()
{
    const bool tilingSupported =
        GetGpuSubdev(0)->HasFeature(Device::GPUSUB_SUPPORTS_TILING);
    if (tilingSupported && m_Tiled)
    {
        m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _TILED, _REQUIRED,
                                    m_VidHeapAttr);
    }
}

//-----------------------------------------------------------------------------
void Surface2D::SetCompressAttr(Memory::Location location, UINT32* pAttr)
{
    if (m_Compressed)
    {
        // Query gpu CAPs to decide whether compression is supported
        // for the surface in sysmem. Warning if not.
        if ((location == Memory::Coherent) || (location == Memory::NonCoherent)
            || (location == Memory::CachedNonCoherent))
        {
            MASSERT(m_pGpuDev);
            if (!m_pGpuDev->CheckFbCapsBit(GpuDevice::SYSMEM_COMPRESSION))
            {
                Printf(Tee::PriHigh, "Warning: Invalid setting. Surface or "
                    "part of surface in system memory do not support Compresssion!\n");

                *pAttr = FLD_SET_DRF(OS32, _ATTR, _COMPR, _NONE, *pAttr);
                return;
            }
        }

        *pAttr = FLD_SET_DRF_NUM(OS32, _ATTR, _COMPR, m_CompressedFlag, *pAttr);

        // For compressed pitch surfaces in generic memory,  there isn't
        // way to guarantee that the surface will be read at the correct
        // alignment for PLC compression.  Therefore, set the DISABLE_PLC_ANY
        // flag if this a pitch surface could be compressed.  (Bug 2032691)
        if ((m_Layout == Pitch) &&
            (GetGpuDev()->GetSubdevice(0)->HasFeature(
                 Device::GPUSUB_SUPPORTS_GENERIC_MEMORY)) &&
            (FLD_TEST_DRF(OS32, _ATTR, _COMPR, _REQUIRED, *pAttr) ||
                FLD_TEST_DRF(OS32, _ATTR, _COMPR, _ANY, *pAttr)))
        {
            *pAttr = FLD_SET_DRF(OS32, _ATTR, _COMPR, _DISABLE_PLC_ANY, *pAttr);
        }

#ifdef LW_VERIF_FEATURES
        *pAttr = FLD_SET_DRF(OS32, _ATTR, _COMPR_COVG, _PROVIDED, *pAttr);
#endif
    }
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetComprCoverage() const
{
#ifdef LW_VERIF_FEATURES
    if (m_Compressed)
    {
        return
            DRF_NUM(OS32, _ALLOC_COMPR_COVG, _MIN, m_ComptagCovMin*LWOS32_ALLOC_COMPR_COVG_SCALE) |
            DRF_NUM(OS32, _ALLOC_COMPR_COVG, _MAX, m_ComptagCovMax*LWOS32_ALLOC_COMPR_COVG_SCALE) |
            DRF_NUM(OS32, _ALLOC_COMPR_COVG, _START, m_ComptagStart*LWOS32_ALLOC_COMPR_COVG_SCALE) |
            DRF_NUM(OS32, _ALLOC_COMPR_COVG, _BITS, m_Comptags);
    }
#endif
    return 0;
}

//-----------------------------------------------------------------------------
void Surface2D::CommitAASamplesAttr()
{
    if (m_AAMode)
    {
        m_VidHeapAttr = FLD_SET_DRF_NUM(OS32, _ATTR, _AA_SAMPLES, m_AAMode, m_VidHeapAttr);
    }
}

//-----------------------------------------------------------------------------
void Surface2D::CommitFormatAttr()
{
    switch (m_Layout)
    {
        case Pitch:
        case Tiled:
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _FORMAT, _PITCH,
                                        m_VidHeapAttr);
            break;
        case Swizzled:
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _FORMAT, _SWIZZLED,
                                        m_VidHeapAttr);
            break;
        case BlockLinear:
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _FORMAT, _BLOCK_LINEAR,
                                        m_VidHeapAttr);
            break;
        default:
            MASSERT(!"Invalid Layout");
            break;
    }
}

//-----------------------------------------------------------------------------
void Surface2D::SetLocationAttr(Memory::Location location, UINT32* pAttr)
{
    if ((Platform::GetSimulationMode() == Platform::Amodel) && 
        (!Platform::IsSelfHosted()) && /* Allow sysmem for cyclops */
        (m_AddressModel == Memory::Paging) &&
        (location != Memory::Fb) && m_AmodelRestrictToFb)
    {
        location = Memory::Fb;
    }

    switch (location)
    {
        case Memory::Optimal:
            // We should have chosen earlier
            MASSERT(!"Setting location Memory::Fb.");
            /* fall through */
        case Memory::Fb:
            *pAttr = FLD_SET_DRF(OS32, _ATTR, _LOCATION, _VIDMEM, *pAttr);
            break;
        case Memory::Coherent:
        case Memory::CachedNonCoherent:
            *pAttr = FLD_SET_DRF(OS32, _ATTR, _LOCATION, _PCI, *pAttr);
            *pAttr = FLD_SET_DRF(OS32, _ATTR, _COHERENCY, _CACHED, *pAttr);
            break;
        case Memory::NonCoherent:
            *pAttr = FLD_SET_DRF(OS32, _ATTR, _COHERENCY, _WRITE_COMBINE,
                                 *pAttr);
            *pAttr = FLD_SET_DRF(OS32, _ATTR, _LOCATION, _PCI, *pAttr);
            break;
        default:
            MASSERT(!"Invalid location");
            break;
    }
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetLocationFlags() const
{
    switch (m_Location)
    {
        case Memory::Coherent:
        case Memory::CachedNonCoherent:
            return DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI)
                 | DRF_DEF(OS02, _FLAGS, _COHERENCY, _CACHED);
        case Memory::NonCoherent:
            return DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI)
                 | DRF_DEF(OS02, _FLAGS, _COHERENCY, _WRITE_COMBINE);
        default:
            MASSERT(!"Invalid Location");
            return 0;
    }
}

//-----------------------------------------------------------------------------
void Surface2D::CommitFormatOverrideAttr()
{
#ifdef LW_VERIF_FEATURES
    if (m_PteKind >= 0)
    {
        m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _FORMAT_OVERRIDE, _ON,
                                    m_VidHeapAttr);
    }
#endif
}

//-----------------------------------------------------------------------------
bool Surface2D::HasC24Scanout() const
{
    return GetGpuSubdev(0)->HasFeature(Device::GPUSUB_HAS_C24_SCANOUT);
}

//-----------------------------------------------------------------------------
void Surface2D::CommitComponentPackingAttr(bool bC24Scanout)
{
    switch (m_ColorFormat)
    {
        case ColorUtils::X8R8G8B8:
        case ColorUtils::X8B8G8R8:
        case ColorUtils::X8RL8GL8BL8:
        case ColorUtils::X8BL8GL8RL8:
            if (bC24Scanout)
                m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _COLOR_PACKING,
                                            _X8R8G8B8, m_VidHeapAttr);
            else
                m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _COLOR_PACKING,
                                            _A8R8G8B8, m_VidHeapAttr);
            break;

        case ColorUtils::Z24S8:
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _Z_TYPE, _FIXED,
                                        m_VidHeapAttr);
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _ZS_PACKING, _Z24S8,
                                        m_VidHeapAttr);
            break;

        case ColorUtils::ZF32:
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _Z_TYPE, _FLOAT,
                                        m_VidHeapAttr);
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _ZS_PACKING, _Z32,
                                        m_VidHeapAttr);
            break;

        case ColorUtils::S8Z24:
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _Z_TYPE, _FIXED,
                                        m_VidHeapAttr);
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _ZS_PACKING, _S8Z24,
                                        m_VidHeapAttr);
            break;

        case ColorUtils::X8Z24:
        case ColorUtils::V8Z24:
        case ColorUtils::X4V4Z24__COV4R4V:
        case ColorUtils::X4V4Z24__COV8R8V:
        case ColorUtils::V8Z24__COV4R12V:
        case ColorUtils::V8Z24__COV8R24V:
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _Z_TYPE, _FIXED,
                                        m_VidHeapAttr);
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _ZS_PACKING, _X8Z24,
                                        m_VidHeapAttr);
            break;

        case ColorUtils::ZF32_X24S8:
        case ColorUtils::ZF32_X16V8X8:
        case ColorUtils::ZF32_X16V8S8:
        case ColorUtils::ZF32_X20V4X8__COV4R4V:
        case ColorUtils::ZF32_X20V4X8__COV8R8V:
        case ColorUtils::ZF32_X20V4S8__COV4R4V:
        case ColorUtils::ZF32_X20V4S8__COV8R8V:
        case ColorUtils::ZF32_X16V8X8__COV4R12V:
        case ColorUtils::ZF32_X16V8S8__COV4R12V:
        case ColorUtils::ZF32_X16V8X8__COV8R24V:
        case ColorUtils::ZF32_X16V8S8__COV8R24V:
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _Z_TYPE, _FLOAT,
                                        m_VidHeapAttr);
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _ZS_PACKING, _Z32_X24S8,
                                        m_VidHeapAttr);
            break;

        case ColorUtils::X8Z24_X16V8S8:
        case ColorUtils::X8Z24_X16V8S8__COV4R12V:
        case ColorUtils::X8Z24_X20V4S8__COV4R4V:
        case ColorUtils::X8Z24_X20V4S8__COV8R8V:
        case ColorUtils::X8Z24_X16V8S8__COV8R24V:
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _Z_TYPE, _FIXED,
                                        m_VidHeapAttr);
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _ZS_PACKING,
                                        _X8Z24_X24S8, m_VidHeapAttr);
            break;

        case ColorUtils::Z16:
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _Z_TYPE, _FIXED,
                                        m_VidHeapAttr);
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _ZS_PACKING, _Z16,
                                        m_VidHeapAttr);
            break;

        case ColorUtils::S8:
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _Z_TYPE, _FIXED,
                                        m_VidHeapAttr);
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _ZS_PACKING, _S8,
                                        m_VidHeapAttr);
            break;

        default:
            break;
    }
}

//-----------------------------------------------------------------------------
void Surface2D::CommitZLwllAttr()
{
    m_VidHeapAttr = FLD_SET_DRF_NUM(OS32, _ATTR, _ZLWLL, m_ZLwllFlag, m_VidHeapAttr);
}

//-----------------------------------------------------------------------------
void Surface2D::CommitZbcAttr()
{
    if (!Platform::UsesLwgpuDriver())
    {
        switch (m_ZbcMode)
        {
            case ZbcDefault:
                m_VidHeapAttr2 = FLD_SET_DRF(OS32, _ATTR2, _ZBC, _DEFAULT,
                                             m_VidHeapAttr2);
                break;

            case ZbcOff:
                m_VidHeapAttr2 = FLD_SET_DRF(OS32, _ATTR2, _ZBC, _PREFER_NO_ZBC,
                                             m_VidHeapAttr2);
                break;

            case ZbcOn:
                m_VidHeapAttr2 = FLD_SET_DRF(OS32, _ATTR2, _ZBC, _PREFER_ZBC,
                                             m_VidHeapAttr2);
                break;
        }
    }
}

//-----------------------------------------------------------------------------
void Surface2D::CommitGpuCacheAttr()
{
    switch (m_GpuCacheMode)
    {
    case GpuCacheDefault:
        m_VidHeapAttr2 = FLD_SET_DRF(OS32, _ATTR2, _GPU_CACHEABLE, _DEFAULT,
                                     m_VidHeapAttr2);
        break;

    case GpuCacheOff:
        m_VidHeapAttr2 = FLD_SET_DRF(OS32, _ATTR2, _GPU_CACHEABLE, _NO,
                                     m_VidHeapAttr2);
        break;

    case GpuCacheOn:
        m_VidHeapAttr2 = FLD_SET_DRF(OS32, _ATTR2, _GPU_CACHEABLE, _YES,
                                     m_VidHeapAttr2);
        break;
    }

    if (m_P2PGpuCacheMode == GpuCacheDefault)
    {
        m_P2PGpuCacheMode = m_GpuCacheMode;
    }

    switch (m_P2PGpuCacheMode)
    {
    case GpuCacheDefault:
        m_VidHeapAttr2 = FLD_SET_DRF(OS32, _ATTR2, _P2P_GPU_CACHEABLE,
                                     _DEFAULT, m_VidHeapAttr2);
        break;

    case GpuCacheOff:
        m_VidHeapAttr2 = FLD_SET_DRF(OS32, _ATTR2, _P2P_GPU_CACHEABLE, _NO,
                                     m_VidHeapAttr2);
        break;

    case GpuCacheOn:
        m_VidHeapAttr2 = FLD_SET_DRF(OS32, _ATTR2, _P2P_GPU_CACHEABLE, _YES,
                                     m_VidHeapAttr2);
        break;
    }
}

//-----------------------------------------------------------------------------
void Surface2D::CommitSplitGpuCacheAttr()
{
    switch (m_SplitGpuCacheMode)
    {
    case GpuCacheDefault:
        // Keep whatever was copied from m_VidHeapAttr2.
        break;

    case GpuCacheOff:
        m_SplitVidHeapAttr2 = FLD_SET_DRF(OS32, _ATTR2, _GPU_CACHEABLE, _NO, m_SplitVidHeapAttr2);
        break;

    case GpuCacheOn:
        m_SplitVidHeapAttr2 = FLD_SET_DRF(OS32, _ATTR2, _GPU_CACHEABLE, _YES, m_SplitVidHeapAttr2);
        break;
    }
}

//-----------------------------------------------------------------------------
void Surface2D::CommitGpuSmmuMode()
{
    m_VidHeapAttr2 = FLD_SET_DRF_NUM(OS32, _ATTR2, _SMMU_ON_GPU, m_GpuSmmuMode, m_VidHeapAttr2);
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetAlignmentFlags() const
{
    UINT32 flags = 0;

    if (m_VidHeapAlign > 0)
    {
        flags |= LWOS32_ALLOC_FLAGS_ALIGNMENT_FORCE;
    }
    if (GetUseVirtMem() || m_AlignHostPage)
    {
        flags |= LWOS32_ALLOC_FLAGS_FORCE_ALIGN_HOST_PAGE;
    }
    return flags;
}

//-----------------------------------------------------------------------------
void Surface2D::CommitAlignment()
{
    m_VidHeapAlign = m_Alignment;

    // TODOWJM: Remove assumptions that LW50 display is only present
    // on G80+
    if (m_Displayable &&
        (GetGpuSubdev(0)->HasFeature(Device::GPUSUB_HAS_ISO_DISPLAY)))
    {
        // G80 displayable surfaces must be aligned to 4KB
        if (m_Alignment < 4096)
            m_VidHeapAlign = 4096;
    }

    // Blocklinear buffers must be aligned to the GOB size.
    if ((m_Layout == BlockLinear) &&
        (Platform::GetSimulationMode() == Platform::Amodel))
    {
        const UINT32 GobAlign = GetGpuDev()->GobWidth() *
            GetGpuDev()->GobHeight();

        if (m_Alignment < GobAlign)
        {
            m_VidHeapAlign = GobAlign;
        }
    }
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetVideoProtectedFlags() const
{
    return m_bVideoProtected
        ? LWOS32_ALLOC_FLAGS_PROTECTED
        : 0;
}

UINT32 Surface2D::GetAcrRegion1Flags() const
{
    return m_bAcrRegion1
        ? LWOS32_ALLOC_FLAGS_WPR1
        : 0;
}
//-----------------------------------------------------------------------------
UINT32 Surface2D::GetAcrRegion2Flags() const
{
    return m_bAcrRegion2
        ? LWOS32_ALLOC_FLAGS_WPR2
        : 0;
}
UINT32 Surface2D::GetTurboCipherFlags() const
{
    return m_TurboCipherEncryption
        ? LWOS32_ALLOC_FLAGS_TURBO_CIPHER_ENCRYPTED
        : 0;
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetScanoutFlags() const
{
    return m_Scanout ? 0 : LWOS32_ALLOC_FLAGS_NO_SCANOUT;
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetAccessFlagsSeg() const
{
    switch (m_Protect)
    {
        case Memory::ReadWrite:
            return DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_WRITE);
        case Memory::Readable:
            return DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_ONLY);
        case Memory::Writeable:
            return DRF_DEF(OS03, _FLAGS, _ACCESS, _WRITE_ONLY);
        default:
            MASSERT(!"Invalid Protect");
            return 0;
    }
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetAccessFlagsPag() const
{
    switch (m_Protect)
    {
        case Memory::ReadWrite:
            return DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE);
        case Memory::Readable:
            return DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_ONLY);
        case Memory::Writeable:
            return DRF_DEF(OS46, _FLAGS, _ACCESS, _WRITE_ONLY);
        default:
            MASSERT(!"Invalid Protect");
            return 0;
    }
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetShaderAccessFlags() const
{
    switch (m_ShaderProtect)
    {
        case Memory::ReadWrite:
            return DRF_DEF(OS46, _FLAGS, _SHADER_ACCESS, _READ_WRITE);
        case Memory::Readable:
            return DRF_DEF(OS46, _FLAGS, _SHADER_ACCESS, _READ_ONLY);
        case Memory::Writeable:
            return DRF_DEF(OS46, _FLAGS, _SHADER_ACCESS, _WRITE_ONLY);
        case Memory::ProtectDefault:
            return DRF_DEF(OS46, _FLAGS, _SHADER_ACCESS, _DEFAULT);
        default:
            MASSERT(!"Invalid Protect");
            return 0;
    }
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetCacheSnoopFlagsSeg() const
{
    if (m_bUseExtSysMemFlags)
    {
        return 0;
    }
    return (m_Location == Memory::Coherent)
        ? DRF_DEF(OS03, _FLAGS, _CACHE_SNOOP, _ENABLE)
        : DRF_DEF(OS03, _FLAGS, _CACHE_SNOOP, _DISABLE);
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetCacheSnoopFlagsPag() const
{
    return (m_Location == Memory::Coherent)
        ? DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _ENABLE)
        : DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _DISABLE);
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetDmaProtocolFlags() const
{
    return 0;
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetExtSysMemCtxDmaFlags() const
{
    return m_bUseExtSysMemFlags
        ? m_ExtSysMemCtxDmaFlags
        : 0;
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetKernelMappingFlags() const
{
    // Notifiers need this, so the RM ISR can write them during SW methods.
    return m_KernelMapping
        ? DRF_DEF(OS03, _FLAGS, _MAPPING, _KERNEL)
        : 0U;
}

//-----------------------------------------------------------------------------
void Surface2D::CommitPhysicalityAttr()
{
    if (m_PhysContig)
        m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _PHYSICALITY, _CONTIGUOUS,
                                    m_VidHeapAttr);
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetPhysicalityFlags() const
{
    return m_PhysContig
        ? DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _CONTIGUOUS)
        : DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS);
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetAddrSpaceFlags() const
{
    return 0;
}

//-----------------------------------------------------------------------------
void Surface2D::CommitPageSizeAttr()
{
    if (m_Split && (m_Location != m_SplitLocation))
    {
        // Split surfaces require to have same page size. Since sysmem
        // only supports 4K page_size. Force both to 4K.
        if (m_PageSize > 4)
        {
            MASSERT(!"Page size has to be 4k for split surface!\n");
        }
        m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _PAGE_SIZE, _4KB,
                                    m_VidHeapAttr);
        return;
    }

    switch (m_PageSize)
    {
        case 0: // _DEFAULT is 0 and it's the default
            break;
        case 4:
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _PAGE_SIZE, _4KB,
                                        m_VidHeapAttr);
            break;
        case 64:
        case 128:
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _PAGE_SIZE, _BIG,
                                        m_VidHeapAttr);
            break;
        case 2048:
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _PAGE_SIZE, _HUGE,
                                        m_VidHeapAttr);
            m_VidHeapAttr2 = FLD_SET_DRF(OS32, _ATTR2, _PAGE_SIZE_HUGE, _2MB,
                                         m_VidHeapAttr2);
            break;
        case 524288:
            m_VidHeapAttr = FLD_SET_DRF(OS32, _ATTR, _PAGE_SIZE, _HUGE,
                                        m_VidHeapAttr);
            m_VidHeapAttr2 = FLD_SET_DRF(OS32, _ATTR2, _PAGE_SIZE_HUGE,
                                         _512MB, m_VidHeapAttr2);
            break;
        default:
            MASSERT(!"Invalid PageSize");
            break;
    }
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetPageSizeFlagsSeg() const
{
    return 0;
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetPageSizeFlagsPag() const
{
    if (m_DualPageSize)
    {
        return DRF_DEF(OS46, _FLAGS, _PAGE_SIZE, _BOTH);
    }

    switch (m_PageSize)
    {
        case 0:
            return DRF_DEF(OS46, _FLAGS, _PAGE_SIZE, _DEFAULT);
        case 4:
            return DRF_DEF(OS46, _FLAGS, _PAGE_SIZE, _4KB);
        case 64:
        case 128:
            return DRF_DEF(OS46, _FLAGS, _PAGE_SIZE, _BIG);
        case 2048:
            return DRF_DEF(OS46, _FLAGS, _PAGE_SIZE, _HUGE);
        case 524288:
            // Deprecated for 512MB PageSize
            return 0;
        default:
            MASSERT(!"Invalid PageSize");
            break;
    }
    return 0;
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetMemAttrsLocationFlags() const
{
    return 0;
}

//-----------------------------------------------------------------------------
// returlwidheapFlag:
//    true: return reverse flag for vidheap interface(VidHeapAllocPitchHeightEx)
//    false: return reverse flag for mapmem interface(MapMemoryDma)
UINT32 Surface2D::GetVaReverseFlags(bool returlwidheapFlag) const
{
#ifdef LW_VERIF_FEATURES
    bool vaReverse = m_VaReverse;

    if (vaReverse)
    {
        if (returlwidheapFlag)
            return LWOS32_ALLOC_FLAGS_FORCE_MEM_GROWS_DOWN;
        else
            return DRF_DEF(OS46, _FLAGS, _DMA_OFFSET_GROWS, _DOWN);
    }
#endif
    return 0;
}

//-----------------------------------------------------------------------------
// returlwidheapFlag:
//    true: return reverse flag for vidheap interface(VidHeapAllocPitchHeightEx)
//    false: return reverse flag for mapmem interface(MapMemoryDma)
//Note:
//    Lwrrently MapMemoryDma can't allocate pa, so GetPaReverseFlags(false)
//    should not happen.
UINT32 Surface2D::GetPaReverseFlags(bool returlwidheapFlag) const
{
#ifdef LW_VERIF_FEATURES
    bool paReverse = m_PaReverse;

    if (paReverse)
    {
        if (returlwidheapFlag)
            return LWOS32_ALLOC_FLAGS_FORCE_MEM_GROWS_DOWN;
        else
            return DRF_DEF(OS46, _FLAGS, _DMA_OFFSET_GROWS, _DOWN);
    }
#endif
    return 0;
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetPteCoalesceFlags() const
{
#ifdef LW_VERIF_FEATURES
    switch (m_MaxCoalesce)
    {
        case 0:
            return DRF_DEF(OS46, _FLAGS, _PTE_COALESCE_LEVEL_CAP, _DEFAULT);
        case 1:
            return DRF_DEF(OS46, _FLAGS, _PTE_COALESCE_LEVEL_CAP, _1);
        case 2:
            return DRF_DEF(OS46, _FLAGS, _PTE_COALESCE_LEVEL_CAP, _2);
        case 4:
            return DRF_DEF(OS46, _FLAGS, _PTE_COALESCE_LEVEL_CAP, _4);
        case 8:
            return DRF_DEF(OS46, _FLAGS, _PTE_COALESCE_LEVEL_CAP, _8);
        case 16:
            return DRF_DEF(OS46, _FLAGS, _PTE_COALESCE_LEVEL_CAP, _16);
        case 32:
            return DRF_DEF(OS46, _FLAGS, _PTE_COALESCE_LEVEL_CAP, _32);
        case 64:
            return DRF_DEF(OS46, _FLAGS, _PTE_COALESCE_LEVEL_CAP, _64);
        case 128:
            return DRF_DEF(OS46, _FLAGS, _PTE_COALESCE_LEVEL_CAP, _128);
        default:
            MASSERT(!"Invalid MaxCoalesce");
    }
#endif
    return 0;
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetPeerMappingFlags() const
{
    return m_bUsePeerMappingFlags
        ? m_PeerMappingFlags
        : GetLoopBackFlags();
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetLoopBackFlags() const
{
#ifdef LW_VERIF_FEATURES
    if (m_LoopBack)
    {
        if (GetGpuSubdev(0)->HasFeature(Device::GPUSUB_SUPPORTS_RM_HYBRID_P2P))
        {
            if (m_LoopBackPeerId != USE_DEFAULT_RM_MAPPING)
            {
                return DRF_DEF(OS46, _FLAGS, _P2P_ENABLE, _LOOPBACK)
                     | DRF_NUM(OS46, _FLAGS, _P2P_LOOPBACK_PEER_ID,
                         m_LoopBackPeerId);
            }
            else
            {
                return DRF_DEF(OS46, _FLAGS, _P2P_ENABLE, _NOSLI)
                     | DRF_NUM(OS46, _FLAGS, _P2P_SUBDEV_ID_SRC, 0)
                     | DRF_NUM(OS46, _FLAGS, _P2P_SUBDEV_ID_TGT, 0);
            }
        }
        else
        {
            return DRF_DEF(OS46, _FLAGS, _P2P_ENABLE, _SLI)
                 | DRF_NUM(OS46, _FLAGS, _P2P_SUBDEV_ID_SRC, 0);
        }
    }
#endif
    return 0;
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetPrivFlagsSeg() const
{
    return 0;
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetPrivFlagsPag() const
{
#ifdef LW_VERIF_FEATURES
    if (m_Priv)
        return DRF_DEF(OS46, _FLAGS, _PRIV, _ENABLE);
#endif
    return 0;
}

//-----------------------------------------------------------------------------
UINT64 Surface2D::GetPhysPageSize() const
{
    return Platform::GetPageSize();
}

//-----------------------------------------------------------------------------
UINT64 Surface2D::CalcLimit(UINT64 DefaultSize) const
{
    return (m_Limit < 0) ? (DefaultSize - m_HiddenAllocSize - m_CdeAllocSize - 1) : m_Limit;
}

//-----------------------------------------------------------------------------
RC Surface2D::CommitFbSpeed()
{
    RC rc;

    if (FbSpeedDefault == m_FbSpeed) return OK;

    if ((GetGpuPhysAddrHint() > 0) || (GetGpuPhysAddrHintMax() > 0))
    {
        Printf(Tee::PriHigh, "Warning: user-specified physical address "
            "assignment overrides FB speed setting.");
        return OK;
    }

    GpuSubdevice *subDev = GetGpuSubdev(0);
    UINT32 regionCount = subDev->GetFbRegionCount();

    // If there is only one FB region, than the FB speed is the same for
    // the entire physical address range, so only GPUs with more than one
    // FB region need to be considered.
    if (regionCount > 1)
    {
        UINT64 minAddress = 0;
        UINT64 maxAddress = 0;

        if (FbSpeedFast == m_FbSpeed)
        {
            // Find the FB region with the fastest performance.

            UINT32 fastPerformance = subDev->GetFbRegionPerformance(0);
            UINT32 fastIndex = 0;

            for (UINT32 i = 1; i < regionCount; ++i)
            {
                UINT32 performance = subDev->GetFbRegionPerformance(i);

                if (performance > fastPerformance)
                {
                    fastPerformance = performance;
                    fastIndex = i;
                }
            }

            subDev->GetFbRegionAddressRange(fastIndex,
                &minAddress, &maxAddress);
        }
        else if (FbSpeedSlow == m_FbSpeed)
        {
            // Find the FB region with the slowest performance.

            UINT32 slowPerformance = subDev->GetFbRegionPerformance(0);
            UINT32 slowIndex = 0;

            for (UINT32 i = 1; i < regionCount; ++i)
            {
                UINT32 performance = subDev->GetFbRegionPerformance(i);

                if (performance < slowPerformance)
                {
                    slowPerformance = performance;
                    slowIndex = i;
                }
            }

            subDev->GetFbRegionAddressRange(slowIndex,
                &minAddress, &maxAddress);
        }
        else
        {
            MASSERT(!"Illegal FB speed value");
        }

        SetGpuPhysAddrHint(minAddress);
        SetGpuPhysAddrHintMax(maxAddress);
    }
    else
    {
        Printf(Tee::PriHigh, "Warning: FB speed ignored for device with homogeneous memory performance\n");
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC Surface2D::CommitPhysAddrOnlyAttr(UINT32 *pAttrib, UINT32 *pAttr2)
{
    if ((m_PhysAddrPageSize != 0) && (m_PhysAddrPageSize != m_PageSize))
    {
        if (m_Split)
        {
            Printf(Tee::PriHigh, "Error: Not ready to support split surface yet!\n");
            return RC::UNSUPPORTED_FUNCTION;
        }

        if (m_PageSize != 0)
        {
            Printf(Tee::PriHigh, "Error: PA pagesize is different from VA pagesize. "
                    "It is only supported for BOTH surface!\n");
            return RC::SOFTWARE_ERROR;
        }

        switch (m_PhysAddrPageSize)
        {
            case 0: // _DEFAULT is 0 and it's the default
                break;
            case 4:
                *pAttrib = FLD_SET_DRF(OS32, _ATTR, _PAGE_SIZE, _4KB, *pAttrib);
                break;
            case 64:
            case 128:
                *pAttrib = FLD_SET_DRF(OS32, _ATTR, _PAGE_SIZE, _BIG, *pAttrib);
                break;
            case 2048:
                *pAttrib = FLD_SET_DRF(OS32, _ATTR, _PAGE_SIZE, _HUGE, *pAttrib);
                *pAttr2 = FLD_SET_DRF(OS32, _ATTR2, _PAGE_SIZE_HUGE, _2MB, *pAttr2);
                break;
            case 524288:
                *pAttrib = FLD_SET_DRF(OS32, _ATTR, _PAGE_SIZE, _HUGE, *pAttrib);
                *pAttr2 = FLD_SET_DRF(OS32, _ATTR2, _PAGE_SIZE_HUGE, _512MB, *pAttr2);
                break;
            default:
                 return RC::SOFTWARE_ERROR;
        }
    }

    return OK;
}

//-----------------------------------------------------------------------------
RC Surface2D::CommitFLAOnlyAttr(UINT32 *pAttrib, UINT32 *pAttr2)
{
    switch (m_FLAPageSizeKb)
    {
        case 2048:
            *pAttrib = FLD_SET_DRF(OS32, _ATTR, _PAGE_SIZE, _HUGE, *pAttrib);
            *pAttr2 = FLD_SET_DRF(OS32, _ATTR2, _PAGE_SIZE_HUGE, _2MB, *pAttr2);
            break;
        case 0:
        case 524288:
            *pAttrib = FLD_SET_DRF(OS32, _ATTR, _PAGE_SIZE, _HUGE, *pAttrib);
            *pAttr2 = FLD_SET_DRF(OS32, _ATTR2, _PAGE_SIZE_HUGE, _512MB, *pAttr2);
            break;
        default:
             return RC::SOFTWARE_ERROR;
    }
    return OK;
}

//-----------------------------------------------------------------------------
RC Surface2D::AllocPhysMemory()
{
    RC rc;

    // Eventually, we would like to use VidHeapControl for all allocs
    if (m_bUseVidHeapAlloc)
    {
        // Turn off splitting for surfaces less than two pages
        if (m_Split)
        {
            if (m_Size <= GetPhysPageSize())
            {
                Printf(Tee::PriLow, "Surface too small to be split\n");
                m_Split = false;
            }

            UINT64 compressionPageSize = 0;
            CHECK_RC(GetGpuDev()->GetCompressionPageSize(&compressionPageSize));
            if (m_Compressed && (m_Size <= compressionPageSize))
            {
                Printf(Tee::PriLow, "Compressed Surface too small to be split\n");
                m_Split = false;
            }
        }

        // RM might change heap attrib MODS passes in. Use a local variable
        UINT32 attrib = m_VidHeapAttr;
        UINT32 attr2 = m_VidHeapAttr2;
        UINT32 flags = m_PaVidHeapFlags;
        UINT64 rangeBegin = 0;
        UINT64 rangeEnd = 0;

        CHECK_RC(CommitPhysAddrOnlyAttr(&attrib, &attr2));

        // If there is a maximum physical address value, then this
        // surface will be allocated within a physical address range.
        if (m_GpuPhysAddrHintMax > 0)
        {
            flags |= LWOS32_ALLOC_FLAGS_USE_BEGIN_END;

            rangeBegin = m_GpuPhysAddrHint;
            rangeEnd = m_GpuPhysAddrHintMax;
        }

        // If there a physical address hint but no maximum address,
        // then this surface should be allocated to a specific
        // physical address.
        else if (m_GpuPhysAddrHint > 0)
        {
            flags |= LWOS32_ALLOC_FLAGS_FIXED_ADDRESS_ALLOCATE;

            // RM requires the address aligned with page boundary (4k/64k)
            // Default page size: 64k-Tesla / 128k-Fermi
            const UINT64 bigPageSize = GetGpuDev()->GetBigPageSize();
            UINT64 page_size = m_PageSize ? m_PageSize<<10 : bigPageSize;
            m_DefClientData.m_VidMemOffset =
                (m_GpuPhysAddrHint/page_size)*page_size;
        }

        // Allocate storage for the surface
        if (GetUsePitchAlloc())
        {
            const UINT64 WholeHeight = (m_Size+m_Pitch-1) / m_Pitch;
            const UINT64 Height = m_Split ? (WholeHeight+1)/2 : WholeHeight;
            MASSERT((Height >> 32) == 0);  // Protect cast in next line
            CHECK_RC(m_pLwRm->VidHeapAllocPitchHeightEx(
                m_Type, flags, &attrib, &attr2,
                static_cast<UINT32>(Height), &m_Pitch,
                m_VidHeapAlign, &m_PteKind, &m_VidHeapCoverage,
                &m_PartStride, m_CtagOffset,
                &m_DefClientData.m_hMem,
                &m_DefClientData.m_VidMemOffset, 0, 0, 0,
                GetGpuDev(), rangeBegin, rangeEnd));

            // Account for possible pitch adjustment
            m_Size = m_Pitch * WholeHeight;
            m_ArrayPitch = (m_Size - (m_ExtraAllocSize + m_HiddenAllocSize + m_CdeAllocSize)) / m_ArraySize;
            if (m_ArrayPitchAlignment)
            {
                // Both size and arraypitch need to set to the desired alignment
                m_ArrayPitch = (m_ArrayPitch + m_ArrayPitchAlignment - 1) & ~(m_ArrayPitchAlignment - 1);
            }
            m_SplitHalfSize = m_Pitch * Height;

            // Align half size down to page
            if (m_Split)
            {
                UINT64 compressionPageSize = 0;
                CHECK_RC(GetGpuDev()->GetCompressionPageSize(&compressionPageSize));
                const UINT64 pageSize = m_Compressed ? compressionPageSize : GetPhysPageSize();

                m_SplitHalfSize = ALIGN_DOWN(m_SplitHalfSize, pageSize);
            }
        }
        else
        {
            m_SplitHalfSize = m_Split ? m_Size / 2 : m_Size;
            UINT32 Height = m_Height;

            // Align half size to page
            if (m_Split)
            {
                UINT64 compressionPageSize = 0;
                CHECK_RC(GetGpuDev()->GetCompressionPageSize(&compressionPageSize));
                const UINT64 pageSize = m_Compressed ? compressionPageSize : GetPhysPageSize();
                m_SplitHalfSize = ALIGN_UP(m_SplitHalfSize, pageSize);
                Height = (Height+1) / 2;
            }

            CHECK_RC(m_pLwRm->VidHeapAllocSizeEx(
                m_Type, flags, &attrib, &attr2, m_SplitHalfSize,
                m_VidHeapAlign, &m_PteKind, &m_VidHeapCoverage,
                &m_PartStride, m_CtagOffset,
                &m_DefClientData.m_hMem,
                &m_DefClientData.m_VidMemOffset, 0, 0, 0, m_Width, Height,
                GetGpuDev(), rangeBegin, rangeEnd));
        }

        UINT32 value = DRF_VAL(OS32, _ATTR, _COMPR, attrib);
        SetCompressed(value != LWOS32_ATTR_COMPR_NONE);
        SetCompressedFlag(value);
        if (m_PhysContig == 0)
        {
            // For default contiguous mode, RM will try to allocate contiguous memory first
            // If it cannot it will fall back to non-contiguous
            // Update m_PhysContig of the surface accordingly
            // To be used for printing in the log file
            m_PhysContig = (DRF_VAL(OS32, _ATTR, _PHYSICALITY, attrib) == LWOS32_ATTR_PHYSICALITY_CONTIGUOUS);
        }

        // Allocate storage for split surface's second half
        if (m_Split)
        {
            attrib = m_SplitVidHeapAttr;
            attr2 = m_SplitVidHeapAttr2;
            INT32 splitPteKind = (m_SplitPteKind >= 0) ? m_SplitPteKind : m_PteKind;
            if (GetUsePitchAlloc())
            {
                const UINT64 Height = (m_Size - m_SplitHalfSize - 1) / m_Pitch + 1;
                UINT32 splitPitch = m_Pitch;
                UINT32 splitVidHeapCoverage = m_VidHeapCoverage;
                UINT32 splitPartStride = m_PartStride;
                MASSERT((Height >> 32) == 0);  // Protect cast in next line
                CHECK_RC(m_pLwRm->VidHeapAllocPitchHeightEx(
                    m_Type, m_PaVidHeapFlags, &attrib, &attr2,
                    static_cast<UINT32>(Height), &splitPitch,
                    m_VidHeapAlign, &splitPteKind, &splitVidHeapCoverage,
                    &splitPartStride, m_CtagOffset,
                    &m_DefClientData.m_hSplitMem,
                    &m_DefClientData.m_SplitVidMemOffset, 0, 0, 0,
                    GetGpuDev()));
                MASSERT(splitPitch == m_Pitch);
            }
            else
            {
                UINT32 splitVidHeapCoverage = m_VidHeapCoverage;
                UINT32 splitPartStride = m_PartStride;
                CHECK_RC(m_pLwRm->VidHeapAllocSizeEx(
                    m_Type, m_PaVidHeapFlags, &attrib, &attr2, m_Size-m_SplitHalfSize,
                    m_VidHeapAlign, &splitPteKind, &splitVidHeapCoverage,
                    &splitPartStride, m_CtagOffset,
                    &m_DefClientData.m_hSplitMem,
                    &m_DefClientData.m_SplitVidMemOffset,
                    0, 0, 0, m_Width, m_Height/2,
                    GetGpuDev()));
            }
        }
    }
    else
    {
        MASSERT(!m_Split);
        m_AllocSysMemFlags = GetPhysicalityFlags();
        if (m_bUseExtSysMemFlags)
        {
            m_AllocSysMemFlags |= m_ExtSysMemAllocFlags;
        }
        else
        {
            m_AllocSysMemFlags |= GetLocationFlags();
        }

        if (m_Displayable)
        {
            m_AllocSysMemFlags |= DRF_DEF(OS02, _FLAGS, _ALLOC_NISO_DISPLAY, _NO);
        }
        else
        {
            //
            // for all non displayable buffers like pushbuffers,
            // notifiers set attribute ALLOC_NISO_DISPLAY to YES
            //
            m_AllocSysMemFlags |= DRF_DEF(OS02, _FLAGS, _ALLOC_NISO_DISPLAY, _YES);
        }

        CHECK_RC(m_pLwRm->AllocSystemMemory(&m_DefClientData.m_hMem,
                    m_AllocSysMemFlags,
                    m_Size, GetGpuDev()));
        m_SplitHalfSize = m_Size;
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC Surface2D::MapPhysMemory(LwRm *pLwRm)
{
    RC rc;

    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    if (!pClientData)
        return RC::SOFTWARE_ERROR;

    // for allocating a new ctxdma
    const UINT32 SegDmaFlags =
        GetAccessFlagsSeg()
        | GetCacheSnoopFlagsSeg()
        | GetPageSizeFlagsSeg()
        | GetDmaProtocolFlags()
        | GetAddrSpaceFlags()
        | GetMemAttrsLocationFlags()
        | GetExtSysMemCtxDmaFlags()
        | GetKernelMappingFlags()
        | GetPrivFlagsSeg();

    // for mapping into an existing ctxdma or virtMem
    const UINT32 PagDmaFlags =
        GetAccessFlagsPag()
        | GetCacheSnoopFlagsPag()
        | GetShaderAccessFlags()
        | GetPageSizeFlagsPag()
        | GetVaReverseFlags(false)
        | GetPteCoalesceFlags()
        | GetPeerMappingFlags()
        | GetPrivFlagsPag();

    // Set up a ctxdma to map this memory into the GPU's address space.
    //
    // For the GPU, we follow the user's requested addressing model:
    //
    //  - Segmentation maps the memory into its own private ctxdma, at offset 0.
    //
    //  - Paging (G80+) maps the memory into the global virtual address
    //    space shared by FB, host, and peer gpu memory.  The surface will be
    //    mapped at some 40-bit offset from 0.

    if (m_AddressModel == Memory::Segmentation)
    {
        if ((GetLocation() == Memory::Fb) && !GetKernelMapping() &&
            !GetGpuSubdev(0)->HasFeature(Device::GPUSUB_SUPPORTS_GPU_SEGMENTATION))
        {
            MASSERT(!"Segmentation is not supported!");
            return RC::SOFTWARE_ERROR;
        }

        if (m_ParentClass)
            MASSERT(!"Special class for segmentation mode is not supported yet!");

#if defined(TEGRA_MODS)
        if (m_RouteDispRM)
        {
            CHECK_RC(ImportMemoryToDisplay());

            CHECK_RC(pLwRm->AllocDisplayContextDma(&pClientData->m_hCtxDmaGpu,
                                            SegDmaFlags,
                                            pClientData->m_hImportMem,
                                            m_HiddenAllocSize, CalcLimit(m_Size)));
        }
        else
#endif
        {
            CHECK_RC(pLwRm->AllocContextDma(&pClientData->m_hCtxDmaGpu,
                                            SegDmaFlags,
                                            pClientData->m_hMem,
                                            m_HiddenAllocSize, CalcLimit(m_Size)));
        }
        m_CtxDmaAllocatedGpu = true;
        pClientData->m_CtxDmaOffsetGpu = 0;
    }
    else
    {
        MASSERT(m_AddressModel == Memory::Paging);

        CHECK_RC(MapPhysMemToGpu(PagDmaFlags, pLwRm));
    }

    // Map to ctxdma for display
    CHECK_RC(MapPhysMemToIso(SegDmaFlags, PagDmaFlags, pLwRm));

    m_MapMemoryDmaFlags = PagDmaFlags;

    return rc;
}

//-----------------------------------------------------------------------------
RC Surface2D::MapPhysMemToGpu(UINT32 Flags, LwRm *pLwRm)
{
    RC rc;
    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    if (!pClientData)
        return RC::SOFTWARE_ERROR;
    LwRm::Handle objHandle = pClientData->m_hMem;

    // Do not map CheetAh-only surfaces with rmapi_tegra
    if (Platform::UsesLwgpuDriver() && m_VASpace == TegraVASpace)
    {
        pClientData->m_hTegraVirtMem = objHandle;
        pClientData->m_TegraVirtAddr = m_HiddenAllocSize;
        return OK;
    }

    if (m_ParentClass)
    {
        if (pLwRm != m_pLwRm)
        {
            Printf(Tee::PriHigh,
                   "Other client duplication with custom parent class is "
                   "not supported!\n");
            return RC::UNSUPPORTED_FUNCTION;
        }
        CHECK_RC(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetGpuDev()),
                              &pClientData->m_hCtxDmaGpu,
                               m_ParentClass,
                              (void*)&m_ParentClassAttr));
        m_CtxDmaAllocatedGpu = true;
    }
    else
    {
        pClientData->m_hCtxDmaGpu =
            GetGpuDev()->GetMemSpaceCtxDma(m_Location, false, pLwRm);
    }

    if (GetSkedReflected() || IsHostReflectedSurf())
    {
        MASSERT(pLwRm == m_pLwRm);
        if (Platform::GetSimulationMode() == Platform::Amodel)
        {
            // Since Amodel doesn't have MMU, so it's not really ready to allocate
            // SKED/HOST reflected buffer, in order to verify the traces before fmodel
            // getting ready, here we hack it to be normal buffer
            Printf(Tee::PriNormal, "Workaround for Amodel: %s is not allocated as "
                "SKED/HOST reflected buffer\n", GetName().c_str());
        }
        else
        {
            objHandle = (LwRm::Handle)GetMappingObj();
        }
    }
#ifdef LW_VERIF_FEATURES
    GpuSubdevice* pSubdev = GetGpuSubdev(0);
    if (m_LoopBack && pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_RM_HYBRID_P2P))
    {
        GpuDevMgr *pDevMgr = ((GpuDevMgr*)DevMgrMgr::d_GraphDevMgr);
        CHECK_RC(pDevMgr->AddPeerMapping(pLwRm, pSubdev, pSubdev, m_LoopBackPeerId,
            m_IsSPAPeerAccess? GpuDevMgr::SPA : GpuDevMgr::GPA));
    }

    if (m_LoopBack && (Platform::GetSimulationMode() == Platform::Hardware))
    {
        // For now only print a warning and allow failures to occur if loopback
        // is attempted on silicon
        Printf(Tee::PriHigh,
               "Warning : Surface2D::MapPhysMemToGpu() loopback may not be "
               "supported on all hardware chipsets!\n");
    }
#endif

    // Whichever context DMA we picked had better exist!
    MASSERT(pClientData->m_hCtxDmaGpu || GetGpuSubdev(0)->IsSOC());

    UINT64 Offset = 0;
    UINT64 Limit = 0;
    UINT64 Free = 0;
    UINT64 Total = 0;

    // Map into the address space to get our virtual address
    if (GetUseVirtMem())
    {
        // ZLwll is not supported for virtual memory
        UINT32 Attr = m_VidHeapAttr;
        UINT32 Attr2 = m_VidHeapAttr2;
        UINT64 rangeBegin = 0;
        UINT64 rangeEnd = 0;
        UINT64 virtAlignment = m_VirtAlignment;

        Attr = FLD_SET_DRF(OS32, _ATTR, _ZLWLL, _NONE, Attr);

        // Include virtual memory alignment in alloc flags
        UINT32 VidHeapFlags = m_VaVidHeapFlags | LWOS32_ALLOC_FLAGS_VIRTUAL;

        if (m_IsSparse)
        {
            VidHeapFlags |= LWOS32_ALLOC_FLAGS_SPARSE;
        }

        // If there is a maximum virtual address value, then this
        // surface will be allocated within a virtual address range.
        if (m_GpuVirtAddrHintMax > 0)
        {
            VidHeapFlags |= LWOS32_ALLOC_FLAGS_USE_BEGIN_END;

            rangeBegin = m_GpuVirtAddrHint;
            rangeEnd = m_GpuVirtAddrHintMax;
        }

        // If there a virtual address hint but no maximum address,
        // then this surface should be allocated to a specific
        // virtual address.
        else if (m_GpuVirtAddrHint > 0)
        {
            VidHeapFlags |= LWOS32_ALLOC_FLAGS_FIXED_ADDRESS_ALLOCATE;

            Offset = m_GpuVirtAddrHint;

            // When doing a virtual allocation, if the alignment isn't
            // set, RM will assume an alignment of 128K because it
            // doesn't know anything about the physical memory that
            // this virtual allocation will be mapped to.  In this
            // case, we come up with an appropriate alignment based
            // on the physical allocation.
            if ((virtAlignment == 0) && HasPhysical())
            {
                CHECK_RC(GetDefaultAlignment(&virtAlignment));

                if ((Offset & (virtAlignment - 1)) != 0)
                {
                    Printf(Tee::PriHigh, "Error: virtual address assignment 0x%llx is not page aligned (0x%llx).\n", Offset, virtAlignment);
                }
            }
        }

        if (virtAlignment)
        {
            VidHeapFlags |= LWOS32_ALLOC_FLAGS_ALIGNMENT_FORCE;
        }

        // For Virtual-only surfaces, delay page table setup
        if (IsVirtualOnly() && !Platform::UsesLwgpuDriver())
        {
            VidHeapFlags |= LWOS32_ALLOC_FLAGS_LAZY;
        }

        const auto hVASpace = GetPrimaryVASpaceHandle();

        const UINT64 Alignment =
            virtAlignment ? virtAlignment : m_VidHeapAlign;

        if (pClientData->m_hVirtMem == 0)
        {
            if (m_bUseVidHeapAlloc && GetUsePitchAlloc())
            {
                const UINT64 wholeHeight = (m_Size+m_Pitch-1) / m_Pitch;

                CHECK_RC(pLwRm->VidHeapAllocPitchHeightEx(
                        m_Type,
                        VidHeapFlags,
                        &Attr,
                        &Attr2,
                        UNSIGNED_CAST(UINT32, wholeHeight),
                        &m_Pitch,
                        Alignment,
                        &m_PteKind,
                        &m_VidHeapCoverage,
                        &m_PartStride,
                        m_CtagOffset,
                        &pClientData->m_hVirtMem,
                        &Offset,
                        &Limit,
                        &Free,
                        &Total,
                        GetGpuDev(),
                        rangeBegin,
                        rangeEnd,
                        hVASpace));

                // Account for possible pitch adjustment
                m_Size = m_Pitch * wholeHeight;
                m_ArrayPitch = (m_Size - (m_ExtraAllocSize + m_HiddenAllocSize + m_CdeAllocSize)) / m_ArraySize;
                if (m_ArrayPitchAlignment)
                {
                    // Both size and arraypitch need to set to the desired alignment
                    m_ArrayPitch = (m_ArrayPitch + m_ArrayPitchAlignment - 1) & ~(m_ArrayPitchAlignment - 1);
                }
            }
            else
            {
                CHECK_RC(pLwRm->VidHeapAllocSizeEx(
                        m_Type,
                        VidHeapFlags,
                        &Attr,
                        &Attr2,
                        m_Size,
                        Alignment,
                        &m_PteKind,
                        &m_VidHeapCoverage,
                        &m_PartStride,
                        m_CtagOffset,
                        &pClientData->m_hVirtMem,
                        &Offset,
                        &Limit,
                        &Free,
                        &Total,
                        0, // width
                        0, // height,
                        GetGpuDev(),
                        rangeBegin,
                        rangeEnd,
                        hVASpace));
            }
        }

        m_VirtMemAllocated = true;

        if (!HasMap())
        {
            pClientData->m_CtxDmaOffsetGpu = Offset;
            pClientData->m_GpuVirtAddr = Offset;
            MASSERT(pClientData->m_hTegraVirtMem == 0);

        }
        else
        {
            // Map first part into virtual address space
            pClientData->m_GpuVirtAddr = 0;
            CHECK_RC(pLwRm->MapMemoryDma(
                         pClientData->m_hVirtMem,
                         objHandle,
                         m_HiddenAllocSize,
                         CalcLimit(m_SplitHalfSize)+1,
                         Flags,
                         &pClientData->m_GpuVirtAddr,
                         GetGpuDev()));

            // Map second part into virtual address space
            pClientData->m_SplitGpuVirtAddr = m_SplitHalfSize - m_HiddenAllocSize;
            if (m_Split)
            {
                MASSERT(m_Size > GetPhysPageSize());
                MASSERT(!GetSkedReflected() && !IsHostReflectedSurf());
                CHECK_RC(pLwRm->MapMemoryDma(
                             pClientData->m_hVirtMem,
                             pClientData->m_hSplitMem,
                             0,
                             GetPartSize(1),
                             Flags,
                             &pClientData->m_SplitGpuVirtAddr,
                             GetGpuDev()));
            }

            m_VirtMemMapped = true;
            pClientData->m_CtxDmaOffsetGpu = pClientData->m_GpuVirtAddr;
        }
    }
    else
    {
        MASSERT(!m_Split);

        m_CtxDmaMappedGpu = false;

        if ((m_GpuVirtAddrHint > 0) || (m_GpuVirtAddrHintMax > 0))
        {
            UINT64 oldCtxDmaOffsetGpu = pClientData->m_CtxDmaOffsetGpu;
            UINT32 count = 0;
            const UINT64 bigPageSize = GetGpuDev()->GetBigPageSize();

            pClientData->m_CtxDmaOffsetGpu += m_GpuVirtAddrHint;

            // If the address constraint is a range, adjust the starting
            // address of the range if it is within PDE 0 as virtual
            // allocations in that range will always fail.
            if (m_GpuVirtAddrHintMax > 0)
            {
                // PDE 0 goes from address zero to 1K times the big page
                // size.  For example, if the big page size is 128KB,
                // the addresses from 0 to 128MB are unusable.
                UINT64 firstValidAddress = bigPageSize * 1024;

                if (pClientData->m_CtxDmaOffsetGpu < firstValidAddress)
                {
                    pClientData->m_CtxDmaOffsetGpu = firstValidAddress;
                }
            }

            // The delta value is the amount to change the address
            // hint by when retrying.

            UINT64 delta = m_Size;

            // Adjust the delta value to account for the page size.
            if (GetLocation() == Memory::Fb)
            {
                delta = (delta + bigPageSize - 1) & ~(bigPageSize - 1);
            }
            else
            {
                // Align to 4K.
                delta = (delta + 0xfff) & ~0xfff;
            }

            Printf(Tee::PriDebug, "Address range delta: %llx\n", delta);

            do
            {
                rc.Clear();
                rc = pLwRm->MapMemoryDma(
                    pClientData->m_hCtxDmaGpu,
                    objHandle,
                    m_HiddenAllocSize,
                    CalcLimit(m_Size)+1,
                    Flags | DRF_DEF(OS46, _FLAGS, _DMA_OFFSET_FIXED, _TRUE),
                    &pClientData->m_CtxDmaOffsetGpu,
                    GetGpuDev());

                ++count;

                if (rc == OK)
                {
                    m_CtxDmaMappedGpu = true;
                }
                else
                {
                    pClientData->m_CtxDmaOffsetGpu += delta;
                }
            } while (!m_CtxDmaMappedGpu &&
                (pClientData->m_CtxDmaOffsetGpu + m_Size < m_GpuVirtAddrHintMax));

            if (rc != OK)
            {
                pClientData->m_CtxDmaOffsetGpu = oldCtxDmaOffsetGpu;
                Printf(Tee::PriDebug, "Failed %u MapMemoryDma attempts to allocate within a specific virtual address range.\n", count);
                rc.Clear();
            }
            else if (count > 1)
            {
                Printf(Tee::PriDebug, "Required %u MapMemoryDma attempts to allocate within a specific virtual address range.\n", count);
            }
        }

        if (!m_CtxDmaMappedGpu && !GetGpuSubdev(0)->IsFakeTegraGpu())
        {
            CHECK_RC(pLwRm->MapMemoryDma(
                pClientData->m_hCtxDmaGpu,
                objHandle,
                m_HiddenAllocSize,
                CalcLimit(m_Size)+1,
                Flags,
                &pClientData->m_CtxDmaOffsetGpu,
                GetGpuDev()));
        }

        m_CtxDmaMappedGpu = true;
    }

    if ((Xp::GetOperatingSystem() != Xp::OS_WINDA) &&
        (Xp::GetOperatingSystem() != Xp::OS_LINDA))
    {
        MASSERT(m_pGpuDev->GetVASpace(FERMI_VASPACE_A, m_pLwRm));
    }

    const bool hasTegraVASpace = m_pGpuDev->GetVASpace(TEGRA_VASPACE_A, m_pLwRm) != 0;

    // Allocate secondary VA space
    if (Utility::CountBits(m_VASpace) > 1 &&
        pClientData->m_hTegraVirtMem == 0)
    {
        // For now we assume that on CheetAh with big GPU
        // there are only two ASIDs: one for GPU one for non-GPU engines.
        MASSERT(m_VASpace == GPUAndTegraVASpace);

        if (hasTegraVASpace)
        {
            m_TegraVirtMemAllocated = true;
            CHECK_RC(MapToVASpace(GetVASpaceHandle(TegraVASpace),
                                  &pClientData->m_hTegraVirtMem,
                                  HasMap() ? &pClientData->m_TegraVirtAddr : nullptr,
                                  pLwRm));
            if (HasMap())
            {
                m_TegraMemMapped = true;
            }
        }
    }

    // On CheetAh without big GPU we don't need to
    // allocate a separate address space for the GPU.
    if (pClientData->m_hTegraVirtMem == 0 &&
        pClientData->m_TegraVirtAddr == 0 &&
        m_VASpace == TegraVASpace         &&
        hasTegraVASpace)
    {
        pClientData->m_hTegraVirtMem = pClientData->m_hVirtMem;
        pClientData->m_TegraVirtAddr = pClientData->m_GpuVirtAddr;
    }

    if (Platform::GetSimulationMode() == Platform::Amodel)
    {
        SendReflectedEscapeWrites(pLwRm);
    }

    // On CheetAh with "kernel mode RM", in which case we're actually running
    // with Linux kernel drivers instead of dGPU RM, we don't have the ability
    // to allocate VA space objects (they are not supported).
    //
    // In this case we will return the original physical memory handle.
    // Appropriate drivers can colwert it to an internal kernel memory handle
    // using LwRm::GetKernelHandle(), which is usable with other
    // kernel drivers.
    if (Platform::UsesLwgpuDriver())
    {
        pClientData->m_hTegraVirtMem = objHandle;
        pClientData->m_TegraVirtAddr = m_HiddenAllocSize;
    }

    // After map, query RM to reset the PTE kind if the PTE is not updated before.
    if (m_PteKind == -1 && IsGpuMapped())
    {
        GpuUtility::GetPteKind(GetCtxDmaOffsetGpu(), GetGpuVASpace(), GetGpuDev(), &m_PteKind, pLwRm);
    }

    // Update layout according to ptekind.
    if (m_Layout == Pitch)
    {
        GpuSubdevice *gpuSubdevice = GetGpuDev()->GetSubdevice(0);
        const UINT32 pteKindTraits = gpuSubdevice->GetKindTraits(m_PteKind);
        if ((pteKindTraits != GpuSubdevice::Ilwalid_Kind) &&
            !(pteKindTraits & GpuSubdevice::Pitch) &&
            !(pteKindTraits & GpuSubdevice::Generic_Memory))
        {
            m_Layout = BlockLinear;
            Printf(Tee::PriDebug, "Update surface %s layout to be blocklinear.\n",
                   GetName().c_str());
        }
    }

    return OK;
}

//-----------------------------------------------------------------------------
RC Surface2D::ImportMemoryToDisplay()
{
    RC rc;
#if defined(TEGRA_MODS)
    INT32 importFd = 0;
    // Get the FD here
    CHECK_RC(m_pLwRm->GetTegraMemFd(m_DefClientData.m_hMem, &importFd));

    LW_OS_DESC_MEMORY_ALLOCATION_PARAMS allocParams = {};
    allocParams.type = LWOS32_TYPE_IMAGE;
    allocParams.descriptor = reinterpret_cast<LwP64>(importFd);
    allocParams.descriptorType = LWOS32_DESCRIPTOR_TYPE_OS_FILE_HANDLE;
    allocParams.limit = m_Size;
    allocParams.flags = 0;
    allocParams.attr |=  m_PhysContig ?
        DRF_DEF(OS32, _ATTR, _PHYSICALITY, _CONTIGUOUS) :
        DRF_DEF(OS32, _ATTR, _PHYSICALITY, _NONCONTIGUOUS);

    switch (m_Layout)
    {
        case Pitch:
            allocParams.attr |= DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH);
            break;
        case BlockLinear:
            allocParams.attr |= DRF_DEF(OS32, _ATTR, _FORMAT, _BLOCK_LINEAR);
            break;
        default:
            Printf(Tee::PriError, "Invalid layout\n");
            return RC::BAD_PARAMETER;
            break;
    }

    if (m_Displayable)
    {
        allocParams.attr |= DRF_DEF(OS32, _ATTR, _LOCATION, _PCI) |
            DRF_DEF(OS32, _ATTR, _COHERENCY, _WRITE_COMBINE);

        allocParams.attr2 |= DRF_DEF(OS32, _ATTR2, _NISO_DISPLAY, _NO);
    }
    else
    {
        allocParams.attr |= DRF_DEF(OS32, _ATTR, _LOCATION, _PCI) |
            DRF_DEF(OS32, _ATTR, _COHERENCY, _WRITE_BACK);
        allocParams.attr2 |= DRF_DEF(OS32, _ATTR2, _NISO_DISPLAY, _YES);
    }

    CHECK_RC(m_pLwRm->AllocDisp(m_pLwRm->GetDeviceHandle(GetGpuDev()),
                &m_DefClientData.m_hImportMem,
                LW01_MEMORY_SYSTEM_OS_DESCRIPTOR,
                &allocParams));
#endif
    return rc;
}

//-----------------------------------------------------------------------------
RC Surface2D::MapToVASpaceInternal
(
    LwRm::Handle  hVASpace,
    bool          bIsFlaMem,
    LwRm::Handle* phVirtMem,
    UINT64*       phVirtAddr,
    LwRm*         pLwRm
)
{
    RC rc;

    MASSERT(phVirtMem);
    *phVirtMem = 0;

    LwRm::Handle objHandle = GetMemHandle(pLwRm);
    if ((GetSkedReflected() || IsHostReflectedSurf()) &&
        // Since Amodel doesn't have MMU, so it's not really ready to allocate
        // SKED/HOST reflected buffer, in order to verify the traces before fmodel
        // getting ready, here we hack it to be normal buffer
        Platform::GetSimulationMode() != Platform::Amodel)
    {
        objHandle = (LwRm::Handle)GetMappingObj();
    }

    const UINT32 vidHeapFlags =
        (m_VaVidHeapFlags | LWOS32_ALLOC_FLAGS_VIRTUAL)
        & ~LWOS32_ALLOC_FLAGS_FIXED_ADDRESS_ALLOCATE;

    UINT64 offset;
    UINT64 limit;
    UINT64 free2;
    UINT64 total2;
    UINT32 attr            = m_VidHeapAttr;
    UINT32 attr2           = m_VidHeapAttr2;
    INT32  pteKind         = m_PteKind;
    UINT32 vidHeapCoverage = m_VidHeapCoverage;
    UINT32 partStride      = m_PartStride;

    attr = FLD_SET_DRF(OS32, _ATTR, _ZLWLL, _NONE, attr);

    if (GetUsePitchAlloc())
    {
        UINT32 pitch = m_Pitch;
        const UINT64 wholeHeight = (m_Size+m_Pitch-1) / m_Pitch;
        CHECK_RC(pLwRm->VidHeapAllocPitchHeightEx(
                m_Type,
                vidHeapFlags,
                &attr,
                &attr2,
                UNSIGNED_CAST(UINT32, wholeHeight),
                &pitch,
                0,
                &pteKind,
                &vidHeapCoverage,
                &partStride,
                m_CtagOffset,
                phVirtMem,
                &offset,
                &limit,
                &free2,
                &total2,
                GetGpuDev(),
                0,
                0,
                hVASpace));
    }
    else
    {
        CHECK_RC(pLwRm->VidHeapAllocSizeEx(
                m_Type,
                vidHeapFlags,
                &attr,
                &attr2,
                m_Size,
                0,
                &pteKind,
                &vidHeapCoverage,
                &partStride,
                m_CtagOffset,
                phVirtMem,
                &offset,
                &limit,
                &free2,
                &total2,
                0, // width
                0, // height,
                GetGpuDev(),
                0,
                0,
                hVASpace));
    }

    // Map surface into VA space
    if (phVirtAddr)
    {
        MASSERT(m_AddressModel == Memory::Paging);

        const UINT32 flags =
            GetAccessFlagsPag()
            | GetCacheSnoopFlagsPag()
            | GetShaderAccessFlags()
            | GetPageSizeFlagsPag()
            | GetVaReverseFlags(false)
            | GetPteCoalesceFlags()
            | GetPeerMappingFlags()
            | GetPrivFlagsPag();

        *phVirtAddr = 0;
        CHECK_RC(pLwRm->MapMemoryDma(
                    *phVirtMem,
                    objHandle,
                    m_HiddenAllocSize,
                    CalcLimit(m_SplitHalfSize)+1,
                    flags,
                    phVirtAddr,
                    GetGpuDev()));
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC Surface2D::MapToVASpace(LwRm::Handle hVASpace, LwRm::Handle* phVirtMem, UINT64* phVirtAddr, LwRm* pLwRm)
{
    if (!pLwRm)
    {
        pLwRm = m_pLwRm;
    }
    return MapToVASpaceInternal(hVASpace, false, phVirtMem, phVirtAddr, pLwRm);
}

//-----------------------------------------------------------------------------
//! Callwlate the default alignment for this surface based on the
//! page size that will be used when allocated.
//!
RC Surface2D::GetDefaultAlignment(UINT64* alignment)
{
    RC rc;

    if (GetLocation() == Memory::Fb)
    {
        if (m_Compressed &&
            (Platform::GetSimulationMode() != Platform::Amodel))
        {
            CHECK_RC(GetGpuDev()->GetCompressionPageSize(alignment));
        }
        else
        {
            *alignment = GetGpuDev()->GetBigPageSize();
        }
    }
    else
    {
        *alignment = GetPhysPageSize();
    }

    return OK;
}

//-----------------------------------------------------------------------------
void Surface2D::SendReflectedEscapeWrites(LwRm *pLwRm)
{
    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    MASSERT(pClientData != 0);

    if (GetSkedReflected())
    {
        // Amodel needs to know the address of the SKED surface.
        UINT32 isSupported_SetSked = 0;
        if (0 == m_pGpuDev->EscapeReadBuffer("supported/SetSkedReflected", 0 /*index*/, sizeof(UINT32), &isSupported_SetSked) &&
            isSupported_SetSked)
        {
            const UINT32 chId = GetGpuManagedChId();
            const UINT64 skedAddress = pClientData->m_CtxDmaOffsetGpu;
            const size_t bufferSize = sizeof(chId) + sizeof(skedAddress);
            unsigned char *buffer = new unsigned char[bufferSize];
            size_t byteIndex = 0;
            for (UINT32 i = 0; i < sizeof(chId); i++)
                buffer[byteIndex++] = (chId >> (8*i)) & 0xff;
            for (UINT32 i = 0; i < sizeof(skedAddress); i++)
                buffer[byteIndex++] = (unsigned char)((skedAddress >> (8*i)) & 0xff);
            MASSERT(byteIndex == bufferSize);
            m_pGpuDev->EscapeWriteBuffer("SetSkedReflected",  0 /*index*/, bufferSize, buffer);
            delete[] buffer;
        }
    }
    else if (IsHostReflectedSurf())
    {
        // Amodel needs to know the chid and address of the HOST surface.
        UINT32 isSupported_SetHost = 0;
        if (0 == m_pGpuDev->EscapeReadBuffer("supported/SetHostReflected", 0 /*index*/, sizeof(UINT32), &isSupported_SetHost) &&
            isSupported_SetHost)
        {
            const UINT32 chid = GetGpuManagedChId();
            const UINT64 address = pClientData->m_CtxDmaOffsetGpu;
            const size_t bufferSize = sizeof(chid) + sizeof(address);
            unsigned char *buffer = new unsigned char[bufferSize];
            size_t byteIndex = 0;
            for (UINT32 i = 0; i < sizeof(chid); i++)
                buffer[byteIndex++] = (chid >> (8*i)) & 0xff;
            for (UINT32 i = 0; i < sizeof(address); i++)
                buffer[byteIndex++] = (unsigned char)((address >> (8*i)) & 0xff);
            MASSERT(byteIndex == bufferSize);
            m_pGpuDev->EscapeWriteBuffer("SetHostReflected",  0 /*index*/, bufferSize, buffer);
            delete[] buffer;
        }
    }
}

//-----------------------------------------------------------------------------
//! Map a virtual allocation to a physical allocation.  This Surface2D
//! represents the map and the other two Surface2D objects represent the
//! virtual and physical allocations.
//!
RC Surface2D::MapVirtToPhys(GpuDevice *gpudev, Surface2D *virtAlloc,
    Surface2D *physAlloc, UINT64 virtualOffset, UINT64 physicalOffset,
    LwRm* pLwRm, bool needGmmuMap)
{
    RC rc;

    m_pGpuDev = gpudev;
    m_CpuMapOffset = physicalOffset;

    m_Location = physAlloc->GetActualLocation(m_Location, GetGpuSubdev(0));
    m_Location = physAlloc->CheckForIndividualLocationOverride(m_Location,
        GetGpuSubdev(0));

    m_AddressModel = virtAlloc->GetAddressModel();

    // This is only for Surface2D objects that represent the mapping of
    // other Surface2D allocations.
    MASSERT(IsMapOnly());
    MASSERT(virtAlloc->HasVirtual());
    MASSERT(physAlloc->HasPhysical());

    MASSERT(!m_Split);

    // Reflected maps are not supported for now.
    MASSERT(!GetSkedReflected() && !IsHostReflectedSurf());

    if (pLwRm && (pLwRm != m_pLwRm))
    {
        m_pLwRm = pLwRm;
    }

    if ((Platform::GetSimulationMode() == Platform::Amodel) &&
        (!Platform::IsSelfHosted()) && /* Allow sysmem for cyclops */
        (GetAddressModel() == Memory::Paging) &&
        (GetLocation() != Memory::Fb) &&
        m_AmodelRestrictToFb)
    {
        Printf(Tee::PriNormal, "Amodel only supports paging model for video memory; forcing address model to Memory::Fb\n");
        SetLocation(Memory::Fb);
    }

    ComputeParams();

    // update m_SplitHalfSize here. Otherwise if surface is created by mapping a virtual-only allocation to a physical-only
    // allocation, that surface will not have a chance to update m_SplitHalfSize. Then it will be treated as a split surface.
    m_SplitHalfSize = m_Size;

    UINT32 mapFlags =
        GetAccessFlagsPag()
        | GetCacheSnoopFlagsPag()
        | GetShaderAccessFlags()
        | GetPageSizeFlagsPag()
        | GetVaReverseFlags(false)
        | GetPteCoalesceFlags()
        | GetPeerMappingFlags()
        | GetPrivFlagsPag();

    AllocData *virtClientData =
        virtAlloc->GetClientData(&m_pLwRm, __FUNCTION__);

    AllocData *physClientData =
        physAlloc->GetClientData(&m_pLwRm, __FUNCTION__);

    AllocData *mapClientData = GetClientData(&m_pLwRm, __FUNCTION__);

    if (!virtClientData || !physClientData || !mapClientData)
        return RC::SOFTWARE_ERROR;

    if (m_ParentClass)
    {
        CHECK_RC(m_pLwRm->Alloc(m_pLwRm->GetDeviceHandle(GetGpuDev()),
                     &mapClientData->m_hCtxDmaGpu,
                     m_ParentClass,
                     (void*)&m_ParentClassAttr));

        m_CtxDmaAllocatedGpu = true;
    }
    else
    {
        mapClientData->m_hCtxDmaGpu =
            GetGpuDev()->GetMemSpaceCtxDma(m_Location, false, m_pLwRm);
    }

#ifdef LW_VERIF_FEATURES
    GpuSubdevice *pSubdev = GetGpuSubdev(0);

    if (m_LoopBack &&
        pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_RM_HYBRID_P2P))
    {
        GpuDevMgr *pDevMgr = ((GpuDevMgr*)DevMgrMgr::d_GraphDevMgr);
        CHECK_RC(pDevMgr->AddPeerMapping(pLwRm, pSubdev, pSubdev, m_LoopBackPeerId,
            m_IsSPAPeerAccess? GpuDevMgr::SPA : GpuDevMgr::GPA));
    }

    if (m_LoopBack && (Platform::GetSimulationMode() == Platform::Hardware))
    {
        // For now only print a warning and allow failures to occur if loopback
        // is attempted on silicon
        Printf(Tee::PriHigh,
               "Warning : Surface2D::MapPhysMemToGpu() loopback may not be "
               "supported on all hardware chipsets!\n");
    }
#endif

    // Whichever context DMA we picked had better exist!
    MASSERT(mapClientData->m_hCtxDmaGpu || mapClientData->m_GpuVirtAddr);

    // If there is an offset into the virtual allocation, callwlate the
    // new virtual address.
    if (virtualOffset > 0)
    {
        MASSERT(virtClientData->m_GpuVirtAddr != 0);

        mapClientData->m_GpuVirtAddr = virtClientData->m_GpuVirtAddr +
            virtualOffset;

        mapFlags |= DRF_DEF(OS46, _FLAGS, _DMA_OFFSET_FIXED, _TRUE);
    }
    else
    {
        mapClientData->m_GpuVirtAddr = 0;
    }

    if (m_InheritPteKind == InheritVirtual)
    {
        mapFlags |= DRF_DEF(OS46, _FLAGS, _PAGE_KIND, _VIRTUAL);
    }

    if (needGmmuMap)
    {
        MASSERT(m_Size > 0);
        CHECK_RC(m_pLwRm->MapMemoryDma(
                     virtClientData->m_hVirtMem,
                     physClientData->m_hMem,
                     physicalOffset,
                     CalcLimit(m_Size)+1,
                     mapFlags,
                     &mapClientData->m_GpuVirtAddr,
                     GetGpuDev()));
        m_VirtMemMapped = true;
    }

    mapClientData->m_CtxDmaOffsetGpu = mapClientData->m_GpuVirtAddr;
    mapClientData->m_hMem = physClientData->m_hMem;
    mapClientData->m_hVirtMem = virtClientData->m_hVirtMem;
    m_ExternalPhysMem  = physAlloc->m_ExternalPhysMem;
    SetGpuVASpace(virtAlloc->GetGpuVASpace());

    if (virtClientData->m_hTegraVirtMem)
    {
        if (virtClientData->m_hTegraVirtMem == virtClientData->m_hVirtMem)
        {
            mapClientData->m_TegraVirtAddr = mapClientData->m_GpuVirtAddr;
        }
        else
        {
            mapClientData->m_TegraVirtAddr = 0;
            CHECK_RC(m_pLwRm->MapMemoryDma(
                         virtClientData->m_hTegraVirtMem,
                         physClientData->m_hMem,
                         physicalOffset,
                         CalcLimit(m_Size)+1,
                         mapFlags,
                         &mapClientData->m_TegraVirtAddr,
                         GetGpuDev()));

            m_TegraMemMapped = true;
        }
    }

    if (virtAlloc->m_Type == LWOS32_TYPE_DEPTH)
    {
        LW0041_CTRL_GET_SURFACE_ZLWLL_ID_PARAMS ZlwllIdParams = {0};
        CHECK_RC(m_pLwRm->Control(
                    physAlloc->m_DefClientData.m_hMem,
                    LW0041_CTRL_CMD_GET_SURFACE_ZLWLL_ID,
                    &ZlwllIdParams, sizeof(ZlwllIdParams)));
        virtAlloc->m_ZLwllRegion = ZlwllIdParams.zlwllId;
    }

    // After map, query RM to reset the PTE kind if the PTE is not updated before.
    if (m_PteKind == -1 && IsGpuMapped())
    {
        GpuUtility::GetPteKind(GetCtxDmaOffsetGpu(), GetGpuVASpace(), GetGpuDev(), &m_PteKind, m_pLwRm);
    }

    return OK;
}

UINT32 Surface2D::GetActualPageSizeKB() const
{
    // Return size from previous call
    if (m_ActualPageSizeKB != ~0U)
    {
        return m_ActualPageSizeKB;
    }

    if (!m_DefClientData.m_hMem)
    {
        Printf(Tee::PriHigh, "Surface2D has not allocated yet!\n");
        return ~0U;
    }

    if (m_AddressModel == Memory::Paging)
    {
        LW0080_CTRL_DMA_GET_PTE_INFO_PARAMS params = { 0 };
        params.gpuAddr = m_DefClientData.m_CtxDmaOffsetGpu;
        params.hVASpace = GetGpuVASpace();
        if (OK == m_pLwRm->ControlByDevice(
                    GetGpuDev(), LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                    &params, sizeof(params)))
        {
            UINT32 NextPageSize = 0;
            for (UINT32 ii = 0; ii < LW0080_CTRL_DMA_GET_PTE_INFO_PTE_BLOCKS; ++ii)
            {
                if (params.pteBlocks[ii].pageSize != 0 &&
                        DRF_VAL(0080_CTRL_DMA_PTE_INFO_PARAMS, _FLAGS, _VALID,
                            params.pteBlocks[ii].pteFlags) ==
                        LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_VALID_TRUE)
                {
                    // Warn of 2 PTEs map GpuAddr to 2 physAddrs with different
                    // page sizes.   If so, need to add code to choose one
                    MASSERT((NextPageSize == 0) ||
                            (NextPageSize == params.pteBlocks[ii].pageSize));

                    NextPageSize = params.pteBlocks[ii].pageSize;
                }
            }
            m_ActualPageSizeKB = NextPageSize / 1024;
        }
        else
        {
            Printf(Tee::PriLow, "Warning: Unable to obtain page size!\n");
            m_ActualPageSizeKB = 0;
        }
    }
    else
    {
        m_ActualPageSizeKB = 0;
    }
    return m_ActualPageSizeKB;
}

//-----------------------------------------------------------------------------
RC Surface2D::MapPhysMemToIso(UINT32 SegDmaFlags, UINT32 PagDmaFlags, LwRm *pLwRm)
{
    RC rc;

    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    if (!pClientData)
        return RC::SOFTWARE_ERROR;

    // A "Displayable" surface is one that can be used with the G8x "ISO"
    // external display controller chip.
    //
    // There are all sorts of odd restrictions on ISO ctxdmas, so we can't
    // always use the same ctxdma and offset as the GPU.
    //
    // For example, in "Paging" address model, ISO needs to see a "physical"
    // ctxdma that just covers the FB.  We can't use the virtual ctxdma.
    //
    // Also, on G82, we can't use Paging at all for BlockLinear surfaces.

    // Default to the same ctxdma for both ISO and GPU for non Displayable surfaces.
    pClientData->m_CtxDmaOffsetIso = pClientData->m_CtxDmaOffsetGpu;
    pClientData->m_hCtxDmaIso = pClientData->m_hCtxDmaGpu;

    if (m_Displayable &&
        (m_Location != Memory::Fb) &&
        (false == GetGpuDev()->GetDisplay()->CanDisplaySysmem()))
    {
        Printf(Tee::PriDebug, "Can't scanout sysmem, set Displayable=false.\n");
        m_Displayable = false;
    }

    if (m_Split)
    {
        Printf(Tee::PriDebug, "Can't scanout split surface, set Displayable=false.\n");
        m_Displayable = false;
    }

    if (m_Displayable)
    {
#if defined(TEGRA_MODS)
        if (m_RouteDispRM)
        {
            CHECK_RC(ImportMemoryToDisplay());
            CHECK_RC(pLwRm->AllocDisplayContextDma(&pClientData->m_hCtxDmaIso,
                                                   SegDmaFlags,
                                                   pClientData->m_hImportMem,
                                                   m_HiddenAllocSize,
                                                   m_Size - 1 - m_HiddenAllocSize));
            m_CtxDmaAllocatedIso = true;
            pClientData->m_CtxDmaOffsetIso = 0;
            return rc;
        }
#endif
        if (GetGpuSubdev(0)->IsSOC())
        {
            pClientData->m_hCtxDmaIso      = pClientData->m_hTegraVirtMem;
            pClientData->m_CtxDmaOffsetIso = pClientData->m_TegraVirtAddr;
        }
        else
        {
            CHECK_RC(pLwRm->AllocContextDma(&pClientData->m_hCtxDmaIso,
                                            SegDmaFlags,
                                            pClientData->m_hMem,
                                            m_HiddenAllocSize,
                                            m_Size - 1 - m_HiddenAllocSize));
            m_CtxDmaAllocatedIso = true;
            pClientData->m_CtxDmaOffsetIso = 0;
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
void Surface2D::PrintParams()
{
    Printf(Tee::PriDebug, "Surf2D allocated a surface:\n");
    Printf(Tee::PriDebug, "  Size = 0x%08llx\n", m_Size);
    Printf(Tee::PriDebug, "  hMem = 0x%08x\n", m_DefClientData.m_hMem);
    Printf(Tee::PriDebug, "  hVirtMem = 0x%08x\n",
           m_DefClientData.m_hVirtMem);
    Printf(Tee::PriDebug, "  GpuVirtAddr = 0x%llx\n",
           m_DefClientData.m_GpuVirtAddr);
    if (GetGpuSubdev(0)->IsSOC())
    {
        Printf(Tee::PriDebug, "  hTegraVirtMem = 0x%08x\n",
               m_DefClientData.m_hTegraVirtMem);
        Printf(Tee::PriDebug, "  TegraVirtAddr = 0x%llx\n",
               m_DefClientData.m_TegraVirtAddr);
    }
    Printf(Tee::PriDebug, "  hCtxDmaGpu = 0x%08x\n",
           m_DefClientData.m_hCtxDmaGpu);
    Printf(Tee::PriDebug, "  OffsetGpu = 0x%llx\n",
           m_DefClientData.m_CtxDmaOffsetGpu);
    Printf(Tee::PriDebug, "  hCtxDmaIso = 0x%08x\n",
           m_DefClientData.m_hCtxDmaIso);
    Printf(Tee::PriDebug, "  OffsetIso = 0x%llx\n",
           m_DefClientData.m_CtxDmaOffsetIso);
    // print this only when we did a VidHeapAlloc
    if (m_bUseVidHeapAlloc)
    {
        Printf(Tee::PriDebug, "  VidMemOffset = 0x%08llx\n",
               m_DefClientData.m_VidMemOffset);
    }
    Printf(Tee::PriDebug, "  Displayable = %s\n", m_Displayable ? "true" : "false");
}

//-----------------------------------------------------------------------------
//! \brief Check whether the surface is suitable for peer mapping
//!
//! \param bPeer : true if the remote mapping is a peer mapping, false is a
//!                shared mapping
//!
//! \return OK if the surface is suatable for peer mapping, not OK otherwise
RC Surface2D::IsSuitableForRemoteMapping(bool bPeer)
{
    RC rc;

    // Check prerequisites.
    if (!m_DefClientData.m_hMem)
    {
        rc = RC::SOFTWARE_ERROR;
        Printf(Tee::PriHigh, "Surface2D has not allocated yet!\n");
    }
    if (bPeer && (m_Location != Memory::Fb))
    {
        rc = RC::SOFTWARE_ERROR;
        Printf(Tee::PriHigh, "Surface2D peer mapping requires FB!\n");
    }
    else if (!bPeer && (m_Location == Memory::Fb))
    {
        rc = RC::SOFTWARE_ERROR;
        Printf(Tee::PriHigh, "Surface2D shared mapping requires Sysmem!\n");
    }

    if (m_AddressModel != Memory::Paging)
    {
        rc = RC::SOFTWARE_ERROR;
        Printf(Tee::PriHigh, "Surface2D %s mapping requires paging!\n",
               bPeer ? "peer" : "shared");
    }
    if (m_Split)
    {
        rc = RC::SOFTWARE_ERROR;
        Printf(Tee::PriHigh,
               "Surface2D %s mapping does not support split surfaces!\n",
               bPeer ? "peer" : "shared");
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Map a surface (potentially onto a different gpu device)
//!
//! \param pRemoteDevice : Pointer to the remote device gaining access to this
//!                        surface (may also be the device the surface was
//!                        allocated on for a loopback mapping)
//! \param pagDmaFlags   : The dma flags to use when mapping the memory
//! \param locSD         : Local subdevice (where the memory is allocated)
//! \param remSD         : Remote subdevice (where the memory is accessed from)
//! \param bUseGlobalCtxDma : true if the global paging ctxdma should be used
//! \param pLwRm            : Pointer to RM client to create the mapping on
//!
//! \return OK if creating the remote mapping succeeds, not OK otherwise
RC Surface2D::CreateRemoteMapping(GpuDevice     *pRemoteDevice,
                                  UINT32         pagDmaFlags,
                                  UINT32         locSD,
                                  UINT32         remSD,
                                  bool           bUseGlobalCtxDma,
                                  LwRm          *pLwRm)
{
    return CreateRemoteMapping(
            pRemoteDevice,
            pagDmaFlags,
            locSD,
            remSD,
            bUseGlobalCtxDma,
            USE_DEFAULT_RM_MAPPING,
            pLwRm);
}

//--------------------------------------------------------------------------------------------------
// Iterate through the clientData's m_hVirtMemGpuRemote map searching for any RemoteMapping
// that matches the locSD,devInst and remSD parameters.
vector<Surface2D::RemoteMapping> Surface2D::GetRemoteMappings
(
    UINT32 locSD,
    UINT32 remDevInst,
    UINT32 remSD,
    const AllocData &clientData
)const
{
    vector<RemoteMapping> remoteMappings;
    map<RemoteMapping, LwRm::Handle>::const_iterator iter;
    for(iter = clientData.m_hVirtMemGpuRemote.begin();
        iter != clientData.m_hVirtMemGpuRemote.end();
        iter++)
    {
        if ((iter->first.m_LocSD == locSD) &&
            (iter->first.m_RemSD == remSD) &&
            (iter->first.m_RemD == remDevInst))
        {
            remoteMappings.push_back(iter->first);
        }
    }
    return remoteMappings;
}

//--------------------------------------------------------------------------------------------------
RC Surface2D::CreateRemoteMapping(GpuDevice     *pRemoteDevice,
                                  UINT32         pagDmaFlags,
                                  UINT32         locSD,
                                  UINT32         remSD,
                                  bool           bUseGlobalCtxDma,
                                  UINT32         peerId,
                                  LwRm          *pLwRm)

{
    RC      rc;
    UINT64  Offset, Limit, Free, Total;
    UINT32  remDev = pRemoteDevice->GetDeviceInst();
    RemoteMapping mapIndex(locSD, remDev, remSD, peerId);

    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    if (!pClientData)
    {
        MASSERT(!"Surface2D::CreateRemoteMapping: unknown client");
        return RC::SOFTWARE_ERROR;
    }

    UINT32 vidHeapAttr  = m_VidHeapAttr;
    UINT32 vidHeapAttr2 = m_VidHeapAttr2;
    LwRm::Handle objHandle = pClientData->m_hMem;

    if (m_FLAPeerMapping && (m_Location == Memory::Fb))
    {
        auto pLwLinkFla = GetGpuDev()->GetSubdevice(0)->GetInterface<LwLinkFla>();
        auto pLwLinkFlaRemote = pRemoteDevice->GetSubdevice(0)->GetInterface<LwLinkFla>();
        if ((pLwLinkFla == nullptr) ||
            !pLwLinkFla->GetFlaEnabled() ||
            (pLwLinkFlaRemote == nullptr) ||
            !pLwLinkFlaRemote->GetFlaEnabled())
        {
            Printf(Tee::PriError, "Current or remote GPU does not support FLA peer mappings!\n");
            return RC::UNSUPPORTED_FUNCTION;
        }

        LwRm::Handle hFLAVAS = m_pGpuDev->GetVASpace(FERMI_VASPACE_A,
                                                     LW_VASPACE_ALLOCATION_INDEX_GPU_FLA,
                                                     pLwRm);
        if (hFLAVAS == 0)
        {
            Printf(Tee::PriError, "FLA VAS not allocated, FLA peer mapping not supported!\n");
            return RC::UNSUPPORTED_FUNCTION;
        }

        // Export the memory to FLA on the source GPU
        LW00F8_ALLOCATION_PARAMETERS fabricParams = { };
        switch (m_FLAPageSizeKb)
        {
            case 2048:
                fabricParams.alignment   = LW_MEMORY_FABRIC_PAGE_SIZE_2M;
                fabricParams.pageSize    = LW_MEMORY_FABRIC_PAGE_SIZE_2M; 
                break;
            case 0:
            case 524288:
                fabricParams.alignment   = LW_MEMORY_FABRIC_PAGE_SIZE_512M;
                fabricParams.pageSize    = LW_MEMORY_FABRIC_PAGE_SIZE_512M; 
                break;
            default:
                Printf(Tee::PriError, "Unsupported FLA page size : %u kb\n", m_FLAPageSizeKb);
                MASSERT(!"See previous error");
                return RC::SOFTWARE_ERROR;
        }
        fabricParams.allocSize   = ALIGN_UP(GetAllocSize(), fabricParams.pageSize);
        fabricParams.map.hVidMem = GetMemHandle(pLwRm);
        fabricParams.map.offset  = 0;
        fabricParams.map.flags   = LW00F8_ALLOC_FLAGS_FORCE_CONTIGUOUS;

        // FLA is not supported in SLI (enforced in Surface2D::Alloc) so using subdevice 0
        // is fine
        CHECK_RC(pLwRm->Alloc(m_pLwRm->GetSubdeviceHandle(GetGpuDev()->GetSubdevice(0)),
                              &pClientData->m_hFLA,
                              LW_MEMORY_FABRIC,
                              &fabricParams));

        CHECK_RC(CommitFLAOnlyAttr(&vidHeapAttr, &vidHeapAttr2));
        objHandle = pClientData->m_hFLA;
    }

    // If an entry has not been created for this locSD/remD/remSD/peerId tuple, then
    // create one
    if (!pClientData->m_hVirtMemGpuRemote.count(mapIndex))
    {
        pClientData->m_hVirtMemGpuRemote[mapIndex] = 0;
        pClientData->m_CtxDmaOffsetGpuRemote[mapIndex] = 0;
        pClientData->m_GpuVirtAddrRemote[mapIndex] = 0;
    }

    if (bUseGlobalCtxDma)
    {
        pClientData->m_hCtxDmaGpuRemote[remDev] =
            pRemoteDevice->GetMemSpaceCtxDma(m_Location, false, pLwRm);
    }
    else
    {
        pClientData->m_hCtxDmaGpuRemote[remDev] = pClientData->m_hCtxDmaGpu;
    }

    if (GetUseVirtMem())
    {
        UINT32 vidHeapFlags = m_VaVidHeapFlags | LWOS32_ALLOC_FLAGS_VIRTUAL;
        if (m_IsSparse)
        {
            vidHeapFlags |= LWOS32_ALLOC_FLAGS_SPARSE;
        }

        if (GetUsePitchAlloc())
        {
            const UINT64 wholeHeight = (m_Size+m_Pitch-1) / m_Pitch;
            CHECK_RC(pLwRm->VidHeapAllocPitchHeightEx(
                    m_Type,
                    vidHeapFlags,
                    &vidHeapAttr,
                    &vidHeapAttr2,
                    UNSIGNED_CAST(UINT32, wholeHeight),
                    &m_Pitch,
                    m_VidHeapAlign,
                    &m_PteKind,
                    &m_VidHeapCoverage,
                    &m_PartStride,
                    m_CtagOffset,
                    &pClientData->m_hVirtMemGpuRemote[mapIndex],
                    &Offset,
                    &Limit,
                    &Free,
                    &Total,
                    pRemoteDevice,
                    0,
                    0,
                    GetPrimaryVASpaceHandle()));
        }
        else
        {
            CHECK_RC(pLwRm->VidHeapAllocSizeEx(
                    m_Type,
                    vidHeapFlags,
                    &vidHeapAttr,
                    &vidHeapAttr2,
                    m_Size,
                    m_VidHeapAlign,
                    &m_PteKind,
                    &m_VidHeapCoverage,
                    &m_PartStride,
                    m_CtagOffset,
                    &pClientData->m_hVirtMemGpuRemote[mapIndex],
                    &Offset,
                    &Limit,
                    &Free,
                    &Total,
                    0, // w
                    0, // h
                    pRemoteDevice,
                    0,
                    0,
                    GetPrimaryVASpaceHandle()));
        }
        CHECK_RC(pLwRm->MapMemoryDma(pClientData->m_hVirtMemGpuRemote[mapIndex],
                    objHandle,
                    m_HiddenAllocSize,
                    m_Size - m_HiddenAllocSize,
                    pagDmaFlags,
                    &pClientData->m_GpuVirtAddrRemote[mapIndex],
                    pRemoteDevice));

        pClientData->m_CtxDmaOffsetGpuRemote[mapIndex] =
            pClientData->m_GpuVirtAddrRemote[mapIndex];
    }
    else
    {
        CHECK_RC(pLwRm->MapMemoryDma(
                pClientData->m_hCtxDmaGpuRemote[remDev],
                pClientData->m_hMem,
                m_HiddenAllocSize,
                m_Size - m_HiddenAllocSize,
                pagDmaFlags,
                &pClientData->m_CtxDmaOffsetGpuRemote[mapIndex],
                pRemoteDevice));
    }

    if ((pLwRm == m_pLwRm) && (m_PeerClientData.size() > 0))
    {
        map<LwRm *, AllocData>::iterator pRmClientData;
        for (pRmClientData = m_PeerClientData.begin();
             pRmClientData != m_PeerClientData.end(); pRmClientData++)
        {
            CHECK_RC(CreateRemoteMapping(pRemoteDevice,
                                         pagDmaFlags,
                                         locSD, remSD, bUseGlobalCtxDma,
                                         peerId,
                                         pRmClientData->first));
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC Surface2D::MapPeerInternal(LwRm *pLwRm)
{
    RC rc = OK;
    GpuDevice * pDevice = GetGpuDev();

    UINT32 PagDmaFlags = DRF_DEF(OS46, _FLAGS, _P2P_ENABLE, _SLI) |
                         DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _DISABLE) |
                         GetAccessFlagsPag() |
                         GetShaderAccessFlags();

    for (UINT32 subdev = 0; subdev < pDevice->GetNumSubdevices(); subdev++)
    {
        CHECK_RC(CreateRemoteMapping(pDevice,
                         PagDmaFlags | DRF_NUM(OS46, _FLAGS, _P2P_SUBDEV_ID_SRC, subdev),
                         subdev, 0, false, pLwRm));

        if (pLwRm == m_pLwRm)
        {
            RemoteMapping mapIndex(subdev, pDevice->GetDeviceInst(), 0,USE_DEFAULT_RM_MAPPING);
            Printf(Tee::PriDebug,
                   " Other-peer access to subdev %d's surf at 0x%llx\n",
                   subdev,
                   m_DefClientData.m_CtxDmaOffsetGpuRemote[mapIndex]);
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC Surface2D::MapPeerInternal(GpuDevice *pRemoteDev, LwRm *pLwRm)
{
    RC rc = OK;
    GpuDevMgr *pDevMgr = ((GpuDevMgr*)DevMgrMgr::d_GraphDevMgr);
    UINT32 PagDmaFlags = DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _DISABLE) |
                         GetAccessFlagsPag() |
                         GetShaderAccessFlags();
    GpuDevice *pLocalDev = GetGpuDev();
    UINT32 RemoteInst = pRemoteDev->GetDeviceInst();

    for (UINT32 locSD = 0; locSD < pLocalDev->GetNumSubdevices(); locSD++)
    {
        for (UINT32 remSD = 0; remSD < pRemoteDev->GetNumSubdevices(); remSD++)
        {
            CHECK_RC(pDevMgr->AddPeerMapping(pLwRm,
                                             pLocalDev->GetSubdevice(locSD),
                                             pRemoteDev->GetSubdevice(remSD)));

            CHECK_RC(CreateRemoteMapping(pRemoteDev,
                         PagDmaFlags |
                             DRF_DEF(OS46, _FLAGS, _P2P_ENABLE, _NOSLI) |
                             DRF_NUM(OS46, _FLAGS, _P2P_SUBDEV_ID_SRC, locSD) |
                             DRF_NUM(OS46, _FLAGS, _P2P_SUBDEV_ID_TGT, remSD),
                         locSD, remSD, true, pLwRm));

            if (pLwRm == m_pLwRm)
            {
                RemoteMapping mapIndex(locSD, RemoteInst, remSD,USE_DEFAULT_RM_MAPPING);
                Printf(Tee::PriDebug,
                       " Other-peer (%d:%d) access to subdevice %d:%d's surf at"
                       " 0x%llx\n",
                       RemoteInst, remSD,
                       pLocalDev->GetDeviceInst(), locSD,
                       m_DefClientData.m_CtxDmaOffsetGpuRemote[mapIndex]);
            }
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC Surface2D::MapLoopbackInternal(LwRm *pLwRm,UINT32 loopbackPeerId)
{
    RC rc;
    GpuDevice * pDevice = GetGpuDev();
    GpuDevMgr *pDevMgr = ((GpuDevMgr*)DevMgrMgr::d_GraphDevMgr);
    UINT32 p2pFlags;

#ifdef LW_VERIF_FEATURES
    if (loopbackPeerId != USE_DEFAULT_RM_MAPPING)
    {
        p2pFlags = DRF_DEF(OS46, _FLAGS, _P2P_ENABLE, _LOOPBACK) |
                   DRF_NUM(OS46, _FLAGS, _P2P_LOOPBACK_PEER_ID, loopbackPeerId);
    }
    else
#endif
    {
        p2pFlags = DRF_DEF(OS46, _FLAGS, _P2P_ENABLE, _NOSLI) |
                   DRF_NUM(OS46, _FLAGS, _P2P_SUBDEV_ID_SRC, 0) |
                   DRF_NUM(OS46, _FLAGS, _P2P_SUBDEV_ID_TGT, 0);
    }

    UINT32 PagDmaFlags = DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _DISABLE) |
                         p2pFlags |
                         GetAccessFlagsPag() |
                         GetShaderAccessFlags();

    CHECK_RC(pDevMgr->AddPeerMapping(pLwRm,
                                     pDevice->GetSubdevice(0),
                                     pDevice->GetSubdevice(0),
                                     loopbackPeerId));

    CHECK_RC(CreateRemoteMapping(pDevice, PagDmaFlags, 0, 0, false, loopbackPeerId, pLwRm));

    return rc;
}

//-----------------------------------------------------------------------------
RC Surface2D::MapSharedInternal(GpuDevice *pSharedDev, LwRm *pLwRm)
{
    RC rc;
    const UINT32 PagDmaFlags =
        GetAccessFlagsPag()
        | GetCacheSnoopFlagsPag()
        | GetShaderAccessFlags()
        | GetPageSizeFlagsPag()
        | GetVaReverseFlags(false)
        | GetPteCoalesceFlags()
        | GetPeerMappingFlags()
        | GetPrivFlagsPag();

    CHECK_RC(CreateRemoteMapping(pSharedDev, PagDmaFlags, 0, 0, true, pLwRm));

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Unmap a remote mapped surface (potentially from a different gpu device)
//!
//! \param pDevice    : The GPU device that remote mapped this surface
//!
void Surface2D::UnmapRemote(GpuDevice *pDevice)
{
    if (!IsPeerMapped(pDevice) && !IsMapShared(pDevice))
        return;

    map<LwRm *, AllocData>::iterator pRmClientData;
    for (pRmClientData = m_PeerClientData.begin();
         pRmClientData != m_PeerClientData.end(); pRmClientData++)
    {
        UnmapRemoteInternal(pDevice, pRmClientData->first, pRmClientData->second);
    }

    UnmapRemoteInternal(pDevice, m_pLwRm, m_DefClientData);
}

void Surface2D::UnmapRemoteInternal
(
    GpuDevice *pDevice,
    LwRm *pLwRm,
    const AllocData &clientData
)
{
    UINT32 devInst = pDevice->GetDeviceInst();
    map<RemoteMapping, LwRm::Handle>::const_iterator pRemVirtMem;
    map<RemoteMapping, UINT64>::const_iterator pRemAddress;
    map<UINT32, LwRm::Handle>::const_iterator pRemCtxDma;

    UINT32 remSDCount = pDevice->GetNumSubdevices();
    UINT32 locSDCount = GetGpuDev()->GetNumSubdevices();

    // Shared system memory mappings only use one entry since it does
    // not require per-subdevice mappings
    if (IsMapShared(pDevice))
    {
        remSDCount = locSDCount = 1;
    }
    // Loopback peer mappings always use index 0 of the remote subdevice
    else if (GetGpuDev() == pDevice)
    {
        remSDCount = 1;
    }

    for (UINT32 remSD = 0; remSD < remSDCount; remSD++)
    {
        for (UINT32 locSD = 0; locSD < locSDCount; locSD++)
        {
            vector<RemoteMapping> mapIndices = GetRemoteMappings(locSD,devInst,remSD,clientData);
            for (UINT32 ii=0; ii < (UINT32)mapIndices.size(); ii++)
            {
                if (GetUseVirtMem())
                {
                    pRemVirtMem = clientData.m_hVirtMemGpuRemote.find(mapIndices[ii]);
                    MASSERT(pRemVirtMem != clientData.m_hVirtMemGpuRemote.end());

                    pRemAddress = clientData.m_GpuVirtAddrRemote.find(mapIndices[ii]);
                    MASSERT(pRemAddress != clientData.m_GpuVirtAddrRemote.end());

                    LwRm::Handle memHandle = clientData.m_hMem;
                    if (m_FLAPeerMapping)
                    {
                        MASSERT(clientData.m_hFLA);
                        memHandle = clientData.m_hFLA;
                    }
                    pLwRm->UnmapMemoryDma(pRemVirtMem->second,
                                          memHandle,
                                          0,
                                          pRemAddress->second,
                                          pDevice);
                    pLwRm->Free(pRemVirtMem->second);
                }
                else
                {
                    pRemCtxDma = clientData.m_hCtxDmaGpuRemote.find(devInst);
                    MASSERT(pRemCtxDma != clientData.m_hCtxDmaGpuRemote.end());

                    pRemAddress = clientData.m_CtxDmaOffsetGpuRemote.find(mapIndices[ii]);
                    MASSERT(pRemAddress != clientData.m_CtxDmaOffsetGpuRemote.end());

                    pLwRm->UnmapMemoryDma(pRemCtxDma->second,
                                          clientData.m_hMem,
                                          0,
                                          pRemAddress->second,
                                          pDevice);
                }
            }
        }
    }
}

//-----------------------------------------------------------------------------
void Surface2D::SetCompressed(bool Compressed)
{
    m_Compressed = Compressed;
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetAAWidthScale() const
{
    switch (m_AAMode)
    {
        case AA_1:
            return 1;

        case AA_2:
        case AA_4:
        case AA_4_Rotated:
        case AA_4_Virtual_8:
        case AA_4_Virtual_16:
            return 2;

        case AA_8:
        case AA_8_Virtual_16:
        case AA_8_Virtual_32:
        case AA_16:
            return 4;

        default:
            return 1;
    }
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetAAHeightScale() const
{
    switch (m_AAMode)
    {
        case AA_1:
        case AA_2:
            return 1;

        case AA_4:
        case AA_4_Rotated:
        case AA_4_Virtual_8:
        case AA_4_Virtual_16:
        case AA_8:
        case AA_8_Virtual_16:
        case AA_8_Virtual_32:
            return 2;

        case AA_16:
            return 4;

        default:
            return 1;
    }
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetAASamples() const
{
    switch (m_AAMode)
    {
        case AA_1:
            return 1;
        case AA_2:
            return 2;
        case AA_4:
        case AA_4_Rotated:
        case AA_4_Virtual_8:
        case AA_4_Virtual_16:
            return 4;
        case AA_6:
            return 6;
        case AA_8_Virtual_16:
        case AA_8_Virtual_32:
            return 8;
        case AA_16:
            return 16;
        default:
            return 0;
    }
}

//-----------------------------------------------------------------------------
void Surface2D::SetAASamples(UINT32 Samples)
{
    switch (Samples)
    {
        case 1:
            m_AAMode = AA_1; break;
        case 2:
            m_AAMode = AA_2; break;
        case 4:
            m_AAMode = AA_4; break;
        case 6:
            m_AAMode = AA_6; break;
        case 8:
            m_AAMode = AA_8; break;
        case 16:
            m_AAMode = AA_16; break;
        default:
            Printf(Tee::PriHigh, "Invalid number of AA samples\n");
            MASSERT(!"Generic assertion failure<Refer to previous message>.");
            m_AAMode = AA_1;
            break;
    }
}

//-----------------------------------------------------------------------------
RC Surface2D::ChangePteKind(INT32 PteKind)
{
    if (GetPteKind() == PteKind)
        return OK;
    UINT64 offset = 0;
    RC rc;
    while (offset < GetAllocSize())
    {
        LW0080_CTRL_DMA_SET_PTE_INFO_PARAMS setParams = {0};
        LW0080_CTRL_DMA_GET_PTE_INFO_PARAMS getParams = {0};
        getParams.gpuAddr = GetCtxDmaOffsetGpu()
            + offset;
        CHECK_RC(m_pLwRm->ControlByDevice(
                    GetGpuDev(),
                    LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                    &getParams,sizeof(getParams)));
        setParams.gpuAddr = getParams.gpuAddr;
        for (UINT32 blockIndex = 0;
                blockIndex < LW0080_CTRL_DMA_GET_PTE_INFO_PTE_BLOCKS; ++blockIndex)
        {
            setParams.pteBlocks[blockIndex] =
                getParams.pteBlocks[blockIndex];
            setParams.pteBlocks[blockIndex].kind = PteKind;
        }
        CHECK_RC(m_pLwRm->ControlByDevice(
                    GetGpuDev(),
                    LW0080_CTRL_CMD_DMA_SET_PTE_INFO,
                    &setParams,sizeof(setParams)));
        offset += GetActualPageSizeKB()*1024;
    }
    SetPteKind(PteKind);
    return rc;
}

//-----------------------------------------------------------------------------
//! Estimate how big a surface will be, even if it hasn't been
//! allocated yet.
//!
UINT64 Surface2D::EstimateSize(GpuDevice *pGpuDev) const
{
    MASSERT(pGpuDev != NULL);
    UINT64 size = 0;

    if (m_DefClientData.m_hMem == 0)
    {
        // If the surface hasn't been allocated yet, then make a
        // temporary copy and use ComputeParams() to callwlate the
        // size.
        //
        Surface2D tmpSurface = *this;
        tmpSurface.m_pGpuDev = pGpuDev;
        tmpSurface.ComputeParams();
        size = tmpSurface.GetSize();
    }
    else
    {
        // If the surface has been allocated already, then just return
        // the size.
        //
        MASSERT(m_pGpuDev == pGpuDev);
        size = m_Size;
    }

    return size;
}

LwRm::Handle Surface2D::GetMemHandle(LwRm *pLwRm) const
{
    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    return pClientData ? pClientData->m_hMem : 0;
}

LwRm::Handle Surface2D::GetVirtMemHandle(LwRm *pLwRm) const
{
    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    return pClientData ? pClientData->m_hVirtMem : 0;
}

LwRm::Handle Surface2D::GetTegraMemHandle(LwRm *pLwRm) const
{
    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    return pClientData ? pClientData->m_hTegraVirtMem : 0;
}

//-----------------------------------------------------------------------------
UINT64 Surface2D::GetPartOffset(UINT32 part) const
{
    switch (part)
    {
        case 0: return 0;
        case 1: return m_SplitHalfSize - m_HiddenAllocSize - m_ExtraAllocSize;
        default: return GetSize();
    }
}

//-----------------------------------------------------------------------------
UINT64 Surface2D::GetPartSize(UINT32 part) const
{
    switch (part)
    {
        case 0: return m_SplitHalfSize - m_HiddenAllocSize - m_ExtraAllocSize - m_CdeAllocSize;
        case 1: return m_Size - m_SplitHalfSize;
        default: return 0;
    }
}

LwRm::Handle Surface2D::GetSplitMemHandle(LwRm *pLwRm) const
{
    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    return pClientData ? pClientData->m_hSplitMem : 0;
}
UINT64 Surface2D::GetSplitVidMemOffset(LwRm *pLwRm) const
{
    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    return pClientData ? pClientData->m_SplitVidMemOffset : 0;
}

//! Deprecated, equivalent to GetCtxDmaHandleIso.
LwRm::Handle Surface2D::GetCtxDmaHandle(LwRm *pLwRm) const
{
    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    return pClientData ? pClientData->m_hCtxDmaIso : 0;
}

//! Get ctxdma handle to be used with the GPU.
LwRm::Handle Surface2D::GetCtxDmaHandleGpu(LwRm *pLwRm) const
{
    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    return pClientData ? pClientData->m_hCtxDmaGpu : 0;
}

//! Get ctxdma handle to be used with the ISO display chip.
LwRm::Handle Surface2D::GetCtxDmaHandleIso(LwRm *pLwRm) const
{
    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    return pClientData ? pClientData->m_hCtxDmaIso : 0;
}

UINT64 Surface2D::GetVidMemOffset(LwRm *pLwRm) const
{
    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    return pClientData ? pClientData->m_VidMemOffset : 0;
}

//! \brief Return physical address of a byte at the given surface offset.
//!
//! The surface can be either in vidmem or in sysmem and does not
//! need be contiguous.
//!
//! \param virtOffset Offset within the surface, regardless of its virtual mapping.
//! \param pPhysAddr Returned physical address.
//! \param pLwRm (optional) RM client.
//!
RC Surface2D::GetPhysAddress(UINT64 virtOffset, PHYSADDR *pPhysAddr, LwRm *pLwRm) const
{
    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    MASSERT(pClientData);
    if (!pClientData)
    {
        return RC::SOFTWARE_ERROR;
    }

    if (virtOffset >= m_Size)
    {
        Printf(Tee::PriHigh, "Error: Surface offset exceeds surface size\n");
        return RC::SOFTWARE_ERROR;
    }

    if (m_ExternalPhysMem != 0)
    {
        *pPhysAddr = m_DefClientData.m_VidMemOffset + virtOffset;
        return OK;
    }

    LwRm::Handle hMem = pClientData->m_hMem;
    if (m_Split && (virtOffset >= m_SplitHalfSize))
    {
        virtOffset -= m_SplitHalfSize;
        hMem = pClientData->m_hSplitMem;
    }

    if (m_bUseVidHeapAlloc)
    {
        LW0041_CTRL_GET_SURFACE_PHYS_ATTR_PARAMS params = {0};
        params.memOffset  = virtOffset;
        params.mmuContext = TEGRA_VASPACE_A;  // CPU view of memory
        RC rc;
        CHECK_RC(pLwRm->Control(hMem, LW0041_CTRL_CMD_GET_SURFACE_PHYS_ATTR,
                                &params, sizeof(params)));
        *pPhysAddr = params.memOffset;
    }
    else
    {
        LW003E_CTRL_GET_SURFACE_PHYS_ATTR_PARAMS params = {0};
        params.memOffset  = virtOffset;
        params.mmuContext = TEGRA_VASPACE_A;  // CPU view of memory
        RC rc;
        CHECK_RC(pLwRm->Control(hMem, LW003E_CTRL_CMD_GET_SURFACE_PHYS_ATTR,
                                &params, sizeof(params)));
        *pPhysAddr = params.memOffset;
    }

    return OK;
}

//! \brief Return the device physical address of a byte at the given surface offset.
//!
//! The surface can be either in vidmem or in sysmem and does not
//! need be contiguous.
//!
//! \param virtOffset Offset within the surface, regardless of its virtual mapping.
//! \param vaSpace    VASpace Type on which the device is operating on.
//!                   GPUVASpace     - indicate GPU physical address.
//!                   TegraVASpace   - indicate non-GPU engine physical address.
//! \param pPhysAddr  Returned physical address.
//! \param pLwRm      (optional) RM client.
//!
RC Surface2D::GetDevicePhysAddress(UINT64 virtOffset, VASpace vaSpace, PHYSADDR *pPhysAddr, LwRm *pLwRm) const
{
    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    MASSERT(pClientData);
    if (!pClientData)
    {
        return RC::SOFTWARE_ERROR;
    }

    if (virtOffset >= m_Size)
    {
        Printf(Tee::PriHigh, "Error: Surface offset exceeds surface size\n");
        return RC::SOFTWARE_ERROR;
    }

    if (m_ExternalPhysMem != 0)
    {
        *pPhysAddr = m_DefClientData.m_VidMemOffset + virtOffset;
        return OK;
    }

    LwRm::Handle hMem = pClientData->m_hMem;
    if (m_Split && (virtOffset >= m_SplitHalfSize))
    {
        virtOffset -= m_SplitHalfSize;
        hMem = pClientData->m_hSplitMem;
    }

    if (m_bUseVidHeapAlloc)
    {
        UINT32                                   mmuContext   = 0;
        LW0041_CTRL_GET_SURFACE_PHYS_ATTR_PARAMS params       = {0};
        RC                                       rc;
        const LwRm::Handle                       tegraVASpace = m_pGpuDev->GetVASpace(TEGRA_VASPACE_A, m_pLwRm);

        switch (vaSpace)
        {
            case GPUVASpace:
                if (m_VASpace == TegraVASpace)
                {
                    // This option is not supported on CheetAh-only surfaces
                    return RC::ILWALID_ARGUMENT;
                }
                MASSERT(m_pGpuDev->GetVASpace(FERMI_VASPACE_A, m_pLwRm));
                mmuContext = FERMI_VASPACE_A;
                break;

            case TegraVASpace:
                if (!tegraVASpace)
                {
                    // This option is not supported on GPU-only surfaces
                    return RC::ILWALID_ARGUMENT;
                }
                mmuContext = TEGRA_VASPACE_A;
                break;

            default:
                MASSERT(!"Unsupported VA space type !");
                return RC::ILWALID_ARGUMENT;
                break;
        }

        params.memOffset  = virtOffset;
        params.mmuContext = mmuContext;

        CHECK_RC(pLwRm->Control(hMem, LW0041_CTRL_CMD_GET_SURFACE_PHYS_ATTR,
                                &params, sizeof(params)));
        *pPhysAddr = params.memOffset;
    }
    else
    {
        LW003E_CTRL_GET_SURFACE_PHYS_ATTR_PARAMS params = {0};
        params.memOffset = virtOffset;
        RC rc;
        CHECK_RC(pLwRm->Control(hMem, LW003E_CTRL_CMD_GET_SURFACE_PHYS_ATTR,
                                &params, sizeof(params)));
        *pPhysAddr = params.memOffset;
    }

    return OK;
}

//! Deprecated, equivalent to GetCtxDmaOffsetIso.
UINT64 Surface2D::GetOffset(LwRm *pLwRm) const
{
    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    return pClientData ? pClientData->m_CtxDmaOffsetIso : 0;
}

//! Get ctxdma offset (in bytes) of the surface, for use with the GPU.
UINT64 Surface2D::GetCtxDmaOffsetGpu(LwRm *pLwRm) const
{
    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    return pClientData ? pClientData->m_CtxDmaOffsetGpu : 0;
}

//! Get ctxdma offset (in bytes) of the surface, for use with the ISO display chip.
UINT64 Surface2D::GetCtxDmaOffsetIso(LwRm *pLwRm) const
{
    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    return pClientData ? pClientData->m_CtxDmaOffsetIso : 0;
}

//! Get ctxdma offset (in bytes) of the surface, for use with CheetAh engines.
UINT64 Surface2D::GetTegraVirtAddr(LwRm *pLwRm) const
{
    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    return pClientData ? pClientData->m_TegraVirtAddr : 0;
}

//-----------------------------------------------------------------------------
void Surface2D::ComputeParams()
{
    // Just compute size if dimensions were not given
    if ((m_Width == 0) || (m_Height == 0))
    {
        // For BL surface w/o width or height, change its arraypitch to gob aligned
        // Here limit the aligment to sim mods first and remove the limitation once
        // the crc mismatch issue for mods sanity tests is resolved. The crc mismatch
        // isssue is tracked on bug 200619571.
        const bool isSim = (Xp::GetOperatingSystem() == Xp::OS_LINUXSIM ||
                            Xp::GetOperatingSystem() == Xp::OS_WINSIM);
        if (isSim && m_Layout == BlockLinear)
        {
            UINT64 alignedArrayPitch = ALIGN_UP(m_ArrayPitch, GetGpuDev()->GobSize());
            if (m_ArrayPitch != alignedArrayPitch)
            {
                Printf(Tee::PriHigh, "Surface %s arraypitch is aligned up to GOB size and is changed from 0x%llx to 0x%llx\n",
                        GetName().c_str(), m_ArrayPitch, alignedArrayPitch);
                m_ArrayPitch = alignedArrayPitch;
            }
        }

        m_Size = m_ArrayPitch * m_ArraySize
            + m_ExtraAllocSize + m_HiddenAllocSize;
        return;
    }

    // Compute various surface parameters

    // Some tests may want to allocate 2D surfaces that are larger than
    // the actually used image area.  Add the extra width and height here.
    m_AllocWidth = m_Width + m_ExtraWidth;
    m_AllocHeight = m_Height + m_ExtraHeight;
    m_AllocDepth = m_Depth;
    const UINT32 bitsPerPixel = GetBitsPerPixel();
    MASSERT(bitsPerPixel != 0);

    if (GetCreatedFromAllocSurface())
    {
        // ALLOC_SURFACE is going to allocate all kinds of buffers including
        // textures, we MODS has to callwlate the surface size based on the
        // trace commands, mip-levels/lwbe-map/block-shrunk/border now only
        // make sense for textures, but we need to use the unified interface.
        SurfaceFormat *formatHelper = SurfaceFormat::CreateFormat(this, GetGpuDev());
        MASSERT(formatHelper);
        UINT64 size = (UINT64)formatHelper->GetSize() / m_ArraySize; // Size of array
        UINT64 array_pitch = size / m_Dimensions;

        // Some 2D images may have pixel compression (e.g., ASTC texture
        // formats), so the allocated width and height may differ from the
        // pixel/texel width and height.
        // Note that The format helper object accounts for m_ExtraWidth and
        // m_ExtraHeight, so they don't need to be added here.
        m_AllocWidth = formatHelper->GetScaledWidth();
        m_AllocHeight = formatHelper->GetScaledHeight();

        switch (m_Layout)
        {
            case Pitch:
                if (m_Tiled)
                {
                    m_AllocHeight = AlignUp<16>(m_AllocHeight);
                }
                m_Pitch = UINT32(array_pitch / (m_AllocHeight * m_AllocDepth));
                break;
            case BlockLinear:
            {
                // Lwrrently RT doesn't need to be mip-mapped, so there's no
                // block shrunking happens for RT, the old logic still works.
                // For texture, some block info is up to change after layouting
                // We just keep the old logic.
                UINT32 BytesPerBlockX = GetGpuDev()->GobWidth() << m_LogBlockWidth;
                UINT32 PixelsPerBlockY =
                    GetGpuDev()->GobHeight() << m_LogBlockHeight;
                UINT32 PixelsPerBlockZ =
                    GetGpuDev()->GobDepth() << m_LogBlockDepth;

                // Round up to block size
                m_Pitch = (m_AllocWidth * bitsPerPixel) / 8;
                m_Pitch = ALIGN_UP(m_Pitch, BytesPerBlockX);
                m_AllocWidth = (m_Pitch * 8) / bitsPerPixel;
                m_AllocHeight = ALIGN_UP(m_AllocHeight, PixelsPerBlockY);
                m_AllocDepth = ALIGN_UP(m_AllocDepth, PixelsPerBlockZ);

                // Store width/height in units of gobs and blocks
                m_GobWidth = ((m_AllocWidth * bitsPerPixel) / 8) / GetGpuDev()->GobWidth();
                m_GobHeight = m_AllocHeight / GetGpuDev()->GobHeight();
                m_GobDepth = m_AllocDepth / GetGpuDev()->GobDepth();
                m_BlockWidth = m_GobWidth >> m_LogBlockWidth;
                m_BlockHeight = m_GobHeight >> m_LogBlockHeight;
                m_BlockDepth = m_GobDepth >> m_LogBlockDepth;
                break;
            }
            case Tiled:
            {
                // Pitch must be computed by formula
                m_Pitch = (m_AllocWidth * bitsPerPixel) / 8;
                UINT32 newPitch = m_Pitch;
                if (OK != GetTiledPitch(m_Pitch, &newPitch))
                {
                    MASSERT(!"Could not find an appropriate tiled pitch");
                }
                m_Pitch = newPitch;
                m_AllocWidth = (m_Pitch * 8) / bitsPerPixel;
                m_AllocHeight = ALIGN_UP(m_AllocHeight, GetGpuDev()->TileHeight());
                m_Size = (UINT64)m_Pitch * m_AllocHeight * m_AllocDepth;
                break;
            }
            default:
                MASSERT(!"Invalid layout");
        }
        delete formatHelper;

        if (m_ArrayPitchAlignment)
        {
            size = ALIGN_UP(size, m_ArrayPitchAlignment);
            m_ArrayPitch = ALIGN_UP(m_ArrayPitch, m_ArrayPitchAlignment);
        }
        if (size > m_ArrayPitch)
        {
            if (m_ArrayPitch != 0)
            {
                Printf(Tee::PriHigh, "The specified array pitch is too small, overriding\n");
            }
            m_ArrayPitch = size;
        }
        else if (size < m_ArrayPitch)
        {
            size = m_ArrayPitch;
        }
        m_Size = size * m_ArraySize;
        m_Size += m_ExtraAllocSize + m_HiddenAllocSize;
        if (m_bCdeAlloc)
        {
            m_CdeAllocSize = ALIGN_UP(m_Size, CdeAllocAlignment) - m_Size;
            m_CdeAllocSize += GetCdeAllocSize(CDE_HORZ);
            m_CdeAllocSize += GetCdeAllocSize(CDE_VERT);
            m_Size += m_CdeAllocSize;
        }

        return;
    }

    switch (m_Layout)
    {
        case Pitch:
            // If pitch is set to a nonzero value, that's an explicitly specified
            // pitch; we use it as is.  If pitch is set to zero, then we
            // automatically pick an appropriate pitch.
            if (m_Pitch == 0)
            {
                m_Pitch = (m_AllocWidth * bitsPerPixel) / 8;
                if (m_Displayable &&
                    (GetGpuSubdev(0)->
                            HasFeature(Device::GPUSUB_HAS_ISO_DISPLAY)))
                {
                    // G80 displayable surfaces must have a 256-byte aligned pitch
                    m_Pitch = (m_Pitch + 0xFF) & ~0xFF;
                }
                else if (m_Displayable && GetGpuSubdev(0)->IsSOC() &&
                        (GetGpuDev()->GetDisplay()->GetDisplayClassFamily() ==
                            Display::LWDISPLAY))
                {
                    if (m_Type == LWOS32_TYPE_LWRSOR)
                    {
                        // Orin - 256B aligned cursor surface
                        m_Pitch = (m_Pitch + 0xFF) & ~0xFF;
                    }
                    else
                    {
                        // Orin - 128B aligned non cursor surface
                        m_Pitch = (m_Pitch + 0x7F) & ~0x7F;
                    }
                }
                else
                {
                    // Most surfaces only need 64-byte pitch alignment
                    m_Pitch = (m_Pitch + 0x3F) & ~0x3F;
                }
            }
            if (m_Tiled)
            {
                m_AllocHeight = AlignUp<16>(m_AllocHeight);
            }
            m_Size = (UINT64)m_Pitch * m_AllocHeight * m_AllocDepth;
            break;
        case Swizzled:
            // Round up to next power of two
            m_AllocWidth = m_AllocHeight = m_AllocDepth = m_ArraySize = 1;
            while (m_AllocWidth < m_Width+m_ExtraWidth)
                m_AllocWidth <<= 1;
            while (m_AllocHeight < m_Height+m_ExtraHeight)
                m_AllocHeight <<= 1;
            m_Pitch = ((m_Width+m_ExtraWidth) * bitsPerPixel) / 8;
            m_Size = (UINT64)m_Pitch * (m_Height+m_ExtraHeight);
            break;
        case BlockLinear:
        {
            UINT32 BytesPerBlockX = GetGpuDev()->GobWidth() << m_LogBlockWidth;
            UINT32 PixelsPerBlockY =
                GetGpuDev()->GobHeight() << m_LogBlockHeight;
            UINT32 PixelsPerBlockZ =
                GetGpuDev()->GobDepth() << m_LogBlockDepth;

            // Round up to block size
            m_Pitch = (m_AllocWidth * bitsPerPixel) / 8;
            m_Pitch = (m_Pitch + BytesPerBlockX-1) & ~(BytesPerBlockX-1);
            m_AllocWidth = (m_Pitch * 8) / bitsPerPixel;
            m_AllocHeight = (m_AllocHeight + PixelsPerBlockY-1) & ~(PixelsPerBlockY-1);
            m_AllocDepth = (m_AllocDepth + PixelsPerBlockZ-1) & ~(PixelsPerBlockZ-1);
            m_Size = (UINT64)m_Pitch * m_AllocHeight * m_AllocDepth;

            // Store width/height in units of gobs and blocks
            m_GobWidth = ((m_AllocWidth * bitsPerPixel) / 8) / GetGpuDev()->GobWidth();
            m_GobHeight = m_AllocHeight / GetGpuDev()->GobHeight();
            m_GobDepth = m_AllocDepth / GetGpuDev()->GobDepth();
            m_BlockWidth = m_GobWidth >> m_LogBlockWidth;
            m_BlockHeight = m_GobHeight >> m_LogBlockHeight;
            m_BlockDepth = m_GobDepth >> m_LogBlockDepth;
            break;
        }
        case Tiled:
        {
            // Pitch must be computed by formula
            m_Pitch = (m_AllocWidth * bitsPerPixel) / 8;
            UINT32 newPitch = m_Pitch;
            if (OK != GetTiledPitch(m_Pitch, &newPitch))
            {
                MASSERT(!"Could not find an appropriate tiled pitch");
            }
            m_Pitch = newPitch;
            m_AllocWidth = (m_Pitch * 8) / bitsPerPixel;
            m_AllocHeight = ALIGN_UP(m_AllocHeight, GetGpuDev()->TileHeight());
            m_Size = (UINT64)m_Pitch * m_AllocHeight * m_AllocDepth;
            break;
        }        default:
            MASSERT(!"Invalid layout");
    }
    if (m_ArrayPitchAlignment)
    {
        // Both size and arraypitch need to set to the desired alignment
        m_Size = (m_Size + m_ArrayPitchAlignment - 1) & ~(m_ArrayPitchAlignment - 1);
        m_ArrayPitch = (m_ArrayPitch + m_ArrayPitchAlignment - 1) & ~(m_ArrayPitchAlignment - 1);
    }
    if (m_Size > m_ArrayPitch)
    {
        if (m_ArrayPitch != 0)
        {
            Printf(Tee::PriHigh, "The specified array pitch is too small, overriding\n");
        }
        m_ArrayPitch = m_Size;
    }
    else if (m_Size < m_ArrayPitch)
    {
        m_Size = m_ArrayPitch;
    }
    m_Size *= m_ArraySize;
    m_Size += m_ExtraAllocSize + m_HiddenAllocSize;
    if (m_bCdeAlloc)
    {
        m_CdeAllocSize = ALIGN_UP(m_Size, CdeAllocAlignment) - m_Size;
        m_CdeAllocSize += GetCdeAllocSize(CDE_HORZ);
        m_CdeAllocSize += GetCdeAllocSize(CDE_VERT);
        m_Size += m_CdeAllocSize;
    }
}

//-----------------------------------------------------------------------------
void Surface2D::UnmapPhysMemory(LwRm *pLwRm)
{
    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    if (!pClientData)
    {
        MASSERT(!"Called UnmapPhysMemory with unmapped client handle.");
        return;
    }

    LwRm::Handle objHandle = pClientData->m_hMem;
    if ((GetSkedReflected() || IsHostReflectedSurf()) &&
        // Since Amodel doesn't have MMU, so it's not really ready to allocate
        // SKED/HOST reflected buffer, in order to verify the traces before fmodel
        // getting ready, here we hack it to be normal buffer
        Platform::GetSimulationMode() != Platform::Amodel)
    {
        objHandle = (LwRm::Handle)GetMappingObj();
    }

    if (pClientData->m_hMem)
    {
        if (m_TegraMemMapped)
        {
            pLwRm->UnmapMemoryDma(pClientData->m_hTegraVirtMem,
                                  objHandle,
                                  0,
                                  pClientData->m_TegraVirtAddr,
                                  GetGpuDev());
            pClientData->m_TegraVirtAddr = 0;
            m_TegraMemMapped = false;
        }

        if (m_VirtMemMapped)
        {
            pLwRm->UnmapMemoryDma(pClientData->m_hVirtMem,
                                  objHandle,
                                  0,
                                  pClientData->m_GpuVirtAddr,
                                  GetGpuDev());
            pClientData->m_GpuVirtAddr = 0;

            if (pClientData->m_hSplitMem)
            {
                pLwRm->UnmapMemoryDma(pClientData->m_hVirtMem,
                                      pClientData->m_hSplitMem,
                                      0,
                                      pClientData->m_SplitGpuVirtAddr,
                                      GetGpuDev());
                pClientData->m_SplitGpuVirtAddr = 0;
            }

            m_VirtMemMapped = false;
        }
        if (m_CtxDmaMappedGpu && !m_GhostSurface && !GetGpuSubdev(0)->IsFakeTegraGpu())
        {
            pLwRm->UnmapMemoryDma(pClientData->m_hCtxDmaGpu,
                                  objHandle,
                                  0,
                                  pClientData->m_CtxDmaOffsetGpu,
                                  GetGpuDev());
            pClientData->m_CtxDmaOffsetGpu = 0;
            m_CtxDmaMappedGpu = false;
        }

        if (m_CtxDmaAllocatedIso)
        {
            pLwRm->Free(pClientData->m_hCtxDmaIso);
            pClientData->m_hCtxDmaIso = 0;
            m_CtxDmaAllocatedIso = false;
        }
    }
}

//-----------------------------------------------------------------------------
void Surface2D::Free()
{
    if (m_pLwRm->IsFreed())
        return;

    if ((m_DefClientData.m_hMem || m_DefClientData.m_hVirtMem) &&
        !m_IsViewOnly)
    {
        map<LwRm *, AllocData>::iterator pRmClientData;
        for (pRmClientData = m_PeerClientData.begin();
             pRmClientData != m_PeerClientData.end(); pRmClientData++)
        {
            UnmapPhysMemory(pRmClientData->first);
        }
        UnmapPhysMemory(m_pLwRm);
        UnmapCde();

        if (!m_GpuObjectMappings.empty())
        {
            GpuObjectMappings::const_iterator iter;
            for (iter = m_GpuObjectMappings.begin();
                 iter != m_GpuObjectMappings.end();
                 ++iter)
            {
                m_pLwRm->UnmapMemoryDma(iter->first,
                                        m_DefClientData.m_hMem,
                                        LW04_MAP_MEMORY_FLAGS_NONE,
                                        iter->second,
                                        GetGpuDev());
            }
            m_GpuObjectMappings.clear();
        }

        GpuDevMgr *pDevMgr = ((GpuDevMgr*)DevMgrMgr::d_GraphDevMgr);
        GpuDevice *pGpuDevice;

        for (pGpuDevice = pDevMgr->GetFirstGpuDevice(); (pGpuDevice != NULL);
             pGpuDevice = pDevMgr->GetNextGpuDevice(pGpuDevice))
        {
            UnmapRemote(pGpuDevice);
        }

        if (m_Address)
            Unmap();  // Unmap memory to ensure free is not deferred, Bug 341066

        // Free up handles from duplicated clients first
        for (pRmClientData = m_PeerClientData.begin();
             pRmClientData != m_PeerClientData.end(); pRmClientData++)
        {
            if (m_TegraVirtMemAllocated)
                pRmClientData->first->Free(pRmClientData->second.m_hTegraVirtMem);

            if (m_VirtMemAllocated)
                pRmClientData->first->Free(pRmClientData->second.m_hVirtMem);

            if (m_CtxDmaAllocatedGpu)
                pRmClientData->first->Free(pRmClientData->second.m_hCtxDmaGpu);

            pRmClientData->first->Free(pRmClientData->second.m_hMem);
            if (pRmClientData->second.m_hSplitMem)
            {
                pRmClientData->first->Free(pRmClientData->second.m_hSplitMem);
            }
            if (pRmClientData->second.m_hFLA)
            {
                pRmClientData->first->Free(pRmClientData->second.m_hFLA);
            }
        }

        if (m_TegraVirtMemAllocated)
            m_pLwRm->Free(m_DefClientData.m_hTegraVirtMem);

        if (m_VirtMemAllocated)
            m_pLwRm->Free(m_DefClientData.m_hVirtMem);

        if (m_CtxDmaAllocatedGpu)
            m_pLwRm->Free(m_DefClientData.m_hCtxDmaGpu);

        if (m_DefClientData.m_hFLA)
        {
            m_pLwRm->Free(m_DefClientData.m_hFLA);
            m_DefClientData.m_hFLA = 0;
        }

        if (m_DefClientData.m_hImportMem)
        {
            m_pLwRm->Free(m_DefClientData.m_hImportMem);
            m_DefClientData.m_hImportMem = 0;
        }

        if (m_DefClientData.m_hMem && !m_GhostSurface &&
            (m_ExternalPhysMem == 0))
        {
            // Only free this physical memory if this object owns it.
            if (HasPhysical())
            {
                m_pLwRm->Free(m_DefClientData.m_hMem);
            }
        }
        m_DefClientData.m_hMem = 0;

        if (m_DefClientData.m_hSplitMem)
        {
            m_pLwRm->Free(m_DefClientData.m_hSplitMem);
        }

        delete [] m_pChannelsBound;
    }
    ClearMembers();
}

//-----------------------------------------------------------------------------
//! Many functions have a default subdevice argument of Gpu::UNSPECIFIED_SUBDEV.
//! This is treated as 0 usually, but is considered an error for FB surfaces
//! on a multi-GPU SLI device.
RC Surface2D::CheckSubdeviceInstance(UINT32 * pSubdev)
{
    MASSERT(pSubdev);
    if ((*pSubdev == Gpu::UNSPECIFIED_SUBDEV) &&
        (GetGpuDev()->GetNumSubdevices() > 1))
    {
        if (GetLocation() == Memory::Fb)
        {
            Printf(Tee::PriHigh, "Surface2D: must specify a subdev for Map on FB SLI surfs!\n");
            return RC::SLI_ERROR;
        }
        else
        {
            // For Multi-Gpu non-framebuffer memory, always return subdevice
            // 0 if the subdevice was unspecified
            *pSubdev = 0;
            return OK;
        }
    }

    *pSubdev = GetGpuDev()->FixSubdeviceInst(*pSubdev);

    return OK;
}

//-----------------------------------------------------------------------------
RC Surface2D::MapRect(UINT32 x, UINT32 y, UINT32 width, UINT32 height, UINT32 Subdev)
{
    if ((x >= GetAllocWidth()) || (x + width > GetAllocWidth()) ||
        (y >= GetAllocHeight()) || (y + height > GetAllocHeight()))
    {
        Printf(Tee::PriHigh, "MapRect called with a rectangle reaching beyond the surface!\n");
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return RC::SOFTWARE_ERROR;
    }

    UINT64 Offset = 0;
    UINT64 Size = EntirePart;

    const UINT32 Bpp = GetBytesPerPixel();
    const UINT64 PageSize = GetPhysPageSize();
    switch (GetLayout())
    {
        case Surface2D::Pitch:
            {
                // Callwlate ideal beginning and end of the mapping
                const UINT64 MapBegin = (static_cast<UINT64>(y) * GetPitch() +
                                         static_cast<UINT64>(x) * Bpp);
                const UINT64 MapEnd =
                    static_cast<UINT64>(y+height-1) * GetPitch() +
                    static_cast<UINT64>(x+width) * Bpp;

                // Align to page
                const UINT64 AlignedMapBegin = MapBegin & ~(PageSize - 1);
                const UINT64 AlignedMapEnd = ((MapEnd - 1) & ~(PageSize - 1)) + PageSize;

                // Set the proposed mapping
                Offset = AlignedMapBegin;
                Size = AlignedMapEnd - AlignedMapBegin;
            }
            break;

        case Surface2D::BlockLinear:
            {
                const UINT32 GpuGobWidth = GetGpuDev()->GobWidth();
                const UINT32 GpuGobHeight = GetGpuDev()->GobHeight();

                UINT64 MinOffset = ~static_cast<UINT64>(0U);
                UINT64 MaxOffset = 0;
                unique_ptr< ::BlockLinear > pBl;

                BlockLinear::Create(&pBl, m_Width, m_Height, m_Depth,
                                    GetLogBlockWidth(), GetLogBlockHeight(),
                                    GetLogBlockDepth(),
                                    Bpp,
                                    GetGpuDev(),
                                    BlockLinear::SelectBlockLinearLayout(
                                        GetPteKind(), GetGpuDev()));

                // Go through each GOB in the specified rectangle
                const UINT32 LastY = y + height - 1;
                const UINT32 AlignedEndY = LastY - (LastY % GpuGobHeight) + GpuGobHeight;
                UINT32 AlignedY = y - (y % GpuGobHeight);
                for ( ; AlignedY < AlignedEndY; AlignedY += GpuGobHeight)
                {
                    const UINT32 LastX = (x + width - 1) * Bpp;
                    const UINT32 AlignedEndX = LastX - (LastX % GpuGobWidth) + GpuGobWidth;
                    UINT32 AlignedX = (x * Bpp) - ((x * Bpp) % GpuGobWidth);
                    for ( ; AlignedX < AlignedEndX; AlignedX += GpuGobWidth)
                    {
                        // Update the min and max surface offsets to account for the GOB
                        const UINT64 Offset = pBl->Address(AlignedX/Bpp, AlignedY, 0);
                        if (Offset < MinOffset) MinOffset = Offset;
                        if (Offset > MaxOffset) MaxOffset = Offset;
                    }
                }
                MASSERT(MinOffset <= MaxOffset);

                // Callwlate boundaries aligned to page
                const UINT64 AlignedMapBegin = MinOffset & ~(PageSize - 1);
                const UINT64 AlignedMapEnd = ((MaxOffset + GpuGobWidth*GpuGobHeight - 1) & ~(PageSize - 1)) + PageSize;

                // Set the proposed mapping
                Offset = AlignedMapBegin;
                Size = AlignedMapEnd - AlignedMapBegin;
            }
            break;

        // Swizzled (and others) not supported - hint to map the whole surface
        default:
            break;
    }

    // Trim map size to surface size
    if (Offset + Size > GetSize())
    {
        MASSERT(Offset <= GetSize());
        Size = GetSize() - Offset;
    }

    return MapRegion(Offset, Size, Subdev);
}

//-----------------------------------------------------------------------------
//! Maps GPU surface into process memory.
//! SLI: if there are multiple gpus in this device, an FB surface exists
//!      on each of them, so we must specify which is to be mapped.
//! @param part Surface part number. 0 selects first part of a split surface.
//!             Non-split surface requires this to be 0. 1 selects second part
//!             of a split surface.
//! @param subdeviceInstance Zero-based subdevice index.
//! @param pLwRm The RM client to use when mapping
//! @param offset Offset from the beginning of the part to map.
//! @param size Size of the region of the part to map.
RC Surface2D::MapInternal
(
    UINT32 part,
    UINT32 subdeviceInstance,
    LwRm  *pLwRm,
    UINT64 offset,
    UINT64 size
)
{
    RC rc;

    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    if (!pClientData)
        return RC::SOFTWARE_ERROR;

    // Should have been allocated, but not mapped
    const LwRm::Handle hMem = (part == 0) ? pClientData->m_hMem :
                                            pClientData->m_hSplitMem;
    if (!hMem)
    {
       Printf(Tee::PriHigh, "Surface2D memory not allocated!\n"
              " Please allocate memory before mapping.\n");
       MASSERT(!"Generic assertion failure<Refer to previous message>.");
       return RC::SOFTWARE_ERROR;
    }

    if (m_Address != 0)
    {
        Printf(Tee::PriHigh, "Surface2D already mapped!\n"
               " Please unmap memory before calling this function.\n");
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return RC::SOFTWARE_ERROR;
    }

    // Check size and offset
    const UINT64 partSize = GetPartSize(part);
    if (offset > partSize)
    {
        Printf(Tee::PriHigh, "Offset to map reaches beyond Surface2D part!\n");
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return RC::SOFTWARE_ERROR;
    }
    if (size == EntirePart)
    {
        size = partSize - offset;
    }
    else if (offset + size > partSize)
    {
        Printf(Tee::PriHigh, "Size to map reaches beyond Surface2D part!\n");
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return RC::SOFTWARE_ERROR;
    }

    UINT32 flags = 0;

    switch (m_MemoryMappingMode)
    {
        case MapDefault:
            flags = FLD_SET_DRF(OS33, _FLAGS, _MAPPING, _DEFAULT, flags);
            break;

        case MapDirect:
            flags = FLD_SET_DRF(OS33, _FLAGS, _MAPPING, _DIRECT, flags);
            break;

        case MapReflected:
            flags = FLD_SET_DRF(OS33, _FLAGS, _MAPPING, _REFLECTED, flags);
            break;

        default:
            break;
    }

    CHECK_RC(pLwRm->MapMemory(
            hMem,
            m_HiddenAllocSize + m_ExtraAllocSize + offset,
            size,
            &m_Address,
            flags,
            GetGpuSubdev(subdeviceInstance)));

    m_MappedSubdev = subdeviceInstance;
    Printf(Tee::PriDebug, "Surface2D mapped sd %d at address = %p\n",
            m_MappedSubdev, m_Address);
    if (Platform::HasClientSideResman())
    {
        // This only works with client-side RM.
        // This is CPU physical address. If the surface is
        // reflected-mapped, it will have a physical address
        // in BAR1 (even if it's a sysmem surface).
        // The usefulness of this is very low...
        m_PhysAddress = Platform::VirtualToPhysical(m_Address);
    }
    else
    {
        // Use virtual address for compatibility.
        // This is really useless, we should remove the argument-less GetPhysAddress()!
        m_PhysAddress = reinterpret_cast<uintptr_t>(m_Address);
    }
    m_MappedPart = part;
    m_MappedOffset = offset + GetPartOffset(part);
    m_MappedSize = size;
    m_MappedClient = pLwRm;

    return OK;
}

//-----------------------------------------------------------------------------
RC Surface2D::Map(UINT32 subdeviceInstance, LwRm *pLwRm)
{
    if (m_Split)
    {
        Printf(Tee::PriHigh, "Cannot call Map() on a split surface!\n");
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return RC::SOFTWARE_ERROR;
    }

    if (!GetClientData(&pLwRm, __FUNCTION__))
    {
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return RC::SOFTWARE_ERROR;
    }

    RC rc;
    CHECK_RC(CheckSubdeviceInstance(&subdeviceInstance));

    return MapInternal(0, subdeviceInstance, pLwRm);
}

//-----------------------------------------------------------------------------
RC Surface2D::MapPart(UINT32 part, UINT32 subdeviceInstance, LwRm *pLwRm)
{
    // Check part number
    if (part >= GetNumParts())
    {
        Printf(Tee::PriHigh, "Invalid Surface2D part specified for mapping!\n");
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return RC::SOFTWARE_ERROR;
    }

    // Check subdevice index
    if (subdeviceInstance >= GetGpuDev()->GetNumSubdevices())
    {
        Printf(Tee::PriHigh, "Invalid subdevice number!\n");
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return RC::SOFTWARE_ERROR;
    }

    if (!GetClientData(&pLwRm, __FUNCTION__))
    {
        MASSERT(!"Could not retrieve client data.");
        return RC::SOFTWARE_ERROR;
    }

    return MapInternal(part, subdeviceInstance, pLwRm);
}

//-----------------------------------------------------------------------------
//! Ensures that the part of the surface containing the specified offset
//! is mapped into CPU address space.
//! @param offset Offset based on which we choose surface part.
//! @param pRelOffset Returned relative offset to the start of mapped part.
//! @param pLwRm RM Client pointer to map with.
RC Surface2D::MapOffset(UINT64 offset, UINT64* pRelOffset, LwRm *pLwRm)
{
    if (offset >= GetSize())
    {
        Printf(Tee::PriHigh, "Offset reaches beyond surface!\n");
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return RC::SOFTWARE_ERROR;
    }

    if (!GetClientData(&pLwRm, __FUNCTION__))
    {
        MASSERT(!"Could not retrieve client data.");
        return RC::SOFTWARE_ERROR;
    }

    if ((m_Address == 0) ||
        (offset < m_MappedOffset) ||
        (offset >= m_MappedOffset+m_MappedSize))
    {
        const UINT32 newPart = (offset < m_SplitHalfSize) ? 0 : 1;
        UINT32 subdev = 0;
        if (m_Address != 0)
        {
            subdev = m_MappedSubdev;
            Unmap();
        }
        RC rc;
        CHECK_RC(MapInternal(newPart, subdev, pLwRm));
    }
    if (pRelOffset != 0)
    {
        *pRelOffset = offset - m_MappedOffset;
    }
    return OK;
}

//-----------------------------------------------------------------------------
RC Surface2D::MapRegion(UINT64 offset, UINT64 size, UINT32 subdev, LwRm *pLwRm)
{
    RC rc;

    if (!GetClientData(&pLwRm, __FUNCTION__))
    {
        MASSERT(!"Could not retrieve client data.");
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(CheckSubdeviceInstance(&subdev));

    const UINT64 firstPartSize = GetPartSize(0);
    if (offset < firstPartSize)
    {
        return MapInternal(0, subdev, pLwRm, offset, size);
    }
    else
    {
        return MapInternal(1, subdev, pLwRm, offset-firstPartSize, size);
    }
}

//-----------------------------------------------------------------------------
void Surface2D::Unmap()
{
    if (!m_DefClientData.m_hMem)
    {
        Printf(Tee::PriHigh, "Surface2D memory not allocated!\n"
               " Please allocate memory before unmapping.\n");
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return;
    }

    if (m_MappedClient == 0)
    {
        Printf(Tee::PriHigh, "Surface2D not mapped!\n"
               " Please map memory before calling this function.\n");
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return;
    }

    AllocData *pClientData = GetClientData(&m_MappedClient, __FUNCTION__);
    if (!pClientData)
    {
        MASSERT(!"Attempting to unmap unknown client!");
        return;
    }

    // Flush surface if it's CPU-cached
    if (m_Location == Memory::CachedNonCoherent)
    {
        FlushCpuCache(FlushAndIlwalidate);
    }

    // Should have been allocated and mapped
    const LwRm::Handle hMem = (m_MappedPart == 0) ? pClientData->m_hMem
                                                  : pClientData->m_hSplitMem;

    m_MappedClient->UnmapMemory(hMem, m_Address, 0, GetGpuSubdev(m_MappedSubdev));

    Printf(Tee::PriDebug, "Surface2D unmapped at address = %p\n", m_Address);
    m_Address = NULL;
    m_PhysAddress = 0;
    m_MappedSubdev = 0xffffffff;
    m_MappedPart = ~0U;
    m_MappedOffset = 0;
    m_MappedSize = 0;
    m_MappedClient = 0;
}

//-----------------------------------------------------------------------------
RC Surface2D::FlushCpuCache(UINT32 flags)
{
    if (m_Address != nullptr &&
        (m_Location == Memory::Coherent || m_Location == Memory::CachedNonCoherent))
    {
        if (m_Split)
        {
            Printf(Tee::PriHigh, "FlushCpuCache on split surfaces is not supported!\n");
            MASSERT(!"FlushCpuCache on split surfaces is not supported!");
            return RC::SOFTWARE_ERROR;
        }
        if (Platform::UsesLwgpuDriver())
        {
            if (m_MappedClient == 0)
            {
                Printf(Tee::PriHigh, "Surface2D not mapped!\n"
                       " Please map memory before calling this function.\n");
                MASSERT(!"Generic assertion failure<Refer to previous message>.");
                return RC::SOFTWARE_ERROR;
            }

            AllocData *pClientData = GetClientData(&m_MappedClient, __FUNCTION__);
            if (!pClientData)
            {
                MASSERT(!"Attempting to flush on unknown client!");
                return RC::SOFTWARE_ERROR;
            }

            return m_MappedClient->FlushUserCache(
                    m_pGpuDev, pClientData->m_hMem, m_MappedOffset, m_MappedSize);
        }
        else
        {
            return Cpu::FlushCpuCacheRange(
                    m_Address,
                    static_cast<UINT08*>(m_Address) + m_MappedSize,
                    flags);
        }
    }
    else if (m_Address != nullptr && m_Location == Memory::NonCoherent)
    {
        Platform::FlushCpuWriteCombineBuffer();
    }
    return OK;
}

//-----------------------------------------------------------------------------
Surface2D::MapContext Surface2D::SaveMapping() const
{
    MapContext Ctx;
    Ctx.Mapped = (m_Address != 0);
    Ctx.MappedSubdev = m_MappedSubdev;
    Ctx.MappedPart = m_MappedPart;
    Ctx.MappedOffset = m_MappedOffset;
    Ctx.MappedSize = m_MappedSize;
    Ctx.MappedClient = m_MappedClient;
    return Ctx;
}

//-----------------------------------------------------------------------------
RC Surface2D::RestoreMapping(const MapContext& Ctx)
{
    const MapContext LwrCtx = SaveMapping();
    if ((LwrCtx.Mapped          != Ctx.Mapped) ||
        (LwrCtx.MappedSubdev    != Ctx.MappedSubdev) ||
        (LwrCtx.MappedPart      != Ctx.MappedPart) ||
        (LwrCtx.MappedOffset    != Ctx.MappedOffset) ||
        (LwrCtx.MappedSize      != Ctx.MappedSize) ||
        (LwrCtx.MappedClient    != Ctx.MappedClient))
    {
        if (m_Address != 0)
        {
            Unmap();
        }
        if (Ctx.Mapped)
        {
            const UINT32 subdev = Ctx.MappedSubdev;
            RC rc;
            CHECK_RC(MapInternal(Ctx.MappedPart, subdev, Ctx.MappedClient,
                Ctx.MappedOffset, Ctx.MappedSize));
        }
    }
    return OK;
}

//-----------------------------------------------------------------------------
Surface2D::MappingSaver::MappingSaver(Surface2D& Surface)
: m_Surface(Surface),
  m_pContext(new Surface2D::MapContext(Surface.SaveMapping()))
{
}

//-----------------------------------------------------------------------------
Surface2D::MappingSaver::~MappingSaver()
{
    m_Surface.RestoreMapping(*m_pContext);
}

//----------------------------------------------------------------------------
RC Surface2D::MapToGpuObject(LwRm::Handle hObject)
{
    if (!m_DefClientData.m_hMem)
    {
        Printf(Tee::PriHigh, "Surface2D memory not allocated!\n");
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return RC::SOFTWARE_ERROR;
    }
    if (m_AddressModel != Memory::Paging)
    {
        Printf(Tee::PriHigh, "Surface2D can be mapped to other objects only in Paging model!\n");
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return RC::SOFTWARE_ERROR;
    }
    if (m_Split)
    {
        Printf(Tee::PriHigh, "Split Surface2D cannot be mapped to other objects!\n");
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return RC::SOFTWARE_ERROR;
    }
    if (hObject == m_DefClientData.m_hCtxDmaGpu)
    {
        Printf(Tee::PriHigh, "Unable to map Surface2D to the same object it was allocated in!\n");
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return RC::SOFTWARE_ERROR;
    }

    if (IsGpuObjectMapped(hObject))
        return OK;

    // Map the memory onto the other ctxdma
    RC rc;
    UINT64 CtxDmaOffsetGpu;
    CHECK_RC(m_pLwRm->MapMemoryDma(
                        hObject,
                        m_DefClientData.m_hMem,
                        m_HiddenAllocSize,
                        CalcLimit(m_Size)+1,
                        m_MapMemoryDmaFlags,
                        &CtxDmaOffsetGpu,
                        GetGpuDev()));
    Printf(Tee::PriDebug, " GPU object 0x%08x access to surf at 0x%llx\n",
        hObject, CtxDmaOffsetGpu);

    m_GpuObjectMappings[hObject] = CtxDmaOffsetGpu;

    return OK;
}

//-----------------------------------------------------------------------------
bool Surface2D::IsGpuObjectMapped(LwRm::Handle hObject) const
{
    return (m_GpuObjectMappings.find(hObject) != m_GpuObjectMappings.end());
}

//-----------------------------------------------------------------------------
UINT64 Surface2D::GetCtxDmaOffsetGpuObject(LwRm::Handle hObject) const
{
    MASSERT(IsGpuObjectMapped(hObject));
    return m_GpuObjectMappings.find(hObject)->second;
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetBitsPerPixel() const
{
    switch (m_ColorFormat)
    {
        case ColorUtils::LWFMT_NONE:
            if (m_BytesPerPixel == 0)
            {
                MASSERT(m_ColorFormat != ColorUtils::LWFMT_NONE);
                return ~0U;
            }
            return 8 * m_BytesPerPixel;

        case ColorUtils::I1:
            return 1;

        case ColorUtils::I2:
            return 2;

        case ColorUtils::I4:
            return 4;

        default:
            return 8 * ColorUtils::PixelBytes(m_ColorFormat);
    }
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetElemsPerPixel() const
{
    return ColorUtils::PixelElements(m_ColorFormat);
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetBytesPerPixel() const
{
    const UINT32 bits = GetBitsPerPixel();
    MASSERT((bits & 7) == 0);
    return bits / 8;
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::GetWordSize() const
{
    return ColorUtils::WordSize(m_ColorFormat);
}

//-----------------------------------------------------------------------------
UINT64 Surface2D::GetPixelOffset(UINT32 x, UINT32 y) const
{
    return GetPixelOffset(x, y, 0, 0);
}

//-----------------------------------------------------------------------------
UINT64 Surface2D::GetPixelOffset(UINT32 x, UINT32 y, UINT32 z, UINT32 a) const
{
    // Should have been allocated
    MASSERT(m_DefClientData.m_hMem);

    // Pixel had better be inside the surface
    // Note that we only verify that it's inside the allocated region, not the
    // user's originally requested region, since LoadCompressedImage will ask
    // for the addresses of gobs outside the originally requested region.
    MASSERT(x < m_AllocWidth);
    MASSERT(y < m_AllocHeight);
    MASSERT(z < m_AllocDepth);
    MASSERT(a < m_ArraySize);

    const UINT32 bitsPerPixel = GetBitsPerPixel();
    switch (m_Layout)
    {
        case Pitch:
        {
            const UINT64 a_offset = static_cast<UINT64>(a) * m_Depth;
            const UINT64 z_offset = (z + a_offset) * m_Height;
            const UINT64 y_offset = (y + z_offset) * m_Pitch;
            return y_offset + (x * bitsPerPixel) / 8;
        }
        case Swizzled:
        {
            UINT32 Min, Mask, xy;

            // Only implemented up to 4Kx4K
            MASSERT(m_Width <= 4096);
            MASSERT(m_Height <= 4096);
            MASSERT(z == 0);
            MASSERT(a == 0);

            Min = (m_Width < m_Height) ? m_Width : m_Height;
            Min = FloorLog2(Min);

            Mask = (1 << (2*Min)) - 1;
            xy = ((x <<  0) & 0x00000001) | ((y <<  1) & 0x00000002) |
                 ((x <<  1) & 0x00000004) | ((y <<  2) & 0x00000008) |
                 ((x <<  2) & 0x00000010) | ((y <<  3) & 0x00000020) |
                 ((x <<  3) & 0x00000040) | ((y <<  4) & 0x00000080) |
                 ((x <<  4) & 0x00000100) | ((y <<  5) & 0x00000200) |
                 ((x <<  5) & 0x00000400) | ((y <<  6) & 0x00000800) |
                 ((x <<  6) & 0x00001000) | ((y <<  7) & 0x00002000) |
                 ((x <<  7) & 0x00004000) | ((y <<  8) & 0x00008000) |
                 ((x <<  8) & 0x00010000) | ((y <<  9) & 0x00020000) |
                 ((x <<  9) & 0x00040000) | ((y << 10) & 0x00080000) |
                 ((x << 10) & 0x00100000) | ((y << 11) & 0x00200000) |
                 ((x << 11) & 0x00400000) | ((y << 12) & 0x00800000);
            xy = (xy & Mask) | (((x | y) << Min) & ~Mask);

            return (static_cast<UINT64>(bitsPerPixel) * xy) / 8;
        }
        case BlockLinear:
        {
            MASSERT((bitsPerPixel & 7) == 0);
            unique_ptr< ::BlockLinear > pBl;

            BlockLinear::Create(&pBl, m_Width, m_Height, m_Depth,
                                m_LogBlockWidth, m_LogBlockHeight,
                                m_LogBlockDepth,
                                bitsPerPixel / 8, GetGpuDev(),
                                BlockLinear::SelectBlockLinearLayout(
                                    GetPteKind(), GetGpuDev()));
            return pBl->Address(x, y, z)
                    + a * m_AllocWidth * m_AllocHeight * m_AllocDepth;
        }
        case Tiled:
        {
            // Tiles are 16 bytes wide by 16 lines tall and arranged in a grid
            // with stride divisble by 16 and lines divible by 16
            const UINT32 xOffs = (x * bitsPerPixel) / 8;
            UINT32 yTileOffset   = (y / GetGpuDev()->TileHeight()) * GetGpuDev()->TileWidth() * m_Pitch;
            UINT32 xTileOffset   = (xOffs / GetGpuDev()->TileWidth()) * GetGpuDev()->TileSize();
            UINT32 yOffset       = (y % GetGpuDev()->TileHeight()) * GetGpuDev()->TileWidth();
            UINT32 xOffset       = xOffs % GetGpuDev()->TileWidth();
            UINT32 pixelOffset   = yTileOffset + xTileOffset + yOffset + xOffset;
            return pixelOffset;
        }
        default:
            MASSERT(!"Invalid layout");
            return 0;
    }
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::ReadPixel(UINT32 x, UINT32 y) const
{
    return ReadPixel(x, y, 0, 0);
}

//-----------------------------------------------------------------------------
UINT32 Surface2D::ReadPixel(UINT32 x, UINT32 y, UINT32 z, UINT32 a) const
{
    // Should have been allocated and mapped
    MASSERT(m_DefClientData.m_hMem);

    const UINT32 bitsPerPixel = GetBitsPerPixel();
    UINT64 Offset = GetPixelOffset(x, y, z, a);
    if (OK != const_cast<Surface2D*>(this)->MapOffset(Offset, &Offset))
    {
        return 0;
    }

    MASSERT(m_Address);

    switch (bitsPerPixel)
    {
        case 1:
            return (MEM_RD08((UINT08 *)m_Address + Offset) >> (x & 7)) & 1;
        case 2:
            return (MEM_RD08((UINT08 *)m_Address + Offset) >> ((x & 3)*2)) & 3;
        case 4:
            return (MEM_RD08((UINT08 *)m_Address + Offset) >> ((x & 1)*4)) & 0xF;
        case 8:
            return MEM_RD08((UINT08 *)m_Address + Offset);
        case 16:
            return MEM_RD16((UINT08 *)m_Address + Offset);
        case 30:
        case 32:
            return MEM_RD32((UINT08 *)m_Address + Offset);
        default:
            MASSERT(!"Invalid BitsPerPixel");
            return 0;
    }
}

//-----------------------------------------------------------------------------
void Surface2D::WritePixel(UINT32 x, UINT32 y, UINT32 Value)
{
    WritePixel(x, y, 0, 0, Value);
}

//-----------------------------------------------------------------------------
void Surface2D::WritePixel(UINT32 x, UINT32 y, UINT32 z, UINT32 a, UINT32 Value)
{
    // Should have been allocated and mapped
    MASSERT(m_DefClientData.m_hMem);
    MASSERT(m_Address);

    UINT32 BytesPerPixel = GetBytesPerPixel();
    UINT64 Offset = GetPixelOffset(x, y, z, a);
    if (OK != MapOffset(Offset, &Offset))
    {
        return;
    }
    switch (BytesPerPixel)
    {
        case 1:
            MEM_WR08((UINT08 *)m_Address + Offset, Value);
            break;
        case 2:
            MEM_WR16((UINT08 *)m_Address + Offset, Value);
            break;
        case 4:
            MEM_WR32((UINT08 *)m_Address + Offset, Value);
            break;
        default:
            MASSERT(!"Invalid BytesPerPixel");
    }
}

//-----------------------------------------------------------------------------
static UINT32 GetWriteValue(UINT32 BytesPerPixel, UINT32 NumWritten,
                                UINT32 PatternLength, UINT32* pPattern)
{
    UINT32 value;
    UINT32 NumOfShifts;
    UINT32 Idx;
    switch(BytesPerPixel)
    {
    case 1:
        NumOfShifts = NumWritten % 4;
        Idx = (NumWritten / 4) % PatternLength;
        value = pPattern[Idx] >> (8*NumOfShifts);
        break;
    case 2:
        NumOfShifts = NumWritten % 2;
        Idx = (NumWritten / 2) % PatternLength;
        value = pPattern[Idx] >> (16*NumOfShifts);
        break;
    case 4:
        value = pPattern[NumWritten % PatternLength];
        break;
    default:
        value = 0;
        MASSERT(!"Invalid bytes per pixel");
    }
    return value;
}

RC Surface2D::FillPatternTable(UINT32 x, UINT32 y, UINT32* pPattern,
                        UINT32 PatternLength, UINT32 Repetitions)
{
    RC rc;
    MASSERT(m_DefClientData.m_hMem);
    MASSERT(m_Address);

    UINT32 BytesPerPixel = GetBytesPerPixel();
    CHECK_RC(Map());
    if (GetSize() < static_cast<UINT64>(PatternLength) * 4 * Repetitions)
    {
        Printf(Tee::PriWarn,
               "allocated size not large enough - pattern will be truncated\n");
    }

    UINT32 NumOfWrites = PatternLength * 4 * (Repetitions+1) / BytesPerPixel;
    //  this will truncate the number of writes to fit inside the surface
    UINT32 EndY = y + NumOfWrites / m_Width;
    UINT32 StartX = x;
    UINT32 NumWritten = 0;
    for(UINT32 StartY = y; StartY <= EndY; StartY++)
    {
        for(; StartX < m_Width; StartX++)
        {
            UINT32 value = GetWriteValue(BytesPerPixel, NumWritten, PatternLength, pPattern);
            WritePixel(StartX, StartY, value);
            NumWritten++;
        }
        StartX = 0;
    }
    // finish off the last line
    UINT32 LwrX = 0;
    while(NumWritten < NumOfWrites)
    {
        UINT32 value = GetWriteValue(BytesPerPixel, NumWritten, PatternLength, pPattern);
        WritePixel(LwrX, EndY + 1, value);
        NumWritten++;
        LwrX++;
    }
    Unmap();
    return OK;
}

//-----------------------------------------------------------------------------
RC Surface2D::Fill(UINT32 Value, UINT32 subdev)
{
    RC rc;
    CHECK_RC(CheckSubdeviceInstance(&subdev));

    const Surface2D::MappingSaver SavedMapping(*this);
    if (m_Address)
    {
        Unmap();
    }

    // This function doesn't care about the layout, since every pixel is going
    // to be set to the same thing.  This could be wasteful if BL/swizzle/pitch
    // rounding has wasted a lot of memory, but most of the time it's simpler.
    // We can always revisit this later if it is a problem.
    UINT32 Part;
    if (Value == 0)
    {
        for (Part = 0; Part < GetNumParts(); Part++)
        {
            const UINT64 Size = GetPartSize(Part);
            CHECK_RC(MapPart(Part, subdev));
            Platform::MemSet(m_Address, 0, static_cast<size_t>(Size));
            Unmap();
        }
        return rc;
    }

    const UINT32 bitsPerPixel = GetBitsPerPixel();
    switch (bitsPerPixel)
    {
        case 1:
        case 2:
        case 4:
        case 8:
            for (Part = 0; Part < GetNumParts(); Part++)
            {
                const UINT64 Size = GetPartSize(Part);
                CHECK_RC(MapPart(Part, subdev));
                Platform::MemSet(m_Address, Value, static_cast<size_t>(Size));
                Unmap();
            }
            break;
        case 16:
        case 36:
        case 64:
            for (Part = 0; Part < GetNumParts(); Part++)
            {
                const UINT64 Size = GetPartSize(Part);
                CHECK_RC(MapPart(Part, subdev));
                for (UINT32 Offset = 0; Offset < Size; Offset += 2)
                {
                    MEM_WR16((UINT08 *)m_Address + Offset, Value);
                }
                Unmap();
            }
            break;
        case 32:
        case 30:
        case 128:
            for (Part = 0; Part < GetNumParts(); Part++)
            {
                const UINT64 Size = GetPartSize(Part);
                CHECK_RC(MapPart(Part, subdev));
                for (UINT32 Offset = 0; Offset < Size; Offset += 4)
                {
                    MEM_WR32((UINT08 *)m_Address + Offset, Value);
                }
                Unmap();
            }
            break;
        default:
            Printf(Tee::PriHigh, "Error: Color format 0x%x (%ubpp) not supported by Surface2D.\n",
                    m_ColorFormat, bitsPerPixel);
            MASSERT(!"Invalid bitsPerPixel");
            return RC::SOFTWARE_ERROR;
    }
    return OK;
}

//-----------------------------------------------------------------------------
RC Surface2D::FillRect
(
    UINT32 x,
    UINT32 y,
    UINT32 Width,
    UINT32 Height,
    UINT32 Value,
    UINT32 subdev
)
{
    RC rc;
    UINT32 BytesPerPixel = GetBytesPerPixel();
    UINT32 PixelX, PixelY;

    const Surface2D::MappingSaver SavedMapping(*this);
    if (m_Address)
    {
        Unmap();
    }

    CHECK_RC(CheckSubdeviceInstance(&subdev));
    CHECK_RC(MapRect(x, y, Width, Height, subdev));
    for (PixelY = y; PixelY < y+Height; PixelY++)
    {
        for (PixelX = x; PixelX < x+Width; PixelX++)
        {
            UINT64 Offset = GetPixelOffset(PixelX, PixelY);
            CHECK_RC(MapOffset(Offset, &Offset));
            switch (BytesPerPixel)
            {
                case 1:
                    MEM_WR08((UINT08 *)m_Address + Offset, Value);
                    break;
                case 2:
                    MEM_WR16((UINT08 *)m_Address + Offset, Value);
                    break;
                case 4:
                    MEM_WR32((UINT08 *)m_Address + Offset, Value);
                    break;
                default:
                    MASSERT(!"Invalid BytesPerPixel");
            }
        }
    }
    return OK;
}

//-----------------------------------------------------------------------------
RC Surface2D::Fill64(UINT16 r, UINT16 g, UINT16 b, UINT16 a, UINT32 subdev)
{
    RC rc;
    CHECK_RC(CheckSubdeviceInstance(&subdev));

    const Surface2D::MappingSaver SavedMapping(*this);
    if (m_Address)
    {
        Unmap();
    }

    // This function doesn't care about the layout, since every pixel is going
    // to be set to the same thing.  This could be wasteful if BL/swizzle/pitch
    // rounding has wasted a lot of memory, but most of the time it's simpler.
    // We can always revisit this later if it is a problem.
    MASSERT(GetBytesPerPixel() == 8);
    for (UINT32 Part = 0; Part < GetNumParts(); Part++)
    {
        const UINT64 Size = GetPartSize(Part);
        CHECK_RC(MapPart(Part, subdev));
        for (UINT32 Offset = 0; Offset < Size; Offset += 8)
        {
            MEM_WR16((UINT08 *)m_Address + Offset+0, r);
            MEM_WR16((UINT08 *)m_Address + Offset+2, g);
            MEM_WR16((UINT08 *)m_Address + Offset+4, b);
            MEM_WR16((UINT08 *)m_Address + Offset+6, a);
        }
        Unmap();
    }
    return OK;
}

//-----------------------------------------------------------------------------
RC Surface2D::FillRect64(UINT32 x, UINT32 y, UINT32 Width, UINT32 Height,
                         UINT16 r, UINT16 g, UINT16 b, UINT16 a, UINT32 subdev)
{
    RC rc;
    UINT32 PixelX, PixelY;

    const Surface2D::MappingSaver SavedMapping(*this);
    if (m_Address)
    {
        Unmap();
    }

    // Should have been allocated
    MASSERT(m_DefClientData.m_hMem);

    MASSERT(GetBytesPerPixel() == 8);
    CHECK_RC(CheckSubdeviceInstance(&subdev));
    CHECK_RC(MapPart(0, subdev));
    for (PixelY = y; PixelY < y+Height; PixelY++)
    {
        for (PixelX = x; PixelX < x+Width; PixelX++)
        {
            UINT64 Offset = GetPixelOffset(PixelX, PixelY);
            CHECK_RC(MapOffset(Offset, &Offset));
            MEM_WR16((UINT08 *)m_Address + Offset+0, r);
            MEM_WR16((UINT08 *)m_Address + Offset+2, g);
            MEM_WR16((UINT08 *)m_Address + Offset+4, b);
            MEM_WR16((UINT08 *)m_Address + Offset+6, a);
        }
    }
    return OK;
}

//-----------------------------------------------------------------------------
RC Surface2D::FillPattern
(
    UINT32 ChunkWidth,
    UINT32 ChunkHeight,
    const char *PatternType,
    const char *PatternStyle,
    const char *PatternDir,
    UINT32 subdev
)
{
    return FillPatternRect(0, 0, m_Width, m_Height, ChunkWidth, ChunkHeight,
                           PatternType, PatternStyle, PatternDir, subdev);
}

//-----------------------------------------------------------------------------
RC Surface2D::DrawPixelARGB
(
    UINT32 x,
    UINT32 y,
    UINT32 RectX,
    UINT32 RectY,
    UINT32 RectWidth,
    UINT32 RectHeight,
    UINT32 ChunkWidth,
    UINT32 ChunkHeight,
    UINT32 ColorARGB,
    UINT32 *pLastColorARGB,
    UINT32 BytesPerPixel,
    UINT32 WordSize
)
{
    RC rc;
    UINT08 Pixel[16] = {};
    UINT32 PixelX, PixelY;

    if (IsYUV())
    {
        if (ChunkWidth != 1 || ChunkHeight != 1)
        {
            MASSERT(!"Unsupported chunk width/height for YUV");
            Printf(Tee::PriError, "Unsupported chunk width/height for YUV\n");
            return RC::UNSUPPORTED_FUNCTION;
        }

        if (IsSurface422Compressed())
        {
            // Skip the odd columns
            if (x & 0x1)
            {
                return RC::OK;
            }

            x /= 2;
        }
        else if (IsSurface420Compressed())
        {
            // skip both odd rows and columns
            if ((x & 0x1) || (y & 0x1))
            {
                return RC::OK;
            }

            x /= 2;
            y /= 2;
        }

        ColorUtils::TransformPixels((const char*)&ColorARGB, (char*)Pixel, 1,
            ColorUtils::A2R10G10B10, m_ColorFormat, m_YCrCbType,
            m_PlanesColorFormat, GetGpuDev()->GetDisplay()->GetClass());

        for (UINT32 a = 0; a < m_ArraySize; a++)
        {
            for (UINT32 z = 0; z < m_Depth; ++z)
            {
                UINT64 offset = GetPixelOffset(x, y, z, a);
                CHECK_RC(MapOffset(offset, &offset));
                UINT08 *pDst = (UINT08 *)m_Address + offset;
                Platform::VirtualWr(pDst, Pixel, BytesPerPixel);
            }
        }

        return RC::OK;
    }
    else if (IsYUVLegacy())
    {
        UINT32 raw_pixels[2];

        if (ChunkWidth > 1)
        {
            raw_pixels[0] = ColorARGB;
            raw_pixels[1] = ColorARGB;
        }
        else
        {
            raw_pixels[0] = *pLastColorARGB;
            raw_pixels[1] = ColorARGB;
        }

        // save up even Color value to be filled at odd pixel
        *pLastColorARGB = ColorARGB;

        // skip the even X pixel. we do the fill at the odd pixel
        if (ChunkWidth == 1 && ! (x & 0x1))
        {
            return OK;
        }

        // colwert tuple to YCrCb format
        ColorUtils::YCrCbColwert((const char*)&raw_pixels,
                                 ColorUtils::A2R10G10B10,
                                 (char*)Pixel, m_ColorFormat, 2,
                                 m_YCrCbType);

        // write tuple together as one dword chunk
        for (UINT32 a = 0; a < m_ArraySize; a++)
        {
            for (UINT32 PixelZ = 0; PixelZ < m_Depth; PixelZ++)
            {
                for (PixelY = RectY + y*ChunkHeight;
                    PixelY < RectY + (y+1)*ChunkHeight;
                    PixelY++)
                {
                    if (PixelY > m_Height)
                        break;
                    for (PixelX = (RectX + x*ChunkWidth) & ~(0x1);
                        PixelX < RectX + (x+1)*ChunkWidth;
                        PixelX+=2)
                    {
                        if (PixelX > m_Width)
                            break;
                        UINT64 Offset = GetPixelOffset(PixelX, PixelY,
                                                       PixelZ, a);
                        CHECK_RC(MapOffset(Offset, &Offset));
                        UINT08 *pDst = (UINT08 *)m_Address + Offset;
                        Platform::VirtualWr(pDst, Pixel, 4);
                    }
                }
            }
        }
    }
    else
    {
        // Colwert to real color format and write out to memory
        if (BytesPerPixel <= 4)
        {
            *reinterpret_cast<UINT32*>(Pixel) = ColorUtils::Colwert(
                    ColorARGB, ColorUtils::A2R10G10B10, m_ColorFormat);
        }
        else
        {
            ColorUtils::Colwert((const char *)&ColorARGB, ColorUtils::A2R10G10B10,
                                (char *)Pixel, m_ColorFormat, 1);
        }
        for (UINT32 a = 0; a < m_ArraySize; a++)
        {
            for (UINT32 PixelZ = 0; PixelZ < m_Depth; PixelZ++)
            {
                for (PixelY = RectY + y*ChunkHeight;
                    PixelY < RectY + (y+1)*ChunkHeight;
                    PixelY++)
                {
                    if (PixelY > m_Height)
                        break;
                    for (PixelX = RectX + x*ChunkWidth;
                        PixelX < RectX + (x+1)*ChunkWidth;
                        PixelX++)
                    {
                        if (PixelX > m_Width)
                            break;
                        UINT64 Offset = GetPixelOffset(PixelX, PixelY,
                                                       PixelZ, a);
                        CHECK_RC(MapOffset(Offset, &Offset));
                        UINT08 *pDst = (UINT08 *)m_Address + Offset;
                        Platform::VirtualWr(pDst, Pixel, BytesPerPixel);
                    }
                }
            }
        }
    }
    return OK;
}

//These map to PatternDir and PatternStyle string properties.
enum PatternDirType {
    PD_HORIZONTAL,
    PD_VERTICAL
};

enum PatternStyleType {
    PS_PRIMARIES,
    PS_100,
    PS_75,
    PS_COLOR_PALETTE,
    PS_MULTIPLE,
    PS_MFGDIAG,
    PS_SHALLOW,
    PS_EDGE,
    PS_50AA,
    PS_CROSSHATCHED,
    PS_LUMA,
    PS_444,
    PS_422,
    PS_LUMA25,
    PS_LUMA50,
    PS_LUMA75,
    PS_DEFLICKER,
    PS_DFPBARS,

    PS_NONE
};

//----------------------------------------------------------------------------
RC Surface2D::FillPatternRectRandom
(

    UINT32 RectX,
    UINT32 RectY,
    UINT32 Width,
    UINT32 Height,
    UINT32 RectWidth,
    UINT32 RectHeight,
    UINT32 ChunkWidth,
    UINT32 ChunkHeight,
    const char * PatternStyle
)
{
    UINT32 LastColorARGB = 0;
    UINT32 BytesPerPixel = GetBytesPerPixel();
    UINT32 WordSize = GetWordSize();
    UINT32 multiCSCMask = 0xFFFFFFFF; // don't filter any bit to keep original behavior

    Random rnd;
    //Seed the random generator with 0, or the specified seed.
    UINT32 Seed = 0;
    UINT32 Len  = 0;

    if (PatternStyle)
    {
        if (!strcmp(PatternStyle, "multicsc2"))
        {
            multiCSCMask = 0x7FFFFFFF; // csc info uses 31:30, forcing bit 31 to 0 in multicsc2 for limit csc value to 0 or 1
        }
        else if (!strcmp(PatternStyle, "multicsc4"))
        {
            multiCSCMask = 0xFFFFFFFF; // csc info uses 31:30, remain 2 csc bits in multicsc4 so csc value can be 0,1,2,3
        }
        else
        {
            sscanf(PatternStyle, "seed=%u%n", &Seed, &Len);
            if (Len != strlen(PatternStyle))
            {
                MASSERT(!"Invalid seed specification, expect 'seed=<number>' format");
                return RC::BAD_PARAMETER;
            }
        }
    }
#ifdef SIM_BUILD
    if (Seed == 0)
    {
        // For back-compatibility with AXL, which runs arch tests that
        // store CRCs of random surfaces in .dpc files in the //arch
        // tree
        rnd.SeedRandom(0, 0x1111111111111111ULL, 0, 0x2222222222222222ULL);
    }
    else
#endif
    {
        rnd.SeedRandom(Seed);
    }

    for (UINT32 y = 0; y < Height; y++)
    {
        for (UINT32 x = 0; x < Width; x++)
        {
            UINT32 ColorARGB;
            // fill the surface with the random pattern
            ColorARGB = rnd.GetRandom();
            ColorARGB &= multiCSCMask;
            DrawPixelARGB(x,y,RectX,RectY,
                          RectWidth,RectHeight,
                          ChunkWidth,ChunkHeight,
                          ColorARGB,&LastColorARGB,
                          BytesPerPixel, WordSize);
        }
    }
    return OK;
}

RC Surface2D::FillPatternRectBars
(
    UINT32 RectX,
    UINT32 RectY,
    UINT32 Width,
    UINT32 Height,
    UINT32 RectWidth,
    UINT32 RectHeight,
    UINT32 ChunkWidth,
    UINT32 ChunkHeight,
    const char* PatternType,
    const char* PatternDir,
    const char* PatternStyle
)
{
    UINT32 LastColorARGB = 0;
    UINT32 BytesPerPixel = GetBytesPerPixel();
    UINT32 WordSize = GetWordSize();
    PatternDirType PDir;
    if (!strcmp(PatternDir, "horizontal"))
    {
        PDir = PD_HORIZONTAL;
    }
    else if(!strcmp(PatternDir, "vertical"))
    {
        PDir = PD_VERTICAL;
    }
    else
    {
        Printf(Tee::PriError,
               "Unrecognized PatternDir (%s) "
               "for Pattern Style %s, PatternType %s\n",
               PatternDir, PatternStyle, PatternType);
        return RC::BAD_PARAMETER;
    }

    PatternStyleType PStyle;
    UINT32 multiCSCNum = 0;
    if(!strcmp(PatternStyle, "primaries"))
    {
        PStyle = PS_PRIMARIES;
    }
    else if(!strcmp(PatternStyle, "100"))
    {
        PStyle = PS_100;
    }
    else if(!strcmp(PatternStyle, "75"))
    {
        PStyle = PS_75;
    }
    else if (!strcmp(PatternStyle, "multicsc2"))
    {
        PStyle = PS_100;
        multiCSCNum = 2;
    }
    else if (!strcmp(PatternStyle, "multicsc4"))
    {
        PStyle = PS_100;
        multiCSCNum = 4;
    }
    else
    {
        Printf(Tee::PriHigh,
               "Unrecognized PatternStyle (%s) for PatternType %s\n",
               PatternStyle, PatternType);
        return RC::BAD_PARAMETER;
    }

    for (UINT32 y = 0; y < Height; y++)
    {
        for (UINT32 x = 0; x < Width; x++)
        {
            UINT32 ColorARGB = 0;
            UINT32 Dist = 0;
            UINT32 Size = 0;
            UINT32 opDist = 0;
            UINT32 opSize = 0;
            switch (PDir)
            {
                case PD_VERTICAL:
                    Dist = y;
                    Size = Height;
                    opDist = x;
                    opSize = Width;
                    break;
                case PD_HORIZONTAL:
                    Dist = x;
                    Size = Width;
                    opDist = y;
                    opSize = Height;
                    break;
            }

            if (PS_PRIMARIES == PStyle)
            {
                // fill the surface with B G R primary color bar pattern
                switch (3 * Dist / Size)
                {
                    case 0  : ColorARGB = 0xC00003FF; break; // Blue
                    case 1  : ColorARGB = 0xC00FFC00; break; // Green
                    case 2  : ColorARGB = 0xFFF00000; break; // Red
                    default : ColorARGB = 0xFFF00000; break; // Red
                }
            }
            else if (PS_100 == PStyle)
            {
                // fill the surface with 100% strength Wh Yl Cy G Mg R B Bk color bar pattern
                switch (8 * Dist / Size)
                {
                    case 0  : ColorARGB = 0xFFFFFFFF; break; // White
                    case 1  : ColorARGB = 0xFFFFFC00; break; // Yellow
                    case 2  : ColorARGB = 0xC00FFFFF; break; // Cyan
                    case 3  : ColorARGB = 0xC00FFC00; break; // Green
                    case 4  : ColorARGB = 0xFFF003FF; break; // Magenta
                    case 5  : ColorARGB = 0xFFF00000; break; // Red
                    case 6  : ColorARGB = 0xC00003FF; break; // Blue
                    case 7  : ColorARGB = 0xC0000000; break; // Black
                    default : ColorARGB = 0xC0000000; break; // Black
                }
                // fill the surface with csc info on alpha bit
                // csc region always cross color region to provide full case cover
                if (multiCSCNum == 2)
                {
                    ColorARGB &= 0x3FFFFFFF; // clear CSC bits
                    switch (2 * opDist / opSize)
                    {
                        case 0  : ColorARGB |= 0x00000000; break; // CSC = 0
                        case 1  :
                        default : ColorARGB |= 0x40000000; break; // CSC = 1
                    }
                }
                else if (multiCSCNum == 4)
                {
                    ColorARGB &= 0x3FFFFFFF; // clear CSC bits
                    switch (4 * opDist / opSize)
                    {
                        case 0  : ColorARGB |= 0x00000000; break; // CSC = 0
                        case 1  : ColorARGB |= 0x40000000; break; // CSC = 1
                        case 2  : ColorARGB |= 0x80000000; break; // CSC = 2
                        case 3  :
                        default : ColorARGB |= 0xC0000000; break; // CSC = 3
                    }
                }
            }
            else if (PS_75 == PStyle)
            {
                // fill the surface with 75% strength Wh Yl Cy G Mg R B Bk color bar pattern
                switch (8 * Dist / Size)
                {
                    case 0  : ColorARGB = 0xF00C0300; break; // White
                    case 1  : ColorARGB = 0xF00C0000; break; // Yellow
                    case 2  : ColorARGB = 0xC00C0300; break; // Cyan
                    case 3  : ColorARGB = 0xC00C0000; break; // Green
                    case 4  : ColorARGB = 0xF0000300; break; // Magenta
                    case 5  : ColorARGB = 0xF0000000; break; // Red
                    case 6  : ColorARGB = 0xC0000300; break; // Blue
                    case 7  : ColorARGB = 0xC0000000; break; // Black
                    default : ColorARGB = 0xC0000000; break; // Black
                }
            }

            DrawPixelARGB(x,y,RectX,RectY,
                          RectWidth,RectHeight,
                          ChunkWidth,ChunkHeight,
                          ColorARGB,&LastColorARGB,
                          BytesPerPixel, WordSize);
        }
    }
    return OK;
}

RC Surface2D::FillPatternRectGradient
(

    UINT32 RectX,
    UINT32 RectY,
    UINT32 Width,
    UINT32 Height,
    UINT32 RectWidth,
    UINT32 RectHeight,
    UINT32 ChunkWidth,
    UINT32 ChunkHeight,
    const char* PatternType,
    const char* PatternDir,
    const char* PatternStyle
)
{
    UINT32 LastColorARGB = 0;
    UINT32 BytesPerPixel = GetBytesPerPixel();
    UINT32 WordSize = GetWordSize();
    PatternDirType PDir;
    if (!strcmp(PatternDir, "horizontal"))
    {
        PDir = PD_HORIZONTAL;
    }
    else if(!strcmp(PatternDir, "vertical"))
    {
        PDir = PD_VERTICAL;
    }
    else
    {
        Printf(Tee::PriError,
               "Unrecognized PatternDir (%s) "
               "for Pattern Style %s, PatternType %s\n",
               PatternDir, PatternStyle, PatternType);
        return RC::BAD_PARAMETER;
    }

    PatternStyleType PStyle;
    if(!strcmp(PatternStyle, "color_palette"))
    {
        PStyle = PS_COLOR_PALETTE;
    }
    else if(!strcmp(PatternStyle, "multiple"))
    {
        PStyle = PS_MULTIPLE;
    }
    else if(!strcmp(PatternStyle, "mfgdiag"))
    {
        PStyle = PS_MFGDIAG;
    }
    else if(!strcmp(PatternStyle, "shallow"))
    {
        PStyle = PS_SHALLOW;
    }
    else
    {
        Printf(Tee::PriHigh,
               "Unrecognized PatternStyle (%s) for PatternType %s\n",
               PatternStyle, PatternType);
        return RC::BAD_PARAMETER;
    }

    for (UINT32 y = 0; y < Height; y++)
    {
        for (UINT32 x = 0; x < Width; x++)
        {
            UINT32 ColorA = 0x00000003;
            UINT32 ColorR = 0;
            UINT32 ColorG = 0;
            UINT32 ColorB = 0;
            UINT32 temp_par, temp_perp;
            UINT32 div_par, div_perp;
            switch (PDir)
            {
                case PD_HORIZONTAL:
                    temp_par  = x;
                    div_par   = Width;
                    temp_perp = y;
                    div_perp  = Height;
                    break;
                case PD_VERTICAL:
                    temp_par  = y;
                    div_par   = Height;
                    temp_perp = x;
                    div_perp  = Width;
                    break;
            }

            if (PS_COLOR_PALETTE == PStyle)
            {
                ColorR = 1023 - ((1023 * temp_perp) / div_perp);
                ColorG = 1023 * (temp_perp + div_par - temp_par) / (div_par + div_perp);
                ColorB = (1023 * temp_par) / div_par;
            }
            else if (PS_MULTIPLE == PStyle)
            {
                switch (4 * temp_perp / div_perp)
                {
                    case 0 :
                        ColorR = 1023 * temp_par / div_par;
                        ColorG = 1023 * temp_par / div_par;
                        ColorB = 1023 * temp_par / div_par;
                        break;
                    case 1 :
                        ColorR = 1023 * temp_par / div_par;
                        ColorG = 0;
                        ColorB = 0;
                        break;
                    case 2 :
                        ColorR = 0;
                        ColorG = 1023 * temp_par / div_par;
                        ColorB = 0;
                        break;
                    case 3 :
                        ColorR = 0;
                        ColorG = 0;
                        ColorB = 1023 * temp_par / div_par;
                        break;
                    default :
                        ColorR = 0;
                        ColorG = 0;
                        ColorB = 1023 * temp_par / div_par;
                        break;
                }
            }
            else if (PS_MFGDIAG == PStyle)
            {
                // Generate one pixel in a2r10g10b10 format.
                // Four stripes, each with diagonal gradients every 256
                // pixels in x and y.  This is the traditional mfg-diags
                // "diagonal stripy white,r,g,b" pattern known and loved.
                switch (4 * temp_perp / div_perp)
                {
                    case 0 :
                        // First 1/4th screen: white.
                        ColorR = ((temp_par + temp_perp) & 0xff) << 2;
                        ColorG = ColorR;
                        ColorB = ColorR;
                        break;
                    case 1 :
                        ColorR = ((temp_par + temp_perp) & 0xff) << 2;
                        ColorG = 0;
                        ColorB = 0;
                        break;
                    case 2 :
                        ColorR = 0;
                        ColorG = ((temp_par + temp_perp) & 0xff) << 2;
                        ColorB = 0;
                        break;
                    default :
                    case 3 :
                        ColorR = 0;
                        ColorG = 0;
                        ColorB = ((temp_par + temp_perp) & 0xff) << 2;
                        break;
                }
            }
            else if (PS_SHALLOW == PStyle)
            {
                switch (4 * temp_perp / div_perp)
                {
                    case 0 :
                        ColorR = 448  + (128 * temp_par / div_par);
                        ColorG = 448  + (128 * temp_par / div_par);
                        ColorB = 448  + (128 * temp_par / div_par);
                        break;
                    case 1 :
                        ColorR = 448  + (128 * temp_par / div_par);
                        ColorG = 512;
                        ColorB = 512;
                        break;
                    case 2 :
                        ColorR = 512;
                        ColorG = 448  + (128 * temp_par / div_par);
                        ColorB = 512;
                        break;
                    case 3 :
                        ColorR = 512;
                        ColorG = 512;
                        ColorB = 448  + (128 * temp_par / div_par);
                        break;
                    default :
                        ColorR = 512;
                        ColorG = 512;
                        ColorB = 448  + (128 * temp_par / div_par);
                        break;
                }
            }

            UINT32 ColorARGB = (ColorA << 30) | (ColorR << 20) | (ColorG << 10) | ColorB;
            DrawPixelARGB(x,y,RectX,RectY,
                          RectWidth,RectHeight,
                          ChunkWidth,ChunkHeight,
                          ColorARGB,&LastColorARGB,
                          BytesPerPixel, WordSize);
        }
    }
    return OK;
}

RC Surface2D::FillPatternRectBox
(

    UINT32 RectX,
    UINT32 RectY,
    UINT32 Width,
    UINT32 Height,
    UINT32 RectWidth,
    UINT32 RectHeight,
    UINT32 ChunkWidth,
    UINT32 ChunkHeight,
    const char* PatternType,
    const char* PatternStyle
)
{
    UINT32 ColorA = 0x00000003;
    UINT32 ColorR;
    UINT32 ColorG;
    UINT32 ColorB;
    UINT32 LastColorARGB = 0;
    UINT32 BytesPerPixel = GetBytesPerPixel();
    UINT32 WordSize = GetWordSize();
    PatternStyleType PStyle;
    if(!strcmp(PatternStyle, "edge"))
    {
        PStyle = PS_EDGE;
    }
    else if (!strcmp(PatternStyle, "50aa"))
    {
        PStyle = PS_50AA;
    }
    else if (!strcmp(PatternStyle, "crosshatched"))
    {
        PStyle = PS_CROSSHATCHED;
    }
    else
    {
        Printf(Tee::PriHigh, "Unrecognized PatternStyle (%s) for PatternType %s\n",
               PatternStyle, PatternType);
        return RC::BAD_PARAMETER;
    }

    for (UINT32 y = 0; y < Height; y++)
    {
        for (UINT32 x = 0; x < Width; x++)
        {
            if (PS_EDGE == PStyle)
            {
                if ((y == 0) || (y == (Height-1)) || (x == 0) || (x == (Width - 1)))
                {
                    ColorR = 0x03FF;
                    ColorG = 0x03FF;
                    ColorB = 0x03FF;
                }
                else
                {
                    ColorR = 0x0000;
                    ColorG = 0x0000;
                    ColorB = 0x0000;
                }
            }
            else if (PS_50AA == PStyle)
            {
                UINT08 y1 = (y    >= ((    Height / 4) - 1)) ? 1 : 0;
                UINT08 y2 = (y    ==  (    Height / 4)     ) ? 1 : 0;
                UINT08 y3 = (y    <= ((    Height / 4) + 1)) ? 1 : 0;
                UINT08 y4 = (y    >= ((3 * Height / 4) - 1)) ? 1 : 0;
                UINT08 y5 = (y    ==  (3 * Height / 4)     ) ? 1 : 0;
                UINT08 y6 = (y    <= ((3 * Height / 4) + 1)) ? 1 : 0;
                UINT08 x1 = (x >= ((    Width  / 4) - 1)) ? 1 : 0;
                UINT08 x2 = (x ==  (    Width  / 4)     ) ? 1 : 0;
                UINT08 x3 = (x <= ((    Width  / 4) + 1)) ? 1 : 0;
                UINT08 x4 = (x >= ((3 * Width  / 4) - 1)) ? 1 : 0;
                UINT08 x5 = (x ==  (3 * Width  / 4)     ) ? 1 : 0;
                UINT08 x6 = (x <= ((3 * Width  / 4) + 1)) ? 1 : 0;
                if (y1 && y6 && x1 && x6)
                {
                    // Inside the edge of the box. Need to do more testing ...
                    if (y3 || y4 || x3 || x4)
                    {
                        // Within the AA line. Need yet more testing ...
                        if ((y2 && !((x == ((    Width  / 4) - 1)) || (x == ((3 * Width  / 4) + 1))))
                            || (y5 && !((x == ((    Width  / 4) - 1)) || (x == ((3 * Width  / 4) + 1))))
                            || (x2 && !((y    == ((    Height / 4) - 1)) || (y    == ((3 * Height / 4) + 1))))
                            || (x5 && !((y    == ((    Height / 4) - 1)) || (y    == ((3 * Height / 4) + 1)))))
                        {
                            // Centre of the line, so make it white ...
                            ColorR = 0x03FF;
                            ColorG = 0x03FF;
                            ColorB = 0x03FF;
                        }
                        else
                        {
                            // edge of the line, so make it grey ...
                            ColorR = 0x0200;
                            ColorG = 0x0200;
                            ColorB = 0x0200;
                        }
                    }
                    else
                    {
                        // Inside the box, so make it black ...
                        ColorR = 0x0000;
                        ColorG = 0x0000;
                        ColorB = 0x0000;
                    }
                }
                else
                {
                    // Outside the box, so make it black ...
                    ColorR = 0x0000;
                    ColorG = 0x0000;
                    ColorB = 0x0000;
                }
            }
            else //CROSSHATCHED
            {
                UINT32 sum;
                UINT32 diff;

                if ((y == 0) || (y == (Height-1)) || (x == 0) || (x == (Width - 1)))
                {
                    ColorR = 0x03FF;
                    ColorG = 0x03FF;
                    ColorB = 0x03FF;
                }
                else
                {
                    sum  = ((y & 0x000F) + (x & 0x000F)) & 0x000F;
                    diff = ((y & 0x000F) - (x & 0x000F)) & 0x000F;
                    if ((sum == 0) || (diff == 0))
                    {
                        ColorR = 0x03FF;
                        ColorG = 0x03FF;
                        ColorB = 0x03FF;
                    }
                    else
                    {
                        if ((sum == 0x0001) || (sum == 0x000F) || (diff == 1) || (diff == 0x000F))
                        {
                            ColorR = 0x0200;
                            ColorG = 0x0200;
                            ColorB = 0x0200;
                        }
                        else
                        {
                            ColorR = 0x0000;
                            ColorG = 0x0000;
                            ColorB = 0x0000;
                        }
                    }
                }
            }

            UINT32 ColorARGB = (ColorA << 30) | (ColorR << 20) | (ColorG << 10) | ColorB;
            DrawPixelARGB(x,y,RectX,RectY,
                          RectWidth,RectHeight,
                          ChunkWidth,ChunkHeight,
                          ColorARGB,&LastColorARGB,
                          BytesPerPixel, WordSize);
        }
    }
    return OK;
}

//----------------------------------------------------------------------------
RC Surface2D::FillPatternRectFSweep
(

    UINT32 RectX,
    UINT32 RectY,
    UINT32 Width,
    UINT32 Height,
    UINT32 RectWidth,
    UINT32 RectHeight,
    UINT32 ChunkWidth,
    UINT32 ChunkHeight,
    const char* PatternType,
    const char* PatternDir,
    const char* PatternStyle
)
{
    UINT32 LastColorARGB = 0;
    UINT32 BytesPerPixel = GetBytesPerPixel();
    UINT32 WordSize = GetWordSize();
    PatternDirType PDir;
    if (!strcmp(PatternDir, "horizontal"))
    {
        PDir = PD_HORIZONTAL;
    }
    else if(!strcmp(PatternDir, "vertical"))
    {
        PDir = PD_VERTICAL;
    }
    else
    {
        Printf(Tee::PriError,
               "Unrecognized PatternDir (%s) "
               "for Pattern Style %s, PatternType %s\n",
               PatternDir, PatternStyle, PatternType);
        return RC::BAD_PARAMETER;
    }

    PatternStyleType PStyle;
    if (!strcmp(PatternStyle, "luma"))
    {
        PStyle = PS_LUMA;
    }
    else if (!strcmp(PatternStyle, "444"))
    {
        PStyle = PS_444;
    }
    else if (!strcmp(PatternStyle, "422"))
    {
        PStyle = PS_422;
    }
    else
    {
        Printf(Tee::PriHigh, "Unrecognized PatternStyle (%s) for PatternType %s\n",
               PatternStyle, PatternType);
        return RC::BAD_PARAMETER;
    }

    for (UINT32 y = 0; y < Height; y++)
    {
        for (UINT32 x = 0; x < Width; x++)
        {
            UINT32 ColorA = 0x00000003;
            UINT32 ColorR;
            UINT32 ColorG;
            UINT32 ColorB;
            double xf = 0.0;
            double xmax = 0.0;
            double pi = 3.141592653589793;
            double yf = 0.0;
            double cb = 0.0;
            double cr = 0.0;
            //UINT32 y;
            UINT32 mid_way;

            switch (PDir)
            {
                case PD_HORIZONTAL:
                    xf      = (double)x;
                    xmax    = (double)(Width);
                    yf      = (double)y;
                    mid_way = Height / 2;
                    break;
                case PD_VERTICAL:
                    xf      = (double)y;
                    xmax    = (double)(Height);
                    yf      = (double)x;
                    mid_way = Width / 2;
                    break;
            }

            if (PS_LUMA == PStyle)
            {
                yf = 0.5 + (cos(pi * xf * xf / (2.0 * xmax)) / 2.2);
                cb = 0.0;
                cr = 0.0;
            }
            else if (PS_444 == PStyle)
            {
                if (yf < mid_way)
                {
                    yf = 0.5 + (cos(pi * xf * xf / (2.0 * xmax)) / 2.2);
                    cb = 0.0;
                    cr = 0.0;
                }
                else
                {
                    yf = 0.5;
                    cb =  (cos(pi * xf * xf / (2.0 * xmax)) / 5.0);
                    cr = -(cos(pi * xf * xf / (2.0 * xmax)) / 5.0);
                }
            }
            else if (PS_422 == PStyle)
            {
                if (yf < mid_way)
                {
                    yf = 0.5 + (cos(pi * xf * xf / (2.0 * xmax)) / 2.2);
                    cb = 0.0;
                    cr = 0.0;
                }
                else
                {
                    yf = 0.5;
                    cb =  (cos(pi * xf * xf / (4.0 * xmax)) / 5.0);
                    cr = -(cos(pi * xf * xf / (4.0 * xmax)) / 5.0);
                }
            }

            ColorR    = (UINT32)(1023.0 * (yf + 2.0 * cr));
            ColorG    = (UINT32)(1023.0 * (yf - cr - cb));
            ColorB    = (UINT32)(1023.0 * (yf + 2.0 * cb));
            UINT32 ColorARGB = (ColorA << 30) | (ColorR << 20) | (ColorG << 10) | ColorB;
            DrawPixelARGB(x,y,RectX,RectY,
                          RectWidth,RectHeight,
                          ChunkWidth,ChunkHeight,
                          ColorARGB,&LastColorARGB,
                          BytesPerPixel, WordSize);
        }
    }
    return OK;
}

//----------------------------------------------------------------------------
RC Surface2D::FillPatternRectHueCircle
(

    UINT32 RectX,
    UINT32 RectY,
    UINT32 Width,
    UINT32 Height,
    UINT32 RectWidth,
    UINT32 RectHeight,
    UINT32 ChunkWidth,
    UINT32 ChunkHeight,
    const char* PatternType,
    const char* PatternStyle
)
{
    UINT32 LastColorARGB = 0;
    UINT32 BytesPerPixel = GetBytesPerPixel();
    UINT32 WordSize = GetWordSize();
    PatternStyleType PStyle;
    if (!strcmp(PatternStyle, "luma25"))
    {
        PStyle = PS_LUMA25;
    }
    else if (!strcmp(PatternStyle, "luma50"))
    {
        PStyle = PS_LUMA50;
    }
    else if (!strcmp(PatternStyle, "luma75"))
    {
        PStyle = PS_LUMA75;
    }
    else
    {
        Printf(Tee::PriHigh, "Unrecognized PatternStyle (%s) for PatternType %s\n",
               PatternStyle, PatternType);
        return RC::BAD_PARAMETER;
    }

    for (UINT32 y = 0; y < Height; y++)
    {
        for (UINT32 x = 0; x < Width; x++)
        {
            UINT32 ColorA = 0x00000003;
            UINT32 ColorR;
            UINT32 ColorG;
            UINT32 ColorB;
            double r;
            double g;
            double b;
            double Y;
            double Cb;
            double Cr;
            double new_Cr;
            double new_Cb;
            double ratio;

            if (PS_LUMA25 == PStyle)
            {
                Y = 0.25;
            }
            else if (PS_LUMA50 == PStyle)
            {
                Y = 0.50;
            }
            else // if (PS_LUMA75 == PStyle)
            {
                Y = 0.75;
            }

            Cr = 0.7 * (0.5 - ((double)(y) / (double)(Height)));
            Cb = 0.7 * (((double)(x) / (double)(Width)) - 0.5);

            // Colwert to RGB ...

            r  = Y + 1.402000 * Cr;
            g  = Y - 0.714136 * Cr - 0.344136 * Cb;
            b  = Y + 1.772000 * Cb;

            // Legalise the RGB values, maintaining the correct hue ...

            if ((r < 0.0) || (r > 1.0) || (g < 0.0) || (g > 1.0) || (b < 0.0) || (b > 1.0))
            {
                if (r < 0.0)
                {
                    new_Cr = -Y / 1.40200;
                    ratio  = new_Cr / Cr;
                    new_Cb = ratio * Cb;
                    Cr     = new_Cr;
                    Cb     = new_Cb;
                    r      = Y + 1.402000 * Cr;
                    g      = Y - 0.714136 * Cr - 0.344136 * Cb;
                    b      = Y + 1.772000 * Cb;
                }
                if (r > 1.0)
                {
                    new_Cr = (1.0 - Y) / 1.40200;
                    ratio  = new_Cr / Cr;
                    new_Cb = ratio * Cb;
                    Cr     = new_Cr;
                    Cb     = new_Cb;
                    r      = Y + 1.402000 * Cr;
                    g      = Y - 0.714136 * Cr - 0.344136 * Cb;
                    b      = Y + 1.772000 * Cb;
                }
                if (b < 0.0)
                {
                    new_Cb = -Y / 1.77220;
                    ratio  = new_Cb / Cb;
                    new_Cr = ratio * Cr;
                    Cr     = new_Cr;
                    Cb     = new_Cb;
                    r      = Y + 1.402000 * Cr;
                    g      = Y - 0.714136 * Cr - 0.344136 * Cb;
                    b      = Y + 1.772000 * Cb;
                }
                if (b > 1.0)
                {
                    new_Cb = (1.0 - Y) / 1.77220;
                    ratio  = new_Cb / Cb;
                    new_Cr = ratio * Cr;
                    Cr     = new_Cr;
                    Cb     = new_Cb;
                    r      = Y + 1.402000 * Cr;
                    g      = Y - 0.714136 * Cr - 0.344136 * Cb;
                    b      = Y + 1.772000 * Cb;
                }
                if (g < 0.0)
                {
                    ratio  = Y / (0.714136 * Cr + 0.344136 * Cb);
                    Cr    *= ratio;
                    Cb    *= ratio;
                    r      = Y + 1.402000 * Cr;
                    g      = Y - 0.714136 * Cr - 0.344136 * Cb;
                    b      = Y + 1.772000 * Cb;
                }
                if (g > 1.0)
                {
                    ratio  = (Y - 1.0) / (0.714136 * Cr + 0.344136 * Cb);
                    Cr    *= ratio;
                    Cb    *= ratio;
                    r      = Y + 1.402000 * Cr;
                    g      = Y - 0.714136 * Cr - 0.344136 * Cb;
                    b      = Y + 1.772000 * Cb;
                }
            }

            ColorR    = (UINT32)(1023.0 * r);
            ColorG    = (UINT32)(1023.0 * g);
            ColorB    = (UINT32)(1023.0 * b);

            UINT32 ColorARGB = (ColorA << 30) | (ColorR << 20) | (ColorG << 10) | ColorB;
            DrawPixelARGB(x,y,RectX,RectY,
                          RectWidth,RectHeight,
                          ChunkWidth,ChunkHeight,
                          ColorARGB,&LastColorARGB,
                          BytesPerPixel, WordSize);
        }
    }
    return OK;
}

//----------------------------------------------------------------------------
RC Surface2D::FillPatternRectSpecial
(

    UINT32 RectX,
    UINT32 RectY,
    UINT32 Width,
    UINT32 Height,
    UINT32 RectWidth,
    UINT32 RectHeight,
    UINT32 ChunkWidth,
    UINT32 ChunkHeight,
    const char* PatternType,
    const char* PatternStyle
)
{
    UINT32 LastColorARGB = 0;
    UINT32 ColorARGB = 0;
    UINT32 multiCSCNum = 0;
    UINT32 BytesPerPixel = GetBytesPerPixel();
    UINT32 WordSize = GetWordSize();
    PatternStyleType PStyle = PS_NONE;
    if (!strcmp(PatternStyle, "deflicker"))
    {
        PStyle = PS_DEFLICKER;
    }
    else if (!strcmp(PatternStyle, "dfpbars"))
    {
        PStyle = PS_DFPBARS;
    }
    else if (!strcmp(PatternStyle, "multicsc2"))
    {
        multiCSCNum = 2;
    }
    else if (!strcmp(PatternStyle, "multicsc4"))
    {
        multiCSCNum = 4;
    }
    else
    {
        Printf(Tee::PriHigh, "Unrecognized PatternStyle (%s) for PatternType %s\n",
               PatternStyle, PatternType);
        return RC::BAD_PARAMETER;
    }

    for (UINT32 y = 0; y < Height; y++)
    {
        for (UINT32 x = 0; x < Width; x++)
        {
            if (multiCSCNum == 2)
            {
                ColorARGB = (x % 2) << 30; // csc info should be 0->1->0->1->...
            }
            else if (multiCSCNum == 4)
            {
                ColorARGB = ((x % 2) << 30) | ((x % 2) << 31); // csc info should be 0->3->0->3->...
            }
            else if (PS_DEFLICKER == PStyle)
            {
                UINT32 ColorA = 0x00000003;
                UINT32 ColorR;
                UINT32 ColorG;
                UINT32 ColorB;
                double pi = 3.141592653589793;
                double r;
                double g;
                double b;
                double Y;
                double Cb;
                double Cr;

                if (y & 1) // ODD line ...
                    Y = 0.5 + 0.5 * ((double)(x) / (double)(Width));
                else         // EVEN line ...
                    Y = 0.5 - 0.5 * ((double)(x) / (double)(Width));

                // Create some chroma modulation, just to make the pattern interesting ...

                Cb = 0.2 * cos(y * pi / 16.0); // Repeat evey 32 pixels
                Cr = 0.2 * cos(x * pi / 16.0); // Repeat evey 32 lines

                // Colwert to RGB ...

                r  = Y + 2.0 * Cr;
                g  = Y - Cr - Cb;
                b  = Y + 2.0 * Cb;

                // Make sure it's legal ...

                if (r < 0.0) r = 0.0;
                if (r > 1.0) r = 1.0;
                if (g < 0.0) g = 0.0;
                if (g > 1.0) g = 1.0;
                if (b < 0.0) b = 0.0;
                if (b > 1.0) b = 1.0;

                ColorR    = (UINT32)(1023.0 * r);
                ColorG    = (UINT32)(1023.0 * g);
                ColorB    = (UINT32)(1023.0 * b);

                ColorARGB = (ColorA << 30) | (ColorR << 20) | (ColorG << 10) | ColorB;
            }
            else // if (PS_DFPBARS == PStyle)
            {
                // There will be six horizontal stripes on the screen.
                // The first stripe will have every even pixel full red.     All odd pixels will be black.
                // The second stripe will have every even pixel full green.  All odd pixels will be black.
                // The third stripe will have every even pixel full blue.    All odd pixels will be black.
                // The fourth stripe will have every odd pixel full red.     All even pixels will be black.
                // The fifth stripe will have every odd pixel full green.    All even pixels will be black.
                // The sixth stripe will have every odd pixel full blue.     All even pixels will be black.
                // If any area is the incorrect color, or is all black, at least one of the LVDS/TMDS channels is bad.
                UINT32 ColorA = 0x00000003;
                UINT32 ColorR = 0;
                UINT32 ColorG = 0;
                UINT32 ColorB = 0;

                switch (6 * y / Height)
                {
                    case 0 :
                        if ((x & 1) == 0)
                            ColorR = 1023;
                        break;
                    case 1 :
                        if ((x & 1) == 0)
                            ColorG = 1023;
                        break;
                    case 2 :
                        if ((x & 1) == 0)
                            ColorB = 1023;
                        break;
                    case 3 :
                        if ((x & 1) == 1)
                            ColorR = 1023;
                        break;
                    case 4 :
                        if ((x & 1) == 1)
                            ColorG = 1023;
                        break;
                    case 5 :
                    default :
                        if ((x & 1) == 1)
                            ColorB = 1023;
                        break;
                }
                ColorARGB = (ColorA << 30) | (ColorR << 20) | (ColorG << 10) | ColorB;
            }
            DrawPixelARGB(x,y,RectX,RectY,
                          RectWidth,RectHeight,
                          ChunkWidth,ChunkHeight,
                          ColorARGB,&LastColorARGB,
                          BytesPerPixel, WordSize);
        }
    }
    return OK;
}

//----------------------------------------------------------------------------
RC Surface2D::FillPatternRectFpGray
(
    UINT32 RectX,
    UINT32 RectY,
    UINT32 Width,
    UINT32 Height,
    UINT32 RectWidth,
    UINT32 RectHeight,
    UINT32 ChunkWidth,
    UINT32 ChunkHeight
)
{
    UINT32 LastColorARGB = 0;
    UINT32 BytesPerPixel = GetBytesPerPixel();
    UINT32 WordSize = GetWordSize();

    for (UINT32 y = 0; y < Height; y++)
    {
        for (UINT32 x = 0; x < Width; x++)
        {
            UINT32 ColorARGB = PaintFpGray(Width, Height, x, y);
            DrawPixelARGB(x,y,RectX,RectY,
                          RectWidth,RectHeight,
                          ChunkWidth,ChunkHeight,
                          ColorARGB,&LastColorARGB,
                          BytesPerPixel, WordSize);
        }
    }
    return OK;
}

//-----------------------------------------------------------------------------
RC Surface2D::FillPatternRectFullRange
(
    UINT32 RectX,
    UINT32 RectY,
    UINT32 Width,
    UINT32 Height,
    UINT32 RectWidth,
    UINT32 RectHeight,
    UINT32 ChunkWidth,
    UINT32 ChunkHeight,
    const char *PatternType,
    const char *PatternStyle,
    const char *PatternDir
)
{
    UINT32 BytesPerPixel = GetBytesPerPixel();
    UINT32 x, y, PixelX, PixelY;
    RC rc;

    UINT32 alphaBits = 0;
    UINT32 multiCSCNum = 0;
    PatternDirType PDir = PD_VERTICAL;

    if (!IsYUVLegacy())
    {
        ColorUtils::CompBitsRGBA(m_ColorFormat, 0, 0, 0, &alphaBits);
    }
    if (!strcmp (PatternStyle, "multicsc2"))
    {
        multiCSCNum = 2;
    }
    else if (!strcmp (PatternStyle, "multicsc4"))
    {
        multiCSCNum = 4;
    }
    if (!strcmp(PatternDir, "horizontal"))
    {
        PDir = PD_HORIZONTAL;
    }

    for (y = 0; y < Height; y++)
    {
        for (x = 0; x < Width; x++)
        {
            UINT64 ColorFullRange = 0;
            UINT32 ColorComps[NumComps], Alpha;
            UINT32 CompBits[NumComps], MaxCompRange;
            UINT32 i;
            UINT32 uRgbStyle, uLuma;
            char   szRgbStyle[16], szLumaStyle[16];
            UINT32 uChroma, uChromaU, uChromaV;

            // Always use the maximum value for the alpha component.
            Alpha = 0xFFFFFFFF;

            // Check whether this is the 'grad' style.
            if (!strcmp(PatternStyle, "grad"))
            {
                if (IsYUVLegacy())
                {
                    Printf (Tee::PriHigh, "The (full_range, grad) pattern is only supported on non-YUV formats.\n");
                    return RC::BAD_PARAMETER;
                }
                for (i = 0; i < NumComps; i ++)
                {
                    if (y & (1 << i))
                    {
                        ColorComps[i] = x;
                    }
                    else
                    {
                        ColorComps[i] = 0;
                    }
                }
            }
            else if (!strcmp(PatternStyle, "continuous"))
            {
                if (IsYUVLegacy())
                {
                    Printf (Tee::PriHigh, "The (full_range, continuous) pattern is only supported on non-YUV formats.\n");
                    return RC::BAD_PARAMETER;
                }
                ColorUtils::CompBitsRGBA (m_ColorFormat, &CompBits[redComp],
                    &CompBits[greenComp], &CompBits[blueComp], 0);
                for (i = 0; i < NumComps; i ++)
                {
                    if (strlen(PatternDir) != NumComps)
                    {
                        Printf (Tee::PriHigh, "Pattern direction for type 'full_range', style 'continuous' must have 3 characters each of +-~0 \n");
                        return RC::BAD_PARAMETER;
                    }
                    UINT64 component = 0;
                    switch(PatternDir[i])
                    {
                    case '+':
                        component = x + y * static_cast<UINT64>(Width);
                        break;
                    case '-':
                        component = (static_cast<UINT64>(Width) * Height - 1 -
                                     (x + y * static_cast<UINT64>(Width)));
                        break;
                    case '|':
                        component = x * static_cast<UINT64>(Height) + y;
                        break;
                    case '^':
                        {
                        const UINT64 max = static_cast<UINT64>(Width) * Height;
                        const UINT64 half = max / 2;
                        component = x + y * static_cast<UINT64>(Width);
                        component = (component >= half ?
                                     max - 2 * (component - half) - 1 :
                                     2 * component);
                        }
                        break;
                    case 'v':
                        {
                        const UINT64 max = static_cast<UINT64>(Width) * Height;
                        const UINT64 half = max / 2;
                        component = x + y * static_cast<UINT64>(Width);
                        component = (component >= half ?
                                     2 * (component - half) :
                                     2 * (half - component) - 1);
                        }
                        break;
                    case '~':
                        {
                        const UINT64 max = static_cast<UINT64>(Width) * Height;
                        const UINT64 half = max / 2;
                        component = x + y * static_cast<UINT64>(Width);
                        component = (component >= half ?
                                     component - half :
                                     component + half);
                        }
                        break;
                    case '0':
                        component = 0;
                        break;
                    }

                    // scale the value up to the maximum range of the pixel
                    MaxCompRange = (1 << CompBits[i]) - 1;
                    UINT64 scaling = (MaxCompRange + 1) / (Width * Height);
                    ColorComps[i] = UINT32(component * scaling);
                }
            }
            else if (multiCSCNum != 0)
            {
                UINT32 Dist = 0;
                UINT32 Size = 0;
                switch (PDir)
                {
                    case PD_VERTICAL:
                        // fill color value increase 1 by 1 in vertical
                        for (i = 0; i < NumComps; i ++)
                        {
                            ColorComps[i] = y + Height * x;
                        }
                        Dist = x;
                        Size = Width;
                        break;
                    case PD_HORIZONTAL:
                        // fill color value increase 1 by 1 in horizontal
                        for (i = 0; i < NumComps; i ++)
                        {
                            ColorComps[i] = Width * y + x;
                        }
                        Dist = y;
                        Size = Height;
                        break;
                }

                // fill the surface csc info on alpha bit
                // split screen to bars with different csc value, bars direction follow PatternDir
                // 2 csc mode, csc value can be 0, 1; 4 csc mode, csc value can be 0, 1, 2, 3
                Alpha = (multiCSCNum * Dist / Size);
                // if 2/10/10/10 format, csc value is 2 bits value
                // if 8/8/8/8 format, csc value is highest 2 bits in 8 bits alpha, so transfer it
                Alpha = (Alpha << alphaBits) >> 2;
            }
            else
            {
                bool bPatternFound = false;

                // Check whether this is the 'rgb_#' style.
                for (uRgbStyle = 0; uRgbStyle < 8; uRgbStyle ++)
                {
                    sprintf (szRgbStyle, "rgb_%d", uRgbStyle);
                    if (!strcmp (PatternStyle, szRgbStyle))
                    {
                        if (IsYUVLegacy())
                        {
                            Printf (Tee::PriHigh, "The (full_range, rgb_#) pattern is only supported on non-YUV formats.\n");
                            return RC::BAD_PARAMETER;
                        }
                        for (i = 0; i < NumComps; i ++)
                        {
                            if (uRgbStyle & (1 << i))
                            {
                                ColorComps[i] = Width * y + x;
                            }
                            else
                            {
                                ColorComps[i] = 0;
                            }
                        }
                        bPatternFound = true;
                        break;
                    }
                }

                // Check whether this is the 'luma_#' style.
                for (uLuma = 1; uLuma <= 254; uLuma ++)
                {
                    sprintf (szLumaStyle, "luma_%d", uLuma);
                    if (!strcmp (PatternStyle, szLumaStyle))
                    {
                        if (!IsYUVLegacy())
                        {
                            Printf (Tee::PriHigh, "The (full_range, luma_#) pattern is only supported on YUV formats.\n");
                            return RC::BAD_PARAMETER;
                        }
                        bPatternFound = true;

                        uChroma = (Width * y + x) / 2;
                        uChromaU = uChroma >> 8;
                        if (uChromaU < 1) uChromaU = 1;
                        if (uChromaU > 254) uChromaU = 254;
                        uChromaV = uChroma & 0xFF;
                        if (uChromaV < 1) uChromaV = 1;
                        if (uChromaV > 254) uChromaV = 254;

                        ColorComps[redComp] = uChromaV;
                        ColorComps[greenComp] = uLuma;
                        ColorComps[blueComp] = uChromaU;
                        Alpha = uLuma;
                    }
                }

                // Check whether the pattern style is invalid.
                if (!bPatternFound)
                {
                    Printf (Tee::PriHigh, "Unrecognized PatternStyle (%s) for PatternType (%s)\n",
                        PatternStyle, PatternType);
                    return RC::BAD_PARAMETER;
                }
            }

            // Encode the pixel color to the surface's pixel format and write it to the surface.
            ColorFullRange = ColorUtils::EncodePixel (m_ColorFormat, ColorComps[redComp],
                ColorComps[greenComp], ColorComps[blueComp], Alpha);

            if (IsYUVLegacy())
            {
                // skip the even X pixel. we do the fill at the odd pixel.
                if (!(x & 0x1))
                {
                    continue;
                }
            }

            for (UINT32 a = 0; a < m_ArraySize; a++)
            {
                for (UINT32 PixelZ = 0; PixelZ < m_Depth; PixelZ++)
                {
                    for (PixelY = RectY + y*ChunkHeight;
                         PixelY < RectY + (y+1)*ChunkHeight;
                         PixelY++)
                    {
                        if (PixelY > m_Height)
                            break;
                        if (IsYUVLegacy())
                        {
                            for (PixelX = (RectX + x*ChunkWidth) & ~(0x1);
                                 PixelX < RectX + (x+1)*ChunkWidth;
                                 PixelX+=2)
                            {
                                if (PixelX > m_Width)
                                    break;
                                UINT64 Offset = GetPixelOffset(PixelX, PixelY, PixelZ, a);
                                CHECK_RC(MapOffset(Offset, &Offset));
                                UINT08 *pDst = (UINT08 *)m_Address + Offset;
                                Platform::VirtualWr(pDst, (char *) &ColorFullRange, 4);
                            }
                        }
                        else
                        {
                            for (PixelX = RectX + x*ChunkWidth;
                                 PixelX < RectX + (x+1)*ChunkWidth;
                                 PixelX++)
                            {
                                if (PixelX > m_Width)
                                    break;
                                UINT64 Offset = GetPixelOffset(PixelX, PixelY, PixelZ, a);
                                CHECK_RC(MapOffset(Offset, &Offset));
                                UINT08 *pDst = (UINT08 *)m_Address + Offset;
                                Platform::VirtualWr(pDst, (char *) &ColorFullRange, BytesPerPixel);
                            }
                        }
                    }
                }
            }
        } // for x
    } // for y
    return OK;
}

//-----------------------------------------------------------------------------
RC Surface2D::FillPatternRect
(
UINT32 RectX,
UINT32 RectY,
UINT32 RectWidth,
UINT32 RectHeight,
UINT32 ChunkWidth,
UINT32 ChunkHeight,
const char *PatternType,
const char *PatternStyle,
const char *PatternDir,
UINT32 subdev
)
{
    RC rc;

    UINT32 Width = (RectWidth + ChunkWidth-1) / ChunkWidth;
    UINT32 Height = (RectHeight + ChunkHeight-1) / ChunkHeight;

    if (IsYUVLegacy())
    {
        if (!CheckYCrCbFillParams(Width, ChunkWidth))
            return RC::BAD_PARAMETER;
    }

    const Surface2D::MappingSaver SavedMapping(*this);
    if (m_Address)
    {
        Unmap();
    }

    CHECK_RC(CheckSubdeviceInstance(&subdev));
    CHECK_RC(MapRect(RectX, RectY, RectWidth, RectHeight, subdev));

    // We need to fill it like 444 surface. Compressed pixels will be skipped in DrawPixelARGB()
    if (IsSurface422Compressed())
    {
        Width *= 2;
    }
    else if (IsSurface420Compressed())
    {
        Width *= 2;
        Height *= 2;
    }

    if (!strcmp(PatternType, "random"))
    {
        return FillPatternRectRandom(RectX,RectY,Width,Height,RectWidth,
                                     RectHeight,ChunkWidth,ChunkHeight,
                                     PatternStyle);
    }
    else if (!strcmp(PatternType, "bars"))
    {
        return FillPatternRectBars(RectX,RectY,Width,Height,RectWidth,
                                       RectHeight,ChunkWidth,ChunkHeight,
                                       PatternType,PatternDir, PatternStyle);
    }
    else if (!strcmp(PatternType, "gradient"))
    {
        return FillPatternRectGradient(RectX,RectY,Width,Height,RectWidth,
                                       RectHeight,ChunkWidth,ChunkHeight,
                                       PatternType,PatternDir, PatternStyle);
    }
    else if (!strcmp(PatternType, "box"))
    {
        return FillPatternRectBox(RectX,RectY,Width,Height,RectWidth,
                                       RectHeight,ChunkWidth,ChunkHeight,
                                       PatternType,PatternStyle);
    }
    else if (!strcmp(PatternType, "fsweep"))
    {
        return FillPatternRectFSweep(RectX,RectY,Width,Height,RectWidth,
                                       RectHeight,ChunkWidth,ChunkHeight,
                                       PatternType,PatternDir, PatternStyle);
    }
    else if (!strcmp(PatternType, "hue_circle"))
    {
        return FillPatternRectHueCircle(RectX,RectY,Width,Height,RectWidth,
                                       RectHeight,ChunkWidth,ChunkHeight,
                                       PatternType,PatternStyle);
    }
    else if (!strcmp(PatternType, "special"))
    {
        return FillPatternRectSpecial(RectX,RectY,Width,Height,RectWidth,
                                       RectHeight,ChunkWidth,ChunkHeight,
                                       PatternType,PatternStyle);
    }
    else if (!strcmp(PatternType, "fp_gray"))
    {
        return FillPatternRectFpGray(RectX,RectY,Width,Height,RectWidth,
                                       RectHeight,ChunkWidth,ChunkHeight);
    }
    else if (!strcmp(PatternType, "full_range"))
    {
        return FillPatternRectFullRange(RectX,RectY,Width,Height,RectWidth,
                                       RectHeight,ChunkWidth,ChunkHeight,
                                       PatternType,PatternStyle,PatternDir);
    }
    else
    {
        Printf(Tee::PriHigh, "Unrecognized PatternType (%s)\n",
               PatternType);
        return RC::BAD_PARAMETER;
    }
}

//-----------------------------------------------------------------------------
RC Surface2D::ApplyFp16GainOffset (float fGainRed, float fOffsetRed,
                                   float fGainGreen, float fOffsetGreen,
                                   float fGainBlue, float fOffsetBlue,
                                   UINT32 subdev)
{
    RC rc;
    CHECK_RC (CheckSubdeviceInstance (&subdev));

    const Surface2D::MappingSaver SavedMapping (*this);
    if (m_Address)
    {
        Unmap ();
    }

    // The gain & offset only make sense on the RF16_GF16_BF16_AF16 format.
    MASSERT (m_ColorFormat == ColorUtils::RF16_GF16_BF16_AF16);
    for (UINT32 Part = 0; Part < GetNumParts (); Part++)
    {
        UINT16 fp16Red, fp16Green, fp16Blue;
        float  fRed, fGreen, fBlue;

        CHECK_RC(MapPart(Part, subdev));
        for (UINT32 Offset = 0; Offset < GetPartSize (Part); Offset += 8)
        {
            // Get the color of this FP16 pixel.
            fp16Red = MEM_RD16 ((UINT08 *) m_Address + Offset + 0);
            fp16Green = MEM_RD16 ((UINT08 *) m_Address + Offset + 2);
            fp16Blue = MEM_RD16 ((UINT08 *) m_Address + Offset + 4);

            // Colwert it to FP32 format.
            fRed = Utility::Float16ToFloat32 (fp16Red);
            fGreen = Utility::Float16ToFloat32 (fp16Green);
            fBlue = Utility::Float16ToFloat32 (fp16Blue);

            // Apply the gain & offset.
            fRed = fRed * fGainRed + fOffsetRed;
            fGreen = fGreen * fGainGreen + fOffsetGreen;
            fBlue = fBlue * fGainBlue + fOffsetBlue;

            // Colwert it back to FP16 format.
            fp16Red = Utility::Float32ToFloat16 (fRed);
            fp16Green = Utility::Float32ToFloat16 (fGreen);
            fp16Blue = Utility::Float32ToFloat16 (fBlue);

            // Update the color of this FP16 pixel.
            MEM_WR16 ((UINT08 *)m_Address + Offset + 0, fp16Red);
            MEM_WR16 ((UINT08 *)m_Address + Offset + 2, fp16Green);
            MEM_WR16 ((UINT08 *)m_Address + Offset + 4, fp16Blue);
        }
        Unmap();
    }
    return OK;
}

//----------------------------------------------------------------------------
UINT32 Surface2D::PaintFpGray(UINT32 Width, UINT32 Height, UINT32 x, UINT32 y, void * extra)
{
    UINT32 barHeight;
    UINT32 barWidth;
    UINT32 color;
    UINT32 gray8;
    UINT32 yLow, yHigh;
    double xL, xR;
    double slope;

    #define HORIZONTAL_BAR_COVERAGE 0.85 // Percentage of screen height the horizontal bars will cover
    barHeight = (UINT32)(Height * HORIZONTAL_BAR_COVERAGE / 15);
    barWidth = (UINT32)((Width+15) / 15);

    if (x == 0 || x == 1 || x == (Width-2) || x == (Width-1))
    {
        return 0xFFFFFFFF; // white
    }

    // Draw horizontal bars (15 of them)
    if (y < (barHeight * 15) && y % barHeight != 0)
    {
        if (y < barHeight * 1)
            gray8 = 0x16;
        else if (y < barHeight * 3)
            gray8 = 0x23;
        else if (y < barHeight * 4)
            gray8 = 0x40;
        else if (y < barHeight * 5)
            gray8 = 0x49;
        else if (y < barHeight * 6)
            gray8 = 0x4A;
        else if (y < barHeight * 7)
            gray8 = 0x4F;
        else if (y < barHeight * 8)
            gray8 = 0x71;
        else if (y < barHeight * 9)
            gray8 = 0x8D;
        else if (y < barHeight * 10)
            gray8 = 0x92;
        else if (y < barHeight * 11)
            gray8 = 0x93;
        else if (y < barHeight * 12)
            gray8 = 0x94;
        else if (y < barHeight * 13)
            gray8 = 0xD5;
        else if (y < barHeight * 14)
            gray8 = 0xAA;
        else if (y < barHeight * 15)
            gray8 = 0x55;
        else
            gray8 = 0xFF;
    }
    else if (y > (barHeight*15)) // Draw the vertical bars at the bottom of the screen (15 of them)
    {
        if (x % barWidth != 0)
        {
            if (x < barWidth * 1)
                gray8 = 0x16;
            else if (x < barWidth * 3)
                gray8 = 0x23;
            else if (x < barWidth * 4)
                gray8 = 0x40;
            else if (x < barWidth * 5)
                gray8 = 0x49;
            else if (x < barWidth * 6)
                gray8 = 0x4A;
            else if (x < barWidth * 7)
                gray8 = 0x4F;
            else if (x < barWidth * 8)
                gray8 = 0x71;
            else if (x < barWidth * 9)
                gray8 = 0x8D;
            else if (x < barWidth * 10)
                gray8 = 0x92;
            else if (x < barWidth * 11)
                gray8 = 0x93;
            else if (x < barWidth * 12)
                gray8 = 0x94;
            else if (x < barWidth * 13)
                gray8 = 0xD5;
            else if (x < barWidth * 14)
                gray8 = 0xAA;
            else if (x < barWidth * 15)
                gray8 = 0x55;
            else
                gray8 = 0xFF;
        }
        else // Put a white line between vertical bars
        {
            gray8 = 0xFF;
        }
    }
    else // Put a white line between horizontal bars
    {
        gray8 = 0xFF;
    }

    // Colwert 8-bit gray color to a2r10g10b10 color
    color = 0xc0000000 // a2 == max
           | (gray8 << (20 + 2))
           | (gray8 << (10 + 2))
           | (gray8 << (0  + 2));

    // Now figure out if this pixel is on a regular bar or one of the diagonals
    // First we check the white diagonal
    xL = Width * 0.05;
    xR = Width * 0.55;
    slope = (Height - (Height*0.10))/(xR-xL);

    if (x < xL)
    {
        return color;
    }
    else if (x <= xR)
    {
        yLow = (UINT32)(slope*(x - xL) + 0);
        yHigh = (UINT32)(slope*(x - xL) + (Height*0.10));

        if (y <= yHigh && y >= yLow)
        {
            return 0xFFFFFFFF;
        }
    }

    // Now check the black diagonal
    xL = (Width * 0.45);
    xR = (Width * 0.95);
    slope = (Height - (Height*0.10))/(xR-xL);

    if (x < xL)
    {
        return color;
    }
    else if (x <= xR)
    {
        yLow = (UINT32)(slope*(x - xL) + 0);
        yHigh = (UINT32)(slope*(x - xL) + (Height*0.10));

        if (y <= yHigh && y >= yLow)
        {
            return 0x0;
        }
    }

    return color;
}

//-----------------------------------------------------------------------------
RC Surface2D::WriteTga(const char *FileName, UINT32 subdev)
{
    RC rc;
    UINT32 BytesPerPixel = GetBytesPerPixel();
    ColorUtils::Format MyColorFormat = m_ColorFormat;

    if (IsYUVLegacy())
    {
        BytesPerPixel = 4;
        MyColorFormat = ColorUtils::A8R8G8B8;
    }

    vector<UINT08> Temp;
    CHECK_RC(CreatePitchImage(&Temp, subdev));

    UINT32 WholeHeight = m_Height * m_Depth * m_ArraySize;

    if (IsYUVLegacy())
    {
        vector<UINT08> FileTemp(m_Width*WholeHeight*4);
        ColorUtils::ColwertYUV422ToYUV444(&Temp[0], &FileTemp[0],
                                          m_Width*WholeHeight, m_ColorFormat);

        Temp = FileTemp;
    }

    rc = ImageFile::WriteTga(FileName, MyColorFormat, &Temp[0],
                             m_Width, WholeHeight, m_Width*BytesPerPixel,
                             false, false, false);
    return rc;
}

//-----------------------------------------------------------------------------
RC Surface2D::WriteTgaRect(UINT32 x, UINT32 y, UINT32 Width, UINT32 Height,
                           const char *FileName, UINT32 subdev)
{
    UINT32 BytesPerPixel = GetBytesPerPixel();
    ColorUtils::Format MyColorFormat = m_ColorFormat;

    if (IsYUVLegacy())
    {
        BytesPerPixel = 4;
        MyColorFormat = ColorUtils::A8R8G8B8;
    }

    vector<UINT08> Temp;
    RC rc;
    CHECK_RC(CreatePitchSubImage(x, y, Width, Height, &Temp, subdev));

    if (IsYUVLegacy())
    {
        vector<UINT08> FileTemp(Width*Height*4);
        ColorUtils::ColwertYUV422ToYUV444(&Temp[0], &FileTemp[0],
                                          Width*Height, m_ColorFormat);
        Temp.swap(FileTemp);
    }

    return ImageFile::WriteTga(FileName, MyColorFormat, &Temp[0],
                             Width, Height, Width*BytesPerPixel,
                             false, false, false);
}

//-----------------------------------------------------------------------------
static RC CheckSurfOnGPU(bool split,  Memory::Location loc)
{
    if(split || loc != Memory::Fb)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    return OK;
}

//-----------------------------------------------------------------------------
RC Surface2D::FillConstantOnGpu(UINT32 constant)
{    RC rc;
    CHECK_RC(CheckSurfOnGPU(m_Split, m_Location));
    CHECK_RC(GpuUtility::FillConstantOnGpu(GetGpuDev(),
                GetMemHandle(),
                GetAllocSize(),
                constant));
    return rc;
}

//-----------------------------------------------------------------------------
RC Surface2D::FillRandomOnGpu(UINT32 seed)
{    RC rc;
    CHECK_RC(CheckSurfOnGPU(m_Split, m_Location));
    CHECK_RC(GpuUtility::FillRandomOnGpu(GetGpuDev(),
                GetMemHandle(),
                GetAllocSize(),
                seed));
    return rc;
}

//-----------------------------------------------------------------------------
RC Surface2D::GetCRC(vector<UINT32> *pCrc, UINT32 subdev)
{
    RC rc;
    vector<UINT08> Temp;
    CHECK_RC(CreatePitchImage(&Temp, subdev));

    ColorUtils::Format fmt = m_ColorFormat;
    UINT32 width = m_Width;

    if (ColorUtils::LWPNGFMT_NONE ==
            ColorUtils::FormatToPNGFormat(m_ColorFormat))
    {
        //fall back to A8R8G8B8
        fmt = ColorUtils::A8R8G8B8;
        width = GetBytesPerPixel() * m_Width / 4;
    }
    pCrc->resize(ColorUtils::PixelElements(fmt), 0);
    Crc::Callwlate(&Temp[0], width, m_Height, fmt, m_Pitch,
                    1,  //1 CRC per pixel component
                    &((*pCrc)[0]));
    return rc;
}

//-----------------------------------------------------------------------------
//The returned CRC must match GetBlockedCRCOnHost()
RC Surface2D::GetBlockedCRCOnGpu(UINT32 *pCrc)
{
    RC rc;

    CHECK_RC(CheckSurfOnGPU(m_Split, m_Location));
    CHECK_RC(GpuUtility::GetBlockedCRCOnGpu(GetGpuDev(),
                GetMemHandle(),
                0,                  //offset
                GetAllocSize(),     //size
                GetLocation(),
                pCrc));
    return rc;
}

//-----------------------------------------------------------------------------
RC Surface2D::GetBlockedCRCOnGpuPerComponent(vector<UINT32> *pCrc)
{
    RC rc;

    CHECK_RC(CheckSurfOnGPU(m_Split, m_Location));
    CHECK_RC(GpuUtility::GetBlockedCRCOnGpuPerComponent(GetGpuDev(),
                GetMemHandle(),
                0,
                GetAllocSize(),
                GetLocation(),
                pCrc));
    return rc;
}

//-----------------------------------------------------------------------------
//This function assumes a input surface in "R8G8B8A8" format. It breaks it down
//into "Crc::s_NumCRCBlocks" blocks and computes CRC for each block
//and writes it to an output surface in "RU32" format where each
//UINT32 represents CRC of all 4 components of that block.
//It finally returns the CRC of output surface

//The result must match that of the GetBlockedCRCOnGpu() where CRC
//is computed by lwca kernel.
RC Surface2D::GetBlockedCRCOnHost(UINT32 subdev, UINT32 *pCrc)
{
    RC rc;
    GpuSubdevice* pSubdev = GetGpuSubdev(subdev);
    MASSERT(m_DefClientData.m_hMem);

    //the lwca kernel works 64 bits at a time, and for the CRCs computed
    //here to match against the kernel, the surface must be 8x
    if (GetAllocSize() % sizeof(UINT64))
    {
        Printf(Tee::PriHigh, "Error: Surfaces must be multiples of 8.\n");
        return RC::SOFTWARE_ERROR;
    }

    const UINT64 surfSizeInDoubles = GetAllocSize()/sizeof(UINT64);
    const UINT64 blockSizeInDoubles = surfSizeInDoubles/Crc::s_NumCRCBlocks;
    const UINT64 blockSizeInBytes = blockSizeInDoubles * sizeof(UINT64);
    if (blockSizeInBytes > (pSubdev->FramebufferBarSize()<<20))
    {
        Printf(Tee::PriHigh, "Error: Surface is too big to map.\n");
        return RC::SOFTWARE_ERROR;
    }

    vector <UINT32> crcOfChunks(Crc::s_NumCRCBlocks, 0);
    const UINT64 bufSizeBytes = blockSizeInBytes > 0 ? blockSizeInBytes : sizeof(UINT64);
    vector<UINT08> buf(bufSizeBytes);

    vector<UINT32> crcTable;
    Crc::GetCRCTable(&crcTable);

    // Compute CRC per block
    for(UINT32 i = 0; i < Crc::s_NumCRCBlocks; i++)
    {
        UINT32 crc = ~0x0;
        if (blockSizeInBytes)
        {
            CHECK_RC(CheckSubdeviceInstance(&subdev));
            CHECK_RC(SurfaceUtils::ReadSurface(*this, i*blockSizeInBytes, &buf[0], blockSizeInBytes, subdev));

            for (UINT32 j = 0 ; j < blockSizeInBytes; j++)
            {
                crc = crcTable[(crc >> 24) ^ buf[j]] ^ (crc << 8);
            }
        }

        // Replicate the kernel when num Chunks is not a multiple of Num threads launched
        // i.e read the last UINT64 for this index
        UINT64 index = blockSizeInDoubles * Crc::s_NumCRCBlocks + i;
        if (index < surfSizeInDoubles)
        {
            CHECK_RC(SurfaceUtils::ReadSurface(*this, index * sizeof(UINT64), &buf[0], sizeof(UINT64), subdev));
            for (UINT32 j = 0 ; j < sizeof(UINT64); j++)
            {
                crc = crcTable[(crc >> 24) ^ buf[j]] ^ (crc << 8);
            }
        }
        crcOfChunks[i] = ~crc;
    }

    //The return surface is treated as a single 32 bit component (RU32), Return its CRC
    ColorUtils::Format Fmt = ColorUtils::RU32;
    Crc::Callwlate(&crcOfChunks[0],      //Address
                static_cast<UINT32>(crcOfChunks.size()), //Width
                1,                       //Height
                Fmt,                     //Format
                static_cast<UINT32>(crcOfChunks.size() * ColorUtils::PixelBytes(Fmt)), //Pitch
                1,                       //Num Bins
                pCrc);                   //bin

    return rc;
}

//-----------------------------------------------------------------------------
//This function assumes a input surface in "R8G8B8A8" format. It breaks it down
//into "Crc::s_NumCRCBlocks" blocks and computes individual CRC for each component
//and writes it to an output surface in "RU32_GU32_BU32_AU32" format where each
//UINT32 represents CRC of a component in that block.
//It finally returns the CRC of output surface

//The result must match that of the GetBlockedCRCOnGpuPerComponent() where CRC
//is computed by lwca kernel.
RC Surface2D::GetBlockedCRCOnHostPerComponent(UINT32 subdev, vector<UINT32> *pCrc)
{
    RC rc;
    MASSERT(m_DefClientData.m_hMem);

    CHECK_RC(CheckSubdeviceInstance(&subdev));
    GpuSubdevice* pSubdev = GetGpuSubdev(subdev);

    //the lwca kernel works 64 bits at a time, and for the CRCs computed
    //here to match against the kernel, the surface must be 8x
    if (GetAllocSize() % sizeof(UINT64))
    {
        Printf(Tee::PriHigh, "Error: Surfaces must be multiples of 8.\n");
        return RC::SOFTWARE_ERROR;
    }
    const UINT64 surfSizeInDoubles = GetAllocSize()/sizeof(UINT64);
    const UINT64 blockSizeInDoubles = surfSizeInDoubles/Crc::s_NumCRCBlocks;
    const UINT64 blockSizeInBytes = blockSizeInDoubles * sizeof(UINT64);
    if (blockSizeInBytes > (pSubdev->FramebufferBarSize()<<20))
    {
        Printf(Tee::PriHigh, "Error: Surface is too big to map.\n");
        return RC::SOFTWARE_ERROR;
    }

    // This function assumes the input surface is laid out in R8G8B8A8 format.
    // It returns CRC for each of the 4 components.
    vector <UINT32> crcOfChunks(4 * Crc::s_NumCRCBlocks, 0);
    const UINT64 bufSizeBytes = blockSizeInBytes > 0 ? blockSizeInBytes : sizeof(UINT64);
    vector<UINT08> buf(bufSizeBytes);

    vector<UINT32> crcTable;
    Crc::GetCRCTable(&crcTable);

    for(UINT32 i = 0; i < Crc::s_NumCRCBlocks; i++)
    {
        UINT32 crc[] = {~0U, ~0U, ~0U, ~0U};

        if (blockSizeInBytes)
        {
            CHECK_RC(SurfaceUtils::ReadSurface(*this, i*blockSizeInBytes, &buf[0], blockSizeInBytes, subdev));

            for (UINT32 j = 0 ; j < blockSizeInBytes; j++)
            {
                crc[j % 4] = crcTable[(crc[j % 4] >> 24) ^ buf[j]] ^ (crc[j % 4] << 8);
            }
        }

        // Replicate the kernel when num Chunks is not a multiple of Num threads launched
        // i.e read the last UINT64 for this index
        UINT64 index = blockSizeInDoubles * Crc::s_NumCRCBlocks + i;
        if (index < surfSizeInDoubles)
        {
            CHECK_RC(SurfaceUtils::ReadSurface(*this, index * sizeof(UINT64), &buf[0], sizeof(UINT64), subdev));

            for (UINT32 j = 0 ; j < sizeof(UINT64); j++)
            {
                crc[j % 4] = crcTable[(crc[j % 4] >> 24) ^ buf[j]] ^ (crc[j % 4] << 8);
            }
        }

        crcOfChunks[4*i] = ~crc[0];
        crcOfChunks[4*i+1] = ~crc[1];
        crcOfChunks[4*i+2] = ~crc[2];
        crcOfChunks[4*i+3] = ~crc[3];
    }

    //The output surface holds a UINT32 CRC per component
    const ColorUtils::Format Fmt = ColorUtils::RU32_GU32_BU32_AU32;
    pCrc->resize(ColorUtils::PixelElements(Fmt));

    //Return the CRC of the output surface
    Crc::Callwlate(&crcOfChunks[0],     //Address
                Crc::s_NumCRCBlocks,    //Width
                1,                      //Height
                Fmt,                    //Format
                Crc::s_NumCRCBlocks * ColorUtils::PixelBytes(Fmt), //Pitch
                1,                      //Num Bins
                &((*pCrc)[0]));         //bin
    return rc;
}

//-----------------------------------------------------------------------------
RC Surface2D::WritePng(const char *FileName, UINT32 subdev)
{
    RC rc;
    UINT32 BytesPerPixel = GetBytesPerPixel();

    vector<UINT08> Temp;
    CHECK_RC(CreatePitchImage(&Temp, subdev));

    UINT32 pngWidth = m_Width;
    ColorUtils::Format pngFormat = m_ColorFormat;

    if (ColorUtils::LWPNGFMT_NONE ==
            ColorUtils::FormatToPNGFormat(m_ColorFormat))
    {
        // Oopsie, there's no PNG color format that matches.
        // Fall back to ARGB 32-bpp and lie about the surface width.
        // We could do this more cleanly inside the png.cpp file, but that
        // file uses evil setjmp/longjmp error handling, and is hard to change
        // without provoking frustrating gcc warning messages.
        pngFormat = ColorUtils::A8R8G8B8;
        pngWidth = GetBytesPerPixel() * m_Width / 4;
    }

    rc = ImageFile::WritePng(FileName, pngFormat, &Temp[0],
                             pngWidth, m_Height*m_Depth*m_ArraySize,
                             m_Width*BytesPerPixel,
                             false, false);
    return rc;
}

//-----------------------------------------------------------------------------
RC Surface2D::WriteText(const char *FileName, UINT32 subdev)
{
    RC rc;
    FILE *f;
    UINT32 BytesPerPixel = GetBytesPerPixel();
    UINT32 x, y;

    vector<UINT08> Temp;
    CHECK_RC(CreatePitchImage(&Temp, subdev));

    // Open the file
    CHECK_RC(Utility::OpenFile(FileName, &f, "wt"));

    // Write each pixel out as a line to the file
    UINT08 *pSrc = &Temp[0];
    for (UINT32 a = 0; a < m_ArraySize; a++)
    {
        for (UINT32 z = 0; z < m_Depth; z++ )
        {
            for (y = 0; y < m_Height; y++)
            {
                for (x = 0; x < m_Width; x++){
                    UINT64 PixelData = 0;
                    switch (BytesPerPixel)
                    {
                        case 1:
                            PixelData = *(UINT08 *)pSrc;
                            break;
                        case 2:
                            PixelData = *(UINT16 *)pSrc;
                            break;
                        case 4:
                            PixelData = *(UINT32 *)pSrc;
                            break;
                        case 8:
                            PixelData = *(UINT64 *)pSrc;
                            break;
                        default:
                            MASSERT(!"Invalid BytesPerPixel");
                    }
                    fprintf(f, "X = %d, Y = %d, PixelData = %llx\n", x, y, PixelData);
                    pSrc += BytesPerPixel;
                }
            }
        }
    }
    fclose(f);
    return rc;
}

//-----------------------------------------------------------------------------
RC Surface2D::ReadTga(const char *FileName, UINT32 subdev)
{
    RC rc;
    UINT32 ImageBytesPerPixel = GetBytesPerPixel();
    UINT32 FileBytesPerPixel;
    ColorUtils::Format MyColorFormat = m_ColorFormat;

    // for YCrCb we read in 4 bytes/pixel and colwert to 2 bytes/pixel
    if (IsYUVLegacy())
    {
        FileBytesPerPixel = 4;
        MyColorFormat = ColorUtils::A8R8G8B8;
    }
    else
    {
        FileBytesPerPixel = ImageBytesPerPixel;
    }

    // Should have been allocated
    MASSERT(m_DefClientData.m_hMem);

    UINT32 WholeHeight = m_Height * m_Depth * m_ArraySize;

    // Read the disk file into a pitch-format buffer
    UINT08 *Temp = new UINT08[m_Width*WholeHeight*FileBytesPerPixel];
    if (!Temp)
        return RC::CANNOT_ALLOCATE_MEMORY;
    rc = ImageFile::ReadTga(FileName, true, MyColorFormat, Temp,
                            m_Width, WholeHeight, m_Width*FileBytesPerPixel);
    if (rc != OK)
    {
        delete[] Temp;
        return rc;
    }

    if (IsYUVLegacy())
    {
        UINT08* ImageTemp = new UINT08[m_Width*WholeHeight*ImageBytesPerPixel];
        if (!ImageTemp) {
            delete[] Temp;
            return RC::CANNOT_ALLOCATE_MEMORY;
        }

        ColorUtils::ColwertYUV444ToYUV422(Temp, ImageTemp,
                                          m_Width*WholeHeight, m_ColorFormat);

        delete[] Temp;
        Temp = ImageTemp;
    }

    // Write out the image to the actual surface
    rc = LoadPitchImage(Temp, subdev);
    delete[] Temp;
    return rc;
}

//-----------------------------------------------------------------------------
RC Surface2D::ReadHdr(const char *FileName, UINT32 subdev)
{
    RC rc;
    UINT32 bytesPerPixel = GetBytesPerPixel();

    // Should have been allocated
    MASSERT(m_DefClientData.m_hMem);

    UINT32 wholeHeight = m_Height * m_Depth * m_ArraySize;
    UINT32 pitch = m_Width*bytesPerPixel;

    // Read the disk file into a pitch-format buffer
    vector<UINT08> localbuffer(m_Width*wholeHeight*bytesPerPixel);

    CHECK_RC(ImageFile::ReadHdr(FileName, &localbuffer,
                            m_Width, wholeHeight, pitch));

    // Write out the image to the actual surface
    CHECK_RC(LoadPitchImage(localbuffer.data(), subdev));

    return rc;
}

//-----------------------------------------------------------------------------
RC Surface2D::ReadPng(const char *FileName, UINT32 subdev)
{
    RC rc;
    UINT32 BytesPerPixel = GetBytesPerPixel();

    // Should have been allocated
    MASSERT(m_DefClientData.m_hMem);

    UINT32 WholeHeight = m_Height * m_Depth * m_ArraySize;

    // Read the disk file into a pitch-format buffer
    UINT08 *Temp = new UINT08[m_Width*WholeHeight*BytesPerPixel];
    if (!Temp)
        return RC::CANNOT_ALLOCATE_MEMORY;
    rc = ImageFile::ReadPng(FileName, true, m_ColorFormat, Temp,
                            m_Width, WholeHeight, m_Width*BytesPerPixel);
    if (rc != OK)
    {
        delete[] Temp;
        return rc;
    }

    // Write out the image to the actual surface
    rc = LoadPitchImage(Temp, subdev);
    delete[] Temp;
    return rc;
}

//-----------------------------------------------------------------------------
RC Surface2D::ReadRaw(const char *FileName, UINT32 subdev)
{
    RC rc;
    FILE *f;
    UINT32 BytesPerPixel = GetBytesPerPixel();

    // Read the disk file into a pitch-format buffer
    CHECK_RC(Utility::OpenFile(FileName, &f, "rb"));
    UINT32 PitchSize = m_Width*m_Height*m_Depth*m_ArraySize*BytesPerPixel;
    UINT08 *Temp = new UINT08[PitchSize];
    if (!Temp)
    {
        fclose(f);
        return RC::CANNOT_ALLOCATE_MEMORY;
    }
    if (PitchSize != fread(Temp, 1, PitchSize, f))
    {
        delete[] Temp;
        fclose(f);
        return RC::FILE_READ_ERROR;
    }
    fclose(f);

    // Write out the image to the actual surface
    rc = LoadPitchImage(Temp, subdev);
    delete[] Temp;
    return rc;
}

//-----------------------------------------------------------------------------
RC Surface2D::ReadSeurat(const char *FileName, UINT32 subdev)
{
    RC rc;
    FILE *f;
    FileHolder fp;
    UINT32 BytesPerPixel = GetBytesPerPixel();

    // open file for read
    CHECK_RC(fp.Open(FileName, "rt"));
    f = fp.GetFile();

    // Should have been allocated
    MASSERT(m_DefClientData.m_hMem);

    // only support A8R8G8B8 color format for now
    if (m_ColorFormat != ColorUtils::A8R8G8B8)
    {
        return RC::BAD_FORMAT;
    }

    UINT32 Min, Max, X, Y, R, G, B;
    UINT32 Counter = 0;
    char Delimiter;
    bool ErrorFlag;
    UINT32 TotalSize = m_Width*m_Height*m_Depth*m_ArraySize*BytesPerPixel;
    vector<UINT08> Temp(TotalSize);

    // Read in resurat file and abstract pixel data.
    // It will be colwerted to A8R8G8B8 color format internally.
    while (fscanf(f, "%c", &Delimiter) != EOF)
    {
        if (Delimiter == '@')
        {
            while (fscanf(f, "%c", &Delimiter) != EOF)
            {
                if (Delimiter == 'i') break;
            }

            ErrorFlag = (fscanf(f, "%d%d"  , &Min, &Max) != 2)
                     || (fscanf(f, "%d%d"  , &X, &Y    ) != 2)
                     || (fscanf(f, "%d%d%d", &R, &G, &B) != 3);

            if (ErrorFlag)
            {
                return RC::UNEXPECTED_RESULT;
            }

            ErrorFlag = (Max < R || R < Min)
                     || (Max < G || G < Min)
                     || (Max < B || B < Min);

            if (ErrorFlag)
            {
                return RC::UNEXPECTED_RESULT;
            }

            UINT32 ShiftBits = 0;
            bool IsShiftRight = true; // shift direction flag
            if (Max > 0xFF)
            {
                IsShiftRight = true;
                do
                {
                    Max >>= 1;
                    ShiftBits++;
                } while (Max > 0xFF);
            }
            else if (Max < 0xFF)
            {
                IsShiftRight = false;
                do
                {
                    Max <<= 1;
                    ShiftBits++;
                } while (Max < 0xFF);
            }

            // colwert to A8R8G8B8 format
            if (IsShiftRight)
            {
                if (Counter >= TotalSize)
                {
                    return RC::UNEXPECTED_RESULT;
                }
                Temp[Counter++] = B >> ShiftBits;
                Temp[Counter++] = G >> ShiftBits;
                Temp[Counter++] = R >> ShiftBits;
                Temp[Counter++] = 0xFF;
            }
            else
            {
                if (Counter >= TotalSize)
                {
                    return RC::UNEXPECTED_RESULT;
                }
                Temp[Counter++] = B << ShiftBits;
                Temp[Counter++] = G << ShiftBits;
                Temp[Counter++] = R << ShiftBits;
                Temp[Counter++] = 0xFF;
            }
        }
    }

    // Write out the image to the actual surface
    rc = LoadPitchImage(&Temp[0], subdev);
    return rc;
}

//-----------------------------------------------------------------------------
RC Surface2D::ReadTagsBinary(const char *FileName)
{
    RC rc;
    FILE *f;
    UINT32 BytesPerPixel = GetBytesPerPixel();

    // Read the disk file into a pitch-format buffer
    CHECK_RC(Utility::OpenFile(FileName, &f, "rb"));
    UINT32 PitchSize = m_Width*m_Height*m_Depth*m_ArraySize*BytesPerPixel;
    vector<UINT32> Temp(PitchSize/sizeof(UINT32));
    const size_t NumRead = fread(&Temp[0], 1, PitchSize, f);
    fclose(f);
    if (PitchSize != NumRead)
    {
        return RC::FILE_READ_ERROR;
    }

    // Write out the image to the actual surface
    return GetGpuSubdev(0)->SetCompressionMap(this, Temp);
}

//-----------------------------------------------------------------------------
RC Surface2D::ReadTagsTga(const char *FileName)
{
    // Read the disk file into a pitch-format buffer
    // The image should have 2 pixels horizontally for every gob, one per ROP tile
    vector<UINT32> TagBuf(2*m_GobWidth*m_GobHeight*m_GobDepth*m_ArraySize);
    RC rc;
    CHECK_RC(ImageFile::ReadTga(FileName, true, ColorUtils::A8R8G8B8, &TagBuf[0],
        2*m_GobWidth, m_GobHeight*m_GobDepth*m_ArraySize, 2*m_GobWidth*sizeof(UINT32)));
    return GetGpuSubdev(0)->SetCompressionMap(this, TagBuf);
}

//-----------------------------------------------------------------------------
void Surface2D::ClearMembers()
{
    m_Width = 0;
    m_Height = 0;
    m_Depth = 1;
    m_ArraySize = 1;
    m_BytesPerPixel = 0;
    m_ColorFormat = ColorUtils::LWFMT_NONE;
    m_LogBlockWidth = 0;
    m_LogBlockHeight = 4;
    m_LogBlockDepth = 0;
    m_Type = LWOS32_TYPE_IMAGE;
    m_Name = "";
    m_YCrCbType = ColorUtils::CCIR601;
    m_Location = Memory::Optimal;
    m_Protect = Memory::ReadWrite;
    m_ShaderProtect = Memory::ProtectDefault;
    m_AddressModel = Memory::OptimalAddress;
    m_DmaProtocol = Memory::Default;
    m_Layout = Pitch;
    m_VASpace = DefaultVASpace;
    m_Tiled = false;
    m_Compressed = false;
    m_CompressedFlag = LWOS32_ATTR_COMPR_REQUIRED;
    m_ComptagStart = 0;
    m_ComptagCovMin = 100;
    m_ComptagCovMax = 100;
    m_Comptags = 0;
    m_AAMode = AA_1;
    m_D3DSwizzled = false;
    m_Displayable = false;
    m_PteKind = -1;
    m_SplitPteKind = -1;
    m_PartStride = 0;
    m_PageSize = 0;
    m_PhysAddrPageSize = 0;
    m_DualPageSize = false;
    m_Alignment = 0;
    m_VirtAlignment = 0;
    m_MemAttrsInCtxDma = false;
    m_VaReverse = false;
    m_PaReverse = false;
    m_MaxCoalesce = 0;
    m_ComponentPacking = false;
    m_Scanout = true;
    m_ZLwllFlag = 0;
    m_WindowOffsetX = 0;
    m_WindowOffsetY = 0;
    m_ViewportOffsetX = 0;
    m_ViewportOffsetY = 0;
    m_ViewportOffsetExist = false;
    m_ZbcMode = ZbcDefault;
    m_ParentClass = 0;
    m_ParentClassAttr = 0;
    m_LoopBack = false;
    m_LoopBackPeerId = USE_DEFAULT_RM_MAPPING;
    m_IsSPAPeerAccess = false;
    m_Priv = false;
    m_GpuVirtAddrHint = 0;
    m_GpuVirtAddrHintMax = 0;
    m_GpuPhysAddrHint = 0;
    m_GpuPhysAddrHintMax = 0;
    m_ForceSizeAlloc = false;
    m_CreatedFromAllocSurface = false;
    m_MemoryMappingMode = MapDefault;
    m_Specialization = NoSpecialization;
    m_GhostSurface = false;
    m_bAllocInUpr = false;
    m_bVideoProtected = false;
    m_bAcrRegion1 = false;
    m_bAcrRegion2 = false;
    m_GpuSmmuMode = GpuSmmuDefault;
    m_InputRange = BYPASS;
    m_FbSpeed = FbSpeedDefault;
    m_InheritPteKind = InheritPhysical;

    m_Split = false;
    m_SplitLocation = Memory::Optimal;

    m_AllocWidth = m_AllocHeight = m_AllocDepth = 0;
    m_GobWidth = m_GobHeight = m_GobDepth = 0;
    m_BlockWidth = m_BlockHeight = m_BlockDepth = 0;
    m_Size = 0;
    m_Pitch = 0;
    m_ArrayPitch = 0;
    m_ArrayPitchAlignment = 0;
    m_HiddenAllocSize = 0;
    m_ExtraAllocSize = 0;
    m_Limit = -1;

    AllocData clearData;
    m_pLwRm = LwRmPtr(0).Get();
    m_PeerClientData.clear();
    m_DefClientData = clearData;

    for (UINT32 i = 0; i < MaxD; i++)
    {
        m_RemoteIsMapped[i] = false;
    }

    m_Address = NULL;
    m_CdeAddress = nullptr;
    m_PhysAddress = 0;
    m_MappedSubdev = 0xffffffff;
    m_MappedPart = ~0U;
    m_MappedOffset = 0;
    m_MappedSize = 0;
    m_MappedClient = 0;
    m_VirtMemAllocated = false;
    m_TegraVirtMemAllocated = false;
    m_CtxDmaAllocatedGpu = false;
    m_CtxDmaAllocatedIso = false;
    m_VirtMemMapped = false;
    m_TegraMemMapped = false;
    m_CtxDmaMappedGpu = false;
    m_TurboCipherEncryption = false;
    m_GpuCacheMode = GpuCacheDefault;
    m_P2PGpuCacheMode = GpuCacheDefault;
    m_SplitGpuCacheMode = GpuCacheDefault;
    m_VaVidHeapFlags = 0;
    m_PaVidHeapFlags = 0;
    m_VidHeapAttr = 0;
    m_VidHeapAttr2 = 0;
    m_VidHeapAlign = 0;
    m_VidHeapCoverage = 0;
    m_AllocSysMemFlags = 0;
    m_MapMemoryDmaFlags = 0;
    m_ZLwllRegion = -1;
    m_CtagOffset = 0;
    m_ActualPageSizeKB = ~0U;

    m_SplitHalfSize = 0;
    m_SplitVidHeapAttr = 0;
    m_SplitVidHeapAttr2 = 0;

    m_DmaBufferAlloc = false;
    m_KernelMapping = false;
    m_pChannelsBound = NULL;
    m_NumChannelsBound = 0;

    m_PhysContig = false;
    m_AlignHostPage = false;
    m_bUseVidHeapAlloc = false;

    m_bUseExtSysMemFlags = false;
    m_ExtSysMemAllocFlags = 0;
    m_ExtSysMemCtxDmaFlags = 0;

    m_bUsePeerMappingFlags = false;
    m_PeerMappingFlags = 0;

    m_pGpuDev = 0;
    m_IsViewOnly = false;

    m_SkedReflected = false;
    m_MappingObjHandle = 0;

    m_IsHostReflectedSurf = false;
    m_GpuManagedChName = "";
    m_GpuManagedChId = 0;

    m_MipLevels = 1;
    m_Border = 0;
    m_Dimensions = 1;
    m_pJsObj = nullptr;
    m_pJsCtx = nullptr;
    m_GpuVirtAddrHintUseVirtAlloc = true;
    m_IsSparse = false;
    m_TileWidthInGobs = 0;
    m_ExtraWidth = 0;
    m_ExtraHeight = 0;
    m_CpuMapOffset = 0;
    m_NeedsPeerMapping = false;
    m_FLAPeerMapping = false;
    m_FLAPageSizeKb = 0;

    m_bCdeAlloc  = false;
    m_CdeAllocSize = 0;

    m_ExternalPhysMem = 0;
    m_IsHtex = false;

    m_RouteDispRM = false;
    m_bEgmOn = false;

    m_AmodelRestrictToFb = true;
}

//-----------------------------------------------------------------------------
RC Surface2D::CreatePitchImage(UINT08 *pBuf, size_t bufSize, UINT32 subdeviceInstance)
{
    RC rc;
    MASSERT(pBuf);
    MASSERT(m_DefClientData.m_hMem); // Should have been allocated

    UINT32 BytesPerPixel = GetBytesPerPixel();
    if (bufSize < m_Width*m_Height*m_Depth*m_ArraySize*BytesPerPixel)
    {
        Printf(Tee::PriHigh, "Copy to buffer failed: Expecting a larger buffer size.\n");
        return RC::SOFTWARE_ERROR;
    }

    // First, copy image to temporary native-format system memory surface,
    // using an efficient memcopy
    vector<UINT08> Temp1(static_cast<size_t>(GetSize()));
    CHECK_RC(CheckSubdeviceInstance(&subdeviceInstance));
    CHECK_RC(SurfaceUtils::ReadSurface(*this, 0, &Temp1[0], Temp1.size(), subdeviceInstance));

    // Colwert to pitch format and throw away the native-format image
    for (UINT32 a = 0; a < m_ArraySize; a++)
    {
        for (UINT32 z = 0; z < m_Depth; z++)
        {
            for (UINT32 y = 0; y < m_Height; y++)
            {
                for (UINT32 x = 0; x < m_Width; x++)
                {
                    size_t Offset = UNSIGNED_CAST(size_t, GetPixelOffset(x, y, z, a));
                    const UINT08 *pSrc = &Temp1[Offset];
                    memcpy(pBuf, pSrc, BytesPerPixel);
                    pBuf += BytesPerPixel;
                }
            }
        }
    }
    return OK;
}

//-----------------------------------------------------------------------------
RC Surface2D::CreatePitchImage(vector<UINT08> *pBuf, UINT32 subdeviceInstance)
{
    RC rc;
    pBuf->resize(m_Width*m_Height*m_Depth*m_ArraySize*GetBytesPerPixel());
    CHECK_RC(CreatePitchImage(&((*pBuf)[0]), pBuf->size(), subdeviceInstance));
    return rc;
}

//-----------------------------------------------------------------------------
RC Surface2D::CreatePitchSubImage(UINT32 x, UINT32 y, UINT32 Width, UINT32 Height,
                                  vector<UINT08> *pBuf, UINT32 subdeviceInstance)
{
    UINT32 BytesPerPixel = GetBytesPerPixel();
    UINT32 PixelX, PixelY;
    RC rc;

    // Should have been allocated
    MASSERT(m_DefClientData.m_hMem);

    // Rectangle had better lie within the surface
    if (x+Width > m_Width)
        return RC::BAD_PARAMETER;
    if (y+Height > m_Height)
        return RC::BAD_PARAMETER;

    const Surface2D::MappingSaver SavedMapping(*this);
    if (m_Address)
    {
        Unmap();
    }

    // Allocate and read back into a pitch-format buffer
    CHECK_RC(CheckSubdeviceInstance(&subdeviceInstance));
    CHECK_RC(MapRect(x, y, Width, Height, subdeviceInstance));
    vector<UINT08> Temp(Width*Height*BytesPerPixel);
    UINT08 *pDst = &Temp[0];
    for (PixelY = y; PixelY < y+Height; PixelY++)
    {
        for (PixelX = x; PixelX < x+Width; PixelX++)
        {
            UINT64 Offset = GetPixelOffset(PixelX, PixelY);
            CHECK_RC(MapOffset(Offset, &Offset));
            const UINT08 *pSrc = (const UINT08 *)m_Address + Offset;
            Platform::VirtualRd(pSrc, pDst, BytesPerPixel);
            pDst += BytesPerPixel;
        }
    }

    // Return the image
    pBuf->swap(Temp);
    return OK;
}

//-----------------------------------------------------------------------------
RC Surface2D::LoadPitchImage(UINT08 *Buf, UINT32 subdeviceInstance)
{
    UINT32 BytesPerPixel = GetBytesPerPixel();
    UINT32 x, y;
    RC rc;
    CHECK_RC(CheckSubdeviceInstance(&subdeviceInstance));

    // Colwert to native format
    if ((sizeof(size_t) == 4) && (GetSize() > 0xFFFFFFFF))
        return RC::DATA_TOO_LARGE;
    vector<UINT08> Temp((size_t)GetSize());
    const UINT08 *pSrc = Buf;
    for (y = 0; y < m_Height; y++)
    {
        for (x = 0; x < m_Width; x++)
        {
            size_t Offset = UNSIGNED_CAST(size_t, GetPixelOffset(x, y));
            memcpy(&Temp[Offset], pSrc, BytesPerPixel);
            pSrc += BytesPerPixel;
        }
    }

    // Copy out to actual surface, using an efficient memcopy
    CHECK_RC(SurfaceUtils::WriteSurface(*this, 0, &Temp[0], Temp.size(), subdeviceInstance));
    return OK;
}

bool Surface2D::IsYUVLegacy() const
{
    return ((m_ColorFormat == ColorUtils::VE8YO8UE8YE8) ||
            (m_ColorFormat == ColorUtils::YO8VE8YE8UE8));
}

bool Surface2D::IsYUV() const
{
    return ((m_ColorFormat == ColorUtils::Y8_U8__Y8_V8_N422) ||
            (m_ColorFormat == ColorUtils::U8_Y8__V8_Y8_N422) ||
            (m_PlanesColorFormat != ColorUtils::LWFMT_NONE &&
             ((m_ColorFormat == ColorUtils::Y8) ||
              (m_ColorFormat == ColorUtils::U8V8) ||
              (m_ColorFormat == ColorUtils::R16) ||
              (m_ColorFormat == ColorUtils::R16_G16) ||
              (m_ColorFormat == ColorUtils::U8) ||
              (m_ColorFormat == ColorUtils::V8))));
}

bool Surface2D::CheckYCrCbFillParams(UINT32 Width, UINT32 ChunkWidth) const
{
    // if ChunkWidth is 1, then we want the total width to be even
    // otherwise, it is only ok if the ChunkWidth is even

    if (ChunkWidth == 1) {
        return !(Width % 2);
    } else {
        return !(ChunkWidth % 2);
    }
}

UINT32 Surface2D::GetCompTags() const
{
#ifdef LW_VERIF_FEATURES
    return GetPhysParams(true).comprFormat;
#else
    return 0;
#endif
}

#ifdef LW_VERIF_FEATURES
LW0041_CTRL_GET_SURFACE_PHYS_ATTR_PARAMS Surface2D::GetPhysParams(bool start) const
{
    MASSERT(m_Compressed);

    // Should have been allocated
    MASSERT(m_DefClientData.m_hMem);

    // Must be block linear vidmem or none of the callwlations here work
    MASSERT(m_Layout == BlockLinear);
    MASSERT(m_Location == Memory::Fb);
    MASSERT(m_PartStride == 256 || m_PartStride == 1024);

    LW0041_CTRL_GET_SURFACE_PHYS_ATTR_PARAMS params = {0};
    params.memOffset = 0;
    bool tags_found = false;

    for (RC rc = m_pLwRm->Control(m_DefClientData.m_hMem,
                                  LW0041_CTRL_CMD_GET_SURFACE_PHYS_ATTR,
                                  &params, sizeof(params));
         /* Blank loop termination case  */ ;
         rc = m_pLwRm->Control(m_DefClientData.m_hMem,
                               LW0041_CTRL_CMD_GET_SURFACE_PHYS_ATTR,
                               &params, sizeof(params)),
         params.memOffset += params.contigSegmentSize)
    {
        if (rc != OK)
        {
            Printf(Tee::PriHigh, "LW0041_CTRL_CMD_GET_SURFACE_PHYS_ATTR failed\n");
            MASSERT(!"Generic assertion failure<Refer to previous message>.");
        }

        if (DRF_VAL(OS32, _ALLOC_COMPR, _COVG_BITS, params.comprFormat) !=
                LWOS32_ALLOC_COMPR_COVG_BITS_DEFAULT)
        {
            if (start)
                break;
            else
                tags_found = true;
        }

        if (params.memOffset + params.contigSegmentSize > m_Size)
        {
            if (start)
            {
                Printf(Tee::PriHigh, "Can not find comp tags\n");
                MASSERT(!"Generic assertion failure<Refer to previous message>.");
            }
            else
            {
                if (!tags_found)
                {
                    Printf(Tee::PriHigh, "Can not find comp tags\n");
                    MASSERT(!"Generic assertion failure<Refer to previous message>.");
                }
                else
                {
                    return params;
                }
            }
        }
    }

    return params;
}
#endif

UINT32 Surface2D::GetTagsOffsetPhys() const
{
#ifdef LW_VERIF_FEATURES
    return UINT32(GetPhysParams(true).memOffset);  // Discards hi32, ick.
#else
    return 0;
#endif
}

UINT32 Surface2D::GetTagsMin() const
{
#ifdef LW_VERIF_FEATURES
    return GetPhysParams(true).comprOffset;
#else
    return 0;
#endif
}

UINT32 Surface2D::GetTagsMax() const
{
#ifdef LW_VERIF_FEATURES
    return GetPhysParams(false).comprOffset;
#else
    return 0;
#endif
}

#define UINT32_PROP_FROM_JS_OBJ(pname) \
    if (OK == pJs->GetProperty(pSurfObj, #pname, &tmpUINT32)) \
        Set##pname(tmpUINT32);

#define BOOL_PROP_FROM_JS_OBJ(pname) \
    if (OK == pJs->GetProperty(pSurfObj, #pname, &tmpbool)) \
        Set##pname(tmpbool);

#define ENUM_PROP_FROM_JS_OBJ(pname,ename,imin,imax) \
    if (OK == pJs->GetProperty(pSurfObj, #pname, &tmpUINT32)) \
    { \
        if ((static_cast<int>(tmpUINT32) >= static_cast<int>(imin)) && (tmpUINT32 <= (imax))) \
            Set##pname((ename)(tmpUINT32)); \
        else \
        { \
            Printf(Tee::PriHigh, "%s value %d is outside legal range %d..%d\n", \
                    #pname, tmpUINT32, (imin), (imax)); \
            return RC::ILWALID_OBJECT_PROPERTY; \
        } \
    } \

//-----------------------------------------------------------------------------
//! Init this Surface2D object from JS properties.
RC Surface2D::InitFromJs (JSObject * testObj, const char * surfName)
{
    MASSERT(testObj);
    MASSERT(surfName);

    RC rc;
    JavaScriptPtr pJs;
    JSObject * pSurfObj;
    UINT32 tmpUINT32;
    bool tmpbool;

    rc = pJs->GetProperty(testObj, surfName, &pSurfObj);
    if (RC::ILWALID_OBJECT_PROPERTY == rc)
        return OK;  // No properties for this surface.
    else if (OK != rc)
        return rc;  // Unexpected error.

    UINT32_PROP_FROM_JS_OBJ (Width)
    UINT32_PROP_FROM_JS_OBJ (Height)
    UINT32_PROP_FROM_JS_OBJ (Depth)
    UINT32_PROP_FROM_JS_OBJ (ArraySize)
    ENUM_PROP_FROM_JS_OBJ   (ColorFormat, ColorUtils::Format, ColorUtils::R5G6B5, ColorUtils::Format_NUM_FORMATS-1)
    UINT32_PROP_FROM_JS_OBJ (LogBlockWidth)
    UINT32_PROP_FROM_JS_OBJ (LogBlockHeight)
    UINT32_PROP_FROM_JS_OBJ (LogBlockDepth)
    ENUM_PROP_FROM_JS_OBJ   (Type, UINT32, LWOS32_TYPE_IMAGE, LWOS32_NUM_MEM_TYPES-1)
    ENUM_PROP_FROM_JS_OBJ   (Location, Memory::Location, Memory::Fb, Memory::Optimal)
    ENUM_PROP_FROM_JS_OBJ   (Protect, Memory::Protect, Memory::Readable, Memory::ReadWrite)
    ENUM_PROP_FROM_JS_OBJ   (ShaderProtect, Memory::Protect, Memory::ProtectDefault, Memory::ReadWrite)
    ENUM_PROP_FROM_JS_OBJ   (AddressModel, Memory::AddressModel, Memory::Segmentation, Memory::OptimalAddress)
    ENUM_PROP_FROM_JS_OBJ   (Layout, Layout, Pitch, BlockLinear)
    BOOL_PROP_FROM_JS_OBJ   (Tiled)
    BOOL_PROP_FROM_JS_OBJ   (Compressed)
    UINT32_PROP_FROM_JS_OBJ (CompressedFlag)
    UINT32_PROP_FROM_JS_OBJ (ComptagStart)
    UINT32_PROP_FROM_JS_OBJ (ComptagCovMin)
    UINT32_PROP_FROM_JS_OBJ (ComptagCovMax)
    UINT32_PROP_FROM_JS_OBJ (AASamples)
    BOOL_PROP_FROM_JS_OBJ   (Displayable)
    UINT32_PROP_FROM_JS_OBJ (PteKind)
    UINT32_PROP_FROM_JS_OBJ (PartStride)
    BOOL_PROP_FROM_JS_OBJ   (VideoProtected)
    BOOL_PROP_FROM_JS_OBJ   (AcrRegion1)
    BOOL_PROP_FROM_JS_OBJ   (AcrRegion2)
    UINT32_PROP_FROM_JS_OBJ (PageSize)
    return OK;
}
//-----------------------------------------------------------------------------
void Surface2D::SaveJSCtxObj (JSContext *ctx, JSObject *obj)
{
    m_pJsCtx = ctx;
    m_pJsObj = obj;
}
void Surface2D::GetJSCtxObj (JSContext **ctx, JSObject **obj)
{
    *ctx = m_pJsCtx;
    *obj = m_pJsObj;
}
//-----------------------------------------------------------------------------
//! Print this object's properties to the log file.
void Surface2D::Print (int teePriority, const char * linePrefix)
{
    Tee::Priority pri = Tee::Priority(teePriority);

    const char * strBool[] =
    {
        "false",
        "true"
    };
    const char * strType[] =
    {
        "IMAGE",
        "DEPTH",
        "TEXTURE",
        "VIDEO",
        "FONT",
        "CURSOR",
        "DMA",
        "INSTANCE",
        "PRIMARY",
        "ZLWLL",
        "UNUSED",
        "SHADER_PROGRAM",
        "OWNER_RM",
        "RESERVED"
    };
    const char * myStrType = "??";
    if (GetType() < sizeof(strType)/sizeof(char*))
        myStrType = strType[GetType()];

    Printf(pri, "%sWidth          = %d\n", linePrefix, GetWidth());
    Printf(pri, "%sHeight         = %d\n", linePrefix, GetHeight());
    Printf(pri, "%sDepth          = %d\n", linePrefix, GetDepth());
    Printf(pri, "%sArraySize      = %d\n", linePrefix, GetArraySize());
    Printf(pri, "%sColorFormat    = %s\n", linePrefix,
           ColorUtils::FormatToString(GetColorFormat()).c_str());
    Printf(pri, "%sLogBlockWidth  = %d\n", linePrefix, GetLogBlockWidth());
    Printf(pri, "%sLogBlockHeight = %d\n", linePrefix, GetLogBlockHeight());
    Printf(pri, "%sLogBlockDepth  = %d\n", linePrefix, GetLogBlockDepth());
    Printf(pri, "%sType           = %d (LWOS32_TYPE_%s)\n",
           linePrefix, GetType(), myStrType);
    Printf(pri, "%sLocation       = %s\n", linePrefix,
           Memory::GetMemoryLocationName(GetLocation()));
    Printf(pri, "%sProtect        = %d%s%s%s\n", linePrefix, GetProtect(),
           (GetProtect() & Memory::Readable) ? " RD" : "",
           (GetProtect() & Memory::Writeable) ? " WR" : "",
           (GetProtect() & Memory::Exelwtable) ? " EX" : "");
    Printf(pri, "%sShaderProtect  = %d%s%s%s%s\n", linePrefix, GetShaderProtect(),
           (GetShaderProtect() & Memory::Readable) ? " RD" : "",
           (GetShaderProtect() & Memory::Writeable) ? " WR" : "",
           (GetShaderProtect() & Memory::Exelwtable) ? " EX" : "",
           (GetShaderProtect() & (Memory::ReadWrite | Memory::Exelwtable)) ? "" : " DF");
    Printf(pri, "%sAddressModel   = %s\n", linePrefix,
           Memory::GetMemoryAddressModelName(GetAddressModel()));
    Printf(pri, "%sPageSize       = %dKB\n", linePrefix, GetPageSize());
    Printf(pri, "%sLayout         = %s\n", linePrefix, GetLayoutStr(GetLayout()));
    Printf(pri, "%sTiled          = %s\n", linePrefix, strBool[GetTiled()]);
    Printf(pri, "%sCompressed     = %s\n", linePrefix, strBool[GetCompressed()]);
    // no getter Printf(pri, "%sCompressedFlag = 0x%x\n", linePrefix, GetCompressedFlag());
    Printf(pri, "%sComptagStart   = 0x%x\n", linePrefix, GetComptagStart());
    Printf(pri, "%sComptagCovMin  = 0x%x\n", linePrefix, GetComptagCovMin());
    Printf(pri, "%sComptagCovMax  = 0x%x\n", linePrefix, GetComptagCovMax());
    Printf(pri, "%sAAMode         = %u\n", linePrefix, GetAAMode());
    Printf(pri, "%sAASamples      = %d\n", linePrefix, GetAASamples());
    Printf(pri, "%sDisplayable    = %s\n", linePrefix, strBool[GetDisplayable()]);
    Printf(pri, "%sPteKind        = 0x%x\n", linePrefix, GetPteKind());
    Printf(pri, "%sPartStride     = %d\n", linePrefix, GetPartStride());
    Printf(pri, "%sVideoProtected = %s\n", linePrefix, strBool[GetVideoProtected()]);
    Printf(pri, "%sCdeAlloc       = %s\n", linePrefix, strBool[GetCdeAlloc()]);
    Printf(pri, "%sAcrRegion1\t= %s\n", linePrefix, strBool[GetAcrRegion1()]);
    Printf(pri, "%sAcrRegion2\t= %s\n", linePrefix, strBool[GetAcrRegion2()]);
    Printf(pri, "%sPeerMappings\n", linePrefix);
    PrintAllocData(pri, linePrefix);
}

void Surface2D::PrintAllocData(Tee::Priority pri, const char *linePrefix)
{
    map<LwRm*, AllocData>::iterator pData = m_PeerClientData.begin();
    while(pData != m_PeerClientData.end())
    {
        pData->second.Print(pri,linePrefix);
        pData++;
    }
    // now print the default client data
    m_DefClientData.Print(pri,linePrefix);

}

//-----------------------------------------------------------------------------
const char * Surface2D::GetLayoutStr(Surface2D::Layout layout) const
{
    switch (layout)
    {
        case Pitch:         return "Pitch";
        case Swizzled:      return "Swizzled";
        case BlockLinear:   return "BlockLinear";
        case Tiled:         return "Tiled";
        default:
            return "unknown Surface2d::Layout";
    }
}

//-----------------------------------------------------------------------------
//! \brief Return true if already bound to this channel.
//!
//! If we aren't, add this channel-handle to the list and return false.
bool Surface2D::RememberBoundChannel(UINT32 hCh)
{
    for (UINT32 i = 0; i < m_NumChannelsBound; i++)
    {
        if (m_pChannelsBound[i] == hCh)
        {
            //Printf(Tee::PriDebug, "Surface2D: channel %x already bound.\n", hCh);
            return true;
        }
    }

    m_NumChannelsBound++;
    UINT32 * p = new UINT32[m_NumChannelsBound];
    if (m_pChannelsBound)
    {
        memcpy(p, m_pChannelsBound, (m_NumChannelsBound-1)*sizeof(UINT32));
        delete [] m_pChannelsBound;
    }
    m_pChannelsBound = p;
    p=NULL;
    m_pChannelsBound[m_NumChannelsBound-1] = hCh;
    return false;
}

//-----------------------------------------------------------------------------
//! \brief Bind the Gpu ctxdma handle to the given channel.
RC Surface2D::BindGpuChannel(UINT32 hCh, LwRm *pLwRm)
{
    if (!m_CtxDmaAllocatedGpu)
    {
        // We're using a shared ctxdma handle, or else we haven't
        // yet run ::Alloc.  Don't bind.
        // We assume that the shared ctxdma handles have already been bound.
        return OK;
    }

    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    if (!pClientData)
    {
        MASSERT(!"Could not retrieve client data.");
        return RC::SOFTWARE_ERROR;
    }

    if (RememberBoundChannel(hCh))
        return OK;

#if defined(TEGRA_MODS)
    if (m_RouteDispRM)
    {
        return pLwRm->BindDispContextDma(hCh, pClientData->m_hCtxDmaGpu);
    }
    else
#endif
    {
        return pLwRm->BindContextDma(hCh, pClientData->m_hCtxDmaGpu);
    }
}

//-----------------------------------------------------------------------------
//! \brief Bind the Iso ctxdma handle to the given channel.
RC Surface2D::BindIsoChannel(UINT32 hCh, LwRm *pLwRm)
{
    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    if (!pClientData)
    {
        MASSERT(!"Could not retrieve client data.");
        return RC::SOFTWARE_ERROR;
    }

    if (m_Displayable &&
        (m_CtxDmaAllocatedIso ||
         (m_CtxDmaAllocatedGpu &&
          pClientData->m_hCtxDmaIso == pClientData->m_hCtxDmaGpu)))
    {
        // We have a non-shared handle for use with ISO.
        if (RememberBoundChannel(hCh))
            return OK;
#if defined(TEGRA_MODS)
        if (m_RouteDispRM)
        {
            return pLwRm->BindDispContextDma(hCh, pClientData->m_hCtxDmaIso);
        }
        else
#endif
        {
            return pLwRm->BindContextDma(hCh, pClientData->m_hCtxDmaIso);
        }
    }
    else
    {
        // We're using a shared ctxdma handle, or else we haven't
        // yet run ::Alloc. Don't bind.
        // We assume that the shared ctxdma handles have already been bound.
        // return OK;

        // After a month (11/19/06?) lets activate the return OK above

        if (!m_Displayable)
        {
            Printf(Tee::PriHigh, "Surface2D::BindIsoChannel called for not Displayable surface!!! Please correct the test!\n");
            Printf(Tee::PriHigh, "   m_Displayable = %s\n", m_Displayable ? "true" : "false");
            Printf(Tee::PriHigh, "   m_CtxDmaAllocatedIso = %s\n", m_CtxDmaAllocatedIso ? "true" : "false");
            Printf(Tee::PriHigh, "   m_CtxDmaAllocatedGpu = %s\n", m_CtxDmaAllocatedGpu ? "true" : "false");
            Printf(Tee::PriHigh, "   m_hCtxDmaIso = 0x%08x\n", pClientData->m_hCtxDmaIso);
            Printf(Tee::PriHigh, "   m_hCtxDmaGpu = 0x%08x\n", pClientData->m_hCtxDmaGpu);
        }

        // Display channels now pre-bind the shared ctxdmas for fb, so
        // the below can be removed when "return OK" will take place

        // Old comment:
        // BARF -- the display channels don't pre-bind the shared ctxdmas.
        // It is tricky to fix this without breaking arch display tests.
        // For now, bind unconditionally and ignore any error that comes out of RM.

        if (RememberBoundChannel(hCh))
            return OK;

        RC rc = pLwRm->BindContextDma(hCh, pClientData->m_hCtxDmaIso);

        if (OK != rc)
        {
            Printf(Tee::PriLow,
                    "Surface2D::BindIsoChannel ignoring RM error %d.\n",
                    UINT32(rc));
        }
        return OK;
    }
}

//-----------------------------------------------------------------------------
//! \brief Duplicate this surface onto a different RM Client.
RC Surface2D::DuplicateSurface(LwRm *pLwRm)
{
    if (!m_DefClientData.m_hMem)
    {
        Printf(Tee::PriHigh,
                "Surface2D::DuplicateSurface : Surface must be allocated to "
                "duplicate.\n");
        return RC::SOFTWARE_ERROR;
    }

    // If the data already is present for this client then nothing to do
    if (GetClientData(&pLwRm, __FUNCTION__))
        return OK;

    RC rc;
    AllocData clearData;
    m_PeerClientData[pLwRm] = clearData;
    AllocData *pClientData = &m_PeerClientData[pLwRm];

    CHECK_RC(pLwRm->DuplicateMemoryHandle(m_DefClientData.m_hMem,
                                          &pClientData->m_hMem,
                                          GetGpuDev()));

    // Eventually, we would like to use VidHeapControl for all allocs
    if (m_bUseVidHeapAlloc)
    {
        pClientData->m_VidMemOffset = m_DefClientData.m_VidMemOffset;

        // Allocate storage for split surface's second half
        if (m_Split)
        {
            CHECK_RC(pLwRm->DuplicateMemoryHandle(m_DefClientData.m_hSplitMem,
                                                  &pClientData->m_hSplitMem,
                                                  GetGpuDev()));
            pClientData->m_SplitVidMemOffset = m_DefClientData.m_SplitVidMemOffset;
        }
    }

    if (HasVirtual())
    {
        CHECK_RC(MapPhysMemory(pLwRm));
    }

    GpuDevMgr *pDevMgr = ((GpuDevMgr*)DevMgrMgr::d_GraphDevMgr);
    GpuDevice *pGpuDevice;

    for (pGpuDevice = pDevMgr->GetFirstGpuDevice(); (pGpuDevice != NULL);
         pGpuDevice = pDevMgr->GetNextGpuDevice(pGpuDevice))
    {
        if (IsPeerMapped(pGpuDevice))
        {
            if (pGpuDevice == GetGpuDev())
            {
                if (pGpuDevice->GetNumSubdevices() > 1)
                {
                    CHECK_RC(MapPeerInternal(pLwRm));
                }
                else
                {
                    CHECK_RC(MapLoopbackInternal(pLwRm,USE_DEFAULT_RM_MAPPING));
                }
            }
            else
            {
                CHECK_RC(MapPeerInternal(pGpuDevice, pLwRm));
            }
        }
        else if (IsMapShared(pGpuDevice))
        {
            CHECK_RC(MapSharedInternal(pGpuDevice, pLwRm));
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Return the bound GPU device, asserting that one is indeed bound
GpuDevice *Surface2D::GetGpuDev() const
{
    MASSERT(m_pGpuDev);
    return m_pGpuDev;
}

//-----------------------------------------------------------------------------
//! \brief Return subdevice of the bound GPU device
GpuSubdevice* Surface2D::GetGpuSubdev(UINT32 subdev) const
{
    GpuSubdevice* pSubDev = GetGpuDev()->GetSubdevice(subdev);
    MASSERT(pSubDev);
    return pSubDev;
}

//-----------------------------------------------------------------------------
//! \brief Peer map the surface to all subdevices within the allocated
//!        GpuDevice (SLI peer mapping).  Only valid for surfaces allocated
//!        in framebuffer memory in paging mode and only for GpuDevices with
//!        more than one subdevice.  The peer mapping that is created is not
//!        valid for use on the subdevice where the memory resides (i.e. the
//!        mapping may not be used in a loopback fashion).
//!
//! \return OK if creating the remote mapping succeeds, not OK otherwise
//!
//! \sa MapPeer(GpuDevice *)
//! \sa MapLoopback()
//! \sa MapShared(GpuDevice *)
RC Surface2D::MapPeer()
{
    RC rc;

    if (IsPeerMapped())
        return OK;

    if (GetGpuDev()->GetNumSubdevices() < 2)
    {
        Printf(Tee::PriHigh, "Surface2D::MapPeer() requires SLI!\n");
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(IsSuitableForRemoteMapping(true));

    CHECK_RC(MapPeerInternal(m_pLwRm));

    m_RemoteIsMapped[GetGpuDev()->GetDeviceInst()] = true;

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Create a peer mapping from all subdevices of the specified remote
//!        GpuDevice to all subdevices on which the surface has been
//!        allocated (hybrid peer mapping). Only valid for surfaces allocated
//!        in framebuffer memory in paging mode and only if the remote
//!        GpuDevice is different that the GpuDevice where the surface has
//!        been allocated.  To create a loopback mapping MapLoopback() should
//!        be used instead
//!
//! \param pRemoteDev : Remote device to peer map to the surface
//!
//! \return OK if creating the peer mapping succeeds, not OK otherwise
//!
//! \sa MapPeer()
//! \sa MapLoopback()
//! \sa MapShared(GpuDevice *)
RC Surface2D::MapPeer(GpuDevice *pRemoteDev)
{
    RC     rc = OK;

    if (IsPeerMapped(pRemoteDev))
        return OK;

    if (pRemoteDev == GetGpuDev())
    {
        MASSERT(!"Use to MapLoopback() to loopback peer map a device\n");
        return RC::SOFTWARE_ERROR;
    }

    if (m_ParentClass)
    {
        Printf(Tee::PriHigh,
               "Peer buffers with custom parent class are not supported!\n");
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(IsSuitableForRemoteMapping(true));

    CHECK_RC(MapPeerInternal(pRemoteDev, m_pLwRm));

    m_RemoteIsMapped[pRemoteDev->GetDeviceInst()] = true;

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Create a loopback mapping through the peer interface for the
//!        surface. Only valid for surfaces allocated in framebuffer memory
//!        in paging mode and only if the allocated GpuDevice has a single
//!        subdevice.
//!
//! \return OK if creating the loopback mapping succeeds, not OK otherwise
//!
//! \sa MapPeer()
//! \sa MapPeer(GpuDevice *)
//! \sa MapShared(GpuDevice *)
RC Surface2D::MapLoopback()
{
    return MapLoopback(USE_DEFAULT_RM_MAPPING);
}

RC Surface2D::MapLoopback(UINT32 loopbackPeerId)
{
    RC rc = OK;
    GpuDevice * pDevice = GetGpuDev();
    UINT32      DeviceInst = GetGpuDev()->GetDeviceInst();
    RemoteMapping mapIndex(0, DeviceInst, 0, loopbackPeerId);

    if (IsPeerMapped() && m_DefClientData.m_CtxDmaOffsetGpuRemote.count(mapIndex))
        return OK;

    if (pDevice->GetNumSubdevices() > 1)
    {
        Printf(Tee::PriHigh,
               "Surface2D::MapLoopback() cannot loopback an SLI device!\n");
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(IsSuitableForRemoteMapping(true));

    if (Platform::GetSimulationMode() == Platform::Hardware)
    {
        // For now only print a warning and allow failures to occur if loopback
        // is attempted on silicon
        Printf(Tee::PriLow,
               "Warning : Surface2D::MapLoopback() not supported on silicon!\n");
    }

    CHECK_RC(MapLoopbackInternal(m_pLwRm,loopbackPeerId));

    Printf(Tee::PriDebug, "Loopback access to gpu device %d's surf at 0x%llx\n",
        pDevice->GetDeviceInst(),
        m_DefClientData.m_CtxDmaOffsetGpuRemote[mapIndex]);

    m_RemoteIsMapped[DeviceInst] = true;

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Create a shared mapping for a surface (not through the peer
//!        interface) allowing GpuDevices other than the allocated GpuDevice to
//!        access the memory. Only valid for surfaces allocated in system
//!        memory in paging mode and only if the shared GpuDevice is different
//!        than the GpuDevice where the surface has been allocated.
//!
//! \param pSharedDev : Shared device to map to the surface
//!
//! \return OK if creating the shared mapping succeeds, not OK otherwise
//!
//! \sa MapPeer()
//! \sa MapPeer(GpuDevice *)
//! \sa MapLoopback()
RC Surface2D::MapShared(GpuDevice *pSharedDev)
{
    RC rc = OK;
    UINT32      DeviceInst = pSharedDev->GetDeviceInst();

    if (IsMapShared(pSharedDev))
        return OK;

    CHECK_RC(IsSuitableForRemoteMapping(false));

    if (pSharedDev == GetGpuDev())
    {
        MASSERT(!"Cannot MapShared() to the allocated device\n");
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(MapSharedInternal(pSharedDev, m_pLwRm));

    RemoteMapping mapIndex(0, DeviceInst, 0, USE_DEFAULT_RM_MAPPING);
    Printf(Tee::PriDebug, "Shard access to gpu device %d's surf at 0x%llx\n",
        GetGpuDev()->GetDeviceInst(),
        m_DefClientData.m_CtxDmaOffsetGpuRemote[mapIndex]);

    m_RemoteIsMapped[DeviceInst] = true;

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Determine whether the surface has been peer mapped.
//!
//! \param pRemoteDev : Optional parameter, if not present, it will check if
//!                     the allocated device has been peer mapped.  If present
//!                     it will check if the specifed GpuDevice has been peer
//!                     mapped.
//!
//! \return true if the specified (or allocated if pRemoteDev is not specified
//!         or NULL) GpuDevice has been peer mapped via MapPeer(),
//!         MapPeer(GpuDevice *), or MapLoopback().  false otherwise
bool Surface2D::IsPeerMapped(GpuDevice *pRemoteDev /* = NULL */) const
{
    if (m_Location != Memory::Fb)
        return false;
    if (NULL == pRemoteDev)
        pRemoteDev = GetGpuDev();

    // Peer and shared mappings are always duplicated between clients so it
    //is enough to check for remote mapping on the main RM client
    return m_RemoteIsMapped[pRemoteDev->GetDeviceInst()];
}

//-----------------------------------------------------------------------------
//! \brief Determine whether the surface has been shared with the specified
//!        GpuDevice.
//!
//! \param pSharedDev : Shared GpuDevice to check.
//!
//! \return true if the specified GpuDevice has been peer mapped via
//!         MapShared(GpuDevice *).  false otherwise
bool Surface2D::IsMapShared(GpuDevice *pSharedDev) const
{
    if (m_Location == Memory::Fb)
        return false;

    MASSERT(pSharedDev);

    // Peer and shared mappings are always duplicated between clients so it
    //is enough to check for remote mapping on the main RM client
    return m_RemoteIsMapped[pSharedDev->GetDeviceInst()];
}

//-----------------------------------------------------------------------------
//! \brief Get the peer mapped GPU context DMA offset for the specified
//!        subdevice on the GpuDevice where the surface has been allocated.
//!        Only valid after a successful call to MapPeer() or MapLoopback().
//!        Surface2D uses 0-based subdevice numbers, like the RM
//!
//! \param subdev : Subdevice to get the peer offset for.
//! \param pLwRm  : Pointer to RM Client to get the peer offset for.
//!
//! \return GPU peer mapped offset
UINT64 Surface2D::GetCtxDmaOffsetGpuPeer(UINT32 subdev, UINT32 peerId, LwRm *pLwRm) const
{
    return GetCtxDmaOffsetGpuPeer(subdev, GetGpuDev(), 0, peerId, pLwRm);
}
UINT64 Surface2D::GetCtxDmaOffsetGpuPeer(UINT32 subdev, LwRm *pLwRm) const
{
    return GetCtxDmaOffsetGpuPeer(subdev, GetGpuDev(), 0, USE_DEFAULT_RM_MAPPING, pLwRm);
}

//-----------------------------------------------------------------------------
//! \brief Get the peer mapped GPU virtual address (only used if the surface
//!        is allocated using virtual memory) for the specified  subdevice on
//!        the GpuDevice where the surface has been allocated.  Only valid
//!        after a successful call to MapPeer() or MapLoopback().  Surface2D
//!        uses 0-based subdevice numbers, like the RM
//!
//! \param subdev : Subdevice to get the peer mapped GPU virtual address for.
//! \param pLwRm  : Pointer to RM Client to get the peer mapped GPU virtual
//!                 address for.
//!
//! \return The GPU peer mapped virtual address
UINT64 Surface2D::GetGpuVirtAddrPeer(UINT32 subdev, LwRm *pLwRm) const
{
    return GetGpuVirtAddrPeer(subdev, GetGpuDev(), 0, pLwRm);
}

//-----------------------------------------------------------------------------
//! \brief Get the peer mapped GPU virtual memory handle (only used if the
//!        surface is allocated using virtual memory) for the specified
//!        subdevice on the GpuDevice where the surface has been allocated.
//!        Only valid after a successful call to MapPeer() or MapLoopback().
//!        Surface2D uses 0-based subdevice numbers, like the RM
//!
//! \param subdev : Subdevice to get the peer mapped GPU virtual memory handle
//!                 for.
//! \param pLwRm  : Pointer to RM Client to get the peer mapped GPU virtual
//!                 memory handle for.
//!
//! \return The GPU peer mapped virtual memory handle
LwRm::Handle Surface2D::GetVirtMemHandleGpuPeer(UINT32 subdev, LwRm *pLwRm) const
{
    return GetVirtMemHandleGpuPeer(subdev, GetGpuDev(), 0, pLwRm);
}

//-----------------------------------------------------------------------------
//! \brief Get the peer context DMA handle for the specified GpuDevice.  Only
//!        valid after a sucessfull call to MapPeer(), MapPeer(GpuDevice *), or
//!        MapLoopback()
//!
//! \param pRemoteDev : Remote device to get the peer context DMA handle for.
//! \param pLwRm      : Pointer to RM Client to get the peer context DMA
//!                     handle for.
//!
//! \return Context DMA handle for the peer device
LwRm::Handle Surface2D::GetCtxDmaHandleGpuPeer
(
    GpuDevice *pRemoteDev,
    LwRm *pLwRm
) const
{
    MASSERT(pRemoteDev);
    MASSERT(IsPeerMapped(pRemoteDev));

    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    if (!pClientData)
        return 0;

    map<UINT32, LwRm::Handle>::const_iterator pRemoteHandles;

    pRemoteHandles = pClientData->m_hCtxDmaGpuRemote.find(
                                                 pRemoteDev->GetDeviceInst());
    return pRemoteHandles->second;
}

//-----------------------------------------------------------------------------
//! \brief Get the peer mapped GPU context DMA offset that should be used on
//!        the remote subdevice (within the remote GpuDevice) to reference the
//!        memory allocated on the local subdevice where the surface has been
//!        allocated.  Only valid after a successful call to MapPeer(),
//!        MapPeer(GpuDevice *), or MapLoopback().  Surface2D uses 0-based
//!        subdevice numbers, like the RM
//!
//! \param locSD      : Subdevice within the GpuDevice that the surface has
//!                     been allocated on to get the offset for.
//! \param pRemoteDev : Remote GpuDevice to retrieve the offset for.
//! \param locSD      : Remote subdevice within the remote GpuDevice to
//!                     retrieve the offset for.
//! \param pLwRm      : Pointer to RM Client to retrieve the offset for.
//!
//! \return The GPU peer mapped offset that the remote subdevice should use to
//!         access memory within the local subdevice
UINT64 Surface2D::GetCtxDmaOffsetGpuPeer(UINT32     locSD,
                                         GpuDevice *pRemoteDev,
                                         UINT32     remSD,
                                         LwRm      *pLwRm) const
{
    return GetCtxDmaOffsetGpuPeer(locSD,pRemoteDev,remSD,USE_DEFAULT_RM_MAPPING,pLwRm);
}

UINT64 Surface2D::GetCtxDmaOffsetGpuPeer(UINT32     locSD,
                                         GpuDevice *pRemoteDev,
                                         UINT32     remSD,
                                         UINT32     peerId,
                                         LwRm      *pLwRm) const
{
    MASSERT(pRemoteDev);
    MASSERT(IsPeerMapped(pRemoteDev));
    MASSERT(remSD < pRemoteDev->GetNumSubdevices());
    MASSERT(locSD < GetGpuDev()->GetNumSubdevices());

    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    if (!pClientData)
        return 0;

    RemoteMapping mapIndex(locSD, pRemoteDev->GetDeviceInst(), remSD, peerId);
    MASSERT(pClientData->m_CtxDmaOffsetGpuRemote.count(mapIndex));

    map<RemoteMapping, UINT64>::const_iterator pRemoteOffsets;
    pRemoteOffsets = pClientData->m_CtxDmaOffsetGpuRemote.find(mapIndex);
    return pRemoteOffsets->second;
}

//-----------------------------------------------------------------------------
//! \brief Get the peer mapped GPU virtual address (only used if the surface
//!        is allocated using virtual memory) that should be used on the remote
//!        subdevice (within the remote GpuDevice) to reference the memory
//!        allocated on the local subdevice where the surface has been
//!        allocated.  Only valid after a successful call to MapPeer(),
//!        MapPeer(GpuDevice *), or MapLoopback().  Surface2D uses 0-based
//!        subdevice numbers, like the RM
//!
//! \param locSD      : Subdevice within the GpuDevice that the surface has
//!                     been allocated on to get the GPU virtual address for.
//! \param pRemoteDev : Remote GpuDevice to retrieve the GPU virtual address
//!                     for.
//! \param locSD      : Remote subdevice within the remote GpuDevice to
//!                     retrieve the GPU virtual address for.
//! \param pLwRm      : Pointer to RM Client to retrieve the GPU virtual
//!                     address for.
//!
//! \return The GPU peer mapped virtual address that the remote subdevice
//!         should use to access memory within the local subdevice
UINT64 Surface2D::GetGpuVirtAddrPeer(UINT32     locSD,
                                     GpuDevice *pRemoteDev,
                                     UINT32     remSD,
                                     LwRm      *pLwRm) const
{
    return GetGpuVirtAddrPeer(locSD,pRemoteDev,remSD,USE_DEFAULT_RM_MAPPING,pLwRm);
}

UINT64 Surface2D::GetGpuVirtAddrPeer(UINT32     locSD,
                                     GpuDevice *pRemoteDev,
                                     UINT32     remSD,
                                     UINT32     peerId,
                                     LwRm      *pLwRm) const
{
    MASSERT(pRemoteDev);
    MASSERT(IsPeerMapped(pRemoteDev));
    MASSERT(remSD < pRemoteDev->GetNumSubdevices());
    MASSERT(locSD < GetGpuDev()->GetNumSubdevices());

    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    if (!pClientData)
        return 0;

    RemoteMapping mapIndex(locSD,pRemoteDev->GetDeviceInst(),remSD,peerId);
    MASSERT(pClientData->m_GpuVirtAddrRemote.count(mapIndex));

    map<RemoteMapping, UINT64>::const_iterator pRemoteAddrs;
    pRemoteAddrs = pClientData->m_GpuVirtAddrRemote.find(mapIndex);
    return pRemoteAddrs->second;
}

//-----------------------------------------------------------------------------
//! \brief Get the peer mapped GPU virtual memory handle (only used if the
//!        surface is allocated using virtual memory) that should be used on
//!        the remote subdevice (within the remote GpuDevice) to reference the
//!        memory allocated on the local subdevice where the surface has been
//!        allocated.  Only valid after a successful call to MapPeer(),
//!        MapPeer(GpuDevice *), or MapLoopback().  Surface2D uses 0-based
//!        subdevice numbers, like the RM
//!
//! \param locSD      : Subdevice within the GpuDevice that the surface has
//!                     been allocated on to get the GPU virtual memory handle
//!                     for.
//! \param pRemoteDev : Remote GpuDevice to retrieve the GPU virtual memory
//!                     handle for.
//! \param locSD      : Remote subdevice within the remote GpuDevice to
//!                     retrieve the GPU virtual memory handle for.
//! \param pLwRm      : Pointer to RM Client to retrieve the GPU virtual
//!                     memory handle for.
//!
//! \return The GPU peer mapped virtual memory handle that the remote subdevice
//!         should use to access memory within the local subdevice
LwRm::Handle Surface2D::GetVirtMemHandleGpuPeer(UINT32     locSD,
                                                GpuDevice *pRemoteDev,
                                                UINT32     remSD,
                                                LwRm      *pLwRm) const
{
    return GetVirtMemHandleGpuPeer(locSD,pRemoteDev,remSD,USE_DEFAULT_RM_MAPPING,pLwRm);
}

LwRm::Handle Surface2D::GetVirtMemHandleGpuPeer(UINT32     locSD,
                                                GpuDevice *pRemoteDev,
                                                UINT32     remSD,
                                                UINT32     peerId,
                                                LwRm      *pLwRm) const
{
    MASSERT(pRemoteDev);
    MASSERT(IsPeerMapped(pRemoteDev));
    MASSERT(remSD < pRemoteDev->GetNumSubdevices());
    MASSERT(locSD < GetGpuDev()->GetNumSubdevices());

    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    if (!pClientData)
        return 0;

    RemoteMapping mapIndex(locSD,pRemoteDev->GetDeviceInst(),remSD,peerId);
    MASSERT(pClientData->m_hVirtMemGpuRemote.count(mapIndex));

    map<RemoteMapping, LwRm::Handle>::const_iterator pRemoteHandles;
    pRemoteHandles = pClientData->m_hVirtMemGpuRemote.find(mapIndex);
    return pRemoteHandles->second;
}

//-----------------------------------------------------------------------------
//! \brief Get the shared context DMA handle for the specified GpuDevice.  Only
//!        valid after a sucessfull call to MapShared()
//!
//! \param pSharedDev : Shared device to get the context DMA handle for.
//! \param pLwRm      : Pointer to RM Client to get the context DMA
//!                     handle for.
//!
//! \return Context DMA handle for the shared device
LwRm::Handle Surface2D::GetCtxDmaHandleGpuShared(GpuDevice *pSharedDev,
                                                 LwRm      *pLwRm) const
{
    MASSERT(pSharedDev);
    MASSERT(IsMapShared(pSharedDev));

    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    if (!pClientData)
        return 0;

    map<UINT32, LwRm::Handle>::const_iterator pRemoteHandles;
    pRemoteHandles = pClientData->m_hCtxDmaGpuRemote.find(
                                                 pSharedDev->GetDeviceInst());
    return pRemoteHandles->second;
}

//-----------------------------------------------------------------------------
//! \brief Get the shared GPU context DMA offset that should be used on the
//!        shared device to reference the memory allocated for the surface.
//!        Only valid after a successful call to MapShared()
//!
//! \param pSharedDev : Shared GpuDevice to retrieve the offset for.
//! \param pLwRm      : Pointer to RM Client to retrieve the offset for.
//!
//! \return The shared offset that the sharing device should use to
//!         access memory within the DmaBuffer
UINT64 Surface2D::GetCtxDmaOffsetGpuShared(GpuDevice *pSharedDev,
                                           LwRm      *pLwRm) const
{
    MASSERT(pSharedDev);
    MASSERT(IsMapShared(pSharedDev));

    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    if (!pClientData)
        return 0;

    RemoteMapping mapIndex(0,pSharedDev->GetDeviceInst(), 0, USE_DEFAULT_RM_MAPPING);
    MASSERT(pClientData->m_CtxDmaOffsetGpuRemote.count(mapIndex));

    map<RemoteMapping, UINT64>::const_iterator pRemoteOffsets;
    pRemoteOffsets = pClientData->m_CtxDmaOffsetGpuRemote.find(mapIndex);
    return pRemoteOffsets->second;
}

//-----------------------------------------------------------------------------
//! \brief Get the shared GPU virtual address (only used if the surface
//!        is allocated using virtual memory) that should be used on the
//!        shared device to reference the memory allocated for the surface.
//!        Only valid after a successful call to MapShared()
//!
//! \param pSharedDev : Shared GpuDevice to retrieve the virtual address for.
//! \param pLwRm      : Pointer to RM Client to retrieve the virtual
//!                     address for.
//!
//! \return The shared virtual address that the sharing device should use to
//!         access memory within the DmaBuffer
UINT64 Surface2D::GetGpuVirtAddrShared(GpuDevice *pSharedDev,
                                       LwRm      *pLwRm) const
{
    MASSERT(pSharedDev);
    MASSERT(IsMapShared(pSharedDev));

    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    if (!pClientData)
        return 0;

    RemoteMapping mapIndex(0,pSharedDev->GetDeviceInst(), 0, USE_DEFAULT_RM_MAPPING);
    MASSERT(pClientData->m_GpuVirtAddrRemote.count(mapIndex));

    map<RemoteMapping, UINT64>::const_iterator pRemoteAddrs;
    pRemoteAddrs = pClientData->m_GpuVirtAddrRemote.find(mapIndex);
    return pRemoteAddrs->second;
}

//-----------------------------------------------------------------------------
//! \brief Get the shared GPU virtual memory handle (only used if the surface
//!        is allocated using virtual memory) that should be used on the
//!        shared device to reference the memory allocated for the surface.
//!        Only valid after a successful call to MapShared()
//!
//! \param pSharedDev : Shared GpuDevice to retrieve the virtual memory handle
//!                     for.
//! \param pLwRm      : Pointer to RM Client to retrieve the virtual
//!                     memory handle for.
//!
//! \return The shared virtual memory handle that the sharing device should use
//!         to access memory within the DmaBuffer
UINT32 Surface2D::GetVirtMemHandleGpuShared(GpuDevice *pSharedDev,
                                            LwRm      *pLwRm) const
{
    MASSERT(pSharedDev);
    MASSERT(IsMapShared(pSharedDev));

    AllocData *pClientData = GetClientData(&pLwRm, __FUNCTION__);
    if (!pClientData)
        return 0;

    RemoteMapping mapIndex(0,pSharedDev->GetDeviceInst(), 0,USE_DEFAULT_RM_MAPPING);
    MASSERT(pClientData->m_hVirtMemGpuRemote.count(mapIndex));

    map<RemoteMapping, LwRm::Handle>::const_iterator pRemoteHandles;
    pRemoteHandles = pClientData->m_hVirtMemGpuRemote.find(mapIndex);
    return pRemoteHandles->second;
}

//-----------------------------------------------------------------------------
// Private function that sets the RM client instance to the default client if
// null and then validates that the surface exists on that client
Surface2D::AllocData *Surface2D::GetClientData
(
    LwRm ** ppClient,
    const char *pCallingFunction
) const
{
    const AllocData *pClientData = NULL;

    if (*ppClient == 0)
        *ppClient = m_pLwRm;

    if (*ppClient == m_pLwRm)
    {
        // default client
        pClientData = &m_DefClientData;
    }
    else
    {
        // do a slow lookup for any other client
        map<LwRm *, AllocData>::const_iterator pData = m_PeerClientData.find(*ppClient);
        if (pData == m_PeerClientData.end())
        {
            Printf(Tee::PriHigh, "%s : Surface not allocated on client %p\n",
                    pCallingFunction, *ppClient);
        }
        else
        {
            pClientData = &pData->second;
        }
    }

    return const_cast<AllocData *>(pClientData);
}

//-----------------------------------------------------------------------------
RC Surface2D::EnableDualPageSize(bool DualPageSize)
{
    RC rc;
    if (DualPageSize == m_DualPageSize)
        return OK;

    // Only enable operation is supported:
    if (!DualPageSize && m_DualPageSize)
        return RC::SOFTWARE_ERROR;

    // Segmentation mode is not supported:
    // (Probably this would require reallocation of m_hCtxDmaGpu
    //  if the support is possible at all)
    if (m_AddressModel == Memory::Segmentation)
        return RC::SOFTWARE_ERROR;

    m_DualPageSize = DualPageSize;

    if (!m_CtxDmaMappedGpu && !m_VirtMemMapped )
        return OK;

    if (m_PageSize == 0)
    {
        // 0 means LWOS32_ATTR_PAGE_SIZE_DEFAULT
        // RM will enable both SMALL and BIG PTE for _DEFAULT attr
        return OK;
    }

    // Remote mappings are not supported:
    for (UINT32 i = 0; i < MaxD; i++)
    {
        if (m_RemoteIsMapped[i])
            return RC::SOFTWARE_ERROR;
    }

    UINT64 OldGpuVirtAddrHint = GetGpuVirtAddrHint();
    UINT64 OldVirtAlign = GetVirtAlignment();

    Utility::CleanupOnReturnArg<Surface2D,void,UINT64>
        RestoreHint(this, &Surface2D::SetGpuVirtAddrHint, OldGpuVirtAddrHint);
    Utility::CleanupOnReturnArg<Surface2D, void, UINT64>
        RestoreVirtAlignment(this, &Surface2D::SetVirtAlignment, OldVirtAlign);

    UINT64 CtxDmaOffsetGpu = GetCtxDmaOffsetGpu(m_pLwRm);

    UnmapPhysMemory(m_pLwRm);
    map<LwRm *, AllocData>::iterator pRmClientData;
    for (pRmClientData = m_PeerClientData.begin();
         pRmClientData != m_PeerClientData.end(); pRmClientData++)
    {
        UnmapPhysMemory(pRmClientData->first);
    }

    SetGpuVirtAddrHint(CtxDmaOffsetGpu);
    SetVirtAlignment(1);  // Prevents RM from rejecting VirtAddrHint

    CHECK_RC(MapPhysMemory(m_pLwRm));
    for (pRmClientData = m_PeerClientData.begin();
         pRmClientData != m_PeerClientData.end(); pRmClientData++)
    {
        CHECK_RC(MapPhysMemory(pRmClientData->first));
    }

    return rc;
}

bool Surface2D::IsDualPageSize() const
{
    // 1. 0 means LWOS32_ATTR_PAGE_SIZE_DEFAULT
    //    RM will enable both SMALL and BIG PTE for _DEFAULT attr
    // 2. DualPageSize is enabled.
    return m_DualPageSize || (m_PageSize == 0);
}

//-----------------------------------------------------------------------------
// Adjust surface properties based on the attribute flags that will be
// passed to RM when the surface is allocated.
//-----------------------------------------------------------------------------
void Surface2D::ConfigFromAttr(UINT32 attr)
{
    switch (DRF_VAL(OS32, _ATTR, _FORMAT, attr))
    {
        case LWOS32_ATTR_FORMAT_PITCH:
            SetLayout(Pitch);
            break;
        case LWOS32_ATTR_FORMAT_SWIZZLED:
            SetLayout(Swizzled);
            break;
        case LWOS32_ATTR_FORMAT_BLOCK_LINEAR:
            SetLayout(BlockLinear);
            break;
        default:
            MASSERT(!"Unknown FORMAT attribute");
            break;
    }

    switch (DRF_VAL(OS32, _ATTR, _AA_SAMPLES, attr))
    {
        case LWOS32_ATTR_AA_SAMPLES_1:
            SetAAMode(AA_1);
            break;
        case LWOS32_ATTR_AA_SAMPLES_2:
            SetAAMode(AA_2);
            break;
        case LWOS32_ATTR_AA_SAMPLES_4:
            SetAAMode(AA_4);
            break;
        case LWOS32_ATTR_AA_SAMPLES_4_ROTATED:
            SetAAMode(AA_4_Rotated);
            break;
        case LWOS32_ATTR_AA_SAMPLES_6:
            SetAAMode(AA_6);
            break;
        case LWOS32_ATTR_AA_SAMPLES_8:
            SetAAMode(AA_8);
            break;
        case LWOS32_ATTR_AA_SAMPLES_16:
            SetAAMode(AA_16);
            break;
        case LWOS32_ATTR_AA_SAMPLES_4_VIRTUAL_8:
            SetAAMode(AA_4_Virtual_8);
            break;
        case LWOS32_ATTR_AA_SAMPLES_4_VIRTUAL_16:
            SetAAMode(AA_4_Virtual_16);
            break;
        case LWOS32_ATTR_AA_SAMPLES_8_VIRTUAL_16:
            SetAAMode(AA_8_Virtual_16);
            break;
        case LWOS32_ATTR_AA_SAMPLES_8_VIRTUAL_32:
            SetAAMode(AA_8_Virtual_32);
            break;
        default:
            MASSERT(!"Unknown AA_SAMPLES attribute");
            break;
    }

    UINT32 value = DRF_VAL(OS32, _ATTR, _ZLWLL, attr);
    SetZLwllFlag(value);

    value = DRF_VAL(OS32, _ATTR, _COMPR, attr);
    SetCompressed(value != LWOS32_ATTR_COMPR_NONE);
    SetCompressedFlag(value);

    if(Platform::Amodel == Platform::GetSimulationMode())
    {
        if (FLD_TEST_DRF(OS32, _ATTR, _LOCATION, _VIDMEM, attr))
        {
            SetLocation(Memory::Fb);
        }
        else if (FLD_TEST_DRF(OS32, _ATTR, _LOCATION, _PCI, attr) &&
                FLD_TEST_DRF(OS32, _ATTR, _COHERENCY, _CACHED, attr))
        {
            SetLocation(Memory::Coherent);
        }
        else if (FLD_TEST_DRF(OS32, _ATTR, _LOCATION, _PCI, attr) &&
                FLD_TEST_DRF(OS32, _ATTR, _COHERENCY, _WRITE_COMBINE, attr))
        {
            SetLocation(Memory::NonCoherent);
        }
        else
        {
            Printf(Tee::PriNormal, "Warning: using the default location since the location "
                    "and coherency attributes aren't a recognized combinatin.\n");
        }
    }

    // Set the rest of attributes
    SetDefaultVidHeapAttr(attr);
}

void Surface2D::ConfigFromAttr2(UINT32 attr2)
{
    if (Platform::UsesLwgpuDriver())
    {
        attr2 = FLD_SET_DRF(OS32, _ATTR2, _ZBC, _DEFAULT, attr2);
    }
    else
    {
        switch (DRF_VAL(OS32, _ATTR2, _ZBC, attr2))
        {
            case LWOS32_ATTR2_ZBC_DEFAULT:
                SetZbcMode(ZbcDefault);
                break;
            case LWOS32_ATTR2_ZBC_PREFER_NO_ZBC:
                SetZbcMode(ZbcOff);
                break;
            case LWOS32_ATTR2_ZBC_PREFER_ZBC:
                SetZbcMode(ZbcOn);
                break;
            default:
                MASSERT(!"Unknown ZBC attribute");
                break;
        }
    }

    switch (DRF_VAL(OS32, _ATTR2, _GPU_CACHEABLE, attr2))
    {
        case LWOS32_ATTR2_GPU_CACHEABLE_DEFAULT:
            SetGpuCacheMode(GpuCacheDefault);
            break;
        case LWOS32_ATTR2_GPU_CACHEABLE_YES:
            SetGpuCacheMode(GpuCacheOn);
            break;
        case LWOS32_ATTR2_GPU_CACHEABLE_NO:
            SetGpuCacheMode(GpuCacheOff);
            break;
        default:
            MASSERT(!"Unknown GPU_CACHEABLE attribute");
            break;
    }

    switch (DRF_VAL(OS32, _ATTR2, _P2P_GPU_CACHEABLE, attr2))
    {
        case LWOS32_ATTR2_P2P_GPU_CACHEABLE_DEFAULT:
            SetP2PGpuCacheMode(GpuCacheDefault);
            break;
        case LWOS32_ATTR2_P2P_GPU_CACHEABLE_YES:
            SetP2PGpuCacheMode(GpuCacheOn);
            break;
        case LWOS32_ATTR2_P2P_GPU_CACHEABLE_NO:
            SetP2PGpuCacheMode(GpuCacheOff);
            break;
        default:
            MASSERT(!"Unknown P2P_GPU_CACHEABLE attribute");
            break;
    }

    SetDefaultVidHeapAttr2(attr2);
}

string Surface2D::GetGpuManagedChName() const
{
    if (!m_IsHostReflectedSurf)
    {
        MASSERT(m_GpuManagedChName.empty());
    }

    return m_GpuManagedChName;
}
//-----------------------------------------------------------------------------
//! \brief Copy the allocation values from another surface.
//!
//! Copy the addreses and other allocation values from another surface.
//! This surface will then represent a "view" of the other surface and
//! should never be freed.
//!
//! \param surface : The surface from which to copy the values
//! \param surface : The virtual address offset from the parent surface
//!
RC Surface2D::SetSurfaceViewParent(const Surface2D *surface, UINT64 offset)
{
    m_CpuMapOffset = offset;

    m_pGpuDev = surface->m_pGpuDev;
    m_DefClientData = surface->m_DefClientData;
    m_DefClientData.m_CtxDmaOffsetGpu += offset;
    m_pLwRm = surface->GetLwRmPtr();

    ComputeParams();

    return OK;
}

UINT64 Surface2D::GetCdeAllocSize(CdeType type) const
{
    if (!m_bCdeAlloc)
        return 0;

    UINT64 dim = (type == CDE_HORZ) ? m_Width : m_Height;
    UINT64 size = 16ULL * ALIGN_UP(dim, 8ULL) / 8ULL;
    size += CdePadding;
    return ALIGN_UP(size, CdeAllocAlignment);
}

RC Surface2D::MapCde()
{
    if (!m_bCdeAlloc)
    {
        Printf(Tee::PriHigh,
               "Surface2D::MapCde : CDE data not allocated\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    AllocData *pClientData = GetClientData(&m_pLwRm, __FUNCTION__);
    if (!pClientData)
    {
        MASSERT(!"Surface2D::MapCde: unknown client");
        return RC::SOFTWARE_ERROR;
    }

    if (!pClientData->m_hMem)
    {
        Printf(Tee::PriHigh,
              "Surface2D::MapCde : Allocate surface before mapping CDE data\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    if (m_CdeAddress != nullptr)
    {
        Printf(Tee::PriLow,
               "Surface2D::MapCde : CDE already mapped\n");
        return OK;
    }

    RC      rc;
    const UINT64 cdeSize = GetCdeAllocSize(CDE_HORZ) +
                           GetCdeAllocSize(CDE_VERT);

    // On CheetAh with "kernel mode RM", in which case we're actually running
    // with Linux kernel drivers instead of dGPU RM, we don't have the ability
    // to allocate VA space objects (they are not supported).
    //
    // In this case we will return the original physical memory handle.
    // Appropriate drivers can colwert it to an internal kernel memory handle
    // using LwRm::GetKernelHandle(), which is usable with other
    // kernel drivers.
    MASSERT(Platform::UsesLwgpuDriver());
    pClientData->m_hCdeVirtMem = pClientData->m_hMem;
    pClientData->m_CdeVirtAddr = m_Size - cdeSize;

    // Normally this memory would be unmapped, but since we are using static
    // surfaces we need to manually map it so we can fill it in
    m_CdeAddress = nullptr;
    CHECK_RC(m_pLwRm->MapMemory(pClientData->m_hMem,
                                m_Size - cdeSize,
                                cdeSize,
                                &m_CdeAddress,
                                LWOS33_FLAGS_MAPPING_DIRECT,
                                GetGpuSubdev(0)));

    return rc;
}

void Surface2D::UnmapCde()
{
    if (m_CdeAddress)
    {
        m_pLwRm->UnmapMemory(m_DefClientData.m_hMem,
                             m_CdeAddress,
                             0,
                             GetGpuSubdev(0));
    }

    m_DefClientData.m_CdeVirtAddr = 0;
    m_DefClientData.m_hCdeVirtMem = 0;
    m_CdeAddress = nullptr;
}

RC Surface2D::GetCtxDmaOffsetCde(CdeType type, UINT64 *pOffset)
{
    if (!m_bCdeAlloc)
    {
        Printf(Tee::PriError, "%s : CDE compbit storage not allocated\n",
               __FUNCTION__);
        return RC::UNSUPPORTED_FUNCTION;
    }
    AllocData *pClientData = GetClientData(&m_pLwRm, __FUNCTION__);
    if (pClientData)
    {
        if (pClientData->m_CdeVirtAddr == 0)
        {
            Printf(Tee::PriError, "%s : CDE compbit storage not mapped\n",
                   __FUNCTION__);
            return RC::UNSUPPORTED_FUNCTION;
        }
        const UINT64 offset = (type == CDE_HORZ) ? 0 : GetCdeAllocSize(CDE_HORZ);
        *pOffset = pClientData->m_CdeVirtAddr + offset;
    }
    else
    {
        *pOffset = 0;
    }
    return OK;
}

RC Surface2D::GetCdeAddress(CdeType type, void **ppvAddress)
{
    if (!m_bCdeAlloc)
    {
        Printf(Tee::PriHigh, "%s : CDE compbit storage not allocated\n",
               __FUNCTION__);
        return RC::UNSUPPORTED_FUNCTION;
    }

    if (m_CdeAddress == nullptr)
    {
        Printf(Tee::PriHigh, "%s : CDE compbit storage not mapped\n",
               __FUNCTION__);
        return RC::UNSUPPORTED_FUNCTION;
    }

    UINT08 *pCdeOffset = static_cast<UINT08 *>(m_CdeAddress);
    if (type == CDE_VERT)
        pCdeOffset += GetCdeAllocSize(CDE_HORZ);
    *ppvAddress = pCdeOffset;
    return OK;
}

bool Surface2D::HasVirtual() const
{
    return (m_Specialization == NoSpecialization) ||
        (m_Specialization == VirtualOnly) ||
        (m_Specialization == VirtualPhysicalNoMap);
}

bool Surface2D::HasPhysical() const
{
    if (m_Specialization == NoSpecialization)
    {
        return true;
    }
    else if (m_Specialization == PhysicalOnly)
    {
        return true;
    }
    else if (m_Specialization == VirtualPhysicalNoMap)
    {
        return true;
    }

    return false;
}

bool Surface2D::HasMap() const
{
    return (m_Specialization == NoSpecialization) ||
        (m_Specialization == MapOnly);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// \brief Return the actual location that will be used for a surface
//
// Given a requested location (Fb, Coherent, etc), return the actual
// location that will be used after applying GetLocationOverride() and
// Memory::Optimal settings.
//
/* static */ Memory::Location Surface2D::GetActualLocation
(
    Memory::Location requestedLocation,
    GpuSubdevice *pGpuSubdevice
)
{
    MASSERT(pGpuSubdevice != NULL);

    Memory::Location actualLocation = requestedLocation;
    if (s_LocationOverride != Surface2D::NO_LOCATION_OVERRIDE)
    {
        actualLocation = static_cast<Memory::Location>(s_LocationOverride);
    }
    actualLocation = pGpuSubdevice->FixOptimalLocation(actualLocation);

    // Use coherent system memory on systems which don't support non-coherent, eg. PPC64LE
    if ((actualLocation == Memory::NonCoherent)
        && !pGpuSubdevice->HasFeature(Device::GPUSUB_HAS_NONCOHERENT_SYSMEM))
    {
        actualLocation = Memory::Coherent;
    }

    // Use non-coherent system memory on GPUs which don't support coherent
    if ((actualLocation == Memory::Coherent)
        && !pGpuSubdevice->HasFeature(Device::GPUSUB_HAS_COHERENT_SYSMEM))
    {
        MASSERT(pGpuSubdevice->HasFeature(Device::GPUSUB_HAS_NONCOHERENT_SYSMEM));
        actualLocation = Memory::NonCoherent;
    }

    // Use only vidmem on GPUs which don't support sysmem
    if ((actualLocation != Memory::Fb)
        && !pGpuSubdevice->HasFeature(Device::GPUSUB_HAS_SYSMEM))
    {
        actualLocation = Memory::Fb;
    }

    if ((s_LocationOverrideSysmem != Surface2D::NO_LOCATION_OVERRIDE) &&
        (actualLocation != Memory::Fb))
    {
        actualLocation = static_cast<Memory::Location>(s_LocationOverrideSysmem);
    }

    if ((actualLocation == Memory::Fb)
        && !pGpuSubdevice->HasFeature(Device::GPUSUB_HAS_FB_HEAP)
        && !pGpuSubdevice->getL2CacheOnlyMode()
        && (pGpuSubdevice->IsSOC()
                || pGpuSubdevice->HasFeature(Device::GPUSUB_IS_STANDALONE_TEGRA_GPU)))
    {
        MASSERT(pGpuSubdevice->HasFeature(Device::GPUSUB_HAS_SYSMEM));
        actualLocation = Memory::NonCoherent;
    }

    // T194 supports I/O coherency, which means the iGPU and dGPU can snoop
    // the CPU's cache.  Coherent sysmem (mapped as WB) has better perf than
    // NonCoherent (mapped as WC) on Delwer CPUs so it is normally recommended.
    if ((actualLocation == Memory::NonCoherent)
        && pGpuSubdevice->HasFeature(Device::GPUSUB_HAS_COHERENT_SYSMEM)
        && Platform::IsTegra()
        && (Xp::GetOperatingSystem() != Xp::OS_LINUXSIM) // not in sim MODS
        && (s_LocationOverride == Surface2D::NO_LOCATION_OVERRIDE))
    {
        actualLocation = Memory::Coherent;
    }

    return actualLocation;
}

//-----------------------------------------------------------------------------
// Remote mapping class, used as an index to the remote mapping data maps to
// allocating large static arrays
Surface2D::RemoteMapping::RemoteMapping
(
    UINT32 locSD,
    UINT32 remD,
    UINT32 remSD,
    UINT32 peerId
) : m_LocSD(locSD), m_RemD(remD), m_RemSD(remSD),m_PeerId(peerId)
{
}

bool Surface2D::RemoteMapping::operator<(const RemoteMapping& rhs) const
{
    if (m_LocSD < rhs.m_LocSD)
        return true;
    else if (m_LocSD > rhs.m_LocSD)
        return false;
    if (m_RemD < rhs.m_RemD)
        return true;
    else if (m_RemD > rhs.m_RemD)
        return false;
    else if (m_RemSD < rhs.m_RemSD)
        return true;
    else if (m_RemSD > rhs.m_RemSD)
        return false;

    return (m_PeerId < rhs.m_PeerId);

}
//-----------------------------------------------------------------------------
// Tiled surfaces on CheetAh only supports certain pitch
RC Surface2D::GetTiledPitch(UINT32 oldPitch, UINT32 * newPitch)
{
    // Tiled surfaces have very specific pitch requirements
    // Specifically p = k * 2^(n+6) where k=[1,3,5,7,9,11,13,15] and
    // n=[0,1,2,3,4,5,6], minimum tiled pitch is 64
    UINT32 minPitch = oldPitch;
    bool pitchFound = false;
    for (UINT32 k = 1; k <= 15; k += 2)
    {
        for (UINT32 n = 0; n <= 6; n++)
        {
            UINT32 lwrPitch = k * (1 << (n + 6));
            if (lwrPitch >= oldPitch &&
                (!pitchFound || lwrPitch < minPitch))
            {
                pitchFound = true;
                minPitch = lwrPitch;
            }
        }
    }
    if (pitchFound)
    {
        *newPitch = minPitch;
        Printf(Tee::PriLow, "Old pitch=%u New pitch=%u!\n", oldPitch, *newPitch);
        return OK;
    }
    Printf(Tee::PriHigh, "No appropriate pitch with Old pitch=%u\n", oldPitch);
    return RC::UNEXPECTED_RESULT;
}

void Surface2D::CommitEgmAttr()
{
    m_VidHeapAttr2 = FLD_SET_DRF(OS32, _ATTR2, _USE_EGM, _TRUE,
            m_VidHeapAttr2);
}

void Surface2D::SetExternalPhysMem
(
    LwRm::Handle physMem,
    PHYSADDR physAddr
)
{
    MASSERT(m_DefClientData.m_hMem == 0);
    MASSERT(m_ExternalPhysMem == 0);
    MASSERT(physMem != 0);

    m_ExternalPhysMem = physMem;
    m_DefClientData.m_VidMemOffset = physAddr;
}

//-----------------------------------------------------------------------------
// AllocData constructor to create an zero'd AllocData object
Surface2D::AllocData::AllocData()
:   m_hGpuVASpace(0),
    m_hMem(0),
    m_hSplitMem(0),
    m_hVirtMem(0),
    m_hTegraVirtMem(0),
    m_hImportMem(0),
    m_hCtxDmaGpu(0),
    m_hCtxDmaIso(0),
    m_GpuVirtAddr(0),
    m_TegraVirtAddr(0),
    m_CtxDmaOffsetGpu(0),
    m_CtxDmaOffsetIso(0),
    m_VidMemOffset(0),
    m_SplitVidMemOffset(0),
    m_SplitGpuVirtAddr(0),
    m_hCdeVirtMem(0),
    m_CdeVirtAddr(0),
    m_hFLA(0)
{
}

//-----------------------------------------------------------------------------
void Surface2D::AllocData::Print(Tee::Priority pri, const char* linePrefix)
{
    Printf(pri, "%s%15s:0x%x\n", linePrefix, "hGpuVASpace",   m_hGpuVASpace);
    Printf(pri, "%s%15s:0x%x\n", linePrefix, "hMem",          m_hMem);
    Printf(pri, "%s%15s:0x%x\n", linePrefix, "hSplitMem",     m_hSplitMem);
    Printf(pri, "%s%15s:0x%x\n", linePrefix, "hVirtMem",      m_hVirtMem);
    Printf(pri, "%s%15s:0x%x\n", linePrefix, "hTegraVirtMem", m_hTegraVirtMem);
    Printf(pri, "%s%15s:0x%x\n", linePrefix, "hCtxDmaGpu",    m_hCtxDmaGpu);
    Printf(pri, "%s%15s:0x%x\n", linePrefix, "hCtxDmaIso",    m_hCtxDmaIso);
    Printf(pri, "%s%15s:0x%x\n", linePrefix, "hFLA",          m_hFLA);

    Printf(pri, "%s%15s:0x%llx\n", linePrefix, "TegraVirtAddr",     m_TegraVirtAddr);
    Printf(pri, "%s%15s:0x%llx\n", linePrefix, "GpuVirtAddr",       m_GpuVirtAddr);
    Printf(pri, "%s%15s:0x%llx\n", linePrefix, "SplitGpuVirtAddr",  m_SplitGpuVirtAddr);
    Printf(pri, "%s%15s:0x%llx\n", linePrefix, "CtxDmaOffsetGpu",   m_CtxDmaOffsetGpu);
    Printf(pri, "%s%15s:0x%llx\n", linePrefix, "CtxDmaOffsetIso",   m_CtxDmaOffsetIso);
    Printf(pri, "%s%15s:0x%llx\n", linePrefix, "VidMemOffset",      m_VidMemOffset);
    Printf(pri, "%s%15s:0x%llx\n", linePrefix, "SplitVidMemOffset", m_SplitVidMemOffset);
    Printf(pri, "%s%15s:0x%x\n",   linePrefix, "hCdeVirtMem",       m_hCdeVirtMem);
    Printf(pri, "%s%15s:0x%llx\n", linePrefix, "CdeVirtAddr",       m_CdeVirtAddr);

    // m_hVirtMemGpuRemote, m_CtxDmaOffsetGpuRemote, & m_GpuVirtAddrRemote maps
    // should all be using the same tuples. So Iterate through a single map to
    // get the tuple and use it to access the rest of the data.
    map<RemoteMapping, LwRm::Handle>::const_iterator pData;
    for (pData=m_hVirtMemGpuRemote.begin(); pData != m_hVirtMemGpuRemote.end(); pData++)
    {
        RemoteMapping mapIndex(
            pData->first.m_LocSD,
            pData->first.m_RemD,
            pData->first.m_RemSD,
            pData->first.m_PeerId);
        Printf(pri, "%s RemoteMapping for:locSD:%d RemD:%d RemSD:%d PeerId:%d\n",
            linePrefix, pData->first.m_LocSD, pData->first.m_RemD, pData->first.m_RemSD,
            pData->first.m_PeerId);
        Printf(pri, "%s%15s:0x%x\n", linePrefix, "hVirtMemGpuRemote", pData->second);
        Printf(pri, "%s%15s:0x%llx\n", linePrefix, "CtxDmaOffsetGpuRemote",
            m_CtxDmaOffsetGpuRemote[mapIndex]);
        Printf(pri, "%s%15s:0x%llx\n", linePrefix, "GpuvirtAddrRemote",
            m_GpuVirtAddrRemote[mapIndex]);
    }
}

//-----------------------------------------------------------------------------
// Remap physical memory, it's needed if we want to change surface attr
RC Surface2D::ReMapPhysMemory()
{
    // For some case(eg.DMA loading), some surfaces are RO in perspective of HW,
    // so we have to modify the protect attribute, however a modification
    // in Surface2D side doesn't change the attribute in RM, so we need to
    // unmap the physical memory and re-map with new Surface2D attributes
    RC rc;
    UnmapPhysMemory(m_pLwRm);
    CHECK_RC(MapPhysMemory(m_pLwRm));

    return OK;
}

bool Surface2D::IsSurface422Compressed() const
{
    // Only chroma surface will be compressed in semiplanar formats
    switch (m_PlanesColorFormat)
    {
        case ColorUtils::Y8___U8V8_N422:
            if (m_ColorFormat == ColorUtils::U8V8)
            {
                return true;
            }
            break;
        case ColorUtils::Y10___U10V10_N422:
        case ColorUtils::Y12___U12V12_N422:
            if (m_ColorFormat == ColorUtils::R16_G16)
            {
                return true;
            }
        default:
            break;
    }

    // Packed YUV formats
    if (m_ColorFormat == ColorUtils::Y8_U8__Y8_V8_N422 ||
        m_ColorFormat == ColorUtils::U8_Y8__V8_Y8_N422)
    {
        return true;
    }

    return false;
}

bool Surface2D::IsSurface420Compressed() const
{
    switch (m_PlanesColorFormat)
    {
        case ColorUtils::Y8___V8U8_N420:
            if (m_ColorFormat == ColorUtils::U8V8)
            {
                return true;
            }
            break;
        case ColorUtils::Y10___V10U10_N420:
        case ColorUtils::Y12___V12U12_N420:
            if (m_ColorFormat == ColorUtils::R16_G16)
            {
                return true;
            }
            break;
        case ColorUtils::Y8___U8___V8_N420:
            if (m_ColorFormat == ColorUtils::U8 || m_ColorFormat == ColorUtils::V8)
            {
                return true;
            }
        default:
            break;
    }

    return false;
}

void Surface2D::GetSysMemAllocAttr(LW_MEMORY_ALLOCATION_PARAMS *pSysMemAllocParams)
{
    pSysMemAllocParams->type = this->GetType();

    switch (m_Location)
    {
        case Memory::Coherent:
        case Memory::CachedNonCoherent:
            pSysMemAllocParams->attr =  DRF_DEF(OS32, _ATTR, _LOCATION, _PCI)
                                        | DRF_DEF(OS32, _ATTR, _COHERENCY, _WRITE_BACK);
            break;
        case Memory::NonCoherent:
            pSysMemAllocParams->attr =  DRF_DEF(OS32, _ATTR, _LOCATION, _PCI)
                                        | DRF_DEF(OS32, _ATTR, _COHERENCY, _UNCACHED);
            break;
        default:
            MASSERT(!"Invalid Location");
            break;
    }

    pSysMemAllocParams->attr |=  m_PhysContig ? DRF_DEF(OS32, _ATTR, _PHYSICALITY, _CONTIGUOUS) :
                                                DRF_DEF(OS32, _ATTR, _PHYSICALITY, _NONCONTIGUOUS);

    switch (m_Layout)
    {
        case Pitch:
        case Tiled:
            pSysMemAllocParams->attr |= DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH);
            break;
        case BlockLinear:
            pSysMemAllocParams->attr |= DRF_DEF(OS32, _ATTR, _FORMAT, _BLOCK_LINEAR);
            break;
        case Swizzled:
            pSysMemAllocParams->attr |= DRF_DEF(OS32, _ATTR, _FORMAT, _SWIZZLED);
            break;
        default:
            MASSERT(!"Invalid Layout");
            break;
    }

    // TODO : RM is not handling _GPU_CACHEABLE_DEFAULT use case and expects a _GPU_CACHEABLE_NO
    pSysMemAllocParams->attr2 |= DRF_DEF(OS32, _ATTR2, _GPU_CACHEABLE, _NO);

    // WAR - 200675513
    // Force Memory type as Coherent for Displayable surface
    // RM is yet to implement the correct behavior for RMCTRL - LW2080_CTRL_CMD_BUS_GET_INFO
    // https://jirasw.lwpu.com/browse/MT-562
    if (m_Displayable)
    {
        pSysMemAllocParams->attr |= DRF_DEF(OS32, _ATTR, _LOCATION, _PCI) |
                                   DRF_DEF(OS32, _ATTR, _COHERENCY, _WRITE_COMBINE);

        pSysMemAllocParams->attr2 |= DRF_DEF(OS32, _ATTR2, _NISO_DISPLAY, _NO);
    }
    else
    {
        pSysMemAllocParams->attr |= DRF_DEF(OS32, _ATTR, _LOCATION, _PCI) |
                                    DRF_DEF(OS32, _ATTR, _COHERENCY, _WRITE_BACK);
        pSysMemAllocParams->attr2 |= DRF_DEF(OS32, _ATTR2, _NISO_DISPLAY, _YES);
    }

    pSysMemAllocParams->size = m_Size;
}

Memory::Location Surface2D::CheckForIndividualLocationOverride
(
    Memory::Location lwrrentLocation,
    GpuSubdevice *pGpuSubdevice
) const
{
#ifdef SIM_BUILD
    if ((g_IndividualLocationOverride.count(m_Name) > 0) &&
        (g_IndividualLocationOverride[m_Name] !=
            Surface2D::NO_LOCATION_OVERRIDE))
    {
        lwrrentLocation = static_cast<Memory::Location>(
            g_IndividualLocationOverride[m_Name]);
    }
#endif
    return lwrrentLocation;
}

//-----------------------------------------------------------------------------
// Surf2D JS reference counter. We noticed that there are many (arch) tests that
// declares a global Surf2D and they never get cleaned up before we do a
// shutdown - causing resource leaks. This is to track all the JS Surface 2D
// instances and have the option to free all instances before teardown.
namespace JSSurf2DTracker
{
    void AddToRefList(Surface2D *pSurf2D);
    void RemoveFromRefList(Surface2D *pSurf2D);
    set<Surface2D*> d_RefList;
    bool d_EnableGarbageCollector = false;
};

void JSSurf2DTracker::EnableGarbageCollector()
{
    d_EnableGarbageCollector = true;
}

RC JSSurf2DTracker::CollectGarbage()
{
    RC rc;

    if (!d_EnableGarbageCollector)
    {
        return OK;
    }

    Printf(Tee::PriLow, "starting JS Surface2D garbage collector\n");
    set<Surface2D*>::iterator it;
    for (it = d_RefList.begin(); it != d_RefList.end(); it++)
    {
        // detach surface2D from JS object
        JSContext *pCtx;
        JSObject *pObj;
        (*it)->GetJSCtxObj(&pCtx, &pObj);
        JS_SetPrivate(pCtx, pObj, NULL);
        // delete each remaining surf2D object (its destructor calls Free)
        delete *it;
    }
    d_RefList.clear();
    return rc;
}

void JSSurf2DTracker::AddToRefList(Surface2D *pSurf2D)
{
    MASSERT(pSurf2D);
    d_RefList.insert(pSurf2D);
}

void JSSurf2DTracker::RemoveFromRefList(Surface2D *pSurf2D)
{
    MASSERT(pSurf2D);
    d_RefList.erase(pSurf2D);
}

//-----------------------------------------------------------------------------
// JavaScript interface
static JSBool C_Surface2D_constructor(JSContext *cx,
                                      JSObject *obj,
                                      uintN argc,
                                      jsval *argv,
                                      jsval *rval)
{
    if(argc != 0)
    {
        JS_ReportError(cx, "Usage: new Surface2D");
        return JS_FALSE;
    }
    Surface2D *pClass = new Surface2D();
    MASSERT(pClass);
    pClass->SaveJSCtxObj(cx, obj);
    JSSurf2DTracker::AddToRefList(pClass);
    return JS_SetPrivate(cx, obj, pClass);
}
static void C_Surface2D_finalize(JSContext *cx, JSObject *obj)
{
    void *pvClass = JS_GetPrivate(cx, obj);
    if (pvClass)
    {
        Surface2D * pClass = (Surface2D*)pvClass;
        JSSurf2DTracker::RemoveFromRefList(pClass);
        delete pClass;
    }
}
static JSClass Surface2D_class =
{
    "Surface2D",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_Surface2D_finalize
};
static SObject Surface2D_Object
(
  "Surface2D",
  Surface2D_class,
  0,
  0,
  "2D surface helper class",
  C_Surface2D_constructor
);

C_(Surface2D_DebugFillOnAlloc);
static STest Surface2D_DebugFillOnAlloc
(
    Surface2D_Object,
    "DebugFillOnAlloc",
    C_Surface2D_DebugFillOnAlloc,
    1,
    "Enable filling all surfaces on allocation with a debug pattern."
);

C_(Surface2D_GpuVirtAddrHint);
static STest Surface2D_GpuVirtAddrHint
(
    Surface2D_Object,
    "GpuVirtAddrHint",
    C_Surface2D_GpuVirtAddrHint,
    2,
    "Set min address when mapping Gpu virtual address for surface."
);

C_(Surface2D_GpuVirtAddrHintMax);
static STest Surface2D_GpuVirtAddrHintMax
(
    Surface2D_Object,
    "GpuVirtAddrHintMax",
    C_Surface2D_GpuVirtAddrHintMax,
    2,
    "Set max address when mapping Gpu virtual address for surface."
);

C_(Surface2D_GpuPhysAddrHint);
static STest Surface2D_GpuPhysAddrHint
(
    Surface2D_Object,
    "GpuPhysAddrHint",
    C_Surface2D_GpuPhysAddrHint,
    2,
    "Set min address when mapping Gpu physical address for surface."
);

C_(Surface2D_GpuPhysAddrHintMax);
static STest Surface2D_GpuPhysAddrHintMax
(
    Surface2D_Object,
    "GpuPhysAddrHintMax",
    C_Surface2D_GpuPhysAddrHintMax,
    2,
    "Set max address when mapping Gpu physical address for surface."
);

C_(Surface2D_Alloc);
static STest Surface2D_Alloc
(
    Surface2D_Object,
    "Alloc",
    C_Surface2D_Alloc,
    1,
    "Allocate the surface"
);

C_(Surface2D_Free);
static STest Surface2D_Free
(
    Surface2D_Object,
    "Free",
    C_Surface2D_Free,
    0,
    "Free the surface"
);

C_(Surface2D_Map);
static STest Surface2D_Map
(
    Surface2D_Object,
    "Map",
    C_Surface2D_Map,
    0,
    "Create a CPU mapping of the surface"
);

C_(Surface2D_MapRect);
static STest Surface2D_MapRect
(
    Surface2D_Object,
    "MapRect",
    C_Surface2D_MapRect,
    0,
    "Create a CPU mapping of part of the surface"
);

C_(Surface2D_MapPart);
static STest Surface2D_MapPart
(
    Surface2D_Object,
    "MapPart",
    C_Surface2D_MapPart,
    0,
    "Create a CPU mapping of the surface"
);

C_(Surface2D_MapBroadcast);
static STest Surface2D_MapBroadcast
(
    Surface2D_Object,
    "MapBroadcast",
    C_Surface2D_MapBroadcast,
    0,
    "Depreciated: Create a broadcast CPU mapping of the surface"
);

C_(Surface2D_Unmap);
static STest Surface2D_Unmap
(
    Surface2D_Object,
    "Unmap",
    C_Surface2D_Unmap,
    0,
    "Destroy the CPU mapping of the surface"
);

C_(Surface2D_GetPixelOffset);
static SMethod Surface2D_GetPixelOffset
(
    Surface2D_Object,
    "GetPixelOffset",
    C_Surface2D_GetPixelOffset,
    0,
    "Get the offset in the surface of a given pixel (x,y)"
);

C_(Surface2D_GetCompTags);
static SMethod Surface2D_GetCompTags
(
    Surface2D_Object,
    "GetCompTags",
    C_Surface2D_GetCompTags,
    0,
    "Get the get compression tags"
);

C_(Surface2D_GetTagsOffsetPhys);
static SMethod Surface2D_GetTagsOffsetPhys
(
    Surface2D_Object,
    "GetTagsOffsetPhys",
    C_Surface2D_GetTagsOffsetPhys,
    0,
    "Get the get compression tags"
);

C_(Surface2D_GetTagsMin);
static SMethod Surface2D_GetTagsMin
(
    Surface2D_Object,
    "GetTagsMin",
    C_Surface2D_GetTagsMin,
    0,
    "tags min"
);

C_(Surface2D_GetTagsMax);
static SMethod Surface2D_GetTagsMax
(
    Surface2D_Object,
    "GetTagsMax",
    C_Surface2D_GetTagsMax,
    0,
    "tags max"
);

C_(Surface2D_FlushCpuCache);
static SMethod Surface2D_FlushCpuCache
(
    Surface2D_Object,
    "FlushCpuCache",
    C_Surface2D_FlushCpuCache,
    0,
    "Flush CPU cache"
);

C_(Surface2D_ReadPixel);
static SMethod Surface2D_ReadPixel
(
    Surface2D_Object,
    "ReadPixel",
    C_Surface2D_ReadPixel,
    0,
    "Read a pixel from the surface"
);

C_(Surface2D_GetPhysAddress);
static SMethod Surface2D_GetPhysAddress
(
    Surface2D_Object,
    "GetPhysAddress",
    C_Surface2D_GetPhysAddress,
    0,
    "Returns the physical address of a byte at a given surface offset"
);

C_(Surface2D_SetPhysContig);
static SMethod Surface2D_SetPhysContig
(
    Surface2D_Object,
    "SetPhysContig",
    C_Surface2D_SetPhysContig,
    0,
    "Set physically contiguous memory for the surface"
);

C_(Surface2D_SetAlignment);
static SMethod Surface2D_SetAlignment
(
    Surface2D_Object,
    "SetAlignment",
    C_Surface2D_SetAlignment,
    0,
    "Set the alignment"
);

C_(Surface2D_GetAllocSize);
static SMethod Surface2D_GetAllocSize
(
    Surface2D_Object,
    "GetAllocSize",
    C_Surface2D_GetAllocSize,
    0,
    "Returns total size allocated for the surface"
);

C_(Surface2D_GetPixelElements);
static SMethod Surface2D_GetPixelElements
(
    Surface2D_Object,
    "GetPixelElements",
    C_Surface2D_GetPixelElements,
    0,
    "Get number of elements in pixel"
);

C_(Surface2D_WritePixel);
static STest Surface2D_WritePixel
(
    Surface2D_Object,
    "WritePixel",
    C_Surface2D_WritePixel,
    0,
    "Write a pixel to the surface"
);

C_(Surface2D_FillPatternTable);
static STest Surface2D_FillPatternTable
(
    Surface2D_Object,
    "FillPatternTable",
    C_Surface2D_FillPatternTable,
    5,
    "Fill a surface with a pattern table from JS"
);

C_(Surface2D_Fill);
static STest Surface2D_Fill
(
    Surface2D_Object,
    "Fill",
    C_Surface2D_Fill,
    0,
    "Fill the entire surface with a value"
);

C_(Surface2D_FillRect);
static STest Surface2D_FillRect
(
    Surface2D_Object,
    "FillRect",
    C_Surface2D_FillRect,
    0,
    "Fill a rectangle of a surface with a value"
);

C_(Surface2D_Fill64);
static STest Surface2D_Fill64
(
    Surface2D_Object,
    "Fill64",
    C_Surface2D_Fill64,
    0,
    "Fill the entire surface with a 64-bit (4x16) color value"
);

C_(Surface2D_FillRect64);
static STest Surface2D_FillRect64
(
    Surface2D_Object,
    "FillRect64",
    C_Surface2D_FillRect64,
    0,
    "Fill a rectangle of a surface with a 64-bit (4x16) color value"
);

C_(Surface2D_FillPattern);
static STest Surface2D_FillPattern
(
    Surface2D_Object,
    "FillPattern",
    C_Surface2D_FillPattern,
    0,
    "Fill the entire surface with a standard pattern"
);

C_(Surface2D_FillPatternRect);
static STest Surface2D_FillPatternRect
(
    Surface2D_Object,
    "FillPatternRect",
    C_Surface2D_FillPatternRect,
    0,
    "Fill a subrectangle of the surface with a standard pattern"
);

C_(Surface2D_FillPatternChunk);
static STest Surface2D_FillPatternChunk
(
    Surface2D_Object,
    "FillPatternChunk",
    C_Surface2D_FillPatternChunk,
    0,
    "Fill the entire surface with a standard pattern, in arbitrarily-sized chunks"
);

C_(Surface2D_ApplyFp16GainOffset);
static STest Surface2D_ApplyFp16GainOffset
(
    Surface2D_Object,
    "ApplyFp16GainOffset",
    C_Surface2D_ApplyFp16GainOffset,
    6,
    "Apply the (float) gain/offset to an fp16 surface"
);

C_(Surface2D_WriteTga);
static STest Surface2D_WriteTga
(
    Surface2D_Object,
    "WriteTga",
    C_Surface2D_WriteTga,
    0,
    "Write the surface to disk as a TGA file"
);

C_(Surface2D_WriteTgaRect);
static STest Surface2D_WriteTgaRect
(
    Surface2D_Object,
    "WriteTgaRect",
    C_Surface2D_WriteTgaRect,
    0,
    "Write a subrectangle of the surface to disk as a TGA file"
);

C_(Surface2D_FillConstantOnGpu);
static STest Surface2D_FillConstantOnGpu
(
    Surface2D_Object,
    "FillConstantOnGpu",
    C_Surface2D_FillConstantOnGpu,
    1,
    "Compute CRC of surface"
);

C_(Surface2D_FillRandomOnGpu);
static STest Surface2D_FillRandomOnGpu
(
    Surface2D_Object,
    "FillRandomOnGpu",
    C_Surface2D_FillRandomOnGpu,
    1,
    "Compute CRC of surface"
);

C_(Surface2D_GetBlockedCRC);
static STest Surface2D_GetBlockedCRC
(
    Surface2D_Object,
    "GetBlockedCRC",
    C_Surface2D_GetBlockedCRC,
    3,
    "Compute CRC of surface"
);

C_(Surface2D_GetCRC);
static STest Surface2D_GetCRC
(
    Surface2D_Object,
    "GetCRC",
    C_Surface2D_GetCRC,
    0,
    "Compute CRC of surface"
);

C_(Surface2D_WritePng);
static STest Surface2D_WritePng
(
    Surface2D_Object,
    "WritePng",
    C_Surface2D_WritePng,
    0,
    "Write the surface to disk as a PNG file"
);

C_(Surface2D_WriteText);
static STest Surface2D_WriteText
(
    Surface2D_Object,
    "WriteText",
    C_Surface2D_WriteText,
    0,
    "Write the surface to disk as a text file"
);

C_(Surface2D_ReadTga);
static STest Surface2D_ReadTga
(
    Surface2D_Object,
    "ReadTga",
    C_Surface2D_ReadTga,
    0,
    "Read the surface from disk as a TGA file"
);

C_(Surface2D_ReadPng);
static STest Surface2D_ReadPng
(
    Surface2D_Object,
    "ReadPng",
    C_Surface2D_ReadPng,
    0,
    "Read the surface from disk as a PNG file"
);

C_(Surface2D_ReadRaw);
static STest Surface2D_ReadRaw
(
    Surface2D_Object,
    "ReadRaw",
    C_Surface2D_ReadRaw,
    0,
    "Read the surface from disk as a raw binary file"
);

C_(Surface2D_ReadSeurat);
static STest Surface2D_ReadSeurat
(
    Surface2D_Object,
    "ReadSeurat",
    C_Surface2D_ReadSeurat,
    0,
    "Read the surface from disk as a seurat file"
);

C_(Surface2D_ReadTagsTga);
static STest Surface2D_ReadTagsTga
(
    Surface2D_Object,
    "ReadTagsTga",
    C_Surface2D_ReadTagsTga,
    0,
    "Read the tag bit surface from disk as a TGA file"
);

C_(Surface2D_SetIndividualLocationOverride);
static SMethod Surface2D_SetIndividualLocationOverride
(
    Surface2D_Object,
    "SetIndividualLocationOverride",
    C_Surface2D_SetIndividualLocationOverride,
    2,
    "Set a location override for a surface with the given name."
);

// Note that these are global properties, or we'd collide with the Pitch
// property of Surface2D.
static SProperty Global_Pitch
(
    "Pitch",
    0,
    Surface2D::Pitch,
    0,
    0,
    JSPROP_READONLY,
    "Pitch surface layout"
);
static SProperty Global_Swizzled
(
    "Swizzled",
    0,
    Surface2D::Swizzled,
    0,
    0,
    JSPROP_READONLY,
    "Swizzled surface layout"
);
static SProperty Global_BlockLinear
(
    "BlockLinear",
    0,
    Surface2D::BlockLinear,
    0,
    0,
    JSPROP_READONLY,
    "Block linear surface layout"
);
static SProperty Global_Tiled
(
    "Tiled",
    0,
    Surface2D::Tiled,
    0,
    0,
    JSPROP_READONLY,
    "Tiled surface layout"
);

static SProperty Global_Flush
(
    "Flush",
    0,
    Surface2D::Flush,
    0,
    0,
    JSPROP_READONLY,
    "Flush flag"
);
static SProperty Global_Ilwalidate
(
    "Ilwalidate",
    0,
    Surface2D::Ilwalidate,
    0,
    0,
    JSPROP_READONLY,
    "Ilwalidate flag"
);
static SProperty Global_FlushAndIlwalidate
(
    "FlushAndIlwalidate",
    0,
    Surface2D::FlushAndIlwalidate,
    0,
    0,
    JSPROP_READONLY,
    "Flush and ilwalidate flag"
);

P_(Global_Get_g_Surface2DLocationOverride);
P_(Global_Set_g_Surface2DLocationOverride);
static SProperty Global_g_Surface2DLocationOverride
(
    "g_Surface2DLocationOverride",
    0,
    Surface2D::GetLocationOverride(),
    Global_Get_g_Surface2DLocationOverride,
    Global_Set_g_Surface2DLocationOverride,
    0,
    "Force all surfaces and pushbuffers to this location"
);

P_(Global_Get_g_Surface2DLocationOverrideSysmem);
P_(Global_Set_g_Surface2DLocationOverrideSysmem);
static SProperty Global_g_Surface2DLocationOverrideSysmem
(
    "g_Surface2DLocationOverrideSysmem",
    0,
    Surface2D::GetLocationOverrideSysmem(),
    Global_Get_g_Surface2DLocationOverrideSysmem,
    Global_Set_g_Surface2DLocationOverrideSysmem,
    0,
    "Force sysmem surfaces and pushbuffers to this location"
);

#define SURFACE2D_READWRITE_PROPERTY(JSType, CType, PropertyName, Method, HelpString) \
    P_(Surface2D_Get_##PropertyName); \
    P_(Surface2D_Set_##PropertyName); \
    static SProperty Surface2D_##PropertyName \
    ( \
        Surface2D_Object, \
        #PropertyName, \
        0, \
        0, \
        Surface2D_Get_##PropertyName, \
        Surface2D_Set_##PropertyName, \
        0, \
        HelpString \
    ); \
    P_(Surface2D_Get_##PropertyName) \
    { \
        MASSERT(pContext != 0); \
        MASSERT(pValue   != 0); \
        Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject); \
        if (!pSurface2D) \
        { \
            return JS_FALSE; \
        } \
        JavaScriptPtr()->ToJsval(pSurface2D->Get##Method(), pValue);    \
        return JS_TRUE; \
    } \
    P_(Surface2D_Set_##PropertyName) \
    { \
        MASSERT(pContext != 0); \
        MASSERT(pValue   != 0); \
        JSType Value; \
        if (OK != JavaScriptPtr()->FromJsval(*pValue, &Value)) \
        { \
            JS_ReportError(pContext, "Error writing Surface2D." #PropertyName); \
            return JS_FALSE; \
        } \
        Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject); \
        if (!pSurface2D) \
        { \
            return JS_FALSE; \
        } \
        pSurface2D->Set##Method((CType)Value); \
        return JS_TRUE; \
    }

#define SURFACE2D_READONLY_PROPERTY(PropertyName, Method, HelpString) \
    P_(Surface2D_Get_##PropertyName); \
    static SProperty Surface2D_##PropertyName \
    ( \
        Surface2D_Object, \
        #PropertyName, \
        0, \
        0, \
        Surface2D_Get_##PropertyName, \
        0, \
        JSPROP_READONLY, \
        HelpString \
    ); \
    P_(Surface2D_Get_##PropertyName) \
    { \
        MASSERT(pContext != 0); \
        MASSERT(pValue   != 0); \
        Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject); \
        if (!pSurface2D) \
        { \
            return JS_FALSE; \
        } \
        JavaScriptPtr()->ToJsval(pSurface2D->Get##Method(), pValue);    \
        return JS_TRUE; \
    }

SURFACE2D_READWRITE_PROPERTY(UINT32, UINT32,                Width,           Width,           "The width of the surface in pixels")
SURFACE2D_READWRITE_PROPERTY(UINT32, UINT32,                Height,          Height,          "The height of the surface in pixels")
SURFACE2D_READWRITE_PROPERTY(UINT32, UINT32,                Depth,           Depth,           "The depth of the surface in pixels")
SURFACE2D_READWRITE_PROPERTY(UINT32, UINT32,                ArraySize,       ArraySize,       "The depth of the surface in pixels")
SURFACE2D_READWRITE_PROPERTY(UINT32, ColorUtils::Format,    ColorFormat,     ColorFormat,     "Color format")
SURFACE2D_READWRITE_PROPERTY(UINT32, UINT32,                LogBlockWidth,   LogBlockWidth,   "log2(gobs per block in X)")
SURFACE2D_READWRITE_PROPERTY(UINT32, UINT32,                LogBlockHeight,  LogBlockHeight,  "log2(gobs per block in Y)")
SURFACE2D_READWRITE_PROPERTY(UINT32, UINT32,                LogBlockDepth,   LogBlockDepth,   "log2(gobs per block in Z)")
SURFACE2D_READWRITE_PROPERTY(UINT32, UINT32,                Pitch,           Pitch,           "The pitch of the surface in bytes")
SURFACE2D_READWRITE_PROPERTY(UINT32, Memory::Location,      Location,        Location,        "Memory space")
SURFACE2D_READWRITE_PROPERTY(UINT32, Memory::Protect,       Protect,         Protect,         "Memory permissions")
SURFACE2D_READWRITE_PROPERTY(UINT32, Memory::AddressModel,  AddressModel,    AddressModel,    "The address model (paging vs. segmentation) of the surface")
SURFACE2D_READWRITE_PROPERTY(UINT32, Surface2D::Layout,     Layout,          Layout,          "Memory layout formula")
SURFACE2D_READWRITE_PROPERTY(UINT32, ColorUtils::YCrCbType, YCrCbType,       YCrCbType,       "YCrCbType")
SURFACE2D_READWRITE_PROPERTY(bool,   bool,                  Tiled,           Tiled,           "Enable tiling for this surface")
SURFACE2D_READWRITE_PROPERTY(bool,   bool,                  Compressed,      Compressed,      "Enable compression for this surface")
SURFACE2D_READWRITE_PROPERTY(UINT32, UINT32,                ComptagStart,    ComptagStart,    "Percentage into the surface to start compression")
SURFACE2D_READWRITE_PROPERTY(UINT32, UINT32,                ComptagCovMin,   ComptagCovMin,   "Minimum percentage of the surface to be compressed")
SURFACE2D_READWRITE_PROPERTY(UINT32, UINT32,                ComptagCovMax,   ComptagCovMax,   "Maximum percentage of the surface to be compressed")
SURFACE2D_READWRITE_PROPERTY(bool,   bool,                  Displayable,     Displayable,     "Surface must obey restrictions on displayable surfaces")
SURFACE2D_READWRITE_PROPERTY(UINT32, UINT32,                PteKind,         PteKind,         "The page kind of the surface")
SURFACE2D_READWRITE_PROPERTY(UINT32, UINT32,                PartStride,      PartStride,      "The page kind of the surface")
SURFACE2D_READWRITE_PROPERTY(bool,   bool,                  PhysContig,      PhysContig,      "Make surface's physical memory contiguous")
SURFACE2D_READWRITE_PROPERTY(bool,   bool,                  AlignHostPage,   AlignHostPage,   "Align surface's memory to host page")
SURFACE2D_READWRITE_PROPERTY(UINT32, UINT32,                Alignment,       Alignment,       "Memory alignment in bytes")
SURFACE2D_READWRITE_PROPERTY(UINT32, UINT32,                VirtAlignment,   VirtAlignment,   "Virtual memory alignment override in bytes")
SURFACE2D_READWRITE_PROPERTY(UINT32, UINT32,                PageSize,        PageSize,        "Page size in KB")
SURFACE2D_READWRITE_PROPERTY(bool,   bool,                  MemAttrsInCtxDma,MemAttrsInCtxDma,"Put memory attributes in ctxdma")
SURFACE2D_READWRITE_PROPERTY(bool,   bool,                  VaReverse,       VaReverse,       "Reverse mapping in virtual address space")
SURFACE2D_READWRITE_PROPERTY(UINT32, UINT32,                MaxCoalesce,     MaxCoalesce,     "PTE coalesce level")
SURFACE2D_READWRITE_PROPERTY(bool,   bool,                  Split,           Split,           "Enable split surface")
SURFACE2D_READWRITE_PROPERTY(UINT32, Memory::Location,      SplitLocation,   SplitLocation,   "Placement of the second half of a split surface")
SURFACE2D_READWRITE_PROPERTY(bool,   bool,                  VideoProtected,  VideoProtected,  "Surface in Video Protected Region or not")
SURFACE2D_READWRITE_PROPERTY(bool,   bool,                  AcrRegion1,      AcrRegion1,      "Surface in ACR1 or not")
SURFACE2D_READWRITE_PROPERTY(bool,   bool,                  AcrRegion2,      AcrRegion2,      "Surface in ACR2 or not")
SURFACE2D_READWRITE_PROPERTY(bool,   bool,                  RouteDispRM,     RouteDispRM,     "Enable import memory and route display surface to display RM")

SURFACE2D_READONLY_PROPERTY(AllocWidth,    AllocWidth,    "The width of the surface in pixels, counting extra padding")
SURFACE2D_READONLY_PROPERTY(AllocHeight,   AllocHeight,   "The height of the surface in pixels, counting extra padding")
SURFACE2D_READONLY_PROPERTY(AllocDepth,    AllocDepth,    "The depth of the surface in pixels, counting extra padding")
SURFACE2D_READONLY_PROPERTY(GobWidth,      GobWidth,      "The width of the surface in gobs, counting extra padding")
SURFACE2D_READONLY_PROPERTY(GobHeight,     GobHeight,     "The height of the surface in gobs, counting extra padding")
SURFACE2D_READONLY_PROPERTY(GobDepth,      GobDepth,      "The depth of the surface in gobs, counting extra padding")
SURFACE2D_READONLY_PROPERTY(BlockWidth,    BlockWidth,    "The width of the surface in blocks, counting extra padding")
SURFACE2D_READONLY_PROPERTY(BlockHeight,   BlockHeight,   "The height of the surface in blocks, counting extra padding")
SURFACE2D_READONLY_PROPERTY(BlockDepth,    BlockDepth,    "The depth of the surface in blocks, counting extra padding")
SURFACE2D_READONLY_PROPERTY(Size,          Size,          "The size of the surface in bytes")
SURFACE2D_READONLY_PROPERTY(hCtxDma,       CtxDmaHandle,  "The context DMA handle for the surface")
SURFACE2D_READONLY_PROPERTY(Offset,        Offset,        "The offset in the context DMA for this surface")
SURFACE2D_READONLY_PROPERTY(PhysAddress,   PhysAddress,   "The physyical address for this surface's mapping")
SURFACE2D_READONLY_PROPERTY(BytesPerPixel, BytesPerPixel, "# of bytes per pixel")
SURFACE2D_READONLY_PROPERTY(VidMemOffset,  VidMemOffset,  "The offset into video memory")
SURFACE2D_READONLY_PROPERTY(NumParts,      NumParts,      "The number of parts of the surface")
SURFACE2D_READONLY_PROPERTY(SplitVidMemOffset,SplitVidMemOffset,"The offset into video memory of the second part of a split surface")
SURFACE2D_READONLY_PROPERTY(VirtAddress,   VirtAddress,   "Virtual address on the GPU")

static RC ReturnErrorIfNull(void * pVariable)
{
    if (!pVariable)
    {
        Printf(Tee::PriNormal,
               "JS attempts to use a Surface2D after it's been freed\n");
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

C_(Surface2D_DebugFillOnAlloc)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    const char usage[] = "Usage: Surface2D.DebugFillOnAlloc(patternWord)";
    UINT32 fillWord = 0;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &fillWord)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    g_DebugFillOnAlloc = true;
    g_DebugFillWord = fillWord;
    RETURN_RC(OK);
}

C_(Surface2D_GpuVirtAddrHint)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJs;
    const char usage[] = "Usage: Surface2D.GpuVirtAddrHint(MinAddrHigh32, MinAddrLow32)";

    UINT32 MinAddrLow, MinAddrHigh;

    if ((NumArguments != 2) ||
        (OK != pJs->FromJsval(pArguments[0], &MinAddrHigh)) ||
        (OK != pJs->FromJsval(pArguments[1], &MinAddrLow)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext,pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));

    pSurface2D->SetGpuVirtAddrHint((UINT64)MinAddrHigh << 32 | (UINT64)MinAddrLow);

    RETURN_RC(OK);
}

C_(Surface2D_GpuVirtAddrHintMax)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJs;
    const char usage[] = "Usage: Surface2D.GpuVirtAddrHintMax(MaxAddrHigh32, MaxAddrLow32)";

    UINT32 MaxAddrLow, MaxAddrHigh;

    if ((NumArguments != 2) ||
        (OK != pJs->FromJsval(pArguments[0], &MaxAddrHigh)) ||
        (OK != pJs->FromJsval(pArguments[1], &MaxAddrLow)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext,pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));

    pSurface2D->SetGpuVirtAddrHintMax((UINT64)MaxAddrHigh << 32 | (UINT64)MaxAddrLow);

    RETURN_RC(OK);
}

C_(Surface2D_GpuPhysAddrHint)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJs;
    const char usage[] = "Usage: Surface2D.GpuPhysAddrHint(MinAddrHigh32, MinAddrLow32)";

    UINT32 MinAddrLow, MinAddrHigh;

    if ((NumArguments != 2) ||
        (OK != pJs->FromJsval(pArguments[0], &MinAddrHigh)) ||
        (OK != pJs->FromJsval(pArguments[1], &MinAddrLow)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext,pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));

    pSurface2D->SetGpuPhysAddrHint((UINT64)MinAddrHigh << 32 | (UINT64)MinAddrLow);

    RETURN_RC(OK);
}

C_(Surface2D_GpuPhysAddrHintMax)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJs;
    const char usage[] = "Usage: Surface2D.GpuPhysAddrHintMax(MaxAddrHigh32, MaxAddrLow32)";

    UINT32 MaxAddrLow, MaxAddrHigh;

    if ((NumArguments != 2) ||
        (OK != pJs->FromJsval(pArguments[0], &MaxAddrHigh)) ||
        (OK != pJs->FromJsval(pArguments[1], &MaxAddrLow)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext,pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));

    pSurface2D->SetGpuPhysAddrHintMax((UINT64)MaxAddrHigh << 32 | (UINT64)MaxAddrLow);

    RETURN_RC(OK);
}

C_(Surface2D_Alloc)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    const char usage[] = "Usage: Surface2D.Alloc(JsGpuDevice)";
    JSObject * pJsObj;

    if ( ((NumArguments == 1) &&
          (OK != pJavaScript->FromJsval(pArguments[0], &pJsObj))) ||
         (NumArguments > 1))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    GpuDevice * pGpuDev;
    if (NumArguments == 1)
    {
        JsGpuDevice *pJsGpuDevice;
        if ((pJsGpuDevice = JS_GET_PRIVATE(JsGpuDevice,
                                           pContext,
                                           pJsObj,
                                           "GpuDevice")) != 0)
        {
            pGpuDev = pJsGpuDevice->GetGpuDevice();
        }
        else
        {
            JS_ReportError(pContext, "Invalid JsGpuDevice\n");
            return JS_FALSE;
        }
    }
    else
    {
        static Deprecation depr("Surface2D.Alloc", "3/30/2019",
                                "Alloc must specify a GpuDevice to allocate on.\n"
                                "Defaulting to the first GpuDevice.\n");
        if (!depr.IsAllowed(pContext))
            return JS_FALSE;

        GpuDevMgr* pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
        pGpuDev = pGpuDevMgr->GetFirstGpuDevice();
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    RETURN_RC(pSurface2D->Alloc(pGpuDev));
}

C_(Surface2D_Free)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: Surface2D.Free()");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    pSurface2D->Free();
    RETURN_RC(OK);
}

C_(Surface2D_Map)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV;
    JavaScriptPtr pJavaScript;

    // Allow default subdev of 0 in most cases.
    // Require explicit subdev for SLI FB surfaces.

    if (((NumArguments == 1) &&
         (OK != pJavaScript->FromJsval(pArguments[0], &subdev)))
        ||
        (NumArguments > 1))
    {
        JS_ReportError(pContext, "Usage: Surface2D.Map(subdev)");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    RETURN_RC(pSurface2D->Map(subdev));
}

C_(Surface2D_MapRect)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    if (NumArguments > 5 || NumArguments < 4)
    {
        JS_ReportError(pContext, "Usage: Surface2D.MapRect(x, y, Width, Height, subdev)");
        return JS_FALSE;
    }

    UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV;
    UINT32 x, y, width, height;
    if ((NumArguments < 4) ||
        (NumArguments > 5) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &x)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &y)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &width)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &height)) ||
        ((NumArguments == 5) &&
         (OK != pJavaScript->FromJsval(pArguments[4], &subdev))))
    {
        JS_ReportError(pContext, "Usage: Surface2D.MapRect(x, y, Width, Height, subdev)");
        return JS_FALSE;
    }

    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    RETURN_RC(pSurface2D->MapRect(x, y, width, height, subdev));
}

C_(Surface2D_MapPart)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    UINT32 part = 0;
    UINT32 subdev = 0;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &part)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &subdev)))
    {
        JS_ReportError(pContext, "Usage: Surface2D.MapPart(part, subdev)");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    RETURN_RC(pSurface2D->MapPart(part, subdev));
}

C_(Surface2D_MapBroadcast)
{
    MASSERT(pContext != 0);
    JS_ReportError(pContext, "Surface2D.MapBroadcast() is depreciated");
    return JS_FALSE;
}

C_(Surface2D_Unmap)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: Surface2D.Unmap()");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    pSurface2D->Unmap();
    RETURN_RC(OK);
}

C_(Surface2D_GetPixelOffset)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    UINT32 x, y;
    JavaScriptPtr pJavaScript;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &x)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &y)))
    {
        JS_ReportError(pContext, "Usage: Surface2D.GetPixelOffset(x, y)");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    if (OK != pJavaScript->ToJsval(pSurface2D->GetPixelOffset(x,y), pReturlwalue))
    {
        JS_ReportError(pContext, "Unable to colwert return value: Surface2D.GetPixelOffset(x, y)");
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_(Surface2D_ReadPixel)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    UINT32 x, y;
    UINT32 pixel;
    JavaScriptPtr pJavaScript;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &x)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &y)))
    {
        JS_ReportError(pContext, "Usage: Surface2D.ReadPixel(x, y)");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    pixel = pSurface2D->ReadPixel(x, y);

    // Set the return value.
    *pReturlwalue = JSVAL_NULL;
    if (OK != pJavaScript->ToJsval((UINT64) pixel, pReturlwalue))
    {
       JS_ReportError(pContext, "Error oclwrred in Surface2D.ReadPixel (...)");
       *pReturlwalue = JSVAL_NULL;
       return JS_FALSE;
    }

    // Success.
    return JS_TRUE;
}

C_(Surface2D_GetPhysAddress)
{
    JavaScriptPtr pJavaScript;
    RC rc;
    UINT32 offset;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    PHYSADDR DevicePhysicalAddress = 0;

    if (OK != pJavaScript->FromJsval(pArguments[0], &offset))
    {
        JS_ReportError(pContext, "Usage: Surface2D.GetPhysAddress(offset)");
    }

    C_CHECK_RC(pSurface2D->GetPhysAddress(offset, &DevicePhysicalAddress));
    // Set the return value.
    *pReturlwalue = JSVAL_NULL;
    if (OK != pJavaScript->ToJsval((UINT64) DevicePhysicalAddress, pReturlwalue))
    {
       JS_ReportError(pContext, "Unable to colwert output value: Surface2D.GetPhysAddress(offset)");
       *pReturlwalue = JSVAL_NULL;
       return JS_FALSE;
    }
    // Success.
    return JS_TRUE;
}

C_(Surface2D_SetPhysContig)
{
    JavaScriptPtr pJavaScript;
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));

    bool val;
    if (OK != pJavaScript->FromJsval(pArguments[0], &val))
    {
        JS_ReportError(pContext, "Usage: Surface2D.SetPhyscontig(val)");
        return JS_FALSE;
    }
    pSurface2D->SetPhysContig(val);

    // Success.
    return JS_TRUE;
}

C_(Surface2D_SetAlignment)
{
    JavaScriptPtr pJavaScript;
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));

    UINT32 alignment;
    if (OK != pJavaScript->FromJsval(pArguments[0], &alignment))
    {
        JS_ReportError(pContext, "Usage: Surface2D.SetAlignment(alignment)");
        return JS_FALSE;
    }
    pSurface2D->SetAlignment(alignment);

    return JS_TRUE;
}

C_(Surface2D_GetAllocSize)
{
    JavaScriptPtr pJavaScript;
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));

    UINT64 totalSize = pSurface2D->GetAllocSize();

    // Set the return value.
    *pReturlwalue = JSVAL_NULL;
    if (OK != pJavaScript->ToJsval(totalSize, pReturlwalue))
    {
       JS_ReportError(pContext, "Unable to colwert return value : Surface2D.GetAllocSize()");
       *pReturlwalue = JSVAL_NULL;
       return JS_FALSE;
    }

    // Success.
    return JS_TRUE;
}

C_(Surface2D_WritePixel)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    UINT32 x, y, Value;
    JavaScriptPtr pJavaScript;
    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &x)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &y)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Value)))
    {
        JS_ReportError(pContext, "Usage: Surface2D.WritePixel(x, y, Value)");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    pSurface2D->WritePixel(x, y, Value);
    RETURN_RC(OK);
}

C_(Surface2D_FillPatternTable)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    UINT32 x, y, Repetitions;
    JsArray JsPatternArray;
    JavaScriptPtr pJavaScript;
    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &x)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &y)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &JsPatternArray)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &Repetitions)))
    {
        JS_ReportError(pContext, "Usage: Surface2D.FillPatternTable(x, y, Array, Repetitions)");
        return JS_FALSE;
    }

    // 32 bit array from JS
    vector<UINT32> pattern(JsPatternArray.size());
    for(size_t idx = 0; idx < JsPatternArray.size(); idx++)
    {
        if(OK != pJavaScript->FromJsval(JsPatternArray[idx], &(pattern[idx])))
        {
            JS_ReportError(pContext, "cannot read from pattern table\n");
            return JS_FALSE;
        }
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    pSurface2D->FillPatternTable(x, y, pattern.data(), (UINT32)JsPatternArray.size(), Repetitions);

    RETURN_RC(OK);
}

C_(Surface2D_Fill)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    UINT32 Value;
    UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV;
    JavaScriptPtr pJavaScript;

    if ((NumArguments < 1) ||
        (NumArguments > 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Value)) ||
        ((NumArguments == 2) &&
         (OK != pJavaScript->FromJsval(pArguments[1], &subdev))))
    {
        JS_ReportError(pContext, "Usage: Surface2D.Fill(Value, subdev)");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    RETURN_RC(pSurface2D->Fill(Value, subdev));
}

C_(Surface2D_FillRect)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    UINT32 x, y, Width, Height, Value;
    UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV;
    JavaScriptPtr pJavaScript;

    if ((NumArguments < 5) ||
        (NumArguments > 6) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &x)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &y)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Width)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &Height)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &Value)) ||
        ((NumArguments == 6) &&
         (OK != pJavaScript->FromJsval(pArguments[5], &subdev))))
    {
        JS_ReportError(pContext, "Usage: Surface2D.Fill(x, y, Width, Height, Value, subdev)");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    RETURN_RC(pSurface2D->FillRect(x, y, Width, Height, Value, subdev));
}

C_(Surface2D_Fill64)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    UINT16 r, g, b, a;
    UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV;
    JavaScriptPtr pJavaScript;

    if ((NumArguments < 4) ||
        (NumArguments > 5) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &r)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &g)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &b)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &a)) ||
        ((NumArguments == 5) &&
         (OK != pJavaScript->FromJsval(pArguments[4], &subdev))))
    {
        JS_ReportError(pContext, "Usage: Surface2D.Fill64(r, g, b, a, subdev)");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    RETURN_RC(pSurface2D->Fill64(r, g, b, a, subdev));
}

C_(Surface2D_FillRect64)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    UINT32 x, y, Width, Height;
    UINT16 r, g, b, a;
    UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV;
    JavaScriptPtr pJavaScript;

    // Allow default subdev of 0 in most cases.
    // Require explicit subdev for SLI FB surfaces.

    if ((NumArguments < 8) ||
        (NumArguments > 9) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &x)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &y)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Width)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &Height)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &r)) ||
        (OK != pJavaScript->FromJsval(pArguments[5], &g)) ||
        (OK != pJavaScript->FromJsval(pArguments[6], &b)) ||
        (OK != pJavaScript->FromJsval(pArguments[7], &a)) ||
        ((NumArguments == 9) &&
         (OK != pJavaScript->FromJsval(pArguments[8], &subdev))))
    {
        JS_ReportError(pContext, "Usage: Surface2D.FillRect64(x, y, Width, Height, r, g, b, a, subdev)");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    RETURN_RC(pSurface2D->FillRect64(x, y, Width, Height, r, g, b, a, subdev));
}

C_(Surface2D_FillPattern)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    string PatternType, PatternStyle, PatternDir;
    UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV;
    JavaScriptPtr pJavaScript;

    // Allow default subdev of 0 in most cases.
    // Require explicit subdev for SLI FB surfaces.

    if ((NumArguments < 3) ||
        (NumArguments > 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &PatternType)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &PatternStyle)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &PatternDir)) ||
        ((NumArguments == 4) &&
         (OK != pJavaScript->FromJsval(pArguments[3], &subdev))))
    {
        JS_ReportError(pContext, "Usage: Surface2D.FillPattern(PatternType, PatternStyle, PatternDir, subdev)");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    RETURN_RC(pSurface2D->FillPattern(1, 1,
                                      PatternType.c_str(),
                                      PatternStyle.c_str(),
                                      PatternDir.c_str(),
                                      subdev));
}

C_(Surface2D_FillPatternRect)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    UINT32 x, y, Width, Height;
    string PatternType, PatternStyle, PatternDir;
    UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV;
    JavaScriptPtr pJavaScript;

    // Allow default subdev of 0 in most cases.
    // Require explicit subdev for SLI FB surfaces.

    if ((NumArguments < 7) ||
        (NumArguments > 8) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &x)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &y)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Width)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &Height)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &PatternType)) ||
        (OK != pJavaScript->FromJsval(pArguments[5], &PatternStyle)) ||
        (OK != pJavaScript->FromJsval(pArguments[6], &PatternDir)) ||
        ((NumArguments == 8) &&
         (OK != pJavaScript->FromJsval(pArguments[7], &subdev))))
    {
        JS_ReportError(pContext, "Usage: Surface2D.FillPattern(X, Y, Width, Height, PatternType, PatternStyle, PatternDir, subdev)");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    RETURN_RC(pSurface2D->FillPatternRect(x, y,
                                          Width, Height,
                                          1, 1,
                                          PatternType.c_str(),
                                          PatternStyle.c_str(),
                                          PatternDir.c_str(),
                                          subdev));
}

C_(Surface2D_FillPatternChunk)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    UINT32 ChunkWidth, ChunkHeight;
    string PatternType, PatternStyle, PatternDir;
    UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV;
    JavaScriptPtr pJavaScript;

    // Allow default subdev of 0 in most cases.
    // Require explicit subdev for SLI FB surfaces.

    if ((NumArguments < 5) ||
        (NumArguments > 6) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &ChunkWidth)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &ChunkHeight)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &PatternType)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &PatternStyle)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &PatternDir)) ||
        ((NumArguments == 6) &&
         (OK != pJavaScript->FromJsval(pArguments[5], &subdev))))
    {
        JS_ReportError(pContext, "Usage: Surface2D.FillPatternChunk(ChunkWidth, ChunkHeight, PatternType, PatternStyle, PatternDir, subdev)");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    RETURN_RC(pSurface2D->FillPattern(ChunkWidth, ChunkHeight,
                                      PatternType.c_str(),
                                      PatternStyle.c_str(),
                                      PatternDir.c_str(),
                                      subdev));
}

C_(Surface2D_ApplyFp16GainOffset)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    UINT32        subdev = Gpu::UNSPECIFIED_SUBDEV;
    FLOAT64       gainRed, offsetRed;
    FLOAT64       gainGreen, offsetGreen;
    FLOAT64       gainBlue, offsetBlue;
    JavaScriptPtr pJavaScript;

    if ((NumArguments != 6) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &gainRed)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &offsetRed)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &gainGreen)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &offsetGreen)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &gainBlue)) ||
        (OK != pJavaScript->FromJsval(pArguments[5], &offsetBlue)))
    {
        JS_ReportError(pContext, "Usage: Surface2D.ApplyFp16GainOffset("
                                 "gainRed, offsetRed, gainGreen, offsetGreen,"
                                 "gainBlue, offsetGreen)");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    pSurface2D->ApplyFp16GainOffset((float)gainRed, (float)offsetRed,
                                    (float)gainGreen, (float)offsetGreen,
                                    (float)gainBlue, (float)offsetBlue, subdev);
    RETURN_RC(OK);
}

C_(Surface2D_WriteTga)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    string FileName;
    UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV;
    JavaScriptPtr pJavaScript;

    if ((NumArguments < 1) ||
        (NumArguments > 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &FileName)) ||
        ((NumArguments == 2) &&
         (OK != pJavaScript->FromJsval(pArguments[1], &subdev))))
    {
        JS_ReportError(pContext, "Usage: Surface2D.WriteTga(FileName, subdev)");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    RETURN_RC(pSurface2D->WriteTga(FileName.c_str(), subdev));
}

C_(Surface2D_WriteTgaRect)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    UINT32 x, y, Width, Height;
    string FileName;
    UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV;
    JavaScriptPtr pJavaScript;

    if ((NumArguments < 5) ||
        (NumArguments > 6) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &x)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &y)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Width)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &Height)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &FileName)) ||
        ((NumArguments == 6) &&
         (OK != pJavaScript->FromJsval(pArguments[5], &subdev))))
    {
        JS_ReportError(pContext, "Usage: Surface2D.WriteTgaRect(x, y, Width, Height, FileName, subdev)");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    RETURN_RC(pSurface2D->WriteTgaRect(x, y, Width, Height, FileName.c_str(), subdev));
}

C_(Surface2D_GetCRC)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JSObject * pReturnObj = 0;
    UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV;
    JavaScriptPtr pJavaScript;
    if ((NumArguments < 1) ||
        (NumArguments > 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &pReturnObj)) ||
        ((NumArguments == 2) &&
         (OK != pJavaScript->FromJsval(pArguments[1], &subdev))))
    {
        JS_ReportError(pContext, "Usage: Surface2D.GetCRC(CRCArray, [subdev])");
        return JS_FALSE;
    }

    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    vector<UINT32> crc;
    C_CHECK_RC(pSurface2D->GetCRC(&crc, subdev));

    for (UINT32 i=0; i < crc.size(); i++)
    {
        C_CHECK_RC(pJavaScript->SetElement(pReturnObj, i, crc[i]));
    }
    RETURN_RC(OK);
}

C_(Surface2D_FillConstantOnGpu)
{
    JavaScriptPtr pJavaScript;
    RC rc;
    UINT32 constant;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &constant)))
    {
        JS_ReportError(pContext, "Usage: Surface2D.FillConstantOnGpu(constant)");
    }
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    RETURN_RC(pSurface2D->FillConstantOnGpu(constant));
}

C_(Surface2D_FillRandomOnGpu)
{
    JavaScriptPtr pJavaScript;
    RC rc;
    UINT32 seed;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &seed)))
    {
        JS_ReportError(pContext, "Usage: Surface2D.FillRandomOnGpu(seed)");
    }
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    RETURN_RC(pSurface2D->FillRandomOnGpu(seed));
}

C_(Surface2D_GetBlockedCRC)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);
    UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV;
    UINT32 mask = 0;
    UINT32 gpuLimit = 0;

    JSObject * pReturnObj = 0;
    JavaScriptPtr pJavaScript;
    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &subdev)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &gpuLimit)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &mask)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &pReturnObj)))
    {
        JS_ReportError(pContext, "Usage: Surface2D.GetBlockedCRC(subdev, GPU limit, mask, CRCArray)");
        return JS_FALSE;
    }

    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    const UINT32 surfSizeMB = UNSIGNED_CAST(UINT32, pSurface2D->GetAllocSize()/(1024 * 1024));

    if (mask != 0xF)    //need individual components's CRC
    {
        vector<UINT32> crc;

        if (pSurface2D->GetLocation() == Memory::Fb && surfSizeMB >= gpuLimit)
            C_CHECK_RC(pSurface2D->GetBlockedCRCOnGpuPerComponent(&crc));
        else
            C_CHECK_RC(pSurface2D->GetBlockedCRCOnHostPerComponent(subdev, &crc));

        for (UINT32 i=0; i < crc.size(); i++)
        {
            C_CHECK_RC(pJavaScript->SetElement(pReturnObj, i, crc[i]));
        }
    }
    else
    {
        UINT32 crc = 0;
        if (pSurface2D->GetLocation() == Memory::Fb && surfSizeMB >= gpuLimit)
            C_CHECK_RC(pSurface2D->GetBlockedCRCOnGpu(&crc));
        else
            C_CHECK_RC(pSurface2D->GetBlockedCRCOnHost(subdev, &crc));

        C_CHECK_RC(pJavaScript->SetElement(pReturnObj, 0, crc));
    }
    RETURN_RC(OK);
}

C_(Surface2D_GetPixelElements)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: Surface2D.GetPixelElements()");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    *pReturlwalue = INT_TO_JSVAL(pSurface2D->GetElemsPerPixel());
    return JS_TRUE;
}

C_(Surface2D_WritePng)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    string FileName;
    UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV;
    JavaScriptPtr pJavaScript;

    if ((NumArguments < 1) ||
        (NumArguments > 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &FileName)) ||
        ((NumArguments == 2) &&
         (OK != pJavaScript->FromJsval(pArguments[1], &subdev))))
    {
        JS_ReportError(pContext, "Usage: Surface2D.WritePng(FileName, subdev)");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    RETURN_RC(pSurface2D->WritePng(FileName.c_str(), subdev));
}

C_(Surface2D_WriteText)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    string FileName;
    UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV;
    JavaScriptPtr pJavaScript;

    if ((NumArguments < 1) ||
        (NumArguments > 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &FileName)) ||
        ((NumArguments == 2) &&
         (OK != pJavaScript->FromJsval(pArguments[1], &subdev))))
    {
        JS_ReportError(pContext, "Usage: Surface2D.WriteText(FileName, subdev)");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    RETURN_RC(pSurface2D->WriteText(FileName.c_str(), subdev));
}

C_(Surface2D_ReadTga)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    string FileName;
    UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV;
    JavaScriptPtr pJavaScript;

    if ((NumArguments < 1) ||
        (NumArguments > 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &FileName)) ||
        ((NumArguments == 2) &&
         (OK != pJavaScript->FromJsval(pArguments[1], &subdev))))
    {
        JS_ReportError(pContext, "Usage: Surface2D.ReadTga(FileName, subdev)");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    RETURN_RC(pSurface2D->ReadTga(FileName.c_str(), subdev));
}

C_(Surface2D_ReadPng)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    string FileName;
    UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV;
    JavaScriptPtr pJavaScript;

    if ((NumArguments < 1) ||
        (NumArguments > 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &FileName)) ||
        ((NumArguments == 2) &&
         (OK != pJavaScript->FromJsval(pArguments[1], &subdev))))
    {
        JS_ReportError(pContext, "Usage: Surface2D.ReadPng(FileName, subdev)");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    RETURN_RC(pSurface2D->ReadPng(FileName.c_str(), subdev));
}

C_(Surface2D_ReadRaw)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    string FileName;
    UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV;
    JavaScriptPtr pJavaScript;

    if ((NumArguments < 1) ||
        (NumArguments > 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &FileName)) ||
        ((NumArguments == 2) &&
         (OK != pJavaScript->FromJsval(pArguments[1], &subdev))))
    {
        JS_ReportError(pContext, "Usage: Surface2D.ReadRaw(FileName, subdev)");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    RETURN_RC(pSurface2D->ReadRaw(FileName.c_str(), subdev));
}

C_(Surface2D_ReadSeurat)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    string FileName;
    UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV;
    JavaScriptPtr pJavaScript;

    if ((NumArguments < 1) ||
        (NumArguments > 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &FileName)) ||
        ((NumArguments == 2) &&
         (OK != pJavaScript->FromJsval(pArguments[1], &subdev))))
    {
        JS_ReportError(pContext, "Usage: Surface2D.ReadSeurat(FileName, subdev)");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    RETURN_RC(pSurface2D->ReadSeurat(FileName.c_str(), subdev));
}

C_(Surface2D_ReadTagsTga)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    string FileName;
    JavaScriptPtr pJavaScript;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &FileName)))
    {
        JS_ReportError(pContext, "Usage: Surface2D.ReadTagsTga(FileName)");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    RETURN_RC(pSurface2D->ReadTagsTga(FileName.c_str()));
}

// -----------------------------------------------------------
C_(Surface2D_GetCompTags)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: Surface2D.GetCompTags()");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    *pReturlwalue = INT_TO_JSVAL(pSurface2D->GetCompTags());
    return JS_TRUE;
}

C_(Surface2D_GetTagsOffsetPhys)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: Surface2D.GetTagsOffsetPhys()");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    *pReturlwalue = INT_TO_JSVAL(pSurface2D->GetTagsOffsetPhys());
    return JS_TRUE;
}

C_(Surface2D_GetTagsMin)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: Surface2D.GetTagsMin()");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    *pReturlwalue = INT_TO_JSVAL(pSurface2D->GetTagsMin());
    return JS_TRUE;
}

C_(Surface2D_GetTagsMax)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: Surface2D.GetTagsMax()");
        return JS_FALSE;
    }
    RC rc;
    Surface2D *pSurface2D = (Surface2D *)JS_GetPrivate(pContext, pObject);
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    *pReturlwalue = INT_TO_JSVAL(pSurface2D->GetTagsMax());
    return JS_TRUE;
}

C_(Surface2D_FlushCpuCache)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    UINT32 flushType = 0;
    if (NumArguments != 1 ||
        OK != JavaScriptPtr()->FromJsval(pArguments[0], &flushType))
    {
        JS_ReportError(pContext, "Usage: Surface2D.FlushCpuCache(flushType)");
        return JS_FALSE;
    }
    Surface2D* pSurface2D = (Surface2D*)JS_GetPrivate(pContext, pObject);
    RC rc;
    C_CHECK_RC(ReturnErrorIfNull(pSurface2D));
    pSurface2D->FlushCpuCache(flushType);
    // Ignore any RC returned by Surface2D::FlushCpuCache to avoid breaks
    // on platforms which don't support it.
    RETURN_RC(OK);
}

C_(Surface2D_SetIndividualLocationOverride)
{
#ifdef SIM_BUILD
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    const char usage[] =
        "Usage: Surface2D.SetIndividualLocationOverride(name, location)";
    string surfaceName;
    INT32 location;

    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &surfaceName)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &location)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    g_IndividualLocationOverride[surfaceName] = location;
#endif
    return JS_TRUE;
}

P_(Global_Get_g_Surface2DLocationOverride)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    INT32 Result = Surface2D::GetLocationOverride();
    if (OK != JavaScriptPtr()->ToJsval(Result, pValue))
    {
        JS_ReportError(pContext, "Failed to get g_Surface2DLocationOverride");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

P_(Global_Set_g_Surface2DLocationOverride)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    INT32 Value;
    if (OK != JavaScriptPtr()->FromJsval(*pValue, &Value))
    {
        JS_ReportError(pContext, "Failed to set g_Surface2DLocationOverride");
        return JS_FALSE;
    }
    Surface2D::SetLocationOverride(Value);
    return JS_TRUE;
}

P_(Global_Get_g_Surface2DLocationOverrideSysmem)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    INT32 Result = Surface2D::GetLocationOverrideSysmem();
    if (OK != JavaScriptPtr()->ToJsval(Result, pValue))
    {
        JS_ReportError(pContext, "Failed to get g_Surface2DLocationOverrideSysmem");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

P_(Global_Set_g_Surface2DLocationOverrideSysmem)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    INT32 Value;
    if (OK != JavaScriptPtr()->FromJsval(*pValue, &Value))
    {
        JS_ReportError(pContext, "Failed to set g_Surface2DLocationOverrideSysmem");
        return JS_FALSE;
    }
    Surface2D::SetLocationOverrideSysmem(Value);
    return JS_TRUE;
}

