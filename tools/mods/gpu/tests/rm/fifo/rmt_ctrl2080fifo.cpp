/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2011,2016-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_ctrl2080fifo.cpp
//! \brief LW2080_CTRL_CMD FIFO tests
//!
#include "ctrl/ctrl2080.h" // LW20_SUBDEVICE_XX
#include "gpu/tests/rmtest.h"
#include "core/include/platform.h"
#include "core/include/memcheck.h"

class Ctrl2080FifoTest: public RmTest
{
public:
    Ctrl2080FifoTest();
    virtual ~Ctrl2080FifoTest();

    virtual string     IsTestSupported();

    virtual RC       Setup();
    virtual RC       Run();
    virtual RC       Cleanup();

private:
    LW2080_CTRL_FIFO_GET_INFO_PARAMS fifoInfoParams;
};

//! \brief Ctrl2080FifoTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
Ctrl2080FifoTest::Ctrl2080FifoTest()
{
    SetName("Ctrl2080FifoTest");
    memset(&fifoInfoParams, 0, sizeof(fifoInfoParams));
}

//! \brief Ctrl2080FifoTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
Ctrl2080FifoTest::~Ctrl2080FifoTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \return True
//-----------------------------------------------------------------------------
string Ctrl2080FifoTest::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa IsTestSupported
//-----------------------------------------------------------------------------
RC Ctrl2080FifoTest::Setup()
{
    RC rc;

    return rc;
}

//! \brief Run the test!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC Ctrl2080FifoTest::Run()
{
    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    RC rc;

    // issue call to retrieve all possible fifo info values
    fifoInfoParams.fifoInfoTbl[LW2080_CTRL_FIFO_INFO_INDEX_INSTANCE_TOTAL].index = LW2080_CTRL_FIFO_INFO_INDEX_INSTANCE_TOTAL;
    fifoInfoParams.fifoInfoTblSize = 1;

    rc = pLwRm->Control(hSubdevice,
                        LW2080_CTRL_CMD_FIFO_GET_INFO,
                        (void*)&fifoInfoParams,
                        sizeof(fifoInfoParams));
    if (rc != OK)
    {
        Printf(Tee::PriHigh, "Ctrl2080FifoTest: GET_INFO failed\n");
        return ~OK;
    }

    // print out values
    Printf(Tee::PriNormal,
           "Ctrl2080FifoTest: FIFO_GET_INFO - INSTANCE_TOTAL  : 0x%x\n",
           (unsigned int)fifoInfoParams.fifoInfoTbl[LW2080_CTRL_FIFO_INFO_INDEX_INSTANCE_TOTAL].data);

    // test proper handling of table size violation
    fifoInfoParams.fifoInfoTblSize = LW2080_CTRL_FIFO_GET_INFO_MAX_ENTRIES+1;
    rc = pLwRm->Control(hSubdevice,
                        LW2080_CTRL_CMD_FIFO_GET_INFO,
                        (void*)&fifoInfoParams,
                        sizeof(fifoInfoParams));

    if (rc != RC::LWRM_ILWALID_PARAM_STRUCT)
    {
        Printf(Tee::PriHigh, "Ctrl2080FifoTest: table size violation test failure: %s\n", rc.Message());
        return ~OK;
    }

    return OK;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC Ctrl2080FifoTest::Cleanup()
{
    return OK;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ Ctrl2080FifoTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(Ctrl2080FifoTest, RmTest,
                 "Ctrl2080FifoTest RM test.");
