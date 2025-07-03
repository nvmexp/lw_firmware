/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_cb33.cpp
//! \brief Test for covering class cb33 and its corresponding control calls
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "lwRmApi.h"
#include "lwmisc.h"

#include "core/include/massert.h"
#include "gpu/tests/rm/utility/dtiutils.h"

#include "class/clcb33.h"
#include "ctrl/ctrlcb33.h"


class Cb33Test : public RmTest
{
public:
    Cb33Test();
    virtual ~Cb33Test();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    RC AllocClass();
    RC testReadyState();
    RC GetReadyState(LwBool *bAccept);
    RC SetReadyState(LwBool *bAccept);

    LwRmPtr                 pLwRm;
    LwRm::Handle            classHandle = 0;
};

//! \brief Cb33Test basic constructor
//!
//! This is just the basic constructor for the Cb33Test class.
//!
//! \sa Setup
//------------------------------------------------------------------------------
Cb33Test::Cb33Test()
{
    SetName("Cb33Test");
}

//! \brief Cb33Test destructor
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
Cb33Test::~Cb33Test()
{
}

//! \brief IsSupported
//!
//! This test is supported on all elwironments.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string Cb33Test::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \brief Setup entry point
//!
//! Allocate hClient (LW01_ROOT) object.
//!
//! \return OK if resource allocations succeed, error code indicating
//!         nature of failure otherwise
//-----------------------------------------------------------------------------
RC Cb33Test::Setup()
{
    RC rc;

    CHECK_RC(InitFromJs());

    return rc;
}



//! \brief Run entry point
//!
//! Kick off system control command tests.
//!
//! \return OK if test passes, error code indicating nature of failure
//!         otherwise
//-----------------------------------------------------------------------------
RC Cb33Test::Run()
{
    RC    rc = OK;
    CHECK_RC(AllocClass());
    CHECK_RC(testReadyState());
    return rc;
}

//! \brief Cleanup entry point
//!
//! Release outstanding resource.
//!
//! \return No status returned; always passes.
//-----------------------------------------------------------------------------
RC Cb33Test::Cleanup()
{
    if(classHandle)
    {
        pLwRm->Free(classHandle);
    }
    return OK;
}

RC Cb33Test::AllocClass()
{
    RC rc;

    LW_CONFIDENTIAL_COMPUTE_ALLOC_PARAMS params = { 0 };
    UINT32 status = 0;

    status = pLwRm->Alloc(pLwRm->GetClientHandle(),
        &classHandle,
        LW_CONFIDENTIAL_COMPUTE, &params);

    if (status != LWOS_STATUS_SUCCESS)
    {
        Printf(Tee::PriNormal,
            "Cb33 class object allocation failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }

    return rc;
}

RC Cb33Test::GetReadyState(LwBool *bAccept)
{
    RC rc;
    LW_CONF_COMPUTE_CTRL_CMD_SYSTEM_GET_GPUS_STATE_PARAMS gpuState = { 0 };
    UINT32 status;

    status = pLwRm->Control(classHandle,
        LW_CONF_COMPUTE_CTRL_CMD_SYSTEM_GET_GPUS_STATE,
        &gpuState, sizeof(gpuState));
    if (status != LWOS_STATUS_SUCCESS)
    {
        Printf(Tee::PriNormal,
            "Get Gpu Ready State failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }
    *bAccept = gpuState.bAcceptClientRequest;
    return OK;
}

RC Cb33Test::SetReadyState(LwBool *bAccept)
{
    RC rc;
    LW_CONF_COMPUTE_CTRL_CMD_SYSTEM_SET_GPUS_STATE_PARAMS gpuState = { 0 };
    UINT32 status;

    gpuState.bAcceptClientRequest = *bAccept;

    status = pLwRm->Control(classHandle,
        LW_CONF_COMPUTE_CTRL_CMD_SYSTEM_SET_GPUS_STATE,
        &gpuState, sizeof(gpuState));
    if (status != LWOS_STATUS_SUCCESS)
    {
        Printf(Tee::PriNormal,
            "Set Gpu Ready State failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }

    return OK;
}

RC Cb33Test::testReadyState()
{
    RC rc;

    LwBool bAcceptClientRequest;

    CHECK_RC(GetReadyState(&bAcceptClientRequest));

    // bAcceptClientRequest should default to LW_FALSE
    if (bAcceptClientRequest)
    {
        return RC::SOFTWARE_ERROR;
    }

    bAcceptClientRequest = LW_TRUE;
    CHECK_RC(SetReadyState(&bAcceptClientRequest));

    // crosscheck that ready state got updated
    CHECK_RC(GetReadyState(&bAcceptClientRequest));
    if (!bAcceptClientRequest)
    {
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}



//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ Cb33Test
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(Cb33Test, RmTest,
                 "Cb33Test RM Test.");
