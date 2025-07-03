/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011,2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file - rmt_verifcomptag.cpp
//! \brief This test is designed to verify the uniqueness and release/reacquire
//! \of comptaglines.
//! Various steps involve in this are :
//!
//! 1) Alloc the multiple Compressible video memory chunks
//! 2) Free few of them inbetween allocation to simulate memory fragmentation a bit
//! 3) extract the PTE properties of each page ( mainly comtagline, kind, size)
//! 4) save comptag assignment level info for specified handle
//! 5) verified the release and reacquire of comptaglines for specified handle
//! 6) Designe a Data Structure to save this info ( here we took linked list as we have
//!    no prior idea hoe many pages are going to alloc)
//! 7) Use this data structure to validate that no two pages have same comptageline
//!    unless they have same "kind" and allocated data to a comptagline is less tha 128KB(on Fermi)
//! 8) Free All resources.
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include <string> // Only use <> for built-in C++ stuff
#include "random.h"
#include "lwos.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "class/cl9097.h"  // FERMI_A
#include "class/cl0070.h"  // LW01_MEMORY_VIRTUAL
#include "ctrl/ctrl0080.h"
#include "fermi/gf100/dev_mmu.h"
#include "gpu/include/gralloc.h"
#include "core/include/memcheck.h"
#include "gpu/include/gpudev.h"

//
// Structure to contain info for a comptagline
//
struct CTAGDATA
{
    UINT32 ctagLine;
    UINT32 kind;
    UINT64 totalOnLine;
    CTAGDATA *next;
};

//
// Data Cover by 1 ComptagLine On Fermi
//

#define COMPTAG_LINE_COVERAGE_FERMI    (128*1024)

//
// This define shopuld be consistent with "memLimitArray"
#define MEM_LIMIT_ARRAY_SIZE 7

//
// Array to specify how much memory we need to allocate and how
// These are Just Raw data which specify the sequence of allocation
// Here 0 value means free an allocted chunk before that index.
//
static UINT32 memLimitArray[MEM_LIMIT_ARRAY_SIZE] = {(5*64*1024),
                                                     (64*1024),
                                                     (1),
                                                     (0),
                                                     (30*64*1024),
                                                     (50*64*1024 + 1024),
                                                     (128*64*1024 + 50)};

#ifdef LW_VERIF_FEATURES
// Kind assigned to specific "index" memory chunk
static INT32 kindArray[MEM_LIMIT_ARRAY_SIZE] = {0};
#endif

static bool allocatiolwidArray[MEM_LIMIT_ARRAY_SIZE] = {false};
static bool mapAllocationArray[MEM_LIMIT_ARRAY_SIZE] = {false};

static bool memDeleted[MEM_LIMIT_ARRAY_SIZE] = {false};

class VerifCompTagTest : public RmTest
{

private:
    LwRm::Handle     m_hVidMem[MEM_LIMIT_ARRAY_SIZE];
    LwRm::Handle     m_hVA;
    LwRm::Handle     m_hSubDev;
    ThreeDAlloc      m_3dAlloc;
    UINT64           m_vidGpuAddr[MEM_LIMIT_ARRAY_SIZE];
    CTAGDATA         *head;
    RC AllocAndMapMem(UINT32 index);
    RC ParsePteData();
    RC VerifyCompTagLine(LW0080_CTRL_DMA_PTE_INFO_PTE_BLOCK *pPteBlock, bool alignment);
    RC GetPteBlock(LW0080_CTRL_DMA_GET_PTE_INFO_PARAMS *getParams, LW0080_CTRL_DMA_PTE_INFO_PTE_BLOCK **validPteBlock);
    LwBool           m_bFbBroken;

public:
    VerifCompTagTest();
    virtual ~VerifCompTagTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

};

//! \brief VerifCompTagTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
VerifCompTagTest::VerifCompTagTest()
{
    SetName("VerifCompTagTest");
    m_3dAlloc.SetOldestSupportedClass(FERMI_A);
}

//! \brief VerifCompTagTest Destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
VerifCompTagTest::~VerifCompTagTest()
{

}

//! \brief Test Is For Fermi
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string VerifCompTagTest::IsTestSupported()
{
    if (!m_3dAlloc.IsSupported(GetBoundGpuDevice()))
        return "Supported only on Fermi+";

    if ((GetBoundGpuDevice())->GetFamily()>= GpuDevice::Ampere)
        return "Not Supported on Ampere+ chips";

    //
    // Query RM to see if compression is enabled.  If it isn't, then there is no
    // point in testing _COMPR == _REQUIRED
    //
    LwRmPtr pLwRm;
    UINT32 retVal;
    struct fbInfo
    {
        LW2080_CTRL_FB_INFO compressionSize;
        LW2080_CTRL_FB_INFO fbBroken;
    } fbInfo;
    LW2080_CTRL_FB_GET_INFO_PARAMS fbInfoParams;
    bool compressionEnabled = true;

    fbInfo.compressionSize.index = LW2080_CTRL_FB_INFO_INDEX_COMPRESSION_SIZE;
    fbInfo.fbBroken.index = LW2080_CTRL_FB_INFO_INDEX_FB_IS_BROKEN;

    fbInfoParams.fbInfoListSize = sizeof (fbInfo) / sizeof (LW2080_CTRL_FB_INFO);
    fbInfoParams.fbInfoList = LW_PTR_TO_LwP64(&fbInfo);

    retVal = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
        LW2080_CTRL_CMD_FB_GET_INFO,
        &fbInfoParams,
        sizeof (fbInfoParams));
    if (OK != RmApiStatusToModsRC(retVal))
    {
        return "Unable to query compression info";
    }
    compressionEnabled = (fbInfo.compressionSize.data != 0);
    if (!compressionEnabled)
    {
        return "Compression not enabled";
    }

    m_bFbBroken = fbInfo.fbBroken.data ? LW_TRUE : LW_FALSE;
    if (m_bFbBroken)
    {
        return "FB broken";
    }

    return RUN_RMTEST_TRUE;
}

//! \brief Allocate the Compressible Vid MEM Chunks
//!
//! \return True if memory allocated successfully
//------------------------------------------------------------------------------
RC VerifCompTagTest::Setup()
{
    RC rc;
    LwRmPtr pLwRm;

    head = NULL;

    LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS vmparams = { 0 };
    CHECK_RC(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                          &m_hVA, LW01_MEMORY_VIRTUAL, &vmparams));

    // Allocate Subdevice handle
    m_hSubDev = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());

    for (UINT32 i = 0 ;i < MEM_LIMIT_ARRAY_SIZE; i++)
    {
        CHECK_RC(AllocAndMapMem(i));
    }
    return rc;
}

//! \brief Run() : Contain the verification Part of test
//!
//! \return True, if processed successfully
//------------------------------------------------------------------------------
RC VerifCompTagTest::Run()
{
    RC rc;

    //
    // Call the function to parse various PTEs
    //
    CHECK_RC(ParsePteData());

    return rc;
}

//! \brief Free All allocated Resources
//!
//------------------------------------------------------------------------------
RC VerifCompTagTest::Cleanup()
{
    CTAGDATA *prevNode= NULL;
    LwRmPtr pLwRm;
    UINT32 i;

    //
    //Deallocate each node of Linked List
    //
    while (head != NULL)
    {
        prevNode = head;
        head = head->next;
        delete prevNode;
    }
    delete head;

    // Free All allocated Memory Chunks

    for (i = 0; i < MEM_LIMIT_ARRAY_SIZE; i++)
    {
        if (memLimitArray[i] == 0 || memDeleted[i] == true)
        {
            continue;
        }
        else
        {
            if(mapAllocationArray[i] == true)
            {
                pLwRm->UnmapMemoryDma(m_hVA, m_hVidMem[i],
                                  DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE), m_vidGpuAddr[i], GetBoundGpuDevice());
            }
            if(allocatiolwidArray[i] == true)
            {
                pLwRm->Free(m_hVidMem[i]);
            }
        }
    }
    pLwRm->Free(m_hVA);

    return OK; // XXX should check RCs above...
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------
//! \brief AllocAndMapMem: Alloc and Map the vid compressible memory
//!
//! \return OK if it pass otherwise Fail.
//-----------------------------------------------------------------------------

RC VerifCompTagTest::AllocAndMapMem(UINT32 index)
{
    RC rc;
    LwRmPtr pLwRm;
    Random m_Random;
    m_Random.SeedRandom(m_TestConfig.Seed());
    UINT32 attr, attr2, indexToDelete, page_size = 0;
    bool found;

    if (memLimitArray[index] == 0)
    {
        //
        // Need to free a memory chunk, but it should not be already freed one or 0
        //
        found = false;
        while (!found)
        {
            indexToDelete = m_Random.GetRandom(0,index-1);
            if (!(memLimitArray[indexToDelete] == 0) && (memDeleted[indexToDelete] != true))
            {
                memDeleted[indexToDelete] = true;
                found = true;
            }
        }

        pLwRm->UnmapMemoryDma(m_hVA, m_hVidMem[indexToDelete],
                              DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE), m_vidGpuAddr[indexToDelete], GetBoundGpuDevice());

        pLwRm->Free(m_hVidMem[indexToDelete]);

        return rc;
    }

    page_size = m_Random.GetRandom(0,1000);

#ifdef LW_VERIF_FEATURES

    LW2080_CTRL_FB_IS_KIND_PARAMS fbIsKindParams = {0};
    INT32 format = 0;

    do
    {
        //
        // Select a Kind which is Supported as well as Compressible.
        // Thats way it will have uncompreesible kind corresponding to
        // specified copressible kind
        //

        bool kindAlloted = false;
        format = m_Random.GetRandom(0,  ((LW_MMU_PTE_KIND_ILWALID - 1)*100))/100;

        // Check if selected kind is already alloted

        for (UINT32 i = 0; i < index; i++)
        {
            if (kindArray[i] == format)
            {
                kindAlloted = true;
                break;
            }
        }

        if (kindAlloted)
        {
            continue;
        }

        // Check if selected Kind is supported
        fbIsKindParams.kind = format;
        fbIsKindParams.operation = LW2080_CTRL_FB_IS_KIND_OPERATION_SUPPORTED;
        CHECK_RC(pLwRm->Control(m_hSubDev,
                        LW2080_CTRL_CMD_FB_IS_KIND,
                        &fbIsKindParams,
                        sizeof (fbIsKindParams)));

        if (fbIsKindParams.result)
        {
            // Check if selected Kind is compressible
            fbIsKindParams.operation = LW2080_CTRL_FB_IS_KIND_OPERATION_COMPRESSIBLE;
            fbIsKindParams.kind = format;
            CHECK_RC(pLwRm->Control(m_hSubDev,
                        LW2080_CTRL_CMD_FB_IS_KIND,
                        &fbIsKindParams,
                        sizeof (fbIsKindParams)));
        }
        else
        {
            continue;
        }

    }while (!fbIsKindParams.result);

    kindArray[index] = format;

#endif

    if (m_bFbBroken)
    {
        attr = DRF_DEF(OS32, _ATTR, _LOCATION, _PCI);
    }
    else
    {
        attr = DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM);
    }

    //
    // Allocate the Vid Mem with compression enable
    //
    attr |= (DRF_DEF(OS32, _ATTR, _FORMAT, _BLOCK_LINEAR) |
            DRF_DEF(OS32, _ATTR, _TILED, _NONE) |
            DRF_DEF(OS32, _ATTR, _DEPTH ,_32)   |
            DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _2)|
            DRF_DEF(OS32, _ATTR, _ZLWLL, _NONE) |
            DRF_DEF(OS32, _ATTR, _COMPR, _REQUIRED) |
            ((page_size < 500) ? DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _4KB) : DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _DEFAULT)) |
            DRF_DEF(OS32, _ATTR, _COHERENCY, _WRITE_COMBINE) |
            DRF_DEF(OS32, _ATTR, _PHYSICALITY, _DEFAULT));
#ifdef LW_VERIF_FEATURES
    attr = (attr | DRF_DEF(OS32, _ATTR, _FORMAT_OVERRIDE, _ON));
#endif

    attr2 = LWOS32_ATTR2_NONE;
    rc  = pLwRm->VidHeapAllocSizeEx(LWOS32_TYPE_IMAGE,
                                       LWOS32_ALLOC_FLAGS_NO_SCANOUT |
                                       LWOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED,
                                       &attr,
                                       &attr2, // pAttr2
                                       memLimitArray[index],
                                       1,    // alignment
#ifdef LW_VERIF_FEATURES
                                       &format, // pFormat
#else
                                       NULL, // pFormat
#endif
                                       NULL, // pCoverage
                                       NULL, // pPartitionStride
                                       (m_hVidMem + index),
                                       NULL, // poffset,
                                       NULL, // pLimit
                                       NULL, // pRtnFree
                                       NULL, // pRtnTotal
                                       0,    // width
                                       0,    // height
                                       GetBoundGpuDevice());
    CHECK_RC(RmApiStatusToModsRC(rc));
    allocatiolwidArray[index] = true;
    if((DRF_VAL(OS32, _ATTR, _COMPR, attr) == LWOS32_ATTR_COMPR_NONE) )
    {
        Printf(Tee::PriHigh, "Could not allocate Vid Memory with Compression enable for the %d time\n",index);
        return RC::SOFTWARE_ERROR;
    }

    //
    // Map a GPU virtual address to the video physical address
    //
    CHECK_RC(pLwRm->MapMemoryDma(m_hVA, m_hVidMem[index], 0, memLimitArray[index],
                                 DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE), (m_vidGpuAddr+index), GetBoundGpuDevice()));

    mapAllocationArray[index] = true;
    return rc;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------
//! \brief ParsePteData: Parse PTE data so that it can further be helpful
//! to validate the comptagline uniqueness steps
//!
//! \return OK if it pass otherwise Fail.
//-----------------------------------------------------------------------------
RC VerifCompTagTest::ParsePteData()
{
    RC rc;
    LwRmPtr pLwRm;
    UINT64 memIndex, pageSize, pageCount, addr;
    LW0080_CTRL_DMA_PTE_INFO_PTE_BLOCK  *pPteBlock = NULL;
    LW2080_CTRL_FB_IS_KIND_PARAMS fbIsKindParams = {0};
    LW0080_CTRL_DMA_GET_PTE_INFO_PARAMS getParams;
    UINT32 *ctagsToHandle;
    UINT32 i;

    memset(&getParams, 0, sizeof(getParams));

    //
    // Loop around all the Memory Chunk
    //
    for (memIndex = 0; memIndex < MEM_LIMIT_ARRAY_SIZE ; memIndex++)
    {
        pPteBlock = NULL;
        if (memLimitArray[memIndex] == 0 || memDeleted[memIndex] == true)
            continue;
        getParams.gpuAddr = m_vidGpuAddr[memIndex];
        CHECK_RC(GetPteBlock(&getParams, &pPteBlock));

        //
        // Callwlate how many pages are there for this allocation
        //
        pageSize = pPteBlock->pageSize;
        pageCount = memLimitArray[memIndex]%pageSize?(memLimitArray[memIndex]/pageSize+1):(memLimitArray[memIndex]/pageSize);

        ctagsToHandle = new UINT32[(UINT32)pageCount];
        //
        // Loop around all the pages of an Allocation
        //
        for (i = 0, addr = m_vidGpuAddr[memIndex]; i < pageCount; i++, addr+= pageSize)
        {
            pPteBlock = NULL;
            getParams.gpuAddr = addr;
            CHECK_RC(GetPteBlock(&getParams, &pPteBlock));

            //
            // Store the ComptagLines for each page of this allocation
            //
            ctagsToHandle[i] = pPteBlock->comptagLine;

        }

        //
        // Release The Comptaglines For this handle
        //
        CHECK_RC(pLwRm->VidHeapReleaseCompression(m_hVidMem[memIndex], GetBoundGpuDevice()));

        for (i = 0, addr = m_vidGpuAddr[memIndex]; i < pageCount; i++, addr+= pageSize)
        {
            pPteBlock = NULL;
            getParams.gpuAddr = addr;
            CHECK_RC(GetPteBlock(&getParams, &pPteBlock));

            // Check if Kind is compressible
            fbIsKindParams.kind = pPteBlock->kind;
            fbIsKindParams.operation = LW2080_CTRL_FB_IS_KIND_OPERATION_COMPRESSIBLE;
            CHECK_RC(pLwRm->Control(m_hSubDev,
                            LW2080_CTRL_CMD_FB_IS_KIND,
                            &fbIsKindParams,
                            sizeof (fbIsKindParams)));

            if (pPteBlock->comptagLine || fbIsKindParams.result)
            {
                Printf(Tee::PriHigh, "After releasing compression , comptagline and Kind should             \
                                      reflect that there is no compression for the page\n");
                return RC::LWRM_ILWALID_DATA;
            }

        }

        //
        // Reacquire ComptagLines for this Handle
        //
        CHECK_RC(pLwRm->VidHeapReacquireCompression(m_hVidMem[memIndex], GetBoundGpuDevice()));

        for (i = 0, addr = m_vidGpuAddr[memIndex]; i < pageCount; i++, addr+= pageSize)
        {
            pPteBlock = NULL;
            getParams.gpuAddr = addr;
            CHECK_RC(GetPteBlock(&getParams, &pPteBlock));

            //
            // Verify that we get the same comptaglines after reacquire as original one.
            //
            if (pPteBlock->comptagLine != ctagsToHandle[i])
            {
                Printf(Tee::PriHigh, "After reacquire comptaglines should be same as original one.\n");
                return RC::LWRM_ILWALID_DATA;
            }

            //
            // Verify its Comptag Uniqueness
            //
            CHECK_RC(VerifyCompTagLine(pPteBlock, !(addr%(128*1024))));
        }
        delete [] ctagsToHandle;
    }

    return rc;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------
//! \brief VerifyCompTagLine: Verify the Comptag uniqueness
//!
//! \return OK if it as per rule otherwise Fail.
//-----------------------------------------------------------------------------

RC VerifCompTagTest::VerifyCompTagLine(LW0080_CTRL_DMA_PTE_INFO_PTE_BLOCK *pPteBlock, bool alignment)
{
    CTAGDATA *node = NULL, *tempNode = NULL, *prevNode = NULL;
    RC rc;

    tempNode = head;

    //
    // Search if this comptag already exists
    //
    while (tempNode != NULL)
    {
        if (tempNode->ctagLine == pPteBlock->comptagLine)
        {
            // For 128kb alignment there should be a new comptagline
            if (alignment)
            {
                Printf(Tee::PriHigh, "ERROR :Same comptagline assign to different 128kb align page\n");
                return RC::LWRM_ILWALID_DATA;
            }
            break;
        }

        prevNode = tempNode;
        tempNode = tempNode->next;
    }

    //
    // If Compatgline is new, create a new node for it and save its data
    //
    if (tempNode == NULL)
    {
        node = new CTAGDATA;
        node->ctagLine = pPteBlock->comptagLine;
        node->kind = pPteBlock->kind;
        node->totalOnLine = pPteBlock->pageSize;
        node->next = NULL;

        if (prevNode == NULL)
        {
            head = node;
            return rc;
        }
        else
        {
            prevNode->next = node;
            return rc;
        }
    }
    //
    // Else verify its uniqueness
    //
    else
    {
        if (tempNode->kind == pPteBlock->kind)
        {
            if ((tempNode->totalOnLine + pPteBlock->pageSize) <= COMPTAG_LINE_COVERAGE_FERMI)
            {
                tempNode->totalOnLine += pPteBlock->pageSize;
                Printf(Tee::PriLow, "Data Assigned to Comptag %d = %d\n", tempNode->ctagLine, (UINT32)tempNode->totalOnLine);
                return rc;
            }
        }
        Printf(Tee::PriHigh, "Comptag Line Assignmet violates its assignment rule\n");
        return RC::LWRM_ILWALID_DATA;
    }
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------
//! \brief GetPteBlock : Get the Valid PTE BLock
//!
//! \return OK if we get it
//-----------------------------------------------------------------------------

RC VerifCompTagTest::GetPteBlock(LW0080_CTRL_DMA_GET_PTE_INFO_PARAMS *getParams, LW0080_CTRL_DMA_PTE_INFO_PTE_BLOCK **validPteBlock)
{
    RC rc;
    UINT32 i;
    LwRmPtr pLwRm;

    CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                            LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                            getParams,
                            sizeof (*getParams)));

    //
    // Because there can be multiple (2) PTE_INFO_PTE_BLOCKs, need to find which
    // one is actually the valid one, there should be only one for now.
    //

    for (i = 0; i < LW0080_CTRL_DMA_GET_PTE_INFO_PTE_BLOCKS; i++)
    {
        if (DRF_VAL(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_VALID, getParams->pteBlocks[i].pteFlags)
                    == LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_VALID_TRUE)
        {
            *validPteBlock = (getParams->pteBlocks) + i;
            break;
        }
    }
    if (*(validPteBlock) == NULL)
    {
        Printf(Tee::PriHigh, "If Memory is Allocated there should be a valid pointer of its PTE\n");
        return RC::LWRM_ILWALID_DATA;
    }
    return rc;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(VerifCompTagTest, RmTest,
    "Verfy the comptag line assignment logic");

