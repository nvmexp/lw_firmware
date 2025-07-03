/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrpolicy_30.h
 * @brief @copydoc pwrpolicy_30.c
 */

#ifndef PWRPOLICY_30_H
#define PWRPOLICY_30_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/3x/pwrpolicy_3x.h"

/* ------------------------- Types Definitions ----------------------------- */
/*!
 * @interface PWR_POLICY_30
 *
 * Interface which checks if PWR_POLICY has expired. If this check is positive
 * interface additionally updates time-stamp for it's next exelwtion.
 *
 * @param[in]   pPolicy     PWR_POLICY pointer
 * @param[in]   ticksNow    OS ticks time at which we evaluate policy expiration
 *
 * @return  Boolean if this PWR_POLICY has expired
 */
#define PwrPolicy30CheckExpiredAndUpdate(fname) LwBool (fname)(PWR_POLICY *pPolicy, LwU32 ticksNow)

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
// PWR_POLICY interfaces
PwrPolicyLoad                       (pwrPolicyLoad_SUPER_30)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrPolicyLoad_SUPER_30");

// PWR_POLICY_30 interfaces
PwrPolicy30CheckExpiredAndUpdate    (pwrPolicy30CheckExpiredAndUpdate)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyCallback", "pwrPolicy30CheckExpiredAndUpdate");

/* ------------------------ Defines ---------------------------------------- */
/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#endif // PWRPOLICY_30_H
