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
#ifdef NONE
/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST
** have a valid implementation for your PlayReady port if this function is still called
** by other OEM_TEE functions once your port is complete.
**
** Any reimplementation MUST exactly match the behavior of the default PK implementation.
**
** This function is only used by remote provisioning.
**
** Synopsis:
**
** Does AES CBC-Mode encryption on a buffer of data.
**
** Operations Performed:
**
**  1. Encrypt the given data with the provided AES key.
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
** f_pKey:                  (in) The AES secret key used to encrypt the buffer.
** f_pbData:            (in/out) The data to be encrypted.
** f_cbData:                (in) The size (in bytes) of the data to be encrypted.
** f_rgbIV:                 (in) The AES CBC IV.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_CbcEncryptData(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pKey,
    __inout_bcount( f_cbData )                            DRM_BYTE                     *f_pbData,
    __in                                                  DRM_DWORD                     f_cbData,
    __in_bcount( DRM_AES_BLOCKLEN )                 const DRM_BYTE                      f_rgbIV[ DRM_AES_BLOCKLEN ] )
{
    return Oem_Aes_CbcEncryptData( OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_BROKER( f_pKey ), f_pbData, f_cbData, f_rgbIV );
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST
** have a valid implementation for your PlayReady port if this function is still called
** by other OEM_TEE functions once your port is complete.
**
** Any reimplementation MUST exactly match the behavior of the default PK implementation.
**
** This function is only used by remote provisioning.
**
** Synopsis:
**
** Does AES CBC-Mode decryption on a buffer of data.
**
** Operations Performed:
**
**  1. Decrypt the given data with the provided AES key.
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
** f_pKey:                  (in) The AES secret key used to decrypt the buffer.
** f_pbData:            (in/out) The data to be decrypted.
** f_cbData:                (in) The size (in bytes) of the data to be decrypted.
** f_rgbIV:                 (in) The AES CBC IV.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_CbcDecryptData(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pKey,
    __inout_bcount( f_cbData )                            DRM_BYTE                     *f_pbData,
    __in                                                  DRM_DWORD                     f_cbData,
    __in_bcount( DRM_AES_BLOCKLEN )                 const DRM_BYTE                      f_rgbIV[ DRM_AES_BLOCKLEN ] )
{
    return Oem_Aes_CbcDecryptData( OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_BROKER( f_pKey ), f_pbData, f_cbData, f_rgbIV );
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
** This function encrypts the given data with an AES key using AES ECB.
**
** Operations Performed:
**
**  1. AES ECB encrypt the data using the provided Key.
**
** Arguments:
**
** f_pContext:              (in/out) The TEE context returned from
**                                   OEM_TEE_BASE_AllocTEEContext.
** f_pKey:                      (in) AES Key to encrypt the data.
** f_pbData:                (in/out) Buffer holding the clear data and receive the encrypted data.
** f_cbData:                    (in) Length of the data to encrypt.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_EcbEncryptData(
    __inout                                               OEM_TEE_CONTEXT              *f_pContext,
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pKey,
    __inout_bcount( f_cbData )                            DRM_BYTE                     *f_pbData,
    __in                                                  DRM_DWORD                     f_cbData )
{
    return Oem_Aes_EcbEncryptData( OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_BROKER( f_pKey ), f_pbData, f_cbData );
}

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
** This function decrypts the given data with an AES key using AES ECB.
**
** Operations Performed:
**
**  1. AES ECB decrypt the data using the provided Key.
**
** Arguments:
**
** f_pContext:              (in/out) The TEE context returned from
**                                   OEM_TEE_BASE_AllocTEEContext.
** f_pKey:                      (in) AES Key to decrypt the data.
** f_pbData:                (in/out) Buffer holding the encrypted data and receive the decrypted data.
** f_cbData:                    (in) Length of the data to decrypt.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_EcbDecryptData(
    __inout                                               OEM_TEE_CONTEXT              *f_pContext,
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pKey,
    __inout_bcount( f_cbData )                            DRM_BYTE                     *f_pbData,
    __in                                                  DRM_DWORD                     f_cbData )
{
    return Oem_Aes_EcbDecryptData( OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_BROKER( f_pKey ), f_pbData, f_cbData );
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
** This function encrypts/decrypts the given data with an AES key using AES CTR.
**
** Operations Performed:
**
**  1. AES CTR encrypt/decrypt the data using the provided Key.
**
** Arguments:
**
** f_pContext:              (in/out) The TEE context returned from
**                                   OEM_TEE_BASE_AllocTEEContext.
** f_pKey:                      (in) AES Key to encrypt the data.
** f_pbData:                (in/out) Buffer holding the clear/encrypted data and receive the encrypted/clear data.
** f_cbData:                    (in) Length of the data to encrypt/decrypt.
** f_pCtrContext:           (in/out) CTR mode context.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
#ifdef NONE
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_CtrProcessData(
    __inout                                               OEM_TEE_CONTEXT              *f_pContext,
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pKey,
    __inout_bcount( f_cbData )                            DRM_BYTE                     *f_pbData,
    __in                                                  DRM_DWORD                     f_cbData,
    __inout_ecount( 1 )                                   DRM_AES_COUNTER_MODE_CONTEXT *f_pCtrContext )
{
    return Oem_Aes_CtrProcessData( OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_BROKER( f_pKey ), f_pbData, f_cbData, f_pCtrContext );
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
** This function signs the given data with an AES key using AES OMAC1.
**
** Operations Performed:
**
**  1. AES OMAC1 sign the provided data.
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
** f_pKey:                      (in) AES Key to sign the data.
** f_pbData:                    (in) Buffer holding the data to sign.
** f_ibData:                    (in) Offset into buffer where the data to sign resides.
** f_cbData:                    (in) Number of bytes to sign.
** f_rgbTag:                   (out) Signature.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_OMAC1_Sign(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pKey,
    __in_bcount( f_ibData + f_cbData )              const DRM_BYTE                     *f_pbData,
    __in                                                  DRM_DWORD                     f_ibData,
    __in                                                  DRM_DWORD                     f_cbData,
    __out_bcount( DRM_AES_BLOCKLEN )                      DRM_BYTE                      f_rgbTag[ DRM_AES_BLOCKLEN ] )
{
    return Oem_Omac1_Sign( OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_BROKER( f_pKey ), f_pbData, f_ibData, f_cbData, f_rgbTag );
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
** This function verifies the given data with an AES key using AES OMAC1.
**
** Operations Performed:
**
**  1. AES OMAC1 verify the provided data.
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
** f_pKey:                      (in) AES Key to verify the data.
** f_pbData:                    (in) Buffer holding the data to verify.
** f_ibData:                    (in) Offset into buffer where the signed data resides.
** f_cbData:                    (in) Number of bytes to verify.
** f_pbSignature:               (in) Buffer holding the signature.
** f_ibSignature:               (in) Offset into buffer where the signature resides.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_OMAC1_Verify(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pKey,
    __in_bcount( f_ibData + f_cbData )              const DRM_BYTE                     *f_pbData,
    __in                                                  DRM_DWORD                     f_ibData,
    __in                                                  DRM_DWORD                     f_cbData,
    __in_bcount( f_ibSignature + DRM_AES_BLOCKLEN ) const DRM_BYTE                     *f_pbSignature,
    __in                                                  DRM_DWORD                     f_ibSignature )
{
    return Oem_Omac1_Verify( OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_BROKER( f_pKey ), f_pbData, f_ibData, f_cbData, f_pbSignature, f_ibSignature );
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
** Refer to OEM_TEE_BASE_DeriveKey for algorithmic information.
**
** Operations Performed:
**
**  1. Use the given derivation key and given Key Derivation ID to derive an AES 128 Key
**     using the NIST algorithm as dislwssed above.
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
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_KDFCTR_r8_L128(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pDeriverKey,
    __in_ecount( 1 )                                const DRM_ID                       *f_pidLabel,
    __in_ecount_opt( 1 )                            const DRM_ID                       *f_pidContext,
    __out_ecount( DRM_AES_KEYSIZE_128 )                   DRM_BYTE                     *f_pbDerivedKey )
{
#if DRM_DBG
    {
        static const DRM_BYTE rgbZero[DRM_AES_KEYSIZE_128] = {0};
        DRMASSERT( !OEM_SELWRE_ARE_EQUAL( OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_BYTES( f_pDeriverKey ), rgbZero, DRM_AES_KEYSIZE_128 ) );
    }
#endif  /* DRM_DBG */

    return Oem_Aes_AES128KDFCTR_r8_L128( OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_BROKER( f_pDeriverKey ), f_pidLabel, f_pidContext, f_pbDerivedKey );
}
#endif
EXIT_PK_NAMESPACE_CODE;

