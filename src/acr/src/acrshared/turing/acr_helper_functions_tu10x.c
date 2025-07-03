/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_helper_functions_tu10x.c
 */
//
// Includes
//
#include "acr.h"
#include "acrdrfmacros.h"
#include "acr_objacr.h"

#include "mmu/mmucmn.h"
#include "dev_top.h"
#include "dev_master.h"
#include "dev_fb.h"
#include "dev_fuse.h"
#include "dev_pwr_csb.h"
#include "dev_bus.h"
#include "sec2mutexreservation.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "dev_falcon_v4.h"
#include "dev_therm.h"
#include "dev_therm_addendum.h"

#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"

#if defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
#include "dev_sec_csb.h"
#endif

// Global variables
#if !defined(BSI_LOCK) && !defined(ACR_UNLOAD) && !defined(ACR_UNLOAD_ON_SEC2)
LSF_LSB_HEADER    g_lsbHeader          ATTR_OVLY(".data") ATTR_ALIGNED(LSF_LSB_HEADER_ALIGNMENT);
#endif
// Global variable for storing the original falcon reset mask
static LwU8               g_resetPLM            ATTR_OVLY(".data");
#if !defined(BSI_LOCK)
LwU8                      g_pWprHeader[LSF_WPR_HEADERS_TOTAL_SIZE_MAX]    ATTR_OVLY(".data") ATTR_ALIGNED(LSF_WPR_HEADER_ALIGNMENT);
ACR_DMA_PROP              g_dmaProp             ATTR_OVLY(".data") ATTR_ALIGNED(LSF_LSB_HEADER_ALIGNMENT);
#endif // When BSI_LOCK is not defined

/*
 * Save/Lock and Restore falcon reset when binary is running
 * TODO: Sanket : Use below enum instead of bool, and rename function to SaveLockRestore
 * enum
 * {
 *  lockFalconReset_SaveAndLock,
 *  lockFalconReset_Restore = 4,
 * } LOCK_FALCON_RESET;
 */
ACR_STATUS
acrSelfLockFalconReset_TU10X(LwBool bLock)
{
    LwU32 reg = 0;

    //
    // We are typecasting WRITE_PROTECTION field to LwU8 below but since the size of that field is controlled by hw,
    // the ct_assert below will help warn us if our assumption is proven incorrect in future
    //
    ct_assert((sizeof(g_resetPLM) * 8) >= DRF_SIZE(LW_CPWR_FALCON_RESET_PRIV_LEVEL_MASK_WRITE_PROTECTION));

#if defined(ACR_UNLOAD)
    reg = ACR_REG_RD32_STALL(CSB, LW_CPWR_FALCON_RESET_PRIV_LEVEL_MASK);
    if (bLock)
    {
        // Lock falcon reset for level 0, 1, 2
        g_resetPLM = (LwU8)(DRF_VAL( _CPWR, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION, reg));
        reg = FLD_SET_DRF( _CPWR, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, reg);
        reg = FLD_SET_DRF( _CPWR, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, reg);
        reg = FLD_SET_DRF( _CPWR, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _DISABLE, reg);
    }
    else
    {
        // Restore original mask
        reg = FLD_SET_DRF_NUM( _CPWR, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION, g_resetPLM, reg);
    }
    ACR_REG_WR32_STALL(CSB, LW_CPWR_FALCON_RESET_PRIV_LEVEL_MASK, reg);
#elif defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
    reg = ACR_REG_RD32_STALL(CSB, LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK);
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
    ACR_REG_WR32_STALL(CSB, LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK, reg);
#else
    // Control is not expected to come here. If you hit this assert, please check build profile config
    ct_assert(0);
#endif

    return ACR_OK;
}

/*!
 * @brief Reads WPR header into global buffer
 */
#if !defined(BSI_LOCK)
ACR_STATUS
acrReadWprHeader_TU10X(void)
{
    LwU32  wprSize  = LW_ALIGN_UP((sizeof(LSF_WPR_HEADER) * LSF_FALCON_ID_END), 
                                LSF_WPR_HEADER_ALIGNMENT);

    // Read the WPR header
    if ((acrIssueDma_HAL(pAcr, g_pWprHeader, LW_FALSE, 0, wprSize, 
            ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp)) != wprSize) 
    {
        return ACR_ERROR_DMA_FAILURE; 
    }

    return ACR_OK;
}
#endif

#ifndef BSI_LOCK
/*!
 * @brief Write failing falcon ID to mailbox1
 */
void
acrWriteFailingFalconIdToMailbox_TU10X(LwU32 falconId)
{
#if defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
    ACR_REG_WR32_STALL(CSB, LW_CSEC_FALCON_MAILBOX1, falconId);
#elif defined(ACR_UNLOAD)
    ACR_REG_WR32_STALL(CSB, LW_CPWR_FALCON_MAILBOX1, falconId);
#else
     ct_assert(0);
#endif
}
#endif

// Skipping this for GP10X BSI_LOCK through this define, as anyway BAR0 is not accessible to read mailbox and debug
#if !defined(BSI_LOCK) || !defined(UNDESIRABLE_OPTIMIZATION_FOR_BSI_RAM_SIZE)
/*!
 * @brief Write status into MAILBOX0
 * Pending: Clean-up after ACR_UNLOAD is moved to sec2 from PMU.
 * CL: https://lwcr.lwpu.com/sw/25021609/
 */
void
acrWriteStatusToFalconMailbox_TU10X(ACR_STATUS status)
{
#if defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
    ACR_REG_WR32_STALL(CSB, LW_CSEC_FALCON_MAILBOX0, status);
#elif defined(ACR_UNLOAD)
    ACR_REG_WR32_STALL(CSB, LW_CPWR_FALCON_MAILBOX0, status);
#else
    ct_assert(0);
#endif
}
#endif

#if !defined(BSI_LOCK) && !defined(ACR_UNLOAD) && !defined(ACR_UNLOAD_ON_SEC2)
/*!
 * @brief Reads LSB header into a global buffer which is then copied to local buffer
 *        pointed out by pLsbHeader. Global buffer is used to match the required
 *        alignment as enforced by falcon DMA engine.
 *
 * @param[in]  PLSF_WPR_HEADER WPR header for a particular falcon ID of interest
 * @param[out] PLSF_LSB_HEADER Output to which LSB header will be copied to
 */
ACR_STATUS
acrReadLsbHeader_TU10X
(
    PLSF_WPR_HEADER  pWprHeader,
    PLSF_LSB_HEADER  pLsbHeader
)
{
    LwU32            lsbSize = LW_ALIGN_UP(sizeof(LSF_LSB_HEADER), 
                               LSF_LSB_HEADER_ALIGNMENT);

    // Read the LSB  header
    if ((acrIssueDma_HAL(pAcr, &g_lsbHeader, LW_FALSE, pWprHeader->lsbOffset, lsbSize, 
             ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp)) != lsbSize)
    {
        return ACR_ERROR_DMA_FAILURE;
    }
 
    acrlibMemcpy_HAL(pAcrlib, pLsbHeader, &g_lsbHeader, sizeof(LSF_LSB_HEADER));
    return ACR_OK;
}
#endif // Not defined BSI_LOCK && ACR_UNLOAD && ACR_UNLOAD_ON_SEC2

ACR_STATUS
acrSaveWprInfoToBSISelwreScratch_TU10X()
{
    ACR_STATUS status           = ACR_OK;
    ACR_STATUS statusCleanup    = ACR_OK;
    LwU8       wprScratchMutexToken;

    if (ACR_OK != (status = acrAcquireSelwreMutex_HAL(pAcr, SEC2_MUTEX_WPR_SCRATCH, &wprScratchMutexToken)))
    {
        return status;
    }

    // TODO  for vyadav by GP104 PS: Use a loop on WPR region IDs to save WPR settings

    LwU32 wpr1StartAddr128K   = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR1_ADDR_LO);
    wpr1StartAddr128K         = DRF_VAL(_PFB, _PRI_MMU_WPR1_ADDR_LO, _VAL, 
                                        wpr1StartAddr128K) >> COLWERT_128K_ALIGNED_TO_4K_ALIGNED_SHIFT;
    
    LwU32 wpr1EndAddr128K     = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR1_ADDR_HI);
    wpr1EndAddr128K           = DRF_VAL(_PFB, _PRI_MMU_WPR1_ADDR_HI, _VAL, 
                                        wpr1EndAddr128K) >> COLWERT_128K_ALIGNED_TO_4K_ALIGNED_SHIFT;

    LwU32 wprAllowReadReg     = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_READ);
    LwU32 wprAllowWriteReg    = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_WRITE);

    LwU8 wpr1ReadPermissions  = DRF_VAL(_PFB, _PRI_MMU_WPR, _ALLOW_READ_WPR1, wprAllowReadReg);

    LwU8 wpr1WritePermissions = DRF_VAL(_PFB, _PRI_MMU_WPR, _ALLOW_WRITE_WPR1, wprAllowWriteReg);

    LwU16 wpr1Size128K = (wpr1EndAddr128K >= wpr1StartAddr128K) ? (wpr1EndAddr128K - wpr1StartAddr128K  + 1) : 0;

    //
    // ACR load binary lwrrently does not set up WPR2. Let's also not update WPR2 settings in secure scratch.
    // At one time, we had working code to save WPR2 settings. But we have removed that code. If anyone
    // needs that code back, please see function acrSaveWprInfoToBSISelwreScratch_GP10X in version#4 of
    // the following file: //sw/rel/gpu_drv/r367/r367_07/uproc/acr/src/acr/pascal/acrucgp10x.c.
    //

    if (wpr1Size128K)
    {
        LwU32 wprScratch9 = ACR_REG_RD32(BAR0, LW_PGC6_BSI_WPR_SELWRE_SCRATCH_9);
        wprScratch9 = FLD_SET_DRF_NUM(_PGC6, _BSI_WPR_SELWRE_SCRATCH_9, _WPR1_START_ADDR_128K_ALIGNED, wpr1StartAddr128K, wprScratch9);
        wprScratch9 = FLD_SET_DRF_NUM(_PGC6, _BSI_WPR_SELWRE_SCRATCH_9, _ALLOW_READ_WPR1,  wpr1ReadPermissions,  wprScratch9);
        wprScratch9 = FLD_SET_DRF_NUM(_PGC6, _BSI_WPR_SELWRE_SCRATCH_9, _ALLOW_WRITE_WPR1, wpr1WritePermissions, wprScratch9);

        ACR_REG_WR32(BAR0, LW_PGC6_BSI_WPR_SELWRE_SCRATCH_9, wprScratch9);
        if (wprScratch9 != ACR_REG_RD32(BAR0, LW_PGC6_BSI_WPR_SELWRE_SCRATCH_9))
        {
            status = ACR_ERROR_PRI_FAILURE;
            goto Cleanup;
        }
    }

    LwU32 wprScratch11  = ACR_REG_RD32(BAR0, LW_PGC6_BSI_WPR_SELWRE_SCRATCH_11);
    wprScratch11 = FLD_SET_DRF_NUM(_PGC6, _BSI_WPR_SELWRE_SCRATCH_11, _WPR1_SIZE_IN_128KB_BLOCKS, wpr1Size128K, wprScratch11);
    ACR_REG_WR32(BAR0, LW_PGC6_BSI_WPR_SELWRE_SCRATCH_11, wprScratch11);
    if (wprScratch11 != ACR_REG_RD32(BAR0, LW_PGC6_BSI_WPR_SELWRE_SCRATCH_11))
    {
        status = ACR_ERROR_PRI_FAILURE;
        goto Cleanup;
    }

Cleanup:
    statusCleanup = acrReleaseSelwreMutex_HAL(pAcr, SEC2_MUTEX_WPR_SCRATCH, wprScratchMutexToken);

    return (status != ACR_OK ? status : statusCleanup);
}


/*!
 * @brief Update ACR version based on which ACR binary is running
 * 1. ACR load binary expects no version in BSI_VPR_SELWRE_SCRATCH_14 to be programmed at the time of its invocation since it is expected to be the first binary of all the 4 ACR binaries to be ilwoked and will record its version
 *    number into BSI_VPR_SELWRE_SCRATCH_14
 * 2. ACR BSI lock and boot binaries expect ACR load to have run previously and so will ensure that version in BSI_VPR_SELWRE_SCRATCH_14 are matching their own version
 * 3. ACR BSI boot needs to ensure that it is run only after ACR BSI lock
 * 4. In order to support driver unload/reload, ACR unload wipes out the version in BSI_VPR_SELWRE_SCRATCH_14 to make the next ACR load have a clean slate but before doing so, it also ensures that matching ACR load has exelwted by
 *    comparing its own version to the one already programmed in BSI_VPR_SELWRE_SCRATCH_14
 */
ACR_STATUS
acrWriteAcrVersionToBsiSelwreScratch_TU10X(LwU32 myAcrSwVersion)
{
    ACR_STATUS      status, statusCleanup;
    LwU8            vprMutexToken;

    if (ACR_OK != (status = acrAcquireSelwreMutex_HAL(pAcr, SEC2_MUTEX_VPR_SCRATCH, &vprMutexToken)))
    {
        return status;
    }

    LwU32 vprScratch14          = ACR_REG_RD32(BAR0, LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14);
    LwU32 acrVersionInScratch   = DRF_VAL(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _ACR_BINARY_VERSION, vprScratch14);

    // AHESASC binary should be the first one, so the version should be 0
#ifdef AHESASC
    if (acrVersionInScratch != 0)
    {
        status = ACR_ERROR_BINARY_SEQUENCE_MISMATCH;
        goto Cleanup;
    }
#endif

    //
    // Unload binary should be run only if LOAD binary was run previously
    // And only if its version matches with LOAD binary
    //
#if defined(ASB) || defined(ACR_UNLOAD) || defined(ACR_UNLOAD_ON_SEC2)
    if (myAcrSwVersion != acrVersionInScratch)
    {
        status = ACR_ERROR_BINARY_SEQUENCE_MISMATCH;
        goto Cleanup;
    }
#endif

    //
    // To sync between binaries run durong BSi phases.
    // This check will ensure that ASB exelwtes only if BSI lock has exelwted.
    //
#if defined(ASB)
    if((ACR_OK == acrCheckIfGc6ExitIndeed_TU10X()) &&
       !FLD_TEST_DRF_DEF(BAR0, _THERM, _SELWRE_WR_SCRATCH_1, _ACR_BSI_LOCK_PHASE_TO_GC6_UDE_HANDOFF, _VAL_ACR_DONE))
    {
        status = ACR_ERROR_BINARY_SEQUENCE_MISMATCH;
        goto Cleanup;
    }
#endif
    // BSI binaries should be run after LOAD binary and its version should match with it
#ifdef BSI_LOCK
    if (myAcrSwVersion != acrVersionInScratch)
    {
        status = ACR_ERROR_BINARY_SEQUENCE_MISMATCH;
        goto Cleanup;
    }
#endif
    // To sync between BSI binaries, this check will ensure that BSI boot exelwtes only if BSI lock has exelwted
/*#if defined(ASB)
    if (!FLD_TEST_DRF_DEF(BAR0, _THERM, _SELWRE_WR_SCRATCH_1, _ACR_BSI_LOCK_PHASE_TO_GC6_UDE_HANDOFF, _VAL_ACR_DONE))
    {
        status = ACR_ERROR_BINARY_SEQUENCE_MISMATCH;
        goto Cleanup;
    }
#endif*/

    // Updating ACR version only in case of LOAD/UNLOAD binaries
#if !defined(BSI_LOCK) && !defined(ASB)
    // Load binary will update the expected version number, while unload binary is restoring it to 0
#ifdef AHESASC
    vprScratch14 = FLD_SET_DRF_NUM(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _ACR_BINARY_VERSION, myAcrSwVersion, vprScratch14);
#elif defined(ACR_UNLOAD) || defined(ACR_UNLOAD_ON_SEC2)
    vprScratch14 = FLD_SET_DRF_NUM(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _ACR_BINARY_VERSION, 0, vprScratch14);
#else
    ct_assert(0);
#endif

    ACR_REG_WR32(BAR0, LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14, vprScratch14);
#endif

Cleanup:
    statusCleanup = acrReleaseSelwreMutex_HAL(pAcr, SEC2_MUTEX_VPR_SCRATCH, vprMutexToken);

    return (status != ACR_OK ? status : statusCleanup);
}

/*!
 * @brief ACR routine to check if falcon is in DEBUG mode or not
 */
LwBool
acrIsDebugModeEnabled_TU10X(void)
{
    LwU32   scpCtl = 0;

#if defined(ACR_UNLOAD)
    scpCtl = ACR_REG_RD32_STALL(CSB, LW_CPWR_PMU_SCP_CTL_STAT);
#elif defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
    scpCtl = ACR_REG_RD32_STALL(CSB, LW_CSEC_SCP_CTL_STAT);
#else
    // Control is not expected to come here. If you hit this assert, please check bumodeild profile config
    ct_assert(0);
#endif

    // Using CPWR define since we dont have in falcon manual, Filed hw bug 200179864
    return !FLD_TEST_DRF( _CPWR, _PMU_SCP_CTL_STAT, _DEBUG_MODE, _DISABLED, scpCtl);
}

ACR_STATUS
acrGetLwrMemLockRange_TU10X
(
    LwU64 *pLwrMemLockRangeStartAddrInBytes,
    LwU64 *pLwrMemLockRangeEndAddrInBytes
)
{
    LwU32 lwrMemlockRangeStartAddr4K = 0;
    LwU32 lwrMemlockRangeEndAddr4K   = 0;
    
    if (NULL == pLwrMemLockRangeStartAddrInBytes ||
        NULL == pLwrMemLockRangeEndAddrInBytes)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }
    lwrMemlockRangeStartAddr4K = DRF_VAL(_PFB, _PRI_MMU_LOCK_ADDR_LO, _VAL,
                                         ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_LOCK_ADDR_LO));
    
    lwrMemlockRangeEndAddr4K =   DRF_VAL(_PFB, _PRI_MMU_LOCK_ADDR_HI, _VAL,
                                         ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_LOCK_ADDR_HI));

    *pLwrMemLockRangeStartAddrInBytes = ((LwU64)lwrMemlockRangeStartAddr4K) << LW_PFB_PRI_MMU_LOCK_ADDR_LO_ALIGNMENT;
    
    *pLwrMemLockRangeEndAddrInBytes   = ((LwU64)lwrMemlockRangeEndAddr4K) << LW_PFB_PRI_MMU_LOCK_ADDR_LO_ALIGNMENT ;
    *pLwrMemLockRangeEndAddrInBytes  |= ((NUM_BYTES_IN_4_KB) - 1);
    
    return ACR_OK;
}
