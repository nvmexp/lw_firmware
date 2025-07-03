/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_helper_functions.c
 */

//
// Includes
//
#include "acr.h"
#include "dev_fb.h"
#include "dev_pwr_pri.h"
#include "dev_pwr_falcon_csb.h"
#include "dev_pwr_csb.h"
#include "dev_falcon_v4.h"
#include "dev_master.h"
#include "dev_sec_addendum.h"
#include "dev_sec_csb_addendum.h"

#ifdef SEC2_PRESENT
#include "dev_sec_csb.h"
#endif

#include "falcon.h"

#ifdef BOOT_FROM_HS_BUILD
#include "hs_entry_checks.h"
#endif

#include "acr_objacr.h"
#include "acr_objacrlib.h"

#include "config/g_acr_private.h"
#include "config/g_hal_register.h"

#include "sec2mutexreservation.h"


#ifdef PKC_ENABLED
#include "bootrom_pkc_parameters.h"
#endif

//
// Global Variables
//
typedef union {
    LwU32                               aes[4];
  #ifdef PKC_ENABLED
    struct pkc_verification_parameters  pkc;
  #endif
} SIGNATURE_PARAMS;

// Sanity check
#ifdef PKC_ENABLED
  ct_assert(LW_OFFSETOF(SIGNATURE_PARAMS, pkc.signature.rsa3k_sig.S) == 0);
#endif

#ifdef BOOT_FROM_HS_BUILD
LwBool    g_dummyUnused    ATTR_OVLY(".data")    ATTR_ALIGNED(ACR_SIG_ALIGN);
#endif

SIGNATURE_PARAMS    g_signature     ATTR_OVLY(".data")              ATTR_ALIGNED(ACR_SIG_ALIGN);

#ifndef BSI_LOCK
ACR_RESERVED_DMEM   g_reserved      ATTR_OVLY(".dataEntry")         ATTR_ALIGNED(ACR_DESC_ALIGN);
#endif

RM_FLCN_ACR_DESC    g_desc          ATTR_OVLY(".dataEntry")         ATTR_ALIGNED(ACR_DESC_ALIGN);

#define SE_TRNG_SIZE_DWORD_32           1

#ifdef IS_SSP_ENABLED
// variable to store canary for SSP
void * __stack_chk_guard;

// Local implementation of SSP handler
void __stack_chk_fail(void)
{
    //
    // Update error code
    // We cannot use FALC_ACR_WITH_ERROR here as SSP protection start early even before CSBERRSTAT is checked
    // and also we cannot halt since NS restart and VHR empty check could be remaining
    //
#if defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
    falc_stxb_i((LwU32*) LW_CSEC_FALCON_MAILBOX0, 0, ACR_ERROR_SSP_STACK_CHECK_FAILED);
#elif defined(ACR_UNLOAD)
    falc_stxb_i((LwU32*) LW_CPWR_FALCON_MAILBOX0, 0, ACR_ERROR_SSP_STACK_CHECK_FAILED);
#else
    ct_assert(0);
#endif
    INFINITE_LOOP();
}

#ifndef BOOT_FROM_HS_BUILD
//
// Setup canary for SSP
// Here we are initializing NS canary to random generated number in NS
//
GCC_ATTRIB_NO_STACK_PROTECT() GCC_ATTRIB_NOINLINE()
static void __canary_setup(void)
{
    __stack_chk_guard = (void *)RANDOM_CANARY_NS;
}

//
// ReInitialize canary for HS in case of AUTO_ACR(only defence) and DGPU (primarily)
// HS canary is initialized to random generated number in HS using SCP
//
GCC_ATTRIB_ALWAYSINLINE()
static inline void acrInitializeStackCanaryHSUsingSCP(void)
{
    ACR_STATUS status = ACR_OK;
    LwU32      rand32 = 0;

    //
    // Set the value of stack canary to a random value to ensure adversary
    // can not craft an attack by looking at the assembly to determine the canary value
    //
    if (ACR_OK != (status = acrScpGetRandomNumber_HAL(pAcr, &rand32)))
    {
        FAIL_ACR_WITH_ERROR(status);
    }

    __stack_chk_guard = (void *)rand32;
}
#endif // ifndef BOOT_FROM_HS_BUILD

//
// ReInitialize canary for HS
// HS canary is initialized to random generated number (TRNG) in HS using SE
//
GCC_ATTRIB_ALWAYSINLINE()
static inline void acrInitializeStackCanaryHSUsingSE(void)
{
    ACR_STATUS status = ACR_OK;
    LwU32      rand32;

    //
    // Set the value of stack canary to a random value to ensure adversary
    // can not craft an attack by looking at the assembly to determine the canary value
    //
    if (ACR_OK != (status = acrGetTRand_HAL(pAcr, &rand32, SE_TRNG_SIZE_DWORD_32)))
    {
        FAIL_ACR_WITH_ERROR(status);
    }
    __stack_chk_guard = (void *)rand32;
}
#endif // IS_SSP_ENABLED

/*!
 * @brief Stack Underflow Exception Installer for enabling the stack underflow exception in secure segment by
 * setting up the stackcfg register with stackbottom value.
 * Using falc_ldxb/stxb_i instruction here because there should be no falc_halt till VHR check and
 * ACR_REG_RD32/WR32 macros have falc_halt in there code.
 *
 * @param None.
 *
 * @return None.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
void
acrStackUnderflowInstaller(void)
{
    LwU32 stackBottomLimit = ACR_STACK_BOTTOM_LIMIT;
    LwU32 var;
    //
    // STACKCFG.BOTTOM is word address, if the SP is equal or less than (STACKCFG.BOTTOM << 2),
    // then an exception will be generated. Therefore pushing stackBottomLimit >> 2.
    //
    stackBottomLimit = stackBottomLimit >> 2;
#if defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
    var = ACR_CSB_REG_RD32_INLINE(LW_CSEC_FALCON_STACKCFG);
    var = FLD_SET_DRF_NUM( _CSEC, _FALCON_STACKCFG, _BOTTOM, stackBottomLimit, var);
    var = FLD_SET_DRF( _CSEC, _FALCON_STACKCFG, _SPEXC, _ENABLE, var );
    ACR_CSB_REG_WR32_INLINE(LW_CSEC_FALCON_STACKCFG, var);
#elif defined(ACR_UNLOAD)
    var = ACR_CSB_REG_RD32_INLINE(LW_CPWR_FALCON_STACKCFG);
    var = FLD_SET_DRF_NUM( _CPWR, _FALCON_STACKCFG, _BOTTOM, stackBottomLimit, var );
    var = FLD_SET_DRF( _CPWR, _FALCON_STACKCFG, _SPEXC, _ENABLE, var );
    ACR_CSB_REG_WR32_INLINE(LW_CPWR_FALCON_STACKCFG, var);
#endif
}

/*!
 * Exception handler which will reside in the secure section. This subroutine will be exelwted whenever exception is triggered.
 */
void acrExceptionHandlerSelwreHang(void)
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
void acrExceptionInstallerSelwreHang(void)
{
    LwU32 secSpr = 0;

#if defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
    ACR_CSB_REG_WR32_INLINE( LW_CSEC_FALCON_IBRKPT1, 0x00000000 );
    ACR_CSB_REG_WR32_INLINE( LW_CSEC_FALCON_IBRKPT2, 0x00000000 );
    ACR_CSB_REG_WR32_INLINE( LW_CSEC_FALCON_IBRKPT3, 0x00000000 );
    ACR_CSB_REG_WR32_INLINE( LW_CSEC_FALCON_IBRKPT4, 0x00000000 );
    ACR_CSB_REG_WR32_INLINE( LW_CSEC_FALCON_IBRKPT5, 0x00000000 );
#elif defined(ACR_UNLOAD)
    ACR_CSB_REG_WR32_INLINE( LW_CPWR_FALCON_IBRKPT1, 0x00000000 );
    ACR_CSB_REG_WR32_INLINE( LW_CPWR_FALCON_IBRKPT2, 0x00000000 );
    ACR_CSB_REG_WR32_INLINE( LW_CPWR_FALCON_IBRKPT3, 0x00000000 );
    ACR_CSB_REG_WR32_INLINE( LW_CPWR_FALCON_IBRKPT4, 0x00000000 );
    ACR_CSB_REG_WR32_INLINE( LW_CPWR_FALCON_IBRKPT5, 0x00000000 );
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

    falc_wspr( EV, acrExceptionHandlerSelwreHang );

    falc_rspr( secSpr, SEC );
    // Clear DISABLE_EXCEPTIONS
    secSpr = FLD_SET_DRF( _FALC, _SEC_SPR, _DISABLE_EXCEPTIONS, _CLEAR, secSpr );
    falc_wspr( SEC, secSpr );
}

/*!
 * Check if CSBERRSTAT_ENABLE bit is TRUE, else set CSBERRSTAT_ENABLE to TRUE
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
void acrValidateCsberrstat(void)
{
    LwU32 csbErrStatVal;
#if defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
        csbErrStatVal = ACR_CSB_REG_RD32_INLINE(LW_CSEC_FALCON_CSBERRSTAT);
        csbErrStatVal = FLD_SET_DRF(_CSEC, _FALCON_CSBERRSTAT, _ENABLE, _TRUE, csbErrStatVal);
        ACR_CSB_REG_WR32_INLINE(LW_CSEC_FALCON_CSBERRSTAT,csbErrStatVal);
#elif defined(ACR_UNLOAD)
        csbErrStatVal = ACR_CSB_REG_RD32_INLINE(LW_CPWR_FALCON_CSBERRSTAT);
        csbErrStatVal = FLD_SET_DRF(_CPWR, _FALCON_CSBERRSTAT, _ENABLE, _TRUE, csbErrStatVal);
        ACR_CSB_REG_WR32_INLINE(LW_CPWR_FALCON_CSBERRSTAT, csbErrStatVal);
#else
    ct_assert(0);
#endif
}

/*!
 * Function to scrub unused DMEM from startAddr to endAddr
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
void
acrScrubDmem(LwU32 startAddr, LwU32 endAddr)
{
    LwU32 *pDmem = NULL;

    // Scrub DMEM from startAddr to endAddr
    for (pDmem = (LwU32*)startAddr;
         pDmem < (LwU32*)endAddr;
         pDmem++)
    {
        *pDmem = 0x0;
    }

}

/*!
 * @brief: Custom start function to replace the default implementation that comes from the toolchain.
 *         We need to setup SP register correctly before jumping to main function.
 *         In case of Boot from HS, main function call is skipped and entry function is called directly
 *         (non-inlined function call is not allowed before setting stack canary) in HS entry.
 */
GCC_ATTRIB_NO_STACK_PROTECT() ATTR_OVLY(".start")
void start(void)
{
#ifdef BOOT_FROM_HS_BUILD
    hsEntryChecks();

    //
    // Below is hack to keep data section, as it is needed by siggen.
    // g_signature is used in patching by siggen but since g_signature
    // is not used by Boot From Hs, so compiler ignores it and makes data
    // as Null.
    // Hence we are introducing g_falchalt to data section not NULL.
    // This needs to be done after hsEntryChecks since it uses GPRs and we
    // are clearing GPRs in hsEntryChecks. Also setting SP should be the
    // first thing we do in HS, which is done in hsEntryChecks.
    //
    {
        g_dummyUnused = LW_TRUE;
    }
    acrEntryFunction();
#else
    // initialize SP to DMEM size
    falc_wspr(SP, LW_CSEC_FLCN_DMEM_SIZE_IN_BLK << LW_CSEC_FLCN_DMEM_BLK_ALIGN_BITS );

    main();
#endif // ifndef BOOT_FROM_HS_BUILD

    falc_halt();
}

#ifndef BOOT_FROM_HS_BUILD
int main(void)
{
    LwU32 start       = (ACR_PC_TRIM((LwU32)_acr_code_start));
    LwU32 end         = (ACR_PC_TRIM((LwU32)_acr_code_end));

#ifndef BSI_LOCK
    //
    // When binary is loaded by BL, g_signature is also filled by BL.
    // For cases, where binary is directly loaded by IMEM(C/D/T) and DMEM(C/D/T)
    // signature needs to be copied here.
    //
    if ((g_signature.aes[0] == 0) && (g_signature.aes[1] == 0) &&
        (g_signature.aes[2] == 0) && (g_signature.aes[3] == 0))
    {
        g_signature.aes[0] = g_reserved.reservedDmem[0];
        g_signature.aes[1] = g_reserved.reservedDmem[1];
        g_signature.aes[2] = g_reserved.reservedDmem[2];
        g_signature.aes[3] = g_reserved.reservedDmem[3];
    }
#endif

#ifdef IS_SSP_ENABLED
    // Setup canary for SSP
    __canary_setup();
#endif

    // Set DEFAULT error code
#if defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
    falc_stxb_i((LwU32*) LW_CSEC_FALCON_MAILBOX0, 0,  ACR_ERROR_BIN_STARTED_BUT_NOT_FINISHED);
#elif defined(ACR_UNLOAD)
    falc_stxb_i((LwU32*) LW_CPWR_FALCON_MAILBOX0, 0,  ACR_ERROR_BIN_STARTED_BUT_NOT_FINISHED);
#else
    ct_assert(0);
#endif

    // Setup bootrom registers for Enhanced AES support
#ifdef IS_ENHANCED_AES
#if defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
    falc_stxb_i((LwU32*)LW_CSEC_FALCON_BROM_ENGIDMASK, 0, ENGINE_ID);
    falc_stxb_i((LwU32*)LW_CSEC_FALCON_BROM_LWRR_UCODE_ID, 0, DRF_NUM(_CSEC_FALCON, _BROM_LWRR_UCODE_ID, _VAL, UCODE_ID));
#elif defined(ACR_UNLOAD)
    falc_stxb_i((LwU32*)LW_CPWR_FALCON_BROM_ENGIDMASK, 0, ENGINE_ID);
    falc_stxb_i((LwU32*)LW_CPWR_FALCON_BROM_LWRR_UCODE_ID, 0, DRF_NUM(_CPWR_FALCON, _BROM_LWRR_UCODE_ID, _VAL, UCODE_ID));
#else
    ct_assert(0);
#endif // For acr profile based check
#endif // For IS_ENHANCED_AES

#ifdef PKC_ENABLED
    g_signature.pkc.pk           = NULL;
    g_signature.pkc.hash_saving  = 0;

#if defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
    falc_stxb_i((LwU32*)LW_CSEC_FALCON_BROM_PARAADDR(0),   0, (LwU32)&g_signature);
    falc_stxb_i((LwU32*)LW_CSEC_FALCON_BROM_ENGIDMASK,     0, ENGINE_ID);
    falc_stxb_i((LwU32*)LW_CSEC_FALCON_BROM_LWRR_UCODE_ID, 0, DRF_NUM(_CSEC_FALCON, _BROM_LWRR_UCODE_ID, _VAL, UCODE_ID));
    falc_stxb_i((LwU32*)LW_CSEC_FALCON_MOD_SEL,            0, DRF_NUM(_CSEC_FALCON, _MOD_SEL, _ALGO, LW_CSEC_FALCON_MOD_SEL_ALGO_RSA3K));
#else
    ct_assert(0);
#endif

#else
    //
    // AES Legacy signature verification
    //

    // Trap next 2 dm* operations, i.e. target will be SCP instead of FB
    falc_scp_trap(TC_2);

    // Load the signature to SCP
    falc_dmwrite(0, ((6 << 16) | (LwU32)(g_signature.aes)));
    falc_dmwait();
#endif // PKC_ENABLED

    //
    // All the required code is already loaded into IMEM
    // Just need to program the SEC spr and jump into code
    //
    falc_wspr(SEC, SVEC_REG(start, (end - start), 0, IS_HS_ENCRYPTED));

    // ACR entry point
    acrEntryWrapper();

    //
    // This should never be reached as we halt in acrEntryFunction for all ACR binaries
    // Halt anyway, just in case... Bootloader is already ilwalidated above.
    //
    falc_halt();

    return 0;
}
#endif // ifndef BOOT_FROM_HS_BUILD

// Clear all SCP registers
GCC_ATTRIB_ALWAYSINLINE()
static inline
void
acrClearSCPRegisters(void)
{
    // First load secret index 0, then Clear all SCP registers
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

// Clear all falcon GPRs
GCC_ATTRIB_ALWAYSINLINE()
static inline
void
acrClearFalconGprs(void)
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

#ifndef BOOT_FROM_HS_BUILD
/*!
 * Wrapper function to set SP to max of DMEM and the call ACR Entry.
 *
 * WARNING: PLEASE DO NOT add or modify and any code below without reviewing
 *  objdump. The assumption is that compiler MUST NOT push any local variables
 *  on to the stack.
 *
 * TODO: Bug 200536090 - use "naked" attribute once falcon-gcc has the support
 *       added.
 *
 */
GCC_ATTRIB_NO_STACK_PROTECT()
void
acrEntryWrapper(void)
{

    //
    // Using falc_wspr intruction to set SP to last physically accessible(by falcon) 4byte aligned address(0xFFFC)
    // On GA100 and prior setting SP = 0x10000 rolls its value to 0x0 which is not desirable when following HS init
    // sequence.
    //
    falc_wspr(SP, ((LW_CSEC_FLCN_DMEM_SIZE_IN_BLK << LW_CSEC_FLCN_DMEM_BLK_ALIGN_BITS)-4));

#if !defined(BSI_LOCK) || !defined(UNDESIRABLE_OPTIMIZATION_FOR_BSI_RAM_SIZE)
    // Clear SCP signature
    falc_scp_forget_sig();

    // Clear all SCP registers
    acrClearSCPRegisters();

    // Reset all falcon GPRs (a0-a15)
    acrClearFalconGprs();
#endif

    // Validate CSBERRSTAT register for pri error handling.
    acrValidateCsberrstat();

    // Write CMEMBASE = 0, to make sure ldd/std instructions are not routed to CSB.
    ACR_CSB_REG_WR32_INLINE(LW_CSEC_FALCON_CMEMBASE, 0);

    // Scrub DMEM from stackBottom to current stack pointer.
    acrScrubDmem((LwU32)_bss_start, (LW_CSEC_FLCN_DMEM_SIZE_IN_BLK << LW_CSEC_FLCN_DMEM_BLK_ALIGN_BITS));

   // Make sure different master cannot restart ACR in case it halts in HS mode
    acrMitigateNSRestartFromHS_HAL(pAcr);

    // Setup the exception handler
    acrExceptionInstallerSelwreHang();

    // Set the stack pointer to the MIN addr so if stack reaches that address then falcon can throw an exception.
    acrStackUnderflowInstaller();

    // Reset both SCP instruction sequencers and Pipe Reset to put SCP in a deterministic state
    acrResetScpLoopTracesAndPipe();

#ifdef IS_SSP_ENABLED

    //
    // We use SCP based random generator for HS stack canary since it is inlined.
    // This also serves as an early and extra stack canary setup for dGPU which later uses SE
    // Initialize canary to random number generated by HS - SCP
    // TODO : Save NS canary in case we decide to return to NS in future (lwrrently we halt in this function)
    //
    acrInitializeStackCanaryHSUsingSCP();

#endif // IS_SSP_ENABLED
    //
    // This is the function where actual ACR starts.
    // We halt in acrEntryFunction itself. Binary won't return back to this function.
    //
    acrEntryFunction();

    //
    // Binary would never come here. Putting falc_halt for safety certification issues.
    // TODO : Restore NS canary in case we decide to return to NS(lwrrently we halt in this function)
    //
    falc_halt();

}
#endif // ifndef BOOT_FROM_HS_BUILD

/*
 *  Initialize all HAL related setup
 */
void
acrEntryFunction(void)
{
    // Declaring variables at the start, not initializing as we are going to clear falcon GPRs
    ACR_STATUS  status;
    ACR_STATUS  statusCleanup;
    LwU8        acrMutexOnlyAllowOneAcrBinaryAtAnyTime;
    LwU32       ucodeBuildVersion;

    // Initializing variables after clearing falcon GPRs
    status         = ACR_OK;
    statusCleanup  = ACR_OK;
    acrMutexOnlyAllowOneAcrBinaryAtAnyTime = 0;
    ucodeBuildVersion = 0;

#ifndef BOOT_FROM_HS_BUILD
    // Lock falcon reset so that no one can reset ACR
    if (ACR_OK != (status = acrSelfLockFalconReset_HAL(pAcr, LW_TRUE)))
    {
        FAIL_ACR_WITH_ERROR(status);
    }

#if defined(ACR_UNLOAD) && defined(DMEM_APERT_ENABLED)
    //
    // Enable DMEM aperture for PMU
    // WARNING: This function must be called before any BAR0 access.
    //
    acrEnableDmemAperture_HAL();
#endif

// ACR_UNLOAD runs on PMU on Turing and accesses BAR0 through DMEM aperture so skipped
#if defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
    //
    // Set BAR0 timeout value
    // WARNING: This function must be called before any BAR0 access.
    //
    acrSetBar0Timeout_HAL();
#endif

#endif // ifndef BOOT_FROM_HS_BUILD

#if defined(AHESASC)
    //
    // Enable SCPM back again, as it gets reset with SEC2 reset that happens prior to AHESASC exelwtion.
    // Refer BUG 3078892 for more details.
    //
    acrEnableLwdclkScpmForAcr_HAL();
#endif

#ifdef IS_SSP_ENABLED
    //
    // Reinitialize canary to random number generated by HS - SE engine
    // TODO : Save HS SCP canary in case we decide to return to Wrapper in future (lwrrently we halt in this function)
    //
    acrInitializeStackCanaryHSUsingSE();
#endif

//    pAcrlib->bInitRegMap = LW_FALSE;
//    acrlibInitRegMapping_HAL();

    // Verify if this binary should be allowed to run on particular chip
    if (ACR_OK != (status = acrCheckIfBuildIsSupported_HAL(pAcr)))
    {
        FAIL_ACR_WITH_ERROR(status);
    }

    // Verify if binary is running on expected falcon/engine
    if (ACR_OK != (status = acrCheckIfEngineIsSupported_HAL(pAcr)))
    {
        FAIL_ACR_WITH_ERROR(status);
    }

    // Get SW defined ucode build version
    if (ACR_OK != (status = acrGetUcodeBuildVersion_HAL(pAcr, &ucodeBuildVersion)))
    if (status != ACR_OK)
    {
        FAIL_ACR_WITH_ERROR(status);
    }

    // Check SW build version against fpf version
    if (ACR_OK != (status = acrCheckFuseRevocationAgainstHWFpfVersion_HAL(pAcr, ucodeBuildVersion)))
    {
        FAIL_ACR_WITH_ERROR(status);
    }

    // Check SW build version against fuse version
    if (ACR_OK != (status = acrCheckFuseRevocationAgainstHWFuseVersion_HAL(pAcr, ucodeBuildVersion)))
    {
        FAIL_ACR_WITH_ERROR(status);
    }

#if BOOT_FROM_HS_BUILD
    // Check whether Bootstrap and FWSEC have run successfully to ensure Chain of Trust.
    if (ACR_OK != (status = acrCheckChainOfTrust_HAL(pAcr)))
    {
        FAIL_ACR_WITH_ERROR(status);
    }
#endif // BOOT_FROM_HS_BUILD

#if ACR_GSP_RM_BUILD
#if AHESASC 
    //Check handoff with booter binary for Pre-Hopper Authenticated GSP-RM Boot
    if (ACR_OK != (status = acrAhesascGspRmCheckHandoff_HAL(pAcr)))
    {
        FAIL_ACR_WITH_ERROR(status);
    }
#endif // AHESASC

#if defined(ACR_UNLOAD) || defined(ACR_UNLOAD_ON_SEC2)
    //Check handoff with booter binary for Pre-Hopper Authenticated GSP-RM Boot
    if (ACR_OK != (status = acrUnloadGspRmCheckHandoff_HAL(pAcr)))
    {
        FAIL_ACR_WITH_ERROR(status);
    }
#endif // (ACR_UNLOAD || ACR_UNLOAD_ON_SEC2)
#endif // ACR_GSP_RM_BUILD

    // Check if Priv Sec is enabled on Prod board
    if (ACR_OK != (status = acrCheckIfPrivSecEnabledOnProd_HAL(pAcr)))
    {
        FAIL_ACR_WITH_ERROR(status);
    }

#if !defined(ASB)
    // Make sure SELWRE_LOCK state in HW is compatible with ACR such that ACR should run fine
    if (ACR_OK != (status = acrValidateSelwreLockHwStateCompatibilityWithACR_HAL(pAcr)))
    {
        FAIL_ACR_WITH_ERROR(status);
    }
#endif

    // Lwrrently UDE is not run on fmodel by default, so this check fails, Bug 200194032
#if defined(AHESASC) && !defined(ACR_FMODEL_BUILD)
    if (ACR_OK != (status = acrCheckIfMemoryRangeIsSet_HAL(pAcr)))
    {
        FAIL_ACR_WITH_ERROR(status);
    }
#endif

    // First acquire the ACR mutex before doing anything else.
    if (ACR_OK != (status = acrAcquireSelwreMutex_HAL(pAcr, SEC2_MUTEX_ACR_ALLOW_ONLY_ONE_ACR_BINARY_AT_ANY_TIME, &acrMutexOnlyAllowOneAcrBinaryAtAnyTime)))
    {
        goto done;
    }

    // Not required for BSI Lock phase as PMU wont be in LS mode after this phase
#ifndef BSI_LOCK
    acrValidateBlocks_HAL();
#endif

    // Call the main function to handle ACR
#if defined(AHESASC)
    if (ACR_OK != (status = acrInit_HAL(pAcr)))
    {
        goto done;
    }
#endif

#if defined(ASB)
    if (ACR_OK != (status = acrBootFalcon_HAL(pAcr)))
    {
        goto done;
    }
#endif

#if defined(ACR_UNLOAD) || defined(ACR_UNLOAD_ON_SEC2)
    if (ACR_OK != (status = acrDeInit()))
    {
        goto done;
    }
#endif

#ifdef ACR_BSI_PHASE
    if (ACR_OK != (status = acrBsiPhase()))
    {
        goto done;
    }
#endif
done:
    statusCleanup = acrReleaseSelwreMutex_HAL(pAcr, SEC2_MUTEX_ACR_ALLOW_ONLY_ONE_ACR_BINARY_AT_ANY_TIME, acrMutexOnlyAllowOneAcrBinaryAtAnyTime);

    if (status == ACR_OK)
    {
        // Copying statusCleanup into status since at the moment we are capturing only a single status from ACR into mailbox
        status = statusCleanup;
    }
    else
    {
        //
        // If ACR failed, then leaving ACR version as 0 may not be safe since it may
        // have failed after successfully booting some falcon and version 0 would
        // allow ACR load to be run on top of ACR load. And we can not write the
        // actual version either since that'd indicate to next binary that ACR load
        // actually finished but we dont know what it actually did. So, we should use
        // a version that causes catastrophic failure for all 4 ACR binaries.
        //
        ucodeBuildVersion = ACR_UCODE_BUILD_VERSION_ILWALID;
    }

    //
    // Write ACR ucode build version to version bits reserved for it in secure scratch register
    // NOTE: We are using sec2 mutex in acrWriteAcrVersionToBsiSelwreScratch()
    //
    statusCleanup = acrWriteAcrVersionToBsiSelwreScratch_HAL(pAcr, ucodeBuildVersion);
    if (status == ACR_OK)
    {
        // Copying statusCleanup into status since at the moment we are capturing only a single status from ACR into mailbox
        status = statusCleanup;
    }

#if ACR_GSP_RM_BUILD
#if AHESASC
    //Write handoff with booter binary for Pre-Hopper Authenticated GSP-RM Boot
    if (status == ACR_OK)
    {
        acrWriteAhesascGspRmHandoffValueToBsiSelwreScratch_HAL(pAcr);
    }
#endif // AHESASC

#if defined(ACR_UNLOAD) || defined(ACR_UNLOAD_ON_SEC2)
    //Write handoff with booter binary for Pre-Hopper Authenticated GSP-RM Boot
    if (status == ACR_OK)
    {
        acrWriteAcrUnloadGspRmHandoffValueToBsiSelwreScratch_HAL(pAcr);
    }
#endif // (ACR_UNLOAD || ACR_UNLOAD_ON_SEC2)
#endif //  ACR_GSP_RM_BUILD
    //
    // Updated mailbox with ACR ucode status
    // Skipping this for GP10X BSI_LOCK through this define, as anyway BAR0 is not accessible to read mailbox and debug
    //
#if !defined(BSI_LOCK) || !defined(UNDESIRABLE_OPTIMIZATION_FOR_BSI_RAM_SIZE)
    acrWriteStatusToFalconMailbox_HAL(pAcr, status);
#endif
    if (status != ACR_OK)
    {
        falc_halt();
    }

   // Handle special exit condition for various paths
#ifdef BSI_LOCK
    //
    // During GC6 exit, various phases are loaded by BSI sequentially. This is controlled by signal RDY4TRANS.
    // Writing STATUS_DIS to RDY4TRANS signals BSI that current phase has finished exelwting and it can start loading the next phase.
    // ACR_REG_WR32 is blocking BAR0 write call, which means BSI might have started loading next phase till this call returns.
    // This is fine as next ucode to run on SEC2 during GC6 exit is SEC2 RTOS which is loaded by ASB running on GSP.
    // ASB itself does not need to trigger RDY4TRANS as it is done by SEC2 RTOS.
    //
    ACR_REG_WR32(BAR0, LW_PPWR_PMU_PMU2BSI_RDY4TRANS, DRF_DEF(_PPWR, _PMU_PMU2BSI_RDY4TRANS, _STATUS, _DIS));
#endif

    //
    // Perform cleanup activities and halt
    // Reset all SCP registers in case one of the preceding calls forgot to scrub confidential data.
    // Reset all falcon GPRs as additional safety measure.
    // NOTE: Cleanup can be skipped for BSI_LOCK and ASB due to BSI RAM size constraints in function implementation
    // Halt in HS happens for all binaries
    //
    acrCleanupAndHalt();

    // TODO : Restore HS SCP canary in case we decide to return to Wrapper in future (lwrrently we halt in this function)
}

/*!
 * @brief Reset all SCP GPRs, falcon GPRs and halt
 */
void
acrCleanupAndHalt(void)
{
    LwU32 secSpr    = 0;
//
// Following register cleanups can be disabled for saving BSI RAM by setting UNDESIRABLE_OPTIMIZATION_FOR_BSI_RAM_SIZE.
// Lwrrently this macro is not set for ASB or BSI_LOCK in Turing.
// ASB/UNLOAD don't need SCP cleanup as they don't do anything confidential, so SCP cleanup can be explicitly disabled for ASB to save some BSI RAM.
//
#if !defined(BSI_LOCK) || !defined(UNDESIRABLE_OPTIMIZATION_FOR_BSI_RAM_SIZE)

    // Reset both SCP instruction sequencers and Pipe Reset to put SCP in a deterministic state
    acrResetScpLoopTracesAndPipe();

    //
    // Leaving intrs/exceptions may create avenues to jump back into HS, although there is no
    // identified vulnerability, there is no good reason to leave intrs/exceptions enabled.
    //
    falc_rspr( secSpr, SEC );
    // SET DISABLE_EXCEPTIONS
    secSpr = FLD_SET_DRF( _FALC, _SEC_SPR, _DISABLE_EXCEPTIONS, _SET, secSpr );
    falc_wspr( SEC, secSpr );

    // Reset all SCP registers.
    acrClearSCPRegisters();

    // Unlock falcon reset to original mask value
#if BOOT_FROM_HS_BUILD
    hsSelfLockFalconReset(LW_FALSE);
#else
    acrSelfLockFalconReset_HAL(pAcr, LW_FALSE);
#endif // BOOT_FROM_HS_BUILD
    // Scrub DMEM from stackBottom to current stack pointer.
    acrScrubDmem(ACR_START_ADDR_OF_DMEM, (LW_CSEC_FLCN_DMEM_SIZE_IN_BLK << LW_CSEC_FLCN_DMEM_BLK_ALIGN_BITS));

    // Reset all falcon GPRs (a0-a15)
    acrClearFalconGprs();
#endif

    // NOTE: Do not put any C code above this as GPRs are cleared
    falc_halt();
}

/*
 * This function will return true if falcon with passed falconId is supported.
 */
LwBool
acrIsFalconSupported(LwU32 falconId)
{
//TODO add support for checking if falcon is present from hardware.
    switch(falconId)
    {
        case LSF_FALCON_ID_PMU:
#ifdef ACR_RISCV_LS
        case LSF_FALCON_ID_PMU_RISCV:
#endif
#ifdef SEC2_PRESENT
        case LSF_FALCON_ID_SEC2:
#endif
        case LSF_FALCON_ID_FECS:
        case LSF_FALCON_ID_GPCCS:
#ifdef LWDEC_PRESENT
        case LSF_FALCON_ID_LWDEC:
#endif
#ifdef LWENC_PRESENT
        case LSF_FALCON_ID_LWENC0:
        case LSF_FALCON_ID_LWENC1:
        case LSF_FALCON_ID_LWENC2:
#endif
#ifdef DPU_PRESENT
        case LSF_FALCON_ID_DPU:
#elif GSP_PRESENT
        case LSF_FALCON_ID_GSPLITE:
#ifdef ACR_RISCV_LS
        case LSF_FALCON_ID_GSP_RISCV:
#endif
#endif
#ifdef FBFALCON_PRESENT
        case LSF_FALCON_ID_FBFALCON:
#endif

#ifdef LWDEC1_PRESENT
        case LSF_FALCON_ID_LWDEC1:
#endif

#ifdef LWDEC_RISCV_PRESENT
        case LSF_FALCON_ID_LWDEC_RISCV:
#endif

#ifdef LWDEC_RISCV_EB_PRESENT
        case LSF_FALCON_ID_LWDEC_RISCV_EB:
#endif

#ifdef LWJPG_PRESENT
        case LSF_FALCON_ID_LWJPG:
#endif

#ifdef OFA_PRESENT
        case LSF_FALCON_ID_OFA:
#endif
            return LW_TRUE;
        default:
            return LW_FALSE;
    }
}

/*!
 * acrMemcmp
 *
 * @brief    Mimics standard memcmp for unaligned memory
 *
 * @param[in] pSrc1   Source1
 * @param[in] pSrc2   Source2
 * @param[in] size    Number of bytes to be compared
 *
 * @return ZERO if equal,
 *         0xff if size argument is passed as zero
 *         non-zero value if values mismatch
 */
LwU8 acrMemcmp(const void *pS1, const void *pS2, LwU32 size)
{
    const LwU8 *pSrc1        = (const LwU8 *)pS1;
    const LwU8 *pSrc2        = (const LwU8 *)pS2;
    LwU8       retVal        = 0;

    if (0 == size)
    {
        //
        // Return a non-zero value
        // WARNING: Note that this does not match with typical memcmp expectations
        //
        return 0xff;
    }

    do
    {
        //
        // If any mismatch is hit, the retVal will contain a non-zero value
        // at the end of the loop. Regardless of where we find the mismatch,
        // we must still continue comparing the entire buffer and return failure at
        // the end, otherwise in cases where signature is being comared,
        // an attacker can measure time taken to return failure and hence
        // identify which byte was mismatching, then try again with a new
        // value for that byte, and eventually find the right value of
        // that byte and so on continue to hunt down the complete value of signature.
        //
        // More details are here:
        //     https://security.stackexchange.com/a/74552
        //     http://beta.ivc.no/wiki/index.php/Xbox_360_Timing_Attack
        //     https://confluence.lwpu.com/display/GFS/Security+Metrics See Defensive strategies
        //
        retVal |= (*(pSrc1++) ^ *(pSrc2++));
        size--;
    } while(size);

    return retVal;
}

/*
 * ACR_MEMSETBYTE_ENABLE flag is used to conditional exclude the acrMemsetByte function for ga100 profile.
 * This function was getting added in ga100 bins but is not called from anywhere in the code for ga100.
*/
#ifdef ACR_MEMSETBYTE_ENABLED

/*!
 * acrMemsetByte
 *
 * @brief    Mimics standard memset for unaligned memory
 *
 * @param[in] pAddr   Memory address to set
 * @param[in] size    Number of bytes to set
 * @param[in] data    byte-value to fill memory
 *
 * @return void
 */
void acrMemsetByte
(
   void *pAddr,
   LwU32 size,
   LwU8  data
)
{
    LwU32  i;
    LwU8  *pBufByte = (LwU8 *)pAddr;

    if (pBufByte != NULL)
    {
        for (i = 0; i < size; i++)
        {
            pBufByte[i] = data;
        }
    }
}
#endif //ACR_MEMSETBYTE_ENABLED
