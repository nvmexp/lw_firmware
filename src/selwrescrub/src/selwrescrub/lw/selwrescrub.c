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
 * @file: psd.c
 */

//
// Includes
//
#include "selwrescrub.h"
#include "dev_lwdec_csb.h"

#ifdef BOOT_FROM_HS_BUILD
#include "hs_entry_checks.h"
#endif

//
// Global Variables
//

typedef unsigned char BYTE;

#ifdef PKC_ENABLED
#include "bootrom_pkc_parameters.h"
#endif


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
SIGNATURE_PARAMS    g_signature     ATTR_OVLY(".data")              ATTR_ALIGNED(16);

#ifdef BOOT_FROM_HS_BUILD
LwBool              g_dummyUnused      ATTR_OVLY(".data")              ATTR_ALIGNED(16);
#endif

//
// Extern Variables
//

//
// Linker script variables holding the start and end
// of secure overlay.
//
extern LwU32 _selwrescrub_code_start[] ATTR_OVLY(".data");
extern LwU32 _selwrescrub_code_end[]   ATTR_OVLY(".data");
extern LwU32 _selwrescrub_data_end[]   ATTR_OVLY(".data");

#ifdef IS_SSP_ENABLED
// variable to store canary for SSP
void * __stack_chk_guard ATTR_OVLY(".bss");

#ifndef BOOT_FROM_HS_BUILD
//
// Setup canary for SSP
// Here we are initializing NS canary to random generated number in NS
//
GCC_ATTRIB_NO_STACK_PROTECT() GCC_ATTRIB_NOINLINE() ATTR_OVLY(".text")
static void __canary_setup(void)
{
    __stack_chk_guard = (void *)RANDOM_CANARY_NS;
}
#endif // ifndef BOOT_FROM_HS_BUILD
#endif //IS_SSP_ENABLED

/*!
 * @brief: Custom start function to replace the default implementation that comes from the toolchain.
           We need a custom start in GP102+ because of DMEMVA. We need to setup SP register correctly
           before jumping to main function. Default start doesn't do this properly.
 */
GCC_ATTRIB_NO_STACK_PROTECT()
void start(void)
{
#ifdef BOOT_FROM_HS_BUILD
    hsEntryChecks();

    //
    // Below is hack to keep data section, as it is needed by siggen.
    // g_signature is used in patching by siggen but since g_signature
    // is not used by Boot From Hs, so compiler ignores it and makes data
    // as Null.
    // Hence we are introducing g_dummyUnused to data section not NULL.
    // This needs to be done after hsEntryChecks since it uses GPRs and we 
    // are clearing GPRs in hsEntryChecks. Also setting SP should be the 
    // first thing we do in HS, which is done in hsEntryChecks.
    //
    {
        g_dummyUnused = LW_TRUE;
    }

    selwrescrubEntry();
#else
    // Initialize SP to DMEM size as reported by HWCFG
    falc_wspr(SP, (DRF_VAL(_CLWDEC, _FALCON_HWCFG, _DMEM_SIZE, falc_ldx_i((unsigned int*)LW_CLWDEC_FALCON_HWCFG, 0)) & 0xff) << 8);

    main();
#endif // ifndef BOOT_FROM_HS_BUILD

    falc_halt();
}

#ifndef BOOT_FROM_HS_BUILD
/*!
 * @brief: main function starts in LS more and jumps to HS after verifying signatures.
 */
int main(void)
{
    LwU32 start = ((LwU32)_selwrescrub_code_start & 0x0FFFFFFF);
    LwU32 end   = ((LwU32)_selwrescrub_code_end & 0x0FFFFFFF);

    falc_stxb_i((LwU32*)LW_CLWDEC_FALCON_MAILBOX0,   0, SELWRESCRUB_RC_ERROR_BIN_STARTED_BUT_NOT_FINISHED);

    // Load the signature first
    asm ("ccr <mt:DMEM,sc:0,sup:0,tc:2> ;");
   

#ifdef PKC_ENABLED
    g_signature.pkc.pk           = NULL;
    g_signature.pkc.hash_saving  = 0;


    falc_stxb_i((LwU32*)LW_CLWDEC_FALCON_BROM_PARAADDR(0),   0, (LwU32)&g_signature);
    falc_stxb_i((LwU32*)LW_CLWDEC_FALCON_BROM_ENGIDMASK,     0, ENGINE_ID);
    falc_stxb_i((LwU32*)LW_CLWDEC_FALCON_BROM_LWRR_UCODE_ID, 0, DRF_NUM(_CLWDEC_FALCON, _BROM_LWRR_UCODE_ID, _VAL, UCODE_ID));
    falc_stxb_i((LwU32*)LW_CLWDEC_FALCON_MOD_SEL,            0, DRF_NUM(_CLWDEC_FALCON, _MOD_SEL, _ALGO, LW_CLWDEC_FALCON_MOD_SEL_ALGO_RSA3K));


#else     
    
    falc_dmwrite(0, ((6 << 16) | (LwU32)(g_signature.aes)));
    falc_dmwait();
#endif // PKC_ENABLED

#ifdef IS_ENHANCED_AES
    // Setup bootrom registers for Enhanced AES support
    falc_stxb_i((LwU32*)LW_CLWDEC_FALCON_BROM_ENGIDMASK,     0, ENGINE_ID);
    falc_stxb_i((LwU32*)LW_CLWDEC_FALCON_BROM_LWRR_UCODE_ID, 0, DRF_NUM(_CLWDEC_FALCON, _BROM_LWRR_UCODE_ID, _VAL, UCODE_ID));
#endif // IS_ENHANCED_AES

#ifdef IS_SSP_ENABLED
    // Setup canary for SSP
    __canary_setup();
#endif  //IS_SSP_ENABLED

    //
    // All the required code is already loaded into IMEM
    // Just need to program the SEC spr and jump into code
    //
    falc_wspr(SEC, SVEC_REG(start, (end - start), 0, ENCRYPT_MODE));

    // Jump to selwrescrub which is HS entry point. We will halt in HS mode, will never return to the code below.
    selwrescrubEntryWrapper();

    return 0;
}
#endif // ifndef BOOT_FROM_HS_BUILD

