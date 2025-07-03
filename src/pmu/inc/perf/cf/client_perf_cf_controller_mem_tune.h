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
 * @file    client_perf_cf_controller_mem_tune.h
 * @brief   Memory tunning CLIENT_PERF_CF Controller related defines.
 *
 * @copydoc client_perf_cf.h
 */

#ifndef CLIENT_PERF_CF_CONTROLLER_MEM_TUNE_H
#define CLIENT_PERF_CF_CONTROLLER_MEM_TUNE_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/client_perf_cf_controller.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct CLIENT_PERF_CF_CONTROLLER_MEM_TUNE CLIENT_PERF_CF_CONTROLLER_MEM_TUNE;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */

/*!
 * CLIENT_PERF_CF_CONTROLLER child class providing attributes of
 * memory tuning CLIENT_PERF_CF Controller.
 */
struct CLIENT_PERF_CF_CONTROLLER_MEM_TUNE
{
    /*!
     * CLIENT_PERF_CF_CONTROLLER parent class.
     * Must be first element of the structure!
     */
    CLIENT_PERF_CF_CONTROLLER   super;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet   (perfCfClientPerfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfClientPerfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE");
BoardObjIfaceModel10GetStatus       (perfCfClientPerfCfControllerIfaceModel10GetStatus_MEM_TUNE)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfClientPerfCfControllerIfaceModel10GetStatus_MEM_TUNE");

/* ------------------------ Include Derived Types --------------------------- */

#endif // CLIENT_PERF_CF_CONTROLLER_MEM_TUNE_H
