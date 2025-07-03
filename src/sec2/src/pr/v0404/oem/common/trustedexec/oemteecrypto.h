/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef _OEMTEECRYPTO_H_
#define _OEMTEECRYPTO_H_ 1

#include <oemteetypes.h>
#include <oemaeskeywraptypes.h>
#include <oemsha256types.h>

ENTER_PK_NAMESPACE;

PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_PARAM_25004, "OEM_TEE_CONTEXT should never be const." )

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_AllocKey(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __out                                                 OEM_TEE_KEY                  *f_pKey );

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL OEM_TEE_CRYPTO_AES128_FreeKey(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __inout                                               OEM_TEE_KEY                  *f_pKey );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_SetKey(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __inout_ecount( 1 )                                   OEM_TEE_KEY                  *f_pKey,
    __in_bcount( DRM_AES_KEYSIZE_128 )              const DRM_BYTE                      f_rgbKey[ DRM_AES_KEYSIZE_128 ] );

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL OEM_TEE_CRYPTO_AES128_GetKey(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __in                                            const OEM_TEE_KEY                  *f_pKey,
    __out_bcount( DRM_AES_KEYSIZE_128 )                   DRM_BYTE                      f_rgbKey[DRM_AES_KEYSIZE_128] );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_CopyKey(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __inout                                               OEM_TEE_KEY                  *f_pPreallocatedDestKey,
    __in                                            const OEM_TEE_KEY                  *f_pSourceKey );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_EncryptOneBlock(
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pKey,
    __inout_bcount( DRM_AES_BLOCKLEN )                    DRM_BYTE                      f_rgbData[ DRM_AES_BLOCKLEN ],
    __in                                            const DRM_DWORD                     f_fCryptoMode);

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_DecryptOneBlock(
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pKey,
    __inout_bcount( DRM_AES_BLOCKLEN )                    DRM_BYTE                      f_rgbData[ DRM_AES_BLOCKLEN ],
    __in                                            const DRM_DWORD                     f_fCryptoMode);

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_CbcEncryptData(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pKey,
    __inout_bcount( f_cbData )                            DRM_BYTE                     *f_pbData,
    __in                                                  DRM_DWORD                     f_cbData,
    __in_bcount( DRM_AES_BLOCKLEN )                 const DRM_BYTE                      f_rgbIV[ DRM_AES_BLOCKLEN ] );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_CbcDecryptData(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pKey,
    __inout_bcount( f_cbData )                            DRM_BYTE                     *f_pbData,
    __in                                                  DRM_DWORD                     f_cbData,
    __in_bcount( DRM_AES_BLOCKLEN )                 const DRM_BYTE                      f_rgbIV[ DRM_AES_BLOCKLEN ] );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_EcbEncryptData(
    __inout                                               OEM_TEE_CONTEXT              *f_pContext,
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pKey,
    __inout_bcount( f_cbData )                            DRM_BYTE                     *f_pbData,
    __in                                                  DRM_DWORD                     f_cbData );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_EcbDecryptData(
    __inout                                               OEM_TEE_CONTEXT              *f_pContext,
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pKey,
    __inout_bcount( f_cbData )                            DRM_BYTE                     *f_pbData,
    __in                                                  DRM_DWORD                     f_cbData );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_CtrProcessData(
    __inout                                               OEM_TEE_CONTEXT              *f_pContext,
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pKey,
    __inout_bcount( f_cbData )                            DRM_BYTE                     *f_pbData,
    __in                                                  DRM_DWORD                     f_cbData,
    __inout_ecount( 1 )                                   DRM_AES_COUNTER_MODE_CONTEXT *f_pCtrContext );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_OMAC1_Sign(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pKey,
    __in_bcount( f_ibData + f_cbData )              const DRM_BYTE                     *f_pbData,
    __in                                                  DRM_DWORD                     f_ibData,
    __in                                                  DRM_DWORD                     f_cbData,
    __out_bcount( DRM_AES_BLOCKLEN )                      DRM_BYTE                      f_rgbTag[ DRM_AES_BLOCKLEN ] );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_OMAC1_Verify(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pKey,
    __in_bcount( f_ibData + f_cbData )              const DRM_BYTE                     *f_pbData,
    __in                                                  DRM_DWORD                     f_ibData,
    __in                                                  DRM_DWORD                     f_cbData,
    __in_bcount( f_ibSignature + DRM_AES_BLOCKLEN ) const DRM_BYTE                     *f_pbSignature,
    __in                                                  DRM_DWORD                     f_ibSignature );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_KDFCTR_r8_L128(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pDeriverKey,
    __in_ecount( 1 )                                const DRM_ID                       *f_pidLabel,
    __in_ecount_opt( 1 )                            const DRM_ID                       *f_pidContext,
    __out_ecount( DRM_AES_KEYSIZE_128 )                   DRM_BYTE                     *f_pbDerivedKey );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_ECC256_AllocKey(
    __inout                                               OEM_TEE_CONTEXT              *f_pContext,
    __out                                                 OEM_TEE_KEY                  *f_pKey );

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL OEM_TEE_CRYPTO_ECC256_FreeKey(
    __inout                                               OEM_TEE_CONTEXT              *f_pContext,
    __inout                                               OEM_TEE_KEY                  *f_pKey );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_ECC256_SetKey(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __inout_ecount( 1 )                                   OEM_TEE_KEY                  *f_pKey,
    __in_bcount( ECC_P256_INTEGER_SIZE_IN_BYTES )   const DRM_BYTE                     *f_pbKey );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_ECC256_GenerateTeeSigningPrivateKey(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pEccKey,
    __out_ecount( 1 )                                     OEM_TEE_KEY                  *f_pSigningKey );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_ECC256_ECDSA_SignHash(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __in_ecount( ECC_P256_INTEGER_SIZE_IN_BYTES )   const DRM_BYTE                      f_rgbMessage[ECC_P256_INTEGER_SIZE_IN_BYTES],
    __in                                            const OEM_TEE_KEY                  *f_pPrivkey,
    __out                                                 SIGNATURE_P256               *f_pSignature )
GCC_ATTRIB_NO_STACK_PROTECT();

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_ECC256_ECDSA_SHA256_Sign(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __in_ecount( f_cbMessageLen )                   const DRM_BYTE                      f_rgbMessage[],
    __in                                            const DRM_DWORD                     f_cbMessageLen,
    __in                                            const OEM_TEE_KEY                  *f_pECCSigningKey,
    __out                                                 SIGNATURE_P256               *f_pSignature )
GCC_ATTRIB_NO_STACK_PROTECT();

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_ECC256_ECDSA_SHA256_Verify(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __in_ecount( f_cbMessageLen )                   const DRM_BYTE                      f_rgbMessage[],
    __in                                            const DRM_DWORD                     f_cbMessageLen,
    __in                                            const PUBKEY_P256                  *f_pPubkey,
    __in                                            const SIGNATURE_P256               *f_pSignature );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_ECC256_Encrypt(
    __inout                                               OEM_TEE_CONTEXT              *f_pContext,
    __in                                            const PUBKEY_P256                  *f_pPubkey,
    __in                                            const PLAINTEXT_P256               *f_pPlaintext,
    __out                                                 CIPHERTEXT_P256              *f_pCiphertext );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_ECC256_Decrypt(
    __inout                                               OEM_TEE_CONTEXT              *f_pContext,
    __in                                            const OEM_TEE_KEY                  *f_pPrivkey,
    __in                                            const CIPHERTEXT_P256              *f_pCiphertext,
    __out                                                 PLAINTEXT_P256               *f_pPlaintext );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_ECC256_GenerateHMACKey(
    __inout                                               OEM_TEE_CONTEXT              *f_pContext,
    __inout                                               PLAINTEXT_P256               *f_pKeys );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_ECC256_GenKeyPairPriv(
    __inout                                               OEM_TEE_CONTEXT              *f_pContext,
    __out                                                 PUBKEY_P256                  *f_pPubKey,
    __out                                                 OEM_TEE_KEY                  *f_pPrivKey )
GCC_ATTRIB_NO_STACK_PROTECT();

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_SHA256_Init(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __out_ecount( 1 )                                     OEM_SHA256_CONTEXT           *f_pShaContext );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_SHA256_Update(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __inout_ecount( 1 )                                   OEM_SHA256_CONTEXT           *f_pShaContext,
    __in_ecount( f_cbBuffer )                       const DRM_BYTE                      f_rgbBuffer[],
    __in                                                  DRM_DWORD                     f_cbBuffer );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_SHA256_Finalize(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __inout_ecount( 1 )                                   OEM_SHA256_CONTEXT           *f_pShaContext,
    __out_ecount( 1 )                                     OEM_SHA256_DIGEST            *f_pDigest );

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL OEM_TEE_CRYPTO_BASE_GetStaticKeyHistoryKey(
    __inout_ecount( 1 )                                   OEM_TEE_KEY                  *f_pKey );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_RANDOM_GetBytes(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __out_bcount( f_cbData )                              DRM_BYTE                     *f_pbData,
    __in                                                  DRM_DWORD                     f_cbData );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AESKEYWRAP_WrapKeyAES128(
    __inout                                               OEM_TEE_CONTEXT              *f_pContext,
    __in                                            const OEM_TEE_KEY                  *f_pKey,
    __in_ecount( 1 )                                const OEM_UNWRAPPED_KEY_AES_128    *f_pPlaintext,
    __out_ecount( 1 )                                     OEM_WRAPPED_KEY_AES_128      *f_pCiphertext );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AESKEYWRAP_UnwrapKeyAES128(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __in                                            const OEM_TEE_KEY                  *f_pKey,
    __in_ecount( 1 )                                const OEM_WRAPPED_KEY_AES_128      *f_pCiphertext,
    __out_ecount( 1 )                                     OEM_UNWRAPPED_KEY_AES_128    *f_pPlaintext );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AESKEYWRAP_WrapKeyECC256(
    __inout                                               OEM_TEE_CONTEXT              *f_pContext,
    __in                                            const OEM_TEE_KEY                  *f_pKey,
    __in_ecount( 1 )                                const OEM_UNWRAPPED_KEY_ECC_256    *f_pUnWrappedKey,
    __out_ecount( 1 )                                     OEM_WRAPPED_KEY_ECC_256      *f_pCiphertext );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AESKEYWRAP_UnwrapKeyECC256(
    __inout                                               OEM_TEE_CONTEXT              *f_pContext,
    __in                                            const OEM_TEE_KEY                  *f_pKey,
    __in_ecount( 1 )                                const OEM_WRAPPED_KEY_ECC_256      *f_pCiphertext,
    __out_ecount( 1 )                                     OEM_UNWRAPPED_KEY_ECC_256    *f_pPlaintext );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL ComputeDmhash_HS(
    __inout                                               DRM_BYTE                     *f_pbHash,
    __in                                                  DRM_BYTE                     *f_pbData,
    __in                                                  DRM_DWORD                     f_dwSize );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Random_GetBytes_HS(
    __out_bcount( f_cbData )                              DRM_BYTE                     *f_pbData,
    __in                                                  DRM_DWORD                     f_cbData );



PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */

EXIT_PK_NAMESPACE;

#endif /* _OEMTEECRYPTO_H_ */

