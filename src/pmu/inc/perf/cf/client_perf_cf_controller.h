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
 * @file    client_perf_cf_controller.h
 * @brief   Common CLIENT_PERF_CF Controller related defines.
 *
 * @copydoc client_perf_cf.h
 */

#ifndef CLIENT_PERF_CF_CONTROLLER_H
#define CLIENT_PERF_CF_CONTROLLER_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct CLIENT_PERF_CF_CONTROLLER CLIENT_PERF_CF_CONTROLLER, CLIENT_PERF_CF_CONTROLLER_BASE;

typedef struct CLIENT_PERF_CF_CONTROLLERS CLIENT_PERF_CF_CONTROLLERS;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Colwenience macro to look-up CLIENT_PERF_CF_CONTROLLERS.
 */
#define CLIENT_PERF_CF_GET_CONTROLLERS()                                      \
    (&(PERF_CF_GET_OBJ()->clientControllers))

/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_CLIENT_PERF_CF_CONTROLLER                   \
    (&(CLIENT_PERF_CF_GET_CONTROLLERS()->super.super))

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Virtual BOARDOBJ child providing attributes common to all
 * CLIENT_PERF_CF Controllers.
 */
struct CLIENT_PERF_CF_CONTROLLER
{
    /*!
     * BOARDOBJ super-class.
     * Must be first element of the structure!
     */
    BOARDOBJ        super;
    /*!
     * Index of internal controller mapped to this client controller.
     * @note This is private internal variable and must not be exposed
     *       via RMCTRLs.
     */
    LwBoardObjIdx   controllerIdx;
};

/*!
 * Collection of all CLIENT_PERF_CF Controllers and related information.
 */
struct CLIENT_PERF_CF_CONTROLLERS
{
    /*!
     * BOARDOBJGRP_E32 super class. This should always be the first member!
     */
    BOARDOBJGRP_E32 super;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// BOARDOBJGRP interfaces.
BoardObjGrpIfaceModel10CmdHandler                         (perfCfClientPerfCfControllerBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfClientPerfCfControllerBoardObjGrpIfaceModel10Set");
BoardObjGrpIfaceModel10CmdHandler                         (perfCfClientPerfCfControllerBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfClientPerfCfControllerBoardObjGrpIfaceModel10GetStatus");


// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet                             (perfCfClientPerfCfControllerGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfClientPerfCfControllerGrpIfaceModel10ObjSetImpl_SUPER");

BoardObjIfaceModel10GetStatus                                 (perfCfClientPerfCfControllerIfaceModel10GetStatus_SUPER)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfClientPerfCfControllerIfaceModel10GetStatus_SUPER");

// CLIENT_PERF_CF_CONTROLLER interfaces.

/* ------------------------ Inline Functions -------------------------------- */
/* ------------------------ Include Derived Types --------------------------- */
#include "perf/cf/client_perf_cf_controller_mem_tune.h"

#endif // CLIENT_PERF_CF_CONTROLLER_H
