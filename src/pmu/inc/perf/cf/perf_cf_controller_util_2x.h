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
 * @file    perf_cf_controller_util_2x.h
 * @brief   Utilization 2.x PERF_CF Controller related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_CONTROLLER_UTIL_2X_H
#define PERF_CF_CONTROLLER_UTIL_2X_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_controller_util.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_CONTROLLER_UTIL_2X PERF_CF_CONTROLLER_UTIL_2X;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * PERF_CF_CONTROLLER child class providing attributes of
 * Utilization 2.x PERF_CF Controller.
 */
struct PERF_CF_CONTROLLER_UTIL_2X
{
    /*!
     * Utilization PERF_CF_CONTROLLER parent class.
     * Must be first element of the structure!
     */
    PERF_CF_CONTROLLER_UTIL super;
    /*!
     * Number of IIR samples above target before increasing clocks.
     */
    LwU8                    IIRCountInc;
    /*!
     * Number of IIR samples below target before decreasing clocks.
     */
    LwU8                    IIRCountDec;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet   (perfCfControllerGrpIfaceModel10ObjSetImpl_UTIL_2X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerGrpIfaceModel10ObjSetImpl_UTIL_2X");
BoardObjIfaceModel10GetStatus       (perfCfControllerIfaceModel10GetStatus_UTIL_2X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerIfaceModel10GetStatus_UTIL_2X");

// PERF_CF_CONTROLLER interfaces.
PerfCfControllerExelwte     (perfCfControllerExelwte_UTIL_2X);
PerfCfControllerLoad        (perfCfControllerLoad_UTIL_2X)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "perfCfControllerLoad_UTIL_2X");
PerfCfControllerSetMultData (perfCfControllerSetMultData_UTIL_2X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerSetMultData_UTIL_2X");

/* ------------------------ Include Derived Types --------------------------- */

#endif // PERF_CF_CONTROLLER_UTIL_2X_H
