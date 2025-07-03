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
 * @file    thrmchannel_pmumon.h
 * @copydoc thrmchannel_pmumon.c
 */
#ifndef THRMCHANNEL_PMUMON_H
#define THRMCHANNEL_PMUMON_H

/* ------------------------- System Includes -------------------------------- */
#include "lib/lib_pmumon.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Macros ----------------------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/*!
 * @brief Period in nanoseconds between samples while in active power callback
 * mode.
 *
 * @todo Replace hard-coded value of 100ms with VBIOS specified sampling rate.
 */
#define THERM_CHANNEL_PMUMON_ACTIVE_POWER_CALLBACK_PERIOD_US()         (100000U)

/*!
 * @brief Period in nanoseconds between samples while in low power callback
 * mode.
 *
 * @todo Replace hardcoded value of 1s with VBIOS specified sampling rate.
 */
#define THERM_CHANNEL_PMUMON_LOW_POWER_CALLBACK_PERIOD_US()           (1000000U)

/* ------------------------- Types Definitions ------------------------------ */
/*!
 * @brief All state required for THERM_CHANNEL PMUMON feature.
 */
typedef struct _THERM_CHANNEL_PMUMON
{
    /*!
     * @brief Queue descriptor for THERM_CHANNEL PMUMON queue.
     */
    PMUMON_QUEUE_DESCRIPTOR queueDescriptor;

    /*!
     * @brief Scratch buffer with sample. Used to relieve stack pressure.
     */
    LW2080_CTRL_THERM_PMUMON_THERM_CHANNEL_SAMPLE sample;
} THERM_CHANNEL_PMUMON;

/* ------------------------- Function Prototypes ---------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Constructor for THRM_CHANNEL PMUMON interface.
 *
 * @return FLCN_OK if successful.
 */
FLCN_STATUS thermChannelPmumonConstruct(void)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermChannelsPmumonConstruct");

/*!
 * @brief Creates and schedules the tmr callback responsible for publishing
 * PMUMON updates.
 *
 * @return FLCN_OK on success.
 */
FLCN_STATUS thermChannelPmumonLoad(void);

/*!
 * @brief Cancels the tmr callback that was scheduled during load.
 */
void thermChannelPmumonUnload(void);

/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#endif // THRMCHANNEL_PMUMON_H
