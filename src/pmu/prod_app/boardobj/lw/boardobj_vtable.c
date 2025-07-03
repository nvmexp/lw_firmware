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
 * @file    boardobj_vtable.c
 * @copydoc boardobj_vtable.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobj_vtable.h"

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief   Board Object Vtable implementation of @ref BoardObjGrpIfaceModel10ObjSet().
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
boardObjVtableGrpIfaceModel10ObjSet
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJ_VTABLE    *pBoardObjVtable;
    FLCN_STATUS         status;

    //
    // Call into BOARDOBJ super-constructor, which will do actual memory
    // allocation.
    //
    PMU_HALT_OK_OR_GOTO(status,
        boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc),
        boardObjVtableGrpIfaceModel10ObjSet_exit);

    pBoardObjVtable = (BOARDOBJ_VTABLE *)*ppBoardObj;

    // Init the BOARDOBJ_VTABLE-specific data to default values.
    pBoardObjVtable->pVirtualTable = NULL;

boardObjVtableGrpIfaceModel10ObjSet_exit:
    return status;
}
/* ------------------------ Private Functions ------------------------------- */
