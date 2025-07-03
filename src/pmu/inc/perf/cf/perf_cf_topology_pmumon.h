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
 * @file    perf_cf_topology_pmumon.h
 * @copydoc perf_cf_topology_pmumon.c
 */

#ifndef PERF_CF_TOPOLOGY_PMUMON_H
#define PERF_CF_TOPOLOGY_PMUMON_H

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
#define PERF_CF_TOPOLOGIES_PMUMON_ACTIVE_POWER_CALLBACK_PERIOD_US()                   \
    (100000U)

/*!
 * @brief   Period between samples while in low power callback mode.
 *
 * @todo    Replace hardcoding of 1s with vbios specified sampling rate.
 */
#define PERF_CF_TOPOLOGIES_PMUMON_LOW_POWER_CALLBACK_PERIOD_US()                      \
    (1000000U)
/* ------------------------- Types Definitions ------------------------------ */
/*!
 * All state required for PERF_CF_TOPOLOGIES PMUMON feature.
 */
typedef struct _PERF_CF_TOPOLOGIES_PMUMON
{
    /*!
     * Queue descriptor for PERF_CF_TOPOLOGIES PMUMON queue.
     */
    PMUMON_QUEUE_DESCRIPTOR queueDescriptor;

    /*!
     * Scratch buffer with sample. Used to relieve stack pressure.
     */
    LW2080_CTRL_PERF_PMUMON_PERF_CF_TOPOLOGIES_SAMPLE sample;

    /*!
     * Scratch buffer of topology status data. Placed here to relieve stack
     * pressure.
     */
    PERF_CF_TOPOLOGYS_STATUS status;
} PERF_CF_TOPOLOGIES_PMUMON;

/* ------------------------- Function Prototypes ---------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief      Constructor for PERF_CF_TOPOLOGIES PMUMON interface.
 *
 * @return     FLCN_OK if successful.
 */
FLCN_STATUS perfCfTopologiesPmumonConstruct(void)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "perfCfTopologiesPmumonConstruct");

/*!
 * @brief      Creates and schedules the tmr callback responsible for publishing
 *             PMUMON updates.
 *
 * @return     FLCN_OK on success.
 */
FLCN_STATUS perfCfTopologiesPmumonLoad(void)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "perfCfTopologiesPmumonLoad");

/*!
 * @brief      Cancels the tmr callback that was scheduled during load.
 */
void perfCfTopologiesPmumonUnload(void)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "perfCfTopologiesPmumonUnload");

/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#endif // PERF_CF_TOPOLOGY_PMUMON_H
