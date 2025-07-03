/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrpolicy_totalgpu_iface.h
 * @brief @copydoc pwrpolicy_totalgpu_iface.c
 */

#ifndef PWRPOLICY_TOTALGPU_IFACE_H
#define PWRPOLICY_TOTALGPU_IFACE_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/3x/pwrpolicy_3x.h"
#include "pmgr/pff.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_POLICY_TOTAL_GPU_INTERFACE PWR_POLICY_TOTAL_GPU_INTERFACE;

/*!
 * Structure representing a proportional-limit-based power policy.
 */
struct PWR_POLICY_TOTAL_GPU_INTERFACE
{
    /*!
     * @copydoc BOARDOBJ_INTERFACE
     */
    BOARDOBJ_INTERFACE      super;
    /*!
     * Power Policy Table Relationship index corresponding to the FB Power
     * Policy to update for this Total GPU Power Policy.
     */
    LwU8  fbPolicyRelIdx;
    /*!
     * Power Policy Table Relationship index corresponding to the Core Power
     * Policy to update for this Total GPU Power Policy.
     */
    LwU8  corePolicyRelIdx;
    /*!
     * Boolean indicating if we use a Power Channel Index to represent static
     * power, or use the fixed value to represent the static power.
     */
    LwBool  bUseChannelIdxForStatic;
    /*!
     * Union representing how we should interpret static power number.
     * Fixed value (in specified units): The value to subtract out of the total
     * available limit before assigning the remainder to Core and FB.
     * Power Channel Index: The index to a power channel representing power
     * consumption for static rail.
     */
    RM_PMU_PMGR_PWR_POLICY_TOTAL_GPU_STATIC_VAL_UNION staticVal;
    /*!
     * Inflection point 0
     * Cap the system below the "Inflection vpstate index" when the current
     * limit is smaller than this "Inflection limit". This inflection limit can
     * help improve some pstate thrashing issue when the power limit is reduced
     * into the "battery" or certain lower pstate range.
     */
    LwU32 limitInflection0;
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_3X_TGP_MULTI_INFLECTIONS))
    /*!
     * Inflection point 1
     */
    LwU32 limitInflection1;
#endif
    /*!
     * Last observed static rail power. This field is only used when
     * bUseChannelIdxForStatic is true. Used to cache static power rail's power
     * reading for policy evaluation.
     */
    LwU32 lastStaticPower;

};

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * BOARDOBJ_INTERFACE interfaces
 */
BoardObjInterfaceConstruct (pwrPolicyConstructImpl_TOTAL_GPU_INTERFACE)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicyConstructImpl_TOTAL_GPU_INTERFACE");

/*!
 * PWR_POLICY_TOTAL_GPU_INTERFACE interfaces
 */
FLCN_STATUS pwrPolicyGetStatus_TOTAL_GPU_INTERFACE(PWR_POLICY_TOTAL_GPU_INTERFACE *pTgpIface, RM_PMU_PMGR_PWR_POLICY_TOTAL_GPU_INTERFACE_BOARDOBJ_GET_STATUS *pGetStatus)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrPolicyQuery_TOTAL_GPU_INTERFACE");

LwU32       pwrPolicy3XChannelMaskGet_TOTAL_GPU_INTERFACE(PWR_POLICY_TOTAL_GPU_INTERFACE *pTgpIface)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicy3XChannelMaskGet_TOTAL_GPU_INTERFACE");
FLCN_STATUS pwrPolicy3XFilter_TOTAL_GPU_INTERFACE(PWR_POLICY_TOTAL_GPU_INTERFACE *pTgpIface, PWR_MONITOR *pMon, PWR_POLICY_3X_FILTER_PAYLOAD_TYPE *pPayload)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicy3XFilter_TOTAL_GPU_INTERFACE");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRPOLICY_TOTALGPU_IFACE_H
