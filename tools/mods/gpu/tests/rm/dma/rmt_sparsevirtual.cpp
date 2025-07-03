/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//-----------------------------------------------------------------------------
//
// DESCRIPTION
//
// Tests allocation of a "sparse" virtual address range. This feature is only
// supported on GM20X and later.
//
//-----------------------------------------------------------------------------

#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/utility.h"

#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/lwrm.h"
#include "lwRmApi.h"
#include "gpu/tests/rmtest.h"
#include "mmu/gmmu_fmt.h" // VMM API

#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl208f.h"
#include "class/cl90f1.h" // FERMI_VASPACE_A
#include "ctrl/ctrl90f1.h"

//------------------------------------------------------------------------------
// Class Declaration
//------------------------------------------------------------------------------

class SparseVirtualTest: public RmTest
{
    public:
        SparseVirtualTest();
        virtual ~SparseVirtualTest();

        virtual string IsTestSupported();
        virtual RC     Setup();
        virtual RC     Run();
        virtual RC     Cleanup();

    protected:

    private:
        RC _AllocateMemory(UINT32 *pAttr,
                           UINT32 *pAttr2,
                           UINT32 *pFlags,
                           UINT32 *pHandle,
                           UINT64 *pOffset,
                           UINT64 *pAlignment,
                           UINT64 *pSize,
                           LW208F_CTRL_DMA_GET_VAS_BLOCK_DETAILS_PARAMS *pParams);

        RC _CheckForIlwalidAndSparse(UINT64 fromAddr,
                                     UINT64 throughAddr,
                                     bool   bSmallPageSizeTableSparse,
                                     bool   bBigPageSizeTableSparse,
                                     bool   bHugePageSizeTableSparse = LW_FALSE);
        RC _CheckForValidAndNonSparse(UINT64 fromAddr,
                                      UINT64 throughAddr,
                                      bool   bSmallPageSizeTableValid,
                                      bool   bBigPageSizeTableValid,
                                      bool   bPartialMiddleMapping);

        // Checks if VMM is enabled
        bool _CheckIsVmmEnabled();

        //
        // Small virtual allocation at offset 0. Tests allocation of "sparse"
        // PTEs.
        //
        RC _TestCase1();

        //
        // Small virtual allocation at an offset larger than a page-size. Tests
        // allocation of "sparse" PTEs surrounded by "non-sparse" PTEs.
        //
        RC _TestCase2();

        //
        // Two small virtual allocations with at least a page-size in-between.
        // Tests allocation of "sparse" PTEs surrounded by "non-sparse" PTEs.
        // Also tests freeing of some "sparse" PTEs while leaving other
        // "sparse" PTEs.
        //
        RC _TestCase3();

        //
        // Small virtual allocation with a full physical mapping. Tests
        // mapping and unmapping of "sparse" PTEs.
        //
        RC _TestCase4();

        //
        // Small virtual allocation with a partial physical mapping. Tests
        // mapping and unmapping of "sparse" PTEs.
        //
        RC _TestCase5();

        //
        // Large virtual allocation that crosses a PDE boundary. Tests
        // allocation of "sparse" PTEs across PDEs.
        //
        RC _TestCase6();
};

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

bool
SparseVirtualTest::_CheckIsVmmEnabled()
{
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle hDefVAS;
    bool bVmmEnabled = false;

    LW_VASPACE_ALLOCATION_PARAMETERS allocParams = {0};
    allocParams.index = LW_VASPACE_ALLOCATION_INDEX_GPU_DEVICE;

    if (OK != pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()), &hDefVAS,
                          FERMI_VASPACE_A, &allocParams))
    {
        return false;
    }

    LW90F1_CTRL_VASPACE_GET_GMMU_FORMAT_PARAMS fmtParams = {0};
    fmtParams.hSubDevice = pLwRm->GetSubdeviceHandle(GetBoundGpuDevice()->GetSubdevice(0));

    rc = pLwRm->Control(hDefVAS, LW90F1_CTRL_CMD_VASPACE_GET_GMMU_FORMAT,
                        &fmtParams, sizeof(fmtParams));
    if (OK == rc && LwP64_NULL != fmtParams.pFmt)
    {
        bVmmEnabled = true;
    }

    pLwRm->Free(hDefVAS);
    return bVmmEnabled;
}

//-------------------------------------------------------------------------------

RC
SparseVirtualTest::_AllocateMemory
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

//------------------------------------------------------------------------------

RC
SparseVirtualTest::_CheckForIlwalidAndSparse
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

        // Check for "non-sparse" one page size before "fromAddr"...
        params.gpuAddr         = fromAddr - pageSize;
        params.skipVASpaceInit = true;
        CHECK_RC(pLwRm->Control(hGpuDev,
                                LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                               &params,
                                sizeof(params)));

        //
        // Only check the given address if the page size is not 0. The
        // LW0080_CTRL_CMD_DMA_GET_PTE_INFO control call will leave the page
        // size at 0 if the page table for the given address does not exist.
        //
        if (params.pteBlocks[i].pageSize != 0)
        {
            Printf(Tee::PriHigh,
                   "    Is Address 0x%llx Non-Sparse? ... ",
                   params.gpuAddr);
            if (FLD_TEST_DRF(0080_CTRL,
                             _DMA_PTE_INFO_PARAMS_FLAGS,
                             _VALID,
                             _FALSE,
                             params.pteBlocks[i].pteFlags) &&
                FLD_TEST_DRF(0080_CTRL,
                             _DMA_PTE_INFO_PARAMS_FLAGS,
                             _GPU_CACHED,
                             _FALSE,
                             params.pteBlocks[i].pteFlags))
            {
                Printf(Tee::PriHigh, "No!!!\n");
                return RC::SOFTWARE_ERROR;
            }
            Printf(Tee::PriHigh, "Yes\n");
        }
    }

    return OK;
}

//------------------------------------------------------------------------------

RC
SparseVirtualTest::_CheckForValidAndNonSparse
(
    UINT64 fromAddr,
    UINT64 throughAddr,
    bool   bSmallPageSizeTableValid,
    bool   bBigPageSizeTableValid,
    bool   bPartialMiddleMapping
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

        if (((pageSize == GpuDevice::PAGESIZE_SMALL) && bSmallPageSizeTableValid) ||
            ((pageSize == bigPageSize) && bBigPageSizeTableValid) )
        {
            // Check for "non-sparse" from "fromAddr" through "throughAddr"...
            Printf(Tee::PriHigh,
                   "    Are PTEs between 0x%llx - 0x%llx (inclusive) Valid and Non-Sparse? ... ",
                   fromAddr,
                   throughAddr);
        }
        else if((pageSize == bigPageSize) && bSmallPageSizeTableValid)
        {
            //
            // If the small page table PTEs are marked valid, then the
            // corresponding big page table PTEs should be marked invalid and
            // "non-sparse".
            //
            // Check for "non-sparse" from "fromAddr" through "throughAddr"...
            //
            Printf(Tee::PriHigh,
                   "    Are PTEs between 0x%llx - 0x%llx (inclusive) Invalid and Non-Sparse? ... ",
                   fromAddr,
                   throughAddr);
        }
        else
        {
            // Check for "sparse" from "fromAddr" through "throughAddr"...
            Printf(Tee::PriHigh,
                   "    Are PTEs between 0x%llx - 0x%llx (inclusive) Invalid? ... ",
                   fromAddr,
                   throughAddr);
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

            if (((pageSize == GpuDevice::PAGESIZE_SMALL) && bSmallPageSizeTableValid) ||
                ((pageSize == bigPageSize) && bBigPageSizeTableValid) )
            {
                // Should be "valid" and "non-sparse".
                if (!FLD_TEST_DRF(0080_CTRL,
                                  _DMA_PTE_INFO_PARAMS_FLAGS,
                                  _VALID,
                                  _TRUE,
                                  params.pteBlocks[i].pteFlags) ||
                    !FLD_TEST_DRF(0080_CTRL,
                                  _DMA_PTE_INFO_PARAMS_FLAGS,
                                  _GPU_CACHED,
                                  _TRUE,
                                  params.pteBlocks[i].pteFlags))
                {
                    Printf(Tee::PriHigh,
                           "No, Address 0x%llx has an issue!!!\n",
                           addr);
                    return RC::SOFTWARE_ERROR;
                }
            }
            else if((pageSize == bigPageSize) && bSmallPageSizeTableValid)
            {
                // Should be "invalid" and "non-sparse".
                if (!FLD_TEST_DRF(0080_CTRL,
                                  _DMA_PTE_INFO_PARAMS_FLAGS,
                                  _VALID,
                                  _FALSE,
                                  params.pteBlocks[i].pteFlags) ||
                    !FLD_TEST_DRF(0080_CTRL,
                                  _DMA_PTE_INFO_PARAMS_FLAGS,
                                  _GPU_CACHED,
                                  _TRUE,
                                  params.pteBlocks[i].pteFlags))
                {
                    Printf(Tee::PriHigh,
                           "    No, Address 0x%llx has an issue!!!\n",
                           addr);
                    return RC::SOFTWARE_ERROR;
                }
            }
            else
            {
                // Should be "invalid" and "sparse".
                if (!FLD_TEST_DRF(0080_CTRL,
                                  _DMA_PTE_INFO_PARAMS_FLAGS,
                                  _VALID,
                                  _FALSE,
                                  params.pteBlocks[i].pteFlags))
                {
                    Printf(Tee::PriHigh,
                           "    No, Address 0x%llx has an issue!!!\n",
                           addr);
                    return RC::SOFTWARE_ERROR;
                }
            }
        }
        Printf(Tee::PriHigh, "Yes\n");

        //
        // Check for "sparse" (or "non-sparse") one page size before
        // "fromAddr"...
        //
        params.gpuAddr         = fromAddr - pageSize;
        params.skipVASpaceInit = true;
        CHECK_RC(pLwRm->Control(hGpuDev,
                                LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                               &params,
                                sizeof(params)));

        //
        // Only check the given address if the page size is not 0. The
        // LW0080_CTRL_CMD_DMA_GET_PTE_INFO control call will leave the page
        // size at 0 if the page table for the given address does not exist.
        //
        if (params.pteBlocks[i].pageSize != 0)
        {
            if (bPartialMiddleMapping &&
                (((pageSize == GpuDevice::PAGESIZE_SMALL) && bSmallPageSizeTableValid) ||
                ((pageSize == bigPageSize) && bBigPageSizeTableValid)) )
            {
                Printf(Tee::PriHigh,
                       "    Is Address 0x%llx Invalid and Sparse? ... ",
                       params.gpuAddr);
                if (!FLD_TEST_DRF(0080_CTRL,
                                  _DMA_PTE_INFO_PARAMS_FLAGS,
                                  _VALID,
                                  _FALSE,
                                  params.pteBlocks[i].pteFlags) ||
                    !FLD_TEST_DRF(0080_CTRL,
                                  _DMA_PTE_INFO_PARAMS_FLAGS,
                                  _GPU_CACHED,
                                  _FALSE,
                                  params.pteBlocks[i].pteFlags))
                {
                    Printf(Tee::PriHigh, "No!!!\n");
                    return RC::SOFTWARE_ERROR;
                }
                Printf(Tee::PriHigh, "Yes\n");
            }
            else
            {
                Printf(Tee::PriHigh,
                       "    Is Address 0x%llx Non-Sparse? ... ",
                       params.gpuAddr);
                if (FLD_TEST_DRF(0080_CTRL,
                                 _DMA_PTE_INFO_PARAMS_FLAGS,
                                 _VALID,
                                 _FALSE,
                                 params.pteBlocks[i].pteFlags) &&
                    FLD_TEST_DRF(0080_CTRL,
                                 _DMA_PTE_INFO_PARAMS_FLAGS,
                                 _GPU_CACHED,
                                 _FALSE,
                                 params.pteBlocks[i].pteFlags))
                {
                    Printf(Tee::PriHigh, "No!!!\n");
                    return RC::SOFTWARE_ERROR;
                }
                Printf(Tee::PriHigh, "Yes\n");
            }
        }
    }

    return OK;
}

//------------------------------------------------------------------------------

RC
SparseVirtualTest::_TestCase1()
{
    LwRmPtr pLwRm;
    UINT64  alignment;
    UINT64  offset;
    UINT64  size;
    UINT32  attr;
    UINT32  attr2;
    UINT32  flags;
    UINT32  handle;
    RC      rc;

    LW208F_CTRL_DMA_GET_VAS_BLOCK_DETAILS_PARAMS params = {0};

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "TEST CASE 1...\n\n");

    //
    // Allocate a 6 KB virtual address range at offset 0 with a "default" page
    // size.
    //
    // Note, a "default" page size results in the size getting rounded up to a
    // "big" page size.
    //
    // Furthermore, both the small and big page tables are allocated and
    // initialized to an invalid state for the allocated virtual address range.
    //
    attr      = DRF_DEF(OS32, _ATTR, _PAGE_SIZE,   _DEFAULT) |
                DRF_DEF(OS32, _ATTR, _LOCATION,    _ANY)     |
                DRF_DEF(OS32, _ATTR, _PHYSICALITY, _DEFAULT);
    attr2     = LWOS32_ATTR_NONE;
    flags     = LWOS32_ALLOC_FLAGS_VIRTUAL | LWOS32_ALLOC_FLAGS_SPARSE;
    handle    = 0;
    offset    = 0;
    alignment = 0;
    size      = 6 * 1024;
    CHECK_RC(_AllocateMemory(&attr,
                             &attr2,
                             &flags,
                             &handle,
                             &offset,
                             &alignment,
                             &size,
                             &params));

    // Check the allocation's PTEs for proper "sparse" and "non-sparse" flags.
    CHECK_RC(_CheckForIlwalidAndSparse(params.alignedAddress,
                                       params.endAddress,
                                       LW_TRUE,
                                       LW_TRUE));

    // Free the allocated virtual address range.
    Printf(Tee::PriHigh, "Freeing all allocation(s)...\n");
    pLwRm->Free(handle);

    return OK;
}

//------------------------------------------------------------------------------

RC
SparseVirtualTest::_TestCase2()
{
    LwRmPtr pLwRm;
    UINT64  alignment;
    UINT64  offset;
    UINT64  size;
    UINT32  attr;
    UINT32  attr2;
    UINT32  flags;
    UINT32  handle;
    RC      rc;

    LW208F_CTRL_DMA_GET_VAS_BLOCK_DETAILS_PARAMS params = {0};

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "TEST CASE 2...\n\n");

    //
    // Allocate a 6 KB virtual address range at offset 257 KB with a "default"
    // page size.
    //
    // Note, a "default" page size results in the size getting rounded up to a
    // "big" page size.
    //
    // Furthermore, both the small and big page tables are allocated and
    // initialized to an invalid state for the allocated virtual address range.
    //
    attr      = DRF_DEF(OS32, _ATTR, _PAGE_SIZE,   _DEFAULT) |
                DRF_DEF(OS32, _ATTR, _LOCATION,    _ANY)     |
                DRF_DEF(OS32, _ATTR, _PHYSICALITY, _DEFAULT);
    attr2     = LWOS32_ATTR_NONE;
    flags     = LWOS32_ALLOC_FLAGS_VIRTUAL                |
                LWOS32_ALLOC_FLAGS_FIXED_ADDRESS_ALLOCATE |
                LWOS32_ALLOC_FLAGS_ALIGNMENT_FORCE        |
                LWOS32_ALLOC_FLAGS_SPARSE;
    handle    = 0;
    offset    = 0x10040400;
    alignment = 0;
    size      = 6 * 1024;
    CHECK_RC(_AllocateMemory(&attr,
                             &attr2,
                             &flags,
                             &handle,
                             &offset,
                             &alignment,
                             &size,
                             &params));

    // Check the allocation's PTEs for proper "sparse" and "non-sparse" flags.
    CHECK_RC(_CheckForIlwalidAndSparse(params.beginAddress,
                                       params.endAddress,
                                       LW_TRUE,
                                       LW_TRUE));

    // Free the allocated virtual address range.
    Printf(Tee::PriHigh, "Freeing all allocation(s)...\n");
    pLwRm->Free(handle);

    return OK;
}

//------------------------------------------------------------------------------

RC
SparseVirtualTest::_TestCase3()
{
    LwRmPtr pLwRm;
    UINT64  alignment;
    UINT64  offset;
    UINT64  size;
    UINT64  beginAddress1;
    UINT64  beginAddress2;
    UINT64  endAddress1;
    UINT64  endAddress2;
    UINT32  attr;
    UINT32  attr2;
    UINT32  flags;
    UINT32  handle1;
    UINT32  handle2;
    RC      rc;

    LW208F_CTRL_DMA_GET_VAS_BLOCK_DETAILS_PARAMS params = {0};

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "TEST CASE 3...\n\n");

    //
    // Allocate a 6 KB virtual address range at offset 0 with a "default" page
    // size.
    //
    // Note, a "default" page size results in the size getting rounded up to a
    // "big" page size.
    //
    // Furthermore, both the small and big page tables are allocated and
    // initialized to an invalid state for the allocated virtual address range.
    //
    attr      = DRF_DEF(OS32, _ATTR, _PAGE_SIZE,   _DEFAULT) |
                DRF_DEF(OS32, _ATTR, _LOCATION,    _ANY)     |
                DRF_DEF(OS32, _ATTR, _PHYSICALITY, _DEFAULT);
    attr2     = LWOS32_ATTR_NONE;
    flags     = LWOS32_ALLOC_FLAGS_VIRTUAL | LWOS32_ALLOC_FLAGS_SPARSE;
    handle1   = 0;
    offset    = 0;
    alignment = 0;
    size      = 6 * 1024;
    CHECK_RC(_AllocateMemory(&attr,
                             &attr2,
                             &flags,
                             &handle1,
                             &offset,
                             &alignment,
                             &size,
                             &params));

    // Cache off the allocated range.
    beginAddress1 = params.beginAddress;
    endAddress1   = params.endAddress;

    //
    // Allocate a 6 KB virtual address range at offset 257 KB with a "default"
    // page size.
    //
    // Note, a "default" page size results in the size getting rounded up to a
    // "big" page size.
    //
    // Furthermore, both the small and big page tables are allocated and
    // initialized to an invalid state for the allocated virtual address range.
    //
    attr      = DRF_DEF(OS32, _ATTR, _PAGE_SIZE,   _DEFAULT) |
                DRF_DEF(OS32, _ATTR, _LOCATION,    _ANY)     |
                DRF_DEF(OS32, _ATTR, _PHYSICALITY, _DEFAULT);
    attr2     = LWOS32_ATTR_NONE;
    flags     = LWOS32_ALLOC_FLAGS_VIRTUAL                |
                LWOS32_ALLOC_FLAGS_FIXED_ADDRESS_ALLOCATE |
                LWOS32_ALLOC_FLAGS_ALIGNMENT_FORCE        |
                LWOS32_ALLOC_FLAGS_SPARSE;
    handle2   = 0;
    offset    = 0x10040400;
    alignment = 0;
    size      = 6 * 1024;
    CHECK_RC(_AllocateMemory(&attr,
                             &attr2,
                             &flags,
                             &handle2,
                             &offset,
                             &alignment,
                             &size,
                             &params));

    // Cache off the allocated range.
    beginAddress2 = params.beginAddress;
    endAddress2   = params.endAddress;

    // Check the allocations' PTEs for proper "sparse" and "non-sparse" flags.
    CHECK_RC(_CheckForIlwalidAndSparse(beginAddress1, endAddress1, LW_TRUE, LW_TRUE));
    CHECK_RC(_CheckForIlwalidAndSparse(beginAddress2, endAddress2, LW_TRUE, LW_TRUE));

    // Free the allocated virtual address ranges.
    Printf(Tee::PriHigh, "Freeing all allocation(s)...\n");
    pLwRm->Free(handle2);

    //
    // Check that freeing one allocation's PTEs did not affect other "sparse"
    // PTEs.
    //
    CHECK_RC(_CheckForIlwalidAndSparse(beginAddress1, endAddress1, LW_TRUE, LW_TRUE));

    pLwRm->Free(handle1);

    return OK;
}

//------------------------------------------------------------------------------

RC
SparseVirtualTest::_TestCase4()
{
    LwRmPtr pLwRm;
    UINT64  alignment;
    UINT64  offset;
    UINT64  offsetPhys;
    UINT64  offsetVirt;
    UINT64  size;
    UINT64  beginAddress1;
    UINT64  endAddress1;
    UINT32  attr;
    UINT32  attr2;
    UINT32  flags;
    UINT32  handle1;
    UINT32  handle2;
    RC      rc;

    LW208F_CTRL_DMA_GET_VAS_BLOCK_DETAILS_PARAMS params = {0};

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "TEST CASE 4...\n\n");

    //
    // Allocate a 6 KB virtual address range at offset 0 with a "default" page
    // size.
    //
    // Note, a "default" page size results in the size getting rounded up to a
    // "big" page size.
    //
    // Furthermore, both the small and big page tables are allocated and
    // initialized to an invalid state for the allocated virtual address range.
    //
    attr      = DRF_DEF(OS32, _ATTR, _PAGE_SIZE,   _DEFAULT) |
                DRF_DEF(OS32, _ATTR, _LOCATION,    _ANY)     |
                DRF_DEF(OS32, _ATTR, _PHYSICALITY, _DEFAULT);
    attr2     = LWOS32_ATTR_NONE;
    flags     = LWOS32_ALLOC_FLAGS_VIRTUAL | LWOS32_ALLOC_FLAGS_SPARSE;
    handle1   = 0;
    offset    = 0;
    alignment = 0;
    size      = 6 * 1024;
    CHECK_RC(_AllocateMemory(&attr,
                             &attr2,
                             &flags,
                             &handle1,
                             &offset,
                             &alignment,
                             &size,
                             &params));

    // Cache off the allocated range.
    beginAddress1 = params.beginAddress;
    endAddress1   = params.endAddress;

    //
    // Allocate a 6 KB physical address range of vid mem (FB) with a "big" page
    // size.
    //
    attr      = DRF_DEF(OS32, _ATTR, _PAGE_SIZE,   _BIG)     |
                DRF_DEF(OS32, _ATTR, _LOCATION,    _VIDMEM)  |
                DRF_DEF(OS32, _ATTR, _PHYSICALITY, _DEFAULT);
    attr2     = LWOS32_ATTR_NONE;
    flags     = 0;
    handle2   = 0;
    offset    = 0;
    alignment = 0;
    size      = 6 * 1024;
    CHECK_RC(_AllocateMemory(&attr,
                             &attr2,
                             &flags,
                             &handle2,
                             &offset,
                             &alignment,
                             &size,
                             NULL));

    //
    // Check the virtual allocation's PTEs for proper "sparse" and "non-sparse"
    // flags.
    //
    CHECK_RC(_CheckForIlwalidAndSparse(beginAddress1, endAddress1, LW_TRUE, LW_TRUE));

    //
    // Map the 6 KB physical address range of FB to the 6 KB virtual address
    // address range using the big page size.
    //
    flags      = DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE) |
                 DRF_DEF(OS46, _FLAGS, _PAGE_SIZE, _BIG);
    offsetPhys = 0;
    offsetVirt = 0;
    size       = 6 * 1024;
    CHECK_RC(pLwRm->MapMemoryDma(handle1,
                                 handle2,
                                 offsetPhys,
                                 size,
                                 flags,
                                &offsetVirt,
                                GetBoundGpuDevice()));

    Printf(Tee::PriHigh, "MAPPED at 0x%llx!!\n", offsetVirt);

    //
    // The virtual allocation should now be "non-sparse" on the big page size
    // page table.
    //
    // NOTE: The amount mapped may be smaller than the amount of virtual space
    // allocated (due to issues related to generic compression support). Thus,
    // the range checked for "valid-and-not-sparse" must be bounded at a page
    // size boundary.
    //
    MASSERT(offsetVirt == beginAddress1);
    CHECK_RC(_CheckForValidAndNonSparse(beginAddress1,
                                       (beginAddress1 + size - 1) & (~static_cast<UINT64>((params.pageSize-1))),
                                        LW_FALSE,
                                        LW_TRUE,
                                        LW_FALSE));

    flags = 0;
    pLwRm->UnmapMemoryDma(handle1, handle2, flags, offsetVirt, GetBoundGpuDevice());

    Printf(Tee::PriHigh, "UNMAPPED!!\n");

    //
    // The virtual allocation should now be "sparse" again on all page size
    // page tables.
    //
    CHECK_RC(_CheckForIlwalidAndSparse(beginAddress1, endAddress1,
                                       _CheckIsVmmEnabled() ?
                                       LW_FALSE : LW_TRUE,
                                       LW_TRUE));

    // Free the allocated virtual address ranges.
    Printf(Tee::PriHigh, "Freeing all allocation(s)...\n");
    pLwRm->Free(handle2);
    pLwRm->Free(handle1);

    return OK;
}

//------------------------------------------------------------------------------

RC
SparseVirtualTest::_TestCase5()
{
    LwRmPtr pLwRm;
    UINT64  alignment;
    UINT64  offset;
    UINT64  offsetPhys;
    UINT64  offsetVirt;
    UINT64  size;
    UINT64  beginAddress1;
    UINT64  endAddress1;
    UINT32  attr;
    UINT32  attr2;
    UINT32  flags;
    UINT32  handle1;
    UINT32  handle2;
    RC      rc;

    LW208F_CTRL_DMA_GET_VAS_BLOCK_DETAILS_PARAMS params = {0};

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "TEST CASE 5...\n\n");

    //
    // Allocate a 12 KB virtual address range at offset 0 with a "default" page
    // size.
    //
    // Note, a "default" page size results in the size getting rounded up to a
    // "big" page size.
    //
    // Furthermore, both the small and big page tables are allocated and
    // initialized to an invalid state for the allocated virtual address range.
    //
    attr      = DRF_DEF(OS32, _ATTR, _PAGE_SIZE,   _DEFAULT) |
                DRF_DEF(OS32, _ATTR, _LOCATION,    _ANY)     |
                DRF_DEF(OS32, _ATTR, _PHYSICALITY, _DEFAULT);
    attr2     = LWOS32_ATTR_NONE;
    flags     = LWOS32_ALLOC_FLAGS_VIRTUAL | LWOS32_ALLOC_FLAGS_SPARSE;
    handle1   = 0;
    offset    = 0;
    alignment = 0;
    size      = 12 * 1024;
    CHECK_RC(_AllocateMemory(&attr,
                             &attr2,
                             &flags,
                             &handle1,
                             &offset,
                             &alignment,
                             &size,
                             &params));

    // Cache off the allocated range.
    beginAddress1 = params.beginAddress;
    endAddress1   = params.endAddress;

    //
    // Allocate a 2 KB physical address range of vid mem (FB) with a "small"
    // page size.
    //
    attr      = DRF_DEF(OS32, _ATTR, _PAGE_SIZE,   _4KB)     |
                DRF_DEF(OS32, _ATTR, _LOCATION,    _VIDMEM)  |
                DRF_DEF(OS32, _ATTR, _PHYSICALITY, _DEFAULT);
    attr2     = LWOS32_ATTR_NONE;
    flags     = 0;
    handle2   = 0;
    offset    = 0;
    alignment = 0;
    size      = 2 * 1024;
    CHECK_RC(_AllocateMemory(&attr,
                             &attr2,
                             &flags,
                             &handle2,
                             &offset,
                             &alignment,
                             &size,
                             NULL));

    //
    // Check the virtual allocation's PTEs for proper "sparse" and "non-sparse"
    // flags.
    //
    CHECK_RC(_CheckForIlwalidAndSparse(beginAddress1, endAddress1, LW_TRUE, LW_TRUE));

    //
    // Map the 2 KB physical address range of FB to the 12 KB virtual address
    // address range at an offset of 4 KB using the small page size.
    //
    flags      = DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE) |
                 DRF_DEF(OS46, _FLAGS, _PAGE_SIZE, _4KB);
    offsetPhys = 0;
    offsetVirt = 4 * 1024;
    size       = 2 * 1024;
    CHECK_RC(pLwRm->MapMemoryDma(handle1,
                                 handle2,
                                 offsetPhys,
                                 size,
                                 flags,
                                &offsetVirt,
                                GetBoundGpuDevice()));

    Printf(Tee::PriHigh, "MAPPED at 0x%llx!!\n", offsetVirt);

    //
    // The virtual allocation should now be "non-sparse" on the small page size
    // page table.
    //
    // NOTE: The amount mapped may be smaller than the amount of virtual space
    // allocated (due to issues related to generic compression support). Thus,
    // the range checked for "valid-and-not-sparse" must be bounded at a page
    // size boundary.
    //
    CHECK_RC(_CheckForValidAndNonSparse(offsetVirt,
                                       (offsetVirt + size - 1) & (~static_cast<UINT64>((params.pageSize-1))),
                                        LW_TRUE,
                                        LW_FALSE,
                                        LW_TRUE));

    flags = 0;
    pLwRm->UnmapMemoryDma(handle1, handle2, flags, offsetVirt, GetBoundGpuDevice());

    Printf(Tee::PriHigh, "UNMAPPED!!\n");

    //
    // The virtual allocation should now be "sparse" again on just the small
    // page size page table.
    //
    // NOTE: Only check the "unmapped" pages due to how the big page size PTE
    // corresponding to the unmapped small page size PTE relies on the small
    // page size PTE to denote sparse.
    //
    CHECK_RC(_CheckForIlwalidAndSparse(beginAddress1,
                                      (beginAddress1 + size - 1) & (~static_cast<UINT64>((params.pageSize-1))),
                                       LW_TRUE, LW_FALSE));

    // Free the allocated virtual address ranges.
    Printf(Tee::PriHigh, "Freeing all allocation(s)...\n");
    pLwRm->Free(handle2);
    pLwRm->Free(handle1);

    return OK;
}

//------------------------------------------------------------------------------

RC
SparseVirtualTest::_TestCase6()
{
    LwRmPtr pLwRm;
    UINT64  alignment;
    UINT64  offset;
    UINT64  size;
    UINT32  attr;
    UINT32  attr2;
    UINT32  flags;
    UINT32  handle;
    RC      rc;

    LW208F_CTRL_DMA_GET_VAS_BLOCK_DETAILS_PARAMS params = {0};

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "TEST CASE 6...\n\n");

    //
    // Allocate a 5 MB virtual address range at offset 126 MB with a "default"
    // page size.
    //
    // Note, a "default" page size results in the size getting rounded up to a
    // "big" page size.
    //
    // Furthermore, both the small and big page tables are allocated and
    // initialized to an invalid state for the allocated virtual address range.
    //
    attr      = DRF_DEF(OS32, _ATTR, _PAGE_SIZE,   _DEFAULT) |
                DRF_DEF(OS32, _ATTR, _LOCATION,    _ANY)     |
                DRF_DEF(OS32, _ATTR, _PHYSICALITY, _DEFAULT);
    attr2     = LWOS32_ATTR_NONE;
    flags     = LWOS32_ALLOC_FLAGS_VIRTUAL                |
                LWOS32_ALLOC_FLAGS_FIXED_ADDRESS_ALLOCATE |
                LWOS32_ALLOC_FLAGS_ALIGNMENT_FORCE        |
                LWOS32_ALLOC_FLAGS_SPARSE;
    handle    = 0;
    offset    = 0x17e00000;
    alignment = 0;
    size      = 2 * 1024 * 1024;
    CHECK_RC(_AllocateMemory(&attr,
                             &attr2,
                             &flags,
                             &handle,
                             &offset,
                             &alignment,
                             &size,
                             &params));

    // Check the allocation's PTEs for proper "sparse" and "non-sparse" flags.
    CHECK_RC(_CheckForIlwalidAndSparse(params.beginAddress,
                                       params.endAddress,
                                       LW_TRUE, LW_TRUE, LW_TRUE));

    // Free the allocated virtual address range.
    Printf(Tee::PriHigh, "Freeing all allocation(s)...\n");
    pLwRm->Free(handle);

    return OK;
}

//------------------------------------------------------------------------------
// Public Member Functions
//------------------------------------------------------------------------------

SparseVirtualTest::SparseVirtualTest()
{
    SetName("SparseVirtualTest");
}

//-----------------------------------------------------------------------------

SparseVirtualTest::~SparseVirtualTest()
{
    return;
}

//-----------------------------------------------------------------------------

string SparseVirtualTest::IsTestSupported()
{
    Gpu::LwDeviceId  chip;
    LwRmPtr          pLwRm;
    GpuDevice       *pGpuDev;
    GpuSubdevice    *pGpuSubDev;
    LwRm::Handle     hGpuDev;
    LwRm::Handle     hGpuSubDevDiag;
    LwU32            numGpuSubDev;
    LwU32            i;

    LW208F_CTRL_DMA_IS_SUPPORTED_SPARSE_VIRTUAL_PARAMS params     = {0};
    LW0080_CTRL_DMA_GET_CAPS_PARAMS                    paramsCaps = {0};

    pGpuDev      = GetBoundGpuDevice();
    hGpuDev      = pLwRm->GetDeviceHandle(pGpuDev);
    numGpuSubDev = pGpuDev->GetNumSubdevices();
    for (i = 0; i < numGpuSubDev; i++)
    {
        pGpuSubDev     = pGpuDev->GetSubdevice(i);
        hGpuSubDevDiag = pGpuSubDev->GetSubdeviceDiag();

        chip = pGpuSubDev->DeviceId();
        if (chip < Gpu::GM200)
        {
            return "Sparse virtual address ranges are not supported. Newer chip required.";
        }

        RC rc = pLwRm->Control(hGpuSubDevDiag,
                               LW208F_CTRL_CMD_DMA_IS_SUPPORTED_SPARSE_VIRTUAL,
                              &params,
                               sizeof(params));
        MASSERT(rc == OK);
        if (rc != OK)
        {
            return "LW208F_CTRL_CMD_DMA_IS_SUPPORTED_SPARSE_VIRTUAL failed!";
        }

        if (!params.bIsSupported)
        {
            return "Sparse virtual address ranges are not supported. Different chip required.";
        }
    }

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

//-----------------------------------------------------------------------------

RC SparseVirtualTest::Setup()
{
    RC rc;

    // Ask our base class(es) to setup internal state based on JS settings.
    CHECK_RC(InitFromJs());

    return OK;
}

//-----------------------------------------------------------------------------

RC SparseVirtualTest::Run()
{
    RC rc;

    CHECK_RC(_TestCase1());
    CHECK_RC(_TestCase2());
    CHECK_RC(_TestCase3());
    CHECK_RC(_TestCase4());
    CHECK_RC(_TestCase5());
    CHECK_RC(_TestCase6());

    return OK;
}

//-----------------------------------------------------------------------------

RC SparseVirtualTest::Cleanup()
{
    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

JS_CLASS_INHERIT(
    SparseVirtualTest,
    RmTest,
    "Tests allocation of a sparse virtual address range. This feature is only supported on GM20X and later.");

