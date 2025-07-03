/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2008 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_ctrl0080vp.cpp
//! \brief LW0080_CTRL_VP Tests
//!

#include "ctrl/ctrl0080.h"
#include "class/cl95b2.h" // LW95B2_VIDEO_MSPDEC
#include "gpu/tests/rmtest.h"
#include "core/include/memcheck.h"

class Ctrl0080VpTest: public RmTest
{
public:
    Ctrl0080VpTest();
    virtual ~Ctrl0080VpTest();

    virtual string     IsTestSupported();

    virtual RC       Setup();
    virtual RC       Run();
    virtual RC       Cleanup();

private:
};

//! \brief Ctrl0080VpTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
Ctrl0080VpTest::Ctrl0080VpTest()
{
    SetName("Ctrl0080VpTest");
}

//! \brief Ctrl0080VpTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
Ctrl0080VpTest::~Ctrl0080VpTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//-----------------------------------------------------------------------------
string Ctrl0080VpTest::IsTestSupported()
{

    if (IsClassSupported(LW95B2_VIDEO_MSPDEC))
        return RUN_RMTEST_TRUE;
    else
        return "The class LW95B2_VIDEO_MSPDEC is not supported on current platform";
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa Setup
RC Ctrl0080VpTest::Setup()
{
    RC rc;

    return rc;
}

//! \brief Run the test!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC Ctrl0080VpTest::Run()
{
    return OK;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC Ctrl0080VpTest::Cleanup()
{
    return OK;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ Ctrl0080VpTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(Ctrl0080VpTest, RmTest,
                 "Ctrl0080 VP RM test.");
