/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: selwrescrubdispgv10x.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_disp.h"
#include "dev_disp_addendum.h"
#include "selwrescrub.h"
#include "selwrescrubreg.h"
#include "config/g_selwrescrub_private.h"
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief: Clear VPR display blanking policy
 */
void
selwrescrubClearVprBlankingPolicy_GV10X(void)
{
    LwU32 lwrHwPolicy = FALC_REG_RD32(BAR0, LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL);

    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_NO_HDCP,        _DISABLE, lwrHwPolicy);
    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_HDCP1X,         _DISABLE, lwrHwPolicy);
    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_HDCP22_TYPE0,   _DISABLE, lwrHwPolicy);
    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_HDCP22_TYPE1,   _DISABLE, lwrHwPolicy);
    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_INTERNAL,       _DISABLE, lwrHwPolicy);
    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_SLI,            _ENABLE,  lwrHwPolicy); // SLI  banking is always enabled, we do the same in VPR app.

    FALC_REG_WR32(BAR0, LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL, lwrHwPolicy);
}
