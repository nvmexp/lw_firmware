/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file:  booter_trng_tu10x.c
 */
//
// Includes
//
#include "booter.h"
#include "dev_bus.h"

#if defined(BOOTER_LOAD) || defined(IS_SSP_ENABLED)
/*!
 * Get a 128 bit random number from SE
 * @param[in]  swDw  Number of dwords for the rand num (i.e. size of array)
 * @param[out] rand  LwU32 array that would hold the random number
 *
 * @return    BOOTER_STATUS whether the write was successful or not
 */
BOOTER_STATUS
booterGetTRand_TU10X(LwU32 rand[], LwU8 szDw)
{
#ifndef DISABLE_SE_ACCESSES
    BOOTER_STATUS status = BOOTER_OK;
#endif
    LwU32 val = 0;

    if (!rand)
    {
        return BOOTER_ERROR_ILWALID_ARGUMENT;
    }

//
// TRNG is not modelled on certain platforms, we will instead of a hardcoded value
// this also helps with debugging as well
//
// TODO for jamesx by GP107 FS: File bug for implementation of SE/TRNG in fmodel
//
#ifdef BOOTER_FIXED_TRNG
    for (val = 0; val < szDw; val++)
    {
        rand[val] = 0xbeefaaa0 | val;
    }

    return BOOTER_OK;
#endif

#ifndef DISABLE_SE_ACCESSES
#ifdef BOOTER_RELOAD
    BOOTER_SELWREBUS_TARGET busTarget = LWDEC_SELWREBUS_TARGET_SE;
#elif defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    BOOTER_SELWREBUS_TARGET busTarget = SEC2_SELWREBUS_TARGET_SE;
#else
    // Control is not expected to come here. If you hit this assert, please check build profile config
    ct_assert(0);
#endif

    // 1. Obtain SE global mutex
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(
        booterSelwreBusReadRegister_HAL(pBooter, busTarget, LW_SSE_SE_CTRL_PKA_MUTEX, &val));
    if (val != LW_SSE_SE_CTRL_PKA_MUTEX_RC_SUCCESS)
    {
        return BOOTER_ERROR_SELWRE_BUS_REQUEST_FAILED;
    }

    // 2. Enable the TRNG
    // TODO for jamesx for Bug 1732094: Timeout and return an error
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterTrngEnable_HAL(pBooter, busTarget));
    while(!booterTrngIsEnabled_HAL(pBooter, busTarget));

    // 3. Get a random number
    booterTrngGetRandomNum_HAL(pBooter, rand, szDw, busTarget);

    // 4. Release SE global mutex
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(
        booterSelwreBusWriteRegister_HAL(pBooter, busTarget,
                                    LW_SSE_SE_CTRL_PKA_MUTEX_RELEASE,
                                    LW_SSE_SE_CTRL_PKA_MUTEX_RELEASE_SE_LOCK_RELEASE));

    return BOOTER_OK;
#endif
}

#ifndef DISABLE_SE_ACCESSES
/*!
 * @brief Enables TRNG (random number generator.
 *
 * @return status
 */
BOOTER_STATUS
booterTrngEnable_TU10X(BOOTER_SELWREBUS_TARGET busTarget)
{
    BOOTER_STATUS status = BOOTER_OK;
    LwU32      smode  = 0;
    LwU32      i      = 0;
    LwBool     bEmu   = (BOOTER_REG_RD32(BAR0, LW_PBUS_EMULATION_REV0) |
                         BOOTER_REG_RD32(BAR0, LW_PBUS_EMULATION_REV1))? LW_TRUE:LW_FALSE;

    //Check if TRNG is already enabled.
    if (booterTrngIsEnabled_HAL(pBooter, busTarget))
    {
        return BOOTER_OK;
    }

    // Turn TRNG into secure & internal random reseed mode
    smode = REF_NUM(LW_SSE_SE_TRNG_SMODE_SELWRE_EN, LW_SSE_SE_TRNG_SMODE_SELWRE_EN_ENABLE)  |
            REF_NUM(LW_SSE_SE_TRNG_SMODE_MAX_REJECTS, LW_SSE_SE_TRNG_SMODE_MAX_REJECTS_10);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(
        booterSelwreBusWriteRegister_HAL(pBooter, busTarget, LW_SSE_SE_TRNG_SMODE, smode));

    if (bEmu)
    {
        smode |= REF_NUM(LW_SSE_SE_TRNG_SMODE_NONCE_MODE, LW_SSE_SE_TRNG_SMODE_NONCE_MODE_ENABLE);

        for (i = 0; i < LW_SSE_SE_TRNG_SEED__SIZE_1; i++)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterSelwreBusWriteRegister_HAL(pBooter,
                         busTarget, LW_SSE_SE_TRNG_SEED(i),0xfeed0000 | i));
        }
    }

    //
    // Not a TYPO: SMODE register needs to be written twice as per HW requirements
    // In case of EMU: This write enables NONCE mode
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(
        booterSelwreBusWriteRegister_HAL(pBooter, busTarget, LW_SSE_SE_TRNG_SMODE, smode));

    // Enable seeding
    if (bEmu)
    {
        // Turn on NONCE reseed
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(
            booterSelwreBusWriteRegister_HAL(pBooter, busTarget,
                                    LW_SSE_SE_TRNG_CTRL, LW_SSE_SE_TRNG_CTRL_CMD_NONCE_RESEED));
    }
    else
    {
        // Turn on Autoseed
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(
            booterSelwreBusWriteRegister_HAL(pBooter, busTarget,
                                    LW_SSE_SE_CTRL_CONTROL, LW_SSE_SE_CTRL_CONTROL_AUTORESEED_ENABLE));
    }

    return BOOTER_OK;
}

/*!
 * @brief Checks if TRNG is already enabled
 *
 * @return LwBool (true or false depending on if enabled)
 */
// TODO for jamesx by GP107 FS: Change return type to BOOTER_STATUS and no longer use OR_HALT
LwBool
booterTrngIsEnabled_TU10X(BOOTER_SELWREBUS_TARGET busTarget)
{
    BOOTER_STATUS status  = BOOTER_OK;
    LwBool     bEmu    = (BOOTER_REG_RD32(BAR0, LW_PBUS_EMULATION_REV0) |
                          BOOTER_REG_RD32(BAR0, LW_PBUS_EMULATION_REV1))? LW_TRUE:LW_FALSE;

    // Initing this value to make sure its failsafe
    LwU32 val = DRF_DEF(_SSE_SE_TRNG, _STATUS, _SELWRE, _DISABLE);

    // TODO: Get rid of this halt here and instead pass the error to caller to handle it, Bug 1872067
    CHECK_STATUS_OK_OR_HALT_WITH_ERROR_CODE(
        booterSelwreBusReadRegister_HAL(pBooter, busTarget, LW_SSE_SE_TRNG_STATUS, &val));

    if (bEmu)
    {
        // Check only NONCE mode and Secure
        if (!(FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _NONCE_MODE, _ENABLE, val) &&
              FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _SELWRE, _ENABLE, val)))
        {
            return LW_FALSE;
        }
    }
    else
    {
        // internal random reseed, secure, seeded mode. Last reseed by host/I_reseed command, no reseeding or generating
        if (!(FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _NONCE_MODE, _DISABLE, val) &&
              FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _SELWRE, _ENABLE, val) &&
              FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _SEEDED, _ENABLE, val) &&
              (FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _LAST_RESEED, _BY_HOST, val) ||
               FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _LAST_RESEED, _BY_I_RESEED, val)) &&
              FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _RAND_GENERATING, _NO_PROGRESS, val) &&
              FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _RAND_RESEEDING, _NO_PROGRESS, val)))
        {
            return LW_FALSE;
        }

        // check if autoseed is enabled
        CHECK_STATUS_OK_OR_HALT_WITH_ERROR_CODE(
            booterSelwreBusReadRegister_HAL(pBooter, busTarget, LW_SSE_SE_CTRL_CONTROL, &val));

        if (FLD_TEST_DRF(_SSE_SE_CTRL, _CONTROL, _AUTORESEED, _DISABLE, val))
        {
            return LW_FALSE;
        }
    }

    return LW_TRUE;

}

/*!
 * @brief Gets a random number from TRNG
 * @param[in] num - the random numbers generated are returned in this
 * @param[in] sz  - number of dwords wanted
 */
// TODO for jamesx by GP107 FS: Change return type to BOOTER_STATUS and no longer use OR_HALT
void
booterTrngGetRandomNum_TU10X(LwU32 num[], LwU8 szDw, BOOTER_SELWREBUS_TARGET busTarget)
{
    BOOTER_STATUS status = BOOTER_OK;
    LwU32 i;
    LwU32 val = 0;

    // Cap the sz to the maximum we can read from TRNG
    if (szDw > LW_SSE_SE_TRNG_RAND__SIZE_1)
    {
        szDw = LW_SSE_SE_TRNG_RAND__SIZE_1;
    }

    // clear ISTAT
    CHECK_STATUS_OK_OR_HALT_WITH_ERROR_CODE(
        booterSelwreBusWriteRegister_HAL(pBooter, busTarget, LW_SSE_SE_TRNG_ISTAT,
                             REF_NUM(LW_SSE_SE_TRNG_ISTAT_RAND_RDY,
                                     LW_SSE_SE_TRNG_ISTAT_RAND_RDY_SET) |
                             REF_NUM(LW_SSE_SE_TRNG_ISTAT_SEED_DONE,
                                     LW_SSE_SE_TRNG_ISTAT_SEED_DONE_SET) |
                             REF_NUM(LW_SSE_SE_TRNG_ISTAT_AGE_ALARM,
                                     LW_SSE_SE_TRNG_ISTAT_AGE_ALARM_SET) |
                             REF_NUM(LW_SSE_SE_TRNG_ISTAT_RQST_ALARM,
                                     LW_SSE_SE_TRNG_ISTAT_RQST_ALARM_SET)));

    // issue cmd to generate random number
    CHECK_STATUS_OK_OR_HALT_WITH_ERROR_CODE(
        booterSelwreBusWriteRegister_HAL(pBooter, busTarget, LW_SSE_SE_TRNG_CTRL,
                                    LW_SSE_SE_TRNG_CTRL_CMD_GEN_RAN_NUM));

    // make sure number is generated
    // TODO for jamesx for Bug 1732094: This needs to have a timeout
    do
    {
        CHECK_STATUS_OK_OR_HALT_WITH_ERROR_CODE(
            booterSelwreBusReadRegister_HAL(pBooter, busTarget, LW_SSE_SE_TRNG_ISTAT, &val));
    }
    while ((val & LW_SSE_SE_TRNG_ISTAT_RAND_RDY_SET) == 0);

    for(i = 0; i < szDw; i++)
    {
        CHECK_STATUS_OK_OR_HALT_WITH_ERROR_CODE(
            booterSelwreBusReadRegister_HAL(pBooter, busTarget, LW_SSE_SE_TRNG_RAND(i), &num[i]));
    }
}
#endif
#endif //BOOTER_LOAD

