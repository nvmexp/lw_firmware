/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2011,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_cl005d.cpp
//! \brief Test ensures we get the event triggers and poll based notification
//!        from inside RM.
//!
//! GPU provides following flavors of notifications  This test verifies both
//! notifiers.
//! 1. Polling based: To wait on the notifcation -> WRITE_ONLY, Simply
//!                   placing the Push buffer and waiting on it.
//! 2. Event based: To get an event from within the GPU -> WRITE_THEN_AWAKEN
//!                 Notifiers can be tied with operating system events
//!                 (and can be used for synchronization.)
//!                 for example: Set an event if the write operation in the
//!                 hardware is successful and write operation is notified.
//!
//! This test only will be execute on WINS systems both SIM and HW and will
//! fail on Linux and MAC as there is no TASKER level API support for that and
//! we want them to fail at that time.

#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "gpu/tests/gputestc.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "lwos.h"         // LW0S0*
#include "gpu/include/notifier.h"
#include "core/include/gpu.h"

#include "lwRmApi.h"

#include "class/cl506f.h" // LW50_CHANNEL_GPFIFO
#include "class/cl826f.h" // G82_CHANNEL_GPFIFO
#include "ctrl/ctrl0073.h"
#include "ctrl/ctrl0000.h"
#include "ctrl/ctrl0080.h"
#include "class/cl5097.h" // LW50_TESLA
#include "class/cl5097sw.h"
#include "class/cl9097.h" // FERMI_A
#include "class/cla097.h" // KEPLER_A
#include "class/cla197.h" // KEPLER_B
#include "class/cla297.h" // KEPLER_C
#include "class/clb097.h" // MAXWELL_A
#include "class/cl9097sw.h"
#include "class/cla097sw.h"
#include "class/cla197sw.h"
#include "class/cla297sw.h"
#include "class/clb097sw.h"
#include "class/cla06fsubch.h"
#include "ctrl/ctrl0080/ctrl0080gpu.h"
#include "gpu/include/gralloc.h"
#include "core/include/memcheck.h"

#define MAXCOUNT 1 //Maximum count for notifiers for >G80

class Class005dTest: public RmTest
{
public:
    Class005dTest();
    virtual ~Class005dTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    RC SetupPollBased();
    RC SetupEventBased();

    RC RunNotifyPollBased();
    RC RunNotifyEventBased();

    Notifier     m_Notifier;
    FLOAT64      m_TimeoutMs = Tasker::NO_TIMEOUT;
    ThreeDAlloc  m_3dAlloc;
};

//! \brief Class005dTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
Class005dTest::Class005dTest()
{
    SetName("Class005dTest");
    m_3dAlloc.SetOldestSupportedClass(LW50_TESLA);
}

//! \brief Class005dTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
Class005dTest::~Class005dTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! If supported then the test will be ilwoked.
//!
//! \return True if the required classes are supported in the current
//!         environment, false otherwise.
//-----------------------------------------------------------------------------
string Class005dTest::IsTestSupported()
{
    if( m_3dAlloc.IsSupported(GetBoundGpuDevice()) )
        return RUN_RMTEST_TRUE;
    return "None of these  LW50_TESLA, FERMI_A+, classes is supported on current platform";
}

//! \brief Setup all necessary state before running the test.
//!
//! Idea is to reserve all the resources required by this test so if
//! required multiple tests can be ran in parallel.
//! Allocates event, uses tasker to allocate event and registers
//! this tasker event with RM. (to get back the event), Calls Poll
//! and event based setup sub-functions.
//!
//! \return RC -> OK if everything is allocated, Specific RC if something
//!         failed while selwring the resources.
//! \sa IsSupported
//! \sa SetupPollBased
//! \sa SetupEventBased
//-----------------------------------------------------------------------------
RC Class005dTest::Setup()
{
    RC rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());
    m_TimeoutMs = GetTestConfiguration()->TimeoutMs();

    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_GR(0)));

    CHECK_RC(m_3dAlloc.Alloc(m_hCh, GetBoundGpuDevice()));

    // Call Sub-functions
    CHECK_RC(SetupPollBased());
    CHECK_RC(SetupEventBased());

    return rc;
}

//! \brief Run the test!
//!
//! After the setup is successful the allocated resources can be used.
//! Run function u calls sub-functions to do Poll based (WRITE_ONLY)
//! and Event based testing (WRITE_THEN_AWAKEN).
//!
//! \return OK if the test passed i.e. if the event is set otherwise
//!         failed with specific RC.
//! \sa Setup
//-----------------------------------------------------------------------------
RC Class005dTest::Run()
{
    RC rc;

    CHECK_RC(RunNotifyPollBased());
    CHECK_RC(RunNotifyEventBased());

    return rc;
}

//! \brief Free any resources that this test selwred during Setup.
//!
//! Cleanup all the allocated resources that the test selwred during Setup.
//! Cleanup can be called immediately if Setup fails or Run fails,
//!
//! \sa Setup
//-----------------------------------------------------------------------------
RC Class005dTest::Cleanup()
{
    RC rc, firstRc;

    m_Notifier.Free();
    m_3dAlloc.Free();
    //
    // Free the channel. If the m_pCh pointer is NULL,
    // the FreeChannel call will not do anything.
    //
    FIRST_RC(m_TestConfig.FreeChannel(m_pCh));
    m_pCh = NULL;

    return firstRc;
}

//-----------------------------------------------------------------------------
// Private Member Functions
//-----------------------------------------------------------------------------

//! \brief SetupPollBased
//!
//! Setup function calls this function. Allocates resources for Poll based
//! testing.
//!
//! \return OK if the test passed, specific RC if it failed
//! \sa Setup
//-----------------------------------------------------------------------------
RC Class005dTest::SetupPollBased()
{
    RC rc;

    //Allocate a Notifier
    CHECK_RC(m_Notifier.Allocate(m_pCh, MAXCOUNT, &m_TestConfig));

    return rc;
}

//! \brief SetupEventBased
//!
//! Setup function calls this function. Allocates resources for Event based
//! testing.
//!
//! \return OK if the test passed, specific RC if it failed
//! \sa Setup
//-----------------------------------------------------------------------------
RC Class005dTest::SetupEventBased()
{
    RC rc;

    //Allocate a Notifier
    CHECK_RC(m_Notifier.Allocate(m_pCh, MAXCOUNT, &m_TestConfig));

    //Allocate an Event
    CHECK_RC(m_Notifier.AllocEvent(m_3dAlloc.GetHandle() ));

    return rc;
}

//! \brief RunNotifyPollBased
//!
//! Exelwtes the Poll based test.
//!
//! \return OK if the test passed, specific RC if it failed
//! \sa Run
//-----------------------------------------------------------------------------
RC Class005dTest::RunNotifyPollBased()
{
    RC rc;
    //
    // Notify Cmds: for _WRITE_ONLY option, wait & poll on the status
    // of the notifier
    //
    CHECK_RC(m_pCh->SetObject(0, m_3dAlloc.GetHandle()));
    CHECK_RC(m_Notifier.Instantiate(0));
    m_Notifier.Clear(LW5097_NOTIFIERS_NOTIFY);
    CHECK_RC(m_Notifier.Write(0));
    CHECK_RC(m_pCh->Write(0, LW5097_NO_OPERATION, 0));
    CHECK_RC(m_pCh->Flush());

    return rc;
}

//! \brief RunNotifyEventBased
//!
//! Exelwtes the Event based test.
//!
//! \return OK if the test passed, specific RC if it failed
//! \sa Run
//-----------------------------------------------------------------------------
RC Class005dTest::RunNotifyEventBased()
{
    RC rc;
    //
    // Notify Cmds: for _WRITE_THEN_AWAKEN option,hooked to OS
    // dependent event
    //
    CHECK_RC(m_pCh->SetObject(0, m_3dAlloc.GetHandle() ));
    CHECK_RC(m_Notifier.Write(0, LW5097_NOTIFY_TYPE_WRITE_THEN_AWAKEN));
    CHECK_RC(m_pCh->Write(0, LW5097_NO_OPERATION, 0));

    m_pCh->Flush();
    // Wait for the notification
    CHECK_RC(m_Notifier.WaitAwaken(m_TimeoutMs));

    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ Class005dTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(Class005dTest, RmTest,
                 "Event RM test.");
