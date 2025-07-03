/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJPERF_H
#define PMU_OBJPERF_H

#include "g_pmu_objperf.h"

#ifndef G_PMU_OBJPERF_H
#define G_PMU_OBJPERF_H

/*!
 * @file    pmu_objperf.h
 * @copydoc pmu_objperf.c
 */

/* ------------------------ System Includes -------------------------------- */
#include "volt/objvolt.h"
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE))
#include "perf/3x/vfe.h"
#endif
#include "perf/vpstate.h"
#include "perf/pstate.h"
#include "perf/changeseq.h"
#include "perf/perf_limit.h"
#include "perf/perfpolicy.h"
#include "lib/lib_perf.h"
#include "main.h"
#include "lib_semaphore.h"

/* ------------------------ Application Includes --------------------------- */
#include "config/g_perf_hal.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Macros for read-write semaphore implementation to synchronize access to perf
 * structs - PSTATE tuple, CLK VF Lwrve, Last Completed Change.
 *
 * This implementation uses the RW semaphore library implementation.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_SEMAPHORE))
    #define perfSemaphoreRWInitialize()                                        \
            OS_SEMAPHORE_CREATE_RW(OVL_INDEX_DMEM(perf))

    #define perfReadSemaphoreTake()                                            \
            OS_SEMAPHORE_RW_TAKE_RD(Perf.pPerfSemaphoreRW);

    #define perfReadSemaphoreGive()                                            \
            OS_SEMAPHORE_RW_GIVE_RD(Perf.pPerfSemaphoreRW);

    #define perfWriteSemaphoreTake()                                           \
            OS_SEMAPHORE_RW_TAKE_WR(Perf.pPerfSemaphoreRW);

    #define perfWriteSemaphoreGive()                                           \
            OS_SEMAPHORE_RW_GIVE_WR(Perf.pPerfSemaphoreRW);
#else
    #define perfSemaphoreRWInitialize()     NULL
    #define perfReadSemaphoreTake()         lwosNOP()
    #define perfReadSemaphoreGive()         lwosNOP()
    #define perfWriteSemaphoreTake()        lwosNOP()
    #define perfWriteSemaphoreGive()        lwosNOP()
#endif

/* ------------------------ Types Definitions ------------------------------ */
/*!
 * PERF Object Definition.
 */
typedef struct
{
    /*!
     * The latest set of Domain Group Limits the PMU sent to the RM.  These are
     * the current domain group limits the PMU is enforcing on the GPU.
     */
    PERF_DOMAIN_GROUP_LIMITS domGrpLimits[RM_PMU_PERF_DOMAIN_GROUP_LIMITS_ID_NUM];
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_ELW_PSTATES_2X)
    /*!
     * The latest PERF state received from the RM.  This represents the current
     * PERF-related state of the GPU.
     */
    RM_PMU_PERF_STATUS  status;
    /*!
     * @copydoc PERF_TABLES
     */
    PERF_TABLES         tables;
#endif

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_3X))
    /*!
     * @copydoc PSTATES
     */
    PSTATES             pstates;
    /*!
     * @copydoc PERF_PSTATE_STATUS
     * TODO: Move this to PMU_PERF_PSTATES_30-specific section when replacement is created for 3.5
     */
    PERF_PSTATE_STATUS  status;
#endif

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VPSTATES))
    /*!
     * @copydoc VPSTATES
     */
    VPSTATES            vpstates;
#endif // (PMUCFG_FEATURE_ENABLED(PMU_PERF_VPSTATES))

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE))
    /*!
     * @copydoc VFE
     */
    VFE                *pVfe;
#endif

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ))
    /*!
     * @copydoc CHANGE_SEQS
     */
    CHANGE_SEQS         changeSeqs;
#endif

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_SEMAPHORE))
    /*!
     * Read/Write semaphore used for synchronizing access to the Perf object.
     * Notably: PSTATE tuples, CLK VF Lwrve, Last Completed Change
     */
    PSEMAPHORE_RW       pPerfSemaphoreRW;
#endif

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PERF_LIMIT))
    /*!
     * @copydoc PERF_LIMITS
     */
    PERF_LIMITS        *pLimits;
#endif

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_POLICY))
    /*!
     * @copydoc PERF_POLICIES
     */
    PERF_POLICIES      *pPolicies;
#endif
} OBJPERF;

/*!
 * Enumeration of PERF task's PMU_OS_TIMER callback entries.
 */
typedef enum
{
    /*!
     * VFE evaluation callback.
     */
    PERF_OS_TIMER_ENTRY_VFE_CALLBACK = 0,

    /*!
     * SW clock counter callback. This callback will run to accumulate HW
     * counters value in the SW counters.
     */
    PERF_OS_TIMER_ENTRY_SW_CLK_CNTR_CALLBACK,

    /*!
     * ADC voltage sampling callback. This callback will run to cache ADC
     * ADC_ACC and ADC_NUM_SAMPLES register values for all ADC devices.
     */
    PERF_OS_TIMER_ENTRY_ADC_VOLTAGE_SAMPLE_CALLBACK,

    /*!
     * Frequency controller callback.
     */
    PERF_OS_TIMER_ENTRY_CLK_FREQ_CONTROLLER_CALLBACK,

    /*!
     * Callback to keep track of the average frequencies for NAFLL domains
     */
    PERF_OS_TIMER_ENTRY_CLK_FREQ_EFF_AVG_CALLBACK,

    /*!
     * Voltage controller callback.
     */
    PERF_OS_TIMER_ENTRY_CLK_VOLT_CONTROLLER_CALLBACK,

    /*!
     * Must always be the last entry.
     */
    PERF_OS_TIMER_ENTRY_NUM_ENTRIES
} PERF_OS_TIMER_ENTRIES;

/* ------------------------ External Definitions --------------------------- */
extern OBJPERF              Perf;
extern OS_TIMER             PerfOsTimer;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
FLCN_STATUS perfPostInit(void)
    // Called when the perf task is schedule for the first time.
    GCC_ATTRIB_SECTION("imem_libPerfInit", "perfPostInit");

#endif // G_PMU_OBJPERF_H
#endif // PMU_OBJPERF_H

