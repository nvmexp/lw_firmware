/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/jscript.h"
#include "gpu/include/gralloc.h"
#include "gpumtest.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/tee.h"
#include "gputestc.h"
#include "gpu/utility/gpuutils.h"
#include "core/include/lwrm.h"
#include "class/cl502d.h" // LW50_TWO
#include "class/cla06fsubch.h"
#include "core/include/channel.h"
#include "core/include/platform.h"

using namespace GpuUtility;

class MemScrubber : public GpuMemTest
{
public:
    MemScrubber();
    ~MemScrubber();

    bool IsSupported() override;
    RC Setup() override;
    RC Run() override;
    RC Cleanup() override;

private:
    enum
    {
        s_TwoD = LWA06F_SUBCHANNEL_2D, // = 3
    };

    struct Chan
    {
        LwRm::Handle    hCh;
        Channel *       pCh;
        Surface2D       sysmemSemaphore;
        GrClassAllocator *  pGrAlloc;
        UINT32          flushes;  //!< Num calls to WriteSemaphoreAndFlush.
        UINT32          waits;    //!< Num calls to WaitSemaphore.

        Chan()
        :    hCh(0)
            ,pCh(nullptr)
            ,pGrAlloc(nullptr)
            ,flushes(0)
            ,waits(0)
        {
        }
    };

    GpuTestConfiguration * m_pTstCfg;
    TwodAlloc   m_TwodAlloc;
    Chan        m_TwodChan;
    UINT32      m_ScrubValue;

    RC AllocTwodChan(int chanId);
    RC InstantiateTwodChan();
    RC FillTwod(MemoryChunkDesc chunk, UINT32 value32);

    RC WriteSemaphoreAndFlush();
    RC WaitSemaphore();

public:
    SETGET_PROP(ScrubValue, UINT32);
};

// JS accesible properties
JS_CLASS_INHERIT(MemScrubber, GpuMemTest, "Memory Scrubber");

CLASS_PROP_READWRITE(MemScrubber, ScrubValue, UINT32,
                     "The value FB will be filled with.");

// Implemenation

struct PollArgs
{
    void*  pSemaphoreMem;
    UINT32 expectedValue;
};

static bool PollSemaphoreWritten (void * pvPollArgs)
{
    PollArgs * pPollArgs = static_cast<PollArgs*>(pvPollArgs);

    return pPollArgs->expectedValue == MEM_RD32(pPollArgs->pSemaphoreMem);
}

MemScrubber::MemScrubber()
 : m_ScrubValue(0)
{
    SetName("MemScrubber");
    m_pTstCfg = GetTestConfiguration();
}

MemScrubber::~MemScrubber() {}

/* virtual */ bool MemScrubber::IsSupported()
{
    return ( GpuMemTest::IsSupported() &&
             m_TwodAlloc.IsSupported(GetBoundGpuDevice()) );
}

/* virtual */ RC MemScrubber::Setup()
{
    RC rc;

    CHECK_RC(GpuMemTest::Setup());
    CHECK_RC(AllocTwodChan(0));
    CHECK_RC(InstantiateTwodChan());

    CHECK_RC(AllocateEntireFramebuffer(
                            true, /* BlockLinear */
                            m_TwodChan.hCh));

    return rc;
}

/* virtual */ RC MemScrubber::Run()
{
    RC rc;

    MemoryChunks::iterator iChunk;
    for (iChunk= GetChunks()->begin(); iChunk != GetChunks()->end(); ++iChunk)
    {
        INT64 segmentSize = 32_MB;
        for (INT64 size = iChunk->size; size > 0; size -= segmentSize)
        {
            MemoryChunkDesc chunk = *iChunk;
            if (size > segmentSize)
            {
                chunk.size = segmentSize;
            }
            else
            {
                chunk.size = size;
            }
            chunk.ctxDmaOffsetGpu += iChunk->size - size;

            CHECK_RC(FillTwod(chunk, m_ScrubValue));
        }
    }

    return OK;
}

RC MemScrubber::FillTwod
(
    MemoryChunkDesc chunk,
    UINT32          value32
)
{
    RC rc;

    UINT32 subChan = s_TwoD;
    UINT64 off = chunk.ctxDmaOffsetGpu;
    UINT32 width = 4096 / sizeof(UINT32);       // in pixels
    UINT32 height = UINT32(chunk.size / 4096);  // in lines

    Channel * pCh = m_TwodChan.pCh;
    MASSERT(pCh);

    // Send one "pixel from CPU" that will be magnified to cover the entire
    // dest half-surface.

    switch (m_TwodAlloc.GetClass())
    {
        case LW50_TWOD:
            CHECK_RC(pCh->Write(subChan, LW502D_SET_DST_CONTEXT_DMA,
                GetBoundGpuDevice()->GetMemSpaceCtxDma(Memory::Fb, false)));
            break;
        default:
            ;
    }
    CHECK_RC(pCh->Write(subChan, LW502D_SET_DST_OFFSET_UPPER,
                        LwU64_HI32(off)));
    CHECK_RC(pCh->Write(subChan, LW502D_SET_DST_OFFSET_LOWER,
                        LwU64_LO32(off)));
    CHECK_RC(pCh->Write(subChan, LW502D_SET_DST_FORMAT,
                        LW502D_SET_DST_FORMAT_V_A8R8G8B8));
    CHECK_RC(pCh->Write(subChan, LW502D_SET_DST_WIDTH,  width));
    CHECK_RC(pCh->Write(subChan, LW502D_SET_DST_HEIGHT, height));

    CHECK_RC(pCh->Write(subChan, LW502D_SET_DST_MEMORY_LAYOUT,
                        LW502D_SET_DST_MEMORY_LAYOUT_V_BLOCKLINEAR));
    CHECK_RC(pCh->Write(subChan, LW502D_SET_DST_BLOCK_SIZE,
            DRF_DEF(502D, _SET_DST_BLOCK_SIZE, _WIDTH, _ONE_GOB) |
            DRF_DEF(502D, _SET_DST_BLOCK_SIZE, _HEIGHT, _TWO_GOBS) |
            DRF_DEF(502D, _SET_DST_BLOCK_SIZE, _DEPTH, _ONE_GOB)));
    CHECK_RC(pCh->Write(subChan, LW502D_SET_DST_DEPTH, 1));
    CHECK_RC(pCh->Write(subChan, LW502D_SET_DST_LAYER, 0));

    CHECK_RC(pCh->Write(subChan, LW502D_SET_OPERATION,
                        LW502D_SET_OPERATION_V_SRCCOPY));
    CHECK_RC(pCh->Write(subChan, LW502D_SET_PIXELS_FROM_CPU_DATA_TYPE, 2,
                         LW502D_SET_PIXELS_FROM_CPU_DATA_TYPE_V_COLOR,
                         LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_A8R8G8B8));
    CHECK_RC(pCh->Write(subChan, LW502D_SET_PIXELS_FROM_CPU_SRC_WIDTH, 10,
        /* LW502D_SET_PIXELS_FROM_CPU_SRC_WIDTH */      1,
        /* LW502D_SET_PIXELS_FROM_CPU_SRC_HEIGHT */     1,
        /* LW502D_SET_PIXELS_FROM_CPU_DX_DU_FRAC */     0,
        /* LW502D_SET_PIXELS_FROM_CPU_DX_DU_INT */      width,
        /* LW502D_SET_PIXELS_FROM_CPU_DY_DV_FRAC */     0,
        /* LW502D_SET_PIXELS_FROM_CPU_DY_DV_INT */      height,
        /* LW502D_SET_PIXELS_FROM_CPU_DST_X0_FRAC */    0,
        /* LW502D_SET_PIXELS_FROM_CPU_DST_X0_INT */     0,
        /* LW502D_SET_PIXELS_FROM_CPU_DST_Y0_FRAC */    0,
        /* LW502D_SET_PIXELS_FROM_CPU_DST_Y0_INT */     0));

    CHECK_RC(pCh->Write(subChan, LW502D_PIXELS_FROM_CPU_DATA,     value32));
    CHECK_RC(WriteSemaphoreAndFlush());
    CHECK_RC(WaitSemaphore());

    return rc;
}

/* virtual */ RC MemScrubber::Cleanup()
{
    RC rc;

    // Deallocate twod channel
    if (m_TwodChan.hCh)
    {
        MASSERT(m_TwodChan.pGrAlloc);
        MASSERT(m_TwodChan.pCh);

        m_TwodChan.pGrAlloc->Free();

        m_pTstCfg->FreeChannel(m_TwodChan.pCh);

        m_TwodChan.sysmemSemaphore.Free();

        m_TwodChan.hCh = 0;
        m_TwodChan.pCh = nullptr;
    }

    // Free's framebuffer
    CHECK_RC(GpuMemTest::Cleanup());

    return OK;
}

RC MemScrubber::AllocTwodChan(int cid)
{
    RC rc;
    Chan & chan = m_TwodChan;
    chan.flushes = 0;
    chan.waits = 0;

    chan.pGrAlloc = &m_TwodAlloc;
    CHECK_RC(m_pTstCfg->AllocateChannelWithEngine(&chan.pCh,
                                                  &chan.hCh,
                                                  chan.pGrAlloc));

    // We want to control when flushing oclwrs, but we also would prefer not
    // to crash the test if we fill the pushbuffer without flushing.
    // Enable autoflush, but set the auto-flush threshold to the max allowed.

    CHECK_RC(chan.pCh->SetAutoFlush(true, 0xffffffff));
    CHECK_RC(chan.pCh->SetAutoGpEntry(true, 0xffffffff));

    // Alloc a small sysmem surface to use for semaphore writes.

    LwRm * pLwRm = chan.pCh->GetRmClient();
    const int x86PageSize = 4096;

    chan.sysmemSemaphore.SetLocation (m_pTstCfg->NotifierLocation());
    chan.sysmemSemaphore.SetLayout (Surface2D::Pitch);
    chan.sysmemSemaphore.SetPitch (x86PageSize);
    chan.sysmemSemaphore.SetWidth (x86PageSize);
    chan.sysmemSemaphore.SetHeight (1);
    chan.sysmemSemaphore.SetDepth (1);
    chan.sysmemSemaphore.SetKernelMapping (true);
    chan.sysmemSemaphore.SetColorFormat(ColorUtils::Y8);
    CHECK_RC(chan.sysmemSemaphore.Alloc (GetBoundGpuDevice(), pLwRm));
    CHECK_RC(chan.sysmemSemaphore.Map(0));
    CHECK_RC(chan.sysmemSemaphore.BindGpuChannel(chan.hCh, pLwRm));

    return rc;
}

RC MemScrubber::InstantiateTwodChan()
{
    RC rc;
    if (!m_TwodChan.hCh)
    {
        Printf(Tee::PriHigh,
               "InstantiateTwodChan called for unallocated channel.\n");
        return RC::SOFTWARE_ERROR;
    }
    Channel *   pCh  = m_TwodChan.pCh;
    Surface2D & surf = m_TwodChan.sysmemSemaphore;
    LwRm *      pLwRm = pCh->GetRmClient();
    CHECK_RC(pCh->SetObject (s_TwoD, m_TwodChan.pGrAlloc->GetHandle()));

    CHECK_RC(pCh->SetContextDmaSemaphore(
            surf.GetCtxDmaHandleGpu(pLwRm)));
    CHECK_RC(pCh->SetSemaphoreOffset(
            surf.GetCtxDmaOffsetGpu(pLwRm)));

    return OK;
}

RC MemScrubber::WriteSemaphoreAndFlush()
{
    RC rc;

    if (m_TwodChan.flushes != m_TwodChan.waits)
    {
        // This channel is already Flushed since the last Wait.
        return OK;
    }

    MEM_WR32(m_TwodChan.sysmemSemaphore.GetAddress(), 0xffffffff);
    CHECK_RC(m_TwodChan.pCh->SemaphoreRelease(++m_TwodChan.flushes));
    CHECK_RC(m_TwodChan.pCh->Flush());

    return rc;
}

RC MemScrubber::WaitSemaphore()
{
    RC rc;
    Chan & chan = m_TwodChan;
    if (chan.flushes != chan.waits)
    {
        const FLOAT64 timeoutMs = m_pTstCfg->TimeoutMs();
        PollArgs pa;
        pa.pSemaphoreMem = chan.sysmemSemaphore.GetAddress();
        pa.expectedValue = chan.flushes;

        if (chan.sysmemSemaphore.GetLocation() == Memory::Fb)
        {
            rc = POLLWRAP_HW( &PollSemaphoreWritten, &pa, timeoutMs );
        }
        else
        {
            rc = POLLWRAP( &PollSemaphoreWritten, &pa, timeoutMs );
        }
        if (OK != rc)
            return rc;

        chan.waits = chan.flushes;
    }

    return rc;
}
