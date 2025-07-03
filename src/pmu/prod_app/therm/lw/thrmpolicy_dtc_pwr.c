/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"
#include "therm/objtherm.h"
#include "therm/thrmpolicy_dtc_pwr.h"
#include "therm/thrmpolicy_dtc.h"

/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions  ------------------------------ */
/*!
 * THERM_POLICY_DTC_PWR implementation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
thermPolicyGrpIfaceModel10ObjSetImpl_DTC_PWR
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_THERM_THERM_POLICY_DTC_PWR_BOARDOBJ_SET  *pSet =
        (RM_PMU_THERM_THERM_POLICY_DTC_PWR_BOARDOBJ_SET *)pBoardObjDesc;
    LwBool                  bFirstConstruct = (*ppBoardObj == NULL);
    FLCN_STATUS             status          = FLCN_OK;
    BOARDOBJ               *pTmpBoardObj;
    THERM_POLICY_DTC_PWR   *pDtcPwr;
    LwU32                   prevPwrPolicyIdx;

    // Call the super class constructor.
    status = thermPolicyGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pDtcPwr      = (THERM_POLICY_DTC_PWR *)*ppBoardObj;
    pTmpBoardObj = &(pDtcPwr->dtc.super);

    // Construct the THERM_POLICY_DTC interface.
    status = thermPolicyGrpIfaceModel10ObjSetImpl_DTC(pModel10, &pTmpBoardObj, size,
            (RM_PMU_BOARDOBJ *)&pSet->dtc);
    if (status != FLCN_OK)
    {
        return status;
    }

    //
    // Set class-specific data. prevPwrPolicyIdx will be undefined on the first
    // construct, but it is not used in that case. It'll most likely be set to
    // 0 (which is most likely a valid power policy index). On subsequent
    // calls, it'll contain a valid value. If it doesn't, it'll be caught in
    // check below.
    //
    prevPwrPolicyIdx          = pDtcPwr->powerPolicyIdx;
    pDtcPwr->powerPolicyIdx   = pSet->powerPolicyIdx;
    pDtcPwr->powerStepmW      = pSet->powerStepmW;

    //
    // Initialize the boolean denoting whether the Thermal Policy is lwrrently
    // engaged.
    //
    thermPolicyDtcPwrIsEngagedSet(pDtcPwr, LW_FALSE);

    if (bFirstConstruct)
    {
        //
        // We only want to set the current limit on the first initialization.
        // Subsequent calls (e.g. change thermal limits) need to leave the
        // current limit unchanged.
        //
        pDtcPwr->limitLwrrmW = LW2080_CTRL_THERMAL_POLICY_PERF_LIMIT_VAL_NONE;
    }
    else if (pDtcPwr->powerPolicyIdx != prevPwrPolicyIdx)
    {
        //
        // There was a change in the target power policy. Need to swap
        // policies, making sure that we transfer any power limit imposed
        // to the new policy.
        //
        PWR_POLICY     *pPrevPwrPolicy;
        PWR_POLICY     *pLwrrPwrPolicy;
        LwU32           limitMaxmW;
        LwU32           limitMinmW;
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, pmgr)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            ATTACH_AND_LOAD_OVERLAY_PWR_POLICY_CLIENT_LOCKED;
            {
                pPrevPwrPolicy = PMGR_GET_PWR_POLICY_IDX(prevPwrPolicyIdx);
                pLwrrPwrPolicy = PMGR_GET_PWR_POLICY_IDX(pDtcPwr->powerPolicyIdx);

                if ((pPrevPwrPolicy == NULL) ||
                    (pLwrrPwrPolicy == NULL))
                {
                    //
                    // If we hit this halt, need to verify that the RM is sending down
                    // valid powerPolicyIdx values.
                    //
                    PMU_HALT();

                    status = FLCN_ERROR;
                    goto thermPolicyGrpIfaceModel10ObjSetImpl_DTC_PWR_Exit;
                }

                //
                // Move the current power limit imposed on the previous power
                // policy to the new power policy. This is to ensure that we do
                // not get any power spikes. Additionally, ake sure that the current
                // limit is within the new policy's limit range; adjust accordingly.
                //
                limitMaxmW = pwrPolicyLimitMaxGet(pLwrrPwrPolicy);
                limitMinmW = pwrPolicyLimitMinGet(pLwrrPwrPolicy);

                pDtcPwr->limitLwrrmW = LW_MAX(pDtcPwr->limitLwrrmW, limitMinmW);
                pDtcPwr->limitLwrrmW = LW_MIN(pDtcPwr->limitLwrrmW, limitMaxmW);

                status = pwrPolicyLimitArbInputSet(pPrevPwrPolicy,
                    LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_INPUT_CLIENT_IDX_THERM_POLICY(
                        BOARDOBJ_GET_GRP_IDX_8BIT(&pDtcPwr->super.super)),
                    pDtcPwr->limitLwrrmW);

                if (status == FLCN_OK)
                {
                    //
                    // Now clear the limit imposed on the previous power policy.
                    // For now, this is by setting the limit of the previous power
                    // policy to its maximum limit.
                    //
                    limitMaxmW = pwrPolicyLimitMaxGet(pPrevPwrPolicy);
                    status = pwrPolicyLimitArbInputSet(pPrevPwrPolicy,
                        LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_INPUT_CLIENT_IDX_THERM_POLICY(
                            BOARDOBJ_GET_GRP_IDX_8BIT(&pDtcPwr->super.super)),
                        limitMaxmW);
                }

 thermPolicyGrpIfaceModel10ObjSetImpl_DTC_PWR_Exit:
                lwosNOP();
            }
            DETACH_OVERLAY_PWR_POLICY_CLIENT_LOCKED;
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }

    return status;
}

/*!
 * DTC_PWR-class implementation.
 *
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
thermPolicyIfaceModel10GetStatus_DTC_PWR
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_THERM_THERM_POLICY_DTC_PWR_BOARDOBJ_GET_STATUS *pGetStatus;
    THERM_POLICY_DTC_PWR                                  *pDtcPwr;
    THERM_POLICY_DTC                                      *pDtc;
    FLCN_STATUS                                            status;

    // Get super class information.
    status = thermPolicyIfaceModel10GetStatus_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        return status;
    }
    pDtcPwr     = (THERM_POLICY_DTC_PWR *)pBoardObj;
    pDtc        = &pDtcPwr->dtc;
    pGetStatus  =
        (RM_PMU_THERM_THERM_POLICY_DTC_PWR_BOARDOBJ_GET_STATUS *)pPayload;

    pGetStatus->limitLwrrmW = pDtcPwr->limitLwrrmW;

    return thermPolicyDtcQuery(pDtc, &pGetStatus->dtc);
}

/*!
 * @copydoc ThermPolicyCallbackExelwte
 */
FLCN_STATUS
thermPolicyCallbackExelwte_DTC_PWR
(
    THERM_POLICY *pPolicy
)
{
    THERM_POLICY_DTC_PWR *pDtcPwr     = (THERM_POLICY_DTC_PWR *)pPolicy;
    THERM_POLICY_DTC     *pDtc        = &pDtcPwr->dtc;
    FLCN_STATUS           status      = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, pmgr)
    };

    LwS32 limitChange;

    // Determine limit changes
    limitChange = thermPolicyDtcGetLevelChange(pDtc,
                                       pDtcPwr->super.channelReading);

    //
    // Set the boolean denoting whether the Thermal Policy is lwrrently
    // engaged.
    //
    // Note: 1. If the current temperature is above the moderate threshold
    //          (policy limit), the policy is engaged.
    //       2. If the current temperature is below the disengage threshold,
    //          the policy is disengaged.
    //       3. In between [disengage threshold, moderate threshold], if the
    //          policy is lwrrently engaged, it will remain engaged and vice
    //          versa. Hence, we don't need to handle this case explicitly.
    //
    if (limitChange < 0)
    {
        thermPolicyDtcPwrIsEngagedSet(pDtcPwr, LW_TRUE);
    }
    else if (limitChange == THERM_POLICY_DTC_LIMIT_CHANGE_IF_DISENGAGED)
    {
        thermPolicyDtcPwrIsEngagedSet(pDtcPwr, LW_FALSE);
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        if (limitChange != 0)
        {
            PWR_POLICY *pPwrPolicy = PMGR_GET_PWR_POLICY_IDX(
                                         pDtcPwr->powerPolicyIdx);
            LwU32       limitMaxmW;
            LwU32       limitMinmW;
            LwU32       lwrrLimitmW;
            LwU32       newLimitmW;

            if (pPwrPolicy == NULL)
            {
                //
                // If we hit this halt, need to verify that the RM is sending down
                // a valid powerPolicyIdx
                //
                PMU_HALT();

                status = FLCN_ERROR;
                goto thermPolicyCallbackExelwte_DTC_PWR_Exit;
            }

            limitMaxmW  = pwrPolicyLimitMaxGet(pPwrPolicy);
            limitMinmW  = pwrPolicyLimitMinGet(pPwrPolicy);

            //
            // If we are lowering the power limit, we want to use the current
            // power value as the starting point to make the thermal policy more
            // responsive. When increasing the power limit, we want the thermal
            // policy to use the last limit impose as the starting point.
            //
            if (limitChange < 0)
            {
                lwrrLimitmW = pwrPolicyValueLwrrGet(pPwrPolicy);
                if (lwrrLimitmW < limitMinmW - (limitChange * pDtcPwr->powerStepmW))
                {
                    newLimitmW = limitMinmW;
                }
                else
                {
                    newLimitmW  = lwrrLimitmW + (limitChange * pDtcPwr->powerStepmW);
                }
            }
            else // limitChange > 0
            {
                lwrrLimitmW = pwrPolicyLimitArbLwrrGet(pPwrPolicy);

                //
                // Do not request limit increase if the current power limit is less
                // than our last request; something else is the limiting factor.
                //
                if (lwrrLimitmW < pDtcPwr->limitLwrrmW)
                {
                    newLimitmW = lwrrLimitmW;
                }
                else
                {
                    if ((limitChange == THERM_POLICY_DTC_LIMIT_CHANGE_IF_DISENGAGED) ||
                        (lwrrLimitmW > limitMaxmW - (limitChange * pDtcPwr->powerStepmW)))
                    {
                        newLimitmW = limitMaxmW;
                    }
                    else
                    {
                        newLimitmW = lwrrLimitmW + (limitChange * pDtcPwr->powerStepmW);
                    }
                }
            }

            // Update power limit if needed
            if (newLimitmW != lwrrLimitmW)
            {
                status = pwrPolicyLimitArbInputSet(pPwrPolicy,
                    LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_INPUT_CLIENT_IDX_THERM_POLICY(
                        BOARDOBJ_GET_GRP_IDX_8BIT(&pDtcPwr->super.super)),
                    newLimitmW);
                if (status == FLCN_OK)
                {
                    pDtcPwr->limitLwrrmW = newLimitmW;
                }
            }
        }

thermPolicyCallbackExelwte_DTC_PWR_Exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/* ---------------------- Private Static Functions -------------------------- */
