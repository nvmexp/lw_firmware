/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   riscv_scp_rand.c
 * @brief  Random number generator
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "lwoslayer.h"
/* ------------------------- Application Includes --------------------------- */
#include "riscv_scp_internals.h"
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
/* ------------------------- Private Functions ------------------------------ */

static LwBool _scpIsDebugModeEnabled(void)
    GCC_ATTRIB_SECTION("imem_libScpRandHs", "_scpIsDebugModeEnabled");


/*!
 * scpStartRand
 *
 * @brief Starts random number generator in SCP.
 */
void scpStartRand(void)
{
    LwU32 val;
    LwBool bDebugMode;
    // Take care of any possible context-switches while using SCP registers
    lwrtosENTER_CRITICAL();
    bDebugMode = _scpIsDebugModeEnabled();
    // Dont use riscv_stx if you are already providing PGSP address
    val = DRF_NUM_SCP(_RNDCTL0, _HOLDOFF_INIT_LOWER, 0xf);
    bar0Write(SCP_REG(_RNDCTL0), val);
    val = DRF_NUM_SCP(_RNDCTL1, _HOLDOFF_INTRA_MASK, 0xf);
    bar0Write(SCP_REG(_RNDCTL1), val);
    val = DRF_NUM_SCP(_RNDCTL11, _AUTOCAL_STATIC_TAP_A, 0x0007) |
          DRF_NUM_SCP(_RNDCTL11, _AUTOCAL_STATIC_TAP_B, 0x0007);

    if (bDebugMode)
    {
        // for debug mode, set both RNG sources to LFSR
        val |= DRF_DEF_SCP(_RNDCTL11, _RAND_SAMPLE_SELECT_A, _LFSR) |
               DRF_DEF_SCP(_RNDCTL11, _RAND_SAMPLE_SELECT_B, _LFSR);
    }
    bar0Write(SCP_REG(_RNDCTL11), val);
    val = DRF_DEF_SCP(_CTL1, _RNG_EN, _ENABLED);

    if (bDebugMode)
    {
        val |= DRF_DEF_SCP(_CTL1, _RNG_FAKE_MODE, _ENABLED);
    }

    bar0Write(SCP_REG(_CTL1), val);
    lwrtosEXIT_CRITICAL();
}

/*!
 * scpStopRand
 *
 * @brief Stops random number generator in SCP
 */
void scpStopRand(void)
{
    bar0Write(SCP_REG(_CTL1), 0);
}

/*!
 * scpGetRand128
 *
 * @brief Generates 128 bit random number using SCP
 *
 * @param [out] pData Stores the generated random number
 */
void scpGetRand128(LwU8 *pData)
{
    LwUPtr physAddr;
    // Take care of any possible context-switches while using SCP registers
    lwrtosENTER_CRITICAL();
    // load rand_ctx to SCP
    sysVirtToEngineOffset((void*)pData, &physAddr, NULL, LW_TRUE);
    riscv_dmwrite(physAddr, SCP_R5);
    // improve RNG quality by collecting entropy across
    // two conselwtive random numbers
    riscv_scp_rand(SCP_R4);
    riscv_scp_rand(SCP_R3);
    riscv_scpWait();

    // mixing in previous whitened rand data
    riscv_scp_xor(SCP_R5, SCP_R4);
    // use AES cipher to whiten random data
    riscv_scp_key(SCP_R4);
    riscv_scp_encrypt(SCP_R3, SCP_R3);
    // retrieve random data and update rand_ctx
    riscv_dmread(physAddr, SCP_R3);
    riscv_scpWait();
    lwrtosEXIT_CRITICAL();
}

static LwBool _scpIsDebugModeEnabled(void)
{
    LwU32  ctlVal;
    ctlVal = RV_SCP_REG_RD32(_CTL_STAT);
    return FLD_TEST_DRF_SCP(_CTL_STAT, _DEBUG_MODE, _DISABLED, ctlVal) ?
                    LW_FALSE : LW_TRUE;
}

