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
 * @file   pwrpolicyrel.c
 * @brief  Interface specification for a PWR_POLICY_RELATIONSHIP.
 *
 * Power Policy Relationships are objects to model how
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrpolicyrel.h"
#include "dbgprintf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry  
 */
FLCN_STATUS
pmgrPwrPolicyRelIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    switch (pBuf->type)
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_TYPE_WEIGHT:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_RELATIONSHIP_WEIGHT);
            return pwrPolicyRelationshipGrpIfaceModel10ObjSetImpl_WEIGHT(pModel10, ppBoardObj,
                sizeof(PWR_POLICY_RELATIONSHIP_WEIGHT), pBuf);

        case LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_TYPE_BALANCE:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_RELATIONSHIP_BALANCE);
            return pwrPolicyRelationshipGrpIfaceModel10ObjSetImpl_BALANCE(pModel10, ppBoardObj,
                sizeof(PWR_POLICY_RELATIONSHIP_BALANCE), pBuf);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc PwrPolicyRelationshipValueLwrrGet
 */
LwU32
pwrPolicyRelationshipValueLwrrGet
(
    PWR_POLICY_RELATIONSHIP *pPolicyRel
)
{
    switch (BOARDOBJ_GET_TYPE(pPolicyRel))
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_TYPE_WEIGHT:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_RELATIONSHIP_WEIGHT);
            return pwrPolicyRelationshipValueLwrrGet_WEIGHT(pPolicyRel);
    }

    return 0;
}

/*!
 * @copydoc PwrPolicyRelationshipLimitLwrrGet
 */
LwU32
pwrPolicyRelationshipLimitLwrrGet
(
    PWR_POLICY_RELATIONSHIP *pPolicyRel
)
{
    switch (BOARDOBJ_GET_TYPE(pPolicyRel))
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_TYPE_WEIGHT:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_RELATIONSHIP_WEIGHT);
            return pwrPolicyRelationshipLimitLwrrGet_WEIGHT(pPolicyRel);
    }

    return 0;
}

/*!
 * @copydoc PwrPolicyRelationshipLimitInputGet
 */
LwU32
pwrPolicyRelationshipLimitInputGet
(
    PWR_POLICY_RELATIONSHIP  *pPolicyRel,
    LwU8                      policyIdx
)
{
    switch (BOARDOBJ_GET_TYPE(pPolicyRel))
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_TYPE_WEIGHT:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_RELATIONSHIP_WEIGHT);
            return pwrPolicyRelationshipLimitInputGet_WEIGHT(pPolicyRel, policyIdx);
    }

    return LW_U8_MIN;
}

/*!
 * @copydoc PwrPolicyRelationshipLimitInputSet
 */
FLCN_STATUS
pwrPolicyRelationshipLimitInputSet
(
    PWR_POLICY_RELATIONSHIP  *pPolicyRel,
    LwU8                      policyIdx,
    LwBool                    bDryRun,
    LwU32                    *pLimit
)
{
    switch (BOARDOBJ_GET_TYPE(pPolicyRel))
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_TYPE_WEIGHT:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_RELATIONSHIP_WEIGHT);
            return pwrPolicyRelationshipLimitInputSet_WEIGHT(pPolicyRel,
                        policyIdx, bDryRun, pLimit);
    }

    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc PwrPolicyRelationshipLoad
 */
FLCN_STATUS
pwrPolicyRelationshipLoad
(
    PWR_POLICY_RELATIONSHIP *pPolicyRel
)
{
    FLCN_STATUS status = FLCN_OK;

    switch (BOARDOBJ_GET_TYPE(pPolicyRel))
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_TYPE_BALANCE:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_RELATIONSHIP_BALANCE);
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPwm)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                status = pwrPolicyRelationshipLoad_BALANCE(pPolicyRel);
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            break;
        }
    }

    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
pwrPolicyRelationshipIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    FLCN_STATUS status = FLCN_OK;

    switch (pBoardObj->type)
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_TYPE_BALANCE:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_RELATIONSHIP_BALANCE);
            status = pwrPolicyRelationshipIfaceModel10GetStatus_BALANCE(pModel10, pPayload);
            break;
    }
    if (status != FLCN_OK)
    {
        goto pwrPolicyRelationshipIfaceModel10GetStatus_done;
    }

    status = boardObjIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto pwrPolicyRelationshipIfaceModel10GetStatus_done;
    }

pwrPolicyRelationshipIfaceModel10GetStatus_done:
    return status;
}

/*!
 * Super-class implementation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrPolicyRelationshipGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PMGR_PWR_POLICY_RELATIONSHIP_BOARDOBJ_SET *pSet =
        (RM_PMU_PMGR_PWR_POLICY_RELATIONSHIP_BOARDOBJ_SET *)pBoardObjDesc;
    PWR_POLICY_RELATIONSHIP                          *pPolicyRel;
    FLCN_STATUS                                       status;

    // Make sure the specified power channel (PWR_CHANNEL) is available.
    if ((Pmgr.pwr.policies.objMask.super.pData[0] & BIT(pSet->policyIdx)) == 0)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NOT_SUPPORTED;
        goto pwrPolicyRelationshipGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto pwrPolicyRelationshipGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }
    pPolicyRel = (PWR_POLICY_RELATIONSHIP *)*ppBoardObj;

    pPolicyRel->pPolicy = PWR_POLICY_GET(pSet->policyIdx);
    if (pPolicyRel->pPolicy == NULL)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        goto pwrPolicyRelationshipGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }

pwrPolicyRelationshipGrpIfaceModel10ObjSetImpl_SUPER_exit:
    return status;
}

/*!
 * @copydoc PwrPolicyRelationshipChannelPwrGet
 */
FLCN_STATUS
pwrPolicyRelationshipChannelPwrGet
(
    PWR_POLICY_RELATIONSHIP             *pPolicyRel,
    PWR_MONITOR                         *pMon,
    PWR_POLICY_3X_FILTER_PAYLOAD_TYPE   *pPayload
)
{
    switch (BOARDOBJ_GET_TYPE(pPolicyRel))
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_TYPE_BALANCE:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_RELATIONSHIP_BALANCE);
            return pwrPolicyRelationshipChannelPwrGet_BALANCE(pPolicyRel,
                    pMon, pPayload);
    }

    PMU_HALT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/* ------------------------- Private Functions ------------------------------ */
