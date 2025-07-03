/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    task_pmgr.h
 * @copydoc task_pmgr.c
 */

#ifndef TASK_PMGR_H
#define TASK_PMGR_H

/* ------------------------- System Includes ------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes -------------------------- */
#include "unit_api.h"

/* ------------------------- Types Definitions ----------------------------- */

/*!
 * Interrupt notify command. Issued from THERM task.
 * Some power devices can trigger hardware slowdown, which will be captured by
 * THERM block. THERM block will send a command containing all its pending
 * interrupt to PMGR, which will dispatch this interrupt info to all its devices.
 *
 * This structure defines the command struct THERM could send to PMGR.
 */
typedef struct
{
    DISP2UNIT_HDR   hdr;    //<! Must be first element of the structure

    /*!
     * pending interrupt - the 32bit value in interrupt register.
     */
    LwU32           pendingIntr;
} DISPATCH_PMGR_NOTIFY;

/*!
 * PWR_CHANNEL query command. Issued from THERM task.
 * THERM/FAN task may request PWR_CHANNEL value for implementing
 * Fan Stop sub-policy.
 *
 * This structure defines the command struct THERM/FAN could send to PMGR.
 */
typedef struct
{
    DISP2UNIT_HDR   hdr;    //<! Must be first element of the structure

    /*!
     * Power Topology Index in the Power Topology Table. It describes the
     * PWR_CHANNEL whose power needs to be queried.
     */
    LwU8            pwrChIdx;

    /*!
     * Fan Policy index which requested for power described by @ref pwrChIdx.
     */
    LwU8            fanPolicyIdx;
} DISPATCH_PMGR_PWR_CHANNEL_QUERY;

/*!
 * Callback trigger command. Issued from PG task.
 * Due to PG (MSCG) restrictions, PG task cannot callwlate PSI current
 * periodically. PMGR task callwlates current if callback is scheduled
 * and updates DMEM. It is also responsible for waking up MSCG if current
 * exceeds threshold.
 *
 * This structure defines the command struct PG could send to PMGR
 */
typedef struct
{
    DISP2UNIT_HDR   hdr;    //<! Must be first element of the structure

    /*!
     * Bool to indicate PSI callback schedule/deschedule condition
     */
    LwBool          bSchedule;
} DISPATCH_PMGR_PG_CALLBACK_TRIGGER;

/*!
 * A union of all available commands to PMGR.
 */
typedef union
{
    DISP2UNIT_HDR       hdr;
    DISP2UNIT_RM_RPC    rpc;

    /*!
     * Notify message passed from THERM.
     */
    DISPATCH_PMGR_NOTIFY              notify;

    /*!
     * Query message passed from THERM.
     */
    DISPATCH_PMGR_PWR_CHANNEL_QUERY   query;

    /*!
     * Callback trigger message passed from PG
     */
    DISPATCH_PMGR_PG_CALLBACK_TRIGGER trigger;
} DISPATCH_PMGR;

/* ------------------------- Defines --------------------------------------- */
/*!
 * Event type for notification from THERM/FAN task.
 */
#define PMGR_EVENT_ID_THERM_PWR_CHANNEL_QUERY       (DISP2UNIT_EVT__COUNT + 0U)
/*!
 * Event type for notification from PG task
 */
#define PMGR_EVENT_ID_PG_CALLBACK_TRIGGER           (DISP2UNIT_EVT__COUNT + 1U)

/*!
 * Event type for notification from Perf task after voltage change
 */
#define PMGR_EVENT_ID_PERF_VOLT_CHANGE_NOTIFY       (DISP2UNIT_EVT__COUNT + 2U)

/*!
 * Event type for notification from Perf task after VF lwrve change
 */
#define PMGR_EVENT_ID_PERF_VF_ILWALIDATION_NOTIFY   (DISP2UNIT_EVT__COUNT + 3U)

/*!
 * Event type for notification from Therm task to service beacon interrupt
 */
#define PMGR_EVENT_ID_PWR_DEV_BEACON_INTERRUPT      (DISP2UNIT_EVT__COUNT + 4U)

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */

#endif // TASK_PMGR_H
