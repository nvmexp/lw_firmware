/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrpolicies_status.h
 * @brief Structure specification for the Power Policy status functionality
 */

#ifndef PWRPOLICIES_STATUS_H
#define PWRPOLICIES_STATUS_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
/* ------------------------- Types Definitions ----------------------------- */

typedef struct
{
    /*!
     * Mask of all requested policies.
     */
    BOARDOBJGRPMASK_E32 mask;

    /*!
     * Timestamp at which this data was collected.
     */
    LwU64_ALIGN32 timestamp;

    /*!
     * Status for each object that can be requested.
     */
    RM_PMU_PMGR_PWR_POLICY_BOARDOBJ_GET_STATUS_UNION policies[
        LW2080_CTRL_PMGR_PWR_POLICY_MAX_POLICIES];
} PWR_POLICIES_STATUS;

/* ------------------------- External definitions --------------------------- */
/* ------------------------- Macros ----------------------------------------- */

/*!
 * List of overlays required for @ref pwrPoliciesStatusGet
 *
 * @note    This path has not actually been tried on non-ODP builds to prove
 *          that this path works.
 */
#define OSTASK_OVL_DESC_DEFINE_PWR_POLICIES_STATUS_GET                         \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibQuery)
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Inline Functions ------------------------------- */

/*!
 * @brief   Initializes a newly created @ref PWR_POLICIES_STATUS structure
 *
 * @param[out]  pPwrPoliciesStatus  Pointer to structure to initialize
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  pPwrPoliciesStatus is NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
pwrPoliciesStatusInit
(
    PWR_POLICIES_STATUS *pPwrPoliciesStatus
)
{
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        pPwrPoliciesStatus != NULL ? FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        pwrPoliciesStatusInit_exit);

    boardObjGrpMaskInit_E32(&pPwrPoliciesStatus->mask);

pwrPoliciesStatusInit_exit:
    return status;
}

/*!
 * @brief   Retrieves an individual @ref PWR_POLICY object's status
 *
 * @param[in]   pPwrPoliciesStatus  Pointer to status structure from which
 *                                  retrieve individual PWR_POLICY status
 * @param[in]   policyIdx           Index of individual @ref PWR_POLICY for
 *                                  which to status
 *
 * @return  Pointer to status of @ref PWR_POLICY at index policyIdx
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline RM_PMU_PMGR_PWR_POLICY_BOARDOBJ_GET_STATUS_UNION *
pwrPoliciesStatusPolicyStatusGet
(
    PWR_POLICIES_STATUS *pPwrPoliciesStatus,
    LwBoardObjIdx policyIdx
)
{
    return &pPwrPoliciesStatus->policies[policyIdx];
}

/*!
 * @copydoc pwrPoliciesStatusPolicyStatusGet
 *
 * @note    Retrieves an individual @ref PWR_POLICY object's status as const
 *          from a const @ref PWR_POLICIES_STATUS object
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline const RM_PMU_PMGR_PWR_POLICY_BOARDOBJ_GET_STATUS_UNION *
pwrPoliciesStatusPolicyStatusGetConst
(
    const PWR_POLICIES_STATUS *pPwrPoliciesStatus,
    LwBoardObjIdx policyIdx
)
{
    return &pPwrPoliciesStatus->policies[policyIdx];
}

#endif // PWRPOLICIES_STATUS_H
