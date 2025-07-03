/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    boardobj_interface.c
 * @copydoc boardobj_interface.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief   BOARDOBJ_INTERFACE constructor.
 *
 * @copydetails BoardObjInterfaceConstruct()
 */
FLCN_STATUS
boardObjInterfaceConstruct
(
    BOARDOBJGRP                *pBObjGrp,
    BOARDOBJ_INTERFACE         *pInterface,
    RM_PMU_BOARDOBJ_INTERFACE  *pInterfaceDesc,
    INTERFACE_VIRTUAL_TABLE    *pVirtualTable
)
{
    pInterface->pVirtualTable = pVirtualTable;

    return FLCN_OK;
}

/*!
 * @copydoc BoardObjInterfaceGetBoardObjFromInterface()
 */
BOARDOBJ *
boardObjInterfaceGetBoardObjFromInterface
(
    BOARDOBJ_INTERFACE *pInterface
)
{
    if (pInterface == NULL)
    {
        return NULL;
    }
    else
    {
        return (BOARDOBJ *)(((char *)pInterface) - pInterface->pVirtualTable->offset);
    }
}

/* ------------------------ Private Functions ------------------------------- */
