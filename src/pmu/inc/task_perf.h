/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_perf.h
 * @brief  Common header for all task PERF files.
 *
 * @section References
 * @ref "../../../drivers/resman/arch/lwalloc/common/inc/rmpmucmdif.h"
 */

#ifndef TASK_PERF_H
#define TASK_PERF_H

/* ------------------------- System Includes ------------------------------- */
#include "pmusw.h"
#include "flcntypes.h"

/* ------------------------- Application Includes -------------------------- */
#include "unit_api.h"
#include "perf/changeseq.h"
#include "perf/perf_limit_client.h"

/* ------------------------- Types Definitions ----------------------------- */
/*!
 * Perf change sequence script completion cmd. Issued from perf daemon task to
 * perf task upon completion of perf change sequence script.
 */
typedef struct
{
    /*!
     * Must be first element of the structure
     */
    DISP2UNIT_HDR   hdr;

    /*!
     * Status of last perf change sequence script exelwtion.
     * If there is any failure, perf task change sequence code will determine
     * if it wants to continue with next change or HALT the processing.
     */
    FLCN_STATUS     completionStatus;
} DISPATCH_PERF_CHANGE_SEQ_SCRIPT_COMPLETION;

/*!
 * Perf change sequence notification register cmd. Issued from clients of perf
 * change sequence.
 */
typedef struct
{
    /*!
     * Must be first element of the structure
     */
    DISP2UNIT_HDR   hdr;

    /*!
     * @copydoc RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION
     */
    RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION     notification;

    /*!
     * @copydoc PERF_CHANGE_SEQ_NOTIFICATION
     */
    PERF_CHANGE_SEQ_NOTIFICATION               *pNotification;
} DISPATCH_PERF_CHANGE_SEQ_NOTIFICATION_REQUEST;

/*!
 * Perf limit arbitration cmd. Issued from clients of perf limits.
 */
typedef struct
{
    /*!
     * Must be first element of the structure
     */
    DISP2UNIT_HDR   hdr;

    /*!
     * Specifies the DMEM overlay containing the perf. limits to update.
     */
    LwU8 dmemOvl;

    /*!
     * Pointer to the PERF_LIMITS_CLIENT.
     */
    PERF_LIMITS_CLIENT *pClient;
} DISPATCH_PERF_LIMITS_CLIENT;

/*!
 * Perf CLK Controllers ilwalidate request
 */
typedef struct
{
    /*!
     * Must be first element of the structure
     */
    DISP2UNIT_HDR   hdr;
} DISPATCH_PERF_CLK_CONTROLLERS_ILWALIDATE;

/*!
 * Perf CLK Controllers ilwalidate request
 */
typedef struct
{
    /*!
     * Must be first element of the structure
     */
    DISP2UNIT_HDR   hdr;
    /*!
     * Current tFAW value to be programmed in HW
     */
    LwU8            tFAW;
} DISPATCH_PERF_CHANGE_SEQ_QUEUE_MEM_TUNE_CHANGE;

/*!
 * Perf VFE ilwalidate request
 */
typedef struct
{
    /*!
     * Must be first element of the structure
     */
    DISP2UNIT_HDR   hdr;
} DISPATCH_PERF_VFE_ILWALIDATE;

/*!
 * A union of all available commands to PERF.
 */
typedef union
{
    DISP2UNIT_HDR       hdr;
    DISP2UNIT_RM_RPC    rpc;

    /*!
     * @copydoc DISPATCH_PERF_CHANGE_SEQ_SCRIPT_COMPLETION
     */
    DISPATCH_PERF_CHANGE_SEQ_SCRIPT_COMPLETION      scriptCompletion;

    /*!
     * @copydoc DISPATCH_PERF_CHANGE_SEQ_NOTIFICATION_REQUEST
     */
    DISPATCH_PERF_CHANGE_SEQ_NOTIFICATION_REQUEST   notificationRequest;

    /*!
     * @copydoc DISPATCH_PERF_LIMITS_CLIENT
     */
    DISPATCH_PERF_LIMITS_CLIENT                     limitsClient;

    /*!
     * @copydoc DISPATCH_PERF_CLK_CONTROLLERS_ILWALIDATE
     */
    DISPATCH_PERF_CLK_CONTROLLERS_ILWALIDATE        clkControllersIlwalidate;

    /*!
     * @copydoc DISPATCH_PERF_CHANGE_SEQ_QUEUE_MEM_TUNE_CHANGE
     */
    DISPATCH_PERF_CHANGE_SEQ_QUEUE_MEM_TUNE_CHANGE  memTune;

    /*!
     * @copydoc DISPATCH_PERF_VFE_ILWALIDATE
     */
    DISPATCH_PERF_VFE_ILWALIDATE                    vfeIlwalidate;
} DISPATCH_PERF;

/* ------------------------- Defines --------------------------------------- */
/*!
 * Event type for perf change sequence script completion. Perf task will use
 * this event type to receive the perf change sequence script completion signal
 * from perf daemon task.
 */
#define PERF_EVENT_ID_PERF_CHANGE_SEQ_SCRIPT_COMPLETION                        \
    (DISP2UNIT_EVT__COUNT + 0U)

/*!
 * Event type for perf change sequence notification register request.
 */
#define PERF_EVENT_ID_PERF_CHANGE_SEQ_NOTIFICATION_REQUEST_REGISTER            \
    (DISP2UNIT_EVT__COUNT + 1U)

/*!
 * Event type for perf change sequence notification unregister request.
 */
#define PERF_EVENT_ID_PERF_CHANGE_SEQ_NOTIFICATION_REQUEST_UNREGISTER          \
    (DISP2UNIT_EVT__COUNT + 2U)

/*!
 * Event type for perf limits clients.
 */
#define PERF_EVENT_ID_PERF_LIMITS_CLIENT                                       \
    (DISP2UNIT_EVT__COUNT + 3U)

/*!
 * Event type for clk controllers ilwalidate.
 */
#define PERF_EVENT_ID_PERF_CLK_CONTROLLERS_ILWALIDATE                          \
    (DISP2UNIT_EVT__COUNT + 4U)

/*!
 * Event type for perf change sequence memory tuning change queue request.
 */
#define PERF_EVENT_ID_PERF_CHANGE_SEQ_QUEUE_MEM_TUNE_CHANGE                    \
    (DISP2UNIT_EVT__COUNT + 5U)

/*!
 * Event type for perf vfe ilwalidate request.
 */
#define PERF_EVENT_ID_PERF_VFE_ILWALIDATE                                     \
    (DISP2UNIT_EVT__COUNT + 6U)

/* ------------------------ Function Prototypes ---------------------------- */

#endif // TASK_PERF_H

