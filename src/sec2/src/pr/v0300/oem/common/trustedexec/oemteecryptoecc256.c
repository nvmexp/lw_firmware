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

typedef struct __tagDRM_CRYPTO_CONTEXT_FOR_ECC_ONLY
{
    DRM_BYTE rgbOpaque[
#if DRM_SUPPORT_PRECOMPUTE_GTABLE
        8182
#else /* DRM_SUPPORT_PRECOMPUTE_GTABLE */
    #if DRM_64BIT_TARGET == 1
        2100
    #else /* DRM_64BIT_TARGET == 1*/
        /*
        ** can map point needs a bit less than 1800
        ** sign and verify ECDSA are ok with around 1300
        ** it is expected as can map point callwlates square root,
        ** while the others do not.
        */
        1800
    #endif /* DRM_64BIT_TARGET == 1*/

#endif /* DRM_SUPPORT_PRECOMPUTE_GTABLE */
     ];
} DRM_CRYPTO_CONTEXT_FOR_ECC_ONLY;
#if defined(SEC_COMPILE)
GCC_ATTRIB_NO_STACK_PROTECT()
static DRM_RESULT DRM_CALL _AllocCryptoContext(
    __inout                                               OEM_TEE_CONTEXT                  *f_pContext,
    __inout                                               DRM_CRYPTO_CONTEXT_FOR_ECC_ONLY **f_ppCryptoCtx ) /*should not be that. */
{
    DRM_RESULT                       dr         = DRM_SUCCESS;
    DRM_CRYPTO_CONTEXT_FOR_ECC_ONLY *pCryptoCtx = NULL;

    DRMASSERT( f_ppCryptoCtx != NULL );

    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( f_pContext, sizeof(*pCryptoCtx), (DRM_VOID**)&pCryptoCtx ) );

    DRMCASSERT( sizeof(DRM_CRYPTO_CONTEXT_FOR_ECC_ONLY) > sizeof(DRMBIGNUM_CONTEXT_STRUCT) );
    ((DRMBIGNUM_CONTEXT_STRUCT *)pCryptoCtx->rgbOpaque)->oContext.pOEMTEEContext = f_pContext;

    *f_ppCryptoCtx = pCryptoCtx;
    pCryptoCtx = NULL;

ErrorExit:
    return dr;
}

static  DRM_VOID DRM_CALL _FreeCryptoContext(
    __inout_opt                                           OEM_TEE_CONTEXT                   *f_pContextAllowNULL,
    __inout                                               DRM_CRYPTO_CONTEXT_FOR_ECC_ONLY  **f_ppCryptoCtx )
{
    DRMASSERT(  f_ppCryptoCtx != NULL );

    ChkVOID( OEM_TEE_BASE_SelwreMemFree( f_pContextAllowNULL, (DRM_VOID**)f_ppCryptoCtx ) );
}
#endif
/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST
** have a valid implementation for your PlayReady port if this function is still called
** by other OEM_TEE functions once your port is complete.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function allocates an uninitialized ECC 256 Key.
**
** Operations Performed:
**
**  1. Allocate an uninitialized ECC 256 Key.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pKey:                  (out) The allocated key.
**                                The caller will free this key using
**                                OEM_TEE_CRYPTO_ECC256_FreeKey.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_ECC256_AllocKey(
    __inout                                               OEM_TEE_CONTEXT              *f_pContext,
    __out                                                 OEM_TEE_KEY                  *f_pKey )
{
    DRMASSERT( f_pKey != NULL );
    return OEM_TEE_BASE_SelwreMemAlloc( f_pContext, sizeof(OEM_TEE_KEY_ECC), (DRM_VOID**)&f_pKey->pKey ); /* Do not zero-initialize key material */
}
#endif

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST
** have a valid implementation for your PlayReady port if this function is still called
** by other OEM_TEE functions once your port is complete.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function frees a key allocated by OEM_TEE_CRYPTO_ECC256_AllocKey.
**
** Operations Performed:
**
**  1. Zero and free the given key.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pKey:               (in/out) The key to be freed.
*/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL OEM_TEE_CRYPTO_ECC256_FreeKey(
    __inout                                               OEM_TEE_CONTEXT              *f_pContext,
    __inout                                               OEM_TEE_KEY                  *f_pKey )
{
    DRMASSERT( f_pKey != NULL );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( f_pContext, &f_pKey->pKey ) );
    ChkVOID( OEM_SELWRE_ZERO_MEMORY( f_pKey, sizeof(*f_pKey) ) );
}
#endif

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST
** have a valid implementation for your PlayReady port if this function is still called
** by other OEM_TEE functions once your port is complete.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function initializes a key allocated by OEM_TEE_CRYPTO_ECC256_AllocKey with the given
** clear ECC256 key.
**
** Operations Performed:
**
**  1. Initializes the given key with the given value.
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
** f_pKey:               (in/out) The key to be initialized.
** f_pbKey:                  (in) The value of the clear key to use.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_ECC256_SetKey(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __inout_ecount( 1 )                                   OEM_TEE_KEY                  *f_pKey,
    __in_bcount( ECC_P256_INTEGER_SIZE_IN_BYTES )   const DRM_BYTE                     *f_pbKey )
{
    DRM_RESULT dr = DRM_SUCCESS;

    AssertChkArg( f_pKey  != NULL );
    AssertChkArg( f_pbKey != NULL );

    OEM_TEE_MEMCPY( OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_ECC256_TO_PRIVKEY_P256( f_pKey ), f_pbKey, ECC_P256_INTEGER_SIZE_IN_BYTES );

ErrorExit:
    return dr;
}
#endif
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
** This function creates an ECDSA P256 signature for a hash of a message using a given private key.
**
** Operations Performed:
**
**  1. Create the ECDSA P256 signature over the given hash.
**
** Arguments:
**
** f_pContextAllowNULL:  (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
**                                This function may receive NULL.
**                                This function may receive an
**                                OEM_TEE_CONTEXT where
**                                cbUnprotectedOEMData == 0 and
**                                pbUnprotectedOEMData == NULL.
** f_rgbMessage:             (in) The hash to be signed.
** f_pPrivkey:               (in) The key to use.
** f_pSignature:            (out) The resulting signature.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_ECC256_ECDSA_SignHash(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __in_ecount( ECC_P256_INTEGER_SIZE_IN_BYTES )   const DRM_BYTE                      f_rgbMessage[ECC_P256_INTEGER_SIZE_IN_BYTES],
    __in                                            const OEM_TEE_KEY                  *f_pPrivkey,
    __out                                                 SIGNATURE_P256               *f_pSignature )
{
    DRM_RESULT                          dr         = DRM_SUCCESS;
    DRM_CRYPTO_CONTEXT_FOR_ECC_ONLY    *pCryptoCtx = NULL;

    ChkDR( _AllocCryptoContext(
        f_pContextAllowNULL,
        &pCryptoCtx ) );

    ChkDR( OEM_ECDSA_SignHash_P256(
        f_rgbMessage,
        ECC_P256_INTEGER_SIZE_IN_BYTES,
        OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_ECC256_TO_PRIVKEY_P256( f_pPrivkey ),
        f_pSignature,
        (struct bigctx_t*)pCryptoCtx,
        sizeof(*pCryptoCtx) ) );

ErrorExit:
    ChkVOID( _FreeCryptoContext( f_pContextAllowNULL, &pCryptoCtx ) );
    return dr;
}
#endif

#if defined (SEC_COMPILE)
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
** This function creates an ECDSA P256 signature for an arbitrarily-sized message using a given
** private key and using SHA256 as the hashing algorithm.
**
** Operations Performed:
**
**  1. Create the ECDSA P256 signature over the given message.
**
** Arguments:
**
** f_pContextAllowNULL:  (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
**                                This function may receive NULL.
**                                This function may receive an
**                                OEM_TEE_CONTEXT where
**                                cbUnprotectedOEMData == 0 and
**                                pbUnprotectedOEMData == NULL.
** f_rgbMessage:             (in) The message to be signed.
** f_cbMessage:              (in) The size of the message to be signed.
** f_pECCSigningKey:         (in) The private ECC signing key to use.
** f_pSignature:            (out) The resulting signature.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_ECC256_ECDSA_SHA256_Sign(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __in_ecount( f_cbMessage )                      const DRM_BYTE                      f_rgbMessage[],
    __in                                            const DRM_DWORD                     f_cbMessage,
    __in                                            const OEM_TEE_KEY                  *f_pECCSigningKey,
    __out                                                 SIGNATURE_P256               *f_pSignature )
{
    DRM_RESULT           dr         = DRM_SUCCESS;
    DRM_SHA256_Digest  shaDigest;
    DRM_SHA256_CONTEXT shaData;

    ChkArg( NULL !=  f_rgbMessage );
    ChkArg( 0    !=  f_cbMessage );

    /*
    ** Hash the message
    */
    ChkDR( OEM_TEE_CRYPTO_SHA256_Init( f_pContextAllowNULL, &shaData ) );
    ChkDR( OEM_TEE_CRYPTO_SHA256_Update( f_pContextAllowNULL, &shaData, f_rgbMessage, f_cbMessage ) );
    ChkDR( OEM_TEE_CRYPTO_SHA256_Finalize( f_pContextAllowNULL, &shaData, &shaDigest ) );

    DRMCASSERT( sizeof(shaDigest) == ECC_P256_INTEGER_SIZE_IN_BYTES );

    /*
    ** Sign the Hash
    */
    ChkDR( OEM_TEE_CRYPTO_ECC256_ECDSA_SignHash(
        f_pContextAllowNULL,
        (const DRM_BYTE*)&shaDigest,
        f_pECCSigningKey,
        f_pSignature ) );

ErrorExit:
    return dr;
}
#endif

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
** This function verifies an ECDSA P256 signature for an arbitrarily-sized message using a given
** public key and using SHA256 as the hashing algorithm.
**
** Operations Performed:
**
**  1. Verify the ECDSA P256 signature over the given message.
**
** Arguments:
**
** f_pContextAllowNULL:  (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
**                                This function may receive NULL.
**                                This function may receive an
**                                OEM_TEE_CONTEXT where
**                                cbUnprotectedOEMData == 0 and
**                                pbUnprotectedOEMData == NULL.
** f_rgbMessage:             (in) The message to be verified.
** f_cbMessage:              (in) The size of the message to be verified.
** f_pPubkey:                (in) The public ECC signing key to use.
** f_pSignature:             (in) The signature to verify.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_ECC256_ECDSA_SHA256_Verify(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __in_ecount( f_cbMessage )                      const DRM_BYTE                      f_rgbMessage[],
    __in                                            const DRM_DWORD                     f_cbMessage,
    __in                                            const PUBKEY_P256                  *f_pPubkey,
    __in                                            const SIGNATURE_P256               *f_pSignature )
{
    DRM_RESULT                          dr         = DRM_SUCCESS;
    DRM_CRYPTO_CONTEXT_FOR_ECC_ONLY    *pCryptoCtx = NULL;

    ChkDR( _AllocCryptoContext(
        f_pContextAllowNULL,
        &pCryptoCtx ) );

    ChkDR( OEM_ECDSA_Verify_P256(
         f_rgbMessage,
         f_cbMessage,
         f_pPubkey,
         f_pSignature,
         (struct bigctx_t*)pCryptoCtx,
         sizeof(*pCryptoCtx) ) );

ErrorExit:
    ChkVOID( _FreeCryptoContext( f_pContextAllowNULL, &pCryptoCtx ) );
    return dr;
}
#endif

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
** This function encrypts data using ECC256.
**
** Operations Performed:
**
**  1. Encrypt the given data with the given public key and return the encrypted result.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pPubkey:                (in) The public ECC encryption key to use.
** f_pPlaintext:             (in) The message to be encrypted.
** f_pCiphertext:           (out) The encrypted version of the message.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_ECC256_Encrypt(
    __inout                                               OEM_TEE_CONTEXT              *f_pContext,
    __in                                            const PUBKEY_P256                  *f_pPubkey,
    __in                                            const PLAINTEXT_P256               *f_pPlaintext,
    __out                                                 CIPHERTEXT_P256              *f_pCiphertext )
{
    DRM_RESULT                        dr         = DRM_SUCCESS;
    DRM_CRYPTO_CONTEXT_FOR_ECC_ONLY  *pCryptoCtx = NULL;

    ChkDR( _AllocCryptoContext(
        f_pContext,
        &pCryptoCtx ) );

    ChkDR( OEM_ECC_Encrypt_P256(
        f_pPubkey,
        f_pPlaintext,
        f_pCiphertext,
        (struct bigctx_t*)pCryptoCtx,
        sizeof(*pCryptoCtx) ) );

ErrorExit:
    ChkVOID( _FreeCryptoContext( f_pContext, &pCryptoCtx ) );
    return dr;
}
#endif

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
** This function decrypts data using ECC256.
**
** Operations Performed:
**
**  1. Decrypt the given data with the given private key and return the decrypted result.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pPrivkey:                (in) The private ECC encryption key to use.
** f_pCiphertext:             (in) The message to be decrypted.
** f_pPlaintext:             (out) The decrypted version of the message.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_ECC256_Decrypt(
    __inout                                               OEM_TEE_CONTEXT              *f_pContext,
    __in                                            const OEM_TEE_KEY                  *f_pPrivkey,
    __in                                            const CIPHERTEXT_P256              *f_pCiphertext,
    __out                                                 PLAINTEXT_P256               *f_pPlaintext )
{
    DRM_RESULT                        dr         = DRM_SUCCESS;
    DRM_CRYPTO_CONTEXT_FOR_ECC_ONLY  *pCryptoCtx = NULL;

    ChkDR( _AllocCryptoContext(
        f_pContext,
        &pCryptoCtx ) );

    ChkDR( OEM_ECC_Decrypt_P256(
        OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_ECC256_TO_PRIVKEY_P256( f_pPrivkey ),
        f_pCiphertext,
        f_pPlaintext,
        (struct bigctx_t*)pCryptoCtx,
        sizeof(*pCryptoCtx) ) );

ErrorExit:
    ChkVOID( _FreeCryptoContext( f_pContext, &pCryptoCtx ) );
    return dr;
}
#endif
#if defined (SEC_COMPILE)
/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST
** have a valid implementation for your PlayReady port if this function is still called
** by other OEM_TEE functions once your port is complete.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function attempts to generate an ECC256-encryptable set of plaintext
** where the second half of plain text is already initialized.
** Refer to OEM_ECC_GenerateHMACKey_P256 for additional details.
**
** Operations Performed:
**
**  1. Randomly generate the first half of the ECC256-encryptable set of plaintext
**     while leaving the second half unchanged.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pKeys:              (in/out) On input, the second half of this
**                                plaintext is a caller-defined value
**                                On output, the first half is populated.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_ECC256_GenerateHMACKey(
    __inout                                               OEM_TEE_CONTEXT              *f_pContext,
    __inout                                               PLAINTEXT_P256               *f_pKeys )
{
    DRM_RESULT                        dr         = DRM_SUCCESS;
    DRM_CRYPTO_CONTEXT_FOR_ECC_ONLY  *pCryptoCtx = NULL;

    ChkDR( _AllocCryptoContext(
        f_pContext,
        &pCryptoCtx ) );

    ChkDR( OEM_ECC_GenerateHMACKey_P256( f_pKeys, (struct bigctx_t*)pCryptoCtx, sizeof(*pCryptoCtx) ) );

ErrorExit:
    ChkVOID( _FreeCryptoContext( f_pContext, &pCryptoCtx ) );
    return dr;
}
#endif
/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST
** have a valid implementation for your PlayReady port if this function is still called
** by other OEM_TEE functions once your port is complete.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function generates an ECC256 public/private key pair.
**
** Operations Performed:
**
**  1. Randomly generate an ECC256 public/private key pair.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pPubKey:               (out) The generated public key.
** f_pPrivKey:              (out) The generated private key.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_ECC256_GenKeyPairPriv(
    __inout                                               OEM_TEE_CONTEXT              *f_pContext,
    __out                                                 PUBKEY_P256                  *f_pPubKey,
    __out                                                 OEM_TEE_KEY                  *f_pPrivKey )
{
    DRM_RESULT                        dr         = DRM_SUCCESS;
    DRM_CRYPTO_CONTEXT_FOR_ECC_ONLY  *pCryptoCtx = NULL;

    ChkDR( _AllocCryptoContext(
        f_pContext,
        &pCryptoCtx ) );

    ChkDR( OEM_ECC_GenKeyPair_P256(
        f_pPubKey,
        OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_ECC256_TO_PRIVKEY_P256( f_pPrivKey ),
        (struct bigctx_t*)pCryptoCtx,
        sizeof(*pCryptoCtx) ) );

ErrorExit:
    ChkVOID( _FreeCryptoContext( f_pContext, &pCryptoCtx ) );
    return dr;
}
#endif
EXIT_PK_NAMESPACE_CODE;

