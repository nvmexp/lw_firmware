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

//!
//! \file rmt_syscontest.cpp
//! \brief RmTest that exercises the Syscon (Canoas) RmControl Interface
//!

#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "gpu/tests/gputestc.h"
#include "lwos.h"
#include "lwRmApi.h"

#include "core/include/memcheck.h"

class SysconTest : public RmTest
{
public:
    SysconTest();
    virtual ~SysconTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
};

//! \brief SysconTest constructor
//! \sa Setup
//------------------------------------------------------------------------------
SysconTest::SysconTest()
{
    SetName("SysconTest");
}

//! \brief SysconTest destructor
//! \sa Cleanup
//------------------------------------------------------------------------------
SysconTest::~SysconTest()
{
}

//! \brief IsTestSupported - Determines if SysconTest is supported
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string SysconTest::IsTestSupported()
{
    // The syscon/canoas object is always available in the RM
    return RUN_RMTEST_TRUE;
}

//! \brief Setup - Sets up the StsconTest state
//! \return Always returns OK
//------------------------------------------------------------------------------
RC SysconTest::Setup()
{
    return OK;
}

//! \brief Cleanup - Cleans up any SysconTest leftovers
//------------------------------------------------------------------------------
RC SysconTest::Cleanup()
{
    return OK;
}

//! \brief Run - Exelwtes SysconTest core tests
//! \return Returns OK if all test pass, otherwise ERROR
//------------------------------------------------------------------------------
RC SysconTest::Run()
{
    //! XXX rmtest_tegra_sanity tests in as2 require the "SysconTest" test.
    Printf(Tee::PriDebug,
        "SysconTest: Failed to locate any Syscon objects\n");

    return OK;
}

//! \brief Linkage to JavaScript
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(SysconTest, RmTest,
    "Noop test, previously was a comprehensive general test infrastructure for canoas.");
