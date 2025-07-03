/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_limit_35_10.h
 * @brief   Arbitration interface file for P-States 3.5 version 1.0.
 */

#ifndef PERF_LIMIT_35_10_H
#define PERF_LIMIT_35_10_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/35/perf_limit_35.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_LIMIT_35_10  PERF_LIMIT_35_10;
typedef struct PERF_LIMITS_35_10 PERF_LIMITS_35_10;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief An individual limit's state for P-states 3.5 version 1.0.
 *
 * Contains data about the behavior of the individual limit, including clock
 * domains to strictly limit, client input, and input into the arbitration
 * process.
 */
struct PERF_LIMIT_35_10
{
    /*!
     * Base class. Must always be first.
     */
    PERF_LIMIT_35 super;

    /*!
     * @brief Mask of clock domains to strictly limit. One bit for each clock
     * domain index.
     *
     * When the bit for a domain is not set, the arbiter will propagate as
     * loosely as possible. For example, a clock will only be adjusted when it
     * is outside of the permissible range for the P-state range. When the bit
     * for a domain is set, the arbiter will find matching frequencies across
     * domains using ratios or V/F intersects.
     */
    BOARDOBJGRPMASK_E32 clkDomainStrictPropagationMask;
};

/*!
 * @brief Container of 3.5 version 1.0 limits.
 */
struct PERF_LIMITS_35_10
{
    /*!
     * Super class.  Must always be first.
     */
    PERF_LIMITS_35 super;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// PERF_LIMITS interfaces
FLCN_STATUS perfLimitsConstruct_35_10 (PERF_LIMITS **ppLimits)
    // Called only at init time -> init overlay.
    GCC_ATTRIB_SECTION("imem_libPerfInit", "perfLimitsConstruct_35_10");

// BOARDOBJ Interfaces
BoardObjGrpIfaceModel10ObjSet (perfLimitGrpIfaceModel10ObjSetImpl_35_10)
    GCC_ATTRIB_SECTION("imem_libPerfLimitBoardobj", "perfLimitGrpIfaceModel10ObjSetImpl_35_10");

/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @copydoc perfLimit35ClkDomainStrictPropagationMaskGet
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfLimit35ClkDomainStrictPropagationMaskGet_10
(
    PERF_LIMIT_35          *pLimit35,
    BOARDOBJGRPMASK_E32    *pClkDomainStrictPropagationMask
)
{
    PERF_LIMIT_35_10   *pLimit35V10 = (PERF_LIMIT_35_10 *)pLimit35;
    FLCN_STATUS         status;

    if ((pLimit35V10 == NULL) ||
        (pClkDomainStrictPropagationMask == NULL))
    {
         PMU_BREAKPOINT();
         status = FLCN_ERR_ILWALID_ARGUMENT;
         goto perfLimit35ClkDomainStrictPropagationMaskGet_10_exit;
    }

    boardObjGrpMaskInit_E32(pClkDomainStrictPropagationMask);
    status = boardObjGrpMaskCopy(pClkDomainStrictPropagationMask,
                                &pLimit35V10->clkDomainStrictPropagationMask);
    if (status != FLCN_OK)
    {
         PMU_BREAKPOINT();
         goto perfLimit35ClkDomainStrictPropagationMaskGet_10_exit;
    }

perfLimit35ClkDomainStrictPropagationMaskGet_10_exit:
    return status;
}

/* ------------------------ Include Derived Types --------------------------- */

#endif // PERF_LIMIT_35_10_H
