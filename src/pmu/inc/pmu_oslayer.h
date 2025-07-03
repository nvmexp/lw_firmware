/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OSLAYER_H
#define PMU_OSLAYER_H

/*!
 * @file    pmu_oslayer.h
 * @copydoc pmu_oslayer.c
 */

/* ------------------------ System Includes --------------------------------- */
#include "lwoslayer.h"
#include "lwostask.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ External Definitions ---------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */

#ifdef DYNAMIC_FLCN_PRIV_LEVEL
void vApplicationFlcnPrivLevelSet(LwU8 privLevelExt, LwU8 privLevelCsb)
    GCC_ATTRIB_SECTION("imem_resident", "vApplicationFlcnPrivLevelSet");
void vApplicationFlcnPrivLevelReset(void)
    GCC_ATTRIB_SECTION("imem_resident", "vApplicationFlcnPrivLevelReset");
#endif // DYNAMIC_FLCN_PRIV_LEVEL
#ifdef FLCNDBG_ENABLED
    void vApplicationIntOnBreak(void)
        GCC_ATTRIB_SECTION("imem_resident", "vApplicationIntOnBreak");
#endif // FLCNDBG_ENABLED

#if defined(DMA_SUSPENSION) && defined(ON_DEMAND_PAGING_BLK)
# define appTaskCriticalEnter       appTaskCriticalEnter_FUNC
# define appTaskCriticalExit        appTaskCriticalExit_FUNC
# define appTaskSchedulerSuspend    appTaskSchedulerSuspend_FUNC

void appTaskCriticalEnter_FUNC(void)
    GCC_ATTRIB_SECTION("imem_resident", "appTaskCriticalEnter");
void appTaskCriticalExit_FUNC(void)
    GCC_ATTRIB_SECTION("imem_resident", "appTaskCriticalExit");
void appTaskSchedulerSuspend_FUNC(void)
    GCC_ATTRIB_SECTION("imem_resident", "appTaskSchedulerSuspend");
#else // if defined(DMA_SUSPENSION) && defined(ON_DEMAND_PAGING_BLK)
# define appTaskCriticalEnter       lwrtosTaskCriticalEnter
# define appTaskCriticalExit        lwrtosTaskCriticalExit
# define appTaskSchedulerSuspend    lwrtosTaskSchedulerSuspend
#endif // if defined(DMA_SUSPENSION) && defined(ON_DEMAND_PAGING_BLK)

#define appTaskSchedulerResume      lwrtosTaskSchedulerResume

#define appTaskCriticalResEnter     lwrtosTaskCriticalEnter
#define appTaskCriticalResExit      lwrtosTaskCriticalExit

#define appTaskSchedulerResSuspend  lwrtosTaskSchedulerSuspend
#define appTaskSchedulerResResume   lwrtosTaskSchedulerResume

#ifdef DMA_REGION_CHECK
LwBool vApplicationIsDmaIdxPermitted(LwU8 dmaIdx)
    GCC_ATTRIB_SECTION("imem_resident", "vApplicationIsDmaIdxPermitted");
#endif // DMA_REGION_CHECK

/* ------------------------ Defines ----------------------------------------- */

#define OSTASK_ENABLED(name)                                                   \
    PMUCFG_FEATURE_ENABLED(PMUTASK_##name)

/*!
 * It is critically important that tasks raise their priorityto this
 * priority-level (or greater) before calling @see lwosDmaSuspend.
 */
#define OSTASK_PRIORITY_DMA_SUSPENDED    (lwrtosTASK_PRIORITY_IDLE + 6)

/*!
 *  Tasks that run during VBLANK require higher priority to minimize chance that
 *  they will get preempted and not finish on time (before VBLANK expires).
 */
#define OSTASK_PRIORITY_VBLANK           (lwrtosTASK_PRIORITY_IDLE + 6)

/*!
 * RTOS supports priority levels 0 to (lwrtosTASK_PRIORITY_MAX - 1), where 0 is
 * lowest priority possible. Lwrrently  lwrtosTASK_PRIORITY_MAX is defined as 8
 */
#define OSTASK_PRIORITY_MAX_POSSIBLE     (lwrtosTASK_PRIORITY_MAX - 1)

#endif // PMU_OSLAYER_H
