/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file task_watchdog-test.c
 *
 * @details    This file contains the unit tests for the Unit Watchdog Task,
 *             implemented by task_watchdog.c.
 */

/* ------------------------ System Includes --------------------------------- */
#include "test-macros.h"
#include "regmock.h"
#include <ut/ut.h>
#include "ut/ut_suite.h"
#include "ut/ut_case.h"
#include "ut/ut_assert.h"
#include "ut/ut_fake_mem.h"

/* ------------------------ Application Includes ---------------------------- */
#include "dev_pwr_csb.h"
#include "csb.h"
#include "pmu/pmuifcmn.h"
#include "pmu/pmuifpmu.h"
// VR-TODO: fix hard coded path
#include "../../_out/tu10x/config/g_tasks.h"

#include "task_watchdog.c"
// VR-TODO: fix hard coded path
#include "../../uproc/libs/lwos/dev/src/lwoswatchdog.c"

/* ------------------------ Local Variables --------------------------------- */
/*!
 * @brief      Fake a memory buffer region that the mocked @ref
 *             lwosCallocResident can use as a heap.
 *
 * @note       Size of 256 is arbitrary for now.
 */
LwU8 validMallocBuffer[256];

/*!
 * @brief      The pointer that the mocked @ref lwosCallocResident uses.
 *
 * @details    The pointer is public so that pre-test methods can cause malloc
 *             to fail by setting the heap to NULL.
 */
LwU8 *mallocBuffer = NULL;

/*!
 * @brief      Boolean representing if mocked @ref timerWatchdogPet_TU10X was
 *             ilwoked.
 *
 * @details    The boolean is public so that tests can see if the Unit Watchdog
 *             Timer method to pet the watchdog was ilwoked, to ensure that the
 *             Unit Watchdog Task code calls the method appropriately.
 */
LwBool wasPet = LW_FALSE;

/* ------------------------ Defines ----------------------------------------- */
/*!
 * @brief      Macro to generate the pre-test function prototype.
 *
 * @param[in]  name  The name of the test.
 *
 * @return     The pre-test function prototype.
 */
#define PRE_TEST_METHOD(name) \
    static void PRE_TEST_NAME(name)()

/*!
 * @brief      Macro to generate the pre-test function name.
 *
 * @param[in]  name  The name of the test.
 *
 * @return     The pre-test function name.
 */
#define PRE_TEST_NAME(name) \
    s_##name##PreTest

/*!
 * @brief      Macro to generate the post-test function prototype.
 *
 * @param[in]  name  The name of the test.
 *
 * @return     The post-test function prototype.
 */
#define POST_TEST_METHOD(name) \
    static void POST_TEST_NAME(name)()

/*!
 * @brief      Macro to generate the post-test function name.
 *
 * @param[in]  name  The name of the test.
 *
 * @return     The post-test function name.
 */
#define POST_TEST_NAME(name) \
    s_##name##PostTest

/* ------------------------ Mocked Functions -------------------------------- */

/*!
 * @brief      Mocked Unit ODP method for memory allocation.
 *
 * @details    The mocked method uses @ref mallocBuffer for the heap to use.
 *
 * @param[in]  bytes  The bytes to allocate.
 *
 * @return     Pointer to the allocated space.
 */
void* lwosCallocResident(LwU32 bytes)
{
    return (void *)mallocBuffer;
}

/*!
 * @brief      Mocked Unit Watchdog Timer method to initialize the watchdog
 *             timer.
 */
void timerWatchdogInit_TU10X()
{
}

/*!
 * @brief      Mocked Unit Watchdog Timer method to pet the watchdog timer.
 *
 * @details    The method sets the @ref wasPet variable to @ref LW_TRUE so that
 *             the tests can see that this method was ilwoked at the correct
 *             moment.
 */
void timerWatchdogPet_TU10X()
{
  wasPet = LW_TRUE;
}

/*!
 * @brief      Mocked Unit LWRTOS method for delaying the lwrrently exelwting
 *             task.
 *
 * @details    Tasks use this method to execute as an infinite loop. However, we
 *             cannot have infinite loops in unit testing for obvious reasons.
 *             This method returns a success status value the first time it is
 *             ilwoked so that the task can execute its logic. The next time,
 *             the method returns an error code so that the task errors out and
 *             the test can continue.
 *
 * @param[in]  ticksToDelay  The number of ticks to delay.
 *
 * @return     FLCN_OK on all even-count ilwocations
 * @return     FLCN_ERROR on all odd-cound ilwocations
 */
FLCN_STATUS lwrtosLwrrentTaskDelayTicks(LwU32 ticksToDelay)
{
  static LwBool firstCall = LW_TRUE;
  FLCN_STATUS status = FLCN_OK;

  if (firstCall)
  {
    firstCall = LW_FALSE;
  }
  else
  {
    firstCall = LW_TRUE;
    status = FLCN_ERROR;
  }

  return status;
}

/*!
 * @brief      Mocked Unit LWRTOS method for handling fatal errors.
 *
 * @details    Mocked just to reduce external unit dependencies. The mocked
 *             method attempts to halt the core, so that the tests can detect that the halt method was exec
 *
 * @param      pTaskHandle  The task handle
 * @param[in]  errorStatus  The error status
 */
void lwrtosHookError(void *pTaskHandle, LwS32 errorStatus)
{
  PMU_HALT();
}

/*!
 * @brief      { function_description }
 *
 * @param      pLwrrentTime  The current time
 */
void osPTimerTimeNsLwrrentGet(FLCN_TIMESTAMP *pLwrrentTime)
{
    pLwrrentTime->parts.hi = 1U;
    pLwrrentTime->parts.lo = 1U;
}

/* -------------------------------------------------------- */
/*!
 * Definition for the Unit Watchdog Task test suite.
 */
UT_SUITE_DEFINE(TASK_WATCHDOG,
                UT_SUITE_SET_COMPONENT("Unit Watchdog Task")
                UT_SUITE_SET_DESCRIPTION("Definition for the Unit Watchdog Task test suite.")
                UT_SUITE_SET_OWNER("vrazdan")
                UT_SUITE_SETUP_HOOK(NULL)
                UT_SUITE_TEARDOWN_HOOK(NULL))

/*!
 * Ensure that Unit Watchdog Task successfully initializes the state of
 * monitored tasks.
 */
UT_CASE_DEFINE(TASK_WATCHDOG, preInitTest_Valid,
  UT_CASE_SET_DESCRIPTION("Ensure that Unit Watchdog Task successfully initializes the state of monitored tasks.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for valid invocation of initializing the Unit
 *             Watchdog Task.
 *
 * @details    Set @ref mallocBuffer to @ref validMallocBuffer so that the code
 *             under test may allocate memory successfully.
 *
 *             The method @ref s_watchdogInitMonitor is ilwoked with the actual
 *             code.
 *
 *             The method @ref timerWatchdogInit_HAL is ilwoked with the mocked
 *             implementation.
 */
PRE_TEST_METHOD(preInitTest_Valid)
{
    mallocBuffer = validMallocBuffer;
}

/*!
 * @brief      Reset all modified state after successfull Unit Watchdog Task
 *             initialization.
 *
 * @details    Ensure that @ref mallocBuffer is set to NULL to restore it to the
 *             default state. Additionally, reset the state of the Unit Watchdog
 *             Framework so that the initialization from this invocation of the
 *             code under test does not affect subsequent tests.
 */
POST_TEST_METHOD(preInitTest_Valid)
{
    mallocBuffer = NULL;

    WatchdogActiveTaskMask = 0U;
    pWatchdogTaskInfo = NULL;
}

/*!
 * @brief      Test that the Unit Watchdog Task successfully initializes itself.
 *
 * @details    The test shall ilwoke the Unit Watchdog Task method that
 *             initializes the Unit. All conditions necessary are set either by
 *             the pre-test method, or by the defines in @ref g_tasks.h .
 *
 *             Parameters: None.
 *
 *             The test passes if the return status is @ref FLCN_OK.
 *
 *             Any other return value constitutes failure.
 */
UT_CASE_RUN(TASK_WATCHDOG, preInitTest_Valid)
{
    FLCN_STATUS status;

    PRE_TEST_NAME(preInitTest_Valid)();

    extern FLCN_STATUS watchdogPreInit(void);
    status = watchdogPreInit();

    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);

    POST_TEST_NAME(preInitTest_Valid)();
}

/* -------------------------------------------------------- */
/*!
 * Ensure that Unit Watchdog Task fails to initialize if there is not sufficient memory.
 */
UT_CASE_DEFINE(TASK_WATCHDOG, preInitTest_MallocFail,
  UT_CASE_SET_DESCRIPTION("Ensure that Unit Watchdog Task fails to initialize if there is not sufficient memory.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for an expected failed invocation of initializing
 *             the Unit Watchdog Task.
 *
 * @details    Set @ref mallocBuffer to NULL so that the code under test may not
 *             allocate memory successfully.
 *
 *             The method @ref s_watchdogInitMonitor is ilwoked with the actual
 *             code.
 *
 *             The method @ref timerWatchdogInit_HAL is ilwoked with the mocked
 *             implementation.
 */
PRE_TEST_METHOD(preInitTest_MallocFail)
{
    mallocBuffer = NULL;
}

/*!
 * @brief      Test that the Unit Watchdog Task fails to initialize itself.
 *
 * @details    The test shall ilwoke the Unit Watchdog Task method that
 *             initializes the Unit. All conditions necessary are set either by
 *             the pre-test method, or by the defines in @ref g_tasks.h .
 *
 *             Parameters: None.
 *
 *             The test passes if the return status is @ref
 *             FLCN_ERR_NO_FREE_MEM.
 *
 *             Any other return value constitutes failure.
 */
UT_CASE_RUN(TASK_WATCHDOG, preInitTest_MallocFail)
{
    FLCN_STATUS status;

    PRE_TEST_NAME(preInitTest_MallocFail)();

    extern FLCN_STATUS watchdogPreInit(void);
    status = watchdogPreInit();

    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_NO_FREE_MEM);
}
/* -------------------------------------------------------- */
/*!
 * Test that the Unit Watchdog Task task loop pets the watchdog if all tasks are in a valid state.
 */
UT_CASE_DEFINE(TASK_WATCHDOG, taskLoop_Valid,
  UT_CASE_SET_DESCRIPTION("Test that the Unit Watchdog Task task loop pets the watchdog if all tasks are in a valid state.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for valid exelwtion of the Unit Watchdog Task task
 *             loop.
 *
 * @details    Set @ref mallocBuffer to @ref validMallocBuffer so that when the
 *             pre-test method ilwokes @ref watchdogPreInit , the monitored task
 *             data is created successfully.
 *
 *             The method @ref lwosWatchdogCheckStatus is ilwoked with the
 *             actual code.
 *
 *             The method @ref lwrtosLwrrentTaskDelayTicks , @ref
 *             timerWatchdogPet_TU10X , @ref watchdogTimeViolationCallback , and
 *             @ref lwrtosHookError are ilwoked with the mocked implementations.
 */
PRE_TEST_METHOD(taskLoop_Valid)
{
    mallocBuffer = validMallocBuffer;

    (void)watchdogPreInit();
}

/*!
 * @brief      Reset all modified state after successful Unit Watchdog Task
 *             loop.
 *
 * @details    Ensure that @ref mallocBuffer is set to NULL to restore it to the
 *             default state. Reset @ref wasPet to LW_FALSE as the default
 *             state. Additionally, reset the state of the Unit Watchdog
 *             Framework so that the initialization from this invocation of the
 *             code under test does not affect subsequent tests.
 */
POST_TEST_METHOD(taskLoop_Valid)
{
    mallocBuffer = NULL;
    wasPet = LW_FALSE;

    WatchdogActiveTaskMask = 0U;
    pWatchdogTaskInfo = NULL;
}

/*!
 * @brief      Test that the Unit Watchdog Task task loop pets the watchdog
 *             timer when all tasks are operating correctly.
 *
 * @details    The test shall ilwoke the Unit Watchdog Task task loop method.
 *             All conditions necessary are set either by the pre-test method,
 *             or by the defines in @ref g_tasks.h .
 *
 *             Parameters: NULL is provided as the task loop parameter is never
 *             used.
 *
 *             The test passes if @ref timerWatchdogPet_TU10X was ilwoked, by
 *             seeing that @ref wasPet is set to @ref LW_TRUE.
 *
 *             If @ref wasPet is @ref LW_FALSE, that constitutes failure.
 */
UT_CASE_RUN(TASK_WATCHDOG, taskLoop_Valid)
{
    PRE_TEST_NAME(taskLoop_Valid)();

    task_watchdog(NULL);

    UT_ASSERT(LW_TRUE == wasPet);

    POST_TEST_NAME(taskLoop_Valid)();
}

/* -------------------------------------------------------- */
/*!
 * Test that the Unit Watchdog Task task loop does not pet the timer and ilwokes
 * the error method if a task is not in a valid state.
 */
UT_CASE_DEFINE(TASK_WATCHDOG, task_MonitoredViolation,
  UT_CASE_SET_DESCRIPTION("* Test that the Unit Watchdog Task task loop does not pet the timer and ilwokes the error method if a task is not in a valid state.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for valid exelwtion of the Unit Watchdog Task task
 *             loop.
 *
 * @details    Set @ref mallocBuffer to @ref validMallocBuffer so that when the
 *             pre-test method ilwokes @ref watchdogPreInit , the monitored task
 *             data is created successfully. After successful initialization,
 *             modify the status of a task to be in an invalid state by having
 *             one pending item in the queue and a timeout of 0, so that the
 *             time value returned by @ref osPTimerTimeNsLwrrentGet is greater
 *             than the deadline.
 *
 *             An item is added to a monitored task so that the @ref
 *             lwosWatchdogCheckStatus status checking state machine is
 *             exelwted.
 *
 *             The methods @ref lwosWatchdogCheckStatus and @ref
 *             watchdogTimeViolationCallback are ilwoked with the actual code.
 *
 *             The method @ref lwrtosLwrrentTaskDelayTicks , @ref
 *             timerWatchdogPet_TU10X , and @ref lwrtosHookError are ilwoked
 *             with the mocked implementations.
 */
PRE_TEST_METHOD(task_MonitoredViolation)
{
    mallocBuffer = validMallocBuffer;

    (void)watchdogPreInit();

    WATCHDOG_TASK_STATE *pLwrrentTaskInfo = NULL;

    // VR-TODO: Replace 0x1 with actual task ID.
    lwosWatchdogAddItem(0x1U);

    s_lwosWatchdogGetTaskInfo(0x1U, &pLwrrentTaskInfo);

    pLwrrentTaskInfo->taskTimeoutNs = 0U;
    pLwrrentTaskInfo->taskStatus = LWBIT32(LWOS_WATCHDOG_ITEM_TIMEOUT);
}

/*!
 * @brief      Reset all modified state after successful Unit Watchdog Task
 *             loop.
 *
 * @details    Ensure that @ref mallocBuffer is set to NULL to restore it to the
 *             default state. Reset @ref wasPet to LW_FALSE as the default
 *             state. Additionally, reset the state of the Unit Watchdog
 *             Framework so that the initialization from this invocation of the
 *             code under test does not affect subsequent tests.
 */
POST_TEST_METHOD(task_MonitoredViolation)
{
    mallocBuffer = NULL;
    wasPet = LW_FALSE;

    WatchdogActiveTaskMask = 0U;
    pWatchdogTaskInfo = NULL;
}

/*!
 * @brief      Test that the Unit Watchdog Task task loop errors when a task is
 *             violating incorrectly, and does not pet the timer.
 *
 * @details    The test shall ilwoke the Unit Watchdog Task task loop method.
 *             All conditions necessary are set either by the pre-test method,
 *             or by the defines in @ref g_tasks.h .
 *
 *             Parameters: NULL is provided as the task loop parameter is never
 *             used.
 *
 *             The test passes if @ref timerWatchdogPet_TU10X was not ilwoked,
 *             by seeing that @ref wasPet remains set to @ref LW_FALSE.
 *             Additionally, the test ensures that there are two ilwocations of
 *             halt, one by @ref watchdogTimeViolationCallback and the other by
 *             @ref lwrtosHookError .
 *
 *             If @ref wasPet is @ref LW_TRUE, or there were not two halts, that
 *             constitutes failure.
 */
UT_CASE_RUN(TASK_WATCHDOG, task_MonitoredViolation)
{
    PRE_TEST_NAME(task_MonitoredViolation)();

    task_watchdog(NULL);

    UT_ASSERT(LW_FALSE == wasPet);
    UT_ASSERT_EQUAL_UINT(2U, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());

    POST_TEST_NAME(task_MonitoredViolation)();
}

/* -------------------------------------------------------- */
/*!
 * Validate that the Unit Watchdog Task implements the task timeout violation
 * callback method.
 */
UT_CASE_DEFINE(TASK_WATCHDOG, task_TimeViolatiolwalid,
  UT_CASE_SET_DESCRIPTION("Validate that the Unit Watchdog Task implements the task timeout violation callback method.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Watchdog Task implements the task timeout
 *             violation method.
 *
 * @details    Parameters: 0x0 is provided, but the value does not matter as it
 *             is not lwrrently used by the method.
 *
 *             The test passes if it compiles and the code under test attempts
 *             to halt once.
 *
 *             The test fails if there is not an invocation of the halt method,
 *             or if the test does not compile.
 */
UT_CASE_RUN(TASK_WATCHDOG, task_TimeViolatiolwalid)
{
    watchdogTimeViolationCallback(0x0U);

    UT_ASSERT_EQUAL_UINT(1U, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
}
