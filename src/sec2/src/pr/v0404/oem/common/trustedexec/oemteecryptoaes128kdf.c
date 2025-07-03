/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <oemtee.h>
#include <oemteecrypto.h>
#include <oemteecryptointernaltypes.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST
** have a valid implementation for your PlayReady port if this function is still called
** by other OEM_TEE functions once your port is complete.
**
** Any reimplementation MUST exactly match the behavior of the default PK implementation.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** Refer to OEM_TEE_BASE_DeriveKey for algorithmic information.
**
** Operations Performed:
**
**  1. Use the given derivation key and given Key Derivation ID to derive an AES 128 Key
**     using the NIST algorithm as dislwssed in the documentation for OEM_TEE_BASE_DeriveKey.
**
** Arguments:
**
** f_pContextAllowNULL: (in/out) The TEE context returned from
**                               OEM_TEE_BASE_AllocTEEContext.
**                               This function may receive NULL.
**                               This function may receive an
**                               OEM_TEE_CONTEXT where
**                               cbUnprotectedOEMData == 0 and
**                               pbUnprotectedOEMData == NULL.
** f_pDeriverKey:           (in) The key to use for the key derivation.
** f_pidLabel:              (in) The input data label used to derive the derived key.
** f_pidContext:            (in) The input data context used to derive the derived key.
**                               If NULL, 16 bytes of 0x00 are used instead.
** f_pbDerivedKey:         (out) The derived AES128 key.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
#if DRM_DBG
static DRM_GLOBAL_CONST DRM_BYTE s_rgbZero[ DRM_AES_KEYSIZE_128 ] = { 0 };
#endif  /* DRM_DBG */
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_KDFCTR_r8_L128(
PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_PARAM_25004, "OEM_TEE_CONTEXT should never be const." )
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pDeriverKey,
    __in_ecount( 1 )                                const DRM_ID                       *f_pidLabel,
    __in_ecount_opt( 1 )                            const DRM_ID                       *f_pidContext,
    __out_ecount( DRM_AES_KEYSIZE_128 )                   DRM_BYTE                     *f_pbDerivedKey )
{
#if DRM_DBG
    {
        DRMASSERT( !OEM_SELWRE_ARE_EQUAL( OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_BYTES( f_pDeriverKey ), s_rgbZero, DRM_AES_KEYSIZE_128 ) );
    }
#endif  /* DRM_DBG */

    return Oem_Aes_AES128KDFCTR_r8_L128( OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_BROKER( f_pDeriverKey ), f_pidLabel, f_pidContext, f_pbDerivedKey );
}
#endif
EXIT_PK_NAMESPACE_CODE;

