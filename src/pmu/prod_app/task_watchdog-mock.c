/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_watchdog-mock.c
 * @brief  Mock implementations for watchdog
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "task_watchdog-mock.h"

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

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
void
watchdogMockInit(void)
{
    RESET_FAKE(watchdogPreInit_MOCK)
    RESET_FAKE(watchdogPreInitTask_MOCK)
}

/* ------------------------- Mocked Functions ------------------------------- */
DEFINE_FAKE_VALUE_FUNC(FLCN_STATUS, watchdogPreInit_MOCK);

DEFINE_FAKE_VALUE_FUNC(FLCN_STATUS, watchdogPreInitTask_MOCK);

/* ------------------------- Static Functions ------------------------------- */
