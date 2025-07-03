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
 * @file: booter_war_functions_ga10x_only.c
 */
#include "booter.h"

#ifndef BOOTER_RELOAD
/*!
 * @brief booterEnableLwdclkScpmForBooter_GA10X:
 *        Enable SCPM at the very beginning of the BOOTER itself.  (Refer bug 3078892)
 *    (*** PREREQUISITE -- SEC2 ENGINE RESET should be protected before this function call ***)
 *
 *     TODO : Move this to 'booter_war_functions_ga10x.c' file when 'dev_sec_pri.h' hwref
 *     file is forked for gh100.
 *
 * @param  None
 *
 * @return void
 */
void
booterEnableLwdclkScpmForBooter_GA10X(void)
{
    LwU32 lwrScpmVal        = BOOTER_REG_RD32(CSB, LW_CSEC_SCPM_CTRL);
    LwU32 lwrScpmPlmVal     = 0;
    LwU32 sourceEnableMask  = 0;

    // Check if SCPM has been enabled already
    if (!FLD_TEST_DRF(_CSEC, _SCPM_CTRL, _EN, _TRUE, lwrScpmVal))
    {
        // Read the current PLM value
        lwrScpmPlmVal  = BOOTER_REG_RD32(CSB, LW_CSEC_SCPM_PRIV_LEVEL_MASK);

        // Lower reads to L0 and block writes on sourceID mismatch. Use CSEC
        lwrScpmPlmVal = FLD_SET_DRF(_CSEC, _SCPM_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL,  _LOWERED, lwrScpmPlmVal);
        lwrScpmPlmVal = FLD_SET_DRF(_CSEC, _SCPM_PRIV_LEVEL_MASK, _SOURCE_WRITE_CONTROL, _BLOCKED, lwrScpmPlmVal);

        // Raise SCPM PLM and source isolate SCPM to only SEC2.
        sourceEnableMask = BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_LOCAL_SOURCE_FALCON);

        lwrScpmPlmVal = FLD_SET_DRF_NUM(_CSEC, _SCPM_PRIV_LEVEL_MASK, _SOURCE_ENABLE, sourceEnableMask, lwrScpmPlmVal);

        // Raise Write Protection level of the SCPM PLM to L3
        lwrScpmPlmVal = FLD_SET_DRF(_CSEC, _SCPM_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ONLY_LEVEL3_ENABLED, lwrScpmPlmVal);
        BOOTER_REG_WR32(CSB, LW_CSEC_SCPM_PRIV_LEVEL_MASK, lwrScpmPlmVal);

        // Assert the RESET_FUNC so that SCPM can be enabled
        lwrScpmVal = BOOTER_REG_RD32(CSB, LW_CSEC_SCPM_CTRL);
        lwrScpmVal = FLD_SET_DRF(_CSEC, _SCPM_CTRL, _RESET_FUNC_OVERRIDE, _ASSERTED, lwrScpmVal);
        BOOTER_REG_WR32(CSB, LW_CSEC_SCPM_CTRL, lwrScpmVal);

        // Set Enable bit and enable SCPM
        lwrScpmVal = BOOTER_REG_RD32(CSB, LW_CSEC_SCPM_CTRL);
        lwrScpmVal = FLD_SET_DRF(_CSEC, _SCPM_CTRL, _EN, _TRUE, lwrScpmVal);
        BOOTER_REG_WR32(CSB, LW_CSEC_SCPM_CTRL, lwrScpmVal);

        // Deassert the RESET_FUNC
        lwrScpmVal = BOOTER_REG_RD32(CSB, LW_CSEC_SCPM_CTRL);
        lwrScpmVal = FLD_SET_DRF(_CSEC, _SCPM_CTRL, _RESET_FUNC_OVERRIDE, _DEASSERTED, lwrScpmVal);
        BOOTER_REG_WR32(CSB, LW_CSEC_SCPM_CTRL, lwrScpmVal);

        //
        // NOTE : We are not restoring the SCPM PLM because SCPM support should be present during the
        // ASB exelwtion as well. Once the ASB exelwtion is done, the PLM will be reset by SEC2 while
        // it loads SEC2-RTOS.
        //
    }
}
#endif

/*!
 * @brief booterIgnoreShaResultRegForBadValueCheck_GA10X:
 *        Check input CSB address to see if Booter needs to ignore bad value check(0xbadfxxxx)
 *        For SHA hash, any dword could match bad value pattern but it's a valid data.
 *        But we don't need to ignore bad value for all CSB registers, just for SHA result
 *        address registers.
 *
 * @param  [in] addr. This represents CSB address.
 *
 * @return LW_TRUE client can ignore bad value check; otherwise can't.
 */
LwBool
booterIgnoreShaResultRegForBadValueCheck_GA10X
(
    LwU32 addr
)
{
    LwU32 i;

#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    for (i = 0; i < LW_CSEC_FALCON_SHA_HASH_RESULT__SIZE_1; i++)
    {
        if (addr == LW_CSEC_FALCON_SHA_HASH_RESULT(i))
        {
            return LW_TRUE;
        }
    }
#elif defined(BOOTER_RELOAD)
    for (i = 0; i < LW_CLWDEC_FALCON_SHA_HASH_RESULT__SIZE_1; i++)
    {
        if (addr == LW_CLWDEC_FALCON_SHA_HASH_RESULT(i))
        {
            return LW_TRUE;
        }
    }
#else
    ct_assert(0);
#endif

    return LW_FALSE;
}

/*!
 * @brief booterIgnoreSePkaBankRegForBadValueCheck_GA10X:
 *        Check input SE address to see if Booter needs to ignore bad value check(0xbadfxxxx).
 *        For RSA operation, any result or intermediate dword could match bad value pattern but it's a valid data.
 *        But we don't need to ignore bad value for all SE PKA registers, just for SE PKA result
 *        address registers.
 *
 * @param  [in] addr. This represents SE register address.
 *
 * @return LW_TRUE client can ignore bad value check; otherwise can't.
 */
LwBool
booterIgnoreSePkaBankRegForBadValueCheck_GA10X
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
