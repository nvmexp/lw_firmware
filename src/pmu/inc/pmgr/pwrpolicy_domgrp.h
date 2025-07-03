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
 * @file  pwrpolicy_domgrp.h
 * @brief @copydoc pwrpolicy_domgrp.c
 */

#ifndef PWRPOLICY_DOMGRP_H
#define PWRPOLICY_DOMGRP_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrpolicy.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_POLICY_DOMGRP PWR_POLICY_DOMGRP;

/*!
 * Structure representing a domgrp-based power policy.
 */
struct PWR_POLICY_DOMGRP
{
    /*!
     * @copydoc PWR_POLICY
     */
    PWR_POLICY super;

    /*!
     * The current Domain Group values this PWR_POLICY object wants to enforce
     * to control the given power channel to the current limit value.
     *
     * These values are assumed to be maximum limits.
     */
    PERF_DOMAIN_GROUP_LIMITS domGrpLimits;
    /*!
     * The current Domain Group ceiling for this PWR_POLICY_DOMGRP object.  Will
     * be used by evaulation of the PWR_POLICY_DOMGRP class to set the highest
     * limits the object may request.  This can enforce a lower ceiling than the
     * maximum (pstate, VF) point on the GPU under various operating conditions.
     */
    PERF_DOMAIN_GROUP_LIMITS domGrpCeiling;

#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_INFLECTION_POINTS_DISABLE)
    /*!
     * State of the inflection limits on the last iteration, before checking for
     * disablement, and timestamp of the last change in limits.
     *
     * This is used to ilwalidate disablement requests when an inflection limit
     * changes, by ignoring requests before the timestamp.
     */
    LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST lastInflectionState;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_INFLECTION_POINTS_DISABLE)

    /*!
     * Cap the system below the "Inflection vpstate index" when the current
     * limit is smaller than this "Inflection limit". This inflection limit can
     * help improve some pstate thrashing issue when the power limit is reduced
     * into the "battery" or certain lower pstate range.
     */
    LwU32 limitInflection;
    /*!
     * A boolean flag to indicate that the output Domain Group limits computed
     * by this Power Policy (via @ref pwrPolicyDomGrpEvaluate) should be floored
     * to the Rated TDP VPstate (commonly referred to as "Base Clock" in the GPU
     * Boost/SmartPower/PWR 2.0 documentation).
     *
     * This is used to implement a Room Temperuatre Power (RTP)
     * Policy/Controller which can only control down to a certain pstate/clock
     * value - i.e. won't sacrifice perf in order to control power.  The
     * expectation is that the RTP controller will be controlling to a lower
     * limit than a Total GPU Power (TGP) controller which controls to a higher
     * limit without this limitation.  The RTP controller should suffice to
     * control power in the Room Temperature Case down to the "Base Clock",
     * ensuring better acoustic performance.  The TGP controller will control
     * power in the worst case/oven temperature case where worse acoustic
     * performance is allowed.
     */
    LwBool bRatedTdpVpstateFloor;
    /*!
     * A boolean flag to indicate that the ouput Domain Group limits computed
     * by this Power Policy (via @ref pwrPolicyDomGrpEvaluate) should be capped
     * to the Full Deflection VPstate. This is used to reduce power immediately
     * when the situation arises (GPIO asserted + power limit change) and allow
     * the controller to step up from below to the power target.
     */
    LwBool bFullDeflectiolwpstateCeiling;
    /*!
     * A boolean flag to denote whether this DOMGRP POLICY is actively capping.
     */
    LwBool bIsCapped;
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PIECEWISE_FREQUENCY_FLOOR))
    /*!
     * Describes the source of the piecewise frequency floor lwrve that the
     * GPC Domain Group will use as a floor.
     */
    LW2080_CTRL_PMGR_PWR_POLICY_INFO_DATA_DOMGRP_PFF_SOURCE     pffSource;
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_DOMAIN_GROUP_LIMITS_MASK))
    /*!
     * Supported domain group limits PERF_DOMAIN_GROUP_XYZ
     */
    BOARDOBJGRPMASK_E32                                         domGrpLimitMask;
#endif
};

/*!
 * Structure representing a PWR_POLICY object's request for a global ceiling on
 * the Domain Group Limits requested by all PWR_POLICY_DOMGRP objects.
 */
typedef struct
{
    /*!
     * Index of the PWR_POLICY requesting the specified ceiling (@ref
     * domGrpCeiling) on all DOMGRP limit requests.
     */
    LwU8 policyIdx;
    /*!
     * The current Domain Group Limits ceiling the given PWR_POLICY object
     * (specified via @ref policyIdx) wants to enforce to control its given
     * power channel to its current limit value.
     *
     * The requests of all PWR_POLICY_DOMGRP objects are ensured to be <= the values specified here.
     */
    PERF_DOMAIN_GROUP_LIMITS domGrpCeiling;
} PWR_POLICY_DOMGRP_GLOBAL_CEILING_REQUEST;

/*!
 * Structure tracking the Global Domain Group Ceiling feature.
 */
typedef struct
{
    /*!
     * Number of request structures available for clients - i.e. the size of the
     * @ref pRequests array.  This array will be right-sized to the number of
     * PWR_POLICY_LIMIT objects which will need this interface.
     */
    LwU8 numRequests;
    /*!
     * Array of global Domain Group Ceiling requests from each PWR_POLICY
     * client.  These entries will be arbitrated for the lowest requested
     * ceilings.
     */
    PWR_POLICY_DOMGRP_GLOBAL_CEILING_REQUEST *pRequests;
    /*!
     * Evaluated global ceiling after arbitrating through all policies
     */
    PERF_DOMAIN_GROUP_LIMITS globalCeiling;
} PWR_POLICY_DOMGRP_GLOBAL_CEILING;

/*!
 * @interface PWR_POLICY_DOMGRP
 *
 * Evaluates the PWR_POLICY_DOMGRP implementation, taking the latest values from
 * the monitored power channel and then specifying any required corrective
 * actions as Domain Group PERF_LIMIT values in @ref
 * PWR_POLICY_DOMGRP::domGrpLimits.
 *
 * @param[in]  pDomGrp PWR_POLICY_DOMAIN_GROUP pointer.
 *
 * @return FLCN_OK
 *     PWR_POLICY structure succesfully evaluated.
 *
 * @return Other errors
 *     Unexpected errors propagated up from type-specific implemenations.
 */
#define PwrPolicyDomGrpEvaluate(fname) FLCN_STATUS (fname) (PWR_POLICY_DOMGRP *pDomGrp)

/*!
 * @interface PWR_POLICY_DOMGRP
 *
 * This is a static interface for the PWR_POLICY_DOMGRP class.
 *
 * Constructs/initializes all state associated with the global Domain Group
 * ceiling feature.
 *
 * @return FLCN_OK
 *      Global Domain Group ceiling feature state successfully constructed and initialized.
 */
#define PwrPolicyDomGrpGlobalCeilingConstruct(fname) FLCN_STATUS (fname)(void)

/*!
 * @interface PWR_POLICY_DOMGRP
 *
 * This is a static interface for the PWR_POLICY_DOMGRP class.
 *
 * This interface allows a PWR_POLICY object to specify/request a global Domain
 * Group ceiling to be applied to all PWR_POLICY_DOMGRP objects.
 *
 * @param[in] policyIdx
 *     Index of the PWR_POLICY object requesting the given global Domain Group ceiling.
 * @param[in] pDomGrpCeiling
 *     The requested global Domain Group ceiling values.
 *
 * @return FLCN_OK
 *     Global Domain Group ceiling request successfully applied.
 * @return FLCN_ERR_ILWALID_STATE
 *     No client request structure available in the @ref
 *     PWR_POLICY_DOMGRP_GLOBAL_CEILING::pRequests array.  This is an unexpected
 *     error, as this array should have been right size when constructing.
 */
#define PwrPolicyDomGrpGlobalCeilingRequest(fname) FLCN_STATUS (fname)(LwU8 policyIdx, PERF_DOMAIN_GROUP_LIMITS *pDomGrpCeiling)

/*!
 * Constructs @ref domGrpLimitMask per policy
 *
 * @param[in/out]     pPolicy   PWR_POLICY_DOMGRP pointer.
 *
 * @return FLCN_OK
 *     Successfully created the mask.
 * @return Other errors
 *     Propagates return values from various calls.
 */
#define PwrPolicyDomGrpLimitsMaskSet(fname) FLCN_STATUS (fname)(PWR_POLICY_DOMGRP *pPolicy)

/*!
 *  @interface PWR_POLICY_DOMGRP
 * 
 * Checks if the policy is actively capping
 *
 * @param[in]  pDomGrp PWR_POLICY_DOMAIN_GROUP pointer.
 *
 * @return LW_TRUE   if pDomGrp is actively capping
 * @return LW_FALSE  if pDomGrp is not actively capping
 */
#define PwrPolicyDomGrpIsCapped(fname) LwBool (fname)(PWR_POLICY_DOMGRP *pPolicy)
/* ------------------------- Defines --------------------------------------- */
/*!
 * Helper macro to return a pointer to a policy's current requested domain group
 * limits.
 *
 * @param[in] pDomGrp PWR_POLICY_DOMGRP pointer
 *
 * @return Pointer to PWR_POLICY_DOMGRP::domGrpLimits
 */
#define pwrPolicyDomGrpLimitsGet(pDomGrp)                                      \
    (&(pDomGrp)->domGrpLimits)

/*!
 * Helper macro to get a pointer to the piecewise frequency floor data from
 * the given PWR_POLICY_DOMGRP power policy.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PIECEWISE_FREQUENCY_FLOOR))
#define pwrPolicyDomGrpPffSourceGet(pDomGrp)   (&(pDomGrp)->pffSource)
#else
#define pwrPolicyDomGrpPffSourceGet(pDomGrp)                                   \
    ((LW2080_CTRL_PMGR_PWR_POLICY_INFO_DATA_DOMGRP_PFF_SOURCE *)(NULL))
#endif

/*!
 * Helper macro to set the piecewise frequency floor data used by the the given
 * PWR_POLICY_DOMGRP power policy.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PIECEWISE_FREQUENCY_FLOOR))
#define pwrPolicyDomGrpPffSourceSet(pDomGrp, pffSourceSet)                     \
    ((pDomGrp)->pffSource = pffSourceSet)
#else
#define pwrPolicyDomGrpPffSourceSet(pDomGrp, pffSourceSet)  do {} while (LW_FALSE)
#endif

/*!
 *  Helper macro to get the requested bit number from @ref domGrpLimitPolicyMask
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_DOMAIN_GROUP_LIMITS_MASK)
#define PWR_POLICIES_DOMGRP_LIMIT_MASK_BIT_GET(_idx)                                      \
    ((pwrPolicies35GetDomGrpLimitsMask(PWR_POLICIES_35_GET()) != NULL) &&                 \
    (boardObjGrpMaskBitGet(pwrPolicies35GetDomGrpLimitsMask(PWR_POLICIES_35_GET()), _idx)))
#else
#define PWR_POLICIES_DOMGRP_LIMIT_MASK_BIT_GET(_idx)  LW_FALSE
#endif

/*!
 * Helper macro to  determine if this PWR_POLICY_DOMGRP has a valid source for
 * a piecewise frequency floor lwrve.
 */
 #define pwrPolicyDomGrpHasPffSource(pDomGrp)                                  \
    ((pwrPolicyDomGrpPffSourceGet(pDomGrp) != NULL)         &&                 \
     (pwrPolicyDomGrpPffSourceGet(pDomGrp)->policyType !=                      \
        LW2080_CTRL_PMGR_PWR_POLICY_DOMGRP_PFF_SOURCE_NONE)     &&             \
     (pwrPolicyDomGrpPffSourceGet(pDomGrp)->policyIdx !=                       \
        LW2080_CTRL_PMGR_PWR_POLICY_INDEX_ILWALID))

/*!
 * Helper macro for set of overlays required for PWR_POLICY_DOMGRP class
 * evaluation (i.e. calling @ref pwrPolicyDomGrpEvaluate() from @ref
 * pwrPoliciesEvaluate_SUPER()).
 */
#define PWR_POLICY_DOMGRP_EVALUATE_OVERLAYS                                    \
    PWR_POLICY_WORKLOAD_MULTIRAIL_EVALUATE_OVERLAYS                            \
    OSTASK_OVL_DESC_DEFINE_PWR_POLICY_DOMGRP_EVALUATE_PIECEWISE_FREQUENCY_FLOOR

/*!
 * Helper macro for set of overlays required for PWR_POLICY_DOMGRP class
 * evaluation (i.e. calling @ref pwrPolicyDomGrpEvaluate() from @ref
 * pwrPoliciesEvaluate_SUPER()) for policies which implement PERF_CF_PWR_MODEL
 * interface.
 */
#define PWR_POLICY_DOMGRP_PERF_CF_PWR_MODEL_EVALUATE_OVERLAYS                  \
    PWR_POLICY_PERF_CF_PWR_MODEL_SCALE_OVERLAYS                                \
    PWR_POLICY_WORKLOAD_COMBINED_1X_EVALUATE_OVERLAYS                          \
    OSTASK_OVL_DESC_DEFINE_PWR_POLICY_DOMGRP_EVALUATE_PIECEWISE_FREQUENCY_FLOOR

#define pwrPolicyDomGrpGlobalCeilingGet()                                      \
    &(Pmgr.pPwrPolicies->domGrpGlobalCeiling.globalCeiling)
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_POLICY interfaces
 */
BoardObjGrpIfaceModel10ObjSet       (pwrPolicyGrpIfaceModel10ObjSetImpl_DOMGRP)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicyGrpIfaceModel10ObjSetImpl_DOMGRP");
BoardObjIfaceModel10GetStatus           (pwrPolicyIfaceModel10GetStatus_DOMGRP)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrPolicyIfaceModel10GetStatus_DOMGRP");

/*!
 * PWR_POLICY_DOMGRP interfaces
 */
PwrPolicyDomGrpEvaluate               (pwrPolicyDomGrpEvaluate)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyDomGrpEvaluate");
PwrPolicyDomGrpGlobalCeilingConstruct (pwrPolicyDomGrpGlobalCeilingConstruct)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicyDomGrpGlobalCeilingConstruct");
PwrPolicyDomGrpGlobalCeilingRequest   (pwrPolicyDomGrpGlobalCeilingRequest);
FLCN_STATUS pwrPoliciesPostInit_DOMGRP(void)
    GCC_ATTRIB_SECTION("imem_libPmgrInit", "pwrPoliciesDomGrpPostInit");
FLCN_STATUS pwrPoliciesDomGrpLimitPolicyMaskSet(void)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPoliciesDomGrpLimitPolicyMaskSet");
PwrPolicyDomGrpLimitsMaskSet (pwrPolicyDomGrpLimitsMaskSet)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyDomGrpLimitsMaskSet");
PwrPolicyDomGrpLimitsMaskSet (pwrPolicyDomGrpLimitsMaskSet_SUPER)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyDomGrpLimitsMaskSet_SUPER");
PwrPolicyDomGrpIsCapped (pwrPolicyDomGrpIsCapped)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyDomGrpIsCapped");
PwrPolicyDomGrpIsCapped (pwrPolicyDomGrpIsCapped_SUPER)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyDomGrpIsCapped_SUPER");

void        pwrPolicyDomGrpLimitsInit(PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits)
    GCC_ATTRIB_SECTION("imem_resident", "pwrPolicyDomGrpLimitsInit");
void        pwrPolicyDomGrpLimitsArbitrate(PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimitsOutput, PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimitsInput);
void        pwrPolicyDomGrpLimitsSetAll(PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits, LwBool bSkipVblank);
void        pwrPolicyDomGrpLimitsGetMin(PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimitsMin);
void        pwrPolicyDomGrpLimitsGetById(PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits, RM_PMU_PERF_DOMAIN_GROUP_LIMITS_ID limitId);
FLCN_STATUS pwrPolicyGetDomGrpLimitsByVpstateIdx(LwU8 vPstateIdx, PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits);
void        pwrPolicyGetDomGrpLimitsFloor(PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimitsOutput, PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimitsInput);
void        pwrPolicyGetDomGrpLimitsCeiling(PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimitsOutput, PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimitsInput);
void        pwrPoliciesDomGrpGlobalCeilingCache(void)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPoliciesDomGrpGlobalCeilingCache");

/*!
 * Helper inline function to translate perf domain group clock freq in any unit of Hz (kHz/MHz)
 * from a source domain (1x/2x) to a target domain (1x/2x) in the same unit of Hz.
 *
 * Supported domain groups: 1) Graphics clock
 *
 * @param[in]      freqInput      input freq value.
 * @param[in]      srcClkDomain   source domain ref@.LW2080_CTRL_CLK_DOMAIN_*
 * @param[in]      tgtClkDomain   target domain ref@.LW2080_CTRL_CLK_DOMAIN_*
 *
 * @return the translated freq values in the same unit as of the input
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwU32
s_pwrPolicyDomGrpFreqTranslate
(
    LwU32  freqInput,
    LwU32  srcClkDomain,
    LwU32  tgtClkDomain
)
{
    if (srcClkDomain != tgtClkDomain)
    {
        //
        // 1. clock domain group = graphics
        // If source is 2x and target is in 1x
        // colwert the target freq to 1x.
        //
        if ((srcClkDomain == LW2080_CTRL_CLK_DOMAIN_GPC2CLK) &&
            (tgtClkDomain == LW2080_CTRL_CLK_DOMAIN_GPCCLK))
        {
            freqInput = LW_UNSIGNED_ROUNDED_DIV(freqInput, 2U);
            goto s_pwrPolicyDomGrpFreqTranslate_done;
        }

        //
        // If source is 1x and target is in 2x
        // colwert the target freq to 2x.
        //
        if ((srcClkDomain == LW2080_CTRL_CLK_DOMAIN_GPCCLK) &&
            (tgtClkDomain == LW2080_CTRL_CLK_DOMAIN_GPC2CLK))
        {
            freqInput *= 2U;
            goto s_pwrPolicyDomGrpFreqTranslate_done;
        }

        //
        // Not able to find a correct translation logic for the specified
        // source and target domain for supported domgrp clock. Please check a)
        // the input domains for the domgrp clock are correct or b) add new
        // translation logic for this domgrp clock
        //
        PMU_HALT();
    }
s_pwrPolicyDomGrpFreqTranslate_done:
    return freqInput;
}

/*!
 * @brief   Accessor for PWR_POLICY_DOMGRP::domGrpLimitsMask.
 *
 * @param[in]   pDomGrp    PWR_POLICY_DOMGRP pointer.
 *
 * @return
 *     @ref PWR_POLICY_DOMGRP::domGrpLimitsMask pointer if feature enabled
 *     NULL pointer if feature is disabled
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline BOARDOBJGRPMASK_E32 *
pwrPolicyDomGrpLimitsMaskGet
(
    PWR_POLICY_DOMGRP *pDomGrp
)
{
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_DOMAIN_GROUP_LIMITS_MASK)
    return &(pDomGrp->domGrpLimitMask);
#else
    return NULL;
#endif
}

/*!
 * @brief   Retrieves @ref PWR_POLICY_DOMGRP::lastInflectionState, if available
 *
 * @param[in]   pDomGrp                 Pointer to DOMGRP object.
 * @param[out]  ppLastInflectionState   Set to pointer to the last inflection
 *                                      state, if available, and @ref NULL
 *                                      otherwise.
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  Either pDomGrp or
 *                                          ppLastInflectionState is @ref NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
pwrPolicyDomGrpLastInflectionStateGet
(
    PWR_POLICY_DOMGRP *pDomGrp,
    LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST **ppLastInflectionState
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDomGrp != NULL) && (ppLastInflectionState != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrPolicyDomGrpLastInflectionStateGet_exit);

#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_INFLECTION_POINTS_DISABLE)
    *ppLastInflectionState = &pDomGrp->lastInflectionState;
#else // PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_INFLECTION_POINTS_DISABLE)
    *ppLastInflectionState = NULL;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_INFLECTION_POINTS_DISABLE)

pwrPolicyDomGrpLastInflectionStateGet_exit:
    return status;
}

/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */
#include "pmgr/pwrpolicy_bangbang.h"
#include "pmgr/pwrpolicy_march_vf.h"
#include "pmgr/pwrpolicy_workload.h"
#include "pmgr/pwrpolicy_workload_multirail_iface.h"
#include "pmgr/pwrpolicy_workload_multirail.h"
#include "pmgr/pwrpolicy_workload_combined_1x.h"
#endif // PWRPOLICY_DOMGRP_H
