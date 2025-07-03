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
 * @file   pwrpolicy_proplimit.c
 * @brief  Interface specification for a PWR_POLICY_PROP_LIMIT.
 *
 * The Proportional Limit class enforces the limit value by computing the
 * proportion of the policy's limit and value:
 *
 *     prop = limit / value
 *
 * The Proportional Limit class then requests to set limits of its dependent
 * PWR_POLICYs (via PWR_POLICY_RELATIONSHIPs) to each dependnent PWR_POLICY's
 * current value times the proportion:
 *
 *     limit' = prop * value
 *
 * The Proportional Limit class will apply this update to all dependent
 * PWR_POLICYs corresponding the PWR_POLICY_RELATIONSHIPs in the range [@ref
 * PWR_POLICY_PROP_LIMIT::policyRelIdxFirst, @ref
 * PWR_POLICY_PROP_LIMIT::policyRelIdxFirst + @ref
 * PWR_POLICY_PROP_LIMIT::policyRelIdxCnt].
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dmemovl.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objperf.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrpolicy_proplimit.h"
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
 * Construct a _PROP_LIMIT PWR_POLICY object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrPolicyGrpIfaceModel10ObjSetImpl_PROP_LIMIT
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PMGR_PWR_POLICY_PROP_LIMIT_BOARDOBJ_SET *pPropLimitSet   =
        (RM_PMU_PMGR_PWR_POLICY_PROP_LIMIT_BOARDOBJ_SET *)pBoardObjDesc;
    PWR_POLICY_PROP_LIMIT                          *pPropLimit;
    FLCN_STATUS                                     status;
    LwBool                                          bFirstConstruct = (*ppBoardObj == NULL);

    // Construct and initialize parent-class object.
    status = pwrPolicyGrpIfaceModel10ObjSetImpl_LIMIT(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        return status;
    }
    pPropLimit = (PWR_POLICY_PROP_LIMIT *)*ppBoardObj;

    // Copy in the _PROP_LIMIT type-specific data.
    pPropLimit->policyRelIdxFirst = pPropLimitSet->policyRelIdxFirst;
    pPropLimit->policyRelIdxCnt   = pPropLimitSet->policyRelIdxCnt;
    pPropLimit->bDummy            = pPropLimitSet->bDummy;

    // On first construct, allocate array for caching limit requests.
    if (bFirstConstruct)
    {
        pPropLimit->pLimitRequests =
            lwosCallocType(pBObjGrp->ovlIdxDmem,
                pPropLimit->policyRelIdxCnt, LwU32);
        if (pPropLimit->pLimitRequests == NULL)
        {
            status = FLCN_ERR_NO_FREE_MEM;
        }
    }

    return status;
}

/*!
 * _PROP_LIMIT implementation.  Clears the bBalanceDirty flag at the start of
 * the PWR_POLICY evaluation iteration.
 *
 * @copydoc PwrPolicyFilter
 */
FLCN_STATUS
pwrPolicyFilter_PROP_LIMIT
(
    PWR_POLICY  *pPolicy,
    PWR_MONITOR *pMon
)
{
    PWR_POLICY_PROP_LIMIT *pPropLimit = (PWR_POLICY_PROP_LIMIT *)pPolicy;

    pPropLimit->bBalanceDirty = LW_FALSE;

    return FLCN_OK;
}

/*!
 * @copydoc PwrPolicy3XFilter
 */
FLCN_STATUS
pwrPolicy3XFilter_PROP_LIMIT
(
    PWR_POLICY                        *pPolicy,
    PWR_MONITOR                       *pMon,
    PWR_POLICY_3X_FILTER_PAYLOAD_TYPE *pPayload
)
{
    FLCN_STATUS status;

    // Call the super class implementation.
    status = pwrPolicy3XFilter_SUPER(pPolicy, pMon, pPayload);
    if (status != FLCN_OK)
    {
        return status;
    }

    return pwrPolicyFilter_PROP_LIMIT(pPolicy, pMon);
}

/*!
 * PROP_LIMIT implementation.
 *
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
pwrPolicyIfaceModel10GetStatus_PROP_LIMIT
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PMGR_PWR_POLICY_PROP_LIMIT_BOARDOBJ_GET_STATUS *pGetStatus =
        (RM_PMU_PMGR_PWR_POLICY_PROP_LIMIT_BOARDOBJ_GET_STATUS *)pPayload;
    PWR_POLICY_PROP_LIMIT *pPropLimit = (PWR_POLICY_PROP_LIMIT *)pBoardObj;

    pGetStatus->bBalanceDirty = pPropLimit->bBalanceDirty;

    return FLCN_OK;
}

/*!
 * @copydoc PwrPolicyLimitEvaluate
 */
FLCN_STATUS
pwrPolicyLimitEvaluate_PROP_LIMIT
(
    PWR_POLICY_LIMIT *pLimit
)
{
    PWR_POLICY_PROP_LIMIT   *pPropLimit = (PWR_POLICY_PROP_LIMIT *)pLimit;
    PWR_POLICY_RELATIONSHIP *pPolicyRel;
    LwU64                    limitU64;
    LwUFXP52_12              propU52_12;
    LwUFXP20_12              prop;
    LwUFXP20_12              prod;
    LwU32                    valueRel;
    LwU32                    limit;
    FLCN_STATUS              status  = FLCN_OK;
    LwU8                     i;

    //
    // Check that divisor value is not 0!  If so, set proportion to highest
    // possible value (0xFFFFFFFF).
    //
    // Otherwise, callwlate the proportion/ratio of the limit and the value.
    // Will update all relationships' limits by this proportion.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_32BIT_OVERFLOW_FIXES))
    {
        if (pPropLimit->super.super.valueLwrr == 0)
        {
            propU52_12 = (LwUFXP52_12)LW_U32_MAX;
        }
        else
        {
            propU52_12 = LW_TYPES_U64_TO_UFXP_X_Y_SCALED(52, 12,
                             pwrPolicyLimitLwrrGet(&pPropLimit->super.super),
                             pPropLimit->super.super.valueLwrr);
        }
    }
    else
    {
        if (pPropLimit->super.super.valueLwrr == 0)
        {
            prop = (LwUFXP20_12)LW_U32_MAX;
        }
        //
        // FXP20.12 - Will overflow for limit >= 1048576.  This should be safe
        // assumption.
        //
        else
        {
            prop = LW_TYPES_U32_TO_UFXP_X_Y_SCALED(20, 12,
                        pwrPolicyLimitLwrrGet(&pPropLimit->super.super),
                        pPropLimit->super.super.valueLwrr);
        }
    }

    // Iterate over set of PWR_POLICY_RELATIONSHIPs for this PWR_POLICY.
    for (i = 0; i < pPropLimit->policyRelIdxCnt; i++)
    {
        pPolicyRel = PWR_POLICY_REL_GET(pPropLimit->policyRelIdxFirst + i);
        if (pPolicyRel == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto pwrPolicyLimitEvaluate_PROP_LIMIT_exit;
        }

        // Read current value corresponding to the relationship.
        valueRel = pwrPolicyRelationshipValueLwrrGet(pPolicyRel);

        //
        // The new limit to request should be the last value times the
        // proportion.
        //
        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_32BIT_OVERFLOW_FIXES))
        {
            //
            // Callwlate the new limit for all policies depending on this
            // Proportional Limit policy.
            //
            limitU64 = multUFXP64(
                           12,
                           (LwU64) { valueRel },
                           propU52_12);

            //
            // Check for 32-bit overflow and if true, cap the new limit to
            // LW_U32_MAX. Otherwise, extract the lower 32 bits of the 64-bit
            // result.
            //
            if (LwU64_HI32(limitU64))
            {
                limit = LW_U32_MAX;
            }
            else
            {
                limit = LwU64_LO32(limitU64);
            }
        }
        else
        {
            //
            // This will overflow when requested new limit >= 1048576.  However,
            // it's not a completely safe assumption that this will never happen -
            // the proportion can be very large when the PWR_POLICY_PROP_LIMIT
            // channel's limit is very high and its value very low.  So, must check
            // for and properly handle overflow here.
            //
            // Can detect overflow when the proportion is > 0xFFFFFFFF / value.  If
            // so, just apply a ceiling at 0xFFFFFFFF.
            //
            if ((valueRel != 0) && (prop > LW_U32_MAX / valueRel))
            {
                prod = (LwUFXP20_12)LW_U32_MAX;
            }
            else
            {
                prod = valueRel * prop;
            }

            limit = LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(20, 12, prod);
        }

        //
        // If not in "dummy" operation, request the new limit to the PWR_POLICY
        // object via the PWR_POLICY_RELATIONSHIP.
        //
        status = pwrPolicyRelationshipLimitInputSet(pPolicyRel,
                    BOARDOBJ_GET_GRP_IDX_8BIT(&pPropLimit->super.super.super.super), pPropLimit->bDummy,
                    &limit);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
        }

        // Cache the requested limit.
        pPropLimit->pLimitRequests[i] = limit;
    }

pwrPolicyLimitEvaluate_PROP_LIMIT_exit:
    return status;
}

/*!
 * @copydoc PwrPolicyPropLimitLimitGet()
 */
void
pwrPolicyPropLimitLimitGet
(
    PWR_POLICY_PROP_LIMIT  *pPropLimit,
    LwU8                    relIdx,
    LwU32                  *pLimitValue
)
{
    PMU_HALT_COND(relIdx < pPropLimit->policyRelIdxCnt);

    *pLimitValue = pPropLimit->pLimitRequests[relIdx];
}

/* ------------------------- Private Functions ------------------------------ */
