/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_PERFMON_H
#define PMU_PERFMON_H

/*!
 * @file    pmu_perfmon.h
 * @copydoc task_perfmon.c
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "main.h"
#include "unit_api.h"
#include "cmdmgmt/cmdmgmt.h"

/* ------------------------ Function Prototypes  --------------------------- */
void perfmonPreInit(void)
    GCC_ATTRIB_SECTION("imem_init", "perfmonPreInit");

/* ------------------------ Defines ---------------------------------------- */
#if (PMUCFG_FEATURE_ENABLED(PMU_PERFMON_NO_RESET_COUNTERS))
/*!
 * Idle counter overflow bit
 */
#define PMU_COUNTER_VALUE_OVERFLOW BIT(31)

/*!
 * Compute effective count if counter has wrapped around
 */
#define PMU_GET_EFF_COUNTER_VALUE(lwr, prev) \
    (((lwr) - (prev)) & ~PMU_COUNTER_VALUE_OVERFLOW)
#endif // PMU_PERFMON_NO_RESET_COUNTERS

/* ------------------------ Types definitions ------------------------------ */
/*!
 * History structure. Count keeps track of previously read idle counter value.
 */
typedef struct
{
    LwU32   count;
} RM_PMU_PERFMON_COUNTER_HIST;

/*!
 * @brief Context information for the Perfmon task.
 *
 * Due to alignment requirements, to optimize space efficiency, pointers
 * should be placed as the first attributes of the structure.
 */
typedef struct
{
    /*!
     * Samples are represented in this buffer as:
     *     %base-counter * samplesInMovingAvg
     */
    LwU16                       *pSampleBuffer;

    /*! Counter-specific state. */
    RM_PMU_PERFMON_COUNTER      *pCounters;

#if (PMUCFG_FEATURE_ENABLED(PMU_PERFMON_NO_RESET_COUNTERS))
    /*! Counter-specific state. */
    RM_PMU_PERFMON_COUNTER_HIST *pCountersHist;

    /*! Last sampled base count */
    LwU32                        baseCount;
#endif // PMU_PERFMON_NO_RESET_COUNTERS

#if (PMUCFG_FEATURE_ENABLED(PMU_PERFMON_TIMER_BASED_SAMPLING))
    /*! Freq at which counters are updated. */
    LwU32                        countersFreqMHz;
#endif

    /*! Conselwtive samples before decrease event. */
    LwU8                         toDecreaseCount;

    /*! Current number of conselwtive lower threshold violations. */
    LwU8                         decreaseCount[RM_PMU_DOMAIN_GROUP_NUM];

    /*! Highest utilization (0.01%) since lower threshold violations. */
    LwU16                        decreasePct[RM_PMU_DOMAIN_GROUP_NUM];

    /*! Index of the counter used as the base counter. */
    LwU8                         baseCounterId;

    /*! Number of samples to keep in moving average callwlations. */
    LwU8                         samplesInMovingAvg;

    /*! Last state-id received by the RM. */
    LwU8                         stateId[RM_PMU_DOMAIN_GROUP_NUM];

    /*! Number of counters to use, specified by the RM */
    LwU8                         numCounters;

    /*!
     * @brief Described in rmpmucmdif.h under rm_pmu_perfmon_cmd_start_t.flags
     *
     * Lwrrently only used as an event mask to enable/disable events.
     */
    LwU8                         flags[RM_PMU_DOMAIN_GROUP_NUM];

    /*! Has the init command been received? */
    LwBool                       bInitialized;
} PERFMON_CONTEXT;

/*!
 * Enumeration of PERFMON task's PMU_OS_TIMER callback entries.
 */
typedef enum
{
    /*!
     * Entry for regular perfmon sampling.
     */
    PERFMON_OS_TIMER_ENTRY_SAMPLE = 0x0,
    /*!
     * Must always be the last entry.
     */
    PERFMON_OS_TIMER_ENTRY_NUM_ENTRIES,
} PERFMON_OS_TIMER_ENTRIES;

/* ------------------------ Global Variables ------------------------------- */
extern PERFMON_CONTEXT  PerfmonContext;
extern OS_TIMER         PerfmonOsTimer;
extern OS_TMR_CALLBACK  OsCBPerfMon;

#endif // PMU_PERFMON_H
