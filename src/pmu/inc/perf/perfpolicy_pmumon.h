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
 * @file    perfpolicy_pmumon.h
 * @copydoc perfpolicy_pmumon.c
 */
#ifndef PERFPOLICY_PMUMON_H
#define PERFPOLICY_PMUMON_H

/* ------------------------- System Includes -------------------------------- */
#include "lib/lib_pmumon.h"

/* ------------------------- Application Includes --------------------------- */
#include "ctrl/ctrl2080/ctrl2080perf.h"

/* ------------------------- Macros ----------------------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/*!
 * @brief Period in microseconds between samples while in active power callback
 * mode.
 *
 * @todo Replace hard-coded value of 100ms with VBIOS specified sampling rate.
 */
#define PERF_POLICY_PMUMON_ACTIVE_POWER_CALLBACK_PERIOD_US()           (100000U)

/*!
 * @brief Period in microseconds between samples while in low power callback
 * mode.
 *
 * @todo Replace hardcoded value of 1s with VBIOS specified sampling rate.
 */
#define PERF_POLICY_PMUMON_LOW_POWER_CALLBACK_PERIOD_US()             (1000000U)

/* ------------------------- Types Definitions ------------------------------ */
/*!
 * @brief All state required for PERF_POLICY PMUMON feature.
 */
typedef struct _PERF_POLICY_PMUMON
{
    /*!
     * @brief Queue descriptor for PERF_POLICY PMUMON queue.
     */
    PMUMON_QUEUE_DESCRIPTOR queueDescriptor;

    /*!
     * @brief Scratch buffer with sample. Used to relieve stack pressure.
     */
    RM_PMU_PERF_PMUMON_PERF_POLICIES_SAMPLE sample;
} PERF_POLICY_PMUMON;

/* ------------------------- Function Prototypes ---------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Constructor for THRM_CHANNEL PMUMON interface.
 *
 * @return FLCN_OK if successful.
 */
FLCN_STATUS perfPolicyPmumonConstruct(void)
    GCC_ATTRIB_SECTION("imem_libPerfPolicyBoardObj", "perfPolicyPmumonConstruct");

/*!
 * @brief Creates and schedules the tmr callback responsible for publishing
 * PMUMON updates.
 *
 * @return FLCN_OK on success.
 */
FLCN_STATUS perfPolicyPmumonLoad(void)
    GCC_ATTRIB_SECTION("imem_libPerfPolicyBoardObj", "perfPolicyPmumonLoad");

/*!
 * @brief Cancels the tmr callback that was scheduled during load.
 */
void perfPolicyPmumonUnload(void)
    GCC_ATTRIB_SECTION("imem_libPerfPolicyBoardObj", "perfPolicyPmumonUnload");

/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#endif // PERFPOLICY_PMUMON_H
