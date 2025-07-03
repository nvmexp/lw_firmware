/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

 /*!
 * @file    perfcfpmsensors_pmumon.h
 * @copydoc perfcfpmsensors_pmumon.c
 */

#ifndef PERF_CF_PM_SENSORS_PMUMON_H
#define PERF_CF_PM_SENSORS_PMUMON_H

/* ------------------------- System Includes -------------------------------- */
#include "lib/lib_pmumon.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Macros ----------------------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/*!
 * @brief   Period between samples while in active power callback mode.
 *
 * @todo    Replace hardcoding of 50ms with vbios specified sampling rate.
 */
#define PERF_CF_PM_SENSORS_PMUMON_ACTIVE_POWER_CALLBACK_PERIOD_US()                   \
    (50000U)

/*!
 * @brief   Period between samples while in low power callback mode.
 *
 * @todo    Replace hardcoding of 1s with vbios specified sampling rate.
 */
#define PERF_CF_PM_SENSORS_PMUMON_LOW_POWER_CALLBACK_PERIOD_US()                      \
    (1000000U)
/* ------------------------- Types Definitions ------------------------------ */
/*!
 * All state required for PERF_CF_SENSORS PMUMON feature.
 */
typedef struct _PERF_CF_PM_SENSORS_PMUMON
{
    /*!
     * Queue descriptor for PERF_CF_SENSORS PMUMON queue.
     */
    PMUMON_QUEUE_DESCRIPTOR queueDescriptor[LW2080_CTRL_PERF_PMUMON_PERF_CF_PM_SENSORS_SAMPLE_BOARDOBJ_ENABLED_COUNT];

    /*!
     * Scratch buffer with sample. Used to relieve stack pressure.
     */
    LW2080_CTRL_PERF_PMUMON_PERF_CF_PM_SENSORS_SAMPLE sample;
    
    /*!
     * Scratch buffer of Sensor status data. Placed here to relieve stack
     * pressure.
     */
    PERF_CF_PM_SENSORS_STATUS status;

    /*!
     * Boolean tracking whether this is the first time load. This is required to
     */
    LwBool bIsFirstLoad;
} PERF_CF_PM_SENSORS_PMUMON;

/* ------------------------- Function Prototypes ---------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief      Constructor for PERF_CF_SENSORS PMUMON interface.
 *
 * @return     FLCN_OK if successful.
 */
FLCN_STATUS perfCfPmSensorsPmumonConstruct(void)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "perfCfPmSensorsPmumonConstruct");

/*!
 * @brief      Creates and schedules the tmr callback responsible for publishing
 *             PMUMON updates.
 *
 * @return     FLCN_OK on success.
 */
FLCN_STATUS perfCfPmSensorsPmumonLoad(void)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "perfCfPmSensorsPmumonLoad");

/*!
* @brief      Cancels the tmr callback that was scheduled during load.
*/
void perfCfPmSensorsPmumonUnload(void)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "perfCfPmSensorsPmumonUnload");
/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#endif // CLK_DOMAIN_PMUMON_H
