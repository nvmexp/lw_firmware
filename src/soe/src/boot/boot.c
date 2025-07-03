/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   boot.c
 * @brief  main file for SOE Boot binary
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "lwoslayer.h"
#include "dev_soe_csb.h"
#include "dev_fuse.h"

/* ------------------------- Application Includes --------------------------- */
#include "scp_rand.h"
#include "scp_internals.h"
#include "scp_crypt.h"
#include "boot.h"
#include "bootutils.h"
#include "bootrom_pkc_parameters.h"
#include "soe_bar0_hs_inlines.h"

// Inserts Assembly to jump to a new 4-byte aligned address in IMEM and calls into function.
#define _selwre_call(func, suffix) \
{\
    asm volatile ("jmp _selwre_call_" #suffix  ";\n" ".align 4\n" "_selwre_call_" #suffix ":\n" "lcall `(24@lo(_" #func "));\n" ::: "memory"); \
}

#define _SELWRE_CALL(func, suffix)  _selwre_call(func, suffix)
#define SELWRE_CALL(func)   _SELWRE_CALL(func, __COUNTER__)

// Insert Assembly 2 JMP statements
#define _dummy_jmp(suffix1, suffix2) \
    asm volatile ("ljmp `(.L_next_" #suffix1   ");\n" ".L_next_" #suffix1 ":\n" "ljmp `(.L_next_" #suffix2  ");\n" ".L_next_" #suffix2 ":\n"  ::: "memory")


#define _DUMMY_JMP(suffix1, suffix2) _dummy_jmp(suffix1, suffix2)
#define DUMMY_JMP()   _DUMMY_JMP(__COUNTER__, __COUNTER__)

/* ------------------------- Prototypes ------------------------------------- */

/* ------------------------- Global Variables ------------------------------- */

// Variables from link script.
extern LwU32 _imem_hsBase[];
extern LwU32 _imem_hsSize[];

//
// START: DMEM Variables defined under .bss_hs and .data_hs are accessed in HS routines.
// These needs to have constant DMEM Location.
// If these change location in DMEM, then HS uCode will need to be re-signed.
// (which we want to limit once we have production signed the code).
//
// DATA_HS: These variables are patched or initialized. .data section should have constant offset,
//         and .data_hs should be the first section to be emitted for constant addresses.
//

#define LW_MAX_HS_SIGNATURES 10

LwU8 gSignatureHs[LW_MAX_HS_SIGNATURES][RSA3K_SIZE_IN_BITS/8]
    GCC_ATTRIB_SECTION("data_hs", "gSignatureHs")
    GCC_ATTRIB_ALIGN(SCP_SIG_SIZE_IN_BYTES);
struct pkc_verification_parameters gHsSignatureVerificationParams
    GCC_ATTRIB_SECTION("data_hs", "gHsSignatureVerificationParams")
    GCC_ATTRIB_ALIGN(SCP_SIG_SIZE_IN_BYTES);
LwU8 gSignatureLsDmem[SCP_SIG_SIZE_IN_BYTES]
    GCC_ATTRIB_SECTION("data_hs", "gSignatureLsDmem")
    GCC_ATTRIB_ALIGN(SCP_SIG_SIZE_IN_BYTES);
LwU8 gSignatureLsImem[SCP_SIG_SIZE_IN_BYTES]
    GCC_ATTRIB_SECTION("data_hs", "gSignatureLsImem")
    GCC_ATTRIB_ALIGN(SCP_SIG_SIZE_IN_BYTES);

union pkc_pk gPkcSWKey
    GCC_ATTRIB_ALIGN(SCP_SIG_SIZE_IN_BYTES)
    GCC_ATTRIB_SECTION("data", "gPkcSWKey") = 
{
    .rsa3k_pk = 
    {
        {
            0xe4f7f5ef, 0xee6f0434, 0xa742283d, 0xb837517d, 0x49e4d804, 0xd8d4d342, 0xa7c8bf31, 0xe15c25cf,
            0x3acb76c7, 0x6ca73cfc, 0xc5002b4d, 0x9dc59b12, 0x8f11945c, 0xe814622b, 0xc745df38, 0xc179824e,
            0x50ed0464, 0x40f4b2de, 0x4ea86960, 0xacb935cf, 0xf1b2d5f8, 0xa4ba5e6f, 0x90a0a1eb, 0x69da0dfa,
            0x6c9df4ee, 0x8839b01e, 0x66f4aabc, 0xb7f981bd, 0x8a4d967e, 0x56d405c6, 0xf32206c3, 0x99657af8,
            0x6ee38993, 0xcc7f8e9c, 0xf713fc76, 0xb7cd1802, 0xd1c3cd47, 0xf48ce818, 0xdc6df38a, 0x28ad0e5c,
            0x1875327b, 0x90c3b70e, 0x7f15e88c, 0x1b74c593, 0x5ac7d9b0, 0xedd70474, 0xed91afeb, 0x991150af,
            0x48da5e52, 0xe1d0112b, 0x7d36d470, 0x4d7daf15, 0x34a604f5, 0x9eab147d, 0x2dfad689, 0xf464bcb8,
            0xdd03d089, 0x86921b53, 0x9f2aed2c, 0xed4c2dfd, 0xab48a852, 0x28c194b9, 0x2389a853, 0x257bb576,
            0xb5251435, 0xfbab449e, 0xbdb9f479, 0xf2678bcf, 0xda80d6b6, 0xc74fa6cf, 0xfd2a7488, 0x6909c425,
            0x02a16ec5, 0x461d82f0, 0xd5a884a2, 0x4ec34431, 0xc169c889, 0x79d65f21, 0xc2ceb8cb, 0xd39c7ce6,
            0x88c62a3d, 0x831f5ac7, 0x33ee1132, 0x06195cbd, 0x9c372678, 0xb8b0c345, 0x83658b0f, 0x8d2ecce4,
            0xf2d64358, 0xdd67364d, 0x67644a8e, 0x50a87a4d, 0xcd0b0f87, 0xc67f1397, 0x31c5f225, 0x91a4f825,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        },
        {
            0x00010001, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        },
        {
            0x2cf026f1, 0x3400ca44, 0x2a940a6c, 0x3a85ce16, 0x9aca2a85, 0x13819510, 0x6e0aaeba, 0xb1742a27,
            0x1721de4e, 0x339f3918, 0xbe2f3205, 0x51e4c344, 0x577fe823, 0x92b0329f, 0x7443aa3e, 0x98b71f36,
            0x0f3d1d23, 0x60cf1800, 0x7b94d115, 0xf1bf27af, 0x458b89ed, 0x2eebe4fa, 0xb0438bf8, 0x6c4879ea,
            0x62d74d7f, 0x19640e2a, 0xefb7cdf9, 0xc6aa04ba, 0xa5dcbb8f, 0xfa8142c3, 0x703938dc, 0x35117442,
            0xdf5336a2, 0xa1b5a533, 0xb1544d87, 0x21bfc273, 0x9e5a18f2, 0x043ce72c, 0xca3660bf, 0x6bd94061,
            0x0ad69c2c, 0xcdf5d0e5, 0x124fb288, 0xf6a9af90, 0xf6b9e34e, 0x44b8aa6b, 0x9f6fa7d0, 0x88c571b1,
            0xccd9f95c, 0xdc121aaa, 0xeacf15d9, 0x4e1336bd, 0xacbc5e30, 0x8c4e374d, 0xaa8bc375, 0x26f67ae6,
            0x75a33f24, 0x966c871a, 0xa4ce0cae, 0x7b24b31d, 0x93fc7cea, 0xa8f3f2ce, 0x199a1be6, 0x58c24fa6,
            0x027aa313, 0x85b39a1c, 0xd558c04b, 0x8ca3a97c, 0x7e769b21, 0x93f0e528, 0xfa6d701a, 0x00c7b1f5,
            0x2ef890de, 0xd5dad21b, 0xaf5c0c11, 0x777585ae, 0x57efc20e, 0x102aaa14, 0x285727d6, 0x4fc0575f,
            0xe59129c1, 0x4d335e3a, 0x8cf0b7f8, 0x9a602f3e, 0xb05a515b, 0xf4bdb164, 0x57f94aeb, 0x58558b23,
            0xf71e8185, 0x9dc466e4, 0x55ae607e, 0xaba144de, 0x20f77067, 0xde424f60, 0xdbd7755d, 0x48ec98ec,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        },
        {
            0x577d3031, 0xdbcce9e5, 0x875ef27b, 0x7df0d67b, 0x211c233a, 0x703b6f8d, 0x836cd31b, 0x6a6286a0,
            0x1c423e74, 0xea669363, 0x16362c50, 0x74492f0a, 0x4685ef82, 0xd4e75c4f, 0x1c5bfba3, 0x31258e1d,
            0xe036f43e, 0x3649bad3, 0x119b1eb2, 0x6a0a305a, 0xe7e3cab7, 0x5728be4f, 0xd15226ec, 0x1ebf9b71,
            0x421cf563, 0x475b473c, 0x865da3aa, 0xc87c6fa1, 0xbaa1487c, 0x477d7a8b, 0x4d4eb6dc, 0x48a56340,
            0x9a1816b2, 0x277b0285, 0x2de13925, 0x8a1505ba, 0x3a6cee1b, 0x3827315e, 0xb4e9a099, 0x0532efc1,
            0x068d503d, 0x7df1e853, 0x2b62c5f3, 0xbc6f3792, 0x188e8c5e, 0x28789515, 0x8bff63cd, 0xf7113b6b,
            0x6966556a, 0xf7b0d1bc, 0x833598e9, 0xdc91e53d, 0x37637356, 0xaf005d67, 0x8bfaebfa, 0x6f6cf626,
            0xe8f44bd2, 0x58a1dcbc, 0xd559ab97, 0x36f10b94, 0xe506f8a6, 0xb51b9a87, 0x5a41657e, 0xd5fceb04,
            0xe347a680, 0xfc189930, 0x77e01a6b, 0xeeee34ae, 0x16ac288f, 0x92eb930e, 0xa3b053a6, 0x0d1212eb,
            0xf4b1adaf, 0x3a69213d, 0x0cfebcdd, 0x99e75d41, 0xcc152f7a, 0x99ce7993, 0x93f2c8b9, 0xd2766c82,
            0x760aa309, 0xd56341bd, 0x14992f22, 0x86616b17, 0x87a9c2e1, 0xf2385b56, 0x4823d497, 0x7d2b347e,
            0x0e53457d, 0xf7795318, 0xdec3c062, 0x65283a27, 0x0b84c304, 0xfee01b13, 0x36bb50a7, 0x8062cdd9,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        }
    }
};

//
// END: Keep location fixed for HS uCode.
//
// PRD/DBG Signatures, copied and cleared before we enter HS uCode, hence don't need to be in .data_hs. These are
// inserted into the image post-compile.
//
LwU8 gSignatureLsDmemPrd[SCP_SIG_SIZE_IN_BYTES]
    GCC_ATTRIB_SECTION("data", "gSignatureLsDmemPrd")
    GCC_ATTRIB_ALIGN(SCP_SIG_SIZE_IN_BYTES);
LwU8 gSignatureLsImemPrd[SCP_SIG_SIZE_IN_BYTES]
    GCC_ATTRIB_SECTION("data", "gSignatureLsImemPrd")
    GCC_ATTRIB_ALIGN(SCP_SIG_SIZE_IN_BYTES);

LwU8 gSignatureLsDmemDbg[SCP_SIG_SIZE_IN_BYTES]
    GCC_ATTRIB_SECTION("data", "gSignatureLsDmemDbg")
    GCC_ATTRIB_ALIGN(SCP_SIG_SIZE_IN_BYTES);
LwU8 gSignatureLsImemDbg[SCP_SIG_SIZE_IN_BYTES]
    GCC_ATTRIB_SECTION("data", "gSignatureLsImemDbg")
    GCC_ATTRIB_ALIGN(SCP_SIG_SIZE_IN_BYTES);

/* ------------------------- Static Variables ------------------------------- */


/* ------------------------- Private Functions ------------------------------ */

void enterSelwreMode(void);
void evHandler(void);
void haltIntrUnmask(void);


/* ------------------------- Public Functions ------------------------------- */

/*!
 * Main entry-point for the RTOS application.
 * @return zero upon success; non-zero negative number upon failure
 */
GCC_ATTRIB_NO_STACK_PROTECT()
int
main(void)
{

    soeWriteStatusToFalconMailbox(SOE_BOOT_BINARY_STARTED_NOT_IN_HS_YET);

    //
    // Map the exception handler for NS (HS will install its own).
    // If HS sig authentication fails, this exception handler will catch it.
    //
    falc_wspr(EV, evHandler);

    //
    // Disable all Falcon interrupt sources.
    //
    REG_WR32_STALL(CSB, LW_CSOE_FALCON_IRQMCLR, 0xFFFFFFFF);

    //
    // Unmask the halt interrupt to host early to detect an init failure before
    // SOE Initialized interrupts
    //
    haltIntrUnmask();

    enterSelwreMode();

    OS_HALT();

    return -1;
}

/*!
 * @brief   returns the ucode HW revlock val from fpf block
 * @param   none
 * @return  returns the fpf revlock val
 *
 */
static inline
LwU32
getUcodeHWRevocationFpfVal(void)
{
    LwU32 revLockValFpf = 0;

    //
    // LW_FPF* defines have moved to LW_FUSE* defines in Ampere.
    // Bug 200435123
    //
    revLockValFpf = _soeBar0RegRd32_LR10(LW_FUSE_OPT_FPF_SEC2_UCODE1_VERSION);
    revLockValFpf = DRF_VAL(_FUSE_OPT_FPF, _SEC2_UCODE1_VERSION, _DATA, revLockValFpf);

    //
    // FPF rev bits are one-bit encoded so we need to get the highest set bit
    // getHighestSetBit returns bitpos starting with index 0 so increment by 1
    // if FPF rev fuse is non-zero.
    //
    if (revLockValFpf != 0)
    {
        HIGHESTBITIDX_32(revLockValFpf); // Macro modifies revLockValFpf
        revLockValFpf += 1;
    }

    return revLockValFpf;
}

/*!
 * The purpose of this function is to setup the SCP to verify HS Signature
 * and then call the function in secure overlay.
 */
GCC_ATTRIB_NO_STACK_PROTECT()
void
enterSelwreMode(void)
{
    LwU32 val32;

    val32 = CSB_REG_RD32(LW_CSOE_SCP_CTL_STAT);
    if (FLD_TEST_DRF(_CSOE_SCP, _CTL_STAT, _DEBUG_MODE, _DISABLED, val32))
    {
        memCopy(gSignatureLsDmem, gSignatureLsDmemPrd, SCP_SIG_SIZE_IN_BYTES);
        memCopy(gSignatureLsImem, gSignatureLsImemPrd, SCP_SIG_SIZE_IN_BYTES);
    }
    else
    {
        memCopy(gSignatureLsDmem, gSignatureLsDmemDbg, SCP_SIG_SIZE_IN_BYTES);
        memCopy(gSignatureLsImem, gSignatureLsImemDbg, SCP_SIG_SIZE_IN_BYTES);
    }

    // Clear the signatures in the image portion of DMEM.
    memClear(gSignatureLsDmemPrd, SCP_SIG_SIZE_IN_BYTES);
    memClear(gSignatureLsImemPrd, SCP_SIG_SIZE_IN_BYTES);
    memClear(gSignatureLsDmemDbg, SCP_SIG_SIZE_IN_BYTES);
    memClear(gSignatureLsImemDbg, SCP_SIG_SIZE_IN_BYTES);

    // Image is assumed to be PKC signed. If not We will fail the verification

    //soeBar0InitTmout_PlatformHS();
    _soeBar0InitTmout_LR10();

    // Check if we need to use SW Supplied Public Key. 
    val32 = _soeBar0RegRd32_LR10(LW_FUSE_OPT_PKC_PK_SELECTION);

    if (FLD_TEST_DRF(_FUSE_OPT, _PKC_PK_SELECTION, _DATA, _ENABLE, val32))
    {
        if (FLD_TEST_DRF(_CSOE_SCP, _CTL_STAT, _DEBUG_MODE, _DISABLED, CSB_REG_RD32(LW_CSOE_SCP_CTL_STAT)))
        {
            gHsSignatureVerificationParams.pk = &gPkcSWKey; 
        }
        else 
        {
            //
            // PKC SW Supplied Public key is not POR for Debug mode. 
            // IF we reach here, we have debug mode enabled, and PKC SW Supplied Public key 
            // enabled in Fuse. This is an invalid combination. 
            // So don't pass the params to boot-rom. This will fail during signature verfication.
            //
            gHsSignatureVerificationParams.pk = NULL;
        }        
    }
    else
    {
        gHsSignatureVerificationParams.pk = NULL;
    }

    gHsSignatureVerificationParams.hash_saving  = 0;

    // Copy the correct signature from the binary for verification by boot-rom. 
    LwU32 signatureIndex = 0;
    LwU32 hwRevocationFpfVal = getUcodeHWRevocationFpfVal();
    if (hwRevocationFpfVal <= SOE_HS_BOOT_VER)
    {
        //
        // Siggen stores signatures in the following order
        // When SOE_NUM_PREV_SIGNATURES is set. 
        //  sig_for_fuse = SOE_HS_BOOT_VER
        //  sig_for_fuse = SOE_HS_BOOT_VER - 1 
        //  sig_for_fuse = SOE_HS_BOOT_VER - 2
        //  sig_for_fuse = SOE_HS_BOOT_VER - 3
        //  ....
        //  sig_for_fuse = SOE_HS_BOOT_VER - (SOE_NUM_PREV_SIGNATURES - 1)
        //  sig_for_fuse = 0  (Signature for Fuse version 0 is always present for internal use)
        //
        if (hwRevocationFpfVal == 0)
        {
            signatureIndex = SOE_NUM_PREV_SIGNATURES;
        }
        else
        {
            signatureIndex = (SOE_HS_BOOT_VER - hwRevocationFpfVal);
        }

        // check for valid signature Index
        if ((signatureIndex >= 0) && 
            (signatureIndex < (SOE_NUM_PREV_SIGNATURES + 1)))
        {
            memCopy((LwU8 *)&gHsSignatureVerificationParams.signature,
                    gSignatureHs[signatureIndex], 
                    sizeof(gSignatureHs[signatureIndex]));
            soeWriteStatusToFalconMailbox((gSignatureHs[signatureIndex][0] << 8)+
                                          (gSignatureHs[signatureIndex][1] << 16)+
                                          (gSignatureHs[signatureIndex][2] << 24)+
                                          signatureIndex);
        }
        // Invalid signatureIndex will be handled by PKC Bootrom which will reject the ucode
    }
    else
    {
        //
        // Newer fuse, older ucode. This means this ucode is already revoked.
        // We don't have a signature that corresponds to this revision, so don't do anything
        // Boot-rom will reject this during signature validation.
        // 
    }
    
    CSB_REG_WR32(LW_CSOE_FALCON_BROM_PARAADDR(0), (LwU32)&gHsSignatureVerificationParams);

    // Enable authentication on SOE only
    CSB_REG_WR32(LW_CSOE_FALCON_BROM_ENGIDMASK, DRF_DEF(_CSOE_FALCON, _BROM_ENGIDMASK, _SOE, _ENABLED));

    // Enable authentication of ucode ID 1 only
    CSB_REG_WR32(LW_CSOE_FALCON_BROM_LWRR_UCODE_ID, DRF_NUM(_CSOE_FALCON, _BROM_LWRR_UCODE_ID, _VAL, SOE_HS_BOOT_ID));

    // Enable PKC, disable AES
    CSB_REG_WR32(LW_CSOE_FALCON_MOD_SEL, DRF_DEF(_CSOE_FALCON, _MOD_SEL, _ALGO, _RSA3K));

    // Write SEC SPR, providing location of secure region for bootrom.
    falc_wspr(SEC, SVEC_REG((LwU32)_imem_hsBase, (LwU32)_imem_hsSize, 0, HS_UCODE_ENCRYPTION));

    //
    // This takes us into secure overlay.
    // HW forces us to enter at start of secure region.
    // __imem_hsBase corresponds to function hsEntry().
    //
    // sample Assembly generated:
    //
    // SELWRE_CALL(_imem_hsBase);
    //     f0d1:   3e d8 f0 00           jmp 0xf0d8;
    //     f0d5:   00 00                 mvi a0 0x0;
    //     ...
    //
    //0000f0d8 <_selwre_call_0>:
    //     f0d8:   7e 00 f2 00           call 0xf200; <- __imem_hsBase 
    //     DUMMY_JMP();
    //     f0dc:   3e e0 f0 00           jmp 0xf0e0;
    //     f0e0:   3e e4 f0 00           jmp 0xf0e4;
    //
    //
    SELWRE_CALL(_imem_hsBase);
    DUMMY_JMP();
    
}

/*!
 * @brief Unmask the halt interrupt
 */
GCC_ATTRIB_NO_STACK_PROTECT()
void
haltIntrUnmask(void)
{
    // Clear the interrupt before unmasking it
    REG_FLD_WR_DRF_DEF_STALL(CSB, _CSOE, _FALCON_IRQSCLR, _HALT, _SET);

    // Now route it to host and unmask it
    REG_FLD_WR_DRF_DEF_STALL(CSB, _CSOE, _FALCON_IRQDEST, _HOST_HALT, _HOST);
    REG_FLD_WR_DRF_DEF_STALL(CSB, _CSOE, _FALCON_IRQMSET, _HALT, _SET);
}

GCC_ATTRIB_NO_STACK_PROTECT()
void
evHandler(void)
{
    falc_halt();
}
