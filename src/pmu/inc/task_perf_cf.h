/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_perf_cf.h
 * @brief  Common header for all TASK PERF_CF files.
 */

#ifndef _TASK_PERF_CF_H
#define _TASK_PERF_CF_H

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "flcntypes.h"

/* ------------------------ Application Includes ---------------------------- */
#include "unit_api.h"
#include "perf/changeseq.h"

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * This structure defines the command struct PERF task could send to PERF CF
 * task for loading memory tuning controller's monitor.
 */
typedef struct
{
    /*!
     * Must be first element of the structure
     */
    DISP2UNIT_HDR                   hdr;
} DISPATCH_PERF_CF_CONTROLLER_MEM_TUNE_MONITOR_LOAD;

/*!
 * A union of all available commands to PERF_CF.
 */
typedef union
{
    DISP2UNIT_HDR       hdr;
    DISP2UNIT_RM_RPC    rpc;

    /*!
     * Memory tuning controller monitor load cmd. Issues from perf task.
     */
    DISPATCH_PERF_CF_CONTROLLER_MEM_TUNE_MONITOR_LOAD   monitorLoad;
} DISPATCH_PERF_CF;

/* ------------------------- Defines --------------------------------------- */
/*!
 * Event type for perf change sequence script exelwtion. Perf task will use
 * this event type to send the perf change sequence script exelwtion request
 * to perf daemon task.
 */
#define PERF_CF_EVENT_ID_PERF_CF_CONTROLLER_MEM_TUNE_MONITOR_LOAD             \
    (DISP2UNIT_EVT__COUNT + 0U)

#endif // _TASK_PERF_CF_H

