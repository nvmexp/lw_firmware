/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_ctrl0041.cpp
//! \brief LW0041_CTRL_CMD_GET_MEM_PAGE_SIZE tests
//!
#include "gpu/tests/rmtest.h"
#include "gpu/include/gpudev.h"
#include "lwRmApi.h"
#include "ctrl/ctrl0041.h"
#include "class/cl0000.h" // LW01_NULL_OBJECT

#define KB 1024ULL

class Ctrl0041Test: public RmTest
{
public:
    Ctrl0041Test();
    virtual ~Ctrl0041Test();

    virtual string     IsTestSupported();

    virtual RC       Setup();
    virtual RC       Run();
    virtual RC       Cleanup();
private:
    bool TestGetPhyPageSize(UINT32 pageSizeAttr, UINT32 testBufferSize, UINT64 expectedPageSize);

    LwRm::Handle hClient = 0;
    LwRm::Handle hDevice = 0;
    LwRm::Handle hSubDev = 0;

    LW0041_CTRL_GET_MEM_PAGE_SIZE_PARAMS memPageSizeParams = {};
};

//! \brief Ctrl0041Test constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
Ctrl0041Test::Ctrl0041Test()
{
    SetName("Ctrl0041Test");
}

//! \brief Ctrl0041Test destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
Ctrl0041Test::~Ctrl0041Test()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \return True
//-----------------------------------------------------------------------------
string Ctrl0041Test::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \brief Setup()
//!
//! allocates the client, device, and subdevice handles
//
//! \return OK
//------------------------------------------------------------------------------
RC Ctrl0041Test::Setup()
{
    RC        rc;
    LwRmPtr   pLwRm;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    // Allocate client handle
    hClient = pLwRm->GetClientHandle();

    // Allocate Device handle
    hDevice = pLwRm->GetDeviceHandle(GetBoundGpuDevice());

    // Allocate Subdevice handle
    hSubDev = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());

    return rc;
}

//! \brief Run the test!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC Ctrl0041Test::Run()
{
    RC     rc;

    Printf(Tee::PriLow, "Ctrl0041Test: Start Test\n");
    Printf(Tee::PriLow, "Ctrl0041Test: Testing 4KB PageSize Memory Allocation...\n");
    if (TestGetPhyPageSize(LWOS32_ATTR_PAGE_SIZE_4KB, 0x1000, 4*KB))
    {
        Printf(Tee::PriHigh, "passed!\n\n");
    }
    else
    {
        Printf(Tee::PriHigh, "failed!\n\n");
    }

    Printf(Tee::PriLow, "Ctrl0041Test: Testing Big PageSize Memory Allocation...\n");
    const UINT64 bigPageSize = GetBoundGpuDevice()->GetBigPageSize();
    if (bigPageSize)
    {
        if (TestGetPhyPageSize(LWOS32_ATTR_PAGE_SIZE_BIG, 0x1000, bigPageSize))
        {
            Printf(Tee::PriHigh, "passed!\n\n");
        }
        else
        {
            Printf(Tee::PriHigh, "failed!\n\n");
            return RC::LWRM_ERROR;
        }
    }
    else
    {
        Printf(Tee::PriHigh, "failed!\n\n");
    }

    Printf(Tee::PriLow, "Ctrl0041Test: Testing Huge Page Size Memory Allocation...\n");
    if (GetBoundGpuDevice()->SupportsPageSize(GpuDevice::PAGESIZE_HUGE))
    {
        if (TestGetPhyPageSize(LWOS32_ATTR_PAGE_SIZE_HUGE, 0x200000,
                               GpuDevice::PAGESIZE_HUGE))
        {
            Printf(Tee::PriHigh, "passed!\n\n");
        }
        else
        {
            Printf(Tee::PriHigh, "failed!\n\n");
            return RC::LWRM_ERROR;
        }
    }
    else
    {
        Printf(Tee::PriHigh, "failed!\n\n");
    }

    Printf(Tee::PriLow, "Ctrl0041Test: Testing Default Page Size Memory Allocation...\n");
    Printf(Tee::PriLow, "Ctrl0041Test: Allocating Memory Size = 4KB + 1KB\n");
    if (TestGetPhyPageSize(LWOS32_ATTR_PAGE_SIZE_4KB, 0x1400, 4*KB))
    {
        Printf(Tee::PriHigh, "passed!\n\n");
    }
    else
    {
        Printf(Tee::PriHigh, "failed!\n\n");
    }

    Printf(Tee::PriLow, "Ctrl0041Test: Allocating Memory Size = 128KB + 4KB\n");
    if (bigPageSize)
    {
        if (TestGetPhyPageSize(LWOS32_ATTR_PAGE_SIZE_BIG, 0x21000, bigPageSize))
        {
            Printf(Tee::PriHigh, "passed!\n\n");
        }
        else
        {
            Printf(Tee::PriHigh, "failed!\n\n");
            return RC::LWRM_ERROR;
        }
    }
    else
    {
        Printf(Tee::PriHigh, "failed!\n\n");
    }

    Printf(Tee::PriLow, "Ctrl0041Test: Allocating Memory Size = 2MB + 64KB\n");
    if (GetBoundGpuDevice()->SupportsPageSize(GpuDevice::PAGESIZE_HUGE))
    {
        if (TestGetPhyPageSize(LWOS32_ATTR_PAGE_SIZE_HUGE, 0x210000,
                               GpuDevice::PAGESIZE_HUGE))
        {
            Printf(Tee::PriHigh, "passed!\n\n");
        }
        else
        {
            Printf(Tee::PriHigh, "failed!\n\n");
            return RC::LWRM_ERROR;
        }
    }
    else
    {
        Printf(Tee::PriHigh, "failed!\n\n");
    }

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC Ctrl0041Test::Cleanup()
{
    return OK;
}
//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//! \brief TestGetPhyPageSize - performs a single test
//!
//! Test an allocation of the specified pagesize memory
//! and return the physical memory pagesize.
//!
//! \return true if the test passes, else false
//------------------------------------------------------------------------------
bool Ctrl0041Test::TestGetPhyPageSize(UINT32 pageSizeAttr, UINT32 testBufferSize, UINT64 expectedPageSize)
{
    RC           rc;
    bool         status = false;
    LwRmPtr      pLwRm;
    UINT64       Offset;
    LwRm::Handle hMemory = 0;

    //Allocate the vid mem
    rc = pLwRm->VidHeapAllocSize(
         (UINT32)LWOS32_TYPE_IMAGE, LWOS32_ATTR_NONE | pageSizeAttr,
         LWOS32_ATTR2_NONE,
         testBufferSize,
         &hMemory,
         &Offset,
         nullptr,
         nullptr,
         nullptr,
         GetBoundGpuDevice());
    if (rc != OK)
    {
        Printf(Tee::PriHigh,"Video Heap Memory Allocation Failed\n");
        return false;
    }

    rc = LwRmControl(hClient,
                     hMemory,
                     LW0041_CTRL_CMD_GET_MEM_PAGE_SIZE,
                     (void*)&memPageSizeParams,
                     sizeof(memPageSizeParams));
    if (rc != OK)
    {
        Printf(Tee::PriHigh,"Failed to get memory page size\n");
        status = false;
        CHECK_RC_CLEANUP(RmApiStatusToModsRC(rc));
    }

    if (expectedPageSize == memPageSizeParams.pageSize)
    {
        Printf(Tee::PriNormal,"Allocated Physical Memory With PageSize = %dKB\n",
               (memPageSizeParams.pageSize/1024));
        status = true;
    }
    else if (expectedPageSize == (4*KB))
    {
        const UINT64 bigPageSize = GetBoundGpuDevice()->GetBigPageSize();
        if (memPageSizeParams.pageSize == bigPageSize)
            Printf(Tee::PriLow, "PMA allocates at a min granularity of %lldKB\n", bigPageSize/1024);

        status = false;
    }
    else
    {
        status = false;
    }

Cleanup:
    if (hMemory)
        pLwRm->Free(hMemory);

    return status;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ Ctrl2080FbTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(Ctrl0041Test, RmTest,
                 "Ctrl0041Test RM test.");

