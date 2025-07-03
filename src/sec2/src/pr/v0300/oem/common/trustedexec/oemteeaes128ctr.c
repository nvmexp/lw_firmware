/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <oemtee.h>
#include <oemteecrypto.h>
#include <oemteeinternal.h>
#include <oembyteorder.h>
#include <drmmathsafe.h>
#include <drmlastinclude.h>

PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_PARAM_25004, "Prefast Noise: OEM_TEE_CONTEXT* should not be const.");

ENTER_PK_NAMESPACE_CODE;

#define OEM_TEE_CONTENT_HEADER_CHECKSUM_SIZE_IN_BYTES 8

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** This function is only used if the client supports playback of AES CTR encrypted content.
** Certain PlayReady clients, such as PRND Transmitters, may not need to implement this function.
**
** Synopsis:
**
** This function decrypts the content key pair (CI/CK) from a leaf license
** using CK from a root license.
**
** Operations Performed:
**
**  1. AES ECB 128 Decrypt the given cipher text into CI/CK using the given root CK.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pRootCK:            (in)     The root license's CK.
** f_pCiphertext:        (in)     The cipher text for CI/CK from inside the XMR license.
** f_pCI:                (in/out) CI.
**                                The OEM_TEE_KEY must be pre-allocated
**                                by the caller using OEM_TEE_BASE_AllocKeyAES128
**                                and freed after use by the caller using
**                                OEM_TEE_BASE_FreeKeyAES128.
** f_pCK:                (in/out) CK.
**                                The OEM_TEE_KEY must be pre-allocated
**                                by the caller using OEM_TEE_BASE_AllocKeyAES128
**                                and freed after use by the caller using
**                                OEM_TEE_BASE_FreeKeyAES128.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_AES128CTR_DecryptContentKeysWithLicenseKey(
    __inout                                               OEM_TEE_CONTEXT            *f_pContext,
    __in                                            const OEM_TEE_KEY                *f_pRootCK,
    __in                                                  DRM_DWORD                   f_cbCiphertext,
    __in_bcount( f_cbCiphertext )                   const DRM_BYTE                   *f_pbCiphertext,
    __inout                                               OEM_TEE_KEY                *f_pCI,
    __inout                                               OEM_TEE_KEY                *f_pCK )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_BYTE   rgbKeys[DRM_AES_KEYSIZE_128*2]; /* Do not zero-initialize key material */

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );
    DRMASSERT( f_pCI          != NULL );
    DRMASSERT( f_pCK          != NULL );
    DRMASSERT( f_pbCiphertext != NULL );

    ChkBOOL( f_cbCiphertext == DRM_AES_KEYSIZE_128 * 2, DRM_E_ILWALID_LICENSE );

    ChkVOID( OEM_TEE_MEMCPY( rgbKeys, f_pbCiphertext, DRM_AES_KEYSIZE_128 * 2 ) );

    ChkDR( OEM_TEE_CRYPTO_AES128_EcbDecryptData(
        f_pContext,
        f_pRootCK,
        rgbKeys,
        sizeof(rgbKeys) ) );

    ChkDR( OEM_TEE_CRYPTO_AES128_SetKey( f_pContext, f_pCI, &rgbKeys[0] ) );
    ChkDR( OEM_TEE_CRYPTO_AES128_SetKey( f_pContext, f_pCK, &rgbKeys[DRM_AES_KEYSIZE_128]) );

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** This function is only used if the client supports playback of AES CTR encrypted content.
** Certain PlayReady clients, such as PRND Transmitters, may not need to implement this function.
**
** Synopsis:
**
** This function verifies the checksum between a root license and a leaf license
** or between a leaf/simple license and a content header to ensure that the KID
** matches the correct content key.
**
** Operations Performed:
**
**  1. AES 128 ECB Encrypt the given KID with the given CI.
**  2. Verify that the given checksum is equal to the encrypted KID.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pCK:                (in)     The license's CK.
** f_pbKID:              (in)     The buffer containing the license's KID.
** f_pbChecksum:         (in)     The buffer containing the checksum.
** f_cbChecksum:         (in)     The size of the checksum.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_AES128CTR_VerifyLicenseChecksum(
    __inout                                               OEM_TEE_CONTEXT            *f_pContext,
    __in                                            const OEM_TEE_KEY                *f_pCK,
    __in_bcount( sizeof(DRM_GUID) )                 const DRM_BYTE                   *f_pbKID,
    __in_bcount( f_cbChecksum )                     const DRM_BYTE                   *f_pbChecksum,
    __in                                                  DRM_DWORD                   f_cbChecksum )
{
    DRM_RESULT  dr    = DRM_SUCCESS;
    DRM_GUID    idKID = {0};

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );

    DRMASSERT( f_pCK        != NULL );
    DRMASSERT( f_pbKID      != NULL );
    DRMASSERT( f_pbChecksum != NULL );
    DRMASSERT( f_cbChecksum > 0 );

    ChkBOOL( f_cbChecksum == OEM_TEE_CONTENT_HEADER_CHECKSUM_SIZE_IN_BYTES, DRM_E_CH_ILWALID_CHECKSUM );

    ChkVOID( OEM_TEE_MEMCPY( &idKID, f_pbKID, sizeof(idKID) ) );
    ChkDR( OEM_TEE_CRYPTO_AES128_EcbEncryptData( f_pContext, f_pCK, (DRM_BYTE*)&idKID, sizeof( idKID ) ) );

    ChkBOOL( OEM_SELWRE_ARE_EQUAL( f_pbChecksum, &idKID, f_cbChecksum ), DRM_E_CH_BAD_KEY );

ErrorExit:
    return dr;
}
#endif

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** This function is only used if the client supports playback of AES CTR encrypted content.
** Certain PlayReady clients, such as PRND Transmitters, may not need to implement this function.
**
** Synopsis:
**
** This function in-place decrypts content into the clear using AES128CTR.
**
** Operations Performed:
**
**  1. In-place decrypt the given content into the clear using AES128CTR.
**
** Arguments:
**
** f_pContext:                      (in/out) The TEE context returned from
**                                           OEM_TEE_BASE_AllocTEEContext.
** f_ePolicyCallingAPI:             (in)     The calling API providing f_pPolicy.
** f_pPolicy:                       (in)     The current content protection policy.
** f_pCK:                           (in)     The content key used to decrypt the content.
** f_cEncryptedRegionMappings:      (in)     The number of encrypted-region mappings.
** f_pcbEncryptedRegionMappings:    (in)     The set of encrypted region mappings as pairs of
**                                           DRM_DWORDs.
**                                           The number of DRM_DWORDs must be a multiple
**                                           of two.  Each DRM_DWORD pair represents
**                                           values indicating how many clear bytes
**                                           followed by how many encrypted bytes are
**                                           present in the data on output.
**                                           The sum of all of these DRM_DWORD pairs
**                                           must be equal to bytecount(clear content),
**                                           i.e. equal to f_pEncrypted->cb.
**                                           If the entire f_pEncrypted is encrypted, there will be
**                                           two DRM_DWORDs with values 0, bytecount(encrypted content).
**                                           If the entire f_pEncrypted is clear, there will be
**                                           two DRM_DWORDs with values bytecount(clear content), 0.
** f_ui64Initializatiolwector:      (in)     The AES128CTR initialization vector.
** f_cbEncryptedToClear:            (in)     The number of bytes of content to decrypt.
** f_pbEncryptedToClear:            (in/out) On input, the encrypted content to decrypt.
**                                           On output, the clear content.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
#ifdef NONE
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_AES128CTR_DecryptContentIntoClear(
    __inout                                         OEM_TEE_CONTEXT                 *f_pContext,
    __in                                            DRM_TEE_POLICY_INFO_CALLING_API  f_ePolicyCallingAPI,
    __in                                      const DRM_TEE_POLICY_INFO             *f_pPolicy,
    __in                                      const OEM_TEE_KEY                     *f_pCK,
    __in                                            DRM_DWORD                        f_cEncryptedRegionMappings,
    __in_ecount( f_cEncryptedRegionMappings ) const DRM_DWORD                       *f_pdwEncryptedRegionMappings,
    __in                                            DRM_UINT64                       f_ui64Initializatiolwector,
    __in                                            DRM_DWORD                        f_cbEncryptedToClear,
    __inout_bcount( f_cbEncryptedToClear )          DRM_BYTE                        *f_pbEncryptedToClear )
{
    DRM_RESULT                     dr                           = DRM_SUCCESS;
    DRM_AES_COUNTER_MODE_CONTEXT   oCtrContext                  = {0};
    DRM_DWORD                      idx                          = 0;
    DRM_DWORD                      cbTotalEncrypted             = 0;
    DRM_DWORD                      cbTotalClear                 = 0;
    DRM_DWORD                      cbTotalSample                = 0;
    DRM_BYTE                      *pbLwrrent                    = NULL;

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );
    DRMASSERT( f_pCK                              != NULL );
    DRMASSERT( f_cEncryptedRegionMappings          > 0 );
    DRMASSERT( ( f_cEncryptedRegionMappings & 1 ) == 0 );     /* Must be DWORD pairs.  Power of two: use & instead of % for better perf */
    DRMASSERT( f_pdwEncryptedRegionMappings       != NULL );
    DRMASSERT( f_cbEncryptedToClear                > 0 );
    DRMASSERT( f_pbEncryptedToClear               != NULL );

    pbLwrrent = f_pbEncryptedToClear;
    oCtrContext.qwInitializatiolwector = f_ui64Initializatiolwector;

    idx = 0;
    while( idx < f_cEncryptedRegionMappings )
    {
        DRM_DWORD    cbClear        = f_pdwEncryptedRegionMappings[ idx++ ];
        DRM_DWORD    cbEncrypted    = f_pdwEncryptedRegionMappings[ idx++ ];

        ChkArg( ( cbClear > 0 ) || ( cbEncrypted > 0 ) );

        ChkDR( DRM_DWordAddSame( &cbTotalClear,      cbClear ) );
        ChkDR( DRM_DWordAddSame( &cbTotalEncrypted,  cbEncrypted  ) );
        ChkDR( DRM_DWordAdd( cbTotalClear,      cbTotalEncrypted,   &cbTotalSample ) );

        ChkArg( cbTotalSample <= f_cbEncryptedToClear );

        if( cbClear > 0 )
        {
            ChkDR( DRM_DWordPtrAddSame( (DRM_DWORD_PTR*)&pbLwrrent, cbClear ) );
        }

        if( cbEncrypted > 0 )
        {
            ChkDR( OEM_TEE_CRYPTO_AES128_CtrProcessData( f_pContext, f_pCK, pbLwrrent, cbEncrypted, &oCtrContext ) );
            ChkDR( DRM_DWordPtrAddSame( (DRM_DWORD_PTR*)&pbLwrrent, cbEncrypted ) );
        }

        /*
        ** Since the encrypted data within the overall encrypted data is effectively treated as one chunk of
        ** encrypted data, we need to update the byte and block offsets in oCtrContext for the next call.
        */
        DRMCASSERT( DRM_AES_BLOCKLEN == 16 );   /* Required for below to work without modification */
        oCtrContext.bByteOffset     = cbTotalEncrypted & (DRM_DWORD)15;      /* Power of two: use & instead of % for better perf */
        oCtrContext.qwBlockOffset   = DRM_UI64HL( 0, cbTotalEncrypted >> (DRM_DWORD)4 );  /* Power of two: use >> instead of / for better perf */
    }

    ChkArg( cbTotalSample == f_cbEncryptedToClear );

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** This function is only used if the client supports playback of AES CTR encrypted content.
** Certain PlayReady clients, such as PRND Transmitters, may not need to implement this function.
** If you do not support handle-backed memory, this API should return DRM_E_NOTIMPL.
**
** Synopsis:
**
** This function in-place decrypts content into an opaque handle using AES128CTR.
**
** Operations Performed:
**
**  1. In-place decrypt the given content into an opaque handle using AES128CTR.
**
** Arguments:
**
** f_pContext:                      (in/out) The TEE context returned from
**                                           OEM_TEE_BASE_AllocTEEContext.
** f_ePolicyCallingAPI:             (in)     The calling API providing f_pPolicy.
** f_pPolicy:                       (in)     The current content protection policy.
** f_pCK:                           (in)     The content key used to decrypt the content.
** f_cEncryptedRegionMappings:      (in)     The number of encrypted-region mappings.
** f_pcbEncryptedRegionMappings:    (in)     The set of encrypted region mappings as pairs of
**                                           DRM_DWORDs.
**                                           The number of DRM_DWORDs must be a multiple
**                                           of two.  Each DRM_DWORD pair represents
**                                           values indicating how many clear bytes
**                                           followed by how many encrypted bytes are
**                                           present in the data on output.
**                                           The sum of all of these DRM_DWORD pairs
**                                           must be equal to bytecount(clear content),
**                                           i.e. equal to f_pEncrypted->cb.
**                                           If the entire f_pEncrypted is encrypted, there will be
**                                           two DRM_DWORDs with values 0, bytecount(encrypted content).
**                                           If the entire f_pEncrypted is clear, there will be
**                                           two DRM_DWORDs with values bytecount(clear content), 0.
** f_ui64Initializatiolwector:      (in)     The AES128CTR initialization vector.
** f_cbEncrypted:                   (in)     The number of bytes of content to decrypt.
** f_pbEncrypted:                   (in)     The bytes of content to decrypt.
** f_hClear:                        (in/out) On input, a handle pre-allocated by
**                                           OEM_TEE_BASE_SelwreMemHandleAlloc with the number of bytes
**                                           in f_cbEncrypted.
**                                           On output, the decrypted bytes.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_AES128CTR_DecryptContentIntoHandle(
    __inout                                         OEM_TEE_CONTEXT                 *f_pContext,
    __in                                            DRM_TEE_POLICY_INFO_CALLING_API  f_ePolicyCallingAPI,
    __in                                      const DRM_TEE_POLICY_INFO             *f_pPolicy,
    __in                                      const OEM_TEE_KEY                     *f_pCK,
    __in                                            DRM_DWORD                        f_cEncryptedRegionMappings,
    __in_ecount( f_cEncryptedRegionMappings ) const DRM_DWORD                       *f_pdwEncryptedRegionMappings,
    __in                                            DRM_UINT64                       f_ui64Initializatiolwector,
    __in                                            DRM_DWORD                        f_cbEncrypted,
    __in_bcount( f_cbEncrypted )              const DRM_BYTE                        *f_pbEncrypted,
    __inout                                         OEM_TEE_MEMORY_HANDLE            f_hClear )
{
    DRM_RESULT  dr      = DRM_SUCCESS;
    DRM_BYTE   *pbClear = NULL;

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );
    DRMASSERT( f_pCK                              != NULL );
    DRMASSERT( f_cEncryptedRegionMappings          > 0 );
    DRMASSERT( ( f_cEncryptedRegionMappings & 1 ) == 0 );     /* Must be DWORD pairs.  Power of two: use & instead of % for better perf */
    DRMASSERT( f_pdwEncryptedRegionMappings       != NULL );
    DRMASSERT( f_cbEncrypted                       > 0 );
    DRMASSERT( f_pbEncrypted                      != NULL );
    DRMASSERT( f_hClear                           != OEM_TEE_MEMORY_HANDLE_ILWALID );

    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( f_pContext, f_cbEncrypted, (DRM_VOID**)&pbClear ) );
    ChkVOID( OEM_TEE_MEMCPY( pbClear, f_pbEncrypted, f_cbEncrypted ) );

    ChkDR( OEM_TEE_AES128CTR_DecryptContentIntoClear(
        f_pContext,
        f_ePolicyCallingAPI,
        f_pPolicy,
        f_pCK,
        f_cEncryptedRegionMappings,
        f_pdwEncryptedRegionMappings,
        f_ui64Initializatiolwector,
        f_cbEncrypted,
        pbClear ) );

    ChkDR( OEM_TEE_BASE_SelwreMemHandleWrite( f_pContext, 0, f_cbEncrypted, pbClear, f_hClear ) );

ErrorExit:
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( f_pContext, (DRM_VOID**)&pbClear ) );
    return dr;
}
#endif

#if defined (SEC_COMPILE)
/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** This function is only used if the client supports playback of AES CTR encrypted content
** that uses scalable licenses.
** Certain PlayReady clients, such as PRND Transmitters, may not need to implement this function.
**
** Synopsis:
**
** This function unshuffles the content key to separate the CI and CK.
**
** Operations Performed:
**
**  1. Unshuffle the content key.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pCI:                (in/out) CI.
**                                The OEM_TEE_KEY must be pre-allocated
**                                by the caller using OEM_TEE_BASE_AllocKeyAES128
**                                and freed after use by the caller using
**                                OEM_TEE_BASE_FreeKeyAES128.
** f_pCK:                (in/out) CK.
**                                The OEM_TEE_KEY must be pre-allocated
**                                by the caller using OEM_TEE_BASE_AllocKeyAES128
**                                and freed after use by the caller using
**                                OEM_TEE_BASE_FreeKeyAES128.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_AES128CTR_UnshuffleScalableContentKeys(
    __inout                                        OEM_TEE_CONTEXT             *f_pContext,
    __inout                                        OEM_TEE_KEY                 *f_pCI,
    __inout                                        OEM_TEE_KEY                 *f_pCK )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_BYTE   rgbKey[2 * DRM_AES_KEYSIZE_128];  /* Do not zero-initialize key material */
    DRM_BYTE   rgbUnshuffledKey[2 * DRM_AES_KEYSIZE_128];  /* Do not zero-initialize key material */
    DRM_DWORD  iByte = 0;
    DRM_DWORD  jByte = 0;

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );

    ChkVOID( OEM_TEE_CRYPTO_AES128_GetKey( f_pContext, f_pCI, rgbKey ) );
    ChkVOID( OEM_TEE_CRYPTO_AES128_GetKey( f_pContext, f_pCK, rgbKey + DRM_AES_KEYSIZE_128 ) );

    for( iByte = 0; iByte < DRM_AES_KEYSIZE_128; iByte++ )
    {
        rgbUnshuffledKey[ iByte ]                       = rgbKey[ jByte++ ];
        rgbUnshuffledKey[ iByte + DRM_AES_KEYSIZE_128 ] = rgbKey[ jByte++ ];
    }

    ChkDR( OEM_TEE_CRYPTO_AES128_SetKey( f_pContext, f_pCI, &rgbUnshuffledKey[0] ) );
    ChkDR( OEM_TEE_CRYPTO_AES128_SetKey( f_pContext, f_pCK, &rgbUnshuffledKey[DRM_AES_KEYSIZE_128] ) );

ErrorExit:
    ChkVOID( OEM_SELWRE_ZERO_MEMORY( rgbKey          , sizeof(rgbKey)           ) );
    ChkVOID( OEM_SELWRE_ZERO_MEMORY( rgbUnshuffledKey, sizeof(rgbUnshuffledKey) ) );
    return dr;
}
#endif
#ifdef NONE
/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** This function is only used if the client supports playback of AES CTR encrypted content
** that uses scalable licenses.
** Certain PlayReady clients, such as PRND Transmitters, may not need to implement this function.
**
** Synopsis:
**
** This function shuffles the content key to mix the CI and CK.
**
** Operations Performed:
**
**  1. Shuffle the content key.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pCI:                (in/out) CI.
**                                The OEM_TEE_KEY must be pre-allocated
**                                by the caller using OEM_TEE_BASE_AllocKeyAES128
**                                and freed after use by the caller using
**                                OEM_TEE_BASE_FreeKeyAES128.
** f_pCK:                (in/out) CK.
**                                The OEM_TEE_KEY must be pre-allocated
**                                by the caller using OEM_TEE_BASE_AllocKeyAES128
**                                and freed after use by the caller using
**                                OEM_TEE_BASE_FreeKeyAES128.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_AES128CTR_ShuffleScalableContentKeys(
    __inout                                        OEM_TEE_CONTEXT             *f_pContext,
    __inout                                        OEM_TEE_KEY                 *f_pCI,
    __inout                                        OEM_TEE_KEY                 *f_pCK )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_BYTE   rgbKey[2 * DRM_AES_KEYSIZE_128];          /* Do not zero-initialize key material */
    DRM_BYTE   rgbShuffledKey[2 * DRM_AES_KEYSIZE_128];  /* Do not zero-initialize key material */
    DRM_DWORD  iByte = 0;
    DRM_DWORD  jByte = 0;

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );

    ChkVOID( OEM_TEE_CRYPTO_AES128_GetKey( f_pContext, f_pCI, rgbKey ) );
    ChkVOID( OEM_TEE_CRYPTO_AES128_GetKey( f_pContext, f_pCK, rgbKey + DRM_AES_KEYSIZE_128 ) );

    for( iByte = 0; iByte < DRM_AES_KEYSIZE_128; iByte++ )
    {
        rgbShuffledKey[ jByte++ ] = rgbKey[ iByte ];
        rgbShuffledKey[ jByte++ ] = rgbKey[ iByte + DRM_AES_KEYSIZE_128 ];
    }

    ChkDR( OEM_TEE_CRYPTO_AES128_SetKey( f_pContext, f_pCI, &rgbShuffledKey[0] ) );
    ChkDR( OEM_TEE_CRYPTO_AES128_SetKey( f_pContext, f_pCK, &rgbShuffledKey[DRM_AES_KEYSIZE_128] ) );

ErrorExit:
    ChkVOID( OEM_SELWRE_ZERO_MEMORY( rgbKey        , sizeof(rgbKey)         ) );
    ChkVOID( OEM_SELWRE_ZERO_MEMORY( rgbShuffledKey, sizeof(rgbShuffledKey) ) );
    return dr;
}
#endif

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** This function is only used if the client supports playback of AES CTR encrypted content
** that uses scalable licenses.
** Certain PlayReady clients, such as PRND Transmitters, may not need to implement this function.
**
** Synopsis:
**
** This function callwlates the content key prime based on a content key.
**
** Operations Performed:
**
**  1. XORs the clear content key with the byte array defined in f_pbPrime.
**  2. AES ECB 128 encrypt the XOR'd byte array to create the content key prime.
**
** Arguments:
**
** f_pContext:                      (in/out) The TEE context returned from
**                                           OEM_TEE_BASE_AllocTEEContext.
** f_pContentKey:                   (in)     The content key used to decrypt the content.
** f_pbPrime:                       (in)     The byte array to XOR with the content key.
** f_pContentKeyPrime:              (in/out) The content key prime.
**                                           The OEM_TEE_KEY must be pre-allocated
**                                           by the caller using OEM_TEE_BASE_AllocKeyAES128
**                                           and freed after use by the caller using
**                                           OEM_TEE_BASE_FreeKeyAES128.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_AES128CTR_CallwlateContentKeyPrimeWithAES128Key(
    __inout                                    OEM_TEE_CONTEXT            *f_pContext,
    __in                                 const OEM_TEE_KEY                *f_pContentKey,
    __in_bcount( DRM_AES_KEYSIZE_128 )   const DRM_BYTE                   *f_pbPrime,
    __inout                                    OEM_TEE_KEY                *f_pContentKeyPrime )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_BYTE   rgbKey[DRM_AES_KEYSIZE_128];  /* Do not zero-initialize key material */
    DRM_DWORD  iByte = 0;

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );

    ChkVOID( OEM_TEE_CRYPTO_AES128_GetKey( f_pContext, f_pContentKey, rgbKey ) );

    for( iByte = 0; iByte < DRM_AES_KEYSIZE_128; iByte++ )
    {
        rgbKey[ iByte ] ^= f_pbPrime[ iByte ];
    }

    ChkDR( OEM_TEE_CRYPTO_AES128_EcbEncryptData(
        f_pContext,
        f_pContentKey,
        rgbKey,
        sizeof(rgbKey) ) );

    ChkDR( OEM_TEE_CRYPTO_AES128_SetKey( f_pContext, f_pContentKeyPrime, &rgbKey[0] ) );

ErrorExit:
    return dr;
}
#endif

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** This function is only used if the client supports playback of AES CTR encrypted content
** that uses scalable licenses.
** Certain PlayReady clients, such as PRND Transmitters, may not need to implement this function.
**
** Synopsis:
**
** This function derives a scalable key based on a byte array passed by the caller.
**
** Operations Performed:
**
**  1. AES ECB 128 encrypt the byte array to create a derived scalable key.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pEncKey:            (in)     The AES key to encrypt the byte array to create a derived scalable key.
** f_pbKey:              (in)     The byte array to be encrypted to create a derived scalable key.
** f_pDerivedScalableKey:(in/out) The derived scalable key.
**                                The OEM_TEE_KEY must be pre-allocated
**                                by the caller using OEM_TEE_BASE_AllocKeyAES128
**                                and freed after use by the caller using
**                                OEM_TEE_BASE_FreeKeyAES128.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_AES128CTR_DeriveScalableKeyWithAES128Key(
    __inout                                    OEM_TEE_CONTEXT            *f_pContext,
    __in                                 const OEM_TEE_KEY                *f_pEncKey,
    __in_bcount( DRM_AES_KEYSIZE_128 )   const DRM_BYTE                   *f_pbKey,
    __inout                                    OEM_TEE_KEY                *f_pDerivedScalableKey )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_BYTE   rgbKey[DRM_AES_KEYSIZE_128];  /* Do not zero-initialize key material */

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );

    ChkVOID( OEM_TEE_MEMCPY( rgbKey, f_pbKey, DRM_AES_KEYSIZE_128 ) );

    ChkDR( OEM_TEE_CRYPTO_AES128_EcbEncryptData(
        f_pContext,
        f_pEncKey,
        rgbKey,
        sizeof(rgbKey) ) );

    ChkDR( OEM_TEE_CRYPTO_AES128_SetKey( f_pContext, f_pDerivedScalableKey, &rgbKey[0] ) );

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** This function is only used if the client supports playback of AES CTR encrypted content
** that uses scalable licenses.
** Certain PlayReady clients, such as PRND Transmitters, may not need to implement this function.
**
** Synopsis:
**
** This function initializes the uplinkX key with an initial value of zeros.
**
** Operations Performed:
**
**  1. Set initial AES 128 value.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pUplinkxKey:        (in/out) The uplinkX key to be initialized.
**                                The OEM_TEE_KEY must be pre-allocated
**                                by the caller using OEM_TEE_BASE_AllocKeyAES128
**                                and freed after use by the caller using
**                                OEM_TEE_BASE_FreeKeyAES128.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_AES128CTR_InitUplinkXKey(
    __inout                                    OEM_TEE_CONTEXT            *f_pContext,
    __inout                                    OEM_TEE_KEY                *f_pUplinkXKey )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_BYTE   rgbKey[DRM_AES_KEYSIZE_128];  /* Do not zero-initialize key material */

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );

    OEM_SELWRE_ZERO_MEMORY( rgbKey, sizeof( rgbKey ) );

    ChkDR( OEM_TEE_CRYPTO_AES128_SetKey( f_pContext, f_pUplinkXKey, rgbKey ) );

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** This function is only used if the client supports playback of AES CTR encrypted content
** that uses scalable licenses.
** Certain PlayReady clients, such as PRND Transmitters, may not need to implement this function.
**
** Synopsis:
**
** This function XORs the current uplinkX key with a derived key for a location defined in the
** leaf's UplinkX data.
**
** Operations Performed:
**
**  1. XOR the clear key of the current UplinkX key and the location's key to create an updated
**     uplinkX key.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pLocationKey:       (in)     The derived key of a location defined in the leaf license's UplinkX data.
** f_pUplinkXKey:        (in/out) The callwlated uplinkX key.
**                                The OEM_TEE_KEY must be pre-allocated
**                                by the caller using OEM_TEE_BASE_AllocKeyAES128
**                                and freed after use by the caller using
**                                OEM_TEE_BASE_FreeKeyAES128.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_AES128CTR_UpdateUplinkXKey(
    __inout                                    OEM_TEE_CONTEXT            *f_pContext,
    __in                                 const OEM_TEE_KEY                *f_pLocationKey,
    __inout                                    OEM_TEE_KEY                *f_pUplinkXKey )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_BYTE   rgbLocationKey[DRM_AES_KEYSIZE_128];  /* Do not zero-initialize key material */
    DRM_BYTE   rgbUplinkXKey[DRM_AES_KEYSIZE_128];   /* Do not zero-initialize key material */
    DRM_DWORD  iByte = 0;

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );

    ChkVOID( OEM_TEE_CRYPTO_AES128_GetKey( f_pContext, f_pLocationKey, rgbLocationKey ) );
    ChkVOID( OEM_TEE_CRYPTO_AES128_GetKey( f_pContext, f_pUplinkXKey, rgbUplinkXKey ) );

    for ( iByte = 0; iByte < DRM_AES_KEYSIZE_128; iByte++ )
    {
        rgbUplinkXKey[ iByte ] ^= rgbLocationKey[ iByte ];
    }

    ChkDR( OEM_TEE_CRYPTO_AES128_SetKey( f_pContext, f_pUplinkXKey, &rgbUplinkXKey[0] ) );

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** This function is only used if the client supports playback of AES CTR encrypted content
** that uses scalable licenses.
** Certain PlayReady clients, such as PRND Transmitters, may not need to implement this function.
**
** Synopsis:
**
** This function decrypts the content key pair (CI/CK) from a leaf license
** using the secondary key from root license and a callwlated uplinkX key.
**
** Operations Performed:
**
**  1. AES ECB 128 encrypt the given cipher text into CI/CK using the above two keys.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pSecondaryKey:      (in)     The root license's secondary key.
** f_pUplinkxKey:        (in)     The callwlated uplinkX key.
** f_pCiphertext:        (in)     The cipher text for CI/CK from inside the XMR license.
** f_pCI:                (in/out) CI.
**                                The OEM_TEE_KEY must be pre-allocated
**                                by the caller using OEM_TEE_BASE_AllocKeyAES128
**                                and freed after use by the caller using
**                                OEM_TEE_BASE_FreeKeyAES128.
** f_pCK:                (in/out) CK.
**                                The OEM_TEE_KEY must be pre-allocated
**                                by the caller using OEM_TEE_BASE_AllocKeyAES128
**                                and freed after use by the caller using
**                                OEM_TEE_BASE_FreeKeyAES128.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_AES128CTR_DecryptContentKeysWithDerivedKeys(
    __inout                                         OEM_TEE_CONTEXT            *f_pContext,
    __in                                      const OEM_TEE_KEY                *f_pSecondaryKey,
    __in                                      const OEM_TEE_KEY                *f_pUplinkxKey,
    __in_bcount( DRM_AES_KEYSIZE_128_X2 )     const DRM_BYTE                   *f_pbCiphertext,
    __inout                                         OEM_TEE_KEY                *f_pCI,
    __inout                                         OEM_TEE_KEY                *f_pCK )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_BYTE   rgbKey[2 * DRM_AES_KEYSIZE_128];  /* Do not zero-initialize key material */

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );
    DRMASSERT( f_pCI != NULL );
    DRMASSERT( f_pCK != NULL );

    ChkVOID( OEM_TEE_MEMCPY( rgbKey, f_pbCiphertext, sizeof( rgbKey ) ) );

    ChkDR( OEM_TEE_CRYPTO_AES128_EcbEncryptData(
        f_pContext,
        f_pUplinkxKey,
        rgbKey,
        sizeof(rgbKey) ) );

    ChkDR( OEM_TEE_CRYPTO_AES128_EcbEncryptData(
        f_pContext,
        f_pSecondaryKey,
        rgbKey,
        sizeof(rgbKey) ) );

    ChkDR( OEM_TEE_CRYPTO_AES128_SetKey( f_pContext, f_pCI, &rgbKey[ 0 ] ) );
    ChkDR( OEM_TEE_CRYPTO_AES128_SetKey( f_pContext, f_pCK, &rgbKey[ DRM_AES_KEYSIZE_128 ] ) );

ErrorExit:
    return dr;
}
#endif

EXIT_PK_NAMESPACE_CODE;

PREFAST_POP; /* __WARNING_NONCONST_PARAM_25004 */

