/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_syncpt.cpp
//! \brief Syncpoint tests
//!

#include "lwos.h"
#include "core/include/jscript.h"
#include "core/include/channel.h"
#include "gpu/include/gpudev.h"
#include "core/include/platform.h"
#include "class/cla06f.h" // KEPLER_CHANNEL_GPFIFO_A
#include "class/cl007d.h" // LW04_SOFTWARE_TEST
#include "class/cle26d.h" // LWE2_SYNCPOINT
#include "class/cl003e.h" // LW01_MEMORY_SYSTEM
#include "class/cla26f.h" // LWA26F_SEMAPHORE
#include "class/cla297.h" // LWA297.*
#include "gpu/utility/surf2d.h"
#include "gpu/tests/rmtest.h"
#include "core/include/tasker.h"
#include "gpu/include/gralloc.h"
#include "gpu/utility/syncpoint.h"
#include "core/include/memcheck.h"

#define SW_OBJ_SUBCH 5

class SyncptTest: public RmTest
{
public:
    SyncptTest();
    virtual ~SyncptTest();

    virtual string     IsTestSupported();

    virtual RC       Setup();
    virtual RC       Run();
    virtual RC       Cleanup();

private:

};

//! \brief SyncptTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
SyncptTest::SyncptTest()
{
    SetName("SyncptTest");
}

//! \brief SyncptTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
SyncptTest::~SyncptTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \return True if KEPLER_CHANNEL_GPFIFO_A/B or LWE2_SYNCPOINT are supported.
//-----------------------------------------------------------------------------
string SyncptTest::IsTestSupported()
{
    if(IsClassSupported(LWE2_SYNCPOINT))
        return RUN_RMTEST_TRUE;
    return "None of the required channel classes are supported on current platform";
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa IsTestSupported
//-----------------------------------------------------------------------------
RC SyncptTest::Setup()
{
    RC           rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    return rc;
}

//! \brief Run() : The body of this test.
//!
//! In this function, it calls several  sub-tests.
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC SyncptTest::Run()
{
    return OK;
}


//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC SyncptTest::Cleanup()
{
    return OK;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ SyncptTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(SyncptTest, RmTest,
                 "SyncptTest RM test.");
