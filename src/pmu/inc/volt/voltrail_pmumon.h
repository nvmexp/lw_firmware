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
 * @file    voltrail_pmumon.h
 * @copydoc voltrail_pmumon.c
 */

#ifndef VOLTRAIL_PMUMON_H
#define VOLTRAIL_PMUMON_H

/* ------------------------- System Includes -------------------------------- */
#include "lib/lib_pmumon.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Macros ----------------------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/*!
 * @brief   Period between samples while in active power callback mode.
 *
 * @todo    Replace hardcoding of 100ms with vbios specified sampling rate.
 */
#define VOLT_RAILS_PMUMON_ACTIVE_POWER_CALLBACK_PERIOD_US()                   \
    (100000U)

/*!
 * @brief   Period between samples while in low power callback mode.
 *
 * @todo    Replace hardcoding of 1s with vbios specified sampling rate.
 */
#define VOLT_RAILS_PMUMON_LOW_POWER_CALLBACK_PERIOD_US()                      \
    (1000000U)
/* ------------------------- Types Definitions ------------------------------ */
/*!
 * All state required for VOLT_RAILS PMUMON feature.
 */
typedef struct _VOLT_RAILS_PMUMON
{
    /*!
     * Queue descriptor for VOLT_RAILS PMUMON queue.
     */
    PMUMON_QUEUE_DESCRIPTOR queueDescriptor;

    /*!
     * VOLT_RAILS boardobjgrp index for lwvdd rail.
     */
    LwBoardObjIdx lwvddIdx;

    /*!
     * VOLT_RAILS boardobjgrp index for msvdd rail.
     */
    LwBoardObjIdx msvddIdx;

    /*!
     * Scratch buffer with sample. Used to relieve stack pressure.
     */
    LW2080_CTRL_VOLT_PMUMON_VOLT_RAILS_SAMPLE sample;
} VOLT_RAILS_PMUMON;

/* ------------------------- Function Prototypes ---------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief      Constructor for VOLT_RAILS PMUMON interface.
 *
 * @return     FLCN_OK if successful.
 */
FLCN_STATUS voltVoltRailsPmumonConstruct(void)
    GCC_ATTRIB_SECTION("imem_libVolt", "voltVoltRailsPmumonConstruct");

/*!
 * @brief      Creates and schedules the tmr callback responsible for publishing
 *             PMUMON updates.
 *
 * @return     FLCN_OK on success.
 */
FLCN_STATUS voltVoltRailsPmumonLoad(void)
    GCC_ATTRIB_SECTION("imem_libVolt", "voltVoltRailsPmumonLoad");

/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#endif // VOLTRAIL_PMUMON_H
