/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_GC6_H
#define PMU_GC6_H

/*!
 * @file pmu_gc6.h
 */

/* ------------------------- System Includes ------------------------------- */
#include "cmdmgmt/cmdmgmt.h"
#include "pmu/pmuifgc6.h"
#include "fan/fancooler.h"
#include "fan/objfan.h"

#define LW_GC6_RTD3_PWM_VALUE      (0)
/* ------------------------- Types Definitions ----------------------------- */

/*!
 * Data used during GC6 exit. TODO: New engine object?
 */
typedef struct
{
    RM_PMU_GC6_CTX        *pCtx;
    RM_PMU_GC6_BSOD_CTX   *pBsodCtx;
    RM_PMU_GC6_EXIT_STATS *pStats;
    LwU32                  gc6EntryStatusMask;
} PMU_GC6;

// Link state
#define LINK_ILWALID            0x0
#define LINK_IN_L2_DISABLED     0x1
#define LINK_IN_L1              0x2
#define LINK_D3_HOT_CYCLE       0x3

/* ------------------------- Function Prototypes  -------------------------- */
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_MULTI_FAN)
void pgGc6SmartFanOvlAttachAndLoad(void)
    GCC_ATTRIB_SECTION("imem_lpwr", "pgGc6SmartFanOvlAttachAndLoad");
#endif

void pgGc6RTD3PreEntryConfig(LwU16 gc6DetachMask)
    GCC_ATTRIB_SECTION("imem_lpwrLoadin", "pgGc6RTD3PreEntryConfig");

void pgGc6LoggingStats(LwU16 gc6DetachMask)
    GCC_ATTRIB_SECTION("imem_lpwrLoadin", "pgGc6LoggingStats");

void pgGc6DetachedMsgSend(void)
    GCC_ATTRIB_SECTION("imem_lpwrLoadin", "pgGc6DetachedMsgSend");

void pgGc6LogErrorStatus(LwU32 status)
    GCC_ATTRIB_SECTION("imem_rtd3Entry", "pgGc6LogErrorStatus");

void rtd3Gc6EntryActionPrepare(RM_PMU_GC6_DETACH_DATA *pGc6Detach)
    GCC_ATTRIB_SECTION("imem_lpwrLoadin", "rtd3Gc6EntryActionPrepare");

void rtd3Gc6EntryActionBeforeLinkL2(RM_PMU_GC6_DETACH_DATA *pGc6Detach)
    GCC_ATTRIB_SECTION("imem_rtd3Entry", "rtd3Gc6EntryActionBeforeLinkL2");

void rtd3Gc6EntryActionAfterLinkL2(RM_PMU_GC6_DETACH_DATA *pGc6Detach)
    GCC_ATTRIB_SECTION("imem_rtd3Entry", "rtd3Gc6EntryActionAfterLinkL2");

void rtd3Gc6EntryActionBeforeIslandTrigger(RM_PMU_GC6_DETACH_DATA *pGc6Detach)
    GCC_ATTRIB_SECTION("imem_rtd3Enry", "rtd3Gc6EntryActionBeforeIslandTrigger");

/* ------------------------ Global Variables ------------------------------- */
extern PMU_GC6 PmuGc6;

/*****************************************************************************/

#endif //PMU_GC6_H

