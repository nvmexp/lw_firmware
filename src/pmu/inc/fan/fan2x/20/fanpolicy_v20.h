/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file fanpolicy_v20.h
 * @brief @copydoc fanpolicy_v20.c
 */

#ifndef FAN_POLICY_V20_H
#define FAN_POLICY_V20_H

/* ------------------------- System Includes ------------------------------- */
#include "fan/fanpolicy.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct FAN_POLICY_V20 FAN_POLICY_V20;

/* ------------------------- Application Includes -------------------------- */
/* ------------------------- Macros ---------------------------------------- */
#define fanPolicyIfaceModel10GetStatus_V20(pModel10, pPayload)                             \
    fanPolicyIfaceModel10GetStatus_SUPER(pModel10, pPayload)

/* ------------------------- Datatypes ------------------------------------- */

/*!
 * Extends FAN_POLICY providing attributes common to all FAN_POLICY_V20.
 */
struct FAN_POLICY_V20
{
    /*!
     * FAN_POLICY super class. This should always be the first member!
     */
    FAN_POLICY  super;

    /*!
     * Parameters obtained from RM.
     */

    /*!
     * Index into the Fan Cooler Table.
     */
    LwU8    coolerIdx;

    /*!
     * Sampling period.
     */
    LwU16   samplingPeriodms;

    /*!
     * PMU specific parameters.
     */

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_MULTI_FAN)
    /*!
     * The time, in microseconds, the next callback should occur. If the policy
     * does not poll, this value shall be FAN_POLICY_TIME_INFINITY.
     */
    LwU32   callbackTimestampus;

#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_MULTI_FAN)
};

/* ------------------------- Defines --------------------------------------- */
/*!
 * Used for fan policies that do not require timer callbacks.
 */
#define FAN_POLICY_TIME_INFINITY    LW_U32_MAX

/* ------------------------ Function Prototypes ---------------------------- */
/*!
 * Board Object interfaces.
 */
BoardObjGrpIfaceModel10ObjSet  (fanPolicyGrpIfaceModel10ObjSetImpl_V20)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommonConstruct", "fanPolicyGrpIfaceModel10ObjSetImpl_V20");

FanPolicyLoad       (fanPolicyLoad_V20);
FanPolicyFanPwmSet  (fanPolicyFanPwmSet_V20);
FanPolicyFanRpmSet  (fanPolicyFanRpmSet_V20);

FLCN_STATUS fanPoliciesLoad_V20(void)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommon", "fanPoliciesLoad_V20");

/* ------------------------ Include Derived Types -------------------------- */

#endif // FAN_POLICY_V20_H
