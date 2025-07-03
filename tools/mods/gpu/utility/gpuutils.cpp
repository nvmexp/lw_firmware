/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   utility.cpp
 * @brief  Implementation of the gpu specific portion of the Utility namespace
 * (utility.h)
 *
 * Also contains JavaScript Utility class.
 */

#include "gpuutils.h"
#include "blocklin.h"
#include "core/include/utility.h"
#include "Lwcm.h"
#include "gpu/tests/gputestc.h"
#include "core/include/lwrm.h"
#include "core/include/display.h"
#include "gpu/display/evo_disp.h"
#include "core/include/framebuf.h"
#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "lwos.h"
#include "core/include/channel.h"
#include "maxwell/gm200/dev_timer.h"
#include "gpu/include/notifier.h"
#include "surf2d.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/deprecat.h"
#include "core/include/platform.h"
#include "class/cle3f1.h" // TEGRA_VASPACE_A
#include "core/utility/goldutil.h" // GoldenUtil::SendTrigger
#include "gpu/include/gpumgr.h"
#include "core/include/mgrmgr.h"
#include "gpu/js_gpusb.h"
#include "gpu/include/dmawrap.h"
#include "core/include/xp.h"
#include "gpu/utility/pcie/pexdev.h"
#include "gpu/include/gralloc.h"
#include "core/include/cpu.h"
#include "gpu/include/testdevicemgr.h"
#include <vector>
#include "gpu/js_gpudv.h"

#if defined(TEGRA_MODS)
#include <regex>
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"
#endif

JS_CLASS(GpuUtils);

//! GpuUtils_Object
/**
 *
 */
/* JS_DOX */ SObject GpuUtils_Object
(
    "GpuUtils",
    GpuUtilsClass,
    0,
    0,
    "Graphics Processing Unit Utilities."
);

// Make GpuUtils functions callable from JavaScript:

namespace GpuUtility
{
    RC AllocateFramebufferChunk
    (
        MemoryChunkDesc * pChunk,
        UINT64            alignment,
        bool              blockLinear,
        UINT64            pageSize,
        LwRm::Handle      hChannel,
        GpuDevice       * pGpuDev,
        bool              contiguous
    );
    RC TryAllocateFramebufferChunk
    (
        MemoryChunkDesc * pChunk,
        UINT64            alignment,
        bool              blockLinear,
        UINT64            pageSize,
        UINT32            type,
        LwRm::Handle      hChannel,
        GpuDevice       * pGpuDev,
        bool              contiguous
    );
};

GpuUtility::DisplayImageDescription::DisplayImageDescription()
{
    Reset();
}

RC GpuUtility::DisplayImageDescription::Print(Tee::Priority Priority) const
{
    Printf(Priority, "   CtxDMAHandle   = 0x%08x\n", CtxDMAHandle);
    Printf(Priority, "   Offset         = 0x%llx\n", Offset);
    Printf(Priority, "   CtxDMAHandleRt = 0x%08x\n", CtxDMAHandleRight);
    Printf(Priority, "   OffsetRight    = 0x%llx\n", OffsetRight);
    Printf(Priority, "   Height         = %u\n", Height);
    Printf(Priority, "   Width          = %u\n", Width);
    Printf(Priority, "   AASamples      = %u\n", AASamples);
    Printf(Priority, "   Layout         = %s\n",
           (Layout==Surface2D::BlockLinear)? "BlockLinear":"Pitch" );
    Printf(Priority, "   Pitch          = %u\n", Pitch);
    Printf(Priority, "   LogBlockHeight = %u\n", LogBlockHeight);
    Printf(Priority, "   NumBlocksWidth = %u\n", NumBlocksWidth);
    Printf(Priority, "   ColorFormat    = %s\n", FormatToString(ColorFormat).c_str());

    return OK;
}

void GpuUtility::DisplayImageDescription::Reset()
{
    hMem = 0;
    CtxDMAHandle = 0;
    Offset = 0;
    hMemRight = 0;
    CtxDMAHandleRight = 0;
    OffsetRight = 0;
    Height = 0;
    Width = 0;
    AASamples = 0;
    Layout = Surface2D::Pitch;
    Pitch = 0;
    LogBlockHeight = 0;
    NumBlocksWidth = 0;
    ColorFormat = ColorUtils::LWFMT_NONE;
}

namespace
{
    bool PushContiguousChunk
    (
        GpuUtility::MemoryChunks*          pChunks,
        const GpuUtility::MemoryChunkDesc& origChunk,
        PHYSADDR                           startPa,
        UINT64                             beginOffs,
        UINT64                             size
    )
    {
        // Don't bother with chunks smaller than 256KB
        if (size < 256_KB)
        {
            return false;
        }

        pChunks->push_back(origChunk);
        auto& chunk = pChunks->back();

        chunk.ctxDmaOffsetGpu += beginOffs;
        chunk.size            =  size;
        chunk.contiguous      =  true;
        chunk.needsFree       =  false;
        chunk.mapOffset       =  beginOffs;
        chunk.fbOffset        =  startPa;

        Printf(Tee::PriLow, "  Using contiguous range offs=0x%llx PA=0x%08llx size=0x%llx VA=0x%08llx\n",
                            beginOffs, startPa, size, chunk.ctxDmaOffsetGpu);

#ifdef DEBUG
        for (UINT32 i = 1; i < pChunks->size(); i++)
        {
            const auto& other = (*pChunks)[i-1];
            MASSERT(other.ctxDmaOffsetGpu + other.size <= chunk.ctxDmaOffsetGpu ||
                    chunk.ctxDmaOffsetGpu + chunk.size <= other.ctxDmaOffsetGpu);
            MASSERT(other.fbOffset + other.size <= chunk.fbOffset ||
                    chunk.fbOffset + chunk.size <= other.fbOffset);
        }
#endif

        return true;
    }

    RC SplitContiguousChunks
    (
        GpuUtility::MemoryChunks*          pChunks,
        const GpuUtility::MemoryChunkDesc& origChunk,
        UINT64*                            pUsedSize
    )
    {
        RC     rc;
        UINT64 offs      = 0;
        UINT64 usedSize  = 0;

        while (offs < origChunk.size)
        {
            PHYSADDR pa = 0;
            UINT64 contigSize = 0;
            CHECK_RC(GpuUtility::GetPhysAddress(origChunk, offs, &pa, &contigSize));

            if (PushContiguousChunk(pChunks, origChunk, pa, offs, contigSize))
            {
                usedSize += contigSize;
            }

            offs += contigSize;
        }

        *pUsedSize = usedSize;

        return rc;
    }
}

RC GpuUtility::TryAllocateFramebufferChunk
(
    MemoryChunkDesc * pChunk,
    UINT64            alignment,
    bool              blockLinear,
    UINT64            pageSize,
    UINT32            type,
    LwRm::Handle      hChannel,
    GpuDevice       * pGpuDev,
    bool              contiguous
)
{
    LwRmPtr pLwRm;
    RC rc;
    UINT64 fbLimit  = 0;
    UINT32 attr     = (blockLinear ?
                        DRF_DEF(OS32, _ATTR, _FORMAT, _BLOCK_LINEAR) :
                        DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH));
    UINT32 flags    = LWOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED |
                        LWOS32_ALLOC_FLAGS_ALIGNMENT_FORCE;
    UINT32 attr2    = LWOS32_ATTR2_NONE;
    UINT32 mapAttr  = 0;

    GpuSubdevice* pSubDev = pGpuDev->GetSubdevice(0);
    const bool    isSoc   = pSubDev->IsSOC();

    if (isSoc)
    {
        // Allocate in sysmem
        attr |= DRF_DEF(OS32, _ATTR, _LOCATION, _PCI);
        if (pSubDev->HasFeature(Device::GPUSUB_HAS_COHERENT_SYSMEM))
        {
            // Cache the surfaces on the CPU on Tegras which support I/O coherency
            attr |= DRF_DEF(OS32, _ATTR, _COHERENCY, _CACHED);
        }
        else
        {
            // Use ARM CPU's store buffers for potentially faster memset
            attr |= DRF_DEF(OS32, _ATTR, _COHERENCY, _WRITE_COMBINE);
        }
        if (contiguous)
        {
            attr |= DRF_DEF(OS32, _ATTR, _PHYSICALITY, _CONTIGUOUS);
        }
        // Add coverage for L3 cache on CheetAh in memory tests
        mapAttr |= DRF_DEF(OS46, _FLAGS, _SYSTEM_L3_ALLOC, _ENABLE_HINT);
    }

    pChunk->isDisplayable = (type == LWOS32_TYPE_PRIMARY);

    // We don't support displaying these chunks with dGPU RM on CheetAh.
    // We could map them to TegraVASpace object, but we've never implemented it.
    if (isSoc)
    {
        pChunk->isDisplayable = false;
    }

    pChunk->contiguous = contiguous;

    if (!pGpuDev->SupportsPageSize(pageSize))
    {
        MASSERT(!"Unsupported page size!");
        return RC::SOFTWARE_ERROR;
    }
    pChunk->pageSizeKB = UNSIGNED_CAST(UINT32, pageSize / 1_KB);
    switch (pageSize)
    {
        case GpuDevice::PAGESIZE_4KB:
            attr |= DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _4KB);
            mapAttr |= DRF_DEF(OS46, _FLAGS, _PAGE_SIZE, _4KB);
            break;

        case GpuDevice::PAGESIZE_BIG1:
        case GpuDevice::PAGESIZE_BIG2:
            attr |= DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _BIG);
            mapAttr |= DRF_DEF(OS46, _FLAGS, _PAGE_SIZE, _BIG);
            break;

        case GpuDevice::PAGESIZE_HUGE:
            attr |= DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _HUGE);
            attr2 |= DRF_DEF(OS32, _ATTR2, _PAGE_SIZE_HUGE, _2MB);
            mapAttr |= DRF_DEF(OS46, _FLAGS, _PAGE_SIZE, _HUGE);
            break;

        case GpuDevice::PAGESIZE_512MB:
            // mapAttr is deprecated for 512 MB PageSize
            attr |= DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _HUGE);
            attr2 |= DRF_DEF(OS32, _ATTR2, _PAGE_SIZE_HUGE, _512MB);
            break;

        default:
            MASSERT(!"Unsupported page size!");
            return RC::SOFTWARE_ERROR;
    }

    // Allocate physical memory
    CHECK_RC(pLwRm->VidHeapAllocSizeEx(
                    type,
                    flags,
                    &attr,
                    &attr2,
                    pChunk->size,
                    alignment,                  // Alignment
                    (INT32*)&(pChunk->pteKind),
                    nullptr,                    // pCoverage
                    nullptr,                    // pPartitionStride
                    &(pChunk->hMem),
                    &pChunk->fbOffset,
                    &fbLimit,
                    nullptr,                    // pRtnFree
                    nullptr,                    // pRtnTotal
                    0,                          // width
                    0,                          // height
                    pGpuDev));
    Utility::CleanupOnReturnArg<LwRm, void, LwRm::Handle>
        freeMemHandleOnError(pLwRm.Get(), &LwRm::Free, pChunk->hMem);

    if (!contiguous)
    {
        pChunk->fbOffset = UNDEFINED_OFFSET;
    }
    else if (isSoc)
    {
        // Get physical address of the first page, because the address returned
        // from VidHeapAllocSizeEx is sometimes 0 (e.g. on SOC).
        // Note: Apparently this is not available in kernel mode RM.
        CHECK_RC(GetPhysAddress(*pChunk, 0, &pChunk->fbOffset, nullptr));
    }

    if (hChannel)
    {
        // Disable breakpoints since it is OK for RM to fail
        // this map - see printf below.
        Platform::DisableBreakPoints nobp;

        mapAttr |= DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE);

        if (isSoc && pSubDev->HasFeature(Device::GPUSUB_HAS_COHERENT_SYSMEM))
        {
            mapAttr |= DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _ENABLE);
        }
        else
        {
            mapAttr |= DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _DISABLE);
        }

        pChunk->hCtxDma = pGpuDev->GetMemSpaceCtxDma(Memory::Fb, false);

        rc = pLwRm->MapMemoryDma(
                pChunk->hCtxDma,
                pChunk->hMem,
                0,
                pChunk->size,
                mapAttr,
                &(pChunk->ctxDmaOffsetGpu),
                pGpuDev);

        if (OK != rc)
        {
            Printf(Tee::PriDebug,
                   "Detected a case where memory could be allocated but a "
                   "corresponding ContextDMA\n"
                   "could not (RC=%d).  This is not an error.\n",
                   rc.Get());

            // It is important that we force this specific RC so that
            // the loop in AllocateEntireFramebuffer knows how to retry.
            return RC::LWRM_INSUFFICIENT_RESOURCES;
        }
    }

    freeMemHandleOnError.Cancel();
    return rc;
}

namespace
{
    UINT64 GetUsableMemorySize(GpuSubdevice* pGpuSubdev)
    {
        UINT64 ramAmount = 0;

#ifdef TEGRA_MODS
        do
        {
            string meminfo;

            const RC rc = Xp::InteractiveFileRead("/proc/meminfo", &meminfo);
            if (rc != RC::OK)
            {
                Printf(Tee::PriWarn, "Failed to read /proc/meminfo\n");
                break;
            }

            vector<string> lines;
            if (Utility::Tokenizer(meminfo, "\n", &lines) != RC::OK)
            {
                Printf(Tee::PriWarn, "Failed to parse /proc/meminfo\n");
                break;
            }

            std::regex  reAvail("^MemAvailable:[ \\t]*(\\d+) kB$");
            std::smatch match;

            for (const string& line : lines)
            {
                if (std::regex_search(line, match, reAvail))
                {
                    ramAmount = Utility::Strtoull(match[1].str().c_str(), nullptr, 10);
                    if (ramAmount == ~0ULL)
                    {
                        ramAmount = 0;
                    }
                    else
                    {
                        ramAmount *= 1_KB;
                    }
                    Printf(Tee::PriLow, "MemAvailable: %llu MB\n", ramAmount / 1_MB);
                    break;
                }
            }

            if (!ramAmount)
            {
                Printf(Tee::PriWarn, "Failed to find MemAvailable in /proc/meminfo\n");
            }
        } while (false);

        if (!ramAmount)
#endif
        {
            ramAmount = pGpuSubdev->GetFB()->GetGraphicsRamAmount();
        }

        return ramAmount;
    }
}

RC GpuUtility::AllocateFramebufferChunk
(
    MemoryChunkDesc * pChunk,
    UINT64            alignment,
    bool              blockLinear,
    UINT64            pageSize,
    LwRm::Handle      hChannel,
    GpuDevice       * pGpuDev,
    bool              contiguous
)
{
    RC rc = TryAllocateFramebufferChunk(
            pChunk,
            alignment,
            blockLinear,
            pageSize,
            LWOS32_TYPE_PRIMARY,
            hChannel,
            pGpuDev,
            contiguous);
    if (RC::LWRM_INSUFFICIENT_RESOURCES == rc)
    {
        rc.Clear();
        rc = TryAllocateFramebufferChunk(
                pChunk,
                alignment,
                blockLinear,
                pageSize,
                LWOS32_TYPE_IMAGE,
                hChannel,
                pGpuDev,
                contiguous);
    }
    return rc;
}
//------------------------------------------------------------------------------
//! \brief Allocate as much physical FB memory as possible, for FB memory tests.
//!
//! May get memory in multiple chunks, if the rm's FB heap is fragmented.
//!
//! Note: there are many different ways to measure FB size:
//! - LwRm::VidHeapInfo() sets its 2nd argument to the LWOS32_PARAMETERS.total
//!   field filled by LWOS32_FUNCTION_INFO.
//! - FrameBuffer::GetGraphicsRamAmount() uses platform-specific register reads
//!   and works for non-contiguous FBs. This is the "Memory Size" that shows up
//!   in the standard spew.
//! - FrameBuffer::GetFbRanges() uses LW2080_CTRL_CMD_FB_GET_FB_REGION_INFO to
//!   supply the actual physical address ranges, which can be measured and
//!   summed to get FB size. This figure should be the same as that returned by
//!   FrameBuffer::GetGraphicsRamAmount().
//! - GpuSubdevice::FbHeapSize() is 0 if LW2080_CTRL_FB_INFO_INDEX_FB_IS_BROKEN
//!   returns true. Otherwise, it's the value returned by
//!   LW2080_CTRL_FB_INFO_INDEX_HEAP_SIZE, purportedly the amount of unreserved
//!   FB, though it returns much smaller values than the other methods.
//!
//! On CheetAh, when we are running CheetAh MODS, we are allocating from system
//! memory, so there is no way to allocate all of memory.  In this case, the
//! amount of memory to allocate must always be passed to this function,
//! otherwise the function will allocate max 1GB.
//!
//! Moreover, memory allocated on CheetAh is always fragmented, because system
//! memory consists of non-contiguous page-size chunks.  We are still trying
//! to allocate 256KB contiguous memory chunks, which is required for some
//! tests, like FastMats(19).
//!
//! This behavior does not apply to dGPU on CheetAh.  We are using RM for dGPU,
//! so the regular memory allocation rules apply.
//------------------------------------------------------------------------------
RC GpuUtility::AllocateEntireFramebuffer
(
    UINT64          minChunkSize,  //!< Smallest chunk to alloc.
    UINT64          maxChunkSize,  //!< Largest chunk to alloc (0 for all)
    UINT64          maxSize,       //!< Max total size to use (0 for all)
    MemoryChunks *  pChunks,       //!< returned data
    bool            blockLinear,   //!< If true, use BlockLinear,
                                   //!< pitch otherwise
    UINT64          minPageSize,   //!< Minimum page size
                                   //!< For SOC this can be nonContigSysmemPages
                                   //!< to indicate that the test requests and
                                   //!< supports non-contiguous memory.
    UINT64          maxPageSize,   //!< Maximum page size
    LwRm::Handle    hChannel,      //!< For MapMemoryDma, if 0 don't alloc or
                                   //!<  use any ctxdma for gpu (mats test does
                                   //!<  this and uses only CPU read/writes).
    GpuDevice *     pGpuDev,       //!< Which gpu device.
    UINT32          minPercentReqd, //!< Minimum fraction of the total size
    bool            contiguous     //!< Require contiguous chunks
)
{
    Utility::StopWatch timeAFB("Memory allocation took", Tee::PriLow);

    // For now, noncontiguous allocs are only supported on SOC
    const bool isSoc = pGpuDev->GetSubdevice(0)->IsSOC();
    MASSERT(contiguous || isSoc);

    if (maxChunkSize > 0 && maxChunkSize < minChunkSize)
    {
        MASSERT(!"AllocateEntireFramebuffer Error: MaxChunkSize cannot be smaller than MinChunkSize");
        return RC::SOFTWARE_ERROR;
    }

    UINT64 pageSize = pGpuDev->GetMaxPageSize(minPageSize, maxPageSize);
    UINT64 nextPageSize = pGpuDev->GetMaxPageSize(minPageSize, pageSize - 1);
    if (pageSize == 0)
    {
        MASSERT(!"AllocateEntireFramebuffer Error: No supported page sizes between minPageSize and maxPageSize");
        return RC::SOFTWARE_ERROR;
    }

    GpuDevice::LockAllocFb lock(pGpuDev);

    MASSERT(pChunks);
    MASSERT(0 == (minChunkSize & (minChunkSize-1)));  // must be power of 2
    FreeEntireFramebuffer(pChunks, pGpuDev);

    pGpuDev->FreeNonEssentialAllocations();

    // Fix args for the (strange) case where MaxSize is quite small.
    // Remember that MaxSize of 0 means "all of FB".
    if (maxSize && (maxSize < minChunkSize))
        maxSize = minChunkSize;

    RC rc;

    LwRmPtr pLwRm;

    // How much FB is there?
    UINT64 largestFreeChunk;
    UINT64 totalHeap;
    UINT64 totalFree;

    const bool requestedContiguous = contiguous;

    if (isSoc)
    {
        if (maxSize == 0)
        {
            const UINT64 ramAmount = GetUsableMemorySize(pGpuDev->GetSubdevice(0));
            // Don't try to allocate all available RAM, leave some RAM for the kernel,
            // MODS and other processes, otherwise the kernel may kill MODS if it runs
            // out of free pages.  The amount to leave out has been determined
            // empirically.
            maxSize = ramAmount - 1_GB;
            Printf(Tee::PriLow, "MaxFbMb test argument not specified, "
                   "defaulting to %llu MB.\n", maxSize / 1_MB);
        }

        // If arbitrarily "large" allocation was requested, allocate memory as
        // non-contiguous on CheetAh.  Normally we can't get contiguous chunks
        // larger than 2_MB from the kernel, so we have to allocate lots of
        // small chunks.  However, that takes a lot of time and we exceed the
        // handle capactiy in LwRm.  So instead, force non-contiguous allocation,
        // then we coalesce any contiguous regions within the allocated area
        // into separate contiguous chunks.
        if (maxSize > 512_MB)
        {
            contiguous = false;
        }
        largestFreeChunk = maxSize;
        totalHeap = maxSize;
        totalFree = maxSize;

        // Show how much memory we're going to allocate and how.
        // This is crucial for debugging OOM issues.
        const UINT64 dispMaxChunkSize = (maxChunkSize > 0) ? maxChunkSize : maxSize;
        Printf(Tee::PriNormal, "Allocating %f MB %scontiguous, max chunk size %f MB\n",
               static_cast<double>(maxSize) / static_cast<double>(1_MB),
               contiguous ? "" : "non-",
               static_cast<double>(dispMaxChunkSize) / static_cast<double>(1_MB));

        // Flush the previous print to the log, because if OOM killer strikes
        // we will not have any chances to write out any queued prints
        Tee::FlushToDisk();
    }
    else
    {
        CHECK_RC(pLwRm->VidHeapInfo(&totalFree, &totalHeap, 0,
                                    &largestFreeChunk, pGpuDev));

        if (0 == totalHeap)
        {
            Printf(Tee::PriError, "Total heap size 0.\n");
            return RC::LWRM_INSUFFICIENT_RESOURCES;
        }

        // Do not allocate any memory in 0FB case
        if (pGpuDev->GetSubdevice(0)->FbHeapSize() == 0)
        {
            return RC::LWRM_INSUFFICIENT_RESOURCES;
        }
    }

    // Conservatively assume that we need 60MB of page tables per 16GB of framebuffer.
    //
    // The actual value is closer to 33MB for GP10x/GV100, but until we can get an RM
    // control call to callwlate the required size for us it's better to be safe.
    //
    // Don't do this on iGPU, since we already put aside a significant amount of sysmem there.
    const UINT64 mapping16GB = 60_MB;
    const UINT64 reserved    = (mapping16GB * totalHeap) / (1ULL << 34);
    const UINT64 targetSize  = (maxSize > 0) && ((maxSize < totalFree) || isSoc) ?
                               maxSize : totalFree - reserved;

    // Is enough FB free to meet the request?
    const UINT32 freePct = static_cast<UINT32>((100 * totalFree) / targetSize);
    if (freePct < minPercentReqd)
    {
        Printf(Tee::PriError,
               "Only %d%% FB memory free, atleast %d%% requested.\n",
               freePct, minPercentReqd);
        return RC::LWRM_INSUFFICIENT_RESOURCES;
    }

    // The allocation may be legthy, esp. on CheetAh with memory allocated in lots
    // of small chunks -- allow other tests to make progress.
    Tasker::DetachThread detach;

    // Allocate chunks until we get as close as we can to the target size.
    set<UINT64> fbOffsets;
    UINT64 allocatedSoFar = 0;
    while (allocatedSoFar + minChunkSize <= targetSize)
    {
        // Callwlate size of next allocation request.
        UINT64 nextChunkRequest = largestFreeChunk;
        if (nextChunkRequest > targetSize - allocatedSoFar)
        {
            nextChunkRequest = targetSize - allocatedSoFar;
        }
        nextChunkRequest &= ~(minChunkSize - 1);

        // Exit when we can't allocate any closer
        if (nextChunkRequest < minChunkSize)
        {
            // The largest free block is < the minimum size of a chunk.
            // We're as close as we're going to get, stop allocating.
            break;
        }

        // On CheetAh, allocate small contiguous chunks.  The probability of obtaining
        // larger contiguous chunks from the kernel is relatively low.  At least
        // 256_KB chunks are needed by NewWfMats.
        if (isSoc && contiguous)
        {
            nextChunkRequest = 256_KB;
        }

        // Ensure chunk isn't larger than the maximum chunk size
        if (maxChunkSize > 0 && nextChunkRequest > maxChunkSize)
        {
            nextChunkRequest = maxChunkSize & ~(minChunkSize - 1);
        }

        // If the next chunk is smaller than a page, reduce the page size
        while (nextChunkRequest < pageSize && nextPageSize != 0)
        {
            pageSize = nextPageSize;
            nextPageSize = pGpuDev->GetMaxPageSize(minPageSize, pageSize - 1);
        }

        // Preferably allocate on page boundaries
        if (nextPageSize != 0)
        {
            nextChunkRequest = ALIGN_DOWN(nextChunkRequest, pageSize);
        }

        MemoryChunkDesc desc = {};
        desc.size = nextChunkRequest;

        const UINT64 alignment = isSoc ? Xp::GetPageSize() : pageSize;
        rc = AllocateFramebufferChunk(
                &desc,
                alignment,
                blockLinear,
                pageSize,
                hChannel,
                pGpuDev,
                contiguous);

        if (rc == OK)
        {
            Printf(Tee::PriLow,
                   "Allocated chunk: phys=0x%09llx, size=0x%09llx, pte=0x%02x, %s\n",
                   desc.fbOffset, desc.size, desc.pteKind,
                   desc.isDisplayable ? "displayable": "non-displayable");
            if (contiguous && fbOffsets.count(desc.fbOffset) > 0)
            {
                MASSERT(!"Two contiguous chunks can't be at the same offset");
                return RC::SOFTWARE_ERROR;
            }
            fbOffsets.insert(desc.fbOffset);

            if (isSoc && requestedContiguous && ! contiguous)
            {
                // Opportunistically assume that there are many contiguous areas
                // within the allocation
                UINT64 usedSize = 0;
                CHECK_RC(SplitContiguousChunks(pChunks, desc, &usedSize));
                Printf(Tee::PriLow, "Using 0x%09llx out of 0x%09llx allocated bytes\n",
                       usedSize, desc.size);

                // Remember this chunk to free the allocated memory, but don't
                // use it during the test, set dummy PA and size
                allocatedSoFar += desc.size;
                desc.size = 1; // make it too small to be used
            }

            // Keep queued prints flushed in case OOM killer strikes
            if (isSoc)
            {
                Tee::FlushToDisk();
            }

            pChunks->push_back(desc);
            allocatedSoFar += desc.size;
        }
        else if (rc == RC::LWRM_INSUFFICIENT_RESOURCES
                 // VidHeapInfo is not supported on CheetAh
                 && ! isSoc)
        {
            // Update "largest free heap chunk" data from RM.
            UINT64 newLargest;
            rc.Clear();
            CHECK_RC(pLwRm->VidHeapInfo(&totalFree, 0, 0,
                                        &newLargest, pGpuDev));

            if (newLargest >= largestFreeChunk)
            {
                if (newLargest > largestFreeChunk)
                {
                    Printf(Tee::PriLow,
                        "Warning: Largest available chunk has increased from "
                        "0x%09llx to 0x%09llx\n",
                        largestFreeChunk, newLargest);
                }

                // What the heck?  Alloc failed even though the request is
                // smaller than totalFree and largestFreeChunk!
                // Just reduce the size of our "largestFreeChunk" by some
                // arbitrary amount before retrying.
                // When largestFreeChunk is < MinChunkSize, we'll exit from
                // the loop near the top.
                newLargest = static_cast<UINT64>(largestFreeChunk * 0.95);

                Printf(Tee::PriLow,
                       "AllocateEntireFramebuffer was unable to allocate chunk of size 0x%llx\n"
                       "even though last time RM reported this as the largest available chunk.\n"
                       "In this situation we will try to allocate a chunk of size 0x%llx.\n",
                       largestFreeChunk, newLargest);
            }
            largestFreeChunk = newLargest;
        }
        else
        {
            // RM alloc failure other than "insufficient resources".
            return rc;
        }
    }

    if (pChunks->size() > 0)
    {
        Printf(Tee::PriLow,
               "AllocateEntireFramebuffer allocated %u chunk(s), "
               "free memory left = 0x%llx\n",
               static_cast<UINT32>(pChunks->size()), totalFree);

        MemoryChunks::iterator pChunk = pChunks->begin();
        Printf(Tee::PriLow, "  pteKind 0x%x\n", pChunk->pteKind);
    }
    else
    {
        Printf(Tee::PriNormal,
               "AllocateEntireFramebuffer didn't allocate any chunks\n");
    }

    // Did we succeed in allocating enough FB?

    const UINT32 allocPct = static_cast<UINT32>(
                                (allocatedSoFar * 100) / targetSize);

    if (allocPct < minPercentReqd)
    {
        Printf(Tee::PriError,
            "Only %d%% FB memory could be allocated, atleast %d%% requested.\n",
            allocPct, minPercentReqd);
        FreeEntireFramebuffer(pChunks, pGpuDev);
        return RC::LWRM_INSUFFICIENT_RESOURCES;
    }

    return rc;
}

//------------------------------------------------------------------------------
// Free the entire framebuffer
//------------------------------------------------------------------------------
RC GpuUtility::FreeEntireFramebuffer
(
    MemoryChunks *pChunks,
    GpuDevice    *pGpuDev
)
{
    MASSERT(pChunks);

    Tasker::DetachThread detach;

    LwRmPtr pLwRm;
    StickyRC firstRc;

    for (MemoryChunks::iterator chunkIter = pChunks->begin();
         chunkIter != pChunks->end();
         chunkIter++)
    {
        if (!chunkIter->needsFree)
        {
            continue;
        }

        if (chunkIter->hCtxDma)
        {
            pLwRm->UnmapMemoryDma(
                    chunkIter->hCtxDma,
                    chunkIter->hMem,
                    0,
                    chunkIter->ctxDmaOffsetGpu,
                    pGpuDev);
        }

        DisplayMgr::TestContext *pTestContext = static_cast<DisplayMgr::TestContext*>
            (chunkIter->pDisplayMgrTestContext);
        if (pTestContext)
        {
            Tasker::AttachThread attach;
            firstRc = pTestContext->DisplayImage((Surface2D *)NULL,
                DisplayMgr::WAIT_FOR_UPDATE);
        }

        if (chunkIter->hCtxDmaIso)
        {
            pLwRm->Free(chunkIter->hCtxDmaIso);
            chunkIter->hCtxDmaIso = 0;
        }

        pLwRm->Free(chunkIter->hMem);
    }

    pChunks->clear();

    return firstRc;
}

//------------------------------------------------------------------------------
//! \brief Find an area of fb memory big enough to hold a displayable image
//!
//! Given chunks allocated by AllocateEntireFramebuffer(), and the
//! dimensions of an image to display on the screen, try to find a
//! region big enough to hold the image.  Preferably between
//! StartLocation and EndLocation (in MB).
//!
//! This routine uses pteKind to determine whether it will be
//! displayed in pitch or block-linear mode, and rounds the image up
//! to the nearest gob if it's block-linear.
//!
//! On failure, return FAILED_TO_GET_IMAGE_OFFSET and set *pFbOffset
//! to the offset of the first chunk.
//!
//! \param pChunks The chunks allocated by AllocateEntireFramebuffer.
//! \param ppDisplayChunk[in,out] On exit, *ppDisplayChunk points to a
//!     memory chunk to set the image, or nullptr if none are
//!     eligible.  If *ppDisplayChunk is non-null on entry, then that
//!     chunk will be given preference.
//! \param pDisplayOffset[in,out] On exit, *pDisplayOffset hold the
//!     offset within *ppDisplayChunk to set the image.  If
//!     *ppDisplayChunk is non-null on entry, the the offset in
//!     *pDisplayOffset will be given preference.
//! \param StartLocation Try to find an offset >= StartLocation (in MB)
//! \param EndLocation Try to find an offset+size <= EndLocation (in MB)
//! \param Height Height of the image
//! \param Pitch Pitch of the image.  If the memory is blocklinear,
//!     this is the image width in bytes.
//! \return OK on success, FAILED_TO_GET_IMAGE_OFFSET on failure.
//------------------------------------------------------------------------------
RC GpuUtility::FindDisplayableChunk
(
    MemoryChunks     *pChunks,
    MemoryChunkDesc **ppDisplayChunk,
    UINT64           *pDisplayOffset,
    UINT64            startLocation,
    UINT64            endLocation,
    UINT32            height,
    UINT32            pitch,
    GpuDevice        *pGpuDev
)
{
    MASSERT(ppDisplayChunk);
    MASSERT(pDisplayOffset);
    const MemoryChunkDesc *pPreferredChunk = *ppDisplayChunk;
    const UINT64 preferredOffset = *pDisplayOffset;
    const UINT32 heightInGobs =
        (height + pGpuDev->GobHeight() - 1) / pGpuDev->GobHeight();
    const UINT32 widthInGobs =
        (pitch + pGpuDev->GobWidth() - 1) / pGpuDev->GobWidth();
    const UINT32 blockLinearSize =
        pGpuDev->GobSize() * heightInGobs * widthInGobs;
    const UINT32 pitchLinearSize = height * pitch;
    RC rc;

    // Clear the return values
    //
    *ppDisplayChunk = nullptr;
    *pDisplayOffset = UNDEFINED_OFFSET;

    // Loop through the chunks to find a region that's big enough,
    // with physical address between StartLocation and EndLocation.
    //
    if (startLocation != 0 || endLocation != 0)
    {
        for (GpuUtility::MemoryChunks::iterator pChunk = pChunks->begin();
             pChunk != pChunks->end(); ++pChunk)
        {
            const UINT32 requiredSize = (pChunk->pteKind == 0 ?
                                         pitchLinearSize : blockLinearSize);
            const UINT64 chunkStart   = max(pChunk->fbOffset,
                                            startLocation * 1_MB);
            const UINT64 chunkEnd     = min(pChunk->fbOffset + pChunk->size,
                                            endLocation * 1_MB);
            if (pChunk->isDisplayable &&
                pChunk->contiguous &&
                chunkStart <= chunkEnd &&
                requiredSize <= chunkEnd - chunkStart &&
                (*ppDisplayChunk == nullptr || &(*pChunk) == pPreferredChunk))
            {
                *ppDisplayChunk = &(*pChunk);
                *pDisplayOffset = chunkStart - pChunk->fbOffset;
            }
        }
    }

    // Failing that, loop through the chunks to find a region that's
    // big enough, ignoring StartLocation and EndLocation.
    //
    if (*ppDisplayChunk == nullptr)
    {
        for (GpuUtility::MemoryChunks::iterator pChunk = pChunks->begin();
             pChunk != pChunks->end(); ++pChunk)
        {
            const UINT32 requiredSize = (pChunk->pteKind == 0 ?
                                         pitchLinearSize : blockLinearSize);
            if (pChunk->isDisplayable &&
                requiredSize <= pChunk->size &&
                (*ppDisplayChunk == nullptr || &(*pChunk) == pPreferredChunk))
            {
                *ppDisplayChunk = &(*pChunk);
                *pDisplayOffset = 0;
            }
        }
    }

    // If we're using the preferred chunk, try to use the preferred offset
    //
    if (*ppDisplayChunk == pPreferredChunk && *ppDisplayChunk != nullptr)
    {
        const UINT32 requiredSize = (pPreferredChunk->pteKind == 0 ?
                                     pitchLinearSize : blockLinearSize);
        if (preferredOffset <= pPreferredChunk->size - requiredSize)
        {
            *pDisplayOffset = preferredOffset;
        }
    }

    if (*ppDisplayChunk == nullptr)
    {
        return RC::FAILED_TO_GET_IMAGE_OFFSET;
    }
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Colwerts a virtual address into physical address.
//!
//! The function returns physical address for chunks in any aperture,
//! including vidmem and sysmem and regardless of physical contiguity.
//!
//! The input offset is compatible with the ctxdma mapping performed by
//! AllocateEntireFramebuffer(). The function finds the chunk to which that
//! offset belongs and then translates the offset into physical address.
//!
//! \param chunk Memory chunks allocated by AllocateEntireFramebuffer().
//! \param ctxDmaOffset Context dma offset for mapping performed by
//!                     AllocateEntireFramebuffer().
//! \param pPhysAddr Returned physical address.
//! \param pContigSize How many contiguous bytes follow the offset.
//!
RC GpuUtility::GetPhysAddress
(
    const MemoryChunks& chunks,
    UINT64 ctxDmaOffset,
    PHYSADDR* pPhysAddr,
    UINT64* pContigSize
)
{
    MemoryChunks::const_iterator chunkIt = chunks.begin();
    for (; chunkIt != chunks.end(); ++chunkIt)
    {
        const UINT64 chunkOffs = chunkIt->ctxDmaOffsetGpu;
        if ((ctxDmaOffset >= chunkOffs) &&
            (ctxDmaOffset < chunkOffs + chunkIt->size))
        {
            return GetPhysAddress(*chunkIt,
                                  ctxDmaOffset - chunkOffs,
                                  pPhysAddr,
                                  pContigSize);
        }
    }
    return RC::SOFTWARE_ERROR;
}

//--------------------------------------------------------------
//! \brief Get PTE kind of a given VA address. if not found, set to -1(invalid PTE kind);
//! \param gpuAddr   a VA address to be queried
//! \param hVASpace  the VA space of the addrees
//! \param pGpuDev   pointer to GPU Device
//! \param pteKind   returned PTE kind.
//!
RC GpuUtility::GetPteKind
(
    LwU64 gpuAddr,
    LwRm::Handle hVASpace,
    GpuDevice* pGpuDev,
    INT32* pteKind,
    LwRm* pLwRm
)
{
    RC rc;
    *pteKind = -1;

    if (Platform::GetSimulationMode() != Platform::Amodel &&
        !Platform::UsesLwgpuDriver())
    {
        LW0080_CTRL_DMA_GET_PTE_INFO_PARAMS getParams = { };
        getParams.gpuAddr = gpuAddr;
        getParams.hVASpace = hVASpace;

        CHECK_RC(pLwRm->ControlByDevice(
                    pGpuDev,
                    LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                    &getParams, sizeof(getParams)));

        for (UINT32 i = 0; i < LW0080_CTRL_DMA_GET_PTE_INFO_PTE_BLOCKS; ++i)
        {
            if (FLD_TEST_DRF(0080_CTRL_DMA_PTE_INFO_PARAMS, _FLAGS, _VALID,
                _TRUE, getParams.pteBlocks[i].pteFlags))
            {
                *pteKind = getParams.pteBlocks[i].kind;
                break;
            }
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Return physical address of a byte at the given chunk offset.
//!
//! The function returns physical address for chunks in any aperture,
//! including vidmem and sysmem and regardless of physical contiguity.
//!
//! \param chunk Memory chunk allocated by AllocateEntireFramebuffer().
//! \param chunkOffs Offset within the chunk.
//! \param pPhysAddr Returned physical address.
//! \param pContigSize How many contiguous bytes follow the offset.
//!
RC GpuUtility::GetPhysAddress
(
    const MemoryChunkDesc& chunk,
    UINT64 chunkOffset,
    PHYSADDR* pPhysAddr,
    UINT64* pContigSize
)
{
    LW0041_CTRL_GET_SURFACE_PHYS_ATTR_PARAMS params = {0};
    params.memOffset  = chunkOffset;
    params.mmuContext = TEGRA_VASPACE_A;  // CPU view of memory
    RC rc;
    CHECK_RC(LwRmPtr()->Control(chunk.hMem,
                                LW0041_CTRL_CMD_GET_SURFACE_PHYS_ATTR,
                                &params, sizeof(params)));

    *pPhysAddr = params.memOffset;
    if (pContigSize)
    {
        *pContigSize = params.contigSegmentSize;
    }
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Fill buffer with Random data using Lwca kernel
//!
RC GpuUtility::FillRandomOnGpu(GpuDevice *pGpuDev,
    LwRm::Handle handle,
    UINT64 size,
    UINT32 seed)
{
    LwSurfRoutines lwSurf;
    if (!lwSurf.IsSupported())
        return RC::UNSUPPORTED_FUNCTION;
    return lwSurf.FillRandom<UINT64>(pGpuDev, handle, size, seed);
}

//------------------------------------------------------------------------------
//! \brief Fill buffer with constant data using Lwca kernel
//!
RC GpuUtility::FillConstantOnGpu(GpuDevice *pGpuDev,
    LwRm::Handle handle,
    UINT64 size,
    UINT32 constant)
{
    LwSurfRoutines lwSurf;
    if (!lwSurf.IsSupported())
        return RC::UNSUPPORTED_FUNCTION;
    const UINT64 constantData = static_cast<UINT64>(constant) << 32 | constant;
    return lwSurf.FillConstant<UINT64>(pGpuDev, handle, size, constantData);
}

//------------------------------------------------------------------------------
//! \brief Get CRC using Lwca
//! Each thread returns CRC of a chunk of buffer
//!
RC GpuUtility::GetBlockedCRCOnGpu(GpuDevice *pGpuDev,
    LwRm::Handle handle,
    UINT64 offset,
    UINT64 size,
    Memory::Location loc,
    UINT32 *pCrc)
{
    if (loc != Memory::Fb)
        return RC::UNSUPPORTED_FUNCTION;

    LwSurfRoutines lwSurf;
    if (!lwSurf.IsSupported())
        return RC::UNSUPPORTED_FUNCTION;

    RC rc;
    vector<UINT32> crc;
    CHECK_RC(lwSurf.GetCRC(pGpuDev, handle, offset, size, loc, false, &crc));
    MASSERT(pCrc);
    *pCrc = crc[0];
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Get CRC using Lwca
//! Each thread returns CRC of a chunk of buffer
//!
RC GpuUtility::GetBlockedCRCOnGpuPerComponent(GpuDevice *pGpuDev,
    LwRm::Handle handle,
    UINT64 offset,
    UINT64 size,
    Memory::Location loc,
    vector<UINT32> *pCrc)
{
    if (loc != Memory::Fb)
        return RC::UNSUPPORTED_FUNCTION;

    LwSurfRoutines lwSurf;
    if (!lwSurf.IsSupported())
        return RC::UNSUPPORTED_FUNCTION;

    return lwSurf.GetCRC(pGpuDev, handle, offset, size, loc, true, pCrc);
}

//------------------------------------------------------------------------------
//! \brief Compare 2 GPU buffers
//!
RC GpuUtility::CompareBuffers(GpuDevice *pGpuDev,
    LwRm::Handle handle1,
    UINT64 size1,
    Memory::Location loc1,
    LwRm::Handle handle2,
    UINT64 size2,
    Memory::Location loc2,
    bool *pResult)
{
    if (loc1 != Memory::Fb && loc2 != Memory::Fb)
        return RC::UNSUPPORTED_FUNCTION;

    LwSurfRoutines lwSurf;
    if (!lwSurf.IsSupported())
        return RC::UNSUPPORTED_FUNCTION;

    return lwSurf.CompareBuffers(pGpuDev, handle1, size1, loc1,
                                 handle2, size2, loc2,
                                 0, /* No mask */
                                 pResult);
}

//------------------------------------------------------------------------------
//! \brief Compare 2 GPU buffers component wise
//! Compares the 4 components in R8G8B8A8 format
RC GpuUtility::CompareBuffersComponents(GpuDevice *pGpuDev,
    LwRm::Handle handle1,
    UINT64 size1,
    Memory::Location loc1,
    LwRm::Handle handle2,
    UINT64 size2,
    Memory::Location loc2,
    UINT32 mask,
    bool *pResult)
{
    if (loc1 != Memory::Fb && loc2 != Memory::Fb)
        return RC::UNSUPPORTED_FUNCTION;

    LwSurfRoutines lwSurf;
    if (!lwSurf.IsSupported())
        return RC::UNSUPPORTED_FUNCTION;
    return lwSurf.CompareBuffers(pGpuDev, handle1, size1, loc1,
                                 handle2, size2, loc2,
                                 mask,   //Mask of components to check
                                 pResult);
}

//------------------------------------------------------------------------------
//! \brief Call Display::SetImage() on a framebuffer offset
//!
//! Given chunks allocated by AllocateEntireFramebuffer(), an offset
//! into the framebuffer, and some minimal modeset info, call
//! Display::SetImage() to display the offset on the selected display.
//!
//! This routine isn't intended to be a full-featured interface; it's
//! a quick-n-dirty way to display something reasonable.  Use
//! Display::SetImage() and/or Surface2D if you want more control.
//!
//! This routine creates a contextDmaIso in the chunk.  It uses
//! pteKind to determine whether to use pitch or block-linear mode.
//!
//! \param pDisplayMgrTestContext The DisplayMgr::TestContext pointer
//! \param pChunk The chunk to display
//! \param chunkOffset Offset within the chunk to display.
//!     See FindDisplayableChunk() for one way to find pChunk and chunkOffset.
//! \param Width Width of the display
//! \param Height Height of the display
//! \param Depth Depth of the display
//! \param Pitch Pitch of the display.  If the memory is blocklinear,
//!     this is the image width in bytes.
//------------------------------------------------------------------------------
RC GpuUtility::SetImageOnChunkLegacy
(
    Display         *pDisplay,
    MemoryChunkDesc *pChunk,
    UINT64           chunkOffset,
    UINT32           width,
    UINT32           height,
    UINT32           depth,
    UINT32           pitch,
    GpuDevice       *pGpuDev
)
{
    MASSERT(pChunk);
    LwRmPtr pLwRm;
    RC rc;

    // Make sure the chunk is displayable
    //
    MASSERT(pChunk->isDisplayable);
    if (!pChunk->isDisplayable)
    {
        return RC::BAD_PARAMETER;
    }

    // Allocate a new ctxDmaIso for the chunk, and bind it to the base
    // channel of all selected displays.  The display ctxdma makes
    // sense only on dGPU.
    //
    if (!pGpuDev->GetSubdevice(0)->IsSOC())
    {
        if (pChunk->hCtxDmaIso != 0)
        {
            pLwRm->Free(pChunk->hCtxDmaIso);
            pChunk->hCtxDmaIso = 0;
        }

        rc = pLwRm->AllocContextDma(
                 &pChunk->hCtxDmaIso,
                 DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_WRITE),
                 pChunk->hMem, 0, pChunk->size - 1);

        if (OK != rc)
        {
            // If we allocate the entire FB and then try to allocate
            // context DMA, we may find that it's not possible to
            // allocate it. The test should continue in this case.
            if (RC::LWRM_ERROR == rc)
            {
                return OK;
            }
            return rc;
        }

        UINT32 remainingDisplays = pDisplay->Selected();
        while (remainingDisplays != 0 && pChunk->hCtxDmaIso != 0)
        {
            UINT32 display = remainingDisplays & (~remainingDisplays + 1);
            LwRm::Handle hBaseCh;
            if (pDisplay->GetEvoDisplay()->GetBaseChannelHandle(display, &hBaseCh) == OK)
                CHECK_RC(pLwRm->BindContextDma(hBaseCh, pChunk->hCtxDmaIso));
            remainingDisplays &= ~display;
        }
    }

    // Make the SetImage call
    //
    DisplayImageDescription desc;
    desc.hMem         = pChunk->hMem;
    desc.CtxDMAHandle = pChunk->hCtxDmaIso;
    desc.Offset       = chunkOffset;
    desc.Height       = height;
    desc.Width        = width;
    desc.Pitch        = pitch;
    desc.AASamples    = 1;
    if (pChunk->pteKind == 0)
    {
        desc.Layout         = Surface2D::Pitch;
        desc.LogBlockHeight = 0;
        desc.NumBlocksWidth = 0;
    }
    else
    {
        desc.Layout         = Surface2D::BlockLinear;
        desc.LogBlockHeight = 0;
        desc.NumBlocksWidth =
            (pitch + pGpuDev->GobWidth() - 1) / pGpuDev->GobWidth();
    }
    desc.ColorFormat  = ColorUtils::ColorDepthToFormat(depth);

    CHECK_RC(pDisplay->SetImage(pDisplay->Selected(), &desc));

    return rc;
}

//------------------------------------------------------------------------------
// return the absolute time in Ms since the Lwpu timer was initialized
// be sure the SetTimerRatio() gets called before you use this function.
UINT32 GpuUtility::GetLwTimerMs(GpuSubdevice *pSubdev)
{
    MASSERT(pSubdev);

    UINT32 high    = pSubdev->RegRd32(LW_PTIMER_TIME_1);
    UINT32 low     = pSubdev->RegRd32(LW_PTIMER_TIME_0);
    UINT32 highChk = pSubdev->RegRd32(LW_PTIMER_TIME_1);

    // since the reads of the high and low parts are not atomic, it is possible
    // for the high-order counter to roll over while we are reading the low-
    // order counter.  Fortunately, this is easy to detect and correct for.
    if (high != highChk)
    {
        low   = pSubdev->RegRd32(LW_PTIMER_TIME_0);
        high  = highChk;
    }

    // Combine into one value and scrap the 20 LSB.
    UINT32 rough = ( ( low >> 20) | (( high & 0xFFFFF) << 12) );

    FLOAT64 adjustVal = 1000000.0 / 1048576.0; // 10^6 / 2^20
    FLOAT64 floatVal  = static_cast<FLOAT64>(rough) / adjustVal;

    return static_cast<UINT32>(floatVal);
}

//------------------------------------------------------------------------------
// Draw 'Scaled' 8x8 'Chars' starting at (X,Y) in graphics mode.
// Upper left corner is (0,0).
//
static const INT32  FONT_WIDTH =   8;
static const INT32  FONT_HEIGHT =  8;
static const INT32  NUM_FONTS  = 256;
static const UINT08 FontTable[NUM_FONTS][FONT_HEIGHT] =
{
    {0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000},
    {0x7e, 0x81, 0xa5, 0x81, 0xbd, 0x99, 0x81, 0x7e},
    {0x7e, 0xff, 0xdb, 0xff, 0xc3, 0xe7, 0xff, 0x7e},
    {0x6c, 0xfe, 0xfe, 0xfe, 0x7c, 0x38, 0x10, 0000},
    {0x10, 0x38, 0x7c, 0xfe, 0x7c, 0x38, 0x10, 0000},
    {0x38, 0x7c, 0x38, 0xfe, 0xfe, 0x7c, 0x38, 0x7c},
    {0x10, 0x10, 0x38, 0x7c, 0xfe, 0x7c, 0x38, 0x7c},
    {0000, 0000, 0x18, 0x3c, 0x3c, 0x18, 0000, 0000},
    {0xff, 0xff, 0xe7, 0xc3, 0xc3, 0xe7, 0xff, 0xff},
    {0000, 0x3c, 0x66, 0x42, 0x42, 0x66, 0x3c, 0000},
    {0xff, 0xc3, 0x99, 0xbd, 0xbd, 0x99, 0xc3, 0xff},
    {0x0f, 0x07, 0x0f, 0x7d, 0xcc, 0xcc, 0xcc, 0x78},
    {0x3c, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x7e, 0x18},
    {0x3f, 0x33, 0x3f, 0x30, 0x30, 0x70, 0xf0, 0xe0},
    {0x7f, 0x63, 0x7f, 0x63, 0x63, 0x67, 0xe6, 0xc0},
    {0x99, 0x5a, 0x3c, 0xe7, 0xe7, 0x3c, 0x5a, 0x99},
    {0x80, 0xe0, 0xf8, 0xfe, 0xf8, 0xe0, 0x80, 0000},
    {0x02, 0x0e, 0x3e, 0xfe, 0x3e, 0x0e, 0x02, 0000},
    {0x18, 0x3c, 0x7e, 0x18, 0x18, 0x7e, 0x3c, 0x18},
    {0x66, 0x66, 0x66, 0x66, 0x66, 0000, 0x66, 0000},
    {0x7f, 0xdb, 0xdb, 0x7b, 0x1b, 0x1b, 0x1b, 0000},
    {0x3e, 0x63, 0x38, 0x6c, 0x6c, 0x38, 0xcc, 0x78},
    {0000, 0000, 0000, 0000, 0x7e, 0x7e, 0x7e, 0000},
    {0x18, 0x3c, 0x7e, 0x18, 0x7e, 0x3c, 0x18, 0xff},
    {0x18, 0x3c, 0x7e, 0x18, 0x18, 0x18, 0x18, 0000},
    {0x18, 0x18, 0x18, 0x18, 0x7e, 0x3c, 0x18, 0000},
    {0000, 0x18, 0x0c, 0xfe, 0x0c, 0x18, 0000, 0000},
    {0000, 0x30, 0x60, 0xfe, 0x60, 0x30, 0000, 0000},
    {0000, 0000, 0xc0, 0xc0, 0xc0, 0xfe, 0000, 0000},
    {0000, 0x24, 0x66, 0xff, 0x66, 0x24, 0000, 0000},
    {0000, 0x18, 0x3c, 0x7e, 0xff, 0xff, 0000, 0000},
    {0000, 0xff, 0xff, 0x7e, 0x3c, 0x18, 0000, 0000},
    {0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000},
    {0x30, 0x78, 0x78, 0x30, 0x30, 0000, 0x30, 0000},
    {0x6c, 0x6c, 0x6c, 0000, 0000, 0000, 0000, 0000},
    {0x6c, 0x6c, 0xfe, 0x6c, 0xfe, 0x6c, 0x6c, 0000},
    {0x30, 0x7c, 0xc0, 0x78, 0x0c, 0xf8, 0x30, 0000},
    {0000, 0xc6, 0xcc, 0x18, 0x30, 0x66, 0xc6, 0000},
    {0x38, 0x6c, 0x38, 0x76, 0xdc, 0xcc, 0x76, 0000},
    {0x60, 0x60, 0xc0, 0000, 0000, 0000, 0000, 0000},
    {0x18, 0x30, 0x60, 0x60, 0x60, 0x30, 0x18, 0000},
    {0x60, 0x30, 0x18, 0x18, 0x18, 0x30, 0x60, 0000},
    {0000, 0x66, 0x3c, 0xff, 0x3c, 0x66, 0000, 0000},
    {0000, 0x30, 0x30, 0xfc, 0x30, 0x30, 0000, 0000},
    {0000, 0000, 0000, 0000, 0000, 0x30, 0x30, 0x60},
    {0000, 0000, 0000, 0xfc, 0000, 0000, 0000, 0000},
    {0000, 0000, 0000, 0000, 0000, 0x30, 0x30, 0000},
    {0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x80, 0000},
    {0x7c, 0xc6, 0xce, 0xde, 0xf6, 0xe6, 0x7c, 0000},
    {0x30, 0x70, 0x30, 0x30, 0x30, 0x30, 0xfc, 0000},
    {0x78, 0xcc, 0x0c, 0x38, 0x60, 0xcc, 0xfc, 0000},
    {0x78, 0xcc, 0x0c, 0x38, 0x0c, 0xcc, 0x78, 0000},
    {0x1c, 0x3c, 0x6c, 0xcc, 0xfe, 0x0c, 0x1e, 0000},
    {0xfc, 0xc0, 0xf8, 0x0c, 0x0c, 0xcc, 0x78, 0000},
    {0x38, 0x60, 0xc0, 0xf8, 0xcc, 0xcc, 0x78, 0000},
    {0xfc, 0xcc, 0x0c, 0x18, 0x30, 0x30, 0x30, 0000},
    {0x78, 0xcc, 0xcc, 0x78, 0xcc, 0xcc, 0x78, 0000},
    {0x78, 0xcc, 0xcc, 0x7c, 0x0c, 0x18, 0x70, 0000},
    {0000, 0x30, 0x30, 0000, 0000, 0x30, 0x30, 0000},
    {0000, 0x30, 0x30, 0000, 0000, 0x30, 0x30, 0x60},
    {0x18, 0x30, 0x60, 0xc0, 0x60, 0x30, 0x18, 0000},
    {0000, 0000, 0xfc, 0000, 0000, 0xfc, 0000, 0000},
    {0x60, 0x30, 0x18, 0x0c, 0x18, 0x30, 0x60, 0000},
    {0x78, 0xcc, 0x0c, 0x18, 0x30, 0000, 0x30, 0000},
    {0x7c, 0xc6, 0xde, 0xde, 0xde, 0xc0, 0x78, 0000},
    {0x30, 0x78, 0xcc, 0xcc, 0xfc, 0xcc, 0xcc, 0000},
    {0xfc, 0x66, 0x66, 0x7c, 0x66, 0x66, 0xfc, 0000},
    {0x3c, 0x66, 0xc0, 0xc0, 0xc0, 0x66, 0x3c, 0000},
    {0xf8, 0x6c, 0x66, 0x66, 0x66, 0x6c, 0xf8, 0000},
    {0xfe, 0x62, 0x68, 0x78, 0x68, 0x62, 0xfe, 0000},
    {0xfe, 0x62, 0x68, 0x78, 0x68, 0x60, 0xf0, 0000},
    {0x3c, 0x66, 0xc0, 0xc0, 0xce, 0x66, 0x3e, 0000},
    {0xcc, 0xcc, 0xcc, 0xfc, 0xcc, 0xcc, 0xcc, 0000},
    {0x78, 0x30, 0x30, 0x30, 0x30, 0x30, 0x78, 0000},
    {0x1e, 0x0c, 0x0c, 0x0c, 0xcc, 0xcc, 0x78, 0000},
    {0xe6, 0x66, 0x6c, 0x78, 0x6c, 0x66, 0xe6, 0000},
    {0xf0, 0x60, 0x60, 0x60, 0x62, 0x66, 0xfe, 0000},
    {0xc6, 0xee, 0xfe, 0xfe, 0xd6, 0xc6, 0xc6, 0000},
    {0xc6, 0xe6, 0xf6, 0xde, 0xce, 0xc6, 0xc6, 0000},
    {0x38, 0x6c, 0xc6, 0xc6, 0xc6, 0x6c, 0x38, 0000},
    {0xfc, 0x66, 0x66, 0x7c, 0x60, 0x60, 0xf0, 0000},
    {0x78, 0xcc, 0xcc, 0xcc, 0xdc, 0x78, 0x1c, 0000},
    {0xfc, 0x66, 0x66, 0x7c, 0x6c, 0x66, 0xe6, 0000},
    {0x78, 0xcc, 0xe0, 0x70, 0x1c, 0xcc, 0x78, 0000},
    {0xfc, 0xb4, 0x30, 0x30, 0x30, 0x30, 0x78, 0000},
    {0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xfc, 0000},
    {0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x78, 0x30, 0000},
    {0xc6, 0xc6, 0xc6, 0xd6, 0xfe, 0xee, 0xc6, 0000},
    {0xc6, 0xc6, 0x6c, 0x38, 0x38, 0x6c, 0xc6, 0000},
    {0xcc, 0xcc, 0xcc, 0x78, 0x30, 0x30, 0x78, 0000},
    {0xfe, 0xc6, 0x8c, 0x18, 0x32, 0x66, 0xfe, 0000},
    {0x78, 0x60, 0x60, 0x60, 0x60, 0x60, 0x78, 0000},
    {0xc0, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x02, 0000},
    {0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x78, 0000},
    {0x10, 0x38, 0x6c, 0xc6, 0000, 0000, 0000, 0000},
    {0000, 0000, 0000, 0000, 0000, 0000, 0000, 0xff},
    {0x30, 0x30, 0x18, 0000, 0000, 0000, 0000, 0000},
    {0000, 0000, 0x78, 0x0c, 0x7c, 0xcc, 0x76, 0000},
    {0xe0, 0x60, 0x60, 0x7c, 0x66, 0x66, 0xdc, 0000},
    {0000, 0000, 0x78, 0xcc, 0xc0, 0xcc, 0x78, 0000},
    {0x1c, 0x0c, 0x0c, 0x7c, 0xcc, 0xcc, 0x76, 0000},
    {0000, 0000, 0x78, 0xcc, 0xfc, 0xc0, 0x78, 0000},
    {0x38, 0x6c, 0x60, 0xf0, 0x60, 0x60, 0xf0, 0000},
    {0000, 0000, 0x76, 0xcc, 0xcc, 0x7c, 0x0c, 0xf8},
    {0xe0, 0x60, 0x6c, 0x76, 0x66, 0x66, 0xe6, 0000},
    {0x30, 0000, 0x70, 0x30, 0x30, 0x30, 0x78, 0000},
    {0x0c, 0000, 0x0c, 0x0c, 0x0c, 0xcc, 0xcc, 0x78},
    {0xe0, 0x60, 0x66, 0x6c, 0x78, 0x6c, 0xe6, 0000},
    {0x70, 0x30, 0x30, 0x30, 0x30, 0x30, 0x78, 0000},
    {0000, 0000, 0xcc, 0xfe, 0xfe, 0xd6, 0xc6, 0000},
    {0000, 0000, 0xf8, 0xcc, 0xcc, 0xcc, 0xcc, 0000},
    {0000, 0000, 0x78, 0xcc, 0xcc, 0xcc, 0x78, 0000},
    {0000, 0000, 0xdc, 0x66, 0x66, 0x7c, 0x60, 0xf0},
    {0000, 0000, 0x76, 0xcc, 0xcc, 0x7c, 0x0c, 0x1e},
    {0000, 0000, 0xdc, 0x76, 0x66, 0x60, 0xf0, 0000},
    {0000, 0000, 0x7c, 0xc0, 0x78, 0x0c, 0xf8, 0000},
    {0x10, 0x30, 0x7c, 0x30, 0x30, 0x34, 0x18, 0000},
    {0000, 0000, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0000},
    {0000, 0000, 0xcc, 0xcc, 0xcc, 0x78, 0x30, 0000},
    {0000, 0000, 0xc6, 0xd6, 0xfe, 0xfe, 0x6c, 0000},
    {0000, 0000, 0xc6, 0x6c, 0x38, 0x6c, 0xc6, 0000},
    {0000, 0000, 0xcc, 0xcc, 0xcc, 0x7c, 0x0c, 0xf8},
    {0000, 0000, 0xfc, 0x98, 0x30, 0x64, 0xfc, 0000},
    {0x1c, 0x30, 0x30, 0xe0, 0x30, 0x30, 0x1c, 0000},
    {0x18, 0x18, 0x18, 0000, 0x18, 0x18, 0x18, 0000},
    {0xe0, 0x30, 0x30, 0x1c, 0x30, 0x30, 0xe0, 0000},
    {0x76, 0xdc, 0000, 0000, 0000, 0000, 0000, 0000},
    {0000, 0x10, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0000},
    {0x78, 0xcc, 0xc0, 0xcc, 0x78, 0x18, 0x0c, 0x78},
    {0000, 0xcc, 0000, 0xcc, 0xcc, 0xcc, 0x7e, 0000},
    {0x1c, 0000, 0x78, 0xcc, 0xfc, 0xc0, 0x78, 0000},
    {0x7e, 0xc3, 0x3c, 0x06, 0x3e, 0x66, 0x3f, 0000},
    {0xcc, 0000, 0x78, 0x0c, 0x7c, 0xcc, 0x7e, 0000},
    {0xe0, 0000, 0x78, 0x0c, 0x7c, 0xcc, 0x7e, 0000},
    {0x30, 0x30, 0x78, 0x0c, 0x7c, 0xcc, 0x7e, 0000},
    {0000, 0000, 0x78, 0xc0, 0xc0, 0x78, 0x0c, 0x38},
    {0x7e, 0xc3, 0x3c, 0x66, 0x7e, 0x60, 0x3c, 0000},
    {0xcc, 0000, 0x78, 0xcc, 0xfc, 0xc0, 0x78, 0000},
    {0xe0, 0000, 0x78, 0xcc, 0xfc, 0xc0, 0x78, 0000},
    {0xcc, 0000, 0x70, 0x30, 0x30, 0x30, 0x78, 0000},
    {0x7c, 0xc6, 0x38, 0x18, 0x18, 0x18, 0x3c, 0000},
    {0xe0, 0000, 0x70, 0x30, 0x30, 0x30, 0x78, 0000},
    {0xc6, 0x38, 0x6c, 0xc6, 0xfe, 0xc6, 0xc6, 0000},
    {0x30, 0x30, 0000, 0x78, 0xcc, 0xfc, 0xcc, 0000},
    {0x1c, 0000, 0xfc, 0x60, 0x78, 0x60, 0xfc, 0000},
    {0000, 0000, 0x7f, 0x0c, 0x7f, 0xcc, 0x7f, 0000},
    {0x3e, 0x6c, 0xcc, 0xfe, 0xcc, 0xcc, 0xce, 0000},
    {0x78, 0xcc, 0000, 0x78, 0xcc, 0xcc, 0x78, 0000},
    {0000, 0xcc, 0000, 0x78, 0xcc, 0xcc, 0x78, 0000},
    {0000, 0xe0, 0000, 0x78, 0xcc, 0xcc, 0x78, 0000},
    {0x78, 0xcc, 0000, 0xcc, 0xcc, 0xcc, 0x7e, 0000},
    {0000, 0xe0, 0000, 0xcc, 0xcc, 0xcc, 0x7e, 0000},
    {0000, 0xcc, 0000, 0xcc, 0xcc, 0x7c, 0x0c, 0xf8},
    {0xc3, 0x18, 0x3c, 0x66, 0x66, 0x3c, 0x18, 0000},
    {0xcc, 0000, 0xcc, 0xcc, 0xcc, 0xcc, 0x78, 0000},
    {0x18, 0x18, 0x7e, 0xc0, 0xc0, 0x7e, 0x18, 0x18},
    {0x38, 0x6c, 0x64, 0xf0, 0x60, 0xe6, 0xfc, 0000},
    {0xcc, 0xcc, 0x78, 0xfc, 0x30, 0xfc, 0x30, 0x30},
    {0xf8, 0xcc, 0xcc, 0xfa, 0xc6, 0xcf, 0xc6, 0xc7},
    {0x0e, 0x1b, 0x18, 0x3c, 0x18, 0x18, 0xd8, 0x70},
    {0x1c, 0000, 0x78, 0x0c, 0x7c, 0xcc, 0x7e, 0000},
    {0x38, 0000, 0x70, 0x30, 0x30, 0x30, 0x78, 0000},
    {0000, 0x1c, 0000, 0x78, 0xcc, 0xcc, 0x78, 0000},
    {0000, 0x1c, 0000, 0xcc, 0xcc, 0xcc, 0x7e, 0000},
    {0000, 0xf8, 0000, 0xf8, 0xcc, 0xcc, 0xcc, 0000},
    {0xfc, 0000, 0xcc, 0xec, 0xfc, 0xdc, 0xcc, 0000},
    {0x3c, 0x6c, 0x6c, 0x3e, 0000, 0x7e, 0000, 0000},
    {0x38, 0x6c, 0x6c, 0x38, 0000, 0x7c, 0000, 0000},
    {0x30, 0000, 0x30, 0x60, 0xc0, 0xcc, 0x78, 0000},
    {0000, 0000, 0000, 0xfc, 0xc0, 0xc0, 0000, 0000},
    {0000, 0000, 0000, 0xfc, 0x0c, 0x0c, 0000, 0000},
    {0xc3, 0xc6, 0xcc, 0xde, 0x33, 0x66, 0xcc, 0x0f},
    {0xc3, 0xc6, 0xcc, 0xdb, 0x37, 0x6f, 0xcf, 0x03},
    {0x18, 0x18, 0000, 0x18, 0x18, 0x18, 0x18, 0000},
    {0000, 0x33, 0x66, 0xcc, 0x66, 0x33, 0000, 0000},
    {0000, 0xcc, 0x66, 0x33, 0x66, 0xcc, 0000, 0000},
    {0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88},
    {0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa},
    {0xdb, 0x77, 0xdb, 0xee, 0xdb, 0x77, 0xdb, 0xee},
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18},
    {0x18, 0x18, 0x18, 0x18, 0xf8, 0x18, 0x18, 0x18},
    {0x18, 0x18, 0xf8, 0x18, 0xf8, 0x18, 0x18, 0x18},
    {0x36, 0x36, 0x36, 0x36, 0xf6, 0x36, 0x36, 0x36},
    {0000, 0000, 0000, 0000, 0xfe, 0x36, 0x36, 0x36},
    {0000, 0000, 0xf8, 0x18, 0xf8, 0x18, 0x18, 0x18},
    {0x36, 0x36, 0xf6, 0x06, 0xf6, 0x36, 0x36, 0x36},
    {0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36},
    {0000, 0000, 0xfe, 0x06, 0xf6, 0x36, 0x36, 0x36},
    {0x36, 0x36, 0xf6, 0x06, 0xfe, 0000, 0000, 0000},
    {0x36, 0x36, 0x36, 0x36, 0xfe, 0000, 0000, 0000},
    {0x18, 0x18, 0xf8, 0x18, 0xf8, 0000, 0000, 0000},
    {0000, 0000, 0000, 0000, 0xf8, 0x18, 0x18, 0x18},
    {0x18, 0x18, 0x18, 0x18, 0x1f, 0000, 0000, 0000},
    {0x18, 0x18, 0x18, 0x18, 0xff, 0000, 0000, 0000},
    {0000, 0000, 0000, 0000, 0xff, 0x18, 0x18, 0x18},
    {0x18, 0x18, 0x18, 0x18, 0x1f, 0x18, 0x18, 0x18},
    {0000, 0000, 0000, 0000, 0xff, 0000, 0000, 0000},
    {0x18, 0x18, 0x18, 0x18, 0xff, 0x18, 0x18, 0x18},
    {0x18, 0x18, 0x1f, 0x18, 0x1f, 0x18, 0x18, 0x18},
    {0x36, 0x36, 0x36, 0x36, 0x37, 0x36, 0x36, 0x36},
    {0x36, 0x36, 0x37, 0x30, 0x3f, 0000, 0000, 0000},
    {0000, 0000, 0x3f, 0x30, 0x37, 0x36, 0x36, 0x36},
    {0x36, 0x36, 0xf7, 0000, 0xff, 0000, 0000, 0000},
    {0000, 0000, 0xff, 0000, 0xf7, 0x36, 0x36, 0x36},
    {0x36, 0x36, 0x37, 0x30, 0x37, 0x36, 0x36, 0x36},
    {0000, 0000, 0xff, 0000, 0xff, 0000, 0000, 0000},
    {0x36, 0x36, 0xf7, 0000, 0xf7, 0x36, 0x36, 0x36},
    {0x18, 0x18, 0xff, 0000, 0xff, 0000, 0000, 0000},
    {0x36, 0x36, 0x36, 0x36, 0xff, 0000, 0000, 0000},
    {0000, 0000, 0xff, 0000, 0xff, 0x18, 0x18, 0x18},
    {0000, 0000, 0000, 0000, 0xff, 0x36, 0x36, 0x36},
    {0x36, 0x36, 0x36, 0x36, 0x3f, 0000, 0000, 0000},
    {0x18, 0x18, 0x1f, 0x18, 0x1f, 0000, 0000, 0000},
    {0000, 0000, 0x1f, 0x18, 0x1f, 0x18, 0x18, 0x18},
    {0000, 0000, 0000, 0000, 0x3f, 0x36, 0x36, 0x36},
    {0x36, 0x36, 0x36, 0x36, 0xff, 0x36, 0x36, 0x36},
    {0x18, 0x18, 0xff, 0x18, 0xff, 0x18, 0x18, 0x18},
    {0x18, 0x18, 0x18, 0x18, 0xf8, 0000, 0000, 0000},
    {0000, 0000, 0000, 0000, 0x1f, 0x18, 0x18, 0x18},
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    {0000, 0000, 0000, 0000, 0xff, 0xff, 0xff, 0xff},
    {0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0},
    {0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f},
    {0xff, 0xff, 0xff, 0xff, 0000, 0000, 0000, 0000},
    {0000, 0000, 0x76, 0xdc, 0xc8, 0xdc, 0x76, 0000},
    {0000, 0x78, 0xcc, 0xf8, 0xcc, 0xf8, 0xc0, 0xc0},
    {0000, 0xfc, 0xcc, 0xc0, 0xc0, 0xc0, 0xc0, 0000},
    {0000, 0xfe, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0000},
    {0xfc, 0xcc, 0x60, 0x30, 0x60, 0xcc, 0xfc, 0000},
    {0000, 0000, 0x7e, 0xd8, 0xd8, 0xd8, 0x70, 0000},
    {0000, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x60, 0xc0},
    {0000, 0x76, 0xdc, 0x18, 0x18, 0x18, 0x18, 0000},
    {0xfc, 0x30, 0x78, 0xcc, 0xcc, 0x78, 0x30, 0xfc},
    {0x38, 0x6c, 0xc6, 0xfe, 0xc6, 0x6c, 0x38, 0000},
    {0x38, 0x6c, 0xc6, 0xc6, 0x6c, 0x6c, 0xee, 0000},
    {0x1c, 0x30, 0x18, 0x7c, 0xcc, 0xcc, 0x78, 0000},
    {0000, 0000, 0x7e, 0xdb, 0xdb, 0x7e, 0000, 0000},
    {0x06, 0x0c, 0x7e, 0xdb, 0xdb, 0x7e, 0x60, 0xc0},
    {0x38, 0x60, 0xc0, 0xf8, 0xc0, 0x60, 0x38, 0000},
    {0x78, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0000},
    {0000, 0xfc, 0000, 0xfc, 0000, 0xfc, 0000, 0000},
    {0x30, 0x30, 0xfc, 0x30, 0x30, 0000, 0xfc, 0000},
    {0x60, 0x30, 0x18, 0x30, 0x60, 0000, 0xfc, 0000},
    {0x18, 0x30, 0x60, 0x30, 0x18, 0000, 0xfc, 0000},
    {0x0e, 0x1b, 0x1b, 0x18, 0x18, 0x18, 0x18, 0x18},
    {0x18, 0x18, 0x18, 0x18, 0x18, 0xd8, 0xd8, 0x70},
    {0x30, 0x30, 0000, 0xfc, 0000, 0x30, 0x30, 0000},
    {0000, 0x76, 0xdc, 0000, 0x76, 0xdc, 0000, 0000},
    {0x38, 0x6c, 0x6c, 0x38, 0000, 0000, 0000, 0000},
    {0000, 0000, 0000, 0x18, 0x18, 0000, 0000, 0000},
    {0000, 0000, 0000, 0000, 0x18, 0000, 0000, 0000},
    {0x0f, 0x0c, 0x0c, 0x0c, 0xec, 0x6c, 0x3c, 0x1c},
    {0x78, 0x6c, 0x6c, 0x6c, 0x6c, 0000, 0000, 0000},
    {0x70, 0x18, 0x30, 0x60, 0x78, 0000, 0000, 0000},
    {0000, 0000, 0x3c, 0x3c, 0x3c, 0x3c, 0000, 0000},
    {0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000}
};

static void PutChar
(
    void * address,
    UINT32 depth,
    UINT32 pitch,
    UINT08 character,
    INT32  scale,
    UINT32 foregroundColor,
    UINT32 backgroundColor,
    bool   bUnderline,
    UINT32 displacementX = 0,
    UINT32 displacementY = 0,
    Surface2D *pSurf = nullptr
)
{
    // Draw the character.
    UINT08 * pY = static_cast<UINT08*>(address);

    if (pSurf == nullptr || pSurf->GetLayout() == Surface2D::Pitch)
    {
        for (INT32 row = 0; row < FONT_HEIGHT; ++row)
        {
            UINT08 mask = FontTable[character][row];

            for (INT32 column = 0; column < FONT_WIDTH; ++column)
            {
                // Mask pixels are left to right.
                bool isForeground =
                    (bUnderline && (row == (FONT_HEIGHT - 1))) ?
                    true :
                    (mask >> (FONT_WIDTH - 1 - column)) & 1;

                for (INT32 scaleY = 0; scaleY < scale; ++scaleY)
                {
                    UINT08 * pX = reinterpret_cast<UINT08*>(
                            pY + pitch*scaleY + column*depth/8*scale);
                    UINT16 * pX16 = reinterpret_cast<UINT16*>(pX);

                    for (INT32 scaleX = scale; scaleX; --scaleX)
                    {
                        if (isForeground)
                        {
                            if (8 == depth)
                            {
                                MEM_WR08(pX, foregroundColor & 0xFF);
                                pX++;
                            }
                            else
                            {
                                MEM_WR16(pX16, foregroundColor & 0xFFFF);
                                pX16++;
                                if (32 == depth)
                                {
                                    MEM_WR16(pX16,
                                             (foregroundColor >> 16) & 0xFFFF);
                                    pX16++;
                                }
                            }
                        }
                        else
                        {
                            if (8 == depth)
                            {
                                MEM_WR08(pX, backgroundColor & 0xFF);
                                pX++;
                            }
                            else
                            {
                                MEM_WR16(pX16, backgroundColor & 0xFFFF);
                                pX16++;

                                if (32 == depth)
                                {
                                    MEM_WR16(pX16,
                                             (backgroundColor >> 16) & 0xFFFF);
                                    pX16++;
                                }
                            }
                        }

                    } // scaleX

                } // scaleY

            } // column

            pY += pitch * scale;

        } // row
    }
    else if (pSurf->GetLayout() == Surface2D::BlockLinear)
    {
        // get height and widths of Gob/Block
        UINT32 gobHeight = pSurf->GetGpuDev()->GobHeight();
        UINT32 gobWidth = pSurf->GetGpuDev()->GobWidth();
        UINT32 blockHeightInGobs = 1 << pSurf->GetLogBlockHeight();
        UINT32 widthInBlocks = pSurf->GetBlockWidth();
        // width and height in bytes
        UINT32 blockWidth = gobWidth << pSurf->GetLogBlockWidth();
        UINT32 blockHeight = blockHeightInGobs * gobHeight;
        UINT32 blockSize = blockWidth * blockHeight;

        for (INT32 row = 0; row < FONT_HEIGHT; ++row)
        {
            UINT08 mask = FontTable[character][row];
            UINT32 gIncrementY = row * scale + displacementY;

            for (INT32 column = 0; column < FONT_WIDTH; ++column)
            {
                // Mask pixels are left to right.
                bool isForeground =
                    (bUnderline && (row == (FONT_HEIGHT - 1))) ?
                    true :
                    (mask >> (FONT_WIDTH - 1 - column)) & 1;
                UINT32 gIncrementX = column * (depth/8) * scale + displacementX;

                for (INT32 scaleY = 0; scaleY < scale; ++scaleY)
                {
                    // Effective address increament for Y direction
                    UINT32 incrementY = gIncrementY + scaleY;
                    UINT32 finalIncrementY =
                        (incrementY / blockHeight) * blockSize * widthInBlocks +
                        (incrementY & (blockHeight - 1)) * blockWidth -
                        displacementY * blockWidth;

                    for (INT32 scaleX = 0; scaleX < scale; ++scaleX)
                    {
                        // Effective Address increment for X direction

                        for (UINT32 idepth = 0; idepth < depth/8; ++idepth)
                        {
                            UINT32 incrementX =
                                gIncrementX + (depth / 8) * scaleX + idepth;
                            UINT32 finalIncrementX =
                                (incrementX / blockWidth) *
                                blockHeight * blockWidth +
                                (incrementX & (blockWidth - 1)) -
                                displacementX;
                            //final address to write the character pattern.
                            UINT08 * pX = reinterpret_cast<UINT08*>(
                                    pY + finalIncrementY + finalIncrementX);
                            if (isForeground)
                            {
                                MEM_WR08(pX,
                                         (foregroundColor >> idepth*8) & 0xFF);
                            }
                            else
                            {
                                MEM_WR08(pX,
                                         (backgroundColor >> idepth*8) & 0xFF);
                            }
                        } // idepth
                    } // scaleX
                } // scaleY
            } // column
        } // row
    } // layout check
    else
    {
        Printf(Tee::PriWarn,
            "%s =>  PutChar is supported for PITCH/BLOCKLINEAR memory layout.",
            __FUNCTION__);
    }
}

RC GpuUtility::PutChars
(
    INT32  x,
    INT32  y,
    const string &chars,
    INT32  scale,
    UINT32 foregroundColor,
    UINT32 backgroundColor,
    bool   bUnderline,
    Surface2D    * pSurf
)
{
    RC rc;
    MASSERT(x >= 0);
    MASSERT(y >= 0);
    MASSERT(scale > 0);
    MASSERT(pSurf);

    Surface2D::MappingSaver saveMapping(*pSurf);
    if (!pSurf->GetAddress())
    {
        CHECK_RC(pSurf->Map());
    }

    UINT32 height, width, depth, pitch;
    UINT64 offset;

    height = pSurf->GetHeight();
    width  = pSurf->GetWidth();
    depth  = ColorUtils::PixelBits(pSurf->GetColorFormat());
    pitch  = pSurf->GetPitch();

    if ((16 != depth) && (32 != depth) && (8 != depth))
        return RC::UNSUPPORTED_DEPTH;

    INT32 numCharsToPrint = static_cast<INT32>(chars.size());

    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        if ((static_cast<UINT32>(x) +
             scale * FONT_WIDTH * numCharsToPrint) > width)
        {
            numCharsToPrint = (width - x) / (scale * FONT_WIDTH);
            if (numCharsToPrint <= 0)
            {
                Printf(Tee::PriNormal, "PutChars: skipping string \"%s\"\n",
                       chars.c_str());
                return OK;
            }
        }
    }

    UINT32 endingPosition = (static_cast<UINT32>(x) +
                             scale * FONT_WIDTH * numCharsToPrint);

    if ((endingPosition > width) ||
        ((static_cast<UINT32>(y) + (scale * FONT_HEIGHT)) > height))
    {
        Printf(Tee::PriNormal,
            "Output of string \"%s\" to surface went out of surface bounds\n",
            chars.c_str());
        return RC::BAD_PARAMETER;
    }

    // Map the primary surface
    UINT08 *pBase, *pAddress, *pStartAddress;
    UINT32 blockWidth = (pSurf->GetGpuDev()->GobWidth() <<
                         pSurf->GetLogBlockWidth());
    UINT32 gobHeight = pSurf->GetGpuDev()->GobHeight();
    UINT32 blockHeight = (1 << pSurf->GetLogBlockHeight()) * gobHeight;

    // Saving an current mapping to ensure that if Mapping gets changed
    // then we restore original mapping when the function exits
    Surface2D::MappingSaver savedMapping(*pSurf);

    // Get the offset value with respect to start of Mapped region
    offset  = pSurf->GetPixelOffset(x, y);
    CHECK_RC(pSurf->MapOffset(offset, &offset));
    pBase    = static_cast<UINT08*>(pSurf->GetAddress());
    pAddress = pBase + offset;
    pStartAddress = pAddress;
    // position of x cordinate wrt start position of a block before
    // writing anything
    UINT32 initialDisplacementX = (x * (depth / 8)) & (blockWidth - 1);
    UINT32 initialDisplacementY = y & (blockHeight - 1);
    // Increment in x cordinate wrt start position of a block
    UINT32 incrementX = initialDisplacementX;

    for (string::size_type j = 0; j < chars.size(); ++j)
    {
        if (pSurf->GetLayout() == Surface2D::BlockLinear)
        {
            ::PutChar(pAddress, depth, pitch, chars[j], scale,
                      foregroundColor, backgroundColor, bUnderline,
                      incrementX & (blockWidth - 1),
                      initialDisplacementY, pSurf);
            // Get address for the next character
            // Adr = startadr + (displacementX/blockwidth)*blocksize +
            //       displacementX % blockwidth
            incrementX += FONT_WIDTH * (depth / 8) * scale;
            pAddress =
                pStartAddress +
                (incrementX / blockWidth) *
                (1 << pSurf->GetLogBlockHeight()) * gobHeight * blockWidth +
                (incrementX & (blockWidth - 1)) - initialDisplacementX;
        }
        else
        {
            ::PutChar(pAddress, depth, pitch, chars[j], scale,
                      foregroundColor, backgroundColor, bUnderline);
            pAddress += (FONT_WIDTH * depth / 8) * scale;
        }
    }
    return rc;
}

RC GpuUtility::PutChar
(
    INT32  x,
    INT32  y,
    char   c,
    INT32  scale,
    UINT32 foregroundColor,
    UINT32 backgroundColor,
    bool   bUnderline,
    Surface2D    * pSurf
)
{
    RC rc;

    MASSERT(x >= 0);
    MASSERT(y >= 0);
    MASSERT(scale > 0);
    MASSERT(pSurf->GetAddress());

    UINT32 height, width, depth, pitch;
    UINT64 offset;

    height = pSurf->GetHeight();
    width  = pSurf->GetWidth();
    depth  = ColorUtils::PixelBits(pSurf->GetColorFormat());
    pitch  = pSurf->GetPitch();

    if ((16 != depth) && (32 != depth) && (8 != depth))
        return RC::UNSUPPORTED_DEPTH;

    if (((static_cast<UINT32>(x) + (scale * FONT_WIDTH)) > width) ||
        ((static_cast<UINT32>(y) + (scale * FONT_HEIGHT)) > height))
    {
        Printf(Tee::PriNormal,
               "Output of %c to surface went out of surface bounds\n",
               c);
        return RC::BAD_PARAMETER;
    }

    // Map the primary surface
    UINT08 *pBase, *pAddress;

    // Saving an current mapping to ensure that if Mapping gets changed
    // then we restore original mapping when the function exits
    Surface2D::MappingSaver savedMapping(*pSurf);

    // Get the offset value with respect to start of Mapped region
    offset  = pSurf->GetPixelOffset(x, y);
    CHECK_RC(pSurf->MapOffset(offset, &offset));
    pBase    = static_cast<UINT08 *>(pSurf->GetAddress());
    pAddress = pBase + offset;

    UINT32 blockWidth = (pSurf->GetGpuDev()->GobWidth() <<
                         pSurf->GetLogBlockWidth());
    UINT32 gobHeight = pSurf->GetGpuDev()->GobHeight();
    UINT32 blockHeight = (1 << pSurf->GetLogBlockHeight()) * gobHeight;

    // position of x cordinate wrt start position of a block before
    // writing anything
    UINT32 initialDisplacementX = (x * (depth / 8)) & (blockWidth - 1);
    UINT32 initialDisplacementY = y & (blockHeight - 1);

    ::PutChar(pAddress, depth, pitch, c, scale,
              foregroundColor, backgroundColor, bUnderline,
              initialDisplacementX, initialDisplacementY, pSurf);

    return rc;
}

RC GpuUtility::PutChars
(
    INT32  x,
    INT32  y,
    const string &chars,
    INT32  scale,           // = 1
    UINT32 foregroundColor, // = 0xFFFFFFFF
    UINT32 backgroundColor, // = 0
    UINT32 disp,            // = DISPLAY_DEFAULT_PUTCHARS
    GpuSubdevice * pSubdev,
    Surface2D::Layout layout  // = Surface2D::Pitch
)
{
    MASSERT(x >= 0);
    MASSERT(y >= 0);
    MASSERT(scale > 0);

    // for blockLinear memory layout we need information about
    // gobs/blocks that is stored in surface2d.
    if (layout != Surface2D::Pitch)
    {
        Printf(Tee::PriError,
               "%s => This is supported only for PITCH memory layout."
               " Use other Putchars method for other layouts",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    GpuSubdevice * pGpuSubdevice = pSubdev;
    MASSERT(pGpuSubdevice);

    Display * pDisplay = pGpuSubdevice->GetParentDevice()->GetDisplay();
    RC rc;

    if (DISPLAY_DEFAULT_PUTCHARS == disp)
        disp = pDisplay->Selected();

    if (0 == disp)
        return RC::ILWALID_DISPLAY_MASK;

    for (UINT32 i = 0; i < 24; ++i)
    {
        if (disp & (1 << i))
        {
            disp = 1 << i;
            break;
        }
    }

    UINT32 height, width, depth, pitch;
    UINT64 offset;
    CHECK_RC(pDisplay->GetMode(disp, &width, &height, &depth, 0));
    CHECK_RC(pDisplay->GetPitch(disp, &pitch));
    CHECK_RC(pDisplay->GetImageOffset(disp, &offset));

    if ((16 != depth) && (32 != depth) && (8 != depth))
        return RC::UNSUPPORTED_DEPTH;

    const UINT32 endingPosistion =
        static_cast<UINT32>(x) +
        (scale * FONT_WIDTH * static_cast<UINT32>(chars.size()));

    if ((endingPosistion > width) ||
        ((static_cast<UINT32>(y) + (scale * FONT_HEIGHT)) > height))
    {
        Printf(Tee::PriNormal,
               "Output of string \"%s\" to screen went out of screen bounds\n",
               chars.c_str());
        return RC::BAD_PARAMETER;
    }

    // Map only region where we want to print message.
    UINT64 xPos = offset + y * pitch;
    UINT32 heightToMap = FONT_HEIGHT * scale;
    UINT64 sizeToMap = xPos + heightToMap * pitch;

    // Map the primary surface
    UINT08 *pBase;
    UINT08 *pAddress;
    LwRmPtr pLwRm;
    CHECK_RC(pLwRm->MapFbMemory(xPos,
                                sizeToMap,
                                (void **)&pBase,
                                0,
                                pGpuSubdevice));

    // Get the offset value with respect to start of Mapped region
    pAddress = pBase + x * depth / 8;

    for (string::size_type j = 0; j < chars.size(); ++j)
    {
        ::PutChar(pAddress, depth, pitch, chars[j], scale,
                  foregroundColor, backgroundColor, false);
        pAddress += (FONT_WIDTH * depth / 8) * scale;
    }

    pLwRm->UnmapFbMemory(pBase, 0, pGpuSubdevice);

    return rc;
}

RC GpuUtility::PutCharsHorizCentered
(
    INT32  screenWidth,
    INT32  y,
    string chars,
    INT32  scale,
    UINT32 foregroundColor,
    UINT32 backgroundColor,
    bool   bUnderline,
    Surface2D* pSurface
)
{
    const INT32 charsWidth =
        scale * FONT_WIDTH * static_cast<UINT32>(chars.size());
    return PutChars((screenWidth - charsWidth) / 2, y, chars, scale,
                    foregroundColor, backgroundColor, bUnderline, pSurface);
}

RC GpuUtility::PutCharsHorizCentered
(
    INT32  screenWidth,
    INT32  y,
    string chars,
    INT32  scale,           // = 1
    UINT32 foregroundColor, // = 0xFFFFFFFF
    UINT32 backgroundColor, // = 0
    UINT32 disp,            // = DISPLAY_DEFAULT_PUTCHARS
    GpuSubdevice * pSubdev,
    Surface2D::Layout layout // Surface2D::Pitch
    )
{
    const INT32 charsWidth =
        scale * FONT_WIDTH * static_cast<UINT32>(chars.size());
    return PutChars((screenWidth - charsWidth) / 2, y, chars, scale,
                      foregroundColor, backgroundColor, disp, pSubdev, layout);
}

RC GpuUtility::DmaCopySurf
(
    Surface2D* pSrcSurf,
    Surface2D* pDstSurf,
    GpuSubdevice* pSubdev,
    FLOAT64 timeoutMs
)
{
    MASSERT(pSrcSurf);
    MASSERT(pDstSurf);
    MASSERT(pSubdev);
    RC rc;
    if ((pDstSurf->GetWidth() < pSrcSurf->GetWidth()) ||
        pDstSurf->GetHeight() < pSrcSurf->GetHeight())
    {
        Printf(Tee::PriError, "Invalid Dst/Src surface dimensions\n");
        return RC::BAD_PARAMETER;
    }

    GpuDevice* pDevice = pSubdev->GetParentDevice();
    GpuTestConfiguration testConfig;
    testConfig.BindGpuDevice(pDevice);
    CHECK_RC(testConfig.InitFromJs());
    {   // Destructor of DmaWrap has to be called before
        //  destructor of testConfig
        DmaWrapper DmaWrap(pSrcSurf,
                           pDstSurf,
                           &testConfig);

        CHECK_RC(DmaWrap.Copy(
                0,                    // Starting src X, in bytes not pixels.
                0,                    // Starting src Y, in lines.
                pSrcSurf->GetPitch(), // Width of copied rect, in bytes.
                pSrcSurf->GetHeight(), // Height of copied rect, in bytes
                0,                    // Starting dst X, in bytes not pixels.
                0,                    // Starting dst Y, in lines.
                timeoutMs,
                pSubdev->GetSubdeviceInst()));
    }
    return rc;
}

RC GpuUtility::FillRgb
(
    Surface2D* pDstSurf,
    FLOAT64 timeoutMs,
    GpuSubdevice* pSubdevice,
    Surface2D* pCachedSurf
)
{
    MASSERT(pDstSurf);
    RC rc;
    Memory::Location dstLoc = pDstSurf->GetLocation();
    UINT32 width = pDstSurf->GetWidth();
    UINT32 height = pDstSurf->GetHeight();
    UINT32 pitch = pDstSurf->GetPitch();
    UINT32 depth = ColorUtils::PixelBytes(pDstSurf->GetColorFormat());

    // directly call Memory::FillRgb if pDstSurf is already
    // in system memory - todo: take account of Optimal...
    if (dstLoc == Memory::NonCoherent ||
        dstLoc == Memory::Coherent)
    {
        return Memory::FillRgb(pDstSurf->GetAddress(),
                               width,
                               height,
                               depth * 8,   // to bits per pixel
                               pitch);
    }

    unique_ptr<Surface2D> pTmpCachedSurf;

    if (!pCachedSurf)
    {
        // allocate a new surface that's identical to Dst, except for location
        pTmpCachedSurf.reset(new Surface2D());
        pCachedSurf = pTmpCachedSurf.get();
        MASSERT(pCachedSurf);
        pCachedSurf->SetWidth(width);
        pCachedSurf->SetHeight(height);
        pCachedSurf->SetColorFormat(pDstSurf->GetColorFormat());
        pCachedSurf->SetLocation(Memory::Coherent);
        // do we need to bind a channel?
        CHECK_RC(pCachedSurf->Alloc(pSubdevice->GetParentDevice()));
        CHECK_RC(pCachedSurf->Map(pSubdevice->GetSubdeviceInst()));
    }
    else
    {
        // validate that CachedSurf and pDstSurf have similar dimension
        if ((pCachedSurf->GetWidth() != width) &&
            (pCachedSurf->GetHeight() != height) &&
            (pCachedSurf->GetPitch() != pitch) &&
            (pCachedSurf->GetColorFormat() != pDstSurf->GetColorFormat()))
        {
            Printf(Tee::PriError, "Input surfaces do not match - aborting\n");
            CHECK_RC(RC::SOFTWARE_ERROR);
        }
    }
    CHECK_RC(Memory::FillRgb(pCachedSurf->GetAddress(),
                             width,
                             height,
                             depth * 8,     // to bits per pixel
                             pitch));

    CHECK_RC(DmaCopySurf(pCachedSurf,
                         pDstSurf,
                         pSubdevice,
                         timeoutMs));

    return rc;
}

RC GpuUtility::GetGpuCacheExist(GpuDevice *pGpuDevice, bool *pGpuCacheExist, LwRm* pLwRm)
{
    UINT08 fb_caps[LW0080_CTRL_FB_CAPS_TBL_SIZE] = { };
    LW0080_CTRL_FB_GET_CAPS_PARAMS fb_params = { };
    RC rc;

    fb_params.capsTbl = LW_PTR_TO_LwP64(fb_caps);
    fb_params.capsTblSize = LW0080_CTRL_FB_CAPS_TBL_SIZE;

    MASSERT(pGpuDevice);

    rc = pLwRm->Control(pLwRm->GetDeviceHandle(pGpuDevice),
                           LW0080_CTRL_CMD_FB_GET_CAPS,
                           &fb_params,
                           sizeof(fb_params));

    if (rc == OK)
    {
        *pGpuCacheExist = (LW0080_CTRL_FB_GET_CAP(fb_caps,
            LW0080_CTRL_FB_CAPS_SUPPORT_CACHED_SYSMEM) != 0);
    }
    else
        *pGpuCacheExist = false;

    return rc;
}

bool GpuUtility::MustRopSysmemBeCached(GpuDevice *pGpuDevice, LwRm* pLwRm, UINT32 hChannel)
{
    MASSERT(pLwRm);
    MASSERT(pGpuDevice);
    RC rc;

    LW0080_CTRL_GR_GET_CAPS_V2_PARAMS grParams;
    memset(&grParams, 0, sizeof(grParams));

    grParams.grRouteInfo.flags = FLD_SET_DRF(2080, _CTRL_GR_ROUTE_INFO_FLAGS, _TYPE, _CHANNEL,
                                           grParams.grRouteInfo.flags);
    grParams.grRouteInfo.route = FLD_SET_DRF_NUM64(2080, _CTRL_GR_ROUTE_INFO_DATA, _CHANNEL_HANDLE,
                                                 hChannel, grParams.grRouteInfo.route);

    grParams.bCapsPopulated = LW_FALSE;

    rc = pLwRm->Control(pLwRm->GetDeviceHandle(pGpuDevice),
                        LW0080_CTRL_CMD_GR_GET_CAPS_V2,
                        &grParams,
                        sizeof(grParams));

    MASSERT(rc == OK);

    bool mustBeCached = (LW0080_CTRL_GR_GET_CAP(grParams.capsTbl,
        LW0080_CTRL_GR_CAPS_ROP_REQUIRES_CACHED_SYSMEM) != 0);

    return mustBeCached;
}

//! Decodes partitions/channels (i.e. individual memory chips) for a specified
//! physical address range of FB.
//!
//! The specified range of physical addresses is divided into equal-size
//! chunks. All dwords in each chunk belong to one single partition and
//! channel. The function finds the maximum size of the chunks for which
//! the above is true. For each chunk it generates an identifier as:
//! id = partition*numChannels+channel which uniquely tells to which
//! partition and channel the chunk belongs.
//!
//! \return The output of the function is an array which contains an identifier
//! for every chunk.
//!
//! \param pSubdev GPU subdevice on which the memory is located.
//! \param chunk Chunk of allocated memory on that subdevice.
//! \param begin Beginning physical address of the chunk to decode.
//! \param end End physical address (offset of first byte after the chunk).
//! \param pChunks Array filled by the function (return value).
//! \param pChunkSize Chunk size in bytes (return value).
RC GpuUtility::DecodeRangePartChan
(
    GpuSubdevice* pSubdev,
    const GpuUtility::MemoryChunkDesc& chunk,
    UINT64 begin,
    UINT64 end,
    vector<UINT08>* pChunks,
    UINT32* pChunkSize
)
{
    MASSERT(pChunks);
    MASSERT(pChunkSize);
    RC rc;

    // Get FB configuration
    FrameBuffer* const pFB = pSubdev->GetFB();
    const UINT32 burstLength = pFB->GetBurstSize();
    const UINT32 numChannels = pFB->GetSubpartitions();

    // Special case if there is only one partition and one channel
    // or FB is in sysmem.
    const UINT32 numFbios = pFB->GetFbioCount();
    if (((numFbios == 1) && (numChannels == 1)) ||
        (pSubdev->HasFeature(Device::GPUSUB_HAS_FRAMEBUFFER_IN_SYSMEM) &&
         !pSubdev->IsSOC()))
    {
        pChunks->clear();
        pChunks->push_back(0);
        if ((end - begin) > 0xFFFFFFFF)
            return RC::DATA_TOO_LARGE;
        *pChunkSize = static_cast<UINT32>(end - begin);
        return OK;
    }
    MASSERT(burstLength > 0);

    // Must be aligned on burst length
    if ((begin % burstLength != 0) || ((end - begin) % burstLength != 0))
    {
        Printf(Tee::PriError,
               "Address range must be aligned on burst length\n");
        return RC::BAD_PARAMETER;
    }

    // Prepare output array
    pChunks->clear();
    pChunks->reserve(size_t((end-begin)/burstLength));

    // Fill the output array, assuming a chunk size of 1
    for (UINT64 addr=begin; addr < end; addr += burstLength)
    {
        FbDecode decode;
        CHECK_RC(pFB->DecodeAddress(&decode, addr, chunk.pteKind,
                                    chunk.pageSizeKB));
        MASSERT(decode.virtualFbio  < numFbios);
        MASSERT(decode.subpartition < numChannels);
        pChunks->push_back(decode.virtualFbio * numChannels +
                           decode.subpartition);
    }

    // Find the smallest number of adjacent chunks with the same id
    if (pChunks->size() > 0xFFFFFFFF)
        return RC::DATA_TOO_LARGE;
    UINT32 chunkSize = (UINT32)pChunks->size();
    for (UINT32 i=1; i < pChunks->size(); i++)
    {
        if ((*pChunks)[i] != (*pChunks)[i-1])
        {
            chunkSize = Utility::Gcf(chunkSize, i);
        }
    }
    MASSERT((pChunks->size() % chunkSize) == 0);

    // Coalesce chunks into optimal chunk size
    for (UINT32 isrc = 0, idest = 0;
         isrc < pChunks->size();
         isrc += chunkSize, idest++)
    {
        const UINT32 id = (*pChunks)[isrc];
        (*pChunks)[idest] = id;
    }

    // Update chunks and callwlate final chunk size in bytes
    pChunks->resize(pChunks->size() / chunkSize);
    UINT64 finalChunkSize = (end - begin) / pChunks->size();
    if (finalChunkSize > 0xFFFFFFFF)
        return RC::DATA_TOO_LARGE;
    *pChunkSize = static_cast<UINT32>(finalChunkSize);
    return OK;
}

//! \brief hepler function to put the current thread to sleep
//!         for a given number(base on ptimer) of milliseconds,
//!         giving up its CPU time to other threads.
//!
//! \param  milliseconds[in ] : Milliseconds to sleep.
//
//------------------------------------------------------------------------------
void GpuUtility::SleepOnPTimer(FLOAT64 milliseconds, GpuSubdevice *pSubdev)
{
    FLOAT64 end = GetLwTimerMs(pSubdev) + milliseconds;
    // Use a more lightweight yield to finish off the sleep operation
    do
    {
        Tasker::PollHwSleep();
    } while (GetLwTimerMs(pSubdev) < end);
}

// GPU version of SendTrigger
//------------------------------------------------------------------------------
#define LW_PBUS_PCI_LW_0         0x00001800 /* R--4R */
void GoldenUtils::SendTrigger(UINT32 subdev)
{
    GpuSubdevice *pSubdev = ((GpuDevMgr *)DevMgrMgr::d_GraphDevMgr)->
                                 GetSubdeviceByGpuInst(subdev);
    MASSERT(pSubdev);

    // LW_PBUS_PCI_LW_0 is a read-only register.
    pSubdev->RegWr32(LW_PBUS_PCI_LW_0, 0x12345678);
}
#undef LW_PBUS_PCI_LW_0

//------------------------------------------------------------------------------
// Script properties and methods.
//------------------------------------------------------------------------------

// Properties
C_(Global_BenchmarkFill);
static STest Global_BenchmarkFill
(
    "BenchmarkFill",
    C_Global_BenchmarkFill,
    2,
    "Benchmark how long it takes to fill a region."
);

C_(Global_GetLwTimerMs);
static SMethod Global_GetLwTimerMs
(
    "GetLwTimerMs",
    C_Global_GetLwTimerMs,
    0,
    "Get the absolute time of the LW counter in ms"
);

C_(Global_TurnFanOn);
static STest Global_TurnFanOn
(
    "TurnFanOn",
    C_Global_TurnFanOn,
    0,
    "Turn the cooling fan on."
);

C_(Global_TurnFanOff);
static STest Global_TurnFanOff
(
    "TurnFanOff",
    C_Global_TurnFanOff,
    0,
    "Turn the cooling fan off."
);

C_(Global_GetChipsetInfo);
static SMethod Global_GetChipsetInfo
(
    "GetChipsetInfo",
    C_Global_GetChipsetInfo,
    1,
    "Get chipset information from the RM"
);

C_(Global_PutChars2);
static STest Global_PutChars2
(
    "PutChars2",
    C_Global_PutChars2,
    5,
    "Draw characters starting at (x,y). Upper left is (0,0)."
);

C_(Global_PutChars);
static STest Global_PutChars
(
    "PutChars",
    C_Global_PutChars,
    5,
    "Deprecated in favor of PutChars2."
);

C_(Global_PutCharsHorizCentered2);
static STest Global_PutCharsHorizCentered2
(
    "PutCharsHorizCentered2",
    C_Global_PutCharsHorizCentered2,
    8,
    "Draw characters horizontally centered."
);

C_(Global_PutCharsHorizCentered);
static STest Global_PutCharsHorizCentered
(
    "PutCharsHorizCentered",
    C_Global_PutCharsHorizCentered,
    6,
    "Deprecated in favor of PutCharsHorizCentered2."
);

C_(Global_FillRgb);
static STest Global_FillRgb
(
    "FillRgb",
    C_Global_FillRgb,
    4,
    "FillRgb pattern to given surface"
);

C_(Global_DmaCopySurf);
static STest Global_DmaCopySurf
(
    "DmaCopySurf2D",
    C_Global_DmaCopySurf,
    3,
    "Copy surfaces using Dma"
);

C_(Global_CompareBuffers);
static STest Global_CompareBuffers
(
   "CompareBuffers",
   C_Global_CompareBuffers,
   3,
   "Compare 2 GPU buffers"
);

// Implementation
C_(Global_FillRgb)
{
    MASSERT(pContext   != 0);
    MASSERT(pArguments != 0);

    const char usage[] =
        "Usage: FillRgb(DstSurf, Subdev, Timeout, CachedSurf - optional)\n";
    if (NumArguments < 3 || NumArguments > 4)
    {
        JS_ReportError(pContext, "invalid num of args\n");
        return JS_FALSE;
    }

    JavaScriptPtr pJS;
    JSObject *jsDstSurf;
    JSObject *jsGpuSubdevice;
    FLOAT64 timeoutMs;

    if ((OK != pJS->FromJsval(pArguments[0], &jsDstSurf)) ||
        (OK != pJS->FromJsval(pArguments[1], &jsGpuSubdevice)) ||
        (OK != pJS->FromJsval(pArguments[2], &timeoutMs)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JSObject *jsCachedSurf;
    if ((NumArguments == 4) &&
        (OK != pJS->FromJsval(pArguments[1], &jsCachedSurf)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsGpuSubdevice *pJsGpuSubdev = JS_GET_PRIVATE(
            JsGpuSubdevice, pContext, jsGpuSubdevice, "GpuSubdevice");
    Surface2D* pDstSurf = JS_GET_PRIVATE(Surface2D, pContext,
                                         jsDstSurf, "Surface2D");

    if (pJsGpuSubdev == nullptr || pDstSurf == nullptr)
    {
        return JS_FALSE;
    }

    Surface2D* pCachedSurf = nullptr;
    if (NumArguments == 4)
    {
        pCachedSurf = JS_GET_PRIVATE(Surface2D, pContext,
                                     jsCachedSurf, "Surface2D");
        if (pCachedSurf == nullptr)
            return JS_FALSE;
    }
    GpuSubdevice *pSubdev = pJsGpuSubdev->GetGpuSubdevice();

    RC rc;
    C_CHECK_RC(GpuUtility::FillRgb(pDstSurf,
                                   timeoutMs,
                                   pSubdev,
                                   pCachedSurf));

    RETURN_RC(rc);
}

C_(Global_DmaCopySurf)
{
    MASSERT(pContext   != 0);
    MASSERT(pArguments != 0);

    const char usage[] = "Usage: DmaCopySurf(DstSurf, SrcSurf, Subdev)\n";
    if (NumArguments != 3)
    {
        JS_ReportError(pContext, "invalid num of args\n");
        return JS_FALSE;
    }

    JavaScriptPtr pJS;
    JSObject *jsDstSurf;
    JSObject *jsSrcSurf;
    JSObject *jsGpuSubdevice;
    if ((OK != pJS->FromJsval(pArguments[0], &jsDstSurf)) ||
        (OK != pJS->FromJsval(pArguments[1], &jsSrcSurf)) ||
        (OK != pJS->FromJsval(pArguments[2], &jsGpuSubdevice)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsGpuSubdevice *pJsGpuSubdev = JS_GET_PRIVATE(JsGpuSubdevice, pContext,
                                                  jsGpuSubdevice,
                                                  "GpuSubdevice");
    Surface2D* pDstSurf = JS_GET_PRIVATE(Surface2D, pContext,
                                         jsDstSurf, "Surface2D");
    Surface2D* pSrcSurf = JS_GET_PRIVATE(Surface2D, pContext,
                                         jsSrcSurf, "Surface2D");

    if (pJsGpuSubdev == nullptr || pDstSurf == nullptr || pSrcSurf == nullptr)
    {
        JS_ReportError(pContext, "Cannot colwert input parameters\n");
        return JS_FALSE;
    }

    GpuSubdevice *pSubdev = pJsGpuSubdev->GetGpuSubdevice();

    RC rc;
    GpuTestConfiguration testConfig;
    testConfig.BindGpuDevice(pSubdev->GetParentDevice());
    C_CHECK_RC(testConfig.InitFromJs());
    FLOAT64 timeoutMs = testConfig.TimeoutMs();
    C_CHECK_RC(GpuUtility::DmaCopySurf(pSrcSurf, pDstSurf, pSubdev, timeoutMs));

    RETURN_RC(rc);
}

C_(Global_CompareBuffers)
{
    MASSERT(pContext   != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    RC rc;
    JavaScriptPtr pJS;
    JSObject *jsSurf1;
    JSObject *jsSurf2;
    UINT32 mask = 0;
    JSObject * pReturnObj = 0;

    const char usage[] = "Usage: CompareBuffers(Surface2D_1, Surface2D_2, mask[default 0xF], result)\n";
    if (4 != NumArguments ||
       (OK != pJS->FromJsval(pArguments[0], &jsSurf1)) ||
       (OK != pJS->FromJsval(pArguments[1], &jsSurf2)) ||
       (OK != pJS->FromJsval(pArguments[2], &mask)) ||
       (OK != pJS->FromJsval(pArguments[3], &pReturnObj)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    Surface2D* pSurf1 = nullptr;
    Surface2D* pSurf2 = nullptr;
    if (((pSurf1 = JS_GET_PRIVATE(Surface2D, pContext, jsSurf1, "Surface2D")) == 0) ||
       ((pSurf2 = JS_GET_PRIVATE(Surface2D, pContext, jsSurf2, "Surface2D")) == 0))
    {
        JS_ReportError(pContext, "Cannot colwert input parameters.\n");
        return JS_FALSE;
    }
    bool result = false;
    MASSERT(pSurf1->GetGpuDev() == pSurf2->GetGpuDev());
    if (mask == 0 ||
        mask == 0xF)   //compare all components
    {
        C_CHECK_RC(GpuUtility::CompareBuffers(pSurf1->GetGpuDev(),
            pSurf1->GetMemHandle(), pSurf1->GetAllocSize(), pSurf1->GetLocation(),
            pSurf2->GetMemHandle(), pSurf2->GetAllocSize(), pSurf2->GetLocation(),
            &result));

    }
    else
    {
        C_CHECK_RC(GpuUtility::CompareBuffersComponents(pSurf1->GetGpuDev(),
            pSurf1->GetMemHandle(), pSurf1->GetAllocSize(), pSurf1->GetLocation(),
            pSurf2->GetMemHandle(), pSurf2->GetAllocSize(), pSurf2->GetLocation(),
            mask,
            &result));
    }
    C_CHECK_RC(pJS->SetElement(pReturnObj, 0, result));
    RETURN_RC(rc);
}

// STest
C_(Global_BenchmarkFill)
{
    STEST_HEADER(2, 3, "Usage: BenchmarkFill(is write combined, loops, GpuSubdevice)");
    STEST_ARG(0, INT32, isWriteCombined);
    STEST_ARG(1, INT32, loops);
    STEST_PRIVATE_OPTIONAL_ARG(2, JsGpuSubdevice, pJsSub, "GpuSubdevice");

    GpuSubdevice* pGpuSubdev = nullptr;
    if (!pJsSub)
    {
        static Deprecation depr("BenchmarkFill", "3/30/2019",
                                "Must specify a GpuSubdevice as the third parameter");
        if (!depr.IsAllowed(pContext))
            return JS_FALSE;

        pGpuSubdev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();
    }
    else
    {
        pGpuSubdev = pJsSub->GetGpuSubdevice();
    }

    LwRmPtr pLwRm;
    LwRm::Handle hMem;

    UINT32 * memAddr  = 0;
    UINT32   memSize  = 4*1024*1024;
    UINT32   memoryFlags;
    if (isWriteCombined)
    {
        memoryFlags =
            DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS) |
            DRF_DEF(OS02, _FLAGS, _LOCATION,    _PCI) |
            DRF_DEF(OS02, _FLAGS, _COHERENCY,   _WRITE_COMBINE);
    }
    else
    {
        memoryFlags =
            DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS) |
            DRF_DEF(OS02, _FLAGS, _LOCATION,    _PCI) |
            DRF_DEF(OS02, _FLAGS, _COHERENCY,   _UNCACHED);
    }

    RC rc;

    C_CHECK_RC(pLwRm->AllocSystemMemory(&hMem, memoryFlags, memSize, pGpuSubdev->GetParentDevice()));
    C_CHECK_RC(pLwRm->MapMemory(hMem, 0, memSize, (void **)&memAddr, 0, pGpuSubdev));

    for (INT32 i = loops; i; --i)
    {
        memset(memAddr, 0, memSize);
    }

    pLwRm->Free(hMem);

    RETURN_RC(RC::OK);
}

// SMethod
C_(Global_GetLwTimerMs)
{
    STEST_HEADER(0, 1, "Usage: GetLwtimerMs(GpuSubdevice)");
    STEST_PRIVATE_OPTIONAL_ARG(0, JsGpuSubdevice, pJsSub, "GpuSubdevice");

    GpuSubdevice* pSubdev = nullptr;
    if (!pJsSub)
    {
        static Deprecation depr("GetLwTimerMs", "3/30/2019",
                                "Must specify a GpuSubdevice object parameter");
        if (!depr.IsAllowed(pContext))
            return JS_FALSE;

        pSubdev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();
    }
    else
    {
        pSubdev = pJsSub->GetGpuSubdevice();
    }

    UINT32 value = GpuUtility::GetLwTimerMs(pSubdev);

    if (OK != pJavaScript->ToJsval(value, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in GetLwTimerMs().");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// STest
C_(Global_TurnFanOn)
{
    static Deprecation depr("TurnFanOn", "3/30/2019",
                            "Use GpuSubdevice.GpioSetOutput(0,1) instead\n");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // Check this is a void method.
    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: TurnFanOn()");
        return JS_FALSE;
    }

    GpuSubdevice* pSubdev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();
    pSubdev->GpioSetOutput(0, 1);

    RETURN_RC(OK);
}

// STest
C_(Global_TurnFanOff)
{
    static Deprecation depr("TurnFanOff", "3/30/2019",
                            "Use GpuSubdevice.GpioSetOutput(0,0) instead\n");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // Check this is a void method.
    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: TurnFanOff()");
        return JS_FALSE;
    }

    GpuSubdevice* pSubdev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();
    pSubdev->GpioSetOutput(0, 0);

    RETURN_RC(OK);
}

C_(Global_GetChipsetInfo)
{
    JavaScriptPtr pJs;
    JSObject * pReturlwals;
    RC rc;

    if ((NumArguments != 1) ||
        (OK != pJs->FromJsval(pArguments[0], &pReturlwals)))
    {
        JS_ReportError(pContext, "Usage: GetChipsetInfo( array )");
        return JS_FALSE;
    }

    LW0000_CTRL_SYSTEM_GET_CHIPSET_INFO_PARAMS chipsetParams;
    memset(&chipsetParams, 0, sizeof(chipsetParams));
    CHECK_RC(LwRmPtr()->Control(LwRmPtr()->GetClientHandle(),
                                LW0000_CTRL_CMD_SYSTEM_GET_CHIPSET_INFO,
                                &chipsetParams,
                                sizeof(chipsetParams)));

    UINT32 vendorId = static_cast<UINT32>(chipsetParams.vendorId);
    UINT32 deviceId = static_cast<UINT32>(chipsetParams.deviceId);

    if (vendorId == LW0000_SYSTEM_CHIPSET_ILWALID_ID)
        vendorId = static_cast<UINT32>(chipsetParams.HBvendorId);
    if (deviceId == LW0000_SYSTEM_CHIPSET_ILWALID_ID)
        deviceId = static_cast<UINT32>(chipsetParams.HBdeviceId);

    C_CHECK_RC(pJs->SetElement(pReturlwals, 0, vendorId ));
    C_CHECK_RC(pJs->SetElement(pReturlwals, 1, deviceId ));
    C_CHECK_RC(pJs->SetElement(pReturlwals, 2,
           string(reinterpret_cast<char *>(chipsetParams.vendorNameString)) ));
    C_CHECK_RC(pJs->SetElement(pReturlwals, 3,
           string(reinterpret_cast<char *>(chipsetParams.chipsetNameString)) ));

    RETURN_RC(OK);
}

// STest
C_(Global_PutChars2)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    bool syntaxError = false;

    // Check the argument.
    INT32  x               = 0;
    INT32  y               = 0;
    string chars;
    INT32  scale           = 0;
    UINT32 foregroundColor = 0;
    UINT32 backgroundColor = 0;
    UINT32 bUnderline      = 0;
    JSObject* jsSurf;
    if (
         (NumArguments < 3)
         || (OK != pJavaScript->FromJsval(pArguments[0], &x))
         || (OK != pJavaScript->FromJsval(pArguments[1], &y))
         || (OK != pJavaScript->FromJsval(pArguments[2], &chars))
         || (OK != pJavaScript->FromJsval(pArguments[3], &scale))
         || (OK != pJavaScript->FromJsval(pArguments[4], &foregroundColor))
         || (OK != pJavaScript->FromJsval(pArguments[5], &backgroundColor))
         || (OK != pJavaScript->FromJsval(pArguments[6], &bUnderline))
         || (OK != pJavaScript->FromJsval(pArguments[7], &jsSurf))
       )
    {
        syntaxError = true;
    }

    Surface2D* pSurf = 0;
    if (!syntaxError)
    {
        pSurf = JS_GET_PRIVATE(Surface2D, pContext, jsSurf, "Surface2D");
        if (!pSurf)
        {
            JS_ReportError(pContext, "Invalid surface\n");
            return JS_FALSE;
        }
    }

    if (syntaxError)
    {
        JS_ReportError(pContext, "Usage: PutChars2(x, y, chars, scale, "
                "foreground color, background color, display, "
                "bUnderline, Surface2D)");
        return JS_FALSE;
    }

    RETURN_RC(GpuUtility::PutChars(x, y, chars, scale, foregroundColor,
                                   backgroundColor, bUnderline != 0, pSurf));
}

// STest
C_(Global_PutChars)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    Printf(Tee::PriWarn, "PutChars is deprecated in favor of PutChars2\n");

    JavaScriptPtr pJavaScript;
    bool syntaxError = false;

    // Check the argument.
    INT32  x;
    INT32  y;
    string chars;
    INT32  scale           = 1;
    UINT32 foregroundColor = 0xFFFFFFFF;
    UINT32 backgroundColor = 0;
    UINT32 display         = GpuUtility::DISPLAY_DEFAULT_PUTCHARS;
    JSObject * pJsObj;
    GpuSubdevice * pGpuSubdev = nullptr;
    if ((NumArguments < 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &x)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &y)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &chars)))
    {
        syntaxError = true;
    }
    if (!syntaxError && (NumArguments > 3))
    {
        if (OK != pJavaScript->FromJsval(pArguments[3], &scale))
            syntaxError = true;
    }
    if (!syntaxError && (NumArguments > 4))
    {
        if (OK != pJavaScript->FromJsval(pArguments[4], &foregroundColor))
            syntaxError = true;
    }
    if (!syntaxError && (NumArguments > 5))
    {
        if (OK != pJavaScript->FromJsval(pArguments[5], &backgroundColor))
            syntaxError = true;
    }
    if (!syntaxError && (NumArguments > 6))
    {
        if (OK != pJavaScript->FromJsval(pArguments[6], &display))
            syntaxError = true;
    }
    if (!syntaxError && (NumArguments > 7))
    {
        if (OK != pJavaScript->FromJsval(pArguments[7], &pJsObj))
        {
            syntaxError = true;
        }
        else
        {
            JsGpuSubdevice *pJsGpuSubdevice = JS_GET_PRIVATE(
                    JsGpuSubdevice, pContext, pJsObj, "GpuSubdevice");
            if (pJsGpuSubdevice != nullptr)
            {
                pGpuSubdev = pJsGpuSubdevice->GetGpuSubdevice();
            }
            else
            {
                JS_ReportError(pContext, "Invalid JsGpuDevice\n");
                return JS_FALSE;
            }
        }
    }

    if (syntaxError)
    {
        JS_ReportError(pContext,
                       "Usage: PutChars(x, y, chars, scale = 1,"
                       " foreground color = 0xFFFFFFFF, background color = 0,"
                       " display = default, jsGpuDevice = null)");
        return JS_FALSE;
    }

    RETURN_RC(GpuUtility::PutChars(x, y, chars, scale, foregroundColor,
                                   backgroundColor, display, pGpuSubdev));
}

// STest
C_(Global_PutCharsHorizCentered2)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    bool syntaxError = false;

    // Check the argument.
    INT32  screenWidth     = 0;
    INT32  y               = 0;
    string chars;
    INT32  scale           = 0;
    UINT32 foregroundColor = 0;
    UINT32 backgroundColor = 0;
    UINT32 bUnderline      = 0;
    JSObject* jsSurf;
    if (
         (NumArguments != 8)
         || (OK != pJavaScript->FromJsval(pArguments[0], &screenWidth))
         || (OK != pJavaScript->FromJsval(pArguments[1], &y))
         || (OK != pJavaScript->FromJsval(pArguments[2], &chars))
         || (OK != pJavaScript->FromJsval(pArguments[3], &scale))
         || (OK != pJavaScript->FromJsval(pArguments[4], &foregroundColor))
         || (OK != pJavaScript->FromJsval(pArguments[5], &backgroundColor))
         || (OK != pJavaScript->FromJsval(pArguments[6], &bUnderline))
         || (OK != pJavaScript->FromJsval(pArguments[7], &jsSurf))
       )
    {
        syntaxError = true;
    }

    Surface2D* pSurf = 0;
    if (!syntaxError)
    {
        pSurf = JS_GET_PRIVATE(Surface2D, pContext, jsSurf, "Surface2D");
        if (!pSurf)
        {
            JS_ReportError(pContext, "Invalid surface\n");
            return JS_FALSE;
        }
    }

    if (syntaxError)
    {
        JS_ReportError(
            pContext,
            "Usage: PutCharsHorizCentered2(screen width, y, chars, scale = 1, "
            "foreground color, background color, bUnderline, Surface2D)");
        return JS_FALSE;
    }

    RETURN_RC(GpuUtility::PutCharsHorizCentered(screenWidth, y, chars, scale,
                foregroundColor, backgroundColor, bUnderline != 0, pSurf));
}

// STest
C_(Global_PutCharsHorizCentered)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    Printf(Tee::PriWarn,
           "PutCharsHorizCentered is deprecated in favor of "
           "PutCharsHorizCentered2\n");

    JavaScriptPtr pJavaScript;
    bool syntaxError = false;

    // Check the argument.
    INT32  screenWidth;
    INT32  y;
    string chars;
    INT32  scale           = 1;
    UINT32 foregroundColor = 0xFFFFFFFF;
    UINT32 backgroundColor = 0;
    UINT32 display         = GpuUtility::DISPLAY_DEFAULT_PUTCHARS;
    JSObject * pJsObj;
    GpuSubdevice * pGpuSubdev = nullptr;
    if ((NumArguments < 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &screenWidth)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &y)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &chars)))
    {
        syntaxError = true;
    }
    if (!syntaxError && (NumArguments > 3))
    {
        if (OK != pJavaScript->FromJsval(pArguments[3], &scale))
            syntaxError = true;
    }
    if (!syntaxError && (NumArguments > 4))
    {
        if (OK != pJavaScript->FromJsval(pArguments[4], &foregroundColor))
            syntaxError = true;
    }
    if (!syntaxError && (NumArguments > 5))
    {
        if (OK != pJavaScript->FromJsval(pArguments[5], &backgroundColor))
            syntaxError = true;
    }
    if (!syntaxError && (NumArguments > 6))
    {
        if (OK != pJavaScript->FromJsval(pArguments[6], &display))
            syntaxError = true;
    }
    if (!syntaxError && (NumArguments > 7))
    {
        if (OK != pJavaScript->FromJsval(pArguments[7], &pJsObj))
        {
            syntaxError = true;
        }
        else
        {
            JsGpuSubdevice *pJsGpuSubdevice = JS_GET_PRIVATE(
                    JsGpuSubdevice, pContext, pJsObj, "GpuSubdevice");
            if (pJsGpuSubdevice != nullptr)
            {
                pGpuSubdev = pJsGpuSubdevice->GetGpuSubdevice();
            }
            else
            {
                JS_ReportError(pContext, "Invalid JsGpuDevice\n");
                return JS_FALSE;
            }
        }
    }
    if (syntaxError)
    {
        JS_ReportError(pContext,
            "Usage: PutCharsHorizCentered(screen width, y, chars, scale = 1,"
            " foreground color = 0xFFFFFFFF, background color = 0,"
            " display = default, JsGpuDevice = null)");
        return JS_FALSE;
    }

    RETURN_RC(GpuUtility::PutCharsHorizCentered(
                    screenWidth, y, chars, scale, foregroundColor,
                    backgroundColor, display, pGpuSubdev));
}

INT32  volatile GpuUtility::RmMsgFilter::s_ChangeCount = 0;
string GpuUtility::RmMsgFilter::s_OrigRmMsg = "";
string GpuUtility::RmMsgFilter::s_RmMsgString = "";
LwRm * GpuUtility::RmMsgFilter::s_pLwRm = nullptr;

GpuUtility::RmMsgFilter::~RmMsgFilter()
{
    if (OK != Restore())
    {
        Printf(Tee::PriWarn,
            "DisableRmTrace : Restoring original RM message filter failed!\n");
    }
}

RC GpuUtility::RmMsgFilter::Set(LwRm *pLwRm, string rmMsgString)
{
    if ((s_RmMsgString != "") && (rmMsgString != s_RmMsgString))
    {
        Printf(Tee::PriError,
               "RmMsgFilter::Set filter already set : \n"
               "\tLwrrent filter   : %s\n"
               "\tRequested filter : %s\n",
               s_RmMsgString.c_str(), rmMsgString.c_str());
        return RC::SOFTWARE_ERROR;
    }

    const INT32 prevRefCount = Cpu::AtomicAdd(&s_ChangeCount, 1);
    if (prevRefCount != 0)
        return OK;

    s_pLwRm = pLwRm;
    s_RmMsgString = rmMsgString;

    RC rc;
    LW0000_CTRL_SYSTEM_DEBUG_RMMSG_CTRL_PARAMS rmmsgParams = { 0 };
    string rmMsgFilter;

    rmmsgParams.cmd = LW0000_CTRL_SYSTEM_DEBUG_RMMSG_CTRL_CMD_GET;
    CHECK_RC(s_pLwRm->Control(s_pLwRm->GetClientHandle(),
                              LW0000_CTRL_CMD_SYSTEM_DEBUG_RMMSG_CTRL,
                              &rmmsgParams,
                              sizeof(rmmsgParams)));
    s_OrigRmMsg.assign((const char *)rmmsgParams.data,
                       (size_t)rmmsgParams.count);
    rmMsgFilter = s_OrigRmMsg;
    if (rmMsgFilter != "")
        rmMsgFilter += ",";
    rmMsgFilter += s_RmMsgString;
    rmmsgParams.cmd = LW0000_CTRL_SYSTEM_DEBUG_RMMSG_CTRL_CMD_SET;
    rmmsgParams.count = static_cast<LwU32>(rmMsgFilter.copy((char *)rmmsgParams.data,
                                         LW0000_CTRL_SYSTEM_DEBUG_RMMSG_SIZE));
    CHECK_RC(s_pLwRm->Control(s_pLwRm->GetClientHandle(),
                              LW0000_CTRL_CMD_SYSTEM_DEBUG_RMMSG_CTRL,
                              &rmmsgParams,
                              sizeof(rmmsgParams)));
    return rc;
}
RC GpuUtility::RmMsgFilter::Restore()
{
    if (s_ChangeCount == 0 && s_RmMsgString == "")
    {
        return OK;
    }

    const INT32 prevRefCount = Cpu::AtomicAdd(&s_ChangeCount, -1);
    MASSERT(prevRefCount > 0);
    if (prevRefCount == 1)
    {
        RC rc;
        LW0000_CTRL_SYSTEM_DEBUG_RMMSG_CTRL_PARAMS rmmsgParams = { 0 };
        rmmsgParams.cmd = LW0000_CTRL_SYSTEM_DEBUG_RMMSG_CTRL_CMD_SET;
        rmmsgParams.count = static_cast<LwU32>(s_OrigRmMsg.copy((char *)rmmsgParams.data,
                                          LW0000_CTRL_SYSTEM_DEBUG_RMMSG_SIZE));
        CHECK_RC(s_pLwRm->Control(s_pLwRm->GetClientHandle(),
                                  LW0000_CTRL_CMD_SYSTEM_DEBUG_RMMSG_CTRL,
                                  &rmmsgParams,
                                  sizeof(rmmsgParams)));
        s_OrigRmMsg = "";
        s_RmMsgString = "";
        s_pLwRm = nullptr;
    }

    return OK;
}

void GpuUtility::ValueStats::AddPoint(UINT32 value)
{
    m_Oclwrrences[value]++;
}

void GpuUtility::ValueStats::PrintReport(Tee::Priority priority, const char *name)
{
    UINT64 sum = 0;
    UINT64 sumSingleBit = 0;
    UINT64 singleBitOclwrrences[32] = {};

    for (auto oclwrence : m_Oclwrrences)
    {
        sum += oclwrence.second;

        for (INT32 bit = Utility::BitScanForward(oclwrence.first); bit >= 0;
             bit = Utility::BitScanForward(oclwrence.first, bit + 1))
        {
            static_assert( (sizeof(oclwrence.first) * 8) == NUMELEMS(singleBitOclwrrences),
                "Incorrect size of singleBitOclwrrences array");
            singleBitOclwrrences[bit] += oclwrence.second;
            sumSingleBit += oclwrence.second;
        }
    }

    Printf(priority, "%s value oclwrrence:\n", name);
    for (auto oclwrence : m_Oclwrrences)
    {
        Printf(priority, "    0x%08x: %10u, %3.1f%%\n",
            oclwrence.first, oclwrence.second, 100.0*oclwrence.second/sum);
    }

    Printf(priority, "%s single bit oclwrrence:\n", name);
    for (UINT32 bitIdx = 0; bitIdx < NUMELEMS(singleBitOclwrrences); bitIdx++)
    {
        if (singleBitOclwrrences[bitIdx] == 0)
            continue;
        Printf(priority, "    0x%08x: %10llu, %3.1f%%\n",
            1 << bitIdx, singleBitOclwrrences[bitIdx],
            100.0*singleBitOclwrrences[bitIdx]/sumSingleBit);
    }
}

void GpuUtility::ValueStats::Reset()
{
    m_Oclwrrences.clear();
}

PROP_CONST( GpuUtils, FONT_WIDTH, FONT_WIDTH );
