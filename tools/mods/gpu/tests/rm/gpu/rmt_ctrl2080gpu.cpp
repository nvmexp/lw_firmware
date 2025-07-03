/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011,2016-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_ctrl2080gpu.cpp
//! \brief LW2080_CTRL_CMD GPU tests
//!
#include "ctrl/ctrl2080.h" // LW20_SUBDEVICE_XX
#include "gpu/tests/rmtest.h"
#include "core/include/platform.h"

#include "class/cl5097.h" // LW50_TESLA
#include "class/cl9097.h" // FERMI_A
#include "class/cla097.h" // KEPLER_A
#include "class/cla197.h" // KEPLER_B
#include "class/cla297.h" // KEPLER_C
#include "core/include/memcheck.h"

class Ctrl2080GpuTest: public RmTest
{
public:
    Ctrl2080GpuTest();
    virtual ~Ctrl2080GpuTest();

    virtual string     IsTestSupported();

    virtual RC       Setup();
    virtual RC       Run();
    virtual RC       Cleanup();

private:
    struct
    {
        LW2080_CTRL_GPU_INFO floorSweep;
        LW2080_CTRL_GPU_INFO ECIDLo;
        LW2080_CTRL_GPU_INFO ECIDHi;
        LW2080_CTRL_GPU_INFO computeEnable;
        LW2080_CTRL_GPU_INFO minorRevisionExt;
    } gpuInfo;

    LW2080_CTRL_GPU_GET_INFO_V2_PARAMS gpuInfoParams;

    // Bug 710555
    //LW2080_CTRL_GPU_RESET_EXCEPTIONS_PARAMS gpuResetExceptionsParams;
};

//! \brief Ctrl2080GpuTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
Ctrl2080GpuTest::Ctrl2080GpuTest()
{
    SetName("Ctrl2080GpuTest");
    memset(&gpuInfo, 0, sizeof(gpuInfo));
    memset(&gpuInfoParams, 0, sizeof(gpuInfoParams));
}

//! \brief Ctrl2080GpuTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
Ctrl2080GpuTest::~Ctrl2080GpuTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \return True
//-----------------------------------------------------------------------------
string Ctrl2080GpuTest::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa IsSupported
//-----------------------------------------------------------------------------
RC Ctrl2080GpuTest::Setup()
{
    RC rc;

    return rc;
}

//! \brief Run the test!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC Ctrl2080GpuTest::Run()
{
    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    RC rc;

    gpuInfo.floorSweep.index       = LW2080_CTRL_GPU_INFO_INDEX_FLOOR_SWEEP;
    gpuInfo.ECIDLo.index           = LW2080_CTRL_GPU_INFO_INDEX_ECID_LO32;
    gpuInfo.ECIDHi.index           = LW2080_CTRL_GPU_INFO_INDEX_ECID_HI32;
    gpuInfo.computeEnable.index    = LW2080_CTRL_GPU_INFO_INDEX_COMPUTE_ENABLE;
    gpuInfo.minorRevisionExt.index = LW2080_CTRL_GPU_INFO_INDEX_MINOR_REVISION_EXT;

    gpuInfoParams.gpuInfoListSize = sizeof (gpuInfo) / sizeof (LW2080_CTRL_GPU_INFO);

    LW2080_CTRL_GPU_INFO *pGpuInfo = (LW2080_CTRL_GPU_INFO*)&gpuInfo;
    for (UINT32 i = 0; i < gpuInfoParams.gpuInfoListSize; i++)
    {
        gpuInfoParams.gpuInfoList[i].index = pGpuInfo[i].index;
    }

    CHECK_RC(pLwRm->Control(hSubdevice,
                            LW2080_CTRL_CMD_GPU_GET_INFO_V2,
                            (void*)&gpuInfoParams,
                            sizeof(gpuInfoParams)));

    for (UINT32 i = 0; i < gpuInfoParams.gpuInfoListSize; i++)
    {
        pGpuInfo[i].data = gpuInfoParams.gpuInfoList[i].data;
    }

    Printf(Tee::PriNormal,
           "Ctrl2080GpuTest: GPU_GET_INFO - floorSweep       : 0x%x\n",
           (unsigned int)gpuInfo.floorSweep.data);
    Printf(Tee::PriNormal,
           "Ctrl2080GpuTest: GPU_GET_INFO - ECIDLo           : 0x%x\n",
           (unsigned int)gpuInfo.ECIDLo.data);
    Printf(Tee::PriNormal,
           "Ctrl2080GpuTest: GPU_GET_INFO - ECIDHi           : 0x%x\n",
           (unsigned int)gpuInfo.ECIDHi.data);
    Printf(Tee::PriNormal,
           "Ctrl2080GpuTest: GPU_GET_INFO - computeEnable    : 0x%x\n",
           (unsigned int)gpuInfo.computeEnable.data);
    Printf(Tee::PriNormal,
           "Ctrl2080GpuTest: GPU_GET_INFO - minorRevisionExt : 0x%x\n",
           (unsigned int)gpuInfo.minorRevisionExt.data);

    switch(gpuInfo.minorRevisionExt.data)
    {
        case LW2080_CTRL_GPU_INFO_MINOR_REVISION_EXT_NONE:
        case LW2080_CTRL_GPU_INFO_MINOR_REVISION_EXT_P:
        case LW2080_CTRL_GPU_INFO_MINOR_REVISION_EXT_V:
        case LW2080_CTRL_GPU_INFO_MINOR_REVISION_EXT_PV:
            break;

        default:
            Printf(Tee::PriNormal,
               "Ctrl2080GpuTest: invalid minorRevisionExt (0x%x)\n",
               (unsigned int)gpuInfo.minorRevisionExt.data);
            rc = RC::DATA_MISMATCH;
            break;
    }

    // Bug 710555: Call failing on win32 and linux build as well
/*
    // this portion of the test only works on lw5x+ gpus
    if(IsClassSupported(LW50_TESLA) ||
       IsClassSupported(FERMI_A)    ||
       IsClassSupported(KEPLER_A)   ||
       IsClassSupported(KEPLER_B)   ||
       IsClassSupported(KEPLER_C))
    {
        gpuResetExceptionsParams.targetEngine = (LwU32)LW2080_CTRL_GPU_RESET_EXCEPTIONS_ENGINE_GR;

        CHECK_RC(pLwRm->Control(hSubdevice,
                                LW2080_CTRL_CMD_GPU_RESET_EXCEPTIONS,
                                (void*)&gpuResetExceptionsParams,
                                sizeof(gpuResetExceptionsParams)));

    } */

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC Ctrl2080GpuTest::Cleanup()
{
    return OK;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ Ctrl2080GpuTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(Ctrl2080GpuTest, RmTest,
                 "Ctrl2080GpuTest RM test.");
