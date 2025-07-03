/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2008 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! DIRECTIONS
//! ----------------------------------------------------------------------------
//! 1.  Save this file to a new name.  rmt_<sensible name for your test>.cpp
//!
//! 2.  Open that file for add in p4
//!
//! 3.  Replace SampleNewRmTest with the name of your test's class
//!
//! 4.  Add your new file to the MODS Makefile
//!     (//sw/dev/gpu_drv/chips_a/diag/mods/gpu/tests/rm/makesrc.inc)
//!     Look for other rmt_ files and add your test in alphabetical order.
//!
//! 5.  Start addressing TODORMT items below.
//!
//! 6.  Remove this comment block (DIRECTIONS) from your file!

//!
//! \file TODORMT - Name of this file
//! \brief TODORMT - Write a file brief comment
//!
//! TODORMT - Write a more detailed file comment

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include <string> // Only use <> for built-in C++ stuff
#include "core/include/memcheck.h"

class SampleNewRmTest : public RmTest
{
public:
    SampleNewRmTest();
    virtual ~SampleNewRmTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:

};

//! \brief TODORMT - Add a brief description for your constructor
//!
//! TODORMT - Add a more detailed description for your constructor
//!
//! \sa Setup
//------------------------------------------------------------------------------
SampleNewRmTest::SampleNewRmTest()
{
    SetName("SampleNewRmTest");
}

//! \brief TODORMT - Add a brief description for your destructor
//!
//! TODORMT - Add a detailed description for your destructor
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
SampleNewRmTest::~SampleNewRmTest()
{

}

//! \brief TODORMT - Add a brief description for IsTestSupported
//!
//! TODORMT - Add a detailed description for IsTestSupported
//!
//! \return RUN_RMTEST_TRUE if the test can be run in the current environment,
//!         skip reason otherwise
//------------------------------------------------------------------------------
string SampleNewRmTest::IsTestSupported()
{
    MASSERT(!"TODORMT - Implement IsTestSupported");
    return "It's a Sample RMTest";
}

//! \brief TODORMT - Add a brief description for Setup
//!
//! TODORMT - Add a detailed description for Setup
//
//! \return TODORMT - Add a descripption for ways setup can pass/fail
//------------------------------------------------------------------------------
RC SampleNewRmTest::Setup()
{
    RC rc;

    // Ask our base class(es) to setup internal state based on JS settings
    // TODORMT - Determine whether you want to InitFromJs in your test.  You
    //           probably do!
    CHECK_RC(InitFromJs());

    MASSERT(!"TODORMT - Implement Setup");

    // Get rid of these lines once you've implemented Setup
    // RC::SOFTWARE_ERROR is used to indicated an assertion failure
    rc = RC::SOFTWARE_ERROR;

    return rc;
}

//! \brief TODORMT - Add a brief description for Run
//!
//! TODORMT - Add a detailed description for Run
//!
//! \return TODORMT - Add a descrption of Run's return values
//------------------------------------------------------------------------------
RC SampleNewRmTest::Run()
{
    RC rc;

    MASSERT(!"TODORMT - Implement Setup");

    // Get rid of these lines once you've implemented Run
    // RC::SOFTWARE_ERROR is used to indicated an assertion failure
    rc = RC::SOFTWARE_ERROR;

    return rc;
}

//! \brief TODORMT - Add a brief description for Cleanup
//!
//! TODORMT - Add a detailed description for Cleanup
//------------------------------------------------------------------------------
RC SampleNewRmTest::Cleanup()
{
    MASSERT(!"TODORMT - Implement a Cleanup function");
    return OK;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//! TODORMT - Private member functions here

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(SampleNewRmTest, RmTest,
    "TODORMT - Write a good short description here");

