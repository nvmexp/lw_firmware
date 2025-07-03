/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWOS_SELWRE_OVLY_H
#define LWOS_SELWRE_OVLY_H

/*!
 * @file    lwosselwreovly.h
 */

/* ------------------------ System includes --------------------------------- */
/* ------------------------ Application includes ---------------------------- */
#include "scp_internals.h"

#if SEC2_CSB_ACCESS
#include "dev_sec_csb.h"
#include "common_hslib.h"
#elif GSPLITE_CSB_ACCESS
#include "dev_gsp_csb.h"
#include "common_hslib.h"
#else
#include "dev_pwr_falcon_csb.h"
#endif

#ifdef IS_PKC_ENABLED
#include "bootrom_pkc_parameters.h"
#include "dev_fuse.h"
#include "lwosreg.h"
#endif

/* ------------------------ Defines ----------------------------------------- */

//
// TODO: dmiyani: remove this define and update all other RTOS based ucodes
// If RTOS ucodes are not updated to use this define, specify the default
//
#ifndef SIG_SIZE_IN_BITS
 #define SIG_SIZE_IN_BITS   128
#endif

ct_assert(LW_TRUE  == 1U);
ct_assert(LW_FALSE == 0U);
#define HASH_SAVING_ENABLE  LW_TRUE
#define HASH_SAVING_DISABLE LW_FALSE

/*!
 * A secure overlay that is loaded in IMEM isn't validated until we load the
 * signature of the overlay and jump to the 0th offset of the overlay. Once we
 * do that, HW verifies the signature, and then marks the block as valid.
 * Once the block is validated, we can execute code at arbitrary non-zero
 * offsets within the overlay.
 * Check if the overlay is valid, the overlay is indeed an HS library, and the
 * falcon is in HS mode already.
 */
#define OS_SEC_HS_LIB_VALIDATE(overlayName, bHashSaving)                        \
        do {                                                                    \
            LwU8 _ovlIdx = OVL_INDEX_IMEM(overlayName);                         \
            if ((_ovlIdx == OVL_INDEX_ILWALID) ||                               \
                !OVL_IMEM_IS_HS_LIB(overlayName) || !OS_SEC_FALC_IS_HS_MODE())  \
            {                                                                   \
                OS_HALT();                                                      \
            }                                                                   \
            osSecHSLoadSignature(_ovlIdx, bHashSaving);                         \
            osSecHSJumpOverlayStart(_ovlIdx);                                   \
        } while (LW_FALSE)

/*!
 * Get the signature for the HS or HS library overlay.
 * Check that the ovlIdx is in the correct range for an HS overlay before
 * using this macro.
 * NOTE: The signatures are emitted by the gt_sections.ld template.
 * Any change in signature size needs a corresponding change in that file.
 */
#ifdef BOOT_FROM_HS                                                       
#define OS_SEC_HS_SIGNATURE(ovlIdx)                                            \
            (&_imem_hs_signatures[(ovlIdx - 1) * (SIG_SIZE_IN_BITS/32)])    
#else                                                                          
#define OS_SEC_HS_SIGNATURE(ovlIdx)                                            \
        (OS_SEC_FALC_IS_DBG_MODE() ?                                           \
            (&_imem_hs_signatures_dbg[(ovlIdx - 1) * (SIG_SIZE_IN_BITS/32)]) : \
            (&_imem_hs_signatures_prd[(ovlIdx - 1) * (SIG_SIZE_IN_BITS/32)]))
#endif

/*!
 *  Similar to OS_SEC_HS_SIGNATURE, except that there are multiple HS signatures
 *  per HS overlay, depending on the fuse ver
 */
#ifdef RUNTIME_HS_OVL_SIG_PATCHING
#define OS_SEC_HS_SIGNATURE_PKC_RUNTIME_SIG_PATCHING(ovlIdx)                                       \
         (_imem_hs_signatures[(ovlIdx - 1)])
#else
#ifdef BOOT_FROM_HS                                                       
#define OS_SEC_HS_SIGNATURE_PKC(ovlIdx, _offsetIdx)                            \
            (_imem_hs_signatures[(ovlIdx - 1)][(_offsetIdx)])
#else                                                                          
#define OS_SEC_HS_SIGNATURE_PKC(ovlIdx, _offsetIdx)                            \
        (OS_SEC_FALC_IS_DBG_MODE() ?                                           \
            (_imem_hs_signatures_dbg[(ovlIdx - 1)][(_offsetIdx)]) :            \
            (_imem_hs_signatures_prd[(ovlIdx - 1)][(_offsetIdx)]))
#endif // BOOT_FROM_HS
#endif // RUNTIME_HS_OVL_SIG_PATCHING

#define SVEC_REG(baseBlk, sizeBlkCnt, secure, encrypted)                       \
        ((baseBlk) | (sizeBlkCnt << 24) | (secure << 16) | (encrypted << 17))

/*!
 * Find out if the falcon is lwrrently exelwting in HS mode
 */
#define OS_SEC_FALC_IS_HS_MODE()                                               \
        (REF_VAL(CSB_FIELD(_SCTL_HSMODE), REG_RD32(CSB, CSB_REG(_SCTL))) ==    \
         CSB_FIELD(_SCTL_HSMODE_TRUE))

/*!
 * Find out if the falcon is in debug mode
 */
#define OS_SEC_FALC_IS_DBG_MODE()                                              \
        (FLD_TEST_DRF_SCP(_CTL_STAT, _DEBUG_MODE, _DISABLED,                   \
                          SCP_REG_RD32(_CTL_STAT)) ?                           \
             LW_FALSE : LW_TRUE)

/*!
 * Checks if the IMEM overlay name specified is an HS library.
 */
#define OVL_IMEM_IS_HS_LIB(overlayName)                                        \
        OS_LABEL(_overlay_imem_##overlayName##_is_hs_lib)

/*!
 * A wrapper to osHSModeSetup with bInTask as LW_TRUE.
 * Using this macro for calling osHSModeSetup from a task context.
 */
#define OS_HS_MODE_SETUP_IN_TASK(ovlIdx, pOvlList, ovlCnt, bHashSaving)         \
        osHSModeSetup(ovlIdx, LW_TRUE, pOvlList, ovlCnt, bHashSaving)

/*!
 * A wrapper to osHSModeCleanup with bInTask as LW_TRUE.
 * Using this macro for calling osHSModeCleanup from a task context.
 */
#define OS_HS_MODE_CLEANUP_IN_TASK(pOvlList, ovlCnt)                           \
        osHSModeCleanup(LW_TRUE, pOvlList, ovlCnt)

#ifdef IS_PKC_ENABLED
// Write to appropriate register based on falcon
#if SEC2_RTOS
    #define FALCON_TYPE   _CSEC
#elif PMU_RTOS
    #define FALCON_TYPE   _CPWR
#elif GSPLITE_RTOS
    #define FALCON_TYPE   _CGSP
#else
    #error !!! Unsupported falcon !!!
#endif

#define _EXPAND_FALCON_REG(falconType, regName) (LW##falconType##regName)
#define EXPAND_FALCON_REG(falconType, regName)  _EXPAND_FALCON_REG(falconType, regName)
#define FALCON_REG(regName)                     EXPAND_FALCON_REG(FALCON_TYPE, regName)
#define FALCON_DRF_NUM(d,r,f,n)                 DRF_NUM(d,r,f,n)

    struct pkc_verification_parameters pkcSignatureParams;
#endif //IS_PKC_ENABLED

/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
extern LwU16 _imem_ovl_tag[];
extern LwU8  _imem_ovl_size[];

//
// TODO: Signatures and ucode_ids do not have to be in resident
//   Move outside of it.
#ifndef IS_PKC_ENABLED

#ifdef BOOT_FROM_HS   
    extern LwU32 _imem_hs_signatures[];
#else
    extern LwU32 _imem_hs_signatures_dbg[];
    extern LwU32 _imem_hs_signatures_prd[];
#endif

#else // IS_PKC_ENABLED
#ifdef RUNTIME_HS_OVL_SIG_PATCHING
extern LwU32 _imem_hs_signatures[][SIG_SIZE_IN_BITS/32];
#else
#ifdef BOOT_FROM_HS   
extern LwU32 _imem_hs_signatures[][NUM_SIG_PER_UCODE][SIG_SIZE_IN_BITS / 32];
#else
extern LwU32 _imem_hs_signatures_dbg[][NUM_SIG_PER_UCODE][SIG_SIZE_IN_BITS/32];
extern LwU32 _imem_hs_signatures_prd[][NUM_SIG_PER_UCODE][SIG_SIZE_IN_BITS/32];
#endif // BOOT_FROM_HS
#endif // RUNTIME_HS_OVL_SIG_PATCHING

extern LwU8  _imem_hs_sig_ucode_id[];
extern LwU8  _imem_hs_sig_ucode_fuse_ver[];
#endif

/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
/*!
 * Note that this function must be inlined. It is intended to be used when in
 * HS mode. If this function is not inlined, it could be placed in a non-secure
 * (NS) or a light secure (LS) overlay. Once we jump out of HS code to NS/LS
 * code, the HS blocks will be ilwalidated by hardware (valid = 0). Declaring a
 * function to be inline allows the compiler to inline the function if
 * optimization is specified. We add an additional attribute to inline the
 * function even if no optimization is specified to make sure that it will be
 * inlined.
 */
static inline void osSecHSJumpOverlayStart(LwU8 ovlIdx)
    GCC_ATTRIB_ALWAYSINLINE();

/*!
 * Note that this function must be inlined. It is intended to be used when in
 * HS mode. If this function is not inlined, it could be placed in a non-secure
 * (NS) or a light secure (LS) overlay. Once we jump out of HS code to NS/LS
 * code, the HS blocks will be ilwalidated by hardware (valid = 0). Declaring a
 * function to be inline allows the compiler to inline the function if
 * optimization is specified. We add an additional attribute to inline the
 * function even if no optimization is specified to make sure that it will be
 * inlined.
 */
static inline void osSecHSLoadSignature(LwU8 ovlIdx, LwBool bHashSaving)
    GCC_ATTRIB_ALWAYSINLINE();

#ifdef IS_PKC_ENABLED
static inline LwU32 osHWFpfVersion(LwU32 ucodeId) 
    GCC_ATTRIB_ALWAYSINLINE();
#endif

void osHSModeSetup(LwU8 ovlIdx, LwBool bInTask, LwU8 *pOvlList, LwU8 ovlCnt, LwBool bHashSaving)
    GCC_ATTRIB_SECTION("imem_resident", "osHSModeSetup");
void osHSModeCleanup(LwBool bInTask, LwU8 *pOvlList, LwU8 ovlCnt)
    GCC_ATTRIB_SECTION("imem_resident", "osHSModeCleanup");

/* ------------------------ Inline Function Definitions --------------------- */
/*!
 * Jump to the 0th offset of the specified overlay. Required for HS overlays
 * in order to mark the overlay blocks as valid when the appropriate signatures
 * are loaded for HW to verify.
 *
 * @param   ovlIdx Overlay index whose start offset to jump to
 */
static inline void osSecHSJumpOverlayStart
(
    LwU8 ovlIdx
)
{
    LwU16 tag   = _imem_ovl_tag[ovlIdx];
    LwU32 vAddr = IMEM_IDX_TO_ADDR(tag);

    preHsLibEntryPointJump();

    //
    // Cast the starting virtual address of this overlay to a function pointer
    // and call it.
    //
    void (*func)(void) = (void (*)(void))vAddr;
    (*func)();


    postHsLibEntryPointJump();
}

#ifdef IS_PKC_ENABLED
/*
 * @brief Get the HW fpf version
 */
static inline LwU32 osHWFpfVersion
(
    LwU32 ucodeId
)
{
    LwU32 fpfFuse = 0;
    #if SEC2_RTOS
        fpfFuse= falc_ldxb_i ((LwU32*)(LW_CSEC_FALCON_UCODE_VERSION(ucodeId - 1)), 0);
        fpfFuse = DRF_VAL( _CSEC, _FALCON_UCODE_VERSION, _VER, fpfFuse);
    #elif GSPLITE_RTOS
        fpfFuse= falc_ldxb_i ((LwU32*)(LW_CGSP_FALCON_UCODE_VERSION(ucodeId - 1)), 0);
        fpfFuse = DRF_VAL( _CGSP, _FALCON_UCODE_VERSION, _VER, fpfFuse);
    #else
        fpfFuse = 0;
    #endif

    return fpfFuse;
}
#endif

/*!
 * Load the signature of the overlay (HS library) requested.
 *
 * @param   ovlIdx Overlay index whose signature to load
 * @param   bHashSaving toggle to turn overlay hash saving on/off
 */
static inline void osSecHSLoadSignature
(
    LwU8    ovlIdx,
    LwBool  bHashSaving
)
{
    LwU32 *pSig = NULL;

    // Check that bHashSaving has a valid value
    if ((bHashSaving != HASH_SAVING_ENABLE) && (bHashSaving != HASH_SAVING_DISABLE))
    {
        // Note: To avoid halting / throwing an error here, we will continue with
        // hash saving disabled if the value passed to the function doesn't make sense
        bHashSaving = HASH_SAVING_DISABLE;
    }

#ifdef IS_PKC_ENABLED
    LwU32 i;
    LwU32 signatureIndex = 0;
    LwU32 hwRevocationFpfVal = osHWFpfVersion(_imem_hs_sig_ucode_id[ovlIdx]);

   // Copy the correct signature from the binary for verification by BROM. 
    if (hwRevocationFpfVal <= _imem_hs_sig_ucode_fuse_ver[ovlIdx])
    {
        //
        // Siggen stores signatures in the following order
        // When NUM_SIG_PER_UCODE is set. 
        //  sig_for_fuse = NUM_SIG_PER_UCODE
        //  sig_for_fuse = NUM_SIG_PER_UCODE - 1 
        //  sig_for_fuse = NUM_SIG_PER_UCODE - 2
        //  sig_for_fuse = NUM_SIG_PER_UCODE - 3
        //  ....
        //  sig_for_fuse = NUM_SIG_PER_UCODE - (NUM_SIG_PER_UCODE - 1)
        //  sig_for_fuse = 0  (If NUM_SIG_PER_UCODE > 1 then Signature for Fuse version 0 is always present for internal use)
        //
        if (hwRevocationFpfVal == 0)
        {
            signatureIndex = NUM_SIG_PER_UCODE - 1;
        }
        else
        {
            signatureIndex = (NUM_SIG_PER_UCODE - hwRevocationFpfVal - 1);
        }
        // Invalid signatureIndex will be handled by PKC Bootrom which will reject the ucode
    }
    else
    {
        //
        // Newer fuse, older ucode. This means this ucode is already revoked.
        // We don't have a signature that corresponds to this revision, so don't do anything
        // BROM will reject this during signature validation.
        // 
    }

#ifdef RUNTIME_HS_OVL_SIG_PATCHING
    pSig = OS_SEC_HS_SIGNATURE_PKC_RUNTIME_SIG_PATCHING(ovlIdx);
#else
    pSig = OS_SEC_HS_SIGNATURE_PKC(ovlIdx, signatureIndex);
#endif // RUNTIME_HS_OVL_SIG_PATCHING

    for(i = 0; i < SIG_SIZE_IN_BITS/32; i++)
    {
        pkcSignatureParams.signature.rsa3k_sig.S[i] = pSig[i];
    }
    pkcSignatureParams.pk          = NULL;
    pkcSignatureParams.hash_saving = bHashSaving;

    falc_stxb_i((LwU32*)FALCON_REG(_FALCON_BROM_PARAADDR(0)),   0, (LwU32)&pkcSignatureParams);
    falc_stxb_i((LwU32*)FALCON_REG(_FALCON_BROM_ENGIDMASK),     0, ENGINE_ID);

    falc_stxb_i((LwU32*)FALCON_REG(_FALCON_BROM_LWRR_UCODE_ID), 0,
       FALCON_DRF_NUM(FALCON_TYPE, _FALCON_BROM_LWRR_UCODE_ID, _VAL, _imem_hs_sig_ucode_id[ovlIdx]));

    falc_stxb_i((LwU32*)FALCON_REG(_FALCON_MOD_SEL),            0,
       FALCON_DRF_NUM(FALCON_TYPE, _FALCON_MOD_SEL, _ALGO, FALCON_REG(_FALCON_MOD_SEL_ALGO_RSA3K)));
#else
    //
    // AES Legacy signature verification
    //

    pSig = OS_SEC_HS_SIGNATURE(ovlIdx);
    asm ("cci 0x2;");
    falc_dmwrite(0, ((6 << 16) | (LwU32)(pSig)));
    falc_dmwait();
#endif // IS_PKC_ENABLED

#ifdef HS_UCODE_ENCRYPTION
    falc_wspr(SEC, SVEC_REG((LwU32)(_imem_ovl_tag[ovlIdx]),
                            (LwU32)(_imem_ovl_size[ovlIdx]),
                            0, 1));
#else
    falc_wspr(SEC, SVEC_REG((LwU32)(_imem_ovl_tag[ovlIdx]),
                            (LwU32)(_imem_ovl_size[ovlIdx]),
                            0, 0));
#endif
}

#endif // LWOS_SELWRE_OVLY_H
