/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017 - 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
*/

/*!
 * @file: fubinit.c
 */

//
// Includes
//
#include "fub.h"
#include "fubreg.h"
#include "config/g_fub_private.h"
#include "config/fub-config.h"
#include <falcon-intrinsics.h>
#include "fub_inline_static_funcs_tu10x.h"
#if (FUBCFG_FEATURE_ENABLED(FUBCORE_TU10X) || FUBCFG_FEATURE_ENABLED(FUBCORE_TU116))
#include "fub_inline_static_funcs_fpf_tu10x.h"
#include "fub_inline_static_funcs_fpf_tu116.h"
#include "fub_inline_static_funcs_tu116.h"
#endif
#if (FUBCFG_FEATURE_ENABLED(FUBCORE_GA10X))
#include "fub_inline_static_funcs_ga100.h"
#endif
#ifdef FUB_ON_LWDEC
#include "dev_lwdec_csb.h"
#elif FUB_ON_SEC2
#include "dev_sec_csb.h"
#else
ct_assert(0);
#endif
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Extern Variables ------------------------------- */
extern RM_FLCN_FUB_DESC  g_desc;
extern LwBool            g_canFalcHalt;
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */

#ifdef IS_SSP_ENABLED
extern void * __stack_chk_guard;
//
// Local implementation of SSP handler
// This function is called by stack checking code if the guard variable is corrupted indicating
// that stack buffer is overrun.
//
GCC_ATTRIB_NO_STACK_PROTECT()
void __stack_chk_fail(void)
{
    //
    // Update error code
    // We cannot use FALC_FUB_WITH_ERROR here as SSP protection start early even before CSBERRSTAT is checked
    // and also we cannot halt since NS restart and VHR empty check could be remaining
    //
#if defined(FUB_ON_SEC2)
    falc_stxb_i((LwU32*) LW_CSEC_FALCON_MAILBOX0, 0, FUB_ERR_SSP_STACK_CHECK_FAILED);
#elif defined(FUB_ON_LWDEC)
    falc_stxb_i((LwU32*) LW_CLWDEC_FALCON_MAILBOX0, 0, FUB_ERR_SSP_STACK_CHECK_FAILED);
#else
    ct_assert(0);
#endif
    INFINITE_LOOP();
}

//
// ReInitialize canary for HS
// HS canary is initialized to random generated number (TRNG) in HS using SCP
//
GCC_ATTRIB_ALWAYSINLINE()
static inline void fubInitializeStackCanaryHSUsingSCP(void)
{
    LwU32      rand32    = 0;
    FUB_STATUS fubStatus = FUB_ERR_GENERIC;

    //
    // Set the value of stack canary to a random value to ensure adversary
    // can not craft an attack by looking at the assembly to determine the canary value
    //
    if (FUB_OK != (fubStatus = fubScpGetRandomNumber_HAL(pFub, &rand32)))
    {
        FAIL_FUB_WITH_ERROR(fubStatus);
    }

    __stack_chk_guard = (void *)rand32;
}
#endif


// Clear all SCP registers
GCC_ATTRIB_ALWAYSINLINE()
static inline
void
fubClearSCPRegisters(void)
{
    // Clear all SCP registers
    falc_scp_xor(SCP_R0, SCP_R0);
    falc_scp_xor(SCP_R1, SCP_R1);
    falc_scp_xor(SCP_R2, SCP_R2);
    falc_scp_xor(SCP_R3, SCP_R3);
    falc_scp_xor(SCP_R4, SCP_R4);
    falc_scp_xor(SCP_R5, SCP_R5);
    falc_scp_xor(SCP_R6, SCP_R6);
    falc_scp_xor(SCP_R7, SCP_R7);    
}

// Clear all falcon GPRs
GCC_ATTRIB_ALWAYSINLINE()
static inline
void
fubClearFalconGprs(void)
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

/*!
 * @brief Reset all SCP GPRs, falcon GPRs and halt
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
void
fubCleanupAndHalt(void)
{
    // Reset all SCP registers.
    fubClearSCPRegisters();

    // Reset all falcon GPRs (a0-a15)
    fubClearFalconGprs();

    // NOTE: Do not put any C code above this as GPRs are cleared
    falc_halt();
}


/*!
 * @brief Stack Underflow Exception Installer for enabling the stack underflow exception in secure segment by
 * setting up the stackcfg register with stackbottom value.
 * Using falc_ldxb/stxb_i instruction here because there should be no falc_halt till VHR check and
 * FALC_REG_RD32/WR32 macros have falc_halt in there code.
 *
 * @param None.
 *
 * @return None.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline 
FUB_STATUS 
fubStackUnderflowInstaller(void)
{
    LwU32      stackBottomLimit = FUB_STACK_BOTTOM_LIMIT;
    LwU32      var              = 0;
    FUB_STATUS fubStatus        = FUB_ERR_GENERIC;
    //
    // STACKCFG.BOTTOM is word address, if the SP is equal or less than (STACKCFG.BOTTOM << 2), 
    // then an exception will be generated. Therefore pushing stackBottomLimit >> 2.
    //
    stackBottomLimit = stackBottomLimit >> 2;
#if defined(FUB_ON_SEC2)
    CHECK_FUB_STATUS(FALC_REG_RD32(CSB, LW_CSEC_FALCON_STACKCFG, &var));
    var = FLD_SET_DRF_NUM( _CSEC, _FALCON_STACKCFG, _BOTTOM, stackBottomLimit, var);
    var = FLD_SET_DRF( _CSEC, _FALCON_STACKCFG, _SPEXC, _ENABLE, var );
    CHECK_FUB_STATUS(FALC_REG_WR32(CSB, LW_CSEC_FALCON_STACKCFG, var));
#elif defined(FUB_ON_LWDEC)
    CHECK_FUB_STATUS(FALC_REG_RD32(CSB, LW_CLWDEC_FALCON_STACKCFG, &var));
    var = FLD_SET_DRF_NUM( _CLWDEC, _FALCON_STACKCFG, _BOTTOM, stackBottomLimit, var );
    var = FLD_SET_DRF( _CLWDEC, _FALCON_STACKCFG, _SPEXC, _ENABLE, var );
    CHECK_FUB_STATUS(FALC_REG_WR32(CSB, LW_CLWDEC_FALCON_STACKCFG, var));
#endif
ErrorExit:
    return fubStatus;
}

/*!
 * Exception handler which will reside in the secure section. This subroutine will be exelwted whenever exception is triggered.
 */
void fubExceptionHandlerSelwreHang(void)
{
    // coverity[no_escape]
    INFINITE_LOOP();
}

/*!
 * Exception Installer which will install a minimum exception handler which just HALT's.
 * This subroutine sets up the Exception vector and other required registers for each exceptions. This will be built when we want to build secure ucode.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubExceptionInstallerSelwreHang(void)
{
    LwU32      secSpr    = 0;
    FUB_STATUS fubStatus = FUB_ERR_GENERIC;

#if defined(FUB_ON_SEC2)
    CHECK_FUB_STATUS(FALC_REG_WR32( CSB, LW_CSEC_FALCON_IBRKPT1, 0x00000000 ));
    CHECK_FUB_STATUS(FALC_REG_WR32( CSB, LW_CSEC_FALCON_IBRKPT2, 0x00000000 ));
    CHECK_FUB_STATUS(FALC_REG_WR32( CSB, LW_CSEC_FALCON_IBRKPT3, 0x00000000 ));
    CHECK_FUB_STATUS(FALC_REG_WR32( CSB, LW_CSEC_FALCON_IBRKPT4, 0x00000000 ));
    CHECK_FUB_STATUS(FALC_REG_WR32( CSB, LW_CSEC_FALCON_IBRKPT5, 0x00000000 ));
#elif defined(FUB_ON_LWDEC)
    CHECK_FUB_STATUS(FALC_REG_WR32( CSB, LW_CLWDEC_FALCON_IBRKPT1, 0x00000000 ));
    CHECK_FUB_STATUS(FALC_REG_WR32( CSB, LW_CLWDEC_FALCON_IBRKPT2, 0x00000000 ));
    CHECK_FUB_STATUS(FALC_REG_WR32( CSB, LW_CLWDEC_FALCON_IBRKPT3, 0x00000000 ));
    CHECK_FUB_STATUS(FALC_REG_WR32( CSB, LW_CLWDEC_FALCON_IBRKPT4, 0x00000000 ));
    CHECK_FUB_STATUS(FALC_REG_WR32( CSB, LW_CLWDEC_FALCON_IBRKPT5, 0x00000000 ));
#else
    ct_assert(0);
#endif

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
    falc_wspr( EV, fubExceptionHandlerSelwreHang );
ErrorExit:
    return fubStatus;
}

/*!
 * Check if CSBERRSTAT_ENABLE bit is TRUE, else set CSBERRSTAT_ENABLE to TRUE
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubValidateCsberrstat(void)
{
    LwU32      csbErrStatVal = 0;
    FUB_STATUS fubStatus     = FUB_ERR_GENERIC;

#if defined(FUB_ON_SEC2)
        CHECK_FUB_STATUS(FALC_REG_RD32(CSB, LW_CSEC_FALCON_CSBERRSTAT, &csbErrStatVal));
        csbErrStatVal = FLD_SET_DRF(_CSEC, _FALCON_CSBERRSTAT, _ENABLE, _TRUE, csbErrStatVal);
        CHECK_FUB_STATUS(FALC_REG_WR32(CSB, LW_CSEC_FALCON_CSBERRSTAT, csbErrStatVal));
#elif defined(FUB_ON_LWDEC)
        CHECK_FUB_STATUS(FALC_REG_RD32(CSB, LW_CLWDEC_FALCON_CSBERRSTAT, &csbErrStatVal));
        csbErrStatVal = FLD_SET_DRF(_CLWDEC, _FALCON_CSBERRSTAT, _ENABLE, _TRUE, csbErrStatVal);
        CHECK_FUB_STATUS(FALC_REG_WR32(CSB, LW_CLWDEC_FALCON_CSBERRSTAT, csbErrStatVal));
#else
    ct_assert(0);
#endif
ErrorExit:
    return fubStatus;
}

/*
 * @brief: Before burning Fuse, FUB checks for all preconditions required for successfully burning the fuses.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS fubPrerequisitesCheck(void)
{
    FUB_STATUS fubStatus      = FUB_ERR_GENERIC;

    // Check if binary is running on expected chip
    CHECK_FUB_STATUS(fubIsChipSupported_HAL());

    // Check if binary is running on expected Falcon
    CHECK_FUB_STATUS(fubIsValidFalconEngine_HAL());

    // Check revocation fuses and return early if FUB is revoked
    CHECK_FUB_STATUS(fubRevokeHsBin_HAL());

    // 
    // Check if local memory range is set.
    // UDE sets this at the end of exelwtion, so this is indirect check to make sure
    // that UDE has exelwted before running FUB.
    //
    CHECK_FUB_STATUS(fubCheckIfLocalMemoryRangeIsSet_HAL());

    //
    // To Burn fuse FUB needs to control GPIO control register in PMGR
    // So exit FUB if in GC6 exit path
    //
    CHECK_FUB_STATUS(fubInGc6Exit_HAL());

ErrorExit:
    return fubStatus;
}


/*!
 * @brief: Custom HS entry function has been replaced by this function to setup SP register 
 *         to safe value at start of HS before any HS code goes on the stack
 *         instead of NS to avoid stack overflow by attacker.
 *         This function also scrubs GPRs, SCP registers and stack.
 *
 * WARNING: PLEASE DO NOT add or modify and any code below without reviewing
 *          objdump. The assumption is that compiler MUST NOT push any local variables
 *          on to the stack.
 *
 * TODO: Bug 200536090 - use "naked" attribute once falcon-gcc has the support 
 *       added.
 *
 */
GCC_ATTRIB_NO_STACK_PROTECT()
void
fubEntryWrapper(void)
{
    // WARNING: Do not add any new local variables. See comments above.
    LwU32 *pDmem = NULL;

    // Clear SCP signature
    falc_scp_forget_sig();

    // Clear all SCP registers
    fubClearSCPRegisters();

    // Reset all falcon GPRs (a0-a15)
    fubClearFalconGprs();

    //
    // Using falc_wspr intruction to set SP to last address of DMEM.
    //
    falc_wspr(SP, (FLCN_DMEM_SIZE_IN_BLK_TURING << FLCN_DMEM_BLK_ALIGN_BITS_TURING));

    // Scrub DMEM from stackBottom to current stack pointer.
    for (pDmem = (LwU32*)FUB_STACK_BOTTOM_LIMIT;
         pDmem < (LwU32*)(FLCN_DMEM_SIZE_IN_BLK_TURING << FLCN_DMEM_BLK_ALIGN_BITS_TURING);
         pDmem++)
    {
        *pDmem = 0x0;
    }

    //
    // This is the function where actual FUB starts.
    // We halt in fubEntryFunction itself. Binary won't return back to this function.
    // 
    fubEntry();

    // Binary would never come here. Putting falc_halt for safety certification issues.
    falc_halt();
}

/*!
 * @brief: Entry of FUB binary. This is the first function to execute in HS.
 */
void
fubEntry(void)
{
    FUB_STATUS             fubStatus        = FUB_ERR_GENERIC;
    FUB_STATUS             fubStatusCleanup = FUB_ERR_GENERIC;
    LwU32                  fubUseCaseMask   = 0;
    FUB_PRIV_MASKS_RESTORE plmStore;

    //
    // Global variable to decide if halt action can be taken in case of failing scenarios
    // This needs to be LW_FALSE until NS_Restart and VHR empty checks are completed
    //
    g_canFalcHalt = LW_FALSE;

    // Validate CSBERRSTAT register for pri error handling.
    if (FUB_OK != (fubStatus = fubValidateCsberrstat()))
    {
        FAIL_FUB_WITH_ERROR(fubStatus);
    }

    // Make sure different initiator cannot restart FUB in case it halts in HS mode
    fubMitigateNSRestartFromHS_HAL(pFub);

    // Setup the exception handler
    if (FUB_OK != (fubStatus = fubExceptionInstallerSelwreHang()))
    {
        FAIL_FUB_WITH_ERROR(fubStatus);
    }

    // Set the stack pointer to the MIN addr so if stack reaches that address then falcon can throw an exception.
    if (FUB_OK != (fubStatus = fubStackUnderflowInstaller()))
    {
        FAIL_FUB_WITH_ERROR(fubStatus);
    }

#ifdef IS_SSP_ENABLED
    // Save NS stack canary, this is required because we want to enable SSP for HS entry(fubEntry) as well
    void * nsCanary = __stack_chk_guard;

    // ReInitialize canary to random number generated by HS
    fubInitializeStackCanaryHSUsingSCP();
#endif


#if defined(FUB_ON_SEC2) && !(defined(AUTO_FUB) && FUBCFG_FEATURE_ENABLED(FUBCORE_TU10X))
    // Lock falcon reset so that no one can reset ACR
    if (FUB_OK != (fubStatus = fubLockFalconReset_HAL(pFub, LW_TRUE)))
    {
        FAIL_FUB_WITH_ERROR(fubStatus);
    }
#endif // (FUB_ON_SEC2) && !(AUTO_FUB && FUBCFG_FEATURE_ENABLED(FUBCORE_TU10X))

#ifdef AUTO_FUB
    //
    // For auto ACR, 
    // PKC mode must be enabled and we never set hash saving bit to 1.
    // So we want to make sure VHR is empty.
    //
    if (FUB_OK != (fubStatus = fubCheckIfVhrEmpty_HAL(pFub)))
    {
        FAIL_FUB_WITH_ERROR(fubStatus);
    }
#endif // AUTO_ACR
    
    //
    // Global variable to decide if halt action can be taken in case of failing scenarios
    // This can be set to LW_TRUE after NS_Restart and VHR empty checks are completed
    //
    if (g_canFalcHalt == LW_TRUE)
    {
        FAIL_FUB_WITH_ERROR(FUB_ERR_UNEXPECTED_HALT_ENABLE);
    }
    g_canFalcHalt = LW_TRUE;

    //
    // Set BAR0 timeout value
    // WARNING: This function must be called before any BAR0 access.
    //
    if (FUB_OK != (fubStatus = fubSetBar0Timeout_HAL()))
    {
        FAIL_FUB_WITH_ERROR(fubStatus);
    }

    plmStore.bAreMasksRaised    = LW_FALSE;
    plmStore.thermVidCtrlMask   = 0;
    plmStore.fpfFuseCtrlMask    = 0;
    plmStore.gc6SciMastMask     = 0;
    plmStore.gc6BsiCtrlMask     = 0;
    plmStore.gc6SciSecTimerMask = 0;

    // Initialise use case mask
    g_desc.useCaseMask = 0;
#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_LWFLASH_REV_FOR_WP_BYPASS) || \
     FUBCFG_FEATURE_ENABLED(FUB_BURN_GFW_REV_FOR_WP_BYPASS))
    //
    // Hardcode useCaseMask so there are no inputs
    // This prevents control over sequencing of fusing as added defense
    //
    g_desc.useCaseMask = LW_FUB_USE_CASE_MASK_LWFLASH_REV_FOR_WP_BYPASS | 
                         LW_FUB_USE_CASE_MASK_GFW_REV_FOR_WP_BYPASS; 
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_AHESASC_FUSE)     || \
     FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_ASB_FUSE)         || \
     FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_LWFLASH_FUSE)     || \
     FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_IMAGESELECT_FUSE) || \
     FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_HULK_FUSE)        || \
     FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_FWSEC_FUSE))
    
     g_desc.useCaseMask = LW_FUB_USE_CASE_MASK_AUTO_QS2_AHESASC_FUSE     |
                          LW_FUB_USE_CASE_MASK_AUTO_QS2_ASB_FUSE         |
                          LW_FUB_USE_CASE_MASK_AUTO_QS2_LWFLASH_FUSE     |
                          LW_FUB_USE_CASE_MASK_AUTO_QS2_IMAGESELECT_FUSE |
                          LW_FUB_USE_CASE_MASK_AUTO_QS2_HULK_FUSE        |
                          LW_FUB_USE_CASE_MASK_AUTO_QS2_FWSEC_FUSE; 
#endif
    
#if FUB_ON_SEC2
    // Isolate whole GPU to SEC2
    CHECK_FUB_STATUS(fubIsolateGpuToSec2UsingDecodeTraps_HAL(pFub, ENABLE_DECODE_TRAP));
#endif

    // Function to raise PLMs
    CHECK_FUB_STATUS(fubProtectRegisters_HAL(pFub, &plmStore, LW_TRUE));

    // Function to check if LWVDD is acceptable
    CHECK_FUB_STATUS(fubCheckLwvdd_HAL(pFub));

    //
    // VQPS is 1.8v power supply pin of the Fuse Macro
    // Enable it to provide current required to burn fuse
    //
    CHECK_FUB_STATUS(fubProgramVqpsForVoltageShift_HAL(pFub, ENABLE_VQPS));

    //
    // Burn LWDEC UCODE<ID> fuse to self revoke FUB and limit FUB exelwtion
    // once per chip with current ucode version. Self Revoke FUB before validating
    // DEVID and use case passed by RM, to reduce exposure of FUB in case attacker
    // re runs FUB multiple times till fubCheckIfFubNeedsToRun_* and does glitching
    // attack on FUB.
    //
#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_SELF_REVOCATION_FUSE))
    CHECK_FUB_STATUS(fubSelfRevoke_HAL(pFub));
#endif

    // Check if all prerequisites are met
    CHECK_FUB_STATUS(fubPrerequisitesCheck());

    // Function to check if FUB needs to run.
    CHECK_FUB_STATUS(fubCheckIfFubNeedsToRun_HAL());

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_GSYNC_ENABLING_FUSE))
    // Check and burn GSYNC enabling fuse
    CHECK_FUB_STATUS(fubCheckAndBurnGsyncEnablingFuse_HAL(pFub, &fubUseCaseMask));
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_DEVID_LICENSE_REVOCATION_FUSE))
    // Check and burn DevId based license revocation fuse
    CHECK_FUB_STATUS(fubCheckAndBurnDevidLicenseRevocationFuse_HAL(pFub, &fubUseCaseMask));
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_ALLOW_DRAM_CHIPID_READ_HULK_LICENSE))
    // Check And Burn fuse Allowing HULK license to read DRAM CHIPID
    CHECK_FUB_STATUS(fubCheckAndBurnAllowingDramChipidReadFuse_HAL(pFub, &fubUseCaseMask));
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_REVOKE_DRAM_CHIPID_READ_HULK_LICENSE))
    // Check And Burn fuse blocking HULK license to read DRAM CHIPID
    CHECK_FUB_STATUS(fubCheckAndBurnBlockingDramChipidReadFuse_HAL(pFub, &fubUseCaseMask));
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_GEFORCE_SKU_RECOVERY_GFW_REV))
    //
    // Check And Burn fuse for VdChip SKU Recovery GFW_REV
    // Burn before SKU recovery fuse in case that fails
    //
    CHECK_FUB_STATUS(fubCheckAndBurnGeforceSkuRecoveryGfwRevFuse_HAL(pFub, &fubUseCaseMask));
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_GEFORCE_SKU_RECOVERY))
    //
    // Perform a re-sense of the LW_FPF fuse macro
    // VdChip SKU recovery fuse needs to check if the GFW_REV[2:2] was burned
    // Therefore force a re-sense
    // After re-sense Updated fuse values are copied to PRIV registers
    //
    CHECK_FUB_STATUS(fubResenseFuse_HAL());

    // Check And Burn fuse for VdChip SKU Recovery
    CHECK_FUB_STATUS(fubCheckAndBurnGeforceSkuRecoveryFuse_HAL(pFub, &fubUseCaseMask));
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_LWFLASH_REV_FOR_WP_BYPASS))
    // Check And Burn Fuse ROM_FLASH_REV for WP_BYPASS mitigation. 
    CHECK_FUB_STATUS(fubCheckAndBurnLwflashRevForWpBypassFuse_HAL(pFub, &fubUseCaseMask));
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_GFW_REV_FOR_WP_BYPASS))
    // Check And Burn Fuse GFW_REV for WP_BYPASS mitigation. 
    CHECK_FUB_STATUS(fubCheckAndBurnGfwRevForWpBypassFuse_HAL(pFub, &fubUseCaseMask));
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_AHESASC_FUSE))
    // Check And Burn Fuse SEC2_UCODE1 so that ahesasc qs1 bin can not run on qs2 boards.
    CHECK_FUB_STATUS(fubCheckAndBurnAutoQs2AhesascFuse_HAL(pFub, &fubUseCaseMask));
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_ASB_FUSE))
    // Check And Burn Fuse GSP_UCODE1 so that asb qs1 bin can not run on qs2 boards.
    CHECK_FUB_STATUS(fubCheckAndBurnAutoQs2AsbFuse_HAL(pFub, &fubUseCaseMask));
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_LWFLASH_FUSE))
    // Check And Burn Fuse PMU_UCODE8 so that lwflash qs1 bin can not run on qs2 boards.
    CHECK_FUB_STATUS(fubCheckAndBurnAutoQs2LwflashFuse_HAL(pFub, &fubUseCaseMask));
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_IMAGESELECT_FUSE))
    // Check And Burn Fuse PMU_UCODE11 so that ImageSelect qs1 bin can not run on qs2 boards.
    CHECK_FUB_STATUS(fubCheckAndBurnAutoQs2ImageSelectFuse_HAL(pFub, &fubUseCaseMask));
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_HULK_FUSE))
    // Check And Burn Fuse SEC2_UCODE10 so that hulk qs1 bin can not run on qs2 boards.
    CHECK_FUB_STATUS(fubCheckAndBurnAutoQs2HulkFuse_HAL(pFub, &fubUseCaseMask));
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_FWSEC_FUSE))
    // Check And Burn Fuse GSP_UCODE9_VERSION so that FwSec qs1 bin can not run on qs2 boards.
    CHECK_FUB_STATUS(fubCheckAndBurnAutoQs2FwSecFuse_HAL(pFub, &fubUseCaseMask));
#endif
    
    //
    // Perform a re-sense of the LW_FPF fuse macro
    // After re-sense Updated fuse values are copied to PRIV registers
    //
    CHECK_FUB_STATUS(fubResenseFuse_HAL());

    // Check if Fuse burn was succesfull
    CHECK_FUB_STATUS(fubFuseBurnCheck_HAL(pFub, fubUseCaseMask));

ErrorExit:
    // LW_FPF_FUSEADDR is accessible from NON HS levels as well, so clearing it here
    fubStatusCleanup = fubClearFpfRegisters_HAL();

    // Disable VQPS
    fubStatusCleanup = fubProgramVqpsForVoltageShift_HAL(pFub, DISABLE_VQPS);

    // Function to restore PLMs
    fubStatusCleanup = fubProtectRegisters_HAL(pFub, &plmStore, LW_FALSE);

#if FUB_ON_SEC2
    // Clear Decode Traps isolating GPU to SEC2
    fubStatusCleanup = fubIsolateGpuToSec2UsingDecodeTraps_HAL(pFub, CLEAR_DECODE_TRAP);
#endif

    // report status
    fubReportStatus_HAL(pFub, (fubStatus != FUB_OK ? fubStatus : fubStatusCleanup));

#ifdef IS_SSP_ENABLED
    // Restore NS canary
    __stack_chk_guard = nsCanary;
#endif

#if defined(FUB_ON_SEC2) && !(defined(AUTO_FUB) && FUBCFG_FEATURE_ENABLED(FUBCORE_TU10X))
    // Unlock falcon reset to original mask value
    (void)fubLockFalconReset_HAL(pFub, LW_FALSE);
#endif
    // Perform cleanup and halt in HS mode itself, for maximum security
    fubCleanupAndHalt();
}

