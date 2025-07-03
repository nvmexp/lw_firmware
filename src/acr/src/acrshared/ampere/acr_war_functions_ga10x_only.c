/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_war_functions_ga10x_only.c
 */
#include "acr.h"
#include "dev_pwr_pri.h"
#include "dev_sec_pri.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_pri_ringstation_sys_addendum.h"
#include "dev_fbpa.h"
#include "dev_fuse.h"
#include "dev_sec_csb.h"
#include "dev_se_seb.h"
#include "dev_se_seb_addendum.h"

#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"


// This value should be kept in sync with PMU LW2080_CTRL_PERF_PERF_CF_MEM_TUNE_EXCEPTION_TFAW_VAL !
#define PERF_PERF_CF_MEM_TUNE_EXCEPTION_TFAW_VAL       80U


/*!
 * @brief acrEnableLwdclkScpmForAcr_GA10X:
 *        Enable SCPM at the very beginning of the ACR itself.  (Refer bug 3078892)
 *    (*** PREREQUISITE -- SEC2 ENGINE RESET should be protected before this function call ***)
 *
 *     TODO : Move this to 'acr_war_functions_ga10x.c' file when 'dev_sec_pri.h' hwref
 *     file is forked for gh100.
 *
 * @param  None
 *
 * @return void
 */
void
acrEnableLwdclkScpmForAcr_GA10X(void)
{
    LwU32 lwrScpmVal        = ACR_REG_RD32(CSB, LW_CSEC_SCPM_CTRL);
    LwU32 lwrScpmPlmVal     = 0;
    LwU32 sourceEnableMask  = 0;

    // Check if SCPM has been enabled already
    if (!FLD_TEST_DRF(_CSEC, _SCPM_CTRL, _EN, _TRUE, lwrScpmVal))
    {
        // Read the current PLM value
        lwrScpmPlmVal  = ACR_REG_RD32(CSB, LW_CSEC_SCPM_PRIV_LEVEL_MASK);

        // Lower reads to L0 and block writes on sourceID mismatch. Use CSEC
        lwrScpmPlmVal = FLD_SET_DRF(_CSEC, _SCPM_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL,  _LOWERED, lwrScpmPlmVal);
        lwrScpmPlmVal = FLD_SET_DRF(_CSEC, _SCPM_PRIV_LEVEL_MASK, _SOURCE_WRITE_CONTROL, _BLOCKED, lwrScpmPlmVal);

        // Raise SCPM PLM and source isolate SCPM to only SEC2.
        sourceEnableMask = BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_LOCAL_SOURCE_FALCON);

        lwrScpmPlmVal = FLD_SET_DRF_NUM(_CSEC, _SCPM_PRIV_LEVEL_MASK, _SOURCE_ENABLE, sourceEnableMask, lwrScpmPlmVal);

        // Raise Write Protection level of the SCPM PLM to L3
        lwrScpmPlmVal = FLD_SET_DRF(_CSEC, _SCPM_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ONLY_LEVEL3_ENABLED, lwrScpmPlmVal);
        ACR_REG_WR32(CSB, LW_CSEC_SCPM_PRIV_LEVEL_MASK, lwrScpmPlmVal);

        // Assert the RESET_FUNC so that SCPM can be enabled
        lwrScpmVal = ACR_REG_RD32(CSB, LW_CSEC_SCPM_CTRL);
        lwrScpmVal = FLD_SET_DRF(_CSEC, _SCPM_CTRL, _RESET_FUNC_OVERRIDE, _ASSERTED, lwrScpmVal);
        ACR_REG_WR32(CSB, LW_CSEC_SCPM_CTRL, lwrScpmVal);

        // Set Enable bit and enable SCPM
        lwrScpmVal = ACR_REG_RD32(CSB, LW_CSEC_SCPM_CTRL);
        lwrScpmVal = FLD_SET_DRF(_CSEC, _SCPM_CTRL, _EN, _TRUE, lwrScpmVal);
        ACR_REG_WR32(CSB, LW_CSEC_SCPM_CTRL, lwrScpmVal);

        // Deassert the RESET_FUNC
        lwrScpmVal = ACR_REG_RD32(CSB, LW_CSEC_SCPM_CTRL);
        lwrScpmVal = FLD_SET_DRF(_CSEC, _SCPM_CTRL, _RESET_FUNC_OVERRIDE, _DEASSERTED, lwrScpmVal);
        ACR_REG_WR32(CSB, LW_CSEC_SCPM_CTRL, lwrScpmVal);

        //
        // NOTE : We are not restoring the SCPM PLM because SCPM support should be present during the
        // ASB exelwtion as well. Once the ASB exelwtion is done, the PLM will be reset by SEC2 while
        // it loads SEC2-RTOS.
        //
    }
}

/*!
 * @brief Reset the decode trap used to protect LW_PPWR_FALCON_ENGCTL on anti-mining SKUs.
 *
 * On GA10X, PMU doesn't have a PLM for LW_PPWR_FALCON_ENGCTL. We use decode trap 14
 * to restrict access to it, to prevent DoS attacks on PMU. To avoid conflict with
 * normal reset paths for pre-os PMU code, we clear this decode trap on ACR unload.
 *
 * @param  None
 *
 * @return ACR_OK if WAR completed successfully, ACR_ERROR_WAR_NOT_POSSIBLE otherwise
 */
ACR_STATUS
acrCleanupMiningWAR_GA10X(void)
{
    LwU32 regVal = 0;

    // See https://confluence.lwpu.com/display/RMPER/Pri-source+Isolation+of+Registers

    // Check mining fuse
    regVal = ACR_REG_RD32(BAR0, LW_FUSE_SPARE_BIT_1);
    if (!FLD_TEST_DRF(_FUSE, _SPARE_BIT_1, _DATA, _ENABLE, regVal))
    {
        return ACR_OK;
    }

    // Check and reset the decode trap
    regVal = ACR_REG_RD32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH);

    if (DRF_VAL(_PPRIV, _SYS_PRI_DECODE_TRAP14_MATCH, _ADDR, regVal) == LW_PPWR_FALCON_ENGCTL)
    {
        // Make sure that ACTION has been set to _FORCE_ERROR_RETURN as expected, otherwise bail!
        regVal = ACR_REG_RD32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION);
        if (!FLD_TEST_DRF(_PPRIV, _SYS_PRI_DECODE_TRAP14_ACTION, _FORCE_ERROR_RETURN, _ENABLE, regVal))
        {
            return ACR_ERROR_WAR_NOT_POSSIBLE;
        }

        //
        // At this point we're sure that this decode trap has been set for PMU to block ENGCTL!
        // This means that the ACR task on SEC2 has booted GA10X RISCV PMU-RTOS, and ACR-unload
        // hasn't cleaned up after it yet.
        //

        //
        // If PMU has not yet lowered the reset PLM, that means ACR-unload was called early.
        // In that case, do not clear the decode trap (in case ACR-unload was started early
        // by an attacker trying to expose PMU's ENGCTL while PMU-RTOS is still up).
        //
        regVal = ACR_REG_RD32(BAR0, LW_PPWR_FALCON_RESET_PRIV_LEVEL_MASK);
        if (FLD_TEST_DRF(_PPWR, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, regVal))
        {
            return ACR_ERROR_WAR_NOT_POSSIBLE;
        }

        //
        // Extra check - make sure PMU has throttled memory, which it should always do on detach/shutdown
        // (given LW_FUSE_SPARE_BIT_1._DATA_ENABLE is set). This is another way to cirlwmvent potential
        // malicious early ACR-unload runs.
        //
        regVal = ACR_REG_RD32(BAR0, LW_PFB_FBPA_CONFIG3);
        if (DRF_VAL(_PFB, _FBPA_CONFIG3, _FAW, regVal) != PERF_PERF_CF_MEM_TUNE_EXCEPTION_TFAW_VAL)
        {
            return ACR_ERROR_WAR_NOT_POSSIBLE;
        }

        // If decode trap 14 is set to match LW_PPWR_FALCON_ENGCTL, it is safe for us to reset it!

        ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG,
                        DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP14_MATCH_CFG, _TRAP_APPLICATION, _DEFAULT_PRIV_LEVEL));
        ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP14_MASK,
                        DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP14_MASK, _ADDR, _I) |
                        DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP14_MASK, _SOURCE_ID, _I));
        ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH,
                        DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP14_MATCH, _ADDR, _I) |
                        DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP14_MATCH, _SOURCE_ID, _I));
        ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION,
                        DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP14_ACTION, _ALL_ACTIONS, _DISABLED));
        ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA1,
                        DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP14_DATA1, _V, _I));
        ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA2,
                        DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP14_DATA2, _V, _I));
    }

    return ACR_OK;
}

/*!
 * @brief acrIgnoreShaResultRegForBadValueCheck_GA10X:
 *        Check input CSB address to see if ACR needs to ignore bad value check(0xbadfxxxx)
 *        For SHA hash, any dword could match bad value pattern but it's a valid data.
 *        But we don't need to ignore bad value for all CSB registers, just for SHA result
 *        address registers.
 *
 * @param  [in] addr. This represents CSB address.
 *
 * @return LW_TRUE client can ignore bad value check; otherwise can't.
 */
LwBool
acrIgnoreShaResultRegForBadValueCheck_GA10X
(
    LwU32 addr
)
{
    LwU32 i;

    for (i = 0; i < LW_CSEC_FALCON_SHA_HASH_RESULT__SIZE_1; i++)
    {
        if (addr == LW_CSEC_FALCON_SHA_HASH_RESULT(i))
        {
            return LW_TRUE;
        }
    }

    return LW_FALSE;
}

/*!
 * @brief acrIgnoreSePkaBankRegForBadValueCheck_GA10X:
 *        Check input SE address to see if ACR needs to ignore bad value check(0xbadfxxxx).
 *        For RSA operation, any result or intermediate dword could match bad value pattern but it's a valid data.
 *        But we don't need to ignore bad value for all SE PKA registers, just for SE PKA result
 *        address registers.
 *
 * @param  [in] addr. This represents SE register address.
 *
 * @return LW_TRUE client can ignore bad value check; otherwise can't.
 */
LwBool
acrIgnoreSePkaBankRegForBadValueCheck_GA10X
(
    LwU32 addr
)
{
    if (addr >= LW_SSE_SE_PKA_BANK_REG_END_OFFSET_DWORD_MIN && addr <= LW_SSE_SE_PKA_BANK_REG_END_OFFSET_DWORD_MAX)
    {
        return LW_TRUE;
    }

    return LW_FALSE;
}

