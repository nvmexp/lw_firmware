/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2011,2013,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_ctrl2080tmr.cpp
//! \brief LW2080_CTRL_TMR Tests
//!

#include "class/cl0005.h"  // LW01_EVENT
#include "ctrl/ctrl2080.h" // LW20_SUBDEVICE_XX
#include "gpu/tests/rmtest.h"
#include "lwRmApi.h"
#include "core/include/memcheck.h"

// used in this file outside of the class functions
//static LW2080_CTRL_TIMER_GET_TIME_PARAMS time1;
//static LW2080_CTRL_TIMER_GET_TIME_PARAMS time2;

static bool bCallbackFired1 = false;
static bool bCallbackFired2 = false;
static bool pollFunc1(void*);
static bool pollFunc2(void*);
static void timerCallback1(void*, void*, LwU32, LwU32, LwU32);
static void timerCallback2(void *, void*, LwU32, LwU32, LwU32);

class Ctrl2080TmrTest: public RmTest
{
public:
    Ctrl2080TmrTest();
    virtual ~Ctrl2080TmrTest();

    virtual string IsTestSupported();

    virtual RC   Setup();
    virtual RC   Run();
    virtual RC   Cleanup();

    RC           timerScheduleTest();
    RC           timerPastCallbackTest();

private:
};

//! \brief Ctrl2080TmrTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
Ctrl2080TmrTest::Ctrl2080TmrTest()
{
    SetName("Ctrl2080TmrTest");
}

//! \brief Ctrl2080TmrTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
Ctrl2080TmrTest::~Ctrl2080TmrTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \return true if running under cmodel, the intended target of this test.
//-----------------------------------------------------------------------------
string Ctrl2080TmrTest::IsTestSupported()
{
    return "Temporarily Disabled as backing API changed";
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa Setup
RC Ctrl2080TmrTest::Setup()
{
    RC rc;

    return rc;
}

//! \brief Run the test!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC Ctrl2080TmrTest::Run()
{
    RC rc;

    //CHECK_RC(timerScheduleTest());
    //CHECK_RC(timerPastCallbackTest());
    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC Ctrl2080TmrTest::Cleanup()
{
    return OK;
}

//! \brief Callback for LW2080_CTRL_CMD_TIMER_SCHEDULE functionality test
//!
//! \sa timerCallback
//-----------------------------------------------------------------------------

static void timerCallback1(void *pArg, void *pData, LwU32 hEvent, LwU32 data, LwU32 status)
{
    bCallbackFired1 = true;
}
static void timerCallback2(void *pArg, void *pData, LwU32 hEvent, LwU32 data, LwU32 status)
{
    bCallbackFired2 = true;
}

//! \brief pollFunc Function. Explicit function for polling
//!
//! Designed so as to remove the need for waitidle
//-----------------------------------------------------------------------------
static bool pollFunc1(void *pArgs)
{
    return bCallbackFired1;
}
static bool pollFunc2(void *pArgs)
{
    return bCallbackFired2;
}

//! \brief Verify LW2080_CTRL_CMD_TIMER_SCHEDULE functionality
//!
//! \sa timerScheduleTest
//-----------------------------------------------------------------------------
RC Ctrl2080TmrTest::timerScheduleTest()
{
    RC rc;
    LwRmPtr pLwRm;
    LW0005_ALLOC_PARAMETERS allocParams;
    LW2080_CTRL_CMD_TIMER_SCHEDULE_PARAMS scheduleParams;
    LWOS10_EVENT_KERNEL_CALLBACK_EX tmrCallback;
    LwRm::Handle hEvent;
    bCallbackFired1 = false;

    Printf(Tee::PriHigh, "Ctrl2080TmrTest: Test Original \n");
    // Set a callback to fire immediately
    memset(&tmrCallback, 0, sizeof(tmrCallback));
    tmrCallback.func = timerCallback1;
    tmrCallback.arg = NULL;

    memset(&allocParams, 0, sizeof(allocParams));
    allocParams.hParentClient = pLwRm->GetClientHandle();
    allocParams.hClass        = LW01_EVENT_KERNEL_CALLBACK_EX;
    allocParams.notifyIndex   = LW2080_NOTIFIERS_TIMER;
    allocParams.data          = LW_PTR_TO_LwP64(&tmrCallback);

    CHECK_RC(pLwRm->Alloc(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                          &hEvent,
                          LW01_EVENT_KERNEL_CALLBACK_EX,
                          &allocParams));

    memset(&scheduleParams, 0, sizeof(scheduleParams));
    scheduleParams.time_nsec = 0;
    scheduleParams.flags = DRF_NUM(2080, _CTRL_TIMER_SCHEDULE_FLAGS, _TIME, LW2080_CTRL_TIMER_SCHEDULE_FLAGS_TIME_REL);

    CHECK_RC_CLEANUP(pLwRm->Control(
                     pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                     LW2080_CTRL_CMD_TIMER_SCHEDULE,
                     &scheduleParams,
                     sizeof(scheduleParams)));

   rc = POLLWRAP(&pollFunc1, NULL, 1000);
   if (OK != rc)
   {
        Printf(Tee::PriHigh, "Ctrl2080TmrTest: callback did not fire\n");
        rc = RC::TIMEOUT_ERROR;
   }

Cleanup:
    pLwRm->Free(hEvent);
    return rc;

}

//! \brief Verify LW2080_CTRL_CMD_TIMER_SCHEDULE functionality
//!
//! \sa timerScheduleTest
//-----------------------------------------------------------------------------
RC Ctrl2080TmrTest::timerPastCallbackTest()
{
  RC rc;
  LwRmPtr pLwRm;
  LW0005_ALLOC_PARAMETERS                 allocParams1, allocParams2;
  LW2080_CTRL_CMD_TIMER_SCHEDULE_PARAMS   scheduleParams1, scheduleParams2;
  LWOS10_EVENT_KERNEL_CALLBACK_EX         tmrCallback1, tmrCallback2;
  LwRm::Handle                            hEvent1 = 0;
  LwRm::Handle                            hEvent2 = 0;
  bCallbackFired1 = false, bCallbackFired2 = false;

  Printf(Tee::PriHigh, "Ctrl2080TmrTest: Test Past\n");
  // Set a callback to fire in the future
  memset(&tmrCallback1, 0, sizeof(tmrCallback1));
  tmrCallback1.func = timerCallback1;
  tmrCallback1.arg = NULL;

  memset(&allocParams1, 0, sizeof(allocParams1));
  allocParams1.hParentClient = pLwRm->GetClientHandle();
  allocParams1.hClass        = LW01_EVENT_KERNEL_CALLBACK_EX;
  allocParams1.notifyIndex   = LW2080_NOTIFIERS_TIMER;
  allocParams1.data          = LW_PTR_TO_LwP64(&tmrCallback1);

  CHECK_RC(pLwRm->Alloc(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                          &hEvent1,
                          LW01_EVENT_KERNEL_CALLBACK_EX,
                          &allocParams1));

  memset(&scheduleParams1, 0, sizeof(scheduleParams1));
  scheduleParams1.time_nsec = 10000* 1000ULL; // 10ms
  scheduleParams1.flags = DRF_NUM(2080, _CTRL_TIMER_SCHEDULE_FLAGS, _TIME,
                                  LW2080_CTRL_TIMER_SCHEDULE_FLAGS_TIME_REL);

  // Schedule Callbacks, doing the immediate callback second
  Printf(Tee::PriHigh, "Ctrl2080TmrTest: Sheduling First Callback\n");
  rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                      LW2080_CTRL_CMD_TIMER_SCHEDULE,
                      &scheduleParams1,
                      sizeof(scheduleParams1));
  if (OK != rc)
  {
    Printf(Tee::PriHigh, "Ctrl2080TmrTest: First Callback failed\n");
    goto Cleanup;
  }

  // Set a callback to fire immediately
  memset(&tmrCallback2, 0, sizeof(tmrCallback2));
  tmrCallback2.func = timerCallback2;
  tmrCallback2.arg = NULL;

  memset(&allocParams2, 0, sizeof(allocParams2));
  allocParams2.hParentClient = pLwRm->GetClientHandle();
  allocParams2.hClass        = LW01_EVENT_KERNEL_CALLBACK_EX;
  allocParams2.notifyIndex   = LW2080_NOTIFIERS_TIMER;
  allocParams2.data          = LW_PTR_TO_LwP64(&tmrCallback2);

  CHECK_RC(pLwRm->Alloc(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                          &hEvent2,
                          LW01_EVENT_KERNEL_CALLBACK_EX,
                          &allocParams2));

  memset(&scheduleParams2, 0, sizeof(scheduleParams2));
  scheduleParams2.time_nsec = 0;
  scheduleParams2.flags = DRF_NUM(2080, _CTRL_TIMER_SCHEDULE_FLAGS, _TIME,
                                  LW2080_CTRL_TIMER_SCHEDULE_FLAGS_TIME_REL);

  Printf(Tee::PriHigh, "Ctrl2080TmrTest: Sheduling Second Callback\n");
  rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                       LW2080_CTRL_CMD_TIMER_SCHEDULE,
                       &scheduleParams2,
                       sizeof(scheduleParams2));
  if(OK != rc){
    Printf(Tee::PriHigh, "Ctrl2080TmrTest: Second Callback failed\n");
    goto Cleanup;
  }

  rc = POLLWRAP(&pollFunc2, NULL, 1000);
  if (OK != rc)
  {
      Printf(Tee::PriHigh, "Ctrl2080TmrTest: immediate callback did not fire\n");
      rc = RC::TIMEOUT_ERROR;
  }

  if (bCallbackFired2 && bCallbackFired1)
  {
      Printf(Tee::PriHigh, "Ctqrl2080TmrTest: unclear if immediate beat future callback!\n");
  }

  rc = POLLWRAP(&pollFunc1, NULL, 1000);
  if (OK != rc)
  {
        Printf(Tee::PriHigh, "Ctrl2080TmrTest: future callback did not fire\n");
        rc = RC::TIMEOUT_ERROR;
  }

Cleanup:
    pLwRm->Free(hEvent1);
    pLwRm->Free(hEvent2);
    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ Ctrl2080TmrTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(Ctrl2080TmrTest, RmTest,
                 "Ctrl2080 Timer control RM test.");
