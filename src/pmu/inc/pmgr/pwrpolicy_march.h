/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrpolicy_march.h
 * @brief @copydoc pwrpolicy_march.c
 */

#ifndef PWRPOLICY_MARCH_H
#define PWRPOLICY_MARCH_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_POLICY_MARCH PWR_POLICY_MARCH;

/*!
 * Structure representing a MARCHING controller interface called by another
 * PWR_POLICY object.
 */
struct PWR_POLICY_MARCH
{
    /*!
     * Class type.
     */
    LwU8        type;

    /*!
     * Number of steps by which the implementing class should respond to various
     * actions.
     */
    LwU8        stepSize;
    /*!
     */
    RM_PMU_PMGR_PWR_POLICY_MARCH_HYSTERESIS
                hysteresis;
    /*!
     * Current uncap limit - as callwlated by output of PWR_POLICY::limitLwrr and
     * PWR_POLICY_MARCH::hysteresis.
     */
    LwU32       uncapLimit;

    /*!
     * Last timer value tracking the amount of time any HW_FAILSAFE slowdown (as
     * represented by RM_PMU_THERM_EVENT_META_ANY_HW_FS) has been violating, as
     * returned via @ref thermEventViolationGetTimer32().
     *
     * If this timer value increments over a sampling period, a HW_FAILSAFE
     * slowdown has been engaged, poisoning the sampled power data (i.e. dynamic
     * power was artificially low due to LDIV slowdown) and the _PSTATE capping
     * algorithm should not uncap due to power less than threshold.
     */
    LwU32       thermHwFsTimernsPrev;

    /*!
     * Last action taken by the _MARCH algorithm.
     *
     * @ref LW2080_CTRL_PMGR_PWR_POLICY_MARCH_ACTION_<xyz>
     */
    LwU8        action;
};

/*!
 * @interface PWR_POLICY_MARCH
 *
 * Constructs a PWR_POLICY_MARCH object, an interface class representing a
 * marching controller which will be fully implemented by classes which extend
 * it.
 *
 * @param[out] pMarch
 *      Pointer to PWR_POLICY_MARCH structure to construct/initialize.
 * @param[in]  pMarchDesc
 *      Pointer to RM_PMU_PMGR_PWR_POLICY_MARCH structure describing the state
 *      of the PWR_POLICY_MARCH structure to create.
 * @param[in]  bFirstConstruct
 *      Boolean indicating first time that PWR_POLICY_MARCH object is being
 *      constructed.
 *
 * @return FLCN_ERR_NOT_SUPPORTED
 *     An unsupported PWR_POLICY_MARCH type was specified.
 *
 * @return FLCN_OK
 *     PWR_POLICY_MARCH initialized successfully.
 */
#define PwrPolicyMarchConstruct(fname) FLCN_STATUS (fname)(PWR_POLICY_MARCH *pMarch, RM_PMU_PMGR_PWR_POLICY_MARCH *pMarchDesc, LwBool bFirstConstruct)

/*!
 * @interface PWR_POLICY_MARCH
 *
 * Queries a PWR_POLICY object for it latest dynamic state.
 *
 * @param[in]  pMarch PWR_POLICY_MARCH pointer.
 * @param[out] pPayload
 *     Pointer to payload structure to populate with the latest dynamic state.
 *
 * @return FLCN_OK
 *     Dynamic state successfully returned.
 * @return Other errors
 *     Unexpected errors propogated up from type-specific implementations.
 */
#define PwrPolicyMarchQuery(fname) FLCN_STATUS (fname)(PWR_POLICY_MARCH *pMarch, RM_PMU_PMGR_PWR_POLICY_MARCH_BOARDOBJ_GET_STATUS *pGetStatus)

/*!
 * @interface PWR_POLICY_MARCH
 *
 * Evaluates the marching algorithm for the current iteration's input value
 * (@ref PWR_POLICY::valueLwrr) and limit value (@ref PWR_POLICY::limitLwrr).
 * Determines which action the marcher should take to ensure that value is <=
 * limit.
 *
 * @param[in] pPolicy  PWR_POLICY pointer corresponding to implementing object.
 * @param[in] pMarch   PWR_POLIYC_MARCH pointer.
 *
 * @return @ref LW2080_CTRL_PMGR_PWR_POLICY_MARCH_ACTION_<xyz>
 */
#define PwrPolicyMarchEvaluateAction(fname) LwU8 (fname)(PWR_POLICY *pPolicy, PWR_POLICY_MARCH *pMarch)

/* ------------------------- Defines --------------------------------------- */
/*!
 * Helper macro to return the step size of a given PWR_POLICY_MARCH object.
 *
 * @param[in] pMarch PWR_POLICY_MARCH pointer
 *
 * @return @ref PWR_POLICY_MARCH::stepSize
 */
#define pwrPolicyMarchStepSizeGet(pMarch)                                     \
    ((pMarch)->stepSize)

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_POLICY_MARCH interfaces
 */
PwrPolicyMarchConstruct      (pwrPolicyMarchConstruct)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicyMarchConstruct");
PwrPolicyMarchConstruct      (pwrPolicyMarchConstruct_SUPER)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicyMarchConstruct_SUPER");
PwrPolicyMarchQuery          (pwrPolicyMarchQuery)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrPolicyMarchQuery");
PwrPolicyMarchEvaluateAction (pwrPolicyMarchEvaluateAction)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyMarchEvaluate");

/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#endif // PWRPOLICY_MARCH_H
