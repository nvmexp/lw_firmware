/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_ctrli2c.cpp
//! \brief LW2080_CTRL_I2C Tests
//!

#include "ctrl/ctrl2080.h" // LW20_SUBDEVICE_XX
#include "core/include/platform.h"
#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/rmtest.h"
#include "core/include/memcheck.h"

class CtrlI2CTest: public RmTest
{
public:
    CtrlI2CTest();
    virtual ~CtrlI2CTest();

    virtual string IsTestSupported();

    virtual RC   Setup();
    virtual RC   Run();
    virtual RC   Cleanup();

private:
    RC    WriteBuffer(LwU32 port);
    RC    ReadBuffer(LwU32 port);
    RC    ReadReg(LwU32 port);
    RC    WriteReg(LwU32 port);

    RC    AccessControl();
    RC    AccessCtrlStart(LwU32 port);
    RC    AccessCtrlStop(LwU32 port);

    RC    AccessCtrlReadBuffer(LwU32 port);
    RC    AccessCtrlWriteBuffer(LwU32 port);
};

//! \brief CtrlI2CTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
CtrlI2CTest::CtrlI2CTest()
{
    SetName("CtrlI2CTest");
}

//! \brief CtrlI2CTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
CtrlI2CTest::~CtrlI2CTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \return true if running under cmodel, the intended target of this test.
//-----------------------------------------------------------------------------
string CtrlI2CTest::IsTestSupported()
{
   if ((Platform::GetSimulationMode() == Platform::RTL) &&
       (GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_HAS_LW2080_CTRL_I2C)))
        return RUN_RMTEST_TRUE;

    return "Supported on Cmodel and should have GPUSUB_HAS_LW2080_CTRL_I2C device feature supported on current platform";
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa Setup
RC CtrlI2CTest::Setup()
{
    Printf(Tee::PriNormal, "CtrlI2CTest: Setup\n");

    return OK;
}

//! \brief Run the test!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC CtrlI2CTest::Run()
{
    RC rc;
    LwU32   i = 0;

    Printf(Tee::PriNormal, "CtrlI2CTest: Run\n");

    for (i=0; i < 4; i++)
    {
        WriteBuffer(i);

        ReadBuffer(i);

        ReadReg (i);
        WriteReg (i);
    }

    AccessControl();

    return rc;
}

RC CtrlI2CTest::WriteBuffer(LwU32 port)
{
    LW2080_CTRL_I2C_WRITE_BUFFER_PARAMS params = {0};
    LwRmPtr pLwRm;
    RC      rc;

    Printf(Tee::PriNormal, "CtrlI2CTest: WriteBuffer\n");

    params.version = LW2080_CTRL_I2C_VERSION_0;
    params.flags = 0;
    params.port = port;
    params.inputBuffer[0] = 0x25;        // addr
    params.inputBuffer[1] = 0x20;        // reg;
    params.inputCount = 2 + 1;
    memcpy(params.inputBuffer+2, "1234567890", 10);

    pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                LW2080_CTRL_CMD_I2C_WRITE_BUFFER,
                &params,
                sizeof (params));
                return rc;
}

RC CtrlI2CTest::ReadBuffer(LwU32 port)
{
    LW2080_CTRL_I2C_READ_BUFFER_PARAMS params = {0};
    LwRmPtr pLwRm;
    RC      rc;

    Printf(Tee::PriNormal, "CtrlI2CTest: ReadBuffer\n");

    params.version = LW2080_CTRL_I2C_VERSION_0;
    params.flags = 0;
    params.port = port;
    params.inputBuffer[0] = 0x36;       // addr
    params.inputBuffer[1] = 0x20;       // reg;
    params.inputCount = 2;
    params.outputCount = 2;

    rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                            LW2080_CTRL_CMD_I2C_READ_BUFFER,
                            &params,
                            sizeof (params));
    return rc;
}

RC CtrlI2CTest::ReadReg(LwU32 port)
{
    LW2080_CTRL_I2C_READ_REG_PARAMS params = {0};
    LwRmPtr pLwRm;
    RC      rc;

    Printf(Tee::PriNormal, "CtrlI2CTest: ReadReg\n");

    params.port = port;
    params.flags = 0;
    params.addr = 0x10;
    params.reg = 0x20;
    params.bufsize = 4;

    rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                            LW2080_CTRL_CMD_I2C_READ_REG,
                            &params,
                            sizeof (params));

    return rc;
}

RC CtrlI2CTest::WriteReg(LwU32 port)
{
    LW2080_CTRL_I2C_READ_REG_PARAMS params = {0};
    LwRmPtr pLwRm;
    RC      rc;

    Printf(Tee::PriNormal, "CtrlI2CTest: WriteReg\n");

    params.port = port;
    params.reg = 0x20;
    params.bufsize = 4;
    strcpy((char *)params.buffer, "1234");

    params.flags = 0;
    params.addr = 0x10;

    rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                            LW2080_CTRL_CMD_I2C_WRITE_REG,
                            &params,
                            sizeof (params));
    return rc;
}

/*
 *
 */

RC CtrlI2CTest::AccessCtrlStart(LwU32 port)
{
    LW2080_CTRL_I2C_ACCESS_PARAMS params = {0};
    LwRmPtr pLwRm;
    RC      rc;

    Printf(Tee::PriNormal, "CtrlI2CTest: AccessCtrlStart\n");

    params.cmd = LW2080_CTRL_I2C_ACCESS_CMD_START;
    params.port = port;

    rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                            LW2080_CTRL_CMD_I2C_ACCESS,
                            &params,
                            sizeof (params));
    return rc;
}

RC CtrlI2CTest::AccessCtrlStop(LwU32 port)
{
    LW2080_CTRL_I2C_ACCESS_PARAMS params = {0};
    LwRmPtr pLwRm;
    RC      rc;

    Printf(Tee::PriNormal, "CtrlI2CTest: AccessCtrlStop\n");

    params.cmd = LW2080_CTRL_I2C_ACCESS_CMD_STOP;
    params.port = port;

    rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                            LW2080_CTRL_CMD_I2C_ACCESS,
                            &params,
                            sizeof (params));
    return rc;
}

RC CtrlI2CTest::AccessCtrlWriteBuffer(LwU32 port)
{
    LW2080_CTRL_I2C_ACCESS_PARAMS params = {0};
    LwRmPtr pLwRm;
    RC      rc;
    LwU8    databuf[120];

    Printf(Tee::PriNormal, "CtrlI2CTest: AccessCtrlReadWriteBuffer\n");

    params.cmd = LW2080_CTRL_I2C_ACCESS_CMD_WRITE_BUFFER;
    params.port = port;
    params.flags = 0;
    params.data = LW_PTR_TO_LwP64(databuf);
    params.dataBuffSize = 20;

    rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                            LW2080_CTRL_CMD_I2C_ACCESS,
                            &params,
                            sizeof (params));
    Printf(Tee::PriNormal, "CtrlI2CTest::AccessCtrlReadWriteBuffer  RC = %d\n", (int)rc);
    return rc;
}

RC CtrlI2CTest::AccessCtrlReadBuffer(LwU32 port)
{
    LW2080_CTRL_I2C_ACCESS_PARAMS params = {0};
    LwRmPtr pLwRm;
    RC      rc;
    LwU8    databuf[120];

    Printf(Tee::PriNormal, "CtrlI2CTest: AccessCtrlReadWriteBuffer\n");

    params.cmd = LW2080_CTRL_I2C_ACCESS_CMD_WRITE_BUFFER;
    params.port = port;
    params.flags = 0;
    params.data = LW_PTR_TO_LwP64(databuf);
    params.dataBuffSize = 10;

    rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                            LW2080_CTRL_CMD_I2C_ACCESS,
                            &params,
                            sizeof (params));
    Printf(Tee::PriNormal, "CtrlI2CTest::AccessCtrlReadWriteBuffer  RC = %d\n", (int)rc);
    return rc;
}

RC CtrlI2CTest::AccessControl()
{
    LwRmPtr pLwRm;
    RC      rc;
    LW2080_CTRL_I2C_ACCESS_PARAMS params = {0};
    LwU32   i = 0;

    Printf(Tee::PriNormal, "CtrlI2CTest::AccessControl\n");

    for (i=0; i< 4; i++)
    {
        params.cmd = LW2080_CTRL_I2C_ACCESS_CMD_ACQUIRE;
        params.flags = 0;
        params.port = i;

        rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                            LW2080_CTRL_CMD_I2C_ACCESS,
                            &params,
                            sizeof (params));

        Printf(Tee::PriNormal, "CtrlI2CTest::AccessControl  LW2080_CTRL_CMD_I2C_ACCESS RC = %d\n", (int)rc);

        AccessCtrlStart(i);
        AccessCtrlWriteBuffer(i);
        AccessCtrlStop(i);

        params.cmd = LW2080_CTRL_I2C_ACCESS_CMD_RELEASE;
        params.flags = 0;
        params.port = i;

        rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                                LW2080_CTRL_CMD_I2C_ACCESS,
                                &params,
                                sizeof (params));
    }

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC CtrlI2CTest::Cleanup()
{
    return OK;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ CtrlI2CTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(CtrlI2CTest, RmTest,
                 "CtrlI2CTest  test.");
