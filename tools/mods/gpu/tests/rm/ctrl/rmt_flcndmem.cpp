 /*
  * LWIDIA_COPYRIGHT_BEGIN
  *
  * Copyright 2014 by LWPU Corporation.  All rights reserved.  All
  * information contained herein is proprietary and confidential to LWPU
  * Corporation.  Any use, reproduction, or disclosure without the written
  * permission of LWPU Corporation is prohibited.
  *
  * LWIDIA_COPYRIGHT_END
  */

#include "gpu/tests/rm/utility/rmtestutils.h"
#include "gpu/tests/rmtest.h"
#include "ctrl/ctrl2080.h"
#include "core/utility/errloggr.h"
#include "gpu/tests/rmtest.h"
#include "gpu/include/gpumgr.h"

#include "class/cl2080.h"
#include "core/include/memcheck.h"

class FlcnDmemTest: public RmTest
{
public:
    FlcnDmemTest();
    virtual ~FlcnDmemTest();

    virtual string IsTestSupported();
    virtual RC   Setup();
    virtual RC   Run();
    virtual RC   Cleanup();
};

//------------------------------------------------------------------------------
FlcnDmemTest::FlcnDmemTest()
{
    SetName("FlcnDmemTest");
}

//! \brief GpuCacheTest destructor
//!
//! Placeholder : doesnt do much.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
FlcnDmemTest::~FlcnDmemTest()
{
}

//! \brief IsTestSupported(), Looks for whether test can execute in current elw.
//!
//! Determines is test is supported by checking for ENCODER class availability.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string FlcnDmemTest::IsTestSupported()
{
    if (!(GetBoundGpuSubdevice()->DeviceId() >= Gpu::GM107))
        return "Test only supported on GM107 or later chips";

    return RUN_RMTEST_TRUE;
}

//! \brief  Setup(): Generally used for any test level allocation
//!
//! Allocates some number of test data structs for later use
//!
//! \return corresponding RC if any allocation fails
//------------------------------------------------------------------------------
RC FlcnDmemTest::Setup()
{
    return OK;
}

//! \brief Cleanup::
//!
//! Cleans up after test completes
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------
RC FlcnDmemTest::Cleanup()
{
    return OK;
}

//! \brief Run:: Used generally for placing all the testing stuff.
//!
//! Runs the required tests.
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------
RC FlcnDmemTest::Run()
{
    LwRmPtr pLwRm;
    RC      rc = OK;

    LW2080_CTRL_FLCN_GET_DMEM_USAGE_PARAMS pmuHeapInfo = {0};
    pmuHeapInfo.flcnID = FALCON_ID_PMU;
    LW2080_CTRL_FLCN_GET_DMEM_USAGE_PARAMS dpuHeapInfo = {0};
    dpuHeapInfo.flcnID = FALCON_ID_DPU;
    LW2080_CTRL_FLCN_GET_DMEM_USAGE_PARAMS sec2HeapInfo = {0};
    sec2HeapInfo.flcnID = FALCON_ID_SEC2;

    //
    // Query PMU DMEM usage
    //
    pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                   LW2080_CTRL_CMD_FLCN_GET_DMEM_USAGE,
                   &pmuHeapInfo,
                   sizeof(pmuHeapInfo));

    Printf(Tee::PriHigh, "[PMU] HeapSize = %u byte, FreeSize = %u byte\n", pmuHeapInfo.heapSize, pmuHeapInfo.heapFree);

    //
    // Query DPU DMEM usage
    //
    pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                   LW2080_CTRL_CMD_FLCN_GET_DMEM_USAGE,
                   &dpuHeapInfo,
                   sizeof(dpuHeapInfo));

    Printf(Tee::PriHigh, "[DPU] HeapSize = %u byte, FreeSize = %u byte\n", dpuHeapInfo.heapSize, dpuHeapInfo.heapFree);

    //
    // Query SEC2 DMEM usage
    //
    pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                   LW2080_CTRL_CMD_FLCN_GET_DMEM_USAGE,
                   &sec2HeapInfo,
                   sizeof(sec2HeapInfo));

    Printf(Tee::PriHigh, "[SEC2] HeapSize = %u byte, FreeSize = %u byte\n", sec2HeapInfo.heapSize, sec2HeapInfo.heapFree);

    return rc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ FlcnDmemTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(FlcnDmemTest, RmTest, "FlcnDmem RM test.");
