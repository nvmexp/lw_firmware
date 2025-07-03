/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pstate_3x.h
 * @brief   Declarations, type definitions, and macros for the PSTATE 3X SW
 *          interface.
 */

#ifndef PSTATE_3X_H
#define PSTATE_3X_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/pstate.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PSTATE_3X PSTATE_3X;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief       PSTATE_3X implementation of @ref BoardObjGrpIfaceModel10ObjSet().
 *
 * Symbolic wrapper for @ref perfPstateGrpIfaceModel10ObjSet_SUPER.
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 *
 * @memberof    PSTATE_3X
 *
 * @protected
 */
#define perfPstateIfaceModel10Set_3X(...) perfPstateGrpIfaceModel10ObjSet_SUPER(__VA_ARGS__)

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */

/*!
 * @brief Common interface for PSTATE 3.0 and later PSTATE objects.
 *
 * @extends PSTATE
 */
struct PSTATE_3X
{
    /*!
     * @brief PSTATE parent class.
     *
     * Must be first element of the structure!
     *
     * @protected
     */
    PSTATE super;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/* ------------------------ Include Derived Types --------------------------- */
#include "perf/30/pstate_30.h"
#include "perf/35/pstate_35.h"

/* ------------------------- End of File ------------------------------------ */
#endif // PSTATE_3X_H
