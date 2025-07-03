/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "lwRmApi.h"
#include "lwmisc.h"

#include "class/clc365.h"
#include "ctrl/ctrlc365.h"

#include "core/include/memcheck.h"

class CtrlC365Test : public RmTest
{
public:
    CtrlC365Test();
    virtual ~CtrlC365Test();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    LwRmPtr pLwRm;
    LwRm::Handle accessCtrBufferHandle = 0;
    RC AllocControlTest();
    RC RegisterMappingTest();
};

//! \brief CtrlC365Test basic constructor
//!
//! This is just the basic constructor for the CtrlC365Test class.
//!
//! \sa Setup
//------------------------------------------------------------------------------
CtrlC365Test::CtrlC365Test()
{
    SetName("CtrlC365Test");
}

//! \brief CtrlC365Test destructor
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
CtrlC365Test::~CtrlC365Test()
{

}

//! \brief IsSupported
//!
//! This test is supported on all elwironments.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string CtrlC365Test::IsTestSupported()
{
    if (IsClassSupported(ACCESS_COUNTER_NOTIFY_BUFFER))
    {
            return RUN_RMTEST_TRUE;
    }
    else
    {
        return "Access counters are not supported on this chip";
    }

}

//! \brief Setup entry point
//-----------------------------------------------------------------------------
RC CtrlC365Test::Setup()
{
    return OK;
}

//! \brief Run entry point
//!
//! \return OK if test passes, error code indicating nature of failure
//!         otherwise
//-----------------------------------------------------------------------------
RC CtrlC365Test::Run()
{
    RC rc;

    CHECK_RC(AllocControlTest());
    CHECK_RC(RegisterMappingTest());

    return rc;
}

//! \brief Cleanup entry point
//!
//! \return No status returned; always passes.
//-----------------------------------------------------------------------------
RC CtrlC365Test::Cleanup()
{
    pLwRm->Free(accessCtrBufferHandle);
    return OK;
}

//-----------------------------------------------------------------------------
// Private Member Functions
//-----------------------------------------------------------------------------

RC CtrlC365Test::AllocControlTest()
{
    LwU32 accessCntrBufferAllocParams = 0;
    UINT32 status = 0;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());

    status = pLwRm->Alloc(hSubdevice,
                         &accessCtrBufferHandle,
                         ACCESS_COUNTER_NOTIFY_BUFFER, &accessCntrBufferAllocParams);

    if (status != LWOS_STATUS_SUCCESS)
    {
        Printf(Tee::PriNormal,
               "Access counter object allocation failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }

    return OK;
}

RC CtrlC365Test::RegisterMappingTest()
{
    UINT32 status = 0;
    LWC365_CTRL_ACCESS_CNTR_BUFFER_GET_REGISTER_MAPPINGS_PARAMS registerMappings = {0};

    status = pLwRm->Control(accessCtrBufferHandle, LWC365_CTRL_CMD_ACCESS_CNTR_BUFFER_GET_REGISTER_MAPPINGS,
                            &registerMappings, sizeof(registerMappings));

    if (status != LWOS_STATUS_SUCCESS)
    {
        Printf(Tee::PriNormal,
               "Access counter register mappings control call failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }
    else
    {
        Printf(Tee::PriNormal, "Register mappings :\n Get : 0x%p\n Put: 0x%p\n",
               (void*)registerMappings.pAccessCntrBufferGet, (void*)registerMappings.pAccessCntrBufferPut);
    }

    return OK;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ CtrlC365Test
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(CtrlC365Test, RmTest,
                 "CtrlC365Test RM Test.");
