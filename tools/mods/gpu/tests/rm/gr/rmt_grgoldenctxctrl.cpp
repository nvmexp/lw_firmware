/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2010 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_grgoldenctxctrl.cpp
//!
#include "gpu/tests/rmtest.h"
#include "core/include/platform.h"
#include "core/include/memcheck.h"

class GrGoldenCtxCtrlTest: public RmTest
{
public:
    GrGoldenCtxCtrlTest();
    virtual ~GrGoldenCtxCtrlTest();

    virtual string     IsTestSupported();

    virtual RC       Setup();
    virtual RC       Run();
    virtual RC       Cleanup();
};

//! \brief GrGoldenCtxCtrlTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
GrGoldenCtxCtrlTest::GrGoldenCtxCtrlTest()
{
    SetName("GrGoldenCtxCtrlTest");
}

//! \brief GrGoldenCtxCtrlTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
GrGoldenCtxCtrlTest::~GrGoldenCtxCtrlTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \return True
//-----------------------------------------------------------------------------
string GrGoldenCtxCtrlTest::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa IsTestSupported
//-----------------------------------------------------------------------------
RC GrGoldenCtxCtrlTest::Setup()
{
    RC rc;

    return rc;
}

//! \brief Run the test!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC GrGoldenCtxCtrlTest::Run()
{
    RC rc;

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC GrGoldenCtxCtrlTest::Cleanup()
{
    return OK;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ GrGoldenCtxCtrlTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(GrGoldenCtxCtrlTest, RmTest,
                 "GrGoldenCtxCtrlTest RM test.");
