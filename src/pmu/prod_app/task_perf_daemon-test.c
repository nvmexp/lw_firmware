/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    task_perf_daenib-test.c
 * @brief   Unit tests for task perf_daemon.
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "lwrtos-mock.h"
#include "task_perf_daemon.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static void* testSetup(void);

/* ------------------------ Defines and Macros ------------------------------ */
/* ------------------------ Type Definitions -------------------------------- */
/* ------------------------ Globals ----------------------------------------- */
/* ------------------------ Local Data -------------------------------------- */
/* ------------------------ Test Suite Declaration--------------------------- */
UT_SUITE_DEFINE(PMU_TASK_PERF_DAEMON,
                UT_SUITE_SET_COMPONENT("TODO")
                UT_SUITE_SET_DESCRIPTION("TODO")
                UT_SUITE_SET_OWNER("TODO")
                UT_SUITE_SETUP_HOOK(testSetup))

UT_CASE_DEFINE(PMU_TASK_PERF_DAEMON, perfDaemonWaitForCompletion_IMPL,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_TASK_PERF_DAEMON, perfDaemonWaitForCompletionFromRM_IMPL,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

/* ------------------------ Test Suite Implementation ----------------------- */
/*!
 * @brief
 *
 * @details
 */
UT_CASE_RUN(PMU_TASK_PERF_DAEMON, perfDaemonWaitForCompletion_IMPL)
{
    FLCN_STATUS status;
    DISPATCH_PERF_DAEMON_COMPLETION request;

    // Check that we report timeout correctly.
    lwrtosQueueReceive_MOCK_fake.return_val = FLCN_ERROR;
    status = perfDaemonWaitForCompletion_IMPL(&request,
                                              PERF_DAEMON_COMPLETION_EVENT_ID_PERF_CHANGE_SEQ_NOTIFICATION_COMPLETION,
                                              NULL);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_TIMEOUT);

    // Check that we handle unexpected event type correctly
    lwrtosQueueReceive_MOCK_fake.return_val = FLCN_OK;
    request.cmd.hdr.eventType = PERF_DAEMON_COMPLETION_EVENT_ID_PERF_CHANGE_SEQ_SCRIPT_EXELWTE_COMPLETION;
    status = perfDaemonWaitForCompletion_IMPL(&request,
                                              PERF_DAEMON_COMPLETION_EVENT_ID_PERF_CHANGE_SEQ_NOTIFICATION_COMPLETION,
                                              NULL);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_STATE);

    // case that we get the correct event type back
    lwrtosQueueReceive_MOCK_fake.return_val = FLCN_OK;
    request.cmd.hdr.eventType = PERF_DAEMON_COMPLETION_EVENT_ID_PERF_CHANGE_SEQ_SCRIPT_EXELWTE_COMPLETION;
    status = perfDaemonWaitForCompletion_IMPL(&request,
                                              PERF_DAEMON_COMPLETION_EVENT_ID_PERF_CHANGE_SEQ_SCRIPT_EXELWTE_COMPLETION,
                                              NULL);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
}

/*!
 * @brief
 *
 * @details
 */
UT_CASE_RUN(PMU_TASK_PERF_DAEMON, perfDaemonWaitForCompletionFromRM_IMPL)
{
    FLCN_STATUS status;
    DISPATCH_PERF_DAEMON_COMPLETION request;
    RM_FLCN_MSG_PMU msg;

    perfDaemonWaitForCompletionMockAddEntry(0, FLCN_ERR_BAR0_PRIV_WRITE_ERROR);

    // Check that we return error from perfDaemonWaitForCompletion
    status = perfDaemonWaitForCompletionFromRM_IMPL(&request,
                                                    &msg,
                                                    PERF_DAEMON_COMPLETION_EVENT_ID_PERF_CHANGE_SEQ_SCRIPT_EXELWTE_COMPLETION,
                                                    NULL);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_BAR0_PRIV_WRITE_ERROR);
}

/* ------------------------- Private Functions ------------------------------ */
static void* testSetup(void)
{
    lwrtosMockInit();
    perfDaemonWaitForCompletionMockInit();
    return NULL;
}

/* ------------------------- End of File ------------------------------------ */
