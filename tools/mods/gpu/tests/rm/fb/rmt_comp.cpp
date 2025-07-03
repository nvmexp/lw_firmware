/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2015,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_comp.cpp
//! \brief Verifies that compression tags support is not turned on for gt206.
//!
//! This test attempts to allocate some data that is compressed and verifies
//! that it passes on non-GT206 cards and fails on GT206.  In addition, the
//! test will try to allocate with the COMPR_ANY set and verify that the
//! allocation does pass on both cards.

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "lwos.h"
#include "lwRmApi.h"
#include "core/utility/errloggr.h"
#include "core/include/channel.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/memcheck.h"

class CompTest : public RmTest
{
public:
    CompTest();
    virtual ~CompTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
};

//! \brief Class Constructor
//!
//! Indicate the name of the test.
//!
//! \sa Setup
//------------------------------------------------------------------------------
CompTest::CompTest()
{
    SetName("CompTest");
}

//! \brief Class Destructor
//!
//! Lwrrently, nothing is done in this function
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
CompTest::~CompTest()
{
}

//! \brief Check that this chip supports compression tags (compression tags
//! may be supported without supporting compression).
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string CompTest::IsTestSupported()
{

    if ((GetBoundGpuDevice())->GetFamily() >= GpuDevice::Ampere)
        return "Not Supported on Ampere+ chips";

    if (GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_HAS_COMPRESSION_TAGS))
    {
        return RUN_RMTEST_TRUE;
    }
    return "GPUSUB_HAS_COMPRESSION_TAGS feature is not supported on current platform";
}

//! \brief Setup the test.
//!
//! Setup internal state for the test.
//
//! \return OK if there are no errors, or an error code.
//------------------------------------------------------------------------------
RC CompTest::Setup()
{
    RC rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    return OK;
}

//! \brief Verify that the compression tags are supported properly.
//!
//! This test will try to allocate some memory with compression required
//! and verify that on the gt206 card, this allocation will fail.  The
//! second part of the test is to allocate some memory that can either be
//! compressed or not.  Verify that this does allocate correctly on
//! the gt206.  Lwrrently, it doesn't check to make sure that the memory
//! allocated is not compressed.
//!
//! \return OK if the test passes or an error code if there is a failure.
//------------------------------------------------------------------------------
RC CompTest::Run()
{
    RC rc;
    LwRmPtr  pLwRm;
    LwRm::Handle hMemory;
    LwU32  attr, size;
    bool CompressionEnabled;
    UINT32 retVal;

    ErrorLogger::StartingTest();

    //
    // Query RM to see if compression is enabled.  If it isn't, then there is no
    // point in testing _COMPR == _REQUIRED
    //
    LW2080_CTRL_FB_INFO fbInfo = {0};
    fbInfo.index = LW2080_CTRL_FB_INFO_INDEX_COMPRESSION_SIZE;
    LW2080_CTRL_FB_GET_INFO_PARAMS fbInfoParams = { 1, LW_PTR_TO_LwP64(&fbInfo) };
    retVal = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
        LW2080_CTRL_CMD_FB_GET_INFO,
        &fbInfoParams,
        sizeof (fbInfoParams));
    CHECK_RC(RmApiStatusToModsRC(retVal));
    CompressionEnabled = (fbInfo.data != 0);
    Printf(Tee::PriHigh,"%s:Compression %s\n",
        __FUNCTION__,
        (CompressionEnabled ? "supported" : "not supported"));

    Printf(Tee::PriHigh,"%s:Testing compression = _REQUIRED allocation\n", __FUNCTION__);
    //
    // First verify that a compression required request fails on GT206 and
    // passes on GPUs that do not have compression disabled.
    //
    attr = DRF_DEF(OS32, _ATTR, _COMPR, _REQUIRED) |
           DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM) |
           DRF_DEF(OS32, _ATTR, _COLOR_PACKING, _X8R8G8B8) |
           DRF_DEF(OS32, _ATTR, _DEPTH, _32) |
           DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1) |
           DRF_DEF(OS32, _ATTR, _FORMAT, _BLOCK_LINEAR);

    size = 0x100;

    rc = pLwRm->VidHeapAllocSize(LWOS32_TYPE_IMAGE, attr, LWOS32_ATTR2_NONE, size, &hMemory,
                                 nullptr, nullptr, nullptr, nullptr,
                                 GetBoundGpuDevice());
    if (rc == LW_OK)
    {
        // Free the memory
        pLwRm->Free(hMemory);
    }

    // GT206 and GPUs with compression disabled do not support compression tags.
    if (!GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_HAS_COMPRESSION))
    {
        if (rc == LW_OK)
        {
            return RC::SOFTWARE_ERROR;
        }
    }
    else if (rc != LW_OK)
    {
        // If compression == _REQUIRED failed, check if compression is enabled
        if (CompressionEnabled)
        {
            // Compression is enabled -- allocation should have succeeded
            return RC::SOFTWARE_ERROR;
        }
    }

    Printf(Tee::PriHigh,"%s:Testing compression = _ANY allocation\n", __FUNCTION__);
    // Now verify that a compression any request passes on all cards.
    attr = DRF_DEF(OS32, _ATTR, _COMPR, _ANY) |
           DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM) |
           DRF_DEF(OS32, _ATTR, _COLOR_PACKING, _X8R8G8B8) |
           DRF_DEF(OS32, _ATTR, _DEPTH, _32) |
           DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1) |
           DRF_DEF(OS32, _ATTR, _FORMAT, _BLOCK_LINEAR);

    size = 0x100;

    rc = pLwRm->VidHeapAllocSize(LWOS32_TYPE_IMAGE, attr, LWOS32_ATTR2_NONE, size, &hMemory,
                                 nullptr, nullptr, nullptr, nullptr,
                                 GetBoundGpuDevice());
    if (rc == LW_OK)
    {
        // Free the memory
        pLwRm->Free(hMemory);
    }

    //
    // This test should be expanded to verify that the requested memory
    // is not compressed on a gt206.
    //

    if (rc != LW_OK)
    {
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

//! \brief Cleans up after the test.
//!
//! Does nothing for this test.
//------------------------------------------------------------------------------
RC CompTest::Cleanup()
{
    return OK;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(CompTest, RmTest,
    "Memory Compression Support Test");

