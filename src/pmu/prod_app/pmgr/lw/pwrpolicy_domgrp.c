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
 * @file   pwrpolicy_domgrp.c
 * @brief  Interface specification for a PWR_POLICY_DOMGRP.
 *
 * The Domain Group class is a virtual class which provides a generic mechanism
 * to control power/current with a Domain Group PERF_LIMITs.  This module does
 * not include any knowledge/policy for how to set these limits, only the
 * framework to set/retrieve these limits.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objperf.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrpolicy_domgrp.h"
#include "therm/objtherm.h"
#include "dbgprintf.h"
//#include "g_pmuifevtrpc.h"
#include "g_pmurmifrpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * The number of limits the power policies will be modifying.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_DOMAIN_GROUP_LIMITS_XBAR)
#define PMGR_PWR_POLICIES_PERF_LIMITS_NUM                                     6
#else
#define PMGR_PWR_POLICIES_PERF_LIMITS_NUM                                     4
#endif

/* ------------------------- Private Functions ------------------------------ */
static FLCN_STATUS s_pwrPolicyDomGrpCeilingGet(PWR_POLICY_DOMGRP *pDomGrp, PERF_DOMAIN_GROUP_LIMITS *pDomGrpCeiling)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyDomGrpCeilingGet");
static FLCN_STATUS s_pwrPolicyDomGrpLimitsSetAll_RM(PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits, LwBool bSkipVblank)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyDomGrpLimitsSetAll_RM");
static FLCN_STATUS s_pwrPolicyDomGrpLimitsSetAll_PMU(PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyDomGrpLimitsSetAll_PMU");
static FLCN_STATUS s_pwrPolicyGetDomGrpLimitsByPffSource(LW2080_CTRL_PMGR_PWR_POLICY_INFO_DATA_DOMGRP_PFF_SOURCE *pPffSource, PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyGetDomGrpLimitsByPffSource");
static FLCN_STATUS s_pwrPolicyDomGrpDomainGroupLimitsCopy(PERF_DOMAIN_GROUP_LIMITS *pDst, const PERF_DOMAIN_GROUP_LIMITS *pSrc)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyDomGrpDomainGroupLimitsCopy");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Construct a _DOMGRP PWR_POLICY object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrPolicyGrpIfaceModel10ObjSetImpl_DOMGRP
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PMGR_PWR_POLICY_DOMGRP_BOARDOBJ_SET *pDomGrpSet =
        (RM_PMU_PMGR_PWR_POLICY_DOMGRP_BOARDOBJ_SET *)pBoardObjDesc;
    PWR_POLICY_DOMGRP                          *pDomGrp;
    FLCN_STATUS                                 status;
    LwBool bFirstConstruct;

    bFirstConstruct = (*ppBoardObj == NULL);

    // Construct and initialize parent-class object.
    status = pwrPolicyGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pDomGrp = (PWR_POLICY_DOMGRP *)*ppBoardObj;

    // Set class-specific data.
    pDomGrp->bRatedTdpVpstateFloor = pDomGrpSet->bRatedTdpVpstateFloor;
    pDomGrp->limitInflection       = pDomGrpSet->limitInflection;
    pwrPolicyDomGrpLimitsInit(&pDomGrp->domGrpLimits);
    // pDomGrp->counter is initialized to 0 by lwosCallocResidentType().

    // Init limits to full deflection vpstate if necessary
    if (pDomGrpSet->bTriggerFullDeflectiolwpstate)
    {
        status = pwrPolicyGetDomGrpLimitsByVpstateIdx(RM_PMU_PERF_VPSTATE_IDX_FULL_DEFLECTION,
            &(pDomGrp->domGrpLimits));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            return status;
        }
        // Set this so next evaluation is capped to full deflection pt
        pDomGrp->bFullDeflectiolwpstateCeiling = LW_TRUE;
    }

    // Init the piecewise frequency floor source
    pwrPolicyDomGrpPffSourceSet(pDomGrp, pDomGrpSet->pffSource);

    if (bFirstConstruct)
    {
        LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST *pLastInflectionState;

        // Initialize the last inflection state, if available.
        PMU_ASSERT_OK_OR_GOTO(status,
            pwrPolicyDomGrpLastInflectionStateGet(pDomGrp, &pLastInflectionState),
            pwrPolicyGrpIfaceModel10ObjSetImpl_DOMGRP_exit);
        if (pLastInflectionState != NULL)
        {
            LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_INIT(
                pLastInflectionState);
        }
    }

pwrPolicyGrpIfaceModel10ObjSetImpl_DOMGRP_exit:
    return status;
}

/*!
 * Initializes the DMEM overlay used by the power policies to set/clear
 * PERF_LIMITS.
 */
FLCN_STATUS
pwrPoliciesPostInit_DOMGRP(void)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_DOMGRP_PMU_PERF_LIMITS))
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_DEFINE_PERF_PERF_LIMITS_CLIENT_INIT
            OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, pmgrPwrPoliciesPerfLimits)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = perfLimitsClientInit(&Pmgr.pPerfLimits,
                                          OVL_INDEX_DMEM(pmgrPwrPoliciesPerfLimits),
                                          PMGR_PWR_POLICIES_PERF_LIMITS_NUM,
                                          LW_FALSE);
            PMU_HALT_COND(status == FLCN_OK);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }

    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
pwrPolicyIfaceModel10GetStatus_DOMGRP
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PMGR_PWR_POLICY_DOMGRP_BOARDOBJ_GET_STATUS *pGetStatus =
        (RM_PMU_PMGR_PWR_POLICY_DOMGRP_BOARDOBJ_GET_STATUS *)pPayload;
    PWR_POLICY_DOMGRP *pDomGrp = (PWR_POLICY_DOMGRP *)pBoardObj;

    // Copy out the state to the RM.
    perfDomGrpLimitsCopyOut(&(pGetStatus->domGrpLimits),
        &(pDomGrp->domGrpLimits));
    perfDomGrpLimitsCopyOut(&(pGetStatus->domGrpCeiling),
        &(pDomGrp->domGrpCeiling));

    // Copy out the capped/uncapped parameter to the RM.
    pGetStatus->bIsCapped = pDomGrp->bIsCapped;

    return FLCN_OK;
}

/*!
 * @copydoc PwrPolicyDomGrpEvaluate
 */
FLCN_STATUS
pwrPolicyDomGrpEvaluate
(
    PWR_POLICY_DOMGRP *pDomGrp
)
{
    PERF_DOMAIN_GROUP_LIMITS  domGrpLimitsOutput;
    FLCN_STATUS               status;

    // First evaluate the Super policy
    status = pwrPolicyEvaluate(&pDomGrp->super);
    if (status != FLCN_OK)
    {
        goto pwrPolicyDomGrpEvaluate_exit;
    }

    // Determine the Domain Group ceiling for the given PWR_POLICY_DOMGRP object.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_pwrPolicyDomGrpCeilingGet(pDomGrp, &pDomGrp->domGrpCeiling),
        pwrPolicyDomGrpEvaluate_exit);

    //
    // Set status back to unsupported, for the case where implementation is
    // missing below:
    //
    status = FLCN_ERR_NOT_SUPPORTED;

    // Call the class-specific logic.
    switch (BOARDOBJ_GET_TYPE(pDomGrp))
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_BANG_BANG_VF:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_BANG_BANG_VF);
            status = pwrPolicyDomGrpEvaluate_BANG_BANG_VF(pDomGrp);
            break;

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_MARCH_VF:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_MARCH_VF);
            status = pwrPolicyDomGrpEvaluate_MARCH_VF(pDomGrp);
            break;

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_WORKLOAD);
            status = pwrPolicyDomGrpEvaluate_WORKLOAD(pDomGrp);
            break;

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_MULTIRAIL:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD);
            status = pwrPolicyDomGrpEvaluate_WORKLOAD_MULTIRAIL(pDomGrp);
            break;

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_COMBINED_1X:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X);
            status = pwrPolicyDomGrpEvaluate_WORKLOAD_COMBINED_1X(pDomGrp);
            break;
    }

    // If completed w/o errors, cache the output values for debugging.
    if (status == FLCN_OK)
    {
        //
        // Apply any peicewise frequency floor that may be associated with
        // this PWR_POLICY_DOMGRP.
        //
        if (pwrPolicyDomGrpHasPffSource(pDomGrp))
        {
            status = s_pwrPolicyGetDomGrpLimitsByPffSource(
                        pwrPolicyDomGrpPffSourceGet(pDomGrp),
                        &domGrpLimitsOutput);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto pwrPolicyDomGrpEvaluate_exit;
            }
            pwrPolicyGetDomGrpLimitsFloor(pwrPolicyDomGrpLimitsGet(pDomGrp),
                &domGrpLimitsOutput);
        }

        //
        // Apply the Domain Group ceiling to the limits callwlated by
        // class-specific algorithms above.
        //
        pwrPolicyGetDomGrpLimitsCeiling(pwrPolicyDomGrpLimitsGet(pDomGrp),
            &pDomGrp->domGrpCeiling);

        //
        // If required, floor this PWR_POLICY_DOMGRP's limits to the Rated TDP
        // VPstate. Skip if full deflection vpstate ceiling is triggered.
        //
        if (pDomGrp->bRatedTdpVpstateFloor)
        {
            status = pwrPolicyGetDomGrpLimitsByVpstateIdx(
                RM_PMU_PERF_VPSTATE_IDX_RATED_TDP, &domGrpLimitsOutput);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto pwrPolicyDomGrpEvaluate_exit;
            }
            pwrPolicyGetDomGrpLimitsFloor(pwrPolicyDomGrpLimitsGet(pDomGrp),
                &domGrpLimitsOutput);
        }
    }

pwrPolicyDomGrpEvaluate_exit:
    return status;
}

/*!
 * @copydoc PwrPolicyDomGrpGlobalCeilingConstruct
 */
FLCN_STATUS
pwrPolicyDomGrpGlobalCeilingConstruct(void)
{
    FLCN_STATUS status = FLCN_OK;
    PWR_POLICIES *pPolicies   = Pmgr.pPwrPolicies;

    //
    // One request is reserved for RM for any
    // global ceiling limit request
    //
    LwU8          numRequests = 1;
    LwU8          i;

    //
    // Iterate of set of PWR_POLICY objects to see which may need the global
    // Domain Group ceiling feature.
    //
    FOR_EACH_INDEX_IN_MASK(32, i, Pmgr.pwr.policies.objMask.super.pData[0])
    {
        PWR_POLICY *pPolicy;

        pPolicy = PWR_POLICY_GET(i);
        if (pPolicy == NULL)
        {
            status = FLCN_ERR_ILWALID_INDEX;
            PMU_BREAKPOINT();
            goto pwrPolicyDomGrpGlobalCeilingConstruct_exit;
        }

        // Lwrrently, only used by the _TOTAL_GPU class.
        if (LW2080_CTRL_PMGR_PWR_POLICY_TYPE_TOTAL_GPU ==
                BOARDOBJ_GET_TYPE(pPolicy))
        {
            numRequests++;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    //
    // If PWR_POLICY objects may issue global Domain Group ceilings requests,
    // allocate and initialize the array of requests.
    //
    if (numRequests != 0)
    {
        // Only allocate if the array has not yet been allocated.
        if (pPolicies->domGrpGlobalCeiling.pRequests == NULL)
        {
            pPolicies->domGrpGlobalCeiling.pRequests =
                lwosCallocType(
                    PWR_POLICY_ALLOCATIONS_DMEM_OVL(OVL_INDEX_DMEM(pmgr)),
                    numRequests,
                    PWR_POLICY_DOMGRP_GLOBAL_CEILING_REQUEST);
            if (pPolicies->domGrpGlobalCeiling.pRequests == NULL)
            {
                status = FLCN_ERR_NO_FREE_MEM;
                PMU_BREAKPOINT();
                goto pwrPolicyDomGrpGlobalCeilingConstruct_exit;
            }
        }
        // Otherwise, ensure that the number of possible requests has not changed.
        else
        {
            PMU_HALT_COND(numRequests ==
                pPolicies->domGrpGlobalCeiling.numRequests);
        }

        // Iinitailze the request structures to _ILWALID client indexes.
        pPolicies->domGrpGlobalCeiling.numRequests = numRequests;
        memset(pPolicies->domGrpGlobalCeiling.pRequests,
                LW2080_CTRL_PMGR_PWR_POLICY_INDEX_ILWALID,
                pPolicies->domGrpGlobalCeiling.numRequests *
                    sizeof(PWR_POLICY_DOMGRP_GLOBAL_CEILING_REQUEST));
    }

pwrPolicyDomGrpGlobalCeilingConstruct_exit:
    return status;
}

/*!
 * @copydoc PwrPolicyDomGrpGlobalCeilingRequest
 */
FLCN_STATUS
pwrPolicyDomGrpGlobalCeilingRequest
(
    LwU8                      policyIdx,
    PERF_DOMAIN_GROUP_LIMITS *pDomGrpCeiling
)
{
    PWR_POLICIES *pPolicies   = Pmgr.pPwrPolicies;
    LwU8          i;

    //
    // Find a request entry for the given PWR_POLICY - i.e. an entry already
    // reserved with the given policyIdx or a free entry with _ILWALID_IDX.
    //
    for (i = 0; i < pPolicies->domGrpGlobalCeiling.numRequests; i++)
    {
        if ((policyIdx == pPolicies->domGrpGlobalCeiling.pRequests[i].policyIdx) ||
            (LW2080_CTRL_PMGR_PWR_POLICY_INDEX_ILWALID ==
                pPolicies->domGrpGlobalCeiling.pRequests[i].policyIdx))
        {
            break;
        }
    }

    // Ensure that room was available within the array.
    if (i >= pPolicies->domGrpGlobalCeiling.numRequests)
    {
        PMU_HALT();
        return FLCN_ERR_ILWALID_STATE;
    }

    // Store the requested values in the found entry.
    pPolicies->domGrpGlobalCeiling.pRequests[i].policyIdx     =  policyIdx;
    pPolicies->domGrpGlobalCeiling.pRequests[i].domGrpCeiling = *pDomGrpCeiling;

    return FLCN_OK;
}

/*!
 * Initiliazes a PERF Domain Group Limit structure to its default/disabled
 * state.
 */
void
pwrPolicyDomGrpLimitsInit
(
    PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits
)
{
    LwU32 index;

    for (index = 0; index < PERF_DOMAIN_GROUP_MAX_GROUPS; index++)
    {
        pDomGrpLimits->values[index] =
            RM_PMU_PREF_DOMAIN_GROUP_LIMIT_VALUE_DISABLE;
    }
}

/*!
 * Arbitrates between the a set of output domain group limits (which may be a
 * previously aribtrated output) and a set of input domain group limits.  Will
 * apply the lowest limits between input and ouput to output.
 *
 * @param[in/out] pDomGrpLimitsOutput Pointer to output domain group limit set.
 * @param[in]     pDomGrpLimitsINput  Pointer to input domain group limit set.
 */
void
pwrPolicyDomGrpLimitsArbitrate
(
    PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimitsOutput,
    PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimitsInput
)
{
    LwU8 i;

    for (i = 0; i < PERF_DOMAIN_GROUP_MAX_GROUPS; i++)
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_DOMAIN_GROUP_LIMITS_MASK))
        {
            if (!PWR_POLICIES_DOMGRP_LIMIT_MASK_BIT_GET(i))
            {
                continue;
            }
        }
        pDomGrpLimitsOutput->values[i] =
            LW_MIN(pDomGrpLimitsOutput->values[i],
                    pDomGrpLimitsInput->values[i]);
    }
}

/*!
 * Submits the requested Perf Domain Groups Limits to the RM arbiter.
 *
 * @param[in] pDomGrpLimits  Pointer to requested Domain Group Limits. The
 *                           buffer used must be able to contain
 *                           RM_PMU_PERF_DOMAIN_GROUP_LIMITS_ID_NUM elements.
 * @param[in] bSkipVblank
 *     Specifies whether corresponding RM pstate change should skip waiting for
 *     blank.
 */
void
pwrPolicyDomGrpLimitsSetAll
(
    PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits,
    LwBool                    bSkipVblank
)
{
    PWR_POLICIES *pPolicies = Pmgr.pPwrPolicies;
    FLCN_STATUS   status;
    LwU8          limitId;
    LwU8          i;
    LwBool        bChange = LW_FALSE;

    // Check if any of the limits are changing before updating the arbiter.
    for (limitId = 0; limitId < RM_PMU_PERF_DOMAIN_GROUP_LIMITS_ID_NUM; limitId++)
    {
        for (i = 0; i < PERF_DOMAIN_GROUP_MAX_GROUPS; i++)
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_DOMAIN_GROUP_LIMITS_MASK))
            {
                 if (!PWR_POLICIES_DOMGRP_LIMIT_MASK_BIT_GET(i))
                 {
                    continue;
                 }
            }
            if (pDomGrpLimits[limitId].values[i] != Perf.domGrpLimits[limitId].values[i])
            {
                bChange = LW_TRUE;
                break;
            }
        }
    }

    // If limits aren't changing, return early.
    if (!bChange)
    {
        return;
    }

    if ((PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_DOMGRP_PMU_PERF_LIMITS)) &&
        (pPolicies->bPmuPerfLimitsClient))
    {
        status = s_pwrPolicyDomGrpLimitsSetAll_PMU(pDomGrpLimits);
    }
    else
    {
        status = s_pwrPolicyDomGrpLimitsSetAll_RM(pDomGrpLimits, bSkipVblank);
    }

    if (status == FLCN_OK)
    {
        // Cache the latest settings in OBJPERF object.
        for (limitId = 0; limitId < RM_PMU_PERF_DOMAIN_GROUP_LIMITS_ID_NUM; limitId++)
        {
            Perf.domGrpLimits[limitId] = pDomGrpLimits[limitId];
        }
    }
}

/*!
 * Accessor for minimum OBJPERF PMU domain group limits.  Will copy the domain
 * group limits minimums into the requested buffer.
 *
 * @param[out] pDomGrpLimits
 *     PERF_DOMAIN_GROUP_LIMITS pointer into which the current domain
 *     group limits will be copied.
 */
void
pwrPolicyDomGrpLimitsGetMin
(
    PERF_DOMAIN_GROUP_LIMITS   *pDomGrpLimitsMin
)
{
    LwU32 domGrp;
    LwU32 limitGrp;

    // Initialize the buffer to no limits.
    pwrPolicyDomGrpLimitsInit(pDomGrpLimitsMin);

    // Copy out the current settings from OBJPERF to requester's buffer.
    for (domGrp = 0; domGrp < PERF_DOMAIN_GROUP_MAX_GROUPS; domGrp++)
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_DOMAIN_GROUP_LIMITS_MASK))
        {
            if (!PWR_POLICIES_DOMGRP_LIMIT_MASK_BIT_GET(domGrp))
            {
                continue;
            }
        }
        for (limitGrp = 0; limitGrp < RM_PMU_PERF_DOMAIN_GROUP_LIMITS_ID_NUM; limitGrp++)
        {
            pDomGrpLimitsMin->values[domGrp] =
                LW_MIN(pDomGrpLimitsMin->values[domGrp],
                       Perf.domGrpLimits[limitGrp].values[domGrp]);
        }
    }
}

/*!
 * Accessor for current OBJPERF PMU domain group limits.  Will copy the domain
 * group limits into the requested buffer.
 *
 * @param[out] pDomGrpLimits
 *     PERF_DOMAIN_GROUP_LIMITS pointer into which the current domain
 *     group limits will be copied.
 * @param[in]  limitId      Specifies the group of domain limits to get.
 */
void
pwrPolicyDomGrpLimitsGetById
(
    PERF_DOMAIN_GROUP_LIMITS          *pDomGrpLimits,
    RM_PMU_PERF_DOMAIN_GROUP_LIMITS_ID limitId
)
{
    // Verify the limit ID is valid.
    PMU_HALT_COND(limitId < RM_PMU_PERF_DOMAIN_GROUP_LIMITS_ID_NUM);

    // Copy out the current settings from OBJPERF to requester's buffer.
    *pDomGrpLimits = Perf.domGrpLimits[limitId];
}

/*!
 * Helper function to pack the PERF_DOMAIN_GROUP_LIMITS with the
 * domain group infomration of the corresponding VPSTATE.
 *
 * @param[in]       vPstateIdx      VPSTATE index to use
 * @param[in/out]   pDomGrpLimits   Pointer to RM_PMU_PERF_DOMAIN_GROUP_LIMITS
 *
 * return FLCN_OK   Succesfully packed the ref@ pDomGrpLimits with VPSTATE info.
 * other error coming from the higher lever functions.
 */
FLCN_STATUS
pwrPolicyGetDomGrpLimitsByVpstateIdx
(
    LwU8    vPstateIdx,
    PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits
)
{
    FLCN_STATUS status;

    VPSTATE *pVpstate = vpstateGetBySemanticIdx(vPstateIdx);
    if (pVpstate == NULL)
    {
        status = FLCN_ERR_ILWALID_INDEX;
    }
    else
    {
        status = perfVPstatePackVpstateToDomGrpLimits(pVpstate, pDomGrpLimits);
    }

    return status;
}

/*!
 * Adjusts a specified set of Domain Group limits ref@ pDomGrpLimitsOutput
 * by flooring the values to the values specified in the packed domain group
 * limits ref@ pDomGrpLimitsInput
 *
 * @param[in/out]   pDomGrpLimitsOutput
 *     PERF_DOMAIN_GROUP_LIMITS pointer containing the Domain Group
 *     limits to adjust to the "3D Boost" VPstate.
 * @param[in]       pDomGrpLimitsInput
 *     RM_PMU_PERF_DOMAIN_GROUP_LIMITS pointer containing the Domain Group
 *     limits as ref for floor value.
 */
void
pwrPolicyGetDomGrpLimitsFloor
(
    PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimitsOutput,
    PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimitsInput
)
{
    LwU8 i;

    for (i = 0; i < PERF_DOMAIN_GROUP_MAX_GROUPS; i++)
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_DOMAIN_GROUP_LIMITS_MASK))
        {
            if (PWR_POLICIES_DOMGRP_LIMIT_MASK_BIT_GET(i) == 0)
            {
                continue;
            }
        }
        pDomGrpLimitsOutput->values[i] =
            LW_MAX(pDomGrpLimitsOutput->values[i],
                    pDomGrpLimitsInput->values[i]);
    }
}

/*!
 * Adjusts a specified set of Domain Group limits ref@ pDomGrpLimitsOutput by
 * placing a ceiling specified by the packed domain group limits
 * ref@ pDomGrpLimitsInput
 *
 * @param[in, out] pDomGrpLimitsOutput
 *     PERF_DOMAIN_GROUP_LIMITS pointer containing the Domain Group
 *     limits to adjust.
 * @param[in]       pDomGrpLimitsInput
 *     RM_PMU_PERF_DOMAIN_GROUP_LIMITS pointer containing the Domain Group
 *     limits as ref for ceiling value.
 *
 */
void
pwrPolicyGetDomGrpLimitsCeiling
(
    PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimitsOutput,
    PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimitsInput
)
{
    LwU8 i;

    for (i = 0; i < PERF_DOMAIN_GROUP_MAX_GROUPS; i++)
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_DOMAIN_GROUP_LIMITS_MASK))
        {
            if (!PWR_POLICIES_DOMGRP_LIMIT_MASK_BIT_GET(i))
            {
                continue;
            }
        }

        pDomGrpLimitsOutput->values[i] =
            LW_MIN(pDomGrpLimitsOutput->values[i],
                    pDomGrpLimitsInput->values[i]);
    }
}

/*!
 * Set the @domGrpLimitsPolicyMask
 *
 * @return FLCN_OK Mask is correctly set 
 */
FLCN_STATUS
pwrPoliciesDomGrpLimitPolicyMaskSet(void)
{
    FLCN_STATUS status = FLCN_OK;
    PWR_POLICIES_35  *pPolicies35 = PWR_POLICIES_35_GET();
    LwBoardObjIdx     i;

    //
    // Populate domGrpLimitPolicyMask by iterating over all
    // policies of type _DOMGRP
    //    
    BOARDOBJGRPMASK_E32 *pTmpMask;
    BOARDOBJGRPMASK_E32 *pPwrPolicyDomGrpLimitPolicyMask =
        pwrPolicies35GetDomGrpLimitsMask(pPolicies35);
    if (pPwrPolicyDomGrpLimitPolicyMask == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NOT_SUPPORTED;
        goto pwrPoliciesDomGrpLimitPolicyMaskSet_exit;
    }

    // Init the pPwrPolicyDomGrpLimitPolicyMask.
    boardObjGrpMaskInit_E32(pPwrPolicyDomGrpLimitPolicyMask);

    // Create the domGrpLimitMask for all policies
    FOR_EACH_INDEX_IN_MASK(32, i, pPolicies35->super.super.domGrpPolicyMask)
    {
        PWR_POLICY_DOMGRP *pPolicy;
        pPolicy = (PWR_POLICY_DOMGRP *)PWR_POLICY_GET(i);
        if (pPolicy == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto pwrPoliciesDomGrpLimitPolicyMaskSet_exit;
        }
        status = pwrPolicyDomGrpLimitsMaskSet(pPolicy);  
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto pwrPoliciesDomGrpLimitPolicyMaskSet_exit;
        }

        //
        // Create the domGrpLimitPolicyMask which is a superset of all
        // individual policies
        //
        pTmpMask = pwrPolicyDomGrpLimitsMaskGet(pPolicy);
        boardObjGrpMaskOr(pPwrPolicyDomGrpLimitPolicyMask,
            pPwrPolicyDomGrpLimitPolicyMask, pTmpMask);        
    }
    FOR_EACH_INDEX_IN_MASK_END;

pwrPoliciesDomGrpLimitPolicyMaskSet_exit:
    return status;
}

/*!
 * @copydoc PwrPolicyDomGrpLimitsMaskSet
 */
FLCN_STATUS
pwrPolicyDomGrpLimitsMaskSet_SUPER
(
    PWR_POLICY_DOMGRP *pPolicy
)
{    
    //
    // Default the domain group limit mask to support
    // PERF_DOMAIN_GROUP_GPC2CLK and PERF_DOMAIN_GROUP_PSTATE
    // Note - Each policy can override this to support more
    // domain group limits
    //
    BOARDOBJGRPMASK_E32 *pPwrPolicyDomGrpLimitsMask;
    pPwrPolicyDomGrpLimitsMask = pwrPolicyDomGrpLimitsMaskGet(pPolicy);
    if (pPwrPolicyDomGrpLimitsMask == NULL)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_STATE;
    }
    boardObjGrpMaskInit_E32(pPwrPolicyDomGrpLimitsMask);
    boardObjGrpMaskBitSet(pPwrPolicyDomGrpLimitsMask,
        PERF_DOMAIN_GROUP_PSTATE);
    boardObjGrpMaskBitSet(pPwrPolicyDomGrpLimitsMask,
        PERF_DOMAIN_GROUP_GPC2CLK);

    return FLCN_OK;
}

/*!
 * @copydoc PwrPolicyDomGrpIsCapped
 */
LwBool
pwrPolicyDomGrpIsCapped_SUPER
(
    PWR_POLICY_DOMGRP *pPolicy
)
{
    LwU8 domGrp;
    if (pPolicy == NULL)
    {
        PMU_BREAKPOINT();
        return LW_FALSE;
    }

    for (domGrp = 0; domGrp < PERF_DOMAIN_GROUP_MAX_GROUPS; domGrp++)
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_DOMAIN_GROUP_LIMITS_MASK))
        {
            if (!PWR_POLICIES_DOMGRP_LIMIT_MASK_BIT_GET(domGrp))
            {
                continue;
            }
        }

        if ((pPolicy->domGrpLimits.values[domGrp]) <
             pPolicy->domGrpCeiling.values[domGrp])
        {
            return LW_TRUE;
        }
    }
    return LW_FALSE;
}

/*!
 * @copydoc PwrPolicyDomGrpIsCapped
 */
LwBool
pwrPolicyDomGrpIsCapped
(
    PWR_POLICY_DOMGRP *pPolicy
)
{
    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_COMBINED_1X:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X);
            pPolicy->bIsCapped = pwrPolicyDomGrpIsCapped_WORKLOAD_COMBINED_1X(pPolicy);
            break;
        default:
            pPolicy->bIsCapped = pwrPolicyDomGrpIsCapped_SUPER(pPolicy);
            break;
    }

    return pPolicy->bIsCapped;
}

/*!
 * @copydoc PwrPolicyDomGrpLimitsMaskSet
 */
FLCN_STATUS
pwrPolicyDomGrpLimitsMaskSet
(
    PWR_POLICY_DOMGRP *pPolicy
)
{
    FLCN_STATUS status = FLCN_OK;
    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        //
        // All other policies have default @ref domGrpLimitMask initialised
        // to support _PSTATE and _GPC2CLK.
        //
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_COMBINED_1X:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X);
            status = pwrPolicyDomGrpLimitsMaskSet_WORKLOAD_COMBINED_1X(pPolicy);
            break;
        default:
            status = pwrPolicyDomGrpLimitsMaskSet_SUPER(pPolicy);
            break;
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrPolicyDomGrpLimitsMaskSet_exit;
    }
pwrPolicyDomGrpLimitsMaskSet_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @interface PWR_POLICY_DOMGRP
 *
 * Helper function to retrieve the Domain Group ceiling (i.e. highest allowed
 * pstate and decouple domain values) for a given PWR_POLICY_DOMGRP object.
 * This interface will include the global Domain Group ceiling settings via @ref
 * PwrPolicyDomGrpGlobalCeilingGet(), as well any PWR_POLICY_DOMGRP specific
 * values.
 *
 * @param[in]   pDomGrp
 *     PWR_POLICY_DOMGRP pointer.
 * @param[out]  pDomGrpCeiling
 *     Pointer in which to return the Domain Group ceiling values for the given
 *     PWR_POLICY_DOMGRP object.
 */
static FLCN_STATUS
s_pwrPolicyDomGrpCeilingGet
(
    PWR_POLICY_DOMGRP        *pDomGrp,
    PERF_DOMAIN_GROUP_LIMITS *pDomGrpCeiling
)
{
    FLCN_STATUS status;
    PERF_DOMAIN_GROUP_LIMITS domGrpLimits;
    PERF_DOMAIN_GROUP_LIMITS domGrpCeilingLimit;
    PWR_POLICIES_INFLECTION_POINTS_DISABLE *pDisable;

    // Retrieve the inflection point disable structure for use later.
    PMU_ASSERT_OK_OR_GOTO(status,
        pwrPoliciesInflectionPointsDisableGet(
            PWR_POLICIES_GET(), &pDisable),
        s_pwrPolicyDomGrpCeilingGet_exit);

    //
    // Initialize to the _MAX_CLOCK VPSTATE - i.e. the highest (pstate, VF)
    // point on the GPU.  This is the default, unbounded ceiling.
    //
    perfPstatePackPstateCeilingToDomGrpLimits(&domGrpCeilingLimit);
    PMU_ASSERT_OK_OR_GOTO(status,
        s_pwrPolicyDomGrpDomainGroupLimitsCopy(
            pDomGrpCeiling, &domGrpCeilingLimit),
        s_pwrPolicyDomGrpCeilingGet_exit);

    // 1. Retrieve the global ceiling.
    pwrPolicyGetDomGrpLimitsCeiling(pDomGrpCeiling,
        pwrPolicyDomGrpGlobalCeilingGet());

    //
    // 2. If this PWR_POLICY_DOMGRP object's inflection limit is violated, bound
    // the ceiling to the inflection vpstate.
    // The current limit can swing because of integral controller and the inflection
    // limits are set w.r.t the pre-integral limits. So use ArbLwrr instead.
    //
    if (pwrPolicyLimitArbLwrrGet(&pDomGrp->super) < pDomGrp->limitInflection)
    {
        status = pwrPolicyGetDomGrpLimitsByVpstateIdx(
            RM_PMU_PERF_VPSTATE_IDX_INFLECTION0, &domGrpLimits);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_pwrPolicyDomGrpCeilingGet_exit;
        }

        pwrPolicyGetDomGrpLimitsCeiling(pDomGrpCeiling, &domGrpLimits);
    }

    //
    // If the inflection disable structure exists, check it to see if inflection
    // points should be disabled.
    //
    if (pDisable != NULL)
    {
        const LwBoardObjIdx pstateIdxDomGrpCeiling = PSTATE_GET_INDEX_BY_LEVEL(
            pDomGrpCeiling->values[PERF_DOMAIN_GROUP_PSTATE]);
        LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST *pLastInflectionState;
        LwBoardObjIdx pstateIdxInflectionDisableMin;

        PMU_ASSERT_OK_OR_GOTO(status,
            pwrPolicyDomGrpLastInflectionStateGet(
                pDomGrp, &pLastInflectionState),
            s_pwrPolicyDomGrpCeilingGet_exit);
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pLastInflectionState != NULL),
            FLCN_ERR_ILWALID_STATE,
            s_pwrPolicyDomGrpCeilingGet_exit);

        //
        // If the ceiling has changed, then we want to ilwalidate previous
        // disablement requests by updating the latest timestamp, and we then
        // store the new ceiling value.
        //
        // Note that there are some requests that may always be valid, so we
        // still have to check for disablement.
        //
        if (pLastInflectionState->pstateIdxLowest !=
                pstateIdxDomGrpCeiling)
        {
            osPTimerTimeNsLwrrentGetUnaligned(
                &pLastInflectionState->timestamp);
            pLastInflectionState->pstateIdxLowest =
                pstateIdxDomGrpCeiling;
        }

        //
        // Retrieve the minimum PSTATE index at which inflection is disabled,
        // and only continue if it's a valid index
        //
        PMU_ASSERT_OK_OR_GOTO(status,
            pwrPoliciesInflectionPointsDisableLowestDisabledPstateGet(
                pDisable,
                pLastInflectionState->timestamp,
                &pstateIdxInflectionDisableMin),
            s_pwrPolicyDomGrpCeilingGet_exit);

        if (pstateIdxInflectionDisableMin !=
                LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID)
        {
            //
            // Translate the index to a level, to compare with levels in the
            // DOMGRPs
            //
            const LwU8 pstateLevelInflectionDisableMin =
                perfPstateGetLevelByIndex(pstateIdxInflectionDisableMin);
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pstateLevelInflectionDisableMin !=
                    LW2080_CTRL_PERF_PSTATE_LEVEL_ILWALID),
                FLCN_ERR_ILWALID_STATE,
                s_pwrPolicyDomGrpCeilingGet_exit);

            //
            // If the ceiling's PSTATE is greater than or equal to the minimum
            // disabled PSTATE, then reset the limits to the initial values.
            //
            if (pDomGrpCeiling->values[PERF_DOMAIN_GROUP_PSTATE] >=
                    pstateLevelInflectionDisableMin)
            {
                PMU_ASSERT_OK_OR_GOTO(status,
                    s_pwrPolicyDomGrpDomainGroupLimitsCopy(
                        pDomGrpCeiling, &domGrpCeilingLimit),
                    s_pwrPolicyDomGrpCeilingGet_exit);
            }
        }
    }

    //
    // 3. If required, impose full deflection vpstate ceiling on this
    // PWR_POLICY_DOMPGRP's limits. This is done b/c the BangBangVF
    // controller does not cap/uncap from the limits initialized in
    // pwrPolicyDomGrpConstruct.
    //
    if (pDomGrp->bFullDeflectiolwpstateCeiling)
    {
        status = pwrPolicyGetDomGrpLimitsByVpstateIdx(
            RM_PMU_PERF_VPSTATE_IDX_FULL_DEFLECTION, &domGrpLimits);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_pwrPolicyDomGrpCeilingGet_exit;
        }

        pwrPolicyGetDomGrpLimitsCeiling(pDomGrpCeiling, &domGrpLimits);

        pDomGrp->bFullDeflectiolwpstateCeiling = LW_FALSE;
    }

s_pwrPolicyDomGrpCeilingGet_exit:
    return status;
}

/*!
 * Submits the requested Perf Domain Groups Limits to the RM arbiter.
 *
 * @param[in] pDomGrpLimits  Pointer to requested Domain Group Limits. The
 *                           buffer used must be able to contain
 *                           RM_PMU_PERF_DOMAIN_GROUP_LIMITS_ID_NUM elements.
 * @param[in] bSkipVblank
 *     Specifies whether corresponding RM pstate change should skip waiting for
 *     blank.
 */
static FLCN_STATUS
s_pwrPolicyDomGrpLimitsSetAll_RM
(
    PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits,
    LwBool                    bSkipVblank
)
{
    PMU_RM_RPC_STRUCT_PERF_DOMAIN_GROUP_LIMIT   rpc;
    FLCN_STATUS                                 status;
    LwU8                                        limitId;

    // Copy out the set of domain group limits.
    for (limitId = 0; limitId < RM_PMU_PERF_DOMAIN_GROUP_LIMITS_ID_NUM; limitId++)
    {
        perfDomGrpLimitsCopyOut(&(rpc.domGrpLimits[limitId]),
            &(pDomGrpLimits[limitId]));
    }
    rpc.bSkipVblank = bSkipVblank;

    // Send the Event RPC to the RM to change the limits (and hence the clocks!).
    PMU_RM_RPC_EXELWTE_BLOCKING(status, PERF, DOMAIN_GROUP_LIMIT, &rpc);

    // Cache the latest settings in OBJPERF object.
    for (limitId = 0; limitId < RM_PMU_PERF_DOMAIN_GROUP_LIMITS_ID_NUM; limitId++)
    {
        Perf.domGrpLimits[limitId] = pDomGrpLimits[limitId];
    }

    return status;
}

/*!
 * Submits the requested Perf Domain Groups Limits to the PMU arbiter.
 *
 * @param[in] pDomGrpLimits  Pointer to requested Domain Group Limits. The
 *                           buffer used must be able to contain
 *                           RM_PMU_PERF_DOMAIN_GROUP_LIMITS_ID_NUM elements.
 */
static FLCN_STATUS
s_pwrPolicyDomGrpLimitsSetAll_PMU
(
    PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits
)
{
    // Mapping of PERF_DOMAIN_GROUPS to actual PERF_LIMIT_IDs
    static const LW2080_CTRL_PERF_PERF_LIMIT_ID
    limits[RM_PMU_PERF_DOMAIN_GROUP_LIMITS_ID_NUM]
          [PERF_DOMAIN_GROUP_MAX_GROUPS] =
    {
        {
            LW2080_CTRL_PERF_PERF_LIMIT_ID_PMU_DOM_GRP_0,
            LW2080_CTRL_PERF_PERF_LIMIT_ID_PMU_DOM_GRP_1,
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_DOMAIN_GROUP_LIMITS_XBAR)
            LW2080_CTRL_PERF_PERF_LIMIT_ID_PWR_POLICY_XBAR,
#endif
        },
        {
            LW2080_CTRL_PERF_PERF_LIMIT_ID_THERM_POLICY_DOM_GRP_0,
            LW2080_CTRL_PERF_PERF_LIMIT_ID_THERM_POLICY_DOM_GRP_1,
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_DOMAIN_GROUP_LIMITS_XBAR)
            LW2080_CTRL_PERF_PERF_LIMIT_ID_THERM_POLICY_XBAR,
#endif
        },
    };
    PERF_LIMITS_CLIENT *pClient = Pmgr.pPerfLimits;
    FLCN_STATUS         status;
    LwU8                domGrpLimitsIdx;
    LwU8                domGrpIdx;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPerfLimitClient)
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, pmgrPwrPoliciesPerfLimits)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        PERF_LIMITS_CLIENT_SEMAPHORE_TAKE(pClient);
        {
            for (domGrpLimitsIdx = 0; domGrpLimitsIdx < RM_PMU_PERF_DOMAIN_GROUP_LIMITS_ID_NUM; domGrpLimitsIdx++)
            {
                for (domGrpIdx = 0; domGrpIdx < PERF_DOMAIN_GROUP_MAX_GROUPS; domGrpIdx++)
                {
                    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_DOMAIN_GROUP_LIMITS_MASK))
                    {
                        if (!PWR_POLICIES_DOMGRP_LIMIT_MASK_BIT_GET(domGrpIdx))
                        {
                            continue;
                        }
                    }

                    LW2080_CTRL_PERF_PERF_LIMIT_ID limitId = limits[domGrpLimitsIdx][domGrpIdx];
                    PERF_LIMITS_CLIENT_ENTRY *pEntry = perfLimitsClientEntryGet(pClient, limitId);
                    if (pEntry == NULL)
                    {
                        continue;
                    }

                    // Clear limit if requested
                    if (pDomGrpLimits[domGrpLimitsIdx].values[domGrpIdx] ==
                        RM_PMU_PREF_DOMAIN_GROUP_LIMIT_VALUE_DISABLE)
                    {
                        PERF_LIMIT_CLIENT_CLEAR_LIMIT(pEntry, limitId);
                    }
                    // Otherwise, set the limit based on the DOM_GRP index
                    else
                    {
                        switch (domGrpIdx)
                        {
                            case RM_PMU_PERF_DOMAIN_GROUP_PSTATE:
                            {
                                // Set DOM_GRP_0 (P-states)
                                PSTATE *pPstate = PSTATE_GET_BY_LEVEL(
                                    pDomGrpLimits[domGrpLimitsIdx].values[domGrpIdx]);
                                LwU8 pstateIdx;
                                if (pPstate == NULL)
                                {
                                    PMU_BREAKPOINT();
                                    continue;
                                }

                                pstateIdx = BOARDOBJ_GET_GRP_IDX(&pPstate->super);
                                PERF_LIMITS_CLIENT_PSTATE_LIMIT(pEntry, limitId,
                                    //pDomGrpLimits[domGrpLimitsIdx].values[domGrpIdx],
                                    pstateIdx,
                                    LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_PSTATE_POINT_NOM);
                                break;
                            }
                            case RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK:
                            {
                                // Get CLK_DOMAIN index associated with DOM_GRP.
                                LwU8 clkDomIdx = clkDomainsGetIdxByDomainGrpIdx(domGrpIdx);
                                if (LW_U8_MAX == clkDomIdx)
                                {
                                    PMU_BREAKPOINT();
                                    continue;
                                }
                                PERF_LIMITS_CLIENT_FREQ_LIMIT(pEntry, limitId,
                                    clkDomIdx,
                                    pDomGrpLimits[domGrpLimitsIdx].values[domGrpIdx]);
                                break;
                            }
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_DOMAIN_GROUP_LIMITS_XBAR)
                            case RM_PMU_PERF_DOMAIN_GROUP_XBARCLK:
                            {
                                LwU32 clkDomIdx;
                                // Get CLK_DOMAIN index associated with DOM_GRP.
                                status = clkDomainsGetIndexByApiDomain(LW2080_CTRL_CLK_DOMAIN_XBARCLK,
                                    (&clkDomIdx));
                                if (status != FLCN_OK)
                                {
                                    PMU_BREAKPOINT();
                                    continue;
                                }
                                PERF_LIMITS_CLIENT_FREQ_LIMIT(pEntry, limitId,
                                    (LwU8)clkDomIdx,
                                    pDomGrpLimits[domGrpLimitsIdx].values[domGrpIdx]);
                                break;
                            }  
#endif                        
                            default:
                            {
                                PMU_BREAKPOINT();
                                status = FLCN_ERR_ILWALID_INDEX;
                                goto s_pwrPolicyDomGrpLimitsSetAll_PMU_exit;
                            }
                        }
                    }
                }
            }

            pClient->hdr.applyFlags = LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ASYNC;
            status = perfLimitsClientArbitrateAndApply(pClient);

s_pwrPolicyDomGrpLimitsSetAll_PMU_exit:
            lwosNOP();
        }
        PERF_LIMITS_CLIENT_SEMAPHORE_GIVE(pClient);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * Query the piecewise frequency floor data source for the current floor
 * value of GPC given their last observed domain value (i.e. power, temperature, etc)
 * and apply this to pDomGrpLimits.
 */
static FLCN_STATUS
s_pwrPolicyGetDomGrpLimitsByPffSource
(
    LW2080_CTRL_PMGR_PWR_POLICY_INFO_DATA_DOMGRP_PFF_SOURCE    *pPffSource,
    PERF_DOMAIN_GROUP_LIMITS                                   *pDomGrpLimits
)
{
    LwU32       freqFloorkHz    = 0;
    FLCN_STATUS status          = FLCN_ERR_NOT_SUPPORTED;

    switch (pPffSource->policyType)
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_DOMGRP_PFF_SOURCE_POWER:
        PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY);
        {
            PWR_POLICY *pPwrPolicy = PWR_POLICY_GET(pPffSource->policyIdx);
            if (pPwrPolicy == NULL)
            {
                status = FLCN_ERR_ILWALID_INDEX;
                PMU_BREAKPOINT();
                goto s_pwrPolicyGetDomGrpLimitsByPffSource_exit;
            }

            status = pwrPolicyGetPffCachedFrequencyFloor(pPwrPolicy, &freqFloorkHz);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto s_pwrPolicyGetDomGrpLimitsByPffSource_exit;
            }
            break;
        }
        case LW2080_CTRL_PMGR_PWR_POLICY_DOMGRP_PFF_SOURCE_THERMAL:
        PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_THERM_POLICY);
        {
            THERM_POLICY *pThermPolicy = THERM_POLICY_GET(pPffSource->policyIdx);
            if (pThermPolicy == NULL)
            {
                status = FLCN_ERR_ILWALID_INDEX;
                PMU_BREAKPOINT();
                goto s_pwrPolicyGetDomGrpLimitsByPffSource_exit;
            }

            status = thermPolicyGetPffCachedFrequencyFloor(pThermPolicy, &freqFloorkHz);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto s_pwrPolicyGetDomGrpLimitsByPffSource_exit;
            }
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_pwrPolicyGetDomGrpLimitsByPffSource_exit;
    }

    // Set RM_PMU_DOMAIN_GROUP_GPC2CLK with the PFF intersect
    pDomGrpLimits->values[RM_PMU_DOMAIN_GROUP_GPC2CLK] = freqFloorkHz;

    // Default set RM_PMU_DOMAIN_GROUP_PSTATE to 0 (no effect during flooring operation)
    pDomGrpLimits->values[RM_PMU_DOMAIN_GROUP_PSTATE] = 0;

s_pwrPolicyGetDomGrpLimitsByPffSource_exit:
    return status;
}

/*!
 * Function to cache the arbitrated global ceiling
 */
void
pwrPoliciesDomGrpGlobalCeilingCache(void)
{
    PWR_POLICIES *pPolicies   = Pmgr.pPwrPolicies;
    LwU8          i;

    // Initialise cached ceiling 
    pwrPolicyDomGrpLimitsInit(pwrPolicyDomGrpGlobalCeilingGet());

    // Arbitrate over all active requests to find their max limit.
    for (i = 0; i < pPolicies->domGrpGlobalCeiling.numRequests; i++)
    {
        //
        // Can fail out of loop on first _ILWALID policyIdx, as array will be
        // filled in ascending order.
        //
        if (LW2080_CTRL_PMGR_PWR_POLICY_INDEX_ILWALID ==
                pPolicies->domGrpGlobalCeiling.pRequests[i].policyIdx)
        {
            break;
        }

        // Impose the requested ceiling.
        pwrPolicyGetDomGrpLimitsCeiling(
            pwrPolicyDomGrpGlobalCeilingGet(),
            &pPolicies->domGrpGlobalCeiling.pRequests[i].domGrpCeiling);
    }
}

/*!
 * @brief   Copies one @ref PERF_DOMAIN_GROUP_LIMITS structure into another.
 *
 * @param[out]  pDst    @ref PERF_DOMAIN_GROUP_LIMITS into which to copy
 * @param[in]   pSrc    @ref PERF_DOMAIN_GROUP_LIMITS out of which to copy
 *
 * @return  @ref FLCN_OK    Always
 */
static FLCN_STATUS
s_pwrPolicyDomGrpDomainGroupLimitsCopy
(
    PERF_DOMAIN_GROUP_LIMITS *pDst,
    const PERF_DOMAIN_GROUP_LIMITS *pSrc
)
{
    LwU8 i;

    for (i = 0; i < PERF_DOMAIN_GROUP_MAX_GROUPS; i++)
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_DOMAIN_GROUP_LIMITS_MASK))
        {
            if (!PWR_POLICIES_DOMGRP_LIMIT_MASK_BIT_GET(i))
            {
                continue;
            }
        }
        pDst->values[i] = pSrc->values[i];
    }

    return FLCN_OK;
}
