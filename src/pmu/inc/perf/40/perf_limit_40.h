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
 * @file    perf_limit_40.h
 * @brief   Arbitration interface file for P-States 4.0.
 */

#ifndef PERF_LIMIT_40_H
#define PERF_LIMIT_40_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/35/perf_limit_35.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_LIMIT_40  PERF_LIMIT_40;
typedef struct PERF_LIMITS_40 PERF_LIMITS_40;

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
struct PERF_LIMIT_40
{
    /*!
     * Base class. Must always be first.
     */
    PERF_LIMIT_35 super;
};

/*!
 * @brief Container of 3.5 version 1.0 limits.
 */
struct PERF_LIMITS_40
{
    /*!
     * Super class.  Must always be first.
     */
    PERF_LIMITS_35 super;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// PERF_LIMITS interfaces
FLCN_STATUS perfLimitsConstruct_40 (PERF_LIMITS **ppLimits)
    // Called only at init time -> init overlay.
    GCC_ATTRIB_SECTION("imem_libPerfInit", "perfLimitsConstruct_40");

// BOARDOBJ Interfaces
BoardObjGrpIfaceModel10ObjSet (perfLimitGrpIfaceModel10ObjSetImpl_40)
    GCC_ATTRIB_SECTION("imem_libPerfLimitBoardobj", "perfLimitGrpIfaceModel10ObjSetImpl_40");

/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @copydoc perfLimit35ClkDomainStrictPropagationMaskGet
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfLimit35ClkDomainStrictPropagationMaskGet_40
(
    PERF_LIMIT_35          *pLimit35,
    BOARDOBJGRPMASK_E32    *pClkDomainStrictPropagationMask
)
{
    PERF_LIMIT_40      *pLimit40 = (PERF_LIMIT_40 *)pLimit35;
    CLK_PROP_REGIME    *pPropRegime;
    FLCN_STATUS         status;

    if ((pLimit40 == NULL) ||
        (pClkDomainStrictPropagationMask == NULL))
    {
         PMU_BREAKPOINT();
         status = FLCN_ERR_ILWALID_ARGUMENT;
         goto perfLimit35ClkDomainStrictPropagationMaskGet_40_exit;
    }

    boardObjGrpMaskInit_E32(pClkDomainStrictPropagationMask);

    // Get clock propagation regime mask from regime id.
    pPropRegime = clkPropRegimesGetByRegimeId(pLimit40->super.super.propagationRegime);
    if (pPropRegime == NULL)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        PMU_BREAKPOINT();
        goto perfLimit35ClkDomainStrictPropagationMaskGet_40_exit;
    }

    status = boardObjGrpMaskCopy(pClkDomainStrictPropagationMask,
                                 clkPropRegimeClkDomainMaskGet(pPropRegime));
    if (status != FLCN_OK)
    {
         PMU_BREAKPOINT();
         goto perfLimit35ClkDomainStrictPropagationMaskGet_40_exit;
    }

perfLimit35ClkDomainStrictPropagationMaskGet_40_exit:
    return status;
}

/* ------------------------ Include Derived Types --------------------------- */

#endif // PERF_LIMIT_40_H
