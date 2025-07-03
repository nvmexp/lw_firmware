/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2013,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// virtual context test.
//

#include "lwos.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "lwRmApi.h"
#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"
#include "class/cl9097.h" // FERMI_A
#include "class/cl906f.h"
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl208f.h"
#include "fermi/gf100/dev_ram.h"

#include "gpu/tests/rmtest.h"
#include "gpu/include/gralloc.h"
#include "core/include/memcheck.h"

class VirtualContextTest: public RmTest
{
public:
    VirtualContextTest();
    virtual ~VirtualContextTest();

    virtual string IsTestSupported();

    virtual RC   Setup();
    virtual RC   Run();
    virtual RC   Cleanup();

private:
    UINT64       m_gpuAddr;

    LwRm::Handle m_hVirtMem;
    LwRm::Handle m_hPhysMem;
    LwRm::Handle m_hClient;
    LwRm::Handle m_hDevice;
    LwRm::Handle m_hSubDev;
    LwRm::Handle m_hSubDevDiag;

    UINT64       m_gpuPhysOffest;
    UINT64       m_gpuVirtOffest;

    FLOAT64      m_TimeoutMs;

    LwRm::Handle m_hCh_first;
    Channel*     m_pCh_first;
    ThreeDAlloc  m_3dAlloc;
    ThreeDAlloc  m_3dAlloc_first;
};

//------------------------------------------------------------------------------
VirtualContextTest::VirtualContextTest() :
    m_gpuAddr(0LL),
    m_hVirtMem(0),
    m_hPhysMem(0),
    m_hClient(0),
    m_hDevice(0),
    m_hSubDev(0),
    m_hSubDevDiag(0),
    m_gpuPhysOffest(0),
    m_gpuVirtOffest(0),
    m_TimeoutMs(0),
    m_hCh_first(0),
    m_pCh_first(nullptr)
{
    SetName("VirtualContextTest");
    m_3dAlloc.SetOldestSupportedClass(FERMI_A);
    m_3dAlloc_first.SetOldestSupportedClass(FERMI_A);
}

//------------------------------------------------------------------------------
VirtualContextTest::~VirtualContextTest()
{
}

//------------------------------------------------------------------------
string VirtualContextTest::IsTestSupported()
{
    if(m_3dAlloc.IsSupported(GetBoundGpuDevice()))
        return RUN_RMTEST_TRUE;
    return "Supported only on Fermi+";
}

//------------------------------------------------------------------------------
RC VirtualContextTest::Setup()
{
    LwRmPtr      pLwRm;
    RC           rc;
    LW208F_CTRL_FIFO_ENABLE_VIRTUAL_CONTEXT_PARAMS vcParams = {0};

    CHECK_RC(InitFromJs());

    m_TimeoutMs = m_TestConfig.TimeoutMs();
    m_TestConfig.SetAllowMultipleChannels(true);
    m_hClient = pLwRm->GetClientHandle();
    m_hDevice = pLwRm->GetDeviceHandle(GetBoundGpuDevice());
    m_hSubDev = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    m_hSubDevDiag = GetBoundGpuSubdevice()->GetSubdeviceDiag();

    // create regular channel
    Printf(Tee::PriNormal,
           "VirtualContextTest: CREATING REGULAR CHANNEL\n");
    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh_first, &m_hCh_first, LW2080_ENGINE_TYPE_GR(0)));
    CHECK_RC(m_3dAlloc_first.Alloc(m_hCh_first, GetBoundGpuDevice()));

    // need to create channel with virtual context flag
    Printf(Tee::PriNormal,
           "VirtualContextTest: CREATING VIRTUAL-CONTEXT CHANNEL\n");
    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_GR(0)));

    // enable virtual context
    vcParams.hChannel = m_hCh;
    CHECK_RC(LwRmControl(m_hClient, m_hSubDevDiag,
                            LW208F_CTRL_CMD_FIFO_ENABLE_VIRTUAL_CONTEXT,
                            (void*)&vcParams,
                            sizeof(vcParams)));

    CHECK_RC(m_3dAlloc.Alloc(m_hCh, GetBoundGpuDevice()));

    return rc;
}

//------------------------------------------------------------------------
RC VirtualContextTest::Run()
{
    RC      rc;
    LwRmPtr pLwRm;
    m_hSubDev = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    m_hClient = pLwRm->GetClientHandle();

    LW2080_CTRL_GPU_INITIALIZE_CTX_PARAMS          initCtx = {0};
    LW2080_CTRL_GPU_PROMOTE_CTX_PARAMS             promoteCtx = {0};
    LW2080_CTRL_GPU_EVICT_CTX_PARAMS               evictCtx = {0};
    LW208F_CTRL_FIFO_CHECK_ENGINE_CONTEXT_PARAMS   chkEngCtxParams = {0};
    LW0080_CTRL_FIFO_GET_ENGINE_CONTEXT_PROPERTIES_PARAMS ctxPropParams = {0};

    //
    // 1) make sure we have no gr context
    //
    {
        chkEngCtxParams.hChannel = m_hCh;
        chkEngCtxParams.engine   = LW2080_ENGINE_TYPE_GRAPHICS;
        CHECK_RC(LwRmControl(m_hClient, m_hSubDevDiag,
                                LW208F_CTRL_CMD_FIFO_CHECK_ENGINE_CONTEXT,
                                (void*)&chkEngCtxParams,
                                sizeof(chkEngCtxParams)));

        if(chkEngCtxParams.exists)
        {
            Printf(Tee::PriNormal,
                   "VirtualContextTest: ERROR: Channel already has context buffer\n");
            rc = RC::SOFTWARE_ERROR;
            return rc;
        }
        else
        {
            Printf(Tee::PriNormal,
                   "VirtualContextTest: PASS: Channel does not have gr context as expected\n");
        }
    }

    //
    // 2) let's give them an engine context
    //
    {
        //UINT64       dynMemLimit;
        //void         *pDummy;
        UINT32       attr;
        UINT32       attr2;
        ctxPropParams.engineId = LW0080_CTRL_FIFO_GET_ENGINE_CONTEXT_PROPERTIES_ENGINE_ID_GRAPHICS;
        // find out the size of gr context buffer
        CHECK_RC(pLwRm->Control(m_hDevice,
                                LW0080_CTRL_CMD_FIFO_GET_ENGINE_CONTEXT_PROPERTIES,
                                (void*)&ctxPropParams,
                                sizeof(ctxPropParams)));
        //
        // Allocate the GPU virtual address
        //
        attr = (DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH) |
                DRF_DEF(OS32, _ATTR, _DEPTH, _32) |
                DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1) |
                DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _DEFAULT));
        attr2 = LWOS32_ATTR2_NONE;
        CHECK_RC(pLwRm->VidHeapAllocSizeEx(LWOS32_TYPE_IMAGE,
                                           LWOS32_ALLOC_FLAGS_VIRTUAL,
                                           &attr,
                                           &attr2, // pAttr2
                                           ctxPropParams.size,
                                           1,    // alignment
                                           NULL, // pFormat
                                           NULL, // pCoverage
                                           NULL, // pPartitionStride
                                           &m_hVirtMem,
                                           &m_gpuVirtOffest, // poffset,
                                           NULL, // pLimit
                                           NULL, // pRtnFree
                                           NULL, // pRtnTotal
                                           0,    // width
                                           0,    // height
                                           GetBoundGpuDevice()));

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
                DRF_DEF(OS32, _ATTR, _COHERENCY, _CACHED) |
                DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _4KB) |
                DRF_DEF(OS32, _ATTR, _PHYSICALITY, _CONTIGUOUS));
        attr2 = LWOS32_ATTR2_NONE;
        CHECK_RC(pLwRm->VidHeapAllocSizeEx(LWOS32_TYPE_IMAGE,
                                           LWOS32_ALLOC_FLAGS_NO_SCANOUT |
                                           LWOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED,
                                           &attr,
                                           &attr2, //pAttr2
                                           ctxPropParams.size,
                                           1,    // alignment
                                           NULL, // pFormat
                                           NULL, // pCoverage
                                           NULL, // pPartitionStride
                                           &m_hPhysMem,
                                           &m_gpuPhysOffest, // poffset,
                                           NULL, // pLimit
                                           NULL, // pRtnFree
                                           NULL, // pRtnTotal
                                           0,    // width
                                           0,    // height
                                           GetBoundGpuDevice()));

        //
        // Now map the GPU virtual address to the physical address
        //
        CHECK_RC(pLwRm->MapMemoryDma(m_hVirtMem, m_hPhysMem, 0, ctxPropParams.size,
                                     DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE) |
                                     DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _ENABLE),
                                     &m_gpuAddr, GetBoundGpuDevice()));

        // Initialize vc
        initCtx.engineType  = LW2080_ENGINE_TYPE_GRAPHICS;
        initCtx.hClient     = m_hClient;
        initCtx.hChanClient = m_hClient;
        initCtx.hObject     = m_hCh;
        initCtx.hVirtMemory = m_hVirtMem;
        initCtx.physAddress = m_gpuPhysOffest;
        initCtx.physAttr    = DRF_DEF(2080, _CTRL_GPU_INITIALIZE_CTX, _APERTURE, _VIDMEM) |
                              DRF_DEF(2080, _CTRL_GPU_INITIALIZE_CTX, _GPU_CACHEABLE, _YES); // Fermi VID is always GpuCached
        initCtx.hDmaHandle  = 0;
        CHECK_RC(pLwRm->Control(m_hSubDev,
                                LW2080_CTRL_CMD_GPU_INITIALIZE_CTX,
                                (void*)&initCtx,
                                sizeof(initCtx)));

        // Promote vc
        promoteCtx.hChanClient = m_hClient;
        promoteCtx.hObject     = m_hCh;
        promoteCtx.engineType  = LW2080_ENGINE_TYPE_GRAPHICS;
        promoteCtx.hClient     = m_hClient;
        promoteCtx.hVirtMemory = m_hVirtMem;
        CHECK_RC(pLwRm->Control(m_hSubDev,
                                LW2080_CTRL_CMD_GPU_PROMOTE_CTX,
                                (void*)&promoteCtx,
                                sizeof(promoteCtx)));

    }

    //
    // 3) Now check if we have a context buffer
    //
    {
        chkEngCtxParams.hChannel = m_hCh;
        chkEngCtxParams.engine   = LW2080_ENGINE_TYPE_GRAPHICS;
        CHECK_RC(LwRmControl(m_hClient, m_hSubDevDiag,
                                LW208F_CTRL_CMD_FIFO_CHECK_ENGINE_CONTEXT,
                                (void*)&chkEngCtxParams,
                                sizeof(chkEngCtxParams)));

        if(chkEngCtxParams.exists)
        {
            Printf(Tee::PriNormal,
                   "VirtualContextTest: PASS: Channel has gr context as expected\n");
        }
        else
        {
            Printf(Tee::PriNormal,
                   "VirtualContextTest: ERROR: Channel does not have gr context!\n");
            rc = RC::SOFTWARE_ERROR;
            return rc;
        }
    }

    //
    // 4) Let's do some work
    //
    {
        // TODO!!
        Printf(Tee::PriNormal,
                "VirtualContextTest: TODO: We need to exercise the channel somehow\n");
    }

    //
    // 5) Evict the context
    //
    {
        evictCtx.engineType  = LW2080_ENGINE_TYPE_GRAPHICS;
        evictCtx.hClient     = m_hClient;
        evictCtx.hChanClient = m_hClient;
        evictCtx.hObject     = m_hCh;

        CHECK_RC(pLwRm->Control(m_hSubDev,
                                LW2080_CTRL_CMD_GPU_EVICT_CTX,
                                (void*)&evictCtx,
                                sizeof(evictCtx)));

    }

    //
    // 6) Check if we are missing the context buffer; we should be
    //
    {
        chkEngCtxParams.hChannel = m_hCh;
        chkEngCtxParams.engine   = LW2080_ENGINE_TYPE_GRAPHICS;
        CHECK_RC(LwRmControl(m_hClient, m_hSubDevDiag,
                                LW208F_CTRL_CMD_FIFO_CHECK_ENGINE_CONTEXT,
                                (void*)&chkEngCtxParams,
                                sizeof(chkEngCtxParams)));

        if(chkEngCtxParams.exists)
        {
            Printf(Tee::PriNormal,
                   "VirtualContextTest: ERROR: Channel has gr context when it shouldn't!\n");
            rc = RC::SOFTWARE_ERROR;
            return rc;
        }
        else
        {
            Printf(Tee::PriNormal,
                   "VirtualContextTest: PASS: Channel does not have gr context as expected\n");
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC VirtualContextTest::Cleanup()
{
    LwRmPtr pLwRm;
    RC      rc;

    pLwRm->UnmapMemoryDma(m_hVirtMem, m_hPhysMem,
                          LW04_MAP_MEMORY_FLAGS_NONE, m_gpuAddr, GetBoundGpuDevice());

    pLwRm->Free(m_hVirtMem);
    pLwRm->Free(m_hPhysMem);
    m_3dAlloc.Free();
    m_3dAlloc_first.Free();

    m_TestConfig.FreeChannel(m_pCh);
    m_TestConfig.FreeChannel(m_pCh_first);

    return rc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ VirtualContextTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(VirtualContextTest, RmTest,
                 "Virtual Context RM test.");
