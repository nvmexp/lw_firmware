 /* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_vprgv100.c
 * @brief  VPR HAL Functions for GV100
 *
 * VPR HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "flcntypes.h"
#include "sec2sw.h"
#include "dev_disp.h"
#include "dev_fb.h"
#include "dev_master.h"
#include "dev_gsp.h"
#include "sec2_bar0_hs.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_objvpr.h"
#include "config/g_vpr_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/* ------------------------- Private Functions ------------------------------ */


/*!
 * @brief Clear display blanking policies.
 */
FLCN_STATUS
vprClearDisplayBlankingPolicies_GV100(void)
{
    // TODO first call into DPU and unlock type1

    LwU32       lwrHwPolicy;
    FLCN_STATUS flcnStatus = FLCN_ERROR;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL, &lwrHwPolicy));

    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_NO_HDCP,      _DISABLE, lwrHwPolicy);
    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_HDCP1X,       _DISABLE, lwrHwPolicy);
    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_INTERNAL,     _DISABLE, lwrHwPolicy);
    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_SLI,          _ENABLE,  lwrHwPolicy); // SLI  banking is always enabled, we do the same in VPR app.
    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_HDCP22_TYPE0, _DISABLE, lwrHwPolicy);
    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_HDCP22_TYPE1, _DISABLE, lwrHwPolicy);
    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _CHANGE_EFFECTIVE,          _AT_NEXT_VBLANK, lwrHwPolicy);

    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL, lwrHwPolicy));

    flcnStatus = FLCN_OK;
ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Check if DISP falcon is in LS mode
 */
FLCN_STATUS
vprCheckIfDispFalconIsInLSMode_GV100(void)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_VPR_APP_DISP_FALCON_IS_NOT_IN_LS_MODE;
    LwU32       flcnSctl   = DRF_DEF(_PGSP, _FALCON_SCTL, _LSMODE, _FALSE);

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PGSP_FALCON_SCTL, &flcnSctl));
    if (FLD_TEST_DRF( _PGSP, _FALCON_SCTL, _LSMODE, _TRUE, flcnSctl))
    {
        flcnStatus = FLCN_OK;
    }

ErrorExit:
    return flcnStatus;
}

