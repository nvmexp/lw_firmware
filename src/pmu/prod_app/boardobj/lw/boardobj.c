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
 * @file    boardobj.c
 * @copydoc boardobj.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "lwos_dma.h"
#include "dmemovl.h"

/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Inline Functions -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjDynamicCast()
 *
 * @note    Failures reported from within this function or its object interface
 *          call should be considered as static errors that indicate an error in
 *          development.
 */
void *
boardObjDynamicCast_IMPL
(
    BOARDOBJGRP                        *pBoardObjGrp,
    BOARDOBJ                           *pBoardObj,
    LW2080_CTRL_BOARDOBJGRP_CLASS_ID    requestedClass,
    LwU8                                requestedType
)
{
    void *pObject = NULL;

    if ((pBoardObjGrp == NULL) ||
        (pBoardObj    == NULL))
    {
        PMU_BREAKPOINT();
        goto boardObjDynamicCast_IMPL_exit;
    }

    //
    // Check to see that the type-cast is within the same class as the object
    // and that the object has the same class as its group.
    //
    if ((BOARDOBJ_GET_CLASS_ID(pBoardObj) != pBoardObjGrp->classId) ||
        (BOARDOBJ_GET_CLASS_ID(pBoardObj) != requestedClass))
    {
        PMU_BREAKPOINT();
        goto boardObjDynamicCast_IMPL_exit;
    }

    // Check to see that type-cast is within the object's class hierarchy.
    BOARDOBJ_VIRTUAL_TABLE_DYNAMIC_CAST(pObject,
                                        pBoardObjGrp,
                                        pBoardObj,
                                        requestedType);

boardObjDynamicCast_IMPL_exit:
    return pObject;
}

/* ------------------------ Private Functions ------------------------------- */
