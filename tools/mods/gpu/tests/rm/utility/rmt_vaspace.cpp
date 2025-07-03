/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <algorithm>
#include "core/include/utility.h"
#include "lwos.h"
#include "core/include/lwrm.h"
#include "gpu/include/gpudev.h"
#include "random.h"
#include "core/include/channel.h"
#include "gpu/include/gralloc.h"

#include "ctrl/ctrl0080.h" // LW01_DEVICE_XX/LW03_DEVICE
#include "class/cl90f1.h"       // FERMI_VASPACE_A
#include "ctrl/ctrl90f1.h"  // FERMI_VASPACE_A
#include "class/cl90b5.h"       // GF100_DMA_COPY
#include "class/cla06fsubch.h"  // LWA06F_SUBCHANNEL_COPY_ENGINE

#include "mmu/gmmu_fmt.h"

#include "gpu/tests/rmtest.h"
#include "rmtestutils.h"
#include "rmt_flagutil.h"
#include "rmt_memalloc.h"
#include "rmt_memmap.h"
#include "rmt_vaspace.h"

#include "core/include/memcheck.h"

//! Value we will tell CE to write to semaphore.
#define SEM_VALUE 0xCAFEBABE

//! Size for copy engine semaphore surface. We only need 4 bytes. Oh well.
#define SEM_SURF_SIZE 4096

// Defined locally to mitigate global warming.
#define KB 1024ULL
#define MB (1024ULL * KB)
#define GB (1024ULL * MB)
#define TB (1024ULL * GB)
#define TO_GB(bytes) ((UINT32)((bytes) / GB))

using namespace rmt;

//-----------------------------------------------------------------------------
VASpace::VASpace(LwRm *pLwRm, GpuDevice *pGpuDev, VASTracker *pTracker,
                 VASpace *pParent, UINT32 vmmRegkey, const UINT32 id) :
    m_pLwRm(pLwRm),
    m_pGpuDev(pGpuDev),
    m_vmmRegkey(vmmRegkey),
    m_id(id),
    m_handle(0),
    m_pParent(pParent),
    m_pTracker(pTracker),
    m_bSubsetCommitted(false)
{
    memset(&m_params, 0, sizeof(m_params));
    memset(&m_caps, 0, sizeof(m_caps));

    RC rc;
    rc = pLwRm->ControlByDevice(pGpuDev, LW0080_CTRL_CMD_DMA_ADV_SCHED_GET_VA_CAPS,
                                &m_caps, sizeof(m_caps));
    MASSERT(OK == rc);
}

//! Destroy a VAS, but first destroy all its virtual allocations.
//-----------------------------------------------------------------------------
VASpace::~VASpace()
{
    SubsetRevoke();
    for (set<VASpace*>::iterator it = m_subsets.begin();
         it != m_subsets.end(); ++it)
    {
        (*it)->SubsetRevoke();
        (*it)->m_pSubset.reset();
        (*it)->m_pParent = NULL;
        m_pTracker->subsets.erase(*it);
    }
    if (m_handle != 0)
    {
        while (!m_allocs.empty())
        {
            Randomizable::deleter(m_allocs.getAny());
        }
        m_pLwRm->Free(m_handle);
        m_pTracker->all.erase(this);
        if (m_pParent != NULL)
        {
            m_pTracker->subsets.erase(this);
            m_pParent->m_subsets.erase(this);
        }
        else
        {
            m_pTracker->parentable.erase(this);
        }
    }
}

//! Flags that will be randomized for VAS creation.
//-----------------------------------------------------------------------------
static const BitFlags &VASFlags()
{
    static BitFlags util;
    STATIC_BIT_FLAG(&util, 0.50, LW_VASPACE_ALLOCATION_FLAGS_MINIMIZE_PTETABLE_SIZE);
    STATIC_BIT_FLAG(&util, 0.50, LW_VASPACE_ALLOCATION_FLAGS_RETRY_PTE_ALLOC_IN_SYS);
    STATIC_BIT_FLAG(&util, 0.10, LW_VASPACE_ALLOCATION_FLAGS_SHARED_MANAGEMENT);
    return util;
}

//! @sa Randomizable::Randomize
//-----------------------------------------------------------------------------
void VASpace::Randomize(const UINT32 seed)
{
    MASSERT(m_handle == 0);

    RandomInit rand(seed);

    memset(&m_params, 0, sizeof(m_params));

    m_params.flags = VASFlags().GetRandom(&rand);

    if (m_pParent != NULL)
    {
        RC rc;

        unique_ptr<MemAllocVirt> pSubset(new MemAllocVirt(m_pLwRm, m_pGpuDev, m_pParent, 0));
        pSubset->Randomize(rand.GetRandom());

        MemAlloc::Params subsetParams = pSubset->GetParams();
        subsetParams.flags |= LWOS32_ALLOC_FLAGS_EXTERNALLY_MANAGED;
        subsetParams.flags |= LWOS32_ALLOC_FLAGS_ALIGNMENT_FORCE;

        // Get caps of parent VAS for PDE bit coverage.
        LW90F1_CTRL_VASPACE_GET_GMMU_FORMAT_PARAMS fmtParams = {0};
        fmtParams.hSubDevice =
            m_pLwRm->GetSubdeviceHandle(m_pGpuDev->GetSubdevice(0));

        rc = m_pLwRm->Control(m_pParent->GetHandle(),
                              LW90F1_CTRL_CMD_VASPACE_GET_GMMU_FORMAT,
                              &fmtParams, sizeof(fmtParams));
        MASSERT(rc == OK);
        const GMMU_FMT *pFmt = (const GMMU_FMT *) LwP64_VALUE(fmtParams.pFmt);

        subsetParams.alignment = mmuFmtLevelPageSize(pFmt->pRoot);
        subsetParams.size = LW_ALIGN_UP(subsetParams.size,
                                        subsetParams.alignment);
        pSubset->SetParams(&subsetParams);

        if (pSubset->CheckValidity() == NULL)
        {
            Platform::DisableBreakPoints nobp;
            rc = pSubset->Create();
            if (rc == OK)
            {
                m_params.vaBase = pSubset->GetParams().offset;
                m_params.vaSize = m_params.vaBase + pSubset->GetParams().size;
                m_params.bigPageSize = UNSIGNED_CAST(LwU32, m_pParent->GetBigPageSize());

                // Commit to subset.
                m_pSubset.reset(pSubset.release());

                // TODO: Randomize for negative testing.
                m_params.flags |= LW_VASPACE_ALLOCATION_FLAGS_SHARED_MANAGEMENT;

                return;
            }
        }
        m_pParent = NULL;
    }

    // Pick big page size if supported.
    UINT64 bigPageSize;
    if (rand.GetRandom(0, 100) < 25)
    {
        if (rand.GetRandom(0, 100) < 25)
        {
            bigPageSize = 128 * KB;
        }
        else
        {
            bigPageSize = 64 * KB;
        }
        m_params.bigPageSize = UNSIGNED_CAST(LwU32, bigPageSize);
    }
    else
    {
        bigPageSize = m_pGpuDev->GetBigPageSize();
    }

    // Somtimes pick a size.
    if (rand.GetRandom(0, 100) < 75)
    {
        VASpaceResizer resizer(m_pLwRm, m_pGpuDev, this);
        resizer.Randomize(rand.GetRandom());
        m_params.vaSize = resizer.GetParams().size;
    }

    // Occassionally pick a base.
    if (rand.GetRandom(0, 100) < 15)
    {
        if (rand.GetRandom(0, 100) < 95)
        {
            UINT64 maxBase = (m_params.vaSize ? m_params.vaSize : BIT64(m_caps.vaBitCount)) - 1;
            m_params.vaBase = rand.GetRandom64(1, maxBase);

            // Usually align to big page size.
            if (rand.GetRandom(0, 100) < 95)
            {
                m_params.vaBase = LW_ALIGN_UP(m_params.vaBase, bigPageSize);
            }
        }
        else
        {
            m_params.vaBase = rand.GetRandom64();
        }
    }
}

//! @sa Randomizable::CheckValidity
//-----------------------------------------------------------------------------
const char *VASpace::CheckValidity() const
{
    if (1 != m_vmmRegkey)
    {
        UINT64 bigPageSize = GetBigPageSize();
        if (m_params.vaBase & (bigPageSize - 1))
        {
            return "base is not aligned to big page size";
        }
    }

    if (m_params.bigPageSize != 0)
    {
        RC rc;
        LwRm::Handle hDefVAS;

        LW_VASPACE_ALLOCATION_PARAMETERS allocParams = {0};
        allocParams.index = LW_VASPACE_ALLOCATION_INDEX_GPU_DEVICE;

        rc = m_pLwRm->Alloc(m_pLwRm->GetDeviceHandle(m_pGpuDev), &hDefVAS,
                            FERMI_VASPACE_A, &allocParams);
        MASSERT(rc == OK);

        LW90F1_CTRL_VASPACE_GET_GMMU_FORMAT_PARAMS fmtParams = {0};
        fmtParams.hSubDevice =
            m_pLwRm->GetSubdeviceHandle(m_pGpuDev->GetSubdevice(0));

        rc = m_pLwRm->Control(hDefVAS,
                              LW90F1_CTRL_CMD_VASPACE_GET_GMMU_FORMAT,
                              &fmtParams, sizeof(fmtParams));
        MASSERT(rc == OK);
        const GMMU_FMT *pFmt = (const GMMU_FMT *) LwP64_VALUE(fmtParams.pFmt);

        m_pLwRm->Free(hDefVAS);

        if (pFmt->version == GMMU_FMT_VERSION_1)
        {
            const UINT64 defBigPageSize = m_pGpuDev->GetBigPageSize();
            if (!m_pGpuDev->GetSubdevice(0)->HasFeature(
                    Device::GPUSUB_SUPPORTS_PER_VASPACE_BIG_PAGE) &&
                (m_params.bigPageSize != defBigPageSize))
            {
                return "invalid big page size";
            }
        }
        else if (m_params.bigPageSize != 64 * KB)
        {
            return "invalid big page size";
        }
    }

    return CheckSize(m_params.vaSize);
}

//! @sa Randomizable::CheckValidity
//-----------------------------------------------------------------------------
const char *VASpace::CheckSize(UINT64 size) const
{
    const LwU64 actualBase = m_params.vaBase ? m_params.vaBase : m_caps.vaRangeLo;
    const LwU64 actualSize = size            ? size            : BIT64(m_caps.vaBitCount);
    if (actualBase >= actualSize)
    {
        return "base is larger than limit";
    }
    if (actualSize > BIT64(m_caps.vaBitCount))
    {
        return "maximum VAS size exceeded";
    }
    if (actualSize < m_params.vaSize)
    {
        return "shrinking VAS is not supported";
    }
    if (1 != m_vmmRegkey)
    {
        UINT64 bigPageSize = GetBigPageSize();
        if (size & (bigPageSize - 1))
        {
            return "size is not aligned to big page size";
        }
    }
    return NULL;
}

//! @sa Randomizable::PrintParams
//-----------------------------------------------------------------------------
void VASpace::PrintParams(const INT32 pri) const
{
    if (m_pParent != NULL)
    {
        Printf(pri,
            "    parent: VASpace(%u)\n", m_pParent->GetID());
    }
    Printf(pri,
        "    vaBase: 0x%llX\n", m_params.vaBase);
    Printf(pri,
        "    vaSize: 0x%llX\n", m_params.vaSize);
    VASFlags().Print(pri,
        "     flags: ", m_params.flags);
}

//-----------------------------------------------------------------------------
UINT64 VASpace::GetBigPageSize() const
{
    UINT64 bigPageSize;
    if (m_params.bigPageSize)
    {
        bigPageSize = m_params.bigPageSize;
    }
    else
    {
        bigPageSize = m_pGpuDev->GetBigPageSize();
    }
    return bigPageSize;
}

//! @sa Randomizable::Create
//-----------------------------------------------------------------------------
RC VASpace::Create()
{
    RC rc;
    MASSERT(m_handle == 0);
    rc = m_pLwRm->Alloc(m_pLwRm->GetDeviceHandle(m_pGpuDev), &m_handle,
                        FERMI_VASPACE_A, &m_params);
    if (rc == OK)
    {
        // NOTE: Heap size does not count space up to base.
        m_pHeap.reset(new Heap(m_params.vaBase,
                               BIT64(m_caps.vaBitCount) - m_params.vaBase,
                               m_pGpuDev->GetSubdevice(0)->GetGpuInst()));
        if ((0 < m_params.vaSize) && (m_params.vaSize < BIT64(m_caps.vaBitCount)))
        {
            CHECK_RC(m_pHeap->AllocAt(BIT64(m_caps.vaBitCount) - m_params.vaSize,
                                      m_params.vaSize));
        }

        if (m_pParent != NULL)
        {
            m_pTracker->subsets.insert(this);
            m_pParent->m_subsets.insert(this);
        }
        else
        {
            m_pTracker->parentable.insert(this);
        }
        m_pTracker->all.insert(this);
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC VASpace::Resize(UINT64 *pSize)
{
    RC rc;
    LW0080_CTRL_DMA_SET_VA_SPACE_SIZE_PARAMS params = { 0 };

    MASSERT(m_handle != 0);

    params.vaSpaceSize = *pSize;
    params.hVASpace    = m_handle;

    CHECK_RC(m_pLwRm->Control(m_pLwRm->GetDeviceHandle(m_pGpuDev),
                              LW0080_CTRL_CMD_DMA_SET_VA_SPACE_SIZE,
                              &params, sizeof(params)));

    // Get info about old/new VAS size.
    UINT64 maxSize = m_pHeap->GetSize();
    UINT64 oldSize = m_params.vaSize;
    UINT64 newSize = m_pHeap->GetBase() + params.vaSpaceSize;

    // Adjust the reserved heap block as necessary.
    if (oldSize < maxSize)
    {
        CHECK_RC(m_pHeap->Free(oldSize));
    }
    if (newSize < maxSize)
    {
        CHECK_RC(m_pHeap->AllocAt(maxSize - newSize, newSize));
    }

    // Commit the new size.
    m_params.vaSize = newSize;
    *pSize          = newSize;

    // Propogate new page directory to subset VA spaces.
    for (set<VASpace*>::iterator it = m_subsets.begin();
         it != m_subsets.end(); ++it)
    {
        if ((*it)->m_bSubsetCommitted)
        {
            (*it)->SubsetCommit();
        }
    }

    return rc;
}

//! Commit shared ownership of the page directory with parent VAS.
//-----------------------------------------------------------------------------
RC VASpace::SubsetCommit()
{
    RC rc;

    MASSERT(m_pParent != NULL);
    MASSERT(m_handle != 0);

    // Get caps of parent VAS for PDE bit coverage.
    LW90F1_CTRL_VASPACE_GET_GMMU_FORMAT_PARAMS fmtParams = {0};
    fmtParams.hSubDevice =
        m_pLwRm->GetSubdeviceHandle(m_pGpuDev->GetSubdevice(0));

    CHECK_RC(m_pLwRm->Control(m_pParent->GetHandle(),
                              LW90F1_CTRL_CMD_VASPACE_GET_GMMU_FORMAT,
                              &fmtParams, sizeof(fmtParams)));
    const GMMU_FMT *pFmt = (const GMMU_FMT *) LwP64_VALUE(fmtParams.pFmt);

    // Get page directory info of parent VAS.
    LW0080_CTRL_DMA_GET_PDE_INFO_PARAMS getParams = {0};
    getParams.hVASpace = m_pParent->GetHandle();
    getParams.gpuAddr  = m_pSubset->GetParams().offset;

    CHECK_RC(m_pLwRm->Control(m_pLwRm->GetDeviceHandle(m_pGpuDev),
                              LW0080_CTRL_CMD_DMA_GET_PDE_INFO,
                              &getParams, sizeof(getParams)));

    // Translate to set page directory params.
    LW0080_CTRL_DMA_SET_PAGE_DIRECTORY_PARAMS setParams = {0};
    setParams.hVASpace    = m_handle;
    setParams.physAddress = getParams.pdbAddr;
    setParams.numEntries  = mmuFmtVirtAddrToEntryIndex(pFmt->pRoot,
                                m_pParent->GetParams().vaSize - 1);
    setParams.flags       = FLD_SET_DRF(0080_CTRL_DMA_SET_PAGE_DIRECTORY, _FLAGS,
                                        _ALL_CHANNELS, _TRUE, setParams.flags);

    switch (getParams.pdeAddrSpace)
    {
        case LW0080_CTRL_DMA_GET_PDE_INFO_PARAMS_PDE_ADDR_SPACE_VIDEO_MEMORY:
            setParams.flags = FLD_SET_DRF(0080_CTRL_DMA_SET_PAGE_DIRECTORY, _FLAGS,
                                          _APERTURE, _VIDMEM, setParams.flags);
            break;
        case LW0080_CTRL_DMA_GET_PDE_INFO_PARAMS_PDE_ADDR_SPACE_SYSTEM_COHERENT_MEMORY:
            setParams.flags = FLD_SET_DRF(0080_CTRL_DMA_SET_PAGE_DIRECTORY, _FLAGS,
                                          _APERTURE, _SYSMEM_COH, setParams.flags);
            break;
        case LW0080_CTRL_DMA_GET_PDE_INFO_PARAMS_PDE_ADDR_SPACE_SYSTEM_NON_COHERENT_MEMORY:
            setParams.flags = FLD_SET_DRF(0080_CTRL_DMA_SET_PAGE_DIRECTORY, _FLAGS,
                                          _APERTURE, _SYSMEM_NONCOH, setParams.flags);
            break;
    }

    // Commit parent VAS page directory to subset VAS.
    CHECK_RC(m_pLwRm->Control(m_pLwRm->GetDeviceHandle(m_pGpuDev),
                              LW0080_CTRL_CMD_DMA_SET_PAGE_DIRECTORY,
                              &setParams, sizeof(setParams)));

    m_bSubsetCommitted = true;

    return rc;
}

//! Revoke shared ownership of the page directory with parent VAS.
//-----------------------------------------------------------------------------
RC VASpace::SubsetRevoke()
{
    RC rc;

    if (!m_bSubsetCommitted)
    {
        return rc;
    }

    // Translate to set page directory params.
    LW0080_CTRL_DMA_UNSET_PAGE_DIRECTORY_PARAMS params = {0};
    params.hVASpace = m_handle;

    // Commit parent VAS page directory to subset VAS.
    CHECK_RC(m_pLwRm->Control(m_pLwRm->GetDeviceHandle(m_pGpuDev),
                              LW0080_CTRL_CMD_DMA_UNSET_PAGE_DIRECTORY,
                              &params, sizeof(params)));

    m_bSubsetCommitted = false;

    return rc;
}

//! Override the paremters of this VAS.
//-----------------------------------------------------------------------------
void VASpace::SetParams(const Params *pParams)
{
    MASSERT(m_handle == 0);
    memcpy(&m_params, pParams, sizeof(m_params));
}

//! Register a virtual allocation with this VAS.
//! @sa MemAllocVirt::Create
//-----------------------------------------------------------------------------
void VASpace::AddAlloc(MemAllocVirt *pVirt)
{
    if (pVirt->GetID() != 0)
    {
        m_allocs.insert(pVirt);
        if (m_allocs.size() == 1)
        {
            m_pTracker->hasAllocs.insert(this);
        }
        ++m_pTracker->numAllocs;
    }
    const MemAlloc::Params &params = pVirt->GetParams();
    m_pHeap->AllocAt(params.size, params.offset);
    // TODO: Use real RM heap dump instead?
}

//! Unregister a virtual allocation from this VAS.
//! @sa MemAllocVirt::~MemAllocVirt
//-----------------------------------------------------------------------------
void VASpace::RemAlloc(MemAllocVirt *pVirt)
{
    const MemAlloc::Params &params = pVirt->GetParams();
    m_pHeap->Free(params.offset);
    if (pVirt->GetID() != 0)
    {
        m_allocs.erase(pVirt);
        if (m_allocs.size() == 0)
        {
            m_pTracker->hasAllocs.erase(this);
        }
        --m_pTracker->numAllocs;
    }
}

//! Register a mapping with this VAS.
//! @sa MemMapGpu::Create
//-----------------------------------------------------------------------------
void VASpace::AddMapping(MemMapGpu *pMap)
{
    if (pMap->GetID() != 0)
    {
        m_mappings.insert(pMap);
        if (m_mappings.size() == 1)
        {
            m_pTracker->hasMaps.insert(this);
        }
        ++m_pTracker->numMaps;
    }
}

//! Unregister a mapping from this VAS.
//! @sa MemMapGpu::~MemMapGpu
//-----------------------------------------------------------------------------
void VASpace::RemMapping(MemMapGpu *pMap)
{
    if (pMap->GetID() != 0)
    {
        m_mappings.erase(pMap);
        if (m_mappings.size() == 0)
        {
            m_pTracker->hasMaps.erase(this);
        }
        --m_pTracker->numMaps;
    }
}

//! Private implemenation of the abstract VA space channel tester interface.
//-----------------------------------------------------------------------------
class VASChanTester : public VASpace::ChannelTester
{
public:
    VASChanTester(LwRm *pLwRm, GpuDevice *pGpuDev,
                  GpuTestConfiguration *pTestConfig, VASpace *pVAS);
    virtual ~VASChanTester();

    RC Create();
    virtual RC MemCopy(const UINT64 dstVA, const UINT64 srcVA,
                       const UINT32 length);

 private:
    enum
    {
        CopySubCh = LWA06F_SUBCHANNEL_COPY_ENGINE
    };

    LwRm                     *m_pLwRm;
    GpuDevice                *m_pGpuDev;
    GpuTestConfiguration     *m_pTestConfig;
    VASpace                  *m_pVAS;
    ::Channel                *m_pChan;
    LwRm::Handle              m_hChan;
    DmaCopyAlloc              m_CopyAlloc;
    ModsEvent                *m_pCopyEvent;
    LwRm::Handle              m_hCopyEvent;
    unique_ptr<MemAllocPhys>  m_pSemPhys;
    unique_ptr<MemAllocVirt>  m_pSemVirt;
    unique_ptr<MemMapCpu>     m_pSemMapCpu;
    unique_ptr<MemMapGpu>     m_pSemMapGpu;
};

//-----------------------------------------------------------------------------
VASChanTester::VASChanTester(LwRm *pLwRm, GpuDevice *pGpuDev,
                             GpuTestConfiguration *pTestConfig, VASpace *pVAS) :
    m_pLwRm(pLwRm),
    m_pGpuDev(pGpuDev),
    m_pTestConfig(pTestConfig),
    m_pVAS(pVAS),
    m_pChan(NULL),
    m_hChan(0),
    m_pCopyEvent(NULL),
    m_hCopyEvent(0)
{
}

//-----------------------------------------------------------------------------
VASChanTester::~VASChanTester()
{
    StickyRC rc;

    if (m_hCopyEvent)
        m_pLwRm->Free(m_hCopyEvent);
    if (m_pCopyEvent)
        Tasker::FreeEvent(m_pCopyEvent);

    m_CopyAlloc.Free();

    if (m_pChan)
        rc = m_pTestConfig->FreeChannel(m_pChan);

    MASSERT(rc == OK);
}

//! Create the channel and semaphore associated with a VAS.
//-----------------------------------------------------------------------------
RC VASChanTester::Create()
{
    RC rc;

    MASSERT(m_hChan == 0);

    // Allocate channel bound to our VA space.
    CHECK_RC(m_pTestConfig->AllocateChannel(&m_pChan, &m_hChan, nullptr, nullptr,
                m_pVAS->GetHandle(), 0, LW2080_ENGINE_TYPE_COPY(0)));

    // Allocate copy engine object bound to our channel.
    CHECK_RC(m_CopyAlloc.Alloc(m_hChan, m_pGpuDev, m_pLwRm));
    CHECK_RC(m_pChan->SetObject(CopySubCh, m_CopyAlloc.GetHandle()));

    // Allocate (auto-resetting) event associated with our copy engine.
    m_pCopyEvent = Tasker::AllocEvent(0, false);
    void* const pOsEvent = Tasker::GetOsEvent(
            m_pCopyEvent,
            m_pLwRm->GetClientHandle(),
            m_pLwRm->GetDeviceHandle(m_pGpuDev));
    CHECK_RC(m_pLwRm->AllocEvent(m_CopyAlloc.GetHandle(),
                                 &m_hCopyEvent,
                                 LW01_EVENT_OS_EVENT,
                                 0,
                                 pOsEvent));

    // Allocate semaphore memory.
    m_pSemPhys.reset(new MemAllocPhys(m_pLwRm, m_pGpuDev, 1, 0));
    MemAlloc::Params physParams = { 0 };
    physParams.flags = LWOS32_ALLOC_FLAGS_NO_SCANOUT |
                       LWOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED;
    physParams.size = SEM_SURF_SIZE;
    physParams.attr = DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _4KB);
    m_pSemPhys->SetParams(&physParams);
    CHECK_RC(m_pSemPhys->Create());

    m_pSemVirt.reset(new MemAllocVirt(m_pLwRm, m_pGpuDev, m_pVAS));
    MemAlloc::Params virtParams = { 0 };
    virtParams.flags = LWOS32_ALLOC_FLAGS_VIRTUAL;
    virtParams.size = SEM_SURF_SIZE;
    virtParams.attr = DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _4KB);
    m_pSemVirt->SetParams(&virtParams);
    CHECK_RC(m_pSemVirt->Create());

    // Map semaphore memory.
    m_pSemMapCpu.reset(new MemMapCpu(m_pLwRm, m_pGpuDev,
                                     m_pSemPhys.get(), 0, SEM_SURF_SIZE));
    CHECK_RC(m_pSemMapCpu->Create());

    m_pSemMapGpu.reset(new MemMapGpu(m_pLwRm, m_pGpuDev,
                                     m_pSemVirt.get(), m_pSemPhys.get()));
    MemMapGpu::Params mapParams = { 0 };
    mapParams.length = SEM_SURF_SIZE;
    m_pSemMapGpu->SetParams(&mapParams);
    CHECK_RC(m_pSemMapGpu->Create());

    return rc;
}

//! Use copy engine to copy between two virtual ranges.
//-----------------------------------------------------------------------------
RC VASChanTester::MemCopy(UINT64 dstVA, UINT64 srcVA, UINT32 length)
{
    RC      rc;
    UINT64  semGpuAddr = m_pSemMapGpu->GetVirtAddr();
    void   *pSemCpu    = m_pSemMapCpu->GetPtr();
    FLOAT64 timeoutMs  = m_pTestConfig->TimeoutMs();

    //
    // This is needed for the WaitOnEvent call below to prevent false timeouts
    // on FMODEL (default TestConfig timeout is 1 second).
    // Other time related utilities appear to scale automatically for simulation,
    // so it is unclear why this does not.
    //
    // TODO: Investigate and fix generally.
    //
    // Update: Now callwlating additional time based on length of copy.
    //         Observed CE throughput on gk10x fmodel of about 8KB/s.
    //         Using 1KB/s as a lower bound to account for CPU/sim variation.
    //
    if ((Platform::GetSimulationMode() != Platform::Hardware))
    {
        timeoutMs += length;
        Printf(Tee::PriHigh,
              "%s: Using increased timeout of %f ms on simulation.\n",
              __FUNCTION__, timeoutMs);
    }

    // Clear semaphore.
    MEM_WR32(pSemCpu, 0);
    CHECK_RC(Platform::FlushCpuWriteCombineBuffer());

    // Schedule channel.
    CHECK_RC(m_pChan->ScheduleChannel(true));

    // Setup source and destination addresses.
    CHECK_RC(m_pChan->Write(CopySubCh, LW90B5_OFFSET_IN_UPPER, LwU64_HI32(srcVA)));
    CHECK_RC(m_pChan->Write(CopySubCh, LW90B5_OFFSET_IN_LOWER, LwU64_LO32(srcVA)));

    CHECK_RC(m_pChan->Write(CopySubCh, LW90B5_OFFSET_OUT_UPPER, LwU64_HI32(dstVA)));
    CHECK_RC(m_pChan->Write(CopySubCh, LW90B5_OFFSET_OUT_LOWER, LwU64_LO32(dstVA)));

    // Setup layout and size (pitch with single line).
    CHECK_RC(m_pChan->Write(CopySubCh, LW90B5_PITCH_IN,  0));
    CHECK_RC(m_pChan->Write(CopySubCh, LW90B5_PITCH_OUT, 0));

    CHECK_RC(m_pChan->Write(CopySubCh, LW90B5_LINE_COUNT,     1));
    CHECK_RC(m_pChan->Write(CopySubCh, LW90B5_LINE_LENGTH_IN, length));

    CHECK_RC(m_pChan->Write(CopySubCh, LW90B5_SET_SRC_DEPTH,  1));
    CHECK_RC(m_pChan->Write(CopySubCh, LW90B5_SET_SRC_LAYER,  0));
    CHECK_RC(m_pChan->Write(CopySubCh, LW90B5_SET_SRC_WIDTH,  length));
    CHECK_RC(m_pChan->Write(CopySubCh, LW90B5_SET_SRC_HEIGHT, 1));

    // Setup semaphore address and payload.
    CHECK_RC(m_pChan->Write(CopySubCh, LW90B5_SET_SEMAPHORE_PAYLOAD, SEM_VALUE));
    CHECK_RC(m_pChan->Write(CopySubCh, LW90B5_SET_SEMAPHORE_A,
                 DRF_NUM(90B5, _SET_SEMAPHORE_A, _UPPER, LwU64_HI32(semGpuAddr))));
    CHECK_RC(m_pChan->Write(CopySubCh, LW90B5_SET_SEMAPHORE_B,
                 DRF_NUM(90B5, _SET_SEMAPHORE_B, _LOWER, LwU64_LO32(semGpuAddr))));

    // Trigger the copy.
    CHECK_RC(m_pChan->Write(CopySubCh, LW90B5_LAUNCH_DMA,
                 DRF_DEF(90B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, _RELEASE_ONE_WORD_SEMAPHORE) |
                 DRF_NUM(90B5, _LAUNCH_DMA, _INTERRUPT_TYPE,
                    LW90B5_LAUNCH_DMA_INTERRUPT_TYPE_BLOCKING) |
                 DRF_DEF(90B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE) |
                 DRF_DEF(90B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH) |
                 DRF_DEF(90B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH) |
                 DRF_DEF(90B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _NON_PIPELINED)));

    CHECK_RC(m_pChan->Flush());

    // Wait for CE interrupt.
    CHECK_RC(Tasker::WaitOnEvent(m_pCopyEvent, timeoutMs));

    // Wait for idle and unschedule channel.
    CHECK_RC(m_pChan->WaitIdle(timeoutMs));
    CHECK_RC(m_pChan->ScheduleChannel(false));

    // Sanity-check the semaphore.
    UINT32 semVal = MEM_RD32(pSemCpu);
    if (semVal != SEM_VALUE)
    {
        Printf(Tee::PriHigh,
              "%s: expected semaphore to be 0x%x, was 0x%x\n",
              __FUNCTION__, SEM_VALUE, semVal);
        Xp::BreakPoint();
        CHECK_RC(RC::DATA_MISMATCH);
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC VASpace::CreateChannel(GpuTestConfiguration *pTestConfig,
                          unique_ptr<VASpace::ChannelTester> *ppChan)
{
    RC                      rc;
    unique_ptr<VASChanTester> pChan;

    pChan.reset(new VASChanTester(m_pLwRm, m_pGpuDev, pTestConfig, this));
    CHECK_RC(pChan->Create());
    *ppChan = std::move(pChan);

    return rc;
}

//-----------------------------------------------------------------------------
VASpaceWithChannel::VASpaceWithChannel(LwRm *pLwRm, GpuDevice *pGpuDev,
                                       VASTracker *pTracker, VASpace *pParent,
                                       UINT32 vmmRegkey, const UINT32 id) :
    VASpace(pLwRm, pGpuDev, pTracker, pParent, vmmRegkey, id)
{
}

//! Create the associated tester channel.
//-----------------------------------------------------------------------------
RC VASpaceWithChannel::CreateChannel(GpuTestConfiguration *pTestConfig)
{
    return VASpace::CreateChannel(pTestConfig, &m_pChan);
}

//-----------------------------------------------------------------------------
VASpaceResizer::VASpaceResizer(LwRm *pLwRm, GpuDevice *pGpuDev, VASpace *pVAS) :
    m_pLwRm(pLwRm),
    m_pGpuDev(pGpuDev),
    m_pVAS(pVAS),
    m_bValid(false)
{
    memset(&m_params, 0, sizeof(m_params));
}

//-----------------------------------------------------------------------------
VASpaceResizer::~VASpaceResizer()
{
}

//! @sa Randomizable::Randomize
//-----------------------------------------------------------------------------
void VASpaceResizer::Randomize(const UINT32 seed)
{
    //
    // Weights for VA space size.
    //
    // Since the picker utility only supports 32-bit values,
    // split into GB-granulariy and byte-granulariy.
    // TODO: Template the picker?
    //
    static Random::PickItem SizePickerGB[] =
    {
       PI_RANGE(5,  TO_GB(0  * GB), TO_GB(0  * GB)),
       PI_RANGE(30, TO_GB(1  * GB), TO_GB(4  * GB)),
       PI_RANGE(40, TO_GB(4  * GB), TO_GB(64 * GB)),
       PI_RANGE(20, TO_GB(64 * GB), TO_GB(1  * TB)),
       PI_RANGE(5,  TO_GB(1  * TB), LW_U32_MAX)
    };
    STATIC_PI_PREP(SizePickerGB);

    // TODO: Don't hard-code 128KB big page PDE alignment.
    static Random::PickItem SizePickerBytes[] =
    {
       PI_RANGE(30, (0       ), (0       )),
       PI_RANGE(10, (128 * MB), (128 * MB)),
       PI_RANGE(10, (256 * MB), (256 * MB)),
       PI_RANGE(10, (384 * MB), (384 * MB)),
       PI_RANGE(10, (512 * MB), (512 * MB)),
       PI_RANGE(10, (640 * MB), (640 * MB)),
       PI_RANGE(10, (768 * MB), (768 * MB)),
       PI_RANGE(10, (896 * MB), (896 * MB)),
       PI_RANGE(5 , (1       ), (GB - 1  )),
    };
    STATIC_PI_PREP(SizePickerBytes);

    RandomInit rand(seed);

    memset(&m_params, 0, sizeof(m_params));

    m_params.size = (UINT64)rand.Pick(SizePickerGB) * GB;
    m_params.size += rand.Pick(SizePickerBytes);

    // Occasionally pick the max size special value.
    if (rand.GetRandom(0, 100) < 5)
    {
        m_params.size = LW0080_CTRL_DMA_SET_VA_SPACE_SIZE_MAX;
    }

    // Usually pick a growing size.
    if (m_params.size < m_pVAS->GetParams().vaSize)
    {
        if (rand.GetRandom(0, 100) < 85)
        {
            m_params.size = m_pVAS->GetParams().vaSize + (rand.GetRandom(0, 10) * 128 * MB);
        }
    }
}

//! @sa Randomizable::CheckValidity
//-----------------------------------------------------------------------------
const char *VASpaceResizer::CheckValidity() const
{
    if (m_pVAS->GetParams().flags & LW_VASPACE_ALLOCATION_FLAGS_SHARED_MANAGEMENT)
    {
        return "shared management VAS cannot be resized";
    }
    if (m_params.size == LW0080_CTRL_DMA_SET_VA_SPACE_SIZE_MAX)
    {
        return NULL;
    }
    if (m_params.size == 0)
    {
        return "size is 0";
    }
    return m_pVAS->CheckSize(m_params.size);
}

//! @sa Randomizable::PrintParams
//-----------------------------------------------------------------------------
void VASpaceResizer::PrintParams(const INT32 pri) const
{
    Printf(pri,
        "     vas: %s(%u)\n", m_pVAS->GetTypeName(), m_pVAS->GetID());
    Printf(pri,
        "    size: 0x%llX\n", m_params.size);
}

//! @sa Randomizable::Create
//-----------------------------------------------------------------------------
RC VASpaceResizer::Create()
{
    RC rc;

    CHECK_RC(m_pVAS->Resize(&m_params.size));
    m_bValid = true;

    return rc;
}

//! Override the paremters of this VAS resizer.
//-----------------------------------------------------------------------------
void VASpaceResizer::SetParams(const Params *pParams)
{
    memcpy(&m_params, pParams, sizeof(m_params));
}
