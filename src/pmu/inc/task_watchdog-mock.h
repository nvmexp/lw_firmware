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
 * @file    task_watchdog-mock.h
 * @brief   Mock declaratiosn for watchdog
 */

#ifndef TASK_WATCHDOG_MOCK_H
#define TASK_WATCHDOG_MOCK_H

/* ------------------------- System Includes -------------------------------- */
#include "ut_port_types.h"
#include "fff.h"
#include "flcnifcmn.h"
#include "task_watchdog.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
/*!
 * @brief   Initializes mocking for watchdog
 */
void watchdogMockInit(void);

/* ------------------------- Mock Prototypes -------------------------------- */
/*!
 * @copydoc watchdogPreInit_MOCK
 *
 * @note    Mock implementation of @ref watchdogPreInit_MOCK
 */
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, watchdogPreInit_MOCK);
/*!
 * @copydoc watchdogPreInitTask
 *
 * @note    Mock implementation of @ref watchdogPreInitTask
 */
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, watchdogPreInitTask_MOCK);

#endif // TASK_WATCHDOG_MOCK_H
