/* LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_vaspace_interop.cpp
//! \brief VASpace interop tests
//!

#include "lwos.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "core/include/utility.h"

#include "class/cle3f1.h"       // TEGRA_VASPACE_A
#include "class/cl90b5.h"       // FERMI_DMA_COPY
#include "class/clb0b5.h"       // MAXWELL_DMA_COPY
#include "class/clc0b5.h"       // PASCAL_DMA_COPY
#include "class/clc3b5.h"       // VOLTA_DMA_COPY
#include "class/clc5b5.h"       // TURING_DMA_COPY
#include "class/clc6b5.h"       // AMPERE_DMA_COPY_A
#include "class/clc7b5.h"       // AMPERE_DMA_COPY_B
#include "class/clc8b5.h"       // HOPPER_DMA_COPY_A
#include "class/clc9b5.h"       // BLACKWELL_DMA_COPY_A
#include "class/cla06fsubch.h"  // LWA06F_SUBCHANNEL_COPY_ENGINE
#include "class/clb0b5sw.h"     // LWB0B5_ALLOCATION_PARAMETERS
#include "class/cl90f1.h"       // FERMI_VASPACE_A
#include "gpu/include/gralloc.h"

#include "core/include/channel.h"
#include "gpu/include/gpudev.h"
#include "gpu/utility/surf2d.h"
#include "gpu/tests/rmtest.h"
#include "core/include/tasker.h"
#include "gpu/tests/rm/utility/rmtestutils.h"
#include "core/include/memcheck.h"

#define USE_INTR

#define CHAN_64K   0
#define CHAN_128K  1
#define CHAN_COUNT 2

#define MAP_SMALL LWOS46_FLAGS_PAGE_SIZE_4KB
#define MAP_BIG   LWOS46_FLAGS_PAGE_SIZE_BIG
#define MAP_HUGE  LWOS46_FLAGS_PAGE_SIZE_HUGE

#define KB         1024
#define MB         (1024 * KB)
#define MEM_SIZE   (4 * MB)
#define CHUNK_SIZE (8 * KB)

#define PHYSMEM_SYS      0
#define PHYSMEM_FB       1
#define PHYSMEM_COUNT    2

typedef struct {
    LwRm::Handle hVASpace;
    LwRm::Handle hVirtMem;
    LwRm::Handle hChan;
    Channel      *pChan;
    LwRm::Handle hNotifyEvent;
    Surface2D    *pSemSurf, *pPatSurf;
    DmaCopyAlloc *pCopyAlloc;
} CHANNEL_INFO, *PCHANNEL_INFO;

class VASpaceInterOpTest: public RmTest
{
public:
    VASpaceInterOpTest();
    virtual ~VASpaceInterOpTest();

    virtual string     IsTestSupported();

    virtual RC       Setup();
    virtual RC       Run();
    virtual RC       Cleanup();

private:
    RC allocPhysicalBuffer(UINT32, UINT64, LwRm::Handle *);
    RC allocVirtualBuffer(LwRm::Handle, UINT64, LwRm::Handle *);
    RC setupCopyEngine(PCHANNEL_INFO, ModsEvent *);
    RC TestInterOp1(UINT32, UINT32, UINT32, UINT32, LwRm::Handle, UINT32, UINT32, UINT32);
    RC TestInterOp2(UINT32, UINT32, LwRm::Handle, UINT32, UINT32, UINT32);
    RC Fill(UINT32, UINT64, UINT64, UINT32);
    RC Compare(UINT32, UINT64, UINT64, UINT32);
    RC LaunchCopy(Channel *, UINT64, UINT64, UINT64, UINT64, UINT32);
    RC WaitForCopyDone(PCHANNEL_INFO, void *, UINT32);

    UINT32       m_bigPageSizes[CHAN_COUNT];
    CHANNEL_INFO m_chanInfo[CHAN_COUNT];
    LwRm::Handle m_hPhysMem[PHYSMEM_COUNT];
    LwRm        *m_pLwRm;
    GpuDevice   *m_pGpuDev;
    ModsEvent   *m_pModsEvent;
    FLOAT64      m_timeoutMs;
};

//! \brief VASpaceInterOpTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
VASpaceInterOpTest::VASpaceInterOpTest() :
    m_pLwRm(NULL),
    m_pGpuDev(NULL),
    m_pModsEvent(NULL),
    m_timeoutMs(0)
{
    SetName("VASpaceInterOpTest");

    m_bigPageSizes[CHAN_64K]  =  64 * KB;
    m_bigPageSizes[CHAN_128K] = 128 * KB;

    memset(m_chanInfo, 0, sizeof(CHANNEL_INFO) * CHAN_COUNT);
    memset(m_hPhysMem, 0, sizeof(LwRm::Handle) * PHYSMEM_COUNT);
}

//! \brief VASpaceInterOpTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
VASpaceInterOpTest::~VASpaceInterOpTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \return True if KEPLER_CHANNEL_GPFIFO_A is supported on the current chip and
//!         it is also supported in MODS as first class.
//-----------------------------------------------------------------------------
string VASpaceInterOpTest::IsTestSupported()
{
    if (IsClassSupported(MAXWELL_DMA_COPY_A) ||
        IsClassSupported(PASCAL_DMA_COPY_A)  ||
        IsClassSupported(VOLTA_DMA_COPY_A)   ||
        IsClassSupported(TURING_DMA_COPY_A)  ||
        IsClassSupported(AMPERE_DMA_COPY_A)  ||
        IsClassSupported(AMPERE_DMA_COPY_B)  ||
        IsClassSupported(HOPPER_DMA_COPY_A)  ||
        IsClassSupported(BLACKWELL_DMA_COPY_A))
    {
        return RUN_RMTEST_TRUE;
    }
    return "Pre-Maxwell gpus not supported";
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa IsTestSupported
//-----------------------------------------------------------------------------
RC VASpaceInterOpTest::allocPhysicalBuffer(UINT32 loc, UINT64 size, LwRm::Handle *pHPhysMem)
{
    RC           rc;
    UINT32       attr, attr2;
    UINT32       locAttr = 0;

    if(loc == PHYSMEM_SYS)
    {
        locAttr = DRF_DEF(OS32, _ATTR, _LOCATION, _PCI);
    }
    else if(loc == PHYSMEM_FB)
    {
        locAttr = DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM);
    }

    attr = DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH) |
        //DRF_DEF(OS32, _ATTR, _COLOR_PACKING, _A8R8G8B8) |
        //DRF_DEF(OS32, _ATTR, _FORMAT, _BLOCK_LINEAR) |
        DRF_DEF(OS32, _ATTR, _DEPTH, _32) |
        locAttr |
        DRF_DEF(OS32, _ATTR, _COMPR, _ANY) |
        DRF_DEF(OS32, _ATTR, _COHERENCY, _WRITE_COMBINE);

    attr2 = LWOS32_ATTR2_NONE;

    CHECK_RC(m_pLwRm->VidHeapAllocSizeEx( LWOS32_TYPE_IMAGE,
                                        LWOS32_ALLOC_FLAGS_NO_SCANOUT |
                                        LWOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED,
                                        &attr,
                                        &attr2, // pAttr2
                                        size,
                                        1,    // alignment
                                        NULL, // pFormat
                                        NULL, // pCoverage
                                        NULL, // pPartitionStride
                                        pHPhysMem,
                                        NULL, // poffset,
                                        NULL, // pLimit
                                        NULL, // pRtnFree
                                        NULL, // pRtnTotal
                                        0,    // width
                                        0,    // height
                                        GetBoundGpuDevice()));
    return rc;
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa IsTestSupported
//-----------------------------------------------------------------------------
RC VASpaceInterOpTest::allocVirtualBuffer(LwRm::Handle hVASpace, UINT64 size, LwRm::Handle *pHVirtMem)
{
    RC           rc;
    UINT32       attr, attr2;

    attr  = LWOS32_ATTR2_NONE;
    attr2 = LWOS32_ATTR2_NONE;

    CHECK_RC(m_pLwRm->VidHeapAllocSizeEx( LWOS32_TYPE_IMAGE,
                                        LWOS32_ALLOC_FLAGS_VIRTUAL,
                                        &attr,
                                        &attr2, //pAttr2
                                        size, // size
                                        1,    // alignment
                                        NULL, // pFormat
                                        NULL, // pCoverage
                                        NULL, // pPartitionStride
                                        pHVirtMem,
                                        NULL, // poffset(fixed address)
                                        NULL, // pLimit
                                        NULL, // pRtnFree
                                        NULL, // pRtnTotal
                                        0,    // width
                                        0,    // height
                                        GetBoundGpuDevice(),
                                        hVASpace));

    return rc;
}

//-----------------------------------------------------------------------------
RC VASpaceInterOpTest::Setup()
{
    RC                               rc;
    LW_VASPACE_ALLOCATION_PARAMETERS vaParams = { 0 };

    m_pLwRm   = GetBoundRmClient();
    m_pGpuDev = GetBoundGpuDevice();

    // On Pascal+ chips 128K big page size is not supported
    if (m_pGpuDev->SupportsPageSize(GpuDevice::PAGESIZE_HUGE))
    {
        m_bigPageSizes[CHAN_128K] = 64 * KB;
    }

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    m_timeoutMs = m_TestConfig.TimeoutMs();
    //
    // This is needed for the WaitOnEvent call below to prevent false timeouts
    // on FMODEL (default TestConfig timeout is 1 second).
    // Using 1KB/s as a lower bound to account for CPU/sim variation
    //
    if ((Platform::GetSimulationMode() != Platform::Hardware))
    {
        m_timeoutMs += CHUNK_SIZE;
    }

    m_TestConfig.SetAllowMultipleChannels(true);

    if (GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_HAS_FB_HEAP))
    {
        CHECK_RC(allocPhysicalBuffer(PHYSMEM_FB, MEM_SIZE, &m_hPhysMem[PHYSMEM_FB]));
    }

    if (IsClassSupported(TEGRA_VASPACE_A))
    {
        CHECK_RC(allocPhysicalBuffer(PHYSMEM_SYS, MEM_SIZE, &m_hPhysMem[PHYSMEM_SYS]));
    }

#ifdef USE_INTR
    m_pModsEvent = Tasker::AllocEvent();
#endif

    for (UINT32 i = 0; i < CHAN_COUNT; i++)
    {
        vaParams.bigPageSize = m_bigPageSizes[i];
        CHECK_RC(m_pLwRm->Alloc(m_pLwRm->GetDeviceHandle(GetBoundGpuDevice()), &m_chanInfo[i].hVASpace, FERMI_VASPACE_A, &vaParams));

        CHECK_RC(allocVirtualBuffer(m_chanInfo[i].hVASpace, MEM_SIZE, &m_chanInfo[i].hVirtMem));

        m_TestConfig.SetChannelType(TestConfiguration::GpFifoChannel);
        CHECK_RC(m_TestConfig.AllocateChannel(&m_chanInfo[i].pChan, &m_chanInfo[i].hChan, nullptr,
                    nullptr, m_chanInfo[i].hVASpace, 0, LW2080_ENGINE_TYPE_GR(0)));
        //Printf(Tee::PriHigh, "SMMUTest: Gpu channel creation successful ");

        CHECK_RC(setupCopyEngine(&m_chanInfo[i], m_pModsEvent));
    }

    return rc;
}

RC VASpaceInterOpTest::setupCopyEngine(PCHANNEL_INFO pChanInfo, ModsEvent *pModsEvent)
{
    RC rc;

    pChanInfo->pCopyAlloc = new DmaCopyAlloc();

    // Alloc the CE for this channel
    CHECK_RC(pChanInfo->pCopyAlloc->Alloc(pChanInfo->hChan, m_pGpuDev, m_pLwRm));
    CHECK_RC(pChanInfo->pChan->SetObject(LWA06F_SUBCHANNEL_COPY_ENGINE, pChanInfo->pCopyAlloc->GetHandle()));

    if (pModsEvent)
    {
        //Associcate Event to Object
        void* const pOsEvent = Tasker::GetOsEvent(
                pModsEvent,
                m_pLwRm->GetClientHandle(),
                m_pLwRm->GetDeviceHandle(GetBoundGpuDevice()));
        CHECK_RC(m_pLwRm->AllocEvent(pChanInfo->pCopyAlloc->GetHandle(),
                                   &pChanInfo->hNotifyEvent,
                                   LW01_EVENT_OS_EVENT,
                                   0,
                                   pOsEvent));
    }

    pChanInfo->pSemSurf = new Surface2D();
    pChanInfo->pSemSurf->SetForceSizeAlloc(true);
    pChanInfo->pSemSurf->SetArrayPitch(1);
    pChanInfo->pSemSurf->SetArraySize(CHUNK_SIZE);
    pChanInfo->pSemSurf->SetColorFormat(ColorUtils::VOID32);
    pChanInfo->pSemSurf->SetAddressModel(Memory::Paging);
    pChanInfo->pSemSurf->SetLayout(Surface2D::Pitch);
    pChanInfo->pSemSurf->SetLocation(Memory::NonCoherent);
    pChanInfo->pSemSurf->SetGpuVASpace(pChanInfo->hVASpace);
    CHECK_RC(pChanInfo->pSemSurf->Alloc(GetBoundGpuDevice()));
    CHECK_RC(pChanInfo->pSemSurf->Map());

    pChanInfo->pPatSurf = new Surface2D();
    pChanInfo->pPatSurf->SetForceSizeAlloc(true);
    pChanInfo->pPatSurf->SetArrayPitch(1);
    pChanInfo->pPatSurf->SetArraySize(CHUNK_SIZE);
    pChanInfo->pPatSurf->SetColorFormat(ColorUtils::VOID32);
    pChanInfo->pPatSurf->SetAddressModel(Memory::Paging);
    pChanInfo->pPatSurf->SetLayout(Surface2D::Pitch);
    pChanInfo->pPatSurf->SetLocation(Memory::NonCoherent);
    pChanInfo->pPatSurf->SetGpuVASpace(pChanInfo->hVASpace);
    pChanInfo->pPatSurf->SetGpuCacheMode(Surface2D::GpuCacheOff);
    CHECK_RC(pChanInfo->pPatSurf->Alloc(GetBoundGpuDevice()));
    CHECK_RC(pChanInfo->pPatSurf->Map());

    return rc;
}

RC VASpaceInterOpTest::LaunchCopy(Channel *pChan, UINT64 srcAddr, UINT64 dstAddr, UINT64 size, UINT64 semAddr, UINT32 semValue)
{
    RC    rc;
    LwU32 subChan = LWA06F_SUBCHANNEL_COPY_ENGINE;

    CHECK_RC(pChan->Write(subChan, LWB0B5_OFFSET_IN_UPPER, LwU64_HI32(srcAddr)));
    CHECK_RC(pChan->Write(subChan, LWB0B5_OFFSET_IN_LOWER, LwU64_LO32(srcAddr)));

    CHECK_RC(pChan->Write(subChan, LWB0B5_OFFSET_OUT_UPPER, LwU64_HI32(dstAddr)));
    CHECK_RC(pChan->Write(subChan, LWB0B5_OFFSET_OUT_LOWER, LwU64_LO32(dstAddr)));

    CHECK_RC(pChan->Write(subChan, LWB0B5_PITCH_IN, 0));
    CHECK_RC(pChan->Write(subChan, LWB0B5_PITCH_OUT, 0));

    CHECK_RC(pChan->Write(subChan, LWB0B5_LINE_COUNT, 1));
    CHECK_RC(pChan->Write(subChan, LWB0B5_LINE_LENGTH_IN, UNSIGNED_CAST(UINT32, size)));

    CHECK_RC(pChan->Write(subChan, LWB0B5_SET_SRC_DEPTH, 1));
    CHECK_RC(pChan->Write(subChan, LWB0B5_SET_SRC_LAYER, 0));
    CHECK_RC(pChan->Write(subChan, LWB0B5_SET_SRC_WIDTH, UNSIGNED_CAST(UINT32, size)));
    CHECK_RC(pChan->Write(subChan, LWB0B5_SET_SRC_HEIGHT, 1));

    CHECK_RC(pChan->Write(subChan, LWB0B5_SET_SEMAPHORE_PAYLOAD, semValue));

    CHECK_RC(pChan->Write(subChan, LWB0B5_SET_SEMAPHORE_A,
                 DRF_NUM(B0B5, _SET_SEMAPHORE_A, _UPPER, LwU64_HI32(semAddr))));

    CHECK_RC(pChan->Write(subChan, LWB0B5_SET_SEMAPHORE_B, LwU64_LO32(semAddr)));
    CHECK_RC(pChan->Write(subChan, LWB0B5_LAUNCH_DMA,
                 DRF_DEF(B0B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, _RELEASE_ONE_WORD_SEMAPHORE) |
                 DRF_NUM(B0B5, _LAUNCH_DMA, _INTERRUPT_TYPE, LWB0B5_LAUNCH_DMA_INTERRUPT_TYPE_BLOCKING) |
                 DRF_DEF(B0B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE) |
                 DRF_DEF(B0B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH) |
                 DRF_DEF(B0B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH) |
                 DRF_DEF(B0B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _NON_PIPELINED)));

    CHECK_RC(pChan->Flush());

    return rc;
}

RC VASpaceInterOpTest::WaitForCopyDone(PCHANNEL_INFO pChanInfo, void *pSemAddr, UINT32 semValue)
{
    RC            rc;
    LwU32         semVal;
    PollArguments args;

    if (m_pModsEvent)
    {
        CHECK_RC(Tasker::WaitOnEvent(m_pModsEvent, m_timeoutMs));
    }
    else
    {
        CHECK_RC(pChanInfo->pChan->WaitIdle(m_timeoutMs));
    }

    args.value  = semValue;
    args.pAddr  = (UINT32 *)pSemAddr;
    CHECK_RC(PollVal(&args, m_timeoutMs));

    semVal = MEM_RD32(pSemAddr);
    if (semVal != semValue)
    {
        Printf(Tee::PriHigh, "%s: expected semaphore to be 0x%x, was 0x%x\n",
               __FUNCTION__, semValue, semVal);
        CHECK_RC(RC::DATA_MISMATCH);
        MASSERT(0);
    }

    return rc;
}

RC VASpaceInterOpTest::Fill(UINT32 chanIdx, UINT64 gpuAddr, UINT64 size, UINT32 val)
{
    RC     rc;
    UINT64 patGpuAddr, semGpuAddr;
    UINT32 loop, semValue;
    void  *semCpuAddr;

    // Copy size must be a greater than the chunk size
    MASSERT(size >= CHUNK_SIZE);
    // Copy size must be a multiple of the chunk size
    MASSERT((size & (CHUNK_SIZE - 1)) == 0);
    loop = UNSIGNED_CAST(UINT32, size/CHUNK_SIZE);

    patGpuAddr = m_chanInfo[chanIdx].pPatSurf->GetCtxDmaOffsetGpu();

    semGpuAddr = m_chanInfo[chanIdx].pSemSurf->GetCtxDmaOffsetGpu();
    semCpuAddr = m_chanInfo[chanIdx].pSemSurf->GetAddress();
    semValue = 0x1234000;

    for (UINT32 i = 0; i < loop; i++)
    {
        CHECK_RC(m_chanInfo[chanIdx].pPatSurf->Fill(val + i));

#ifdef VAS_INTEROP_RMTEST_DEBUG
        Printf(Tee::PriHigh, "%s: Copying chunk %d\n", __FUNCTION__, i);
#endif
        MEM_WR32(semCpuAddr, 0);

        CHECK_RC(LaunchCopy(m_chanInfo[chanIdx].pChan, patGpuAddr, gpuAddr + (i * CHUNK_SIZE),
                            CHUNK_SIZE, semGpuAddr, semValue));

        CHECK_RC(WaitForCopyDone(&m_chanInfo[chanIdx], semCpuAddr, semValue));

        semValue++;
#ifdef VAS_INTEROP_RMTEST_DEBUG
        Printf(Tee::PriHigh, "%s: Copying chunk %d Done !!!\n", __FUNCTION__, i);
#endif
    }

    return rc;
}

RC VASpaceInterOpTest::Compare(UINT32 chanIdx, UINT64 gpuAddr, UINT64 size, UINT32 val)
{
    RC rc;
    UINT64 patGpuAddr, semGpuAddr;
    UINT32 loop, i, semValue;
    void *semCpuAddr, *patCpuAddr;
    vector<UINT32> fillVec(1);

    // Copy size must be a greater than the chunk size
    MASSERT(size >= CHUNK_SIZE);
    // Copy size must be a multiple of the chunk size
    MASSERT((size & (CHUNK_SIZE - 1)) == 0);
    loop = UNSIGNED_CAST(UINT32, size/CHUNK_SIZE);

    patGpuAddr = m_chanInfo[chanIdx].pPatSurf->GetCtxDmaOffsetGpu();
    patCpuAddr = m_chanInfo[chanIdx].pPatSurf->GetAddress();

    semGpuAddr = m_chanInfo[chanIdx].pSemSurf->GetCtxDmaOffsetGpu();
    semCpuAddr = m_chanInfo[chanIdx].pSemSurf->GetAddress();
    semValue   = 0x1234000;

    for (i = 0; i < loop; i++)
    {
#ifdef VAS_INTEROP_RMTEST_DEBUG
        Printf(Tee::PriHigh, "%s: Copying chunk %d\n", __FUNCTION__, i);
#endif
        MEM_WR32(semCpuAddr, 0);

        CHECK_RC(LaunchCopy(m_chanInfo[chanIdx].pChan, gpuAddr + (i * CHUNK_SIZE), patGpuAddr,
                            CHUNK_SIZE, semGpuAddr, semValue));

        CHECK_RC(WaitForCopyDone(&m_chanInfo[chanIdx], semCpuAddr, semValue));

        semValue++;
#ifdef VAS_INTEROP_RMTEST_DEBUG
        Printf(Tee::PriHigh, "%s: Copying chunk %d Done !!!\n", __FUNCTION__, i);
#endif

        fillVec[0] = val + i;

        rc = Memory::ComparePattern(patCpuAddr, &fillVec, CHUNK_SIZE);
        if (rc != OK)
        {
            Printf(Tee::PriHigh, "%s: Data was not copied successfully by CE\n", __FUNCTION__);
            CHECK_RC(RC::DATA_MISMATCH);
        }
    }

    return rc;
}

//! \brief Run() : The body of this test.
//!
//! In this function, it calls several  sub-tests.
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC VASpaceInterOpTest::Run()
{
    RC rc;

    for (UINT32 i = 0; i < PHYSMEM_COUNT; i++)
    {
        if (m_hPhysMem[i])
        {
            CHECK_RC(TestInterOp1(CHAN_64K, MAP_BIG, CHAN_128K, MAP_BIG, m_hPhysMem[i], 0xCAFEBABE, 64 * 1024, 64 * 1024));
            CHECK_RC(TestInterOp1(CHAN_64K, MAP_BIG, CHAN_128K, MAP_SMALL, m_hPhysMem[i], 0xCAFEBABE, 64 * 1024, 64 * 1024));
            CHECK_RC(TestInterOp1(CHAN_64K, MAP_SMALL, CHAN_128K, MAP_BIG, m_hPhysMem[i], 0xCAFEBABE, 64 * 1024, 64 * 1024));
            if (m_pGpuDev->SupportsPageSize(GpuDevice::PAGESIZE_HUGE))
            {
                CHECK_RC(TestInterOp1(CHAN_64K, MAP_SMALL, CHAN_128K, MAP_HUGE, m_hPhysMem[i], 0xCAFEBABE, 64 * 1024, 64 * 1024));
                CHECK_RC(TestInterOp1(CHAN_64K, MAP_BIG, CHAN_128K, MAP_HUGE, m_hPhysMem[i], 0xCAFEBABE, 64 * 1024, 64 * 1024));
                CHECK_RC(TestInterOp1(CHAN_64K, MAP_HUGE, CHAN_128K, MAP_HUGE, m_hPhysMem[i], 0xCAFEBABE, 64 * 1024, 64 * 1024));
            }
            CHECK_RC(TestInterOp2(CHAN_64K, CHAN_128K, m_hPhysMem[i], 0xDEADBEEF, 64 * 1024, 64 * 1024));
        }
    }

    return rc;
}

// Test whether a channel can see updates to memory by another channel.
// We test the interop between channels with different Big Page sized Virtual Address Space,
// w.r.t big-big & big-small page size mappings.
// Chan1 will write to the memory with a mapping specified "mapPageSizeAttr1", and
// Chan2 will read the physical memory with the mapping specified by "mapPageSizeAttr2".
// We then check whether chan2 sees the updates correctly.

RC VASpaceInterOpTest::TestInterOp1(UINT32 chan1, UINT32 mapPageSizeAttr1, UINT32 chan2,
                                    UINT32 mapPageSizeAttr2, LwRm::Handle hPhysMem,
                                    UINT32 value, UINT32 offset, UINT32 size)
{
    RC      rc;
    UINT32  attr;
    UINT64  gpuAddr1, gpuAddr2;

    Printf(Tee::PriHigh, "%s: Start \n", __FUNCTION__);

    //
    // Now map the GPU virtual address to the physical address
    //

    gpuAddr1 = gpuAddr2 = 0;

    attr = DRF_NUM(OS46, _FLAGS, _PAGE_SIZE, mapPageSizeAttr1);
    CHECK_RC(m_pLwRm->MapMemoryDma(m_chanInfo[chan1].hVirtMem, hPhysMem, offset, size,
                                 attr, &gpuAddr1, GetBoundGpuDevice()));

    attr = DRF_NUM(OS46, _FLAGS, _PAGE_SIZE, mapPageSizeAttr2);
    CHECK_RC(m_pLwRm->MapMemoryDma(m_chanInfo[chan2].hVirtMem, hPhysMem, offset, size,
                                 attr, &gpuAddr2, GetBoundGpuDevice()));

#ifdef VAS_INTEROP_RMTEST_DEBUG
    Printf(Tee::PriHigh, "%s: Gpuaddr1: 0x%llx  Gpuaddr2: 0x%llx\n", __FUNCTION__, gpuAddr1, gpuAddr2);
#endif

    CHECK_RC(m_chanInfo[chan1].pChan->ScheduleChannel(true));
    CHECK_RC(m_chanInfo[chan2].pChan->ScheduleChannel(true));

    CHECK_RC(Fill(chan1, gpuAddr1, size, value));

    CHECK_RC(Compare(chan2, gpuAddr2, size, value));

    CHECK_RC(m_chanInfo[chan1].pChan->ScheduleChannel(false));
    CHECK_RC(m_chanInfo[chan2].pChan->ScheduleChannel(false));

    m_pLwRm->UnmapMemoryDma(m_chanInfo[chan1].hVirtMem, hPhysMem, LWOS47_FLAGS_DEFER_TLB_ILWALIDATION_FALSE, gpuAddr1, GetBoundGpuDevice());
    m_pLwRm->UnmapMemoryDma(m_chanInfo[chan2].hVirtMem, hPhysMem, LWOS47_FLAGS_DEFER_TLB_ILWALIDATION_FALSE, gpuAddr2, GetBoundGpuDevice());

    Printf(Tee::PriHigh, "%s: End \n", __FUNCTION__);

    return rc;
}

// Test whether updates to adjacent memory from different channels happen correctly.
// Chan1 will write to the first half, and chan2 writes to the second half.
// We check whether the updates happen correctly.

RC VASpaceInterOpTest::TestInterOp2(UINT32 chan1, UINT32 chan2, LwRm::Handle hPhysMem,
                             UINT32 value, UINT32 offset, UINT32 size)
{
    RC      rc;
    UINT32  attr;
    UINT64  gpuAddr1, gpuAddr2;
    UINT32 *cpuAddr;

    Printf(Tee::PriHigh, "%s: Start \n", __FUNCTION__);

    gpuAddr1 = gpuAddr2 = 0;

    //
    // Now map the GPU virtual address to the physical address
    //
    attr = LWOS32_ATTR2_NONE;

    CHECK_RC(m_pLwRm->MapMemory(hPhysMem, offset, 2 * size, (void **)&cpuAddr, 0, GetBoundGpuSubdevice()));

    CHECK_RC(m_pLwRm->MapMemoryDma(m_chanInfo[chan1].hVirtMem, hPhysMem, offset, size,
                                 attr, &gpuAddr1, GetBoundGpuDevice()));
    CHECK_RC(m_pLwRm->MapMemoryDma(m_chanInfo[chan2].hVirtMem, hPhysMem, offset + size, size,
                                 attr, &gpuAddr2, GetBoundGpuDevice()));

#ifdef VAS_INTEROP_RMTEST_DEBUG
    Printf(Tee::PriHigh, "%s: Gpuaddr1: 0x%llx  Gpuaddr2: 0x%llx\n", __FUNCTION__, gpuAddr1, gpuAddr2);
#endif

    CHECK_RC(m_chanInfo[chan1].pChan->ScheduleChannel(true));
    CHECK_RC(m_chanInfo[chan2].pChan->ScheduleChannel(true));

    CHECK_RC(Fill(chan1, gpuAddr1, size, value));
    CHECK_RC(Fill(chan2, gpuAddr2, size, value));

    vector<UINT32> fillVec(1);

    for (UINT32 i = 0; i < 2; i++)
    {
        for (UINT32 j = 0; j < size/CHUNK_SIZE; j++)
        {
            fillVec[0] = value + j;
            rc = Memory::ComparePattern(cpuAddr + (i * size/4) + (j * CHUNK_SIZE/4), &fillVec, CHUNK_SIZE);
            if (rc != OK)
            {
                Printf(Tee::PriHigh, "%s: Data was not copied successfully by CE\n", __FUNCTION__);
                CHECK_RC(RC::DATA_MISMATCH);
            }
        }
    }

    CHECK_RC(m_chanInfo[chan1].pChan->ScheduleChannel(false));
    CHECK_RC(m_chanInfo[chan2].pChan->ScheduleChannel(false));

    m_pLwRm->UnmapMemory(hPhysMem, cpuAddr, 0, GetBoundGpuSubdevice());
    m_pLwRm->UnmapMemoryDma(m_chanInfo[chan1].hVirtMem, hPhysMem, LWOS47_FLAGS_DEFER_TLB_ILWALIDATION_FALSE, gpuAddr1, GetBoundGpuDevice());
    m_pLwRm->UnmapMemoryDma(m_chanInfo[chan2].hVirtMem, hPhysMem, LWOS47_FLAGS_DEFER_TLB_ILWALIDATION_FALSE, gpuAddr2, GetBoundGpuDevice());

    Printf(Tee::PriHigh, "%s: End \n", __FUNCTION__);

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC VASpaceInterOpTest::Cleanup()
{
    RC rc;
    UINT32 i;

    for (i = 0; i < CHAN_COUNT; i++)
    {
        if (m_chanInfo[i].hNotifyEvent)
            m_pLwRm->Free(m_chanInfo[i].hNotifyEvent);

        if (m_chanInfo[i].pCopyAlloc)
        {
            m_chanInfo[i].pCopyAlloc->Free();
            delete m_chanInfo[i].pCopyAlloc;
        }

        if (m_chanInfo[i].pChan)
            rc = m_TestConfig.FreeChannel(m_chanInfo[i].pChan);

        if (m_chanInfo[i].pSemSurf)
        {
            m_chanInfo[i].pSemSurf->Free();
            delete m_chanInfo[i].pSemSurf;
        }
        if (m_chanInfo[i].pPatSurf)
        {
            m_chanInfo[i].pPatSurf->Free();
            delete m_chanInfo[i].pPatSurf;
        }

        if (m_chanInfo[i].hVirtMem)
            m_pLwRm->Free(m_chanInfo[i].hVirtMem);

        if (m_chanInfo[i].hVASpace)
            m_pLwRm->Free(m_chanInfo[i].hVASpace);
    }

    if (m_pModsEvent)
    {
        Tasker::FreeEvent(m_pModsEvent);
    }

    for (i = 0; i < PHYSMEM_COUNT; i++)
    {
        if (m_hPhysMem[i])
            m_pLwRm->Free(m_hPhysMem[i]);
    }

    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------
//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ MultiVATest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(VASpaceInterOpTest, RmTest,
                 "VASpaceInterOpTest RM test.");
