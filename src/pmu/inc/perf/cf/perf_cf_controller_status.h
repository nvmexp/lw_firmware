/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_cf_controller_status.h
 * @brief   PERF_CF Controller status defines.
 *          This is a seperate header to remove cirlwlar dependency
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_CONTROLLER_STATUS_H
#define PERF_CF_CONTROLLER_STATUS_H

/* ------------------------ System Includes --------------------------------- */
#include "boardobj/boardobjgrp.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
/*!
 * Structure storing statuses of all PERF_CF_CONTROLLERS.
 */
typedef struct
{
    /*!
     * Mask of requested controllers, subset of the mask of all (valid) controllers.
     */
    BOARDOBJGRPMASK_E32 mask;
    /*!
     * Array of PERF_CF_CONTROLLER statuses.
     */
    RM_PMU_PERF_CF_CONTROLLERS_2X_BOARDOBJ_GET_STATUS_UNION
                        controllers[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_MAX_OBJECTS];
} PERF_CF_CONTROLLERS_STATUS,
*PPERF_CF_CONTROLLERS_STATUS;

/*!
 * Union of @ref PERF_CF_CONTROLLER STATUSes supported by
 * @ref PERF_CF_CONTROLLERS_UTIL_2X
 */
typedef union
{
    RM_PMU_BOARDOBJ                                 boardObj;       // virtual
    RM_PMU_PERF_CF_CONTROLLER_GET_STATUS            controller;     // virtual
    RM_PMU_PERF_CF_CONTROLLER_UTIL_GET_STATUS       controllerUtil;
    RM_PMU_PERF_CF_CONTROLLER_UTIL_2X_GET_STATUS    controllerUtil2x;
} PERF_CF_CONTROLLERS_REDUCED_BOARDOBJ_GET_STATUS_UNION;

/*!
 * Structure storing statuses of PERF_CF_CONTROLLER (Reduced Size) controllers.
 */
typedef struct
{
    /*!
     * Mask of requested controllers, subset of the mask of all (valid) controllers.
     */
    BOARDOBJGRPMASK_E32 mask;
    /*!
     * Array of PERF_CF_CONTROLLER_REDUCED statuses.
     */
    PERF_CF_CONTROLLERS_REDUCED_BOARDOBJ_GET_STATUS_UNION
                        controllers[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_MAX_OBJECTS];
} PERF_CF_CONTROLLERS_REDUCED_STATUS;

/* ------------------------- Inline Functions ------------------------------- */

/*!
 * @brief   Initializes a newly created @ref PWR_POLICIES_REDUCED_STATUS structure
 *
 * @param[out]  pPerfCfControllersStatus  Pointer to structure to initialize
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  pPerfCfControllersStatus is NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfControllersReducedStatusInit
(
    PERF_CF_CONTROLLERS_REDUCED_STATUS *pPerfCfControllersStatus
)
{
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        pPerfCfControllersStatus != NULL ? FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllersReducedStatusInit_exit);

    boardObjGrpMaskInit_E32(&pPerfCfControllersStatus->mask);

perfCfControllersReducedStatusInit_exit:
    return status;
}

#endif //PERF_CF_CONTROLLER_STATUS_H
