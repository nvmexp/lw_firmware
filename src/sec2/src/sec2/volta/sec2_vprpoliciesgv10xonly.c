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
 * @file   sec2_vprpoliciesgv10xonly.c
 *
 * This file includes implementation of various vpr policy functions used by PR
 * and VPR app on GV10X
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "sec2_hs.h"
#include "dev_disp.h"
#include "dev_disp_addendum.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib_mutex.h"
#include <oemcommonlw.h>

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
 * FLCN_ERR_HS_MUTEX_ACQUIRE_FAILED is returned if the mutex acquirement failed.
 * FLCN_ERR_HS_MUTEX_RELEASE_FAILED is returned if the mutex release failed.
 * FLCN_ERR_BAR0_PRIV_WRITE_ERROR is returned if we failed to update the value of scratch registers.
 */
FLCN_STATUS
sec2ProgramDispBlankingPolicyToHwHS_GV10X
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
        DRF_BASE(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL_BLANK_VPR_ON_SLI) - DRF_BASE(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL_BLANKING_POLICIES) ==
        DRF_BASE(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_DISP_POLICY_BLANK_VPR_ON_SLI) - DRF_BASE(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_DISP_BLANKING_POLICIES)
        );
    ct_assert(
        DRF_BASE(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL_BLANK_VPR_ON_HDCP22_TYPE0) - DRF_BASE(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL_BLANKING_POLICIES) ==
        DRF_BASE(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_DISP_POLICY_BLANK_VPR_ON_HDCP22_TYPE0) - DRF_BASE(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_DISP_BLANKING_POLICIES)
        );
    ct_assert(
        DRF_BASE(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL_BLANK_VPR_ON_HDCP22_TYPE1) - DRF_BASE(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL_BLANKING_POLICIES) ==
        DRF_BASE(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_DISP_POLICY_BLANK_VPR_ON_HDCP22_TYPE1) - DRF_BASE(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_DISP_BLANKING_POLICIES)
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

/* ------------------------- Private Functions ------------------------------ */
