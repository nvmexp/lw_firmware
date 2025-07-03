/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: main.c : NS part of PUB in C, Will be moved to SPARK in coming instances
 */

//
// Includes
//
#include "fub.h"
#include "dev_sec_csb.h"
#include <falcon-intrinsics.h>
#include "lwuproc.h"
#include "lwctassert.h"

#define INFINITE_LOOP() while(LW_TRUE)

// #ifdef PKC_ENABLED
#include "bootrom_pkc_parameters.h"
// #endif

// TODO : These status Macros are temporary until we have a seperate Status list for SPARK in RM
#define UCODE_ERROR_BIN_STARTED_BUT_NOT_FINISHED 0x1
#define UCODE_ERROR_SSP_STACK_CHECK_FAILED 0x2

typedef union {
    LwU32                               aes[4];
    struct pkc_verification_parameters  pkc;
} SIGNATURE_PARAMS;

ct_assert(LW_OFFSETOF(SIGNATURE_PARAMS, pkc.signature.rsa3k_sig.S) == 0);

SIGNATURE_PARAMS    g_signature     ATTR_OVLY(".data")              ATTR_ALIGNED(16);

ACR_RESERVED_DMEM   g_reserved      ATTR_OVLY(".dataEntry")         ATTR_ALIGNED(16);

RM_FLCN_ACR_DESC    g_desc          ATTR_OVLY(".dataEntry")         ATTR_ALIGNED(16);

//
// Linker script variables holding the start and end
// of secure overlay.
//
extern LwU32 _pub_code_start[]           ATTR_OVLY(".data");
extern LwU32 _pub_code_end[]             ATTR_OVLY(".data");
extern LwU32 _pub_resident_code_start[]  ATTR_OVLY(".data");
extern LwU32 _pub_resident_code_end[]    ATTR_OVLY(".data");

#define SE_TRNG_SIZE_DWORD_32           1

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
    falc_stxb_i((LwU32*) LW_CSEC_FALCON_MAILBOX0, 0, UCODE_ERROR_SSP_STACK_CHECK_FAILED);
    INFINITE_LOOP();
}

//
// Setup canary for SSP
// Here we are initializing NS canary to random generated number in NS
//
GCC_ATTRIB_NO_STACK_PROTECT() GCC_ATTRIB_NOINLINE()
static void __canary_setup(void)
{
    __stack_chk_guard = (void *)RANDOM_CANARY_NS;
}

#define PUB_PC_TRIM(pc) (pc & 0x0FFFFFFF)

int main(int argc, char **Argv)
{
    LwU32 start       = (PUB_PC_TRIM((LwU32)_pub_code_start));
    LwU32 end         = (PUB_PC_TRIM((LwU32)_pub_code_end));

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

    // Setup canary for SSP
    __canary_setup();

    falc_stxb_i((LwU32*) LW_CSEC_FALCON_MAILBOX0, 0,  UCODE_ERROR_BIN_STARTED_BUT_NOT_FINISHED);

#ifdef PKC_ENABLED
    g_signature.pkc.pk           = NULL;
    g_signature.pkc.hash_saving  = 0;

    falc_stxb_i((LwU32*)LW_CSEC_FALCON_BROM_PARAADDR(0),   0, (LwU32)&g_signature);
    falc_stxb_i((LwU32*)LW_CSEC_FALCON_BROM_ENGIDMASK,     0, ENGINE_ID);
    falc_stxb_i((LwU32*)LW_CSEC_FALCON_BROM_LWRR_UCODE_ID, 0, DRF_NUM(_CSEC_FALCON, _BROM_LWRR_UCODE_ID, _VAL, UCODE_ID));
    falc_stxb_i((LwU32*)LW_CSEC_FALCON_MOD_SEL,            0, DRF_NUM(_CSEC_FALCON, _MOD_SEL, _ALGO, LW_CSEC_FALCON_MOD_SEL_ALGO_RSA3K));
#else
    //
    // AES Legacy signature verification
    //

    // Trap next 2 dm* operations, i.e. target will be SCP instead of FB
    falc_scp_trap(TC_2);
    // falc_stxb_i((LwU32*) LW_CSEC_MAILBOX(0), 0,  g_signature[0]);
    // falc_stxb_i((LwU32*) LW_CSEC_MAILBOX(1), 0,  g_signature[1]);
    // falc_stxb_i((LwU32*) LW_CSEC_MAILBOX(2), 0,  g_signature[2]);
    // falc_stxb_i((LwU32*) LW_CSEC_MAILBOX(3), 0,  g_signature[3]);

    // Load the signature to SCP
    falc_dmwrite(0, ((6 << 16) | (LwU32)(&g_signature)));
    falc_dmwait();
#endif // PKC_ENABLED

    //
    // All the required code is already loaded into IMEM
    // Just need to program the SEC spr and jump into code
    //
    falc_wspr(SEC, SVEC_REG(start, (end - start), 0, IS_HS_ENCRYPTED));

    fub_entry_wrapper();

    falc_halt();

    return 0;
}
