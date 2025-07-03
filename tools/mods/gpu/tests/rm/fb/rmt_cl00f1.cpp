/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_cl00f1.cpp
//! \brief Empty test. Will be deleted once all level file references have been cleaned up.
//!

#include "gpu/tests/rmtest.h"

class Class00f1Test: public RmTest
{
public:
    Class00f1Test();
    virtual ~Class00f1Test();

    virtual string     IsTestSupported();

    virtual RC       Setup();
    virtual RC       Run();
    virtual RC       Cleanup();

private:
};

//! \brief Class00f1Test constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
Class00f1Test::Class00f1Test()
{
    SetName("Class00f1Test");
}

//! \brief Class00f1Test destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
Class00f1Test::~Class00f1Test()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \sa IsTestSupported
//-----------------------------------------------------------------------------
string Class00f1Test::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
RC Class00f1Test::Setup()
{
    RC rc;

    return rc;
}

//! \brief Run the test!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC Class00f1Test::Run()
{
    RC           rc;

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC Class00f1Test::Cleanup()
{
    RC rc;

    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ Class00f1Test
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(Class00f1Test, RmTest,
                 "Class001f RM test.");
