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
 * @file: fub.c
 */

//
// Includes
//
#include "dev_lwdec_csb.h"
#ifdef FUB_ON_SEC2
#include "dev_sec_csb.h"
#endif
#include "fub.h"
#include "config/g_fub_private.h"
#include "fub_inline_static_funcs_tu10x.h"
#if (FUBCFG_FEATURE_ENABLED(FUBCORE_TU10X) || FUBCFG_FEATURE_ENABLED(FUBCORE_TU116))
#include "fub_inline_static_funcs_fpf_tu10x.h"
#include "fub_inline_static_funcs_fpf_tu116.h"
#include "fub_inline_static_funcs_tu116.h"
#endif
#if (FUBCFG_FEATURE_ENABLED(FUBCORE_GA10X))
#include "fub_inline_static_funcs_ga100.h"
#endif
#include <falcon-intrinsics.h>

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

SIGNATURE_PARAMS             g_signature  ATTR_OVLY(".data")      ATTR_ALIGNED(FUB_DESC_ALIGN);

FUB_RESERVED_DMEM g_reserved      ATTR_OVLY(".dataEntry") ATTR_ALIGNED(FUB_DESC_ALIGN);

RM_FLCN_FUB_DESC  g_desc          ATTR_OVLY(".dataEntry") ATTR_ALIGNED(FUB_DESC_ALIGN);
// g_canFalcHalt needs to be set to LW_TRUE only after NS restart and VHR empty checks are completed
LwBool            g_canFalcHalt   ATTR_OVLY(".data");

//
// Extern Variables
//

//
// Linker script variables holding the start and end
// of secure overlay.
//
extern LwU32 _fub_code_start[] ATTR_OVLY(".data");
extern LwU32 _fub_code_end[]   ATTR_OVLY(".data");
extern LwU32 _fub_data_end[]   ATTR_OVLY(".data");
#ifdef IS_SSP_ENABLED
// variable to store canary for SSP
void * __stack_chk_guard;

//
// Setup canary for SSP
// Here we are initializing NS canary to random generated number in NS
//
GCC_ATTRIB_NO_STACK_PROTECT()
static void __canary_setup(void)
{
    __stack_chk_guard = (void *)RANDOM_CANARY_NS;
}

#endif
/*!
 * @brief: Custom start function to replace the default implementation that comes from the toolchain.
 *         We need a custom start in GP102+ because of DMEMVA. We need to setup SP register correctly
 *         before jumping to main function. Default start doesn't do this properly.
 */
void start(void)
{
#ifdef FUB_ON_LWDEC
     falc_wspr(SP, (DRF_VAL(_CLWDEC, _FALCON_HWCFG, _DMEM_SIZE, falc_ldxb_i((unsigned int*)LW_CLWDEC_FALCON_HWCFG, 0)) & 0xff) << 8);
#elif FUB_ON_SEC2
     falc_wspr(SP, (DRF_VAL(_CSEC, _FALCON_HWCFG, _DMEM_SIZE, falc_ldxb_i((unsigned int*)LW_CSEC_FALCON_HWCFG, 0)) & 0xff) << 8);
#endif

#ifdef IS_SSP_ENABLED
    // Setup canary for SSP
    __canary_setup();
#endif
     main();
}

/*!
 * @brief: main function starts in NS mode and jumps to HS after verifying signatures.
 */
int main(void)
{
    LwU32 startOffset   = ((LwU32)_fub_code_start & 0x0FFFFFFF);
    LwU32 endOffset     = ((LwU32)_fub_code_end & 0x0FFFFFFF);

#ifdef PKC_ENABLED
#ifdef FUB_ON_SEC2
    g_signature.pkc.pk           = NULL;
    g_signature.pkc.hash_saving  = 0;
    falc_stxb_i((LwU32*)LW_CSEC_FALCON_BROM_PARAADDR(0),   0, (LwU32)&g_signature);
    falc_stxb_i((LwU32*)LW_CSEC_FALCON_BROM_ENGIDMASK,     0, ENGINE_ID);
    falc_stxb_i((LwU32*)LW_CSEC_FALCON_BROM_LWRR_UCODE_ID, 0, DRF_NUM(_CSEC_FALCON, _BROM_LWRR_UCODE_ID, _VAL, UCODE_ID));
    falc_stxb_i((LwU32*)LW_CSEC_FALCON_MOD_SEL,            0, DRF_NUM(_CSEC_FALCON, _MOD_SEL, _ALGO, LW_CSEC_FALCON_MOD_SEL_ALGO_RSA3K));
#else
    // FUB_ON_LWDEC
    ct_assert(0);  
#endif
#else
    //
    // AES Legacy signature verification
    // Load the signature first
    //
    asm ("ccr <mt:DMEM,sc:0,sup:0,tc:2> ;");
    falc_dmwrite(0, ((6 << 16) | (LwU32)(&g_signature.aes)));
    falc_dmwait();
#endif //PKC_ENABLED

    // Set FUB Progress code as entering in HS mode.
#ifdef FUB_ON_LWDEC
    falc_stxb_i((LwU32*) LW_CLWDEC_FALCON_MAILBOX0, 0,  FUB_PROGRESS_BIN_ENTERING_HS_CODE);
#elif FUB_ON_SEC2
    falc_stxb_i((LwU32*) LW_CSEC_FALCON_MAILBOX0, 0,  FUB_PROGRESS_BIN_ENTERING_HS_CODE);
#endif

#ifdef IS_ENHANCED_AES
    // Setup bootrom registers for Enhanced AES support
#ifdef FUB_ON_LWDEC
    falc_stxb_i((LwU32*)LW_CLWDEC_FALCON_BROM_ENGIDMASK,     0, ENGINE_ID);
    falc_stxb_i((LwU32*)LW_CLWDEC_FALCON_BROM_LWRR_UCODE_ID, 0, DRF_NUM(_CLWDEC_FALCON, _BROM_LWRR_UCODE_ID, _VAL, UCODE_ID));
#elif FUB_ON_SEC2
    falc_stxb_i((LwU32*)LW_CSEC_FALCON_BROM_ENGIDMASK,     0, ENGINE_ID);
    falc_stxb_i((LwU32*)LW_CSEC_FALCON_BROM_LWRR_UCODE_ID, 0, DRF_NUM(_CSEC_FALCON, _BROM_LWRR_UCODE_ID, _VAL, UCODE_ID));
#endif // FUB_ON_LWDEC/SEC2

#endif // IS_ENHANCED_AES

    //
    // All the required code is already loaded into IMEM
    // Just need to program the SEC spr and jump into code
    //
    if (endOffset > startOffset)
    {
        falc_wspr(SEC, SVEC_REG(startOffset, (endOffset - startOffset), 0, IS_HS_ENCRYPTED));

        //  Wrapper HS Entry function
        fubEntryWrapper();
    }

    //
    // This should never be reached as we halt in fubEntry
    // Although exception handler halts, in case 
    // HS signature validation fails above, halt here again anyway
    //
    falc_halt(); 

    // FUB will halt in fubEntry itself, so it will never reach here.
    return 0;
}
