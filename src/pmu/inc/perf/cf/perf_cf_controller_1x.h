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
 * @file    perf_cf_controller.h
 * @brief   PERF_CF Controllers 1X related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_CONTROLLER_1X_H
#define PERF_CF_CONTROLLER_1X_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_controller.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_CONTROLLERS_1X   PERF_CF_CONTROLLERS_1X;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @copydoc perfCfControllerIfaceModel10SetHeader_SUPER
 */
#define perfCfControllerIfaceModel10SetHeader_1X \
    perfCfControllerIfaceModel10SetHeader_SUPER

/*!
 * @copydoc perfCfControllerIfaceModel10GetStatusHeader_SUPER
 */
#define perfCfControllerIfaceModel10GetStatusHeader_1X \
    perfCfControllerIfaceModel10GetStatusHeader_SUPER

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Collection of all PERF_CF Controllers and related information.
 */
struct PERF_CF_CONTROLLERS_1X
{
    /*!
     * Super class. This should always be the first member!
     */
    PERF_CF_CONTROLLERS super;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// BOARDOBJGRP interfaces.
BoardObjGrpIfaceModel10CmdHandler       (perfCfControllerBoardObjGrpIfaceModel10Set_1X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerBoardObjGrpIfaceModel10Set_1X");
BoardObjGrpIfaceModel10CmdHandler       (perfCfControllerBoardObjGrpIfaceModel10GetStatus_1X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerBoardObjGrpIfaceModel10GetStatus_1X");

BoardObjIfaceModel10GetStatus               (perfCfControllerIfaceModel10GetStatus_1X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerIfaceModel10GetStatus_1X");

// PERF_CF_CONTROLLERS interfaces
PerfCfControllersPostInit   (perfCfControllersPostInit_1X)
    GCC_ATTRIB_SECTION("imem_perfCfInit", "perfCfControllersPostInit_1X");

/* ------------------------ Inline Functions -------------------------------- */
/* ------------------------ Include Derived Types --------------------------- */

#endif // PERF_CF_CONTROLLER_1X_H
