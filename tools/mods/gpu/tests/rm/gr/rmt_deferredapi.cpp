/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011,2019-2020 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_GpuDeferredApiTest.cpp
//! \brief To verify basic functionality of new FBBA Cache (lwrrently igt21a
//!  specific) and its resman code paths.
//!
//! Lwrrently includes Basic API testing and PTE fields verificaiton for GPU
//! Cacheable bit. The APIs that are covered are VidHeapControl(),
//! AllocContextDam(), MapMemoryDma().

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include <string>          // Only use <> for built-in C++ stuff
#include "class/cl8697.h"
#include "core/utility/errloggr.h"
#include "lwos.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/notifier.h"
#include "lwRmApi.h"
#include "class/cl007d.h"  // LW04_SOFTWARE_TEST
#include "class/cl826f.h"  // G82_CHANNEL_GPFIFO
#include "class/cl9097.h"  // FERMI_A
#include "class/cl902d.h" // FERMI_TWOD_A
#include "class/cl2080.h"
#include "ctrl/ctrl5080.h"
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl208f.h"
#include "class/cl5080.h"
#include "class/cl866f.h"
#include "class/cl2080.h" // LW2080_ENGINE_TYPE_GRAPHICS
#include "gpu/include/gralloc.h"
#include "core/include/memcheck.h"

#define MAX_MEM_LIMIT            (1*1024)

#define NOTIFIER_MAXCOUNT        1             //Maximum count for notifiers for class LW50_TESLA

#define SW_OBJ_SUBCH 5

#ifdef CHECK_RC
#undef CHECK_RC
#endif

#define CHECK_RC(f)              \
      do {                       \
         if (OK != (rc = (f)))   \
         {                       \
            Printf(Tee::PriHigh,                                           \
               "CHECK_RC returned %d at %s %d\n",                          \
               static_cast<UINT32>(rc), __FILE__, __LINE__);               \
            MASSERT(!"Generic mods assert failure<refer above message>."); \
            return rc;           \
         }                       \
      } while (0)

class GpuDeferredApiTest : public RmTest
{
public:
    GpuDeferredApiTest();
    virtual ~GpuDeferredApiTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual RC AllocCtxBuffer();
    virtual RC AllocZlwllCtxBuffer();
    virtual RC AllocGfxpCtxBuffers();
    virtual RC AllocGfxpPoolCtxBuffers();
    virtual RC SetupGfxpPool();
    virtual RC AllocPhysicalMem(UINT64 size, UINT64 align, LwRm::Handle *memHandle, UINT64 *gpuOffset);
    virtual RC AllocVirtualMem(UINT64 size, UINT64 align, LwRm::Handle *memHandle, UINT64 *gpuOffset);

private:

    Channel*         m_pCh_first;
    LwRm::Handle     m_hCh_first;

    LwRm::Handle     m_hObj;
    LwRm::Handle     m_hObjGr2d;
    LwRm::Handle     m_hObj5080;
    FLOAT64          m_TimeoutMs;

    LwRm::Handle     m_hSemCtxDma;
    LwRm::Handle     m_hSemMem;
    LwU32*           m_pSem;

    LwU32            m_gpuFamily;

    LwBool           m_supportsRtv;

    UINT64          m_gpuAddr;
    UINT64          m_gpuZlwllAddr;
    UINT64          m_gpuPreemptAddr;
    UINT64          m_gpuSpillAddr;
    UINT64          m_gpuPagepoolAddr;
    UINT64          m_gpuBetacbAddr;
    UINT64          m_gpuRtvAddr;

    UINT64          m_gpuPhysOffset;
    UINT64          m_gpuZlwllPhysOffset;
    UINT64          m_gpuPreemptPhysOffset;
    UINT64          m_gpuSpillPhysOffset;
    UINT64          m_gpuPagepoolPhysOffset;
    UINT64          m_gpuBetacbPhysOffset;
    UINT64          m_gpuRtvPhysOffset;

    UINT64          m_gpuVirtOffset;
    UINT64          m_gpuZlwllVirtOffset;
    UINT64          m_gpuPreemptVirtOffset;
    UINT64          m_gpuSpillVirtOffset;
    UINT64          m_gpuPagepoolVirtOffset;
    UINT64          m_gpuBetacbVirtOffset;
    UINT64          m_gpuRtvVirtOffset;

    LwRm::Handle    m_hVirtMem;
    LwRm::Handle    m_hPhysMem;
    LwRm::Handle    m_hZlwllVirtMem;
    LwRm::Handle    m_hZlwllPhysMem;
    LwRm::Handle    m_hPreemptVirtMem;
    LwRm::Handle    m_hPreemptPhysMem;
    LwRm::Handle    m_hSpillVirtMem;
    LwRm::Handle    m_hSpillPhysMem;
    LwRm::Handle    m_hPagepoolVirtMem;
    LwRm::Handle    m_hPagepoolPhysMem;
    LwRm::Handle    m_hBetacbVirtMem;
    LwRm::Handle    m_hBetacbPhysMem;
    LwRm::Handle    m_hRtvVirtMem;
    LwRm::Handle    m_hRtvPhysMem;

    LwRm::Handle    m_hClient;
    LwRm::Handle    m_hDevice;
    LwRm::Handle    m_hSubDev;
    LwRm::Handle    m_hSubDevDiag;

    LwRm::Handle    m_hPoolPhysMem;
    LwRm::Handle    m_hPoolVirtMem;
    UINT64          m_gpuPoolVirtOffset;
    UINT64          m_gpuPoolPhysOffset;
    UINT64          m_gpuPoolAddr;

    LwRm::Handle    m_hPoolCtrlBlockPhysMem;
    LwRm::Handle    m_hPoolCtrlBlockVirtMem;
    UINT64          m_gpuPoolCtrlBlockVirtOffset;
    UINT64          m_gpuPoolCtrlBlockPhysOffset;
    UINT64          m_gpuPoolCtrlBlockAddr;

    UINT32          *m_poolCtrlBlockCpuAddr;
    UINT64          m_ctrlWords;

    ThreeDAlloc     m_3dAlloc_gr;
    ThreeDAlloc     m_3dAlloc_first;
    RC AllocMemory
    (
        UINT32            type,
        UINT32            flags,
        UINT32            attr,
        UINT32            attr2,
        UINT64            memSize,
        UINT32          * pHandle
    );

    LW0080_CTRL_FIFO_GET_ENGINE_CONTEXT_PROPERTIES_PARAMS ctxPropParams;
    LW0080_CTRL_FIFO_GET_ENGINE_CONTEXT_PROPERTIES_PARAMS ctxZlwllPropParams;
};

//! \brief GpuDeferredApiTest constructor
//!
//! Placeholder : doesnt do much, much funcationality in Setup()
//!
//! \sa Setup
//------------------------------------------------------------------------------
GpuDeferredApiTest::GpuDeferredApiTest() :
    m_pCh_first(nullptr),
    m_hCh_first(0),
    m_hObj(0),
    m_hObjGr2d(0),
    m_hObj5080(0),
    m_TimeoutMs(Tasker::NO_TIMEOUT),
    m_hSemCtxDma(0),
    m_hSemMem(0),
    m_pSem(nullptr),
    m_gpuFamily(0),
    m_supportsRtv(LW_FALSE),
    m_gpuAddr(0LL),
    m_gpuZlwllAddr(0LL),
    m_gpuPreemptAddr(0LL),
    m_gpuSpillAddr(0LL),
    m_gpuPagepoolAddr(0LL),
    m_gpuBetacbAddr(0LL),
    m_gpuRtvAddr(0LL),
    m_gpuPhysOffset(0),
    m_gpuZlwllPhysOffset(0),
    m_gpuPreemptPhysOffset(0),
    m_gpuSpillPhysOffset(0),
    m_gpuPagepoolPhysOffset(0),
    m_gpuBetacbPhysOffset(0),
    m_gpuRtvPhysOffset(0),
    m_gpuVirtOffset(0),
    m_gpuZlwllVirtOffset(0),
    m_gpuPreemptVirtOffset(0),
    m_gpuSpillVirtOffset(0),
    m_gpuPagepoolVirtOffset(0),
    m_gpuBetacbVirtOffset(0),
    m_gpuRtvVirtOffset(0),
    m_hVirtMem(0),
    m_hPhysMem(0),
    m_hZlwllVirtMem(0),
    m_hZlwllPhysMem(0),
    m_hPreemptVirtMem(0),
    m_hPreemptPhysMem(0),
    m_hSpillVirtMem(0),
    m_hSpillPhysMem(0),
    m_hPagepoolVirtMem(0),
    m_hPagepoolPhysMem(0),
    m_hBetacbVirtMem(0),
    m_hBetacbPhysMem(0),
    m_hRtvVirtMem(0),
    m_hRtvPhysMem(0),
    m_hClient(0),
    m_hDevice(0),
    m_hSubDev(0),
    m_hSubDevDiag(0),
    m_hPoolPhysMem(0),
    m_hPoolVirtMem(0),
    m_gpuPoolVirtOffset(0),
    m_gpuPoolPhysOffset(0),
    m_gpuPoolAddr(0LL),
    m_hPoolCtrlBlockPhysMem(0),
    m_hPoolCtrlBlockVirtMem(0),
    m_gpuPoolCtrlBlockVirtOffset(0),
    m_gpuPoolCtrlBlockPhysOffset(0),
    m_gpuPoolCtrlBlockAddr(0LL),
    m_poolCtrlBlockCpuAddr(nullptr),
    m_ctrlWords(0),
    ctxPropParams({}),
    ctxZlwllPropParams({})
{
    SetName("GpuDeferredApiTest");
    m_3dAlloc_gr.SetOldestSupportedClass(FERMI_A);
    m_3dAlloc_first.SetOldestSupportedClass(FERMI_A);
}

//! \brief GpuDeferredApiTest destructor
//!
//! Placeholder : doesnt do much, much funcationality in Setup()
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
GpuDeferredApiTest::~GpuDeferredApiTest()
{

}

RC GpuDeferredApiTest::AllocPhysicalMem
(
    UINT64 size,
    UINT64 align,
    LwRm::Handle *memHandle,
    UINT64 *gpuOffset
)
{
    RC           rc;
    LwRmPtr      pLwRm;
    UINT32       attr;
    UINT32       attr2;

    //
    // Allocate the physical address
    //
    attr = (DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH) |
            DRF_DEF(OS32, _ATTR, _DEPTH, _32) |
            DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1) |
            DRF_DEF(OS32, _ATTR, _TILED, _NONE) |
            DRF_DEF(OS32, _ATTR, _ZLWLL, _NONE) |
            DRF_DEF(OS32, _ATTR, _COMPR, _NONE) |
            DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM) |
            DRF_DEF(OS32, _ATTR, _COHERENCY, _UNCACHED) |
            DRF_DEF(OS32, _ATTR, _PHYSICALITY, _CONTIGUOUS));
    attr2 = LWOS32_ATTR2_NONE;

    CHECK_RC(pLwRm->VidHeapAllocSizeEx(LWOS32_TYPE_IMAGE,
                                       LWOS32_ALLOC_FLAGS_NO_SCANOUT |
                                       LWOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED,
                                       &attr,
                                       &attr2, //pAttr2
                                       size,
                                       align,    // alignment
                                       NULL, // pFormat
                                       NULL, // pCoverage
                                       NULL, // pPartitionStride
                                       memHandle,
                                       gpuOffset, // poffset,
                                       NULL, // pLimit
                                       NULL, // pRtnFree
                                       NULL, // pRtnTotal
                                       0,    // width
                                       0,    // height
                                       GetBoundGpuDevice()));

    return rc;
}

RC GpuDeferredApiTest::AllocVirtualMem
(
    UINT64 size,
    UINT64 align,
    LwRm::Handle *memHandle,
    UINT64 *gpuOffset
)
{
    RC           rc;
    LwRmPtr      pLwRm;
    UINT32       attr;
    UINT32       attr2;

    //
    // Allocate the GPU virtual address
    //
    attr = (DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH) |
            DRF_DEF(OS32, _ATTR, _DEPTH, _32) |
            DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1) |
            DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _DEFAULT) |
            DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM) |
            DRF_DEF(OS32, _ATTR, _COHERENCY, _UNCACHED) |
            DRF_DEF(OS32, _ATTR, _PHYSICALITY, _CONTIGUOUS));
    attr2 = LWOS32_ATTR2_NONE;

    CHECK_RC(pLwRm->VidHeapAllocSizeEx(LWOS32_TYPE_IMAGE,
                                       LWOS32_ALLOC_FLAGS_VIRTUAL,
                                       &attr,
                                       &attr2, // pAttr2
                                       size,
                                       align,    // alignment
                                       NULL, // pFormat
                                       NULL, // pCoverage
                                       NULL, // pPartitionStride
                                       memHandle,
                                       gpuOffset, // poffset,
                                       NULL, // pLimit
                                       NULL, // pRtnFree
                                       NULL, // pRtnTotal
                                       0,    // width
                                       0,    // height
                                       GetBoundGpuDevice()));

    return rc;
}

RC GpuDeferredApiTest::AllocCtxBuffer()
{
    RC           rc;
    LwRmPtr      pLwRm;

    // find out the size of gr context buffer
    ctxPropParams.engineId = LW0080_CTRL_FIFO_GET_ENGINE_CONTEXT_PROPERTIES_ENGINE_ID_GRAPHICS;
    CHECK_RC(pLwRm->Control(m_hDevice,
                            LW0080_CTRL_CMD_FIFO_GET_ENGINE_CONTEXT_PROPERTIES,
                            (void*)&ctxPropParams,
                            sizeof(ctxPropParams)));

    CHECK_RC(AllocVirtualMem(ctxPropParams.size, 1, &m_hVirtMem, &m_gpuVirtOffset));
    CHECK_RC(AllocPhysicalMem(ctxPropParams.size, 1, &m_hPhysMem, &m_gpuPhysOffset));

    //
    // Now map the GPU virtual address to the physical address
    //
    CHECK_RC(pLwRm->MapMemoryDma(m_hVirtMem, m_hPhysMem, 0, ctxPropParams.size,
                                 DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE) |
                                 DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _ENABLE),
                                 &m_gpuAddr, GetBoundGpuDevice()));

    return rc;
}

RC GpuDeferredApiTest::AllocZlwllCtxBuffer()
{
    RC           rc;
    LwRmPtr      pLwRm;

    // find out the size of gr zlwll context buffer
    ctxZlwllPropParams.engineId = LW0080_CTRL_FIFO_GET_ENGINE_CONTEXT_PROPERTIES_ENGINE_ID_GRAPHICS_ZLWLL;
    CHECK_RC(pLwRm->Control(m_hDevice,
                            LW0080_CTRL_CMD_FIFO_GET_ENGINE_CONTEXT_PROPERTIES,
                            (void*)&ctxZlwllPropParams,
                            sizeof(ctxZlwllPropParams)));

    CHECK_RC(AllocVirtualMem(ctxZlwllPropParams.size, 1, &m_hZlwllVirtMem, &m_gpuZlwllVirtOffset));
    CHECK_RC(AllocPhysicalMem(ctxZlwllPropParams.size, 1, &m_hZlwllPhysMem, &m_gpuZlwllPhysOffset));

    //
    // Now map the GPU virtual address to the physical address
    //
    CHECK_RC(pLwRm->MapMemoryDma(m_hZlwllVirtMem, m_hZlwllPhysMem, 0, ctxZlwllPropParams.size,
                                 DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE) |
                                 DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _ENABLE),
                                 &m_gpuZlwllAddr, GetBoundGpuDevice()));

    return rc;
}

RC GpuDeferredApiTest::AllocGfxpCtxBuffers()
{
    RC           rc;
    LwRmPtr      pLwRm;

    // find out the size of gr preempt context buffer
    ctxPropParams.engineId = LW0080_CTRL_FIFO_GET_ENGINE_CONTEXT_PROPERTIES_ENGINE_ID_GRAPHICS_PREEMPT;
    CHECK_RC(pLwRm->Control(m_hDevice,
                            LW0080_CTRL_CMD_FIFO_GET_ENGINE_CONTEXT_PROPERTIES,
                            (void*)&ctxPropParams,
                            sizeof(ctxPropParams)));

    CHECK_RC(AllocVirtualMem(ctxPropParams.size, 1, &m_hPreemptVirtMem, &m_gpuPreemptVirtOffset));
    CHECK_RC(AllocPhysicalMem(ctxPropParams.size, 1, &m_hPreemptPhysMem, &m_gpuPreemptPhysOffset));

    //
    // Now map the GPU virtual address to the physical address
    //
    CHECK_RC(pLwRm->MapMemoryDma(m_hPreemptVirtMem, m_hPreemptPhysMem, 0, ctxPropParams.size,
                                 DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE) |
                                 DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _ENABLE),
                                 &m_gpuPreemptAddr, GetBoundGpuDevice()));

    // find out the size of gr Spill context buffer
    ctxPropParams.engineId = LW0080_CTRL_FIFO_GET_ENGINE_CONTEXT_PROPERTIES_ENGINE_ID_GRAPHICS_SPILL;
    CHECK_RC(pLwRm->Control(m_hDevice,
                            LW0080_CTRL_CMD_FIFO_GET_ENGINE_CONTEXT_PROPERTIES,
                            (void*)&ctxPropParams,
                            sizeof(ctxPropParams)));

    CHECK_RC(AllocVirtualMem(ctxPropParams.size, 1, &m_hSpillVirtMem, &m_gpuSpillVirtOffset));
    CHECK_RC(AllocPhysicalMem(ctxPropParams.size, 1, &m_hSpillPhysMem, &m_gpuSpillPhysOffset));

    //
    // Now map the GPU virtual address to the physical address
    //
    CHECK_RC(pLwRm->MapMemoryDma(m_hSpillVirtMem, m_hSpillPhysMem, 0, ctxPropParams.size,
                                 DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE) |
                                 DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _ENABLE),
                                 &m_gpuSpillAddr, GetBoundGpuDevice()));

    // find out the size of gr Pagepool context buffer
    ctxPropParams.engineId = LW0080_CTRL_FIFO_GET_ENGINE_CONTEXT_PROPERTIES_ENGINE_ID_GRAPHICS_PAGEPOOL;
    CHECK_RC(pLwRm->Control(m_hDevice,
                            LW0080_CTRL_CMD_FIFO_GET_ENGINE_CONTEXT_PROPERTIES,
                            (void*)&ctxPropParams,
                            sizeof(ctxPropParams)));

    CHECK_RC(AllocVirtualMem(ctxPropParams.size, 1, &m_hPagepoolVirtMem, &m_gpuPagepoolVirtOffset));
    CHECK_RC(AllocPhysicalMem(ctxPropParams.size, 1, &m_hPagepoolPhysMem, &m_gpuPagepoolPhysOffset));

    //
    // Now map the GPU virtual address to the physical address
    //
    CHECK_RC(pLwRm->MapMemoryDma(m_hPagepoolVirtMem, m_hPagepoolPhysMem, 0, ctxPropParams.size,
                                 DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE) |
                                 DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _ENABLE),
                                 &m_gpuPagepoolAddr, GetBoundGpuDevice()));

    // find out the size of gr Betacb context buffer
    ctxPropParams.engineId = LW0080_CTRL_FIFO_GET_ENGINE_CONTEXT_PROPERTIES_ENGINE_ID_GRAPHICS_BETACB;
    CHECK_RC(pLwRm->Control(m_hDevice,
                            LW0080_CTRL_CMD_FIFO_GET_ENGINE_CONTEXT_PROPERTIES,
                            (void*)&ctxPropParams,
                            sizeof(ctxPropParams)));

    CHECK_RC(AllocVirtualMem(ctxPropParams.size, 1, &m_hBetacbVirtMem, &m_gpuBetacbVirtOffset));
    CHECK_RC(AllocPhysicalMem(ctxPropParams.size, 1, &m_hBetacbPhysMem, &m_gpuBetacbPhysOffset));

    //
    // Now map the GPU virtual address to the physical address
    //
    CHECK_RC(pLwRm->MapMemoryDma(m_hBetacbVirtMem, m_hBetacbPhysMem, 0, ctxPropParams.size,
                                 DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE) |
                                 DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _ENABLE),
                                 &m_gpuBetacbAddr, GetBoundGpuDevice()));

    if (m_supportsRtv)
    {
        // find out the size of gr RTV context buffer
        ctxPropParams.engineId = LW0080_CTRL_FIFO_GET_ENGINE_CONTEXT_PROPERTIES_ENGINE_ID_GRAPHICS_RTV;
        CHECK_RC(pLwRm->Control(m_hDevice,
                                LW0080_CTRL_CMD_FIFO_GET_ENGINE_CONTEXT_PROPERTIES,
                                (void*)&ctxPropParams,
                                sizeof(ctxPropParams)));

        CHECK_RC(AllocVirtualMem(ctxPropParams.size, 1, &m_hRtvVirtMem, &m_gpuRtvVirtOffset));
        CHECK_RC(AllocPhysicalMem(ctxPropParams.size, 1, &m_hRtvPhysMem, &m_gpuRtvPhysOffset));

        //
        // Now map the GPU virtual address to the physical address
        //
        CHECK_RC(pLwRm->MapMemoryDma(m_hRtvVirtMem, m_hRtvPhysMem, 0, ctxPropParams.size,
                                     DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE) |
                                     DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _ENABLE),
                                     &m_gpuRtvAddr, GetBoundGpuDevice()));

    }

    return rc;
}

RC GpuDeferredApiTest::AllocGfxpPoolCtxBuffers()
{
    RC           rc;
    LwRmPtr      pLwRm;
    LW2080_CTRL_GR_GFX_POOL_QUERY_SIZE_PARAMS poolQuery;

    poolQuery.maxSlots = 6;
    CHECK_RC(pLwRm->Control(m_hSubDev,
                            LW2080_CTRL_CMD_GR_GFX_POOL_QUERY_SIZE,
                            (void*)&poolQuery,
                            sizeof(poolQuery)));

    CHECK_RC(AllocVirtualMem(poolQuery.ctrlStructSize, poolQuery.ctrlStructAlign, &m_hPoolCtrlBlockVirtMem, &m_gpuPoolCtrlBlockVirtOffset));
    CHECK_RC(AllocPhysicalMem(poolQuery.ctrlStructSize, poolQuery.ctrlStructAlign, &m_hPoolCtrlBlockPhysMem, &m_gpuPoolCtrlBlockPhysOffset));

    //
    // Now map the GPU virtual address to the physical address
    //
    CHECK_RC(pLwRm->MapMemoryDma(m_hPoolCtrlBlockVirtMem, m_hPoolCtrlBlockPhysMem,
                                 0, poolQuery.ctrlStructSize,
                                 DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE) |
                                 DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _ENABLE),
                                 &m_gpuPoolCtrlBlockAddr, GetBoundGpuDevice()));

    // Get a CPU address as well
    CHECK_RC(pLwRm->MapMemory(m_hPoolCtrlBlockPhysMem, 0,
                              poolQuery.ctrlStructSize,
                              (void **)&m_poolCtrlBlockCpuAddr,
                              0, GetBoundGpuSubdevice()));

    CHECK_RC(AllocVirtualMem(poolQuery.poolSize, poolQuery.poolAlign, &m_hPoolVirtMem, &m_gpuPoolVirtOffset));
    CHECK_RC(AllocPhysicalMem(poolQuery.poolSize, poolQuery.poolAlign, &m_hPoolPhysMem, &m_gpuPoolPhysOffset));

    //
    // Now map the GPU virtual address to the physical address
    //
    CHECK_RC(pLwRm->MapMemoryDma(m_hPoolVirtMem, m_hPoolPhysMem, 0, poolQuery.poolSize,
                                 DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE) |
                                 DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _ENABLE),
                                 &m_gpuPoolAddr, GetBoundGpuDevice()));

    m_ctrlWords = poolQuery.ctrlStructSize / 4;

    return rc;
}

RC GpuDeferredApiTest::SetupGfxpPool()
{
    RC           rc;
    LwRmPtr      pLwRm;
    LW2080_CTRL_GR_GFX_POOL_INITIALIZE_PARAMS poolInit;
    LW2080_CTRL_GR_GFX_POOL_ADD_SLOTS_PARAMS poolSlots;
    LW2080_CTRL_GR_GFX_POOL_REMOVE_SLOTS_PARAMS rmPoolSlots;
    UINT32          foo;

    poolInit.pControlStructure = (LwP64)m_poolCtrlBlockCpuAddr;
    poolInit.maxSlots = 6;
    CHECK_RC(pLwRm->Control(m_hSubDev,
                            LW2080_CTRL_CMD_GR_GFX_POOL_INITIALIZE,
                            (void*)&poolInit,
                            sizeof(poolInit)));

    for (UINT64 x = 0; x < m_ctrlWords; x++)
    {
        foo = MEM_RD32(m_poolCtrlBlockCpuAddr + x);
        Printf(Tee::PriHigh,
                      "Pool Ctrl Block[%lld]: 0x%08x\n",
                      x, foo);
    }

    poolSlots.numSlots = 4;
    poolSlots.slots[0] = 0;
    poolSlots.slots[1] = 1;
    poolSlots.slots[2] = 33;
    poolSlots.slots[3] = 8;
    poolSlots.pControlStructure = (LwP64)m_poolCtrlBlockCpuAddr;
    CHECK_RC(pLwRm->Control(m_hSubDev,
                            LW2080_CTRL_CMD_GR_GFX_POOL_ADD_SLOTS,
                            (void*)&poolSlots,
                            sizeof(poolSlots)));

    for (UINT64 x = 0; x < m_ctrlWords; x++)
    {
        foo = MEM_RD32(m_poolCtrlBlockCpuAddr + x);
        Printf(Tee::PriHigh,
                      "Pool Ctrl Block[%lld]: 0x%08x\n",
                      x, foo);
    }

    rmPoolSlots.numSlots = 3;
    rmPoolSlots.slots[0] = 20;
    rmPoolSlots.slots[1] = 1;
    rmPoolSlots.slots[2] = 8;
    rmPoolSlots.pControlStructure = (LwP64)m_poolCtrlBlockCpuAddr;
    rmPoolSlots.bRemoveSpecificSlots = 1;
    CHECK_RC(pLwRm->Control(m_hSubDev,
                            LW2080_CTRL_CMD_GR_GFX_POOL_REMOVE_SLOTS,
                            (void*)&rmPoolSlots,
                            sizeof(rmPoolSlots)));

    for (UINT64 x = 0; x < m_ctrlWords; x++)
    {
        foo = MEM_RD32(m_poolCtrlBlockCpuAddr + x);
        Printf(Tee::PriHigh,
                      "Pool Ctrl Block[%lld]: 0x%08x\n",
                      x, foo);
    }

    return rc;
}

//! \brief IsTestSupported(), Looks for whether test can execute in current elw.
//!
//! Check for LW50_DEFERRED_API_CLASS class availibility
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string GpuDeferredApiTest::IsTestSupported()
{
    bool       bIsSupported = true;
   // Returns true only if the class is supported.
   bIsSupported = (IsClassSupported(LW50_DEFERRED_API_CLASS) && (m_3dAlloc_gr.IsSupported(GetBoundGpuDevice())));

   Printf(Tee::PriLow,"%d:GpuDeferredApiTest::IsSupported retured %d \n",
         __LINE__, bIsSupported);

   if(bIsSupported)
       return RUN_RMTEST_TRUE;
   return "Either LW50_DEFERRED_API_CLASS or none of FERMI_A+ classes are supported on current platform";
}

//! \brief  Setup(): Generally used for any test level allocation
//!
//! Allocating the channel and software object for use in the test
//
//! \return corresponding RC if any allocation fails
//------------------------------------------------------------------------------
RC GpuDeferredApiTest::Setup()
{
    RC         rc;
    LwRmPtr    pLwRm;
    LW208F_CTRL_FIFO_ENABLE_VIRTUAL_CONTEXT_PARAMS vcParams = {0};

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    m_TimeoutMs = m_TestConfig.TimeoutMs();

    m_TestConfig.SetAllowMultipleChannels(true);
    m_hClient = pLwRm->GetClientHandle();
    m_hDevice = pLwRm->GetDeviceHandle(GetBoundGpuDevice());
    m_hSubDev = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());

    m_hSubDevDiag = GetBoundGpuSubdevice()->GetSubdeviceDiag();

    // create regular channel
    Printf(Tee::PriNormal,
           "GpuDeferredApiTest: CREATING REGULAR CHANNEL\n");
    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh_first,
                                          &m_hCh_first,
                                          LW2080_ENGINE_TYPE_GRAPHICS));
    CHECK_RC(m_3dAlloc_first.Alloc(m_hCh_first, GetBoundGpuDevice()));

    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh,
                                          &m_hCh,
                                          LW2080_ENGINE_TYPE_GRAPHICS));

    // enable virtual context
    vcParams.hChannel = m_hCh;
    CHECK_RC(LwRmControl(m_hClient, m_hSubDevDiag,
                            LW208F_CTRL_CMD_FIFO_ENABLE_VIRTUAL_CONTEXT,
                            (void*)&vcParams,
                            sizeof(vcParams)));
    CHECK_RC(m_3dAlloc_gr.Alloc(m_hCh, GetBoundGpuDevice()));
    CHECK_RC(pLwRm->Alloc(m_hCh, &m_hObjGr2d, FERMI_TWOD_A, NULL));

    // SW class
    CHECK_RC(pLwRm->Alloc(m_hCh, &m_hObj, LW04_SOFTWARE_TEST, NULL));

    CHECK_RC(AllocMemory(LWOS32_TYPE_IMAGE,
                         0, // flags
                         0, // pAttr
                         0, // pAttr2
                         MAX_MEM_LIMIT,
                         &m_hSemMem));

    CHECK_RC(pLwRm->MapMemory(
                m_hSemMem,
                0, MAX_MEM_LIMIT,
                ((void**)&m_pSem),
                0, GetBoundGpuSubdevice()));

    CHECK_RC(pLwRm->AllocContextDma(
                &m_hSemCtxDma,
                DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_WRITE) |
                DRF_DEF(OS03, _FLAGS, _HASH_TABLE, _DISABLE),
                m_hSemMem, 0, MAX_MEM_LIMIT-1));

    return rc;
}

//! \brief Run:: Used generally for placing all the testing stuff.
//!
//! Runs the required tests. Lwrrently we have only Basic API testing in here.
//! Will be adding more tests.
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------------
RC GpuDeferredApiTest::Run()
{
    RC      rc;
    LwRmPtr pLwRm;
    LW208F_CTRL_FIFO_CHECK_ENGINE_CONTEXT_PARAMS   chkEngCtxParams = {0};
    LW5080_CTRL_DEFERRED_API_PARAMS                deferredParams = {0};
    LW5080_CTRL_REMOVE_API_PARAMS                  rmDeferredParams = {0};
    LwU32 apiHandle;
    GpuSubdevice *pSubdev;

    //
    // Test the deferred api path to the gpu cache allocation policy controls.
    // If we made it this far, it should be supported, fail if not.
    //
    if (!IsClassSupported(LW50_DEFERRED_API_CLASS))
    {
        Printf(Tee::PriHigh,"GpuDeferredApiTest: - LW50_DEFERRED_API_CLASS not supported, failing test.\n");
        CHECK_RC_CLEANUP(RC::LWRM_INSUFFICIENT_RESOURCES);
    }

    Printf(Tee::PriLow,"GpuDeferredApiTest: - Testing gpu cache alloc policy setting via class 5080 deferred API path...\n");

    CHECK_RC_CLEANUP(pLwRm->Alloc(m_hCh_first, &m_hObj5080, LW50_DEFERRED_API_CLASS, NULL));

    pSubdev = GetBoundGpuSubdevice();
    m_gpuFamily = (GetBoundGpuDevice())->GetFamily();

    if(m_gpuFamily >= GpuDevice::Turing)
    {
        m_supportsRtv = LW_TRUE;
    }

    m_hSubDev = pLwRm->GetSubdeviceHandle(pSubdev);
    m_hClient = pLwRm->GetClientHandle();

    //
    // 1) make sure we have no gr context
    //
    chkEngCtxParams.hChannel = m_hCh;
    chkEngCtxParams.engine   = LW2080_ENGINE_TYPE_GRAPHICS;
    CHECK_RC(LwRmControl(m_hClient, m_hSubDevDiag,
                            LW208F_CTRL_CMD_FIFO_CHECK_ENGINE_CONTEXT,
                            (void*)&chkEngCtxParams,
                            sizeof(chkEngCtxParams)));

    if(chkEngCtxParams.exists)
    {
        Printf(Tee::PriNormal,
               "GpuDeferredApiTest: ERROR: Channel already has context buffer\n");
        rc = RC::SOFTWARE_ERROR;
        return rc;
    }
    else
    {
        Printf(Tee::PriNormal,
               "GpuDeferredApiTest: PASS: Channel does not have gr context as expected\n");
    }

    AllocCtxBuffer();
    AllocZlwllCtxBuffer();

    m_pCh_first->SetObject(SW_OBJ_SUBCH, m_hObj5080);

    apiHandle = 0x10101010;

    Printf(Tee::PriLow,"GpuDeferredApiTest: Send CTXSW_ZLWLL_BIND via deferred channel api...\n");

    // Setup the deferred api bundle
    deferredParams.hApiHandle = apiHandle++;
    deferredParams.cmd        = LW2080_CTRL_CMD_GR_CTXSW_ZLWLL_BIND;
    deferredParams.flags = DRF_DEF(5080_CTRL_CMD, _DEFERRED_API_FLAGS, _DELETE, _EXPLICIT);
    deferredParams.api_bundle.ZlwllCtxsw.hClient = m_hClient;
    deferredParams.api_bundle.ZlwllCtxsw.hChannel = m_hCh;
    deferredParams.api_bundle.ZlwllCtxsw.vMemPtr = m_gpuZlwllAddr;
    deferredParams.api_bundle.ZlwllCtxsw.zlwllMode = LW2080_CTRL_CTXSW_ZLWLL_MODE_SEPARATE_BUFFER;

    CHECK_RC_CLEANUP(pLwRm->Control(m_hObj5080, LW5080_CTRL_CMD_DEFERRED_API,
                            (void*)&deferredParams, sizeof(deferredParams)));

    // Trigger the deferred API bundle via the pushbuffer calls
    m_pCh_first->Write(SW_OBJ_SUBCH, LW5080_DEFERRED_API, deferredParams.hApiHandle);
    m_pCh_first->Flush();
    m_pCh_first->WaitIdle(m_TimeoutMs);

    rmDeferredParams.hApiHandle = deferredParams.hApiHandle;
    CHECK_RC_CLEANUP(pLwRm->Control(m_hObj5080, LW5080_CTRL_CMD_REMOVE_API,
                            (void*)&rmDeferredParams, sizeof(rmDeferredParams)));

    // Setup the deferred api bundle for preemption
    if(m_gpuFamily >= GpuDevice::Pascal)
    {
        Printf(Tee::PriLow,"GpuDeferredApiTest: setting up GfxP via deferred channel api...\n");
        AllocGfxpCtxBuffers();
        AllocGfxpPoolCtxBuffers();
        SetupGfxpPool();

        deferredParams.hApiHandle = apiHandle++;
        deferredParams.cmd = LW2080_CTRL_CMD_GR_CTXSW_PREEMPTION_BIND;
        deferredParams.flags = DRF_DEF(5080_CTRL_CMD, _DEFERRED_API_FLAGS, _DELETE, _EXPLICIT);
        deferredParams.api_bundle.PreemptionCtxsw.hClient = m_hClient;
        deferredParams.api_bundle.PreemptionCtxsw.hChannel = m_hCh;
        deferredParams.api_bundle.PreemptionCtxsw.vMemPtrs[LW2080_CTRL_CMD_GR_CTXSW_PREEMPTION_BIND_BUFFERS_MAIN] = m_gpuPreemptAddr;
        deferredParams.api_bundle.PreemptionCtxsw.vMemPtrs[LW2080_CTRL_CMD_GR_CTXSW_PREEMPTION_BIND_BUFFERS_SPILL] = m_gpuSpillAddr;
        deferredParams.api_bundle.PreemptionCtxsw.vMemPtrs[LW2080_CTRL_CMD_GR_CTXSW_PREEMPTION_BIND_BUFFERS_BETACB] = m_gpuBetacbAddr;
        deferredParams.api_bundle.PreemptionCtxsw.vMemPtrs[LW2080_CTRL_CMD_GR_CTXSW_PREEMPTION_BIND_BUFFERS_PAGEPOOL] = m_gpuPagepoolAddr;
        if (m_supportsRtv)
        {
            deferredParams.api_bundle.PreemptionCtxsw.vMemPtrs[LW2080_CTRL_CMD_GR_CTXSW_PREEMPTION_BIND_BUFFERS_RTV] = m_gpuRtvAddr;
        }
        deferredParams.api_bundle.PreemptionCtxsw.gfxpPreemptMode = LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_GFX_GFXP;
        deferredParams.api_bundle.PreemptionCtxsw.flags =
            FLD_SET_DRF(2080, _CTRL_GR_SET_CTXSW_PREEMPTION_MODE_FLAGS, _GFXP, _SET, 0);

        CHECK_RC_CLEANUP(pLwRm->Control(m_hObj5080, LW5080_CTRL_CMD_DEFERRED_API,
                                        (void*)&deferredParams, sizeof(deferredParams)));

        // Trigger the deferred API bundle via the pushbuffer calls
        m_pCh_first->Write(SW_OBJ_SUBCH, LW5080_DEFERRED_API, deferredParams.hApiHandle);
        m_pCh_first->Flush();
        m_pCh_first->WaitIdle(m_TimeoutMs);

        deferredParams.hApiHandle = apiHandle++;
        deferredParams.cmd = LW2080_CTRL_CMD_GR_CTXSW_PREEMPTION_BIND;
        deferredParams.flags = DRF_DEF(5080_CTRL_CMD, _DEFERRED_API_FLAGS, _DELETE, _EXPLICIT);
        deferredParams.api_bundle.PreemptionCtxsw.hClient = m_hClient;
        deferredParams.api_bundle.PreemptionCtxsw.hChannel = m_hCh;
        deferredParams.api_bundle.PreemptionCtxsw.vMemPtrs[LW2080_CTRL_CMD_GR_CTXSW_PREEMPTION_BIND_BUFFERS_CONTEXT_POOL] = m_gpuPoolAddr;
        deferredParams.api_bundle.PreemptionCtxsw.vMemPtrs[LW2080_CTRL_CMD_GR_CTXSW_PREEMPTION_BIND_BUFFERS_CONTEXT_POOL_CONTROL] = m_gpuPoolCtrlBlockAddr;
        deferredParams.api_bundle.PreemptionCtxsw.vMemPtrs[LW2080_CTRL_CMD_GR_CTXSW_PREEMPTION_BIND_BUFFERS_CONTEXT_POOL_CONTROL_CPU] = (LwU64)m_poolCtrlBlockCpuAddr;
        deferredParams.api_bundle.PreemptionCtxsw.gfxpPreemptMode = LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_GFX_GFXP_POOL;
        deferredParams.api_bundle.PreemptionCtxsw.flags =
            FLD_SET_DRF(2080, _CTRL_GR_SET_CTXSW_PREEMPTION_MODE_FLAGS, _GFXP, _SET, 0);

        CHECK_RC_CLEANUP(pLwRm->Control(m_hObj5080, LW5080_CTRL_CMD_DEFERRED_API,
                                        (void*)&deferredParams, sizeof(deferredParams)));

        // Trigger the deferred API bundle via the pushbuffer calls
        m_pCh_first->Write(SW_OBJ_SUBCH, LW5080_DEFERRED_API, deferredParams.hApiHandle);
        m_pCh_first->Flush();
        m_pCh_first->WaitIdle(m_TimeoutMs);

        rmDeferredParams.hApiHandle = deferredParams.hApiHandle;
        CHECK_RC_CLEANUP(pLwRm->Control(m_hObj5080, LW5080_CTRL_CMD_REMOVE_API,
                                (void*)&rmDeferredParams, sizeof(rmDeferredParams)));
    }

    Printf(Tee::PriLow,"GpuDeferredApiTest: Send GPU_INITIALIZE_CTX via deferred channel api...\n");

    //
    // 2) let's give them an engine context
    //
    // Setup the deferred api bundle
    deferredParams.cmd = LW2080_CTRL_CMD_GPU_INITIALIZE_CTX;
    deferredParams.hApiHandle = apiHandle++;
    deferredParams.flags = DRF_DEF(5080_CTRL_CMD, _DEFERRED_API_FLAGS, _DELETE, _EXPLICIT);
    deferredParams.api_bundle.InitCtx.engineType  = LW2080_ENGINE_TYPE_GRAPHICS;
    deferredParams.api_bundle.InitCtx.hClient     = m_hClient;
    deferredParams.api_bundle.InitCtx.hObject     = m_hCh;
    deferredParams.api_bundle.InitCtx.hChanClient = m_hClient;
    deferredParams.api_bundle.InitCtx.hVirtMemory = m_hVirtMem;
    deferredParams.api_bundle.InitCtx.physAddress = m_gpuPhysOffset;
    deferredParams.api_bundle.InitCtx.physAttr = DRF_DEF(2080, _CTRL_GPU_INITIALIZE_CTX, _APERTURE, _VIDMEM) |
                                                 DRF_DEF(2080, _CTRL_GPU_INITIALIZE_CTX, _GPU_CACHEABLE, _YES); // Fermi VID is always GpuCached
    deferredParams.api_bundle.InitCtx.hDmaHandle  = 0;
    deferredParams.api_bundle.InitCtx.index       = 1;

    CHECK_RC_CLEANUP(pLwRm->Control(m_hObj5080, LW5080_CTRL_CMD_DEFERRED_API,
                            (void*)&deferredParams, sizeof(deferredParams)));

    // Trigger the deferred API bundle via the pushbuffer calls
    m_pCh_first->Write(SW_OBJ_SUBCH, LW5080_DEFERRED_API, deferredParams.hApiHandle);
    m_pCh_first->Flush();
    m_pCh_first->WaitIdle(m_TimeoutMs);

    rmDeferredParams.hApiHandle = deferredParams.hApiHandle;
    CHECK_RC_CLEANUP(pLwRm->Control(m_hObj5080, LW5080_CTRL_CMD_REMOVE_API,
                            (void*)&rmDeferredParams, sizeof(rmDeferredParams)));

    Printf(Tee::PriLow,"GpuDeferredApiTest: Send GPU_PROMOTE_CTX via deferred channel api...\n");

    // Setup the deferred api bundle
    deferredParams.hApiHandle = apiHandle++;
    deferredParams.cmd        = LW2080_CTRL_CMD_GPU_PROMOTE_CTX;
    deferredParams.flags = DRF_DEF(5080_CTRL_CMD, _DEFERRED_API_FLAGS, _DELETE, _EXPLICIT);
    deferredParams.api_bundle.PromoteCtx.engineType = LW2080_ENGINE_TYPE_GRAPHICS;
    deferredParams.api_bundle.PromoteCtx.hClient    = m_hClient;
    deferredParams.api_bundle.PromoteCtx.hObject   = m_hCh;
    deferredParams.api_bundle.PromoteCtx.hChanClient = m_hClient;
    deferredParams.api_bundle.PromoteCtx.hVirtMemory = m_hVirtMem;

    CHECK_RC_CLEANUP(pLwRm->Control(m_hObj5080, LW5080_CTRL_CMD_DEFERRED_API,
                            (void*)&deferredParams, sizeof(deferredParams)));

    // Trigger the deferred API bundle via the pushbuffer calls
    m_pCh_first->Write(SW_OBJ_SUBCH, LW5080_DEFERRED_API, deferredParams.hApiHandle);
    m_pCh_first->Flush();
    m_pCh_first->WaitIdle(m_TimeoutMs);

    rmDeferredParams.hApiHandle = deferredParams.hApiHandle;
    CHECK_RC_CLEANUP(pLwRm->Control(m_hObj5080, LW5080_CTRL_CMD_REMOVE_API,
                            (void*)&rmDeferredParams, sizeof(rmDeferredParams)));

    Printf(Tee::PriLow,"GpuDeferredApiTest: After GPU_PROMOTE_CTX should have a valid contetx, chk it...\n");

    //
    // 3) Now check if we have a context buffer
    //
    chkEngCtxParams.hChannel = m_hCh;
    chkEngCtxParams.engine   = LW2080_ENGINE_TYPE_GRAPHICS;
    CHECK_RC(LwRmControl(m_hClient, m_hSubDevDiag,
                            LW208F_CTRL_CMD_FIFO_CHECK_ENGINE_CONTEXT,
                            (void*)&chkEngCtxParams,
                            sizeof(chkEngCtxParams)));

    if(chkEngCtxParams.exists)
    {
        Printf(Tee::PriNormal,
               "GpuDeferredApiTest: PASS: Channel has gr context as expected\n");
    }
    else
    {
        Printf(Tee::PriNormal,
               "GpuDeferredApiTest: ERROR: Channel does not have gr context!\n");
        rc = RC::SOFTWARE_ERROR;
        return rc;
    }

    //
    // 4) Let's do some work
    //
    // TODO!!
    Printf(Tee::PriNormal,
            "GpuDeferredApiTest: TODO: We need to exercise the channel somehow\n");

    Printf(Tee::PriLow,"GpuDeferredApiTest: Send GPU_EVICT_CTX via deferred channel api...\n");

    // Setup the evict deferred api bundle
    deferredParams.hApiHandle = apiHandle++;
    deferredParams.cmd        = LW2080_CTRL_CMD_GPU_EVICT_CTX;
    deferredParams.flags = DRF_DEF(5080_CTRL_CMD, _DEFERRED_API_FLAGS, _DELETE, _EXPLICIT);
    deferredParams.api_bundle.EvictCtx.engineType = LW2080_ENGINE_TYPE_GRAPHICS;
    deferredParams.api_bundle.EvictCtx.hClient       = m_hClient;
    deferredParams.api_bundle.EvictCtx.hObject       = m_hCh;
    deferredParams.api_bundle.PromoteCtx.hChanClient = m_hClient;

    CHECK_RC_CLEANUP(pLwRm->Control(m_hObj5080, LW5080_CTRL_CMD_DEFERRED_API,
                            (void*)&deferredParams, sizeof(deferredParams)));

    // Trigger the deferred API bundle via the pushbuffer calls
    // m_pCh_first->SetObject(0, m_hObj5080);
    m_pCh_first->Write(SW_OBJ_SUBCH, LW5080_DEFERRED_API, deferredParams.hApiHandle);
    m_pCh_first->Flush();
    m_pCh_first->WaitIdle(m_TimeoutMs);

    rmDeferredParams.hApiHandle = deferredParams.hApiHandle;
    CHECK_RC_CLEANUP(pLwRm->Control(m_hObj5080, LW5080_CTRL_CMD_REMOVE_API,
                            (void*)&rmDeferredParams, sizeof(rmDeferredParams)));

    Printf(Tee::PriLow,"GpuDeferredApiTest: After GPU_EVICT_CTX should NOT have a valid contetx, chk it...\n");

    //
    // 6) Check if we are missing the context buffer; we should be
    //
    chkEngCtxParams.hChannel = m_hCh;
    chkEngCtxParams.engine   = LW2080_ENGINE_TYPE_GRAPHICS;
    CHECK_RC(LwRmControl(m_hClient, m_hSubDevDiag,
                            LW208F_CTRL_CMD_FIFO_CHECK_ENGINE_CONTEXT,
                            (void*)&chkEngCtxParams,
                            sizeof(chkEngCtxParams)));

    if(chkEngCtxParams.exists)
    {
        Printf(Tee::PriNormal,
               "GpuDeferredApiTest: ERROR: Channel has gr context when it shouldn't!\n");
        rc = RC::SOFTWARE_ERROR;
        return rc;
    }
    else
    {
        Printf(Tee::PriNormal,
               "GpuDeferredApiTest: PASS: Channel does not have gr context as expected\n");
    }

Cleanup:

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! Freeing the channel and the software object that we allocated in Setup().
//------------------------------------------------------------------------------
RC GpuDeferredApiTest::Cleanup()
{
    RC rc, firstRc;
    LwRmPtr pLwRm;

    pLwRm->Free(m_hObj);
    pLwRm->Free(m_hObjGr2d);
    pLwRm->Free(m_hObj5080);

    m_3dAlloc_gr.Free();
    m_3dAlloc_first.Free();

    pLwRm->UnmapMemory(m_hSemMem, m_pSem, 0, GetBoundGpuSubdevice());
    pLwRm->Free(m_hSemCtxDma);
    pLwRm->Free(m_hSemMem);

    FIRST_RC(m_TestConfig.FreeChannel(m_pCh));
    FIRST_RC(m_TestConfig.FreeChannel(m_pCh_first));

    pLwRm->UnmapMemoryDma(m_hVirtMem, m_hPhysMem,
                          LW04_MAP_MEMORY_FLAGS_NONE, m_gpuAddr, GetBoundGpuDevice());

    pLwRm->UnmapMemoryDma(m_hZlwllVirtMem, m_hZlwllPhysMem,
                          LW04_MAP_MEMORY_FLAGS_NONE, m_gpuZlwllAddr, GetBoundGpuDevice());

    pLwRm->Free(m_hVirtMem);
    pLwRm->Free(m_hPhysMem);

    pLwRm->Free(m_hZlwllVirtMem);
    pLwRm->Free(m_hZlwllPhysMem);

    if(m_gpuFamily >= GpuDevice::Pascal)
    {
        pLwRm->UnmapMemoryDma(m_hPreemptVirtMem, m_hPreemptPhysMem,
                              LW04_MAP_MEMORY_FLAGS_NONE, m_gpuPreemptAddr, GetBoundGpuDevice());
        pLwRm->Free(m_hPreemptVirtMem);
        pLwRm->Free(m_hPreemptPhysMem);

        pLwRm->UnmapMemoryDma(m_hSpillVirtMem, m_hSpillPhysMem,
                              LW04_MAP_MEMORY_FLAGS_NONE, m_gpuSpillAddr, GetBoundGpuDevice());
        pLwRm->Free(m_hSpillVirtMem);
        pLwRm->Free(m_hSpillPhysMem);

        pLwRm->UnmapMemoryDma(m_hPagepoolVirtMem, m_hPagepoolPhysMem,
                              LW04_MAP_MEMORY_FLAGS_NONE, m_gpuPagepoolAddr, GetBoundGpuDevice());
        pLwRm->Free(m_hPagepoolVirtMem);
        pLwRm->Free(m_hPagepoolPhysMem);

        pLwRm->UnmapMemoryDma(m_hBetacbVirtMem, m_hBetacbPhysMem,
                              LW04_MAP_MEMORY_FLAGS_NONE, m_gpuBetacbAddr, GetBoundGpuDevice());
        pLwRm->Free(m_hBetacbVirtMem);
        pLwRm->Free(m_hBetacbPhysMem);

        if (m_supportsRtv)
        {
            pLwRm->UnmapMemoryDma(m_hRtvVirtMem, m_hRtvPhysMem,
                                  LW04_MAP_MEMORY_FLAGS_NONE, m_gpuRtvAddr, GetBoundGpuDevice());
            pLwRm->Free(m_hRtvVirtMem);
            pLwRm->Free(m_hRtvPhysMem);
        }

        // cleanup GfxP Pool memory
        pLwRm->UnmapMemoryDma(m_hPoolVirtMem, m_hPoolPhysMem,
                              LW04_MAP_MEMORY_FLAGS_NONE, m_gpuPoolAddr, GetBoundGpuDevice());
        pLwRm->Free(m_hPoolVirtMem);
        pLwRm->Free(m_hPoolPhysMem);

        pLwRm->UnmapMemoryDma(m_hPoolCtrlBlockVirtMem, m_hPoolCtrlBlockPhysMem,
                              LW04_MAP_MEMORY_FLAGS_NONE, m_gpuPoolCtrlBlockAddr, GetBoundGpuDevice());
        pLwRm->UnmapMemory(m_hPoolCtrlBlockPhysMem, m_poolCtrlBlockCpuAddr, 0, GetBoundGpuSubdevice());
        pLwRm->Free(m_hPoolCtrlBlockVirtMem);
        pLwRm->Free(m_hPoolCtrlBlockPhysMem);
    }

    return firstRc;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//! \brief AllocMemory()
//!
//! This function allocates the memory as per the input params type,
//! flags, attr, attr2, memsize.
//! Also verifies cacheable bit in the return params : what we
//! requested sysmem/vidmem , caching required/not required/default
//! against what was actually allocated.
//! If allocation successful, *(pHandle) will contain the handle to the
//! allocated space for callers use.
//!
//! \return OK if allocation and verification of cacheable bit passed, specific
//! RC if allocation failed,and RC::PARAMETER_MISMATCH if allocation successful
//! but cacheable bit not in accordance of what it should be.
//! \sa BasicApiTest()
//-----------------------------------------------------------------------------
RC GpuDeferredApiTest::AllocMemory
(
    UINT32            type,
    UINT32            flags,
    UINT32            attr,
    UINT32            attr2,
    UINT64            memSize,
    UINT32          * pHandle
)
{
    RC                rc;
    LwRmPtr           pLwRm;
    UINT32            attrSaved, attr2Saved;
    UINT32            requestedCacheable, allocatedCacheable;

    //
    // Save attr and attr2 before trying allocation as we have to pass ptr of
    // attr and attr2 for allocation and the values change to what gets allocated.
    //
    attrSaved       = attr;
    attr2Saved      = attr2;

    // Try to allocate what caller requested.
    CHECK_RC(pLwRm->VidHeapAllocSizeEx(type,   // type
                                       flags,  // flags
                                       &attr,  // pAttr
                                       &attr2, // pAttr2
                                       memSize,// Size
                                       1,      // alignment
                                       NULL,   // pFormat
                                       NULL,   // pCoverage
                                       NULL,   // pPartitionStride
                                       pHandle,// pHandle
                                       NULL,   // poffset,
                                       NULL,   // pLimit
                                       NULL,   // pRtnFree
                                       NULL,   // pRtnTotal
                                       0,      // width
                                       0,      // height
                                       GetBoundGpuDevice()));

    // Now verify the GPU CACHEABLE bit in return params.

    // extract what we requested for GPU_CACHEABLE_BIT
    requestedCacheable = DRF_VAL(OS32, _ATTR2, _GPU_CACHEABLE, attr2Saved);

    // extract what got allocated for GPU_CACHEABLE_BIT
    allocatedCacheable = DRF_VAL(OS32, _ATTR2, _GPU_CACHEABLE, attr2);

    // if vidmem was requested
    if (DRF_VAL(OS32, _ATTR, _LOCATION, attrSaved) == LWOS32_ATTR_LOCATION_VIDMEM)
    {
        // if CACHEABLE_YES got allocated, check whether we requested for it.
        if (allocatedCacheable == LWOS32_ATTR2_GPU_CACHEABLE_YES)
        {
            // for vidmem, CACHEABLE_YES is allocated by default or if we ask for it.
            if ( requestedCacheable == LWOS32_ATTR2_GPU_CACHEABLE_YES ||
                 requestedCacheable == LWOS32_ATTR2_GPU_CACHEABLE_DEFAULT )
            {
                // Everything is fine, print we allocated cacheable vidmem
                Printf(Tee::PriLow,"%d:GpuDeferredApiTest::Allocated cacheable vidmem.\n",__LINE__);
            }
            else
            {
                //
                // We have hit this else, so CACHEABLE_YES got allocated even
                // though we asked not to. So print error and return.
                //
                Printf(Tee::PriHigh,"%d:GpuDeferredApiTest::Error : Return parameters mismatch with requested params,\
                                     Requested %s allocation for vidmem, got %s.\n",
                       __LINE__, "GPU_CACHEABLE_NO", "GPU_CACHEABLE_YES");
                CHECK_RC_CLEANUP(RC::PARAMETER_MISMATCH);
            }
        }
        // if CACHEABLE_NO got allocated, check what we had requested.
        else if (allocatedCacheable == LWOS32_ATTR2_GPU_CACHEABLE_NO)
        {
            if (requestedCacheable == LWOS32_ATTR2_GPU_CACHEABLE_NO)
            {
                //
                // Everything is fine, we had requested CACHEABLE_NO and thats
                // what got allocated.
                //
                Printf(Tee::PriLow,"%d:GpuDeferredApiTest::Allocated non-cacheable vidmem.\n",__LINE__);
            }
            else
            {
                //
                // We have hit this else, so CACHEABLE_NO got allocated even
                // though we asked for CACHEABLE_YES or CACHEABLE_DEFAULT
                // _DEFAULT translates to _YES for vidmem.
                //
                Printf(Tee::PriHigh,"%d:GpuDeferredApiTest::Error : Return parameters mismatch with requested params,\
                                     Requested %s allocation for vidmem, got %s.\n",
                       __LINE__,
                       requestedCacheable == LWOS32_ATTR2_GPU_CACHEABLE_YES ? "GPU_CACHEABLE_YES" : "GPU_CACHEABLE_DEFAULT",
                       "GPU_CACHEABLE_NO");
                CHECK_RC_CLEANUP(RC::PARAMETER_MISMATCH);
            }
        }
        else
        {
            //
            // Hitting this else, that means the API returned _CACHEABLE_DEFAULT.
            // The API should only return CACHEABLE_YES or CACHEABLE_NO to indicate
            // what was allocated. _DEFAULT should never be returned. So print error.
            //
            Printf(Tee::PriHigh,"%d:GpuDeferredApiTest::Error : Got GPU_CACHEABLE_DEFAULT in return params,\
                                 it should be either GPU_CACHEABLE_YES or GPU_CACHEABLE_NO.\n",
                   __LINE__);
            CHECK_RC_CLEANUP(RC::PARAMETER_MISMATCH);

        }
    }
    //
    // if _LOCATION is not vidmem, the rest all(_PCI/AGP/ANY) translate to sysmem.
    // so go down the else path and verify cacheable bit according to what should
    // be allocated for sysmem.
    //
    else
    {
        // if CACHEABLE_YES got allocated, check whether we requested for it.
        if (allocatedCacheable == LWOS32_ATTR2_GPU_CACHEABLE_YES)
        {
            //
            // for sysmem, CACHEABLE mem is only allocated if we specifically ask for it.
            // CACHEABLE_DEFAULT translates to CACHEABLE_NO for sysmem.
            //
            if (requestedCacheable == LWOS32_ATTR2_GPU_CACHEABLE_YES)
            {
                // Everything is fine, we got CACHEABLE_YES since we asked for it.
                Printf(Tee::PriLow,"%d:GpuDeferredApiTest::Allocated cacheable sysmem.\n",__LINE__);
            }
            else
            {
                //
                // We have hit this else, so CACHEABLE_YES got allocated even though we asked
                // for _DEFAULT or _NO. _DEFAULT should translate to _NO for sysmem.
                // So print error and return.
                //
                Printf(Tee::PriHigh,"%d:GpuDeferredApiTest::Error : Return parameters mismatch with requested params,\
                                     Requested %s allocation for sysmem, got %s.\n",
                       __LINE__,
                       requestedCacheable == LWOS32_ATTR2_GPU_CACHEABLE_NO ? "GPU_CACHEABLE_NO" : "GPU_CACHEABLE_DEFAULT",
                       "GPU_CACHEABLE_YES");
                CHECK_RC_CLEANUP(RC::PARAMETER_MISMATCH);
            }
        }
        // if CACHEABLE_NO got allocated, check what we had requested.
        else if (allocatedCacheable == LWOS32_ATTR2_GPU_CACHEABLE_NO)
        {
            // if CACHEABLE_NO or CACHEABLE_DEFAULT was requested.
            if (requestedCacheable == LWOS32_ATTR2_GPU_CACHEABLE_NO ||
                requestedCacheable == LWOS32_ATTR2_GPU_CACHEABLE_DEFAULT)
            {
                // Everything is fine, we got CACHEABLE_NO because we asked for _NO or _DEFAULT.
                Printf(Tee::PriLow,"%d:GpuDeferredApiTest::Allocated non-cacheable sysmem.\n",__LINE__);
            }
            else
            {
                //
                // We have hit this else, so CACHEABLE_NO got allocated even though we asked
                // for CACHEABLE_YES. So print error and return.
                //
                Printf(Tee::PriHigh,"%d:GpuDeferredApiTest::Error : Return parameters mismatch with requested params,\
                                     Requested %s allocation for sysmem, got %s.\n",
                       __LINE__,
                       "GPU_CACHEABLE_YES",
                       "GPU_CACHEABLE_NO");
                CHECK_RC_CLEANUP(RC::PARAMETER_MISMATCH);
            }
        }
        else
        {
            //
            // Hitting this else, that means the API returned _CACHEABLE_DEFAULT.
            // The API should only return CACHEABLE_YES or CACHEABLE_NO to indicate
            // what was allocated. _DEFAULT should never be returned. So print error.
            //
            Printf(Tee::PriHigh,"%d:GpuDeferredApiTest::Error : Got GPU_CACHEABLE_DEFAULT in return params,\
                                 it should be either GPU_CACHEABLE_YES or GPU_CACHEABLE_NO.\n",
                   __LINE__);
            CHECK_RC_CLEANUP(RC::PARAMETER_MISMATCH);
        }
    }
    return rc;

Cleanup:
    pLwRm->Free(*pHandle);
    *pHandle = 0;
    return rc;

}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! SSet up a JavaScript class that creates and owns a C++ GpuDeferredApiTest object
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(GpuDeferredApiTest, RmTest,
    "GpuDeferredApiTest RMTEST that tests GLW5080_CTRL_CMD_DEFERRED_API");

