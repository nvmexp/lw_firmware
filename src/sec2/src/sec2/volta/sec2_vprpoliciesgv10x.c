/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_vprpoliciesgv10x.c
 *
 * This file includes implementation of various vpr policy functions used by PR
 * and VPR app on GV10X
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "sec2_hs.h"
#include "dev_disp.h"
#include "dev_disp_addendum.h"
#include "dev_fuse.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "dev_lw_xve.h"
#include "lwdevid.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib_mutex.h"
#include <oemcommonlw.h>
#include "config/g_sec2_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*
 * This function updates the display blanking policy in following steps:
 * 1. Read current policy value.
 * 2. Merge policies for new policy value.
 * 3. Write new value to HW.
 *
 * @param[in]  policyValForContext      The policy value for current context.
 * @param[in]  bType1LockingRequired    Indicates if type1 lock is required.
 *
 * @return  On failure, bubble up the failure returned from the callee.
 */
FLCN_STATUS
sec2UpdateDispBlankingPolicyHS_GV10X
(
    LwU32  policyValForContext,
    LwBool bType1LockingRequired
)
{
    LwBool      bAllowVprScanoutInternal = LW_FALSE;
    LwU32       alwaysEnabledPolicies    = 0;
    LwU32       lwrPolicyInHw            = 0;
    LwU32       newPolicyForHw           = 0;
    FLCN_STATUS flcnStatus               = FLCN_OK;

    // SLI will always be blanked.
    alwaysEnabledPolicies = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_SLI, _ENABLE, alwaysEnabledPolicies);
    CHECK_FLCN_STATUS(sec2AllowVprScanoutToInternalHS_HAL(&Sec2, &bAllowVprScanoutInternal));

    if (!bAllowVprScanoutInternal)
    {
        alwaysEnabledPolicies = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_INTERNAL, _ENABLE, alwaysEnabledPolicies);
    }

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL, &lwrPolicyInHw));

    //
    // Logic to select the new policy value for HW:
    // 1.  We need to preserve the current policies HW is enforcing. Basically we can only upgrade here, not downgrade.
    //          Downgrade will happen when VPR size goes down to zero.
    // 2.  Always pick up the "always enabled policies" because we may be starting from scratch (no previous policy in HW)
    // 3.  And finally, pick up the policies this context requires.
    // So, we need to enable a particular policy if it is enabled in either of the three variables - alwaysEnabledPolicies, or lwrPolicyInHw or policyValForContext.
    // So, basically do a logical OR of every policy from all three variables:
    //      newPolicyForHw = alwaysEnabledPolicies |LOGICALOR| lwrPolicyInHw |LOGICALOR| policyValForContext;
    // IMPORTANT: Do not do bitwise OR here. It may work today since the policy value encompasses just 1 bit. But if the policy value expands to cover
    // multiple bits, then the bitwise OR will not work (lwrrently we have ENABLE=1 and DISABLE=0. If the policy meaning changes and we're required
    // to add a third meaning, thereby adding NEW_POLICY_BEHAVIOUR=2, then the bitwise OR will not work)
    //

    // It's ok to pick alwaysEnabledPolicies policies this way, since this a direct assignment rather than any bitwise operation
    newPolicyForHw = alwaysEnabledPolicies;

    // Handle no HDCP digital policy
    if (FLD_TEST_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_NO_HDCP, _ENABLE, lwrPolicyInHw) ||
        FLD_TEST_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_NO_HDCP, _ENABLE, policyValForContext))
    {
        newPolicyForHw = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_NO_HDCP, _ENABLE, newPolicyForHw);
    }

    // Handle HDCP1x digital policy
    if (FLD_TEST_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_HDCP1X, _ENABLE, lwrPolicyInHw) ||
        FLD_TEST_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_HDCP1X, _ENABLE, policyValForContext))
    {
        newPolicyForHw = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_HDCP1X, _ENABLE, newPolicyForHw);
    }

    // Handle HDCP2.2 type 0 digital policy
    if (FLD_TEST_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_HDCP22_TYPE0, _ENABLE, lwrPolicyInHw) ||
        FLD_TEST_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_HDCP22_TYPE0, _ENABLE, policyValForContext))
    {
        newPolicyForHw = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_HDCP22_TYPE0, _ENABLE, newPolicyForHw);
    }

    // Handle HDCP2.2 type 1 digital policy
    if (FLD_TEST_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_HDCP22_TYPE1, _ENABLE, lwrPolicyInHw) ||
        FLD_TEST_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_HDCP22_TYPE1, _ENABLE, policyValForContext))
    {
        newPolicyForHw = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_HDCP22_TYPE1, _ENABLE, newPolicyForHw);
    }

    // Handle internal panel policy
    if (FLD_TEST_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_INTERNAL, _ENABLE, lwrPolicyInHw) ||
        FLD_TEST_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_INTERNAL, _ENABLE, policyValForContext))
    {
        newPolicyForHw = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_INTERNAL, _ENABLE, newPolicyForHw);
    }

    // Tell HW to apply the policy IMMEDIATELY instead of waiting for next vblank
    newPolicyForHw = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _CHANGE_EFFECTIVE, _IMMEDIATE, newPolicyForHw);

    // All ok so far. Time to program new policies in HW.
    CHECK_FLCN_STATUS(sec2ProgramDispBlankingPolicyToHwHS_HAL(&Sec2, newPolicyForHw));

ErrorExit:
    return flcnStatus;
}

/*
 * This function determines the internal panel trust level.
 *
 * @param[in/out]  pbAllowVprScanout  The pointer points to the boolean buffer indicating
 *                                    whether internal panel is allowed.
 *
 * @return  FLCN_OK if the internal panel truse level is determined.
 *          FLCN_ERR_ILWALID_ARGUMENT if the input argument is invalid.
 */
FLCN_STATUS
sec2AllowVprScanoutToInternalHS_GV10X
(
    LwBool *pbAllowVprScanout
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    if (pbAllowVprScanout == NULL)
    {
        CHECK_FLCN_STATUS(FLCN_ERR_ILWALID_ARGUMENT);
    }

    // PR is not POR of GV100 NB, so we will never allow internal panel to do VPR.
    *pbAllowVprScanout = LW_FALSE;

ErrorExit:
    return flcnStatus;
}

/*
 * @brief  Acquire/Release HDCP TYPE1 lock
 *         Lock will be acquired when we update display blanking policy from PR app
 *         Lock will be released when VPR size goes down to 0 in VPR app
 *
 * @param[in]  bLock    Boolean to specify if it is Lock acquire or release call
 *
 * @return always LW_OK for GV10X_and_later
 */
FLCN_STATUS
sec2HdcpType1LockHs_GV10X
(
    LwBool bLock
)
{
    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
