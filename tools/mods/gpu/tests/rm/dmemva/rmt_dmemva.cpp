/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_dmva.cpp
//! \brief rmtest for DMVA
//!

#include "gpu/tests/rmtest.h"
#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/tee.h"
#include "core/include/rc.h"
#include "core/include/utility.h"

#include "lwos.h"
#include "core/include/channel.h"
#include "core/include/platform.h"

#include "rmt_dmemva.h"
#include "rmt_dmemva_test_cases.h"

//! \brief DmvaTest constructor
//!
//! Doesn't do much except SetName.
//!
//! \sa Setup
//------------------------------------------------------------------------------
DmvaTest::DmvaTest() : m_bDmvaTestSec(false), m_bDmvaTestPmu(false), m_bDmvaTestLwdec(false),  m_bDmvaTestGsp(false)
{
    SetName("DmvaTest");
}

//! \brief DmvaTest destructor
//!
//! Doesn't do much.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
DmvaTest::~DmvaTest()
{
}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string DmvaTest::IsTestSupported()
{
    if (!(GetBoundGpuSubdevice()->DeviceId() >= Gpu::GP100))
        return "Test only supported on GP10x or later chips";
    bool isTestAll = (m_bDmvaTestSec && m_bDmvaTestGsp && m_bDmvaTestPmu && m_bDmvaTestLwdec);
    if (!isTestAll && m_bDmvaTestGsp && !(GetBoundGpuSubdevice()->DeviceId() >= Gpu::GV100))
        return "GSP test only supported on GV10x or later chips";
    return RUN_RMTEST_TRUE;
}

//! \brief Do Init from Js
//!
//! \return OK if no error happens, otherwise corresponding rc
//------------------------------------------------------------------------------
RC DmvaTest::Setup()
{
    return OK;
}

//! \brief Run the test
//!
//! \return OK all tests pass, else corresponding rc
//------------------------------------------------------------------------------
RC DmvaTest::Run()
{
    if (!m_bDmvaTestSec && !m_bDmvaTestGsp && !m_bDmvaTestPmu && !m_bDmvaTestLwdec)
    {
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    TestCtl testCtl = {m_bDmvaTestSec, m_bDmvaTestPmu, m_bDmvaTestLwdec, m_bDmvaTestGsp};

    return runTestSuite(this, testCtl);
}

//! \brief Cleanup, does nothing for this test
//------------------------------------------------------------------------------
RC DmvaTest::Cleanup()
{
    return OK;
}

LwU32 DmvaTest::memrd32(LwU32 *addr)
{
    return MEM_RD32(addr);
}

void DmvaTest::memwr32(LwU32 *addr, LwU32 data)
{
    MEM_WR32(addr, data);
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(DmvaTest, RmTest, "DMVA Test. \n");

CLASS_PROP_READWRITE(DmvaTest, bDmvaTestSec, bool, "DMVA test sec2 falcon, default = false");
CLASS_PROP_READWRITE(DmvaTest, bDmvaTestPmu, bool, "DMVA test pmu falcon, default = false");
CLASS_PROP_READWRITE(DmvaTest, bDmvaTestLwdec, bool, "DMVA test lwdec falcon, default = false");
CLASS_PROP_READWRITE(DmvaTest, bDmvaTestGsp, bool, "DMVA test GSP falcon, default = false");
