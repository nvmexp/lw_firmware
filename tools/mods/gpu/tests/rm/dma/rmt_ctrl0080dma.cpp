/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2011,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_ctrl0080dma.cpp
//! \brief LW0080_CTRL_CMD_DMA Tests
//! Main purpose of the test is to verify the FILL_PTE API
//!

#include "core/include/utility.h"
#include "lwos.h"
#include "core/include/jscript.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "core/include/xp.h"
#include "class/cl90f1.h"  // FERMI_VASPACE_A
#include "class/cl007d.h"  // LW04_SOFTWARE_TEST
#include "class/cl0070.h"  // LW01_MEMORY_VIRTUAL
#include "ctrl/ctrl0080.h" // LW01_DEVICE_XX/LW03_DEVICE

#include "gpu/tests/rmtest.h"
#include "core/include/memcheck.h"

class Ctrl0080DmaTest: public RmTest
{
public:
    Ctrl0080DmaTest();
    virtual ~Ctrl0080DmaTest();

    virtual string   IsTestSupported();

    virtual RC       Setup();
    virtual RC       Run();
    virtual RC       Cleanup();

private:
    RC               TestFillPteMem();

    UINT32           m_memSize;

    UINT64           m_sysGpuAddr;
    UINT64           m_sysOrigGpuAddr;
    UINT64 *         m_sysPageArray;
    LwRm::Handle     m_hSysMem;

    UINT64           m_vidGpuAddr;
    LwRm::Handle     m_hVidMem;

    LwNotification * m_pNotifier;

    LwRm::Handle     m_hObj;
    LwRm::Handle     m_hVA;

    LwRm::Handle     m_hNotMem;
    LwRm::Handle     m_hNotDma;

    FLOAT64          m_TimeoutMs;

};

//! \brief Ctrl0080DmaTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
Ctrl0080DmaTest::Ctrl0080DmaTest() :
    m_sysGpuAddr(0LL),
    m_sysOrigGpuAddr(0LL),
    m_sysPageArray(NULL),
    m_hSysMem(0),
    m_vidGpuAddr(0LL),
    m_hVidMem(0),
    m_pNotifier(NULL),
    m_hObj(0),
    m_hVA(0),
    m_hNotMem(0),
    m_hNotDma(0)
{
    SetName("Ctrl0080DmaTest");
}

//! \brief Ctrl0080DmaTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
Ctrl0080DmaTest::~Ctrl0080DmaTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \return True if dGPU virtual memory is supported
//-----------------------------------------------------------------------------
string Ctrl0080DmaTest::IsTestSupported()
{
    Xp::OperatingSystem xpOS = Xp::GetOperatingSystem();
    if (Xp::OS_MAC   == xpOS ||
        Xp::OS_MACRM == xpOS ||
        Xp::OS_LINUX == xpOS)
    {
        return "This test is not supported for MAC and Linux";
    }

    if( IsClassSupported(LW01_MEMORY_VIRTUAL) )
        return RUN_RMTEST_TRUE;
    return "LW01_MEMORY_VIRTUAL class is not supported on current platform";
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa IsTestSupported
//-----------------------------------------------------------------------------
RC Ctrl0080DmaTest::Setup()
{
    UINT32  memLimit;
    LwRmPtr pLwRm;
    UINT32  attr;
    UINT32  attr2;
    RC      rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    m_TimeoutMs = m_TestConfig.TimeoutMs();

    m_TestConfig.SetAllowMultipleChannels(true);

    m_memSize = 128*1024;
    memLimit = m_memSize - 1;

    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_GR(0)));

    CHECK_RC(pLwRm->Alloc(m_hCh, &m_hObj, LW04_SOFTWARE_TEST, NULL));

    CHECK_RC(pLwRm->AllocSystemMemory(&m_hNotMem,
             DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS) |
             DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI) |
             DRF_DEF(OS02, _FLAGS, _COHERENCY, _CACHED),
             m_memSize, GetBoundGpuDevice()));

    CHECK_RC(pLwRm->AllocContextDma(&m_hNotDma,
                                    DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_WRITE) |
                                    DRF_DEF(OS03, _FLAGS, _TYPE, _NOTIFIER),
                                    m_hNotMem, 0, memLimit));

    CHECK_RC(pLwRm->MapMemory(m_hNotMem, 0, m_memSize, (void **)&m_pNotifier, 0, GetBoundGpuSubdevice()));

    LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS vmparams = { 0 };
    CHECK_RC(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                          &m_hVA, LW01_MEMORY_VIRTUAL, &vmparams));

    //
    // Allocate the system physical address
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
                                       m_memSize,
                                       1,    // alignment
                                       NULL, // pFormat
                                       NULL, // pCoverage
                                       NULL, // pPartitionStride
                                       &m_hSysMem,
                                       NULL, // poffset,
                                       NULL, // pLimit
                                       NULL, // pRtnFree
                                       NULL, // pRtnTotal
                                       0,    // width
                                       0,    // height
                                       GetBoundGpuDevice()));

    //
    // Allocate the video physical address
    //
    attr = (DRF_DEF(OS32, _ATTR, _FORMAT, _BLOCK_LINEAR) |
            DRF_DEF(OS32, _ATTR, _DEPTH, _32) |
            DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1) |
            DRF_DEF(OS32, _ATTR, _TILED, _NONE) |
            DRF_DEF(OS32, _ATTR, _ZLWLL, _NONE) |
            DRF_DEF(OS32, _ATTR, _COMPR, _NONE) |
            DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM) |
            DRF_DEF(OS32, _ATTR, _COHERENCY, _WRITE_COMBINE) |
            DRF_DEF(OS32, _ATTR, _PHYSICALITY, _DEFAULT));
    attr2 = LWOS32_ATTR2_NONE;
    CHECK_RC(pLwRm->VidHeapAllocSizeEx(LWOS32_TYPE_IMAGE,
                                       LWOS32_ALLOC_FLAGS_NO_SCANOUT |
                                       LWOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED,
                                       &attr,
                                       &attr2, // pAttr2
                                       m_memSize,
                                       1,    // alignment
                                       NULL, // pFormat
                                       NULL, // pCoverage
                                       NULL, // pPartitionStride
                                       &m_hVidMem,
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
    CHECK_RC(pLwRm->MapMemoryDma(m_hVA, m_hSysMem, 0, m_memSize,
                                 DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE), &m_sysGpuAddr, GetBoundGpuDevice()));
    m_sysOrigGpuAddr = m_sysGpuAddr;

    //
    // Map a GPU virtual address to the video physical address
    //
    CHECK_RC(pLwRm->MapMemoryDma(m_hVA, m_hVidMem, 0, m_memSize,
                                 DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE), &m_vidGpuAddr, GetBoundGpuDevice()));

    return rc;
}

//! \brief Run the test!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC Ctrl0080DmaTest::Run()
{
    RC rc;
    CHECK_RC(TestFillPteMem());
    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC Ctrl0080DmaTest::Cleanup()
{
    RC rc, firstRc;
    LwRmPtr pLwRm;

    pLwRm->UnmapMemory(m_hNotMem, m_pNotifier, 0, GetBoundGpuSubdevice());

    pLwRm->UnmapMemoryDma(m_hVA, m_hSysMem,
                          LW04_MAP_MEMORY_FLAGS_NONE, m_sysGpuAddr, GetBoundGpuDevice());

    pLwRm->UnmapMemoryDma(m_hVA, m_hVidMem,
                          LW04_MAP_MEMORY_FLAGS_NONE, m_vidGpuAddr, GetBoundGpuDevice());

    pLwRm->Free(m_hNotDma);
    pLwRm->Free(m_hNotMem);
    pLwRm->Free(m_hSysMem);
    pLwRm->Free(m_hVidMem);
    pLwRm->Free(m_hVA);
    pLwRm->Free(m_hObj);

    FIRST_RC(m_TestConfig.FreeChannel(m_pCh));

    return firstRc;
}

//! \brief TestFillPteMem : verfies the functionality of FILL_PTE
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC Ctrl0080DmaTest::TestFillPteMem()
{
    LwRmPtr pLwRm;
    RC      rc;
    UINT32  pageCount, i, j, counter, pageSize = 0;
    UINT64  vidPage, gpuAddr;
    UINT32 *  preFillPteField = NULL;
    UINT32 *  postFillPteField = NULL;
    LW0080_CTRL_DMA_GET_PTE_INFO_PARAMS getParams;
    LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS fillParams;
    LW0080_CTRL_DMA_PTE_INFO_PTE_BLOCK  *pPteBlock = NULL;

    memset(&getParams, 0, sizeof(getParams));
    memset(&fillParams, 0, sizeof(fillParams));

    //
    // First, try the system memory mapping.
    // Means Using allocated System memory.
    //

    // Query the RM for the PTE info to manipulate the callwlation further

    getParams.gpuAddr = m_sysGpuAddr;

    CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                            LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                            &getParams,
                            sizeof (getParams)));

    //
    // Because there can be multiple (2) PTE_INFO_PTE_BLOCKs, need to find which
    // one is actually the valid one, there should be only one for now.
    //
    pPteBlock = NULL;
    for (j = 0; j < LW0080_CTRL_DMA_GET_PTE_INFO_PTE_BLOCKS; j++)
    {
        if (DRF_VAL(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_VALID, getParams.pteBlocks[j].pteFlags)
            == LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_VALID_TRUE)
        {
            pPteBlock = getParams.pteBlocks + j;
            break;
        }
    }
    //RM_ASSERT(pPteBlock);
    if (pPteBlock)
        pageSize = pPteBlock->pageSize;

    // XXX: if pageSize is zero, we're sunk, someone needs to fix this test
    MASSERT(0 != pageSize);

    //
    // Fill some allocated memory with the page table data.
    //
    pageCount = UINT32(m_memSize/pageSize);

    preFillPteField = new UINT32[pageCount];
    postFillPteField = new UINT32[pageCount];

    //
    // Fill few chunks of PTE at a time of whole allocation, and verified
    // that PTE which r not updated should not be change along with the
    // verification of PTE's value which r updated.
    //

    // For System memory page sizes are small so dividing whole allocation into 4 parts.
    for (counter = pageCount/4; counter <= pageCount; counter += (pageCount/4))
    {
        m_sysPageArray = new UINT64[counter * sizeof(UINT64)];

        for (i = 0; i < pageCount; i++)
        {
            if (i < counter)
            {
                m_sysPageArray[i] = i * pageSize;
            }
            preFillPteField[i] = 0;
            postFillPteField[i] = 0;
        }

        // read the PTE fields before FILL PTE
        for (i = 0, gpuAddr = m_sysGpuAddr; i < pageCount; i++, gpuAddr += pageSize)
        {
            getParams.gpuAddr = gpuAddr;

            CHECK_RC_CLEANUP(pLwRm->Control(
                            pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                            LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                            &getParams,
                            sizeof (getParams)));

            //
            // Because there can be multiple (2) PTE_INFO_PTE_BLOCKs, need to find which
            // one is actually the valid one, there should be only one for now.
            //
            pPteBlock = NULL;
            for (j = 0; j < LW0080_CTRL_DMA_GET_PTE_INFO_PTE_BLOCKS; j++)
            {
                if (DRF_VAL(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_VALID, getParams.pteBlocks[j].pteFlags)
                    == LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_VALID_TRUE)
                {
                    pPteBlock = getParams.pteBlocks + j;
                    break;
                }
            }
            //RM_ASSERT(pPteBlock);
            if (pPteBlock)
                preFillPteField[i] = pPteBlock->pteFlags;
        }

        // Here we are modifying the PTE's filed for specified number of pages

        fillParams.gpuAddr = m_sysGpuAddr;
        fillParams.hwResource.hClient = pLwRm->GetClientHandle();
        fillParams.hwResource.hDevice = pLwRm->GetDeviceHandle(GetBoundGpuDevice());
        fillParams.hwResource.hMemory = m_hSysMem;
        fillParams.offset = 0;
        fillParams.pageCount = counter;
        fillParams.pageArray = (LwP64)m_sysPageArray;
        fillParams.pteMem = 0;
        fillParams.flags = DRF_DEF(0080, _CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS, _VALID, _FALSE) |
                        DRF_DEF(0080, _CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS, _CONTIGUOUS, _FALSE) |
                        DRF_DEF(0080, _CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS, _APERTURE, _VIDEO_MEMORY);

        if (IsClassSupported(FERMI_VASPACE_A))
        {
            // N.B. _OVERRIDE_PAGE_SIZE_TRUE is needed to specify which PTE array (big or small) to fill
            fillParams.flags |= DRF_DEF(0080, _CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS, _OVERRIDE_PAGE_SIZE, _TRUE);
        }

        fillParams.pageSize = pageSize;

        CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                                LW0080_CTRL_CMD_DMA_FILL_PTE_MEM,
                                &fillParams,
                                sizeof (fillParams)));

        // read the PTE fields after FILL PTE
        for (i = 0, gpuAddr = m_sysGpuAddr; i < pageCount; i++, gpuAddr += pageSize)
        {
            getParams.gpuAddr = gpuAddr;

            CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                            LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                            &getParams,
                            sizeof (getParams)));

            //
            // Because there can be multiple (2) PTE_INFO_PTE_BLOCKs, need to find which
            // one is actually the valid one, this time we want the same pagesize as before
            // the FILL_PTE_MEM call.
            //
            pPteBlock = NULL;
            for (j = 0; j < LW0080_CTRL_DMA_GET_PTE_INFO_PTE_BLOCKS; j++)
            {
                if (getParams.pteBlocks[j].pageSize == fillParams.pageSize)
                {
                    pPteBlock = getParams.pteBlocks + j;
                    break;
                }
            }
            //RM_ASSERT(pPteBlock);
            if (pPteBlock)
                postFillPteField[i] = pPteBlock->pteFlags;
        }

        //
        // Verify that PTE entries are updated.
        // It is specific to Fermi as to this functinality
        // is needed mainly for page migration supported by Fermi MMU
        // which mainly include updation of "Valid" and "Aperture" PTE fields
        //
        if (IsClassSupported(FERMI_VASPACE_A))
        {
            for (i = (counter - pageCount/4); i < pageCount; i++)
            {
                if (i < counter)
                {
                    if ((DRF_VAL(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_VALID, preFillPteField[i]) ==
                        DRF_VAL(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_VALID, postFillPteField[i])))
                    {
                        Printf(Tee::PriHigh, " PTE's VALID or APERTURE field is same which should not be the case\n");
                        CHECK_RC_CLEANUP( RC::DATA_MISMATCH );
                    }
                }
                else if (i >= counter)
                {
                    if ((DRF_VAL(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_VALID, preFillPteField[i]) !=
                        DRF_VAL(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_VALID, postFillPteField[i])) ||
                        (DRF_VAL(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_APERTURE, preFillPteField[i]) !=
                        DRF_VAL(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_APERTURE, postFillPteField[i])))
                    {
                        Printf(Tee::PriHigh, " PTE's VALID or APERTURE field should be same \n");
                        CHECK_RC_CLEANUP( RC::DATA_MISMATCH );
                    }
                }
            }
        }

        delete [] m_sysPageArray;
        m_sysPageArray = NULL;
    }

    delete [] preFillPteField;
    delete [] postFillPteField;

    //
    // Next, try the video memory mapping.
    // Means Using allocated Video memory
    //

    // Query the RM for PTE info to manipulate the callwlation further
    getParams.gpuAddr = m_vidGpuAddr;

    CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                            LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                            &getParams,
                            sizeof (getParams)));

    //
    // Because there can be multiple (2) PTE_INFO_PTE_BLOCKs, need to find which
    // one is actually the valid one, there should be only one for now.
    //
    pPteBlock = NULL;
    for (j = 0; j < LW0080_CTRL_DMA_GET_PTE_INFO_PTE_BLOCKS; j++)
    {
        if (DRF_VAL(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_VALID, getParams.pteBlocks[j].pteFlags)
            == LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_VALID_TRUE)
        {
            pPteBlock = getParams.pteBlocks + j;
            break;
        }
    }
    //RM_ASSERT(pPteBlock);
    if (pPteBlock)
        pageSize = pPteBlock->pageSize; // if pageSize is zero, we're sunk, someone needs to fix this test

    //
    // Fill few chunks of PTE at a time of whole allocation, and verified
    // that PTE which r not updated should not be change along with the
    // verification of PTE's value which r updated.
    //

    // For Video memory paze sizes are big so taking one page at a time to Update.

    pageCount = UINT32(m_memSize/pageSize);

    preFillPteField = new UINT32[pageCount];
    postFillPteField = new UINT32[pageCount];

    for (counter = 1; counter <=  pageCount; counter++)
    {
        vidPage = 0;

        for (i = 0; i < pageCount; i++)
        {
            preFillPteField[i] = 0;
            postFillPteField[i] = 0;
        }

        // read the PTE fields before FILL PTE
        for (i = 0, gpuAddr = m_vidGpuAddr; i < pageCount; i++, gpuAddr += pageSize)
        {
            getParams.gpuAddr = gpuAddr;

            CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                            LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                            &getParams,
                            sizeof (getParams)));
            //
            // Because there can be multiple (2) PTE_INFO_PTE_BLOCKs, need to find which
            // one is actually the valid one, there should be only one for now.
            //
            pPteBlock = NULL;
            for (j = 0; j < LW0080_CTRL_DMA_GET_PTE_INFO_PTE_BLOCKS; j++)
            {
                if (DRF_VAL(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_VALID, getParams.pteBlocks[j].pteFlags)
                    == LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_VALID_TRUE)
                {
                    pPteBlock = getParams.pteBlocks + j;
                    break;
                }
            }
            //RM_ASSERT(pPteBlock);
            if (pPteBlock)
                preFillPteField[i] = pPteBlock->pteFlags;
        }

         // Here we are modifying the PTE's filed for specified number of pages

        fillParams.gpuAddr = m_vidGpuAddr;
        fillParams.hwResource.hClient = pLwRm->GetClientHandle();
        fillParams.hwResource.hDevice = pLwRm->GetDeviceHandle(GetBoundGpuDevice());
        fillParams.hwResource.hMemory = m_hVidMem;
        fillParams.offset = 0;
        fillParams.pageCount = counter;
        fillParams.pageArray = (LwP64)&vidPage;
        fillParams.pteMem = 0;
        fillParams.flags = DRF_DEF(0080, _CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS, _VALID, _FALSE) |
                        DRF_DEF(0080, _CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS, _CONTIGUOUS, _TRUE) |
                        DRF_DEF(0080, _CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS, _APERTURE, _SYSTEM_COHERENT_MEMORY);
        if (IsClassSupported(FERMI_VASPACE_A))
        {
            // N.B. _OVERRIDE_PAGE_SIZE_TRUE is needed to specify which PTE array (big or small) to fill
            fillParams.flags |= DRF_DEF(0080, _CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS, _OVERRIDE_PAGE_SIZE, _TRUE);
        }
        fillParams.pageSize = pageSize;

        CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                                LW0080_CTRL_CMD_DMA_FILL_PTE_MEM,
                                &fillParams,
                                sizeof (fillParams)));

        // read the PTE fields after FILL PTE
        for (i = 0, gpuAddr = m_vidGpuAddr; i < pageCount; i++, gpuAddr += pageSize)
        {
            getParams.gpuAddr = gpuAddr;

            CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                            LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                            &getParams,
                            sizeof (getParams)));
            //
            // Because there can be multiple (2) PTE_INFO_PTE_BLOCKs, need to find which
            // one is actually the valid one, this time we want the same pagesize as before
            // the FILL_PTE_MEM call.
            //
            pPteBlock = NULL;
            for (j = 0; j < LW0080_CTRL_DMA_GET_PTE_INFO_PTE_BLOCKS; j++)
            {
                if (getParams.pteBlocks[j].pageSize == fillParams.pageSize)
                {
                    pPteBlock = getParams.pteBlocks + j;
                    break;
                }
            }
            //RM_ASSERT(pPteBlock);
            if (pPteBlock)
                postFillPteField[i] = pPteBlock->pteFlags;
        }
        //
        // Verify that PTE entries are updated.
        // It is specific to Fermi as to this functinality
        // is needed mainly for page migration supported by Fermi MMU
        // which mainly include updation of "Valid" and "Aperture" PTE fields
        //

        if (IsClassSupported(FERMI_VASPACE_A))
        {
            for (i = (counter - 1); i < pageCount; i++)
            {
                if (i < counter)
                {
                    if ((DRF_VAL(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_VALID, preFillPteField[i]) ==
                        DRF_VAL(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_VALID, postFillPteField[i])))
                    {
                        Printf(Tee::PriHigh, " PTE's VALID or APERTURE field is same even after the PTE update\n");
                        CHECK_RC_CLEANUP( RC::DATA_MISMATCH );
                    }
                }
                else if (i >= counter)
                {
                    if ((DRF_VAL(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_VALID, preFillPteField[i]) !=
                        DRF_VAL(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_VALID, postFillPteField[i])) ||
                        (DRF_VAL(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_APERTURE, preFillPteField[i]) !=
                        DRF_VAL(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_APERTURE, postFillPteField[i])))
                    {
                        Printf(Tee::PriHigh, " PTE's VALID or APERTURE field should be same \n");
                        CHECK_RC_CLEANUP( RC::DATA_MISMATCH );
                    }
                }
            }
        }
    }
Cleanup:

    delete [] m_sysPageArray;
    delete [] preFillPteField;
    delete [] postFillPteField;
    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ Ctrl0080DmaTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(Ctrl0080DmaTest, RmTest,
                 "Ctrl0080DmaTest RM test.");

