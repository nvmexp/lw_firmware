/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    task_therm.h
 * @copydoc task_therm.c
 */

#ifndef TASK_THERM_H
#define TASK_THERM_H

#include "g_task_therm.h"

#ifndef G_TASK_THERM_H
#define G_TASK_THERM_H

/* ------------------------- System Includes ------------------------------- */
#include "pmusw.h"
#include "pmu_fanctrl.h"
#include "lwostimer.h"
#include "pmu_objsmbpbi.h"

/* ------------------------- Application Includes -------------------------- */
#include "unit_api.h"

/* ------------------------- Types Definitions ----------------------------- */
/*!
 * Generic dispatching structure for sending work to the THERM task.
 */
typedef union
{
    DISP2UNIT_HDR       hdr;
    DISP2UNIT_RM_RPC    rpc;
} DISPATCH_THERM;

/*!
 * Generic dispatching structure used to send work to tasks and subtasks (THERM,
 * FANCTRL, SMBPBI) within task_therm from other tasks, such as task for CMD
 * dispatching, or resident task for interrupt handling.
 */
typedef union
{
    DISP2UNIT_HDR       hdr;
    DISP2UNIT_RM_RPC    rpc;

    DISPATCH_THERM      therm;     //<! Dispatch to THERM
    DISPATCH_SMBPBI     smbpbi;    //<! Dispatch to SMBPBI
    DISPATCH_FANCTRL    fanctrl;   //<! Dispatch to FANCTRL
} DISP2THERM_CMD;

/*!
 * Enumeration of THERM task's PMU_OS_TIMER callback entries.
 */
typedef enum
{
    /*!
     * Entry for 1Hz THERM callbacks
     */
    THERM_OS_TIMER_ENTRY_1HZ_CALLBACKS,

    /*!
     * Entry for Fan Policy callbacks
     */
    THERM_OS_TIMER_ENTRY_FAN_POLICY_CALLBACKS,

    /*!
     * Entry for Therm Policy callbacks
     */
    THERM_OS_TIMER_ENTRY_THERM_POLICY_CALLBACKS,

    /*!
     * Entry for Therm Monitor callbacks. Callback required for GP102+.
     */
    THERM_OS_TIMER_ENTRY_THERM_MONITOR_CALLBACKS,

    /*!
     * Must always be the last entry
     */
    THERM_OS_TIMER_ENTRY_NUM_ENTRIES
} THERM_OS_TIMER_ENTRIES;

/* ------------------------- Defines --------------------------------------- */
/*!
 * Event type for thermal interrupt notifications
 *
 * @ref thermEventService, _thermEventTaskNotify
 */
#define THERM_SIGNAL_IRQ                            (DISP2UNIT_EVT__COUNT + 0U)

/*!
 * Event type for notification from Perf task after VF lwrve change
 */
#define THERM_EVENT_ID_PERF_VF_ILWALIDATION_NOTIFY  (DISP2UNIT_EVT__COUNT + 1U)

/*!
 * Event type for DETACH request from CMDMGMT task.
 */
#define THERM_DETACH_REQUEST                        (DISP2UNIT_EVT__COUNT + 2U)

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
extern LwrtosQueueHandle ThermI2cQueue;
extern OS_TIMER          ThermOsTimer;

/* ------------------------- Public Functions ------------------------------ */
mockable void thermPreInit(void)
    GCC_ATTRIB_SECTION("imem_init", "thermPreInit");

#endif // G_TASK_THERM_H
#endif // TASK_THERM_H
