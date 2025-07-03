/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

 /* ------------------------ System Includes --------------------------------- */
#include "lwoslayer.h"
#include "riscv_scp_internals.h"
#include "scp_rand.h"
#include "drivers/mpu.h"
#include "riscv_csr.h"
#include "dev_pwr_csb.h"


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
    LwBool bLockdownEngaged = LW_FALSE;

    // Always make sure u-mode is INSEC
    csr_clear(LW_RISCV_CSR_SRSP, DRF_DEF64(_RISCV_CSR, _SRSP, _URSEC, _SEC));

    if (REG_FLD_TEST_DRF_DEF(CSB, _CPWR, _PMU_SCP_CTL_CFG, _LOCKDOWN_SCP, _ENABLE))
    {
        //
        // NOTE: this code is a WAR for GA10X! We need to restrict BAR0 access to SCP
        // registers, so we go into SCP lockdown.
        //
        // We need secure access in order to enable the RNG hardware under lockdown.
        //
        // If we are in little-hammer (SCP) lockdown, make sure we have enabled
        // secure mode for SCP. This should have been done by FMC already,
        // but just in case (and for futureproofing in case FMC no longer does that).
        //
        // This also assumes that both SPM.MSECM and SPM.SSECM have been set by bootrom or FMC.
        //
        bLockdownEngaged = LW_TRUE;
        csr_set(LW_RISCV_CSR_SRSP, DRF_DEF64(_RISCV_CSR, _SRSP, _SRSEC, _SEC));
    }

    // Make sure SCP is enabled (in case we're in lockdown, this must be done after we go to SEC)
    REG_WR32(CSB, LW_CPWR_PMU_SCP_CTL0,
        DRF_DEF(_CPWR_PMU_SCP, _CTL0, _CTL_EN,            _ENABLED) |
        DRF_DEF(_CPWR_PMU_SCP, _CTL0, _SEQ_EN,            _ENABLED) |
        DRF_DEF(_CPWR_PMU_SCP, _CTL0, _SF_CMD_IFACE_EN,   _ENABLED) |
        DRF_DEF(_CPWR_PMU_SCP, _CTL0, _SF_PUSH_IFACE_EN,  _ENABLED) |
        DRF_DEF(_CPWR_PMU_SCP, _CTL0, _SF_FETCH_IFACE_EN, _ENABLED));

    // Init rand
    scpStartRand();

    //
    // SCP RNG has been started, which must be done in SEC mode (in the case
    // when lockdown is on), but now we want to unconditionally go to INSEC
    // to create a random key that can be used by SSP RNG
    // from u-mode, which will be INSEC.
    //
    csr_clear(LW_RISCV_CSR_SRSP, DRF_DEF64(_RISCV_CSR, _SRSP, _SRSEC, _SEC));

    s_lwosSspInitRandomKey();

    if (bLockdownEngaged)
    {
        // If lockdown is on, go back to SEC to disable SCP RNG.
        csr_set(LW_RISCV_CSR_SRSP, DRF_DEF64(_RISCV_CSR, _SRSP, _SRSEC, _SEC));
    }

    scpStopRand();

    // Finally, unconditionally set s-mode to INSEC.
    csr_clear(LW_RISCV_CSR_SRSP, DRF_DEF64(_RISCV_CSR, _SRSP, _SRSEC, _SEC));
}

LwUPtr
lwosSspGetEncryptedCanary(void)
{
    LwU8 scpBuffer[SCP_AES_SIZE_IN_BYTES + SCP_BUF_ALIGNMENT - 1U];
    LwU8 *const pScpBuffer = (void*)LW_ALIGN_UP((LwUPtr)scpBuffer, (LwUPtr)SCP_BUF_ALIGNMENT);
    const FLCN_U64 pseudoRand = s_lwosSspPseudoRandomValueGet();
    LwUPtr dataPa;

    // MMINTS-TODO: move this whole code to kernel and expose syscalls.
    if (lwrtosIS_KERNEL())
    {
        mpuVirtToPhys((void*)pScpBuffer, &dataPa, NULL);
        dataPa -= LW_RISCV_AMAP_DMEM_START;
    }
    else
    {
        sysVirtToEngineOffset((void*)pScpBuffer, &dataPa, NULL, LW_TRUE);
    }

    (void)memcpy(pScpBuffer, (const LwU8*)&pseudoRand, sizeof(pseudoRand));

    // Transfer the pseudo-random number to SCP
    riscv_dmwrite(dataPa, SCP_R3);
    riscv_scpWait();

    //
    // Encrypt the pseudo-random number using the true random number cached
    // in SCP at init time
    //
    riscv_scp_key(LWOS_SSP_SCP_RESERVED_REGISTER);
    riscv_scp_encrypt(SCP_R3, SCP_R3);

    // Retreive the encrypted pseudo-random number
    riscv_dmread(dataPa, SCP_R3);
    riscv_scpWait();

    //
    // Use the lowest 64 bits of the encrypted result for the canary
    // pScpBuffer is guaranteed to be 64-bit-aligned since it's 128-bit-aligned,
    // so we can safely dereference it.
    //
    return *((LwUPtr*)pScpBuffer);
}

/* ------------------------ Private Functions ------------------------------- */
static LW_FORCEINLINE void
s_lwosSspInitRandomKey(void)
{
    riscv_scp_rand(SCP_R6);
    riscv_scp_rand(LWOS_SSP_SCP_RESERVED_REGISTER);

    // use AES cipher to whiten random data
    riscv_scp_key(SCP_R6);
    riscv_scp_encrypt(LWOS_SSP_SCP_RESERVED_REGISTER, LWOS_SSP_SCP_RESERVED_REGISTER);

    //
    // Need to wait for SCP to be idle before we report the key is truly ready.
    // If we turn off the RNG before the random numbers are transferred to the
    // registers, SCP will stall forever waiting for them.
    //
    // Note that this works even though we haven't previously issued a trapped
    // dmread/dmwrite.
    //
    riscv_scpWait();
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
