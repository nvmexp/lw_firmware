/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*
 * @file lwoswatchdog-test.c
 *
 * @details    This file contains all the unit tests for the Unit Watchdog
 *             Framework. The tests utilize the register mocking framework to
 *             ensure the correct values are being "written" to the hardware
 *             watchdog registers, and the falcon-intrinsic mocking framework to
 *             verify that the proper error handling methods are called when
 *             needed, without crashing the test.
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
#include "csb.h"
#include "lwoswatchdog.c"
#include "osptimer-mock.h"

/* ------------------------ Local Variables --------------------------------- */
/*!
 * @brief      Dummy global variable representing a monitored task.
 *
 * @details    If tests need to either presume a task is monitored with a
 *             certain configuration, or if tests need to capture information
 *             from code under test, the structure is already defined and ready
 *             for use.
 */
WATCHDOG_TASK_STATE defaultMonitoredTaskState;

/*!
 * @brief   Sequence of timestamps to return from
 *          @ref osPTimerTimeNsLwrrentGet_MOCK_Lwstom
 */
static FLCN_TIMESTAMP *pPTimerTimeNsLwrrentGetReturnSequence;

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

/* ------------------------ Static Functions -------------------------------- */
/*!
 * @brief      Helper method to set @ref defaultMonitoredTaskState to a known
 *             initial value.
 *
 * @details    lastItemStartTime is chosen to be 0 for easy math when testing
 *             timeout conditions.
 *
 *             taskTimeoutNs is chosen to be 1 for easy math as well.
 *
 *             itemsInQueue is chosen to be 0 to represent a task in it's most
 *             common, correct state.
 *
 *             taskStatus is chosen to be in the @ref LWOS_WATCHDOG_TASK_WAITING
 *             state to represent a task in it's most common, correct state.
 */
static void createdefaultMonitoredTaskStateTracked(void)
{
    defaultMonitoredTaskState.lastItemStartTime = 0U;
    defaultMonitoredTaskState.taskTimeoutNs = 0x1U;
    defaultMonitoredTaskState.itemsInQueue = 0U;
    defaultMonitoredTaskState.taskStatus = LWBIT32(LWOS_WATCHDOG_TASK_WAITING);
}

/*!
 * @brief      Helper method to set @ref defaultMonitoredTaskState to a known
 *             initial value.
 *
 * @details    lastItemStartTime is chosen to be 0 for easy math when testing
 *             timeout conditions.
 *
 *             taskTimeoutNs is chosen to be 1 for easy math as well.
 *
 *             itemsInQueue is chosen to be 0 to represent a task in it's most
 *             common, correct state.
 *
 *             taskStatus is chosen to be in the @ref LWOS_WATCHDOG_TASK_WAITING
 *             state to represent a task in it's most common, correct state.
 */
static void resetdefaultMonitoredTaskStateTracked(void)
{
    defaultMonitoredTaskState.lastItemStartTime = 0U;
    defaultMonitoredTaskState.taskTimeoutNs = 0x1U;
    defaultMonitoredTaskState.itemsInQueue = 0U;
    defaultMonitoredTaskState.taskStatus = LWBIT32(LWOS_WATCHDOG_TASK_WAITING);
}

/* ------------------------ Mocked Functions -------------------------------- */
/*!
 * @brief      Mocked Unit LWRTOS method for entering a critical section.
 *
 * @details    There is no benefit in using the actual vPortEnterCritical
 *             method, as the register writes would just be mocked anyways. So
 *             this mocked method reduces the number of Unit dependencies
 *             without impacting testing.
 */
void vPortEnterCritical()
{
}

/*!
 * @brief      Mocked Unit LWRTOS method for exiting a critical section.
 *
 * @details    There is no benefit in using the actual vPortExitCritical
 *             method, as the register writes would just be mocked anyways. So
 *             this mocked method reduces the number of Unit dependencies
 *             without impacting testing.
 */
void vPortExitCritical()
{
}

/*!
 * @brief      Mocked Unit ODP method for modifying the lwrrently exelwting
 *             task's overlay list.
 *
 * @details    This method is mocked to reduce outside Unit dependencies and
 *             eliminate the need to mock a task.
 *
 *             See @ref lwosTaskOverlayDescListExec for the actual method's
 *             documentation.
 *
 * @param[in]  pOvlDescList  The ovl description list
 * @param[in]  ovlDescCnd    Size of the list
 * @param[in]  bReverse      Direction of the iteration.
 */
void lwosTaskOverlayDescListExec
(
    OSTASK_OVL_DESC *pOvlDescList,
    LwU32            ovlDescCnd,
    LwBool           bReverse
)
{
}

/*!
 * @brief      Mocked Unit Timers method retrieving the current time.
 *             task's overlay list.
 *
 * @details    This method is mocked to reduce outside Unit dependencies.
 *
 *             See @ref osPTimerTimeNsLwrrentGet
 *
 * @param      pLwrrentTime  The current time
 */
static void
osPTimerTimeNsLwrrentGet_MOCK_Lwstom
(
    FLCN_TIMESTAMP *pLwrrentTime
)
{
    const LwLength callCount = osPTimerTimeNsLwrrentGet_MOCK_fake.call_count - 1U;
    *pLwrrentTime = pPTimerTimeNsLwrrentGetReturnSequence[callCount];
}

/* -------------------------------------------------------- */
/*!
 * Definition of the Unit Watchdog Framework test suite for libUT.
 */
UT_SUITE_DEFINE(LWOSWATCHDOG,
                UT_SUITE_SET_COMPONENT("Unit Watchdog Framework")
                UT_SUITE_SET_DESCRIPTION("Unit tests for the Unit Watchdog Framework")
                UT_SUITE_SET_OWNER("vrazdan")
                UT_SUITE_SETUP_HOOK(NULL)
                UT_SUITE_TEARDOWN_HOOK(NULL))

/*!
 * Ensure Unit Watchdog Framework implements the watchdog timer expiration callback.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, callback_Valid,
  UT_CASE_SET_DESCRIPTION("Ensure Unit Watchdog Framework implements the watchdog timer expiration callback.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Watchdog Framework implements the watchdog
 *             timer expiration callback.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework method to
 *             handle the watchdog timer exipiring. The test shall verity the
 *             Unit implements the method by the test compiling. The test shall
 *             verify the Unit implements the method by also checking that the
 *             number of halts is 1, because the code under test implementation
 *             halts when the timer expires.
 *
 *             No external unit dependencies are used.
 *
 *             @ref lwrtosTaskGetLwrrentTaskHandle is mocked.
 */
UT_CASE_RUN(LWOSWATCHDOG, callback_Valid)
{
    lwosWatchdogCallbackExpired_IMPL();

    UT_ASSERT_EQUAL_UINT(1U, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
}

/* -------------------------------------------------------- */
/*!
 * Call the Unit Watchdog Framework method to set the Unit's parameters with
 * valid parameters.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, setParameters_Valid,
  UT_CASE_SET_DESCRIPTION("Call the Unit Watchdog Framework method to set the Unit's parameters with valid parameters.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for a valid invocation of setting the Unit
 *             Watchdog Framework parameters.
 *
 * @details    Before the test is run, set the global variables @ref
 *             pWatchdogTaskInfo and @ref WatchdogActiveTaskMask of the Unit
 *             Watchdog Framework to known values, so that after the code under
 *             test is exelwted, the test can verify that the proper values were
 *             written to the variables, and not that the variables were already
 *             set correctly.
 */
PRE_TEST_METHOD(setParameters_Valid)
{
    pWatchdogTaskInfo = NULL;
    WatchdogActiveTaskMask = 0U;
}

/*!
 * @brief      Post-test teardown after testing setting the Unit Watchdog
 *             Framework parameters.
 *
 * @details    Since the test causes @ref pWatchdogTaskInfo and @ref
 *             WatchdogActiveTaskMask to be modified, the post-test ensures that
 *             the variables are set back to clean, unmodified values so that
 *             subsequent tests are not affected.
 */
POST_TEST_METHOD(setParameters_Valid)
{
    pWatchdogTaskInfo = NULL;
    WatchdogActiveTaskMask = 0U;
}

/*!
 * @brief      Test that the Unit Watchdog Framework correctly sets its state
 *             variables when provided valid parameters.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework method that
 *             sets the Unit's global state during initialization.
 *
 *             Parameters: The address provided is of the stack local variable,
 *             so non-NULL. The mask value is non-zero.
 *
 *             The test passes if the return status is @ref FLCN_OK, @ref
 *             pWatchdogTaskInfo is set to the address provided, and that @ref
 *             WatchdogActiveTaskMask is set to the value provided.
 *
 *             Any other return value, or any other values set to the two
 *             variables, constitutes failure.
 */
UT_CASE_RUN(LWOSWATCHDOG, setParameters_Valid)
{
    WATCHDOG_TASK_STATE info = {};
    LwU32 taskMask = 0xFU;
    FLCN_STATUS status;

    PRE_TEST_NAME(setParameters_Valid)();

    status = lwosWatchdogSetParameters(&info, taskMask);

    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_PTR(pWatchdogTaskInfo, &info);
    UT_ASSERT_EQUAL_UINT(WatchdogActiveTaskMask, taskMask);

    POST_TEST_NAME(setParameters_Valid)();
}

/* -------------------------------------------------------- */
/*!
 * Call the Unit Watchdog Framework method to set the Unit's parameters with an
 * invalid pointer parameter.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, setParameters_IlwalidInfoPointer,
  UT_CASE_SET_DESCRIPTION("Call the Unit Watchdog Framework method to set the Unit's parameters with an invalid pointer parameter.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for a valid invocation of setting the Unit
 *             Watchdog Framework parameters.
 *
 * @details    Before the test is run, set the global variables @ref
 *             pWatchdogTaskInfo and @ref WatchdogActiveTaskMask of the Unit
 *             Watchdog Framework to known values, so that after the code under
 *             test is exelwted, the test can verify that the variables were not
 *             modified.
 */
PRE_TEST_METHOD(setParameters_IlwalidInfoPointer)
{
    pWatchdogTaskInfo = (WATCHDOG_TASK_STATE *)(0x42U);
    WatchdogActiveTaskMask = 7U;
}

/*!
 * @brief      Post-test teardown after testing setting the Unit Watchdog
 *             Framework parameters.
 *
 * @details    Since the pre-test causes @ref pWatchdogTaskInfo and @ref
 *             WatchdogActiveTaskMask to be modified, the post-test ensures that
 *             the variables are set back to clean, unmodified values so that
 *             subsequent tests are not affected.
 */
POST_TEST_METHOD(setParameters_IlwalidInfoPointer)
{
    pWatchdogTaskInfo = NULL;
    WatchdogActiveTaskMask = 0U;
}

/*!
 * @brief      Test that the Unit Watchdog Framework does not set the parameters
 *             when invalid parameters are provided.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework method that
 *             sets the Unit's global state during initialization.
 *
 *             Parameters: The address provided is NULL. The mask value is
 *             non-zero.
 *
 *             The test passes if the return status is @ref
 *             FLCN_ERR_ILWALID_ARGUMENT, @ref pWatchdogTaskInfo is not
 *             modified, and that @ref WatchdogActiveTaskMask is not modified.
 *
 *             Any other return value, or any other values for two variables,
 *             constitutes failure.
 */
UT_CASE_RUN(LWOSWATCHDOG, setParameters_IlwalidInfoPointer)
{
    LwU32 taskMask = 0xFU;
    FLCN_STATUS status;

    PRE_TEST_NAME(setParameters_IlwalidInfoPointer)();

    status = lwosWatchdogSetParameters(NULL, taskMask);

    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_ARGUMENT);
    UT_ASSERT_EQUAL_PTR(pWatchdogTaskInfo, (WATCHDOG_TASK_STATE *)(0x42U));
    UT_ASSERT_EQUAL_UINT(WatchdogActiveTaskMask, 7U);

    POST_TEST_NAME(setParameters_IlwalidInfoPointer)();
}

/* -------------------------------------------------------- */
/*!
 * Call the Unit Watchdog Framework method to set the Unit's parameters with
 * valid parameters, where one is NULL.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, setParameters_ValidNullInfo,
  UT_CASE_SET_DESCRIPTION("Call the Unit Watchdog Framework method to set the Unit's parameters with valid parameters, where one is NULL.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for a valid invocation of setting the Unit
 *             Watchdog Framework parameters.
 *
 * @details    Before the test is run, set the global variables @ref
 *             pWatchdogTaskInfo and @ref WatchdogActiveTaskMask of the Unit
 *             Watchdog Framework to known values, so that after the code under
 *             test is exelwted, the test can verify that the proper values were
 *             written to the variables, and not that the variables were already
 *             set correctly.
 */
PRE_TEST_METHOD(setParameters_ValidNullInfo)
{
    pWatchdogTaskInfo = (WATCHDOG_TASK_STATE *)(0x42U);
    WatchdogActiveTaskMask = 11U;
}

/*!
 * @brief      Post-test teardown after testing setting the Unit Watchdog
 *             Framework parameters.
 *
 * @details    Since both the pre-test and test cause @ref pWatchdogTaskInfo and
 *             @ref WatchdogActiveTaskMask to be modified, the post-test ensures
 *             that the variables are set back to clean, unmodified values so
 *             that subsequent tests are not affected.
 */
POST_TEST_METHOD(setParameters_ValidNullInfo)
{
    pWatchdogTaskInfo = NULL;
    WatchdogActiveTaskMask = 0U;
}

/*!
 * @brief      Test that the Unit Watchdog Framework correctly sets its state
 *             variables when provided valid parameters.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework method that
 *             sets the Unit's global state during initialization.
 *
 *             Parameters: The address provided is NULL. The mask value is zero.
 *             Because the mask value is zero, it is valid that the pointer
 *             provided is NULL.
 *
 *             The test passes if the return status is @ref FLCN_OK, @ref
 *             pWatchdogTaskInfo is set to the address provided, and that @ref
 *             WatchdogActiveTaskMask is set to the value provided.
 *
 *             Any other return value, or any other values set to the two
 *             variables, constitutes failure.
 */
UT_CASE_RUN(LWOSWATCHDOG, setParameters_ValidNullInfo)
{
    LwU32 taskMask = 0x0U;
    FLCN_STATUS status;

    PRE_TEST_NAME(setParameters_ValidNullInfo)();

    status = lwosWatchdogSetParameters(NULL, taskMask);

    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_PTR(pWatchdogTaskInfo, NULL);
    UT_ASSERT_EQUAL_UINT(WatchdogActiveTaskMask, 0U);

    POST_TEST_NAME(setParameters_ValidNullInfo)();
}

/* -------------------------------------------------------- */
/*!
 * Call the Unit Watchdog Framework method to get the task index of a monitored
 * task, with valid parameters.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, getIndex_Valid,
  UT_CASE_SET_DESCRIPTION("Call the Unit Watchdog Framework method to get the task index of a monitored task, with valid parameters.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the correct task index for a monitored task is
 *             returned.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework method that
 *             retrieves the task index of a monitored task.
 *
 *             Parameters: The mask value shall be 0x3. The task ID shall be
 *             0x1. The pointer to store the task index in shall be a valid
 *             stack address.
 *
 *             The test passes if the return status is @ref FLCN_OK and the
 *             stack variable holding the monitored task index is 0x1.
 *
 *             Any other return value, or any other values set to the stack
 *             variable, constitutes failure.
 */
UT_CASE_RUN(LWOSWATCHDOG, getIndex_Valid)
{
    const LwU32 theMask = 3U;
    const LwU8 taskId = 1U;
    LwU8 taskIndex = 0U;
    FLCN_STATUS status;

    status = lwosWatchdogGetTaskIndex(theMask, taskId, &taskIndex);

    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_UINT(taskIndex, 1U);
}
/* -------------------------------------------------------- */
/*!
 * Call the Unit Watchdog Framework method to get the task index of a monitored
 * task, with an invalid task ID.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, getIndex_IlwalidTaskID,
  UT_CASE_SET_DESCRIPTION("Call the Unit Watchdog Framework method to get the task index of a monitored task, with an invalid task ID.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that an error code is returned when an invalid task ID is
 *             provided.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework method that
 *             retrieves the task index of a monitored task.
 *
 *             Parameters: The mask value shall be 0x3. The task ID shall be 40.
 *             The pointer to store the task index in shall be a valid stack
 *             address.
 *
 *             The test passes if the return status is @ref
 *             FLCN_ERR_ILWALID_ARGUMENT and the stack variable holding the
 *             monitored task index is still 0x0.
 *
 *             Any other return value, or any other values set to the stack
 *             variable, constitutes failure.
 */
UT_CASE_RUN(LWOSWATCHDOG, getIndex_IlwalidTaskID)
{
    const LwU32 theMask = 3U;
    const LwU8 taskId = 40U;
    LwU8 taskIndex = 0U;
    FLCN_STATUS status;

    status = lwosWatchdogGetTaskIndex(theMask, taskId, &taskIndex);

    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_ARGUMENT);
    UT_ASSERT_EQUAL_UINT(taskIndex, 0U);
}
/* -------------------------------------------------------- */
/*!
 * Ilwoke the get task index method with an invalid pointer.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, getIndex_IlwalidPointer,
  UT_CASE_SET_DESCRIPTION("Ilwoke the get task index method with an invalid pointer.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that an error code is returned when an invalid pointer is
 *             provided.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework method that
 *             retrieves the task index of a monitored task.
 *
 *             Parameters: The mask value shall be 0x3. The task ID shall be 1.
 *             The pointer to store the task index in shall be NULL.
 *
 *             The test passes if the return status is @ref
 *             FLCN_ERR_ILWALID_ARGUMENT.
 *
 *             Any other return value constitutes failure.
 */
UT_CASE_RUN(LWOSWATCHDOG, getIndex_IlwalidPointer)
{
    const LwU32 theMask = 3U;
    const LwU8 taskId = 1U;
    FLCN_STATUS status;

    status = lwosWatchdogGetTaskIndex(theMask, taskId, NULL);

    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_ARGUMENT);
}
/* -------------------------------------------------------- */
/*!
 * Ilwoke the get task index method with the task ID of a not monitored task.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, getIndex_UntrackedTaskID,
  UT_CASE_SET_DESCRIPTION("Ilwoke the get task index method with the task ID of a not monitored task.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that an error code is returned when an unmonitored task ID
 *             is provided.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework method that
 *             retrieves the task index of a monitored task.
 *
 *             Parameters: The mask value shall be 0x3. The task ID shall be
 *             0x7. The pointer to store the task index in shall be a valid
 *             stack pointer.
 *
 *             The test passes if the return status is @ref
 *             FLCN_ERR_OBJECT_NOT_FOUND and the stack variable is unmodified.
 *
 *             Any other return value, or if the stack variable representing the
 *             task index value changes, constitutes failure.
 */
UT_CASE_RUN(LWOSWATCHDOG, getIndex_UntrackedTaskID)
{
    const LwU32 theMask = 3U;
    const LwU8 taskId = 7U;
    LwU8 taskIndex = 0U;
    FLCN_STATUS status;

    status = lwosWatchdogGetTaskIndex(theMask, taskId, &taskIndex);

    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_OBJECT_NOT_FOUND);
    UT_ASSERT_EQUAL_UINT(taskIndex, 0U);
}
/* -------------------------------------------------------- */
/*!
 * Ilwoke the get task information method with valid parameters.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, getInfo_Valid,
  UT_CASE_SET_DESCRIPTION("Ilwoke the get task information method with valid parameters.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Create a valid monitored task for retrieval.
 *
 * @details    In order for the test to verify that the correct task information
 *             is returned, task information must be created and stored by the
 *             Unit Watchdog Framework. The pre-test method creates a monitored
 *             task through @ref createdefaultMonitoredTaskStateTracked and
 *             updates the Unit Watchdog Framework's state to believe it is
 *             monitoring that task.
 *
 *             The code under test shall ilwoke @ref lwosWatchdogGetTaskIndex ,
 *             which shall not be mocked.
 */
PRE_TEST_METHOD(getInfo_Valid)
{
    createdefaultMonitoredTaskStateTracked();
    pWatchdogTaskInfo = &defaultMonitoredTaskState;
    WatchdogActiveTaskMask = 1U;
}

/*!
 * @brief      Reset the valid monitored task and Unit Watchdog Framework state.
 *
 * @details    After the test exelwtes, the Unit Watchdog Framework state must
 *             be reset back to when it has no knowledge of monitoring any
 *             tasks. The monitored task state shall also be reset.
 */
POST_TEST_METHOD(getInfo_Valid)
{
    pWatchdogTaskInfo = NULL;
    WatchdogActiveTaskMask = 0U;
    resetdefaultMonitoredTaskStateTracked();
}

/*!
 * @brief      Test that the correct task information is retrieved when provided
 *             valid parameters.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework method to get a
 *             monitored task's info.
 *
 *             Parameters: The task ID shall be 0x0, so that the task's index is
 *             0 which is where the pre-test method stored the data, and the
 *             pointer to store the results shall be a valid stack pointer.
 *
 *             Passing criteria: The test passes if the return status is @ref
 *             FLCN_OK and the monitored task information address is the same as
 *             what was provided.
 *
 *             If any of the passing criteria is false, the test fails.
 */
UT_CASE_RUN(LWOSWATCHDOG, getInfo_Valid)
{
    const LwU8 taskId = 0U;
    WATCHDOG_TASK_STATE taskInfo;
    WATCHDOG_TASK_STATE *pTaskInfo = NULL;

    FLCN_STATUS status;

    PRE_TEST_NAME(getInfo_Valid)();

    status = s_lwosWatchdogGetTaskInfo(taskId, &pTaskInfo);

    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_PTR(pTaskInfo, &defaultMonitoredTaskState);

    POST_TEST_NAME(getInfo_Valid)();
}
/* -------------------------------------------------------- */
/*!
 * Ilwoke the get task information method with an invalid pointer.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, getInfo_IlwalidPointer,
  UT_CASE_SET_DESCRIPTION("Ilwoke the get task information method with an invalid pointer.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the the method returns an error status code when
 *             provided invalid parameters.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework method to get a
 *             monitored task's info.
 *
 *             Parameters: The task ID shall be 0x0, and the pointer to store
 *             the results shall be NULL.
 *
 *             Passing criteria: The test passes if the return status is @ref
 *             FLCN_ERR_ILWALID_ARGUMENT.
 *
 *             If any of the passing criteria is false, the test fails.
 */
UT_CASE_RUN(LWOSWATCHDOG, getInfo_IlwalidPointer)
{
    const LwU8 taskId = 0U;
    FLCN_STATUS status;

    status = s_lwosWatchdogGetTaskInfo(taskId, NULL);

    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_ARGUMENT);
}
/* -------------------------------------------------------- */
/*!
 * Ilwoke the get task information method with an invalid task ID.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, getInfo_IlwalidTaskID,
  UT_CASE_SET_DESCRIPTION("Ilwoke the get task information method with an invalid task ID.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Create a valid monitored task.
 *
 * @details    In order for the test to verify that the code under test only
 *             returns the correct information when a task is monitored and an
 *             error code otherwise, the Unit Watchdog Framework must think that
 *             the internal monitoring state is valid and that there is at least
 *             one monitored task. The pre-test method creates a monitored task
 *             through @ref createdefaultMonitoredTaskStateTracked and updates
 *             the Unit Watchdog Framework's state to believe it is monitoring
 *             that task.
 *
 *             The code under test shall ilwoke @ref lwosWatchdogGetTaskIndex ,
 *             which shall not be mocked.
 */
PRE_TEST_METHOD(getInfo_IlwalidTaskID)
{
    createdefaultMonitoredTaskStateTracked();
    pWatchdogTaskInfo = &defaultMonitoredTaskState;
    WatchdogActiveTaskMask = 1U;
}

/*!
 * @brief      Reset the valid monitored task and Unit Watchdog Framework state.
 *
 * @details    After the test exelwtes, the Unit Watchdog Framework state must
 *             be reset back to when it has no knowledge of monitoring any
 *             tasks. The monitored task state shall also be reset.
 */
POST_TEST_METHOD(getInfo_IlwalidTaskID)
{
    pWatchdogTaskInfo = NULL;
    WatchdogActiveTaskMask = 0U;
    resetdefaultMonitoredTaskStateTracked();
}

/*!
 * @brief      Test that the the method returns an error status code when
 *             provided an invalid task ID.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework method to get a
 *             monitored task's info.
 *
 *             Parameters: The task ID shall be 13, and the pointer to store the
 *             results shall be a valid stack address. The value 13 is used as
 *             the task ID, as in the pre-test setup, the Unit Watchdog
 *             Framework is set up with only one monitored task with task ID 0.
 *
 *             Passing criteria: The test passes if the return status is @ref
 *             FLCN_ERR_OBJECT_NOT_FOUND and no value is set to the provided
 *             pointer.
 *
 *             If any of the passing criteria is false, the test fails.
 */
UT_CASE_RUN(LWOSWATCHDOG, getInfo_IlwalidTaskID)
{
    const LwU8 taskId = 13U;
    WATCHDOG_TASK_STATE taskInfo;
    WATCHDOG_TASK_STATE *pTaskInfo = NULL;

    FLCN_STATUS status;

    PRE_TEST_NAME(getInfo_IlwalidTaskID)();

    status = s_lwosWatchdogGetTaskInfo(taskId, &pTaskInfo);

    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_OBJECT_NOT_FOUND);
    UT_ASSERT_EQUAL_PTR(pTaskInfo, NULL);

    POST_TEST_NAME(getInfo_IlwalidTaskID)();
}
/* -------------------------------------------------------- */
/*!
 * Ilwoke the static helper method to start an item with valid parameters.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, startItem_Valid,
  UT_CASE_SET_DESCRIPTION("Ilwoke the static helper method to start an item with valid parameters.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Create a valid monitored task that can begin work on an item.
 *
 * @details    In order for the code under test to execute successfully, there
 *             must exist a task that is monitored by the Unit Watchdog
 *             Framework that has pending work and is able to start it. The a
 *             monitored task through @ref
 *             createdefaultMonitoredTaskStateTracked and updates the Unit
 *             Watchdog Framework's state to believe it is monitoring that task.
 *             The monitored task data is modified so that there is 1 item in
 *             the queue.
 *
 *             Additionally, the pre-test method mocks
 *             @ref osPTimerTimeNsLwrrentGet for retrieving the current system
 *             time. It is mocked to return a fixed value when called by the code
 *             under test.
 *
 *             The methods @ref s_lwosWatchdogGetTaskInfo and
 *             @ref lwrtosHookError are both called with their actual
 *             implementations.
 *
 *             The methods @ref lwrtosENTER_CRITICAL , @ref lwrtosEXIT_CRITICAL,
 *             and @ref lwrtosTaskGetLwrrentTaskHandle are mocked.
 */
PRE_TEST_METHOD(startItem_Valid)
{
    static FLCN_TIMESTAMP startItem_Valid_PTimerValues[] =
    {
        {
            .data = 0x1U,
        },
    };

    createdefaultMonitoredTaskStateTracked();
    pWatchdogTaskInfo = &defaultMonitoredTaskState;
    defaultMonitoredTaskState.itemsInQueue = 1U;
    WatchdogActiveTaskMask = 1U;

    osPTimerMockInit();
    osPTimerTimeNsLwrrentGet_MOCK_fake.lwstom_fake =
        osPTimerTimeNsLwrrentGet_MOCK_Lwstom;
    pPTimerTimeNsLwrrentGetReturnSequence =
        startItem_Valid_PTimerValues;
}

/*!
 * @brief      Reset the Unit Watchdog Framework state.
 *
 * @details    Reset the Unit so that no tasks are monitored, and the global
 *             task used for testing is reset back to its original state. The
 *             mocked registers are automatically reset by the UTF.
 */
POST_TEST_METHOD(startItem_Valid)
{
    pWatchdogTaskInfo = NULL;
    WatchdogActiveTaskMask = 0U;
    resetdefaultMonitoredTaskStateTracked();
}

/*!
 * @brief      Test that a monitored task's state updates correctly when
 *             starting an item.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework static helper
 *             method to update a monitored task when it starts work on a new
 *             item.
 *
 *             Parameters: The task ID shall be 0, which is valid because only
 *             one task is set to be monitored by the pre-test setup.
 *
 *             Passing criteria: The test passes if the return value is @ref
 *             FLCN_OK , the monitored task's state is updated to STARTED, and
 *             the time the task started the work is the value the mocked PTIMER
 *             registers are set to return.
 *
 *             If any of the passing criteria are false, the test fails.
 */
UT_CASE_RUN(LWOSWATCHDOG, startItem_Valid)
{
    const LwU8 taskId = 0U;
    FLCN_STATUS status;

    PRE_TEST_NAME(startItem_Valid)();

    status = s_lwosWatchdogStartItem(taskId);

    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_UINT(defaultMonitoredTaskState.taskStatus, LWBIT32(LWOS_WATCHDOG_ITEM_STARTED));
    UT_ASSERT_EQUAL_UINT(defaultMonitoredTaskState.lastItemStartTime, 0x1U);

    POST_TEST_NAME(startItem_Valid)();
}
/* -------------------------------------------------------- */
/*!
 * Ilwoke the static helper method to start an item with a task with no pending
 * items.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, startItem_IlwalidTaskItemCount,
  UT_CASE_SET_DESCRIPTION("Ilwoke the static helper method to start an item with a task with no pending items.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Create an invalid monitored task that has no pending items.
 *
 * @details    In order for the test to execute successfully, there must exist a
 *             task that is monitored by the Unit Watchdog Framework that has no
 *             pending work. The monitored task is created through @ref
 *             createdefaultMonitoredTaskStateTracked and updates the Unit
 *             Watchdog Framework's state to believe it is monitoring that task.
 *
 *             Additionally, the pre-test method mocks
 *             @ref osPTimerTimeNsLwrrentGet for retrieving the current system
 *             time. It is mocked to return a fixed value when called by the code
 *             under test.
 *
 *             The methods @ref s_lwosWatchdogGetTaskInfo and
 *             @ref lwrtosHookError are both called with their actual
 *             implementations.
 *
 *             The methods @ref lwrtosENTER_CRITICAL , @ref lwrtosEXIT_CRITICAL,
 *             and @ref lwrtosTaskGetLwrrentTaskHandle are mocked.
 */
PRE_TEST_METHOD(startItem_IlwalidTaskItemCount)
{
    static FLCN_TIMESTAMP startItem_IlwalidTaskItemCount_PTimerValues[] =
    {
        {
            .data = 0x1U,
        },
    };

    createdefaultMonitoredTaskStateTracked();
    defaultMonitoredTaskState.itemsInQueue = 0U;

    pWatchdogTaskInfo = &defaultMonitoredTaskState;
    WatchdogActiveTaskMask = 1U;

    osPTimerMockInit();
    osPTimerTimeNsLwrrentGet_MOCK_fake.lwstom_fake =
        osPTimerTimeNsLwrrentGet_MOCK_Lwstom;
    pPTimerTimeNsLwrrentGetReturnSequence =
        startItem_IlwalidTaskItemCount_PTimerValues;
}

/*!
 * @brief      Reset the Unit Watchdog Framework state.
 *
 * @details    Reset the Unit so that no tasks are monitored, and the global
 *             task used for testing is reset back to its original state. The
 *             mocked registers are automatically reset by the UTF.
 */
POST_TEST_METHOD(startItem_IlwalidTaskItemCount)
{
    pWatchdogTaskInfo = NULL;
    WatchdogActiveTaskMask = 0U;
    resetdefaultMonitoredTaskStateTracked();
}

/*!
 * @brief      Test that the code under test detects the task is in an invalid
 *             state.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework static helper
 *             method to update a monitored task when it starts work on a new
 *             item.
 *
 *             Parameters: The task ID shall be 0, which is valid because only
 *             one task is set to be monitored by the pre-test setup.
 *
 *             Passing criteria: The test passes if the return value is @ref
 *             FLCN_ERR_ILLEGAL_OPERATION , the monitored task's state is still
 *             WAITING, the task's last start time is still 0, and the number of
 *             halts exelwted is 1.
 *
 *             If any of the passing criteria are false, the test fails.
 */
UT_CASE_RUN(LWOSWATCHDOG, startItem_IlwalidTaskItemCount)
{
    const LwU8 taskId = 0U;
    FLCN_STATUS status;

    PRE_TEST_NAME(startItem_IlwalidTaskItemCount)();

    status = s_lwosWatchdogStartItem(taskId);

    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILLEGAL_OPERATION);
    UT_ASSERT_EQUAL_UINT(defaultMonitoredTaskState.lastItemStartTime, 0U);
    UT_ASSERT_EQUAL_UINT(defaultMonitoredTaskState.taskStatus, LWBIT32(LWOS_WATCHDOG_TASK_WAITING));
    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_HALT_COUNT(), 1U);

    POST_TEST_NAME(startItem_IlwalidTaskItemCount)();
}
/* -------------------------------------------------------- */
/*!
 * Ilwoke the static helper method to start an item with an invalid task ID.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, startItem_IlwalidTaskID,
  UT_CASE_SET_DESCRIPTION("Ilwoke the static helper method to start an item with an invalid task ID.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the code under test detects the task ID is invalid.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework static helper
 *             method to update a monitored task when it starts work on a new
 *             item.
 *
 *             Parameters: The task ID shall be 60, which is invalid because the
 *             maximum task ID is @ref WATCHDOG_MAX_TASK_ID.
 *
 *             Passing criteria: The test passes if the return value is @ref
 *             FLCN_ERR_ILWALID_ARGUMENT.
 *
 *             If any of the passing criteria are false, the test fails.
 */
UT_CASE_RUN(LWOSWATCHDOG, startItem_IlwalidTaskID)
{
    const LwU8 taskId = 60U;
    FLCN_STATUS status;

    status = s_lwosWatchdogStartItem(taskId);

    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_ARGUMENT);
}
/* -------------------------------------------------------- */
/*!
 * Ilwoke the static helper method to complete an item with valid parameters.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, completeItem_Valid,
  UT_CASE_SET_DESCRIPTION("Ilwoke the static helper method to complete an item with valid parameters.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Create a valid monitored task that can complete work on an item.
 *
 * @details    In order for the code under test to execute successfully, there
 *             must exist a task that is monitored by the Unit Watchdog
 *             Framework that has started work and is able to complete it. The
 *             monitored task is created through @ref
 *             createdefaultMonitoredTaskStateTracked and the pre-test method
 *             updates the Unit Watchdog Framework's state to believe it is
 *             monitoring that task. The monitored task data is modified so that
 *             there is 1 item in the queue, and that the task timeout is 10.
 *
 *             Additionally, the pre-test method mocks
 *             @ref osPTimerTimeNsLwrrentGet for retrieving the current system
 *             time. It is mocked to return a fixed value when called by the code
 *             under test.
 *
 *             The methods @ref s_lwosWatchdogGetTaskInfo , @ref lwrtosHookError
 *             and , @ref watchdogTimeViolationCallback are all called with their actual
 *             implementations.
 *
 *             The methods @ref lwrtosENTER_CRITICAL , @ref lwrtosEXIT_CRITICAL,
 *             and @ref lwrtosTaskGetLwrrentTaskHandle are mocked.
 */
PRE_TEST_METHOD(completeItem_Valid)
{
    static FLCN_TIMESTAMP completeItem_Valid_PTimerValues[] =
    {
        {
            .data = 0x1U,
        },
    };

    createdefaultMonitoredTaskStateTracked();

    pWatchdogTaskInfo = &defaultMonitoredTaskState;
    WatchdogActiveTaskMask = 1U;

    defaultMonitoredTaskState.itemsInQueue = 1U;
    defaultMonitoredTaskState.lastItemStartTime = 0U;
    defaultMonitoredTaskState.taskTimeoutNs = 10U;

    osPTimerMockInit();
    osPTimerTimeNsLwrrentGet_MOCK_fake.lwstom_fake =
        osPTimerTimeNsLwrrentGet_MOCK_Lwstom;
    pPTimerTimeNsLwrrentGetReturnSequence =
        completeItem_Valid_PTimerValues;
}

/*!
 * @brief      Reset the Unit Watchdog Framework state.
 *
 * @details    Reset the Unit so that no tasks are monitored, and the global
 *             task used for testing is reset back to its original state. The
 *             mocked registers are automatically reset by the UTF.
 */
POST_TEST_METHOD(completeItem_Valid)
{
    pWatchdogTaskInfo = NULL;
    WatchdogActiveTaskMask = 0U;
    resetdefaultMonitoredTaskStateTracked();
}

/*!
 * @brief      Test that a monitored task's state updates correctly when
 *             completing an item.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework static helper
 *             method to update a monitored task when it completes work on an
 *             item.
 *
 *             Parameters: The task ID shall be 0, which is valid because only
 *             one task is set to be monitored by the pre-test setup.
 *
 *             Passing criteria: The test passes if the return value is @ref
 *             FLCN_OK , the monitored task's state is updated to both WAITING
 *             and COMPLETED, and the task has no items in its queue.
 *
 *             If any of the passing criteria are false, the test fails.
 */
UT_CASE_RUN(LWOSWATCHDOG, completeItem_Valid)
{
    const LwU8 taskId = 0U;
    FLCN_STATUS status;

    PRE_TEST_NAME(completeItem_Valid)();

    status = s_lwosWatchdogCompleteItem(taskId);

    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_UINT(defaultMonitoredTaskState.itemsInQueue, 0U);
    UT_ASSERT_EQUAL_UINT(defaultMonitoredTaskState.taskStatus, LWBIT32(LWOS_WATCHDOG_TASK_WAITING) | LWBIT32(LWOS_WATCHDOG_ITEM_COMPLETED));

    POST_TEST_NAME(completeItem_Valid)();
}
/* -------------------------------------------------------- */
/*!
 * Ilwoke the static helper method to complete an item with a task that has
 * exceeded its time limit.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, completeItem_TaskTimeout,
  UT_CASE_SET_DESCRIPTION("Ilwoke the static helper method to complete an item with a task that has exceeded its time limit.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Create a valid monitored task that has exceeded its timelimit
 *             working on an item.
 *
 * @details    In order for the code under test to execute successfully, there
 *             must exist a task that is monitored by the Unit Watchdog
 *             Framework that has started work and is able to complete it. The a
 *             monitored task through @ref
 *             createdefaultMonitoredTaskStateTracked and updates the Unit
 *             Watchdog Framework's state to believe it is monitoring that task.
 *             The monitored task data is modified so that there is 1 item in
 *             the queue, that the task timeout is 10, and that the task started
 *             the item at time 0.
 *
 *             Additionally, the pre-test method mocks
 *             @ref osPTimerTimeNsLwrrentGet for retrieving the current system
 *             time. It is mocked to return a fixed value when called by the code
 *             under test.
 *
 *             The methods @ref s_lwosWatchdogGetTaskInfo , @ref lwrtosHookError
 *             and , @ref watchdogTimeViolationCallback are all called with their actual
 *             implementations.

 *             The methods @ref lwrtosENTER_CRITICAL , @ref lwrtosEXIT_CRITICAL,
 *             and @ref lwrtosTaskGetLwrrentTaskHandle are mocked.
 */
PRE_TEST_METHOD(completeItem_TaskTimeout)
{
    static FLCN_TIMESTAMP completeItem_TaskTimeout_PTimerValues[] =
    {
        {
            .data = 0xFFU,
        },
    };

    createdefaultMonitoredTaskStateTracked();

    pWatchdogTaskInfo = &defaultMonitoredTaskState;
    WatchdogActiveTaskMask = 1U;

    defaultMonitoredTaskState.itemsInQueue = 1U;
    defaultMonitoredTaskState.lastItemStartTime = 0U;
    defaultMonitoredTaskState.taskTimeoutNs = 10U;

    osPTimerMockInit();
    osPTimerTimeNsLwrrentGet_MOCK_fake.lwstom_fake =
        osPTimerTimeNsLwrrentGet_MOCK_Lwstom;
    pPTimerTimeNsLwrrentGetReturnSequence =
        completeItem_TaskTimeout_PTimerValues;
}

/*!
 * @brief      Reset the Unit Watchdog Framework state.
 *
 * @details    Reset the Unit so that no tasks are monitored, and the global
 *             task used for testing is reset back to its original state. The
 *             mocked registers are automatically reset by the UTF.
 */
POST_TEST_METHOD(completeItem_TaskTimeout)
{
    pWatchdogTaskInfo = NULL;
    WatchdogActiveTaskMask = 0U;
    resetdefaultMonitoredTaskStateTracked();
}

/*!
 * @brief      Test that a monitored task's state updates correctly when
 *             completing an item after exceeding its time limit.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework static helper
 *             method to update a monitored task when it completes work on an
 *             item.
 *
 *             Parameters: The task ID shall be 0, which is valid because only
 *             one task is set to be monitored by the pre-test setup.
 *
 *             Passing criteria: The test passes if the return value is @ref
 *             FLCN_ERR_TIMEOUT , the monitored task's state is updated to
 *             WAITING and COMPLETED and TIMEOUT, the task has no items in its
 *             queue, and that the code attempted to halt the core once.
 *
 *             If any of the passing criteria are false, the test fails.
 */
UT_CASE_RUN(LWOSWATCHDOG, completeItem_TaskTimeout)
{
    const LwU8 taskId = 0U;
    FLCN_STATUS status;

    PRE_TEST_NAME(completeItem_TaskTimeout)();

    status = s_lwosWatchdogCompleteItem(taskId);

    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_TIMEOUT);
    UT_ASSERT_EQUAL_UINT(defaultMonitoredTaskState.itemsInQueue, 0U);
    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_HALT_COUNT(), 1U);
    UT_ASSERT_EQUAL_UINT(defaultMonitoredTaskState.taskStatus, LWBIT32(LWOS_WATCHDOG_TASK_WAITING) | LWBIT32(LWOS_WATCHDOG_ITEM_COMPLETED) | LWBIT32(LWOS_WATCHDOG_ITEM_TIMEOUT));

    POST_TEST_NAME(completeItem_TaskTimeout)();
}
/* -------------------------------------------------------- */
/*!
 * Ilwoke the static helper method to complete an item with a task that has no
 * work items.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, completeItem_IlwalidItemCount,
  UT_CASE_SET_DESCRIPTION("Ilwoke the static helper method to complete an item with a task that has no work items.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Create a valid monitored task that has no work items.
 *
 * @details    In order for the code under test to execute successfully, there
 *             must exist a task that is monitored by the Unit Watchdog
 *             Framework that has started work and is able to complete it. The a
 *             monitored task through @ref
 *             createdefaultMonitoredTaskStateTracked and updates the Unit
 *             Watchdog Framework's state to believe it is monitoring that task.
 *             The monitored task data is modified so that there is no item in
 *             the queue.
 *
 *             The methods @ref s_lwosWatchdogGetTaskInfo and @ref
 *             lwrtosHookError are called with their actual implementations.
 *
 *             The methods @ref lwrtosENTER_CRITICAL , @ref lwrtosEXIT_CRITICAL,
 *             and @ref lwrtosTaskGetLwrrentTaskHandle are mocked.
 */
PRE_TEST_METHOD(completeItem_IlwalidItemCount)
{
    createdefaultMonitoredTaskStateTracked();

    pWatchdogTaskInfo = &defaultMonitoredTaskState;
    WatchdogActiveTaskMask = 1U;

    defaultMonitoredTaskState.itemsInQueue = 0U;
}

/*!
 * @brief      Reset the Unit Watchdog Framework state.
 *
 * @details    Reset the Unit so that no tasks are monitored, and the global
 *             task used for testing is reset back to its original state.
 */
POST_TEST_METHOD(completeItem_IlwalidItemCount)
{
    pWatchdogTaskInfo = NULL;
    WatchdogActiveTaskMask = 0U;
    resetdefaultMonitoredTaskStateTracked();
}

/*!
 * @brief      Test that the code under test detects that the task is ilwoking
 *             the method in an invalid state.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework static helper
 *             method to update a monitored task when it completes work on an
 *             item.
 *
 *             Parameters: The task ID shall be 0, which is valid because only
 *             one task is set to be monitored by the pre-test setup.
 *
 *             Passing criteria: The test passes if the return value is @ref
 *             FLCN_ERR_ILLEGAL_OPERATION and that the code attempted to halt
 *             the core once.
 *
 *             If any of the passing criteria are false, the test fails.
 */
UT_CASE_RUN(LWOSWATCHDOG, completeItem_IlwalidItemCount)
{
    const LwU8 taskId = 0U;
    FLCN_STATUS status;

    PRE_TEST_NAME(completeItem_IlwalidItemCount)();

    status = s_lwosWatchdogCompleteItem(taskId);

    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILLEGAL_OPERATION);
    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_HALT_COUNT(), 1U);

    POST_TEST_NAME(completeItem_IlwalidItemCount)();
}
/* -------------------------------------------------------- */
/*!
 * Ilwoke the static helper method to complete an item with an invalid task ID.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, completeItem_IlwalidTaskID,
  UT_CASE_SET_DESCRIPTION("Ilwoke the static helper method to complete an item with an invalid task ID.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Create a valid monitored task.
 *
 * @details    In order for the code under test to execute successfully, there
 *             must exist a task that is monitored by the Unit Watchdog
 *             Framework. The monitored task is created through @ref
 *             createdefaultMonitoredTaskStateTracked and updates the Unit
 *             Watchdog Framework's state to believe it is monitoring that task.
 *
 *             The method @ref s_lwosWatchdogGetTaskInfo is called with its
 *             actual implementations.
 */
PRE_TEST_METHOD(completeItem_IlwalidTaskID)
{
    createdefaultMonitoredTaskStateTracked();

    pWatchdogTaskInfo = &defaultMonitoredTaskState;
    WatchdogActiveTaskMask = 1U;
}

/*!
 * @brief      Reset the Unit Watchdog Framework state.
 *
 * @details    Reset the Unit so that no tasks are monitored, and the global
 *             task used for testing is reset back to its original state.
 */
POST_TEST_METHOD(completeItem_IlwalidTaskID)
{
    pWatchdogTaskInfo = NULL;
    WatchdogActiveTaskMask = 0U;
    resetdefaultMonitoredTaskStateTracked();
}

/*!
 * @brief      Test that the code under test detects that the task ID is
 *             invalid.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework static helper
 *             method to update a monitored task when it completes work on an
 *             item.
 *
 *             Parameters: The task ID shall be 90, which is invalid.
 *
 *             Passing criteria: The test passes if the return value is @ref
 *             FLCN_ERR_ILWALID_ARGUMENT.
 *
 *             If any of the passing criteria are false, the test fails.
 */
UT_CASE_RUN(LWOSWATCHDOG, completeItem_IlwalidTaskID)
{
    const LwU8 taskId = 90U;
    FLCN_STATUS status;

    PRE_TEST_NAME(completeItem_IlwalidTaskID)();

    status = s_lwosWatchdogCompleteItem(taskId);

    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_ARGUMENT);

    POST_TEST_NAME(completeItem_IlwalidTaskID)();
}
/* -------------------------------------------------------- */
/*!
 * Ilwoke the Unit Watchdog Framework method exposed to tasks for adding an item
 * from an ISR with valid parameters.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, addItemFromISR_Valid,
  UT_CASE_SET_DESCRIPTION("Ilwoke the Unit Watchdog Framework method exposed to tasks for adding an item from an ISR with valid parameters.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Create a valid monitored task.
 *
 * @details    In order for the code under test to execute successfully, there
 *             must exist a task that is monitored by the Unit Watchdog
 *             Framework. The monitored task is created through @ref
 *             createdefaultMonitoredTaskStateTracked and updates the Unit
 *             Watchdog Framework's state to believe it is monitoring that task.
 *             The monitored task is set to have 1 item in its queue.
 *
 *             The methods @ref s_lwosWatchdogGetTaskInfo and @ref
 *             lwrtosHookError are called with their actual implementations.
 *
 *             The method @ref lwrtosTaskGetLwrrentTaskHandle is mocked.
 */
PRE_TEST_METHOD(addItemFromISR_Valid)
{
    createdefaultMonitoredTaskStateTracked();

    pWatchdogTaskInfo = &defaultMonitoredTaskState;
    WatchdogActiveTaskMask = 1U;

    defaultMonitoredTaskState.itemsInQueue = 1U;
}

/*!
 * @brief      Reset the Unit Watchdog Framework state.
 *
 * @details    Reset the Unit so that no tasks are monitored, and the global
 *             task used for testing is reset back to its original state.
 */
POST_TEST_METHOD(addItemFromISR_Valid)
{
    pWatchdogTaskInfo = NULL;
    WatchdogActiveTaskMask = 0U;
    resetdefaultMonitoredTaskStateTracked();
}

/*!
 * @brief      Test that a monitored task's state updates correctly when adding
 *             an item from an ISR.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework method to add
 *             an item to a monitored task from an ISR.
 *
 *             Parameters: The task ID shall be 0, which is valid because only
 *             one task is set to be monitored by the pre-test setup.
 *
 *             Passing criteria: The test passes if the return value is @ref
 *             FLCN_OK and the task has 2 items in its queue.
 *
 *             If any of the passing criteria are false, the test fails.
 */
UT_CASE_RUN(LWOSWATCHDOG, addItemFromISR_Valid)
{
    const LwU8 taskId = 0U;
    FLCN_STATUS status;

    PRE_TEST_NAME(addItemFromISR_Valid)();

    status = lwosWatchdogAddItemFromISR(taskId);

    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_UINT(defaultMonitoredTaskState.itemsInQueue, 2U);

    POST_TEST_NAME(addItemFromISR_Valid)();
}
/* -------------------------------------------------------- */
/*!
 * Ilwoke the Unit Watchdog Framework method exposed to tasks for adding an item
 * from an ISR with a task having too many items.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, addItemFromISR_IlwalidItemCount,
  UT_CASE_SET_DESCRIPTION("Ilwoke the Unit Watchdog Framework method exposed to tasks for adding an item from an ISR with a task having too many items.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Create a valid monitored task.
 *
 * @details    In order for the code under test to execute successfully, there
 *             must exist a task that is monitored by the Unit Watchdog
 *             Framework. The monitored task is created through @ref
 *             createdefaultMonitoredTaskStateTracked and updates the Unit
 *             Watchdog Framework's state to believe it is monitoring that task.
 *             The monitored task is set to have @ref LW_U8_MAX items in its
 *             queue.
 *
 *             The methods @ref s_lwosWatchdogGetTaskInfo and @ref
 *             lwrtosHookError are called with their actual implementations.
 *
 *             The method @ref lwrtosTaskGetLwrrentTaskHandle is mocked.
 */
PRE_TEST_METHOD(addItemFromISR_IlwalidItemCount)
{
    createdefaultMonitoredTaskStateTracked();

    pWatchdogTaskInfo = &defaultMonitoredTaskState;
    WatchdogActiveTaskMask = 1U;

    defaultMonitoredTaskState.itemsInQueue = 255U;
}

/*!
 * @brief      Reset the Unit Watchdog Framework state.
 *
 * @details    Reset the Unit so that no tasks are monitored, and the global
 *             task used for testing is reset back to its original state.
 */
POST_TEST_METHOD(addItemFromISR_IlwalidItemCount)
{
    pWatchdogTaskInfo = NULL;
    WatchdogActiveTaskMask = 0U;

    resetdefaultMonitoredTaskStateTracked();
}

/*!
 * @brief      Test that the Unit Watchdog Framework detects when attempting to
 *             add an item from an ISR to a task with too many items.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework method to add
 *             an item to a monitored task from an ISR.
 *
 *             Parameters: The task ID shall be 0, which is valid because only
 *             one task is set to be monitored by the pre-test setup.
 *
 *             Passing criteria: The test passes if the return value is @ref
 *             FLCN_ERR_ILLEGAL_OPERATION , the code attempted to halt the core
 *             once, and the task has @ref LW_U8_MAX items in its queue.
 *
 *             If any of the passing criteria are false, the test fails.
 */
UT_CASE_RUN(LWOSWATCHDOG, addItemFromISR_IlwalidItemCount)
{
    const LwU8 taskId = 0U;
    FLCN_STATUS status;

    PRE_TEST_NAME(addItemFromISR_IlwalidItemCount)();

    status = lwosWatchdogAddItemFromISR(taskId);

    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILLEGAL_OPERATION);
    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_HALT_COUNT(), 1U);
    UT_ASSERT_EQUAL_UINT(defaultMonitoredTaskState.itemsInQueue, LW_U8_MAX);

    POST_TEST_NAME(addItemFromISR_IlwalidItemCount)();
}
/* -------------------------------------------------------- */
/*!
 * Ilwoke the Unit Watchdog Framework method exposed to tasks for adding an item
 * from an ISR with an invalid task ID.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, addItemFromISR_IlwalidTaskID,
  UT_CASE_SET_DESCRIPTION("Ilwoke the Unit Watchdog Framework method exposed to tasks for adding an item from an ISR with an invalid task ID.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Create a valid monitored task.
 *
 * @details    In order for the code under test to execute successfully, there
 *             must exist a task that is monitored by the Unit Watchdog
 *             Framework. The monitored task is created through @ref
 *             createdefaultMonitoredTaskStateTracked and updates the Unit
 *             Watchdog Framework's state to believe it is monitoring that task.
 *
 *             The methods @ref s_lwosWatchdogGetTaskInfo and @ref
 *             lwrtosHookError are called with their actual implementations.
 *
 *             The method @ref lwrtosTaskGetLwrrentTaskHandle is mocked.
 */
PRE_TEST_METHOD(addItemFromISR_IlwalidTaskID)
{
    createdefaultMonitoredTaskStateTracked();

    pWatchdogTaskInfo = &defaultMonitoredTaskState;
    WatchdogActiveTaskMask = 1U;
}

/*!
 * @brief      Reset the Unit Watchdog Framework state.
 *
 * @details    Reset the Unit so that no tasks are monitored, and the global
 *             task used for testing is reset back to its original state.
 */
POST_TEST_METHOD(addItemFromISR_IlwalidTaskID)
{
    pWatchdogTaskInfo = NULL;
    WatchdogActiveTaskMask = 0U;

    resetdefaultMonitoredTaskStateTracked();
}

/*!
 * @brief      Test that the Unit Watchdog Framework detects when attempting to
 *             add an item from an ISR when provided an invalid task ID.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework method to add
 *             an item to a monitored task from an ISR.
 *
 *             Parameters: The task ID shall be 90, which is invalid.
 *
 *             Passing criteria: The test passes if the return value is @ref
 *             FLCN_ERR_ILWALID_ARGUMENT.
 *
 *             If any of the passing criteria are false, the test fails.
 */
UT_CASE_RUN(LWOSWATCHDOG, addItemFromISR_IlwalidTaskID)
{
    const LwU8 taskId = 90U;
    FLCN_STATUS status;

    PRE_TEST_NAME(addItemFromISR_IlwalidTaskID)();

    status = lwosWatchdogAddItemFromISR(taskId);

    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_ARGUMENT);

    POST_TEST_NAME(addItemFromISR_IlwalidTaskID)();
}
/* -------------------------------------------------------- */
/*!
 * Ilwoke the Unit Watchdog Framework method exposed to tasks for adding an item
 * with valid parameters.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, addItem_Valid,
  UT_CASE_SET_DESCRIPTION("Ilwoke the Unit Watchdog Framework method exposed to tasks for adding an item with valid parameters.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Create a valid monitored task.
 *
 * @details    In order for the code under test to execute successfully, there
 *             must exist a task that is monitored by the Unit Watchdog
 *             Framework. The monitored task is created through @ref
 *             createdefaultMonitoredTaskStateTracked and updates the Unit
 *             Watchdog Framework's state to believe it is monitoring that task.
 *             The monitored task is set to have 1 item in its queue.
 *
 *             The method @ref lwosWatchdogAddItemFromISR is called with its
 *             actual implementation.
 *
 *             The methods @ref lwrtosENTER_CRITICAL and @ref
 *             lwrtosEXIT_CRITICAL are mocked.
 */
PRE_TEST_METHOD(addItem_Valid)
{
    createdefaultMonitoredTaskStateTracked();

    pWatchdogTaskInfo = &defaultMonitoredTaskState;
    WatchdogActiveTaskMask = 1U;

    defaultMonitoredTaskState.itemsInQueue = 1U;
}

/*!
 * @brief      Reset the Unit Watchdog Framework state.
 *
 * @details    Reset the Unit so that no tasks are monitored, and the global
 *             task used for testing is reset back to its original state.
 */
POST_TEST_METHOD(addItem_Valid)
{
    pWatchdogTaskInfo = NULL;
    WatchdogActiveTaskMask = 0U;

    resetdefaultMonitoredTaskStateTracked();
}

/*!
 * @brief      Test that a monitored task's state updates correctly when adding
 *             an item from an ISR.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework method to add
 *             an item to a monitored task.
 *
 *             Parameters: The task ID shall be 0, which is valid because only
 *             one task is set to be monitored by the pre-test setup.
 *
 *             Passing criteria: The test passes if the return value is @ref
 *             FLCN_OK and the task has 2 items in its queue.
 *
 *             If any of the passing criteria are false, the test fails.
 */
UT_CASE_RUN(LWOSWATCHDOG, addItem_Valid)
{
    const LwU8 taskId = 0U;
    FLCN_STATUS status;

    PRE_TEST_NAME(addItem_Valid)();

    status = lwosWatchdogAddItem(taskId);

    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_UINT(defaultMonitoredTaskState.itemsInQueue, 2U);

    POST_TEST_NAME(addItem_Valid)();
}
/* -------------------------------------------------------- */
/*!
 * Ilwoke the Unit Watchdog Framework method exposed to tasks for adding an item
 * with an invalid task ID.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, addItem_IlwalidTaskID,
  UT_CASE_SET_DESCRIPTION("Ilwoke the Unit Watchdog Framework method exposed to tasks for adding an item with an invalid task ID.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Create a valid monitored task.
 *
 * @details    In order for the code under test to execute successfully, there
 *             must exist a task that is monitored by the Unit Watchdog
 *             Framework. The monitored task is created through @ref
 *             createdefaultMonitoredTaskStateTracked and updates the Unit
 *             Watchdog Framework's state to believe it is monitoring that task.
 *
 *             The method @ref lwosWatchdogAddItemFromISR is called with its
 *             actual implementation.
 *
 *             The methods @ref lwrtosENTER_CRITICAL and @ref
 *             lwrtosEXIT_CRITICAL are mocked.
 */
PRE_TEST_METHOD(addItem_IlwalidTaskID)
{
    createdefaultMonitoredTaskStateTracked();

    pWatchdogTaskInfo = &defaultMonitoredTaskState;
    WatchdogActiveTaskMask = 1U;
}

/*!
 * @brief      Reset the Unit Watchdog Framework state.
 *
 * @details    Reset the Unit so that no tasks are monitored, and the global
 *             task used for testing is reset back to its original state.
 */
POST_TEST_METHOD(addItem_IlwalidTaskID)
{
    pWatchdogTaskInfo = NULL;
    WatchdogActiveTaskMask = 0U;

    resetdefaultMonitoredTaskStateTracked();
}

/*!
 * @brief      Test that the Unit Watchdog Framework detects when attempting to
 *             add an item when provided an invalid task ID.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework method to add
 *             an item to a monitored task.
 *
 *             Parameters: The task ID shall be 90, which is invalid.
 *
 *             Passing criteria: The test passes if the return value is @ref
 *             FLCN_ERR_ILWALID_ARGUMENT.
 *
 *             If any of the passing criteria are false, the test fails.
 */
UT_CASE_RUN(LWOSWATCHDOG, addItem_IlwalidTaskID)
{
    const LwU8 taskId = 90U;
    FLCN_STATUS status;

    PRE_TEST_NAME(addItem_IlwalidTaskID)();

    status = lwosWatchdogAddItem(taskId);

    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_ARGUMENT);

    POST_TEST_NAME(addItem_IlwalidTaskID)();
}
/* -------------------------------------------------------- */
/*!
 * Ilwoke the Unit Watchdog Framework method to check the status of all
 * monitored tasks, with a task that is in the completed state.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, checkStatus_ValidCompleted,
  UT_CASE_SET_DESCRIPTION("Ilwoke the Unit Watchdog Framework method to check the status of all monitored tasks, with a task that is in the completed state.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Create a valid monitored task that has completed work on an item.
 *
 * @details    In order for the code under test to execute successfully, there
 *             must exist a task that is monitored by the Unit Watchdog
 *             Framework that has completed work and has items in its queue. The
 *             monitored task is initialized by @ref
 *             createdefaultMonitoredTaskStateTracked and the pre-test method
 *             updates the Unit Watchdog Framework's state to believe it is
 *             monitoring that task. The monitored task data is modified so that
 *             there is 1 item in the queue, that the task timeout is 10, and
 *             the task is in the COMPLETED state.
 *
 *             Additionally, the pre-test method mocks
 *             @ref osPTimerTimeNsLwrrentGet for retrieving the current system
 *             time. It is mocked to return a fixed value when called by the code
 *             under test.
 *
 *             The methods @ref s_lwosWatchdogGetTaskInfo is called with its
 *             actual implementations.
 */
PRE_TEST_METHOD(checkStatus_ValidCompleted)
{
    static FLCN_TIMESTAMP checkStatus_ValidCompleted_PTimerValues[] =
    {
        {
            .data = 0x1U,
        },
    };

    createdefaultMonitoredTaskStateTracked();

    pWatchdogTaskInfo = &defaultMonitoredTaskState;
    WatchdogActiveTaskMask = 1U;

    defaultMonitoredTaskState.itemsInQueue = 1U;
    defaultMonitoredTaskState.lastItemStartTime = 0U;
    defaultMonitoredTaskState.taskTimeoutNs = 10U;
    defaultMonitoredTaskState.taskStatus = LWBIT32(LWOS_WATCHDOG_ITEM_COMPLETED);

    osPTimerMockInit();
    osPTimerTimeNsLwrrentGet_MOCK_fake.lwstom_fake =
        osPTimerTimeNsLwrrentGet_MOCK_Lwstom;
    pPTimerTimeNsLwrrentGetReturnSequence =
        checkStatus_ValidCompleted_PTimerValues;
}

/*!
 * @brief      Reset the Unit Watchdog Framework state.
 *
 * @details    Reset the Unit so that no tasks are monitored, and the global
 *             task used for testing is reset back to its original state. The
 *             mocked registers are automatically reset by the UTF.
 */
POST_TEST_METHOD(checkStatus_ValidCompleted)
{
    pWatchdogTaskInfo = NULL;
    WatchdogActiveTaskMask = 0U;

    resetdefaultMonitoredTaskStateTracked();
}

/*!
 * @brief      Test that the Unit Watchdog Framework assesses the status of the
 *             monitored task correctly.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework method that
 *             checks the status of the monitored task in the COMPLETED state,
 *             and updates the task's state correctly.
 *
 *             Parameters: The pointer shall be a valid stack address.
 *
 *             Passing criteria: The test passes if the return value is @ref
 *             FLCN_OK , the monitored task's state is updated to WAITING, and
 *             the pointer has no value written to it.
 *
 *             If any of the passing criteria are false, the test fails.
 */
UT_CASE_RUN(LWOSWATCHDOG, checkStatus_ValidCompleted)
{
    LwU32 failingTask = 0U;
    FLCN_STATUS status;

    PRE_TEST_NAME(checkStatus_ValidCompleted)();

    status = lwosWatchdogCheckStatus(&failingTask);

    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_UINT(defaultMonitoredTaskState.taskStatus, LWBIT32(LWOS_WATCHDOG_TASK_WAITING));
    UT_ASSERT_EQUAL_UINT(failingTask, 0x0U);

    POST_TEST_NAME(checkStatus_ValidCompleted)();
}
/* -------------------------------------------------------- */
/*!
 * Ilwoke the Unit Watchdog Framework method to check the status of all
 * monitored tasks, with a task that is in the waiting state.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, checkStatus_ValidWaiting,
  UT_CASE_SET_DESCRIPTION("Ilwoke the Unit Watchdog Framework method to check the status of all monitored tasks, with a task that is in the completed state.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Create a valid monitored task that is in the waiting state.
 *
 * @details    In order for the code under test to execute successfully, there
 *             must exist a task that is monitored by the Unit Watchdog
 *             Framework that is in the waiting state and has items in its queue. The
 *             monitored task is initialized by @ref
 *             createdefaultMonitoredTaskStateTracked and the pre-test method
 *             updates the Unit Watchdog Framework's state to believe it is
 *             monitoring that task. The monitored task data is modified so that
 *             there is 1 item in the queue, that the task timeout is 10, and
 *             the task is in the WAITING state.
 *
 *             Additionally, the pre-test method mocks
 *             @ref osPTimerTimeNsLwrrentGet for retrieving the current system
 *             time. It is mocked to return a fixed value when called by the code
 *             under test.
 *
 *             The methods @ref s_lwosWatchdogGetTaskInfo is called with its
 *             actual implementations.
 */
PRE_TEST_METHOD(checkStatus_ValidWaiting)
{
    static FLCN_TIMESTAMP checkStatus_ValidWaiting_PTimerValues[] =
    {
        {
            .data = 0x1U,
        },
    };

    createdefaultMonitoredTaskStateTracked();

    pWatchdogTaskInfo = &defaultMonitoredTaskState;
    WatchdogActiveTaskMask = 1U;

    defaultMonitoredTaskState.itemsInQueue = 1U;
    defaultMonitoredTaskState.lastItemStartTime = 0U;
    defaultMonitoredTaskState.taskTimeoutNs = 10U;
    defaultMonitoredTaskState.taskStatus = LWBIT32(LWOS_WATCHDOG_TASK_WAITING);

    osPTimerMockInit();
    osPTimerTimeNsLwrrentGet_MOCK_fake.lwstom_fake =
        osPTimerTimeNsLwrrentGet_MOCK_Lwstom;
    pPTimerTimeNsLwrrentGetReturnSequence =
        checkStatus_ValidWaiting_PTimerValues;
}

/*!
 * @brief      Reset the Unit Watchdog Framework state.
 *
 * @details    Reset the Unit so that no tasks are monitored, and the global
 *             task used for testing is reset back to its original state. The
 *             mocked registers are automatically reset by the UTF.
 */
POST_TEST_METHOD(checkStatus_ValidWaiting)
{
    pWatchdogTaskInfo = NULL;
    WatchdogActiveTaskMask = 0U;

    resetdefaultMonitoredTaskStateTracked();
}

/*!
 * @brief      Test that the Unit Watchdog Framework assesses the status of the
 *             monitored task correctly.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework method that
 *             checks the status of the monitored task in the WAITING state,
 *             and updates the task's state correctly.
 *
 *             Parameters: The pointer shall be a valid stack address.
 *
 *             Passing criteria: The test passes if the return value is @ref
 *             FLCN_OK , the monitored task's state shall remain WAITING,
 *             and the pointer has no value written to it.
 *
 *             If any of the passing criteria are false, the test fails.
 */
UT_CASE_RUN(LWOSWATCHDOG, checkStatus_ValidWaiting)
{
    LwU32 failingTask = 0U;
    FLCN_STATUS status;

    PRE_TEST_NAME(checkStatus_ValidWaiting)();

    status = lwosWatchdogCheckStatus(&failingTask);

    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_UINT(defaultMonitoredTaskState.taskStatus, LWBIT32(LWOS_WATCHDOG_TASK_WAITING));
    UT_ASSERT_EQUAL_UINT(failingTask, 0x0U);

    POST_TEST_NAME(checkStatus_ValidWaiting)();
}

/* -------------------------------------------------------- */
/*!
 * Ilwoke the Unit Watchdog Framework method to check the status of all
 * monitored tasks, with a task that has exceeded its time limit working on an
 * item with a valid pointer parameter.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, checkStatus_IlwalidTimeoutValidPointer,
  UT_CASE_SET_DESCRIPTION("Ilwoke the Unit Watchdog Framework method to check the status of all monitored tasks, with a task that has exceeded its time limit working on an item.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Create a valid monitored task that is working on an item and
 *             exceeded its timeout.
 *
 * @details    In order for the code under test to execute successfully, there
 *             must exist a task that is monitored by the Unit Watchdog
 *             Framework that is processing a work item and has items in its
 *             queue. The monitored task is initialized by @ref
 *             createdefaultMonitoredTaskStateTracked and the pre-test method
 *             updates the Unit Watchdog Framework's state to believe it is
 *             monitoring that task. The monitored task data is modified so that
 *             there is 1 item in the queue, that the task timeout is 10, and
 *             the task is in the STARTED state.
 *
 *             Additionally, the pre-test method mocks
 *             @ref osPTimerTimeNsLwrrentGet for retrieving the current system
 *             time. It is mocked to return a fixed value when called by the code
 *             under test.
 *
 *             The methods @ref s_lwosWatchdogGetTaskInfo is called with its
 *             actual implementations.
 */
PRE_TEST_METHOD(checkStatus_IlwalidTimeoutValidPointer)
{
    static FLCN_TIMESTAMP checkStatus_IlwalidTimeoutValidPointer_PTimerValues[] =
    {
        {
            .data = 0x100U,
        },
    };

    createdefaultMonitoredTaskStateTracked();

    WatchdogActiveTaskMask = 1U;
    pWatchdogTaskInfo = &defaultMonitoredTaskState;

    defaultMonitoredTaskState.itemsInQueue = 1U;
    defaultMonitoredTaskState.lastItemStartTime = 0U;
    defaultMonitoredTaskState.taskTimeoutNs = 10U;
    defaultMonitoredTaskState.taskStatus = LWBIT32(LWOS_WATCHDOG_ITEM_STARTED);

    osPTimerMockInit();
    osPTimerTimeNsLwrrentGet_MOCK_fake.lwstom_fake =
        osPTimerTimeNsLwrrentGet_MOCK_Lwstom;
    pPTimerTimeNsLwrrentGetReturnSequence =
        checkStatus_IlwalidTimeoutValidPointer_PTimerValues;
}

/*!
 * @brief      Reset the Unit Watchdog Framework state.
 *
 * @details    Reset the Unit so that no tasks are monitored, and the global
 *             task used for testing is reset back to its original state. The
 *             mocked registers are automatically reset by the UTF.
 */
POST_TEST_METHOD(checkStatus_IlwalidTimeoutValidPointer)
{
    pWatchdogTaskInfo = NULL;
    WatchdogActiveTaskMask = 0U;
    resetdefaultMonitoredTaskStateTracked();
}

/*!
 * @brief      Test that the Unit Watchdog Framework assesses the status of the
 *             monitored task correctly.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework method that
 *             checks the status of the monitored task in the STARTED state and
 *             updates the task's state correctly, given that it has violated
 *             its timeout.
 *
 *             Parameters: The pointer shall be a valid stack address, which has
 *             the value 0x10 so that when the code under test writes to it, the
 *             test can verify only the correct bit was set.
 *
 *             Passing criteria: The test passes if the return value is @ref
 *             FLCN_ERR_TIMEOUT , the monitored task's state is updated to both
 *             STARTED and TIMEOUT, and the pointer has the first bit set,
 *             indicating the monitored task is failing.
 *
 *             If any of the passing criteria are false, the test fails.
 */
UT_CASE_RUN(LWOSWATCHDOG, checkStatus_IlwalidTimeoutValidPointer)
{
    LwU32 failingTask = 0x10U;
    FLCN_STATUS status;

    PRE_TEST_NAME(checkStatus_IlwalidTimeoutValidPointer)();

    status = lwosWatchdogCheckStatus(&failingTask);

    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_TIMEOUT);
    UT_ASSERT_EQUAL_UINT(defaultMonitoredTaskState.taskStatus, LWBIT32(LWOS_WATCHDOG_ITEM_STARTED) | LWBIT32(LWOS_WATCHDOG_ITEM_TIMEOUT));
    UT_ASSERT_EQUAL_UINT(failingTask, 0x11U);

    POST_TEST_NAME(checkStatus_IlwalidTimeoutValidPointer)();
}

/* -------------------------------------------------------- */
/*!
 * Ilwoke the Unit Watchdog Framework method to check the status of all
 * monitored tasks, with a task that has exceeded its time limit working on an
 * item with an invalid pointer parameter.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, checkStatus_IlwalidTimeoutIlwalidPointer,
  UT_CASE_SET_DESCRIPTION("Ilwoke the Unit Watchdog Framework method to check the status of all monitored tasks, with a task that has exceeded its time limit working on an item with an invalid pointer.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Create a valid monitored task that is working on an item and
 *             exceeded its timeout.
 *
 * @details    In order for the code under test to execute successfully, there
 *             must exist a task that is monitored by the Unit Watchdog
 *             Framework that is processing a work item and has items in its
 *             queue. The monitored task is initialized by @ref
 *             createdefaultMonitoredTaskStateTracked and the pre-test method
 *             updates the Unit Watchdog Framework's state to believe it is
 *             monitoring that task. The monitored task data is modified so that
 *             there is 1 item in the queue, that the task timeout is 10, and
 *             the task is in the STARTED state.
 *
 *             Additionally, the pre-test method mocks
 *             @ref osPTimerTimeNsLwrrentGet for retrieving the current system
 *             time. It is mocked to return a fixed value when called by the code
 *             under test.
 *
 *             The methods @ref s_lwosWatchdogGetTaskInfo is called with its
 *             actual implementations.
 */
PRE_TEST_METHOD(checkStatus_IlwalidTimeoutIlwalidPointer)
{
    static FLCN_TIMESTAMP checkStatus_IlwalidTimeoutIlwalidPointer_PTimerValues[] =
    {
        {
            .data = 0x100U,
        },
    };

    createdefaultMonitoredTaskStateTracked();

    pWatchdogTaskInfo = &defaultMonitoredTaskState;
    WatchdogActiveTaskMask = 1U;

    defaultMonitoredTaskState.itemsInQueue = 1U;
    defaultMonitoredTaskState.lastItemStartTime = 0U;
    defaultMonitoredTaskState.taskTimeoutNs = 10U;
    defaultMonitoredTaskState.taskStatus = LWBIT32(LWOS_WATCHDOG_ITEM_STARTED);

    osPTimerMockInit();
    osPTimerTimeNsLwrrentGet_MOCK_fake.lwstom_fake =
        osPTimerTimeNsLwrrentGet_MOCK_Lwstom;
    pPTimerTimeNsLwrrentGetReturnSequence =
        checkStatus_IlwalidTimeoutIlwalidPointer_PTimerValues;
}

/*!
 * @brief      Reset the Unit Watchdog Framework state.
 *
 * @details    Reset the Unit so that no tasks are monitored, and the global
 *             task used for testing is reset back to its original state. The
 *             mocked registers are automatically reset by the UTF.
 */
POST_TEST_METHOD(checkStatus_IlwalidTimeoutIlwalidPointer)
{
    pWatchdogTaskInfo = NULL;
    WatchdogActiveTaskMask = 0U;
    resetdefaultMonitoredTaskStateTracked();
}

/*!
 * @brief      Test that the Unit Watchdog Framework assesses the status of the
 *             monitored task correctly.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework method that
 *             checks the status of the monitored task in the STARTED state and
 *             updates the task's state correctly, given that it has violated
 *             its timeout, while not crashing due to a NULL pointer being
 *             provided.
 *
 *             Parameters: The pointer shall be NULL.
 *
 *             Passing criteria: The test passes if the return value is @ref
 *             FLCN_ERR_TIMEOUT and the monitored task's state is updated to
 *             both STARTED and TIMEOUT.
 *
 *             If any of the passing criteria are false, the test fails.
 */
UT_CASE_RUN(LWOSWATCHDOG, checkStatus_IlwalidTimeoutIlwalidPointer)
{
    FLCN_STATUS status;

    PRE_TEST_NAME(checkStatus_IlwalidTimeoutIlwalidPointer)();

    status = lwosWatchdogCheckStatus(NULL);

    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_TIMEOUT);
    UT_ASSERT_EQUAL_UINT(defaultMonitoredTaskState.taskStatus, LWBIT32(LWOS_WATCHDOG_ITEM_STARTED) | LWBIT32(LWOS_WATCHDOG_ITEM_TIMEOUT));

    POST_TEST_NAME(checkStatus_IlwalidTimeoutIlwalidPointer)();
}

/* -------------------------------------------------------- */
/*!
 * Ilwoke the Unit Watchdog Framework method to check the status of all
 * monitored tasks, with a task that has no pending items.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, checkStatus_ValidNoItems,
  UT_CASE_SET_DESCRIPTION("Ilwoke the Unit Watchdog Framework method to check the status of all monitored tasks, with a task that has no pending items.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Create a valid monitored task that has no pending items.
 *
 * @details    In order for the code under test to execute successfully, there
 *             must exist a task that is monitored by the Unit Watchdog
 *             Framework that has no pending items. The monitored task is
 *             initialized by @ref createdefaultMonitoredTaskStateTracked and
 *             the pre-test method updates the Unit Watchdog Framework's state
 *             to believe it is monitoring that task. The monitored task data is
 *             modified so that there is no item in the queue, that the task
 *             timeout is 10, and the task is in the STARTED state.
 *
 *             Additionally, the pre-test method mocks
 *             @ref osPTimerTimeNsLwrrentGet for retrieving the current system
 *             time. It is mocked to return a fixed value when called by the code
 *             under test.
 *
 *             The methods @ref s_lwosWatchdogGetTaskInfo is called with its
 *             actual implementations.
 */
PRE_TEST_METHOD(checkStatus_ValidNoItems)
{
    static FLCN_TIMESTAMP checkStatus_ValidNoItems_PTimerValues[] =
    {
        {
            .data = 0x1U,
        },
    };

    createdefaultMonitoredTaskStateTracked();

    WatchdogActiveTaskMask = 1U;
    pWatchdogTaskInfo = &defaultMonitoredTaskState;

    defaultMonitoredTaskState.itemsInQueue = 0U;
    defaultMonitoredTaskState.lastItemStartTime = 0U;
    defaultMonitoredTaskState.taskTimeoutNs = 10U;
    defaultMonitoredTaskState.taskStatus = LWBIT32(LWOS_WATCHDOG_ITEM_STARTED);

    osPTimerMockInit();
    osPTimerTimeNsLwrrentGet_MOCK_fake.lwstom_fake =
        osPTimerTimeNsLwrrentGet_MOCK_Lwstom;
    pPTimerTimeNsLwrrentGetReturnSequence =
        checkStatus_ValidNoItems_PTimerValues;
}

/*!
 * @brief      Reset the Unit Watchdog Framework state.
 *
 * @details    Reset the Unit so that no tasks are monitored, and the global
 *             task used for testing is reset back to its original state. The
 *             mocked registers are automatically reset by the UTF.
 */
POST_TEST_METHOD(checkStatus_ValidNoItems)
{
    pWatchdogTaskInfo = NULL;
    WatchdogActiveTaskMask = 0U;
    resetdefaultMonitoredTaskStateTracked();
}

/*!
 * @brief      Test that the Unit Watchdog Framework assesses the status of the
 *             monitored task correctly.
 *
 * @details    The test shall ilwoke the Unit Watchdog Framework method that
 *             checks the status of the monitored task that has no pending
 *             items, and updates the task's status correctly.
 *
 *             Parameters: The pointer shall be a valid stack address.
 *
 *             Passing criteria: The test passes if the return value is @ref
 *             FLCN_OK , the monitored task's state shall be WAITING, and the
 *             pointer has no value written to it.
 *
 *             If any of the passing criteria are false, the test fails.
 */
UT_CASE_RUN(LWOSWATCHDOG, checkStatus_ValidNoItems)
{
    LwU32 failingTask = 0x100U;
    FLCN_STATUS status;

    PRE_TEST_NAME(checkStatus_ValidNoItems)();

    status = lwosWatchdogCheckStatus(&failingTask);

    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_UINT(defaultMonitoredTaskState.taskStatus, LWBIT32(LWOS_WATCHDOG_TASK_WAITING));
    UT_ASSERT_EQUAL_UINT(failingTask, 0x100U);

    POST_TEST_NAME(checkStatus_ValidNoItems)();
}

/* -------------------------------------------------------- */
/*!
 * Ilwoke the public Unit Watchdog Framework method for a task to complete a
 * work item.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, attachAndComplete_Valid,
  UT_CASE_SET_DESCRIPTION("Ilwoke the public Unit Watchdog Framework method for a task to complete a work item.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Create a valid monitored task that can complete work on an item.
 *
 * @details    In order for the code under test to execute successfully, there
 *             must exist a task that is monitored by the Unit Watchdog
 *             Framework that has started work and is able to complete it. The
 *             monitored task is created through @ref
 *             createdefaultMonitoredTaskStateTracked and the pre-test method
 *             updates the Unit Watchdog Framework's state to believe it is
 *             monitoring that task. The monitored task data is modified so that
 *             there is 1 item in the queue, that the task timeout is 10.
 *
 *             Additionally, the pre-test method mocks the two HW registers used
 *             for retrieving the current system time. The registers are mocked
 *             to return a fixed value when called by the code under test. The
 *             return value is chosen to be 1 so that the task "completes" the
 *             work item within its time limit.
 *
 *             The method @ref s_lwosWatchdogCompleteItem is called with its
 *             actual implementation.
 *
 *             The methods @ref osTaskIdGet and @ref lwosTaskOverlayDescListExec
 *             are mocked.
 */
PRE_TEST_METHOD(attachAndComplete_Valid)
{
    static FLCN_TIMESTAMP attachAndComplete_Valid_PTimerValues[] =
    {
        {
            .data = 0x1U,
        },
    };

    createdefaultMonitoredTaskStateTracked();

    pWatchdogTaskInfo = &defaultMonitoredTaskState;
    WatchdogActiveTaskMask = 1U;

    defaultMonitoredTaskState.itemsInQueue = 1U;
    defaultMonitoredTaskState.lastItemStartTime = 0U;
    defaultMonitoredTaskState.taskTimeoutNs = 10U;

    osPTimerMockInit();
    osPTimerTimeNsLwrrentGet_MOCK_fake.lwstom_fake =
        osPTimerTimeNsLwrrentGet_MOCK_Lwstom;
    pPTimerTimeNsLwrrentGetReturnSequence =
        attachAndComplete_Valid_PTimerValues;
}

/*!
 * @brief      Reset the Unit Watchdog Framework state.
 *
 * @details    Reset the Unit so that no tasks are monitored, and the global
 *             task used for testing is reset back to its original state. The
 *             mocked registers are automatically reset by the UTF.
 */
POST_TEST_METHOD(attachAndComplete_Valid)
{
    pWatchdogTaskInfo = NULL;
    WatchdogActiveTaskMask = 0U;
    resetdefaultMonitoredTaskStateTracked();
}

/*!
 * @brief      Test that a monitored task's state updates correctly when
 *             completing an item.
 *
 * @details    The test shall ilwoke the public Unit Watchdog Framework method
 *             to update a monitored task when it completes work on an item.
 *
 *             Parameters: None.
 *
 *             Passing criteria: The test passes if the return value is @ref
 *             FLCN_OK.
 *
 *             If any of the passing criteria are false, the test fails.
 */
UT_CASE_RUN(LWOSWATCHDOG, attachAndComplete_Valid)
{
    FLCN_STATUS status;

    PRE_TEST_NAME(attachAndComplete_Valid)();

    status = lwosWatchdogAttachAndCompleteItem();

    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);

    POST_TEST_NAME(attachAndComplete_Valid)();
}

/* -------------------------------------------------------- */
/*!
 * Ilwoke the public Unit Watchdog Framework method for a task to complete a
 * work item.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, attachAndComplete_Ilwalid,
  UT_CASE_SET_DESCRIPTION("Ilwoke the public Unit Watchdog Framework method for a task to complete a work item.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Create a valid monitored task that cannot complete work on an item.
 *
 * @details    In order for the code under test to execute successfully, there
 *             must exist a task that is monitored by the Unit Watchdog
 *             Framework that has no work items. The monitored task is created
 *             through @ref createdefaultMonitoredTaskStateTracked and the
 *             pre-test method updates the Unit Watchdog Framework's state to
 *             believe it is monitoring that task. The monitored task data is
 *             modified so that there is no items in the queue, that the task
 *             timeout is 10.
 *
 *             Additionally, the pre-test method mocks the two HW registers used
 *             for retrieving the current system time. The registers are mocked
 *             to return a fixed value when called by the code under test. The
 *             return value is chosen to be 1.
 *
 *             The method @ref s_lwosWatchdogCompleteItem is called with its
 *             actual implementation.
 *
 *             The methods @ref osTaskIdGet and @ref lwosTaskOverlayDescListExec
 *             are mocked.
 */
PRE_TEST_METHOD(attachAndComplete_Ilwalid)
{
    static FLCN_TIMESTAMP attachAndComplete_Ilwalid_PTimerValues[] =
    {
        {
            .data = 0x1U,
        },
    };

    createdefaultMonitoredTaskStateTracked();

    WatchdogActiveTaskMask = 1U;
    pWatchdogTaskInfo = &defaultMonitoredTaskState;

    defaultMonitoredTaskState.itemsInQueue = 0U;
    defaultMonitoredTaskState.lastItemStartTime = 0U;
    defaultMonitoredTaskState.taskTimeoutNs = 10U;

    osPTimerMockInit();
    osPTimerTimeNsLwrrentGet_MOCK_fake.lwstom_fake =
        osPTimerTimeNsLwrrentGet_MOCK_Lwstom;
    pPTimerTimeNsLwrrentGetReturnSequence =
        attachAndComplete_Ilwalid_PTimerValues;
}

/*!
 * @brief      Reset the Unit Watchdog Framework state.
 *
 * @details    Reset the Unit so that no tasks are monitored, and the global
 *             task used for testing is reset back to its original state. The
 *             mocked registers are automatically reset by the UTF.
 */
POST_TEST_METHOD(attachAndComplete_Ilwalid)
{
    pWatchdogTaskInfo = NULL;
    WatchdogActiveTaskMask = 0U;
    resetdefaultMonitoredTaskStateTracked();
}

/*!
 * @brief      Test that the Unit Watchdog Framework detects when a task is
 *             attempting to complete an item without having any items.
 *
 * @details    The test shall ilwoke the public Unit Watchdog Framework method
 *             to update a monitored task when it completes work on an item.
 *
 *             Parameters: None.
 *
 *             Passing criteria: The test passes if the return value is @ref
 *             FLCN_ERR_ILLEGAL_OPERATION and the code under test has attempted
 *             to halt the core once.
 *
 *             If any of the passing criteria are false, the test fails.
 */
UT_CASE_RUN(LWOSWATCHDOG, attachAndComplete_Ilwalid)
{
    FLCN_STATUS status;

    PRE_TEST_NAME(attachAndComplete_Ilwalid)();

    status = lwosWatchdogAttachAndCompleteItem();

    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILLEGAL_OPERATION);
    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_HALT_COUNT(), 1U);

    POST_TEST_NAME(attachAndComplete_Ilwalid)();
}

/* -------------------------------------------------------- */
/*!
 * Ilwoke the public Unit Watchdog Framework method for a task to start a
 * work item.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, attachAndStart_Valid,
  UT_CASE_SET_DESCRIPTION("Ilwoke the public Unit Watchdog Framework method for a task to start a work item.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Create a valid monitored task that can start work on an item.
 *
 * @details    In order for the code under test to execute successfully, there
 *             must exist a task that is monitored by the Unit Watchdog
 *             Framework that can start work on a pending item. The monitored
 *             task is created through @ref
 *             createdefaultMonitoredTaskStateTracked and the pre-test method
 *             updates the Unit Watchdog Framework's state to believe it is
 *             monitoring that task. The monitored task data is modified so that
 *             there is 1 item in the queue, that the task timeout is 10.
 *
 *             Additionally, the pre-test method mocks the two HW registers used
 *             for retrieving the current system time. The registers are mocked
 *             to return a fixed value when called by the code under test. The
 *             return value is chosen to be 1.
 *
 *             The method @ref s_lwosWatchdogStartItem is called with its actual
 *             implementation.
 *
 *             The methods @ref osTaskIdGet and @ref lwosTaskOverlayDescListExec
 *             are mocked.
 */
PRE_TEST_METHOD(attachAndStart_Valid)
{
    static FLCN_TIMESTAMP attachAndStart_Valid_PTimerValues[] =
    {
        {
            .data = 0x1U,
        },
    };

    createdefaultMonitoredTaskStateTracked();
    pWatchdogTaskInfo = &defaultMonitoredTaskState;
    WatchdogActiveTaskMask = 1U;

    defaultMonitoredTaskState.itemsInQueue = 1U;
    defaultMonitoredTaskState.lastItemStartTime = 0U;
    defaultMonitoredTaskState.taskTimeoutNs = 10U;

    osPTimerMockInit();
    osPTimerTimeNsLwrrentGet_MOCK_fake.lwstom_fake =
        osPTimerTimeNsLwrrentGet_MOCK_Lwstom;
    pPTimerTimeNsLwrrentGetReturnSequence =
        attachAndStart_Valid_PTimerValues;
}

/*!
 * @brief      Reset the Unit Watchdog Framework state.
 *
 * @details    Reset the Unit so that no tasks are monitored, and the global
 *             task used for testing is reset back to its original state. The
 *             mocked registers are automatically reset by the UTF.
 */
POST_TEST_METHOD(attachAndStart_Valid)
{
    pWatchdogTaskInfo = NULL;
    WatchdogActiveTaskMask = 0U;
    resetdefaultMonitoredTaskStateTracked();
}

/*!
 * @brief      Test that a monitored task's state updates correctly when
 *             starting an item.
 *
 * @details    The test shall ilwoke the public Unit Watchdog Framework method
 *             to update a monitored task when it starts work on an item.
 *
 *             Parameters: None.
 *
 *             Passing criteria: The test passes if the return value is @ref
 *             FLCN_OK.
 *
 *             If any of the passing criteria are false, the test fails.
 */
UT_CASE_RUN(LWOSWATCHDOG, attachAndStart_Valid)
{
    FLCN_STATUS status;

    PRE_TEST_NAME(attachAndStart_Valid)();

    status = lwosWatchdogAttachAndStartItem();

    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);

    POST_TEST_NAME(attachAndStart_Valid)();
}

/* -------------------------------------------------------- */
/*!
 * Ilwoke the public Unit Watchdog Framework method for a task to start a work
 * item.
 */
UT_CASE_DEFINE(LWOSWATCHDOG, attachAndStart_Ilwalid,
  UT_CASE_SET_DESCRIPTION("Ilwoke the public Unit Watchdog Framework method for a task to start a work item.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Create a valid monitored task that cannot start work on an item.
 *
 * @details    In order for the code under test to execute successfully, there
 *             must exist a task that is monitored by the Unit Watchdog
 *             Framework that has no work items. The monitored task is created
 *             through @ref createdefaultMonitoredTaskStateTracked and the
 *             pre-test method updates the Unit Watchdog Framework's state to
 *             believe it is monitoring that task. The monitored task data is
 *             modified so that there is no items in the queue, that the task
 *             timeout is 10.
 *
 *             Additionally, the pre-test method mocks the two HW registers used
 *             for retrieving the current system time. The registers are mocked
 *             to return a fixed value when called by the code under test. The
 *             return value is chosen to be 1.
 *
 *             The method @ref s_lwosWatchdogStartItem is called with its actual
 *             implementation.
 *
 *             The methods @ref osTaskIdGet and @ref lwosTaskOverlayDescListExec
 *             are mocked.
 */
PRE_TEST_METHOD(attachAndStart_Ilwalid)
{
    static FLCN_TIMESTAMP attachAndStart_Ilwalid_PTimerValues[] =
    {
        {
            .data = 0x1U,
        },
    };

    createdefaultMonitoredTaskStateTracked();

    WatchdogActiveTaskMask = 1U;
    pWatchdogTaskInfo = &defaultMonitoredTaskState;

    defaultMonitoredTaskState.itemsInQueue = 0U;
    defaultMonitoredTaskState.lastItemStartTime = 0U;
    defaultMonitoredTaskState.taskTimeoutNs = 10U;

    osPTimerMockInit();
    osPTimerTimeNsLwrrentGet_MOCK_fake.lwstom_fake =
        osPTimerTimeNsLwrrentGet_MOCK_Lwstom;
    pPTimerTimeNsLwrrentGetReturnSequence =
        attachAndStart_Ilwalid_PTimerValues;
}

/*!
 * @brief      Reset the Unit Watchdog Framework state.
 *
 * @details    Reset the Unit so that no tasks are monitored, and the global
 *             task used for testing is reset back to its original state. The
 *             mocked registers are automatically reset by the UTF.
 */
POST_TEST_METHOD(attachAndStart_Ilwalid)
{
    pWatchdogTaskInfo = NULL;
    WatchdogActiveTaskMask = 0U;
    resetdefaultMonitoredTaskStateTracked();
}

/*!
 * @brief      Test that the Unit Watchdog Framework detects when a task is
 *             attempting to start an item without having any items.
 *
 * @details    The test shall ilwoke the public Unit Watchdog Framework method
 *             to update a monitored task when it starts work on an item.
 *
 *             Parameters: None.
 *
 *             Passing criteria: The test passes if the return value is @ref
 *             FLCN_ERR_ILLEGAL_OPERATION and the code under test has attempted
 *             to halt the core once.
 *
 *             If any of the passing criteria are false, the test fails.
 */
UT_CASE_RUN(LWOSWATCHDOG, attachAndStart_Ilwalid)
{
    FLCN_STATUS status;

    PRE_TEST_NAME(attachAndStart_Ilwalid)();

    status = lwosWatchdogAttachAndStartItem();

    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILLEGAL_OPERATION);
    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_HALT_COUNT(), 1U);

    POST_TEST_NAME(attachAndStart_Ilwalid)();
}

/* -------------------------------------------------------- */
