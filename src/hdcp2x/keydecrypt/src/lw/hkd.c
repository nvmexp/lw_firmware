/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: hkd.c
 */

//
// Includes
//
#include "hkd.h"

//
// Global Variables
//

LwU8 g_signature[16] ATTR_OVLY(".data") ATTR_ALIGNED(16);
LwU8 g_lc128[16] ATTR_OVLY(".data") ATTR_ALIGNED(16);

LwU8 g_lc128e[] ATTR_OVLY(".data") ATTR_ALIGNED(16) =
{
    0x93,0xce,0x5a,0x56,0xa0,0xa1,0xf4,0xf7,
    0x3c,0x65,0x8a,0x1b,0xd2,0xae,0xf0,0xf7
};

LwU8 g_lc128e_debug[] ATTR_OVLY(".data") ATTR_ALIGNED(16) =
{
    0x93,0xce,0x5a,0x56,0xa0,0xa1,0xf4,0xf7,
    0x3c,0x65,0x8a,0x1b,0xd2,0xae,0xf0,0xf7
};


/*!
 * HkdDecryptLc128
 *
 * Decrypts LC128 global constant.
 *
 */
void hkdDecryptLc128Hs(LwU8 *pLc128e, LwU8 *pOutLc128)
{

    falc_scp_trap(TC_INFINITY);
    falc_scp_secret(HKD_SCP_KEY_INDEX_LC128, SCP_R1);
    falc_scp_encrypt_sig(SCP_R1, SCP_R2);
    falc_scp_rkey10(SCP_R2, SCP_R1);
    falc_scp_key(SCP_R1);
    falc_trapped_dmwrite(falc_sethi_i((LwU32)pLc128e, SCP_R2));
    falc_dmwait();
    falc_scp_decrypt(SCP_R2, SCP_R3);
    falc_trapped_dmread(falc_sethi_i((LwU32)pOutLc128, SCP_R3));
    falc_dmwait();
    falc_scp_forget_sig();
    falc_scp_trap(0);
}

//
// Linker script variables holding the start and end
// of secure overlay.
//
extern LwU32 _ovl1_code_start[] ATTR_OVLY(".data");
extern LwU32 _ovl1_code_end[]   ATTR_OVLY(".data");

int main(int argc, char **Argv)
{
    LwU32 start = ((LwU32)_ovl1_code_start & 0x0FFFFFFF);
    LwU32 end   = ((LwU32)_ovl1_code_end & 0x0FFFFFFF);

    hkdInitErrorCodeNs();

    // Load the signature first
    falc_scp_trap(TC_INFINITY);
    falc_dmwrite(0, ((6 << 16) | (LwU32)(g_signature)));
    falc_dmwait();
    falc_scp_trap(0);

    //
    // All the required code is already loaded into IMEM
    // Just need to program the SEC spr and jump into code
    //
    falc_wspr(SEC, SVEC_REG(start, (end - start), 0, ENABLE_ENCRYPTION));

    // Call the main function to handle ACR
    hkdStart();

    // We should never reach here
    falc_halt();

    return 0;
}
