/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrpolicies_3x.h
 * @brief Structure specification for the Power Policy functionality container -
 * all as specified by the Power Policy Table 3.X.
 *
 * https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Power_Tables/Power_Policy_Table_3.X
 *
 * All of the actual functionality is found in @ref pwrpolicy.c.
 */

#ifndef PWRPOLICIES_3X_H
#define PWRPOLICIES_3X_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrpolicies.h"

/* ------------------------- Types Definitions ----------------------------- */
/*!
 * PWR_POLICIES_3X structure.  Contains all class-specific state for
 * PWR_POLICY_3X implementations (including _30 and _35).
 */
typedef struct PWR_POLICIES_3X
{
    /*
     * All PWR_POLICIES fields.
     */
    PWR_POLICIES super;
    /*!
     * Base sampling period for all power policies in milli-seconds.
     */
    LwU16        baseSamplePeriodms;
    /*!
     * Minimum client sampling period to request new samples.
     */
    LwU16        minClientSamplePeriodms;
    /*!
     * Lower Sampling Multiplier.
     */
    LwU8         lowSamplingMult;
    /*!
     * Mask for all lwrrently expired policies.
     */
    LwU32        expiredPolicyMask;

    /*!
     * Use 1x or 2x for graphics clock domain (i.e. GPCCLK or GPC2CLK for example)
     */
    LwU32        graphicsClkDom;
} PWR_POLICIES_3X;

/*!
 * @interface PWR_POLICIES_3X
 *
 * @note Requirement logic: The method to get the "ticksExpired" is different in
 * centralised callback and legacy callback feature.
 *
 * @note Type-specific implementations of PWR_POLICIES_3X should implement this
 * interface and @ref pwrPolicies3xCallbackBody() to their type-specific
 * implementation.
 *
 * @param[in] ticksExpired
 *     Number of RTOS ticks which have expired.  Used to determine which
 *     PWR_POLICYs must evaluate, based on their tick targest.
 *     A.K.A. "ticksNow".
 */
#define PwrPolicies3xCallbackBody(fname) void (fname)(LwU32 ticksExpired)

/*!
 * @interface PWR_POLICIES_3X
 *
 * Interface to PWR_POLICIES_3X SW state and controllers.
 */
#define PwrPolicies3xLoad(fname) FLCN_STATUS (fname)(PWR_POLICIES_3X *pPolicies3x, LwU32 ticksNow)

/* ------------------------- Macros ---------------------------------------- */
/*!
 * Helper macro to get pointer to PWR_POLICIES_3X object.
 *
 * CRPTODO - implement dynamic type-checking/casting.
 */
#define PWR_POLICIES_3X_GET()                                                 \
    ((PWR_POLICIES_3X *)Pmgr.pPwrPolicies)

#define pwrPoliciesGetGraphicsClkDomain(pPwrPolicies)                         \
        ((PWR_POLICIES_3X *)pPwrPolicies)->graphicsClkDom

/*!
 * @brief   List of an overlay descriptors required by input filtering
 *          during pwr policy evaluation.
 *
 * @note    Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM after
 *          updates to this macro. Failure to do so might result in falcon halt.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_INPUT_FILTERING))
#define OSTASK_OVL_DESC_DEFINE_PWR_POLICIES_EVALUATE_PWR_POLICY_INPUT_FILTERING    \
    OSTASK_OVL_DESC_DEFINE_LIB_LW64
#else
#define OSTASK_OVL_DESC_DEFINE_PWR_POLICIES_EVALUATE_PWR_POLICY_INPUT_FILTERING    \
    OSTASK_OVL_DESC_ILWALID
#endif

/*!
 * Helper macro for set of overlays required for PWR_POLICIES_3X filtering (@ref
 * pwrPoliciesFilter_3X)).
 */
#define PWR_POLICIES_3X_FILTER_OVERLAYS                                       \
    PWR_POLICY_WORKLOAD_MULTIRAIL_FILTER_OVERLAYS                             \
    PWR_POLICY_WORKLOAD_COMBINED_1X_FILTER_OVERLAYS

/*!
 * Helper macro to get minClientSamplePeriodms from the PWR_POLICIES_3X structure.
 * Returns 0 if PWR_POLICIES are not present.
 */
#define PWR_POLICIES_3X_GET_MIN_CLIENT_SAMPLE_PERIOD_MS()                     \
    ((Pmgr.pPwrPolicies != NULL) ?                                            \
     (((PWR_POLICIES_3X *)(Pmgr.pPwrPolicies))->minClientSamplePeriodms) :    \
     0)

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * Global PWR_POLICIES interfaces
 */
PwrPoliciesConstructHeader (pwrPoliciesConstructHeader_3X)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPoliciesConstructHeader_3X");
PwrPoliciesEvaluate        (pwrPoliciesEvaluate_3X)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyCallback", "pwrPoliciesEvaluate_3X");
PwrPolicies3xLoad          (pwrPolicies3xLoad)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrPolicies3xLoad");

void pwrPolicies3XScheduleEvaluationCallback(LwU32 ticksNow, LwBool bLowSampling);
void pwrPolicies3XCancelEvaluationCallback(void);
PwrPoliciesProcessPerfStatus    (pwrPoliciesProcessPerfStatus_3X);

/* ------------------------- Child Class Includes --------------------------- */
#include "pmgr/3x/pwrpolicies_30.h"
#include "pmgr/3x/pwrpolicies_35.h"

#endif // PWRPOLICIES_3X_H

