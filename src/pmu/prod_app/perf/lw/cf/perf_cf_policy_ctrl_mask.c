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
 * @file    perf_cf_policy_ctrl_mask.c
 * @copydoc perf_cf_policy_ctrl_mask.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf.h"
#include "perf/cf/perf_cf_policy_ctrl_mask.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfCfPolicyGrpIfaceModel10ObjSetImpl_CTRL_MASK
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PERF_CF_POLICY_CTRL_MASK *pDescPolicy =
        (RM_PMU_PERF_CF_POLICY_CTRL_MASK *)pBoardObjDesc;
    PERF_CF_CONTROLLERS *pControllers = PERF_CF_GET_CONTROLLERS();
    PERF_CF_POLICY_CTRL_MASK   *pPolicyCtrlMask;
    BOARDOBJGRPMASK_E32         mask;
    LwBool                      bFirstConstruct = (*ppBoardObj == NULL);
    FLCN_STATUS                 status          = FLCN_OK;

    status = perfCfPolicyGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto perfCfPolicyGrpIfaceModel10ObjSetImpl_CTRL_MASK_exit;
    }
    pPolicyCtrlMask = (PERF_CF_POLICY_CTRL_MASK *)*ppBoardObj;

    // Process the controllers mask input.
    boardObjGrpMaskInit_E32(&mask);
    status = boardObjGrpMaskImport_E32(&mask, &(pDescPolicy->maskControllers));
    if (status != FLCN_OK)
    {
        goto perfCfPolicyGrpIfaceModel10ObjSetImpl_CTRL_MASK_exit;
    }

    if (!bFirstConstruct)
    {
        // We cannot allow subsequnet SET calls to change these parameters.
        if (!boardObjGrpMaskIsEqual(&pPolicyCtrlMask->maskControllers, &mask))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfCfPolicyGrpIfaceModel10ObjSetImpl_CTRL_MASK_exit;
        }
    }

    // Set member variables.
    boardObjGrpMaskInit_E32(&(pPolicyCtrlMask->maskControllers));
    status = boardObjGrpMaskCopy(&(pPolicyCtrlMask->maskControllers), &mask);
    if (status != FLCN_OK)
    {
        goto perfCfPolicyGrpIfaceModel10ObjSetImpl_CTRL_MASK_exit;
    }

    // Verify controller mask.
    if (!boardObjGrpMaskIsSubset(&(pPolicyCtrlMask->maskControllers),
                                 &(pControllers->super.objMask)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfPolicyGrpIfaceModel10ObjSetImpl_CTRL_MASK_exit;
    }

perfCfPolicyGrpIfaceModel10ObjSetImpl_CTRL_MASK_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfPolicyIfaceModel10GetStatus_CTRL_MASK
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_CF_POLICY_CTRL_MASK_GET_STATUS *pStatus =
        (RM_PMU_PERF_CF_POLICY_CTRL_MASK_GET_STATUS *)pPayload;
    PERF_CF_POLICY_CTRL_MASK *pPolicyCtrlMask =
        (PERF_CF_POLICY_CTRL_MASK *)pBoardObj;
    FLCN_STATUS status = FLCN_OK;

    (void)pStatus;
    (void)pPolicyCtrlMask;

    status = perfCfPolicyIfaceModel10GetStatus_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto perfCfPolicyIfaceModel10GetStatus_CTRL_MASK_exit;
    }

    // CTRL_MASK class specific implementation.

perfCfPolicyIfaceModel10GetStatus_CTRL_MASK_exit:
    return status;
}

/*!
 * @copydoc PerfCfPolicyActivate
 */
FLCN_STATUS
perfCfPolicyActivate_CTRL_MASK
(
    PERF_CF_POLICY         *pPolicy,
    LwBool                  bActivate,
    BOARDOBJGRPMASK_E32    *pMaskControllers
)
{
    PERF_CF_POLICY_CTRL_MASK *pPolicyCtrlMask =
        (PERF_CF_POLICY_CTRL_MASK *)pPolicy;
    FLCN_STATUS status = FLCN_OK;

    if (bActivate)
    {
        status = boardObjGrpMaskOr(pMaskControllers, pMaskControllers,
                                   &(pPolicyCtrlMask->maskControllers));
        if (status != FLCN_OK)
        {
            goto perfCfPolicyActivate_CTRL_MASK_exit;
        }
    }

perfCfPolicyActivate_CTRL_MASK_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
