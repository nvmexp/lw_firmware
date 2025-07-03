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
 * @file: acr_war_functions_tu1xx_only.c  
 */
#include "acr.h"
#include "dev_hshub.h"
#include "dev_fb.h"
#include "dev_host.h"
#include "dev_fuse.h"

#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"

#if defined(AHESASC) || defined(ASB) || defined(BSI_LOCK)
#include "dev_sec_csb.h"
#elif defined(ACR_UNLOAD)
#include "dev_pwr_csb.h"
#endif

/*!
 * @brief ACR routine to set priv level mask for LTCS/HSHUB config registers
 */
ACR_STATUS
acrProtectLwlinkRegs_TU10X(LwBool bProtect)
{
    // Read the priv level mask for HSHUB config regs
    LwU32 hshubCfgMask  = ACR_REG_RD32(BAR0, LW_PFB_HSHUB_LWL_CFG_PRIV_LEVEL_MASK);

    // Read the priv level masks for LTCS regs
    LwU32 fbhubLtcsMask = ACR_REG_RD32(BAR0, LW_PFB_NISO_AMAP_PRIV_LEVEL_MASK);
    LwU32 hshubLtcsMask = ACR_REG_RD32(BAR0, LW_PFB_HSHUB_LTC_CFG_PRIV_LEVEL_MASK);

    if (bProtect)
    {
        // Allow writes from priv level 2 only

        hshubCfgMask  = FLD_SET_DRF(_PFB_HSHUB, _LWL_CFG_PRIV_LEVEL_MASK,
                            _WRITE_PROTECTION_LEVEL0, _DISABLE, hshubCfgMask);
        hshubCfgMask  = FLD_SET_DRF(_PFB_HSHUB, _LWL_CFG_PRIV_LEVEL_MASK,
                            _WRITE_PROTECTION_LEVEL1, _DISABLE, hshubCfgMask);
        hshubCfgMask  = FLD_SET_DRF(_PFB_HSHUB, _LWL_CFG_PRIV_LEVEL_MASK,
                            _WRITE_PROTECTION_LEVEL2, _ENABLE,  hshubCfgMask);
        hshubCfgMask  = FLD_SET_DRF(_PFB_HSHUB, _LWL_CFG_PRIV_LEVEL_MASK,
                            _WRITE_VIOLATION, _REPORT_ERROR,    hshubCfgMask);

        fbhubLtcsMask = FLD_SET_DRF(_PFB_NISO, _AMAP_PRIV_LEVEL_MASK,
                            _WRITE_PROTECTION_LEVEL0, _DISABLE, fbhubLtcsMask);
        fbhubLtcsMask = FLD_SET_DRF(_PFB_NISO, _AMAP_PRIV_LEVEL_MASK,
                            _WRITE_PROTECTION_LEVEL1, _DISABLE, fbhubLtcsMask);
        fbhubLtcsMask = FLD_SET_DRF(_PFB_NISO, _AMAP_PRIV_LEVEL_MASK,
                            _WRITE_PROTECTION_LEVEL2, _ENABLE,  fbhubLtcsMask);
        fbhubLtcsMask = FLD_SET_DRF(_PFB_NISO, _AMAP_PRIV_LEVEL_MASK,
                            _WRITE_VIOLATION, _REPORT_ERROR,    fbhubLtcsMask);

        hshubLtcsMask = FLD_SET_DRF(_PFB_HSHUB, _LTC_CFG_PRIV_LEVEL_MASK,
                            _WRITE_PROTECTION_LEVEL0, _DISABLE, hshubLtcsMask);
        hshubLtcsMask = FLD_SET_DRF(_PFB_HSHUB, _LTC_CFG_PRIV_LEVEL_MASK,
                            _WRITE_PROTECTION_LEVEL1, _DISABLE, hshubLtcsMask);
        hshubLtcsMask = FLD_SET_DRF(_PFB_HSHUB, _LTC_CFG_PRIV_LEVEL_MASK,
                            _WRITE_PROTECTION_LEVEL2, _ENABLE,  hshubLtcsMask);
        hshubLtcsMask = FLD_SET_DRF(_PFB_HSHUB, _LTC_CFG_PRIV_LEVEL_MASK,
                            _WRITE_VIOLATION, _REPORT_ERROR,    hshubLtcsMask);
    }
    else
    {
        // Allow writes from all levels

        hshubCfgMask  = FLD_SET_DRF(_PFB_HSHUB, _LWL_CFG_PRIV_LEVEL_MASK,
                            _WRITE_PROTECTION_LEVEL0, _ENABLE, hshubCfgMask);
        hshubCfgMask  = FLD_SET_DRF(_PFB_HSHUB, _LWL_CFG_PRIV_LEVEL_MASK,
                            _WRITE_PROTECTION_LEVEL1, _ENABLE, hshubCfgMask);
        hshubCfgMask  = FLD_SET_DRF(_PFB_HSHUB, _LWL_CFG_PRIV_LEVEL_MASK,
                            _WRITE_PROTECTION_LEVEL2, _ENABLE, hshubCfgMask);

        fbhubLtcsMask = FLD_SET_DRF(_PFB_NISO, _AMAP_PRIV_LEVEL_MASK,
                            _WRITE_PROTECTION_LEVEL0, _ENABLE, fbhubLtcsMask);
        fbhubLtcsMask = FLD_SET_DRF(_PFB_NISO, _AMAP_PRIV_LEVEL_MASK,
                            _WRITE_PROTECTION_LEVEL1, _ENABLE, fbhubLtcsMask);
        fbhubLtcsMask = FLD_SET_DRF(_PFB_NISO, _AMAP_PRIV_LEVEL_MASK,
                            _WRITE_PROTECTION_LEVEL2, _ENABLE, fbhubLtcsMask);

        hshubLtcsMask = FLD_SET_DRF(_PFB_HSHUB, _LTC_CFG_PRIV_LEVEL_MASK,
                            _WRITE_PROTECTION_LEVEL0, _ENABLE, hshubLtcsMask);
        hshubLtcsMask = FLD_SET_DRF(_PFB_HSHUB, _LTC_CFG_PRIV_LEVEL_MASK,
                            _WRITE_PROTECTION_LEVEL1, _ENABLE, hshubLtcsMask);
        hshubLtcsMask = FLD_SET_DRF(_PFB_HSHUB, _LTC_CFG_PRIV_LEVEL_MASK,
                            _WRITE_PROTECTION_LEVEL2, _ENABLE, hshubLtcsMask);
    }

    ACR_REG_WR32(BAR0, LW_PFB_HSHUB_LWL_CFG_PRIV_LEVEL_MASK, hshubCfgMask);
    ACR_REG_WR32(BAR0, LW_PFB_NISO_AMAP_PRIV_LEVEL_MASK, fbhubLtcsMask);
    ACR_REG_WR32(BAR0, LW_PFB_HSHUB_LTC_CFG_PRIV_LEVEL_MASK, hshubLtcsMask);

    return ACR_OK;
}

/* WAR to fix Turing security bug which can cause HW damage (HW bug 2400726, sw bug for this WAR - 2401005)
 * WARNING: This change is unrelated to ACR functionality, please do not update/reuse/copy elsewhere.
 * It is being put here only because we need a level 3 binary to update this register on cold boot (AHESASC) and GC6 exit (BSI_LOCK).
 * As it is security issue, we need to fix it as soon as possible in driver.
 * This WAR should ideally go in VBIOS, but we are doing it here because we cannot update vbios for TU10X/TU11X now since already in market.
 * Issue description:
 * Turing has ISM_ONLY JTAG mode which allows ISM registers on the JTAG chain to be accessed by non-secure SW.
 * This was supposed to provide access only to ISM registers which don't have confidential data and pose no security vulnerabilities.
 * But through this mode, register JTAG2ISM_LW_ISM_TSOSC_A_scanout is also accessible which is PRIV accessible and PLM protected.
 * This register access exploit could lead to physical damage.
 * So here we are changing JTAG mode from ISM_ONLY (HW default) to STANDARD to close this vulnerability.
 * Ampere_and_later does not have this issue (http://lwbugs/2400726/3)
 */
ACR_STATUS
acrProtectHost2JtagWARBug2401005_TU10X(void)
{
    LwU32         fuseInternalSku  = ACR_REG_RD32(BAR0, LW_FUSE_OPT_INTERNAL_SKU);
    LwU32         jtagSecReg       = ACR_REG_RD32(BAR0, LW_PJTAG_ACCESS_SEC);
    LwU32         jtagModeReadBack = 0;
    LwU32         readCount        = 0;

    // If the SKU is INTERNAL or Jtag access MODE is already set to STANDARD, we can skip the jtag security enhancement.
    // Since we want to support ISM mode for INTERNAL boards.And since VBIOS is also fixing this issues as per http://lwbugs/2401005/56 for CSPs,
    // we are adding the fix in ACR to only protect this for GPUs already in market which cannot update vbios (TU10X/TU11X)

    if ((FLD_TEST_DRF(_FUSE, _OPT_INTERNAL_SKU, _DATA, _ENABLE, fuseInternalSku)) ||
        (FLD_TEST_DRF(_PJTAG, _ACCESS_SEC, _MODE, _STANDARD, jtagSecReg)))
    {
       return ACR_OK;
    }

    jtagSecReg = FLD_SET_DRF(_PJTAG, _ACCESS_SEC, _SWITCH_TO_STANDARD, _REQ, jtagSecReg);
    ACR_REG_WR32(BAR0, LW_PJTAG_ACCESS_SEC, jtagSecReg);

    //  Writing _REQ to _SWITCH_TO_STANDARD changes JTAG mode which can be verified through value _STANDARD of read-only field _MODE
    //  We try to read-back MODE_STANDARD to confirm whether the mode switch succeeded. But if it is delayed due to some reason
    //  we might end up reading old value and take action where in fact the write might succeed later.
    //  Thus we keep reading-back for a certain count (10000) to check if the value has been written correctly and if it exceeds the count we timeout

    jtagModeReadBack = ACR_REG_RD32(BAR0, LW_PJTAG_ACCESS_SEC);
    while(!FLD_TEST_DRF(_PJTAG, _ACCESS_SEC, _MODE, _STANDARD, jtagModeReadBack))
    {
        if (readCount > 10000)
        {
            return ACR_ERROR_TIMEOUT;
        }

        jtagModeReadBack = ACR_REG_RD32(BAR0, LW_PJTAG_ACCESS_SEC);
        readCount++;
    }

    return ACR_OK;
}
