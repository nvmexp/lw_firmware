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
 * @file   pwrpolicy_balance.c
 * @brief  Interface specification for a PWR_POLICY_BALANCE.
 *
 * Balance Algorithm Theory:
 *
 * To be added in follow-up CL which implements functionality.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dmemovl.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrpolicy_balance.h"
#include "pmgr/pwrpolicyrel_balance.h"
#include "dbgprintf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Construct a _BALANCE PWR_POLICY object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrPolicyGrpIfaceModel10ObjSetImpl_BALANCE
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PMGR_PWR_POLICY_BALANCE_BOARDOBJ_SET *pBalanceSet    =
        (RM_PMU_PMGR_PWR_POLICY_BALANCE_BOARDOBJ_SET *)pBoardObjDesc;
    PWR_POLICY_BALANCE                          *pBalance;
    FLCN_STATUS                                  status;
    LwU8                                         idxCount;
    LwBool                                       bFirstConstruct = (*ppBoardObj == NULL);

    idxCount = pBalanceSet->policyRelIdxLast -
               pBalanceSet->policyRelIdxFirst + 1;

    // Construct and initialize parent-class object.
    status = pwrPolicyGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        return status;
    }
    pBalance = (PWR_POLICY_BALANCE *)*ppBoardObj;

    if (bFirstConstruct)
    {
        // Allocate the relationship sorting buffer.
        pBalance->pRelEntries = lwosCallocType(pBObjGrp->ovlIdxDmem,
            idxCount,
            LW2080_CTRL_PMGR_PWR_POLICY_STATUS_DATA_BALANCE_RELATIONSHIP_ENTRY);
    }

    // Copy in the _BALANCE type-specific data.
    pBalance->policyRelIdxFirst = pBalanceSet->policyRelIdxFirst;
    pBalance->policyRelIdxLast  = pBalanceSet->policyRelIdxLast;
    pBalance->policyRelIdxCount = idxCount;

    return status;
}

/*!
 * _BALANCE implementation of QUERY interface.
 *
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
pwrPolicyIfaceModel10GetStatus_BALANCE
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PMGR_PWR_POLICY_BALANCE_BOARDOBJ_GET_STATUS *pGetStatus =
        (RM_PMU_PMGR_PWR_POLICY_BALANCE_BOARDOBJ_GET_STATUS *)pPayload;
    PWR_POLICY_BALANCE *pBalance = (PWR_POLICY_BALANCE *)pBoardObj;

    // Copy out the sorted buffer of _RELATIONSHIP_ENTRYs.
    memcpy(pGetStatus->relEntries,
            pBalance->pRelEntries,
            pBalance->policyRelIdxCount *
            sizeof(LW2080_CTRL_PMGR_PWR_POLICY_STATUS_DATA_BALANCE_RELATIONSHIP_ENTRY));

    return FLCN_OK;
}

/*!
 * @copydoc PwrPolicyBalanceEvaluate()
 */
FLCN_STATUS
pwrPolicyBalanceEvaluate
(
    PWR_POLICY_BALANCE *pBalance
)
{
    //
    // Caching idxCount to reduce IMEM use, since compiler does not know
    // that it will remain unmodified and treats it as volatile.
    //
    LwU8       idxCount = pBalance->policyRelIdxCount;
    LwU8       i;
    LwU8       j;
    FLCN_STATUS status   = FLCN_OK;
    LW2080_CTRL_PMGR_PWR_POLICY_STATUS_DATA_BALANCE_RELATIONSHIP_ENTRY
        tmpRelEntry;

    // First evaluate the Super policy
    status = pwrPolicyEvaluate(&pBalance->super);
    if (status != FLCN_OK)
    {
       return status;
    }

    //
    // 1. Build array of _RELATIONSHIP_ENTRY structures for each relationship
    // referenced by this policy.
    //
    for (i = 0; i < idxCount; i++)
    {
        PWR_POLICY_RELATIONSHIP_BALANCE *pPolicy;

        pBalance->pRelEntries[i].relIdx = pBalance->policyRelIdxFirst + i;

        pPolicy = (PWR_POLICY_RELATIONSHIP_BALANCE *)
            PWR_POLICY_REL_GET(pBalance->policyRelIdxFirst + i);
        if (pPolicy == NULL)
        {
            PMU_BREAKPOINT();
            return FLCN_ERR_ILWALID_INDEX;
        }

        pwrPolicyRelationshipBalanceLimitsDiffGet(
            pPolicy,
            &pBalance->pRelEntries[i].limitLower,
            &pBalance->pRelEntries[i].limitDiff);
    }

    //
    // 2. Sort _RELATIONSHIP_ENTRY structures on following criteria:
    //     1) Lowest limit
    //     2) Largest limit difference
    //
    // @note Normally sorting loops look like:
    //      for (i = 0; i < n-1; i++)
    //          for (j = i+1; j < n; j++) { ... }
    // however an extra iteration of outher loop allows further IMEM reduction.
    //
    for (i = 0; i < idxCount; i++)
    {
        for (j = i + 1; j < idxCount; j++)
        {
            // If lower element found later in the array, swap elements.
            if ((pBalance->pRelEntries[j].limitLower < pBalance->pRelEntries[i].limitLower) ||
                ((pBalance->pRelEntries[j].limitLower == pBalance->pRelEntries[i].limitLower) &&
                 (pBalance->pRelEntries[j].limitDiff >  pBalance->pRelEntries[i].limitDiff)))
            {
                tmpRelEntry = pBalance->pRelEntries[j];
                pBalance->pRelEntries[j] = pBalance->pRelEntries[i];
                pBalance->pRelEntries[i] = tmpRelEntry;
            }
        }
    }

    // 3. Iterate over sorted relationships, balancing each.
    for (i = 0; i < idxCount; i++)
    {
        PWR_POLICY_RELATIONSHIP_BALANCE *pPolicy;

        pPolicy = (PWR_POLICY_RELATIONSHIP_BALANCE *)PWR_POLICY_REL_GET(pBalance->pRelEntries[i].relIdx);
        if (pPolicy == NULL)
        {
            PMU_BREAKPOINT();
            return FLCN_ERR_ILWALID_INDEX;
        }

        status = pwrPolicyRelationshipBalanceEvaluate(pPolicy);
        if (status != FLCN_OK)
        {
            return status;
        }
    }

    return status;
}

/*!
 * @copydoc PwrPolicy3XChannelMaskGet
 */
LwU32
pwrPolicy3XChannelMaskGet_BALANCE
(
    PWR_POLICY *pPolicy
)
{
    PWR_POLICY_BALANCE *pBalance = (PWR_POLICY_BALANCE *)pPolicy;
    LwU32 channelMask = 0;
    LwU8  relIdx;

    // Update the channelMask with super class channel.
    channelMask = BIT(pPolicy->chIdx);

    // Update the channelMask with phaseEstimateChIdx of each balance policyrel.
    for (relIdx = pBalance->policyRelIdxFirst;
            relIdx <= pBalance->policyRelIdxLast; relIdx++)
    {
        if (!BOARDOBJGRP_IS_VALID(PWR_POLICY_RELATIONSHIP, relIdx))
        {
            PMU_BREAKPOINT();
            continue;
        }

        if (LW2080_CTRL_PMGR_PWR_CHANNEL_INDEX_ILWALID !=
                PWR_POLICY_REL_BALANCE_GET_PHASE_EST_CHIDX(relIdx))
        {
            channelMask |= BIT(PWR_POLICY_REL_BALANCE_GET_PHASE_EST_CHIDX(relIdx));
        }
    }

    return channelMask;
}

/*!
 * @copydoc PwrPolicy3XFilter
 */
FLCN_STATUS
pwrPolicy3XFilter_BALANCE
(
    PWR_POLICY     *pPolicy,
    PWR_MONITOR    *pMon,
    PWR_POLICY_3X_FILTER_PAYLOAD_TYPE *pPayload
)
{
    PWR_POLICY_BALANCE *pBalance = (PWR_POLICY_BALANCE *)pPolicy;
    LwU8        relIdx;
    FLCN_STATUS status = FLCN_OK;

    //
    // Call the super class implementation.
    // NOTE: We dont use the power channel for balance policy, so this function
    // is NOT doing any useful work for us.
    //
    status = pwrPolicy3XFilter_SUPER(pPolicy, pMon, pPayload);
    if (status != FLCN_OK)
    {
        return status;
    }

    //
    // Iterate over each balancing policy relationships to update their phase
    // estimate channel power/current.
    //
    for (relIdx = pBalance->policyRelIdxFirst;
            relIdx <= pBalance->policyRelIdxLast; relIdx++)
    {
        PWR_POLICY_RELATIONSHIP *pRelationship;

        pRelationship = PWR_POLICY_REL_GET(relIdx);
        if (pRelationship == NULL)
        {
            PMU_BREAKPOINT();
            return FLCN_ERR_ILWALID_INDEX;
        }

        status =
            pwrPolicyRelationshipChannelPwrGet(pRelationship, pMon, pPayload);
        if (status != FLCN_OK)
        {
            return status;
        }
    }

    return status;
}

/* ------------------------- Private Functions ------------------------------ */
