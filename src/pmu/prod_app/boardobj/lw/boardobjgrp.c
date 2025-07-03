/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    boardobjgrp.c
 * @copydoc boardobjgrp.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "pmu/ssurface.h"

/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * @brief   Identifier for an empty array of BoardObj vtables.
 *
 * @note    Only to be used with calls to @ref BOARDOBJGRP_IFACE_MODEL_10_SET[_COMPACT]()
 *          to indicate that the given group does not have any vtables
 *          implemented for its class.
 */
BOARDOBJ_VIRTUAL_TABLE *BoardObjGrpVirtualTablesNotImplemented[0] = {};

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Static Asserts ---------------------------------- */

// Make sure the enum LW2080_CTRL_BOARDOBJGRP_CLASS_ID hasn't exceeded its size
ct_assert(LW2080_CTRL_BOARDOBJGRP_CLASS_ID__MAX < (sizeof(LW2080_CTRL_BOARDOBJGRP_CLASS_ID) << 8));

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Helper function for BOARDOBJGRP_OBJ_GET macro
 *
 * The bulk of the logic for BOARDOBJGRP_OBJ_GET is placed in this helper
 * function to reduce overall memory footprint.  Callers only need to
 * exit with an error code if BOARDOBJGRP_OBJ_GET returns NULL.
 *
 * @param[in]   pBoardObjGrp  BOARDOBJGRP_GRP_GET(class)
 * @param[in]   objIdx  Index of an object to retrieve
 *
 * @note   boardObjGrpObjGet is placed in the resident section.
 *
 * @return Pointer to the BOARDOBJ object at the specified index
 *         within the group, or NULL if index is invalid.
 */
BOARDOBJ *
boardObjGrpObjGet_IMPL
(
    BOARDOBJGRP  *pBoardObjGrp,
    LwBoardObjIdx objIdx
)
{
    if (objIdx >= pBoardObjGrp->objSlots)
    {
        return NULL;
    }

    return BOARDOBJGRP_OBJ_GET_BY_GRP_PTR(pBoardObjGrp, objIdx);
}

/* ------------------------ Private Functions ------------------------------- */
