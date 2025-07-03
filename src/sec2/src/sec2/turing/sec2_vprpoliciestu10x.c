/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_vprpoliciestu10x.c
 *
 * This file includes implementation of various vpr policy functions used by PR
 * and VPR app on TU10X
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "sec2_hs.h"
#include "dev_disp.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*
 * HS helper function to update the blanking policy in DISP policy register and
 * BSI secure scratch register.
 *
 * @param[in]  policy  The policy to be written into secure scratch registers.
 *
 * @return
 * FLCN_ERR_BAR0_PRIV_WRITE_ERROR is returned if we failed to update the value of scratch registers.
 */
FLCN_STATUS
sec2ProgramDispBlankingPolicyToHwHS_TU10X
(
    LwU32 policy
)
{
    FLCN_STATUS flcnStatus           = FLCN_OK;
    LwU32       aonScratch;

    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL, policy));

    //
    // Update BSI secure scratch with new value.
    // Mutex is not required for this, since the saving is done by PR app running on SEC2, and
    // the other agent reading this scratch would be ACR BSI Lock binary, which also can only run on
    // SEC2 and only as part of GC6 hence these 2 entities cannot run together. Also the update to
    // the scratch is atomic and not a RMW and only by a single entity (PR app) so there is never a
    // possibility of 2 entities writing this together and messing up the value.
    //
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PGC6_AON_VPR_SELWRE_SCRATCH_VPR_BLANKING_POLICY_CTRL, policy));

    // Error handling if the above write to BSI scratch gets dropped due to any hacking attempt
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PGC6_AON_VPR_SELWRE_SCRATCH_VPR_BLANKING_POLICY_CTRL, &aonScratch));
    if (aonScratch != policy)
    {
        // The write to the BSI secure scratch register didn't go through
        CHECK_FLCN_STATUS(FLCN_ERR_BAR0_PRIV_WRITE_ERROR);
    }

ErrorExit:
    return flcnStatus;
}

/*
 * This function determines the internal panel trust level based on LW_FUSE_OPT_NOTEBOOK.
 * If the fuse is not blown (desktop sku),  this function will return FALSE and VPR is not allowed
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
sec2AllowVprScanoutToInternalHS_TU10X
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
    // If NOTEBOOK fuse is not blown (desktop SKU) then internal panel is not possible.
    // Always return FALSE to disallow VPR scanout  when fuse is not blown,  ie,
    // PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL_BLANK_VPR_ON_INTERNAL will be ENABLED
    //
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_NOTEBOOK, &data32));
    if (FLD_TEST_DRF(_FUSE, _OPT_NOTEBOOK, _DATA, _DISABLE, data32))
    {
        goto ErrorExit;
    }

    // If haven't found a reason to return FALSE,  then allow VPR scanout
    *pbAllowVprScanout = LW_TRUE;

ErrorExit:
    return flcnStatus;
}
/* ------------------------- Private Functions ------------------------------ */
