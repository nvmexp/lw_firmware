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
 * @file: booter_start.c
 */

//
// Includes
//
#include "booter.h"
#include "sec2mutexreservation.h"

#ifdef BOOT_FROM_HS_BUILD
#include "hs_entry_checks.h"
#endif

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
LwBool    g_dummyUnused    ATTR_OVLY(".data")    ATTR_ALIGNED(BOOTER_SIG_ALIGN);
#endif

SIGNATURE_PARAMS     g_signature    ATTR_OVLY(".data")      ATTR_ALIGNED(BOOTER_SIG_ALIGN);

BOOTER_RESERVED_DMEM g_reserved     ATTR_OVLY(".dataEntry") ATTR_ALIGNED(BOOTER_DESC_ALIGN);

BOOTER_DMA_PROP      g_dmaProp[BOOTER_DMA_CONTEXT_COUNT]          ATTR_OVLY(".data") ATTR_ALIGNED(256);

GspFwWprMeta         g_gspFwWprMeta  ATTR_OVLY(".data") ATTR_ALIGNED(256);

#define SE_TRNG_SIZE_DWORD_32           1

#ifdef IS_SSP_ENABLED
// variable to store canary for SSP
void * __stack_chk_guard;

// Local implementation of SSP handler
void __stack_chk_fail(void)
{
    //
    // Update error code
    // We cannot use FALC_BOOTER_WITH_ERROR here as SSP protection start early even before CSBERRSTAT is checked
    // and also we cannot halt since NS restart and VHR empty check could be remaining
    //
#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    falc_stxb_i((LwU32*) LW_CSEC_FALCON_MAILBOX0, 0, BOOTER_ERROR_SSP_STACK_CHECK_FAILED);
#elif defined(BOOTER_RELOAD)
    falc_stxb_i((LwU32*) LW_CLWDEC_FALCON_MAILBOX0, 0, BOOTER_ERROR_SSP_STACK_CHECK_FAILED);
#else
    ct_assert(0);
#endif

    // coverity[no_escape]
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
// ReInitialize canary for HS
// HS canary is initialized to random generated number in HS using SCP
//
GCC_ATTRIB_ALWAYSINLINE()
static inline void booterInitializeStackCanaryHSUsingSCP(void)
{
    BOOTER_STATUS status;
    LwU32 rand32;

    //
    // Set the value of stack canary to a random value to ensure adversary
    // can not craft an attack by looking at the assembly to determine the canary value
    //
    status = booterScpGetRandomNumber_HAL(pBooter, &rand32);
    if (status != BOOTER_OK)
    {
        FAIL_BOOTER_WITH_ERROR(status);
    }
    else
    {
        __stack_chk_guard = (void *) rand32;
    }
}
#endif // ifndef BOOT_FROM_HS_BUILD

//
// ReInitialize canary for HS
// HS canary is initialized to random generated number (TRNG) in HS using SE
//
GCC_ATTRIB_ALWAYSINLINE()
static inline void booterInitializeStackCanaryHSUsingSE(void)
{
    BOOTER_STATUS status;
    LwU32 rand32;

    //
    // Set the value of stack canary to a random value to ensure adversary
    // can not craft an attack by looking at the assembly to determine the canary value
    //
    status = booterGetTRand_HAL(pBooter, &rand32, SE_TRNG_SIZE_DWORD_32);
    if (status != BOOTER_OK)
    {
        FAIL_BOOTER_WITH_ERROR(status);
    }
    else
    {
        __stack_chk_guard = (void *) rand32;
    }
}
#endif // IS_SSP_ENABLED

#ifndef BOOT_FROM_HS_BUILD
/*!
 * @brief Stack Underflow Exception Installer for enabling the stack underflow exception in secure segment by
 * setting up the stackcfg register with stackbottom value.
 * Using falc_ldxb/stxb_i instruction here because there should be no falc_halt till VHR check and
 * BOOTER_REG_RD32/WR32 macros have falc_halt in there code.
 *
 * @param None.
 *
 * @return None.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
void
booterStackUnderflowInstaller(void)
{
    LwU32 stackBottomLimit = BOOTER_STACK_BOTTOM_LIMIT;
    LwU32 var;
    //
    // STACKCFG.BOTTOM is word address, if the SP is equal or less than (STACKCFG.BOTTOM << 2),
    // then an exception will be generated. Therefore pushing stackBottomLimit >> 2.
    //
    stackBottomLimit = stackBottomLimit >> 2;
#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    var = BOOTER_CSB_REG_RD32_INLINE(LW_CSEC_FALCON_STACKCFG);
    var = FLD_SET_DRF_NUM( _CSEC, _FALCON_STACKCFG, _BOTTOM, stackBottomLimit, var);
    var = FLD_SET_DRF( _CSEC, _FALCON_STACKCFG, _SPEXC, _ENABLE, var );
    BOOTER_CSB_REG_WR32_INLINE(LW_CSEC_FALCON_STACKCFG, var);
#elif defined(BOOTER_RELOAD)
    var = BOOTER_CSB_REG_RD32_INLINE(LW_CLWDEC_FALCON_STACKCFG);
    var = FLD_SET_DRF_NUM( _CLWDEC, _FALCON_STACKCFG, _BOTTOM, stackBottomLimit, var );
    var = FLD_SET_DRF( _CLWDEC, _FALCON_STACKCFG, _SPEXC, _ENABLE, var );
    BOOTER_CSB_REG_WR32_INLINE(LW_CLWDEC_FALCON_STACKCFG, var);
#endif
}

/*!
 * Exception handler which will reside in the secure section. This subroutine will be exelwted whenever exception is triggered.
 */
void booterExceptionHandlerSelwreHang(void)
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
void booterExceptionInstallerSelwreHang(void)
{
    LwU32 secSpr = 0;

#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    BOOTER_CSB_REG_WR32_INLINE( LW_CSEC_FALCON_IBRKPT1, 0x00000000 );
    BOOTER_CSB_REG_WR32_INLINE( LW_CSEC_FALCON_IBRKPT2, 0x00000000 );
    BOOTER_CSB_REG_WR32_INLINE( LW_CSEC_FALCON_IBRKPT3, 0x00000000 );
    BOOTER_CSB_REG_WR32_INLINE( LW_CSEC_FALCON_IBRKPT4, 0x00000000 );
    BOOTER_CSB_REG_WR32_INLINE( LW_CSEC_FALCON_IBRKPT5, 0x00000000 );
#elif defined(BOOTER_RELOAD)
    BOOTER_CSB_REG_WR32_INLINE( LW_CLWDEC_FALCON_IBRKPT1, 0x00000000 );
    BOOTER_CSB_REG_WR32_INLINE( LW_CLWDEC_FALCON_IBRKPT2, 0x00000000 );
    BOOTER_CSB_REG_WR32_INLINE( LW_CLWDEC_FALCON_IBRKPT3, 0x00000000 );
    BOOTER_CSB_REG_WR32_INLINE( LW_CLWDEC_FALCON_IBRKPT4, 0x00000000 );
    BOOTER_CSB_REG_WR32_INLINE( LW_CLWDEC_FALCON_IBRKPT5, 0x00000000 );
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

    falc_wspr( EV, booterExceptionHandlerSelwreHang );

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
void booterValidateCsberrstat(void)
{
    LwU32 csbErrStatVal;
#if defined(BOOTER_LOAD) || (BOOTER_UNLOAD)
        csbErrStatVal = BOOTER_CSB_REG_RD32_INLINE(LW_CSEC_FALCON_CSBERRSTAT);
        csbErrStatVal = FLD_SET_DRF(_CSEC, _FALCON_CSBERRSTAT, _ENABLE, _TRUE, csbErrStatVal);
        BOOTER_CSB_REG_WR32_INLINE(LW_CSEC_FALCON_CSBERRSTAT,csbErrStatVal);
#elif defined(BOOTER_RELOAD)
        csbErrStatVal = BOOTER_CSB_REG_RD32_INLINE(LW_CLWDEC_FALCON_CSBERRSTAT);
        csbErrStatVal = FLD_SET_DRF(_CLWDEC, _FALCON_CSBERRSTAT, _ENABLE, _TRUE, csbErrStatVal);
        BOOTER_CSB_REG_WR32_INLINE(LW_CLWDEC_FALCON_CSBERRSTAT, csbErrStatVal);
#else
    ct_assert(0);
#endif
}
#endif // ifndef BOOT_FROM_HS_BUILD

/*!
 * Function to scrub unused DMEM from startAddr to endAddr
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
void
booterScrubDmem(LwU32 startAddr, LwU32 endAddr)
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
    booterEntryFunction();
#else
    // initialize SP to DMEM size
#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    falc_wspr(SP, LW_CSEC_FLCN_DMEM_SIZE_IN_BLK << LW_CSEC_FLCN_DMEM_BLK_ALIGN_BITS );
#elif defined(BOOTER_RELOAD)
    falc_wspr(SP, LW_CLWDEC_FLCN_DMEM_SIZE_IN_BLK << LW_CLWDEC_FLCN_DMEM_BLK_ALIGN_BITS );
#endif

    main();
#endif // ifndef BOOT_FROM_HS_BUILD

    falc_halt();
}

#ifndef BOOT_FROM_HS_BUILD
int main(void)
{
    LwU32 start       = (BOOTER_PC_TRIM((LwU32)_booter_code_start));
    LwU32 end         = (BOOTER_PC_TRIM((LwU32)_booter_code_end));

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

#ifdef IS_SSP_ENABLED
    // Setup canary for SSP
    __canary_setup();
#endif

    // Set DEFAULT error code
#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    falc_stxb_i((LwU32*) LW_CSEC_FALCON_MAILBOX0, 0, BOOTER_ERROR_BIN_STARTED_BUT_NOT_FINISHED);
#elif defined(BOOTER_RELOAD)
    falc_stxb_i((LwU32*) LW_CLWDEC_FALCON_MAILBOX0, 0, BOOTER_ERROR_BIN_STARTED_BUT_NOT_FINISHED);
#else
    ct_assert(0);
#endif

    // Setup bootrom registers for Enhanced AES / PKC support
#ifdef IS_ENHANCED_AES
#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    falc_stxb_i((LwU32*)LW_CSEC_FALCON_BROM_ENGIDMASK, 0, ENGINE_ID);
    falc_stxb_i((LwU32*)LW_CSEC_FALCON_BROM_LWRR_UCODE_ID, 0, DRF_NUM(_CSEC_FALCON, _BROM_LWRR_UCODE_ID, _VAL, UCODE_ID));
#elif defined(BOOTER_RELOAD)
    falc_stxb_i((LwU32*)LW_CLWDEC_FALCON_BROM_ENGIDMASK, 0, ENGINE_ID);
    falc_stxb_i((LwU32*)LW_CLWDEC_FALCON_BROM_LWRR_UCODE_ID, 0, DRF_NUM(_CLWDEC_FALCON, _BROM_LWRR_UCODE_ID, _VAL, UCODE_ID));
#else
    ct_assert(0);
#endif // For booter profile based check
#endif // For IS_ENHANCED_AES

#ifdef PKC_ENABLED
    g_signature.pkc.pk           = NULL;
    g_signature.pkc.hash_saving  = 0;

#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    falc_stxb_i((LwU32*)LW_CSEC_FALCON_BROM_PARAADDR(0),   0, (LwU32)&g_signature);
    falc_stxb_i((LwU32*)LW_CSEC_FALCON_BROM_ENGIDMASK,     0, ENGINE_ID);
    falc_stxb_i((LwU32*)LW_CSEC_FALCON_BROM_LWRR_UCODE_ID, 0, DRF_NUM(_CSEC_FALCON, _BROM_LWRR_UCODE_ID, _VAL, UCODE_ID));
    falc_stxb_i((LwU32*)LW_CSEC_FALCON_MOD_SEL,            0, DRF_NUM(_CSEC_FALCON, _MOD_SEL, _ALGO, LW_CSEC_FALCON_MOD_SEL_ALGO_RSA3K));
#elif defined(BOOTER_RELOAD)
    falc_stxb_i((LwU32*)LW_CLWDEC_FALCON_BROM_PARAADDR(0),   0, (LwU32)&g_signature);
    falc_stxb_i((LwU32*)LW_CLWDEC_FALCON_BROM_ENGIDMASK,     0, ENGINE_ID);
    falc_stxb_i((LwU32*)LW_CLWDEC_FALCON_BROM_LWRR_UCODE_ID, 0, DRF_NUM(_CLWDEC_FALCON, _BROM_LWRR_UCODE_ID, _VAL, UCODE_ID));
    falc_stxb_i((LwU32*)LW_CLWDEC_FALCON_MOD_SEL,            0, DRF_NUM(_CLWDEC_FALCON, _MOD_SEL, _ALGO, LW_CLWDEC_FALCON_MOD_SEL_ALGO_RSA3K));
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

    // Booter entry point
    booterEntryWrapper();

    //
    // This should never be reached as we halt in booterEntryFunction for all Booter binaries
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
booterClearSCPRegisters(void)
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
booterClearFalconGprs(void)
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
 * Wrapper function to set SP to max of DMEM and the call Booter Entry.
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
booterEntryWrapper(void)
{

    //
    // Using falc_wspr intruction to set SP to last physically accessible(by falcon) 4byte aligned address(0xFFFC)
    // On GA100 and prior setting SP = 0x10000 rolls its value to 0x0 which is not desirable when following HS init
    // sequence.
    //
#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    falc_wspr(SP, ((LW_CSEC_FLCN_DMEM_SIZE_IN_BLK << LW_CSEC_FLCN_DMEM_BLK_ALIGN_BITS)-4));
#elif defined(BOOTER_RELOAD)
    falc_wspr(SP, ((LW_CLWDEC_FLCN_DMEM_SIZE_IN_BLK << LW_CLWDEC_FLCN_DMEM_BLK_ALIGN_BITS)-4));
#else
    ct_assert(0);
#endif

    // Clear SCP signature
    falc_scp_forget_sig();

    // Clear all SCP registers
    booterClearSCPRegisters();

    // Reset all falcon GPRs (a0-a15)
    booterClearFalconGprs();

    // Validate CSBERRSTAT register for pri error handling.
    booterValidateCsberrstat();

    // Write CMEMBASE = 0, to make sure ldd/std instructions are not routed to CSB.
#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    BOOTER_CSB_REG_WR32_INLINE(LW_CSEC_FALCON_CMEMBASE, 0);

    // Scrub DMEM from stackBottom to current stack pointer.
    booterScrubDmem((LwU32)_bss_start, (LW_CSEC_FLCN_DMEM_SIZE_IN_BLK << LW_CSEC_FLCN_DMEM_BLK_ALIGN_BITS));
#elif defined(BOOTER_RELOAD)
    // LWDEC doesn't have CMEM, no need to update CMEMBASE

    // Scrub DMEM from stackBottom to current stack pointer.
    booterScrubDmem((LwU32)_bss_start, (LW_CLWDEC_FLCN_DMEM_SIZE_IN_BLK << LW_CLWDEC_FLCN_DMEM_BLK_ALIGN_BITS));
#else
    ct_assert(0);
#endif

   // Make sure different master cannot restart Booter in case it halts in HS mode
    booterMitigateNSRestartFromHS_HAL(pBooter);

    // Setup the exception handler
    booterExceptionInstallerSelwreHang();

    // Set the stack pointer to the MIN addr so if stack reaches that address then falcon can throw an exception.
    booterStackUnderflowInstaller();

    // Reset both SCP instruction sequencers and Pipe Reset to put SCP in a deterministic state
    booterResetScpLoopTracesAndPipe();

#ifdef IS_SSP_ENABLED

    //
    // We use SCP based random generator for HS stack canary since it is inlined.
    // This also serves as an early and extra stack canary setup for dGPU which later uses SE
    // Initialize canary to random number generated by HS - SCP
    // TODO : Save NS canary in case we decide to return to NS in future (lwrrently we halt in this function)
    //
    booterInitializeStackCanaryHSUsingSCP();

#endif // IS_SSP_ENABLED
    //
    // This is the function where actual Booter starts.
    // We halt in booterEntryFunction itself. Binary won't return back to this function.
    //
    booterEntryFunction();

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
booterEntryFunction(void)
{
    // Declaring variables at the start, not initializing as we are going to clear falcon GPRs
    BOOTER_STATUS status, statusCleanup;
    LwU8 acrMutexOnlyAllowOneAcrBinaryAtAnyTime;
    LwU32 ucodeBuildVersion;

    // Initializing variables after clearing falcon GPRs
    status = BOOTER_OK;
    ucodeBuildVersion = 0;

#ifndef BOOT_FROM_HS_BUILD
#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    // Lock falcon reset so that no one can reset Booter
    // Note that LWDEC doesn't have secure reset, so we don't do this for reload
    if (BOOTER_OK != (status = booterSelfLockFalconReset_HAL(pBooter, LW_TRUE)))
    {
        FAIL_BOOTER_WITH_ERROR(status);
    }
#endif
    //
    // Set BAR0 timeout value
    // WARNING: This function must be called before any BAR0 access.
    //
    booterSetBar0Timeout_HAL(pBooter);
#endif // ifndef BOOT_FROM_HS_BUILD

#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    //
    // Enable SCPM back again, as it gets reset with SEC2 reset that happens prior to LOAD/UNLOAD exelwtion.
    // Refer BUG 3078892 for more details.
    //
    booterEnableLwdclkScpmForBooter_HAL();
#endif

#ifdef IS_SSP_ENABLED
    //
    // Reinitialize canary to random number generated by HS - SE engine
    // TODO : Save HS SCP canary in case we decide to return to Wrapper in future (lwrrently we halt in this function)
    //
    booterInitializeStackCanaryHSUsingSE();
#endif

    // Verify if this binary should be allowed to run on particular chip
    if (BOOTER_OK != (status = booterCheckIfBuildIsSupported_HAL(pBooter)))
    {
        FAIL_BOOTER_WITH_ERROR(status);
    }

    // Verify if binary is running on expected falcon/engine
    if (BOOTER_OK != (status = booterCheckIfEngineIsSupported_HAL(pBooter)))
    {
        FAIL_BOOTER_WITH_ERROR(status);
    }

    // Get SW defined ucode build version
    if (BOOTER_OK != (status = booterGetUcodeBuildVersion_HAL(pBooter, &ucodeBuildVersion)))
    {
        FAIL_BOOTER_WITH_ERROR(status);
    }

    //
    // Check SW build version against fpf version. Normally, we would also
    // check the corresponding non-FPF fuse on certain chips, but no such fuse
    // exists for Booter, so we just check the FPF fuse only here (we've
    // ensured that it is available even on pre-Ampere chips).
    //
    if (BOOTER_OK != (status = booterCheckFuseRevocationAgainstHWFpfVersion_HAL(pBooter, ucodeBuildVersion)))
    {
        FAIL_BOOTER_WITH_ERROR(status);
    }

#if BOOT_FROM_HS_BUILD
    // Check whether Bootstrap and FWSEC have run successfully to ensure Chain of Trust.
    if (BOOTER_OK != (status = booterCheckChainOfTrust_HAL(pBooter)))
    {
        FAIL_BOOTER_WITH_ERROR(status);
    }
#endif // BOOT_FROM_HS_BUILD


    // Check if Priv Sec is enabled on Prod board
    if (BOOTER_OK != (status = booterCheckIfPrivSecEnabledOnProd_HAL(pBooter)))
    {
        FAIL_BOOTER_WITH_ERROR(status);
    }

    //
    // Check Booter / ACR versions in secure scratch (to ensure correct ucode sequences)
    // Note: acquires and releases SEC2_MUTEX_VPR_SCRATCH
    //
    if (BOOTER_OK != (status = booterCheckBooterVersionInSelwreScratch_HAL(pBooter, ucodeBuildVersion)))
    {
        FAIL_BOOTER_WITH_ERROR(status);
    }

    // First acquire the ACR mutex before doing anything else (to lock out other Booter and ACR ucodes)
    {
        // coverity can't see that we initialize acrMutexOnlyAllowOneAcrBinaryAtAnyTime in booterAcquireSelwreMutex_HAL
        acrMutexOnlyAllowOneAcrBinaryAtAnyTime = 0;

        status = booterAcquireSelwreMutex_HAL(pBooter, SEC2_MUTEX_ACR_ALLOW_ONLY_ONE_ACR_BINARY_AT_ANY_TIME, &acrMutexOnlyAllowOneAcrBinaryAtAnyTime);
        if (status != BOOTER_OK)
        {
            FAIL_BOOTER_WITH_ERROR(status);
        }
    }

    booterValidateBlocks_HAL();

#if defined(BOOTER_LOAD)
    if (BOOTER_OK != (status = booterInit()))
    {
        goto done;
    }
#endif

#if defined(BOOTER_RELOAD)
    if (BOOTER_OK != (status = booterReInit()))
    {
        goto done;
    }
#endif

#if defined(BOOTER_UNLOAD)
    if (BOOTER_OK != (status = booterDeInit()))
    {
        goto done;
    }
#endif

done:
    statusCleanup = booterReleaseSelwreMutex_HAL(pBooter, SEC2_MUTEX_ACR_ALLOW_ONLY_ONE_ACR_BINARY_AT_ANY_TIME, acrMutexOnlyAllowOneAcrBinaryAtAnyTime);

    if (status == BOOTER_OK)
    {
        status = statusCleanup;
    }
    else
    {
        //
        // Booter has errored, leave an invalid Booter binary version.
        // - Don't leave Booter binary version as 0 (otherwise Booter Load
        //   could run on top of Booter Load).
        // - Don't leave actual Booter binary version (otherwise other Booter
        //   ucodes could run).
        //
        ucodeBuildVersion = BOOTER_UCODE_BUILD_VERSION_ILWALID;
    }


    //
    // Write or clear Booter binary version in secure scratch
    // Note: acquires and releases SEC2_MUTEX_VPR_SCRATCH
    //
    statusCleanup = booterWriteBooterVersionToSelwreScratch_HAL(pBooter, ucodeBuildVersion);
    if (status == BOOTER_OK)
    {
        status = statusCleanup;
    }

    // Update mailbox with Booter ucode status
    booterWriteStatusToFalconMailbox_HAL(pBooter, status);
    if (status != BOOTER_OK)
    {
        falc_halt();
    }

    //
    // Perform cleanup activities and halt
    // Reset all SCP registers in case one of the preceding calls forgot to scrub confidential data.
    // Reset all falcon GPRs as additional safety measure.
    // Halt in HS happens for all binaries
    //
    booterCleanupAndHalt();

    // TODO : Restore HS SCP canary in case we decide to return to Wrapper in future (lwrrently we halt in this function)
}

/*!
 * @brief Reset all SCP GPRs, falcon GPRs and halt
 */
void
booterCleanupAndHalt(void)
{
    LwU32 secSpr    = 0;

    // Reset both SCP instruction sequencers and Pipe Reset to put SCP in a deterministic state
    booterResetScpLoopTracesAndPipe();

    //
    // Leaving intrs/exceptions may create avenues to jump back into HS, although there is no
    // identified vulnerability, there is no good reason to leave intrs/exceptions enabled.
    //
    falc_rspr( secSpr, SEC );
    // SET DISABLE_EXCEPTIONS
    secSpr = FLD_SET_DRF( _FALC, _SEC_SPR, _DISABLE_EXCEPTIONS, _SET, secSpr );
    falc_wspr( SEC, secSpr );

    // Reset all SCP registers.
    booterClearSCPRegisters();

    // Unlock falcon reset to original mask value
#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
#if BOOT_FROM_HS_BUILD
    hsSelfLockFalconReset(LW_FALSE);
#else
    booterSelfLockFalconReset_HAL(pBooter, LW_FALSE);
#endif // BOOT_FROM_HS_BUILD
#endif // defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    // Scrub DMEM from stackBottom to current stack pointer.

#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    booterScrubDmem(BOOTER_START_ADDR_OF_DMEM, (LW_CSEC_FLCN_DMEM_SIZE_IN_BLK << LW_CSEC_FLCN_DMEM_BLK_ALIGN_BITS));
#elif defined(BOOTER_RELOAD)
    booterScrubDmem(BOOTER_START_ADDR_OF_DMEM, (LW_CLWDEC_FLCN_DMEM_SIZE_IN_BLK << LW_CLWDEC_FLCN_DMEM_BLK_ALIGN_BITS));
#else
    ct_assert(0);
#endif

    // Reset all falcon GPRs (a0-a15)
    booterClearFalconGprs();

    // NOTE: Do not put any C code above this as GPRs are cleared
    falc_halt();
}

/*
 * This function will return true if falcon with passed falconId is supported.
 */
LwBool
booterIsFalconSupported(LwU32 falconId)
{
//TODO add support for checking if falcon is present from hardware.
    switch(falconId)
    {
        case LSF_FALCON_ID_GSP_RISCV:
            return LW_TRUE;
        default:
            return LW_FALSE;
    }
}

/*!
 * booterMemcmp
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
LwU8 booterMemcmp(const void *pS1, const void *pS2, LwU32 size)
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

/*!
 * booterMemsetByte
 *
 * @brief    Mimics standard memset for unaligned memory
 *
 * @param[in] pAddr   Memory address to set
 * @param[in] size    Number of bytes to set
 * @param[in] data    byte-value to fill memory
 *
 * @return void
 */
void booterMemsetByte
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
