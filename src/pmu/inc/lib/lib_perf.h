/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_LIB_PERF_H
#define PMU_LIB_PERF_H

/*!
 * @file    lib_perf.h
 * @copydoc lib_perf.c
 */

/* ------------------------ System Includes -------------------------------- */
#include "lwostimer.h"
#include "unit_api.h"

/* ------------------------ Application Includes --------------------------- */
#include "perf/vpstate.h"

/* ------------------------ Types Definitions ------------------------------ */
/*!
 * Retrieves the latest Domain Group value (pstate index for the Pstate Domain
 * Group, frequency for decoupled Domain Groups) sent from the RM.  This is the
 * current state of the Domain Group.
 *
 * @param[in] domGrpIdx  Index of the requested Domain Group.
 *
 * @return Domain Group value.
 */
#define PerfDomGrpGetValue(fname) LwU32 (fname) (LwU8 domGrpIdx)

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/*!
 * Enumeration of PERF Domain Groups.
 *
 * _PSTATE - Domain Group for Pstate/MCLK.  On pstates 1.0 boards, this is the
 *     only supported domain group.
 * _GPC2CLK - Domain Group lead by GPC2CLK (also usually includes HUBCLK,
 *     XBARCLK, LTC2CLK).  This is the first Domain Group added with Pstates
 *     2.0.
 * _XBARCLK - Domain Group lead by XBARCLK
 *
 */
#define PERF_DOMAIN_GROUP_PSTATE                RM_PMU_PERF_DOMAIN_GROUP_PSTATE
#define PERF_DOMAIN_GROUP_GPC2CLK               RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK
#define PERF_DOMAIN_GROUP_XBARCLK               RM_PMU_PERF_DOMAIN_GROUP_XBARCLK

/*!
 * Maximum number Domain Groups supported.
 *
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_DOMAIN_GROUP_LIMITS_XBAR))
#define PERF_DOMAIN_GROUP__LAST                 PERF_DOMAIN_GROUP_XBARCLK
#else
#define PERF_DOMAIN_GROUP__LAST                 PERF_DOMAIN_GROUP_GPC2CLK
#endif

/*!
 * Maximum number Domain Groups supported.
 *
 */
#define PERF_DOMAIN_GROUP_MAX_GROUPS            (PERF_DOMAIN_GROUP__LAST+1U)

/* -------------------------- Compile time checks -------------------------- */
ct_assert(PERF_DOMAIN_GROUP_MAX_GROUPS <= RM_PMU_PERF_DOMAIN_GROUP_MAX_GROUPS);

/*!
 * This structure specifies the set of Domain Group Limits PMGR wishes to
 * enforce.
 */
typedef struct
{
    /*!
     * Domain-Group specific value (pstate index for pstate domain group, frequency
     * of leading clock domain for decoupled domains) to which the PMU specifies the
     * RM should limit the set of Domain Groups supported on this GPU.
     */
    LwU32 values[PERF_DOMAIN_GROUP_MAX_GROUPS];
} PERF_DOMAIN_GROUP_LIMITS;

/*!
 * Helper inline function to copy PERF_DOMAIN_GROUP_LIMITS to 
 * RM_PMU structure RM_PMU_PERF_DOMAIN_GROUP_LIMITS.
 *
 * @param[out] pLimitsOut Pointer to RM_PMU_PERF_DOMAIN_GROUP_LIMITS.
 * @param[in]  pLimitsIn  Pointer to PERF_DOMAIN_GROUP_LIMITS.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
perfDomGrpLimitsCopyOut
(
    RM_PMU_PERF_DOMAIN_GROUP_LIMITS *pLimitsOut,
    PERF_DOMAIN_GROUP_LIMITS        *pLimitsIn
)
{
    LwU8 idx;
    for (idx = 0; idx < PERF_DOMAIN_GROUP_MAX_GROUPS; idx++)
    {
        pLimitsOut->values[idx] = pLimitsIn->values[idx];
    }
    for (; idx < RM_PMU_PERF_DOMAIN_GROUP_MAX_GROUPS; idx++)
    {
        pLimitsOut->values[idx] = RM_PMU_PREF_DOMAIN_GROUP_LIMIT_VALUE_DISABLE;
    }
}

/*!
 * Helper inline function to copy RM_PMU_PERF_DOMAIN_GROUP_LIMITS to 
 * internal PMU structure PERF_DOMAIN_GROUP_LIMITS.
 *
 * @param[out] pLimitsOut Pointer to PERF_DOMAIN_GROUP_LIMITS.
 * @param[in]  pLimitsIn  Pointer to RM_PMU_PERF_DOMAIN_GROUP_LIMITS.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
perfDomGrpLimitsCopyIn
(
    PERF_DOMAIN_GROUP_LIMITS        *pLimitsOut,
    RM_PMU_PERF_DOMAIN_GROUP_LIMITS *pLimitsIn
)
{
    LwU8 idx;
    for (idx = 0; idx < PERF_DOMAIN_GROUP_MAX_GROUPS; idx++)
    {
        pLimitsOut->values[idx] = pLimitsIn->values[idx];
    }
}
/* ------------------------ Function Prototypes ---------------------------- */
FLCN_STATUS perfVPstatePackVpstateToDomGrpLimits(VPSTATE *pVpstate, PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits)
    GCC_ATTRIB_SECTION("imem_perfVf", "perfVPstatePackVpstateToDomGrpLimits");
PerfDomGrpGetValue  (perfDomGrpGetValue)
    GCC_ATTRIB_SECTION("imem_perfVf", "perfDomGrpGetValue");

#include "perf/2x/perfps20.h"
#include "perf/3x/perfps30.h"

#endif // PMU_LIB_PERF_H

