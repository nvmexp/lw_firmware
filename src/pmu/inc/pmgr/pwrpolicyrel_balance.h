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
 * @file  pwrpolicyrel_balance.h
 * @brief @copydoc pwrpolicyrel_balance.c
 */

#ifndef PWRPOLICYREL_BALANCE_H
#define PWRPOLICYREL_BALANCE_H


/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrpolicyrel.h"

/* ------------------------------ Macros ------------------------------------*/
/*!
 * MACRO to get the index into Power Topology Table @ref PWR_CHANNEL
 * The PWR_CHANNEL object at this index is used to estimate the power for the
 * corresponding phase.
 */
#define PWR_POLICY_REL_BALANCE_GET_PHASE_EST_CHIDX(relIdx)                   \
    ((PWR_POLICY_RELATIONSHIP_BALANCE *)                                     \
        PWR_POLICY_REL_GET(relIdx))->phaseEstimateChIdx

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_POLICY_RELATIONSHIP_BALANCE PWR_POLICY_RELATIONSHIP_BALANCE;

struct PWR_POLICY_RELATIONSHIP_BALANCE
{
    /*!
     * @copydoc PWR_POLICY_RELATIONSHIP
     */
    PWR_POLICY_RELATIONSHIP super;

    /*!
     * PWR_POLICY pointer for secondary policy in this balancing relationship.
     */
    PWR_POLICY             *pSecPolicy;

    /*!
     * PWM Source - @ref RM_PMU_PMGR_PWM_SOURCE_<xyz>
     */
    RM_PMU_PMGR_PWM_SOURCE  pwmSource;

    /*!
     * IRB PWM source descriptor.
     */
    PWM_SOURCE_DESCRIPTOR  *pPwmSrcDesc;

    /*!
     * RAW PWM period in units of the PWM generator's clock.
     */
    LwU32                   pwmPeriod;

    /*!
     * PWM duty cycle at start. Unitless quantity in %.
     * Represented as unsigned FXP 16_16
     */
    LwUFXP16_16             pwmDutyCycleInitial;

    /*!
     * PWM duty cycle setp size. Unitless quantity in %.
     * Represented as unsigned FXP 16_16
     */
    LwUFXP16_16             pwmDutyCycleStepSize;

    /*!
     * Current PWM percent driven out on the balancing circuit's GPIO.
     */
    LwUFXP16_16             pwmPctLwrr;

    /*!
     * Array of structures describing the dynamic state of both
     * PWR_POLICY_PROP_LIMIT objects.
     */
    LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_STATUS_BALANCE_PROP_LIMIT
        propLimits[LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_STATUS_BALANCE_MAX_PROP_LIMITS];


    /*!
     * Latest value retreived from the monitored PWR_CHANNEL ref@ phaseEstimateChIdx.
     */
    LwU32                   phaseEstimateValueLwrr;

    /*!
     * Index into Power Topology Table @ref PWR_CHANNEL
     * The PWR_CHANNEL object at this index is used to estimate the power for the
     * corresponding phase. This estimate will be used by the
     * @ref PWR_POLICY_BALANCE object referenced by @ref PWR_POLICY_RELATIONSHIP::policyIdx.
     *
     * Value of @ref LW2080_CTRL_PMGR_PWR_CHANNEL_INDEX_ILWALID means the estimate
     * is not available and hence this value should not be used.
     */
    LwU8                    phaseEstimateChIdx;

    /*!
     * The phase count multiplier coefficient, when needed to estimate
     * the total while observing from one phase.
     */
    LwU8                    phaseCountMultiplier;

    /*!
     * @ref LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_STATUS_BALANCE_ACTION_<xyz>
     */
    LwS8                    action;

    /*!
     * Boolean indicating that the BALANCE relationship's PWM value should be
     * locked to a simulated value, overriding the behavior of the BALANCE
     * relationship controller.
     *
     * When this value is LW_TRUE, @ref pwmDutyCycleInitial will contain the
     * simulated value which the controller will apply via @ref
     * pwrPolicyRelationshipLoad_BALANCE().
     */
    LwBool                  bPwmSim;
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_BALANCE_VIOLATION_AWARE))
    /*!
     * Index of RM_PMU_THERM_EVENT_<xyz> providing violation information.
     */
    LwU8                    thrmEventIdx;
    /*!
     * Violation structure that will contain the information about the
     * violation observed on the given RM_PMU_THERM_EVENT_<xyz>
     */
    LIB_PMGR_VIOLATION      violation;
#endif
};

/*!
 * @interface PWR_POLICY_RELATIONSHIP_BALANCE
 *
 * Retrieves the lower limit requested from the set of PWR_POLICYs to which the
 * given PWR_POLICY_RELATIONSHIP_BALANCE points and the difference between the
 * lower and higher limit.
 *
 * Intended to be used by the PWR_POLICY_BALANCE class to sort its set of
 * PWR_POLICY_RELATIONSHIP_BALANCE objects for the order in which it should
 * balance them.
 *
 * @param[in]  pBalance     PWR_POLICY_RELATIONSHIP_BALANCE pointer
 * @param[out] pLimitLower  Pointer in which to return the lower PWR_POLICY limit.
 * @param[out] pLimitDiff
 *     Pointer in which to return the difference in the PWR_POLICY limits.
 */
#define PwrPolicyRelationshipBalanceLimitsDiffGet(fname) void (fname)(PWR_POLICY_RELATIONSHIP_BALANCE *pBalance, LwU32 *pLimitLower, LwU32 *pLimitDiff)

/*!
 * @interface PWR_POLICY_RELATIONSHIP_BALANCE
 *
 * Evaluates the PWR_POLICY_RELATIONSHIP_BALANCE algorithm, shifting power from
 * one input PWR_CHANNEL to another based on which PWR_CHANNEL's controlling
 * PWR_POLICY_PROP_LIMIT is requesting a lower clock limit.
 *
 * @param[in]  pBalance     PWR_POLICY_RELATIONSHIP_BALANCE pointer
 *
 * @return FLCN_OK
 *     Balancing algorithm successfully evaluated and new PWM settings applied (if applicable).
 * @return Other unexpected errors
 *     An unexpected error oclwred during evaluation.  Most likely in applying the PWM settings.
 */
#define PwrPolicyRelationshipBalanceEvaluate(fname) FLCN_STATUS (fname)(PWR_POLICY_RELATIONSHIP_BALANCE *pBalance)

/*!
 * @interface PWR_POLICY_RELATIONSHIP_BALANCE
 *
 * Retrieves the PWM percent the PWR_POLICY_RELATIONSHIP_BALANCE object has
 * assigned for the balanced phase to the requested PWR_CHANNEL index.
 *
 * @param[in]  pBalance     PWR_POLICY_RELATIONSHIP_BALANCE pointer
 * @param[in]  chIdx        Index of PWR_CHANNEL object for which PWM Pct is requested.
 * @param[in]  chIdxEval
 *     Channel index of the PWR_CHANNEL which originally evaluated this
 *     function.  This is used to avoids cirlwlar references.
  * @param[in]  bScaleWithPhaseCount
 *     Boolean indicating if we need to scale with phase count multiplier
 *
 * @return @ref PWR_POLICY_RELATIONSHIP_BALANCE::pwmPctLwrr or 1 - @ref
 *     PWR_POLICY_RELATIONSHIP_BALANCE::pwmPctLwrr depending on whether the
 *     requested chIdx is for the primary or secondary policy.
 */
#define PwrPolicyRelationshipBalancePwmPctGetIdx(fname) LwUFXP16_16 (fname)(PWR_POLICY_RELATIONSHIP_BALANCE *pBalance, LwU8 chIdx, LwU8 chIdxEval, LwBool bScaleWithPhaseCount)

/*!
 * @interface PWR_POLICY_RELATIONSHIP_BALANCE
 *
 * Retrieves the PWM percent from the PWR_POLICY_RELATIONSHIP_BALANCE object.
 *
 * @param[in]  pBalance     PWR_POLICY_RELATIONSHIP_BALANCE pointer
 * @param[in]  bPrimary
 *     Boolean indicating whether to return primary/normal or secondary/ilwerted
 *     value.
 * @param[in]  bScaleWithPhaseCount
 *     Boolean indicating if we need to scale with phase count multiplier
*
 * @return @ref PWR_POLICY_RELATIONSHIP_BALANCE::pwmPctLwrr or 1 - @ref
 *     PWR_POLICY_RELATIONSHIP_BALANCE::pwmPctLwrr depending on whether the
 *     requested chIdx is for the primary or secondary policy.
 */
#define PwrPolicyRelationshipBalancePwmPctGet(fname) LwUFXP16_16 (fname)(PWR_POLICY_RELATIONSHIP_BALANCE *pBalance, LwBool bPrimary, LwBool bScaleWithPhaseCount)

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_POLICY_RELATIONSHIP interfaces
 */
BoardObjGrpIfaceModel10ObjSet                  (pwrPolicyRelationshipGrpIfaceModel10ObjSetImpl_BALANCE)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicyRelationshipGrpIfaceModel10ObjSetImpl_BALANCE");
PwrPolicyRelationshipLoad          (pwrPolicyRelationshipLoad_BALANCE)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrPolicyRelationshipLoad_BALANCE");
BoardObjIfaceModel10GetStatus                      (pwrPolicyRelationshipIfaceModel10GetStatus_BALANCE)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrPolicyRelationshipIfaceModel10GetStatus_BALANCE");

/*!
 * PWR_POLICY_RELATIONSHIP_BALANCE interfaces
 */
PwrPolicyRelationshipBalanceLimitsDiffGet (pwrPolicyRelationshipBalanceLimitsDiffGet)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyRelationshipBalanceLimitsDiffGet");
PwrPolicyRelationshipBalanceEvaluate      (pwrPolicyRelationshipBalanceEvaluate)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyRelationshipBalanceEvaluate");
PwrPolicyRelationshipBalancePwmPctGetIdx  (pwrPolicyRelationshipBalancePwmPctGetIdx)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrPolicyRelationshipBalancePwmPctGetIdx");
PwrPolicyRelationshipBalancePwmPctGet     (pwrPolicyRelationshipBalancePwmPctGet)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrPolicyRelationshipBalancePwmPctGet");
PwrPolicyRelationshipChannelPwrGet        (pwrPolicyRelationshipChannelPwrGet_BALANCE)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyRelationshipChannelPwrGet_BALANCE");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRPOLICYREL_BALANCE_H
