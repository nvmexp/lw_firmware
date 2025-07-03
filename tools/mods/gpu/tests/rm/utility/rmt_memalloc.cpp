/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2016,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <algorithm>
#include "lwmisc.h"
#include "core/include/utility.h"
#include "core/include/platform.h"
#include "core/include/xp.h"
#include "lwos.h"
#include "gpu/include/gpudev.h"
#include "random.h"

#include "rmt_flagutil.h"
#include "rmt_memalloc.h"
#include "rmt_memmap.h"
#include "rmt_vaspace.h"
#include "core/include/memcheck.h"

// Defined locally to mitigate global warming.
#define KB 1024ULL
#define MB (1024ULL * KB)
#define GB (1024ULL * MB)
#define TB (1024ULL * GB)
#define TO_GB(bytes) ((UINT32)((bytes) / GB))

using namespace rmt;

//-----------------------------------------------------------------------------
MemAlloc::MemAlloc(LwRm *pLwRm, GpuDevice *pGpuDev, const UINT32 id) :
    m_pLwRm(pLwRm),
    m_pGpuDev(pGpuDev),
    m_id(id),
    m_handle(0)
{
    memset(&m_params, 0, sizeof(m_params));
}

//! Destroy a memory allocation, but first destroy any mappings associated
//! with it.
//-----------------------------------------------------------------------------
MemAlloc::~MemAlloc()
{
    if (m_handle != 0)
    {
        ClearMappings();
        m_pLwRm->Free(m_handle);
    }
}

//! Destroy any mappings associated with this memory allocation.
//-----------------------------------------------------------------------------
void MemAlloc::ClearMappings()
{
    while (!m_mappings.empty())
    {
        Randomizable::deleter(m_mappings.back());
    }
}

//! Memory allocation flags that will be randomized and pretty-printed.
//-----------------------------------------------------------------------------
static const BitFlags &MemFlags()
{
    static BitFlags util;
    STATIC_BIT_FLAG(&util, 0.80, LWOS32_ALLOC_FLAGS_NO_SCANOUT);
    STATIC_BIT_FLAG(&util, 0.80, LWOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED);
    STATIC_BIT_FLAG(&util, 0.10, LWOS32_ALLOC_FLAGS_FORCE_MEM_GROWS_UP);
    STATIC_BIT_FLAG(&util, 0.10, LWOS32_ALLOC_FLAGS_FORCE_MEM_GROWS_DOWN);
    STATIC_BIT_FLAG(&util, 0.10, LWOS32_ALLOC_FLAGS_FORCE_ALIGN_HOST_PAGE);
    STATIC_BIT_FLAG(&util, 0.10, LWOS32_ALLOC_FLAGS_FIXED_ADDRESS_ALLOCATE);
    STATIC_BIT_FLAG(&util, 0.10, LWOS32_ALLOC_FLAGS_ALIGNMENT_HINT);
    STATIC_BIT_FLAG(&util, 0.20, LWOS32_ALLOC_FLAGS_ALIGNMENT_FORCE);
    STATIC_BIT_FLAG(&util, 0.10, LWOS32_ALLOC_FLAGS_USE_BEGIN_END);
    // TODO: determine validity dynamically based on protected regions
    STATIC_BIT_FLAG(&util, 0.00, LWOS32_ALLOC_FLAGS_PROTECTED);
    STATIC_BIT_FLAG(&util, 0.00, LWOS32_ALLOC_FLAGS_VIRTUAL); // not randomized
    STATIC_BIT_FLAG(&util, 0.50, LWOS32_ALLOC_FLAGS_LAZY);
    STATIC_BIT_FLAG(&util, 0.05, LWOS32_ALLOC_FLAGS_EXTERNALLY_MANAGED);
    STATIC_BIT_FLAG(&util, 0.05, LWOS32_ALLOC_FLAGS_SPARSE);
    STATIC_BIT_FLAG(&util, 0.50, LWOS32_ALLOC_FLAGS_MAXIMIZE_ADDRESS_SPACE);
    STATIC_BIT_FLAG(&util, 0.30, LWOS32_ALLOC_FLAGS_PREFER_PTES_IN_SYSMEMORY);
    return util;
};

//! Memory allocation attributes that will be randomized and pretty-printed.
//-----------------------------------------------------------------------------
static const BitFields &MemAttr()
{
    static BitFields util;
    STATIC_BIT_FIELD(&util, LWOS32_ATTR_DEPTH);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_DEPTH, 0.05, _UNKNOWN);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_DEPTH, 0.05, _8);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_DEPTH, 0.25, _16);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_DEPTH, 0.25, _24);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_DEPTH, 0.25, _32);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_DEPTH, 0.25, _64);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_DEPTH, 0.05, _128);
    STATIC_BIT_FIELD(&util, LWOS32_ATTR_COMPR_COVG);
    STATIC_BIT_FIELD(&util, LWOS32_ATTR_AA_SAMPLES);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_AA_SAMPLES, 0.2, _1);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_AA_SAMPLES, 0.2, _2);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_AA_SAMPLES, 0.2, _4);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_AA_SAMPLES, 0.2, _4_ROTATED);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_AA_SAMPLES, 0.05, _6);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_AA_SAMPLES, 0.2, _8);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_AA_SAMPLES, 0.05, _16);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_AA_SAMPLES, 0.2, _4_VIRTUAL_8);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_AA_SAMPLES, 0.2, _4_VIRTUAL_16);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_AA_SAMPLES, 0.2, _8_VIRTUAL_16);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_AA_SAMPLES, 0.2, _8_VIRTUAL_32);
    STATIC_BIT_FIELD(&util, LWOS32_ATTR_COMPR);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_COMPR, 0.8, _NONE);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_COMPR, 0.2, _REQUIRED);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_COMPR, 0.2, _ANY);
    STATIC_BIT_FIELD(&util, LWOS32_ATTR_FORMAT);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_FORMAT, 0.9, _PITCH);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_FORMAT, 0.1, _SWIZZLED);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_FORMAT, 0.1, _BLOCK_LINEAR);
    STATIC_BIT_FIELD(&util, LWOS32_ATTR_Z_TYPE);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_Z_TYPE, 0.5, _FIXED);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_Z_TYPE, 0.5, _FLOAT);
    STATIC_BIT_FIELD(&util, LWOS32_ATTR_ZS_PACKING);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_ZS_PACKING, 0.1, _Z24S8);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_ZS_PACKING, 0.1, _S8Z24);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_ZS_PACKING, 0.1, _Z32);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_ZS_PACKING, 0.1, _Z24X8);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_ZS_PACKING, 0.1, _X8Z24);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_ZS_PACKING, 0.1, _Z32_X24S8);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_ZS_PACKING, 0.1, _X8Z24_X24S8);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_ZS_PACKING, 0.1, _Z16);
    STATIC_BIT_FIELD(&util, LWOS32_ATTR_PAGE_SIZE);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_PAGE_SIZE, 0.5, _DEFAULT);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_PAGE_SIZE, 0.1, _4KB);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_PAGE_SIZE, 0.1, _BIG);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_PAGE_SIZE, 0.1, _HUGE);
    STATIC_BIT_FIELD(&util, LWOS32_ATTR_LOCATION);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_LOCATION, 0.50, _VIDMEM);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_LOCATION, 0.40, _PCI);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_LOCATION, 0.10, _ANY);
    STATIC_BIT_FIELD(&util, LWOS32_ATTR_PHYSICALITY);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_PHYSICALITY, 0.2, _DEFAULT);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_PHYSICALITY, 0.1, _NONCONTIGUOUS);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_PHYSICALITY, 0.1, _CONTIGUOUS);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_PHYSICALITY, 0.1, _ALLOW_NONCONTIGUOUS);
    STATIC_BIT_FIELD(&util, LWOS32_ATTR_COHERENCY);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_COHERENCY, 0.1, _UNCACHED);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_COHERENCY, 0.1, _CACHED);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_COHERENCY, 0.1, _WRITE_COMBINE);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_COHERENCY, 0.1, _WRITE_THROUGH);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_COHERENCY, 0.1, _WRITE_PROTECT);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR_COHERENCY, 0.1, _WRITE_BACK);
    STATIC_BIT_FIELD_PREP(&util);
    return util;
}

//! MOAR!
//-----------------------------------------------------------------------------
static const BitFields &MemAttr2()
{
    static BitFields util;
    STATIC_BIT_FIELD(&util, LWOS32_ATTR2_ZBC);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR2_ZBC, 0.25, _DEFAULT);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR2_ZBC, 0.25, _PREFER_NO_ZBC);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR2_ZBC, 0.25, _PREFER_ZBC);
    STATIC_BIT_FIELD(&util, LWOS32_ATTR2_GPU_CACHEABLE);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR2_GPU_CACHEABLE, 0.30, _DEFAULT);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR2_GPU_CACHEABLE, 0.30, _YES);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR2_GPU_CACHEABLE, 0.30, _NO);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR2_GPU_CACHEABLE, 0.05, _ILWALID);
    STATIC_BIT_FIELD(&util, LWOS32_ATTR2_P2P_GPU_CACHEABLE);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR2_P2P_GPU_CACHEABLE, 0.30, _DEFAULT);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR2_P2P_GPU_CACHEABLE, 0.30, _YES);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR2_P2P_GPU_CACHEABLE, 0.30, _NO);
    STATIC_BIT_FIELD(&util, LWOS32_ATTR2_32BIT_POINTER);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR2_32BIT_POINTER, 0.95, _DISABLE);
    STATIC_BIT_VALUE(&util, LWOS32_ATTR2_32BIT_POINTER, 0.05, _ENABLE);
    STATIC_BIT_FIELD_PREP(&util);
    return util;
}

//! @sa Randomizable::Randomize
//-----------------------------------------------------------------------------
void MemAlloc::Randomize(const UINT32 seed)
{
    // We shouldn't randomize once created.
    MASSERT(m_handle == 0);

    RandomInit rand(seed);
    memset(&m_params, 0, sizeof(m_params));

    m_params.flags = MemFlags().GetRandom(&rand);
    m_params.attr = MemAttr().GetRandom(&rand);
    m_params.attr2 = MemAttr2().GetRandom(&rand);
    m_params.size = rand.GetRandom64();
    m_params.offset = rand.GetRandom64();
    m_params.alignment = rand.GetRandom64();
    m_params.rangeBeg = rand.GetRandom64();
    m_params.rangeEnd = rand.GetRandom64();
}

//! @sa Randomizable::CheckValidity
//-----------------------------------------------------------------------------
const char *MemAlloc::CheckValidity() const
{
    if (m_params.size == 0)
    {
        return "size is 0";
    }
    if (FLD_TEST_DRF(OS32, _ATTR, _TILED, _REQUIRED, m_params.attr))
    {
        return "tiling not supported on LW50+ chips";
    }
    if (FLD_TEST_DRF(OS32, _ATTR, _ZLWLL, _REQUIRED, m_params.attr))
    {
        if (FLD_TEST_DRF(OS32, _ATTR, _DEPTH, _UNKNOWN, m_params.attr) ||
            FLD_TEST_DRF(OS32, _ATTR, _DEPTH, _8, m_params.attr) ||
            FLD_TEST_DRF(OS32, _ATTR, _DEPTH, _128, m_params.attr))
        {
            return "Invalid depth 8/UNKNOWN/128 for Z-lwlling";
        }
        if (FLD_TEST_DRF(OS32, _ATTR, _AA_SAMPLES, _16, m_params.attr) ||
            FLD_TEST_DRF(OS32, _ATTR, _AA_SAMPLES, _6, m_params.attr))
        {
            return "Invalid AA Samples Attr 6/16";
        }
    }
    if (FLD_TEST_DRF(OS32, _ATTR, _PAGE_SIZE, _HUGE, m_params.attr))
    {
        if (FLD_TEST_DRF(OS32, _ATTR2, _PAGE_SIZE_HUGE, _2MB, m_params.attr2)
            && !m_pGpuDev->SupportsPageSize(GpuDevice::PAGESIZE_2MB))
        {
            return "2MB pages not supported on this platform";
        }
        if (FLD_TEST_DRF(OS32, _ATTR2, _PAGE_SIZE_HUGE, _512MB, m_params.attr2)
            && !m_pGpuDev->SupportsPageSize(GpuDevice::PAGESIZE_512MB))
        {
            return "512MB pages not supported on this platform";
        }
    }
    return NULL;
}

//! @sa Randomizable::CheckFeasibility
//-----------------------------------------------------------------------------
const char *MemAlloc::CheckFeasibility() const
{
    if (m_params.flags & LWOS32_ALLOC_FLAGS_FIXED_ADDRESS_ALLOCATE)
    {
        return "fixed address allocation may fail";
    }
    if (m_params.flags & LWOS32_ALLOC_FLAGS_USE_BEGIN_END)
    {
        return "Range based allocation may fail";
    }
    if ((m_params.flags & LWOS32_ALLOC_FLAGS_ALIGNMENT_FORCE) ||
        (m_params.flags & LWOS32_ALLOC_FLAGS_ALIGNMENT_HINT))
    {
        return "a requested alignment may be impossible";
    }
    if (FLD_TEST_DRF(OS32, _ATTR, _COMPR, _REQUIRED, m_params.attr) &&
        !FLD_TEST_DRF(OS32, _ATTR, _LOCATION, _VIDMEM, m_params.attr))
    {
        return "sysmem may not compressible";
    }
    if (FLD_TEST_DRF(OS32, _ATTR, _ZLWLL, _ANY, m_params.attr))
    {
        return "zlwll allocation may be skipped when not required";
    }

    //
    // The reason is when Z-lwlling is enabled, fbZLwllAlloc_GF100()
    // is called. Meanwhile the height and pitch values are set to 1
    // by lwrm.cpp. When Z-lwlling attribute is not SHARED, the height
    // or width or both are made 0 which prompts RM failure. Hence expecting
    // a failure here
    //
    if (FLD_TEST_DRF(OS32, _ATTR, _ZLWLL, _REQUIRED, m_params.attr))
    {
        return "zlwll allocation may fail under test conditions";
    }
    return NULL;
}

//! @sa Randomizable::PrintParams
//-----------------------------------------------------------------------------
void MemAlloc::PrintParams(const INT32 pri) const
{
    MemFlags().Print(pri,
        "        flags: ", m_params.flags);
    MemAttr().Print(pri,
        "         attr: ", m_params.attr);
    MemAttr2().Print(pri,
        "        attr2: ", m_params.attr2);
    Printf(pri,
        "         size: 0x%llX\n", m_params.size);
    Printf(pri,
        "    alignment: 0x%llX\n", m_params.alignment);
    Printf(pri,
        "       offset: 0x%llX\n", m_params.offset);
    Printf(pri,
        "        limit: 0x%llX\n", m_params.limit);
    Printf(pri,
        "     rangeBeg: 0x%llX\n", m_params.rangeBeg);
    Printf(pri,
        "     rangeEnd: 0x%llX\n", m_params.rangeEnd);
}

//! @sa Randomizable::Create
//-----------------------------------------------------------------------------
RC MemAlloc::Create()
{
    MASSERT(m_handle == 0);
    VASpace *pVAS = GetVASpace();
    return m_pLwRm->VidHeapAllocSizeEx(LWOS32_TYPE_IMAGE,
                                       m_params.flags,
                                       &m_params.attr,
                                       &m_params.attr2,
                                       &m_params.size,
                                       &m_params.alignment,
                                       &m_handle,
                                       &m_params.offset,
                                       &m_params.limit,
                                       m_pGpuDev,
                                       m_params.rangeBeg,
                                       m_params.rangeEnd,
                                       pVAS ? pVAS->GetHandle() : 0);
}

//! Override the paremters of this allocation.
//-----------------------------------------------------------------------------
void MemAlloc::SetParams(const Params *pParams)
{
    MASSERT(m_handle == 0);
    memcpy(&m_params, pParams, sizeof(m_params));
}

//! Get the page size associated with this allocation.
//-----------------------------------------------------------------------------
UINT64 MemAlloc::GetPageSize() const
{
    switch (DRF_VAL(OS32, _ATTR, _PAGE_SIZE, m_params.attr))
    {
        case LWOS32_ATTR_PAGE_SIZE_4KB:
            return GpuDevice::PAGESIZE_4KB;
        case LWOS32_ATTR_PAGE_SIZE_BIG:
            return m_pGpuDev->GetBigPageSize();
        case LWOS32_ATTR_PAGE_SIZE_HUGE:
            switch (DRF_VAL(OS32, _ATTR2, _PAGE_SIZE_HUGE, m_params.attr2))
            {
                case LWOS32_ATTR2_PAGE_SIZE_HUGE_2MB:
                    return GpuDevice::PAGESIZE_2MB;
                case LWOS32_ATTR2_PAGE_SIZE_HUGE_512MB:
                    return GpuDevice::PAGESIZE_512MB;
            }
            break;
    }
    return 0;
}

//! Register a mapping with this allocation.
//! @sa MemMapGpu::Create
//-----------------------------------------------------------------------------
void MemAlloc::AddMapping(MemMapGpu *pMap)
{
    m_mappings.push_back(pMap);
}

//! Unregister a mapping from this allocation.
//! @sa MemMapGpu::~MemMapGpu
//-----------------------------------------------------------------------------
void MemAlloc::RemMapping(MemMapGpu *pMap)
{
    m_mappings.erase(std::find(m_mappings.begin(), m_mappings.end(), pMap));
}

//-----------------------------------------------------------------------------
MemAllocVirt::MemAllocVirt(LwRm *pLwRm, GpuDevice *pGpuDev, VASpace *pVAS,
                           const UINT32 id) :
    MemAlloc(pLwRm, pGpuDev, id),
    m_pVAS(pVAS)
{
}

//! Destroy a virtual allocation, but first unregister it from its parent
//! VA space.
//-----------------------------------------------------------------------------
MemAllocVirt::~MemAllocVirt()
{
    if (GetHandle())
    {
        ClearMappings();
        m_pVAS->RemAlloc(this);
    }
}

//! Get the page size associated with this allocation.
//-----------------------------------------------------------------------------
UINT64 MemAllocVirt::GetPageSize() const
{
    switch (DRF_VAL(OS32, _ATTR, _PAGE_SIZE, m_params.attr))
    {
        case LWOS32_ATTR_PAGE_SIZE_BIG:
            return m_pVAS->GetBigPageSize();
        default:
            return MemAlloc::GetPageSize();
    }
    return 0;
}

//! @sa Randomizable::Randomize
//-----------------------------------------------------------------------------
void MemAllocVirt::Randomize(const UINT32 seed)
{
    //
    // Weights for virtual allocation size.
    //
    // Since the picker utility only supports 32-bit values,
    // split into GB-granulariy and byte-granulariy.
    // TODO: Template the picker?
    //
    static Random::PickItem SizePickerGB[] =
    {
       PI_RANGE(99, TO_GB(0  * GB), TO_GB(0  * GB)),
       PI_RANGE(4 , TO_GB(1  * GB), TO_GB(4  * GB)),
       PI_RANGE(1 , TO_GB(1  * TB), LW_U32_MAX)
    };
    STATIC_PI_PREP(SizePickerGB);

    static Random::PickItem SizePickerBytes[] =
    {
       PI_RANGE(1 , (0       ), (0       )),
       PI_RANGE(60, (1       ), (128 * KB)),
       PI_RANGE(20, (128 * KB), (1   * MB)),
       PI_RANGE(20, (1   * MB), (4  * MB)),  // To trigger huge page sized allocation
       PI_RANGE(10, (4   * MB), (16  * MB)),
       PI_RANGE(5 , (16  * MB), (GB - 1  ))
    };
    STATIC_PI_PREP(SizePickerBytes);

    RandomInit rand(seed);

    // Randomize common parameters.
    MemAlloc::Randomize(rand.GetRandom());

    if (0 != m_pVAS->GetVmmRegkey())
    {
        m_params.flags &= ~LWOS32_ALLOC_FLAGS_EXTERNALLY_MANAGED;
        m_params.flags &= ~LWOS32_ALLOC_FLAGS_SPARSE;
    }

    m_params.flags |= LWOS32_ALLOC_FLAGS_VIRTUAL;
    m_params.size = (UINT64)rand.Pick(SizePickerGB) * GB;
    m_params.size += rand.Pick(SizePickerBytes);
    m_params.offset = (UINT64)rand.Pick(SizePickerGB) * GB;
    m_params.offset += rand.Pick(SizePickerBytes);
    if (rand.GetRandom(0, 100) < 98)
    {
        m_params.alignment = 1ULL << rand.GetRandom64(0, 39);
    }
    else
    {
        m_params.alignment = (UINT64)rand.Pick(SizePickerGB) * GB;
        m_params.alignment += rand.Pick(SizePickerBytes);
    }
    m_params.rangeBeg = (UINT64)rand.Pick(SizePickerGB) * GB;
    m_params.rangeBeg += rand.Pick(SizePickerBytes);
    m_params.rangeEnd = (UINT64)rand.Pick(SizePickerGB) * GB;
    m_params.rangeEnd += rand.Pick(SizePickerBytes);
    if ((m_params.flags & LWOS32_ALLOC_FLAGS_USE_BEGIN_END) &&
        FLD_TEST_DRF(OS32, _ATTR2, _32BIT_POINTER, _ENABLE, m_params.attr2))
    {
        if (m_params.rangeEnd != 0)
        {
            m_params.rangeEnd = MIN(0xffffffff, m_params.rangeEnd);

        }
        else
        {
            m_params.rangeEnd = 0xffffffff;
        }
    }
}

//! @sa Randomizable::CheckValidity
//-----------------------------------------------------------------------------
const char *MemAllocVirt::CheckValidity() const
{
    const char *msg = MemAlloc::CheckValidity();
    if (msg)
    {
        return msg;
    }
    if (m_params.size >= TB)
    {
        return "size is larger than max VA";
    }
    Heap *pHeap = m_pVAS->GetHeap();
    if (m_params.flags & LWOS32_ALLOC_FLAGS_FIXED_ADDRESS_ALLOCATE)
    {
        RC rc = pHeap->AllocAt(m_params.size, m_params.offset);
        if (rc == OK)
        {
            pHeap->Free(m_params.offset);
        }
        else
        {
            return "impossible fixed address allocation";
        }
    }
    else if (m_params.flags & LWOS32_ALLOC_FLAGS_USE_BEGIN_END)
    {
        UINT64 offset;
        if (m_params.rangeBeg > m_params.rangeEnd)
        {
            return "start address is greater than end address";
        }
        if (m_params.size > (m_params.rangeEnd - m_params.rangeBeg + 1))
        {
            return "size is larger than length of range";
        }
        RC rc = pHeap->AllocRange(m_params.size, &offset, m_params.rangeBeg,
                                  m_params.rangeEnd);
        if (rc == OK)
        {
            pHeap->Free(offset);
        }
        else
        {
            return "impossible range based allocation";
        }
    }
    else if (FLD_TEST_DRF(OS32, _ATTR2, _32BIT_POINTER, _ENABLE, m_params.attr2))
    {
        UINT64 offset;
        RC rc = pHeap->AllocRange(m_params.size, &offset, 0, 0xFFFFFFFF);
        if (rc == OK)
        {
            pHeap->Free(offset);
        }
        else
        {
            return "impossible 32-bit pointer allocation";
        }
    }
    else
    {
        if (m_params.size > pHeap->GetLargestFree())
        {
            return "size too large for current heap";
        }
    }
    return NULL;
}

//! @sa Randomizable::CheckFeasibility
//-----------------------------------------------------------------------------
const char *MemAllocVirt::CheckFeasibility() const
{
    const char *msg = MemAlloc::CheckFeasibility();
    if (msg)
    {
        return msg;
    }
    if (FLD_TEST_DRF(OS32, _ATTR2, _32BIT_POINTER, _ENABLE, m_params.attr2))
    {
        return "32-bit pointer allocation may fail";
    }

    Heap *pHeap = m_pVAS->GetHeap();
    const UINT64 largestFree = pHeap->GetLargestFree();
    if ((m_params.size * 2) > largestFree)
    {
        return "size may be too large for current heap";
    }

    UINT64 bigPageSize = m_pVAS->GetBigPageSize();
    if (largestFree < (bigPageSize << 11)) // 2x PDE coverage
    {
        return "VAS low on free space";
    }

    return NULL;
}

//! @sa Randomizable::PrintParams
//-----------------------------------------------------------------------------
void MemAllocVirt::PrintParams(const INT32 pri) const
{
    Printf(pri,
        "          vas: %s(%u)\n", m_pVAS->GetTypeName(), m_pVAS->GetID());
    MemAlloc::PrintParams(pri);
}

//! @sa Randomizable::Create
//-----------------------------------------------------------------------------
RC MemAllocVirt::Create()
{
    RC rc;
    rc = MemAlloc::Create();
    if (rc == OK)
    {
        // Create mapping heap.
        m_pHeap.reset(new Heap(m_params.offset, m_params.size,
                               m_pGpuDev->GetSubdevice(0)->GetGpuInst()));

        // Register with parent VAS.
        m_pVAS->AddAlloc(this);
    }
    return rc;
}

//! Extend mapping registration by trackng the mapping within our mapping heap.
//-----------------------------------------------------------------------------
void MemAllocVirt::AddMapping(MemMapGpu *pMap)
{
    MemAlloc::AddMapping(pMap);

    const UINT64 base  = pMap->GetPageAlignedVirtBase();
    const UINT64 limit = pMap->GetPageAlignedVirtLimit();
    m_pHeap->AllocAt(limit - base + 1, base);
}

//! Extend mapping unregistrations by removing the mapping from our mapping heap.
//-----------------------------------------------------------------------------
void MemAllocVirt::RemMapping(MemMapGpu *pMap)
{
    m_pHeap->Free(pMap->GetPageAlignedVirtBase());
    MemAlloc::RemMapping(pMap);
}

//-----------------------------------------------------------------------------
MemAllocPhys::MemAllocPhys(LwRm *pLwRm, GpuDevice *pGpuDev,
                           const UINT32 largeAllocWeight,
                           const UINT32 largePageInSysmem, const UINT32 id)
    : MemAlloc(pLwRm, pGpuDev, id)
    , m_largeAllocWeight(largeAllocWeight)
    , m_largePageInSysmem(largePageInSysmem)
{
}

//! Destroy a physical allocation, but first verify it contains its
//! expected contents.
//-----------------------------------------------------------------------------
MemAllocPhys::~MemAllocPhys()
{
    StickyRC rc;
    if (GetHandle() != 0)
    {
        rc = VerifyChunks();
    }
    MASSERT(rc == OK);
}

//! @sa Randomizable::Randomize
//-----------------------------------------------------------------------------
void MemAllocPhys::Randomize(const UINT32 seed)
{
    // Weights for physical allocation size.
    static Random::PickItem SizePicker[] =
    {
       PI_RANGE( 5, (0       ), (0       )),
       PI_RANGE(60, (1       ), (128 * KB)),
       PI_RANGE(20, (128 * KB), (1   * MB)),
       //
       // The following ranges are needed to cover 2MB page size,
       // but result in significantly slower sim time to init/verify contents.
       // Therefore the probabilities are made slim, but can be increased
       // with the "-large_alloc_weight" command line argument.
       //
       PI_RANGE(m_largeAllocWeight * 4, (1   * MB), (4   * MB)),
       PI_RANGE(m_largeAllocWeight * 2, (4   * MB), (8   * MB)),
       PI_RANGE(m_largeAllocWeight * 1, (8   * MB), (16  * MB)),
    };
    STATIC_PI_PREP(SizePicker);

    RandomInit rand(seed);

    // Randomize common parameters.
    MemAlloc::Randomize(rand.GetRandom());

    // Usually mask off the virtual-only flags, but allow negative case.
    if (rand.GetRandom(0, 100) < 95)
    {
        m_params.flags &= ~LWOS32_ALLOC_FLAGS_VIRTUAL_ONLY;
    }

    //
    // Usually disable SYSMEM aperture if not supported.
    //
    // TODO: This should check for SYSMEM and set to ANY, but RM lwrrently
    //       forces ANY to SYSMEM.
    //
    if (!FLD_TEST_DRF(OS32, _ATTR, _LOCATION, _VIDMEM, m_params.attr) &&
        !m_pGpuDev->GetSubdevice(0)->HasFeature(Device::GPUSUB_HAS_SYSMEM) &&
        (rand.GetRandom(0, 100) < 95))
    {
        m_params.attr = FLD_SET_DRF(OS32, _ATTR, _LOCATION, _VIDMEM, m_params.attr);
    }

    // Pick a reasonably size for physical.
    m_params.size = (UINT64)rand.Pick(SizePicker);
    m_params.offset = (UINT64)rand.Pick(SizePicker);
    if (rand.GetRandom(0, 100) < 98)
    {
        m_params.alignment = 1ULL << rand.GetRandom64(0, 23);
    }
    else
    {
        m_params.alignment = (UINT64)rand.Pick(SizePicker);
    }
    m_params.rangeBeg = (UINT64)rand.Pick(SizePicker);
    m_params.rangeEnd = (UINT64)rand.Pick(SizePicker);
}

//! @sa Randomizable::CheckValidity
//-----------------------------------------------------------------------------
const char *MemAllocPhys::CheckValidity() const
{
    const UINT64 bigPageSize = m_pGpuDev->GetBigPageSize();

    // Check for common allocation validty.
    const char *msg = MemAlloc::CheckValidity();
    if (msg)
    {
        return msg;
    }
    // Check physical-specific constraints.
    if (m_params.flags & LWOS32_ALLOC_FLAGS_VIRTUAL_ONLY)
    {
        return "virtual-only flag with physical alloc";
    }
    // TODO: Check should be for SYSMEM only once RM is fixed.
    if (!FLD_TEST_DRF(OS32, _ATTR, _LOCATION, _VIDMEM, m_params.attr) &&
        !m_pGpuDev->GetSubdevice(0)->HasFeature(Device::GPUSUB_HAS_SYSMEM))
    {
        return "SYSMEM aperture not supported on this GPU";
    }
    if (!FLD_TEST_DRF(OS32, _ATTR, _LOCATION, _VIDMEM, m_params.attr) &&
        (m_params.flags & LWOS32_ALLOC_FLAGS_FIXED_ADDRESS_ALLOCATE))
    {
        return "Fixed address sysmem allocations not supported";
    }
    if (FLD_TEST_DRF(OS32, _ATTR2, _32BIT_POINTER, _ENABLE, m_params.attr2))
    {
        return "32-bit pointer attribute not supported for physical allocs";
    }
    if (!FLD_TEST_DRF(OS32, _ATTR, _LOCATION, _VIDMEM, m_params.attr) &&
        FLD_TEST_DRF(OS32, _ATTR, _PAGE_SIZE, _HUGE, m_params.attr))
    {
        return "Huge pages not supported in sysmem";
    }
    if (((1 != m_largePageInSysmem) || (bigPageSize != Platform::GetPageSize())) &&
        !FLD_TEST_DRF(OS32, _ATTR, _LOCATION, _VIDMEM, m_params.attr) &&
        FLD_TEST_DRF(OS32, _ATTR, _PAGE_SIZE, _BIG, m_params.attr))
    {
        return "Big pages not supported in sysmem";
    }
    if (FLD_TEST_DRF(OS32, _ATTR, _FORMAT, _SWIZZLED, m_params.attr) &&
        FLD_TEST_DRF(OS32, _ATTR, _PHYSICALITY, _NONCONTIGUOUS, m_params.attr) &&
        FLD_TEST_DRF(OS32, _ATTR, _LOCATION, _VIDMEM, m_params.attr))
    {
        return "noncontiguous allocations with bank swizzling not allowed";
    }
    return NULL;
}

//! @sa Randomizable::CheckFeasibility
//-----------------------------------------------------------------------------
const char *MemAllocPhys::CheckFeasibility() const
{
    // Check for common allocation feasibility.
    const char *msg = MemAlloc::CheckFeasibility();
    if (msg)
    {
        return msg;
    }
    // Check physical-specific constraints.
    if (!FLD_TEST_DRF(OS32, _ATTR, _LOCATION, _VIDMEM, m_params.attr) &&
        FLD_TEST_DRF(OS32, _ATTR, _PHYSICALITY, _CONTIGUOUS, m_params.attr))
    {
        return "contiguous sysmem allocations may fail";
    }
    // TODO: check heap
    return NULL;
}

//! Record a sub-region of memory as being initialized to
//! a deterministic sequence of values.
//!
//! Lwrrently the sequence is a trivial increment every 4 bytes:
//! *(UINT32*)pMem[offset + 0x0] = seed + 0;
//! *(UINT32*)pMem[offset + 0x4] = seed + 1;
//! *(UINT32*)pMem[offset + 0x8] = seed + 2;
//! ...
//! *(UINT32*)pMem[offset + length - 4] = seed + (length / 4) - 1;
//!
//! If bInit == TRUE, the memory is actually written to these
//! values (through a BAR1 CPU mapping).
//! Otherwise the values are are only tracked
//! (assumed to be written by another means - e.g. w/ copy engine).
//!
//! @note Both offset and length must be 4-byte aligned.
//-----------------------------------------------------------------------------
RC MemAllocPhys::AddChunk(const UINT64 offset, const UINT64 length,
                          const UINT32 seed, const bool bInit)
{
    RC rc;
    const UINT64 limit = offset + length - 1;

    // Check alignment and bounds.
    MASSERT(length > 0);
    MASSERT((offset & 0x3) == 0);
    MASSERT((length & 0x3) == 0);
    MASSERT(limit < m_params.size);

    Printf(Tee::PriHigh,
           "%s(%u): Adding chunk 0x%llX-0x%llX = 0x%X...\n",
           GetTypeName(), GetID(), offset, limit, seed);

    // If requested, map to BAR1 and initialize the memory.
    if (bInit)
    {
        // Map chunk to BAR1.
        unique_ptr<MemMapCpu> pMap(new MemMapCpu(m_pLwRm, m_pGpuDev,
                                               this, offset, length));
        CHECK_RC(pMap->Create());
        UINT08 *pMem = (UINT08*)pMap->GetPtr();

        // This might take a while - print something to minimize suspicion.
        Printf(Tee::PriHigh,
               "%s(%u): Initializing chunk: 0x%llX-0x%llX = 0x%X...\n",
               GetTypeName(), GetID(), offset, limit, seed);

        //
        // Batch the BAR1 writes into a single call
        // by storing the entire chunk in a temporary buffer.
        //
        // This improves performance for simulation debug builds.
        //
        vector<UINT32> tmp((size_t)(length >> 2));
        for (UINT32 i = 0; i < (UINT32)tmp.size(); ++i)
        {
            tmp[i] = seed + i;
        }
        Platform::VirtualWr(pMem, &tmp[0], (UINT32)length);
    }

    // If upper_bound() is not the first chunk, need to start one before.
    map<UINT64, Chunk>::iterator it = m_chunks.upper_bound(offset);
    if (it != m_chunks.begin())
    {
        --it;
    }

    // Loop over all chunks that can overlap with the new one.
    for (; it != m_chunks.end(); ++it)
    {
        const UINT64 off = it->first;
        const UINT64 lim = off + it->second.length - 1;

        // If before the new chunk, skip.
        if (lim < offset)
        {
            continue;
        }
        // If past the new chunk, done.
        if (off > limit)
        {
            break;
        }
        // If overlap with end of new chunk, split off end of old chunk.
        if (lim > limit)
        {
            const UINT64 endDelta = limit - off + 1;
            const Chunk chunk =
            {
                it->second.length - endDelta,
                (UINT32)(it->second.seed + (endDelta >> 2))
            };
            m_chunks[limit + 1] = chunk;
        }
        // If overlap with start of new chunk, truncate old chunk.
        if (off < offset)
        {
            it->second.length = offset - off;
        }
        // Otherwise remove old chunk.
        else
        {
            m_chunks.erase(it--);
        }
    }

    // Finally commit our new chunk.
    const Chunk chunk = {length, seed};
    m_chunks[offset] = chunk;

    return rc;
}

//! Copy a range of chunks from one physical allocation
//! to another.
//! The source and destination ranges may not overlap, although
//! they may reside within the same allocation.
//-----------------------------------------------------------------------------
RC MemAllocPhys::CopyChunks(const UINT64 offsetDst, const UINT64 offsetSrc,
                            const UINT64 length, const MemAllocPhys *pSrc)
{
    RC rc;
    const UINT64 limitDst = offsetDst + length - 1;
    const UINT64 limitSrc = offsetSrc + length - 1;

    // Check bounds.
    (void)limitDst; // force usage
    MASSERT(limitDst < m_params.size);
    MASSERT(limitSrc < pSrc->m_params.size);

    // Check overlap.
    if (pSrc == this)
    {
        MASSERT(!((offsetSrc <= offsetDst) && (offsetDst <= limitSrc)));
        MASSERT(!((offsetDst <= offsetSrc) && (offsetSrc <= limitDst)));
    }

    // If upper_bound() is not the first chunk, need to start one before.
    map<UINT64, Chunk>::const_iterator it = pSrc->m_chunks.upper_bound(offsetSrc);
    if (it != pSrc->m_chunks.begin())
    {
        --it;
    }

    // Loop over all source chunks in range.
    for (; it != pSrc->m_chunks.end(); ++it)
    {
        // Clamp to the source range.
        const UINT64 offSrc = MAX(offsetSrc, it->first);
        const UINT64 limSrc = MIN(limitSrc, it->first + it->second.length - 1);

        // If before the source range, skip.
        if (limSrc < offsetSrc)
        {
            continue;
        }
        // If past the source range, done.
        if (offSrc > limitSrc)
        {
            break;
        }

        // Adjust offsets for destination chunk.
        const UINT64 offDst = offsetDst + (offSrc - offsetSrc);
        const UINT64 lenDst = limSrc - offSrc + 1;
        const UINT32 delta  = (UINT32)(offSrc - it->first) >> 2;
        CHECK_RC(AddChunk(offDst, lenDst, it->second.seed + delta));
    }

    return rc;
}

//! Verify that the chunks of memory lwrrently tracked contain
//! their expected values (read through BAR1 CPU mapping).
//-----------------------------------------------------------------------------
RC MemAllocPhys::VerifyChunks()
{
    RC rc;

    if (FLD_TEST_DRF(OS32, _ATTR, _LOCATION, _PCI, m_params.attr) &&
        FLD_TEST_DRF(OS32, _ATTR2, _GPU_CACHEABLE, _YES, m_params.attr2))
    {
        m_pGpuDev->GetSubdevice(0)->IlwalidateL2Cache(0);
    }

    // Verify each chunk against its expected sequence.
    for (map<UINT64, Chunk>::iterator it = m_chunks.begin();
         it != m_chunks.end(); ++it)
    {
        const UINT64 offset = it->first;
        const UINT64 length = it->second.length;

        // Map chunk to BAR1.
        unique_ptr<MemMapCpu> pMap(new MemMapCpu(m_pLwRm, m_pGpuDev,
                                               this, offset, length));
        CHECK_RC(pMap->Create());
        const UINT08 *pMem = (const UINT08*)pMap->GetPtr();

        // This might take a while - print something to minimize suspicion.
        Printf(Tee::PriHigh,
               "%s(%u): Verifying chunk 0x%llX-0x%llX = 0x%X...\n",
               GetTypeName(), GetID(), offset,
               offset + length - 1, it->second.seed);

        //
        // Batch the BAR1 reads into a single call
        // by storing the entire chunk in a temporary buffer.
        //
        // This improves performance for simulation debug builds.
        //
        vector<UINT32> tmp((size_t)(length >> 2));
        Platform::VirtualRd(pMem, &tmp[0], (UINT32)length);

        for (UINT32 i = 0; i < (UINT32)tmp.size(); ++i)
        {
            const UINT32 actual = tmp[i];
            const UINT32 expect = it->second.seed + i;
            if (actual != expect)
            {
                Printf(Tee::PriHigh,
                    "%s(%u): Chunk miscompare at offset 0x%llX, "
                    "0x%X (actual) != 0x%X (expected)\n",
                    GetTypeName(), GetID(), offset + (i << 2), actual, expect);
                Xp::BreakPoint();
                return RC::DATA_MISMATCH;
            }
        }
    }

    return rc;
}

