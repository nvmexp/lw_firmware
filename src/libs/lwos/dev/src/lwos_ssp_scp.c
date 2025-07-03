/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------ System Includes --------------------------------- */
#include "lwoslayer.h"
#include "scp_internals.h"
#include "scp_rand.h"

/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ Macro definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Public variables -------------------------------- */
/* ------------------------ Static Function Prototypes  --------------------- */
/*!
 * @brief   Initializes a random value in SCP to be later used to help provide
 *          more cryptographically secure random values
 *
 * @note    After this function exelwtes, it will leave a true random number in
 *          the SCP register specifieid by @ref LWOS_SSP_SCP_RESERVED_REGISTER.
 *          This register *MUST NOT* be accessed after this function exelwtes.
 */
static void s_lwosSspInitRandomKey(void);

/*!
 * @brief   Retrieves a pseudo-random value (the current PTIMER value)
 *
 * @return  A "pseudo-random" value (from PTIMER)
 */
static FLCN_U64 s_lwosSspPseudoRandomValueGet(void);

/* ------------------------ Compile-Time Checks ----------------------------- */
/*
 * If this file is being compiled (i.e., IS_SSP_ENABLED_WITH_SCP is set), then
 * the "base" SSP macro should be defined as well.
 */
#ifndef IS_SSP_ENABLED
ct_assert(LW_FALSE);
#endif // IS_SSP_ENABLED

// Make sure that SCP_BUF_ALIGNMENT implies 64-bit alignment
ct_assert(SCP_AES_SIZE_IN_BYTES >= sizeof(LwUPtr));
ct_assert(SCP_BUF_ALIGNMENT     >= sizeof(LwUPtr));
ct_assert(SCP_BUF_ALIGNMENT % sizeof(LwUPtr) == 0);

/* ------------------------ Public Functions  ------------------------------- */
void
lwosSspRandInit(void)
{
    scpStartRand();
    s_lwosSspInitRandomKey();
    scpStopRand();
}

LwUPtr
lwosSspGetEncryptedCanary(void)
{
    LwU8 scpBuffer[SCP_AES_SIZE_IN_BYTES + SCP_BUF_ALIGNMENT - 1U];
    LwU8 *const pScpBuffer = (void *)LW_ALIGN_UP((LwUPtr)scpBuffer, (LwUPtr)SCP_BUF_ALIGNMENT);
    const FLCN_U64 pseudoRand = s_lwosSspPseudoRandomValueGet();
    const LwU32 dataPa = osDmemAddressVa2PaGet(pScpBuffer);

    (void)memcpy(pScpBuffer, (const LwU8 *)&pseudoRand, sizeof(pseudoRand));

    // Trap DMA to redirect to SCP
    falc_scp_trap(TC_INFINITY);

    // Transfer the pseudo-random number to SCP
    falc_trapped_dmwrite(falc_sethi_i(dataPa, SCP_R3));
    falc_dmwait();

    //
    // Encrypt the pseudo-random number using the true random number cached
    // in SCP at init time
    //
    falc_scp_key(LWOS_SSP_SCP_RESERVED_REGISTER);
    falc_scp_encrypt(SCP_R3, SCP_R3);

    // Retreive the encrypted pseudo-random number
    falc_trapped_dmread(falc_sethi_i(dataPa, SCP_R3));
    falc_dmwait();

    // Turn off trapped DMA
    falc_scp_trap(0U);

    // Use the lowest dword of the encrypted result for the canary
    // pScpBuffer is guaranteed to be 32-bit-aligned since it's 128-bit-aligned,
    // so we can safely dereference it.
    //
    return *((LwUPtr*)pScpBuffer);
}

/* ------------------------ Private Functions ------------------------------- */
static void
s_lwosSspInitRandomKey(void)
{
    falc_scp_rand(SCP_R6);
    falc_scp_rand(LWOS_SSP_SCP_RESERVED_REGISTER);

     // use AES cipher to whiten random data
    falc_scp_key(SCP_R6);
    falc_scp_encrypt(LWOS_SSP_SCP_RESERVED_REGISTER, LWOS_SSP_SCP_RESERVED_REGISTER);

    //
    // Need to wait for SCP to be idle before we report the key is truly ready.
    // If we turn off the RNG before the random numbers are transferred to the
    // registers, SCP will stall forever waiting for them.
    //
    // Note that this works even though we haven't previously issued a trapped
    // dmread/dmwrite.
    //
    falc_scp_trap(TC_INFINITY);
    falc_dmwait();
    falc_scp_trap(0U);
}

static FLCN_U64
s_lwosSspPseudoRandomValueGet(void)
{
    FLCN_TIMESTAMP time1;
    FLCN_TIMESTAMP time2;

    // We want to check that PTIMER is actually incrementing
    osPTimerTimeNsLwrrentGet(&time1);
    do
    {
        osPTimerTimeNsLwrrentGet(&time2);
    } while (time2.data <= time1.data);

    return time2;
}
