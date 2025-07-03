/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <drmtee.h>
#include <drmteesupported.h>
#include <drmteexb.h>
#include <drmteekbcryptodata.h>
#include <drmbcertformatparser.h>
#include <drmbcertformatbuilder.h>
#include <drmmathsafe.h>
#include <drmteebase.h>
#include <oemtee.h>
#include <oemteecryptointernaltypes.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;
#if !defined (SEC_USE_CERT_TEMPLATE) && defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_TEE_LPROV_IMPL_IsLPROVSupported( DRM_VOID )
{
    return TRUE;
}

PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_USING_FAILED_RESULT_6102, "Ignore Using Failed Result warning since out parameters are all initialized." );

#define DRM_TEE_BCERT_MAX_MANUFACTURER_STRING_LENGTH_IN_WCHARS   (DRM_BCERT_MAX_MANUFACTURER_STRING_LENGTH / sizeof( DRM_WCHAR ))

#define DRM_TEE_BCERT_KEY_IDX_SIGNING         0
#define DRM_TEE_BCERT_KEY_IDX_ENCRYPTION      1
#define DRM_TEE_BCERT_KEY_IDX_PRND_ENCRYPTION 2

static const DRM_DWORD c_rgdwBCERTKeyUsages[] = { DRM_BCERT_KEYUSAGE_SIGN, DRM_BCERT_KEYUSAGE_ENCRYPT_KEY, DRM_BCERT_KEYUSAGE_PRND_ENCRYPT_KEY };

// LWE (nkuo) - changed due to compile error "missing braces around initializer"
static const DRM_ID g_idKeyDerivationCertificatePrivateKeysWrap = { { 0x9c, 0xe9, 0x34, 0x32, 0xc7, 0xd7, 0x40, 0x16, 0xba, 0x68, 0x47, 0x63, 0xf8, 0x01, 0xe1, 0x36 } };

static const DRM_TEE_KEY_TYPE    g_rgeKeyTypes[]  = { DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_SIGN, DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_ENCRYPT, DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_PRND };

static DRM_NO_INLINE DRM_RESULT DRM_CALL _GenerateNewECCKeys(
    __inout                         DRM_TEE_CONTEXT             *f_pContext,
    __in                            DRM_DWORD                    f_cECCKeys,
    __out_ecount( f_cECCKeys )      DRM_TEE_KEY                 *f_pECCKeys,
    __out_ecount( f_cECCKeys )      PUBKEY_P256                 *f_pPubKeys );
static DRM_NO_INLINE DRM_RESULT DRM_CALL _GenerateNewECCKeys(
    __inout                         DRM_TEE_CONTEXT             *f_pContext,
    __in                            DRM_DWORD                    f_cECCKeys,
    __out_ecount( f_cECCKeys )      DRM_TEE_KEY                 *f_pECCKeys,
    __out_ecount( f_cECCKeys )      PUBKEY_P256                 *f_pPubKeys )
{
    DRM_RESULT                       dr             = DRM_SUCCESS;
    DRM_DWORD                        iKey           = 0;
    OEM_TEE_CONTEXT                 *pOemTeeCtx     = NULL;

    AssertChkArg( f_cECCKeys <= DRM_NO_OF( g_rgeKeyTypes ) );

    DRMASSERT( f_pContext != NULL );
    DRMASSERT( f_pECCKeys != NULL );
    DRMASSERT( f_pPubKeys != NULL );

    pOemTeeCtx = &f_pContext->oContext;

    for( iKey = 0; iKey < f_cECCKeys; iKey++ )
    {
        ChkDR( DRM_TEE_BASE_IMPL_AllocKeyECC256( pOemTeeCtx, g_rgeKeyTypes[ iKey ], &f_pECCKeys[ iKey ] ) );
        ChkDR( OEM_TEE_PROV_GenerateRandomECC256KeyPair( pOemTeeCtx, &f_pECCKeys[iKey].oKey, &f_pPubKeys[iKey] ) );
    }

ErrorExit:
    /* f_pECCKeys is caller-allocated and the caller will clean them up on failure */
    return dr;
}

/*
** This function attempts to call OEM_TEE_LPROV_UnprotectModelCertificatePrivateSigningKey.
** If that function returns DRM_E_NOTIMPL, the default implementation is used.
** Refer to the OEM_TEE_LPROV_UnprotectModelCertificatePrivateSigningKey documentation
** for the details for this default implementation.
*/
static DRM_NO_INLINE DRM_RESULT DRM_CALL _UnprotectModelPrivateSigningKey(
    __inout                             OEM_TEE_CONTEXT            *f_pContext,
    __in                          const DRM_TEE_BYTE_BLOB          *f_pOEMProtectedCertificatePrivateKeys,
    __inout                             OEM_TEE_KEY                *f_pModelCertPrivateKey )
{
    DRM_RESULT      dr               = DRM_SUCCESS;
    DRM_TEE_KEY     oCTK             = DRM_TEE_KEY_EMPTY;
    DRM_TEE_KEY     oDerivedKey      = DRM_TEE_KEY_EMPTY;
    const DRM_BYTE *pbWrappedKey     = NULL;
    DRM_DWORD       cbWrappedKey     = 0;
    DRM_DWORD       ibWrappedKey     = 0;
    DRM_DWORD       cbDefaultPrivKey = 0;
    DRM_BYTE       *pbDefaultPrivKey = NULL;

    DRMASSERT( f_pOEMProtectedCertificatePrivateKeys != NULL );
    DRMASSERT( f_pModelCertPrivateKey != NULL );
    DRMASSERT( f_pModelCertPrivateKey->pKey != NULL );

    dr = OEM_TEE_LPROV_UnprotectModelCertificatePrivateSigningKey(
        f_pContext,
        f_pOEMProtectedCertificatePrivateKeys->cb,
        f_pOEMProtectedCertificatePrivateKeys->pb,
        &cbDefaultPrivKey,
        &pbDefaultPrivKey,
        f_pModelCertPrivateKey );
    if( dr == DRM_E_NOTIMPL )
    {
        /* Use the default implementation */
        dr = DRM_SUCCESS;

        AssertChkArg( ( cbDefaultPrivKey == 0 ) == ( pbDefaultPrivKey == NULL ) );

        if( cbDefaultPrivKey > 0 )
        {
            /*
            ** The TEE has the private key inside it (as opposed to it being passed in),
            ** but it's wrapped by the default protection mechanism.
            ** We will unwrap it so the OEM doesn't have to reimplement that functionality.
            */
            pbWrappedKey = pbDefaultPrivKey;
            cbWrappedKey = cbDefaultPrivKey;
        }
        else
        {
            /* The TEE doesn't have the private key inside it, so it MUST have been passed in. */
            ChkArg( IsBlobAssigned( f_pOEMProtectedCertificatePrivateKeys ) );
            pbWrappedKey = f_pOEMProtectedCertificatePrivateKeys->pb;
            cbWrappedKey = f_pOEMProtectedCertificatePrivateKeys->cb;
        }

        ChkDR( DRM_TEE_BASE_IMPL_AllocKeyAES128( f_pContext, DRM_TEE_KEY_TYPE_TK, &oCTK ) );
        ChkDR( DRM_TEE_BASE_IMPL_AllocKeyAES128( f_pContext, DRM_TEE_KEY_TYPE_TKD, &oDerivedKey ) );

        /* Derive the wrapping key from the CTK */
        {
            DRM_DWORD dwidCTK;  /* initialized by OEM_TEE_BASE_GetCTKID */
            ChkDR( OEM_TEE_BASE_GetCTKID( f_pContext, &dwidCTK ) );
            ChkDR( OEM_TEE_BASE_GetTKByID( f_pContext, dwidCTK, &oCTK.oKey ) );
        }
        ChkDR( OEM_TEE_BASE_DeriveKey( f_pContext, &oCTK.oKey, &g_idKeyDerivationCertificatePrivateKeysWrap, NULL, &oDerivedKey.oKey ) );

        /* Unwrap the ECC private key */
        ChkDR( OEM_TEE_BASE_UnwrapECC256KeyFromPersistedStorage( f_pContext, &oDerivedKey.oKey, &cbWrappedKey, &ibWrappedKey, pbWrappedKey, f_pModelCertPrivateKey ) );
    }
    else
    {
        ChkDR( dr );
        AssertChkArg( ( cbDefaultPrivKey == 0 ) && ( pbDefaultPrivKey == NULL ) );  /* The OEM should NEVER return success and populate these out params. */
    }

ErrorExit:
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( f_pContext, &oCTK.oKey ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( f_pContext, &oDerivedKey.oKey ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( f_pContext, (DRM_VOID**)&pbDefaultPrivKey ) );
    return dr;
}

static DRM_NO_INLINE DRM_RESULT DRM_CALL _SetManufacturingStrings(
    __inout                             OEM_TEE_CONTEXT                   *f_pOemTeeCtx,
    __inout                             DRM_BCERTFORMAT_BUILDER_CONTEXT   *f_pBuilderCtx,
    __in                          const DRM_BCERTFORMAT_PARSER_CHAIN_DATA *f_pChainData )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_WCHAR  rgwchOemModelInfo[DRM_TEE_BCERT_MAX_MANUFACTURER_STRING_LENGTH_IN_WCHARS + 1]   = {0};
    DRM_DWORD  cchOemModelInfo                                                                 = DRM_NO_OF(rgwchOemModelInfo);

    /* Fill-in the manufacturer strings */
    dr = OEM_TEE_LPROV_GetDeviceModelInfo( f_pOemTeeCtx, &cchOemModelInfo, rgwchOemModelInfo );
    if( dr != DRM_E_NOTIMPL )
    {
        ChkDR( dr );
        ChkBOOL( cchOemModelInfo <= DRM_TEE_BCERT_MAX_MANUFACTURER_STRING_LENGTH_IN_WCHARS, DRM_E_BCERT_MANUFACTURER_STRING_TOO_LONG );

        ChkDR( DRM_BCERTFORMAT_SetModelName(
            f_pBuilderCtx,
            cchOemModelInfo * sizeof(DRM_WCHAR),
            ( DRM_BYTE * )rgwchOemModelInfo ) );
    }
    else
    {
        /*
        ** If the device model info is not explicitly provided by the OEM, try to
        ** propagate the manufacturer strings from the parent certificate, if any.
        */
        if( f_pChainData->oManufacturingName.pbString != NULL )
        {
            ChkDR( DRM_BCERTFORMAT_SetModelName(
                f_pBuilderCtx,
                f_pChainData->oManufacturingName.cbString,
                f_pChainData->oManufacturingName.pbString ) );
        }
        if( f_pChainData->oManufacturingModel.pbString != NULL )
        {
            ChkDR( DRM_BCERTFORMAT_SetManufacturerName(
                f_pBuilderCtx,
                f_pChainData->oManufacturingModel.cbString,
                f_pChainData->oManufacturingModel.pbString ) );
        }
        if( f_pChainData->oManufacturingModel.pbString != NULL )
        {
            ChkDR( DRM_BCERTFORMAT_SetModelNumber(
                f_pBuilderCtx,
                f_pChainData->oManufacturingNumber.cbString,
                f_pChainData->oManufacturingNumber.pbString ) );
        }
    }

ErrorExit:
    return dr;
}

static DRM_NO_INLINE DRM_RESULT DRM_CALL _BuildPlayReadyCertificateChain(
    __inout                                     DRM_TEE_CONTEXT                     *f_pContext,
    __inout                                     DRM_STACK_ALLOCATOR_CONTEXT         *f_poBuilderStack,
    __in                                  const DRM_BCERTFORMAT_PARSER_CHAIN_DATA   *f_pChainData,
    __in                                        DRM_DWORD                            f_cbCertChain,
    __in                                  const DRM_BYTE                            *f_pbCertChain,
    __in                                  const DRM_TEE_BYTE_BLOB                   *f_pOEMProtectedModelCertPrivateKey,
    __in                                  const DRM_ID                              *f_pidApplication,
    __in                                        DRM_DWORD                            f_cECCKeys,
    __in_ecount( f_cECCKeys )             const DRM_TEE_KEY                         *f_pECCKeys,
    __in_ecount( f_cECCKeys )             const PUBKEY_P256                         *f_pPubKeys,
    __out                                       DRM_DWORD                           *f_pcbCertificate,
    __deref_out_bcount( *f_pcbCertificate )     DRM_BYTE                           **f_ppbCertificate );
static DRM_NO_INLINE DRM_RESULT DRM_CALL _BuildPlayReadyCertificateChain(
    __inout                                     DRM_TEE_CONTEXT                     *f_pContext,
    __inout                                     DRM_STACK_ALLOCATOR_CONTEXT         *f_poBuilderStack,
    __in                                  const DRM_BCERTFORMAT_PARSER_CHAIN_DATA   *f_pChainData,
    __in                                        DRM_DWORD                            f_cbCertChain,
    __in                                  const DRM_BYTE                            *f_pbCertChain,
    __in                                  const DRM_TEE_BYTE_BLOB                   *f_pOEMProtectedModelCertPrivateKey,
    __in                                  const DRM_ID                              *f_pidApplication,
    __in                                        DRM_DWORD                            f_cECCKeys,
    __in_ecount( f_cECCKeys )             const DRM_TEE_KEY                         *f_pECCKeys,
    __in_ecount( f_cECCKeys )             const PUBKEY_P256                         *f_pPubKeys,
    __out                                       DRM_DWORD                           *f_pcbCertificate,
    __deref_out_bcount( *f_pcbCertificate )     DRM_BYTE                           **f_ppbCertificate ) 
{
    DRM_RESULT                           dr                                                             = DRM_SUCCESS;
    DRM_DWORD                            rgdwCertFeatureSet[ DRM_BCERT_MAX_FEATURES ]                   = { 0 };    /* Note: Must remain in scope until certificate is fully built */
    DRM_BCERTFORMAT_BUILDER_KEYDATA     *pCertKeys                                                      = NULL;
    DRM_DWORD                            cbCertificateAllocated                                         = 0;
    DRM_BYTE                            *pbCertificateAllocated                                         = NULL;
    OEM_TEE_CONTEXT                     *pOemTeeCtx                                                     = NULL;
    DRM_BCERTFORMAT_BUILDER_CONTEXT     *pBuilderCtx                                                    = NULL;
    DRM_TEE_KEY                          oModelPrivKey                                                  = DRM_TEE_KEY_EMPTY;
    DRM_BCERTFORMAT_CERT_DEVICE_DATA     oDeviceCertData                                                = {0};

    /* Arguments are checked in the PRITEE proxy layer */
    DRMASSERT( f_pContext                         != NULL );
    DRMASSERT( f_pOEMProtectedModelCertPrivateKey != NULL );
    DRMASSERT( f_pECCKeys                         != NULL );
    DRMASSERT( f_pPubKeys                         != NULL );
    DRMASSERT( f_pcbCertificate                   != NULL );
    DRMASSERT( f_ppbCertificate                   != NULL );

    ChkArg( f_pChainData != NULL );

    pOemTeeCtx = &f_pContext->oContext;

    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, sizeof( *pBuilderCtx ), (DRM_VOID **)&pBuilderCtx ) );
    ChkVOID( OEM_TEE_ZERO_MEMORY( pBuilderCtx, sizeof( *pBuilderCtx ) ) );

    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, DRM_BINARY_DEVICE_CERT_MAX_KEYUSAGES * sizeof( *pCertKeys ), (DRM_VOID **)&pCertKeys ) );
    ChkVOID( OEM_TEE_ZERO_MEMORY( pCertKeys, DRM_BINARY_DEVICE_CERT_MAX_KEYUSAGES * sizeof( *pCertKeys ) ) );

    /*
    ** Validate the security version and platform identifier (if necessary) with the expected OEM
    ** values.
    */
    {
        DRM_DWORD   dwSelwrityVersion   = 0;
        DRM_DWORD   dwPlatformID        = 0;
        ChkDR( OEM_TEE_LPROV_GetModelSelwrityVersion( pOemTeeCtx, &dwSelwrityVersion, &dwPlatformID ) );
        if( f_pChainData->dwSelwrityVersion != DRM_BCERT_SELWRITY_VERSION_UNSPECIFIED &&
            dwSelwrityVersion != OEM_TEE_LPROV_SELWRITY_VERSION_IGNORE )
        {
            ChkBOOL( f_pChainData->dwSelwrityVersion == dwSelwrityVersion, DRM_E_BCERT_ILWALID_SELWRITY_VERSION );
        }
        if( f_pChainData->dwPlatformID != DRM_BCERT_SELWRITY_VERSION_PLATFORM_UNSPECIFIED &&
            dwPlatformID != OEM_TEE_LPROV_PLATFORM_ID_IGNORE )
        {
            ChkBOOL( f_pChainData->dwPlatformID == dwPlatformID, DRM_E_BCERT_ILWALID_PLATFORM_IDENTIFIER );
        }
    }

    oDeviceCertData.cbMaxLicense    = DRM_BCERT_MAX_LICENSE_SIZE;
    oDeviceCertData.cbMaxHeader     = DRM_BCERT_MAX_HEADER_SIZE;
    oDeviceCertData.dwMaxChainDepth = DRM_BCERT_MAX_LICENSE_CHAIN_DEPTH;

    {
        DRM_ID  oCertID;        /* Initialized later to random value, gets copied into pBuilderCtx */
        DRM_ID  oCertClientID;  /* Initialized later, gets copied into pBuilderCtx */
        ChkDR( OEM_TEE_BASE_GenerateRandomBytes( pOemTeeCtx, sizeof( oCertID.rgb ), oCertID.rgb ) );     /* Generate a unique Identifier for this certificate. */

        ChkDR( DRM_TEE_BASE_IMPL_GetAppSpecificHWID( pOemTeeCtx, f_pidApplication, &oCertClientID ) );

        ChkDR( DRM_BCERTFORMAT_StartDeviceCertificateChain(
            f_poBuilderStack,
            f_cbCertChain,
            f_pbCertChain,
            &oDeviceCertData,
            &oCertID,
            &oCertClientID,
            pBuilderCtx ) );
    }

    ChkDR( _SetManufacturingStrings( pOemTeeCtx, pBuilderCtx, f_pChainData ) );

    DRMCASSERT( DRM_BINARY_DEVICE_CERT_MAX_KEYUSAGES > DRM_TEE_BCERT_KEY_IDX_SIGNING );
    DRMCASSERT( DRM_BINARY_DEVICE_CERT_MAX_KEYUSAGES > DRM_TEE_BCERT_KEY_IDX_ENCRYPTION );
    DRMCASSERT( DRM_BINARY_DEVICE_CERT_MAX_KEYUSAGES > DRM_TEE_BCERT_KEY_IDX_PRND_ENCRYPTION );

    pCertKeys[DRM_TEE_BCERT_KEY_IDX_SIGNING].cKeyUsages       = 1;
    pCertKeys[DRM_TEE_BCERT_KEY_IDX_SIGNING].rgdwKeyUsages[0] = c_rgdwBCERTKeyUsages[DRM_TEE_BCERT_KEY_IDX_SIGNING];
    pCertKeys[DRM_TEE_BCERT_KEY_IDX_SIGNING].cbKey         = ECC_P256_PUBKEY_SIZE_IN_BYTES;
    pCertKeys[DRM_TEE_BCERT_KEY_IDX_SIGNING].wKeyType      = DRM_BCERT_KEYTYPE_ECC256;
    ChkVOID( OEM_TEE_MEMCPY( pCertKeys[DRM_TEE_BCERT_KEY_IDX_SIGNING].rgbKey, ( DRM_BYTE * )&f_pPubKeys[DRM_TEE_BCERT_KEY_IDX_SIGNING], ECC_P256_PUBKEY_SIZE_IN_BYTES ) );

    pCertKeys[DRM_TEE_BCERT_KEY_IDX_ENCRYPTION].cKeyUsages       = 1;
    pCertKeys[DRM_TEE_BCERT_KEY_IDX_ENCRYPTION].rgdwKeyUsages[0] = c_rgdwBCERTKeyUsages[DRM_TEE_BCERT_KEY_IDX_ENCRYPTION];
    pCertKeys[DRM_TEE_BCERT_KEY_IDX_ENCRYPTION].cbKey            = ECC_P256_PUBKEY_SIZE_IN_BYTES;
    pCertKeys[DRM_TEE_BCERT_KEY_IDX_ENCRYPTION].wKeyType         = DRM_BCERT_KEYTYPE_ECC256;
    ChkVOID( OEM_TEE_MEMCPY( pCertKeys[DRM_TEE_BCERT_KEY_IDX_ENCRYPTION].rgbKey, ( DRM_BYTE * )&f_pPubKeys[DRM_TEE_BCERT_KEY_IDX_ENCRYPTION], ECC_P256_PUBKEY_SIZE_IN_BYTES ) );

    if( DRM_BCERT_IS_FEATURE_SUPPORTED( f_pChainData->dwLeafFeatureSet, DRM_BCERT_FEATURE_RECEIVER ) )
    {
        pCertKeys[DRM_TEE_BCERT_KEY_IDX_PRND_ENCRYPTION].cKeyUsages       = 1;
        pCertKeys[DRM_TEE_BCERT_KEY_IDX_PRND_ENCRYPTION].rgdwKeyUsages[0] = c_rgdwBCERTKeyUsages[DRM_TEE_BCERT_KEY_IDX_PRND_ENCRYPTION];
        pCertKeys[DRM_TEE_BCERT_KEY_IDX_PRND_ENCRYPTION].cbKey            = ECC_P256_PUBKEY_SIZE_IN_BYTES;
        pCertKeys[DRM_TEE_BCERT_KEY_IDX_PRND_ENCRYPTION].wKeyType         = DRM_BCERT_KEYTYPE_ECC256;
        ChkVOID( OEM_TEE_MEMCPY( pCertKeys[DRM_TEE_BCERT_KEY_IDX_PRND_ENCRYPTION].rgbKey, ( DRM_BYTE * )&f_pPubKeys[DRM_TEE_BCERT_KEY_IDX_PRND_ENCRYPTION], ECC_P256_PUBKEY_SIZE_IN_BYTES ) );
    }

    ChkDR( DRM_BCERTFORMAT_SetKeyInfo( pBuilderCtx, f_cECCKeys, pCertKeys ) );

    {
        DRM_DWORD   cFeatureEntries = 0;
        DRM_DWORD   iFeature;   /* loop variable */
        for( iFeature = 0; iFeature < DRM_BCERT_MAX_FEATURES; iFeature++ )
        {
            if( DRM_BCERT_IS_FEATURE_SUPPORTED( f_pChainData->dwLeafFeatureSet, iFeature ) )
            {
                rgdwCertFeatureSet[ cFeatureEntries++ ] = iFeature;
            }
        }
        ChkDR( DRM_BCERTFORMAT_SetFeatures( pBuilderCtx, cFeatureEntries, rgdwCertFeatureSet ) );
    }
    ChkDR( DRM_BCERTFORMAT_SetSelwrityLevel( pBuilderCtx, f_pChainData->dwLeafSelwrityLevel ) );

    /*
    ** Use the security version from the model certificate chain in this leaf certificate.  The chain
    ** platform ID has already been checked for consistency.
    */
    ChkDR( DRM_BCERTFORMAT_SetSelwrityVersion(
        pBuilderCtx,
        f_pChainData->dwPlatformID,
        f_pChainData->dwSelwrityVersion ) );

    ChkDR( DRM_TEE_BASE_IMPL_AllocKeyECC256( pOemTeeCtx, DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_SIGN, &oModelPrivKey ) );

    ChkDR( _UnprotectModelPrivateSigningKey(
        pOemTeeCtx,
        f_pOEMProtectedModelCertPrivateKey,
        &oModelPrivKey.oKey ) );

    /* Validate the correct public key exists */
    ChkBOOL( DRM_BCERTFORMAT_PARSER_CHAIN_DATA_IS_KEY_VALID( f_pChainData->dwValidKeyMask, DRM_BCERTFORMAT_CHAIN_DATA_KEY__SIGN ), DRM_E_BCERT_ILWALID_KEY_USAGE );

    /* First call callwlates the size of the certificate buffer */
    dr = DRM_BCERTFORMAT_FinishCertificateChain(
        pBuilderCtx,
        pOemTeeCtx,
        OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_ECC256_TO_BROKER( &oModelPrivKey.oKey ),
        &f_pChainData->rgoLeafPubKeys[DRM_BCERTFORMAT_CHAIN_DATA_KEY__SIGN],
        &cbCertificateAllocated,
        NULL,
        NULL );
    DRM_REQUIRE_BUFFER_TOO_SMALL( dr );

    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, cbCertificateAllocated, ( DRM_VOID ** )&pbCertificateAllocated ) );

    /* Second call serializes the certificate chain */
    ChkDR( DRM_BCERTFORMAT_FinishCertificateChain(
        pBuilderCtx,
        pOemTeeCtx,
        OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_ECC256_TO_BROKER( &oModelPrivKey.oKey ),
        &f_pChainData->rgoLeafPubKeys[DRM_BCERTFORMAT_CHAIN_DATA_KEY__SIGN],
        &cbCertificateAllocated,
        pbCertificateAllocated,
        NULL ) );

    *f_pcbCertificate = cbCertificateAllocated;
    *f_ppbCertificate = pbCertificateAllocated;
    pbCertificateAllocated = NULL;

ErrorExit:
    ChkVOID( OEM_TEE_BASE_FreeKeyECC256( pOemTeeCtx, &oModelPrivKey.oKey ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID **)&pbCertificateAllocated ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID **)&pBuilderCtx ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID **)&pCertKeys ) );
    return dr;
}

/*
** This function attempts to call OEM_TEE_LPROV_UnprotectLeafCertificatePrivateKeys.
** If that function returns DRM_E_NOTIMPL, the default implementation is used.
** Refer to the OEM_TEE_LPROV_UnprotectLeafCertificatePrivateKeys documentation
** for the details for this default implementation.
*/
static DRM_NO_INLINE DRM_RESULT DRM_CALL _UnprotectLeafPrivateKeys(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pOEMProtectedCertificatePrivateKeys,
    __in                                                  DRM_DWORD                   f_cLeafCertPrivateKeys,
    __deref_inout_ecount( f_cLeafCertPrivateKeys )        DRM_TEE_KEY               **f_ppLeafCertPrivateKeys )
{
    DRM_RESULT           dr                = DRM_SUCCESS;
    DRM_DWORD            cbDefaultPrivKeys = 0;
    DRM_BYTE            *pbDefaultPrivKeys = NULL;
    OEM_TEE_CONTEXT     *pOemTeeCtx        = NULL;
    DRM_TEE_BYTE_BLOB    oDefaultPrivKeys  = DRM_TEE_BYTE_BLOB_EMPTY;
    DRM_DWORD            cDefaultPrivKeys  = 0;
    DRM_TEE_KEY         *pDefaultPrivKeys  = NULL;
    DRM_DWORD            iKey;             /* loop var */

    pOemTeeCtx = &f_pContext->oContext;

    DRMASSERT( f_pOEMProtectedCertificatePrivateKeys != NULL );
    DRMASSERT( f_cLeafCertPrivateKeys == 2 || f_cLeafCertPrivateKeys == 3 );
    DRMASSERT( f_ppLeafCertPrivateKeys != NULL );
    DRMASSERT( *f_ppLeafCertPrivateKeys != NULL );

    for( iKey = 0; iKey < f_cLeafCertPrivateKeys; iKey++ )
    {
        ChkDR( DRM_TEE_BASE_IMPL_AllocKeyECC256( pOemTeeCtx, g_rgeKeyTypes[ iKey ], &( *f_ppLeafCertPrivateKeys )[ iKey ] ) );
    }

    dr = OEM_TEE_LPROV_UnprotectLeafCertificatePrivateKeys(
        pOemTeeCtx,
        f_pOEMProtectedCertificatePrivateKeys->cb,
        f_pOEMProtectedCertificatePrivateKeys->pb,
        &cbDefaultPrivKeys,
        &pbDefaultPrivKeys,
        &( *f_ppLeafCertPrivateKeys )[ 0 ].oKey,
        &( *f_ppLeafCertPrivateKeys )[ 1 ].oKey,
        f_cLeafCertPrivateKeys == 3 ? &( *f_ppLeafCertPrivateKeys )[ 2 ].oKey : NULL );
    if( dr == DRM_E_NOTIMPL )
    {
        /* Use the default implementation. */
        dr = DRM_SUCCESS;

        /* Parsing the LPKB will allocate keys, so we don't need the caller-allocated keys */
        ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, f_cLeafCertPrivateKeys, f_ppLeafCertPrivateKeys ) );
        *f_ppLeafCertPrivateKeys = NULL;

        AssertChkArg( ( cbDefaultPrivKeys == 0 ) == ( pbDefaultPrivKeys == NULL ) );

        if( cbDefaultPrivKeys > 0 )
        {
            /*
            ** The TEE has the private keys inside it (as opposed to it being passed in).
            ** They're wrapped by the default protection mechanism (i.e. as a LPKB).
            ** We will unwrap them so the OEM doesn't have to reimplement that functionality.
            */
            ChkDR( DRM_TEE_BASE_AllocBlob( f_pContext, DRM_TEE_BLOB_ALLOC_BEHAVIOR_WEAKREF, cbDefaultPrivKeys, pbDefaultPrivKeys, &oDefaultPrivKeys ) );
            ChkDR( DRM_TEE_KB_ParseAndVerifyLPKB( f_pContext, &oDefaultPrivKeys, &cDefaultPrivKeys, &pDefaultPrivKeys ) );
        }
        else
        {
            /*
            ** The TEE doesn't have the private keys inside it, so they MUST have been passed in.
            ** They're wrapped by the default protection mechanism (i.e. as a LPKB).
            ** We will unwrap them so the OEM doesn't have to reimplement that functionality.
            */
            ChkArg( IsBlobAssigned( f_pOEMProtectedCertificatePrivateKeys ) );
            ChkDR( DRM_TEE_KB_ParseAndVerifyLPKB( f_pContext, f_pOEMProtectedCertificatePrivateKeys, &cDefaultPrivKeys, &pDefaultPrivKeys ) );
        }

        /* The LPKB must have had a PRND privkey if and only if the certificate has a PRND pubkey. */
        ChkBOOL( cDefaultPrivKeys == f_cLeafCertPrivateKeys, DRM_E_TEE_ILWALID_KEY_DATA );

        /* Now the caller will own pDefaultPrivKeys and free them instead of freeing the keys they allocated (which we freed above) */
        *f_ppLeafCertPrivateKeys = pDefaultPrivKeys;
        pDefaultPrivKeys = NULL;
    }
    else
    {
        ChkDR( dr );
        AssertChkArg( ( cbDefaultPrivKeys == 0 ) && ( pbDefaultPrivKeys == NULL ) );  /* The OEM should NEVER return success and populate these out params. */
    }

ErrorExit:
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&pbDefaultPrivKeys ) );
    if( pDefaultPrivKeys != NULL )
    {
        ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, cDefaultPrivKeys, &pDefaultPrivKeys ) );
    }
    return dr;
}

static DRM_NO_INLINE DRM_RESULT DRM_CALL _GenerateNewLeafCertificateKeysAndBuildNewLeafCertificate(
    __inout                       DRM_TEE_CONTEXT                   *f_pContext,
    __in                          DRM_DWORD                          f_cECCKeys,
    __in                          DRM_TEE_KEY                       *f_pECCKeys,
    __in_ecount(f_cECCKeys) const DRM_BCERTFORMAT_PARSER_CHAIN_DATA *f_pChainData,
    __in                          DRM_DWORD                          f_cbCertChain,
    __in                    const DRM_BYTE                          *f_pbCertChain,
    __in                    const DRM_TEE_BYTE_BLOB                 *f_pOEMProtectedCertificatePrivateKeys,
    __in                    const DRM_ID                            *f_pidApplication,
    __out                         DRM_TEE_BYTE_BLOB                 *f_pNewCertificate )
{
    DRM_RESULT                   dr                     = DRM_SUCCESS;
    OEM_TEE_CONTEXT             *pOemTeeCtx             = NULL;
    PUBKEY_P256                 *pPubKeys               = NULL;
    DRM_DWORD                    cbOutputCertificate    = 0;
    DRM_BYTE                    *pbOutputCertificate    = NULL;
    DRM_DWORD                    cbPubKeys              = 0;
    DRM_DWORD                    cbBuilderStack         = DRM_BCERTFORMAT_BUILDER_STACK_SIZE_MIN;
    DRM_STACK_ALLOCATOR_CONTEXT  oBuilderStack;         /* Initialized by DRM_STK_Init */
    DRM_BYTE                    *pbBuilderStack         = NULL;
    DRM_TEE_BYTE_BLOB            oOutputCertificate     = DRM_TEE_BYTE_BLOB_EMPTY;

    pOemTeeCtx = &f_pContext->oContext;

    cbPubKeys = f_cECCKeys * sizeof( PUBKEY_P256 );
    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, cbPubKeys, (DRM_VOID**)&pPubKeys ) );
    ChkDR( _GenerateNewECCKeys( f_pContext, f_cECCKeys, f_pECCKeys, pPubKeys ) );

    /*
    ** We are attempting to minimize the amount of memory used to build a certificate chain.
    ** We do this by allocating a relatively small amount of memory
    ** (DRM_BCERTFORMAT_BUILDER_STACK_SIZE_MIN) and attempting to build the chain.  This
    ** will fail with DRM_E_OUTOFMEMORY if there is not enough space.  If this happens, we
    ** double the amount of memory we allocated and try again.  We repeat this until the
    ** amount of memory we would allocate exceeds a value which should work for all valid
    ** certificates (DRM_BCERTFORMAT_STACK_SIZE_MAX). If we exceed that maximum, we fail
    ** parsing with DRM_E_OUTOFMEMORY.
    */
    do
    {
        ChkBOOL( cbBuilderStack <= DRM_BCERTFORMAT_PARSER_STACK_SIZE_MAX, DRM_E_OUTOFMEMORY );
        ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&pbBuilderStack ) );
        ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, cbBuilderStack, (DRM_VOID **)&pbBuilderStack ) );
        ChkDR( DRM_STK_Init( &oBuilderStack, pbBuilderStack, cbBuilderStack, TRUE ) );

        cbBuilderStack <<= 1;

        dr = _BuildPlayReadyCertificateChain(
            f_pContext,
            &oBuilderStack,
            f_pChainData,
            f_cbCertChain,
            f_pbCertChain,
            f_pOEMProtectedCertificatePrivateKeys,
            f_pidApplication,
            f_cECCKeys,
            f_pECCKeys,
            pPubKeys,
            &cbOutputCertificate,
            &pbOutputCertificate );

    } while( dr == DRM_E_OUTOFMEMORY );

    ChkDR( dr );
    ChkDR( DRM_TEE_BASE_IMPL_AllocBlobAndTakeOwnership( f_pContext, cbOutputCertificate, &pbOutputCertificate, &oOutputCertificate ) );
    ChkVOID( DRM_TEE_BASE_TransferBlobOwnership( f_pNewCertificate, &oOutputCertificate ) );

ErrorExit:
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID **)&pPubKeys ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID **)&pbBuilderStack ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID **)&pbOutputCertificate ) );
    return dr;
}

/*
** Synopsis:
**
** This function performs local provisioning.
**
** Operations Performed:
**
**  1. Verify the signature on the given DRM_TEE_CONTEXT.
**  2. If a previous PlayReady Private Key Blob (PPKB) was given,
**     parse the given previous PPKB and verify its signature.
**  3. Obtain a certificate chain by calling an OEM API.
**     This may be a certificate chain ending in a model or leaf certificate.
**  4. IF the certificate chain ends in a model certificate:
**     a. Generate two new ECC keypairs for signing and encryption.
**     b. If the given certificate chain supports being a PRND receiver,
**        generate a new ECC keypair for PRND encryption.
**     c. Obtain the model certificate's private signing key by calling an OEM API.
**        This key must be protected per the PlayReady Compliance and
**        Robustness Rules.
**     d. Generate a new PlayReady Device Certificate based on data inside the
**        inside the given model certificate chain and data provided
**        by OEM functions.  Include the newly generated public keys for
**        signing, encryption, and (optionally) PRND encryption.
**     e. Append the new certificate to the given model certificate chain.
**     f. Via an OEM API, sign the new PlayReady Device Certificate using the
**        model certificate's private signing key.
**     ELSE
**     g. Obtain the leaf certificate's private keys by calling an OEM API.
**        These keys must be protected per the PlayReady Compliance and
**        Robustness Rules.
**  5. If a previous PPKB was not given, generate a new Secure Store (SST) AES key.
**     Otherwise, obtain the SST AES key from the previous PPKB.
**  6. Build and sign a new PPKB.  Inside this PPKB, include the SST key and all leaf
**     certificate private keys (whether they were newly generated keys from
**     step 4.a and 4.b or existing keys from step 4.g).
**  7. Return the new PPKB and (complete) PlayReady Device Certificate Chain.
**
** This function is called inside Drm_Initialize when DRM_TEE_BASE_CheckDeviceKeys
** fails with DRM_E_TEE_PROVISIONING_REQUIRED.  This will be a rare oclwrrence,
** but it will happen at least once for a device that supports local provisioning
** (the first time Drm_Initialize is called).
**
** Arguments:
**
** f_pContext:                          (in/out) The TEE context returned from
**                                               DRM_TEE_BASE_AllocTEEContext.
** f_pPrevPPKB:                             (in) The previous PlayReady Private Key Blob (PPKB),
**                                               if any.
** f_pCertificate:                          (in) The model (group) certificate including
**                                               the complete chain by which it is signed
**                                               OR the leaf (device) certificate including
**                                               the complete chain by which it is signed.
**                                               Caller will get this from Oem_Device_GetCert.
**                                               This parameter may be ignored if the TEE
**                                               natively has access to the certificate.
** f_pOEMProtectedCertificatePrivateKeys:   (in) The model (group) certificate's private signing key
**                                               OR the leaf (device) certificate's private keys
**                                               returned from
**                                               Oem_Device_GetOEMProtectedPrivateKeys.
**                                               This parameter may be ignored if the TEE
**                                               natively has access to the private key(s).
**                                               These key(s) must be protected per the
**                                               PlayReady Compliance and Robustness Rules.
**                                               By default:
**                                               A model (group) certificate's private signing key
**                                               is protected by being directly AES128 key-wrapped
**                                               using the CTK and
**                                               g_idKeyDerivationCertificatePrivateKeysWrap
**                                               while a leaf (device) certificate's private keys
**                                               are protected as an LPKB.
** f_pidApplication:                        (in) An application id to be used as part of the HWID.
** f_pPPKB:                                (out) The new PlayReady Private Key Blob (PPKB).
**                                               This should be freed with DRM_TEE_BASE_FreeBlob.
** f_pNewCertificate:                      (out) The PlayReady client certificate (complete chain).
**                                               This should be freed with DRM_TEE_BASE_FreeBlob.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_LPROV_GenerateDeviceKeys(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pPrevPPKB,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pCertificate,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pOEMProtectedCertificatePrivateKeys,
    __in                  const DRM_ID                       *f_pidApplication,
    __out                       DRM_TEE_BYTE_BLOB            *f_pPPKB,
    __out                       DRM_TEE_BYTE_BLOB            *f_pNewCertificate )
{
    DRM_RESULT                         dr                    = DRM_SUCCESS;
    DRM_BOOL                           fModelExpected        = FALSE;
    DRM_DWORD                          cECCKeys              = 0;
    DRM_TEE_KEY                       *pECCKeys              = NULL;
    DRM_DWORD                          cbCertificate         = 0;
    DRM_BYTE                          *pbCertificate         = NULL;
    DRM_TEE_BYTE_BLOB                  oCertificate          = DRM_TEE_BYTE_BLOB_EMPTY;
    OEM_TEE_CONTEXT                   *pOemTeeCtx            = NULL;
    DRM_BCERTFORMAT_PARSER_CHAIN_DATA *pChainData            = NULL;
    DRM_TEE_BYTE_BLOB                  oOutputCertificate    = DRM_TEE_BYTE_BLOB_EMPTY;

    /* Arguments are checked in the PRITEE proxy layer */
    DRMASSERT( f_pContext                            != NULL );
    DRMASSERT( f_pPrevPPKB                           != NULL );
    DRMASSERT( f_pCertificate                        != NULL );
    DRMASSERT( f_pOEMProtectedCertificatePrivateKeys != NULL );
    DRMASSERT( f_pPPKB                               != NULL );
    DRMASSERT( f_pNewCertificate                     != NULL );

    pOemTeeCtx = &f_pContext->oContext;

    DRMASSERT( IsBlobConsistent( f_pCertificate ) );
    DRMASSERT( IsBlobConsistent( f_pPrevPPKB    ) );

    ChkVOID( OEM_TEE_ZERO_MEMORY( f_pPPKB, sizeof(*f_pPPKB) ) );
    ChkVOID( OEM_TEE_ZERO_MEMORY( f_pNewCertificate, sizeof(*f_pNewCertificate) ) );

    dr = OEM_TEE_LPROV_UnprotectCertificate( pOemTeeCtx, f_pCertificate->cb, f_pCertificate->pb, &cbCertificate, &pbCertificate );
    if( dr != DRM_E_NOTIMPL )
    {
        /* Use the returned model/leaf certificate instead of the given model/leaf certificate */
        ChkDR( dr );
        ChkDR( DRM_TEE_BASE_AllocBlob( f_pContext, DRM_TEE_BLOB_ALLOC_BEHAVIOR_WEAKREF, cbCertificate, pbCertificate, &oCertificate ) );
        f_pCertificate = &oCertificate;
    }
    else
    {
        /* Simply use the given model/leaf certificate */
        dr = DRM_SUCCESS;
    }

    fModelExpected = OEM_TEE_LPROV_IsModelCertificateExpected();

    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, sizeof(*pChainData), ( DRM_VOID ** )&pChainData ) );

    ChkDR( DRM_TEE_BASE_IMPL_ParseCertificateChain(
        pOemTeeCtx,
        NULL,                                                                                  /* f_pftLwrrentTime                 */
        (DRM_DWORD)( fModelExpected ? DRM_BCERT_CERTTYPE_ISSUER : DRM_BCERT_CERTTYPE_DEVICE ), /* f_dwExpectedLeafCertType         */
        f_pCertificate->cb,
        f_pCertificate->pb,
        pChainData ) );

    cECCKeys = 2;   /* MUST have at least PR ECC Encrypt, PR ECC Sign */

    if( DRM_BCERT_IS_FEATURE_SUPPORTED( pChainData->dwLeafFeatureSet, DRM_BCERT_FEATURE_RECEIVER ) )
    {
        cECCKeys++;
    }

    ChkDR( DRM_TEE_BASE_IMPL_AllocateKeyArray( f_pContext, cECCKeys, &pECCKeys ) );

    if( fModelExpected )
    {
        /* Group (model) certificate available. */
        ChkDR( _GenerateNewLeafCertificateKeysAndBuildNewLeafCertificate(
            f_pContext,
            cECCKeys,
            pECCKeys,
            pChainData,
            f_pCertificate->cb,
            f_pCertificate->pb,
            f_pOEMProtectedCertificatePrivateKeys,
            f_pidApplication,
            &oOutputCertificate ) );
    }
    else
    {
        /* 
        ** Leaf (device) certificate already available.  Use that certificate and fetch its private keys.
        ** ((1u << cECCKeys) - 1) creates the mask of the keys we expect.  This works because the first two keys are required
        ** and only the last (PRND) is optional.  Therefore we can either have a mask of 0x3 or 0x7.
        */
        ChkBOOL( pChainData->dwValidKeyMask == ((1u << cECCKeys) - 1), DRM_E_BCERT_NO_PUBKEY_WITH_REQUESTED_KEYUSAGE );
        ChkDR( DRM_TEE_BASE_AllocBlob( f_pContext, DRM_TEE_BLOB_ALLOC_BEHAVIOR_COPY, f_pCertificate->cb, f_pCertificate->pb, &oOutputCertificate ) );
        ChkDR( _UnprotectLeafPrivateKeys( f_pContext, f_pOEMProtectedCertificatePrivateKeys, cECCKeys, &pECCKeys ) );
    }

    ChkDR( DRM_TEE_KB_FinalizePPKB( f_pContext, cECCKeys, pECCKeys, f_pPrevPPKB, f_pPPKB ) );
    ChkVOID( DRM_TEE_BASE_TransferBlobOwnership( f_pNewCertificate, &oOutputCertificate ) );

ErrorExit:
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, ( DRM_VOID ** )&pChainData ) );
    ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, cECCKeys, &pECCKeys ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, ( DRM_VOID ** )&pbCertificate ) );
    ChkVOID( DRM_TEE_BASE_FreeBlob( f_pContext, &oOutputCertificate ) );
    ChkVOID( DRM_TEE_BASE_FreeBlob( f_pContext, &oCertificate ) );
    return dr;
}

PREFAST_POP;
#endif
EXIT_PK_NAMESPACE_CODE;
