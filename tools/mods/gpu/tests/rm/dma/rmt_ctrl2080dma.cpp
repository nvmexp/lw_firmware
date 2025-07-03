/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2011,2016,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_ctrl2080dma.cpp
//! \brief LW2080_CTRL_CMD DMA tests
//!
#include "ctrl/ctrl2080.h" // LW20_SUBDEVICE_XX
#include "gpu/tests/rmtest.h"
#include "core/include/platform.h"
#include "core/include/memcheck.h"

class Ctrl2080DmaTest: public RmTest
{
public:
    Ctrl2080DmaTest();
    virtual ~Ctrl2080DmaTest();

    virtual string     IsTestSupported();

    virtual RC       Setup();
    virtual RC       Run();
    virtual RC       Cleanup();

private:
    LW2080_CTRL_DMA_GET_INFO_PARAMS dmaInfoParams;
};

//! \brief Ctrl2080DmaTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
Ctrl2080DmaTest::Ctrl2080DmaTest()
{
    SetName("Ctrl2080DmaTest");
    memset(&dmaInfoParams, 0, sizeof(dmaInfoParams));
}

//! \brief Ctrl2080DmaTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
Ctrl2080DmaTest::~Ctrl2080DmaTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \return True
//-----------------------------------------------------------------------------
string Ctrl2080DmaTest::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa IsSupported
//-----------------------------------------------------------------------------
RC Ctrl2080DmaTest::Setup()
{
    RC rc;

    return rc;
}

//! \brief Run the test!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC Ctrl2080DmaTest::Run()
{
    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    RC rc = OK;

    // issue call to retrieve all possible dma info values
    dmaInfoParams.dmaInfoTbl[LW2080_CTRL_DMA_INFO_INDEX_SYSTEM_ADDRESS_SIZE].index = LW2080_CTRL_DMA_INFO_INDEX_SYSTEM_ADDRESS_SIZE;
    dmaInfoParams.dmaInfoTblSize = 1;

    rc = pLwRm->Control(hSubdevice,
                        LW2080_CTRL_CMD_DMA_GET_INFO,
                        (void*)&dmaInfoParams,
                        sizeof(dmaInfoParams));
    if (rc != OK)
    {
        Printf(Tee::PriHigh, "Ctrl2080DmaTest: GET_INFO failed\n");
        return ~OK;
    }

    // print out values
    Printf(Tee::PriNormal,
           "Ctrl2080DmaTest: DMA_GET_INFO - SYSTEM_ADDRESS_SIZE  : 0x%x\n",
           (unsigned int)dmaInfoParams.dmaInfoTbl[LW2080_CTRL_DMA_INFO_INDEX_SYSTEM_ADDRESS_SIZE].data);

    // test proper handling of table size violation
    dmaInfoParams.dmaInfoTblSize = LW2080_CTRL_DMA_GET_INFO_MAX_ENTRIES+1;
    rc = pLwRm->Control(hSubdevice,
                        LW2080_CTRL_CMD_DMA_GET_INFO,
                        (void*)&dmaInfoParams,
                        sizeof(dmaInfoParams));

    if (rc != RC::LWRM_ILWALID_PARAM_STRUCT)
    {
        Printf(Tee::PriHigh, "Ctrl2080DmaTest: table size violation test failure: %s\n", rc.Message());
        return ~OK;
    }

    return OK;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC Ctrl2080DmaTest::Cleanup()
{
    return OK;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ Ctrl2080DmaTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(Ctrl2080DmaTest, RmTest,
                 "Ctrl2080DmaTest RM test.");
