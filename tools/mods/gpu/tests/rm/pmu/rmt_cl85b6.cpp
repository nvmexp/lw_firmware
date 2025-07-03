/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_cl85b6.cpp
//! \brief To verify basic functionality of new PMU hw and its resman code
//!
//! Includes the basic bootstrap and reset tests, command & msg queue tests,
//! Event register-unregister & negative testing of CMD submit API.

#include "gpu/tests/rmtest.h"
#include "gpu/tests/gputestc.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/channel.h"
#include "lwRmApi.h"
#include "lwos.h"
#include "core/include/utility.h"
#include "core/utility/errloggr.h"
#include <stdio.h>
#include "ctrl/ctrl0000.h"
#include "ctrl/ctrl0080.h"
#include "class/cl8597.h"  // GT214_TESLA
#include "class/cl8697.h"  // GT21A_TESLA
#include "class/cl9097.h"  // FERMI_A
#include "class/cl9297.h"  // FERMI_C
#include "class/cl91c0.h"  // FERMI_COMPUTE_B
#include "class/cla097.h"  // KEPLER_A
#include "class/cla197.h"  // KEPLER_B
#include "class/cla297.h"  // KEPLER_C
#include "class/cl85b6.h"  // GT212_SUBDEVICE_PMU
#include "ctrl/ctrl85b6.h" // GT212_SUBDEVICE_PMU CTRL
#include "gpu/perf/pmusub.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "rmpmucmdif.h"
#include "pmu/pmuseqinst.h"
#include "core/include/platform.h"
#include "core/include/xp.h"
#include "core/include/memcheck.h"

class Class85b6Test : public RmTest
{
public:
    Class85b6Test();
    virtual ~Class85b6Test();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    PMU                    *m_pPmu;
    UINT32                  m_pmuUcodeStateSaved;
    FLOAT64                 m_TimeoutMs;
    GpuSubdevice           *m_Parent;                     // GpuSubdevice that owns this PMU
    RC Class85b6DummyTest();

    // Size for PMU scripts (byte arrays) containing just one EXIT command.
    // 8 bytes ought to be enough, but in practice we get memory corruption,
    // leading to crashes later in other tests, see bug 1229766.
    enum { InPayloadSize = 3*sizeof(UINT32) };
};

//! \brief Class85b6Test constructor
//!
//! Placeholder : doesn't do much, much funcationality in Setup()
//!
//! \sa Setup
//-----------------------------------------------------------------------------
Class85b6Test::Class85b6Test()
{
    SetName("Class85b6Test");
    m_pPmu               = NULL;
    m_pmuUcodeStateSaved = 0;
    m_TimeoutMs          = Tasker::NO_TIMEOUT;
    m_Parent             = nullptr;
}

//! \brief Class85b6Test  destructor
//!
//! Placeholder : doesn't do much, much funcationality in Cleanup()
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
Class85b6Test::~Class85b6Test()
{

}

//! \brief IsTestSupported(), Looks for whether test can execute in current elw.
//!
//! The test is basic PMU class: Check for Class availibility.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//-----------------------------------------------------------------------------
string Class85b6Test::IsTestSupported()
{
    bool       bIsSupported;

    // Returns true only if the class is supported.
    bIsSupported = IsClassSupported(GT212_SUBDEVICE_PMU);

    Printf(Tee::PriHigh,"%d:Class85b6Test::IsSupported retured %d \n",
          __LINE__, bIsSupported);

    if(bIsSupported)
        return RUN_RMTEST_TRUE;
    return "GT212_SUBDEVICE_PMU class is not supported on current platform";
}

//! \brief Setup(): Generally used for any test level allocation
//!
//! Checking if the bootstrap rom image file is present, also obtaining
//! the instance of PMU class, through which all acces to PMU will be done.
//
//! \return RC::SOFTWARE_ERROR if the PMU bootstrap file is not found,
//! other corresponding RC for other failures
//! \sa Run()
//-----------------------------------------------------------------------------
RC Class85b6Test::Setup()
{
    RC rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    m_TimeoutMs = GetTestConfiguration()->TimeoutMs();
    m_Parent = GetBoundGpuSubdevice();
    // Get PMU class instance
    rc = (GetBoundGpuSubdevice()->GetPmu(&m_pPmu));

    if (OK != rc)
    {
        Printf(Tee::PriHigh,"PMU not supported\n");
        return rc;
    }

    //
    // get and save the current PMU ucode state.
    // We should restore the PMU ucode to this state
    // after our tests are done.
    //
    rc = m_pPmu->GetUcodeState(&m_pmuUcodeStateSaved);
    if (OK != rc)
    {
        Printf(Tee::PriHigh,
               "%d: Failed to get PMU ucode state\n",
                __LINE__);
        return rc;
    }

    return rc;
}

//! \brief Run(): Used generally for placing all the testing stuff.
//!
//! Run() as said in Setup() has bootstrap and reset tests lwrrently, will
//! be adding more tests for command and msg queue testing.
//!
//! \return OK if the passed, specific RC if failed
//! \sa Setup()
//-----------------------------------------------------------------------------
RC Class85b6Test::Run()
{
    RC      rc;
    UINT32  lwrPmuUcodeState;
    //
    // We're going to be generating error interrupts.
    // Don't want to choke on those.
    //
    CHECK_RC(ErrorLogger::StartingTest());
    ErrorLogger::IgnoreErrorsForThisTest();

    // Get current ucode state and run CmdMsgQTest and NegativeTest only if pmu is running
    CHECK_RC(m_pPmu->GetUcodeState(&lwrPmuUcodeState));
    if (lwrPmuUcodeState == LW85B6_CTRL_PMU_UCODE_STATE_READY)
    {
        CHECK_RC(Class85b6DummyTest());
    }
    else
    {
        Printf(Tee::PriHigh,"%d:Class85b6Test:: Skipping PMU Class85b6DummyTest as PMU is not bootstrapped.\n", __LINE__);
    }

    return rc;
}

//! \brief Cleanup()
//!
//! As everything done in Run (lwrrently) this cleanup acts again like a
//! placeholder except few buffer free up we might need allocated in Setup
//! \sa Run()
//-----------------------------------------------------------------------------
RC Class85b6Test::Cleanup()
{
    ErrorLogger::TestCompleted();
    return OK;
}

//-----------------------------------------------------------------------------
// Private Member Functions
//-----------------------------------------------------------------------------

//! \brief Class85b6DummyTest()
//!
//! This function runs the following tests as per the specified order
//!   a. Sends a memory seq script run command to PMU.
//!   b. Waits for the PMU message in response to the command
//!   c. Prints the received message.
//!
//! \return OK if all the tests passed, specific RC if failed
//! \sa Run()
//-----------------------------------------------------------------------------
RC Class85b6Test::Class85b6DummyTest()
{
    RC rc;

    //
    // This test was added when PMU was introduced and it was developed around
    // first RM->PMU command, a sequencer RUN command exelwting a MCLK switch.
    // Since than PMU grew dozen tasks and 10-s of commands while this test
    // remained in its early from. PMU commands moved to RPC infrastructure and
    // SEQ command remained among last ones since this MODS use-case prevented
    // it's refactoring. Today RM cannot operate without the PMU and before 
    // this test has a chance to run RM and PMU exchange multiple commands and
    // messages. Therefore there is no point in running the test in its current
    // form. I am removing it's implementation while preserving this interface
    // as a placeholder for future tests.
    //
    rc = OK;

    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ Class85b6Test
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(Class85b6Test, RmTest,
"Test new class addition, class named 85b6, class used specifically for PMU");

