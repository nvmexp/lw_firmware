/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwmisc.h"
#include "core/include/utility.h"
#include "lwos.h"
#include "gpu/include/gpudev.h"
#include "random.h"

#include "rmt_flagutil.h"
#include "rmt_vaspace.h"
#include "rmt_memalloc.h"
#include "rmt_memmap.h"
#include "core/include/memcheck.h"

// Defined locally to mitigate global warming.
#define KB 1024ULL
#define MB (1024ULL * KB)
#define GB (1024ULL * MB)
#define TB (1024ULL * GB)
#define TO_GB(bytes) ((UINT32)((bytes) / GB))

using namespace rmt;

//-----------------------------------------------------------------------------
MemMapGpu::MemMapGpu(LwRm *pLwRm, GpuDevice *pGpuDev,
                     MemAllocVirt *pVirt, MemAllocPhys *pPhys,
                     const UINT32 id) :
    m_pLwRm(pLwRm),
    m_pGpuDev(pGpuDev),
    m_id(id),
    m_pVirt(pVirt),
    m_pPhys(pPhys),
    m_virtAddr(0)
{
    memset(&m_params, 0, sizeof(m_params));
}

//! Destroy a mapping, but first unregister it from its virtual and physical
//! allocations.
//-----------------------------------------------------------------------------
MemMapGpu::~MemMapGpu()
{
    if (m_virtAddr != 0)
    {
        m_pVirt->GetVASpace()->RemMapping(this);
        m_pVirt->RemMapping(this);
        m_pPhys->RemMapping(this);
        m_pLwRm->UnmapMemoryDma(m_pVirt->GetHandle(),
                                m_pPhys->GetHandle(),
                                0, // flags
                                m_virtAddr,
                                m_pGpuDev);
    }
}

//! Memory mapping strategies that will be randomized and pretty-printed.
//-----------------------------------------------------------------------------
static const BitFields &MapStrategy()
{
    static BitFields util;
    STATIC_BIT_FIELD(&util, LW_MEMMAP_STRATEGY_TYPE);
    STATIC_BIT_VALUE(&util, LW_MEMMAP_STRATEGY_TYPE, 0.8, _ALLOC_MAPPING);
    STATIC_BIT_FIELD_PREP(&util);
    return util;
}

//! Memory mappings flags that will be randomized and pretty-printed.
//-----------------------------------------------------------------------------
static const BitFields &MapFlags()
{
    static BitFields util;
    STATIC_BIT_FIELD(&util, LWOS46_FLAGS_ACCESS);
    STATIC_BIT_VALUE(&util, LWOS46_FLAGS_ACCESS, 0.9, _READ_WRITE);
    STATIC_BIT_VALUE(&util, LWOS46_FLAGS_ACCESS, 0.1, _READ_ONLY);
    STATIC_BIT_VALUE(&util, LWOS46_FLAGS_ACCESS, 0.1, _WRITE_ONLY);
    STATIC_BIT_FIELD(&util, LWOS46_FLAGS_32BIT_POINTER);
    STATIC_BIT_VALUE(&util, LWOS46_FLAGS_32BIT_POINTER, 0.9, _DISABLE);
    STATIC_BIT_VALUE(&util, LWOS46_FLAGS_32BIT_POINTER, 0.1, _ENABLE);
    STATIC_BIT_FIELD(&util, LWOS46_FLAGS_CACHE_SNOOP); // TODO
    STATIC_BIT_VALUE(&util, LWOS46_FLAGS_CACHE_SNOOP, 1.0, _ENABLE);
    STATIC_BIT_FIELD(&util, LWOS46_FLAGS_KERNEL_MAPPING);
    STATIC_BIT_VALUE(&util, LWOS46_FLAGS_KERNEL_MAPPING, 0.9, _NONE);
    STATIC_BIT_VALUE(&util, LWOS46_FLAGS_KERNEL_MAPPING, 0.9, _ENABLE);
    STATIC_BIT_FIELD(&util, LWOS46_FLAGS_SHADER_ACCESS);
    STATIC_BIT_VALUE(&util, LWOS46_FLAGS_SHADER_ACCESS, 0.1, _DEFAULT);
    STATIC_BIT_VALUE(&util, LWOS46_FLAGS_SHADER_ACCESS, 0.1, _READ_ONLY);
    STATIC_BIT_VALUE(&util, LWOS46_FLAGS_SHADER_ACCESS, 0.1, _WRITE_ONLY);
    STATIC_BIT_VALUE(&util, LWOS46_FLAGS_SHADER_ACCESS, 0.1, _READ_WRITE);
    STATIC_BIT_FIELD(&util, LWOS46_FLAGS_PAGE_SIZE);
    STATIC_BIT_VALUE(&util, LWOS46_FLAGS_PAGE_SIZE, 0.9, _DEFAULT);
    STATIC_BIT_VALUE(&util, LWOS46_FLAGS_PAGE_SIZE, 0.1, _4KB);
    STATIC_BIT_VALUE(&util, LWOS46_FLAGS_PAGE_SIZE, 0.1, _BIG);
    STATIC_BIT_VALUE(&util, LWOS46_FLAGS_PAGE_SIZE, 0.1, _BOTH);
    STATIC_BIT_VALUE(&util, LWOS46_FLAGS_PAGE_SIZE, 0.1, _HUGE);
    STATIC_BIT_FIELD(&util, LWOS46_FLAGS_DMA_OFFSET_GROWS);
    STATIC_BIT_VALUE(&util, LWOS46_FLAGS_DMA_OFFSET_GROWS, 0.9, _UP);
    STATIC_BIT_VALUE(&util, LWOS46_FLAGS_DMA_OFFSET_GROWS, 0.1, _DOWN);
    STATIC_BIT_FIELD(&util, LWOS46_FLAGS_DMA_OFFSET_FIXED);
    STATIC_BIT_VALUE(&util, LWOS46_FLAGS_DMA_OFFSET_FIXED, 0.9, _FALSE);
    STATIC_BIT_VALUE(&util, LWOS46_FLAGS_DMA_OFFSET_FIXED, 0.1, _TRUE);
    STATIC_BIT_FIELD(&util, LWOS46_FLAGS_TLB_LOCK);
    STATIC_BIT_VALUE(&util, LWOS46_FLAGS_TLB_LOCK, 0.9, _DISABLE);
    STATIC_BIT_VALUE(&util, LWOS46_FLAGS_TLB_LOCK, 0.1, _ENABLE);
    STATIC_BIT_FIELD(&util, LWOS46_FLAGS_DMA_UNICAST_REUSE_ALLOC); // not randomized
    STATIC_BIT_VALUE(&util, LWOS46_FLAGS_DMA_UNICAST_REUSE_ALLOC, 1.0, _FALSE);
    STATIC_BIT_FIELD(&util, LWOS46_FLAGS_DEFER_TLB_ILWALIDATION); // TODO
    STATIC_BIT_VALUE(&util, LWOS46_FLAGS_DEFER_TLB_ILWALIDATION, 0.1, _FALSE);
    STATIC_BIT_FIELD_PREP(&util);
    return util;
}

//! @sa Randomizable::Randomize
//-----------------------------------------------------------------------------
void MemMapGpu::Randomize(const UINT32 seed)
{
    MASSERT(m_virtAddr == 0);

    RandomInit rand(seed);

    memset(&m_params, 0, sizeof(m_params));

    m_params.flags = MapFlags().GetRandom(&rand);
    m_params.strategy = MapStrategy().GetRandom(&rand);

    const MemAlloc::Params &virtParams = m_pVirt->GetParams();
    const MemAlloc::Params &physParams = m_pPhys->GetParams();
    const bool bFixed = FLD_TEST_DRF(OS46, _FLAGS, _DMA_OFFSET_FIXED, _TRUE, m_params.flags);

    // Usually pick a valid virtual offset, but allow for negative cases.
    if (rand.GetRandom(0, 100) < 98)
    {
        m_params.offsetVirt = rand.GetRandom64(0, virtParams.size + 10);
        if (rand.GetRandom(0, 100) < 98)
        {
            const UINT64 pageSize = GetMappingPageSize();
            if (bFixed)
            {
                m_params.offsetVirt = LW_ALIGN_DOWN(m_params.offsetVirt, pageSize);
            }
            else
            {
                const UINT64 mapAddr = virtParams.offset + m_params.offsetVirt;
                m_params.offsetVirt = LW_ALIGN_DOWN(mapAddr, pageSize) - virtParams.offset;
            }
        }
    }
    else
    {
        m_params.offsetVirt = rand.GetRandom64();
    }

    // Usually pick a valid physical offset, but allow for negative cases.
    if (rand.GetRandom(0, 100) < 98)
    {
        m_params.offsetPhys = rand.GetRandom64(0, physParams.size + 10);
    }
    else
    {
        m_params.offsetPhys = rand.GetRandom64();
    }

    // Usually pick a valid length, but allow for negative cases.
    if (rand.GetRandom(0, 100) < 98)
    {
        m_params.length = rand.GetRandom64(0, MIN(virtParams.size - m_params.offsetVirt,
                                                  physParams.size - m_params.offsetPhys) + 10);
    }
    else
    {
        m_params.length = rand.GetRandom64();
    }
}

//! @sa Randomizable::CheckValidity
//-----------------------------------------------------------------------------
const char *MemMapGpu::CheckValidity() const
{
    const MemAlloc::Params &virtParams = m_pVirt->GetParams();
    const MemAlloc::Params &physParams = m_pPhys->GetParams();

    if (m_params.length == 0)
    {
        return "length is 0";
    }

    // TODO: Implement with UpdatePde2
    if (virtParams.flags & LWOS32_ALLOC_FLAGS_EXTERNALLY_MANAGED)
    {
        return "cannot map externally managed virtual memory";
    }

    const bool bFixed = FLD_TEST_DRF(OS46, _FLAGS, _DMA_OFFSET_FIXED, _TRUE, m_params.flags);

    const UINT64 virtPageSize  = m_pVirt->GetPageSize();
    const UINT64 virtBase      = virtParams.offset;
    const UINT64 virtLimit     = virtParams.offset + virtParams.size - 1;

    const UINT64 mapPageSize   = GetMappingPageSize();
    const UINT64 mapPageOffset = m_params.offsetPhys & (mapPageSize - 1);
    const UINT64 mapAddr       = (bFixed ? 0 : virtBase) + m_params.offsetVirt;
    const UINT64 mapBase       = LW_ALIGN_DOWN(mapAddr, mapPageSize);
    const UINT64 mapLimit      = LW_ALIGN_UP(mapBase + mapPageOffset + m_params.length,
                                             mapPageSize) - 1;
    const UINT64 mapSize       = mapLimit - mapBase + 1;

    if (1 != m_pVirt->GetVASpace()->GetVmmRegkey())
    {
        if ((virtPageSize != 0) && (virtPageSize != mapPageSize))
        {
            return "incompatible mapping and virtual page size";
        }
    }
    else
    {
        if (FLD_TEST_DRF(OS32, _ATTR, _PAGE_SIZE, _BIG, physParams.attr) &&
            (mapPageSize != 4096) &&
            (mapPageSize != m_pVirt->GetVASpace()->GetBigPageSize()))
        {
            return "incompatible physical and virtual big page size";
        }
    }
    if ((0             != (mapAddr & (mapPageSize - 1))) &&
        (mapPageOffset != (mapAddr & (mapPageSize - 1))))
    {
        return "virtual offset not aligned to page size or page offset";
    }
    if (!FLD_TEST_DRF(OS32, _ATTR, _COMPR, _NONE, physParams.attr))
    {
        if (m_pGpuDev->GetSubdevice(0)->HasFeature(
                Device::GPUSUB_SUPPORTS_PER_VASPACE_BIG_PAGE))
        {
            if (0 != (mapBase & (m_pVirt->GetVASpace()->GetBigPageSize() - 1)))
            {
                return "not aligned to compression (big) page size";
            }
        }
        else
        {
            UINT64 comprPageSize;
            m_pGpuDev->GetCompressionPageSize(&comprPageSize);
            if (0 != (mapBase & (comprPageSize - 1)))
            {
                return "not aligned to compression page size";
            }
        }
    }
    if (!(virtBase <= mapBase))
    {
        return "virtual offset out of range";
    }
    if (!(mapBase <= mapLimit))
    {
        return "virtual limit overflow";
    }
    if (!(mapLimit <= virtLimit))
    {
        return "virtual limit out of range";
    }

    Heap *pHeap = m_pVirt->GetHeap();
    RC rc = pHeap->AllocAt(mapSize, mapBase);
    if (rc == OK)
    {
        pHeap->Free(mapBase);
    }
    else
    {
        return "overlap with other virtual mappings";
    }

    if (!(m_params.offsetPhys + m_params.length > m_params.offsetPhys))
    {
        return "physical limit overflow";
    }
    if (!(m_params.offsetPhys + m_params.length <= physParams.size))
    {
        return "physical limit out of range";
    }

    return NULL;
}

//! @sa Randomizable::CheckFeasibility
//-----------------------------------------------------------------------------
const char *MemMapGpu::CheckFeasibility() const
{
    const MemAlloc::Params &physParams = m_pPhys->GetParams();

    if ((m_pPhys->GetPageSize() != (4 * KB)) &&
         FLD_TEST_DRF(OS32, _ATTR, _PHYSICALITY, _NONCONTIGUOUS, physParams.attr))
    {
        return "mapping may fail if the physical allocation ilwolved is non-contig and uses big/huge page";
    }
    return NULL;
}

//! @sa Randomizable::PrintParams
//-----------------------------------------------------------------------------
void MemMapGpu::PrintParams(const INT32 pri) const
{
    Printf(pri,
        "          virt: %s(%u)\n", m_pVirt->GetTypeName(), m_pVirt->GetID());
    Printf(pri,
        "          phys: %s(%u)\n", m_pPhys->GetTypeName(), m_pPhys->GetID());
    Printf(pri,
        "  virtPageSize: 0x%llX\n", m_pVirt->GetPageSize());
    Printf(pri,
        "   mapPageSize: 0x%llX\n", GetMappingPageSize());
    Printf(pri,
        "    offsetVirt: 0x%llX\n", m_params.offsetVirt);
    Printf(pri,
        "    offsetPhys: 0x%llX\n", m_params.offsetPhys);
    Printf(pri,
        "        length: 0x%llX\n", m_params.length);
    MapFlags().Print(pri,
        "         flags: ", m_params.flags);
    MapStrategy().Print(pri,
        "      strategy: ", m_params.strategy);
}

//! @sa Randomizable::Create
//-----------------------------------------------------------------------------
RC MemMapGpu::Create()
{
    RC rc;
    MASSERT(m_virtAddr == 0);
    UINT64 offsetVirt = m_params.offsetVirt;

    switch (DRF_VAL(_MEMMAP, _STRATEGY, _TYPE, m_params.strategy))
    {
        case LW_MEMMAP_STRATEGY_TYPE_ALLOC_MAPPING:
            // AllocMapping (fully RM managed).
            rc = m_pLwRm->MapMemoryDma(m_pVirt->GetHandle(),
                                       m_pPhys->GetHandle(),
                                       m_params.offsetPhys,
                                       m_params.length,
                                       m_params.flags,
                                       &offsetVirt,
                                       m_pGpuDev);
            if (rc == OK)
            {
                m_virtAddr = offsetVirt;
                m_pPhys->AddMapping(this);
                m_pVirt->AddMapping(this);
                m_pVirt->GetVASpace()->AddMapping(this);
            }
            break;
        case LW_MEMMAP_STRATEGY_TYPE_FILL_PTE_MEM:
            // TODO
            rc = RC::UNSUPPORTED_FUNCTION;
            break;
        case LW_MEMMAP_STRATEGY_TYPE_UPDATE_PDE_2:
            // TODO
            rc = RC::UNSUPPORTED_FUNCTION;
            break;
    }

    return rc;
}

//! Override the paremters of this mapping.
//-----------------------------------------------------------------------------
void MemMapGpu::SetParams(const Params *pParams)
{
    MASSERT(m_virtAddr == 0);
    memcpy(&m_params, pParams, sizeof(m_params));
}

//! Get mapping virtual address base aligned down to page size.
//-----------------------------------------------------------------------------
UINT64 MemMapGpu::GetPageAlignedVirtBase() const
{
    return LW_ALIGN_DOWN(m_virtAddr, GetMappingPageSize());
}

//! Get mapping virtual address limit aligned up to page size.
//-----------------------------------------------------------------------------
UINT64 MemMapGpu::GetPageAlignedVirtLimit() const
{
    return LW_ALIGN_UP(m_virtAddr + m_params.length, GetMappingPageSize()) - 1;
}

//! Get mapping page size.
//-----------------------------------------------------------------------------
UINT64 MemMapGpu::GetMappingPageSize() const
{
    //
    // Per vaspace big page size is supported on GM20X+ chips. In order to
    // enable verification of the same by MODS, overriding of the big page
    // size by small page size is allowed.
    //
    if (m_pGpuDev->GetSubdevice(0)->HasFeature(
            Device::GPUSUB_SUPPORTS_PER_VASPACE_BIG_PAGE) &&
        FLD_TEST_DRF(OS46, _FLAGS, _PAGE_SIZE, _4KB, m_params.flags))
    {
        return 4 * KB;
    }
    else
    {
        return m_pPhys->GetPageSize();
    }
}

//-----------------------------------------------------------------------------
MemMapCpu::MemMapCpu(LwRm *pLwRm, GpuDevice *pGpuDev, const MemAllocPhys *pPhys,
                     const UINT64 offset, const UINT64 length) :
    m_pLwRm(pLwRm),
    m_pGpuDev(pGpuDev),
    m_pPhys(pPhys),
    m_offset(offset),
    m_length(length),
    m_cpuAddr(NULL)
{
}

//-----------------------------------------------------------------------------
MemMapCpu::~MemMapCpu()
{
    if (m_cpuAddr != NULL)
    {
        m_pLwRm->UnmapMemory(m_pPhys->GetHandle(),
                             m_cpuAddr,
                             0, // flags
                             m_pGpuDev->GetSubdevice(0));
    }
}

//! Create the CPU mapping.
//-----------------------------------------------------------------------------
RC MemMapCpu::Create()
{
    MASSERT(m_cpuAddr == NULL);
    return m_pLwRm->MapMemory(m_pPhys->GetHandle(),
                              m_offset,
                              m_length,
                              &m_cpuAddr,
                              0, // flags
                              m_pGpuDev->GetSubdevice(0));
}

