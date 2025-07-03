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
 * @file    perf_limit_40.c
 * @copydoc perf_limit_40.h
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
 * PERF_LIMIT_40 implementation
 *
 * @copydoc PerfLimitsConstruct
 */
FLCN_STATUS
perfLimitsConstruct_40
(
    PERF_LIMITS **ppLimits
)
{
    PERF_LIMITS_40 *pLimits40;
    FLCN_STATUS     status = FLCN_OK;

    pLimits40 = lwosCallocType(OVL_INDEX_DMEM(perfLimit), 1U, PERF_LIMITS_40);
    if (pLimits40 == NULL)
    {
        PMU_HALT();
        status = FLCN_ERR_NO_FREE_MEM;
        goto perfLimitsConstruct_40_exit;
    }
    *ppLimits = &pLimits40->super.super;

    //
    // Set PERF_LIMITS version as super class uses this information to
    // call version specific interfaces.
    //
    pLimits40->super.super.version = LW2080_CTRL_PERF_PERF_LIMITS_VERSION_40;

    // Construct the PERF_LIMITS_35 SUPER object
    status = perfLimitsConstruct_35(ppLimits);
    if (status != FLCN_OK)
    {
        PMU_HALT();
        goto perfLimitsConstruct_40_exit;
    }

perfLimitsConstruct_40_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfLimitGrpIfaceModel10ObjSetImpl_40
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pDesc
)
{
    RM_PMU_PERF_PERF_LIMIT_40 *pLimit40Desc;
    PERF_LIMIT_40             *pLimit40;
    FLCN_STATUS                status;

    // Call super-class constructor.
    status = perfLimitGrpIfaceModel10ObjSetImpl_35(pModel10, ppBoardObj, size, pDesc);
    if (status != FLCN_OK)
    {
        goto perfLimitGrpIfaceModel10ObjSetImpl_40_exit;
    }
    pLimit40     = (PERF_LIMIT_40 *)*ppBoardObj;
    pLimit40Desc = (RM_PMU_PERF_PERF_LIMIT_40 *)pDesc;

    // Set PERF_LIMIT_40 parameters.
    (void)pLimit40;
    (void)pLimit40Desc;

perfLimitGrpIfaceModel10ObjSetImpl_40_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */

