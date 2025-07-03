/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_cf_controller_mem_tune_2x.h
 * @brief   Memory tunning v2.x PERF_CF Controller related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_CONTROLLER_MEM_TUNE_2X_H
#define PERF_CF_CONTROLLER_MEM_TUNE_2X_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_controller.h"
#include "g_lwconfig.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_CONTROLLER_MEM_TUNE_2X   PERF_CF_CONTROLLER_MEM_TUNE_2X;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * PERF_CF_CONTROLLER child class providing attributes of
 * memory tuning v2.x PERF_CF Controller.
 */
struct PERF_CF_CONTROLLER_MEM_TUNE_2X
{
    /*!
     * PERF_CF_CONTROLLER parent class.
     * Must be first element of the structure!
     */
    PERF_CF_CONTROLLER      super;

    // TODO-Chandrashis: Introduce new fields.

};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet   (perfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE_2X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE_2X");
BoardObjIfaceModel10GetStatus       (perfCfControllerIfaceModel10GetStatus_MEM_TUNE_2X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerIfaceModel10GetStatus_MEM_TUNE_2X");

// PERF_CF_CONTROLLER interfaces.
PerfCfControllerExelwte     (perfCfControllerExelwte_MEM_TUNE_2X);
PerfCfControllerLoad        (perfCfControllerLoad_MEM_TUNE_2X)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "perfCfControllerLoad_MEM_TUNE_2X");
PerfCfControllerSetMultData (perfCfControllerSetMultData_MEM_TUNE_2X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerSetMultData_MEM_TUNE_2X");

/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @brief      Interface to check whether MEM_TUNE_2X controller is enabled.
 *
 * @return  LW_TRUE     if enabled
 * @return  LW_FALSE    if disabled
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwBool
perfCfControllerMemTune2xIsEnabled(void)
{
    return (LWCFG(GLOBAL_FEATURE_GR1129_MEM_TUNE_2X_CONTROLLER) &&
            PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MEM_TUNE_2X));
}

/* ------------------------ Include Derived Types --------------------------- */

#endif // PERF_CF_CONTROLLER_MEM_TUNE_2X_H
