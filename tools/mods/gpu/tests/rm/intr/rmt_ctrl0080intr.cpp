/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2014,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_ctrl0080intr.cpp
//! \brief LW0080_CTRL_INTR Tests
//!

#include "core/include/channel.h"
#include "class/cl9097.h"  //FERMI_A
#include "class/cl906f.h"  //GF100_CHANNEL_GPFIFO
#include "class/cla06f.h"  // KEPLER_CHANNEL_GPFIFO_A
#include "class/cla16f.h"  // KEPLER_CHANNEL_GPFIFO_B
#include "class/cla26f.h"  // KEPLER_CHANNEL_GPFIFO_C
#include "class/clb06f.h"  // MAXWELL_CHANNEL_GPFIFO_A
#include "class/clc06f.h"  // PASCAL_CHANNEL_GPFIFO_A
#include "class/clc36f.h"  // VOLTA_CHANNEL_GPFIFO_A
#include "class/clc46f.h"  // TURING_CHANNEL_GPFIFO_A
#include "class/clc56f.h"  // AMPERE_CHANNEL_GPFIFO_A
#include "class/clc86f.h"  // HOPPER_CHANNEL_GPFIFO_A
#include "class/cla06fsubch.h"
#include "ctrl/ctrl0080.h" // LW01_DEVICE_XX/LW03_DEVICE
#include "core/include/platform.h"
#include "gpu/tests/rmtest.h"
#include "gpu/include/notifier.h"
#include "class/cl9097sw.h"
#include "gpu/include/gralloc.h"
#include "core/include/memcheck.h"

class Ctrl0080IntrTest: public RmTest
{
public:
    Ctrl0080IntrTest();
    virtual ~Ctrl0080IntrTest();

    virtual string     IsTestSupported();

    virtual RC       Setup();
    virtual RC       Run();
    virtual RC       Cleanup();

private:
    RC               TestNonStalledInterrupt();

    FLOAT64          m_TimeoutMs;
    Notifier         m_Notifier;
    ThreeDAlloc      m_3dAlloc;
};

//! \brief Ctrl0080IntrTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
Ctrl0080IntrTest::Ctrl0080IntrTest()
{
    SetName("Ctrl0080IntrTest");
    m_3dAlloc.SetOldestSupportedClass(FERMI_A);
}

//! \brief Ctrl0080IntrTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
Ctrl0080IntrTest::~Ctrl0080IntrTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \return True if GF100_CHANNEL_GPFIFO (for non-stalling interrupts)
//           and FERMI_A is supported on the current chip.
//-----------------------------------------------------------------------------
string Ctrl0080IntrTest::IsTestSupported()
{
    if (IsClassSupported(GF100_CHANNEL_GPFIFO)
        && m_3dAlloc.IsSupported(GetBoundGpuDevice()))
    {
        return RUN_RMTEST_TRUE;
    }

    if ((IsClassSupported(KEPLER_CHANNEL_GPFIFO_A) || IsClassSupported(KEPLER_CHANNEL_GPFIFO_B)  ||
         IsClassSupported(KEPLER_CHANNEL_GPFIFO_C) || IsClassSupported(MAXWELL_CHANNEL_GPFIFO_A) ||
         IsClassSupported(PASCAL_CHANNEL_GPFIFO_A) || IsClassSupported(VOLTA_CHANNEL_GPFIFO_A)   ||
         IsClassSupported(TURING_CHANNEL_GPFIFO_A) || IsClassSupported(AMPERE_CHANNEL_GPFIFO_A)  ||
         IsClassSupported(HOPPER_CHANNEL_GPFIFO_A)) && m_3dAlloc.IsSupported(GetBoundGpuDevice()))
    {
        return RUN_RMTEST_TRUE;
    }

    return "Supported only on Fermi+ and should support CHANNEL_GPFIFO class";
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa Setup
RC Ctrl0080IntrTest::Setup()
{
    RC      rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());
    m_TimeoutMs = GetTestConfiguration()->TimeoutMs();

    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_GR(0)));

    CHECK_RC(m_3dAlloc.Alloc(m_hCh, GetBoundGpuDevice()));
    m_Notifier.Allocate(m_pCh, 1, GetTestConfiguration());
    return rc;
}

//! \brief Run the test!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC Ctrl0080IntrTest::Run()
{
    RC rc;

    CHECK_RC(TestNonStalledInterrupt());

    return rc;
}

RC Ctrl0080IntrTest::TestNonStalledInterrupt()
{
    LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS eventParams = {0};
    LwRmPtr pLwRm;
    LwRm::Handle hNotifyEvent = 0;
    ModsEvent *pNotifyEvent=NULL;
    void *pEventAddr=NULL;
    RC rc;

    pNotifyEvent = Tasker::AllocEvent();
    pEventAddr = Tasker::GetOsEvent(
            pNotifyEvent,
            pLwRm->GetClientHandle(),
            pLwRm->GetDeviceHandle(GetBoundGpuDevice()));

    // Associate event to object
    CHECK_RC_CLEANUP(pLwRm->AllocEvent(
        pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
        &hNotifyEvent,
        LW01_EVENT_OS_EVENT,
        LW2080_NOTIFIERS_GRAPHICS | LW01_EVENT_NONSTALL_INTR,
        pEventAddr));

    eventParams.event = LW2080_NOTIFIERS_GRAPHICS;
    eventParams.action = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_SINGLE;

    CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                &eventParams, sizeof(eventParams)));

    m_pCh->SetObject(0, m_3dAlloc.GetHandle());
    m_pCh->Write(LWA06F_SUBCHANNEL_3D, LW9097_SET_REPORT_SEMAPHORE_D,
                    DRF_DEF(9097, _SET_REPORT_SEMAPHORE_D, _OPERATION, _TRAP));
    CHECK_RC_CLEANUP(m_pCh->Flush());

    CHECK_RC_CLEANUP(Tasker::WaitOnEvent(pNotifyEvent, m_TestConfig.TimeoutMs()));
    Printf(Tee::PriLow, "%s Pass\n", __FUNCTION__);
    rc = m_pCh->CheckError();

Cleanup:
    if (hNotifyEvent)
        pLwRm->Free(hNotifyEvent);

    if (pNotifyEvent != NULL)
        Tasker::FreeEvent(pNotifyEvent);

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC Ctrl0080IntrTest::Cleanup()
{
    RC rc, firstRc;
    m_Notifier.Free();
    m_3dAlloc.Free();
    FIRST_RC(m_TestConfig.FreeChannel(m_pCh));

    return firstRc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ Ctrl0080IntrTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(Ctrl0080IntrTest, RmTest,
                 "Ctrl0080 intr RM test.");
