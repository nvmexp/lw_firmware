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
 * @file    fancooler_pmumon.h
 * @copydoc fancooler_pmumon.c
 */
#ifndef FANCOOLER_PMUMON_H
#define FANCOOLER_PMUMON_H

/* ------------------------- System Includes -------------------------------- */
#include "lib/lib_pmumon.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Macros ----------------------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/*!
 * @brief Period in microseconds between samples while in active power callback
 * mode.
 *
 * @todo Replace hard-coded value of 100ms with VBIOS specified sampling rate.
 */
#define FAN_COOLER_PMUMON_ACTIVE_POWER_CALLBACK_PERIOD_US()            (100000U)

/*!
 * @brief Period in microseconds between samples while in low power callback
 * mode.
 *
 * @todo Replace hardcoded value of 1s with VBIOS specified sampling rate.
 */
#define FAN_COOLER_PMUMON_LOW_POWER_CALLBACK_PERIOD_US()              (1000000U)

/* ------------------------- Types Definitions ------------------------------ */
/*!
 * @brief All state required for FAN_COOLER PMUMON feature.
 */
typedef struct _FAN_COOLER_PMUMON
{
    /*!
     * @brief Queue descriptor for FAN_COOLER PMUMON queue.
     */
    PMUMON_QUEUE_DESCRIPTOR queueDescriptor;

    /*!
     * @brief Scratch buffer with sample. Used to relieve stack pressure.
     */
    RM_PMU_FAN_PMUMON_FAN_COOLERS_SAMPLE sample;
} FAN_COOLER_PMUMON;

/* ------------------------- Function Prototypes ---------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Constructor for THRM_CHANNEL PMUMON interface.
 *
 * @return FLCN_OK if successful.
 */
FLCN_STATUS fanCoolerPmumonConstruct(void)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommonConstruct", "fanCoolerPmumonConstruct");

/*!
 * @brief Creates and schedules the tmr callback responsible for publishing
 * PMUMON updates.
 *
 * @return FLCN_OK on success.
 */
FLCN_STATUS fanCoolerPmumonLoad(void)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommon", "fanCoolerPmumonLoad");

/*!
 * @brief Cancels the tmr callback that was scheduled during load.
 */
void fanCoolerPmumonUnload(void)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommon", "fanCoolerPmumonUnload");

/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#endif // FANCOOLER_PMUMON_H
