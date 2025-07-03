/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrpolicy_35.h
 * @brief @copydoc pwrpolicy_35.c
 */

#ifndef PWRPOLICY_35_H
#define PWRPOLICY_35_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrpolicy.h"

/* ------------------------- Types Definitions ----------------------------- */

/*!
 * Gets perfCfControllers index per policy
 *
 * @param[in]     pPolicy  PWR_POLICY pointer.
 * @param[out]    pMask    Mask of referenced PERF_CF_CONTROLLERS
 *
 * @return FLCN_OK
 *     Successfully created the mask.
 * @return Other errors
 *     Propagates return values from various calls.
 */
#define PwrPolicy35PerfCfControllerMaskGet(fname) FLCN_STATUS (fname)(PWR_POLICY *pPolicy, BOARDOBJGRPMASK_E32 *pMask)

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
FLCN_STATUS  pwrPolicy35SetMultData(PWR_POLICY *pPolicy, PWR_POLICY_MULTIPLIER_DATA *pMultData)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicy35SetMultData");
PwrPolicy35PerfCfControllerMaskGet (pwrPolicy35PerfCfControllerMaskGet)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicy35PerfCfControllerMaskGet");

/* ------------------------ Defines ---------------------------------------- */
/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#endif // PWRPOLICY_35_H
