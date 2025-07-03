/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_vprpoliciesgp10x.c
 *
 * This file includes implementation of various vpr policy functions used by PR
 * and VPR app on GP10X
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "sec2_hs.h"
#include "mmu/mmucmn.h"
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
// The timeout to wait DPU clear the response flag.
#define DPU_TYPE1_LOCK_RESPONSE_TIMEOUT_VALUE_NS        50*1000*1000 // 50 ms

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*
 * @brief  Acquire/Release HDCP TYPE1 lock
 *         Lock will be acquired when we update display blanking policy from PR app
 *         Lock will be released when VPR size goes down to 0 in VPR app
 *
 * @param[in]  bLock    Boolean to specify if it is Lock acquire or release call
 */
FLCN_STATUS
sec2HdcpType1LockHs_GP10X
(
    LwBool bLock
)
{
    FLCN_STATUS flcnStatus                    = FLCN_OK;
    FLCN_STATUS cleanupStatus1                = FLCN_OK;
    FLCN_STATUS cleanupStatus2                = FLCN_OK;
    LwU8        dispSelwreScratchMutexToken   = LW_MUTEX_OWNER_ID_ILWALID;
    LwU8        dispSelwreWrScratchMutexToken = LW_MUTEX_OWNER_ID_ILWALID;
    LwU8        vprSelwreScratchMutexToken    = LW_MUTEX_OWNER_ID_ILWALID;
    LwU8        playreadyMutexToken           = LW_MUTEX_OWNER_ID_ILWALID;
    LwU32       data32                        = 0;
    LwU32       ptimer0Start;
    LwU32       ptimer0Lwrrent;

    // Step1: Acquire Playready generic mutex to make sure no one can fire the request again before the ongoing request is handled
    if (FLCN_OK != (flcnStatus = mutexAcquire_hs(LW_MUTEX_ID_PLAYREADY, VPR_MUTEX_ACQUIRE_TIMEOUT_NS, &playreadyMutexToken)))
    {
        CHECK_FLCN_STATUS(FLCN_ERR_HS_MUTEX_ACQUIRE_FAILED);
    }

    //
    // Step2: Ensure that there is no outstanding request to DPU. If there is an outstanding request this indicates an internal error.
    // (The access to LW_PDISP_SELWRE_WR_SCRATCH_0 needs to be protected by level 2 Display scratch mutex)
    //
    if (FLCN_OK != (flcnStatus = mutexAcquire_hs(LW_MUTEX_ID_DISP_SELWRE_WR_SCRATCH, VPR_MUTEX_ACQUIRE_TIMEOUT_NS, &dispSelwreWrScratchMutexToken)))
    {
        CHECK_FLCN_STATUS(FLCN_ERR_HS_MUTEX_ACQUIRE_FAILED);
    }

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PDISP_SELWRE_WR_SCRATCH_0, &data32));
    if (!FLD_TEST_DRF(_PDISP, _SELWRE_WR_SCRATCH_0, _VPR_HDCP22_TYPE1_LOCK_RESPONSE, _RESET, data32))
    {
        CHECK_FLCN_STATUS(FLCN_ERR_DPU_IS_BUSY);
    }

    //
    // Step3: Raise the priv level mask for LW_PDISP_SELWRE_SCRATCH register where we will write the type1 lock request,
    // so that the register won't be written by untrusted client. (The access to LW_PDISP_SELWRE_SCRATCH_0 needs to be
    // protected by level 3 Display scratch mutex)
    //
    if (FLCN_OK != (flcnStatus = mutexAcquire_hs(LW_MUTEX_ID_DISP_SELWRE_SCRATCH, VPR_MUTEX_ACQUIRE_TIMEOUT_NS, &dispSelwreScratchMutexToken)))
    {
        CHECK_FLCN_STATUS(FLCN_ERR_HS_MUTEX_ACQUIRE_FAILED);
    }

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PDISP_SELWRE_SCRATCH_PRIV_LEVEL_MASK, &data32));
    data32 = FLD_SET_DRF(_PDISP, _SELWRE_SCRATCH_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ALL_LEVELS_DISABLED, data32);
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PDISP_SELWRE_SCRATCH_PRIV_LEVEL_MASK, data32));

    // Step4: Fill the value to LW_PDISP_SELWRE_SCRATCH_0 so that DPU can lock/unlock HDCP2.2 type1 base on the value
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PDISP_SELWRE_SCRATCH_0, &data32));
    if (bLock)
    {
        data32 = FLD_SET_DRF(_PDISP, _SELWRE_SCRATCH_0, _VPR_HDCP22_TYPE1_LOCK, _ENABLE, data32);
    }
    else
    {
        data32 = FLD_SET_DRF(_PDISP, _SELWRE_SCRATCH_0, _VPR_HDCP22_TYPE1_LOCK, _DISABLE, data32);
    }
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PDISP_SELWRE_SCRATCH_0, data32));

    if (FLCN_OK != (flcnStatus = mutexRelease_hs(LW_MUTEX_ID_DISP_SELWRE_SCRATCH, dispSelwreScratchMutexToken)))
    {
        CHECK_FLCN_STATUS(FLCN_ERR_HS_MUTEX_RELEASE_FAILED);
    }
    dispSelwreScratchMutexToken = LW_MUTEX_OWNER_ID_ILWALID;

    // Step5: Set response bit to AWAITING 
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PDISP_SELWRE_WR_SCRATCH_0, &data32));
    data32 = FLD_SET_DRF(_PDISP, _SELWRE_WR_SCRATCH_0, _VPR_HDCP22_TYPE1_LOCK_RESPONSE, _AWAITING_RESPONSE, data32);
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PDISP_SELWRE_WR_SCRATCH_0, data32));

    if (FLCN_OK != (flcnStatus = mutexRelease_hs(LW_MUTEX_ID_DISP_SELWRE_WR_SCRATCH, dispSelwreWrScratchMutexToken)))
    {
        CHECK_FLCN_STATUS(FLCN_ERR_HS_MUTEX_RELEASE_FAILED);
    }
    dispSelwreWrScratchMutexToken = LW_MUTEX_OWNER_ID_ILWALID;

    // Step6: Trigger the intr to DPU.
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PDISP_DSI_EVENT_FRC_HDCP, 
                      DRF_DEF(_PDISP, _DSI_EVENT_FRC_HDCP, _TYPE1_LOCK, _ASSERT)));

    // Step7: Poll for the completion from DPU
    ptimer0Start = REG_RD32(CSB, LW_CSEC_FALCON_PTIMER0);
    do
    {
        // Check the RESPONSE value set by DPU, then return proper status code
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PDISP_SELWRE_WR_SCRATCH_0, &data32));
        if (!FLD_TEST_DRF(_PDISP, _SELWRE_WR_SCRATCH_0, _VPR_HDCP22_TYPE1_LOCK_RESPONSE, _AWAITING_RESPONSE, data32))
        {
            if ((bLock) && (FLD_TEST_DRF(_PDISP, _SELWRE_WR_SCRATCH_0, _VPR_HDCP22_TYPE1_LOCK_RESPONSE, _SUCCESS_TYPE1_ENGAGED, data32)))
            {
                flcnStatus = FLCN_OK;
            }
            else if ((!bLock) && (FLD_TEST_DRF(_PDISP, _SELWRE_WR_SCRATCH_0, _VPR_HDCP22_TYPE1_LOCK_RESPONSE, _SUCCESS_TYPE1_OFF, data32)))
            {
                flcnStatus = FLCN_OK;
            }
            else if (FLD_TEST_DRF(_PDISP, _SELWRE_WR_SCRATCH_0, _VPR_HDCP22_TYPE1_LOCK_RESPONSE, _FAILED, data32))
            {
                flcnStatus = FLCN_ERR_HDCP_TYPE1_LOCK_FAILED;
            }
            else
            {
                flcnStatus = FLCN_ERR_HDCP_TYPE1_LOCK_UNKNOWN;
            }

            CHECK_FLCN_STATUS(flcnStatus);
            break;
        }

        // Check if the timeout is reached, then return timeout error
        ptimer0Lwrrent = REG_RD32(CSB, LW_CSEC_FALCON_PTIMER0);
        if ((ptimer0Lwrrent - ptimer0Start) > DPU_TYPE1_LOCK_RESPONSE_TIMEOUT_VALUE_NS)
        {
            CHECK_FLCN_STATUS(FLCN_ERR_DPU_TIMEOUT_FOR_HDCP_TYPE1_LOCK_REQUEST);
        }
    } while (LW_TRUE);

    // Step8: Also save the lock status to BSI secure scratch so that ACR can help restore the setting at GC6
    if (FLCN_OK != (flcnStatus = mutexAcquire_hs(LW_MUTEX_ID_VPR_SCRATCH, VPR_MUTEX_ACQUIRE_TIMEOUT_NS, &vprSelwreScratchMutexToken)))
    {
        CHECK_FLCN_STATUS(FLCN_ERR_HS_MUTEX_ACQUIRE_FAILED);
    }

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14, &data32));
    if (bLock)
    {
        data32 = FLD_SET_DRF(_PGC6_BSI, _VPR_SELWRE_SCRATCH_14, _HDCP22_TYPE1_LOCK, _ENABLE, data32);
    }
    else
    {
        data32 = FLD_SET_DRF(_PGC6_BSI, _VPR_SELWRE_SCRATCH_14, _HDCP22_TYPE1_LOCK, _DISABLE, data32);
    }
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14, data32));

ErrorExit:
    if (vprSelwreScratchMutexToken != LW_MUTEX_OWNER_ID_ILWALID)
    {
        if (FLCN_OK != (cleanupStatus1 = mutexRelease_hs(LW_MUTEX_ID_VPR_SCRATCH, vprSelwreScratchMutexToken)))
        {
            cleanupStatus1 = FLCN_ERR_HS_MUTEX_RELEASE_FAILED;
        }
    }

    if (dispSelwreWrScratchMutexToken == LW_MUTEX_OWNER_ID_ILWALID)
    {
        // Acquire level 2 Display scratch mutex for access to LW_PDISP_SELWRE_WR_SCRATCH_0 (WR PLM = 2)
        if (FLCN_OK == (cleanupStatus2 = mutexAcquire_hs(LW_MUTEX_ID_DISP_SELWRE_WR_SCRATCH, VPR_MUTEX_ACQUIRE_TIMEOUT_NS, &dispSelwreWrScratchMutexToken)))
        {
            // Reset the RESPONSE value no matter the result is pass or fail
            if (FLCN_OK == (cleanupStatus2 = BAR0_REG_RD32_HS_ERRCHK(LW_PDISP_SELWRE_WR_SCRATCH_0, &data32)))
            {
                data32 = FLD_SET_DRF(_PDISP, _SELWRE_WR_SCRATCH_0, _VPR_HDCP22_TYPE1_LOCK_RESPONSE, _RESET, data32);
                cleanupStatus2 = BAR0_REG_WR32_HS_ERRCHK(LW_PDISP_SELWRE_WR_SCRATCH_0, data32);
            }
        }
        else
        {
            cleanupStatus2 = FLCN_ERR_HS_MUTEX_ACQUIRE_FAILED;
        }

        if ((cleanupStatus2 != FLCN_OK) && (cleanupStatus1 == FLCN_OK))
        {
            cleanupStatus1 = cleanupStatus2;
        }
    }

    // Release level 2 Display scratch mutex if it's not released before exiting this function
    if (cleanupStatus1 != FLCN_ERR_HS_MUTEX_ACQUIRE_FAILED)
    {
        if (FLCN_OK != (cleanupStatus2 = mutexRelease_hs(LW_MUTEX_ID_DISP_SELWRE_WR_SCRATCH, dispSelwreWrScratchMutexToken)))
        {
            cleanupStatus2 = FLCN_ERR_HS_MUTEX_RELEASE_FAILED;
        }

        if ((cleanupStatus2 != FLCN_OK) && (cleanupStatus1 == FLCN_OK))
        {
            cleanupStatus1 = cleanupStatus2;
        }
    }

    // Release level 3 Display scratch mutex if it's not released before exiting this function
    if (dispSelwreScratchMutexToken != LW_MUTEX_OWNER_ID_ILWALID)
    {
        if (FLCN_OK != (cleanupStatus2 = mutexRelease_hs(LW_MUTEX_ID_DISP_SELWRE_SCRATCH, dispSelwreScratchMutexToken)))
        {
            cleanupStatus2 = FLCN_ERR_HS_MUTEX_RELEASE_FAILED;
        }

        if ((cleanupStatus2 != FLCN_OK) && (cleanupStatus1 == FLCN_OK))
        {
            cleanupStatus1 = cleanupStatus2;
        }
    }

    // Release the Playready generic mutex if it's not released before exiting this function
    if (playreadyMutexToken != LW_MUTEX_OWNER_ID_ILWALID)
    {
        if (FLCN_OK != (cleanupStatus2 = mutexRelease_hs(LW_MUTEX_ID_PLAYREADY, playreadyMutexToken)))
        {
            cleanupStatus2 = FLCN_ERR_HS_MUTEX_RELEASE_FAILED;
        }

        if ((cleanupStatus2 != FLCN_OK) && (cleanupStatus1 == FLCN_OK))
        {
            cleanupStatus1 = cleanupStatus2;
        }
    }

    if (flcnStatus == FLCN_OK)
    {
        flcnStatus = cleanupStatus1;
    }
    return flcnStatus;
}

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
sec2UpdateDispBlankingPolicyHS_GP10X
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

    // SLI and RWPR will always be blanked.
    alwaysEnabledPolicies = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_SLI, _ENABLE, alwaysEnabledPolicies);
    alwaysEnabledPolicies = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_RWPR, _ENABLE, alwaysEnabledPolicies);
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

    // Handle analog.
    if (FLD_TEST_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_ANALOG, _ENABLE, lwrPolicyInHw) ||
        FLD_TEST_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_ANALOG, _ENABLE, policyValForContext))
    {
        newPolicyForHw = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_ANALOG, _ENABLE, newPolicyForHw);
    }

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

    // Handle HDCP2.2 digital policy
    if (FLD_TEST_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_HDCP22, _ENABLE, lwrPolicyInHw) ||
        FLD_TEST_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_HDCP22, _ENABLE, policyValForContext))
    {
        newPolicyForHw = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_HDCP22, _ENABLE, newPolicyForHw);
    }

    // Handle internal panel policy
    if (FLD_TEST_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_INTERNAL, _ENABLE, lwrPolicyInHw) ||
        FLD_TEST_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_INTERNAL, _ENABLE, policyValForContext))
    {
        newPolicyForHw = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_INTERNAL, _ENABLE, newPolicyForHw);
    }

    // All ok so far. Time to program new policies in HW.
    CHECK_FLCN_STATUS(sec2ProgramDispBlankingPolicyToHwHS_HAL(&Sec2, newPolicyForHw));

    if (bType1LockingRequired)
    {
        //
        // If HDCP22 type1 locking is required, call into DPU to set stream type 1 and lock it.
        //
        // How SEC2 -> DPU communication works:
        // SEC2 doesn't maintain any state about HDCP2.2 type1.
        // Every time SEC2 sees HDCP2.2 type 1 GUID, it will send a command to DPU to lock type1.
        // In DPU, we maintain a boolean state on DISP scratch register for SEC2's type1 lock request.
        // Each time PR app (this code) on SEC2 calls DPU to "lock" to type 1, SEC2 will set that boolean state to true.
        // DPU will treat SEC2's lock with highest priority.
        // Once VPR size goes down to 0 in VPR app, SEC2 VPR app wipes blanking policies, writes 0 to the DPU communication scratch register and calls DPU to "unlock"
        // PR app (this code) will never call into DPU to unlock type1 lock.
        //
        if (FLCN_OK != (flcnStatus = sec2HdcpType1LockHs_HAL(&Sec2, LW_TRUE)))
        {
            //
            // Lock has failed. Revert blanking policy to what it was before this context's policies, then return error.
            // (Intentionally dropping the return code so that the error code of HDCP2.2 type1 lock will still be returned to caller)
            //
            (void)sec2ProgramDispBlankingPolicyToHwHS_HAL(&Sec2, lwrPolicyInHw);
            goto ErrorExit;
        }
    }

ErrorExit:
    return flcnStatus;
}

/* ------------------------- Private Functions ------------------------------ */
/*
 * This function determines the internal panel trust level based on LW_FUSE_OPT_MOBILE
 * and denylisted devids.  If the fuse is not blown (desktop sku) or devid is one of the 5
 * denylisted devids,  this function will return FALSE and VPR is not allowed
 * (_BLANK_VPR_ON_INTERNAL is set to _ENABLE)
 *
 * @param[in/out]  pbAllowVprScanout  The pointer points to the boolean buffer indicating
 *                                    whether internal panel is allowed.
 *
 * @return  FLCN_OK if the internal panel truse level is determined.
 *          FLCN_ERR_ILWALID_ARGUMENT if the input argument is invalid.
 *          FLCN_ERR_BAR0_PRIV_READ_ERROR is returned if priv read failed.
 */
FLCN_STATUS
sec2AllowVprScanoutToInternalHS_GP10X
(
    LwBool *pbAllowVprScanout
)
{
    LwU32       data32;
    FLCN_STATUS flcnStatus = FLCN_OK;

    if (pbAllowVprScanout == NULL)
    {
        CHECK_FLCN_STATUS(FLCN_ERR_ILWALID_ARGUMENT);
    }

    //
    // Set pbAllowVprScanout to FALSE in case we hit read errors,  this ensures
    // we return the correct value.  pbAllowVprScanout is only changed to TRUE
    // after all reads are complete.
    //
    *pbAllowVprScanout = LW_FALSE;

    //
    // If mobile fuse is not blown (desktop SKU) then internal panel is not possible.
    // Always return FALSE to disallow VPR scanout  when fuse is not blown,  ie,
    // PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL_BLANK_VPR_ON_INTERNAL will be ENABLED
    //
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_MOBILE, &data32));
    if (FLD_TEST_DRF(_FUSE, _OPT_MOBILE, _DATA, _DISABLE, data32))
    {
        goto ErrorExit;
    }

    // 
    // If the devid is one of the denylisted devids, then return FALSE
    // TODO: Pull in dev_lw_pcfg_xve.ref from HW to SW tree through ref2h
    //       so we don't have to explictly do base+offset
    //
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(DRF_BASE(LW_PCFG) + LW_XVE_ID, &data32));
    data32 = DRF_VAL(_XVE, _ID, _DEVICE_CHIP, data32);
    switch (data32)
    {
        //
        // The list of denylisted devids is dolwmented in below link.
        // https://docs.google.com/spreadsheets/d/1smG_wIuMXcktsVjhl0B1jHQTx_jISqXzXDMwDEX0uUI/edit#gid=0
        // The .xls also includes the following IDS: 0x1BB6, 0x1BB7, 0x1BB8.  These ids will be modified to 
        // move eDP to link D before production.
        //
        case LW_PCI_DEVID_DEVICE_GP104_N17_G3:
        case LW_PCI_DEVID_DEVICE_GP104_N17_G2:
        case LW_PCI_DEVID_DEVICE_GP104_N17_G2_2:
        case LW_PCI_DEVID_DEVICE_GP106_N17_G1:
        case LW_PCI_DEVID_DEVICE_GP106_N17_G1_2:
            //
            // LW Bug 1838077 - Check secure scratch bit to determine if constrained Lwflash load
            // was successful.  If the constrained LWFlash ucode did not successfully execute and indicate
            // success in secure scratch, then goto ErrorExit  (pbAllowVprScanout initialized to LW_FALSE)
            // else pbAllowVprScanout will be set to LW_TRUE below.
            //
            CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PGC6_BSI_PLAYREADY_SELWRE_SCRATCH_8, &data32));
            if (FLD_TEST_DRF(_PGC6_BSI, _PLAYREADY_SELWRE_SCRATCH_8, _PDUB_STATUS, _FAILURE, data32))
            {
                goto ErrorExit;
            }
            break;
        default:
            break;
    }

    // If haven't found a reason to return FALSE,  then allow VPR scanout
    *pbAllowVprScanout = LW_TRUE;

ErrorExit:
    return flcnStatus;
}

/*
 * HS helper function to update the blanking policy in DISP policy register and
 * BSI secure scratch register.
 *
 * @param[in]  policy  The policy to be written into secure scratch registers.
 *
 * @return
 * FLCN_ERR_HS_MUTEX_ACQUIRE_FAILED is returned if the mutex acquirement failed.
 * FLCN_ERR_HS_MUTEX_RELEASE_FAILED is returned if the mutex release failed.
 * FLCN_ERR_BAR0_PRIV_WRITE_ERROR is returned if we failed to update the value of scratch registers.
 */
FLCN_STATUS
sec2ProgramDispBlankingPolicyToHwHS_GP10X
(
    LwU32 policy
)
{
    FLCN_STATUS flcnStatus           = FLCN_OK;
    FLCN_STATUS tmpFlcnStatus        = FLCN_OK;
    LwU8        vprScratchMutexToken = LW_MUTEX_OWNER_ID_ILWALID;
    LwU32       vprInfo;
    LwU32       data32;

    flcnStatus = mutexAcquire_hs(LW_MUTEX_ID_VPR_SCRATCH, PR_MUTEX_ACQUIRE_TIMEOUT_NS, &vprScratchMutexToken);
    if (flcnStatus != FLCN_OK)
    {
        CHECK_FLCN_STATUS(FLCN_ERR_HS_MUTEX_ACQUIRE_FAILED);
    }

    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL, policy));

    // Also update policy value in BSI secure scratch.
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14, &vprInfo));

    // Ensure the bit positions of the blanking policy are matching for all bits between DISP register and BSI scratch register.
    ct_assert(
        DRF_BASE(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL_BLANK_VPR_ON_NO_HDCP) - DRF_BASE(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL_BLANKING_POLICIES) ==
        DRF_BASE(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_DISP_POLICY_BLANK_VPR_ON_NO_HDCP) - DRF_BASE(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_DISP_BLANKING_POLICIES)
        );
    ct_assert(
        DRF_BASE(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL_BLANK_VPR_ON_HDCP1X) - DRF_BASE(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL_BLANKING_POLICIES) ==
        DRF_BASE(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_DISP_POLICY_BLANK_VPR_ON_HDCP1X) - DRF_BASE(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_DISP_BLANKING_POLICIES)
        );
    ct_assert(
        DRF_BASE(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL_BLANK_VPR_ON_INTERNAL) - DRF_BASE(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL_BLANKING_POLICIES) ==
        DRF_BASE(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_DISP_POLICY_BLANK_VPR_ON_INTERNAL) - DRF_BASE(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_DISP_BLANKING_POLICIES)
        );
    ct_assert(
        DRF_BASE(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL_BLANK_VPR_ON_ANALOG) - DRF_BASE(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL_BLANKING_POLICIES) ==
        DRF_BASE(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_DISP_POLICY_BLANK_VPR_ON_ANALOG) - DRF_BASE(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_DISP_BLANKING_POLICIES)
        );
    ct_assert(
        DRF_BASE(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL_BLANK_VPR_ON_SLI) - DRF_BASE(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL_BLANKING_POLICIES) ==
        DRF_BASE(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_DISP_POLICY_BLANK_VPR_ON_SLI) - DRF_BASE(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_DISP_BLANKING_POLICIES)
        );
    ct_assert(
        DRF_BASE(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL_BLANK_VPR_ON_HDCP22) - DRF_BASE(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL_BLANKING_POLICIES) ==
        DRF_BASE(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_DISP_POLICY_BLANK_VPR_ON_HDCP22) - DRF_BASE(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_DISP_BLANKING_POLICIES)
        );
    ct_assert(
        DRF_BASE(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL_BLANK_RWPR) - DRF_BASE(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL_BLANKING_POLICIES) ==
        DRF_BASE(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_DISP_POLICY_BLANK_RWPR) - DRF_BASE(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_DISP_BLANKING_POLICIES)
        );

    vprInfo = FLD_SET_DRF_NUM(_PGC6_BSI, _VPR_SELWRE_SCRATCH_14, _DISP_BLANKING_POLICIES,
        DRF_VAL(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANKING_POLICIES, policy), vprInfo);

    // Update BSI secure scratch with new value.
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14, vprInfo));

    // Error handling if the above write to BSI scratch gets dropped due to any hacking attempt
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14, &data32));
    if (data32 != vprInfo)
    {
        // The write to the BSI secure scratch register didn't go through. Cleanup and Fail.
        CHECK_FLCN_STATUS(FLCN_ERR_BAR0_PRIV_WRITE_ERROR);
    }

ErrorExit:
    if (vprScratchMutexToken != LW_MUTEX_OWNER_ID_ILWALID)
    {
        tmpFlcnStatus = mutexRelease_hs(LW_MUTEX_ID_VPR_SCRATCH, vprScratchMutexToken);
        if ((flcnStatus == FLCN_OK) && (tmpFlcnStatus != FLCN_OK))
        {
            flcnStatus = FLCN_ERR_HS_MUTEX_RELEASE_FAILED;
        }
    }

    return flcnStatus;
}

