/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_ibmnputest.cpp
//! \Performs a test of lwlink architecture by dumping device registers.
//!

#include "core/include/jscript.h"
#include "core/include/memcheck.h"
#include "core/include/mgrmgr.h"
#include "core/include/script.h"
#include "device/interface/lwlink.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/tests/rmtest.h"
#include "ibmnpu.h"
#include "lwlink.h"

#include <string> // Only use <> for built-in C++ stuff
class IbmNpuTest : public RmTest
{
public:
    TestDeviceMgr *m_pTestDeviceMgr;

    IbmNpuTest();
    virtual ~IbmNpuTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:

};

//! Constructor of IbmNpuTest
//------------------------------------------------------------------------------
IbmNpuTest::IbmNpuTest()
 : m_pTestDeviceMgr(nullptr)
{
    SetName("IbmNpuTest");
}

//! Destructor of IbmNpuTest
//------------------------------------------------------------------------------
IbmNpuTest::~IbmNpuTest() {}

//! Lwlink only exists on GP100+
//------------------------------------------------------------------------------
string IbmNpuTest::IsTestSupported()
{
    UINT32          gpuFamily;

    gpuFamily = (GetBoundGpuDevice())->GetFamily();

    if (gpuFamily < GpuDevice::Pascal)
        return "Test only supported on GP100 or later chips";

    if (DevMgrMgr::d_TestDeviceMgr == nullptr)
        return "LwLink manager not initialized";

    m_pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
    if (m_pTestDeviceMgr->NumDevices() == 0)
        return "No devices found to test";

    return RUN_RMTEST_TRUE;
}

//! Checks to see if device manager exists. If it does, the devices have probably been
//! Initialized already. Otherwise, initialize everything.
//------------------------------------------------------------------------------
RC IbmNpuTest::Setup()
{
    RC rc;

    CHECK_RC(InitFromJs());

    // Need to reset it here in case TestIsSupported was skipped
    m_pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);

    return rc;
}

//! Reads and prints all the registers read from all the devices detected/registered
//------------------------------------------------------------------------------
RC IbmNpuTest::Run()
{
    RC rc;

    for (UINT32 ii = 0; ii < m_pTestDeviceMgr->NumDevices(); ii++)
    {
        TestDevicePtr pDevice;
        CHECK_RC(m_pTestDeviceMgr->GetDevice(ii, &pDevice));
        if (pDevice->SupportsInterface<LwLink>())
        {
            CHECK_RC(pDevice->GetInterface<LwLink>()->DumpAllRegs());
        }
    }

    return rc;
}

//! Clean up of the ebridge devices will be done automatically in Gpu::Shutdown
//!
//! Nothing to do here
//------------------------------------------------------------------------------
RC IbmNpuTest::Cleanup()
{
    return OK;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(IbmNpuTest, RmTest,
    "Test dumping of registers from discovered ibmnpu devices");

