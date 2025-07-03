/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  fanpolicy_v30.c
 * @brief FAN Fan Policy Model V30
 *
 * This module is a collection of functions managing and manipulating state
 * related to fan policies in the Fan Policy Table V30.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "fan/objfan.h"
#include "main.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Constructor for the FAN_POLICY_V30 super-class.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
fanPolicyGrpIfaceModel10ObjSetImpl_V30
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    FLCN_STATUS status;

    // Call super-class implementation.
    status = fanPolicyGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto fanPolicyGrpIfaceModel10ObjSetImpl_V30_exit;
    }

fanPolicyGrpIfaceModel10ObjSetImpl_V30_exit:
    return status;
}

/*!
 * FAN_POLICY_V30 implementation.
 *
 * @copydoc FanPolicyLoad
 */
FLCN_STATUS
fanPolicyLoad_V30
(
    FAN_POLICY *pPol
)
{
    FLCN_STATUS status;

    // Call super-class implementation.
    status = fanPolicyLoad_SUPER(pPol);
    if (status != FLCN_OK)
    {
        goto fanPolicyLoad_V30_exit;
    }
fanPolicyLoad_V30_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
