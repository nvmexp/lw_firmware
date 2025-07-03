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
 * @file  pwrpolicy_proplimit.h
 * @brief @copydoc pwrpolicy_proplimit.c
 */

#ifndef PWRPOLICY_PROPLIMIT_H
#define PWRPOLICY_PROPLIMIT_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrpolicy_limit.h"
#include "pmgr/3x/pwrpolicy_3x.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_POLICY_PROP_LIMIT PWR_POLICY_PROP_LIMIT;

/*!
 * Structure representing a proportional-limit-based power policy.
 */
struct PWR_POLICY_PROP_LIMIT
{
    /*!
     * @copydoc PWR_POLICY_LIMIT
     */
    PWR_POLICY_LIMIT super;

    /*!
     * Index of first Power Policy Relationship in the Power Policy Table.  This
     * PWR_POLICY_PROP_LIMIT will adjust the limits of all PWR_POLICYs
     * corresponding to the PWR_POLICY_RELATIONSHIPs in the range
     * [policyRelIdxFirst, @ref policyRelIdxLast].
     */
    LwU8    policyRelIdxFirst;
    /*!
     * Index of last Power Policy Relationship in the Power Policy Table.  This
     * PWR_POLICY_PROP_LIMIT will adjust the limits of all PWR_POLICYs
     * corresponding to the PWR_POLICY_RELATIONSHIPs in the range [@ref
     * policyRelIdxFirst, @ref policyRelIdxFirst + policyRelIdxCnt).
     */
    LwU8 policyRelIdxCnt;
    /*!
     * Boolean indicating whether this PWR_POLICY_PROP_LIMIT object has been
     * dirtied by the @ref PWR_POLICY_BALANCE algorithm (via @ref
     * PWR_POLICY_RELATIONSHIP_BALANCE) and thus its capping request may be
     * inaclwrate.
     */
    LwBool  bBalanceDirty;
    /*!
     * Boolean flag indicating whether "dummy" operation is desired for this
     * PWR_POLICY_PROP_LIMIT object.  When "dummy" operation is engaged, the
     * PWR_POLICY_PROP_LIMIT object will compute the desired limit requests for
     * all referenced (via PWR_POLICY_RELATIONSHIPs) PWR_POLICY objects, but
     * will only store them internally and not issue the requests.
     *
     * This functionality is useful for IRB the PWR_POLICY_PROP_LIMIT is
     * controlling a single phase of LWVDD, such that any limit can be enforced
     * entirely via IRB by shifting the phase away -
     * i.e. even a limit of 0 can be satisfied.
     */
    LwBool bDummy;
    /*!
     * Array of cached limits requested by this PWR_POLICY_PROP_LIMIT to the
     * various PWR_POLICYs represented with its PWR_POLICY_RELATIONSHIPs.
     * Indexes are relative to @ref policyRelIdxFirst.
     */
    LwU32  *pLimitRequests;
};

/*!
 * @interface PWR_POLICY_PROP_LIMIT
 *
 * Retrieves the last requested limits the given PWR_POLICY_PROP_LIMIT offset
 * requested to another PWR_POLICY via a specified PWR_POLICY_RELATIONSHIP (as a
 * relative offset from @ref PWR_POLICY_PROP_LIMIT::policyRelIdxFirst).
 *
 * @param[in]  pPropLimit   PWR_POLICY_PROP_LIMIT pointer
 * @param[in]  relIdx
 *     Index of requested PWR_POLICY_RELATIONSHIP as an offset from @ref
 *     PWR_POLICY_PROP_LIMIT::policyRelIdxFirst.
 * @param[out] pLimitValue  Pointer in which to return last requested limit.
 */
#define PwrPolicyPropLimitLimitGet(fname) void (fname)(PWR_POLICY_PROP_LIMIT *pPropLimit, LwU8 relIdx, LwU32 *pLimitValue)

/* ------------------------- Defines --------------------------------------- */
/*!
 * Accessor macro for @ref PWR_POLICY_PROP_LIMIT::bBalanceDirty.
 *
 * @param[in] pPropLimit  PWR_POLICY_PROP_LIMIT pointer.
 *
 * @return @ref PWR_POLICY_PROP_LIMIT::bBalanceDirty
 */
#define pwrPolicyPropLimitBalanceDirtyGet(pPropLimit)                         \
    ((pPropLimit)->bBalanceDirty)

/*!
 * Mutator. macro for @ref PWR_POLICY_PROP_LIMIT::bBalanceDirty.
 *
 * @param[in] pPropLimit  PWR_POLICY_PROP_LIMIT pointer.
 * @param[in] bDirty      Dirty flag state to set.
 */
#define pwrPolicyPropLimitBalanceDirtySet(pPropLimit, bDirty)                 \
do {                                                                          \
    (pPropLimit)->bBalanceDirty = (bDirty);                                   \
} while (LW_FALSE)

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_POLICY interfaces
 */
BoardObjGrpIfaceModel10ObjSet      (pwrPolicyGrpIfaceModel10ObjSetImpl_PROP_LIMIT)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicyGrpIfaceModel10ObjSetImpl_PROP_LIMIT");
PwrPolicyFilter        (pwrPolicyFilter_PROP_LIMIT)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyFilter_PROP_LIMIT");
BoardObjIfaceModel10GetStatus          (pwrPolicyIfaceModel10GetStatus_PROP_LIMIT)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrPolicyIfaceModel10GetStatus_PROP_LIMIT");

/*!
 * PWR_POLICY_3X interfaces
 */
PwrPolicy3XFilter      (pwrPolicy3XFilter_PROP_LIMIT)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicy3XFilter_PROP_LIMIT");

/*!
 * PWR_POLICY_LIMIT interfaces
 */
PwrPolicyLimitEvaluate (pwrPolicyLimitEvaluate_PROP_LIMIT)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyLimitEvaluate_PROP_LIMIT");

/*!
 * PWR_POLICY_PROP_LIMIT interfaces
 */
PwrPolicyPropLimitLimitGet (pwrPolicyPropLimitLimitGet)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyPropLimitLimitGet");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRPOLICY_PROPLIMIT_H
