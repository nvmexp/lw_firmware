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
 * @file  pwrpolicy.h
 * @brief @copydoc pwrpolicy.c
 */

#ifndef PWRPOLICY_H
#define PWRPOLICY_H

/* ------------------------- System Includes ------------------------------- */
#include "pmgr/lib_pmgr.h"
#include "pmgr/pwrmonitor.h"
#include "boardobj/boardobjgrp.h"

/* ------------------------- Forward Definitions --------------------------- */
/*!
 * Adding forward definitions here since PWR_POLICY structure uses the pointer
 * to PWR_POLICY_MULTIPLIER_DATA which is defined in pwrpolicies_35.h
 */
typedef struct PWR_POLICY_MULTIPLIER_DATA PWR_POLICY_MULTIPLIER_DATA;
typedef struct PWR_POLICY                 PWR_POLICY, PWR_POLICY_BASE;

/* ------------------------- Application Includes -------------------------- */
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------------ Macros ------------------------------------*/
/*!
 * Helper macro which returns the overlay index to use for allocating memory
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_ALLOCATE_FROM_OS_HEAP)
#define PWR_POLICY_ALLOCATIONS_DMEM_OVL(_ovlIdxDmem) OVL_INDEX_OS_HEAP
#else
#define PWR_POLICY_ALLOCATIONS_DMEM_OVL(_ovlIdxDmem) _ovlIdxDmem
#endif

/* ------------------------- Types Definitions ----------------------------- */

#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_INTEGRAL_CONTROL)
/*!
 * The structure representing the Integral Control in a power policy.
 */
typedef struct
{
    /*!
     * No of past samples, looking at which the control
     * algorithm does an adjustment to the future samples.
     */
    LwU8   pastSampleCount;

    /*!
     * No of future samples to apply the limit adjustment.
     */
    LwU8   nextSampleCount;

    /*!
     * The minimum value of the bounding box for the limit
     * adjustment, a ratio from the current policy limit.
     *
     * Unitless Unsigned FXP 4.12.
     */
    LwUFXP4_12 ratioLimitMin;

    /*!
     * The maximum value of the bounding box for the limit
     * adjustment, a ratio from the current policy limit.
     *
     * Unitless Unsigned FXP 4.12.
     */
    LwUFXP4_12 ratioLimitMax;

     /*!
     * The aclwmulated sum of differences between
     * the current power value and current policy limit.
     */
    LwS32  lwrrRunningDiff;

    /*!
     * The cirlwlar buffer index that tracks the
     * addition or subtraction of power values to
     * the aclwmulated sum of differences.
     */
    LwU32  lwrrSampleIndex;

    /*!
     * The cirlwlar buffer to hold the difference between current
       power and current policy limit.
     */
     LwS32  *pData;

    /*!
     * The new callwlated power limit for all the future samples.
     */
    LwS32  lwrrIntegralLimit;
} PWR_POLICY_INTEGRAL;
#endif

/*!
 * Arbitration struture for current limit value this PWR_POLICY object is
 * enforcing.  This is the arbitrated output (lowest) of all the @ref
 * LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_INPUT.  Will always be within range of
 * [@ref limitMin, @ref limitMax].
 */
typedef struct
{
    /*!
     * Arbitration function - either max (LW_TRUE) or min (LW_FALSE).
     */
    LwBool bArbMax;
    /*!
     * Current number of active limits.  Will always be <= @ref
     * LW2080_CTRL_PMGR_PWR_POLICY_MAX_LIMIT_INPUTS.
     */
    LwU8   numInputs;
    /*!
     * Arbitrated output value.
     */
    LwU32  output;
    /*!
     * Array of LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_INPUT entries.  Has valid
     * indexes in the range of [0, @ref numInputs).
     */
    LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_INPUT *pInputs;
} PWR_POLICY_LIMIT_ARBITRATION;

/*!
 * Power Policy 3.x Only
 * Structure representing Block Average Filter functionality as an Input Filter
 */
typedef struct
{
    /*!
     * Filter type.
     */
    LwU8   type;
    /*!
     * Counter to insert samples for block averaging
     * resets to 0 when it is same as the blockSize.
     */
    LwU8   count;
    /*!
     * Filter block size.
     */
    LwU8   blockSize;
    /*
     * Input data array (i.e non negative current or power values)
     * gets cleared up when counter resets.
     */
    LwU32 *pFilterData;
} PWR_POLICY_3X_FILTER_BLOCK;

/*!
 * Power Policy 3.x Only
 * Structure representing Moving Average filter functionality as an Input
 * Filter.
 */
typedef struct
{
    /*!
     * Filter type.
     */
    LwU8   type;
    /*!
     * Counter to help in inserting the new samples
     * resets to 0 when it becomes same as windowSize.
     */
    LwU8   count;
    /*!
     * Filter Window size
     */
    LwU8   windowSize;
    /*!
     * Boolean indicating if window is full
     */
    LwBool bWindowFull;
    /*
     * Input data array (i.e non negative current or power values)
     * Acts as cirlwlar buffer for evaluating moving average.
     */
    LwU32 *pFilterData;
} PWR_POLICY_3X_FILTER_MOVING;

/*!
 * Power Policy 3.x Only
 * Structure representing Infinite Impulse Response (IIR) filter functionality
 * as an Input Filter.
 */
typedef struct
{
    /*!
     * Filter type.
     */
    LwU8        type;
    /*!
     * Last sample.
     */
    LwUFXP20_12 lastSample;
    /*!
     * Divisor.
     * (TBD) newSample = (lastSample * (divisor - 1) / divisor) + (lwrrentSample / divisor).
     */
    LwU8        divisor;
} PWR_POLICY_3X_FILTER_IIR;

/*!
 * Power Policy 3.x Only
 * Union of all possible power policy filter data structure.
 */
typedef union
{
    LwU8                        type;
    PWR_POLICY_3X_FILTER_BLOCK  block;
    PWR_POLICY_3X_FILTER_MOVING moving;
    PWR_POLICY_3X_FILTER_IIR    iir;
} PWR_POLICY_3X_FILTER;

/*!
 * Main structure representing a power policy.
 */
struct PWR_POLICY
{
    /*!
     * BOARDOBJ_VTABLE super-class.
     */
    BOARDOBJ_VTABLE     super;

    /*!
     * Index into Power Topology Table (PWR_CHANNEL) for input channel.
     */
    LwU8                chIdx;
    /*!
     * Number of other PWR_POLICYs which will request a LIMIT_INPUT on this
     * PWR_POLICY.
     */
    LwU8                numLimitInputs;
    /*!
     * Units of limit values.  @ref LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_<xyz>
     */
    LwU8                limitUnit;
    /*!
     * Cache policy error status for later use (group with other 8-bit fields).
     */
    FLCN_STATUS         status;
    /*!
     * Minimum allowed limit value.
     */
    LwU32               limitMin;
    /*!
     * Maximum allowed limit value.
     */
    LwU32               limitMax;

    /*!
     * Copydoc@ PWR_POLICY_LIMIT_ARBITRATION
     */
    PWR_POLICY_LIMIT_ARBITRATION limitArbLwrr;

    /*!
     * Latest value retreived from the monitored PWR_CHANNEL.
     */
    LwU32               valueLwrr;

    /*!
     * This is the actual limit on which, the policy is operating.
     * This is the effective limit from _LIMIT_ARBITRATION::output
     * and any other SUPER policy algorithm (for e.g Integral Control).
     */
    LwU32               limitLwrr;

#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_INTEGRAL_CONTROL)
    /*!
     * Integral Control
     */
    PWR_POLICY_INTEGRAL integral;
#endif

    // Power Policy 3.x only fields
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_3X)
    /*!
     * Filter data.
     */
    PWR_POLICY_3X_FILTER filter;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_3X)

    // Power policy 3.0 only fields.
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_30)
    /*!
     * Period of this power policy (in OS ticks).
     */
    LwU32                ticksPeriod;
    /*!
     * Expiration time (next callback) of this power policy (is OS ticks).
     */
    LwU32                ticksNext;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_30)

    // Power Policy 3.5 only fields
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_35)
    /*!
     * This policy's sampling period is sampleMult * baseSamplingPeriodms.
     */
    LwU8                        sampleMult;
    /*!
     * Reset controller state on next cycle.
     */
    LwBool                      bReset;
    /*!
     * Multiplier data structure shared by this policy.
     */
    PWR_POLICY_MULTIPLIER_DATA *pMultData;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_35)
};

/*!
 * @interface PWR_POLICY
 *
 * Loads a PWR_POLICY specified by pPolicy. The Load interface would be a collection of all HW
 * related operations (as opposed to a 'Construct' interface). This function will be called
 * after pwrPolicyConstruct finished and will program any HW state.
 *
 * @param[in]   pPolicy     PWR_POLICY pointer.
 * @param[in]   ticksNow    OS ticks timestamp to synchronize all load() code
 *
 * @return FLCN_OK
 *      The Load for this policy exelwted successfully.
 *
 * @return FLCN_ERR_NOT_SUPPORTED
 *      This interface is not supported on specific policy.
 *
 * @return Other errors.
 *      Unexpected errors propogated up from type-specific implementations.
 */
#define PwrPolicyLoad(fname) FLCN_STATUS (fname)(PWR_POLICY *pPolicy, LwU32 ticksNow)

/*!
 * @interface PWR_POLICY
 *
 * Retrieves the latest PWR_CHANNEL value, stores it in the PWR_POLICY object
 * for later use/querying, and then applies any necessary input filtering.  The
 * stored values (both the native input and filtered value) can be used later to
 * evaluate and take any required policy actions.
 *
 * @param[in]  pPolicy   PWR_POLICY pointer
 * @param[in]  pMon      PWR_MONITOR pointer
 *
 * @return FLCN_OK
 *     Value successfully filtered and cached.
 *
 * @return Other errors
 *     Unexpected errors returned from PWR_MONITOR.
 */
#define PwrPolicyFilter(fname) FLCN_STATUS (fname)(PWR_POLICY *pPolicy, PWR_MONITOR *pMon)

/*!
 * @interface PWR_POLICY
 *
 * Returns the latest filtered PWR_CHANNEL @ref value for use by other policies as
 * the "observed" input value on that policy.  For example, this interface is
 * used by the PWR_POLICY_RELATIONSHIP structure to retrieve the current value
 * of the corresponding PWR_POLICY.
 *
 * @param[in] pPolicy PWR_POLICY pointer
 *
 * @return Filtered value from PWR_POLICY object.
 */
#define PwrPolicyValueLwrrGet(fname) LwU32 (fname)(PWR_POLICY *pPolicy)

/*!
 * @interface PWR_POLICY
 *
 * Interface by which a client PWR_POLICY can get the most recent limit value
 * it requested for this PWR_POLICY.  This interface is used by PWR_POLICYs which
 * take corrective actions by altering the power limits of other PWR_POLICYs.
 *
 * @param[in]   pPolicy      PWR_POLICY pointer
 * @param[in]   pwrPolicyIdx PWR_POLICY index value for the requesting client.
 *
 * @return
 *     Last requested Limit value from given client ref@ pwrPolicyIdx.
 */
#define PwrPolicyLimitArbInputGet(fname) LwU32 (fname)(PWR_POLICY *pPolicy, LwU8 pwrPolicyIdx)

/*!
 * @interface PWR_POLICY
 *
 * Interface by which a client PWR_POLICY can request a new limit value for this
 * PWR_POLICY.  This interface is used by PWR_POLICYs which take corrective
 * actions by altering the power limits of other PWR_POLICYs.
 *
 * @param[in]  pPolicy      PWR_POLICY pointer
 * @param[in]  pwrPolicyIdx PWR_POLICY index value for the requesting client.
 * @param[in]  limitValue   Limit value this client is requesting.
 *
 * @return FLCN_OK
 *     Limit value successfully applied as input.
 *
 * @return FLCN_ERR_ILWALID_STATE
 *     Could not set a corresponding _LIMIT_INPUT structure for these values
 *     because no free entries remained in @ref PWR_POLICY::limitInputs.inputs
 *     array.
 */
#define PwrPolicyLimitArbInputSet(fname) FLCN_STATUS (fname)(PWR_POLICY *pPolicy, LwU8 pwrPolicyIdx, LwU32 limitValue)

/*!
 * @interface PWR_POLICY
 *
 * To evaluate each individual policy
 *
 * @param[in]  pPolicy      PWR_POLICY pointer
 */
#define PwrPolicyEvaluate(fname) FLCN_STATUS (fname)(PWR_POLICY *pPolicy)

/*!
 * @interface PWR_POLICY
 *
 * To get the cached PFF frequency floor which was evaluted by the given PWR_POLICY.
 *
 * @param[in]  pPolicy      PWR_POLICY pointer
 * @param[out] pFreqkHz     Output of pff evaluation
 */
#define PwrPolicyGetPffCachedFrequencyFloor(fname) FLCN_STATUS (fname)(PWR_POLICY *pPolicy, LwU32 *pFreqkHz)

/*!
 * @interface PWR_POLICY
 *
 * Interface called when VF lwve has been ilwalidated. Frequencies of PFF tuples will
 * be adjusted at this time with any OC from the VF lwrve for example.
 *
 * @param[in]  pPolicy      PWR_POLICY pointer
 */
#define PwrPolicyVfIlwalidate(fname) FLCN_STATUS (fname)(PWR_POLICY *pPolicy)

/* ------------------------- Macros ---------------------------------------- */
/*!
 * Helper macro to retrieve the effective and cached policy limit.
 * This is the actual policy limit on which the policy is operating.
 * This could be the effective limit after applying any other policy specific
 * controls to LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_ARBITRATION::output and
 * could be more than the LIMIT_ARBITRATION::output.
 */
#define pwrPolicyLimitLwrrGet(pPolicy)                                        \
    (pPolicy)->limitLwrr

/*!
 * Helper macro to retrieve the arbitrated output of the @ref
 * PWR_POLICY::limitArbLwrr structure.  This value is the current arbitrated
 * output of all input requests to the arbiter.
 *
 * @param[in] pPolicy PWR_POLICY pointer
 *
 * @return @ref LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_ARBITRATION::output
 */
#define pwrPolicyLimitArbLwrrGet(pPolicy)                                     \
    (pPolicy)->limitArbLwrr.output

/*!
 * Helper macro to retrieve the maximum allowed limit value
 *
 * @param[in]  pPolicy    PWR_POLICY pointer
 *
 * @return @ref PWR_POLICY::limitMax
 */
#define pwrPolicyLimitMaxGet(pPolicy)                                         \
    (pPolicy)->limitMax

/*!
 * Helper macro to retrieve the minimum allowed limit value
 *
 * @param[in]  pPolicy    PWR_POLICY pointer
 *
 * @return @ref PWR_POLICY::limitMin
 */
#define pwrPolicyLimitMinGet(pPolicy)                                         \
    (pPolicy)->limitMin

/*!
 * Helper macro to retrieve the Units of limit values.
 * @ref LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_<xyz>
 *
 * @param[in]  pPolicy    PWR_POLICY pointer
 *
 * @return @ref PWR_POLICY::limitUnit
 */
#define pwrPolicyLimitUnitGet(pPolicy)                                         \
    ((PWR_POLICY *)pPolicy)->limitUnit

/*!
 * Mutator macro for @ref PWR_POLICY::valueLwrr.
 *
 * @param[in]  pPolicy    PWR_POLICY pointer
 * @param[in]  valueToSet New value to set
 */
#define pwrPolicyValueLwrrSet(pPolicy, valueToSet)                            \
do {                                                                          \
    (pPolicy)->valueLwrr = (valueToSet);                                      \
} while (LW_FALSE)

#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_3X)
#define pwrPolicyGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppPolicy, size, pDesc)         \
    pwrPolicyGrpIfaceModel10ObjSetImpl_SUPER_3X(pModel10, ppPolicy, size, pDesc)
#else
#define pwrPolicyGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppPolicy, size, pDesc)         \
    pwrPolicyGrpIfaceModel10ObjSetImpl_SUPER_2X(pModel10, ppPolicy, size, pDesc)
#endif

/*!
  * Helper PWR_POLICY MACRO for @copydoc BOARDOBJ_TO_INTERFACE_CAST
  */
#define PWR_POLICY_BOARDOBJ_TO_INTERFACE_CAST(pPolicy, type)                 \
    BOARDOBJ_TO_INTERFACE_CAST((pPolicy), PMGR, PWR_POLICY, type)

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_POLICY interfaces
 */
BoardObjGrpIfaceModel10ObjSet                   (pwrPolicyGrpIfaceModel10ObjSetImpl)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicyGrpIfaceModel10ObjSetImpl");
PwrPolicyLoad                       (pwrPolicyLoad_SUPER)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrPolicyLoad_SUPER");
PwrPolicyLoad                       (pwrPolicyLoad)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrPolicyLoad");
BoardObjIfaceModel10GetStatus                       (pwrPolicyIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrPolicyIfaceModel10GetStatus");
PwrPolicyValueLwrrGet               (pwrPolicyValueLwrrGet)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyClient", "pwrPolicyValueLwrrGet");
PwrPolicyLimitArbInputSet           (pwrPolicyLimitArbInputSet)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyClient", "pwrPolicyLimitArbInputSet");
PwrPolicyLimitArbInputGet           (pwrPolicyLimitArbInputGet)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyClient", "pwrPolicyLimitArbInputGet");
PwrPolicyEvaluate                   (pwrPolicyEvaluate);

PwrPolicyFilter                     (pwrPolicyFilter_SUPER)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyFilter_SUPER");
PwrPolicyGetPffCachedFrequencyFloor (pwrPolicyGetPffCachedFrequencyFloor)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyGetPffCachedFrequencyFloor");
PwrPolicyVfIlwalidate               (pwrPolicyVfIlwalidate)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyVfIlwalidate");

/*!
 * Integral controller constructor
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_INTEGRAL_CONTROL)
FLCN_STATUS pwrPolicyConstructIntegralControl(PWR_POLICY *pPolicy, RM_PMU_PMGR_PWR_POLICY_BOARDOBJ_SET *pDesc, LwU8 ovlIdxDmem)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicyConstructIntegralControl");
#endif

/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */
#include "pmgr/pwrpolicy_balance.h"
#include "pmgr/pwrpolicy_domgrp.h"
#include "pmgr/pwrpolicy_limit.h"
#include "pmgr/pwrpolicy_workload_single_1x.h"
#include "pmgr/2x/pwrpolicy_2x.h"
#include "pmgr/3x/pwrpolicy_3x.h"
#include "pmgr/3x/pwrpolicy_35.h"
#endif // PWRPOLICY_H
