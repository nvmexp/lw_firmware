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
 * @file    task_perf_daemon-mock.h
 * @brief   Data required for configuring mock task_perf_daemon interfaces.
 */

#ifndef TASK_PERF_DAEMON_MOCK_H
#define TASK_PERF_DAEMON_MOCK_H

/* ------------------------ Includes ---------------------------------------- */
#include "pmusw.h"
#include "ut_port_types.h"
#include "fff.h"

/* ------------------------ Function Prototypes ----------------------------- */
void perfDaemonTaskMockInit(void);

void perfDaemonWaitForCompletionMockInit(void);
void perfDaemonWaitForCompletionMockAddEntry(LwU8 entry, FLCN_STATUS status);
LwU8 perfDaemonWaitForCompletionMockNumCalled(void);

void perfDaemonWaitForCompletionFromRMMockInit(void);
void perfDaemonWaitForCompletionFromRMMockAddEntry(LwU8 entry, FLCN_STATUS status);
/*!
 * @copydoc perfDaemonPreInitTask
 *
 * @note    Mock implementation of @ref perfDaemonPreInitTask
 */
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, perfDaemonPreInitTask_MOCK);

#endif // TASK_PERF_DAEMON_MOCK_H
