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
 * @file    perf_cf_controller_2x.h
 * @brief   PERF_CF Controllers 2X related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_CONTROLLER_2X_H
#define PERF_CF_CONTROLLER_2X_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_controller.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_CONTROLLERS_2X   PERF_CF_CONTROLLERS_2X;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @copydoc perfCfControllerIfaceModel10SetHeader_SUPER
 */
#define perfCfControllerIfaceModel10SetHeader_2X \
    perfCfControllerIfaceModel10SetHeader_SUPER

/*!
 * @copydoc perfCfControllerIfaceModel10GetStatusHeader_SUPER
 */
#define perfCfControllerIfaceModel10GetStatusHeader_2X \
    perfCfControllerIfaceModel10GetStatusHeader_SUPER

/*!
 * @copydoc perfCfControllerIfaceModel10GetStatus_UTIL_2X
 */
#define perfCfControllerGetReducedStatus_UTIL_2X \
    perfCfControllerIfaceModel10GetStatus_UTIL_2X

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Collection of all PERF_CF Controllers and related information.
 */
struct PERF_CF_CONTROLLERS_2X
{
    /*!
     * Super class. This should always be the first member!
     */
    PERF_CF_CONTROLLERS   super;
    /*!
     * Mask of subset of controllers supported by 
     * @ref PERF_CF_CONTROLLERS_REDUCED_STATUS
     */
     BOARDOBJGRPMASK_E32  controllersReducedMask;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// BOARDOBJGRP interfaces.
BoardObjGrpIfaceModel10CmdHandler       (perfCfControllerBoardObjGrpIfaceModel10Set_2X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerBoardObjGrpIfaceModel10Set_2X");
BoardObjGrpIfaceModel10CmdHandler       (perfCfControllerBoardObjGrpIfaceModel10GetStatus_2X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerBoardObjGrpIfaceModel10GetStatus_2X");
BoardObjIfaceModel10GetStatus               (perfCfControllerIfaceModel10GetStatus_2X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerIfaceModel10GetStatus_2X");

// PERF_CF_CONTROLLERS interfaces
PerfCfControllersPostInit   (perfCfControllersPostInit_2X)
    GCC_ATTRIB_SECTION("imem_perfCfInit", "perfCfControllersPostInit_2X");
FLCN_STATUS perfCfControllersGetReducedStatus_2X(PERF_CF_CONTROLLERS_2X *pControllers2x, PERF_CF_CONTROLLERS_REDUCED_STATUS *pStatus)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllersGetReducedStatus_2X");

/* ------------------------ Inline Functions -------------------------------- */
/* ------------------------ Include Derived Types --------------------------- */

#endif // PERF_CF_CONTROLLER_2X_H
