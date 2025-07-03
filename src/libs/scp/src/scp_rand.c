/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   scp_rand.c
 * @brief  Random number generator
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "lwoslayer.h"

/* ------------------------- Application Includes --------------------------- */
#include "scp_rand.h"
#include "scp_internals.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static void _scpRandEntry(void)
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("imem_libScpRandHs", "start");

static LwBool _scpIsDebugModeEnabled(void)
    GCC_ATTRIB_SECTION("imem_libScpRandHs", "_scpIsDebugModeEnabled");

/*!
 * startRand
 *
 * @brief Starts random number generator in SCP.
 */
void scpStartRand(void)
{
    LwU32 val;
    LwBool bDebugMode;

    bDebugMode = _scpIsDebugModeEnabled();

    // set RNG parameter control registers and enable RNG
    val = DRF_NUM_SCP(_RNDCTL0, _HOLDOFF_INIT_LOWER, 0x7fff);
    falc_stx(SCP_REG(_RNDCTL0), val);

    val = DRF_NUM_SCP(_RNDCTL1, _HOLDOFF_INTRA_MASK, 0x03ff);
    falc_stx(SCP_REG(_RNDCTL1), val);

    val = DRF_NUM_SCP(_RNDCTL11, _AUTOCAL_STATIC_TAP_A, 0x000f) |
          DRF_NUM_SCP(_RNDCTL11, _AUTOCAL_STATIC_TAP_B, 0x000f);

    if (bDebugMode)
    {
        // for debug mode, set both RNG sources to LFSR
        val |= DRF_DEF_SCP(_RNDCTL11, _RAND_SAMPLE_SELECT_A, _LFSR) |
               DRF_DEF_SCP(_RNDCTL11, _RAND_SAMPLE_SELECT_B, _LFSR);
    }

    falc_stx(SCP_REG(_RNDCTL11), val);

    val = DRF_DEF_SCP(_CTL1, _RNG_EN, _ENABLED);

    // for debug mode, enable "fake mode"
    if (bDebugMode)
    {
        val |= DRF_DEF_SCP(_CTL1, _RNG_FAKE_MODE, _ENABLED);
    }
    falc_stx(SCP_REG(_CTL1), val);
}

/*!
 * stopRand
 *
 * @brief Stops random number generator in SCP
 */
void scpStopRand(void)
{
    falc_stx(SCP_REG(_CTL1), 0);
}

/*!
 * getRand128
 *
 * @brief Generates 128 bit random number using SCP
 *
 * @param [out] pData Stores the generated random number
 */
void scpGetRand128(LwU8 *pData)
{
    LwU32  dataPa;

    // load rand_ctx to SCP
    dataPa = osDmemAddressVa2PaGet(pData);
    falc_scp_trap(TC_INFINITY);
    falc_trapped_dmwrite(falc_sethi_i(dataPa, SCP_R5));

    // improve RNG quality by collecting entropy across
    // two conselwtive random numbers
    falc_scp_rand(SCP_R4);
    falc_scp_rand(SCP_R3);

    // trapped dmwait
    falc_dmwait();
    // mixing in previous whitened rand data
    falc_scp_xor(SCP_R5, SCP_R4);

    // use AES cipher to whiten random data
    falc_scp_key(SCP_R4);
    falc_scp_encrypt(SCP_R3, SCP_R3);

    // retrieve random data and update rand_ctx
    // TODO: Update the rand_ctx
    falc_trapped_dmread(falc_sethi_i(dataPa, SCP_R3));
    falc_dmwait();

    // Reset trap to 0
    falc_scp_trap(0);
}

static LwBool _scpIsDebugModeEnabled(void)
{
    LwU32  ctlVal;
    ctlVal = SCP_REG_RD32(_CTL_STAT);
    return FLD_TEST_DRF_SCP(_CTL_STAT, _DEBUG_MODE, _DISABLED, ctlVal) ?
                    LW_FALSE : LW_TRUE;
}

/*!
 * _scpRandEntry
 *
 * @brief Entry function of SCP rand library. This function merely returns. It
 *        is used to validate the overlay blocks corresponding to the secure
 *        overlay before jumping to arbitrary functions inside it. The linker
 *        script should make sure that it puts this section at the top of the
 *        overlay to ensure that we can jump to the overlay's 0th offset before
 *        exelwting code at arbitrary offsets inside the overlay.
 */
static void _scpRandEntry(void)
{
    return;
}
