/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_limit_35_10.c
 * @copydoc perf_limit_35_10.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "dmemovl.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objperf.h"

#include "pmu_objclk.h"
#include "volt/objvolt.h"

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * PERF_LIMIT_35_10 implementation
 *
 * @copydoc PerfLimitsConstruct
 */
FLCN_STATUS
perfLimitsConstruct_35_10
(
    PERF_LIMITS **ppLimits
)
{
    PERF_LIMITS_35_10  *pLimits35V10;
    FLCN_STATUS         status = FLCN_OK;

    pLimits35V10 = lwosCallocType(OVL_INDEX_DMEM(perfLimit), 1U, PERF_LIMITS_35_10);
    if (pLimits35V10 == NULL)
    {
        PMU_HALT();
        status = FLCN_ERR_NO_FREE_MEM;
        goto perfLimitsConstruct_35_10_exit;
    }
    *ppLimits = &pLimits35V10->super.super;

    //
    // Set PERF_LIMITS version as super class uses this information to
    // call version specific interfaces.
    //
    pLimits35V10->super.super.version = LW2080_CTRL_PERF_PERF_LIMITS_VERSION_35_10;

    // Construct the PERF_LIMITS_35 SUPER object
    status = perfLimitsConstruct_35(ppLimits);
    if (status != FLCN_OK)
    {
        PMU_HALT();
        goto perfLimitsConstruct_35_10_exit;
    }

perfLimitsConstruct_35_10_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfLimitGrpIfaceModel10ObjSetImpl_35_10
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pDesc
)
{
    RM_PMU_PERF_PERF_LIMIT_35_10 *pLimit35V10Desc;
    PERF_LIMIT_35_10             *pLimit35V10;
    FLCN_STATUS                   status;

    // Call super-class constructor.
    status = perfLimitGrpIfaceModel10ObjSetImpl_35(pModel10, ppBoardObj, size, pDesc);
    if (status != FLCN_OK)
    {
        goto perfLimitGrpIfaceModel10ObjSetImpl_35_10_exit;
    }
    pLimit35V10     = (PERF_LIMIT_35_10 *)*ppBoardObj;
    pLimit35V10Desc = (RM_PMU_PERF_PERF_LIMIT_35_10 *)pDesc;

    // Set PERF_LIMIT_35_10 parameters.
    boardObjGrpMaskInit_E32(&(pLimit35V10->clkDomainStrictPropagationMask));
    status = boardObjGrpMaskImport_E32(&(pLimit35V10->clkDomainStrictPropagationMask),
                                       &(pLimit35V10Desc->clkDomainStrictPropagationMask));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfLimitGrpIfaceModel10ObjSetImpl_35_10_exit;
    }

perfLimitGrpIfaceModel10ObjSetImpl_35_10_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */

