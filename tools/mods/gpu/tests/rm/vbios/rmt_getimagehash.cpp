/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_getimagehash.cpp
//! \brief Test the control call that extracts image hash in vbios

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "ctrl/ctrl2080.h"
#include "gpu/include/gpudev.h"
#include <string>
#include "core/include/memcheck.h"    //must be included last for mem leak tracing at DEBUG_TRACE_LEVEL 1

class GetImageHashTest : public RmTest
{
public:
    GetImageHashTest();
    virtual ~GetImageHashTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
};

//! \brief Constructor for GetImageHashTest
//!
//! \sa Setup
//------------------------------------------------------------------------------
GetImageHashTest::GetImageHashTest()
{
    SetName("GetImageHashTest");
}

//! \brief Destructor for GetImageHashTest
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
GetImageHashTest::~GetImageHashTest()
{
}

//! \brief support all the chips
//!
//------------------------------------------------------------------------------
string GetImageHashTest::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \brief
//
//------------------------------------------------------------------------------
RC GetImageHashTest::Setup()
{
    return LW_OK;
}

//! \brief Use the control call to extract the hash and print it out.
//!
//! \return LW_OK
//------------------------------------------------------------------------------
RC GetImageHashTest::Run()
{
    RC rc;
    LwRmPtr                                      pLwRm;
    LW2080_CTRL_BIOS_GET_IMAGE_HASH_PARAMS       imageHashParam = {0};
    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                            LW2080_CTRL_CMD_BIOS_GET_IMAGE_HASH,
                            &imageHashParam,
                            sizeof(imageHashParam) ) );
    Printf(Tee::PriNormal,"Image Hash is ");
    for (unsigned int i = 0; i < LW2080_CTRL_BIOS_IMAGE_HASH_SIZE; i++)
    {
        Printf(Tee::PriNormal,"%x", imageHashParam.imageHash[i]);
    }
    Printf(Tee::PriNormal,"\n");
    return LW_OK;
}

//! \brief
//------------------------------------------------------------------------------
RC GetImageHashTest::Cleanup()
{
    return LW_OK;
}
//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(GetImageHashTest, RmTest,
    "Test to extract vbios image hash");
