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
 * @file  pwrpolicy_balance.h
 * @brief @copydoc pwrpolicy_balance.c
 */


#ifndef PWRPOLICY_BALANCE_H
#define PWRPOLICY_BALANCE_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrpolicy.h"
#include "pmgr/3x/pwrpolicy_3x.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_POLICY_BALANCE PWR_POLICY_BALANCE;

/*!
 * Structure representing a workload-based power policy.
 */
struct PWR_POLICY_BALANCE
{
    /*!
     * @copydoc PWR_POLICY
     */
    PWR_POLICY  super;

    /*!
     * Index of first Power Policy Relationship object for this class.
     * This class will run the power balancing algorithm on all controllers specified
     * by @ref PWR_POLICY_RELATIONSHIP objects in the range
     * (@ref policyRelIdxFirst, @ref policyRelIdxLast)
     */
    LwU8        policyRelIdxFirst;

    /*!
     * Index of last Power Policy Relationship object for this class.
     * This class will run the power balancing algorithm on all controllers specified
     * by @ref PWR_POLICY_RELATIONSHIP objects in the range
     * (@ref policyRelIdxFirst, @ref policyRelIdxLast)
     */
    LwU8        policyRelIdxLast;

    /*!
     * Count of the Power Policy Relatinship objects for this class.
     * Redundant field introduced to reduce IMEM size, always equaling to
     * (@ref policyRelIdxLast - @ref policyRelIdxFirst + 1).
     */
    LwU8        policyRelIdxCount;

    /*!
     * Sorting buffer for relationship entries.  The PWR_POLICY_BALANCE class
     * uses this buffer to sort all its PWR_POLICY_RELATIONSHIP_BALANCE objects
     * based on the requested limits of each's PWR_POLICYs.  The
     * PWR_POLICY_BALANCE class then iterates over the sorted array to do actual
     * balancing.
     */
    LW2080_CTRL_PMGR_PWR_POLICY_STATUS_DATA_BALANCE_RELATIONSHIP_ENTRY
               *pRelEntries;
};

/*!
 * @interface PWR_POLICY_BALANCE
 *
 * Evaluates the PWR_POLICY_BALANCE implementation, trying to balance power draw
 * between a number of other PWR_POLICY objects in order to equally saturate
 * those PWR_POLICYs in a manner that they all cap performance equally.
 *
 * @param[in] pBalance  PWR_POLICY_BALANCE pointer
 *
 * @return FLCN_OK
 *     Balancing completed successfully.
 * @return Other errors
 *     Unexpected errors oclwrred during balancing.
 */
#define PwrPolicyBalanceEvaluate(fname) FLCN_STATUS (fname)(PWR_POLICY_BALANCE *pBalance)

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_POLICY interfaces
 */
BoardObjGrpIfaceModel10ObjSet         (pwrPolicyGrpIfaceModel10ObjSetImpl_BALANCE)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicyGrpIfaceModel10ObjSetImpl_BALANCE");
BoardObjIfaceModel10GetStatus             (pwrPolicyIfaceModel10GetStatus_BALANCE)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrPolicyIfaceModel10GetStatus_BALANCE");

/*!
 * PWR_POLICY_3X interfaces
 */
PwrPolicy3XChannelMaskGet (pwrPolicy3XChannelMaskGet_BALANCE)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicy3XChannelMaskGet_BALANCE");
PwrPolicy3XFilter         (pwrPolicy3XFilter_BALANCE)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicy3XFilter_BALANCE");

/*!
 * PWR_POLICY_BALANCE interfaces.
 */
PwrPolicyBalanceEvaluate  (pwrPolicyBalanceEvaluate)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyBalanceEvaluate");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRPOLICY_BALANCE_H
