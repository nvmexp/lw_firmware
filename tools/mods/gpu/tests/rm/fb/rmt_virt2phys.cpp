/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2015,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_virt2phys.cpp
//! \brief Verify virtual-to-physical address translation. Uses busverifybar2 to check 4kb-page mappings then
//!        checks the first 1kb of big-page mappings
//!
//! Verify virtual-to-physical address translation. Uses busverifybar2 to check 4kb-page mappings then
//!        checks the first 1kb of big-page mappings

#include "lwos.h"
#include "lwRmApi.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "class/cl0040.h" // LW01_MEMORY_LOCAL_USER
#include "class/cl5097.h" // LW50_TESLA
#include "class/cl5097sw.h" // LW50_TESLA
#include "class/cl9097.h" //FERMI_A
#include "class/cl9097sw.h" //FERMI_A
#include "class/cl0070.h"
#include "ctrl/ctrl2080.h"
#include "gpu/include/gralloc.h"
#include "core/include/memcheck.h"

class Virtual2Physical : public RmTest
{
public:
    Virtual2Physical();
    virtual ~Virtual2Physical();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:

    bool TestMapping(UINT32 pageSizeAttr, UINT32 testBufferSize);

    LwRm::Handle m_hClient = 0;
    LwRm::Handle m_hDevice = 0;
    LwRm::Handle m_hSubDev = 0;
    ThreeDAlloc  m_3dAlloc;
};

//! \brief Virtual2Physical constructor
//!
//! \sa Setup
//------------------------------------------------------------------------------
Virtual2Physical::Virtual2Physical()
{
    SetName("Virtual2Physical");
    m_3dAlloc.SetOldestSupportedClass(FERMI_A);
}

//! \brief Virtual2Physical destructor
//!
//! All resources should be freed in the Cleanup member function, so
//! destructors shouldn't do much.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
Virtual2Physical::~Virtual2Physical()
{

}

//! \brief This tests is supported on FERMI only
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string Virtual2Physical::IsTestSupported()
{
    //
    // Run this test only on FERMI and above.
    //
    if (m_3dAlloc.IsSupported(GetBoundGpuDevice()))
    {
        return RUN_RMTEST_TRUE;
    }
    else
        return "Supported only on Fermi+";

}

//! \brief Setup()
//!
//! allocates the client, device, and subdevice handles
//
//! \return OK
//------------------------------------------------------------------------------
RC Virtual2Physical::Setup()
{
    RC rc;
    LwRmPtr   pLwRm;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    // Allocate client handle
    m_hClient = pLwRm->GetClientHandle();

    // Allocate Device handle
    m_hDevice = pLwRm->GetDeviceHandle(GetBoundGpuDevice());

    // Allocate Subdevice handle
    m_hSubDev = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());

    return rc;
}

//! \brief Run()
//!
//! Core test functionality.  Performs 2 tests, one on small pages (4KB) and one on big pages
//! Each test will verify address translation for the allocation of associated page-size
//!
//! \return OK when both tests pass else return LWRM_ERROR
//------------------------------------------------------------------------------
RC Virtual2Physical::Run()
{
    RC rc;

    Printf(Tee::PriLow, "Virtual2Physical: Start Test\n");

    Printf(Tee::PriLow, "Virtual2Physical: Testing 4K Page Mappings...  ");
    if (TestMapping(LWOS32_ATTR_PAGE_SIZE_4KB, 0x1000) )
    {
        Printf(Tee::PriLow, "passed!\n");
    }
    else
    {
        Printf(Tee::PriLow, "failed!\n");
        return RC::LWRM_ERROR;
    }

    Printf(Tee::PriLow, "Virtual2Physical: Testing Big Page Mappings...  ");
    if (TestMapping(LWOS32_ATTR_PAGE_SIZE_BIG, 0x1000) )
    {
        Printf(Tee::PriLow, "passed!\n");
    }
    else
    {
        Printf(Tee::PriLow, "failed!\n");
        return RC::LWRM_ERROR;
    }

    return rc;
}

//! \brief Cleanup()
//!
//! Nothing to clean up from this test
//------------------------------------------------------------------------------
RC Virtual2Physical::Cleanup()
{
    return OK;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//! \brief TestMapping - performs a single test
//!
//! Tests an allocation of the specified pagesize and overall allocation size
//! Uses busVerifyBar2 to perform a test of the allocation
//!
//! \return true if the test passes, else false
//------------------------------------------------------------------------------
bool Virtual2Physical::TestMapping(UINT32 pageSizeAttr, UINT32 testBufferSize)
{
    RC              rc;
    LwRmPtr         pLwRm;
    UINT32          status;
    UINT08          *cpuAddr = nullptr;
    UINT64          Offset;
    LwRm::Handle    hMemory = 0;
    LW2080_CTRL_BUS_MAP_BAR2_PARAMS mapBar2Params = {0};
    LW2080_CTRL_BUS_VERIFY_BAR2_PARAMS verifyBar2Params = {0};
    LW2080_CTRL_BUS_UNMAP_BAR2_PARAMS unmapBar2Params = {0};

    if (pageSizeAttr != LWOS32_ATTR_PAGE_SIZE_4KB &&
        pageSizeAttr != LWOS32_ATTR_PAGE_SIZE_BIG )
    {
        Printf(Tee::PriHigh, "Virtual2Physical: Invalid Page Size specification for test: %d\n", pageSizeAttr);
        return false;
    }

    // First allocate the vid mem and set the cpu mapping
    CHECK_RC_CLEANUP(pLwRm->VidHeapAllocSize(
        (UINT32)LWOS32_TYPE_IMAGE, LWOS32_ATTR_NONE | pageSizeAttr, LWOS32_ATTR2_NONE, testBufferSize, &hMemory, &Offset,
        nullptr, nullptr, nullptr,
        GetBoundGpuDevice()));

    CHECK_RC_CLEANUP(pLwRm->MapMemory(
        hMemory, 0, testBufferSize, (void **)&cpuAddr, 0, GetBoundGpuSubdevice()));

    mapBar2Params.hMemory = hMemory;

    status = LwRmControl(m_hClient, m_hSubDev,
                         LW2080_CTRL_CMD_BUS_MAP_BAR2,
                         &mapBar2Params, sizeof(LW2080_CTRL_BUS_MAP_BAR2_PARAMS));

    if (status != LW_OK )
    {
        Printf(Tee::PriHigh, "Virtual2Physical: Failed to map BAR2\n");
        return false;
    }

    verifyBar2Params.hMemory = hMemory;
    verifyBar2Params.offset = 0;
    verifyBar2Params.size = testBufferSize;

    status = LwRmControl(m_hClient, m_hSubDev,
                         LW2080_CTRL_CMD_BUS_VERIFY_BAR2,
                         &verifyBar2Params, sizeof(LW2080_CTRL_BUS_VERIFY_BAR2_PARAMS));

    if (status != LW_OK )
    {
        Printf(Tee::PriHigh, "Virtual2Physical: Failed to verify BAR2\n");
        return false;
    }

    unmapBar2Params.hMemory = hMemory;

    status = LwRmControl(m_hClient, m_hSubDev,
                         LW2080_CTRL_CMD_BUS_UNMAP_BAR2,
                         &unmapBar2Params, sizeof(LW2080_CTRL_BUS_UNMAP_BAR2_PARAMS));

    if (status != LW_OK )
    {
        Printf(Tee::PriHigh, "Virtual2Physical: Failed to un-map BAR2\n");
        return false;
    }

Cleanup:
    // Unmap the FB registers and video memory
    if (cpuAddr)
        pLwRm->UnmapMemory(hMemory, cpuAddr, 0, GetBoundGpuSubdevice());
    if (hMemory)
        pLwRm->Free(hMemory);

    return true;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Virtual2Physical, RmTest,
    "Verify Virtual-to-Physical Address Translation");

