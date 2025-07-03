/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2016,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_sparseaccess.cpp
//! \brief Test the sparse + non naïve clients like CE and make sure the MMU doesn’t fault and we read back zeroes.
//!
//! Test the following scenarios
//! 1) Copy from sparse to a mapped memory region (Surface2D)
//!    - Should not fault
//!    - Should overwrite everything in the mapped memory region with 0's.
//!
//! 2) Copy from sparse to another sparse region
//!    - Should not fault
//!    - Should have no other effects
//!
//! 3) Copy from a mapped memory region (Surface2D) to sparse
//!    - Should not fault
//!    - Should not have any effect on the sparse, we should only get 0's if we try to read from it
//!
//! 4) Copy from a mapped memory region (Surface2D) to a partially mapped sparse
//!    - Should not fault
//!    - Should not have any effect on the unmapped area in sparse, we should only get 0's if we try to read from it
//!    - Should overwrite the mapped area in sparse, we should get the expected values if we try to read from it
//!
//! 5) Copy from a mapped memory region (Surface2D) to a fully mapped sparse
//!    - Should not fault
//!    - Should overwrite the entire area in sparse (since it's fully mapped), we should get the expected values if we try to read from it
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include <string> // Only use <> for built-in C++ stuff

#include "core/include/channel.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/lwrm.h"
#include "lwRmApi.h"
#include "core/include/platform.h"
#include "gpu/utility/surf2d.h"
#include "core/include/utility.h"

#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl208f.h"

#include "class/cl90b5.h"
#include "class/cl90b5sw.h"
#include "class/cla0b5.h"
#include "class/cla06fsubch.h"
#include "class/cl0000.h"

#include "core/include/memcheck.h"

#define SEMA_CODE       0xA1B2C3D4 // magic # that sema is to return in notifier
#define KB              1024
#define MB              (1024 * 1024)

class SparseAccessTest : public RmTest
{
public:
    SparseAccessTest();
    virtual ~SparseAccessTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    // Channel
    Channel     *_pCh;
    LwRm::Handle _hCh;
    LwU32        _subCh;

    // CE
    LwRm::Handle _hCeObj;
    LwRm::Handle _hEngClass;

    GpuSubdevice *_masterGpu;

    RC _AllocateMemory(UINT32 *pAttr,
                       UINT32 *pAttr2,
                       UINT32 *pFlags,
                       UINT32 *pHandle,
                       UINT64 *pOffset,
                       UINT64 *pAlignment,
                       UINT64 *pSize,
                       LW208F_CTRL_DMA_GET_VAS_BLOCK_DETAILS_PARAMS *pParams);

    RC _InstantiateCe(LwRm::Handle *pHCeObj);

    RC _GetAllSupportedEngines(LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS *pEngineParams);

    RC _GetAllSupportedClasses(LW2080_CTRL_GPU_GET_ENGINE_CLASSLIST_PARAMS *pClassParams,
                               LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS       *pEngineParams);

    RC _PruneEngineList(LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS *pEngineParams,
                        const LwU32                           *pEnginesToKeep,
                        LwU32                                  keepEnginesCount);

    RC _PruneClassList(LW2080_CTRL_GPU_GET_ENGINE_CLASSLIST_PARAMS *pClassParams,
                       const LwRm::Handle                          *pClassesToKeep,
                       LwU32                                        keepClassesCount);

    RC _ClassToEngine(LwRm::Handle engineClass, LwU32 *engine);

    RC _CheckForIlwalidAndSparse(UINT64 fromAddr, UINT64 throughAddr,
                                 bool bSmallPageSizeTableSparse,
                                 bool bBigPageSizeTableSparse,
                                 bool bHugePageSizeTableSparse);

    RC _CeCopyBlock(LwRm::Handle hLwrCeObj, LwU32 locSubChannel,
        LwU64 source, Memory::Location srcLoc, Surface2D* pLocSrc,
        LwU64 dest, Memory::Location destLoc, Surface2D* pLocDest,
        LwU64 blockSize, Surface2D* pLocSema);

    RC _AllocSparse(UINT32 *handle, UINT64 *offset, UINT64 *size,
                    UINT32 bSmallPage, bool bHugePageSizeTableSparse);
    RC _AllocPhysical(UINT32 *handle, UINT64 *offset, UINT64 *size, UINT32 bSmallPage);

    RC _AllocSurface(Surface2D *surf, UINT64 size);

    RC _WaitUnblock(Surface2D *sema);

    RC _Test(Surface2D *src,  UINT64 srcAddr,  UINT64 srcOffset,
             Surface2D *dest, UINT64 destAddr, UINT64 destOffset,
             UINT64 size,
             UINT32 expVal,
             UINT32 origVal);

    RC _TestSparseToMapped(UINT32 bSmallPage, UINT64 size, // total size of allocation
                           UINT64 chunkSize,               // size of chunk to be copied
                           bool bHugePageSizeTableSparse);
    RC _TestSparseToSparse(UINT32 bSmallPage, UINT64 size, // total size of allocation
                           UINT64 chunkSize,               // size of chunk to be copied
                           bool bHugePageSizeTableSparse);
    RC _TestMappedToPartialMappedSparse(UINT32 bSmallPage, UINT64 virtSize,
                                        bool bHugePageSizeTableSparse);
};

//! \brief Constructor
//!
//! \sa Setup
//------------------------------------------------------------------------------
SparseAccessTest::SparseAccessTest()
{
    _pCh   = 0;
    _hCh   = 0;
    _subCh = 0;
    _masterGpu = 0;
    _hCeObj    = 0;
    _hEngClass = 0;

    SetName("SparseAccessTest");
}

//! \brief Destructor
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
SparseAccessTest::~SparseAccessTest()
{

}

//! \brief Test if the test is supported
//!
//! \return RUN_RMTEST_TRUE if the test can be run in the current environment,
//!         skip reason otherwise
//------------------------------------------------------------------------------
string SparseAccessTest::IsTestSupported()
{
    LwRmPtr          pLwRm;
    GpuDevice       *pGpuDev;
    LwRm::Handle     hGpuDev;

    LW0080_CTRL_DMA_GET_CAPS_PARAMS paramsCaps = {0};

    pGpuDev      = GetBoundGpuDevice();
    hGpuDev      = pLwRm->GetDeviceHandle(pGpuDev);

    paramsCaps.capsTblSize = LW0080_CTRL_DMA_CAPS_TBL_SIZE;
    RC rc = pLwRm->Control(hGpuDev,
                           LW0080_CTRL_CMD_DMA_GET_CAPS,
                          &paramsCaps,
                           sizeof(paramsCaps));
    MASSERT(rc == OK);
    if (rc != OK)
    {
        return "LW0080_CTRL_CMD_DMA_GET_CAPS failed! Cannot determine whether the test can run.";
    }

    if(!(LW0080_CTRL_GR_GET_CAP(paramsCaps.capsTbl,
                                LW0080_CTRL_DMA_CAPS_SPARSE_VIRTUAL_SUPPORTED)))
    {
        return "Sparse virtual address ranges are not supported. Capability disabled.";
    }

    return RUN_RMTEST_TRUE;
}

//! \brief Intiialize channel and an instance to the copy engine
//!
//! \return RC structure
//------------------------------------------------------------------------------
RC SparseAccessTest::Setup()
{
    RC rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    // Setup the basic stuff
    _masterGpu = GetBoundGpuSubdevice();

    // Allocate a channel for CE
    _subCh = LWA06F_SUBCHANNEL_COPY_ENGINE;
    CHECK_RC_CLEANUP(m_TestConfig.AllocateChannel(&_pCh, &_hCh, LW2080_ENGINE_TYPE_COPY(0)));

    // Instantiate CE
    CHECK_RC_CLEANUP(_InstantiateCe(&_hCeObj));

Cleanup:
    return rc;
}

//! \brief Run all the tests
//!
//! \return RC structure
//------------------------------------------------------------------------------
RC SparseAccessTest::Run()
{
    const GpuDevice *pGpuDev = GetBoundGpuDevice();
    RC rc;

    // Big Pages
    CHECK_RC(_TestSparseToMapped(0, 512 * KB, 128 * KB, LW_FALSE));

    CHECK_RC(_TestSparseToSparse(0, 512 * KB, 128 * KB, LW_FALSE));

    CHECK_RC(_TestMappedToPartialMappedSparse(0, 512 * KB, LW_FALSE));

    // Small Pages
    CHECK_RC(_TestSparseToMapped(1, 512 * KB, 128 * KB, LW_FALSE));

    CHECK_RC(_TestSparseToSparse(1, 512 * KB, 128 * KB, LW_FALSE));

    CHECK_RC(_TestMappedToPartialMappedSparse(1, 512 * KB, LW_FALSE));

    //
    // Huge Pages. Requesting virtual allocation of 2MB with DEFAULT flag will
    // trigger 2MB PTE usage
    //
    if (pGpuDev->SupportsPageSize(GpuDevice::PAGESIZE_HUGE))
    {
        CHECK_RC(_TestSparseToMapped(0, 2 * MB, 512 * KB, LW_TRUE));

        CHECK_RC(_TestSparseToSparse(0, 2 * MB, 512 * KB, LW_TRUE));

        CHECK_RC(_TestMappedToPartialMappedSparse(0, 2 * MB, LW_TRUE));
    }

    return rc;
}

//! \brief Cleanup the channel
//!
//! \return RC structure
//------------------------------------------------------------------------------
RC SparseAccessTest::Cleanup()
{
    m_TestConfig.FreeChannel(_pCh);

    return OK;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//! \brief Allocate Memory
//!
//! \return RC Structure
RC
SparseAccessTest::_AllocateMemory
(
    UINT32 *pAttr,
    UINT32 *pAttr2,
    UINT32 *pFlags,
    UINT32 *pHandle,
    UINT64 *pOffset,
    UINT64 *pAlignment,
    UINT64 *pSize,
    LW208F_CTRL_DMA_GET_VAS_BLOCK_DETAILS_PARAMS *pParams
)
{
    LwRmPtr       pLwRm;
    GpuDevice    *pGpuDev;
    GpuSubdevice *pGpuSubDev;
    LwRm::Handle  hGpuSubDevDiag;
    RC            rc;

    pGpuDev        = GetBoundGpuDevice();
    pGpuSubDev     = GetBoundGpuSubdevice();
    hGpuSubDevDiag = pGpuSubDev->GetSubdeviceDiag();

    // Check for required arguments.
    if (!pAttr || !pAttr2 || !pFlags || !pHandle || !pOffset)
    {
        return RC::SOFTWARE_ERROR;
    }

    // Denote detail of the memory to be allocated.
    Printf(Tee::PriHigh, "Trying to Allocate %llu Byte(s) ", *pSize);

    if (*pFlags & LWOS32_ALLOC_FLAGS_VIRTUAL)
    {
        Printf(Tee::PriHigh, "Of Virtual Address Range ");
    }
    else
    {
        Printf(Tee::PriHigh, "Of Physical Address Range ");
    }

    Printf(Tee::PriHigh, "At Offset 0x%llx ", *pOffset);

    if (FLD_TEST_DRF(OS32, _ATTR, _PAGE_SIZE, _DEFAULT, *pAttr))
    {
        Printf(Tee::PriHigh, "With Page Size Default");
    }
    else if (FLD_TEST_DRF(OS32, _ATTR, _PAGE_SIZE, _4KB, *pAttr))
    {
        Printf(Tee::PriHigh, "With Page Size 4 KB");
    }
    else if (FLD_TEST_DRF(OS32, _ATTR, _PAGE_SIZE, _BIG, *pAttr))
    {
        Printf(Tee::PriHigh, "With Page Size Big");
    }
    else
    {
        MASSERT(0);
    }
    Printf(Tee::PriHigh, "...\n");

    // Allocate the memory with the given arguments.
    CHECK_RC(pLwRm->VidHeapAllocSizeEx(LWOS32_TYPE_IMAGE, // Type
                                      *pFlags,            // Flags
                                       pAttr,             // pAttr
                                       pAttr2,            // pAttr2
                                      *pSize,             // Size
                                      *pAlignment,        // Alignment
                                       NULL,              // pFormat
                                       NULL,              // pCoverage
                                       NULL,              // pPartitionStride
                                       0,                 // CtagOffset
                                       pHandle,           // pHandle
                                       pOffset,           // pOffset
                                       NULL,              // pLimit
                                       NULL,              // pRtnFree
                                       NULL,              // pRtnTotal
                                       0,                 // width
                                       0,                 // height
                                       pGpuDev));         // gpudev

    Printf(Tee::PriHigh, "ALLOCATED!!\n");
    if (*pFlags & LWOS32_ALLOC_FLAGS_VIRTUAL)
    {
        //
        // For virtual allocations, retrieve details about the memory "actually"
        // allocated.
        //
        pParams->virtualAddress = *pOffset;
        CHECK_RC(pLwRm->Control(hGpuSubDevDiag,
                                LW208F_CTRL_CMD_DMA_GET_VAS_BLOCK_DETAILS,
                                pParams,
                                sizeof(*pParams)));

        Printf(Tee::PriHigh, "Details About Actual Allocated Virtual Address Range...\n"                  );
        Printf(Tee::PriHigh, "* Begin Address        : 0x%llx                 \n", pParams->beginAddress  );
        Printf(Tee::PriHigh, "* End Address          : 0x%llx                 \n", pParams->endAddress    );
        Printf(Tee::PriHigh, "* Aligned Address      : 0x%llx                 \n", pParams->alignedAddress);
        Printf(Tee::PriHigh, "                                                \n"                         );

        MASSERT(pParams->beginAddress <= pParams->alignedAddress);
        MASSERT(pParams->alignedAddress == *pOffset &&
               *pOffset <= pParams->endAddress);
    }
    else
    {
        //
        // For non-virtual allocations, the caller should not have passed in a
        // pParams argument since it will not be used.
        //
        MASSERT(pParams == NULL);
    }

    return OK;
}

//! \brief Setup CE
//!
//! 1). get all the classes the device supports
//! 2). keep only the classes we want
//! 3). control call to instantiate CE
//!
//! \return RC structure
RC
SparseAccessTest::_InstantiateCe
(
    LwRm::Handle *pHCeObj
)
{
    RC rc = OK;
    LwRmPtr pLwRm;

    LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS       engineParams = {0};
    LW2080_CTRL_GPU_GET_ENGINE_CLASSLIST_PARAMS classParams  = {0};
    LW90B5_ALLOCATION_PARAMETERS                allocParams  = {0};

    LwU32        engineType  = 0;
    LwRm::Handle engineClass = 0;

    const LwU32 enginesToKeep[] = {
        LW2080_ENGINE_TYPE_COPY0,
        LW2080_ENGINE_TYPE_COPY1,
        LW2080_ENGINE_TYPE_COPY2,
    };
    const LwRm::Handle classesToKeep[] = {
        KEPLER_DMA_COPY_A,
        GF100_DMA_COPY,
    };

    CHECK_RC_CLEANUP(_GetAllSupportedEngines(&engineParams));
    CHECK_RC_CLEANUP(_PruneEngineList(
                &engineParams, enginesToKeep,
                sizeof(enginesToKeep) / sizeof(LwU32)));
    CHECK_RC_CLEANUP(_GetAllSupportedClasses(&classParams, &engineParams));
    CHECK_RC_CLEANUP(_PruneClassList(
                &classParams, classesToKeep,
                sizeof(classesToKeep) / sizeof(LwRm::Handle)));

    engineClass = ((LwU32*)classParams.classList)[0];
    _ClassToEngine(engineClass, &engineType);
    allocParams.version = 0;
    allocParams.engineInstance = engineType - LW2080_ENGINE_TYPE_COPY0;
    CHECK_RC_CLEANUP(pLwRm->Alloc(_hCh, &_hCeObj, engineClass, &allocParams));

    _hEngClass = engineClass;

Cleanup:
    delete[] (LwU32*)classParams.classList;

    return rc;
}

//! \brief populates engineParams with all engines supported by master GPU
//!
//! \return RC structure
RC
SparseAccessTest::_GetAllSupportedEngines
(
    LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS *pEngineParams
)
{
    RC rc;
    LwRmPtr pLwRm;

    // find out the number of engines
    CHECK_RC_CLEANUP(pLwRm->Control(
                pLwRm->GetSubdeviceHandle(_masterGpu),
                LW2080_CTRL_CMD_GPU_GET_ENGINES_V2,
                pEngineParams,
                sizeof(*pEngineParams)));

Cleanup:
    return rc;
}

//! \brief ClassParams with all engine classes across all engines
//!        supported by the master GPU
//
//! WARNING: flattens class list from all engines.
//
//! \return RC structure
RC
SparseAccessTest::_GetAllSupportedClasses
(
    LW2080_CTRL_GPU_GET_ENGINE_CLASSLIST_PARAMS *pClassParams,
    LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS       *pEngineParams
)
{
    RC rc;
    LwRmPtr pLwRm;
    LwU32  totalClassCount = 0;
    LwU32  counter = 0;
    LwU32 *classList = 0;
    LwU32 *engineList = (LwU32*)pEngineParams->engineList;

    // return error if no engiens supported.
    if (pEngineParams->engineCount == 0)
    {
        return RC::BAD_PARAMETER;
    }

    // figure out how many classes there are across all engines
    for (LwU32 i = 0; i < pEngineParams->engineCount; ++i) {
        pClassParams->engineType = engineList[i];
        pClassParams->numClasses = 0;
        pClassParams->classList = 0;

        CHECK_RC_CLEANUP(pLwRm->Control(
                    pLwRm->GetSubdeviceHandle(_masterGpu),
                    LW2080_CTRL_CMD_GPU_GET_ENGINE_CLASSLIST,
                    pClassParams,
                    sizeof(LW2080_CTRL_GPU_GET_ENGINE_CLASSLIST_PARAMS)));
        totalClassCount += pClassParams->numClasses;
    }

    // allocate enough space for all the classes
    classList = new LwU32[totalClassCount];

    // populate the class list with all the classes from all engiens supported
    for (LwU32 i = 0; i < pEngineParams->engineCount; ++i) {
        pClassParams->engineType = engineList[i];
        pClassParams->classList = LW_PTR_TO_LwP64(classList + counter);

        CHECK_RC_CLEANUP(pLwRm->Control(
                    pLwRm->GetSubdeviceHandle(_masterGpu),
                    LW2080_CTRL_CMD_GPU_GET_ENGINE_CLASSLIST,
                    pClassParams,
                    sizeof(LW2080_CTRL_GPU_GET_ENGINE_CLASSLIST_PARAMS)));

        counter += pClassParams->numClasses;
    }

    pClassParams->classList = LW_PTR_TO_LwP64(classList);

Cleanup:
    return rc;
}

//! \brief removes any undesired engines from engineList
//!
//! \return RC structure
RC
SparseAccessTest::_PruneEngineList
(
    LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS *pEngineParams,
    const LwU32 *pEnginesToKeep,
    LwU32 keepEnginesCount
)
{
    LwU32 *engineList = (LwU32*)pEngineParams->engineList;

    if (keepEnginesCount <= 0)
    {
        return RC::BAD_PARAMETER;
    }

    for (INT32 i = pEngineParams->engineCount - 1; i >= 0; --i)
    {
        LwU32 j;
        // find the engine in the list
        for (j = 0; j < keepEnginesCount; ++j)
        {
            if (pEnginesToKeep[j] == engineList[i])
            {
                break;
            }
        }

        // if the engine isn't found, remove it
        if (j >= keepEnginesCount) {
            LwU32 temp = engineList[i];
            engineList[i] = engineList[pEngineParams->engineCount - 1];
            engineList[pEngineParams->engineCount - 1] = temp;
            --pEngineParams->engineCount;
        }
    }

    return OK;
}

//! \brief removes any undesired engine classes from classList
//!
//! \return RC structure
RC
SparseAccessTest::_PruneClassList
(
    LW2080_CTRL_GPU_GET_ENGINE_CLASSLIST_PARAMS *pClassParams,
    const LwRm::Handle *pClassesToKeep,
    LwU32 keepClassesCount
)
{
    LwU32 *classList = (LwU32*)pClassParams->classList;

    if (keepClassesCount <= 0)
    {
        return RC::BAD_PARAMETER;
    }

    for (INT32 i = pClassParams->numClasses - 1; i >= 0; --i)
    {
        LwU32 j;
        // find class in the list
        for (j = 0; j < keepClassesCount; ++j)
        {
            if (pClassesToKeep[j] == classList[i])
            {
                break;
            }
        }

        // if the engine isn't found, remove it
        if (j >= keepClassesCount) {
            LwU32 temp = classList[i];
            classList[i] = classList[pClassParams->numClasses - 1];
            classList[pClassParams->numClasses - 1] = temp;
            --pClassParams->numClasses;
        }
    }

    return OK;
}

//! \brief Obtain supported engine given a supported class
//!
//! Engine type is stored in *engine. Puts LW2080_ENGINE_TYPE_NULL into
//! *engine if class->engine mapping cannot be determined.
//!
//! \return RC structure
RC
SparseAccessTest::_ClassToEngine
(
    LwRm::Handle engineClass,
    LwU32       *engine
)
{
    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS locEngineParams = {0};
    LW2080_CTRL_GPU_GET_ENGINE_CLASSLIST_PARAMS locClassParams = {0};
    vector<LwU32> locClassList;

    // step 1: query the supported engines
    CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(_masterGpu),
                    LW2080_CTRL_CMD_GPU_GET_ENGINES_V2,
                    &locEngineParams,
                    sizeof(locEngineParams)));

    // step 4: choose an engine to get all the classes for
    for (LwU32 i = 0; i < locEngineParams.engineCount; i++)
    {
        // step 5: get all the classes for the selected engine
        locClassParams.engineType = locEngineParams.engineList[i];
        locClassParams.numClasses = 0;
        locClassParams.classList = 0;
        CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(_masterGpu),
                        LW2080_CTRL_CMD_GPU_GET_ENGINE_CLASSLIST,
                        &locClassParams, sizeof(locClassParams)));

        locClassList.resize(locClassParams.numClasses);
        locClassParams.classList = LW_PTR_TO_LwP64(&(locClassList[0]));

        CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(_masterGpu),
                LW2080_CTRL_CMD_GPU_GET_ENGINE_CLASSLIST,
                &locClassParams, sizeof(locClassParams)));

        // step 6: check if our class is in that engine
        for (LwU32 j = 0; j < locClassParams.numClasses; j++)
        {
            if (engineClass == locClassList[j])
            {
                *engine = locEngineParams.engineList[i];
                return rc;
            }
        }
    }

Cleanup:
    *engine = LW2080_ENGINE_TYPE_NULL;
    return rc;
}

//! \brief Check if a memory region is actually sparse
//
//! \return RC structure
RC
SparseAccessTest::_CheckForIlwalidAndSparse
(
    UINT64 fromAddr,
    UINT64 throughAddr,
    bool   bSmallPageSizeTableSparse,
    bool   bBigPageSizeTableSparse,
    bool   bHugePageSizeTableSparse
)
{
    LwRmPtr       pLwRm;
    GpuDevice    *pGpuDev;
    LwRm::Handle  hGpuDev;
    UINT64        addr;
    UINT32        i;
    UINT64        pageSize;
    UINT64        bigPageSize;
    RC            rc;

    LW0080_CTRL_DMA_GET_PTE_INFO_PARAMS params = {0};

    pGpuDev = GetBoundGpuDevice();
    hGpuDev = pLwRm->GetDeviceHandle(pGpuDev);
    bigPageSize = pGpuDev->GetBigPageSize();

    //
    // For each page table size, ensure that the sparse bits are set (or not
    // set) for the given address range.
    //
    for (i = 0; i < LW0080_CTRL_DMA_GET_PTE_INFO_PTE_BLOCKS; i++)
    {
        //
        // Do an initial retrieval of PTE information for "fromAddr" in order
        // to get the page size.
        //
        params.gpuAddr         = fromAddr;
        params.skipVASpaceInit = true;
        CHECK_RC(pLwRm->Control(hGpuDev,
                                LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                               &params,
                                sizeof(params)));

        //
        // The LW0080_CTRL_CMD_DMA_GET_PTE_INFO control call will leave the
        // page size at 0 if the page tables for this iteration of "i" do not
        // exist for the given address (i.e. small page page tables are used
        // but not the big page page tables...). If that happens, skip to
        // checking the next page size.
        //
        pageSize = params.pteBlocks[i].pageSize;
        if (pageSize == 0)
        {
            continue;
        }

        Printf(Tee::PriHigh, "Checking PTEs wth Page Size: 0x%llx\n", pageSize);

        //
        // Align "fromAddr" and "throughAddr" to the page size so that PTE
        // entries are checked on page-aligned boundaries.
        //
        fromAddr    &= ~(pageSize - 1);
        throughAddr &= ~(pageSize - 1);

        // Check for "sparse" from "fromAddr" through "throughAddr"...
        Printf(Tee::PriHigh,
               "    Are PTEs between 0x%llx - 0x%llx (inclusive) Invalid and ",
               fromAddr,
               throughAddr);
        if (((pageSize == GpuDevice::PAGESIZE_SMALL) && bSmallPageSizeTableSparse) ||
            ((pageSize == bigPageSize) && bBigPageSizeTableSparse) ||
            ((pageSize == GpuDevice::PAGESIZE_HUGE) && bHugePageSizeTableSparse))
        {
            Printf(Tee::PriHigh,"Sparse? ... ");
        }
        else
        {
            Printf(Tee::PriHigh,"Non-Sparse? ... ");
        }
        for (addr = fromAddr; addr <= throughAddr; addr += pageSize)
        {
            // Retrieve the PTE flags for "addr".
            params.gpuAddr         = addr;
            params.skipVASpaceInit = true;
            CHECK_RC(pLwRm->Control(hGpuDev,
                                    LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                                   &params,
                                    sizeof(params)));

            if (!FLD_TEST_DRF(0080_CTRL,
                              _DMA_PTE_INFO_PARAMS_FLAGS,
                              _VALID,
                              _FALSE,
                              params.pteBlocks[i].pteFlags))
            {
                // All sparse address range PTEs should be invalid.
                Printf(Tee::PriHigh,
                       "    No, Address 0x%llx has an issue!!!\n",
                       addr);
                return RC::SOFTWARE_ERROR;
            }
            else if ((pageSize == GpuDevice::PAGESIZE_SMALL) &&
                      ((bSmallPageSizeTableSparse &&
                       !FLD_TEST_DRF(0080_CTRL,
                                    _DMA_PTE_INFO_PARAMS_FLAGS,
                                    _GPU_CACHED,
                                    _FALSE,
                                    params.pteBlocks[i].pteFlags)) ||
                      (!bSmallPageSizeTableSparse &&
                       FLD_TEST_DRF(0080_CTRL,
                                    _DMA_PTE_INFO_PARAMS_FLAGS,
                                    _GPU_CACHED,
                                    _FALSE,
                                    params.pteBlocks[i].pteFlags))))
            {
                //
                // All sparse address range small page size PTEs should be
                // marked "sparse".
                //
                Printf(Tee::PriHigh,
                       "    No, Address 0x%llx has an issue!!!\n",
                       addr);
                return RC::SOFTWARE_ERROR;
            }
            else if ((pageSize == bigPageSize) &&
                      ((bBigPageSizeTableSparse &&
                        !FLD_TEST_DRF(0080_CTRL,
                                      _DMA_PTE_INFO_PARAMS_FLAGS,
                                      _GPU_CACHED,
                                      _FALSE,
                                      params.pteBlocks[i].pteFlags)) ||
                       (!bBigPageSizeTableSparse &&
                        FLD_TEST_DRF(0080_CTRL,
                                     _DMA_PTE_INFO_PARAMS_FLAGS,
                                     _GPU_CACHED,
                                     _FALSE,
                                     params.pteBlocks[i].pteFlags))))
            {
                //
                // Depending on whether or not the corresponding small page
                // size PTEs of a big page size PTE have been set to valid once
                // before, the big page size PTE may no longer be tracking the
                // sparse bit. Instead, it may be relying on the small page
                // size PTEs to denote sparse.
                //
                Printf(Tee::PriHigh,
                       "    No, Address 0x%llx has an issue!!!\n",
                       addr);
                return RC::SOFTWARE_ERROR;
            }
            else if ((pageSize == GpuDevice::PAGESIZE_HUGE) &&
                      ((bHugePageSizeTableSparse &&
                        !FLD_TEST_DRF(0080_CTRL,
                                      _DMA_PTE_INFO_PARAMS_FLAGS,
                                      _GPU_CACHED,
                                      _FALSE,
                                      params.pteBlocks[i].pteFlags)) ||
                       (!bHugePageSizeTableSparse &&
                        FLD_TEST_DRF(0080_CTRL,
                                     _DMA_PTE_INFO_PARAMS_FLAGS,
                                     _GPU_CACHED,
                                     _FALSE,
                                     params.pteBlocks[i].pteFlags))))
            {
                Printf(Tee::PriHigh,
                       "    No, Address 0x%llx has an issue!!!\n",
                       addr);
                return RC::SOFTWARE_ERROR;
            }
        }
        Printf(Tee::PriHigh, "Yes\n");
    }

    return OK;
}

//! \brief CE driven copy of a block of 1D memory (either FB, coherent, or non-coherent)
//!
//! \return RC structure
//------------------------------------------------------------------------------
RC
SparseAccessTest::_CeCopyBlock
(
    LwRm::Handle hLwrCeObj, LwU32 locSubChannel,
    LwU64 source, Memory::Location srcLoc, Surface2D* pLocSrc,
    LwU64 dest, Memory::Location destLoc, Surface2D* pLocDest,
    LwU64 blockSize, Surface2D* pLocSema
)
{
    LwU32 addrTypeFlags = 0;

    // step 1: set the src and destination addresses
    _pCh->SetObject(locSubChannel, hLwrCeObj);
    _pCh->Write(locSubChannel, LWA0B5_OFFSET_IN_UPPER, LwU64_HI32(source));
    _pCh->Write(locSubChannel, LWA0B5_OFFSET_IN_LOWER, LwU64_LO32(source));

    _pCh->Write(locSubChannel, LWA0B5_OFFSET_OUT_UPPER, LwU64_HI32(dest));
    _pCh->Write(locSubChannel, LWA0B5_OFFSET_OUT_LOWER, LwU64_LO32(dest));

    //
    // step 2: set where the addresses point to (either FB, non-coherent sysmem,
    // or coherent sysmem
    //
    switch (srcLoc)
    {
        case Memory::Fb:
            _pCh->Write(locSubChannel, LWA0B5_SET_SRC_PHYS_MODE,
                        DRF_DEF(A0B5, _SET_SRC_PHYS_MODE, _TARGET, _LOCAL_FB));
            break;
        case Memory::NonCoherent:
            _pCh->Write(locSubChannel, LWA0B5_SET_SRC_PHYS_MODE,
                        DRF_DEF(A0B5, _SET_SRC_PHYS_MODE, _TARGET, _NONCOHERENT_SYSMEM));
            break;
        case Memory::Coherent:
            _pCh->Write(locSubChannel, LWA0B5_SET_SRC_PHYS_MODE,
                        DRF_DEF(A0B5, _SET_SRC_PHYS_MODE, _TARGET, _COHERENT_SYSMEM));
            break;
        default:
            return RC::BAD_PARAMETER;
    }

    switch (destLoc)
    {
        case Memory::Fb:
            _pCh->Write(locSubChannel, LWA0B5_SET_DST_PHYS_MODE,
                        DRF_DEF(A0B5, _SET_DST_PHYS_MODE, _TARGET, _LOCAL_FB));
            break;
        case Memory::NonCoherent:
            _pCh->Write(locSubChannel, LWA0B5_SET_DST_PHYS_MODE,
                        DRF_DEF(A0B5, _SET_DST_PHYS_MODE, _TARGET, _NONCOHERENT_SYSMEM));
            break;
        case Memory::Coherent:
            _pCh->Write(locSubChannel, LWA0B5_SET_DST_PHYS_MODE,
                        DRF_DEF(A0B5, _SET_DST_PHYS_MODE, _TARGET, _COHERENT_SYSMEM));
            break;
        default:
            return RC::BAD_PARAMETER;
    }

    //
    // step 3: set the necessary surface/block parameters based on what kind of
    // address was passed in.
    //
    if (0 == pLocSrc)
    {
        _pCh->Write(locSubChannel, LWA0B5_SET_SRC_DEPTH, 1);
        _pCh->Write(locSubChannel, LWA0B5_SET_SRC_LAYER, 0);
        _pCh->Write(locSubChannel, LWA0B5_SET_SRC_WIDTH, LwU64_LO32(blockSize));
        _pCh->Write(locSubChannel, LWA0B5_SET_SRC_HEIGHT, 1);
        _pCh->Write(locSubChannel, LWA0B5_LINE_LENGTH_IN, LwU64_LO32(blockSize));
    }
    else
    {
        _pCh->Write(locSubChannel, LWA0B5_PITCH_IN, pLocSrc->GetPitch());
        _pCh->Write(locSubChannel, LWA0B5_SET_SRC_DEPTH, 1);
        _pCh->Write(locSubChannel, LWA0B5_SET_SRC_LAYER, 0);
        _pCh->Write(locSubChannel, LWA0B5_SET_SRC_WIDTH, LwU64_LO32(blockSize));
        _pCh->Write(locSubChannel, LWA0B5_SET_SRC_HEIGHT, 1);
        _pCh->Write(locSubChannel, LWA0B5_LINE_LENGTH_IN, LwU64_LO32(blockSize));
    }
    addrTypeFlags |= DRF_DEF(A0B5, _LAUNCH_DMA, _SRC_TYPE, _VIRTUAL);

    if (0 == pLocDest)
    {
    }
    else
    {
        _pCh->Write(locSubChannel, LWA0B5_PITCH_OUT, pLocDest->GetPitch());
        _pCh->Write(locSubChannel, LWA0B5_SET_DST_DEPTH, 1);
        _pCh->Write(locSubChannel, LWA0B5_SET_DST_LAYER, 0);
        _pCh->Write(locSubChannel, LWA0B5_SET_DST_WIDTH, LwU64_LO32(pLocDest->GetSize()));
        _pCh->Write(locSubChannel, LWA0B5_SET_DST_HEIGHT, 1);
    }
    addrTypeFlags |= DRF_DEF(A0B5, _LAUNCH_DMA, _DST_TYPE, _VIRTUAL);

    // step 4: set how big the block of memory is
    _pCh->Write(locSubChannel, LWA0B5_LINE_COUNT, 1);

    // step 5: set the semaphores
    _pCh->Write(locSubChannel, LWA0B5_SET_SEMAPHORE_PAYLOAD, SEMA_CODE);

    _pCh->Write(locSubChannel, LWA0B5_SET_SEMAPHORE_A,
                DRF_NUM(A0B5, _SET_SEMAPHORE_A, _UPPER, LwU64_HI32(pLocSema->GetCtxDmaOffsetGpu())));
    _pCh->Write(locSubChannel, LWA0B5_SET_SEMAPHORE_B, LwU64_LO32(pLocSema->GetCtxDmaOffsetGpu()));

    // step 6: launch the CE
    _pCh->Write(locSubChannel, LWA0B5_LAUNCH_DMA,
            DRF_DEF(A0B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, _RELEASE_ONE_WORD_SEMAPHORE) |
            DRF_NUM(A0B5, _LAUNCH_DMA, _INTERRUPT_TYPE, LW90B5_LAUNCH_DMA_INTERRUPT_TYPE_NONE) |
            DRF_DEF(A0B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE) |
            DRF_DEF(A0B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH) |
            DRF_DEF(A0B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH) |
            DRF_DEF(A0B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _NON_PIPELINED) |
            addrTypeFlags);

    _pCh->Flush();

    return OK;
}

//! \brief Allocate a sparse region
//
//! \return RC structure
RC
SparseAccessTest::_AllocSparse
(
    UINT32 *handle,
    UINT64 *offset,
    UINT64 *size,
    UINT32  bSmallPage,
    bool bHugePageSizeTableSparse
)
{
    RC     rc = OK;
    UINT64 alignment;
    UINT32 attr;
    UINT32 attr2;
    UINT32 flags;

    LW208F_CTRL_DMA_GET_VAS_BLOCK_DETAILS_PARAMS params = {0};

    //
    // Note, a "default" page size results in the size getting rounded up to a
    // "big" page size.
    //
    // Furthermore, both the small and big page tables are allocated and
    // initialized to an invalid state for the allocated virtual address range.
    //
    if (bSmallPage)
    {
        attr = DRF_DEF(OS32, _ATTR, _PAGE_SIZE,   _4KB) |
               DRF_DEF(OS32, _ATTR, _LOCATION,    _ANY)  |
               DRF_DEF(OS32, _ATTR, _PHYSICALITY, _DEFAULT);
    }
    else
    {
        attr = DRF_DEF(OS32, _ATTR, _PAGE_SIZE,   _DEFAULT) |
               DRF_DEF(OS32, _ATTR, _LOCATION,    _ANY)  |
               DRF_DEF(OS32, _ATTR, _PHYSICALITY, _DEFAULT);
    }
    attr2     = LWOS32_ATTR_NONE;
    flags     = LWOS32_ALLOC_FLAGS_VIRTUAL | LWOS32_ALLOC_FLAGS_SPARSE;
    alignment = 0;
    CHECK_RC(_AllocateMemory(&attr,
                             &attr2,
                             &flags,
                              handle,
                              offset,
                             &alignment,
                              size,
                             &params));

    CHECK_RC(_CheckForIlwalidAndSparse(params.alignedAddress, params.endAddress,
                                       LW_TRUE, LW_TRUE, bHugePageSizeTableSparse));

    return rc;
}

//! \brief Allocate a physical region
//
//! \return RC structure
RC
SparseAccessTest::_AllocPhysical
(
    UINT32 *handle,
    UINT64 *offset,
    UINT64 *size,
    UINT32  bSmallPage
)
{
    RC     rc = OK;
    UINT64 alignment;
    UINT32 attr;
    UINT32 attr2;
    UINT32 flags;

    if (bSmallPage)
    {
        attr = DRF_DEF(OS32, _ATTR, _PAGE_SIZE,   _4KB) |
               DRF_DEF(OS32, _ATTR, _LOCATION,    _VIDMEM)  |
               DRF_DEF(OS32, _ATTR, _PHYSICALITY, _DEFAULT);
    }
    else
    {
        attr = DRF_DEF(OS32, _ATTR, _PAGE_SIZE,   _DEFAULT) |
               DRF_DEF(OS32, _ATTR, _LOCATION,    _VIDMEM)  |
               DRF_DEF(OS32, _ATTR, _PHYSICALITY, _DEFAULT);
    }
    attr2     = LWOS32_ATTR_NONE;
    flags     = 0;
    alignment = 0;
    CHECK_RC(_AllocateMemory(&attr,
                             &attr2,
                             &flags,
                              handle,
                              offset,
                             &alignment,
                              size,
                              0));

    return rc;
}

//! \brief Allocate a surface that works with copy engine
//
//! \return RC structure
RC
SparseAccessTest::_AllocSurface
(
    Surface2D *surf,
    UINT64     size
)
{
    RC rc = OK;

    surf->SetForceSizeAlloc(true);
    surf->SetArrayPitch(1);
    surf->SetArraySize(UNSIGNED_CAST(UINT32, size));
    surf->SetColorFormat(ColorUtils::Y8);
    surf->SetAddressModel(Memory::Paging);
    surf->SetLayout(Surface2D::Pitch);
    surf->SetLocation(Memory::Fb);
    CHECK_RC(surf->Alloc(GetBoundGpuDevice()));
    CHECK_RC(surf->Map());
    surf->Fill(0);
    Platform::FlushCpuWriteCombineBuffer();

    return rc;
}

//! \brief Wait for the semaphore to be released
//
//! \return RC structure
RC
SparseAccessTest::_WaitUnblock
(
    Surface2D *sema
)
{
    ModsEvent *pModsEvent = Tasker::AllocEvent();
    do
    {
        if (MEM_RD32(sema->GetAddress()) == SEMA_CODE)
            break;

    } while (Tasker::WaitOnEvent(pModsEvent, m_TestConfig.TimeoutMs()));
    Tasker::ResetEvent(pModsEvent);
    Tasker::FreeEvent(pModsEvent);

    return OK;
}

//! \brief Generic process to copy from src to dest and verify the success of copying.
//
// Generic Process
//  1) Verify if the original value in the entire destination memory region is correct
//  2) Copy from source to destination
//  3) Verify if the final value at [destOffset <--> destOffset + size] is correct
//  4) Verify if the final value at other places in dest is unchanged
//
//! \return RC structure
RC
SparseAccessTest::_Test
(
    Surface2D *src,  UINT64 srcAddr,  UINT64 srcOffset,
    Surface2D *dest, UINT64 destAddr, UINT64 destOffset,
    UINT64 size,        // size of data to copy
    UINT32 expVal,      // expected value at [destOffset <--> destOffset + size]
    UINT32 origVal      // original value in the entire destination memory region
)
{
    RC        rc    = OK;
    UINT32    value = 0;
    Surface2D sema;

    // Allocate for semaphore
    CHECK_RC(_AllocSurface(&sema, KB));
    sema.Fill(0x0);

    //
    // Pre Validation
    // Only do this, if the dest is a surface2D
    // Test the entire range of dest - if any value does not match origVal, return error
    //
    if (dest) {
        for (UINT32 i = 0; i < dest->GetSize() / sizeof(UINT32); ++i)
        {
            UINT32  val  = 0;
            UINT32 *addr = (UINT32*)dest->GetAddress() + i;

            val = MEM_RD32((void*)addr);

            if (val != origVal)
            {
                Printf(Tee::PriHigh,
                       "Issue found! At address 0x%llx, the value is 0x%x rather than 0x%x!\n",
                       (UINT64)addr, val, origVal);
                return RC::SOFTWARE_ERROR;
            }
        }
    }

    // Copying
    if (src) {
        value = MEM_RD32((void*)((UINT64)src->GetAddress() + (UINT64)srcOffset));
        Printf(Tee::PriHigh, "The source is a surface2D\n");
        Printf(Tee::PriHigh, "Before copy, the four bytes: 0x%x, at address: 0x%llx\n\n",
               value, (UINT64)src->GetAddress() + (UINT64)srcOffset);
    }
    if (dest) {
        value = MEM_RD32((void*)((UINT64)dest->GetAddress() + (UINT64)destOffset));
        Printf(Tee::PriHigh, "The destination is a surface2D\n");
        Printf(Tee::PriHigh, "Before copy, the four bytes: 0x%x, at address: 0x%llx\n\n",
               value, (UINT64)dest->GetAddress() + (UINT64)destOffset);
    }

    Printf(Tee::PriHigh, "Copying from src to dest... \n");

    CHECK_RC(_CeCopyBlock(
             _hCeObj,
              LWA06F_SUBCHANNEL_COPY_ENGINE,
              srcAddr  + srcOffset,  Memory::Fb, src,
              destAddr + destOffset, Memory::Fb, dest,
              size,
             &sema));

    CHECK_RC(_WaitUnblock(&sema));

    if (src) {
        value = MEM_RD32((void*)((UINT64)src->GetAddress() + (UINT64)srcOffset));
        Printf(Tee::PriHigh, "The source is a surface2D\n");
        Printf(Tee::PriHigh, "After copy, the four bytes: 0x%x, at address: 0x%llx\n\n",
               value, (UINT64)src->GetAddress() + (UINT64)srcOffset);
    }
    if (dest) {
        value = MEM_RD32((void*)((UINT64)dest->GetAddress() + (UINT64)destOffset));
        Printf(Tee::PriHigh, "The destination is a surface2D\n");
        Printf(Tee::PriHigh, "After copy, the four bytes: 0x%x, at address: 0x%llx\n\n",
               value, (UINT64)dest->GetAddress() + (UINT64)destOffset);
    }

    //
    // Post Validation
    // Only do this, if the dest is a surface2D
    // 1) Verify before the copied region, the value is not changed
    // 2) Verfiy in the copied region, the value is updated to expVal
    // 3) Verify after the copied region, the value is not changed
    //
    if (dest) {
        // 1)
        for (UINT32 i = 0; i < destOffset / sizeof(UINT32); ++i)
        {
            UINT32  val  = 0;
            UINT32 *addr = (UINT32*)dest->GetAddress() + i;

            val = MEM_RD32((void*)addr);

            if (val != origVal)
            {
                Printf(Tee::PriHigh,
                       "Issue found! At address 0x%llx, the value is 0x%x rather than 0x%x!\n",
                       (UINT64)addr, val, origVal);
                return RC::SOFTWARE_ERROR;
            }
        }
        // 2)
        for (UINT32 i = (UINT32)destOffset / sizeof(UINT32); i < (UINT32)(destOffset + size) / sizeof(UINT32); ++i)
        {
            UINT32  val  = 0;
            UINT32 *addr = (UINT32*)dest->GetAddress() + i;

            val = MEM_RD32((void*)addr);

            if (val != expVal)
            {
                Printf(Tee::PriHigh,
                       "Issue found! At address 0x%llx, the value is 0x%x rather than 0x%x!\n",
                       (UINT64)addr, val, expVal);
                return RC::SOFTWARE_ERROR;
            }
        }
        // 3)
        for (UINT32 i = (UINT32)(destOffset + size) / sizeof(UINT32); i < dest->GetSize() / sizeof(UINT32); ++i)
        {
            UINT32  val  = 0;
            UINT32 *addr = (UINT32*)dest->GetAddress() + i;

            val = MEM_RD32((void*)addr);

            if (val != origVal)
            {
                Printf(Tee::PriHigh,
                       "Issue found! At address 0x%llx, the value is 0x%x rather than 0x%x!\n",
                       (UINT64)addr, val, origVal);
                return RC::SOFTWARE_ERROR;
            }
        }
    }

    return rc;
}

//! \brief Copy from sparse to mapped memory region
//
// Generic Process
//  1) Allocate a 512KB/2MB sparse region and 512KB/2MB surface2D.
//  2) Populate the surface2D with some data
//  3) Copy from Sparse to this surface
//  4) Verify if the data on the surface was erased.
//
// Variations
//  1) Copy the entire 512KB/2MB over
//  2) Copy the first 128KB/512KB over
//  3) Copy the last 128KB/512KB over
//  4) Copy the middle 128KB/512KB over
//
//! \return RC structure
RC
SparseAccessTest::_TestSparseToMapped
(
    UINT32 bSmallPage,
    UINT64 size,
    UINT64 chunkSize,
    bool bHugePageSizeTableSparse
)
{
    RC        rc      = OK;
    LwRmPtr   pLwRm;
    UINT32    hSparse = 0;
    UINT64    offset  = 0;
    Surface2D dest;

    Printf(Tee::PriHigh, "Test: Copy Sparse to Mapped\n");

    // Allocate Sparse
    CHECK_RC(_AllocSparse(&hSparse, &offset, &size, bSmallPage,
                          bHugePageSizeTableSparse));

    // Allocate destination surface and surface for semaphore
    CHECK_RC(_AllocSurface(&dest, size));

    // Copy the entire 512KB/2MB sparse to the dest
    dest.Fill(0xff);
    CHECK_RC(_Test(0, offset, 0,
          &dest, dest.GetCtxDmaOffsetGpu(), 0,
          size,
          0x0,
          0xffffffff));

    // Copy the first 128KB/512KB sparse to the dest
    dest.Fill(0xff);
    CHECK_RC(_Test(0, offset, 0,
          &dest, dest.GetCtxDmaOffsetGpu(), 0,
          chunkSize,
          0x0,
          0xffffffff));

    // Copy the last 128KB/512KB sparse to the dest
    dest.Fill(0xff);
    CHECK_RC(_Test(0, offset, size - chunkSize,
          &dest, dest.GetCtxDmaOffsetGpu(), size - chunkSize,
          chunkSize,
          0x0,
          0xffffffff));

    // Copy the third 128KB/512KB sparse to the dest
    dest.Fill(0xff);
    CHECK_RC(_Test(0, offset, 2 * chunkSize,
          &dest, dest.GetCtxDmaOffsetGpu(), 2 * chunkSize,
          chunkSize,
          0x0,
          0xffffffff));

    // Free the allocated virtual address range.
    Printf(Tee::PriHigh, "Freeing all allocation(s)...\n");
    pLwRm->Free(hSparse);

    return rc;
}

//! \brief Copy from sparse to another sparse region
//
// Generic Process
//  1) Allocate two 512KB/2MB sparse regions
//  2) Copy from sparse to the other sparse
//  3) Verify there is no faulting
//
// Variations
//  1) Copy the entire 512KB/2MB over
//  2) Copy the first 128KB/512KB over
//  3) Copy the last 128KB/512KB over
//  4) Copy the middle 128KB/512KB over
//! \return RC structure
RC
SparseAccessTest::_TestSparseToSparse
(
    UINT32 bSmallPage,
    UINT64 size,
    UINT64 chunkSize,
    bool bHugePageSizeTableSparse
)
{
    RC        rc      = OK;
    LwRmPtr   pLwRm;
    UINT32    hSparse = 0;
    UINT32    hSparseDest = 0;
    UINT64    offset  = 0;
    UINT64    destOffset = offset + size;

    Printf(Tee::PriHigh, "Test: Copy Sparse to Sparse\n");

    // Allocate Sparse
    CHECK_RC(_AllocSparse(&hSparse, &offset, &size, bSmallPage,
                          bHugePageSizeTableSparse));
    CHECK_RC(_AllocSparse(&hSparseDest, &destOffset, &size, bSmallPage,
                          bHugePageSizeTableSparse));

    // Copy the entire 512KB/2MB sparse to the other sparse
    CHECK_RC(_Test(0, offset, 0,
          0, destOffset, 0,
          size,
          0x0,
          0x0));

    // Copy the first 128KB/512KB sparse to the other sparse
    CHECK_RC(_Test(0, offset, 0,
          0, destOffset, 0,
          chunkSize,
          0x0,
          0x0));

    // Copy the last 128KB/512KB sparse to the other sparse
    CHECK_RC(_Test(0, offset, size - chunkSize,
          0, destOffset, size - chunkSize,
          chunkSize,
          0x0,
          0x0));

    // Copy the third 128KB/512KB sparse to the other sparse
    CHECK_RC(_Test(0, offset, 2 * chunkSize,
          0, destOffset, 2 * chunkSize,
          chunkSize,
          0x0,
          0x0));

    // Free the allocated virtual address range.
    Printf(Tee::PriHigh, "Freeing all allocation(s)...\n");
    pLwRm->Free(hSparse);
    pLwRm->Free(hSparseDest);

    return rc;
}

//! \brief Copy from a mapped memory region to a sparse region
//
// Generic Process
//  1) Allocate a 512kb sparse region and two 512kb surface2D.
//  2) Populate the surface2D with some data
//  3) Copy from surface2D to the sparse region
//  4) Copy from sparse to another surface region
//  5) Verify if there is data on the other surface.
//     - There shouldn't be any data other than on the region
//       that is later mapped to physical memory!
//
// Variations
//  1) Copy the first 128kb over
//  2) Copy the last 128kb over
//  3) Copy the middle 128kb over
//
//! \return RC structure
RC
SparseAccessTest::_TestMappedToPartialMappedSparse
(
    UINT32 bSmallPage,
    LwU64 virtSize,
    bool bHugePageSizeTableSparse
)
{
    RC        rc         = OK;
    LwRmPtr   pLwRm;
    UINT32    hSparse    = 0;
    UINT64    offset     = 0;
    UINT32    hPhys      = 0;
    UINT64    physOffset = 0;
    UINT64    offsetVirt = 0;
    UINT64    physSize   = 128 * KB;
    Surface2D src;
    Surface2D dest;

    Printf(Tee::PriHigh, "Test: Copy Mapped to Partially Mapped Sparse\n");

    // Allocate Sparse
    CHECK_RC(_AllocSparse(&hSparse, &offset, &virtSize, bSmallPage,
                          bHugePageSizeTableSparse));

    // Allocate 128KB of Physical
    CHECK_RC(_AllocPhysical(&hPhys, &physOffset, &physSize, bSmallPage));

    //
    // Map the 128KB physical address range of FB to the third kb of virtual address
    // address range using the default(BIG) page size.
    //
    if (bSmallPage) {
        CHECK_RC(pLwRm->MapMemoryDma(hSparse,
                                     hPhys,
                                     0,
                                     physSize,
                                     DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE) |
                                     DRF_DEF(OS46, _FLAGS, _PAGE_SIZE, _4KB),
                                    &offsetVirt,
                                    GetBoundGpuDevice()));
    }
    else
    {
        CHECK_RC(pLwRm->MapMemoryDma(hSparse,
                                     hPhys,
                                     0,
                                     physSize,
                                     DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE) |
                                     DRF_DEF(OS46, _FLAGS, _PAGE_SIZE, _DEFAULT),
                                    &offsetVirt,
                                    GetBoundGpuDevice()));
    }
    Printf(Tee::PriHigh, "Mapped at 0x%llx!!\n", offsetVirt);

    // Allocate destination surface and surface for semaphore
    CHECK_RC(_AllocSurface(&src, virtSize));
    CHECK_RC(_AllocSurface(&dest, virtSize));

    // Copy the first 128kb sparse to the other sparse
    src.Fill(0xff);
    dest.Fill(0x0);
    CHECK_RC(_Test(&src, src.GetCtxDmaOffsetGpu(), 0,
          0, offset, 0,
          physSize,
          0x0,
          0x0));
    CHECK_RC(_Test(0, offset, 0,
          &dest, dest.GetCtxDmaOffsetGpu(), 0,
          physSize,
          0xffffffff,
          0x0));

    // Copy the last 128kb sparse to the other sparse
    src.Fill(0xff);
    dest.Fill(0x0);
    CHECK_RC(_Test(&src, src.GetCtxDmaOffsetGpu(), virtSize - physSize,
          0, offset, virtSize - 2 * physSize,
          physSize,
          0x0,
          0x0));
    CHECK_RC(_Test(0, offset, virtSize - 2 * physSize,
          &dest, dest.GetCtxDmaOffsetGpu(), virtSize - physSize,
          physSize,
          0x0,
          0x0));

    // Copy the third 128kb sparse to the other sparse
    src.Fill(0xff);
    dest.Fill(0x0);
    CHECK_RC(_Test(&src, src.GetCtxDmaOffsetGpu(), 2 * physSize,
          0, offset, 2 * physSize,
          physSize,
          0x0,
          0x0));
    CHECK_RC(_Test(0, offset, 2 * physSize,
          &dest, dest.GetCtxDmaOffsetGpu(), 2 * physSize,
          physSize,
          0x0,
          0x0));

    // Unmap the physical memory region from sparse
    pLwRm->UnmapMemoryDma(hSparse, hPhys, 0, offsetVirt, GetBoundGpuDevice());
    Printf(Tee::PriHigh, "Unmapped!!\n");

    // Free the allocated virtual address range.
    Printf(Tee::PriHigh, "Freeing all allocation(s)...\n");
    pLwRm->Free(hPhys);
    pLwRm->Free(hSparse);

    return rc;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(SparseAccessTest, RmTest,
    "TODORMT - Write a good short description here");

