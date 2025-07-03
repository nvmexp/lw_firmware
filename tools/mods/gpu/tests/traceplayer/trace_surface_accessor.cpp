/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "trace_surface_accessor.h"
#include "core/include/channel.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "gpu/tests/gputestc.h"
#include "gpu/include/gpudev.h"
#include "gpu/utility/surf2d.h"
#include "gpu/utility/surfrdwr.h"
#include "lwos.h"
#include "lwmisc.h"

#include "class/cla06fsubch.h"
#include "class/clc3b5.h"

#define UINT64_UPPER(n) (UINT32((n) >> 32))
#define UINT64_LOWER(n) (UINT32(n))

// ----------------------------------------------------------------------------
MfgTracePlayer::SurfaceAccessor::~SurfaceAccessor()
{
    Free();
}

// ----------------------------------------------------------------------------
void MfgTracePlayer::SurfaceAccessor::Setup
(
    GpuTestConfiguration * pTestConfig,
    LwRm *                 pLwRm,
    GpuDevice *            pGpuDev,
    LwRm::Handle           hVASpace,
    UINT32                 sizeMB
)
{
    m_SysmemSurface.SetLocation(Memory::NonCoherent);
    m_SysmemSurface.SetArrayPitch(sizeMB * 1_MB);
    m_SysmemSurface.SetGpuVASpace(hVASpace);
    m_SysmemSurface.SetVASpace(Surface2D::GPUVASpace);
    m_SysmemSurface.SetColorFormat(ColorUtils::Y8);
    m_SysmemSurface.SetGpuCacheMode(Surface2D::GpuCacheOff);
    m_SysmemSurface.SetMemoryMappingMode(Surface2D::MapDirect);
    m_pTestConfig = pTestConfig;
    m_pGpuDevice  = pGpuDev;
    m_pLwRm       = pLwRm;
    m_hVASpace    = hVASpace;
}

// ----------------------------------------------------------------------------
RC MfgTracePlayer::SurfaceAccessor::Free()
{
    if (!m_Mutex.get())
        return RC::OK;

    StickyRC rc;
    {
        Tasker::MutexHolder mh(m_Mutex);

        if (m_Semaphore.IsAllocated())
            m_Semaphore.Free();

        if (m_SysmemSurface.IsAllocated())
            m_SysmemSurface.Free();

        if (m_pCh)
        {
            m_DmaAlloc.Free();
            rc = m_pTestConfig->FreeChannel(m_pCh);
            m_pCh = nullptr;
        }

        m_bAllocated = false;
    }
    m_Mutex = Tasker::NoMutex();
    return rc;
}

// ----------------------------------------------------------------------------
RC MfgTracePlayer::SurfaceAccessor::ReadBytes
(
    Surface2D * pSurf,
    UINT32      subdev,
    UINT64      offset,
    void      * buf,
    UINT64      sizeBytes
)
{
    // If a direct mapping is possible, no need to do a copy, just access the surface
    // directly
    if (IsDirectMappingPossible(pSurf))
    {
        SurfaceUtils::MappingSurfaceReader reader(*pSurf);
        reader.SetTargetSubdevice(subdev);
        return reader.ReadBytes(offset, buf, sizeBytes);
    }

    return AccessSurface(pSurf, offset, buf, sizeBytes, AccessMode::Read);
}

// ----------------------------------------------------------------------------
RC MfgTracePlayer::SurfaceAccessor::WriteBytes
(
    Surface2D  * pSurf,
    UINT64       offset,
    const void * buf,
    UINT64       sizeBytes
)
{
    // If a direct mapping is possible, no need to do a copy, just access the surface
    // directly
    if (IsDirectMappingPossible(pSurf))
    {
        SurfaceUtils::MappingSurfaceWriter writer(*pSurf);
        return writer.WriteBytes(offset, buf, sizeBytes);
    }

    return AccessSurface(pSurf, offset, const_cast<void *>(buf), sizeBytes, AccessMode::Write);
}

// ----------------------------------------------------------------------------
bool MfgTracePlayer::SurfaceAccessor::IsDirectMappingPossible(Surface2D *pSurf)
{
    if (m_pGpuDevice->GetSubdevice(0)->IsSOC())
        return true;

    if (pSurf->GetLocation() == Memory::Fb)
        return false;

    // If the surface is in system memory, only pitch linear, non-GPU cached surfaces
    // may be directly mapped.  Note : the surfaces used in the trace test do not
    // setup Surface2D members, but instead directly set the attributes do not
    // use the Surface2D utility functions which rely on normal surface setup
    if (!FLD_TEST_DRF(OS32, _ATTR, _FORMAT, _PITCH, pSurf->GetVidHeapAttr()))
        return false;

    if (!FLD_TEST_DRF(OS32, _ATTR2, _GPU_CACHEABLE, _NO, pSurf->GetVidHeapAttr2()))
        return false;

    return true;
}

// ----------------------------------------------------------------------------
RC MfgTracePlayer::SurfaceAccessor::Allocate()
{
    RC rc;

    if (m_bAllocated)
        return OK;

    m_Mutex = Tasker::CreateMutex("TraceSurfaceAccessorMutex",   Tasker::mtxUnchecked);

    m_pTestConfig->SetAllowMultipleChannels(true);
    m_pTestConfig->SetChannelType(TestConfiguration::GpFifoChannel);
    UINT32 hCh = ~0U;
    CHECK_RC(m_pTestConfig->AllocateChannelGr(&m_pCh,
                                              &hCh,
                                              nullptr,
                                              m_hVASpace,
                                              &m_DmaAlloc,
                                              0,
                                              LW2080_ENGINE_TYPE_ALLENGINES));
    CHECK_RC(m_pCh->SetObject(LWA06F_SUBCHANNEL_COPY_ENGINE, m_DmaAlloc.GetHandle()));

    CHECK_RC(m_SysmemSurface.Alloc(m_pGpuDevice, m_pLwRm));

    if (!m_SysmemSurface.IsAllocated())
    {
        Printf(Tee::PriError, "Unable to allocate SurfaceAccessor memory, no free GPU VAs\n");
        return RC::RESOURCE_IN_USE;
    }

    Printf(Tee::PriLow,
           "Allocated SurfaceAccessor at GPU VA 0x%llx\n",
           m_SysmemSurface.GetCtxDmaOffsetGpu());
    CHECK_RC(m_SysmemSurface.Map());

    m_Semaphore.Setup(NotifySemaphore::FOUR_WORDS,
                      Memory::NonCoherent,
                      1);
    CHECK_RC(m_Semaphore.Allocate(m_pCh, 0));

    m_bAllocated = true;
    return rc;
}

// ----------------------------------------------------------------------------
RC MfgTracePlayer::SurfaceAccessor::AccessSurface
(
    Surface2D  * pSurf,
    UINT64       offset,
    void       * buf,
    UINT64       sizeBytes,
    AccessMode   mode
)
{
    MASSERT((mode == AccessMode::Read) || (mode == AccessMode::Write));

    RC rc;
    CHECK_RC(Allocate());

    Tasker::MutexHolder mh(m_Mutex);
    if (!m_bAllocated)
    {
        Printf(Tee::PriError, "Attempt to use SurfaceAccessor after Free\n");
        MASSERT(!"Attempt to use SurfaceAccessor after Free");
        return RC::SOFTWARE_ERROR;
    }

    UINT64 totalAccessed = 0;
    while (totalAccessed != sizeBytes)
    {
        // Determine the width and height to read/write using the copy engine starting at
        // the GOB size to ensure compatibility with block linear surfaces
        const UINT64 bytesToAccess = min(m_SysmemSurface.GetSize(), sizeBytes - totalAccessed);
        const INT32 lowBit = Utility::BitScanForward(bytesToAccess);
        const UINT32 lineWidth = min(lowBit >= 0 ? 1U << lowBit : 0U,
                                     m_pGpuDevice->GobWidth());
        const UINT32 linesToAccess =
            static_cast<UINT32>(min(bytesToAccess / lineWidth, static_cast<UINT64>(_UINT32_MAX)));

        if (mode == AccessMode::Write)
        {
            Platform::MemCopy(m_SysmemSurface.GetAddress(), buf, lineWidth * linesToAccess);
            CHECK_RC(Copy(&m_SysmemSurface,
                          0,
                          pSurf,
                          offset + totalAccessed,
                          linesToAccess,
                          lineWidth));
        }
        else if (mode == AccessMode::Read)
        {
            CHECK_RC(Copy(pSurf,
                          offset + totalAccessed,
                          &m_SysmemSurface,
                          0,
                          linesToAccess,
                          lineWidth));
            Platform::MemCopy(buf, m_SysmemSurface.GetAddress(), lineWidth * linesToAccess);
        }
        buf = static_cast<UINT08 *>(buf) + lineWidth * linesToAccess;
        totalAccessed += lineWidth * linesToAccess;
    }

    return rc;
}

// ----------------------------------------------------------------------------
RC MfgTracePlayer::SurfaceAccessor::Copy
(
    Surface2D *pSrcSurf,
    UINT64     srcOffset,
    Surface2D *pDstSurf,
    UINT64     dstOffset,
    UINT32     linesToWrite,
    UINT32     lineWidth
)
{
    RC rc;

    pSrcSurf->BindGpuChannel(m_pCh->GetHandle());
    pDstSurf->BindGpuChannel(m_pCh->GetHandle());

    m_Semaphore.SetPayload(0);

    UINT32 SubChan = LWA06F_SUBCHANNEL_COPY_ENGINE;
    UINT32 layoutMem = 0;

    const UINT64 srcDmaOffset = pSrcSurf->GetCtxDmaOffsetGpu() + srcOffset;
    const UINT64 dstDmaOffset = pDstSurf->GetCtxDmaOffsetGpu() + dstOffset;
    CHECK_RC(m_pCh->Write(SubChan, LWC3B5_OFFSET_IN_UPPER,
                          2,
                          UINT64_UPPER(srcDmaOffset),
                          UINT64_LOWER(srcDmaOffset)));

    CHECK_RC(m_pCh->Write(SubChan, LWC3B5_OFFSET_OUT_UPPER,
                          2,
                          UINT64_UPPER(dstDmaOffset),
                          UINT64_LOWER(dstDmaOffset)));

    // This copy is replacing a CPU BAR1 access therefore it must behave as if
    // the CPU is doing the access, CPU access is always naive and assumes
    // a pitch linear surface, therefore, *do not* set the layout to block linear
    // in either the source or destination surface
    layoutMem |= DRF_DEF(C3B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH);
    CHECK_RC(m_pCh->Write(SubChan, LWC3B5_PITCH_IN,  lineWidth));

    layoutMem |= DRF_DEF(C3B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH);
    CHECK_RC(m_pCh->Write(SubChan, LWC3B5_PITCH_OUT, lineWidth));

    layoutMem |= DRF_DEF(C3B5, _LAUNCH_DMA, _SRC_TYPE, _VIRTUAL);
    layoutMem |= DRF_DEF(C3B5, _LAUNCH_DMA, _DST_TYPE, _VIRTUAL);

    CHECK_RC(m_pCh->Write(SubChan, LWC3B5_LINE_LENGTH_IN, lineWidth));
    CHECK_RC(m_pCh->Write(SubChan, LWC3B5_LINE_COUNT, linesToWrite));

    // Setup the completion semaphore
    CHECK_RC(m_pCh->Write(SubChan,
        LWC3B5_SET_SEMAPHORE_A,
        2,
        UINT64_UPPER(m_Semaphore.GetCtxDmaOffsetGpu(0)),
        UINT64_LOWER(m_Semaphore.GetCtxDmaOffsetGpu(0))));

    CHECK_RC(m_pCh->Write(SubChan, LWC3B5_SET_SEMAPHORE_PAYLOAD, 0x87654321));
    layoutMem |= DRF_DEF(C3B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, _RELEASE_FOUR_WORD_SEMAPHORE);
    layoutMem |= DRF_DEF(C3B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE);

    m_Semaphore.SetOneTriggerPayload(0, 0x87654321);
    // Use NON_PIPELINED to WAR hw bug 618956.
    CHECK_RC(m_pCh->Write(SubChan, LWC3B5_LAUNCH_DMA,
                          layoutMem |
                          DRF_DEF(C3B5, _LAUNCH_DMA, _INTERRUPT_TYPE, _NONE) |
                          DRF_DEF(C3B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE,
                                  _NON_PIPELINED) |
                          DRF_DEF(C3B5,
                                  _LAUNCH_DMA,
                                  _MULTI_LINE_ENABLE,
                                  _TRUE)));

    CHECK_RC(m_pCh->Flush());
    CHECK_RC(m_Semaphore.Wait(m_pTestConfig->TimeoutMs(),
                              &NotifySemaphore::CondGreaterEq));
    return rc;
}
