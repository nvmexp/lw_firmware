/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_ctrl0080bif.cpp
//! \brief LW0080_CTRL_CMD_BIF Tests
//!

#include "gpu/tests/rmtest.h"
#include "core/include/utility.h"
#include "ctrl/ctrl0080.h" // LW01_DEVICE_XX/LW03_DEVICE
#include "core/include/memcheck.h"

class Ctrl0080BifTest: public RmTest
{
public:
    Ctrl0080BifTest();
    virtual ~Ctrl0080BifTest();

    virtual string   IsTestSupported();

    virtual RC       Setup();
    virtual RC       Run();
    virtual RC       Cleanup();
};

//! \brief Ctrl0080BifTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
Ctrl0080BifTest::Ctrl0080BifTest()
{
    SetName("Ctrl0080BifTest");
}

//! \brief Ctrl0080BifTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
Ctrl0080BifTest::~Ctrl0080BifTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \return True
//-----------------------------------------------------------------------------
string Ctrl0080BifTest::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa IsSupported
//-----------------------------------------------------------------------------
RC Ctrl0080BifTest::Setup()
{
    RC rc;

    return rc;
}

//! \brief Run the test!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC Ctrl0080BifTest::Run()
{
    LwRmPtr pLwRm;
    RC      rc;
    LW0080_CTRL_BIF_GET_DMA_BASE_SYSMEM_ADDR_PARAMS dmaBaseAddrParams= {0};

    // Get DmaSysmemBaseAddr
    CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                            LW0080_CTRL_CMD_BIF_GET_DMA_BASE_SYSMEM_ADDR,
                            &dmaBaseAddrParams,
                            sizeof(dmaBaseAddrParams)));

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC Ctrl0080BifTest::Cleanup()
{
    RC rc;

    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ Ctrl0080BifTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(Ctrl0080BifTest, RmTest,
                 "Ctrl0080BifTest RM test.");

