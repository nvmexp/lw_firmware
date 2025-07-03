/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2011,2016-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
//! \file PbiCmdTest.cpp
//! \brief PbiCmdTest functionality test
//! \Small example file to test PBI COMMANDS
//!
#include "core/include/tee.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "core/include/lwrm.h"
#include "lwSelwrity.h"
#include "rmpbicmdif.h"
#include "ctrl/ctrl208f.h"
#include "class/cl85b6.h"
#include "lwRmApi.h"
#include "core/include/platform.h"
#include "core/include/rc.h"
#include "core/include/memcheck.h"

class PbiCmdTest: public RmTest
{
public:
    PbiCmdTest();
    virtual ~PbiCmdTest();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    LwRm::Handle    m_hSubDevDiag;
};

//!
//! \brief PbiCmdTest constructor
//!
//!PbiCmdTest constructor does not do much. Functionality
//! mostly lies in Setup().
//! \sa Setup
//------------------------------------------------------------------------------
PbiCmdTest::PbiCmdTest()
{
    SetName("PbiCmdTest");
    m_hSubDevDiag   = 0;

}

//!
//! \brief PbiCmdTest destructor
//!
//! PbiCmdTest destructor does not do much. Functionality
//! mostly lies in Cleanup().
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
PbiCmdTest::~PbiCmdTest()
{
}

//!
//! \brief IsTestSupported()
//! This test is run for gt21x and above
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------
string PbiCmdTest::IsTestSupported()
{
    //disable the test on fmod
    if (Platform::GetSimulationMode() == Platform::Fmodel)
    {
        return "Not Supported on F-Model";
    }

    if (!(IsClassSupported(GT212_SUBDEVICE_PMU)))
    {
        return "Class GT212_SUBDEVICE_PMU not supported on current platform";
    }
    return RUN_RMTEST_TRUE;
}

//!
//! \brief PbiCmdTest Setup
//!
//! \Setup all necessary state before running the test.
//-----------------------------------------------------------------------------
RC PbiCmdTest::Setup()
{
    RC rc;

    CHECK_RC(InitFromJs());
    m_hSubDevDiag = GetBoundGpuSubdevice()->GetSubdeviceDiag();

    return rc;
}

//! \brief Run() : Check PBI commands
//!
//--------------------------------------
RC PbiCmdTest::Run()
{
    RC rc = 0;

    LW208F_CTRL_BIF_PBI_WRITE_COMMAND_PARAMS writeBifPbiParam = {0};
    LW208F_CTRL_BIF_CONFIG_REG_READ_PARAMS writeConfigRegParam = {0};
    LW208F_CTRL_BIF_CONFIG_REG_WRITE_PARAMS readConfigRegParam = {0};
    LwRmPtr  pLwRm;

    memset(&writeBifPbiParam, 0, sizeof(writeBifPbiParam));

   // write to a reg (just an example to write an read back to any of the registers)
    writeConfigRegParam.RegIndex = PBI_REGISTER_DATA_IN;
    writeConfigRegParam.data = 0x1234;

    rc = LwRmControl(pLwRm->GetClientHandle(), m_hSubDevDiag,
                        LW208F_CTRL_CMD_BIF_CONFIG_REG_WRITE,
                        (void*)&writeConfigRegParam,
                        sizeof (LW208F_CTRL_BIF_CONFIG_REG_WRITE_PARAMS));

    if(rc != OK)
        Printf(Tee::PriHigh,"\nCould not write data in reg\n");

    // read back the register
    readConfigRegParam.RegIndex = PBI_REGISTER_DATA_IN;

    rc = LwRmControl(pLwRm->GetClientHandle(), m_hSubDevDiag,
                        LW208F_CTRL_CMD_BIF_CONFIG_REG_READ,
                        (void*)&readConfigRegParam,
                        sizeof (LW208F_CTRL_BIF_CONFIG_REG_READ_PARAMS));
    if(rc != OK)
        Printf(Tee::PriHigh,"\nCould not read Datain reg\n");
    else
    {
        Printf(Tee::PriHigh," data read back : %d",readConfigRegParam.data);
        if(readConfigRegParam.data != 0x1234)
            Printf(Tee::PriHigh," Error reading register");
    }

    // Send command
    writeBifPbiParam.cmdFuncId = LW_PBI_COMMAND_FUNC_ID_GET_CAPABILITIES;
    writeBifPbiParam.data = 0;
    rc = LwRmControl(pLwRm->GetClientHandle(), m_hSubDevDiag,
                        LW208F_CTRL_CMD_BIF_PBI_WRITE_COMMAND,
                        (void*)&writeBifPbiParam,
                        sizeof (LW208F_CTRL_BIF_PBI_WRITE_COMMAND_PARAMS));

    if(rc != OK)
        Printf(Tee::PriHigh,"\nCommand not exelwted properly\n");

    // Check status
    if(writeBifPbiParam.status == LW_PBI_COMMAND_STATUS_SUCCESS)
    {
        Printf(Tee::PriHigh,"\nCommand exelwted properly, status success\n");

        // Read the data out register, to get relevent
        readConfigRegParam.RegIndex = PBI_REGISTER_DATA_OUT;

        rc = LwRmControl(pLwRm->GetClientHandle(), m_hSubDevDiag,
                        LW208F_CTRL_CMD_BIF_CONFIG_REG_READ,
                        (void*)&readConfigRegParam,
                        sizeof (LW208F_CTRL_BIF_CONFIG_REG_READ_PARAMS));
        if(rc != OK)
            Printf(Tee::PriHigh,"\nCould not read Dataout reg\n");
        else
            Printf(Tee::PriHigh," data out : %d\n",readConfigRegParam.data);
    }
    else
        Printf(Tee::PriHigh,"\n status after command exelwtion = %d \n ", writeBifPbiParam.status);
    return OK;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
RC PbiCmdTest::Cleanup()
{
    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(PbiCmdTest, RmTest, "test pbi commands");
