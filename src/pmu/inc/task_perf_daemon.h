/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_perf_daemon.h
 * @brief  Common header for all task PERF DAEMON files.
 *
 * @section References
 * @ref "../../../drivers/resman/arch/lwalloc/common/inc/rmpmucmdif.h"
 */

#ifndef TASK_PERF_DAEMON_H
#define TASK_PERF_DAEMON_H

#include "g_task_perf_daemon.h"

#ifndef G_TASK_PERF_DAEMON_H
#define G_TASK_PERF_DAEMON_H

/* ------------------------- System Includes ------------------------------- */
#include "main.h"
#include "pmusw.h"
#include "flcntypes.h"
#include "pmu_objperf.h"
#include "perf/perf_daemon.h"

/* ------------------------- Application Includes -------------------------- */
#include "unit_api.h"

/* ------------------------- Types Definitions ----------------------------- */

/*!
 * Perf change sequence notification cmd. Issues from perf change sequence.
 *
 * This structure defines the command struct perf change sequence will send
 * to all the client tasks registered for the perf change sequence notification.
 */
typedef struct
{
    /*!
     * Must be first element of the structure
     */
    DISP2UNIT_HDR                                   hdr;

    /*!
     * @copydoc RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_DATA
     */
    RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_DATA    data;
} DISPATCH_PERF_CHANGE_SEQ_NOTIFICATION;

/*!
 * @brief   Message queue for perf daemon task. If perf daemon task wants
 *          to offload some work to other task/falcon/RM, it will wait for the
 *          completion signal on @ref PMU_QUEUE_ID_PERF_DAEMON_RESPONSE.
 */
typedef union
{
    DISP2UNIT_HDR       hdr;
    DISP2UNIT_RM_RPC    rpc;
} DISPATCH_PERF_DAEMON_COMPLETION;

/*!
 * Perf change sequence script execute cmd. Issues from perf task.
 *
 * This structure defines the command struct PERF task could send to PERF DAEMON
 * task for building and exelwting perf change sequence script.
 */
typedef struct
{
    /*!
     * Must be first element of the structure
     */
    DISP2UNIT_HDR   hdr;

    /*!
     * Perf change sequence object pointer
     */
    CHANGE_SEQ     *pChangeSeq;
} DISPATCH_PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE;

/*!
 * Perf change sequence lpwr post script execute cmd. Issues from LPWR task.
 *
 * This structure defines the command struct LPWR task could send to PERF DAEMON
 * task for building and exelwting perf change sequence lpwr post script.
 */
typedef struct
{
    /*!
     * Must be first element of the structure
     */
    DISP2UNIT_HDR   hdr;

    /*!
     * LPWR script id
     */
    LwU8            lpwrScriptId;
} DISPATCH_PERF_DAEMON_CHANGE_SEQ_LPWR_POST_SCRIPT;

/*!
 * CLK MCLK switch cmd. Issues from perf task.
 *
 * This structure defines the command struct PERF task could send to PERF DAEMON
 * task for mclk switch request
 */
typedef struct
{
    /*!
     * Must be first element of the structure
     */
    DISP2UNIT_HDR                   hdr;

    /*!
     * Mclk Switch request structure
     */
    RM_PMU_STRUCT_CLK_MCLK_SWITCH   mclkSwitchRequest;
} DISPATCH_PERF_DAEMON_MCLK_SWITCH;

/*!
 * A union of all available commands to PERF DAEMON.
 */
typedef union
{
    DISP2UNIT_HDR       hdr;
    DISP2UNIT_RM_RPC    rpc;

    /*!
     * Perf change sequence script execute request passed from PERF task.
     */
    DISPATCH_PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE      scriptExelwte;

    /*!
     * Perf change sequence lpwr post script execute request passed from
     * LPWR task.
     */
    DISPATCH_PERF_DAEMON_CHANGE_SEQ_LPWR_POST_SCRIPT    lpwrPostScriptExelwte;

    /*!
     * mclk switch request passed from PERF task.
     */
    DISPATCH_PERF_DAEMON_MCLK_SWITCH                    mclkSwitch;
} DISPATCH_PERF_DAEMON;

/* ------------------------- Defines --------------------------------------- */
/*!
 * Event type for perf change sequence script exelwtion. Perf task will use
 * this event type to send the perf change sequence script exelwtion request
 * to perf daemon task.
 */
#define PERF_DAEMON_EVENT_ID_PERF_CHANGE_SEQ_SCRIPT_EXELWTE                   \
    (DISP2UNIT_EVT__COUNT + 0U)

/*!
 * Event type for perf change sequence lpwr post script exelwtion. LPWR task
 * will use this event type to send the perf change sequence lpwr post script
 * exelwtion request to perf daemon task.
 */
#define PERF_DAEMON_EVENT_ID_PERF_CHANGE_SEQ_LPWR_POST_SCRIPT_EXELWTE         \
    (DISP2UNIT_EVT__COUNT + 1U)

/*!
 * Event type for mclk switch on fbcln. PERF task will use this event type to
 * send the mclk switch request to perf daemon task.
 */
#define PERF_DAEMON_EVENT_ID_PERF_MCLK_SWITCH                                 \
    (DISP2UNIT_EVT__COUNT + 2U)

/*!
 * Event type for perf change sequence notification completion. All the
 * clients of perf change sequence notification will send this event as
 * signal of completion to the perf daemon task. Perf change sequence will
 * wait for this signal from each client before sending the notification
 * to the next client.
 */
#define PERF_DAEMON_COMPLETION_EVENT_ID_PERF_CHANGE_SEQ_NOTIFICATION_COMPLETION     \
    (DISP2UNIT_EVT__COUNT + 0U)

/*!
 * Event type for perf change sequence script completion. RM perf change
 * sequence will send this event as signal of completion to the perf daemon task.
 */
#define PERF_DAEMON_COMPLETION_EVENT_ID_PERF_CHANGE_SEQ_SCRIPT_EXELWTE_COMPLETION   \
    (DISP2UNIT_EVT__COUNT + 1U)

/* ------------------------- Function Prototypes  -------------------------- */
mockable FLCN_STATUS perfDaemonWaitForCompletion(DISPATCH_PERF_DAEMON_COMPLETION *pRequest, LwU8 eventType, LwUPtr timeout);
mockable FLCN_STATUS perfDaemonWaitForCompletionFromRM(DISPATCH_PERF_DAEMON_COMPLETION *pRequest, RM_FLCN_MSG_PMU *pMsg, LwU8 eventType, LwUPtr timeout);

/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */

#endif // G_TASK_PERF_DAEMON_H
#endif // TASK_PERF_DAEMON_H
