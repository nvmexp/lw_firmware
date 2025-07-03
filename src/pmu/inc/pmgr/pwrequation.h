/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrequation.h
 * @brief @copydoc pwrequation.c
 */

#ifndef PWREQUATION_H
#define PWREQUATION_H

/* ------------------------- System Includes -------------------------------- */
#include "boardobj/boardobjgrp.h"

/* ------------------------- Application Includes --------------------------- */
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PWR_EQUATION PWR_EQUATION, PWR_EQUATION_BASE;

/* ------------------------------ Macros ------------------------------------ */
/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_PWR_EQUATION \
    (&(Pmgr.pwr.equations.super))

/*!
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define PWR_EQUATION_GET(_objIdx) \
    (BOARDOBJGRP_OBJ_GET(PWR_EQUATION, (_objIdx)))

/* ------------------------- Types Definitions ------------------------------ */

/*!
 * Main structure representing a power equation.
 */
struct PWR_EQUATION
{
    /*!
     * BOARDOBJ super-class.
     */
    BOARDOBJ super;

};

/* ------------------------- Macros ----------------------------------------- */
/* ------------------------- Function Prototypes  --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * PWR_EQUATION interfaces
 */

/*!
 * Constructor for the PWR_EQUATION super class
 */
BoardObjGrpIfaceModel10ObjSet (pwrEquationGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrEquationGrpIfaceModel10ObjSetImpl_SUPER");

/*!
 * Board Object interfaces.
 */
//
// PMGR is running short of IMEM, having a all construct overlay
// attached together will hit the IMEM limit on pre-pascal chips.
//
BoardObjGrpIfaceModel10CmdHandler (pwrEquationBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_pmgrLibBoardObj", "pwrEquationBoardObjGrpIfaceModel10Set");
BoardObjGrpIfaceModel10SetHeader  (pmgrPwrEquationIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pmgrPwrEquationIfaceModel10SetHeader");
BoardObjGrpIfaceModel10SetEntry   (pmgrPwrEquationIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pmgrPwrEquationIfaceModel10SetEntry");

/* ------------------------- Debug Macros ----------------------------------- */
/* ------------------------- Child Class Includes --------------------------- */
#include "pmgr/pwrequation_ba1x_scale.h"
#include "pmgr/pwrequation_ba15_scale.h"
#include "pmgr/pwrequation_leakage.h"
#include "pmgr/pwrequation_dynamic.h"

#endif // PWREQUATION_H
