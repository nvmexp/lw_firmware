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
 * @file    task_perf_daemon-mock.c
 * @brief   Mock implementations of task_perf_daemon public interfaces.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "task_perf_daemon.h"
#include "task_perf_daemon-mock.h"

/* ------------------------ Macros and Defines ------------------------------ */
#define PERF_DAEMON_WAIT_FOR_COMPLETION_MOCK_MAX_ENTRIES                (32U)
#define PERF_DAEMON_WAIT_FOR_COMPLETION_FROM_RM_MOCK_MAX_ENTRIES         (4U)

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Configuration data for the mocked version of perfDaemonWaitForCompletion().
 */
typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
    } entries[PERF_DAEMON_WAIT_FOR_COMPLETION_MOCK_MAX_ENTRIES];
} PERF_DAEMON_WAIT_FOR_COMPLETION_MOCK_CONFIG;

/*!
 * Configuration data for the mocked version of perfDaemonWaitForCompletionFromRM().
 */
typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
    } entries[PERF_DAEMON_WAIT_FOR_COMPLETION_FROM_RM_MOCK_MAX_ENTRIES];
} PERF_DAEMON_WAIT_FOR_COMPLETION_FROM_RM_MOCK_CONFIG;

/* ------------------------ Static Variables -------------------------------- */
static PERF_DAEMON_WAIT_FOR_COMPLETION_MOCK_CONFIG perfDaemonWaitForCompletion_MOCK_CONFIG;
static PERF_DAEMON_WAIT_FOR_COMPLETION_FROM_RM_MOCK_CONFIG perfDaemonWaitForCompletionFromRM_MOCK_CONFIG;

/* ------------------------ Utility Functions ------------------------------- */

void
perfDaemonTaskMockInit(void)
{
    RESET_FAKE(perfDaemonPreInitTask_MOCK);
}

/*!
 * @brief Initialize the mock configuration data for perfDaemonWaitForCompletion().
 *
 * Zeros out the structure and sets the return values to FLCN_ERROR. It is the
 * responsibility of the unit tests to provide expected and return values prior
 * to running.
 */
void perfDaemonWaitForCompletionMockInit(void)
{
    memset(&perfDaemonWaitForCompletion_MOCK_CONFIG, 0x00, sizeof(perfDaemonWaitForCompletion_MOCK_CONFIG));
    for (LwU8 i = 0; i < PERF_DAEMON_WAIT_FOR_COMPLETION_MOCK_MAX_ENTRIES; ++i)
    {
        perfDaemonWaitForCompletion_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
    }
}

/*!
 * @brief Adds an entry to the perfDeamonWaitForCompletion_MOCK_CONFIG data.
 *
 * @param[in]  entry    The entry (or call number) for the test.
 * @param[in]  status   Value to return from the mock function.
 */
void perfDaemonWaitForCompletionMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < PERF_DAEMON_WAIT_FOR_COMPLETION_MOCK_MAX_ENTRIES);
    perfDaemonWaitForCompletion_MOCK_CONFIG.entries[entry].status = status;
}

/*!
 * @brief Returns the number of times the mock function was called.
 *
 * @return the number of times called.
 */
LwU8 perfDaemonWaitForCompletionMockNumCalled(void)
{
    return perfDaemonWaitForCompletion_MOCK_CONFIG.numCalls;
}

/*!
 * @brief Initializes the mock configuration data for perfDaemonWaitForCompletionFromRM().
 *
 * Zeros out the structure. Responsibility of the test to provide expected
 * values and return values prior to running tests.
 */
void perfDaemonWaitForCompletionFromRMMockInit(void)
{
    memset(&perfDaemonWaitForCompletionFromRM_MOCK_CONFIG, 0x00, sizeof(perfDaemonWaitForCompletionFromRM_MOCK_CONFIG));
    for (LwU8 i = 0; i < PERF_DAEMON_WAIT_FOR_COMPLETION_FROM_RM_MOCK_MAX_ENTRIES; ++i)
    {
        perfDaemonWaitForCompletionFromRM_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
    }
}

/*!
 * @brief Adds an entry to the perfDaemonWaitForCompletionFromRM_MOCK_CONFIG
 * data.
 *
 * If the pointer to the expected values is null, the mock function will not
 * check the expected values when the mock function is called.
 *
 * @param[in]  entry   The entry (or call number) for the test.
 * @param[in]  status  Value to return from the mock function
 */
void perfDaemonWaitForCompletionFromRMMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < PERF_DAEMON_WAIT_FOR_COMPLETION_FROM_RM_MOCK_MAX_ENTRIES);
    perfDaemonWaitForCompletionFromRM_MOCK_CONFIG.entries[entry].status = status;
}

/* ------------------------ Mocked Functions -------------------------------- */
/*!
 * @brief MOCK implementation of perfDaemonWaitForCompletion().
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref perfDaemonWaitForCompletion_MOCK_CONFIG.
 *
 * @param[in]
 * @return perfDaemonWaitForCompletion_MOCK_CONFIG.status
 */
FLCN_STATUS
perfDaemonWaitForCompletion_MOCK
(
    DISPATCH_PERF_DAEMON_COMPLETION *pRequest,
    LwU8                             eventType,
    LwUPtr                           timeout
)
{
    LwU8 entry = perfDaemonWaitForCompletion_MOCK_CONFIG.numCalls;
    ++perfDaemonWaitForCompletion_MOCK_CONFIG.numCalls;

    return perfDaemonWaitForCompletion_MOCK_CONFIG.entries[entry].status;
}

/*!
 * @brief MOCK implementation of perfDaemonWaitForCompletion().
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref perfDaemonWaitForCompletion_MOCK_CONFIG.
 *
 * @param[in]   pRequest
 * @param[in]   pMsg
 * @param[in]   eventType
 * @param[in]   timeout
 *
 * @return perfDaemonWaitForCompletion_MOCK_CONFIG.status
 */
FLCN_STATUS
perfDaemonWaitForCompletionFromRM_MOCK
(
    DISPATCH_PERF_DAEMON_COMPLETION *pRequest,
    RM_FLCN_MSG_PMU                 *pMsg,
    LwU8                             eventType,
    LwUPtr                           timeout
)
{
    LwU8 entry = perfDaemonWaitForCompletionFromRM_MOCK_CONFIG.numCalls;
    ++perfDaemonWaitForCompletionFromRM_MOCK_CONFIG.numCalls;

    return perfDaemonWaitForCompletionFromRM_MOCK_CONFIG.entries[entry].status;
}

DEFINE_FAKE_VALUE_FUNC(FLCN_STATUS, perfDaemonPreInitTask_MOCK);
