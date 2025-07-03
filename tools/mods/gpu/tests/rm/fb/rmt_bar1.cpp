/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// This test try to Create Cpu visible FB segment and then set the surface's
// parameter for remapper.
//

#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "lwos.h"
#include "lwRmApi.h"
#include "gpu/include/gralloc.h"
#include "class/cl003e.h" // LW01_MEMORY_SYSTEM
#include "class/cl0040.h" // LW01_MEMORY_LOCAL_USER
#include "class/cl0070.h" // LW01_MEMORY_VIRTUAL
#include "class/cl50a0.h" // LW50_MEMORY_VIRTUAL
#include "class/cl902d.h" // FERMI_TWOD_A
#include "class/cl907f.h" // GF100_REMAPPER
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl208f.h"
#include "ctrl/ctrl907f.h"
#include "core/utility/errloggr.h"
#include "core/include/channel.h"
#include "core/include/memcheck.h"

#define RM_PAGE_SIZE   0x1000

#define MEM_SIZE       0x10000
#define PTE_MEM_SIZE   0x2000

#define MEM_HANDLE     0xba40001

class ClassBar1Test : public RmTest
{
public:
    ClassBar1Test();
    virtual ~ClassBar1Test();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    LwRm::Handle                   m_objectHandle;
    LwRm::Handle                   m_remapperObj;
    LwRm::Handle                   m_hVA;
    LwRm::Handle                   m_sysMemHandle;
    LwRm::Handle                   m_pteMemHandle;
    LwRm::Handle                   m_pteDmaHandle;
    Channel *                      m_pCh;
    TwodAlloc                      m_TwoD;
    bool                           m_bIsBar1Virtual;

    RC allocObject();
    RC freeObject();
    RC alignedContiguousTest();
    RC unalignedContiguousTest();
    RC alignedNonContiguousTest();
    RC unalignedNonContiguousTest();
    RC remapperTest();
    RC testMem(LW0080_CTRL_FB_CREATE_FB_SEGMENT_PARAMS *pCreateParams);
};

//! \brief ClassBar1Test constructor.
//-----------------------------------------------------------------------------
ClassBar1Test::ClassBar1Test()
{
    SetName("ClassBar1Test");
    m_objectHandle = 0;
    m_remapperObj = 0;
    m_hVA = 0;
    m_sysMemHandle = 0;
    m_pteMemHandle = 0;
    m_pteDmaHandle = 0;
    m_pCh = 0;
    m_bIsBar1Virtual = false;
}

//! \brief ClassBar1Test destructor.
//-----------------------------------------------------------------------------
ClassBar1Test::~ClassBar1Test()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \sa IsTestSupported
//! \return true if virtual memory and remappers are supported
//-----------------------------------------------------------------------------
string ClassBar1Test::IsTestSupported()
{
    if (IsClassSupported(LW50_MEMORY_VIRTUAL))
        return RUN_RMTEST_TRUE;

    return "LW50_MEMORY_VIRTUAL class is not supported";
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
RC ClassBar1Test::Setup()
{
    RC       rc;
    LwRmPtr  pLwRm;
    UINT32   attr;
    UINT32   attr2;
    UINT32   i;
    UINT32   pageSize = 0;
    UINT64   sysGpuAddr;
    UINT64   memLimit;
    UINT64   pteAddr;
    LW0080_CTRL_DMA_GET_PTE_INFO_PARAMS  getParams;
    LW0080_CTRL_DMA_PTE_INFO_PTE_BLOCK * pPteBlock = NULL;
    LW208F_CTRL_BUS_IS_BAR1_VIRTUAL_PARAMS bar1InfoParams;

    memset(&getParams, 0, sizeof(getParams));

    // TC1
    ErrorLogger::StartingTest();
    ErrorLogger::IgnoreErrorsForThisTest();

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    CHECK_RC(pLwRm->Control(GetBoundGpuSubdevice()->GetSubdeviceDiag(),
                            LW208F_CTRL_CMD_BUS_IS_BAR1_VIRTUAL,
                            &bar1InfoParams,
                            sizeof (bar1InfoParams)));

    m_bIsBar1Virtual = bar1InfoParams.bIsVirtual;

    // Override any JS settings for the channel type, this test requires
    // a GpFifoChannel channel.
    m_TestConfig.SetChannelType(TestConfiguration::GpFifoChannel);

    // Allocate channel
    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_GR(0)));

    CHECK_RC(allocObject());

    if (IsClassSupported(GF100_REMAPPER))
    {
        CHECK_RC(pLwRm->Alloc(m_hCh, &m_remapperObj, GF100_REMAPPER, NULL));
    }

    //
    // Allocate mappable VA object
    //
    LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS vmparams = { 0 };
    CHECK_RC(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                          &m_hVA, LW01_MEMORY_VIRTUAL, &vmparams));

    //
    // Allocate the system physical memory
    //
    attr = (DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH) |
            DRF_DEF(OS32, _ATTR, _DEPTH, _32) |
            DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1) |
            DRF_DEF(OS32, _ATTR, _TILED, _NONE) |
            DRF_DEF(OS32, _ATTR, _ZLWLL, _NONE) |
            DRF_DEF(OS32, _ATTR, _COMPR, _NONE) |
            DRF_DEF(OS32, _ATTR, _LOCATION, _PCI) |
            DRF_DEF(OS32, _ATTR, _COHERENCY, _UNCACHED) |
            DRF_DEF(OS32, _ATTR, _PHYSICALITY, _NONCONTIGUOUS));

    attr2 = LWOS32_ATTR2_NONE;

    CHECK_RC(pLwRm->VidHeapAllocSizeEx(LWOS32_TYPE_IMAGE,
                                       LWOS32_ALLOC_FLAGS_NO_SCANOUT |
                                       LWOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED,
                                       &attr,
                                       &attr2, // pAttr2
                                       MEM_SIZE,
                                       1,    // alignment
                                       NULL, // pFormat
                                       NULL, // pCoverage
                                       NULL, // pPartitionStride
                                       &m_sysMemHandle,
                                       NULL, // poffset,
                                       NULL, // pLimit
                                       NULL, // pRtnFree
                                       NULL, // pRtnTotal
                                       0,    // width
                                       0,    // height
                                       GetBoundGpuDevice()));

    //
    // Map a GPU virtual address to the system physical address
    //
    CHECK_RC(pLwRm->MapMemoryDma(m_hVA, m_sysMemHandle, 0, MEM_SIZE,
                                 DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE), &sysGpuAddr, GetBoundGpuDevice()));

    getParams.gpuAddr = sysGpuAddr;

    CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                            LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                            &getParams,
                            sizeof (getParams)));
    //
    // Because there can be multiple (2) PTE_INFO_PTE_BLOCKs, need to find which
    // one is actually the valid one, there should be only one for now.
    //
    pPteBlock = NULL;
    for (i = 0; i < LW0080_CTRL_DMA_GET_PTE_INFO_PTE_BLOCKS; i++)
    {
        if (DRF_VAL(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_VALID, getParams.pteBlocks[i].pteFlags)
            == LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_VALID_TRUE)
        {
            pPteBlock = getParams.pteBlocks + i;
            break;
        }
    }
    if (pPteBlock)
    {
        pageSize = pPteBlock->pageSize;
    }

    if (pageSize != RM_PAGE_SIZE)
    {
        Printf(Tee::PriHigh,
               "ClassBar1Test: expected page size 0x%x, actual 0x%x\n",
               RM_PAGE_SIZE, pageSize);
        return RC::UNEXPECTED_RESULT;
    }

    memLimit = PTE_MEM_SIZE-1;
    CHECK_RC(pLwRm->AllocMemory(&m_pteMemHandle, LW01_MEMORY_SYSTEM,
                                DRF_DEF(OS02, _FLAGS, _LOCATION,_PCI) |
                                DRF_DEF(OS02, _FLAGS, _COHERENCY, _UNCACHED) |
                                DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS) |
                                DRF_DEF(OS02, _FLAGS, _MAPPING, _NO_MAP),
                                NULL,
                                &memLimit,
                                GetBoundGpuDevice()));

    CHECK_RC(pLwRm->AllocContextDma(&m_pteDmaHandle,
                                    LWOS03_FLAGS_ACCESS_READ_WRITE,
                                    m_pteMemHandle,
                                    0,
                                    PTE_MEM_SIZE-1));
    CHECK_RC((pLwRm->MapMemory(m_pteMemHandle, 0, PTE_MEM_SIZE, (void **)&pteAddr, 0, GetBoundGpuSubdevice())));

    return rc;
}

//! \brief Run the test!
//!
//! \sa Run
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC ClassBar1Test::Run()
{
    RC rc;

    CHECK_RC(alignedContiguousTest());

    // We cannot support overmap when Bar1 is physical.
    if(m_bIsBar1Virtual)
    {
        CHECK_RC(unalignedContiguousTest());
    }

    if (IsClassSupported(GF100_REMAPPER))
    {
        CHECK_RC(remapperTest());
    }

    return rc;
}

//! \brief Run a simple test with offsets and length aligned to a page boundary
//!
//! \sa unalignedContiguousTest
//-----------------------------------------------------------------------------
RC ClassBar1Test::alignedContiguousTest()
{
    RC       rc;
    LwRmPtr  pLwRm;
    LW0080_CTRL_FB_CREATE_FB_SEGMENT_PARAMS  createParams = {0};
    LW0080_CTRL_FB_DESTROY_FB_SEGMENT_PARAMS destroyParams = {0};

    memset(&createParams, 0, sizeof(createParams));
    createParams.VidOffset   = 0;
    createParams.Offset      = 0;
    createParams.ValidLength = MEM_SIZE;
    createParams.Length      = MEM_SIZE;
    createParams.hMemory     = MEM_HANDLE;
    createParams.hClient     = pLwRm->GetClientHandle();
    createParams.hDevice     = pLwRm->GetDeviceHandle(GetBoundGpuDevice());
    createParams.Flags       = DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _FIXED_OFFSET, _NO) |
                               DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _MAP_CPUVA, _YES) |
                               DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _CONTIGUOUS, _TRUE);

    CHECK_RC(pLwRm->Control(
        pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
        LW0080_CTRL_CMD_FB_CREATE_FB_SEGMENT,
        &createParams,
        sizeof(createParams)));

    CHECK_RC_CLEANUP(testMem(&createParams));

    destroyParams.hMemory = createParams.hMemory;

    CHECK_RC(pLwRm->Control(
        pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
        LW0080_CTRL_CMD_FB_DESTROY_FB_SEGMENT,
        &destroyParams,
        sizeof(destroyParams)));

    return rc;

Cleanup:
    destroyParams.hMemory = createParams.hMemory;

    pLwRm->Control(
        pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
        LW0080_CTRL_CMD_FB_DESTROY_FB_SEGMENT,
        &destroyParams,
        sizeof(destroyParams));

    return rc;
}

//! \brief Run a more complex test with overmap
//!
//! \sa unalignedContiguousTest
//-----------------------------------------------------------------------------
RC ClassBar1Test::unalignedContiguousTest()
{
    RC       rc;
    LwRmPtr  pLwRm;
    LW0080_CTRL_FB_CREATE_FB_SEGMENT_PARAMS  createParams = {0};
    LW0080_CTRL_FB_DESTROY_FB_SEGMENT_PARAMS destroyParams = {0};

    memset(&createParams, 0, sizeof(createParams));
    createParams.VidOffset   = 0;
    createParams.Offset      = 0;
    createParams.ValidLength = MEM_SIZE;
    createParams.Length      = 2 * MEM_SIZE;
    createParams.hMemory     = MEM_HANDLE;
    createParams.hClient     = pLwRm->GetClientHandle();
    createParams.hDevice     = pLwRm->GetDeviceHandle(GetBoundGpuDevice());
    createParams.Flags       = DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _FIXED_OFFSET, _NO) |
                               DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _MAP_CPUVA, _YES) |
                               DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _CONTIGUOUS, _TRUE);

    CHECK_RC(pLwRm->Control(
        pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
        LW0080_CTRL_CMD_FB_CREATE_FB_SEGMENT,
        &createParams,
        sizeof(createParams)));

    CHECK_RC_CLEANUP(testMem(&createParams));

    destroyParams.hMemory = createParams.hMemory;

    CHECK_RC(pLwRm->Control(
        pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
        LW0080_CTRL_CMD_FB_DESTROY_FB_SEGMENT,
        &destroyParams,
        sizeof(destroyParams)));

    return rc;

Cleanup:
    destroyParams.hMemory = createParams.hMemory;

    pLwRm->Control(
        pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
        LW0080_CTRL_CMD_FB_DESTROY_FB_SEGMENT,
        &destroyParams,
        sizeof(destroyParams));

    return rc;
}

//! \brief Run a simple test with offsets and length  aligned to a page boundary, but with a noncontigous system memory allocation
//!
//! \sa alignedNonContiguousTest
//-----------------------------------------------------------------------------
RC ClassBar1Test::alignedNonContiguousTest()
{
    RC       rc;
    LwRmPtr  pLwRm;
    LW0080_CTRL_FB_CREATE_FB_SEGMENT_PARAMS  createParams = {0};
    LW0080_CTRL_FB_DESTROY_FB_SEGMENT_PARAMS destroyParams = {0};

    memset(&createParams, 0, sizeof(createParams));
    createParams.hCtxDma     = m_pteDmaHandle;
    createParams.VidOffset   = 0;
    createParams.Offset      = 0;
    createParams.ValidLength = MEM_SIZE;
    createParams.Length      = 2*MEM_SIZE;
    createParams.hMemory     = MEM_HANDLE;
    createParams.hClient     = pLwRm->GetClientHandle();
    createParams.hDevice     = pLwRm->GetDeviceHandle(GetBoundGpuDevice());
    createParams.Flags       = DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _FIXED_OFFSET, _NO) |
                               DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _MAP_CPUVA, _YES) |
                               DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _CONTIGUOUS, _FALSE);

    CHECK_RC(pLwRm->Control(
        pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
        LW0080_CTRL_CMD_FB_CREATE_FB_SEGMENT,
        &createParams,
        sizeof(createParams)));

    destroyParams.hMemory = createParams.hMemory;

    CHECK_RC(pLwRm->Control(
        pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
        LW0080_CTRL_CMD_FB_DESTROY_FB_SEGMENT,
        &destroyParams,
        sizeof(destroyParams)));
    return rc;
}

//! \brief Run a more complex test with overmap on a noncontiguous system memory allocation
//!
//! \sa unalignedContiguousTest
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
RC ClassBar1Test::unalignedNonContiguousTest()
{
    RC       rc;
    LwRmPtr  pLwRm;
    LW0080_CTRL_FB_CREATE_FB_SEGMENT_PARAMS  createParams = {0};
    LW0080_CTRL_FB_DESTROY_FB_SEGMENT_PARAMS destroyParams = {0};

    memset(&createParams, 0, sizeof(createParams));
    createParams.hCtxDma     = m_pteDmaHandle;
    createParams.VidOffset   = 0;
    createParams.Offset      = 0;
    createParams.ValidLength = MEM_SIZE;
    createParams.Length      = 2*MEM_SIZE;
    createParams.hMemory     = MEM_HANDLE;
    createParams.hClient     = pLwRm->GetClientHandle();
    createParams.hDevice     = pLwRm->GetDeviceHandle(GetBoundGpuDevice());
    createParams.Flags       = DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _FIXED_OFFSET, _NO) |
                               DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _MAP_CPUVA, _YES) |
                               DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _CONTIGUOUS, _FALSE);

    CHECK_RC(pLwRm->Control(
        pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
        LW0080_CTRL_CMD_FB_CREATE_FB_SEGMENT,
        &createParams,
        sizeof(createParams)));

    destroyParams.hMemory = createParams.hMemory;

    CHECK_RC(pLwRm->Control(
        pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
        LW0080_CTRL_CMD_FB_DESTROY_FB_SEGMENT,
        &destroyParams,
        sizeof(destroyParams)));
    return rc;
}

//! \brief Use the ramapper on the allocated fb segment
//!
//! \sa remapperTest
//-----------------------------------------------------------------------------
RC ClassBar1Test::remapperTest()
{
    RC       rc;
#ifdef LW_LDDM
    LwU32    attr1, flags1, size1, pitch1, height1, width1;
    LWOS32_PARAMETERS memAlignmentParams;
    LWOS32_PARAMETERS deleteResource;
    void * rmHandle;
    UINT32 i;
    LW907F_CTRL_SET_SURFACE_PARAMS SetSurfaceParams = {0};

    memset(&memAlignmentParams, 0, sizeof(LWOS32_PARAMETERS));
    memAlignmentParams.function = LWOS32_FUNCTION_GET_MEM_ALIGNMENT;

    attr1 = DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM)      |
            DRF_DEF(OS32, _ATTR, _COMPR, _REQUIRED)       |
            DRF_DEF(OS32, _ATTR, _FORMAT, _BLOCK_LINEAR)  |
            DRF_DEF(OS32, _ATTR, _ZS_PACKING, _Z24S8)     |
            DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1)         |
            DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);

    flags1 = LWOS32_ALLOC_FLAGS_NO_SCANOUT | LWOS32_ALLOC_FLAGS_IGNORE_BANK_PLACEMENT;
    size1  = 100;
    height1 = 10;
    width1  = 100;
    pitch1  = 100;

    memAlignmentParams.data.AllocHintAlignment.alignType       = LWOS32_TYPE_DEPTH;
    memAlignmentParams.data.AllocHintAlignment.alignAttr       = attr1;
    memAlignmentParams.data.AllocHintAlignment.alignInputFlags = flags1;
    memAlignmentParams.data.AllocHintAlignment.alignSize       = size1;
    memAlignmentParams.data.AllocHintAlignment.alignHeight     = height1;
    memAlignmentParams.data.AllocHintAlignment.alignWidth      = width1;
    memAlignmentParams.data.AllocHintAlignment.alignPitch      = pitch1;
    memAlignmentParams.data.AllocHintAlignment.pHandle         = &memAlignmentParams;

    memAlignmentParams.hRoot         = pLwRm->GetClientHandle();
    memAlignmentParams.hObjectParent = pLwRm->GetDeviceHandle(GetBoundGpuDevice());

    LwRmVidHeapControl(&memAlignmentParams);

    rmHandle = memAlignmentParams.data.AllocHintAlignment.pContainer;

    memset(&createParams, 0, sizeof(createParams));

    createParams.VidOffset   = 0;
    createParams.Offset      = 0;
    createParams.ValidLength = MEM_SIZE;
    createParams.Length      = MEM_SIZE;
    createParams.hMemory     = MEM_HANDLE;
    createParams.hClient     = pLwRm->GetClientHandle();
    createParams.hDevice     = pLwRm->GetDeviceHandle(GetBoundGpuDevice());
    createParams.Flags       = DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _FIXED_OFFSET, _NO) |
                               DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _CONTIGUOUS, _TRUE);

    CHECK_RC(pLwRm->Control(
        pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
        LW0080_CTRL_CMD_FB_CREATE_FB_SEGMENT,
        &createParams,
        sizeof(createParams)));

    SetSurfaceParams.pMemory = (LwP64)createParams.pCpuAddress;
    SetSurfaceParams.hMemory = createParams.hMemory;
    SetSurfaceParams.size    = (LwU32)createParams.Length;
    SetSurfaceParams.format  = 0;

    for (i = 0; i < GetBoundGpuDevice()->GetNumSubdevices(); i++)
    {
        LwRm::Handle hSubDevice = pLwRm->GetSubdeviceHandle(GetBoundGpuDevice()->GetSubdevice(i));
        SetSurfaceParams.hSubDevice = hSubDevice;

        CHECK_RC(pLwRm->Control(
        m_remapperObj,
        LW907F_CTRL_CMD_SET_SURFACE,
        &SetSurfaceParams,
        sizeof(SetSurfaceParams)));
    }


    memset(&deleteResource, 0, sizeof(deleteResource));
    deleteResource.hRoot         = pLwRm->GetClientHandle();
    deleteResource.hObjectParent = pLwRm->GetDeviceHandle(GetBoundGpuDevice());
    deleteResource.function      = LWOS32_FUNCTION_RESOURCE_DELETE;
    deleteResource.data.ResourceDelete.pData = rmHandle;
    LwRmVidHeapControl(&deleteResource);

    destroyParams.hMemory = createParams.hMemory;
    CHECK_RC(pLwRm->Control(
        pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
        LW0080_CTRL_CMD_FB_DESTROY_FB_SEGMENT,
        &destroyParams,
        sizeof(destroyParams)));
#endif

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//----------------------------------------------------------------------------
RC ClassBar1Test::Cleanup()
{
    RC rc, firstRc;
    LwRmPtr     pLwRm;

    pLwRm->Free(m_pteDmaHandle);
    pLwRm->Free(m_pteMemHandle);
    pLwRm->Free(m_sysMemHandle);
    pLwRm->Free(m_hVA);
    pLwRm->Free(m_remapperObj);
    CHECK_RC(freeObject());
    FIRST_RC(m_TestConfig.FreeChannel(m_pCh));
    m_pCh = NULL;

    return firstRc;
}

//! \brief ClassBar1Test::allocObject()
//! \sa allocObject
//----------------------------------------------------------------------------
RC ClassBar1Test::allocObject()
{
    RC          rc;
    LwRmPtr     pLwRm;

    if (IsClassSupported(GF100_REMAPPER))
    {
        CHECK_RC(pLwRm->Alloc(m_hCh, &m_objectHandle, FERMI_TWOD_A, NULL));
    }

    return rc;
}

//! \brief ClassBar1Test::freeObject()
//! \sa freeObject
//---------------------------------------------------------------------------
RC ClassBar1Test::freeObject()
{
    RC          rc;
    LwRmPtr     pLwRm;

    if (IsClassSupported(GF100_REMAPPER))
    {
        pLwRm->Free(m_objectHandle);
    }
    m_objectHandle = 0;
    return rc;
}

//! \brief ClassBar1Test::testMem()
//! Write a known verifiable pattern to all the locations in the created
//! segment.  If the total length exceeds the valid length, then overmap
//! will be enabled in the RM.  In this case, the beginning entries will be
//! overwritten by the later accesses, so verify that the overmap took place.
//!
//! \sa testMem
//---------------------------------------------------------------------------
RC ClassBar1Test::testMem(LW0080_CTRL_FB_CREATE_FB_SEGMENT_PARAMS *pCreateParams)
{
    RC rc;
    UINT32 *ip;
    UINT64 i;
    UINT64 totalPages;
    UINT64 overMapMod;
    UINT32 actualVal;
    UINT32 testVal;

    totalPages = (pCreateParams->Length + pCreateParams->Offset + RM_PAGE_SIZE -1 )/RM_PAGE_SIZE;
    overMapMod = (pCreateParams->ValidLength + pCreateParams->Offset + RM_PAGE_SIZE - 1) /RM_PAGE_SIZE;

    ip = (UINT32 *)pCreateParams->pCpuAddress;
    for (i = 0; i < totalPages; i++)
    {
        testVal  = 0xf0000000 | UINT32(i);
        MEM_WR32(ip, testVal);
        ip += RM_PAGE_SIZE/sizeof(*ip);
    }

    ip = (UINT32 *)pCreateParams->pCpuAddress;
    for (i = 0; i < totalPages; i++)
    {
        if ((pCreateParams->ValidLength < pCreateParams->Length) &&
            (i < (totalPages - overMapMod)))
        {
            testVal  = 0xf0000000 | UINT32(i + overMapMod);
        }
        else
        {
            testVal  = 0xf0000000 | UINT32(i);
        }
        actualVal = MEM_RD32(ip);

        if (testVal != actualVal)
        {
            Printf(Tee::PriHigh, "ClassBar1Test: expected 0x%x, actual 0x%x\n",
                   testVal, actualVal);
            return RC::DATA_MISMATCH;
        }

        ip += RM_PAGE_SIZE/sizeof(*ip);
    }

    return rc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ ClassBar1Test object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(ClassBar1Test, RmTest,
                 "Test Bar1 S/W method to ilwalidate PTE.");
