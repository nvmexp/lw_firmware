/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2016-2017 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

/*!
* @file   dpu_dpu0207.c
* @brief  DPU 02.07 Hal Functions
*
* DPU HAL functions implement chip-specific initialization, helper functions
*/

/* ------------------------- System Includes -------------------------------- */
#include "dpusw.h"
#include "dev_disp.h"
#include "dev_disp_addendum.h"
#include "dpu_objdpu.h"
#include "dispflcn_regmacros.h"
#include "lib_intfchdcp_cmn.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_dpu_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
extern LwU32 gHdcpOverlayMapping[HDCP_OVERLAYID_MAX];

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */

/*!
 * @brief Early initialization for DPU 02.07
 *
 * Any general initialization code not specific to particular engines should be
 * done here. This function must be called prior to starting the scheduler.
 */
FLCN_STATUS
dpuInit_v02_07(void)
{
    LwU32 plmVal           = 0;
    LwU32 lwrUcodeLevelCSB = 0;
    LwU32 csbLvlMask       = 0;

    //
    // To derive the lwrUcodeLevel, we first need to get the
    // max possible ucode level of the falcon from the SCTL register.
    //
    lwrUcodeLevelCSB = DFLCN_DRF_VAL(SCTL, _UCODE_LEVEL,
                               DFLCN_REG_RD32(SCTL));

    csbLvlMask = DFLCN_DRF_VAL(SCTL1, _CSBLVL_MASK,
                               DFLCN_REG_RD32(SCTL1));

    // Final level to be used for CSB writes will be an AND of the max ucode level and CSBLVL_MASK.
    lwrUcodeLevelCSB &= csbLvlMask;

    if (1 == lwrUcodeLevelCSB)
    {
        //
        // DPU ucode should never be running at level 1.
        // It normally runs at level2. Or it can be run at level0 (NS mode) when we disable ACR completely.
        // However it never runs at level1. If we catch this condition then do a hard halt here.
        // Otherwise, the code below will fail to run since it changes PLM permissions for level 2 which
        // can't be done by level 1 SW.
        // If DPU is ever moved to run at level 1, then the code below needs to be updated.
        // While it's possible to detect level to be 1 and do the right thing instead of halting, it will be
        // a waste of IMEM space for now, so let's skip coding up below code for level 1 for now.
        //
        DPU_HALT();
    }
    else if (0 != lwrUcodeLevelCSB)
    {
        //
        // Special note about LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_PRIV_LEVEL_MASK:
        // We have a fuse "opt_selwre_hdcp_vpr_policy_wr_selwre" to control the hw
        // reset value of the PLM. Not blowing the fuse leaves the PLM allowing write
        // access to all priv levels. If we prevent level 0 and 2 from being able to
        // write to LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL via the code below, SKUs
        // where the fuse is unblown will not longer benefit from the fuse setting.
        // On GP10X, this fuse is not blown on debug key boards and possibly ones
        // created on special requests. So, it is possible, we will have to provide
        // custom DPU ucode for such cases.
        //
        // Another way folks could change the default PLM setting for this PLM is by
        // requesting HULK and thus they get write access to this register. As of
        // 12/20/2016, we are not aware of existence of any such HULK request for
        // GP10X. So, likely, we do not need to be worried about this scenario.
        //
        // To satisfy the first need (special SKU) we could try to read the fuse value
        // and skip the code below if the fuse is not blown. Unfortunately, DPU does
        // not have access to fuse registers. So, this is not possible. As an
        // alternative DPU code could look at the PLM value coming into this function
        // but that raises the security risk (say if someone managed to exploit a
        // vulnerability in PMU to downgrade the PLM).
        //
        // To satisfy the second need (HULK) the only way would be to check the PLM as
        // described above.
        //
        // The current plan is to just "wait and watch" i.e. if a need comes up
        // we will deal with it at that time. For now we want to keep this area as
        // secure as possible and also avoid code complexity.
        //


        //
        // This PLM: LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_PRIV_LEVEL_MASK defaults to level 2+ write access on GP102/4/6,
        // but we need it to be level3+ writable only. (This is already level3+ writable on GP107/8, it was fixed by an ECO)
        // This PLM maps to the register which controls VPR display blanking policies.
        //
        plmVal = EXT_REG_RD32(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_PRIV_LEVEL_MASK);
        plmVal = FLD_SET_DRF(_PDISP, _UPSTREAM_HDCP_VPR_POLICY_PRIV_LEVEL_MASK,
                             _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
        plmVal = FLD_SET_DRF(_PDISP, _UPSTREAM_HDCP_VPR_POLICY_PRIV_LEVEL_MASK,
                             _WRITE_PROTECTION_LEVEL2, _DISABLE, plmVal);
        EXT_REG_WR32(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_PRIV_LEVEL_MASK, plmVal);

        //
        // This PLM: LW_PDISP_CLK_REM_LINK_PRIV_LEVEL_MASK defaults to level 2+ write access on GP102/4/6,
        // but we need it to be level3+ writable only.
        // This PLM maps to the register which marks various padlinks as internal or external.
        // The padlinks in this register marked as internal are used as an input to determine whether the
        // panel connected to that link is internal or not.
        //
        plmVal = EXT_REG_RD32(LW_PDISP_CLK_REM_LINK_PRIV_LEVEL_MASK);
        plmVal = FLD_SET_DRF(_PDISP, _CLK_REM_LINK_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0,
                             _DISABLE, plmVal);
        plmVal = FLD_SET_DRF(_PDISP, _CLK_REM_LINK_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2,
                             _DISABLE, plmVal);
        EXT_REG_WR32(LW_PDISP_CLK_REM_LINK_PRIV_LEVEL_MASK, plmVal);

        //
        // This PLM: LW_PDISP_SELWRE_SCRATCH_PRIV_LEVEL_MASK defaults to level 2+ write access,
        // but we need it to be level3+ writable only.
        // This PLM controls access to LW_PDISP_SELWRE_SCRATCH(i), i = 0/1/2/3.
        // LW_PDISP_SELWRE_SCRATCH(0) bit [0:0] is used to store the HDCP2.2 Type1 Lock status requested by SEC2,
        // which is level3 data, hence we need to make this register to level3 writable only.
        //
        plmVal = EXT_REG_RD32(LW_PDISP_SELWRE_SCRATCH_PRIV_LEVEL_MASK);
        plmVal = FLD_SET_DRF(_PDISP, _SELWRE_SCRATCH_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0,
                             _DISABLE, plmVal);
        plmVal = FLD_SET_DRF(_PDISP, _SELWRE_SCRATCH_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2,
                             _DISABLE, plmVal);
        EXT_REG_WR32(LW_PDISP_SELWRE_SCRATCH_PRIV_LEVEL_MASK, plmVal);
    }

    //
    // After we are done with GP100+ specific stuff, call into the GM20X version dpuInit_v02_05
    // to do common initialization which is common between GP10X and GM20X.
    // Usually the dpuInit_v02_05 should be called before any GP100+ specific code above exelwtes. But the parent
    // InitDpuApp does not handle the error code returned from here and proceeds to boot further. So if any error
    // happens in dpuInit_v02_05, we would not execute the code above, and the DPU will still continue to boot.
    // And we want the above code to be exelwted in all cases. So let's first execute the code above,
    // and then call dpuInit_v02_05.
    //
    return dpuInit_v02_05();
}

/*!
 * Enable level 3 protection on DMEM to avoid sensitive data leaking. Note that
 * some DPU registers are also protected by DMEM PLM, so we need to ensure they
 * are all configured before this function is called, otherwise we will fail to
 * access those registers since DPU is only running at level 2.
 */
void
dpuRaiseDmemPlmToLevel3_v02_07(void)
{
    LwU32 plmVal;

    plmVal = REG_RD32(CSB, LW_PDISP_FALCON_DMEM_PRIV_LEVEL_MASK);
    plmVal = FLD_SET_DRF(_PDISP_FALCON, _DMEM_PRIV_LEVEL_MASK, _WRITE_PROTECTION,
                         _ALL_LEVELS_DISABLED, plmVal);
    plmVal = FLD_SET_DRF(_PDISP_FALCON, _DMEM_PRIV_LEVEL_MASK, _READ_PROTECTION,
                         _ALL_LEVELS_DISABLED, plmVal);
    REG_WR32(CSB, LW_PDISP_FALCON_DMEM_PRIV_LEVEL_MASK, plmVal);
}

/*!
 * @brief Routine to initialize HDCP1X/HDCP22 common part.
 */
void
dpuHdcpCmnInit_v02_07(void)
{
    dpuHdcpCmnInit_v02_05();

    gHdcpOverlayMapping[HDCP22WIRED_LIBI2CSW]   = OVL_INDEX_IMEM(libI2cSw); // v0207 supports I2CSW.
}
