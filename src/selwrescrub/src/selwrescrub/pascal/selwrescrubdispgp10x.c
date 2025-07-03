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
 * @file: selwrescrubdispgp10x.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_disp.h"
#include "dev_disp_addendum.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "selwrescrub.h"
#include "selwrescrubreg.h"
#include "sec2mutexreservation.h"
// #include "config/g_selwrescrub_private.h"
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
selwrescrubClearVprBlankingPolicy_GP10X(void)
{
    LwU32 lwrHwPolicy = FALC_REG_RD32(BAR0, LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL);

    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_NO_HDCP,   _DISABLE, lwrHwPolicy);
    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_HDCP1X,    _DISABLE, lwrHwPolicy);
    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_HDCP22,    _DISABLE, lwrHwPolicy);
    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_INTERNAL,  _DISABLE, lwrHwPolicy);
    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_ANALOG,    _DISABLE, lwrHwPolicy);
    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_SLI,       _ENABLE,  lwrHwPolicy); // SLI  banking is always enabled, we do the same in VPR app.
    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_RWPR,             _ENABLE,  lwrHwPolicy); // RWPR banking is always enabled, we do the same in VPR app.

    FALC_REG_WR32(BAR0, LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL, lwrHwPolicy);
}

/*!
 * @brief: Enable Display CRC
 */
void
selwrescrubEnableDisplayCRC_GP10X(void)
{
    LwU32 crcReg = FALC_REG_RD32(BAR0, LW_PDISP_UPSTREAM_CRC);

    FALC_REG_WR32(BAR0, LW_PDISP_UPSTREAM_CRC, FLD_SET_DRF(_PDISP, _UPSTREAM_CRC, _ACCESS, _ENABLE, crcReg));
}


/*!
 * @brief: Release the HDCP2.2 type1 lock state in DISP secure scratch register
 */
SELWRESCRUB_RC
selwrescrubReleaseType1LockInDispScratch_GP10X(void)
{
    SELWRESCRUB_RC  status              = SELWRESCRUB_RC_OK;
    FLCNMUTEX       dispScratch0Mutex   = {SEC2_MUTEX_PDISP_SELWRE_SCRATCH_0, 0, LW_FALSE};

    // Acquire mutex before updating PDISP_SELWRE_SCRATCH_0
    if (SELWRESCRUB_RC_OK != (status = selwrescrubAcquireSelwreMutex_HAL(pSelwrescrub, &dispScratch0Mutex)))
    {
        return status;
    }

    // RMW operation to update the TYPE1_LOCK flag
    FLD_WR_DRF_DEF(BAR0, _PDISP, _SELWRE_SCRATCH_0, _VPR_HDCP22_TYPE1_LOCK, _DISABLE);

    if (LW_TRUE == dispScratch0Mutex.bValid)
    {
        status = selwrescrubReleaseSelwreMutex_HAL(pSelwrescrub, &dispScratch0Mutex);
    }

    return (status);
}

/*!
 * @brief: Release the HDCP2.2 type1 lock state in BSI secure scratch register
 */
void
selwrescrubReleaseType1LockInBsiScratch_GP10X(void)
{
    //
    // We are expecting here that the SEC2_MUTEX_VPR_SCRATCH mutex has been acquired by the caller
    // of this function so we can directly go and do RMW on LW_PGC6_BSI_VPR_SELWRE_SCRATCH_* registers.
    //
    FLD_WR_DRF_DEF(BAR0, _PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _HDCP22_TYPE1_LOCK, _DISABLE);
}

/*!
 * @brief: Upgrade write protection of PDISP_SELWRE_SCRATCH(i) registers by changing their PLM value.
 */
SELWRESCRUB_RC
selwrescrubUpgradeDispSelwreScratchWriteProtection_GP10X(void)
{
    // RMW operation to update the WRITE_PROTECTION field in the PLM of PDISP_SELWRE_SCRATCH(0)
    FLD_WR_DRF_DEF(BAR0, _PDISP, _SELWRE_SCRATCH_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ALL_LEVELS_DISABLED);

    return SELWRESCRUB_RC_OK;
}
