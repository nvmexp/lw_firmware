/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    changeseq_daemon-test.c
 * @brief   Unit tests for logic in changeseq_daemon.
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "pmusw.h"

#include "lwos_dma-mock.h"
#include "perf/changeseq.h"
#include "perf/changeseq-mock.h"
#include "perf/changeseq_daemon.h"
#include "perf/changeseq_daemon-mock.h"
#include "task_perf_daemon.h"
#include "task_perf_daemon-mock.h"

/* ------------------------ Function Prototypes ----------------------------- */
static void *testCaseSetup(void);
static void initializeChangeSequencer(CHANGE_SEQ *pChangeSeq);
static void initializeScript(CHANGE_SEQ_SCRIPT *pScript, LwU32 numSteps);
static void initializeNotificationData(RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_DATA *pData);

/* ------------------------ Test Suite Declaration--------------------------- */
/*!
 * @brief The test group for the base change sequencer daemon.
 *
 * @defgroup CHANGESEQ_DAEMON_TEST_SUITE
 */
UT_SUITE_DEFINE(CHANGESEQ_DAEMON,
                UT_SUITE_SET_COMPONENT("Change Sequencer Deamon")
                UT_SUITE_SET_DESCRIPTION("TODO")
                UT_SUITE_SET_OWNER("Jeff Hutchins")
                UT_SUITE_SETUP_HOOK(testCaseSetup))

/*!
 * @brief Test case setup function.
 *
 * The test suite setups each test case by initializing the data for the
 * following mocked functions:
 *
 * @li dmaRead()
 * @li lwrtosQueueSendBlocking()
 * @li perfDaemonChangeSeqFlush_STEP()
 * @li perfDaemonWaitForCompletion()
 *
 * @return NULL
 */
static void *testCaseSetup(void)
{
    lwrtosQueueSendBlockingMockInit();
    RESET_FAKE(perfDaemonChangeSeqRead_STEP_MOCK);
    RESET_FAKE(perfDaemonChangeSeqFlush_STEP_MOCK);
    perfDaemonWaitForCompletionMockInit();
    FFF_RESET_HISTORY();
    dmaMockInit();
    FFF_RESET_HISTORY();
}

/* --------------- perfDaemonChangeSeqScriptStepInsert_IMPL() --------------- */
/*!
 * @brief Attempt to add a step into a script containing the maximum number of
 * steps.
 *
 * @verbatim
 * Test Case ID   : CHANGESEQ_DAEMON.ScriptInsertTooManySteps
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_DAEMON_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize a change sequencer script.
 * @li Add LW2080_CTRL_PERF_CHANGE_SEQ_SCRIPT_VF_SWITCH_MAX_STEPS steps.
 *
 * Test Exelwtion:
 * @li Add a new step to the change sequencer script.
 * @li Expect @ref perfDaemonChangeSeqScriptStepInsert_IMPL() to return
 * @ref FLCN_ERR_ILWALID_ARGUMENT
 *
 * Test Tear Down: N/A
 */
UT_CASE_DEFINE(CHANGESEQ_DAEMON, ScriptInsertTooManySteps,
                UT_CASE_SET_DESCRIPTION("Insert a step into a script with max steps.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CHANGESEQ_DAEMON, ScriptInsertTooManySteps)
{
    CHANGE_SEQ changeSeq;
    CHANGE_SEQ_SCRIPT script;
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER step;

    initializeScript(&script, LW2080_CTRL_PERF_CHANGE_SEQ_SCRIPT_VF_SWITCH_MAX_STEPS);

    UT_ASSERT_EQUAL_UINT(perfDaemonChangeSeqScriptStepInsert_IMPL(&changeSeq, &script, &step), FLCN_ERR_ILWALID_ARGUMENT);
    UT_ASSERT_EQUAL_UINT(script.hdr.data.numSteps, LW2080_CTRL_PERF_CHANGE_SEQ_SCRIPT_VF_SWITCH_MAX_STEPS);
}

/*!
 * @brief Verify the change sequencer daemon can handle the case where it
 * encountered an error flushing the step to the frame buffer.
 *
 * @verbatim
 * Test Case ID   : CHANGESEQ_DAEMON.ScriptInsertErrorFlushingStep
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * When a step is added to the script, the change sequencer stores the step
 * in a buffer in the frame buffer. This test is designed to catch the case
 * where writing the step to the frame buffer fails.
 *
 * @ingroup CHANGESEQ_DAEMON_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize a change sequencer script.
 * @li Configure @ref perfDaemonChangeSeqFlush_STEP_MOCK() to return an error.
 *
 * Test Exelwtion:
 * @li Call @ref perfDaemonChangeSeqScriptStepInsert_IMPL() with a new step.
 * @li Expect @ref perfDaemonChangeSeqScriptStepInsert_IMPL() to return the
 * error @ref perfDaemonChangeSeqFlush_STEP_MOCK() returned.
 *
 * Test Tear Down: N/A
 */
UT_CASE_DEFINE(CHANGESEQ_DAEMON, ScriptInsertErrorFlushingStep,
                UT_CASE_SET_DESCRIPTION("Error flushing the step to FB.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CHANGESEQ_DAEMON, ScriptInsertErrorFlushingStep)
{
    CHANGE_SEQ changeSeq;
    CHANGE_SEQ_SCRIPT script;
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER step;

    initializeScript(&script, 0);
    perfDaemonChangeSeqFlush_STEP_MOCK_fake.return_val = FLCN_ERR_ILWALID_PATH;

    UT_ASSERT_EQUAL_UINT(perfDaemonChangeSeqScriptStepInsert_IMPL(&changeSeq, &script, &step), FLCN_ERR_ILWALID_PATH);
    UT_ASSERT_EQUAL_UINT(script.hdr.data.numSteps, 0);
}

/*!
 * @brief Test a successful insertion of a step into the change sequencer
 * script.
 *
 * @verbatim
 * Test Case ID   : CHANGESEQ_DAEMON.ScriptInsertSuccess
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_DAEMON_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize a change sequencer script with at least one free slot.
 * @li Configure @ref perfDaemonChangeSeqFlush_STEP_MOCK() to return
 * @ref FLCN_OK
 *
 * Test Exelwtion:
 * @li Call @ref perfDaemonChangeSeqScriptStepInsert_IMPL() with a new step.
 * @li Expect @ref perfDaemonChangeSeqScriptStepInsert_IMPL() to return
 * @ref FLCN_OK
 */
UT_CASE_DEFINE(CHANGESEQ_DAEMON, ScriptInsertSuccess,
                UT_CASE_SET_DESCRIPTION("Success inserting a step.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CHANGESEQ_DAEMON, ScriptInsertSuccess)
{
    CHANGE_SEQ changeSeq;
    CHANGE_SEQ_SCRIPT script;
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER step;

    initializeScript(&script, LW2080_CTRL_PERF_CHANGE_SEQ_SCRIPT_VF_SWITCH_MAX_STEPS / 2);
    perfDaemonChangeSeqFlush_STEP_MOCK_fake.return_val = FLCN_OK;

    UT_ASSERT_EQUAL_UINT(perfDaemonChangeSeqScriptStepInsert_IMPL(&changeSeq, &script, &step), FLCN_OK);
    UT_ASSERT_EQUAL_UINT(script.hdr.data.numSteps, LW2080_CTRL_PERF_CHANGE_SEQ_SCRIPT_VF_SWITCH_MAX_STEPS / 2 + 1);
}

/* ----------------- perfDaemonChangeSeqFireNotifications() ----------------- */
/*!
 * @brief Validate the caller's input.
 *
 * @verbatim
 * Test Case ID   : CHANGESEQ_DAEMON.FireNotificationsIlwalidInput
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * Test case where the caller provides invalid input. The function should return
 * an error indicating such.
 *
 * @ingroup CHANGESEQ_DAEMON_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize a RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_DATA with an invalid
 * notification number.
 *
 * Test Exelwtion:
 * @li Call @ref perfDaemonChangeSeqFireNotifications() with the notification data.
 * @li Expect @ref perfDaemonChangeSeqFireNotifications() to return
 * @ref FLCN_ERR_ILWALID_ARGUMENT
 */
UT_CASE_DEFINE(CHANGESEQ_DAEMON, FireNotificationsIlwalidInput,
                UT_CASE_SET_DESCRIPTION("No notifications are present.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CHANGESEQ_DAEMON, FireNotificationsIlwalidInput)
{
    CHANGE_SEQ changeSeq;
    RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_DATA data = { 0 };

    initializeChangeSequencer(&changeSeq);
    initializeNotificationData(&data);
    data.notification = RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_MAX;

    UT_ASSERT_EQUAL_UINT(perfDaemonChangeSeqFireNotifications(&changeSeq, &data), FLCN_ERR_ILWALID_ARGUMENT);
    UT_ASSERT_EQUAL_UINT(perfDaemonWaitForCompletionMockNumCalled(), 0);
}

/*!
 * @brief No clients have registered for notification.
 *
 * @verbatim
 * Test Case ID   : CHANGESEQ_DAEMON.FireNotificationNonePresent
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * In the case where no client has registered for notification, the function
 * should return @ref FLCN_OK.
 *
 * @ingroup CHANGESEQ_DAEMON_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the change sequencer.
 * @li Initialize notification data.
 * @li Call @ref perfDaemonChangeSeqFireNotifications() with the notification data.
 * @li Expect @ref perfDaemonChangeSeqFireNotifications() to return
 * @ref FLCN_OK
 *
 * Test Exelwtion:
 * @li Call @ref perfDaemonChangeSeqFireNotifications() with the notification data.
 * @li Expect @ref perfDaemonChangeSeqFireNotifications() to return
 * @ref FLCN_OK
 */
UT_CASE_DEFINE(CHANGESEQ_DAEMON, FireNotificationNonePresent,
                UT_CASE_SET_DESCRIPTION("No notifications are present.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CHANGESEQ_DAEMON, FireNotificationNonePresent)
{
    //! @todo Implement test.
}

/*!
 * @brief No clients have registered for notification.
 *
 * @verbatim
 * Test Case ID   : CHANGESEQ_DAEMON.FireNotificationsPresent
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * In the case where multip clients have registered for notification, the
 * function should notify all the clients.
 *
 * @ingroup CHANGESEQ_DAEMON_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the change sequencer.
 * @li Initialize notification data.
 * @li Initialize the client list to notify.
 * @li Initialize the mocked functions @ref lwrtosQueueSendBlocking_MOCK() and
 * @ref perfDaemonWaitForCompletion_MOCK().
 *
 * Test Exelwtion:
 * @li Call @ref perfDaemonChangeSeqFireNotifications() with the notification data.
 * @li Expect @ref perfDaemonChangeSeqFireNotifications() to return
 * @ref FLCN_OK
 */
UT_CASE_DEFINE(CHANGESEQ_DAEMON, FireNotificationsPresent,
                UT_CASE_SET_DESCRIPTION("Notifications are present.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CHANGESEQ_DAEMON, FireNotificationsPresent)
{
    CHANGE_SEQ changeSeq;
    RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_DATA data = { 0 };
    PERF_CHANGE_SEQ_NOTIFICATION notifications[4];

    initializeChangeSequencer(&changeSeq);
    initializeNotificationData(&data);
    memset(notifications, 0x00, sizeof(notifications));
    for (LwU8 i = 0; i < 3; ++i)
    {
        notifications[i].pNext = &notifications[i+1];
    }
    changeSeq.notifications[RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_PRE_PSTATE].pNotification = &notifications[0];
    for (LwU8 j = 0; j < 4; ++j)
    {
        lwrtosQueueSendBlockingMockAddEntry(j, FLCN_OK, NULL);
        perfDaemonWaitForCompletionMockAddEntry(j, FLCN_OK);
    }

    UT_ASSERT_EQUAL_UINT(perfDaemonChangeSeqFireNotifications(&changeSeq, &data), FLCN_OK);
    UT_ASSERT_EQUAL_UINT(perfDaemonWaitForCompletionMockNumCalled(), 4);
}

/* --------------- perfDaemonChangeSeqScriptExelwteAllSteps() --------------- */
/*!
 * @brief Exelwt a script with no steps present.
 *
 * @verbatim
 * Test Case ID   : CHANGESEQ_DAEMON.ExelwteScriptNoSteps
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * When attempting to execute a script with zero steps, should expect the
 * function to return success.
 *
 * @ingroup CHANGESEQ_DAEMON_TEST_SUITE
 *
 * When exelwting a script, the change sequencer reads the current step from
 * the frame buffer. Upon exelwting the step, the step (and its results) is
 * written back to the frame buffer.
 *
 * Test Setup:
 * @li Initialize the change sequencer
 * @li Initialize the change sequencer script with 0 steps
 *
 * Test Exelwtion:
 * @li Call @ref perfDaemonChangeSeqScriptExelwteAllSteps().
 * @li Expect @ref perfDaemonChangeSeqScriptExelwteAllSteps() to return
 * @ref FLCN_OK.
 * @li Expect the number of calls to @ref dmaRead() to be zero.
 */
UT_CASE_DEFINE(CHANGESEQ_DAEMON, ExelwteScriptNoSteps,
                UT_CASE_SET_DESCRIPTION("Attempt to execute a script with 0 steps.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CHANGESEQ_DAEMON, ExelwteScriptNoSteps)
{
    CHANGE_SEQ changeSeq;
    CHANGE_SEQ_SCRIPT script;

    initializeChangeSequencer(&changeSeq);
    initializeScript(&script, 0);

    UT_ASSERT_EQUAL_UINT(
        perfDaemonChangeSeqScriptExelwteAllSteps(&changeSeq,&script),
        FLCN_OK
    );
    UT_ASSERT_EQUAL_UINT8(dmaMockReadNumCalls(), 0);
    UT_ASSERT_EQUAL_UINT8(perfDaemonChangeSeqFlush_STEP_MOCK_fake.call_count, 0);
}

/*!
 * @brief Tests the case where an individual step returns an error.
 *
 * @verbatim
 * Test Case ID   : CHANGESEQ_DAEMON.ExelwteStepFailure
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * In the case where an individual step fails in its exelwtion, that error
 * should stop exelwting the script and propagate the error up to the caller.
 *
 * Test Setup:
 * @li Create a change sequencer script with 4 steps.
 * @li Initialize the mock @ref perfDaemonChangeSeqScriptExelwteStep_35_MOCK()
 * to return @ref FLCN_ERR_ILLEGAL_OPERATION on the third call. All other
 * calls are to return @ref FLCN_OK.
 *
 * Test Exelwtion:
 * @li Call @ref perfDaemonChangeSeqScriptExelwteAllSteps().
 * @li Expect @ref perfDaemonChangeSeqScriptExelwteAllSteps() to return the
 * error that was returned by the mock funtion.
 * @li Expect the mock @ref perfDaemonChangeSeqScriptExelwteStep_35_MOCK() was
 * called three times.
 */
UT_CASE_DEFINE(CHANGESEQ_DAEMON, ExelwteStepFailure,
                UT_CASE_SET_DESCRIPTION("Attempt to execute a script with 0 steps.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CHANGESEQ_DAEMON, ExelwteStepFailure)
{
    //! @todo Implement test
}

/*!
 * @brief Tests the case where a script is successfully exelwted.
 *
 * @verbatim
 * Test Case ID   : CHANGESEQ_DAEMON.ExelwteScriptSteps
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_DAEMON_TEST_SUITE
 */
UT_CASE_DEFINE(CHANGESEQ_DAEMON, ExelwteScriptSteps,
                UT_CASE_SET_DESCRIPTION("Attempt to execute a script with steps.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CHANGESEQ_DAEMON, ExelwteScriptSteps)
{
    CHANGE_SEQ changeSeq;
    CHANGE_SEQ_SCRIPT script;

    initializeChangeSequencer(&changeSeq);
    initializeScript(&script, 2);

    perfDaemonChangeSeqRead_STEP_MOCK_fake.return_val = FLCN_OK;
    perfDaemonChangeSeqFlush_STEP_MOCK_fake.return_val = FLCN_OK;

    // for PERF_CHANGE_SEQ_PROFILE_END()
    UTF_IO_WRITE32(LW_CMSDEC_FALCON_PTIMER0, 0x00000000);
    UTF_IO_WRITE32(LW_CMSDEC_FALCON_PTIMER1, 0x00000000);

    UT_ASSERT_EQUAL_UINT(
        perfDaemonChangeSeqScriptExelwteAllSteps(&changeSeq, &script),
        FLCN_OK
    );
    UT_ASSERT_EQUAL_UINT8(perfDaemonChangeSeqRead_STEP_MOCK_fake.call_count, 2);
    UT_ASSERT_EQUAL_UINT8(perfDaemonChangeSeqFlush_STEP_MOCK_fake.call_count, 2);
}

/* ------------------------ Private Functions ------------------------------- */
static void initializeChangeSequencer(CHANGE_SEQ *pChangeSeq)
{
    memset(pChangeSeq, 0x00, sizeof(*pChangeSeq));
    pChangeSeq->version = LW2080_CTRL_PERF_CHANGE_SEQ_VERSION_35;
    pChangeSeq->cpuStepIdMask = 0;
    pChangeSeq->cpuAdvertisedStepIdMask = 0;
}

/*!
 * @brief Initializes the change sequencer script for testing.
 *
 * The change sequencer script is setup to have the specified number of steps.
 * The steps themselves are dummy/mocked steps.
 *
 * @ingroup CHANGESEQ_DAEMON_TEST_SUITE
 *
 * @param  pScript   The change sequencer script to initialize for testing.
 * @param  numSteps  The number of steps to populate the script with.
 */
static void initializeScript(CHANGE_SEQ_SCRIPT *pScript, LwU32 numSteps)
{
    memset(pScript, 0x00, sizeof(*pScript));
    pScript->hdr.data.numSteps = numSteps;
    pScript->hdr.data.lwrStepIndex = 0;
}

/*!
 * @brief Initializes the notification data for testing.
 *
 * @ingroup CHANGESEQ_DAEMON_TEST_SUITE
 *
 * @param  pData  The notification data to initialize for testing.
 */
static void initializeNotificationData(RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_DATA *pData)
{
    memset(pData, 0x00, sizeof(*pData));
    pData->notification = RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_PRE_PSTATE;
}
