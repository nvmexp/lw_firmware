/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrpolicies_35.c
 * @brief  Interface specification for PWR_POLICIES.
 *
 * The PWR_POLICIES is a group of PWR_POLICY and some global data fields.
 * Functions inside this file applies to the whole PWR_POLICIES instead of
 * a single PWR_POLICY. for PWR_POLICY functions, please refer to
 * pwrpolicy_35.c.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmu_objperf.h"
#include "perf/cf/perf_cf_controller.h"
#include "perf/cf/perf_cf.h"
#include "logger.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static FLCN_STATUS s_pwrPolicies35Filter(PWR_POLICIES_35 *pPolicies35, LwU32 ticksNow, BOARDOBJGRPMASK_E32 *pMask)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyCallback", "s_pwrPolicies35Filter");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Constructs the PWR_POLICY object (as applicable).
 *
 * Initializes the DMEM overlay used by the power policies to set/clear
 * PERF_LIMITS.
 *
 * @copydoc PwrPoliciesConstruct
 */
FLCN_STATUS
pwrPoliciesConstruct_35
(
    PWR_POLICIES **ppPwrPolicies
)
{
    FLCN_STATUS status = FLCN_OK;

    // Construct the PWR_POLICIES_35 object
    *ppPwrPolicies = (PWR_POLICIES *)lwosCallocType(OVL_INDEX_DMEM(pmgr),
                        1, PWR_POLICIES_35);
    if (*ppPwrPolicies == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NO_FREE_MEM;
        goto pwrPoliciesConstruct_35_exit;
    }

    // Construct PWR_POLICIES SUPER object state.
    status = pwrPoliciesConstruct_SUPER(ppPwrPolicies);

pwrPoliciesConstruct_35_exit:
    return status;
}

/*!
 * @brief   Construct MULTIPLIER data for all PWR_POLICIES.
 *
 * @param[in]   pPolicies    PWR_POLICIES_35 object pointer.
 *
 * @return  FLCN_OK     MULTIPLIER data constructed successfully
 * @return  other       Propagates return values from various calls
 */
FLCN_STATUS
pwrPolicies35MultDataConstruct
(
    PWR_POLICIES_35 *pPolicies
)
{
    PWR_POLICY_MULTIPLIER_DATA *pMD;
    PWR_POLICY                 *pPolicy;
    LwBoardObjIdx               i;
    FLCN_STATUS                 status = FLCN_OK;

    BOARDOBJGRP_ITERATOR_BEGIN(PWR_POLICY, pPolicy, i, NULL)
    {
        // If Multiplier data were already allocated => just look-up to re-use.
        FOR_EACH_MULTIPLIER(pPolicies, pMD)
        {
            if (pMD->samplingMultiplier == pPolicy->sampleMult)
            {
                break;
            }
        }

        // Otherwise create new Multiplier data structure.
        if (pMD == NULL)
        {
            // Multiplier encountered for the first time => allocate node, ...
            pMD = lwosCallocType(OVL_INDEX_DMEM(pmgrPwrPolicy35MultData), 1,
                                 PWR_POLICY_MULTIPLIER_DATA);
            if (pMD == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_NO_FREE_MEM;
                goto pwrPolicies35MultDataConstruct_exit;
            }

            //
            // ... initialize it, ...
            //

            boardObjGrpMaskInit_E32(&(pMD->maskPolicies));
            boardObjGrpMaskInit_E32(&(pMD->channelsStatus.mask));
            if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_35_FILTER_PAYLOAD_PERF_CF_CONTROLLERS_REDUCED_STATUS))
            {
                PMU_ASSERT_OK_OR_GOTO(status,
                    perfCfControllersReducedStatusInit(
                    pwrPolicy35FilterPayloadPerfCfControllersStatusGet(pMD)),
                    pwrPolicies35MultDataConstruct_exit);
            }

            pMD->samplingMultiplier = pPolicy->sampleMult;

            pMD->samplingPeriodTicks =
                LWRTOS_TIME_US_TO_TICKS(pPolicies->super.baseSamplePeriodms * 1000);
            pMD->samplingPeriodTicks *= pMD->samplingMultiplier;

            // ... and insert it into the linked list.
            pMD->pNext               = pPolicies->pMultDataHead;
            pPolicies->pMultDataHead = pMD;
        }

        status = pwrPolicy35SetMultData(pPolicy, pMD);
        if (status != FLCN_OK)
        {
            goto pwrPolicies35MultDataConstruct_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

pwrPolicies35MultDataConstruct_exit:
    return status;
}

/*!
 * PWR_POLICIES_35 specific implementation
 *
 * @copydoc PwrPoliciesConstructHeader
 */
FLCN_STATUS
pwrPoliciesConstructHeader_35
(
    PWR_POLICIES                                 **ppPolicies,
    PWR_MONITOR                                   *pMon,
    RM_PMU_PMGR_PWR_POLICY_BOARDOBJGRP_SET_HEADER *pHdr
)
{
    PWR_POLICIES *pPolicies = *ppPolicies;
    FLCN_STATUS   status;

    // Call SUPER-class implementation
    status = pwrPoliciesConstructHeader_3X(ppPolicies, pMon, pHdr);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrPoliciesConstructHeader_35_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL))
    {
        BOARDOBJGRPMASK_E32 *pPerfCfPwrModelPolicyMask =
            pwrPolicies35GetPerfCfPwrModelPolicyMask(pPolicies);
        if (pPerfCfPwrModelPolicyMask == NULL)
        {
            PMU_BREAKPOINT();
            goto pwrPoliciesConstructHeader_35_exit;
        }

        // Init and copy in the perfCfPwrModelPolicyMask.
        boardObjGrpMaskInit_E32(pPerfCfPwrModelPolicyMask);
        status = boardObjGrpMaskImport_E32(pPerfCfPwrModelPolicyMask,
                     &(pHdr->perfCfPwrModelPolicyMask));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto pwrPoliciesConstructHeader_35_exit;
        }
    }
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_DOMGRP_DATA_MAP)
        (void)memcpy(((PWR_POLICIES_35 *)pPolicies)->domGrpDataMap,
            &(pHdr->domGrpDataMap),
            (sizeof(RM_PMU_PMGR_PWR_POLICY_DOMGRP_MAP_ENTRY) *
            RM_PMU_PWR_POLICY_DOMGRP_MAP_MAX_CLK_DOMAINS));
#endif

pwrPoliciesConstructHeader_35_exit:
    return status;
}
/*!
 * PWR_POLICIES_35 implementation
 *
 * @copydoc PwrPolicies3xLoad
 */
FLCN_STATUS
pwrPolicies3xLoad_35
(
    PWR_POLICIES_3X *pPolicies3x,
    LwU32 ticksNow
)
{
    PWR_POLICIES_35 *pPolicies35 = (PWR_POLICIES_35 *)pPolicies3x;
    PWR_POLICY_MULTIPLIER_DATA *pMD;
    FLCN_STATUS status = FLCN_OK;
    OSTASK_OVL_DESC ovlDesc35Load[] = {
        PWR_POLICIES_35_OVERLAYS_LOAD
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDesc35Load);
    {
        // Loop over all MULT_DATA and load their SW state.
        FOR_EACH_MULTIPLIER(pPolicies35, pMD)
        {
            //
            // Set the target for the next expiration of this MULT group as now +
            // sampling period (all in RTOS ticks).
            //
            pMD->expirationTimeTicks = ticksNow + pMD->samplingPeriodTicks;

            //
            // Sample the PWR_CHANNEL data for this MULT group.  This will cache the
            // "first" readings so that won't need to burn the first controller
            // evaluation.
            //
            status = pwrChannelsStatusGet(&pMD->channelsStatus);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto pwrPolicies3xLoad_35_exit;
            }
            if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_35_FILTER_PAYLOAD_PERF_CF_CONTROLLERS_REDUCED_STATUS))
            {
                PMU_ASSERT_OK_OR_GOTO(status,
                    perfCfControllersGetReducedStatus_2X(
                    (PERF_CF_CONTROLLERS_2X *)PERF_CF_GET_CONTROLLERS(),
                    pwrPolicy35FilterPayloadPerfCfControllersStatusGet(pMD)),
                    pwrPolicies3xLoad_35_exit);
            }
        }
    }
pwrPolicies3xLoad_35_exit:
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDesc35Load);

    return status;
}

/*!
 * PWR_POLICIES_35 implementation of PwrPolicies3xCallbackBody virtual interface.
 *
 * @copydoc PwrPolicies3xCallbackBody
 */
void
pwrPolicies3xCallbackBody_35
(
    LwU32   ticksExpired
)
{
    PWR_POLICIES_35    *pPolicies35          = PWR_POLICIES_35_GET();
    BOARDOBJGRPMASK_E32 expiredViolationMask;
    BOARDOBJGRPMASK_E32 expiredPolicyMask;
    FLCN_STATUS         status;

    PWR_POLICIES_EVALUATE_PROLOGUE(&pPolicies35->super.super);
    {
        // Handle PWR_VIOLATION objects before PWR_POLICY ones.
        boardObjGrpMaskInit_E32(&expiredViolationMask);
        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_VIOLATION))
        {
            (void)pwrViolationsExpiredMaskGet(ticksExpired, &expiredViolationMask);
        }

        // Filter any expired PWR_POLICYs, as determined via _MULT_DATA.
        boardObjGrpMaskInit_E32(&expiredPolicyMask);
        status = s_pwrPolicies35Filter(pPolicies35, ticksExpired, &expiredPolicyMask);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto pwrPolicies3xCallbackBody_35_exit;
        }

        //
        // If any PWR_POLICY or PWR_VIOLATION objects to evaluate, call into
        // pwrPoliciesEvaluate_3X().
        //
        if (!boardObjGrpMaskIsZero(&expiredViolationMask) ||
            !boardObjGrpMaskIsZero(&expiredPolicyMask))
        {
            pwrPoliciesEvaluate_3X(&pPolicies35->super.super,
                                expiredPolicyMask.super.pData[0],
                                &(expiredViolationMask.super));
        }
    }
pwrPolicies3xCallbackBody_35_exit:
    PWR_POLICIES_EVALUATE_EPILOGUE(&pPolicies35->super.super);
}

/* ------------------------- Static Functions ------------------------------- */
/*!
 * Helper function which will identiy PWR_POLICY objects which have expired (via
 * the PWR_POLICIES_35::pMultData tracking array), evaluate their corresponding
 * PWR_CHANNELs via @ref pwrChannelsStatusGet(), and then filter those
 * PWR_POLICYs from the PWR_CHANNELS_STATUS structure.
 *
 * After filtering is completed, this interface will return the set of
 * PWR_POLICY objects which need to be evaluated into the @ref pMask
 * parameter.
 *
 * @param[in] pPolicies35
 *     PWR_POLICIES_35 pointer
 * @param[in] ticksNow
 *     RTOS tick count at the lwrrrent expiration of callback.  Used to
 *     determine which PWR_POLICY_MULT_DATA groups have expired, and thus which
 *     PWR_CHANNEL_STATUS structures should be queried and which PWR_POLICY
 *     objects should be filtered.
 * @param[out] pMask
 *     Pointer in which the mask of expired PWR_POLICY objects will be returned.
 *     The caller should pass this mask into the appropriate implemenation of
 *     @ref PwrPoliciesEvaluate().
 *
 * @return FLCN_OK
 *     PWR_POLICIES_35 successfully filtered.
 * @return Other errors
 *     An unexpected (and catastrophic) error oclwrred.
 */
static FLCN_STATUS
s_pwrPolicies35Filter
(
    PWR_POLICIES_35     *pPolicies35,
    LwU32                ticksNow,
    BOARDOBJGRPMASK_E32 *pMask
)
{
    PWR_POLICY_MULTIPLIER_DATA *pMD;
    PWR_POLICY       *pPolicy;
    LwBoardObjIdx     p;
    FLCN_STATUS       status = FLCN_OK;
    OSTASK_OVL_DESC   ovlDescMultData[] = {
        PWR_POLICIES_35_OVERLAYS_MULT_DATA
    };
    OSTASK_OVL_DESC   ovlDesc35Filter[] = {
        PWR_POLICIES_35_OVERLAYS_FILTER
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescMultData);
    {
        // Loop over all MULT_DATA and load their SW state.
        FOR_EACH_MULTIPLIER(pPolicies35, pMD)
        {
            // Check if multipler has expired.
            if (OS_TMR_IS_BEFORE(ticksNow, pMD->expirationTimeTicks))
            {
                continue;
            }

            // Compute next expiration time.
            do
            {
                pMD->expirationTimeTicks += pMD->samplingPeriodTicks;
            } while (!OS_TMR_IS_BEFORE(ticksNow, pMD->expirationTimeTicks));

            //
            // Sample the PWR_CHANNEL data for this MULT group.  This will
            // provide the "payload" to be used to filter the PWR_POLICY objects
            // within this MULT_DATA below.
            //
            status = pwrChannelsStatusGet(&pMD->channelsStatus);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto s_pwrPolicies35Filter_detachOverlays;
            }

            if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_35_FILTER_PAYLOAD_PERF_CF_CONTROLLERS_REDUCED_STATUS))
            {
                PMU_ASSERT_OK_OR_GOTO(status,
                    perfCfControllersGetReducedStatus_2X(
                    (PERF_CF_CONTROLLERS_2X *)PERF_CF_GET_CONTROLLERS(),
                    pwrPolicy35FilterPayloadPerfCfControllersStatusGet(pMD)),
                s_pwrPolicies35Filter_detachOverlays);
            }

            // OR the mask of PWR_POLICYs within this MULT_DATA into pMask.
            (void)boardObjGrpMaskOr(pMask, pMask, &pMD->maskPolicies);
        }

    }
s_pwrPolicies35Filter_detachOverlays:
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescMultData);
    if (status != FLCN_OK)
    {
        goto s_pwrPolicies35Filter_exit;
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDesc35Filter);
    {
        //
        // Filter each PWR_POLICY within the MULT_DATA, using the
        // MULT_DATA's PWR_CHANNELS_STATUS as the payload.
        //
        BOARDOBJGRP_ITERATOR_BEGIN(PWR_POLICY, pPolicy, p, &pMask->super)
        {
            pMD = pPolicy->pMultData;
            status = pwrPolicy3XFilter(pPolicy, pPolicies35->super.super.pMon,
                pMD);
            if (status == FLCN_ERR_MORE_PROCESSING_REQUIRED)
            {
                // Re-adjust the mask on which evaluation needs to be skipped.
                boardObjGrpMaskBitClr(pMask, p);
                status = FLCN_OK;
            }
            else if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto s_pwrPolicies35Filter_detach35Filter;
            }
        }
        BOARDOBJGRP_ITERATOR_END;
    }
s_pwrPolicies35Filter_detach35Filter:
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDesc35Filter);

s_pwrPolicies35Filter_exit:
    return status;
}
