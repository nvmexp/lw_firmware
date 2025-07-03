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
 * @file   pwrpolicies.c
 * @brief  Interface specification for PWR_POLICIES.
 *
 * The PWR_POLICIES is a group of PWR_POLICY and some global data fields.
 * Functions inside this file applies to the whole PWR_POLICIES instead of
 * a single PWR_POLICY. for PWR_POLICY functions, please refer to
 * pwrpolicy_3x.c.
 */
/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dmemovl.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objperf.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrpolicies.h"
#include "dbgprintf.h"
#include "pmu_oslayer.h"
#include "task_therm.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------- Type Definitions ------------------------------- */
/*!
 * This is the penalty period over which capping should be applied
 * after a device on the channel has reported a tampering incident.
 */
#define  PWR_POLICIES_TAMPER_PENALTY_PERIOD_NS  3000000000u

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_SEMAPHORE)
LwrtosSemaphoreHandle PmgrPwrPoliciesSemaphore;
#endif

extern OS_TMR_CALLBACK OsCBPwrPolicy;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static void s_pwrPoliciesArbitrateAndApply(PWR_POLICIES *pPolicies)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPoliciesArbitrateAndApply");
static LwBool s_pwrPoliciesIsTamperCappingNeeded(PWR_POLICIES *pPolicies)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPoliciesIsTamperCappingNeeded");
static void s_pwrPoliciesDomGrpEvaluate(PWR_POLICIES *pPolicies, LwU32 expiredPolicyMask)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPoliciesDomGrpEvaluate");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Public function called from constructPmgr function to create
 * semaphore used by PWR_POLICIES
 *
 * @return  FLCN_OK     On success
 * @return  Others      Error return value from failed child function
 */
FLCN_STATUS
constructPmgrPwrPolicies(void)
{
    FLCN_STATUS status = FLCN_OK;

    // Create PWR_POLICIES binary semaphore for locking critical section
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_SEMAPHORE))
    {
        status = lwrtosSemaphoreCreateBinaryGiven(&PmgrPwrPoliciesSemaphore,
                                                  OVL_INDEX_OS_HEAP);
        if (status != FLCN_OK)
        {
            goto constructPmgrPwrPolicies_exit;
        }
    }

constructPmgrPwrPolicies_exit:
    return status;
}

/*!
 * Constructs the PWR_POLICY object (as applicable).
 *
 * Initializes the DMEM overlay used by the power policies to set/clear
 * PERF_LIMITS.
 *
 * @copydoc PwrPoliciesConstruct
 */
FLCN_STATUS
pwrPoliciesConstruct
(
    PWR_POLICIES **ppPwrPolicies
)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_35))
    {
        pwrPoliciesConstruct_35(ppPwrPolicies);
    }
    // TODO: Add _30 and _2X here for cases which can be pre-determined.

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_DOMGRP))
    {
        status = pwrPoliciesPostInit_DOMGRP();
    }

    return status;
}

/*!
 * Construct Header of Power Policy Table. This will initialize all data
 * that does not belong to any policy entry or policyRel entry.
 *
 * @param[in/out] ppPolicies
 *     Pointer to PWR_POLICIES * pointer at which to populate PWR_POLICIES
 *     structure.
 * @param[in]     pMon
 *     Pointer to power monitor object.
 * @param[in]     pHdr
 *     Pointer to RM buffer specifying requested set of PWR_POLICYs.
 *
 * @return FLCN_OK
 *     Successful construction of all supported PWR_POLICYs.
 */
FLCN_STATUS
pwrPoliciesConstructHeader
(
    PWR_POLICIES                                 **ppPolicies,
    PWR_MONITOR                                   *pMon,
    RM_PMU_PMGR_PWR_POLICY_BOARDOBJGRP_SET_HEADER *pHdr
)
{
    FLCN_STATUS   status;
    PWR_POLICIES *pPolicies      = *ppPolicies;
    PWR_POLICIES_INFLECTION_POINTS_DISABLE *pDisable;

    pPolicies->version               = pHdr->version;
    pPolicies->pMon                  = pMon;
    pPolicies->bEnabled              = pHdr->bEnabled;
    // TODO-JBH: Remove bPmuPerfLimitsClient once PS35 is fully enabled
    pPolicies->bPmuPerfLimitsClient  = pHdr->bPmuPerfLimitsClient;
    pPolicies->domGrpPolicyMask      = pHdr->domGrpPolicyMask;
    pPolicies->domGrpPolicyPwrMask   = pHdr->domGrpPolicyPwrMask;
    pPolicies->domGrpPolicyThermMask = pHdr->domGrpPolicyThermMask;
    pPolicies->limitPolicyMask       = pHdr->limitPolicyMask;
    pPolicies->balancePolicyMask     = pHdr->balancePolicyMask;

    perfDomGrpLimitsCopyIn(&pPolicies->globalCeiling, &pHdr->globalCeiling);

    //
    // Temporary hack since RM is not equipped to 
    // send down XBAR limits.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_DOMAIN_GROUP_LIMITS_XBAR))
    {
        pPolicies->globalCeiling.values[PERF_DOMAIN_GROUP_XBARCLK] =
            RM_PMU_PREF_DOMAIN_GROUP_LIMIT_VALUE_DISABLE;
    }

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_SEMANTIC_POLICY_TABLE))
    LwU8 i;
    for (i = 0; i < LW2080_CTRL_PMGR_PWR_POLICY_IDX_NUM_INDEXES; i++)
    {
        pPolicies->semanticPolicyTbl[i] = pHdr->semanticPolicyTbl[i];
    }
#endif

    PMU_ASSERT_OK_OR_GOTO(status,
        pwrPoliciesInflectionPointsDisableGet(
            pPolicies, &pDisable),
        pwrPoliciesConstructHeader_exit);

    // Request or clear API inflection point disablement.
    if (pDisable != NULL)
    {
        if (LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_VALID(
                &pHdr->inflectionPointsDisableRequest))
        {
            //
            // Override the timestamp for the API request so that it is never
            // ilwalidated.
            //
            LwU64_ALIGN32_PACK(
                &pHdr->inflectionPointsDisableRequest.timestamp,
                &(LwU64){ LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_TIMESTAMP_INFINITELY_RECENT });

            PMU_ASSERT_OK_OR_GOTO(status,
                pwrPoliciesInflectionPointsDisableRequest(
                    pDisable,
                    LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID_API,
                    &pHdr->inflectionPointsDisableRequest),
                pwrPoliciesConstructHeader_exit);
        }
        else
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                pwrPoliciesInflectionPointsDisableClear(
                    pDisable,
                    LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID_API),
                pwrPoliciesConstructHeader_exit);
        }
    }
    else
    {
        //
        // Ensure that profiles that don't support inflection points don't have
        // clients try to disable them.
        //
        PMU_ASSERT_TRUE_OR_GOTO(status,
            !LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_VALID(
                &pHdr->inflectionPointsDisableRequest),
            FLCN_ERR_ILWALID_ARGUMENT,
            pwrPoliciesConstructHeader_exit);
    }

pwrPoliciesConstructHeader_exit:
    return status;
}

/*!
 * PWR_POLICY handler for the RM_PMU_BOARDOBJ_CMD_GRP_GET_STATUS interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
pwrPolicyBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS     status;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libLeakage)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libFxpBasic)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libFxpExtended)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        ATTACH_AND_LOAD_OVERLAY_PWR_POLICY_CLIENT_LOCKED;
        {
            status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS(E32,                // _grpType
                PWR_POLICY,                                     // _class
                pBuffer,                                        // _bu
                pmgrPwrPolicyIfaceModel10GetStatusHeader,                   // _hf
                pwrPolicyIfaceModel10GetStatus,                                 // _ef
                all.pmgr.pwrPolicyGrpGetStatusPack.policies);   // _sse
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto pwrPolicyBoardObjGrpIfaceModel10GetStatus_exit;
            }

            if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_RELATIONSHIP))
            {
                status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS(E32,                // _grpType
                    PWR_POLICY_RELATIONSHIP,                        // _class
                    pBuffer,                                        // _bu
                    NULL,                                           // _hf
                    pwrPolicyRelationshipIfaceModel10GetStatus,                     // _ef
                    all.pmgr.pwrPolicyGrpGetStatusPack.policyRels); // _sse
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto pwrPolicyBoardObjGrpIfaceModel10GetStatus_exit;
                }
            }

            if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_VIOLATION))
            {
                status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS(E32,                // _grpType
                    PWR_VIOLATION,                                  // _class
                    pBuffer,                                        // _bu
                    NULL,                                           // _hf
                    pwrViolationIfaceModel10GetStatus,                          // _ef
                    all.pmgr.pwrPolicyGrpGetStatusPack.violations); // _sse
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto pwrPolicyBoardObjGrpIfaceModel10GetStatus_exit;
                }
            }

pwrPolicyBoardObjGrpIfaceModel10GetStatus_exit:
            lwosNOP();
        }
        DETACH_OVERLAY_PWR_POLICY_CLIENT_LOCKED;
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * Super function to perform shared evaluation action for Policy 2X and 3X.
 *
 * @copydoc PwrPoliciesEvaluate
 */
void
pwrPoliciesEvaluate_SUPER
(
    PWR_POLICIES    *pPolicies,
    LwU32            expiredPolicyMask,
    BOARDOBJGRPMASK *pExpiredViolationMask
)
{
    LwU32   mask;
    LwU32   i;

    // First evaluate PWR_VIOLATION objects.
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_VIOLATION))
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_DEFINE_LIB_LW64
            OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, therm)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            (void)pwrViolationsEvaluate(pExpiredViolationMask);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }

    //
    // If PWR_VIOLATION objects are enabled we could reach here when
    // the expiredPolicyMask is ZERO so avoid further processing.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_VIOLATION) &&
        (expiredPolicyMask == 0))
    {
        return;
    }

    //
    // Evaluate the set to PWR_POLICY_LIMIT policies, which may request new
    // limit values for the PWR_POLICY_DOMGRP policies.
    //
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_DEFINE_PWR_POLICY_LIMIT_EVALUATE_PIECEWISE_FREQUENCY_FLOOR
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            mask = expiredPolicyMask & pPolicies->limitPolicyMask;
            FOR_EACH_INDEX_IN_MASK(32, i, mask)
            {
                PWR_POLICY_LIMIT *pPolicy;

                pPolicy = (PWR_POLICY_LIMIT *)PWR_POLICY_GET(i);
                if (pPolicy == NULL)
                {
                    PMU_BREAKPOINT();
                    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
                    return;
                }

                (void)pwrPolicyLimitEvaluate(pPolicy);
            }
            FOR_EACH_INDEX_IN_MASK_END;
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }

    //
    // Evaluate the set of PWR_POLICY_BALANCE policies, which may alter the
    // power draw on various input rails, and thus the capping limit requests
    // above.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_BALANCE) &&
        ((mask = expiredPolicyMask & pPolicies->balancePolicyMask) != 0))
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libFxpExtended)
            OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libLeakage)
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPwm)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            FOR_EACH_INDEX_IN_MASK(32, i, mask)
            {
                PWR_POLICY_BALANCE *pPolicy;

                pPolicy = (PWR_POLICY_BALANCE *)PWR_POLICY_GET(i);
                if (pPolicy == NULL)
                {
                    PMU_BREAKPOINT();
                    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
                    return;
                }

                (void)pwrPolicyBalanceEvaluate(pPolicy);
            }
            FOR_EACH_INDEX_IN_MASK_END;
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }

    //
    // Now that all limits are finalized (above), evaluate all policies which
    // take action by changing the clocks.
    //

    //
    // SPTODO: Implement single PwrPolicyEvaluate (interface), don't need
    // virtual-class-specific interfaces anymore.
    //

    //
    // 1. PWR_POLICY_DOMGRP policies which may request new domain group
    // PERF_LIMIT values.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_DOMGRP))
    {
        s_pwrPoliciesDomGrpEvaluate(pPolicies, expiredPolicyMask);
    }

    // Arbitrate and apply all requests.
    s_pwrPoliciesArbitrateAndApply(pPolicies);
}

/*!
 * @copydoc PwrPoliciesStatusGet
 */
FLCN_STATUS
pwrPoliciesStatusGet
(
    PWR_POLICIES *pPwrPolicies,
    PWR_POLICIES_STATUS *pStatus
)
{
    FLCN_STATUS status;
    BOARDOBJGRP_E32 *const pPoliciesBoardObjGrp =
        (BOARDOBJGRP_E32 *)BOARDOBJGRP_GRP_GET(PWR_POLICY);
    PWR_POLICY *pPwrPolicy;
    LwBoardObjIdx i;

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskIsSubset(
                &pStatus->mask, &pPoliciesBoardObjGrp->objMask) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        pwrPoliciesStatusGet_exit);

    osPTimerTimeNsLwrrentGetUnaligned(&pStatus->timestamp);

    BOARDOBJGRP_ITERATOR_BEGIN(PWR_POLICY, pPwrPolicy, i, &pStatus->mask.super)
    {
        BOARDOBJ_IFACE_MODEL_10 *pObjModel10 =
            boardObjIfaceModel10FromBoardObjGet(&pPwrPolicy->super.super);

        PMU_ASSERT_OK_OR_GOTO(status,
            pwrPolicyIfaceModel10GetStatus(
                pObjModel10,
                &pwrPoliciesStatusPolicyStatusGet(pStatus, i)->boardObj),
            pwrPoliciesStatusGet_exit);
    }
    BOARDOBJGRP_ITERATOR_END;

pwrPoliciesStatusGet_exit:
    return status;
}

/*!
 * @brief   Public function to load all power policies.
 *
 * @param[in]   pPolicies   PWR_POLICIES pointer
 */
FLCN_STATUS
pwrPoliciesLoad
(
    PWR_POLICIES *pPolicies
)
{
    FLCN_STATUS status   = FLCN_OK;
    LwU32       ticksNow = lwrtosTaskGetTickCount32();
    LwU32       i;
    OSTASK_OVL_DESC ovlDescListPolicy[] = {
        OSTASK_OVL_DESC_DEFINE_PWR_POLICIES_LOAD
    };

    // Attach all the overlays list
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListPolicy);
    {
        // Load PWR_POLICY objects only if PWR_POLICY evaluation is enabled.
        if (pPolicies->bEnabled)
        {
            //
            // PWR_VIOLATION objects must be evaluated before PWR_POLICY ones,
            // so keeping same relative order here as well.
            //
            if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_VIOLATION))
            {
                OSTASK_OVL_DESC ovlDescList[] = {
                    OSTASK_OVL_DESC_DEFINE_LIB_LW64
                    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, therm)
                };

                OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
                {
                    status = pwrViolationsLoad(ticksNow);
                }
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto pwrPoliciesLoad_detach;
                }
            }

            //
            // Update all PWR_POLICYs to cache and filter their latest readings from
            // PWR_CHANNELs.
            //
            FOR_EACH_INDEX_IN_MASK(32, i, Pmgr.pwr.policies.objMask.super.pData[0])
            {
                PWR_POLICY *pPolicy;

                pPolicy = PWR_POLICY_GET(i);
                if (pPolicy == NULL)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_INDEX;
                    goto pwrPoliciesLoad_detach;
                }

                status = pwrPolicyLoad(pPolicy, ticksNow);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto pwrPoliciesLoad_detach;
                }
            }
            FOR_EACH_INDEX_IN_MASK_END;
        }

        //
        // Update all PWR_POLICY_RELATIONSHIPs and apply any initial policy actions
        // which may be required.
        // Must always load PWR_POLICY_RELATIONSHIP objects, even if PWR_POLICY
        // evaluation is disabled.  This is necessary for the PWR_CHANNELs and
        // PWR_CHRELATIONSHIPs which depend on the initial values of the
        // PWR_POLICY_RELATIONSHIPs.
        //
        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_RELATIONSHIP_LOAD))
        {
            FOR_EACH_INDEX_IN_MASK(32, i, Pmgr.pwr.policyRels.objMask.super.pData[0])
            {
                PWR_POLICY_RELATIONSHIP *pPolicy;

                pPolicy = PWR_POLICY_REL_GET(i);
                if (pPolicy == NULL)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_INDEX;
                    goto pwrPoliciesLoad_detach;
                }

                status = pwrPolicyRelationshipLoad(pPolicy);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto pwrPoliciesLoad_detach;
                }
            }
            FOR_EACH_INDEX_IN_MASK_END;
        }
    }
pwrPoliciesLoad_detach:
    // Detach all the overlays list
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListPolicy);
    if (status != FLCN_OK)
    {
        goto pwrPoliciesLoad_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_3X) &&
         (Pmgr.pPwrPolicies->version ==
            RM_PMU_PMGR_PWR_POLICY_DESC_TABLE_VERSION_3X))
    {
        status = pwrPolicies3xLoad(PWR_POLICIES_3X_GET(), ticksNow);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto pwrPoliciesLoad_exit;
        }
    }

pwrPoliciesLoad_exit:
    return status;
}

/*!
 * Calls vf ilwalidate on all power policies
 *
 * @return FLCN_OK
 * @return Other errors
 *      Errors propagated up from functions called.
 */
FLCN_STATUS
pwrPoliciesVfIlwalidate
(
    PWR_POLICIES *pPolicies
)
{
    LwU8          i;
    FLCN_STATUS   status = FLCN_OK;

    //
    // Note that when taking both the perf semaphore and the PWR_POLICIES
    // semaphore, the perf semaphore must be taken first.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PIECEWISE_FREQUENCY_FLOOR))
    {
        perfReadSemaphoreTake();
    }
    PWR_POLICIES_SEMAPHORE_TAKE;

    if (pPolicies->bEnabled)
    {
        FOR_EACH_INDEX_IN_MASK(32, i, Pmgr.pwr.policies.objMask.super.pData[0])
        {
            PWR_POLICY *pPolicy;

            pPolicy = PWR_POLICY_GET(i);
            if (pPolicy == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_INDEX;
                goto pwrPoliciesVfIlwalidate_exit;
            }

            status = pwrPolicyVfIlwalidate(pPolicy);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto pwrPoliciesVfIlwalidate_exit;
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }

pwrPoliciesVfIlwalidate_exit:
    PWR_POLICIES_SEMAPHORE_GIVE;
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PIECEWISE_FREQUENCY_FLOOR))
    {
        perfReadSemaphoreGive();
    }
    return status;
}

/*!
 * @Copydoc BoardObjGrpIfaceModel10SetHeader
 */
FLCN_STATUS
pmgrPwrPolicyIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PMGR_PWR_POLICY_BOARDOBJGRP_SET_HEADER *pHdr;
    PWR_POLICIES **ppPolicies = &Pmgr.pPwrPolicies;
    PWR_MONITOR   *pMon       = Pmgr.pPwrMonitor;
    FLCN_STATUS    status;

    // Call to Board Object Group constructor must always be first!
    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pmgrPwrPolicyIfaceModel10SetHeader_exit;
    }

    pHdr = (RM_PMU_PMGR_PWR_POLICY_BOARDOBJGRP_SET_HEADER *)pHdrDesc;

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_LAZY_CONSTRUCT) &&
        (*ppPolicies == NULL))
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_30) &&
            (pHdr->version >= RM_PMU_PMGR_PWR_POLICY_DESC_TABLE_VERSION_3X))
        {
            *ppPolicies = (PWR_POLICIES *)lwosCallocType(pBObjGrp->ovlIdxDmem,
                                                        1, PWR_POLICIES_30);
        }
        else if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_2X))
        {
            *ppPolicies = (PWR_POLICIES *)lwosCallocType(pBObjGrp->ovlIdxDmem,
                                                        1, PWR_POLICIES_2X);
        }

        if (*ppPolicies == NULL)
        {
            status = FLCN_ERR_NO_FREE_MEM;
            goto pmgrPwrPolicyIfaceModel10SetHeader_exit;
        }
    }
    //
    // If LAZY_CONSTRUCT disabled and *ppPolicies are not pre-constructed
    // (i.e. NULL) bail out with error.  Need to implement @ref
    // PwrPoliciesConstruct (for) the given PWR_POLICIES version.
    //
    else if (*ppPolicies == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto pmgrPwrPolicyIfaceModel10SetHeader_exit;
    }

    // Set the global PWR_POLICIES state.
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_3X) &&
        (pHdr->version >= RM_PMU_PMGR_PWR_POLICY_DESC_TABLE_VERSION_3X))
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_35))
        {
            pwrPoliciesConstructHeader_35(ppPolicies, pMon, pHdr);
        }
        else
        {
            pwrPoliciesConstructHeader_3X(ppPolicies, pMon, pHdr);
        }
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_2X))
    {
        pwrPoliciesConstructHeader_2X(ppPolicies, pMon, pHdr);
    }
    //
    // If RM and PMU state are not matching up, throw PMU_BREAKPOINT() and
    // return error.
    //
    else
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto pmgrPwrPolicyIfaceModel10SetHeader_exit;
    }

pmgrPwrPolicyIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * Set penalty start time if PWR CHANNEL has been compromised
 *
 * Should be called by PWR CHANNEL in case tampering is detected
 *
 * @param[in] pPolices  PWR_POLICIES pointer
 */
void
pwrPoliciesPwrChannelTamperDetected
(
    PWR_POLICIES *pPolicies
)
{
    if (pPolicies == NULL)
    {
        PMU_BREAKPOINT();
        return;
    }

    pPolicies->bChannelTampered = LW_TRUE;

    osPTimerTimeNsLwrrentGet(&pPolicies->tamperPenaltyStart);
}
/* ------------------------- Private Functions ------------------------------ */
/*!
 * Helper function to arbitrate between current requests/decisions of all power
 * policies and then apply that resulting output to the SW/HW state.
 *
 * @param[in] pPolices  PWR_POLICIES pointer
 */
static void
s_pwrPoliciesArbitrateAndApply
(
    PWR_POLICIES *pPolicies
)
{
    PERF_DOMAIN_GROUP_LIMITS
        domGrpLimitsOutput[RM_PMU_PERF_DOMAIN_GROUP_LIMITS_ID_NUM];
    LwU8   i;
    LwBool bCapped = LW_FALSE;

    //
    // SPTODO: Move these arbitration functions to each virtual class's
    // module.
    //

    // 1. Domain Group Limits:
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_DOMGRP) &&
        (pPolicies->domGrpPolicyMask != 0))
    {
        //
        // 1a. Initialize the arbitrated domain-group output buffers to
        // completely uncapped.
        //
        for (i = 0; i < RM_PMU_PERF_DOMAIN_GROUP_LIMITS_ID_NUM; i++)
        {
            pwrPolicyDomGrpLimitsInit(&domGrpLimitsOutput[i]);
        }

        //
        // 1b: Check for channel tampering and apply penalty limit
        // if needed
        //
        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_TAMPER_DETECT_HANDLE_2X))
        {
            //
            // Power Policy Tamper Handling
            //
            // If any device of a channel has been tampered with,
            // restrict GPC freqency to the min frequency of P0, and skip
            // policy limit arbitration
            //
            // @see s_pwrPoliciesIsTamperCappingNeeded and
            // pwrPoliciesPwrChannelTamperDetected for tamper detection logic
            //
            if ((s_pwrPoliciesIsTamperCappingNeeded(pPolicies)) &&
                (perfGetPstateCount() != 0))
            {
                LwU32 minFreqKHz;
                PERF_DOMAIN_GROUP_LIMITS domGrpLimitsInput;

                pwrPolicyDomGrpLimitsInit(&domGrpLimitsInput);

                // TODO: hide this ugly check with PSTATE interfaces
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ELW_PSTATES_3X))
                {
                    LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY pstateClkEntry;

                    perfPstateClkFreqGet(PSTATE_GET_BY_LEVEL(perfGetPstateCount() - 1),
                        clkDomainsGetIdxByDomainGrpIdx(RM_PMU_DOMAIN_GROUP_GPC2CLK),
                        &pstateClkEntry);

                    minFreqKHz = pstateClkEntry.min.freqkHz;
                }
#else
                {
                    RM_PMU_PERF_PSTATE_DOMAIN_GROUP_INFO *pPstateDomGrp;

                    // Get the domain group for P0
                    pPstateDomGrp =
                        perfPstateDomGrpGet(perfGetPstateCount() - 1,
                            RM_PMU_DOMAIN_GROUP_GPC2CLK);
                    minFreqKHz = pPstateDomGrp->minFreqKHz;
                }
#endif
                // Set GPC freq limit to the minimum value of the capped PState
                domGrpLimitsInput.values[RM_PMU_DOMAIN_GROUP_GPC2CLK] =
                    minFreqKHz;

                pwrPolicyDomGrpLimitsArbitrate(
                    &domGrpLimitsOutput[RM_PMU_PERF_DOMAIN_GROUP_LIMITS_ID_PWR],
                    &domGrpLimitsInput);
            }
        }

        //
        // 1c. Arbitrate output domain group limits of all the power-based
        // _DOMGRP policies.
        //
        FOR_EACH_INDEX_IN_MASK(32, i, pPolicies->domGrpPolicyPwrMask)
        {
            PWR_POLICY_DOMGRP *pPolicy;

            pPolicy = (PWR_POLICY_DOMGRP *)PWR_POLICY_GET(i);
            if (pPolicy == NULL)
            {
                PMU_BREAKPOINT();
                return;
            }
            if (!bCapped &&
                (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_3X_SLEEP_RELAXED)))
            {
                bCapped = pwrPolicyDomGrpIsCapped(pPolicy);
            }
            pwrPolicyDomGrpLimitsArbitrate(
                &domGrpLimitsOutput[RM_PMU_PERF_DOMAIN_GROUP_LIMITS_ID_PWR],
                pwrPolicyDomGrpLimitsGet(pPolicy));
        }
        FOR_EACH_INDEX_IN_MASK_END;

        //
        // 1d. Arbitrate output domain group limits of all the thermal-based
        // _DOMGRP policies.
        //
        FOR_EACH_INDEX_IN_MASK(32, i, pPolicies->domGrpPolicyThermMask)
        {
            PWR_POLICY_DOMGRP *pPolicy;

            pPolicy = (PWR_POLICY_DOMGRP *)PWR_POLICY_GET(i);
            if (pPolicy == NULL)
            {
                PMU_BREAKPOINT();
                return;
            }
            if (!bCapped &&
               (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_3X_SLEEP_RELAXED)))
            {
                bCapped = pwrPolicyDomGrpIsCapped(pPolicy);
            }
            pwrPolicyDomGrpLimitsArbitrate(
                &domGrpLimitsOutput[RM_PMU_PERF_DOMAIN_GROUP_LIMITS_ID_THERM],
                pwrPolicyDomGrpLimitsGet(pPolicy));
        }
        FOR_EACH_INDEX_IN_MASK_END;

        // 1e. Apply the new policy decisions to the PERF engine/object.
        pwrPolicyDomGrpLimitsSetAll(domGrpLimitsOutput, LW_FALSE);

        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_3X_SLEEP_RELAXED))
        {
            LwU32 samplePeriodus =
            PWR_POLICIES_3X_GET()->baseSamplePeriodms * 1000;
            if (bCapped)
            {
                if (pPolicies->unCapCount >=
                    LW2080_CTRL_PMGR_PWR_POLICY_3X_MAX_UNCAPPED_CYCLES)
                {
                    osTmrCallbackUpdate(&OsCBPwrPolicy, samplePeriodus,
                        (samplePeriodus * (PWR_POLICIES_3X_GET()->lowSamplingMult)),
                        OS_TIMER_RELAXED_MODE_USE_NORMAL,
                        OS_TIMER_UPDATE_USE_BASE_STORED);
                }
                pPolicies->unCapCount = 0;
            }
            else
            {
                if ((pPolicies->unCapCount <
                    LW2080_CTRL_PMGR_PWR_POLICY_3X_MAX_UNCAPPED_CYCLES) &&
                    (++(pPolicies->unCapCount) >=
                    LW2080_CTRL_PMGR_PWR_POLICY_3X_MAX_UNCAPPED_CYCLES))
                {
                    osTmrCallbackUpdate(&OsCBPwrPolicy, samplePeriodus,
                        (samplePeriodus * (PWR_POLICIES_3X_GET()->lowSamplingMult)),
                        OS_TIMER_RELAXED_MODE_USE_SLEEP,
                        OS_TIMER_UPDATE_USE_BASE_STORED);
                }
            }
        }
    }
}

/*!
 * @Copydoc BoardObjGrpIfaceModel10GetStatusHeader
 */
FLCN_STATUS
pmgrPwrPolicyIfaceModel10GetStatusHeader
(
    BOARDOBJGRP_IFACE_MODEL_10          *pModel10,
    RM_PMU_BOARDOBJGRP   *pBuf,
    BOARDOBJGRPMASK      *pMask
)
{
    FLCN_STATUS status = FLCN_OK;
    RM_PMU_PMGR_PWR_POLICY_BOARDOBJGRP_GET_STATUS_HEADER *pHdr =
        (RM_PMU_PMGR_PWR_POLICY_BOARDOBJGRP_GET_STATUS_HEADER *)pBuf;

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_DOMGRP))
    {
        PERF_DOMAIN_GROUP_LIMITS domGrpLimits;
        PWR_POLICIES_INFLECTION_POINTS_DISABLE *pDisable;

        //
        // Copy out the overall domain group limits for the PMU.
        // TODO-JBH: Add support to RMCTRL/LWAPI to retrive _LIMITS_ID_THERM values.
        //
        pwrPolicyDomGrpLimitsGetById(&domGrpLimits, RM_PMU_PERF_DOMAIN_GROUP_LIMITS_ID_PWR);

        perfDomGrpLimitsCopyOut(&(pHdr->domGrpLimits), &domGrpLimits);

        PMU_ASSERT_OK_OR_GOTO(status,
            pwrPoliciesInflectionPointsDisableGet(
                PWR_POLICIES_GET(), &pDisable),
            pmgrPwrPolicyIfaceModel10GetStatusHeader_exit);

        if (pDisable != NULL)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                pwrPoliciesInflectionPointsDisableExport(
                    pDisable, &pHdr->inflectionPointsDisable),
                pmgrPwrPolicyIfaceModel10GetStatusHeader_exit);
        }
    }

pmgrPwrPolicyIfaceModel10GetStatusHeader_exit:
    return status;
}

/*!
 * Checks whether tamper capping needs to be applied or not
 *
 * Capping penalty has to be applied iff
 *    1) penalyStart is a non-zero value
 *    2) if 1) is true, elapsed time < PWR_POLICY_TAMPER_PENALTY_PERIOD_NS
 *
 * This function also resets penaltyStart if
 * elapsed time >= PWR_POLICY_TAMPER_PENALTY_PERIOD_NS
 *
 * @param[in]     pPolicies   PWR_POLICIES pointer
 *
 * @return  LW_TRUE  if tamper capping needs to be applied
 * @return  LW_FALSE if tamper capping does not need to be applied
 */
static LwBool
s_pwrPoliciesIsTamperCappingNeeded
(
     PWR_POLICIES *pPolicies
)
{
    FLCN_TIMESTAMP current;
    FLCN_TIMESTAMP elapsed;

    // if tamperPenaltyStart == 0, we are OK. No capping required
    if ((pPolicies->tamperPenaltyStart.parts.lo == 0) &&
        (pPolicies->tamperPenaltyStart.parts.hi == 0))
    {
        return LW_FALSE;
    }

    // Get elapsed time
    osPTimerTimeNsLwrrentGet(&current);
    lw64Sub(&elapsed.data, &current.data, &pPolicies->tamperPenaltyStart.data);

    current.parts.lo = PWR_POLICIES_TAMPER_PENALTY_PERIOD_NS;
    current.parts.hi = 0;

    //
    // if elapsed time >= PWR_POLICY_TAMPER_PENALTY_PERIOD_NS
    // result = 1 or 0. Reset penalty time to 0 in this case
    //
    if (lwU64CmpGE(&elapsed.data, &current.data))
    {
        pPolicies->tamperPenaltyStart.parts.hi = 0;
        pPolicies->tamperPenaltyStart.parts.lo = 0;
        pPolicies->bChannelTampered = LW_FALSE;

        return LW_FALSE;
    }

    return LW_TRUE;
}

/*!
 * Helper function to perform evaluation of policies in @ref domGrpPolicyMask
 *
 * TODO-Tariq: Would prefer to put this function in pwrpolicy_domgrp.[hc] file
 * but there is a cirlwlar dependency between header files that needs to be
 * resolved to do this.
 */
static void
s_pwrPoliciesDomGrpEvaluate
(
    PWR_POLICIES *pPolicies,
    LwU32         expiredPolicyMask
)
{
    LwU32               mask;
    LwBoardObjIdx       i;
    FLCN_STATUS         status;
    BOARDOBJGRPMASK_E32 perfCfPwrModelPolicyMask;
    OSTASK_OVL_DESC     ovlDescList[] = {
        PWR_POLICY_DOMGRP_EVALUATE_OVERLAYS
    };

    mask = expiredPolicyMask & pPolicies->domGrpPolicyMask;
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL))
    {
        boardObjGrpMaskInit_E32(&perfCfPwrModelPolicyMask);
        status = boardObjGrpMaskCopy(&perfCfPwrModelPolicyMask,
            pwrPolicies35GetPerfCfPwrModelPolicyMask(pPolicies));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            return;
        }

        mask &= ~(perfCfPwrModelPolicyMask.super.pData[0]);
    }

    pwrPoliciesDomGrpGlobalCeilingCache();

    if (mask != 0)
    {
        // Attach overlays for policies not implementing PERF_CF_PWR_MODEL
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            FOR_EACH_INDEX_IN_MASK(32, i, mask)
            {
                PWR_POLICY_DOMGRP *pPolicy;

                pPolicy = (PWR_POLICY_DOMGRP *)PWR_POLICY_GET(i);
                if (pPolicy == NULL)
                {
                    PMU_BREAKPOINT();
                    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
                    return;
                }

                (void)pwrPolicyDomGrpEvaluate(pPolicy);
            }
            FOR_EACH_INDEX_IN_MASK_END;
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL))
    {
        BOARDOBJGRPMASK_E32 tmpPolicyMask;
        OSTASK_OVL_DESC ovlDescListPerfCfPwrModel[] = {
            PWR_POLICY_DOMGRP_PERF_CF_PWR_MODEL_EVALUATE_OVERLAYS
        };

        // Initialize and update tmpPolicyMask
        boardObjGrpMaskInit_E32(&tmpPolicyMask);
        tmpPolicyMask.super.pData[0] =
            expiredPolicyMask & pPolicies->domGrpPolicyMask;

        status = boardObjGrpMaskAnd(&tmpPolicyMask, &tmpPolicyMask,
            &perfCfPwrModelPolicyMask);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            return;
        }

        if (!boardObjGrpMaskIsZero(&tmpPolicyMask))
        {
            PWR_POLICY *pPolicy;

            // Attach overlays for policies implementing PERF_CF_PWR_MODEL
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListPerfCfPwrModel);
            {
                BOARDOBJGRP_ITERATOR_BEGIN(PWR_POLICY, pPolicy, i,
                    &tmpPolicyMask.super)
                {
                    PMU_HALT_OK_OR_GOTO(status,
                        pwrPolicyDomGrpEvaluate((PWR_POLICY_DOMGRP *)pPolicy),
                        s_pwrPoliciesDomGrpEvaluate_perf_cf_pwr_model_detach);
                }
                BOARDOBJGRP_ITERATOR_END;
s_pwrPoliciesDomGrpEvaluate_perf_cf_pwr_model_detach:
                lwosNOP();
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListPerfCfPwrModel);
        }
    }
}
