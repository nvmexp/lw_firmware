/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011,2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_dispbroken.cpp
//! \brief Test for covering system-related control commands.
//!
//! This test basically aims to test DISP engine registers and verifies that
//! access to them is disabled via FECS. So any write or read to these regs
//! should not succeed and returns FECS_PRI_FLOORSWEEP (0xbadf1300) for read.
//! This test checks availability of LW04_DISPLAY_COMMON. For display broken sku,
//! this class is not listed. So whenever this class is not supported,
//! this test will run on that GPU.
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "lwRmApi.h"
#include "core/include/utility.h"
#include "lwmisc.h"
#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/lwrm.h"
#include "core/include/xp.h"
#include <string> // Only use <> for built-in C++ stuff

#include "class/cl0073.h" // LW04_DISPLAY_COMMON
#include "disp/v02_00/dev_disp.h"
#include "core/include/memcheck.h"

#define FECS_PRI_FLOORSWEEP 0xbadf1300

class DispBrokenTest : public RmTest
{
public:
    DispBrokenTest();
    virtual ~DispBrokenTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
private:
        LwRmPtr m_pLwRm;
};

//! \brief DispBrokenTest basic constructor
//!
//! This is just the basic constructor for the DispBrokenTest class.
//!
//! \sa Setup
//------------------------------------------------------------------------------
DispBrokenTest::DispBrokenTest()
{
    SetName("DispBrokenTest");
}

//! \brief DispBrokenTest destructor
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
DispBrokenTest::~DispBrokenTest()
{

}

//! \brief IsSupported
//!
//! This test is supported on DISPLESS platforms where DISP is broken.
//! It is only for GF119 as of now which is no longer supported.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string DispBrokenTest::IsTestSupported()
{
    if (m_pLwRm->IsClassSupported(LW04_DISPLAY_COMMON, GetBoundGpuDevice()))
    {
        return "Display Class is supported on current platform so test not applicable";
    }
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        return "Test is applicable only on Hardware";
    }
    //TODO: remove this & uncomment 2nd line once we have another GPU with DISPLESS platform.
    return "GF119 is no longer supported on this branch.";
    
    //return RUN_RMTEST_TRUE;
}

//! \brief Setup entry point
//!
//! Allocate hClient (LW01_ROOT) object.
//!
//! \return OK if resource allocations succeed, error code indicating
//!         nature of failure otherwise
//-----------------------------------------------------------------------------
RC DispBrokenTest::Setup()
{
    InitFromJs();
    return OK;
}

//! \brief Run entry point
//!
//! Kick off system control command tests.
//!
//! \return OK if test passes, error code indicating nature of failure
//!         otherwise
//-----------------------------------------------------------------------------
RC DispBrokenTest::Run()
{
    RC    rc;
    LwU32 dispRegAddr[] = { LW_PDISP_DSI_PCLK,
                            LW_PDISP_SUPERVISOR_MAIN,
                            LW_PDISP_DMI_MEMACC,
                            LW_PDISP_COMP_LWDPS_CONTROL(0),
                            LW_PDISP_RG_UNDERFLOW(0),
                            LW_PDISP_RG_RASTER_EXTEND(0),
                            LW_PDISP_SF_HDMI_ACR_CTRL(0)};
    LwU32 dispRegTotal = sizeof(dispRegAddr)/sizeof(LwU32);
    if (!m_pLwRm->GetClientHandle())
    {
        return (RC::DID_NOT_INITIALIZE_RM);
    }
    // Run only on GF119 which has DISP BROKEN as of now.
    // Removing this GF119 check because Fermi family is no longer supported!
    LwU32 i, data;
    // Read-Write randomly few registers of DISP and expect 0xBADF1300 value from them.
    for (i = 0; i < dispRegTotal; i++)
    {
        data = GetBoundGpuSubdevice()->RegRd32(dispRegAddr[i]);
        if (data != FECS_PRI_FLOORSWEEP)
        {
            Printf(Tee::PriError,
            "DispBrokenTest: Register value is unexpected. 0x%x\n \
            Check if GPU is really display floorswept\n", data);
            return RC::DATA_MISMATCH;
        }
        GetBoundGpuSubdevice()->RegWr32(dispRegAddr[i], 0x1);
        data = GetBoundGpuSubdevice()->RegRd32(dispRegAddr[i]);
        if (data != FECS_PRI_FLOORSWEEP)
        {
            Printf(Tee::PriError,
            "DispBrokenTest: Register value got 0x%x updated unexpectedly.\n \
            Check if GPU is really display floorswept\n", data);
            return RC::DATA_MISMATCH;
        }
    }
    return rc;
}

//! \brief Cleanup entry point
//!
//! \return No status returned; always passes.
//-----------------------------------------------------------------------------
RC DispBrokenTest::Cleanup()
{
    return OK;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ DispBrokenTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(DispBrokenTest, RmTest, "Test for broken display engine.");
