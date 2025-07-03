/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_helper_functions_tu10x_only.c  
 */
#include "acr.h"
#include "dev_fb.h"
#include "dev_timer.h"
#include "dev_gc6_island.h"
#include "dev_pri_ringstation_sys_addendum.h"

#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"

/*
 * @brief: Upgrade PLM of PTIMER register during ACR load and Restore it to Default during unload 
 */
ACR_STATUS
acrProtectHostTimerRegisters_TU10X(LwBool bProtect)
{
    LwU32 plm = ACR_REG_RD32(BAR0, LW_PTIMER_TIME_PRIV_LEVEL_MASK);

    if(bProtect)
    {
        // During ACR load set plm so only level 3 falcon can access.
        plm = FLD_SET_DRF(_PTIMER, _TIME_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plm);
        plm = FLD_SET_DRF(_PTIMER, _TIME_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plm);
        plm = FLD_SET_DRF(_PTIMER, _TIME_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _DISABLE, plm);
        plm = FLD_SET_DRF(_PTIMER, _TIME_PRIV_LEVEL_MASK, _WRITE_VIOLATION, _REPORT_ERROR, plm);
    }
    else
    {
        // During ACR unload set plm to default value.
        plm = FLD_SET_DRF(_PTIMER, _TIME_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _DEFAULT_PRIV_LEVEL, plm);
        plm = FLD_SET_DRF(_PTIMER, _TIME_PRIV_LEVEL_MASK, _WRITE_VIOLATION, _REPORT_ERROR, plm);
    }

    ACR_REG_WR32(BAR0, LW_PTIMER_TIME_PRIV_LEVEL_MASK, plm);
    return ACR_OK;

}

/*!
 * Check if WAR is required for incorrect HW init value of falcon SubWprs, Bug 200444247 has details
 * This check is required because we need to disable all unprogrammed subWprs in acrDisableAllFalconSubWprs_TU10X
 */
LwBool
acrCheckIfWARRequiredForBadHwInitOfSubWPR_TU10X
(
    PACR_FLCN_CONFIG pFlcnCfg,
    LwU32            flcnSubWprId
)
{
    LwU32 regCfga = ACR_REG_RD32(BAR0, (pFlcnCfg->subWprRangeAddrBase + (FALCON_SUB_WPR_INDEX_REGISTER_STRIDE * flcnSubWprId)));
    LwU32 regCfgb = ACR_REG_RD32(BAR0, (pFlcnCfg->subWprRangeAddrBase + (FALCON_SUB_WPR_INDEX_REGISTER_STRIDE * flcnSubWprId)
                                                                      + FALCON_SUB_WPR_CONFIG_REGISTER_OFFSET_DIFF));

    LwU32 startAddr = DRF_VAL( _PFB, _PRI_MMU_FALCON_PMU_CFGA, _ADDR_LO, regCfga);
    LwU32 endAddr   = DRF_VAL( _PFB, _PRI_MMU_FALCON_PMU_CFGB, _ADDR_HI, regCfgb);

    //
    // Turing HW has a bug 200444247 with HW init value of subWPR LO/HI.
    // Those registers init to 0 which means 4K of subWPR. In code below,
    // we are exploiting that property to determine whether a subWPR has been setup.
    // WARNING: If anyone actually used a subWPR of 4KB in size for any use,
    // this check will flag that as an unused WPR leading to unexpected behaviour
    // but this is the best we can do for Turing. Note that we are not checking startAddr = endAddr.
    // So, only 1 region can run into this problem
    //
    if ((startAddr == 0) && (endAddr == 0))
    {
        return LW_TRUE;
    }
    return LW_FALSE;
}

/*!
 * Temporary Change : Allow level2 FECS to write to SELWRE_SCRATCH_GROUP_15
 * This is required for FECS ucode to access this registers for ramchain scrub sequence
 * This PLM update is also done in VBIOS, but to avoid revlock and till everyone moves to TOT VBIOS,
 * we are updating this PLM in ACR as well.
 * TODO: Delete this function, once everyone moves to TOT vbios
 */
ACR_STATUS
acrAllowFecsToUpdateSelwreScratchGroup15_TU10X(void)
{
    LwU32 plm = ACR_REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK);

    if ((DRF_VAL( _PGC6, _AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK, _SOURCE_ENABLE, plm) & BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_SEC2))
            != BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_SEC2))
    {
        //
        // Latest vbios is used which has already lowered PLM
        // Early return is required because AHESASC which runs on SEC2 is not in the source list of this updated PLM
        //
        return ACR_OK;
    }

    plm = FLD_SET_DRF( _PGC6, _AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK, _READ_PROTECTION,         _ALL_LEVELS_ENABLED, plm );
    plm = FLD_SET_DRF( _PGC6, _AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _ENABLE,             plm );
    plm = FLD_SET_DRF( _PGC6, _AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL,     _LOWERED,            plm );

    plm = FLD_SET_DRF_NUM( _PGC6, _AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK, _SOURCE_ENABLE, BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_FECS), plm);

    ACR_REG_WR32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK, plm);

    return ACR_OK;
}

