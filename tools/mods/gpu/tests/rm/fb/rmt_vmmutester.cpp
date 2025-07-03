/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_vmmutester.cpp
//! \brief TURING_VMMU_A Test
//!

#include "gpu/tests/rmtest.h"
#include "lwRmApi.h"
#include "lwos.h"
#include "core/include/jscript.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "core/include/lwrm.h"
#include "gpu/include/gpudev.h"

#include "class/cl0000.h"

#include "class/clc58b.h" // TURING_VMMU_A
#include "core/include/memcheck.h"

#define ALLOCATION_ALIGNMENT    (256 * 1024 * 1024)         // 256MB alignment is needed for SPA
#define ALLOCATION_SIZE         (4 * ALLOCATION_ALIGNMENT)  // Testing 1GB allocation size

class VmmuTester: public RmTest
{
public:
    VmmuTester();
    virtual ~VmmuTester();

    virtual string     IsTestSupported();

    virtual RC       Setup();
    virtual RC       Run();
    virtual RC       Cleanup();

private:
    RC MapGpaSpaTest(LwU64 rangeBegin, LwU64 rangeEnd);

    LwU32        m_hClient;
    LwU32        m_hDevice;
    LwU32        m_hSubdevice;
    LwRm::Handle m_hVmmuClassObj;
    LwU64        m_fbMaxPhysAddr;
};

//! \brief VmmuTester constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
VmmuTester::VmmuTester()
{
    SetName("VmmuTester");
}

//! \brief VmmuTester destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
VmmuTester::~VmmuTester()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \return True if G8x+
//-----------------------------------------------------------------------------
string VmmuTester::IsTestSupported()
{
    if(IsClassSupported(TURING_VMMU_A))
        return RUN_RMTEST_TRUE;
    return "TURING_VMMU_A not supported";
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa IsTestSupported
//-----------------------------------------------------------------------------
RC VmmuTester::Setup()
{
    RC      rc = OK;
    LwRmPtr pLwRm;
    TURING_VMMU_A_ALLOCATION_PARAMETERS params ={0};
    LW2080_CTRL_CMD_FB_GET_FB_REGION_INFO_PARAMS fbRegionInfoParams = {0};
    LwU32 status, i;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    // Allocate TURING_VMMU_A class object
    params.gfid = 1;
    CHECK_RC_CLEANUP(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                                   &m_hVmmuClassObj, TURING_VMMU_A, &params));

    m_hClient = pLwRm->GetClientHandle();
    m_hDevice = pLwRm->GetDeviceHandle(GetBoundGpuDevice());
    m_hSubdevice = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());

    status = LwRmControl(m_hClient, m_hSubdevice,
                         LW2080_CTRL_CMD_FB_GET_FB_REGION_INFO,
                         &fbRegionInfoParams,
                         sizeof(fbRegionInfoParams));

    if (status != LW_OK)
    {
        CHECK_RC_CLEANUP(RmApiStatusToModsRC(status));
    }

    m_fbMaxPhysAddr = 0;
    for (i = 0; i < fbRegionInfoParams.numFBRegions; ++i)
    {
        m_fbMaxPhysAddr = LW_MAX(m_fbMaxPhysAddr, fbRegionInfoParams.fbRegion[i].limit);
    }

Cleanup:
    return rc;
}

//! \brief Run the test!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC VmmuTester::Run()
{
    RC           rc = OK;
    LwRmPtr      pLwRm;
    LwU64        spaRangeBegin = 0x1E0000000;
    LwU32 i = 0;

    //
    // Allocate Physical memory from RM and then use the same to map to
    // defined GPA ranges
    //
    CHECK_RC_CLEANUP(MapGpaSpaTest(0, 0));

    //
    // RangeBased checks. Hope this will pass else try couple of times on
    // different boundaries
    //
    for (i = 1; i < 7; i++)
    {
        LwU64 rangeBegin = i * spaRangeBegin;
        if (rangeBegin < m_fbMaxPhysAddr)
        {
            rc = MapGpaSpaTest(rangeBegin - 1,
                               rangeBegin + ALLOCATION_SIZE + 1);
        }
        else
            break;

        //
        // Test will fail if we get any error apart from failing
        // rangeBased allocations
        //
        if (rc == OK || rc != RC::LWRM_INSUFFICIENT_RESOURCES)
            break;
    }

Cleanup:
    return rc;
}

//! \brief Allocate a physical location and map it to GPA!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC VmmuTester::MapGpaSpaTest(LwU64 rangeBegin, LwU64 rangeEnd)
{
    RC rc;
    LwRmPtr pLwRm;
    LWOS32_PARAMETERS allocParams = {0};
    LwU32 hMemory;
    LwU64 mappedGpa;

    // Step - 1 - Allocate Physical Memory
    allocParams.hRoot = m_hClient;;
    allocParams.hObjectParent = m_hDevice;
    allocParams.function = LWOS32_FUNCTION_ALLOC_SIZE;
    allocParams.data.AllocSize.owner = 0x61626364;
    allocParams.data.AllocSize.hMemory = 0;// out, we're allowing RM to create the handle.
    allocParams.data.AllocSize.type = LWOS32_TYPE_IMAGE;
    allocParams.data.AllocSize.size = ALLOCATION_SIZE;
    allocParams.data.AllocSize.flags |= LWOS32_ALLOC_FLAGS_ALIGNMENT_FORCE; // Need 256MB aligned allocations
    allocParams.data.AllocSize.alignment = ALLOCATION_ALIGNMENT;
    allocParams.data.AllocSize.attr = DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM) |
                                      DRF_DEF(OS32, _ATTR, _PHYSICALITY, _CONTIGUOUS);
    allocParams.data.AllocSize.attr |= DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _DEFAULT);

    // Do we have a range Hint ?
    if (rangeBegin != rangeEnd)
    {
        allocParams.data.AllocSize.flags |= LWOS32_ALLOC_FLAGS_USE_BEGIN_END;
        allocParams.data.AllocSize.rangeBegin = rangeBegin;
        allocParams.data.AllocSize.rangeEnd = rangeEnd;
    }

    rc = RmApiStatusToModsRC(LwRmVidHeapControl((void*)&allocParams));
    if(rc == OK)
    {
        // Save of memory handle for further usage
        hMemory  = allocParams.data.AllocSize.hMemory;
    }
    else
        return rc;

    // Step - 2 - Map Physical memory using RmMapMemoryDma using TURING_VMMU_A class handle
    rc = RmApiStatusToModsRC(LwRmMapMemoryDma(m_hClient, m_hDevice,
                                              m_hVmmuClassObj, hMemory,
                                              0, ALLOCATION_SIZE, 0, &mappedGpa));

    // Step - 3 Unmap VMMU Mappings
    rc = RmApiStatusToModsRC(LwRmUnmapMemoryDma(m_hClient, m_hDevice,
                                                m_hVmmuClassObj, hMemory,
                                                0, 0));

    // Step-4 Free Allocated Physical memory
    if (allocParams.data.AllocSize.hMemory)
    {
        rc = RmApiStatusToModsRC(LwRmFree(m_hClient, m_hDevice,
                                          allocParams.data.AllocSize.hMemory));
    }

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC VmmuTester::Cleanup()
{
    LwRmPtr pLwRm;

    if (m_hVmmuClassObj)
    {
        pLwRm->Free(m_hVmmuClassObj);
    }

    return OK;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ VmmuTester
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(VmmuTester, RmTest,
                 "VmmuTester RM test.");
