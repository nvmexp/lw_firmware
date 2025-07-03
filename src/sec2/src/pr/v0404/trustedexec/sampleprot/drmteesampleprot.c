/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <drmtee.h>
#include <drmteesupported.h>
#include <drmteebase.h>
#include <drmteexb.h>
#include <drmteekbcryptodata.h>
#include <drmerr.h>
#include <oemtee.h>
#include <drmbcertformatparser.h>
#include <oembyteorder.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;
#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_TEE_IMPL_SAMPLEPROT_IsSAMPLEPROTSupported( DRM_VOID )
{
    return TRUE;
}

/*
** Synopsis:
**
** This function generates a random AES key to be used for sample
** protection. It returns an escrowed key blob for use by the TEE
** and an encrypted key to be decrypted by the codec.
**
** Operations Performed:
**
** 1. Validate the codec-provided certificate and look for a public key having
**    the DRM_BCERT_KEYUSAGE_ENCRYPTKEY_SAMPLE_PROTECTION_AES128CTR usage.
** 2. Generate a random AES key.
** 3. Build a SPKB containing the randomly generated sample protection key.
** 4. ECC encrypt the AES key using the codec certificate public key.
** 5. Return both the SPKB and encrypted key.
**
** Arguments:
**
** f_pContext:       (in/out) The TEE context returned from
**                            DRM_TEE_BASE_AllocTEEContext.
** f_pCertificate:   (in)     The codec sample protection certificate.
** f_pRKB:           (in)     The revocation blob returned from
**                            DRM_TEE_REVOCATION_IngestRevocationInfo.
** f_pSPKB:          (out)    The Sample Protection Key Blob (SPKB) used
**                            to apply sample protection after content
**                            decryption in the TEE.
** f_pEncryptedKey:  (out)    The encrypted key used by the codec to
**                            remove sample protection.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_SAMPLEPROT_PrepareSampleProtectionKey(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pCertificate,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pRKB,
    __out                       DRM_TEE_BYTE_BLOB            *f_pSPKB,
    __out                       DRM_TEE_BYTE_BLOB            *f_pEncryptedKey )
{
    DRM_RESULT                         dr                   = DRM_SUCCESS;
    DRM_RESULT                         drTmp                = DRM_SUCCESS;
    DRM_TEE_KEY                        oSPKey               = DRM_TEE_KEY_EMPTY;
    CIPHERTEXT_P256                    oSPKeyEncrypted;     /* OEM_TEE_ZERO_MEMORY */
    DRM_TEE_RKB_CONTEXT                oRKBCtx;             /* Initialized in DRM_TEE_IMPL_KB_ParseAndVerifyRKB */
    DRM_TEE_BYTE_BLOB                  oSPKBEncrypted       = DRM_TEE_BYTE_BLOB_EMPTY;
    DRM_TEE_BYTE_BLOB                  oSPKB                = DRM_TEE_BYTE_BLOB_EMPTY;
    OEM_TEE_CONTEXT                   *pOemTeeCtx           = NULL;
    DRM_BCERTFORMAT_PARSER_CHAIN_DATA *pChainData           = NULL;

    ChkVOID( OEM_TEE_ZERO_MEMORY( &oSPKeyEncrypted, sizeof( oSPKeyEncrypted ) ) );

    ChkArg( f_pContext      != NULL );
    ChkArg( f_pCertificate  != NULL );
    ChkArg( f_pSPKB         != NULL );
    ChkArg( f_pEncryptedKey != NULL );

    pOemTeeCtx = &f_pContext->oContext;

    {
        DRMFILETIME oLwrrentTime = {0};

        ChkDRAllowENOTIMPL( OEM_TEE_CLOCK_GetSelwrelySetSystemTime( pOemTeeCtx, &oLwrrentTime ) );

        ChkBOOL( IsBlobAssigned( f_pRKB ), DRM_E_RIV_TOO_SMALL );

        ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, sizeof(*pChainData), ( DRM_VOID ** )&pChainData ) );

        /* Parse the certificate */
        ChkDR( DRM_TEE_IMPL_BASE_ParseCertificateChain(
            pOemTeeCtx,
            &oLwrrentTime,                          /* f_pftLwrrentTime                 */
            DRM_BCERT_CERTTYPE_APPLICATION,         /* f_dwExpectedLeafCertType         */
            f_pCertificate->cb,
            f_pCertificate->pb,
            pChainData ) );

        AssertChkBOOL( pChainData->dwLeafSelwrityLevel <= DRM_BCERT_SELWRITYLEVEL_2000 );
        ChkBOOL( DRM_BCERTFORMAT_PARSER_CHAIN_DATA_IS_KEY_VALID( pChainData->dwValidKeyMask, DRM_BCERTFORMAT_CHAIN_DATA_KEY__SAMPLEPROT ), DRM_E_BCERT_NO_PUBKEY_WITH_REQUESTED_KEYUSAGE );
    }

    ChkDR( DRM_TEE_IMPL_KB_ParseAndVerifyRKB(
        f_pContext,
        f_pRKB,
        &oRKBCtx,
        pChainData->cDigests,
        (const DRM_RevocationEntry *)&pChainData->rgoDigests[0] ) );

    ChkDR( DRM_TEE_IMPL_BASE_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_SAMPLEPROT, &oSPKey ) );

    /* Generate the session key */
    ChkDR( OEM_TEE_BASE_GenerateRandomAES128Key( pOemTeeCtx, &oSPKey.oKey ) );

    ChkDR( OEM_TEE_BASE_ECC256_Encrypt_AES128Keys( pOemTeeCtx, &pChainData->rgoLeafPubKeys[DRM_BCERTFORMAT_CHAIN_DATA_KEY__SAMPLEPROT], NULL, &oSPKey.oKey, &oSPKeyEncrypted ) );
    ChkDR( DRM_TEE_IMPL_BASE_AllocBlob( f_pContext, DRM_TEE_BLOB_ALLOC_BEHAVIOR_COPY, ECC_P256_CIPHERTEXT_SIZE_IN_BYTES, &oSPKeyEncrypted.m_rgbCiphertext[0], &oSPKBEncrypted ) );

    ChkDR( DRM_TEE_IMPL_KB_BuildSPKB(
        f_pContext,
        &oSPKey,
        pChainData->dwLeafSelwrityLevel,
        pChainData->cDigests,
        (const DRM_RevocationEntry *)&pChainData->rgoDigests[0],
        oRKBCtx.dwRIV,
        &oSPKB ) );

    ChkVOID( DRM_TEE_IMPL_BASE_TransferBlobOwnership( f_pEncryptedKey, &oSPKBEncrypted ) );
    ChkVOID( DRM_TEE_IMPL_BASE_TransferBlobOwnership( f_pSPKB, &oSPKB ) );

ErrorExit:
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, ( DRM_VOID ** )&pChainData ) );

    //
    // LWE (kwilson) - Can't use ChkDR within ErrorExit. Cache the return value in drTmp and check value
    // of dr, before overwriting to be sure we are not overwriting a previous failure with success.
    //
    drTmp = DRM_TEE_IMPL_BASE_FreeBlob( f_pContext, &oSPKBEncrypted );
    if (dr == DRM_SUCCESS)
    {
        dr = drTmp;
    }
    drTmp = DRM_TEE_IMPL_BASE_FreeBlob( f_pContext, &oSPKB );
    if (dr == DRM_SUCCESS)
    {
        dr = drTmp;
    }

    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &oSPKey.oKey ) );
    return dr;
}

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_TEE_IMPL_SAMPLEPROT_IsDecryptionModeSupported(
    __inout                     OEM_TEE_CONTEXT              *f_pOemTeeCtx,
    __in                        DRM_DWORD                     f_dwMode )
{
    DRM_RESULT dr                   = DRM_SUCCESS;
    DRM_BOOL   fResult              = FALSE;
    DRM_DWORD  cchManufacturerName  = 0;
    DRM_WCHAR *pwszManufacturerName = NULL;
    DRM_DWORD  cchModelName         = 0;
    DRM_WCHAR *pwszModelName        = NULL;
    DRM_DWORD  cchModelNumber       = 0;
    DRM_WCHAR *pwszModelNumber      = NULL;
    DRM_DWORD  cbProperties         = 0;
    DRM_BYTE  *pbProperties         = NULL;
    DRM_DWORD  rgdwFunctionMap[DRM_TEE_METHOD_FUNCTION_MAP_COUNT] = {0};

    /* Always allow decryption to a handle. */
    if( f_dwMode == OEM_TEE_DECRYPTION_MODE_HANDLE )
    {
        fResult = TRUE;
        goto ErrorExit;
    }

    ChkDR( OEM_TEE_BASE_GetVersionInformation(
        f_pOemTeeCtx,
        &cchManufacturerName,
        &pwszManufacturerName,
        &cchModelName,
        &pwszModelName,
        &cchModelNumber,
        &pwszModelNumber,
        rgdwFunctionMap,
        &cbProperties,
        &pbProperties ) );

    if( DRM_TEE_PROPERTY_IS_SET( cbProperties, pbProperties, DRM_TEE_PROPERTY_REQUIRES_SAMPLE_PROTECTION ) )
    {
        /* If sample protection is enabled, then allow only sample protected output */
        fResult = (f_dwMode == OEM_TEE_DECRYPTION_MODE_SAMPLE_PROTECTION);
    }
    else
    {
        /* If sample protection is not enabled, then allow anything but sample protection */
        switch( f_dwMode )
        {
            case OEM_TEE_DECRYPTION_MODE_SAMPLE_PROTECTION:
                fResult = FALSE;
                break;
            case OEM_TEE_DECRYPTION_MODE_NOT_SELWRE:
            case OEM_TEE_DECRYPTION_MODE_HANDLE:
                fResult = TRUE;
                break;
            default:
                AssertChkBOOL(FALSE);
                break;
        }
    }

ErrorExit:
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( f_pOemTeeCtx, ( DRM_VOID ** )&pwszManufacturerName ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( f_pOemTeeCtx, ( DRM_VOID ** )&pwszModelName ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( f_pOemTeeCtx, ( DRM_VOID ** )&pwszModelNumber ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( f_pOemTeeCtx, ( DRM_VOID ** )&pbProperties ) );

    if( DRM_FAILED(dr) )
    {
        fResult = FALSE;
    }

    return fResult;
}
#endif

#ifdef NONE
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_SAMPLEPROT_ApplySampleProtection(
    __inout                                         OEM_TEE_CONTEXT        *f_pContext,
    __in                                      const OEM_TEE_KEY            *f_pSPK,
    __in                                            DRM_UINT64              f_ui64Initializatiolwector,
    __in                                            DRM_DWORD               f_cbClearToSampleProtected,
    __inout_bcount( f_cbClearToSampleProtected )    DRM_BYTE               *f_pbClearToSampleProtected )
{
    return OEM_TEE_SAMPLEPROT_ApplySampleProtection(
        f_pContext,
        f_pSPK,
        f_ui64Initializatiolwector,
        f_cbClearToSampleProtected,
        f_pbClearToSampleProtected );
}
#endif
EXIT_PK_NAMESPACE_CODE;
