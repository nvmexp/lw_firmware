/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2011, 2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_cl826f.cpp
//! \brief G82_CHANNEL_GPFIFO Test
//!

#include "lwos.h"
#include "core/include/jscript.h"
#include "core/include/channel.h"
#include "gpu/include/gpudev.h"
#include "core/include/platform.h"
#include "class/cl007d.h" // LW04_SOFTWARE_TEST
#include "class/cl826f.h" // G82_CHANNEL_GPFIFO

#include "gpu/tests/rmtest.h"
#include "core/include/memcheck.h"

class Class826fTest: public RmTest
{
public:
    Class826fTest();
    virtual ~Class826fTest();

    virtual string     IsTestSupported();

    virtual RC Setup() { RC rc; return rc; }
    virtual RC Run() { RC rc; return rc; }
    virtual RC Cleanup() { RC rc; return rc; }
};

//! \brief Class826fTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
Class826fTest::Class826fTest()
{
    SetName("Class826fTest");
}

//! \brief Class826fTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
Class826fTest::~Class826fTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \return True if G82_CHANNEL_GPFIFO is supported on the current chip
//-----------------------------------------------------------------------------
string Class826fTest::IsTestSupported()
{
    return "G82_CHANNEL_GPFIFO class is not supported on current platform";
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ Class826fTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(Class826fTest, RmTest,
                 "Class826fTest RM test.");
