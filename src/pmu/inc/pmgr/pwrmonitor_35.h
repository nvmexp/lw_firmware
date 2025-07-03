/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrmonitor_35.h
 * @brief @copydoc pwrmonitor_35.c
 */

#ifndef PWRMONITOR_35_H
#define PWRMONITOR_35_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrmonitor.h"
#include "task_pmgr.h"

/* ------------------------- Types Definitions ----------------------------- */
/*!
 * Structure extending the PWR_MONITOR object for PWR_CHANNEL_35.
 */
typedef struct
{
    /*
     * All PWR_MONITOR fields. Must be first element in this structure
     */
    PWR_MONITOR          super;
    /*!
     * RM advertized polling period that will be used by the PMU to poll the
     * channels internal to PMU [us].
     */
    LwU32                pollingPeriodus;
    /*!
     * Structure containing the last periodically polled channel status.
     */
    PWR_CHANNELS_STATUS *pPollingStatus;
} PWR_MONITOR_35;

/* ------------------------- Defines --------------------------------------- */
/*!
 * Helper macro to get pointer to PWR_MONITOR_35 object.
 *
 * CRPTODO - implement dynamic type-checking/casting.
 */
#define PWR_MONITOR_35_GET()                                                 \
    ((PWR_MONITOR_35 *)Pmgr.pPwrMonitor)

/* ------------------------- Function Prototypes  -------------------------- */
FLCN_STATUS pwrMonitorPolledTupleGet(LwU8 channelIdx, LW2080_CTRL_PMGR_PWR_TUPLE *pTuple)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrMonitorPolledTupleGet");

/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * Global PWR_MONITOR_35 interfaces
 */
PwrMonitorConstruct (pwrMonitorConstruct_35)
    GCC_ATTRIB_SECTION("imem_libPmgrInit", "pwrMonitorConstruct_35");

#endif // PWRMONITOR_35_H
