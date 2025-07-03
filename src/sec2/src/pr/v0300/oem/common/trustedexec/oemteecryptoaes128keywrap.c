/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <oemteecrypto.h>
#include <oemteecryptointernaltypes.h>
#include <oemaeskeywrap.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port if this function is still called by other
** OEM_TEE functions once your port is complete.
**
** Any reimplementation MUST exactly match the behavior of the default PK implementation.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** Refer to OEM_TEE_BASE_WrapAES128KeyForPersistedStorage for algorithmic information.
**
** Operations Performed:
**
**  1. Wrap the given plaintext AES key and 8 bytes of entropy using the given AES wrapping key
**     using the AES Key Wrap algorithm and return the wrapped key.
**
** Arguments:
**
** f_pContext:              (in/out) The TEE context returned from
**                                   OEM_TEE_BASE_AllocTEEContext.
** f_pKey:                      (in) The key with which to wrap f_pPlaintext.
** f_pPlaintext:                (in) The key to be wrapped.
** f_pCiphertext:              (out) The wrapped key.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AESKEYWRAP_WrapKeyAES128(
    __inout                                               OEM_TEE_CONTEXT              *f_pContext,
    __in                                            const OEM_TEE_KEY                  *f_pKey,
    __in_ecount( 1 )                                const OEM_UNWRAPPED_KEY_AES_128    *f_pPlaintext,
    __out_ecount( 1 )                                     OEM_WRAPPED_KEY_AES_128      *f_pCiphertext )
{
    return Oem_AesKeyWrap_WrapKeyAES128(
        OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_BROKER( f_pKey ),
        f_pPlaintext,
        f_pCiphertext );
}
#endif
/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port if this function is still called by other
** OEM_TEE functions once your port is complete.
**
** Any reimplementation MUST exactly match the behavior of the default PK implementation.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** Refer to OEM_TEE_BASE_UnwrapAES128KeyFromPersistedStorage for algorithmic information.
**
** Operations Performed:
**
**  1. Unwrap the given ciphertext AES key using the given AES wrapping key
**     using the AES Key Wrap algorithm and return the unwrapped key.
**
** Arguments:
**
** f_pContextAllowNULL:     (in/out) The TEE context returned from
**                                   OEM_TEE_BASE_AllocTEEContext.
**                                   This function may receive NULL.
**                                   This function may receive an
**                                   OEM_TEE_CONTEXT where
**                                   cbUnprotectedOEMData == 0 and
**                                   pbUnprotectedOEMData == NULL.
** f_pKey:                      (in) The key with which to unwrap f_pCiphertext.
** f_pCiphertext:               (in) The key to unwrap.
** f_pPlaintext:               (out) The unwrapped key.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AESKEYWRAP_UnwrapKeyAES128(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __in                                            const OEM_TEE_KEY                  *f_pKey,
    __in_ecount( 1 )                                const OEM_WRAPPED_KEY_AES_128      *f_pCiphertext,
    __out_ecount( 1 )                                     OEM_UNWRAPPED_KEY_AES_128    *f_pPlaintext )
{
    return Oem_AesKeyWrap_UnwrapKeyAES128( OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_BROKER( f_pKey ), f_pCiphertext, f_pPlaintext );
}
#endif

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port if this function is still called by other
** OEM_TEE functions once your port is complete.
**
** Any reimplementation MUST exactly match the behavior of the default PK implementation.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** Refer to OEM_TEE_BASE_WrapECC256KeyForPersistedStorage for algorithmic information.
**
** Operations Performed:
**
**  1. Wrap the given plaintext ECC key and metadata using the given AES wrapping key
**     using the AES Key Wrap algorithm and return the wrapped key.
**
** Arguments:
**
** f_pContext:              (in/out) The TEE context returned from
**                                   OEM_TEE_BASE_AllocTEEContext.
** f_pKey:                      (in) The key with which to wrap f_pUnWrappedKey.
** f_pUnWrappedKey:             (in) The key to be wrapped.
** f_pCiphertext:              (out) The wrapped key.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AESKEYWRAP_WrapKeyECC256(
    __inout                                               OEM_TEE_CONTEXT              *f_pContext,
    __in                                            const OEM_TEE_KEY                  *f_pKey,
    __in_ecount( 1 )                                const OEM_UNWRAPPED_KEY_ECC_256    *f_pUnwrappedKey,
    __out_ecount( 1 )                                     OEM_WRAPPED_KEY_ECC_256      *f_pCiphertext )
{
    return Oem_AesKeyWrap_WrapKeyECC256(
        OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_BROKER( f_pKey ),
        f_pUnwrappedKey,
        f_pCiphertext );
}
#endif
/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port if this function is still called by other
** OEM_TEE functions once your port is complete.
**
** Any reimplementation MUST exactly match the behavior of the default PK implementation.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** Refer to OEM_TEE_BASE_UnwrapECC256KeyFromPersistedStorage for algorithmic information.
**
** Operations Performed:
**
**  1. Unwrap the given ciphertext ECC key using the given AES wrapping key
**     using the AES Key Wrap algorithm and return the unwrapped key.
**
** Arguments:
**
** f_pContext:              (in/out) The TEE context returned from
**                                   OEM_TEE_BASE_AllocTEEContext.
** f_pKey:                      (in) The key with which to unwrap f_pCiphertext.
** f_pCiphertext:               (in) The key to unwrap.
** f_pPlaintext:               (out) The unwrapped key.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AESKEYWRAP_UnwrapKeyECC256(
    __inout                                               OEM_TEE_CONTEXT              *f_pContext,
    __in                                            const OEM_TEE_KEY                  *f_pKey,
    __in_ecount( 1 )                                const OEM_WRAPPED_KEY_ECC_256      *f_pCiphertext,
    __out_ecount( 1 )                                     OEM_UNWRAPPED_KEY_ECC_256    *f_pPlaintext )
{
    return Oem_AesKeyWrap_UnwrapKeyECC256(
        OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_BROKER( f_pKey ),
        f_pCiphertext,
        f_pPlaintext );
}
#endif
EXIT_PK_NAMESPACE_CODE;

