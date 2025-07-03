/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrpolicy_2x.c
 * @brief  Interface specification for a PWR_POLICY.
 *
 * A "Power Policy" is a logical construct representing a behavior (a limit
 * value) to enforce on a power rail, as monitored by a Power Channel.  This
 * limit is enforced via a mechanism specified in the Power Policy
 * implementation.
 *
 * PWR_POLICY is a virtual class/interface.  It alone does not specify a full
 * power policy/mechanism.  Classes which implement PWR_POLICY specify a full
 * mechanism for control.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmgr/2x/pwrpolicy_2x.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @copydoc PwrPolicyFilter
 */
FLCN_STATUS
pwrPolicyFilter_2X
(
    PWR_POLICY  *pPolicy,
    PWR_MONITOR *pMon
)
{
    FLCN_STATUS status  = FLCN_OK;
    LwU32       powermW;

    // Get the latest power reading for the channel this policy is managing.
    status = pwrMonitorChannelStatusGet(pMon, pPolicy->chIdx, &powermW);
    if (status != FLCN_OK)
    {
        goto _pwrPolicyFilter_2X_done;
    }
    pPolicy->valueLwrr = powermW;

    // Call into any class-specific implementations for extra filtering.
    status = pwrPolicyFilter_SUPER(pPolicy, pMon);

_pwrPolicyFilter_2X_done:
    pPolicy->status = status;
    return status;
}

