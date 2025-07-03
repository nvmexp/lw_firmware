/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  task_watchdog.h
 *
 * @brief Common header for all task WATCHDOG files.
 */
#ifndef TASK_WATCHDOG_H
#define TASK_WATCHDOG_H

#include "g_task_watchdog.h"

#ifndef G_TASK_WATCHDOG_H
#define G_TASK_WATCHDOG_H

#include "lwoswatchdog.h"
#include "pmu_objpmu.h"

/* ------------------------- Compile Time Asserts --------------------------- */
#ifdef WATCHDOG_ENABLE
    ct_assert(PMUCFG_FEATURE_ENABLED(PMUTASK_WATCHDOG));
#else
    ct_assert(!PMUCFG_FEATURE_ENABLED(PMUTASK_WATCHDOG));
#endif

/* ------------------------ External Definitions ---------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
mockable FLCN_STATUS watchdogPreInit(void)
    GCC_ATTRIB_SECTION("imem_init", "watchdogPreInit");

void watchdogTimeViolationCallback(LwU32 failingTaskMask)
    GCC_ATTRIB_SECTION("imem_resident", "watchdogTimeViolationCallback");
/* ------------------------- Defines ---------------------------------------- */
/*!
 * @brief      The watchdog task period, in microseconds.
 */
#define WATCHDOG_TASK_PERIOD_US                 (1000U)

/*!
 * @brief      The unitless amount to scale the HW watchdog timer timeout value.
 *
 * @note       The chosen value 5 is arbitrary.
 */
#define WATCHDOG_TIMEOUT_SCALE_FACTOR           (5U)

/*!
 * @brief      Amount of time, in RTOS ticks, that the watchdog task waits
 *             before running.
 */
#define WATCHDOG_TASK_PERIOD_OS_TICKS (WATCHDOG_TASK_PERIOD_US / LWRTOS_TICK_PERIOD_US)

/*!
 * @brief      The minimum timeout value in nanoseconds the applications are
 *             allowed to set is 1 ms.
 */
#define WATCHDOG_APPLICATION_MINIMUM_TIMEOUT_NS (1000000U)

/*!
 * @brief      The application timeout value in nanoseconds used by the Watchdog
 *             task when a task does not want a particular time enforcement.
 */
#define WATCHDOG_APPLICATION_DEFAULT_TIMEOUT_NS (LW_U32_MAX)

/*!
 * @brief      Watchdog timer runs at the frequency of the PMU and so the
 *             timeout value needs to be scaled to match the frequency of the
 *             RTOS tick period.
 *
 * @return     The number of engine clock ticks to program the HW watchdog
 *             timer.
 */
#define WATCHDOG_GET_HW_TIMER_TIMEOUT_ENGCLK() \
    (WATCHDOG_TIMEOUT_SCALE_FACTOR * WATCHDOG_TASK_PERIOD_OS_TICKS * (PMU_GET_PWRCLK_HZ() / LWRTOS_TICK_RATE_HZ))

#endif // G_TASK_WATCHDOG_H
#endif // TASK_WATCHDOG_H
