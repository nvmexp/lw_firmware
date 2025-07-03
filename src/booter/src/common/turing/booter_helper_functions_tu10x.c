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
 * @file: booter_helper_functions_tu10x.c
 */
//
// Includes
//
#include "booter.h"
#include "sec2mutexreservation.h"

// Global variable for storing the original falcon reset mask
#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
static LwU8               g_resetPLM            ATTR_OVLY(".data");
#endif

#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
/*
 * Save/Lock and Restore falcon reset when binary is running
 * TODO: Sanket : Use below enum instead of bool, and rename function to SaveLockRestore
 * enum
 * {
 *  lockFalconReset_SaveAndLock,
 *  lockFalconReset_Restore = 4,
 * } LOCK_FALCON_RESET;
 */
BOOTER_STATUS
booterSelfLockFalconReset_TU10X(LwBool bLock)
{
    LwU32 reg = 0;

    //
    // We are typecasting WRITE_PROTECTION field to LwU8 below but since the size of that field is controlled by hw,
    // the ct_assert below will help warn us if our assumption is proven incorrect in future
    //
    ct_assert((sizeof(g_resetPLM) * 8) >= DRF_SIZE(LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK_WRITE_PROTECTION));

    reg = BOOTER_REG_RD32(CSB, LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK);
    if (bLock)
    {
        // Lock falcon reset for level 0, 1, 2
        g_resetPLM = (LwU8)(DRF_VAL( _CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION, reg));
        reg = FLD_SET_DRF( _CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, reg);
        reg = FLD_SET_DRF( _CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, reg);
        reg = FLD_SET_DRF( _CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _DISABLE, reg);
    }
    else
    {
        // Restore original mask
        reg = FLD_SET_DRF_NUM( _CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION, g_resetPLM, reg);
    }
    BOOTER_REG_WR32(CSB, LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK, reg);

    return BOOTER_OK;
}
#endif

/*!
 * @brief Write status into MAILBOX0
 * Pending: Clean-up after BOOTER_RELOAD is moved to sec2 from LWDEC.
 * CL: https://lwcr.lwpu.com/sw/25021609/
 */
void
booterWriteStatusToFalconMailbox_TU10X(BOOTER_STATUS status)
{
#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    BOOTER_REG_WR32(CSB, LW_CSEC_FALCON_MAILBOX0, status);
#elif defined(BOOTER_RELOAD)
    BOOTER_REG_WR32(CSB, LW_CLWDEC_FALCON_MAILBOX0, status);
#else
    ct_assert(0);
#endif
}

/*!
 * @brief ACR routine to check if falcon is in DEBUG mode or not
 */
LwBool
booterIsDebugModeEnabled_TU10X(void)
{
    LwU32   scpCtl = 0;

#if defined(BOOTER_RELOAD)
    scpCtl = BOOTER_REG_RD32(CSB, LW_CLWDEC_SCP_CTL_STAT);
#elif defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    scpCtl = BOOTER_REG_RD32(CSB, LW_CSEC_SCP_CTL_STAT);
#else
    // Control is not expected to come here. If you hit this assert, please check bumodeild profile config
    ct_assert(0);
#endif

#if defined(BOOTER_RELOAD)
    return !FLD_TEST_DRF( _CLWDEC, _SCP_CTL_STAT, _DEBUG_MODE, _DISABLED, scpCtl);
#elif defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    return !FLD_TEST_DRF( _CSEC, _SCP_CTL_STAT, _DEBUG_MODE, _DISABLED, scpCtl);
#else
    // Control is not expected to come here. If you hit this assert, please check bumodeild profile config
    ct_assert(0);
#endif
}

BOOTER_STATUS
booterCheckBooterVersionInSelwreScratch_TU10X(LwU32 myBooterVersion)
{
    BOOTER_STATUS status, statusCleanup;
    LwU8 vprMutexToken;
    LwU32 reg;
    LwU32 booterVersionInScratch;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterAcquireSelwreMutex_HAL(pBooter, SEC2_MUTEX_VPR_SCRATCH, &vprMutexToken));

	reg = BOOTER_REG_RD32(BAR0, LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14);

    booterVersionInScratch = DRF_VAL(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _BOOTER_BINARY_VERSION, reg);

#if defined(BOOTER_LOAD)
    // Booter Load is the first one, so version should be 0
    if (booterVersionInScratch != 0)
    {
        status = BOOTER_ERROR_BINARY_SEQUENCE_MISMATCH;
        goto cleanup;
    }
    
    //
    // Booter Load should also run before any ACR ucode (including legacy ones),
    // so ACR binary version should also be 0
    //
    LwU32 acrVersionInScratch = DRF_VAL(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _ACR_BINARY_VERSION, reg);
    if (acrVersionInScratch != 0)
    {
        status = BOOTER_ERROR_BINARY_SEQUENCE_MISMATCH;
        goto cleanup;
    }
#else
	// All other Booter ucodes (Reload and Unload) should agree with Load's version
    if (myBooterVersion != booterVersionInScratch)
    {
        status = BOOTER_ERROR_BINARY_SEQUENCE_MISMATCH;
        goto cleanup;
    }
#endif

cleanup:
    statusCleanup = booterReleaseSelwreMutex_HAL(pBooter, SEC2_MUTEX_VPR_SCRATCH, vprMutexToken);
    return ((status != BOOTER_OK) ? status : statusCleanup);
}

BOOTER_STATUS
booterWriteBooterVersionToSelwreScratch_TU10X(LwU32 myBooterVersion)
{
#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    BOOTER_STATUS status, statusCleanup;
    LwU8 vprMutexToken;
    LwU32 reg;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterAcquireSelwreMutex_HAL(pBooter, SEC2_MUTEX_VPR_SCRATCH, &vprMutexToken));

    reg = BOOTER_REG_RD32(BAR0, LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14);

#if defined(BOOTER_LOAD)
    // Only Booter Load writes actual version
    reg = FLD_SET_DRF_NUM(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _BOOTER_BINARY_VERSION, myBooterVersion, reg);
#elif defined(BOOTER_UNLOAD)
    // Booter Unload clears version from scratch
    reg = FLD_SET_DRF_NUM(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _BOOTER_BINARY_VERSION, 0, reg);
#endif

    BOOTER_REG_WR32(BAR0, LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14, reg);

    statusCleanup = booterReleaseSelwreMutex_HAL(pBooter, SEC2_MUTEX_VPR_SCRATCH, vprMutexToken);
    return ((status != BOOTER_OK) ? status : statusCleanup);
#else  // defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    return BOOTER_OK;
#endif
}

BOOTER_STATUS
booterCheckBooterHandoffsInSelwreScratch_TU10X(void)
{
    BOOTER_STATUS status, statusCleanup;
    LwU8 vprMutexToken;
    LwU32 reg;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterAcquireSelwreMutex_HAL(pBooter, SEC2_MUTEX_VPR_SCRATCH, &vprMutexToken));

	reg = BOOTER_REG_RD32(BAR0, LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14);

#if defined(BOOTER_LOAD)
    // Booter Load is the first one, so all handoffs should have _VALUE_INIT
    if (!(FLD_TEST_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _BOOTER_LOAD_HANDOFF, _VALUE_INIT, reg) &&
          FLD_TEST_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _AHESASC_BOOTER_RELOAD_HANDOFF, _VALUE_INIT, reg) &&
          FLD_TEST_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _BOOTER_RELOAD_ACR_UNLOAD_HANDOFF, _VALUE_INIT, reg) &&
          FLD_TEST_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _ACR_UNLOAD_BOOTER_UNLOAD_HANDOFF, _VALUE_INIT, reg)))
    {
        status = BOOTER_ERROR_INCORRECT_BOOTER_HANDOFFS;
        goto cleanup;
    }
#elif defined(BOOTER_RELOAD)
    // Booter Reload expects Booter Load and AHESASC to have run already
    if (!(FLD_TEST_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _BOOTER_LOAD_HANDOFF, _VALUE_DONE, reg) &&
          FLD_TEST_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _AHESASC_BOOTER_RELOAD_HANDOFF, _VALUE_DONE, reg)))
    {
        status = BOOTER_ERROR_INCORRECT_BOOTER_HANDOFFS;
        goto cleanup;
    }
#elif defined(BOOTER_UNLOAD)
    // Booter Unload expects Booter Load, AHESASC, Booter Reload, and ACR Unload to have run already
    if (!(FLD_TEST_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _BOOTER_LOAD_HANDOFF, _VALUE_DONE, reg) &&
          FLD_TEST_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _AHESASC_BOOTER_RELOAD_HANDOFF, _VALUE_DONE, reg) &&
          FLD_TEST_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _BOOTER_RELOAD_ACR_UNLOAD_HANDOFF, _VALUE_DONE, reg) &&
          FLD_TEST_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _ACR_UNLOAD_BOOTER_UNLOAD_HANDOFF, _VALUE_DONE, reg)))
    {
        status = BOOTER_ERROR_INCORRECT_BOOTER_HANDOFFS;
        goto cleanup;
    }
#else
#error "Booter ucode not one of Booter Load, Booter Reload, Booter Unload"
#endif

cleanup:
    statusCleanup = booterReleaseSelwreMutex_HAL(pBooter, SEC2_MUTEX_VPR_SCRATCH, vprMutexToken);
    return ((status != BOOTER_OK) ? status : statusCleanup);
}

BOOTER_STATUS
booterWriteBooterHandoffsToSelwreScratch_TU10X(void)
{
    BOOTER_STATUS status, statusCleanup;
    LwU8 vprMutexToken;
    LwU32 reg;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterAcquireSelwreMutex_HAL(pBooter, SEC2_MUTEX_VPR_SCRATCH, &vprMutexToken));

	reg = BOOTER_REG_RD32(BAR0, LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14);

#if defined(BOOTER_LOAD)
    // Booter Load sets its own handoff bit
    reg = FLD_SET_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _BOOTER_LOAD_HANDOFF, _VALUE_DONE, reg);
#elif defined(BOOTER_RELOAD)
    // Booter Reload sets its own handoff bit
    reg = FLD_SET_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _BOOTER_RELOAD_ACR_UNLOAD_HANDOFF, _VALUE_DONE, reg);
#elif defined(BOOTER_UNLOAD)
    // Booter Unload clears all handoff bits
    reg = FLD_SET_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _BOOTER_LOAD_HANDOFF, _VALUE_INIT, reg);
    reg = FLD_SET_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _AHESASC_BOOTER_RELOAD_HANDOFF, _VALUE_INIT, reg);
    reg = FLD_SET_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _BOOTER_RELOAD_ACR_UNLOAD_HANDOFF, _VALUE_INIT, reg);
    reg = FLD_SET_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _ACR_UNLOAD_BOOTER_UNLOAD_HANDOFF, _VALUE_INIT, reg);
#else
#error "Booter ucode not one of Booter Load, Booter Reload, Booter Unload"
#endif

    BOOTER_REG_WR32(BAR0, LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14, reg);

    statusCleanup = booterReleaseSelwreMutex_HAL(pBooter, SEC2_MUTEX_VPR_SCRATCH, vprMutexToken);
    return ((status != BOOTER_OK) ? status : statusCleanup);
}


/*!
 * @brief Reads GSP-RM's WPR header into the provided buffer using the provided DMA prop
 */
BOOTER_STATUS
booterReadGspRmWprHeader_TU10X(GspFwWprMeta *pWprMeta, LwU64 addr, BOOTER_DMA_PROP *pDmaProp)
{
    LwU32 off = 0;

    //
    // Booter makes assumptions regarind GspFwWprMeta alignment (for DMA, etc.)
    // Fail loudly at compile-time if size ever changes so we can revisit.
    //
    ct_assert(sizeof(*pWprMeta) == 256);

    pDmaProp->baseAddr = (addr >> 8);  // baseAddr is 256 byte aligned

    if ((booterIssueDma_HAL(pBooter, pWprMeta, LW_FALSE, off, sizeof(*pWprMeta),
                            BOOTER_DMA_FROM_FB, BOOTER_DMA_SYNC_AT_END, pDmaProp)) != sizeof(*pWprMeta))
    {
        return BOOTER_ERROR_DMA_FAILURE;
    }

    return BOOTER_OK;
}

