/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: HS code of scrub binary
 */
/* ------------------------- System Includes -------------------------------- */
#include "selwrescrub.h"
#include "sec2mutexreservation.h"
#include "config/g_selwrescrub_private.h"
#include "dev_lwdec_csb.h"
#include "dev_lwdec_csb_addendum.h"
#include "selwrescrubreg.h"

#ifdef BOOT_FROM_HS_BUILD
#include "hs_entry_checks.h"
#endif

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
//
// @brief: Scrub buffer init value
//

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */

#ifdef IS_SSP_ENABLED

// variable to store canary for SSP
extern void * __stack_chk_guard;

// Local implementation of SSP handler
void __stack_chk_fail(void)
{
    // Update error code
    falc_stxb_i((LwU32*) LW_CLWDEC_FALCON_MAILBOX0, 0,  SELWRESCRUB_RC_ERROR_SSP_STACK_CHECK_FAILED);
    INFINITE_LOOP();
}

#ifndef BOOT_FROM_HS_BUILD
//
// Initialize canary for HS
// HS canary is initialized to random generated number (TRNG) in HS using SCP
//
GCC_ATTRIB_ALWAYSINLINE()
static inline
SELWRESCRUB_RC
selwrescrubInitializeStackCanaryHSUsingSCP(void)
{
    SELWRESCRUB_RC status = SELWRESCRUB_RC_OK;
    LwU32          rand32;

    //
    // Set the value of stack canary to a random value to ensure adversary
    // can not craft an attack by looking at the assembly to determine the canary value
    //
    if (SELWRESCRUB_RC_OK != (status = selwrescrubScpGetRandomNumber_HAL(pSelwrescrub, &rand32)))
    {
        goto ErrorExit;
    }

    __stack_chk_guard = (void *)rand32;

ErrorExit:
    return status;
}
#endif //BOOT_FROM_HS_BUILD
#endif  //IS_SSP_ENABLED


#ifndef BOOT_FROM_HS_BUILD
/*!
 * @brief Stack Underflow Exception Installer for enabling the stack underflow exception in secure segment by
 * setting up the stackcfg register with stackbottom value.
 * TODO: User falc_ldxb/stxb_i instruction here because there should be no falc_halt till VHR check because
 * FALC_REG_RD32/WR32 macros have falc_halt in there code.
 *
 * @param None.
 *
 * @return None.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
void
selwrescrubStackUnderflowInstaller(void)
{
    LwU32 stackBottomLimit = SELWRESCRUB_STACK_BOTTOM_LIMIT;
    LwU32 var = 0;
    //
    // STACKCFG.BOTTOM is word address, if the SP is equal or less than (STACKCFG.BOTTOM << 2),
    // then an exception will be generated. Therefore pushing stackBottomLimit >> 2.
    //
    stackBottomLimit = stackBottomLimit >> 2;
    var = FALC_CSB_REG_RD32_INLINE(LW_CLWDEC_FALCON_STACKCFG);
    var = FLD_SET_DRF_NUM( _CLWDEC, _FALCON_STACKCFG, _BOTTOM, stackBottomLimit, var );
    var = FLD_SET_DRF( _CLWDEC, _FALCON_STACKCFG, _SPEXC, _ENABLE, var );
    FALC_CSB_REG_WR32_INLINE(LW_CLWDEC_FALCON_STACKCFG, var);
}

/*!
 * Exception handler which will reside in the secure section. This subroutine will be exelwted whenever exception is triggered.
 */
void selwrescrubExceptionHandlerSelwreHang(void)
{
    // coverity[no_escape]
    while(1);
}

/*!
 * Exception Installer which will install a minimum exception handler which just HALT's.
 * This subroutine sets up the Exception vector and other required registers for each exceptions. This will be built when we want to build secure ucode.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
void selwrescrubExceptionInstallerSelwreHang(void)
{
    LwU32 secSpr = 0;

    FALC_CSB_REG_WR32_INLINE( LW_CLWDEC_FALCON_IBRKPT1, 0x00000000 );
    FALC_CSB_REG_WR32_INLINE( LW_CLWDEC_FALCON_IBRKPT2, 0x00000000 );
    FALC_CSB_REG_WR32_INLINE( LW_CLWDEC_FALCON_IBRKPT3, 0x00000000 );
    FALC_CSB_REG_WR32_INLINE( LW_CLWDEC_FALCON_IBRKPT4, 0x00000000 );
    FALC_CSB_REG_WR32_INLINE( LW_CLWDEC_FALCON_IBRKPT5, 0x00000000 );

    // Clear IE0 bit in CSW.
    falc_sclrb( 16 );

    // Clear IE1 bit in CSW.
    falc_sclrb( 17 );

    // Clear IE2 bit in CSW.
    falc_sclrb( 18 );

    // Clear Exception bit in CSW.
    falc_sclrb( 24 );

    falc_rspr( secSpr, SEC );

    // Clear DISABLE_EXCEPTIONS
    secSpr = FLD_SET_DRF( _FALC, _SEC_SPR, _DISABLE_EXCEPTIONS, _CLEAR, secSpr );
    falc_wspr( SEC, secSpr );
    falc_wspr( EV, selwrescrubExceptionHandlerSelwreHang );
}

/*!
 *  Check if CSBERRSTAT_ENABLE bit is TRUE, else set CSBERRSTAT_ENABLE to TRUE
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
void _selwrescrubValidateCsberrstat(void)
{
    LwU32 csbErrorStatVal = 0;
    csbErrorStatVal = FALC_CSB_REG_RD32_INLINE(LW_CLWDEC_FALCON_CSBERRSTAT);
    csbErrorStatVal = FLD_SET_DRF(_CLWDEC, _FALCON_CSBERRSTAT, _ENABLE, _TRUE, csbErrorStatVal);
    FALC_CSB_REG_WR32_INLINE(LW_CLWDEC_FALCON_CSBERRSTAT, csbErrorStatVal);
}

// Clear all falcon GPRs
GCC_ATTRIB_ALWAYSINLINE()
static inline
void
_selwrescrubClearFalconGprs(void)
{
    // Clearing all falcon GPRs
    asm volatile
    (
        "clr.w a0;"
        "clr.w a1;"
        "clr.w a2;"
        "clr.w a3;"
        "clr.w a4;"
        "clr.w a5;"
        "clr.w a6;"
        "clr.w a7;"
        "clr.w a8;"
        "clr.w a9;"
        "clr.w a10;"
        "clr.w a11;"
        "clr.w a12;"
        "clr.w a13;"
        "clr.w a14;"
        "clr.w a15;"
    );
}

// Clear all SCP registers
GCC_ATTRIB_ALWAYSINLINE() 
static inline void _selwrescrubClearSCPRegisters(void)
{
    // First load secret index0 before clearing all SCP registers
    falc_scp_secret(0, SCP_R0);
    falc_scp_xor(SCP_R0, SCP_R0);

    falc_scp_secret(0, SCP_R1);
    falc_scp_xor(SCP_R1, SCP_R1);

    falc_scp_secret(0, SCP_R2);
    falc_scp_xor(SCP_R2, SCP_R2);

    falc_scp_secret(0, SCP_R3);
    falc_scp_xor(SCP_R3, SCP_R3);

    falc_scp_secret(0, SCP_R4);
    falc_scp_xor(SCP_R4, SCP_R4);

    falc_scp_secret(0, SCP_R5);
    falc_scp_xor(SCP_R5, SCP_R5);

    falc_scp_secret(0, SCP_R6);
    falc_scp_xor(SCP_R6, SCP_R6);

    falc_scp_secret(0, SCP_R7);    
    falc_scp_xor(SCP_R7, SCP_R7);
}

/*!
 * Function to scrub BSS and Unused DMEM 
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
void
selwrescrubScrubDmem(LwU32 startAddr, LwU32 endAddr)
{
    LwU32 *pDmem = NULL;

    // Scrub DMEM from _bss_start to current SP
    for (pDmem =  (LwU32*)startAddr;
         pDmem < (LwU32*)endAddr;
         pDmem++)
    {
        *pDmem = 0x0;
    }
}

/*!
 * Wrapper function to cler SCP, set Forget_Sig and call SELWRESCRUB Entry function.
 */
GCC_ATTRIB_NO_STACK_PROTECT()
void
selwrescrubEntryWrapper(void)
{
    // initialize SP to DMEM size.
    falc_wspr(SP, LW_CLWDEC_FLCN_DMEM_SIZE_IN_BLK << LW_CLWDEC_FLCN_DMEM_BLK_ALIGN_BITS );

    // Clear SCP signature
    falc_scp_forget_sig();

    //clear all SCP registers
    _selwrescrubClearSCPRegisters();

    // Reset all falcon GPRs (a0-a15)
    _selwrescrubClearFalconGprs();

    // Validate CSBERRSTAT register for pri error handling.
    _selwrescrubValidateCsberrstat();

    {
        LwU32 lwrrentSP = 0;

        falc_rspr( lwrrentSP, SP);

        // Scrub BSS and Unused Dmem upto current SP
        selwrescrubScrubDmem((LwU32)_bss_start, lwrrentSP);
    }

    // Make sure different initiator cannot restart Scrubber in case it halts in HS mode  
    selwrescrubMitigateNSRestartFromHS_GV100();

    // Setup the exception handler
    selwrescrubExceptionInstallerSelwreHang();

    // Set the stack pointer to the MIN addr so if stack reaches that address then falcon can throw an exception.
    selwrescrubStackUnderflowInstaller();

    // Reset SCP's both instuction sequencers and pipe to put SCP in a more deterministic state
    selwrescrubResetScpLoopTracesAndPipe_HAL();

#ifdef IS_SSP_ENABLED
    {
        SELWRESCRUB_RC  status = SELWRESCRUB_RC_ERROR;

        // SCP based TRNG for SSP. We reinitialize stack canary for HS after NS
        if (SELWRESCRUB_RC_OK != (status = selwrescrubInitializeStackCanaryHSUsingSCP()))
        {
            falc_stxb_i((LwU32*) LW_CLWDEC_FALCON_MAILBOX0, 0, status);
            falc_halt();
        }
    }
#endif  //IS_SSP_ENABLED

    // This is the function where actual Selwrescrub starts.
    selwrescrubEntry();

    // Binary would never come here. Putting falc_halt for safety certification issues.
    falc_halt();
}

#endif //BOOT_FROM_HS_BUILD

/*!
 * @brief Cleanup and halt.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
void selwrescrubDoFalconHalt(void)
{
    falc_halt();
}

/* 
 * @brief: Entry of scrub binary. This is the first function to execute in HS.
 */
void
selwrescrubEntry(void)
{
    SELWRESCRUB_RC  status          = SELWRESCRUB_RC_OK;
    SELWRESCRUB_RC  statusCleanup   = SELWRESCRUB_RC_OK;

    VPR_ACCESS_CMD vprRangeMMU;
    VPR_ACCESS_CMD vprCapsBsiScratch;

    LwBool         bCheckWpr2WriteProtection;
    LwBool         bCheckMemLockRangeProtection;
    LwBool         bIsDispEngineEnabledInFuse;
    LwU32          syncScrubSizeMB;
    FLCNMUTEX      vprScratchMutex         = {SEC2_MUTEX_VPR_SCRATCH,          0, LW_FALSE};
    FLCNMUTEX      mmuVprWprWriteMutex     = {SEC2_MUTEX_MMU_VPR_WPR_WRITE,    0, LW_FALSE};
    LwU32          secSprDisableExceptions = 0;


#ifndef BOOT_FROM_HS_BUILD
    // Set BAR0 Timeout
    selwrescrubSetBar0Timeout_HAL();
#endif

    // Check revocation fuses and return early if we have been revoked.
    if (SELWRESCRUB_RC_OK != (status = selwrescrubRevokeHsBin_HAL(pSelwrescrub)))
    {
        goto CLEANUP_REPORT_STATUS_AND_HALT;
    }

#if BOOT_FROM_HS_BUILD
    // Check whether Bootstrap and FWSEC have run successfully to ensure Chain of Trust.
    if (SELWRESCRUB_RC_OK != (status = selwrescrubCheckChainOfTrust_HAL(pSelwrescrub)))
    {
        goto CLEANUP_REPORT_STATUS_AND_HALT;
    }
#endif

    // Check if priv sec is enabled on Prod Board
    if (SELWRESCRUB_RC_OK != (status = selwrescrubCheckIfPrivSecEnabledOnProd_HAL()))
    {
        goto CLEANUP_REPORT_STATUS_AND_HALT;
    }

    // Ilwalidate IMEM Blocks.
    selwrescrubValidateBlocks_HAL(pSelwrescrub);

    //
    // Acquire SEC2_MUTEX_VPR_SCRATCH mutex here so we can update the scrubber binary
    // version in LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15_SCRUBBER_BINARY_VERSION
    //
    if (SELWRESCRUB_RC_OK != (status = selwrescrubAcquireSelwreMutex_HAL(pSelwrescrub, &vprScratchMutex)))
    {
        goto CLEANUP_REPORT_STATUS_AND_HALT;
    }

    // Write scrubber binary version to secure scratch register and immediately release the VPR_SCRATCH mutex.
    if (SELWRESCRUB_RC_OK != (status = selwrescrubWriteScrubberBilwersionToBsiSelwreScratch_HAL()) ||
        SELWRESCRUB_RC_OK != (status = selwrescrubReleaseSelwreMutex_HAL(pSelwrescrub,&vprScratchMutex)))
    {
        goto CLEANUP_REPORT_STATUS_AND_HALT;
    }

    // Upgrade write protection of PDISP_SELWRE_SCRATCH(0) only if DISP engine is enabled
    bIsDispEngineEnabledInFuse = selwrescrubIsDispEngineEnabledInFuse_HAL();
    if (bIsDispEngineEnabledInFuse)
    {
        if (SELWRESCRUB_RC_OK != (status = selwrescrubUpgradeDispSelwreScratchWriteProtection_HAL()))
        {
            goto CLEANUP_REPORT_STATUS_AND_HALT;
        }
    }
    //
    // Program decode traps so FECS writes to VPR_WPR_WRITE registers get promoted to level3.
    // TODO: This will probably change in Volta, so we would need to fork a GP10X & Volta HAL for this.
    // Scrubber binary still does not have full HAL support. Going forward, we need to decide if we
    // want to keep this binary alive or merge this code into ACR binary.
    // If this binary stays alive, then full HAL support implementation would be imminent.
    //
    if (SELWRESCRUB_RC_OK != (status = selwrescrubProgramRequiredDecodeTraps_HAL()))
    {
        goto CLEANUP_REPORT_STATUS_AND_HALT;
    }

    if (selwrescrubInGC6Exit_HAL())
    {
        status = SELWRESCRUB_RC_OK;
        goto CLEANUP_REPORT_STATUS_AND_HALT;
    }

    //
    // Before we change system state - do FB scrub, release VPR range etc,
    // We have to validate that we have not been ilwoked by attacker.
    // We will check the devinit handoff and return early if we don't see handoff from devinit.
    //
    if (SELWRESCRUB_RC_OK != (status = selwrescrubCheckDevinitHandoffIsAsExpected_HAL()))
    {
        goto CLEANUP_REPORT_STATUS_AND_HALT;
    }

    // Read VPR range from MMU
    vprRangeMMU.cmd = VPR_SETTINGS_GET;
    selwrescrubGetSetVprMmuSettings_HAL(pSelwrescrub,&vprRangeMMU);

    // Get VPR region info saved in BSI secure scratch
    if (SELWRESCRUB_RC_OK != (status = selwrescrubGetVPRRangeFromBsiScratch_HAL(pSelwrescrub, &vprCapsBsiScratch)))
    {
        goto CLEANUP_REPORT_STATUS_AND_HALT;
    }

    // Make sure VPR enable state matches in both MMU and BSI scratch
    if (vprRangeMMU.bVprEnabled != vprCapsBsiScratch.bVprEnabled)
    {
        // If VPR enabled status is mismatching in MMU v/s BSI, this is a serious error. Don't proceed.
        status = SELWRESCRUB_RC_VPR_ENABLE_STATE_MISMATCH_BETWEEN_MMU_AND_BSI;
        goto CLEANUP_REPORT_STATUS_AND_HALT;
    }

    if (vprRangeMMU.bVprEnabled)
    {
        // Get size of VPR region that was synchronously scrubbed by vbios
        if (SELWRESCRUB_RC_OK != (status = selwrescrubGetVbiosVprSyncScrubSize_HAL(pSelwrescrub, &syncScrubSizeMB)))
        {
            goto CLEANUP_REPORT_STATUS_AND_HALT;
        }

        if (vprRangeMMU.vprRangeStartInBytes > vprRangeMMU.vprRangeEndInBytes)
        {
            // Start has to be <= End. Otherwise it is not a valid range.
            status = SELWRESCRUB_RC_ILWALID_RANGE;
            goto CLEANUP_REPORT_STATUS_AND_HALT;
        }

        if ((vprRangeMMU.vprRangeStartInBytes != vprCapsBsiScratch.vprRangeStartInBytes) ||
            ((vprRangeMMU.vprRangeEndInBytes + ((LwU64)syncScrubSizeMB << VPR_ADDR_ALIGN_IN_BSI_1MB_SHIFT)) != vprCapsBsiScratch.vprRangeEndInBytes))
        {
            // report error to RM
            status = SELWRESCRUB_RC_VPR_RANGE_MISMATCH_BETWEEN_MMU_AND_BSI;
            goto CLEANUP_REPORT_STATUS_AND_HALT;
        }

        //
        // Check alignment of start and end addresses. We need to ensure correct alignment
        // because of the way we're treating the addresses in the code while scrubbing.
        // VPR start address     should be 1 MB aligned.
        // (VPR end address + 1) should be 1 MB aligned.
        // For E.g.:
        // Typical start Address here is: 0x78000000
        // Typical end   Address here is: 0xf80fffff
        //
        if ( (vprRangeMMU.vprRangeStartInBytes      & ((NUM_BYTES_IN_1_MB) - 1)) ||
             ((vprRangeMMU.vprRangeEndInBytes + 1)  & ((NUM_BYTES_IN_1_MB) - 1)))
         {
            status = SELWRESCRUB_RC_VPR_RANGE_NOT_PROPERLY_ALIGNED;
            goto CLEANUP_REPORT_STATUS_AND_HALT;
         }

        // Validate VPR has been scrubbed
        if (SELWRESCRUB_RC_OK != (status = selwrescrubValidateVprHasBeenScrubbed_HAL(pSelwrescrub, vprRangeMMU.vprRangeStartInBytes, vprRangeMMU.vprRangeEndInBytes)))
        {
            // Abort region unlock
            goto CLEANUP_REPORT_STATUS_AND_HALT;
        }

        //
        // Validate WPR2/Memlock range write protection to match the VPR range
        // The write protected range will cover the entire VPR range. If not,
        // that is a sign of a problem. We will fail here and not release VPR
        // range in that case
        //
        if (SELWRESCRUB_RC_OK != (status = selwrescrubValidateWriteProtectionIsSetOlwPRRange_HAL(pSelwrescrub,vprRangeMMU.vprRangeStartInBytes,vprRangeMMU.vprRangeEndInBytes, &bCheckWpr2WriteProtection, &bCheckMemLockRangeProtection)))
        {
            goto CLEANUP_REPORT_STATUS_AND_HALT;
        }
    }

    //
    // We have validated everything that is required and now ready to change system state.
    //
    //
    // Acquire SEC2_MUTEX_VPR_SCRATCH mutex again here, so we can update
    // LW_PGC6_BSI_VPR_SELWRE_SCRATCH_* registers
    //
    if (SELWRESCRUB_RC_OK != (status = selwrescrubAcquireSelwreMutex_HAL(pSelwrescrub,&vprScratchMutex)))
    {
        goto CLEANUP_REPORT_STATUS_AND_HALT;
    }

    // Update the handoff value to signal that we are taking over.
    if (SELWRESCRUB_RC_OK != (status = selwrescrubUpdateHandoffValScrubberBinTakingOver_HAL()))
    {
        goto CLEANUP_REPORT_STATUS_AND_HALT;
    }

    //
    // Start VPR free sequence now that the VPR scrub completion has been validated. This
    // should be done only if VPR is actually there
    //
    if (vprRangeMMU.bVprEnabled)
    {
        // Acquire mutex before updating VPR related settings in MMU registers
        if (SELWRESCRUB_RC_OK != (status = selwrescrubAcquireSelwreMutex_HAL(pSelwrescrub,&mmuVprWprWriteMutex)))
        {
            goto CLEANUP_REPORT_STATUS_AND_HALT;
        }
    
        // Remove write protection - WPR2/memory lock range.
        if (SELWRESCRUB_RC_OK != (status = selwrescrubRemoveWriteProtection_HAL(pSelwrescrub,bCheckWpr2WriteProtection, bCheckMemLockRangeProtection)))
        {
            goto CLEANUP_REPORT_STATUS_AND_HALT;
        }
    
        // Unlock VPR range
        vprRangeMMU.cmd                     = VPR_SETTINGS_SET;
        vprRangeMMU.vprRangeStartInBytes    = ILWALID_VPR_ADDR_LO_IN_BYTES;
        vprRangeMMU.vprRangeEndInBytes      = ILWALID_VPR_ADDR_HI_IN_BYTES;
        vprRangeMMU.bVprEnabled             = LW_FALSE;
        selwrescrubGetSetVprMmuSettings_HAL(pSelwrescrub,&vprRangeMMU);
    }

    if (bIsDispEngineEnabledInFuse)
    {
        // Now that VPR has been scrubbed:

        // Clear the VPR display blanking policy.
        selwrescrubClearVprBlankingPolicy_HAL();

        // Enable Display CRC.
        selwrescrubEnableDisplayCRC_HAL();

        // Release the HDCP2.2 type1 lock in DISP secure scratch register
        if (SELWRESCRUB_RC_OK != (status = selwrescrubReleaseType1LockInDispScratch_HAL()))
        {
            goto CLEANUP_REPORT_STATUS_AND_HALT;
        }
    }

    // Release the HDCP2.2 type1 lock in BSI secure scratch register
    selwrescrubReleaseType1LockInBsiScratch_HAL();

    //
    // Last step is to update handoff bits to indicate that scrubber bin is done.
    // This is a secure handoff to other parts of SW to indicate that scrubber bin is done.
    //
    selwrescrubUpdateHandoffValScrubberBinDone_HAL();

CLEANUP_REPORT_STATUS_AND_HALT:
    if (LW_TRUE == mmuVprWprWriteMutex.bValid)
    {
        statusCleanup = selwrescrubReleaseSelwreMutex_HAL(pSelwrescrub,&mmuVprWprWriteMutex);
    }

    if (LW_TRUE == vprScratchMutex.bValid)
    {
        statusCleanup = selwrescrubReleaseSelwreMutex_HAL(pSelwrescrub,&vprScratchMutex);
    }

    // report status
    selwrescrubReportStatus_HAL(pSelwrescrub, status != SELWRESCRUB_RC_OK ? status : statusCleanup, 0);

#if BOOT_FROM_HS_BUILD
    hsClearSCPRegisters();

    hsResetScpLoopTracesAndPipe();
#else
    //clear all SCP registers
    _selwrescrubClearSCPRegisters();

    // Reset SCP's both instuction sequencers and pipe to put SCP in a more deterministic state
    selwrescrubResetScpLoopTracesAndPipe_HAL();
#endif
    //
    // Leaving intrs/exceptions may create avenues to jump back into HS, although there is no
    // identified vulnerability, there is no good reason to leave intrs/exceptions enabled.
    //
    falc_rspr( secSprDisableExceptions, SEC );
    // SET DISABLE_EXCEPTIONS
    secSprDisableExceptions = FLD_SET_DRF( _FALC, _SEC_SPR, _DISABLE_EXCEPTIONS, _SET, secSprDisableExceptions );
    falc_wspr( SEC, secSprDisableExceptions );


#if BOOT_FROM_HS_BUILD
    hsScrubDmem(SELWRESCRUB_DMEM_START_ADDR, (LW_CLWDEC_FLCN_DMEM_SIZE_IN_BLK << LW_CLWDEC_FLCN_DMEM_BLK_ALIGN_BITS));

    // Reset all falcon GPRs (a0-a15)
    hsClearFalconGprs();
#else
    selwrescrubScrubDmem(SELWRESCRUB_DMEM_START_ADDR, (LW_CLWDEC_FLCN_DMEM_SIZE_IN_BLK << LW_CLWDEC_FLCN_DMEM_BLK_ALIGN_BITS));

    // Reset all falcon GPRs (a0-a15)
    _selwrescrubClearFalconGprs();

#endif // BOOT_FROM_HS_BUILD

    // We are halting in HS mode itself, for maximum security.
    selwrescrubDoFalconHalt();
}

