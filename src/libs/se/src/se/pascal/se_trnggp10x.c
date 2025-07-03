/*
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */


/*!
 * @file   se_trnggp10x.c
 * @brief  SE HAL functions for supporting a true random number generator.
 *
 */
#include "se_objse.h"
#include "secbus_ls.h"
#include "secbus_hs.h"
#include "config/g_se_hal.h"
#include "dev_se_seb.h"
#include "se_private.h"
#include "flcnifcmn.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static SE_STATUS _seTrueRandomGetRandom_GP10X(LwU32 *pRandNum, LwU32 sizeInDWords)
    GCC_ATTRIB_SECTION("imem_libSE", "_seTrueRandomGetRandom_GP10X");

static SE_STATUS _seTrueRandomGetRandomHs_GP10X(LwU32 *pRandNum, LwU32 sizeInDWords)
    GCC_ATTRIB_SECTION("imem_libSEHs", "_seTrueRandomGetRandomHs_GP10X");

/* ------------------------- Implementation --------------------------------- */

/*!
 * @brief Enables true random number generator.
 *
 * @return status
 */
SE_STATUS seTrueRandomEnable_GP10X(void)
{
    SE_STATUS status = SE_OK;

    // Check if TRNG is already enabled. 
    if (seIsTrueRandomEnabled_HAL())
    {
        return SE_OK;
    }

    //
    // Turn TRNG into secure & internal random reseed mode
    // Kimi Yang reviewed and said to always write this twice
    //
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChk(LW_SSE_SE_TRNG_SMODE,
                                                   DRF_DEF(_SSE, _SE_TRNG_SMODE, _SELWRE_EN, _ENABLE) |
                                                   DRF_DEF(_SSE, _SE_TRNG_SMODE, _MAX_REJECTS, _10)));
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChk(LW_SSE_SE_TRNG_SMODE,
                                                   DRF_DEF(_SSE, _SE_TRNG_SMODE, _SELWRE_EN, _ENABLE) |
                                                   DRF_DEF(_SSE, _SE_TRNG_SMODE, _MAX_REJECTS, _10)));

    //
    // Enable 256 bit. 
    // When R256 is set to 1, SE will collect 256 bits of data in the RANDx registers before asserting the
    // ISTAT.RAND_RDY bit. The collected data will be available in RAND0 through RAND7. When R256 is set to
    // 0, the SE will collect 128 bits of data before asserting the ISTAT.RAND_RDY bit. This data will be
    // available in RAND0 through RAND3. In this latter case, registers RAND4 through RAND7 are unused and will
    // read back as 0
    //
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChk(LW_SSE_SE_TRNG_MODE, DRF_DEF(_SSE, _SE_TRNG_MODE, _R256, _ENABLE)));

    // Turn on Autoseed
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChk(LW_SSE_SE_CTRL_CONTROL, DRF_DEF(_SSE, _SE_CTRL_CONTROL, _AUTORESEED, _ENABLE)));

ErrorExit:
    return status;
}

/*!
 * @brief Enables true random number generator in HS Mode.
 *
 * @return status
 */
SE_STATUS seTrueRandomEnableHs_GP10X(void)
{
    SE_STATUS status = SE_OK;

    // Check if TRNG is already enabled. 
    if (seIsTrueRandomEnabledHs_HAL())
    {
        return SE_OK;
    }

    //
    // Turn TRNG into secure & internal random reseed mode
    // Kimi Yang reviewed and said to always write this twice
    //
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChkHs(LW_SSE_SE_TRNG_SMODE,
                                                   DRF_DEF(_SSE, _SE_TRNG_SMODE, _SELWRE_EN, _ENABLE) |
                                                   DRF_DEF(_SSE, _SE_TRNG_SMODE, _MAX_REJECTS, _10)));
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChkHs(LW_SSE_SE_TRNG_SMODE,
                                                   DRF_DEF(_SSE, _SE_TRNG_SMODE, _SELWRE_EN, _ENABLE) |
                                                   DRF_DEF(_SSE, _SE_TRNG_SMODE, _MAX_REJECTS, _10)));

    //
    // Enable 256 bit. 
    // When R256 is set to 1, SE will collect 256 bits of data in the RANDx registers before asserting the
    // ISTAT.RAND_RDY bit. The collected data will be available in RAND0 through RAND7. When R256 is set to
    // 0, the SE will collect 128 bits of data before asserting the ISTAT.RAND_RDY bit. This data will be
    // available in RAND0 through RAND3. In this latter case, registers RAND4 through RAND7 are unused and will
    // read back as 0
    //
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChkHs(LW_SSE_SE_TRNG_MODE, DRF_DEF(_SSE, _SE_TRNG_MODE, _R256, _ENABLE)));

    // Turn on Autoseed
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChkHs(LW_SSE_SE_CTRL_CONTROL, DRF_DEF(_SSE, _SE_CTRL_CONTROL, _AUTORESEED, _ENABLE)));

ErrorExit:
    return status;
}

/*!
 * @brief Checks if true random number generator is already enabled
 *
 * @return LwBool (true or false depending on if enabled)
 */
LwBool seIsTrueRandomEnabled_GP10X(void)
{
    SE_STATUS status = SE_OK;
    LwU32     val;

    CHECK_SE_STATUS(seSelwreBusReadRegisterErrChk(LW_SSE_SE_TRNG_STATUS, &val));

    //
    // Check TRNG status:  Internal random reseed, secure, seeded mode. 
    //                     Last reseed by host/I_reseed command, no reseeding or generating
    //                     R256 Mode
    //
    if (!(FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _NONCE_MODE, _DISABLE, val)          &&
          FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _SELWRE, _ENABLE, val)               &&
          FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _SEEDED, _ENABLE, val)               &&
          FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _R256,   _ENABLE, val)               &&
         (FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _LAST_RESEED, _BY_HOST, val) ||
          FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _LAST_RESEED, _BY_I_RESEED, val))    &&
          FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _RAND_GENERATING, _NO_PROGRESS, val) &&
          FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _RAND_RESEEDING, _NO_PROGRESS, val)))
    {
        return LW_FALSE;
    }

    // autoseed is enabled
    CHECK_SE_STATUS(seSelwreBusReadRegisterErrChk(LW_SSE_SE_CTRL_CONTROL, &val));
    if (FLD_TEST_DRF(_SSE_SE_CTRL, _CONTROL, _AUTORESEED, _DISABLE, val))
    {
        return LW_FALSE;
    }

ErrorExit:
    return ((status == SE_OK) ? LW_TRUE : LW_FALSE);
}

/*!
 * @brief Checks if true random number generator is already enabled in Hs Mode
 *
 * @return LwBool (true or false depending on if enabled)
 */
LwBool seIsTrueRandomEnabledHs_GP10X(void)
{
    SE_STATUS status = SE_OK;
    LwU32     val;

    CHECK_SE_STATUS(seSelwreBusReadRegisterErrChkHs(LW_SSE_SE_TRNG_STATUS, &val));

    //
    // Check TRNG status:  Internal random reseed, secure, seeded mode. 
    //                     Last reseed by host/I_reseed command, no reseeding or generating
    //                     R256 Mode
    //
    if (!(FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _NONCE_MODE, _DISABLE, val)          &&
          FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _SELWRE, _ENABLE, val)               &&
          FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _SEEDED, _ENABLE, val)               &&
          FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _R256,   _ENABLE, val)               &&
         (FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _LAST_RESEED, _BY_HOST, val) ||
          FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _LAST_RESEED, _BY_I_RESEED, val))    &&
          FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _RAND_GENERATING, _NO_PROGRESS, val) &&
          FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _RAND_RESEEDING, _NO_PROGRESS, val)))
    {
        return LW_FALSE;
    }

    // autoseed is enabled
    CHECK_SE_STATUS(seSelwreBusReadRegisterErrChkHs(LW_SSE_SE_CTRL_CONTROL, &val));
    if (FLD_TEST_DRF(_SSE_SE_CTRL, _CONTROL, _AUTORESEED, _DISABLE, val))
    {
        return LW_FALSE;
    }

ErrorExit:
    return ((status == SE_OK) ? LW_TRUE : LW_FALSE);
}

/*!
 * @brief Helper function for true random number generator.  The function will
 *        return a random number up to LW_SSE_SE_TRNG_RAND__SIZE_1 dwords.  The
 *        function will need to be called multiple times to generate a random 
 *        number bigger than LW_SSE_SE_TRNG_RAND__SIZE_1.
 *
 * @param[out] pRandNum     - the random number generated is returned in this
 * @param[in]  sizeInDWords - the size of the random number requested in dwords
 *                            upto LW_SSE_SE_TRNG_RAND__SIZE_1.
 */
static SE_STATUS _seTrueRandomGetRandom_GP10X(LwU32 *pRandNum, LwU32 sizeInDWords)
{
    SE_STATUS status = SE_OK;
    LwU32     i, val;

    // Verify input data
    if (sizeInDWords > LW_SSE_SE_TRNG_RAND__SIZE_1)
    {
        return SE_ERR_BAD_INPUT_DATA;
    }

    // clear ISTAT
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChk(LW_SSE_SE_TRNG_ISTAT,
                                                   DRF_DEF(_SSE, _SE_TRNG_ISTAT, _RAND_RDY,   _SET) |
                                                   DRF_DEF(_SSE, _SE_TRNG_ISTAT, _SEED_DONE,  _SET) |
                                                   DRF_DEF(_SSE, _SE_TRNG_ISTAT, _AGE_ALARM,  _SET) |
                                                   DRF_DEF(_SSE, _SE_TRNG_ISTAT, _RQST_ALARM, _SET)));


    // issue cmd to generate random number
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChk(LW_SSE_SE_TRNG_CTRL, LW_SSE_SE_TRNG_CTRL_CMD_GEN_RAN_NUM));

    // make sure number is generated
    do
    {
        CHECK_SE_STATUS(seSelwreBusReadRegisterErrChk(LW_SSE_SE_TRNG_ISTAT, &val));
    } while ((val & LW_SSE_SE_TRNG_ISTAT_RAND_RDY_SET) == 0);

    for (i = 0; i < sizeInDWords; i++)
    {
        CHECK_SE_STATUS(seSelwreBusReadRegisterErrChk(LW_SSE_SE_TRNG_RAND(i), &val));
        pRandNum[i] = val;
    }

ErrorExit:
    return status;
}

/*!
 * @brief Helper function for true random number generator in HS Mode.  The function will
 *        return a random number up to LW_SSE_SE_TRNG_RAND__SIZE_1 dwords.  The
 *        function will need to be called multiple times to generate a random 
 *        number bigger than LW_SSE_SE_TRNG_RAND__SIZE_1.
 *
 * @param[out] pRandNum     - the random number generated is returned in this
 * @param[in]  sizeInDWords - the size of the random number requested in dwords
 *                            upto LW_SSE_SE_TRNG_RAND__SIZE_1.
 */
static SE_STATUS _seTrueRandomGetRandomHs_GP10X(LwU32 *pRandNum, LwU32 sizeInDWords)
{
    SE_STATUS status = SE_OK;
    LwU32     i, val;

    // Verify input data
    if (sizeInDWords > LW_SSE_SE_TRNG_RAND__SIZE_1)
    {
        return SE_ERR_BAD_INPUT_DATA;
    }

    // clear ISTAT
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChkHs(LW_SSE_SE_TRNG_ISTAT,
                                                   DRF_DEF(_SSE, _SE_TRNG_ISTAT, _RAND_RDY,   _SET) |
                                                   DRF_DEF(_SSE, _SE_TRNG_ISTAT, _SEED_DONE,  _SET) |
                                                   DRF_DEF(_SSE, _SE_TRNG_ISTAT, _AGE_ALARM,  _SET) |
                                                   DRF_DEF(_SSE, _SE_TRNG_ISTAT, _RQST_ALARM, _SET)));


    // issue cmd to generate random number
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChkHs(LW_SSE_SE_TRNG_CTRL, LW_SSE_SE_TRNG_CTRL_CMD_GEN_RAN_NUM));

    // make sure number is generated
    do
    {
        CHECK_SE_STATUS(seSelwreBusReadRegisterErrChkHs(LW_SSE_SE_TRNG_ISTAT, &val));
    } while ((val & LW_SSE_SE_TRNG_ISTAT_RAND_RDY_SET) == 0);

    for (i = 0; i < sizeInDWords; i++)
    {
        CHECK_SE_STATUS(seSelwreBusReadRegisterErrChkHs(LW_SSE_SE_TRNG_RAND(i), &val));
        pRandNum[i] = val;
    }

ErrorExit:
    return status;
}

/*!
 * @brief Gets a true random number from true random number generator
 *        This function will call the helper function to generate a random number
 *        as many times as necessary to get the correct size.
 *
 * @param[out] pRandNum     - the random numbers generated are returned in this
 * @param[in]  sizeInDWords - the size of the random number in dwords
 *
 * @return status
 */
SE_STATUS seTrueRandomGetNumber_GP10X(LwU32 *pRandNum, LwU32 sizeInDWords)
{
    SE_STATUS status       = SE_OK;
    LwU32     sizeInSERegs = 0;
    LwU32     genRandLoop  = 0;
    LwU32     randCount    = 0;
    LwU32     wordCount    = LW_SSE_SE_TRNG_RAND__SIZE_1;
    LwU32     scrubRandomNumber[LW_SSE_SE_TRNG_RAND__SIZE_1] = { 0 };
    LwU32     val;

    //
    // Enable TRNG if not enabled
    // TODO:  Add a timeout and error for this
    //
    while (seIsTrueRandomEnabled_HAL(&Se) == LW_FALSE)
    {
        CHECK_SE_STATUS(seTrueRandomEnable_HAL(&Se));
    }

    //
    // Double check R256 is enabled.  If R256 is not enabled, LW_SSE_SE_TRNG_RAND__SIZE_1
    // is not valid.  Randd4-7 will return 0.
    //
    CHECK_SE_STATUS(seSelwreBusReadRegisterErrChk(LW_SSE_SE_TRNG_STATUS, &val));
    if (FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _R256, _DISABLE, val))
    {
        return  SE_ERR_TRNG_R256_NOT_ENABLED;
    }

    sizeInSERegs = LW_CEIL(sizeInDWords, LW_SSE_SE_TRNG_RAND__SIZE_1);

    for (genRandLoop = 0; genRandLoop < sizeInSERegs; genRandLoop++)
    {
        if (sizeInDWords < LW_SSE_SE_TRNG_RAND__SIZE_1)
        {
            wordCount = sizeInDWords;
        }

        CHECK_SE_STATUS(_seTrueRandomGetRandom_GP10X(&pRandNum[randCount], wordCount));
        randCount += LW_SSE_SE_TRNG_RAND__SIZE_1;

        if (sizeInDWords >= LW_SSE_SE_TRNG_RAND__SIZE_1)
        {
            sizeInDWords -= LW_SSE_SE_TRNG_RAND__SIZE_1;
        }
    }

    // Call Random Number Generator one additional time to scrub
    CHECK_SE_STATUS(_seTrueRandomGetRandom_GP10X(scrubRandomNumber, LW_SSE_SE_TRNG_RAND__SIZE_1));

    // Scrub the scrub random number
    for (genRandLoop = 0; genRandLoop < LW_SSE_SE_TRNG_RAND__SIZE_1; genRandLoop++)
    {
        scrubRandomNumber[genRandLoop] = 0;
    }


ErrorExit:
    return status;
}

/*!
 * @brief Gets a true random number from true random number generator in HS Mode
 *        This function will call the helper function to generate a random number
 *        as many times as necessary to get the correct size.
 *
 * @param[out] pRandNum     - the random numbers generated are returned in this
 * @param[in]  sizeInDWords - the size of the random number in dwords
 *
 * @return status
 */
SE_STATUS seTrueRandomGetNumberHs_GP10X(LwU32 *pRandNum, LwU32 sizeInDWords)
{
    SE_STATUS status       = SE_OK;
    LwU32     sizeInSERegs = 0;
    LwU32     genRandLoop  = 0;
    LwU32     randCount    = 0;
    LwU32     wordCount    = LW_SSE_SE_TRNG_RAND__SIZE_1;
    LwU32     scrubRandomNumber[LW_SSE_SE_TRNG_RAND__SIZE_1];
    LwU32     val;

    seMemSetHs(scrubRandomNumber, 0, LW_SSE_SE_TRNG_RAND__SIZE_1);
    //
    // Enable TRNG if not enabled
    // TODO:  Add a timeout and error for this
    //
    while (seIsTrueRandomEnabledHs_HAL(&Se) == LW_FALSE)
    {
        CHECK_SE_STATUS(seTrueRandomEnableHs_HAL(&Se));
    }

    //
    // Double check R256 is enabled.  If R256 is not enabled, LW_SSE_SE_TRNG_RAND__SIZE_1
    // is not valid.  Randd4-7 will return 0.
    //
    CHECK_SE_STATUS(seSelwreBusReadRegisterErrChkHs(LW_SSE_SE_TRNG_STATUS, &val));
    if (FLD_TEST_DRF(_SSE_SE_TRNG, _STATUS, _R256, _DISABLE, val))
    {
        return  SE_ERR_TRNG_R256_NOT_ENABLED;
    }

    sizeInSERegs = LW_CEIL(sizeInDWords, LW_SSE_SE_TRNG_RAND__SIZE_1);

    for (genRandLoop = 0; genRandLoop < sizeInSERegs; genRandLoop++)
    {
        if (sizeInDWords < LW_SSE_SE_TRNG_RAND__SIZE_1)
        {
            wordCount = sizeInDWords;
        }

        CHECK_SE_STATUS(_seTrueRandomGetRandomHs_GP10X(&pRandNum[randCount], wordCount));
        randCount += LW_SSE_SE_TRNG_RAND__SIZE_1;

        if (sizeInDWords >= LW_SSE_SE_TRNG_RAND__SIZE_1)
        {
            sizeInDWords -= LW_SSE_SE_TRNG_RAND__SIZE_1;
        }
    }

    // Call Random Number Generator one additional time to scrub
    CHECK_SE_STATUS(_seTrueRandomGetRandomHs_GP10X(scrubRandomNumber, LW_SSE_SE_TRNG_RAND__SIZE_1));

    // Scrub the scrub random number
    seMemSetHs(scrubRandomNumber, 0, LW_SSE_SE_TRNG_RAND__SIZE_1);


ErrorExit:
    return status;
}
