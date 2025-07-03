/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <drmteekbcryptodata.h>
#include <drmteexb.h>
#include <drmtee.h>
#include <oemtee.h>
#include <drmlastinclude.h>
#include <drmmathsafe.h>
#include <drmsal.h>

ENTER_PK_NAMESPACE_CODE;
#if defined (SEC_COMPILE)

typedef struct __tagDRM_TEE_KB_SPKBData
{
    DRM_DWORD               dwSelwrityLevel;
    DRM_DWORD               cCertDigests;
    DRM_RevocationEntry     rgbCertDigests[DRM_BCERT_MAX_CERTS_PER_CHAIN];
    DRM_DWORD               dwRIV;
    OEM_WRAPPED_KEY_AES_128 oKey;
} DRM_TEE_KB_SPKBData;

typedef struct __tagDRM_TEE_KB_TPKBData
{
    DRM_BOOL                fKSessionInitialized;
    OEM_WRAPPED_KEY_AES_128 oKSession; /* RPROV lwrrently only uses 1 key, but the interface allows for more.  */
    OEM_WRAPPED_KEY_ECC_256 rgoPrivkeys[RPROV_KEYPAIR_COUNT]; /* TPKB has at most 3 keys: encrypt, sign, PRND */
    PUBKEY_P256             rgoPubkeys[RPROV_KEYPAIR_COUNT];
    DRM_BYTE                rgbChallengeGenerationNonce[OEM_PROVISIONING_NONCE_LENGTH]; /* nonce consumed at RPROV generate challenge call */
} DRM_TEE_KB_TPKBData;

typedef struct __tagDRM_TEE_KB_CDKB_PolicyRestriction
{
    DRM_WORD  wCategory;
    DRM_ID    idType;
    DRM_DWORD cbConfiguration;
    DRM_DWORD ibConfiguration;
} DRM_TEE_KB_CDKB_PolicyRestriction;

typedef struct __tagDRM_TEE_KB_CDKB_ContextData
{
    DRM_DWORD cbCopyOfUnprotectedOEMData;
    DRM_DWORD ibCopyOfUnprotectedOEMData;
} DRM_TEE_KB_CDKB_ContextData;

typedef struct __tagDRM_TEE_KB_CDKBData
{
    DRM_DWORD                           cbCDKBData;
    DRM_ID                              idSession;
    DRM_TEE_KB_CDKB_ContextData         oContext;
    DRM_KID                             oKID;
    DRM_LID                             oLID;
    DRM_DWORD                           dwDecryptionMode;
    DRM_WORD                            wSelwrityLevel;
    DRM_DWORD                           dwBeginDate;
    DRM_DWORD                           dwEndDate;
    DRM_BOOL                            fRealTimeExpiration;
    DRM_WORD                            rgwOPLs[OUTPUT_PROTECTION__COUNT]; /* The caller of DRM_TEE_KB_BuildCDKBCryptoData is responsible for setting these values, so initialization to zero will */
    DRM_DWORD                           dwPlayEnablers;
    DRM_DWORD                           cKeys;
    OEM_WRAPPED_KEY_AES_128             rgKeys[3]; /* CI/CK and optionally Sample protection */
    DRM_DWORD                           cRestrictions;
    DRM_TEE_KB_CDKB_PolicyRestriction   rgRestrictions[1];
} DRM_TEE_KB_CDKBData;

typedef struct __tagDRM_TEE_KB_RKBData
{
    DRM_DWORD           cbRKBData;
    DRM_TEE_RKB_CONTEXT oContext;
    DRM_DWORD           cCrlDigestEntries;
    DRM_RevocationEntry rgCrlDigestEntries[1]; /* For RKBs with more than one CRL digest entry, allocate a larger buffer and use this parameter as the first item in the array */
} DRM_TEE_KB_RKBData;
#endif
#ifdef NONE
typedef struct __tagDRM_TEE_KB_CEKBData
{
    DRM_BOOL fRoot;
    DRM_DWORD dwEncryptionMode;
    DRM_DWORD dwSelwrityLevel;
    OEM_WRAPPED_KEY_AES_128 rgCICK[2];
} DRM_TEE_KB_CEKBData;

typedef struct __tagDRM_TEE_KB_NKBData
{
    DRM_DWORD cbNKB;
    DRM_DWORD cNonces;
    DRM_ID    rgNonces[1]; /* To support multiple nonces, allocate a larger buffer and use this as the first item in the array */
} DRM_TEE_KB_NKBData;

static const DRM_ID idSessionZero = {0};
#endif
#if defined (SEC_COMPILE)
GCC_ATTRIB_NO_STACK_PROTECT()
static DRM_RESULT DRM_CALL _CryptoData_WrapKeys(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                                  DRM_BOOL                    f_fPersist,
    __in                                                  DRM_TEE_XB_KB_TYPE          f_eKeyBlobType,
    __in                                                  DRM_DWORD                   f_cEccKeys,
    __in_ecount_opt( f_cEccKeys )                   const DRM_TEE_KEY                *f_pEccKeys,
    __out_ecount_opt( f_cEccKeys )                        OEM_WRAPPED_KEY_ECC_256    *f_pEccWrappedKeys,
    __in                                                  DRM_DWORD                   f_cAesKeys,
    __in_ecount_opt( f_cAesKeys )                   const DRM_TEE_KEY                *f_pAesKeys,
    __out_ecount_opt( f_cAesKeys )                        OEM_WRAPPED_KEY_AES_128    *f_pAesWrappedKeys,
    __out_ecount_opt( 1 )                                 DRM_DWORD                  *f_pdwidCTK )
{
    DRM_RESULT       dr         = DRM_SUCCESS;
    DRM_TEE_KEY      oTKD       = DRM_TEE_KEY_EMPTY;
    OEM_TEE_CONTEXT *pOemTeeCtx = NULL;
    DRM_DWORD        iKey;

    ChkArg( f_pContext != NULL );
    ChkArg( (f_cEccKeys == 0) == (f_pEccKeys        == NULL) );
    ChkArg( (f_cEccKeys == 0) == (f_pEccWrappedKeys == NULL) );
    ChkArg( (f_cAesKeys == 0) == (f_pAesKeys        == NULL) );
    ChkArg( (f_cAesKeys == 0) == (f_pAesWrappedKeys == NULL) );

    pOemTeeCtx = &f_pContext->oContext;

    ChkDR( DRM_TEE_BASE_IMPL_DeriveTKDFromTK( f_pContext, NULL, f_eKeyBlobType, DRM_TEE_XB_KB_OPERATION_ENCRYPT, &f_fPersist, &oTKD ) );

    for( iKey = 0; iKey < f_cEccKeys; iKey++ )
    {
        DRM_DWORD ibData = 0;
        ChkDR( OEM_TEE_BASE_WrapECC256KeyForPersistedStorage( pOemTeeCtx, &oTKD.oKey, &f_pEccKeys[iKey].oKey, &ibData, ( DRM_BYTE * )&f_pEccWrappedKeys[iKey] ) );
    }

    for( iKey = 0; iKey < f_cAesKeys; iKey++ )
    {
        DRM_DWORD ibData = 0;
        if( f_fPersist )
        {
            ChkDR( OEM_TEE_BASE_WrapAES128KeyForPersistedStorage( pOemTeeCtx, &oTKD.oKey, &f_pAesKeys[iKey].oKey, &ibData, ( DRM_BYTE * )&f_pAesWrappedKeys[iKey] ) );
        }
        else
        {
            ChkDR( OEM_TEE_BASE_WrapAES128KeyForTransientStorage( pOemTeeCtx, &oTKD.oKey, &f_pAesKeys[iKey].oKey, &ibData, ( DRM_BYTE * )&f_pAesWrappedKeys[iKey] ) );
        }
    }

    if( f_pdwidCTK != NULL )
    {
        *f_pdwidCTK = oTKD.dwidTK;
    }

ErrorExit:
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &oTKD.oKey ) );

    if( DRM_FAILED( dr ) )
    {
        dr = DRM_E_TEE_ILWALID_KEY_DATA;
    }
    return dr;
}

PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_USING_FAILED_RESULT_6102, "Ignore Using Failed Result warning since out parameters are all initialized." );

GCC_ATTRIB_NO_STACK_PROTECT()
static DRM_RESULT DRM_CALL _CryptoData_UnwrapKeys(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                                  DRM_BOOL                    f_fPersist,
    __in_ecount_opt( 1 )                            const DRM_DWORD                  *f_pdwidTK,
    __in                                                  DRM_TEE_XB_KB_TYPE          f_eKeyBlobType,
    __in                                                  DRM_DWORD                   f_cEccKeys,
    __in_ecount_opt( f_cEccKeys )                   const OEM_WRAPPED_KEY_ECC_256    *f_pEccWrappedKeys,
    __in                                                  DRM_DWORD                   f_cEccTypes,
    __in_ecount_opt( f_cEccTypes )                  const DRM_TEE_KEY_TYPE           *f_pEccTypes,
    __out_ecount_opt( f_cEccKeys )                        DRM_TEE_KEY                *f_pEccKeys,
    __in                                                  DRM_DWORD                   f_cAesKeys,
    __in_ecount_opt( f_cAesKeys )                   const OEM_WRAPPED_KEY_AES_128    *f_pAesWrappedKeys,
    __in                                                  DRM_DWORD                   f_cAesTypes,
    __in_ecount_opt( f_cAesTypes )                  const DRM_TEE_KEY_TYPE           *f_pAesTypes,
    __out_ecount_opt( f_cAesKeys )                        DRM_TEE_KEY                *f_pAesKeys )
{
    DRM_RESULT         dr         = DRM_SUCCESS;
    const DRM_TEE_KEY *pTK        = NULL;
    DRM_TEE_KEY        oTK        = DRM_TEE_KEY_EMPTY;
    DRM_TEE_KEY        oTKD       = DRM_TEE_KEY_EMPTY;
    OEM_TEE_CONTEXT   *pOemTeeCtx = NULL;
    DRM_DWORD          iKey;

    AssertChkArg( f_pContext != NULL );
    AssertChkArg( ( f_cEccKeys == 0 ) == ( f_pEccKeys == NULL ) );
    AssertChkArg( ( f_cEccKeys == 0 ) == ( f_pEccWrappedKeys == NULL ) );
    AssertChkArg( ( f_cEccTypes == 0 ) == ( f_pEccKeys == NULL ) );
    AssertChkArg( ( f_cEccTypes == 0 ) == ( f_cEccKeys == 0 ) );
    AssertChkArg( f_cEccKeys <= f_cEccTypes );
    AssertChkArg( ( f_cAesKeys == 0 ) == ( f_pAesKeys == NULL ) );
    AssertChkArg( ( f_cAesKeys == 0 ) == ( f_pAesWrappedKeys == NULL ) );
    AssertChkArg( ( f_cAesTypes == 0 ) == ( f_pAesTypes == NULL ) );
    AssertChkArg( ( f_cAesTypes == 0 ) == ( f_cAesKeys == 0 ) );
    AssertChkArg( f_cAesKeys <= f_cAesTypes );

    pOemTeeCtx = &f_pContext->oContext;

    if( f_fPersist && f_pdwidTK != NULL )
    {
        oTK.dwidTK = *f_pdwidTK;
        ChkDR( DRM_TEE_BASE_IMPL_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_TK, &oTK ) );
        ChkDR( OEM_TEE_BASE_GetTKByID( pOemTeeCtx, *f_pdwidTK, &oTK.oKey ) );
        pTK = &oTK;
    }

    ChkDR( DRM_TEE_BASE_IMPL_DeriveTKDFromTK( f_pContext, pTK, f_eKeyBlobType, DRM_TEE_XB_KB_OPERATION_ENCRYPT, &f_fPersist, &oTKD ) );

    for( iKey = 0; iKey < f_cEccKeys; iKey++ )
    {
        DRM_DWORD ibData = 0;
        DRM_DWORD cbData = sizeof(f_pEccWrappedKeys[iKey]);
        DRM_TEE_KEY_TYPE eEccType = f_cEccTypes == 1 ? *f_pEccTypes : f_pEccTypes[ iKey ];

        ChkDR( DRM_TEE_BASE_IMPL_AllocKeyECC256( pOemTeeCtx, eEccType, &f_pEccKeys[ iKey ] ) );
        ChkDR( OEM_TEE_BASE_UnwrapECC256KeyFromPersistedStorage( pOemTeeCtx, &oTKD.oKey, &cbData, &ibData, ( const DRM_BYTE * )&f_pEccWrappedKeys[iKey], &f_pEccKeys[iKey].oKey ) );
    }

    for( iKey = 0; iKey < f_cAesKeys; iKey++ )
    {
        DRM_DWORD ibData = 0;
        DRM_DWORD cbData = sizeof(f_pAesWrappedKeys[iKey]);
        DRM_TEE_KEY_TYPE eAesType = f_cAesTypes == 1 ? *f_pAesTypes : f_pAesTypes[ iKey ];

        ChkDR( DRM_TEE_BASE_IMPL_AllocKeyAES128( pOemTeeCtx, eAesType, &f_pAesKeys[ iKey ] ) );
        if( f_fPersist )
        {
            ChkDR( OEM_TEE_BASE_UnwrapAES128KeyFromPersistedStorage( pOemTeeCtx, &oTKD.oKey, &cbData, &ibData, ( const DRM_BYTE * )&f_pAesWrappedKeys[iKey], &f_pAesKeys[iKey].oKey ) );
        }
        else
        {
            ChkDR( OEM_TEE_BASE_UnwrapAES128KeyFromTransientStorage( pOemTeeCtx, &oTKD.oKey, &cbData, &ibData, ( const DRM_BYTE * )&f_pAesWrappedKeys[iKey], &f_pAesKeys[iKey].oKey ) );
        }
    }

ErrorExit:
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &oTK.oKey  ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &oTKD.oKey ) );

    if( DRM_FAILED( dr ) )
    {
        dr = DRM_E_TEE_ILWALID_KEY_DATA;
    }

    return dr;
}

PREFAST_POP;

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_BuildPPKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                                  DRM_DWORD                   f_cKeys,
    __in_ecount( f_cKeys )                          const DRM_TEE_KEY                *f_pKeys,
    __in_ecount( 1 )                                const DRM_TEE_KEY                *f_pSSTKey,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_pPPKB )
{
    DRM_RESULT           dr         = DRM_SUCCESS;
    DRM_TEE_KB_PPKBData  oPPKBData  = { 0 };

    AssertChkArg( f_pContext      != NULL );
    AssertChkArg( f_cKeys         >= 2    );
    AssertChkArg( f_pKeys         != NULL );
    AssertChkArg( f_pPPKB         != NULL );
    AssertChkArg( f_pKeys[0].eType == DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_SIGN );
    AssertChkArg( f_pKeys[1].eType == DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_ENCRYPT );
    AssertChkArg( f_cKeys < 3 || f_pKeys[2].eType == DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_PRND );
    AssertChkArg( f_pSSTKey != NULL );
    AssertChkArg( f_pSSTKey->eType == DRM_TEE_KEY_TYPE_PR_SST );

    ChkDR( _CryptoData_WrapKeys(
        f_pContext,
        TRUE,
        DRM_TEE_XB_KB_TYPE_PPKB,
        f_cKeys,
        f_pKeys,
        oPPKBData.rgKeys,
        1,
        f_pSSTKey,
        &oPPKBData.oSSTKey,
        &oPPKBData.dwidTK ) );

    oPPKBData.cKeys = f_cKeys;

    ChkDR( DRM_TEE_KB_Build(
        f_pContext,
        DRM_TEE_XB_KB_TYPE_PPKB,
        sizeof(oPPKBData),
        ( DRM_BYTE * )&oPPKBData,
        f_pPPKB ) );

ErrorExit:
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_ParseAndVerifyPPKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pPPKB,
    __inout                                               DRM_DWORD                  *f_pcKeys,
    __deref_out_ecount( *f_pcKeys )                       DRM_TEE_KEY               **f_ppKeys,
    __out_ecount_opt( 1 )                                 DRM_TEE_KB_PPKBData        *f_pPPKBData )
{
    DRM_RESULT             dr                 = DRM_SUCCESS;
    DRM_TEE_KEY           *pKeys              = NULL;
    DRM_DWORD              cKeys              = 0;
    DRM_DWORD              iSSTKey            = 0;
    DRM_DWORD              cbPPKBDataWeakRef  = 0;
    DRM_TEE_KB_PPKBData   *pPPKBDataWeakRef   = NULL;
    DRM_TEE_KEY_TYPE       rgeAesTypes[ 1 ]   = { DRM_TEE_KEY_TYPE_PR_SST };
    DRM_TEE_KEY_TYPE       rgeEccTypes[ 3 ]   = { DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_SIGN, DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_ENCRYPT, DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_PRND };

    /* Arguments are checked in the PRITEE proxy layer */
    DRMASSERT( f_pContext != NULL );
    DRMASSERT( f_pPPKB    != NULL );
    DRMASSERT( f_pcKeys   != NULL );
    DRMASSERT( f_ppKeys   != NULL );

    DRMASSERT( IsBlobAssigned( f_pPPKB ) );

    ChkDR( DRM_TEE_KB_ParseAndVerifyWithReconstitution(
        f_pContext,
        f_pPPKB,
        DRM_TEE_XB_KB_TYPE_PPKB,
        NULL,
        &cbPPKBDataWeakRef,
        ( DRM_BYTE ** )&pPPKBDataWeakRef ) );

    ChkBOOL( cbPPKBDataWeakRef == sizeof(DRM_TEE_KB_PPKBData), DRM_E_TEE_ILWALID_KEY_DATA );
    AssertChkArg( pPPKBDataWeakRef->cKeys <= DRM_NO_OF( rgeEccTypes ) );

    ChkDR( DRM_TEE_BASE_IMPL_AllocateKeyArray( f_pContext, pPPKBDataWeakRef->cKeys + 1, &pKeys ) );     /* +1 for SST key */
    cKeys = pPPKBDataWeakRef->cKeys + 1; /* +1 for SST key */
    iSSTKey = pPPKBDataWeakRef->cKeys;

    ChkDR( _CryptoData_UnwrapKeys(
        f_pContext,
        TRUE,
        &pPPKBDataWeakRef->dwidTK,
        DRM_TEE_XB_KB_TYPE_PPKB,
        pPPKBDataWeakRef->cKeys,
        pPPKBDataWeakRef->rgKeys,
        DRM_NO_OF( rgeEccTypes ),
        rgeEccTypes,
        pKeys,
        DRM_NO_OF( rgeAesTypes ),
        &pPPKBDataWeakRef->oSSTKey,
        DRM_NO_OF( rgeAesTypes ),
        rgeAesTypes,
        &pKeys[ iSSTKey ] ) );

    if( f_pPPKBData != NULL )
    {
        *f_pPPKBData = *pPPKBDataWeakRef;
    }

    *f_pcKeys = cKeys;
    *f_ppKeys = pKeys;
    cKeys = 0;
    pKeys = NULL;

ErrorExit:
    ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, cKeys, &pKeys ) );
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_BuildSPKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in_ecount( 1 )                                const DRM_TEE_KEY                *f_pKey,
    __in                                                  DRM_DWORD                   f_dwSelwrityLevel,
    __in                                                  DRM_DWORD                   f_cCertDigests,
    __in_ecount(f_cCertDigests)                     const DRM_RevocationEntry        *f_pCertDigests,
    __in                                                  DRM_DWORD                   f_dwRIV,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_pSPKB )
{
    DRM_RESULT           dr            = DRM_SUCCESS;
    DRM_TEE_KB_SPKBData  oSPKBData     = { 0 };
    DRM_DWORD            cbCertDigests = 0;

    AssertChkArg( f_pContext     != NULL );
    AssertChkArg( f_pKey         != NULL );
    AssertChkArg( f_pCertDigests != NULL );
    AssertChkArg( f_pSPKB        != NULL );
    AssertChkArg( f_pKey->eType == DRM_TEE_KEY_TYPE_SAMPLEPROT );

    ChkArg( f_cCertDigests <= DRM_BCERT_MAX_CERTS_PER_CHAIN );

    DRMASSERT( !OEM_SELWRE_ARE_EQUAL( &f_pContext->idSession, &idSessionZero, sizeof(idSessionZero) ) );

    ChkDR( _CryptoData_WrapKeys(
        f_pContext,
        FALSE,
        DRM_TEE_XB_KB_TYPE_SPKB,
        0,
        NULL,
        NULL,
        1,
        f_pKey,
        &oSPKBData.oKey,
        NULL ) );

    oSPKBData.dwRIV           = f_dwRIV;
    oSPKBData.dwSelwrityLevel = f_dwSelwrityLevel;
    oSPKBData.cCertDigests    = f_cCertDigests;

    /* f_cCertDigests is at most DRM_BCERT_MAX_CERTS_PER_CHAIN */
    cbCertDigests = f_cCertDigests * sizeof(DRM_RevocationEntry);
    ChkVOID( OEM_TEE_MEMCPY( oSPKBData.rgbCertDigests, f_pCertDigests, cbCertDigests ) );

    ChkDR( DRM_TEE_KB_Build(
        f_pContext,
        DRM_TEE_XB_KB_TYPE_SPKB,
        sizeof(oSPKBData),
        ( DRM_BYTE * )&oSPKBData,
        f_pSPKB ) );

ErrorExit:
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_ParseAndVerifySPKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pSPKB,
    __out                                                 DRM_DWORD                  *f_pdwSelwrityLevel,
    __out                                                 DRM_DWORD                  *f_pcCertDigests,
    __out_ecount( *f_pcCertDigests )                      DRM_RevocationEntry        *f_pCertDigests,
    __out                                                 DRM_DWORD                  *f_pdwRIV,
    __out                                                 DRM_DWORD                  *f_pcKeys,
    __deref_out_ecount( *f_pcKeys )                       DRM_TEE_KEY               **f_ppKeys )
{
    DRM_RESULT           dr                      = DRM_SUCCESS;
    DRM_TEE_KB_SPKBData *pSPKBCryptoDataWeakRef  = NULL;
    DRM_DWORD            cbSPKBCryptoDataWeakRef = 0;
    DRM_TEE_KEY         *pKeys                   = NULL;
    DRM_DWORD            cbCertDigests           = 0;
    DRM_TEE_KEY_TYPE     rgeAesTypes[ 1 ]        = { DRM_TEE_KEY_TYPE_SAMPLEPROT };

    AssertChkArg( f_pContext         != NULL );
    AssertChkArg( f_pSPKB            != NULL );
    AssertChkArg( f_pdwSelwrityLevel != NULL );
    AssertChkArg( f_pcCertDigests    != NULL );
    AssertChkArg( f_pCertDigests     != NULL );
    AssertChkArg( f_pdwRIV           != NULL );
    AssertChkArg( f_pcKeys           != NULL );
    AssertChkArg( f_ppKeys           != NULL );

    ChkDR( DRM_TEE_KB_ParseAndVerify(
        f_pContext,
        f_pSPKB,
        DRM_TEE_XB_KB_TYPE_SPKB,
        &cbSPKBCryptoDataWeakRef,
        ( DRM_BYTE ** )&pSPKBCryptoDataWeakRef ) );

    ChkBOOL( cbSPKBCryptoDataWeakRef == sizeof(*pSPKBCryptoDataWeakRef), DRM_E_TEE_ILWALID_KEY_DATA );

    ChkDR( DRM_TEE_BASE_IMPL_AllocateKeyArray( f_pContext, 1, &pKeys ) );

    ChkDR( _CryptoData_UnwrapKeys(
        f_pContext,
        FALSE,
        NULL,
        DRM_TEE_XB_KB_TYPE_SPKB,
        0,
        NULL,
        0,
        NULL,
        NULL,
        DRM_NO_OF( rgeAesTypes ),
        &pSPKBCryptoDataWeakRef->oKey,
        DRM_NO_OF( rgeAesTypes ),
        rgeAesTypes,
        pKeys ) );

    ChkBOOL( pSPKBCryptoDataWeakRef->dwSelwrityLevel > 0, DRM_E_TEE_ILWALID_KEY_DATA );
    ChkBOOL( pSPKBCryptoDataWeakRef->cCertDigests <= DRM_BCERT_MAX_CERTS_PER_CHAIN, DRM_E_TEE_ILWALID_KEY_DATA );
    ChkBOOL( *f_pcCertDigests >= pSPKBCryptoDataWeakRef->cCertDigests, DRM_E_BUFFERTOOSMALL );

    /* pSPKBCryptoDataWeakRef->cCertDigests <= DRM_BCERT_MAX_CERTS_PER_CHAIN */
    cbCertDigests = pSPKBCryptoDataWeakRef->cCertDigests * sizeof(DRM_RevocationEntry);
    ChkVOID( OEM_TEE_MEMCPY( f_pCertDigests, pSPKBCryptoDataWeakRef->rgbCertDigests, cbCertDigests ) );

    *f_pdwRIV           = pSPKBCryptoDataWeakRef->dwRIV;
    *f_pdwSelwrityLevel = pSPKBCryptoDataWeakRef->dwSelwrityLevel;
    *f_pcCertDigests    = pSPKBCryptoDataWeakRef->cCertDigests;

    *f_pcKeys           = DRM_NO_OF( rgeAesTypes );
    *f_ppKeys           = pKeys;
    pKeys               = NULL;

ErrorExit:
    ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, DRM_NO_OF( rgeAesTypes ), &pKeys ) );
    return dr;

}
#endif
#ifdef NONE
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_BuildTPKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in_ecount_opt( 1 )                            const DRM_TEE_KEY                *f_pKeySession,
    __in_ecount( RPROV_KEYPAIR_COUNT )              const DRM_TEE_KEY                *f_pECCKeysPrivate,
    __in_ecount( RPROV_KEYPAIR_COUNT )              const PUBKEY_P256                *f_pECCKeysPublic,
    __in_ecount( OEM_PROVISIONING_NONCE_LENGTH )    const DRM_BYTE                   *f_pbNonce,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_pTPKB )
{
    DRM_RESULT           dr           = DRM_SUCCESS;
    DRM_DWORD            cbCryptoData = 0;
    DRM_TEE_KB_TPKBData *pTPKBData    = NULL;

    AssertChkArg( f_pContext        != NULL );
    AssertChkArg( f_pECCKeysPrivate != NULL );
    AssertChkArg( f_pECCKeysPublic  != NULL );
    AssertChkArg( f_pbNonce         != NULL );

    if( f_pKeySession != NULL )
    {
        AssertChkArg( f_pKeySession[0].eType == DRM_TEE_KEY_TYPE_RPROV_SESSION_KEY );
    }

    cbCryptoData = sizeof( DRM_TEE_KB_TPKBData );
    ChkDR( DRM_TEE_BASE_MemAlloc( f_pContext, cbCryptoData, ( DRM_VOID ** )&pTPKBData ) );
    ChkVOID( OEM_TEE_ZERO_MEMORY( pTPKBData, cbCryptoData ) );

    ChkDR( _CryptoData_WrapKeys(
        f_pContext,
        FALSE,
        DRM_TEE_XB_KB_TYPE_TPKB,
        RPROV_KEYPAIR_COUNT,
        f_pECCKeysPrivate,
        pTPKBData->rgoPrivkeys,
        f_pKeySession==NULL?0u:1u,
        f_pKeySession,
        f_pKeySession==NULL?NULL:&pTPKBData->oKSession,
        NULL ) );

    pTPKBData->fKSessionInitialized = ( NULL == f_pKeySession )?FALSE:TRUE;

    DRMSIZEASSERT( sizeof( pTPKBData->rgbChallengeGenerationNonce ), OEM_PROVISIONING_NONCE_LENGTH );
    ChkVOID( OEM_TEE_MEMCPY( pTPKBData->rgbChallengeGenerationNonce, f_pbNonce, OEM_PROVISIONING_NONCE_LENGTH ) );
    ChkVOID( OEM_TEE_MEMCPY( &pTPKBData->rgoPubkeys, f_pECCKeysPublic, sizeof(*f_pECCKeysPublic)*RPROV_KEYPAIR_COUNT ) );

    ChkDR( DRM_TEE_KB_Build(
        f_pContext,
        DRM_TEE_XB_KB_TYPE_TPKB,
        cbCryptoData,
        ( DRM_BYTE * )pTPKBData,
        f_pTPKB ) );

ErrorExit:
    ChkVOID( DRM_TEE_BASE_MemFree( f_pContext, ( DRM_VOID ** )&pTPKBData ) );
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_ParseAndVerifyTPKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pTPKB,
    __deref_out_ecount_opt( *f_pcoKSession )              DRM_TEE_KEY               **f_ppoKSession,
    __out_ecount( 1 )                                     DRM_DWORD                  *f_pcoKSession,
    __deref_opt_out_ecount( RPROV_KEYPAIR_COUNT )         DRM_TEE_KEY               **f_ppoECCKeysPrivate,
    __out_ecount( RPROV_KEYPAIR_COUNT )                   PUBKEY_P256                *f_poECCKeysPublic,
    __out_ecount_opt( OEM_PROVISIONING_NONCE_LENGTH )     DRM_BYTE                   *f_pbNonce )
{
    DRM_RESULT           dr                      = DRM_SUCCESS;
    DRM_TEE_KB_TPKBData *pTPKBCryptoDataWeakRef  = NULL;
    DRM_DWORD            cbTPKBCryptoDataWeakRef = 0;
    DRM_TEE_KEY         *pKSession               = NULL;
    DRM_TEE_KEY         *pPrivKeys               = NULL;
    DRM_DWORD            idx                     = 0;
    DRM_TEE_KEY_TYPE     rgeAesTypes[ 1 ]        = { DRM_TEE_KEY_TYPE_RPROV_SESSION_KEY };
    DRM_TEE_KEY_TYPE     rgeEccTypes[ 3 ]        = { DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_SIGN, DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_ENCRYPT, DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_PRND };
    DRMCASSERT( RPROV_KEYPAIR_COUNT == DRM_NO_OF( rgeEccTypes ) );

    AssertChkArg( f_pContext           != NULL );
    AssertChkArg( f_pTPKB              != NULL );
    AssertChkArg( f_poECCKeysPublic    != NULL );
    AssertChkArg( f_pcoKSession        != NULL );

    ChkDR( DRM_TEE_KB_ParseAndVerify(
        f_pContext,
        f_pTPKB,
        DRM_TEE_XB_KB_TYPE_TPKB,
        &cbTPKBCryptoDataWeakRef,
        ( DRM_BYTE ** )&pTPKBCryptoDataWeakRef ) );

    ChkBOOL( cbTPKBCryptoDataWeakRef == sizeof(*pTPKBCryptoDataWeakRef), DRM_E_TEE_ILWALID_KEY_DATA );

    if ( pTPKBCryptoDataWeakRef->fKSessionInitialized )
    {
        AssertChkArg( f_ppoKSession != NULL );

        *f_pcoKSession = DRM_NO_OF( rgeAesTypes );
        ChkDR( DRM_TEE_BASE_IMPL_AllocateKeyArray( f_pContext, *f_pcoKSession, &pKSession ) );
    }
    else
    {
        *f_pcoKSession = 0;
    }

    if ( f_ppoECCKeysPrivate != NULL )
    {
        ChkDR( DRM_TEE_BASE_IMPL_AllocateKeyArray( f_pContext, DRM_NO_OF( rgeEccTypes ), &pPrivKeys ) );
    }

    ChkDR( _CryptoData_UnwrapKeys(
        f_pContext,
        FALSE,
        NULL,
        DRM_TEE_XB_KB_TYPE_TPKB,
        f_ppoECCKeysPrivate != NULL ? DRM_NO_OF( rgeEccTypes ) : (DRM_DWORD)0,
        f_ppoECCKeysPrivate != NULL ? pTPKBCryptoDataWeakRef->rgoPrivkeys : NULL,
        f_ppoECCKeysPrivate != NULL ? DRM_NO_OF( rgeEccTypes ) : (DRM_DWORD)0,
        f_ppoECCKeysPrivate != NULL ? rgeEccTypes : NULL,
        pPrivKeys,
        pKSession != NULL ? (DRM_DWORD)DRM_NO_OF( rgeAesTypes ) : (DRM_DWORD)0,
        pKSession != NULL ? &pTPKBCryptoDataWeakRef->oKSession : NULL,
        pKSession != NULL ? (DRM_DWORD)DRM_NO_OF( rgeAesTypes ) : (DRM_DWORD)0,
        pKSession != NULL ? rgeAesTypes : NULL,
        pKSession ) );

    if ( pKSession != NULL )
    {
        *f_ppoKSession = pKSession;
        pKSession = NULL;
    }

    for ( idx = 0; idx < RPROV_KEYPAIR_COUNT; idx++ )
    {
        f_poECCKeysPublic[ idx ] = pTPKBCryptoDataWeakRef->rgoPubkeys[ idx ];
    }

    if ( f_ppoECCKeysPrivate != NULL )
    {
        *f_ppoECCKeysPrivate = pPrivKeys;
        pPrivKeys = NULL;
    }

    if ( f_pbNonce != NULL )
    {
        ChkVOID( OEM_TEE_MEMCPY( f_pbNonce, pTPKBCryptoDataWeakRef->rgbChallengeGenerationNonce, OEM_PROVISIONING_NONCE_LENGTH ) );
    }
ErrorExit:
    if ( pKSession != NULL )
    {
        ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, *f_pcoKSession, &pKSession ) );
    }
    if ( pPrivKeys != NULL )
    {
        ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, RPROV_KEYPAIR_COUNT, &pPrivKeys ) );
    }
    return dr;
}
#endif

#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_BuildLKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                                  DRM_BOOL                    f_fPersist,
    __in_ecount( 2 )                                const DRM_TEE_KEY                *f_pKeys,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_pLKB )
{
    DRM_RESULT           dr           = DRM_SUCCESS;
    DRM_TEE_KB_LKBData   oLKBData     = { 0 };

    AssertChkArg( f_pContext       != NULL );
    AssertChkArg( f_pKeys          != NULL );
    AssertChkArg( f_pLKB           != NULL );
    AssertChkArg( f_pKeys[0].eType == DRM_TEE_KEY_TYPE_PR_CI );
    AssertChkArg( f_pKeys[1].eType == DRM_TEE_KEY_TYPE_PR_CK );

    /*
    ** We add the session ID to the crypto data for LKB if the key is not to be persisted.
    ** Otherwise we store an all zero ID indicating that the key is to be persisted.  Therefore
    ** the real session ID should NEVER be all zeros.
    */
    DRMASSERT( !OEM_SELWRE_ARE_EQUAL( &f_pContext->idSession, &idSessionZero, sizeof(idSessionZero) ) );

    ChkDR( _CryptoData_WrapKeys(
        f_pContext,
        f_fPersist,
        DRM_TEE_XB_KB_TYPE_LKB,
        0,
        NULL,
        NULL,
        2,
        f_pKeys,
        oLKBData.rgKeys,
        &oLKBData.dwidTK ) );

    oLKBData.fPersist = f_fPersist;

    ChkDR( DRM_TEE_KB_Build(
        f_pContext,
        DRM_TEE_XB_KB_TYPE_LKB,
        sizeof(oLKBData),
        ( DRM_BYTE * )&oLKBData,
        f_pLKB ) );

ErrorExit:
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_ParseAndVerifyLKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pLKB,
    __out                                                 DRM_DWORD                  *f_pcKeys,
    __deref_out_ecount( *f_pcKeys )                       DRM_TEE_KEY               **f_ppKeys )
{
    DRM_RESULT           dr                     = DRM_SUCCESS;
    DRM_TEE_KB_LKBData  *pLKBCryptoDataWeakRef  = NULL;
    DRM_DWORD            cLKBCryptoDataWeakRef  = 0;
    DRM_TEE_KEY         *pKeys                  = NULL;
    DRM_TEE_KEY_TYPE     rgeAesTypes[ 2 ]       = { DRM_TEE_KEY_TYPE_PR_CI, DRM_TEE_KEY_TYPE_PR_CK };

    AssertChkArg( f_pContext != NULL );
    AssertChkArg( f_pLKB     != NULL );
    AssertChkArg( f_pcKeys   != NULL );
    AssertChkArg( f_ppKeys   != NULL );

    ChkDR( DRM_TEE_KB_ParseAndVerify(
        f_pContext,
        f_pLKB,
        DRM_TEE_XB_KB_TYPE_LKB,
        &cLKBCryptoDataWeakRef,
        ( DRM_BYTE ** )&pLKBCryptoDataWeakRef ) );

    ChkBOOL( cLKBCryptoDataWeakRef == sizeof(*pLKBCryptoDataWeakRef), DRM_E_TEE_ILWALID_KEY_DATA );

    ChkDR( DRM_TEE_BASE_IMPL_AllocateKeyArray( f_pContext, DRM_NO_OF( rgeAesTypes ), &pKeys ) );

    ChkDR( _CryptoData_UnwrapKeys(
        f_pContext,
        pLKBCryptoDataWeakRef->fPersist,
        pLKBCryptoDataWeakRef->fPersist ? &pLKBCryptoDataWeakRef->dwidTK : NULL,
        DRM_TEE_XB_KB_TYPE_LKB,
        0,
        NULL,
        0,
        NULL,
        NULL,
        DRM_NO_OF( rgeAesTypes ),
        pLKBCryptoDataWeakRef->rgKeys,
        DRM_NO_OF( rgeAesTypes ),
        rgeAesTypes,
        pKeys ) );

    *f_pcKeys           = DRM_NO_OF( rgeAesTypes );
    *f_ppKeys           = pKeys;
    pKeys               = NULL;

ErrorExit:
    ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, DRM_NO_OF( rgeAesTypes ), &pKeys ) );
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_BuildDKB(
    __inout_opt                                           DRM_TEE_CONTEXT            *f_pContext,
    __in_ecount( 1 )                                      DRM_TEE_KEY                *f_pKey,
    __in                                                  DRM_DWORD                   f_dwRevision,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_pDKB )
{
    DRM_RESULT           dr           = DRM_SUCCESS;
    DRM_TEE_KB_DKBData   oDKBData     = { 0 };

    AssertChkArg( f_pContext       != NULL );
    AssertChkArg( f_pKey           != NULL );
    AssertChkArg( f_pDKB           != NULL );
    AssertChkArg( f_pKey->eType    == DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_ENCRYPT );

    ChkDR( _CryptoData_WrapKeys(
        f_pContext,
        TRUE,
        DRM_TEE_XB_KB_TYPE_DKB,
        1,
        f_pKey,
        &oDKBData.oKey,
        0,
        NULL,
        NULL,
        &oDKBData.dwidTK ) );

    oDKBData.dwRevision = f_dwRevision;

    ChkDR( DRM_TEE_KB_Build(
        f_pContext,
        DRM_TEE_XB_KB_TYPE_DKB,
        sizeof(oDKBData),
        ( DRM_BYTE * )&oDKBData,
        f_pDKB ) );

ErrorExit:
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_ParseAndVerifyDKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pDKB,
    __in                                                  DRM_DWORD                   f_dwRevision,
    __deref_out_ecount( 1 )                               DRM_TEE_KEY               **f_ppKey )
{
    DRM_RESULT           dr                     = DRM_SUCCESS;
    DRM_TEE_KB_DKBData  *pDKBCryptoDataWeakRef  = NULL;
    DRM_DWORD            cDKBCryptoDataWeakRef  = 0;
    DRM_TEE_KEY         *pKeys                  = NULL;
    DRM_TEE_KEY_TYPE     rgeEccTypes[ 1 ]       = { DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_ENCRYPT };

    AssertChkArg( f_pContext != NULL );
    AssertChkArg( f_pDKB     != NULL );
    AssertChkArg( f_ppKey    != NULL );

    ChkDR( DRM_TEE_KB_ParseAndVerify(
        f_pContext,
        f_pDKB,
        DRM_TEE_XB_KB_TYPE_DKB,
        &cDKBCryptoDataWeakRef,
        ( DRM_BYTE ** )&pDKBCryptoDataWeakRef ) );

    ChkBOOL( cDKBCryptoDataWeakRef == sizeof(*pDKBCryptoDataWeakRef), DRM_E_TEE_ILWALID_KEY_DATA );

    ChkDR( DRM_TEE_BASE_IMPL_AllocateKeyArray( f_pContext, 1, &pKeys ) );

    ChkDR( _CryptoData_UnwrapKeys(
        f_pContext,
        TRUE,
        &pDKBCryptoDataWeakRef->dwidTK,
        DRM_TEE_XB_KB_TYPE_DKB,
        DRM_NO_OF( rgeEccTypes ),
        &pDKBCryptoDataWeakRef->oKey,
        DRM_NO_OF( rgeEccTypes ),
        rgeEccTypes,
        pKeys,
        0,
        NULL,
        0,
        NULL,
        NULL ) );

    /* Ensure that the revision requested by the caller is the same as the one in the DBK */
    ChkBOOL( f_dwRevision == pDKBCryptoDataWeakRef->dwRevision, DRM_E_TEE_ILWALID_KEY_DATA );

    *f_ppKey = &pKeys[0];
    pKeys = NULL;

ErrorExit:
    ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, DRM_NO_OF( rgeEccTypes ), &pKeys ) );
    return dr;
}
#endif

#ifdef NONE
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_BuildMRKB(
    __inout_opt                                           DRM_TEE_CONTEXT            *f_pContext,
    __in_ecount( 2 )                                      DRM_TEE_KEY                *f_pMIMK,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_pMRKB )
{
    DRM_RESULT           dr           = DRM_SUCCESS;
    DRM_TEE_KB_MRKBData  oMRKBData    = { 0 };

    AssertChkArg( f_pContext       != NULL );
    AssertChkArg( f_pMIMK          != NULL );
    AssertChkArg( f_pMRKB          != NULL );

    ChkDR( _CryptoData_WrapKeys(
        f_pContext,
        FALSE,
        DRM_TEE_XB_KB_TYPE_MRKB,
        0,
        NULL,
        NULL,
        2,
        f_pMIMK,
        oMRKBData.rgKeys,
        NULL ) );

    ChkDR( DRM_TEE_KB_Build(
        f_pContext,
        DRM_TEE_XB_KB_TYPE_MRKB,
        sizeof(oMRKBData),
        ( DRM_BYTE * )&oMRKBData,
        f_pMRKB ) );

ErrorExit:
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_ParseAndVerifyMRKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pMRKB,
    __deref_out_ecount( 2 )                               DRM_TEE_KEY               **f_ppKeys )
{
    DRM_RESULT            dr                      = DRM_SUCCESS;
    DRM_TEE_KB_MRKBData  *pMRKBCryptoDataWeakRef  = NULL;
    DRM_DWORD             cMRKBCryptoDataWeakRef  = 0;
    DRM_TEE_KEY          *pKeys                   = NULL;
    DRM_TEE_KEY_TYPE      rgeAesTypes[ 2 ]        = { DRM_TEE_KEY_TYPE_PR_MI, DRM_TEE_KEY_TYPE_PR_MK };

    AssertChkArg( f_pContext       != NULL );
    AssertChkArg( f_pMRKB          != NULL );
    AssertChkArg( f_ppKeys         != NULL );

    ChkDR( DRM_TEE_KB_ParseAndVerify(
        f_pContext,
        f_pMRKB,
        DRM_TEE_XB_KB_TYPE_MRKB,
        &cMRKBCryptoDataWeakRef,
        ( DRM_BYTE ** )&pMRKBCryptoDataWeakRef ) );

    ChkBOOL( cMRKBCryptoDataWeakRef == sizeof(*pMRKBCryptoDataWeakRef), DRM_E_TEE_ILWALID_KEY_DATA );

    ChkDR( DRM_TEE_BASE_IMPL_AllocateKeyArray( f_pContext, DRM_NO_OF( rgeAesTypes ), &pKeys ) );

    ChkDR( _CryptoData_UnwrapKeys(
        f_pContext,
        FALSE,
        NULL,
        DRM_TEE_XB_KB_TYPE_MRKB,
        0,
        NULL,
        0,
        NULL,
        NULL,
        DRM_NO_OF( rgeAesTypes ),
        pMRKBCryptoDataWeakRef->rgKeys,
        DRM_NO_OF( rgeAesTypes ),
        rgeAesTypes,
        pKeys ) );

    *f_ppKeys         = pKeys;
    pKeys             = NULL;

ErrorExit:
    ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, DRM_NO_OF( rgeAesTypes ), &pKeys ) );
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_BuildMTKB(
    __inout_opt                                           DRM_TEE_CONTEXT            *f_pContext,
    __in_ecount( 1 )                                const DRM_TEE_KB_MTKB_Context    *f_pMTKBContext,
    __in_ecount( 2 )                                      DRM_TEE_KEY                *f_pMIMK,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_pMTKB )
{
    DRM_RESULT           dr               = DRM_SUCCESS;
    DRM_TEE_KB_MTKBData  oMTKBCryptoData  = {0};

    AssertChkArg( f_pContext        != NULL );
    AssertChkArg( f_pMTKBContext    != NULL );
    AssertChkArg( f_pMIMK           != NULL );
    AssertChkArg( f_pMTKB           != NULL );

    ChkDR( _CryptoData_WrapKeys(
        f_pContext,
        FALSE,
        DRM_TEE_XB_KB_TYPE_MTKB,
        0,
        NULL,
        NULL,
        2,
        f_pMIMK,
        oMTKBCryptoData.rgMIMK,
        NULL ) );

    oMTKBCryptoData.oContext = *f_pMTKBContext;

    ChkDR( DRM_TEE_KB_Build(
        f_pContext,
        DRM_TEE_XB_KB_TYPE_MTKB,
        sizeof(oMTKBCryptoData),
        ( DRM_BYTE * )&oMTKBCryptoData,
        f_pMTKB ) );

ErrorExit:
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_ParseAndVerifyMTKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pMTKB,
    __out_ecount_opt( 1 )                                 DRM_TEE_KB_MTKB_Context    *f_pMTKBContext,
    __deref_out_ecount( 2 )                               DRM_TEE_KEY               **f_ppKeys )
{
    DRM_RESULT            dr                      = DRM_SUCCESS;
    DRM_TEE_KB_MTKBData  *pMTKBCryptoDataWeakRef  = NULL;
    DRM_DWORD             cMTKBCryptoDataWeakRef  = 0;
    DRM_TEE_KEY          *pKeys                   = NULL;
    DRM_TEE_KEY_TYPE      rgeAesTypes[ 2 ]        = { DRM_TEE_KEY_TYPE_PR_MI, DRM_TEE_KEY_TYPE_PR_MK };

    AssertChkArg( f_pContext       != NULL );
    AssertChkArg( f_pMTKB          != NULL );
    AssertChkArg( f_pMTKBContext   != NULL );
    AssertChkArg( f_ppKeys         != NULL );

    ChkDR( DRM_TEE_KB_ParseAndVerify(
        f_pContext,
        f_pMTKB,
        DRM_TEE_XB_KB_TYPE_MTKB,
        &cMTKBCryptoDataWeakRef,
        ( DRM_BYTE ** )&pMTKBCryptoDataWeakRef ) );

    ChkBOOL( cMTKBCryptoDataWeakRef == sizeof(*pMTKBCryptoDataWeakRef), DRM_E_TEE_ILWALID_KEY_DATA );

    *f_ppKeys = NULL;

    ChkDR( DRM_TEE_BASE_IMPL_AllocateKeyArray( f_pContext, DRM_NO_OF( rgeAesTypes ), &pKeys ) );

    ChkDR( _CryptoData_UnwrapKeys(
        f_pContext,
        FALSE,
        NULL,
        DRM_TEE_XB_KB_TYPE_MTKB,
        0,
        NULL,
        0,
        NULL,
        NULL,
        DRM_NO_OF( rgeAesTypes ),
        pMTKBCryptoDataWeakRef->rgMIMK,
        DRM_NO_OF( rgeAesTypes ),
        rgeAesTypes,
        pKeys ) );

    if( f_pMTKBContext != NULL )
    {
        *f_pMTKBContext = pMTKBCryptoDataWeakRef->oContext;
    }

    *f_ppKeys         = pKeys;
    pKeys             = NULL;

ErrorExit:
    ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, DRM_NO_OF( rgeAesTypes ), &pKeys ) );
    return dr;
}
#endif
#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_BuildCDKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                                  DRM_BOOL                    f_fSelfContained,
    __in                                                  DRM_DWORD                   f_cKeys,
    __in_ecount( f_cKeys )                          const DRM_TEE_KEY                *f_pKeys,
    __in_ecount( 1 )                                      DRM_TEE_POLICY_INFO        *f_pPolicy,
   __out                                                  DRM_TEE_BYTE_BLOB          *f_pCDKB )
{
    DRM_RESULT           dr                 = DRM_SUCCESS;
    DRM_DWORD            cbCDKBCryptoData   = 0;
    DRM_TEE_KB_CDKBData *pCDKBCryptoData    = NULL;
    OEM_TEE_CONTEXT     *pOemTeeCtx         = NULL;
    DRM_DWORD            iPolicy            = 0;
    DRM_DWORD            ibVariableData     = 0;
    DRM_DWORD            ibEnd              = 0;

    AssertChkArg( f_pContext          != NULL );
    AssertChkArg( f_pPolicy           != NULL );
    AssertChkArg( f_pKeys             != NULL );
    AssertChkArg( f_pCDKB             != NULL );
    AssertChkArg( f_cKeys == 2 || f_cKeys == 3 );
    AssertChkArg( f_pKeys[0].eType == DRM_TEE_KEY_TYPE_PR_CI );
    AssertChkArg( f_pKeys[1].eType == DRM_TEE_KEY_TYPE_PR_CK );
    AssertChkArg( f_cKeys != 3 || f_pKeys[2].eType == DRM_TEE_KEY_TYPE_SAMPLEPROT );
    AssertChkArg( (f_pPolicy->cRestrictions == 0) == (f_pPolicy->pRestrictions == NULL) );

    DRMASSERT( f_cKeys != 3 || f_pPolicy->dwDecryptionMode == OEM_TEE_AES128CTR_DECRYPTION_MODE_SAMPLE_PROTECTION );
    DRMASSERT( f_cKeys == 3 || f_pPolicy->dwDecryptionMode != OEM_TEE_AES128CTR_DECRYPTION_MODE_SAMPLE_PROTECTION );

    pOemTeeCtx = &f_pContext->oContext;

    /* Callwlate CDKB size */
    if( f_pPolicy->cRestrictions > 1 )
    {
        ChkDR( DRM_DWordMult( f_pPolicy->cRestrictions - 1, sizeof(DRM_TEE_KB_CDKB_PolicyRestriction), &cbCDKBCryptoData ) );
    }
    ChkDR( DRM_DWordAddSame( &cbCDKBCryptoData, sizeof(DRM_TEE_KB_CDKBData) ) );

    /* End of constant size data */
    ibVariableData = cbCDKBCryptoData;

    if( f_fSelfContained )
    {
        AssertChkArg( pOemTeeCtx != NULL );
        ChkDR( DRM_DWordAddSame( &cbCDKBCryptoData, pOemTeeCtx->cbUnprotectedOEMData ) );
    }

    for( iPolicy = 0; iPolicy < f_pPolicy->cRestrictions; iPolicy++ )
    {
        ChkDR( DRM_DWordAddSame( &cbCDKBCryptoData, f_pPolicy->pRestrictions[iPolicy].cbRestrictionConfiguration ) );
    }

    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, cbCDKBCryptoData, ( DRM_VOID ** )&pCDKBCryptoData ) );
    ChkVOID( OEM_TEE_ZERO_MEMORY( pCDKBCryptoData, cbCDKBCryptoData ) );

    ChkDR( _CryptoData_WrapKeys(
        f_pContext,
        FALSE,
        DRM_TEE_XB_KB_TYPE_CDKB,
        0,
        NULL,
        NULL,
        f_cKeys,
        f_pKeys,
        pCDKBCryptoData->rgKeys,
        NULL ) );

    pCDKBCryptoData->cbCDKBData             = cbCDKBCryptoData;
    pCDKBCryptoData->cKeys                  = f_cKeys;
    pCDKBCryptoData->cRestrictions          = f_pPolicy->cRestrictions;
    pCDKBCryptoData->oKID                   = f_pPolicy->oKID;
    pCDKBCryptoData->oLID                   = f_pPolicy->oLID;
    pCDKBCryptoData->dwBeginDate            = f_pPolicy->dwBeginDate;
    pCDKBCryptoData->dwDecryptionMode       = f_pPolicy->dwDecryptionMode;
    pCDKBCryptoData->dwEndDate              = f_pPolicy->dwEndDate;
    pCDKBCryptoData->dwPlayEnablers         = f_pPolicy->dwPlayEnablers;
    pCDKBCryptoData->fRealTimeExpiration    = f_pPolicy->fRealTimeExpirationPresent;
    pCDKBCryptoData->idSession              = f_pContext->idSession;
    pCDKBCryptoData->wSelwrityLevel         = f_pPolicy->wSelwrityLevel;

    /* Always enable Miracast bit */
    pCDKBCryptoData->dwPlayEnablers |= DRM_TEE_PLAY_ENABLER_TYPE_HDCP_MIRACAST;

    DRMCASSERT( sizeof(pCDKBCryptoData->rgwOPLs) == sizeof(f_pPolicy->rgwOutputProtectionLevels) );
    ChkVOID( OEM_TEE_MEMCPY( pCDKBCryptoData->rgwOPLs, f_pPolicy->rgwOutputProtectionLevels, sizeof(f_pPolicy->rgwOutputProtectionLevels) ) );

    for( iPolicy = 0; iPolicy < f_pPolicy->cRestrictions; iPolicy++ )
    {
        DRM_DWORD cbRestriction = f_pPolicy->pRestrictions[iPolicy].cbRestrictionConfiguration;

        pCDKBCryptoData->rgRestrictions[iPolicy].wCategory       = f_pPolicy->pRestrictions[iPolicy].wRestrictionCategory;
        pCDKBCryptoData->rgRestrictions[iPolicy].idType          = f_pPolicy->pRestrictions[iPolicy].idRestrictionType;
        pCDKBCryptoData->rgRestrictions[iPolicy].cbConfiguration = cbRestriction;

        if( cbRestriction > 0 )
        {
            ChkDR( DRM_DWordAdd( ibVariableData, cbRestriction, &ibEnd ) );
            DRMASSERT( ibEnd <= cbCDKBCryptoData );
            ChkBOOL( ibEnd <= cbCDKBCryptoData, DRM_E_BUFFERTOOSMALL );
            pCDKBCryptoData->rgRestrictions[iPolicy].ibConfiguration = ibVariableData;
            ChkVOID( OEM_TEE_MEMCPY_IDX( pCDKBCryptoData, ibVariableData, f_pPolicy->pRestrictions[iPolicy].pbRestrictionConfiguration, 0, cbRestriction ) );
            ibVariableData = ibEnd;
        }
    }

    if( f_fSelfContained )
    {
        if( pOemTeeCtx->cbUnprotectedOEMData > 0 )
        {
            ChkDR( DRM_DWordAdd( ibVariableData, pOemTeeCtx->cbUnprotectedOEMData, &ibEnd ) );
            DRMASSERT( ibEnd <= cbCDKBCryptoData );
            ChkBOOL( ibEnd <= cbCDKBCryptoData, DRM_E_BUFFERTOOSMALL );
            pCDKBCryptoData->oContext.cbCopyOfUnprotectedOEMData = pOemTeeCtx->cbUnprotectedOEMData;
            pCDKBCryptoData->oContext.ibCopyOfUnprotectedOEMData = ibVariableData;
            ChkVOID( OEM_TEE_MEMCPY_IDX( pCDKBCryptoData, ibVariableData, pOemTeeCtx->pbUnprotectedOEMData, 0, pOemTeeCtx->cbUnprotectedOEMData ) );
            ibVariableData = ibEnd;
        }
    }

    ChkDR( DRM_TEE_KB_Build(
        f_pContext,
        DRM_TEE_XB_KB_TYPE_CDKB,
        cbCDKBCryptoData,
        ( DRM_BYTE * )pCDKBCryptoData,
        f_pCDKB ) );

ErrorExit:
    ChkVOID( DRM_TEE_BASE_MemFree( f_pContext, ( DRM_VOID ** )&pCDKBCryptoData ) );
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL _KB_ParseCDKBCryptoData(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in_ecount( 1 )                                const DRM_TEE_KB_CDKBData        *f_pCDKBCryptoData,
    __in                                                  DRM_BOOL                    f_fSelfContained,
    __out                                                 DRM_DWORD                  *f_pcKeys,
    __deref_out_ecount( *f_pcKeys )                       DRM_TEE_KEY               **f_ppKeys,
    __out_ecount( 1 )                                     DRM_TEE_POLICY_INFO        *f_pPolicy )
{
    DRM_RESULT                  dr                      = DRM_SUCCESS;
    OEM_TEE_CONTEXT            *pOemTeeCtx              = NULL;
    DRM_DWORD                   cbUnprotectedOEMData    = 0;
    DRM_BYTE                   *pbUnprotectedOEMData    = NULL;
    DRM_TEE_KEY                *pKeys                   = NULL;
    DRM_DWORD                   cRestrictions           = 0;
    DRM_TEE_POLICY_RESTRICTION *pRestrictions           = NULL;
    DRM_TEE_KEY_TYPE            rgeAesTypes[ 3 ]        = { DRM_TEE_KEY_TYPE_PR_CI, DRM_TEE_KEY_TYPE_PR_CK, DRM_TEE_KEY_TYPE_SAMPLEPROT };

    AssertChkArg( f_pContext        != NULL );
    AssertChkArg( f_pCDKBCryptoData != NULL );
    AssertChkArg( f_pcKeys          != NULL );
    AssertChkArg( f_ppKeys          != NULL );
    AssertChkArg( f_pPolicy         != NULL );

    pOemTeeCtx = &f_pContext->oContext;
    AssertChkArg( !f_fSelfContained || pOemTeeCtx != NULL );

    DRMASSERT( f_pCDKBCryptoData->cKeys == 2 || f_pCDKBCryptoData->cKeys == 3 );

    /* Self-contained TEE context */
    if( f_fSelfContained )
    {
        if( f_pCDKBCryptoData->oContext.cbCopyOfUnprotectedOEMData > 0 )
        {
            DRM_DWORD ibUnprotectedOEMData = f_pCDKBCryptoData->oContext.ibCopyOfUnprotectedOEMData;
            DRM_DWORD ibEnd;

            cbUnprotectedOEMData = f_pCDKBCryptoData->oContext.cbCopyOfUnprotectedOEMData;

            ChkDR( DRM_DWordAdd( ibUnprotectedOEMData, cbUnprotectedOEMData, &ibEnd ) );
            ChkBOOL( ibEnd <= f_pCDKBCryptoData->cbCDKBData, DRM_E_BUFFER_BOUNDS_EXCEEDED );
            ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, cbUnprotectedOEMData, (DRM_VOID **)&pbUnprotectedOEMData ) );
            ChkVOID( OEM_TEE_MEMCPY( pbUnprotectedOEMData, (DRM_BYTE *)f_pCDKBCryptoData + ibUnprotectedOEMData, cbUnprotectedOEMData ) );
        }

        ChkVOID( DRM_TEE_BASE_MemFree( f_pContext, (DRM_VOID**)&pOemTeeCtx->pbUnprotectedOEMData ) );
        pOemTeeCtx->cbUnprotectedOEMData = cbUnprotectedOEMData;
        pOemTeeCtx->pbUnprotectedOEMData = pbUnprotectedOEMData;
        pbUnprotectedOEMData             = NULL;

        f_pContext->idSession = f_pCDKBCryptoData->idSession;
    }
    else
    {
        ChkBOOL( OEM_SELWRE_ARE_EQUAL( &f_pContext->idSession, &f_pCDKBCryptoData->idSession, sizeof(f_pCDKBCryptoData->idSession) ), DRM_E_TEE_ILWALID_KEY_DATA );
        DRMASSERT( f_pCDKBCryptoData->oContext.cbCopyOfUnprotectedOEMData == 0 );
    }

    ChkDR( DRM_TEE_BASE_IMPL_AllocateKeyArray( f_pContext, f_pCDKBCryptoData->cKeys, &pKeys ) );

    if( f_pCDKBCryptoData->cRestrictions > 0 )
    {
        DRM_DWORD cbRestrictions;
        DRM_DWORD iRestriction;

        ChkDR( DRM_DWordMult( f_pCDKBCryptoData->cRestrictions, sizeof(*pRestrictions), &cbRestrictions ) );
        ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, cbRestrictions, ( DRM_VOID ** )&pRestrictions ) );
        ChkVOID( OEM_TEE_ZERO_MEMORY( pRestrictions, cbRestrictions ) );

        for( iRestriction = 0; iRestriction < f_pCDKBCryptoData->cRestrictions; iRestriction++ )
        {
            DRM_DWORD cbRestriction = f_pCDKBCryptoData->rgRestrictions[iRestriction].cbConfiguration;

            pRestrictions[iRestriction].wRestrictionCategory       = f_pCDKBCryptoData->rgRestrictions[iRestriction].wCategory;
            pRestrictions[iRestriction].idRestrictionType          = f_pCDKBCryptoData->rgRestrictions[iRestriction].idType;
            pRestrictions[iRestriction].cbRestrictionConfiguration = cbRestriction;

            if( cbRestriction > 0 )
            {
                DRM_DWORD ibEnd;
                DRM_DWORD ibRestriction = f_pCDKBCryptoData->rgRestrictions[iRestriction].ibConfiguration;

                ChkDR( DRM_DWordAdd( ibRestriction, cbRestriction, &ibEnd ) );
                ChkBOOL( ibEnd <= f_pCDKBCryptoData->cbCDKBData, DRM_E_BUFFER_BOUNDS_EXCEEDED );
                ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, cbRestriction, ( DRM_VOID ** )&pRestrictions[iRestriction].pbRestrictionConfiguration ) );
                ChkVOID( OEM_TEE_MEMCPY( pRestrictions[iRestriction].pbRestrictionConfiguration, ( DRM_BYTE * )f_pCDKBCryptoData + ibRestriction, cbRestriction ) );
            }
        }
    }

    ChkDR( _CryptoData_UnwrapKeys(
        f_pContext,
        FALSE,
        NULL,
        DRM_TEE_XB_KB_TYPE_CDKB,
        0,
        NULL,
        0,
        NULL,
        NULL,
        f_pCDKBCryptoData->cKeys,
        f_pCDKBCryptoData->rgKeys,
        f_pCDKBCryptoData->cKeys,
        rgeAesTypes,
        pKeys ) );

    f_pPolicy->cRestrictions              = f_pCDKBCryptoData->cRestrictions;
    f_pPolicy->oKID                       = f_pCDKBCryptoData->oKID;
    f_pPolicy->oLID                       = f_pCDKBCryptoData->oLID;
    f_pPolicy->dwBeginDate                = f_pCDKBCryptoData->dwBeginDate;
    f_pPolicy->dwDecryptionMode           = f_pCDKBCryptoData->dwDecryptionMode;
    f_pPolicy->dwEndDate                  = f_pCDKBCryptoData->dwEndDate;
    f_pPolicy->dwPlayEnablers             = f_pCDKBCryptoData->dwPlayEnablers;
    f_pPolicy->fRealTimeExpirationPresent = f_pCDKBCryptoData->fRealTimeExpiration;
    f_pPolicy->wSelwrityLevel             = f_pCDKBCryptoData->wSelwrityLevel;
    f_pPolicy->pRestrictions              = pRestrictions;

    ChkVOID( OEM_TEE_MEMCPY( f_pPolicy->rgwOutputProtectionLevels, f_pCDKBCryptoData->rgwOPLs, sizeof(f_pPolicy->rgwOutputProtectionLevels) ) );

    if( f_pPolicy->dwDecryptionMode == OEM_TEE_AES128CTR_DECRYPTION_MODE_SAMPLE_PROTECTION )
    {
        ChkBOOL( f_pCDKBCryptoData->cKeys == 3, DRM_E_TEE_ILWALID_KEY_DATA );
    }
    else
    {
        ChkBOOL( f_pCDKBCryptoData->cKeys == 2, DRM_E_TEE_ILWALID_KEY_DATA );
    }

    pRestrictions = NULL;
    *f_pcKeys     = f_pCDKBCryptoData->cKeys;
    *f_ppKeys     = pKeys;
    pKeys         = NULL;

ErrorExit:
    ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, f_pCDKBCryptoData->cKeys, &pKeys ) );

    if( pRestrictions != NULL )
    {
        ChkVOID( DRM_TEE_KB_FreeCDKBCryptoDataRestrictionArray(
            f_pContext,
            cRestrictions,
            pRestrictions ) );
    }
    ChkVOID( DRM_TEE_BASE_MemFree( f_pContext, (DRM_VOID**)&pbUnprotectedOEMData ) );

    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_ParseAndVerifyCDKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pCDKB,
    __in                                                  DRM_BOOL                    f_fSelfContained,
    __out                                                 DRM_DWORD                  *f_pcKeys,
    __deref_out_ecount( *f_pcKeys )                       DRM_TEE_KEY               **f_ppKeys,
    __out_ecount( 1 )                                     DRM_TEE_POLICY_INFO        *f_pPolicy )
{
    DRM_RESULT            dr                      = DRM_SUCCESS;
    DRM_TEE_KB_CDKBData  *pCDKBCryptoDataWeakRef  = NULL;
    DRM_DWORD             cCDKBCryptoDataWeakRef  = 0;

    AssertChkArg( f_pContext       != NULL );
    AssertChkArg( f_pCDKB          != NULL );
    AssertChkArg( f_pcKeys         != NULL );
    AssertChkArg( f_ppKeys         != NULL );
    AssertChkArg( f_pPolicy        != NULL );

    ChkDR( DRM_TEE_KB_ParseAndVerifyWithReconstitution(
        f_pContext,
        f_pCDKB,
        DRM_TEE_XB_KB_TYPE_CDKB,
        &f_fSelfContained,
        &cCDKBCryptoDataWeakRef,
        ( DRM_BYTE ** )&pCDKBCryptoDataWeakRef ) );

    ChkDR( _KB_ParseCDKBCryptoData(
        f_pContext,
        pCDKBCryptoDataWeakRef,
        f_fSelfContained,
        f_pcKeys,
        f_ppKeys,
        f_pPolicy ) );

ErrorExit:
    return dr;

}

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_KB_FreeCDKBCryptoDataRestrictionArray(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                                  DRM_DWORD                   f_cRestrictions,
    __inout_opt                                           DRM_TEE_POLICY_RESTRICTION *f_pRestrictions )
{
    DRM_DWORD        iRestriction = 0;

    DRMASSERT( f_pContext != NULL );
    if( f_pContext != NULL )
    {
        if( f_cRestrictions  > 0
         && f_pRestrictions != NULL )
        {
            for( iRestriction = 0; iRestriction < f_cRestrictions; iRestriction++ )
            {
                ChkVOID( DRM_TEE_BASE_MemFree( f_pContext, (DRM_VOID**)&f_pRestrictions[iRestriction].pbRestrictionConfiguration ) );
            }
        }

        ChkVOID( DRM_TEE_BASE_MemFree( f_pContext, (DRM_VOID**)&f_pRestrictions ) );
    }
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_BuildRKB(
    __inout_opt                                           DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_RKB_CONTEXT        *f_pRKBContext,
    __in                                                  DRM_DWORD                   f_cCrlDigestEntries,
    __in_ecount_opt( f_cCrlDigestEntries )          const DRM_RevocationEntry        *f_pCrlDigestEntries,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_pRKB )
{
    DRM_RESULT          dr              = DRM_SUCCESS;
    DRM_DWORD           cbRKBCryptoData = 0;
    DRM_TEE_KB_RKBData *pRKBCryptoData  = NULL;
    DRM_DWORD           iCrl            = 0;

    AssertChkArg( f_pContext         != NULL );
    AssertChkArg( f_pRKBContext      != NULL );
    AssertChkArg( f_pRKB             != NULL );
    AssertChkArg( (f_cCrlDigestEntries == 0) == (f_pCrlDigestEntries == NULL) );

    /* DRM_TEE_KB_RKBData has one CRL entry already */
    if( f_cCrlDigestEntries > 1 )
    {
        ChkDR( DRM_DWordMult( f_cCrlDigestEntries - 1, sizeof(DRM_RevocationEntry), &cbRKBCryptoData ) );
    }

    ChkDR( DRM_DWordAddSame( &cbRKBCryptoData, sizeof(DRM_TEE_KB_RKBData) ) );
    ChkDR( DRM_TEE_BASE_MemAlloc( f_pContext, cbRKBCryptoData, ( DRM_VOID ** )&pRKBCryptoData ) );
    ChkVOID( OEM_TEE_ZERO_MEMORY( pRKBCryptoData, cbRKBCryptoData ) );

    pRKBCryptoData->oContext          = *f_pRKBContext;
    pRKBCryptoData->cbRKBData         = cbRKBCryptoData;
    pRKBCryptoData->cCrlDigestEntries = f_cCrlDigestEntries;

    for( iCrl = 0; iCrl < f_cCrlDigestEntries; iCrl++ )
    {
        ChkVOID( OEM_TEE_MEMCPY( pRKBCryptoData->rgCrlDigestEntries[iCrl], f_pCrlDigestEntries[iCrl], sizeof(f_pCrlDigestEntries[iCrl]) ) );
    }

    ChkDR( DRM_TEE_KB_Build(
        f_pContext,
        DRM_TEE_XB_KB_TYPE_RKB,
        cbRKBCryptoData,
        ( DRM_BYTE * )pRKBCryptoData,
        f_pRKB ) );

ErrorExit:
    ChkVOID( DRM_TEE_BASE_MemFree( f_pContext, ( DRM_VOID ** )&pRKBCryptoData ) );
    return dr;
}

PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_UNINITIALIZED_MEMORY_6001, "Ignore Using Uninitialized Memory for *f_pcCrlDigestEntries since it not used unless it is checked for NULL." );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_ParseAndVerifyRKB(
    __inout                                            DRM_TEE_CONTEXT              *f_pContext,
    __in                                         const DRM_TEE_BYTE_BLOB            *f_pRKB,
    __out                                              DRM_TEE_RKB_CONTEXT          *f_pRkbCtx,
    __in                                               DRM_DWORD                     f_cDigests,
    __in_ecount_opt(f_cDigests)                  const DRM_RevocationEntry          *f_pDigests )
{
    DRM_RESULT            dr                       = DRM_SUCCESS;
    DRM_TEE_KB_RKBData   *pRKBCryptoDataWeakRef    = NULL;
    DRM_DWORD             cbRKBCryptoDataWeakRef   = 0;

    AssertChkArg( f_pContext       != NULL );
    AssertChkArg( f_pRKB           != NULL );
    AssertChkArg( f_pRkbCtx        != NULL );

    ChkDR( DRM_TEE_KB_ParseAndVerify(
        f_pContext,
        f_pRKB,
        DRM_TEE_XB_KB_TYPE_RKB,
        &cbRKBCryptoDataWeakRef,
        ( DRM_BYTE ** )&pRKBCryptoDataWeakRef ) );

    *f_pRkbCtx = pRKBCryptoDataWeakRef->oContext;

    if( f_pDigests != NULL )
    {
        DRM_DWORD idxInputDigest;

        for( idxInputDigest = 0; idxInputDigest < f_cDigests; idxInputDigest++ )
        {
            DRM_DWORD idxRKBDigest;
            for( idxRKBDigest = 0; idxRKBDigest < pRKBCryptoDataWeakRef->cCrlDigestEntries; idxRKBDigest++ )
            {
                if( OEM_SELWRE_ARE_EQUAL( &pRKBCryptoDataWeakRef->rgCrlDigestEntries[idxRKBDigest], f_pDigests[idxInputDigest] , sizeof(DRM_RevocationEntry) ) )
                {
                    ChkDR( DRM_E_CERTIFICATE_REVOKED );
                }
            }
        }

    }

ErrorExit:
    return dr;
}
#endif
#ifdef NONE

PREFAST_POP;

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_BuildCEKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                                  DRM_BOOL                    f_fRoot,
    __in                                                  DRM_DWORD                   f_dwEncryptionMode,
    __in                                                  DRM_DWORD                   f_dwSelwrityLevel,
    __in_ecount( 2 )                                const DRM_TEE_KEY                *f_pKeys,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_pCEKB )
{
    DRM_RESULT           dr              = DRM_SUCCESS;
    DRM_TEE_KB_CEKBData  oCEKBCryptoData = { 0 };

    AssertChkArg( f_pContext        != NULL );
    AssertChkArg( f_pKeys           != NULL );
    AssertChkArg( f_pKeys[0].eType  == DRM_TEE_KEY_TYPE_PR_CI );
    AssertChkArg( f_pKeys[1].eType  == DRM_TEE_KEY_TYPE_PR_CK );

    ChkDR( _CryptoData_WrapKeys(
        f_pContext,
        FALSE,
        DRM_TEE_XB_KB_TYPE_CEKB,
        0,
        NULL,
        NULL,
        2,
        f_pKeys,
        oCEKBCryptoData.rgCICK,
        NULL ) );

    oCEKBCryptoData.dwEncryptionMode = f_dwEncryptionMode;
    oCEKBCryptoData.dwSelwrityLevel  = f_dwSelwrityLevel;
    oCEKBCryptoData.fRoot            = f_fRoot;

    ChkDR( DRM_TEE_KB_Build(
        f_pContext,
        DRM_TEE_XB_KB_TYPE_CEKB,
        sizeof(oCEKBCryptoData),
        ( DRM_BYTE * )&oCEKBCryptoData,
        f_pCEKB ) );

ErrorExit:
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_ParseAndVerifyCEKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pCEKB,
    __out_opt                                             DRM_BOOL                   *f_pfRoot,
    __out_opt                                             DRM_DWORD                  *f_pdwSelwrityLevel,
    __out_opt                                             DRM_DWORD                  *f_pdwEncryptionMode,
    __out                                                 DRM_DWORD                  *f_pcKeys,
    __deref_out_ecount( *f_pcKeys )                       DRM_TEE_KEY               **f_ppKeys )
{
    DRM_RESULT            dr                      = DRM_SUCCESS;
    DRM_TEE_KB_CEKBData  *pCEKBCryptoDataWeakRef  = NULL;
    DRM_DWORD             cCEKBCryptoDataWeakRef  = 0;
    DRM_TEE_KEY          *pKeys                   = NULL;
    DRM_TEE_KEY_TYPE      rgeAesTypes[ 2 ]        = { DRM_TEE_KEY_TYPE_PR_CI, DRM_TEE_KEY_TYPE_PR_CK };

    AssertChkArg( f_pContext  != NULL );
    AssertChkArg( f_pCEKB     != NULL );
    AssertChkArg( f_pcKeys    != NULL );
    AssertChkArg( f_ppKeys    != NULL );

    ChkDR( DRM_TEE_KB_ParseAndVerify(
        f_pContext,
        f_pCEKB,
        DRM_TEE_XB_KB_TYPE_CEKB,
        &cCEKBCryptoDataWeakRef,
        ( DRM_BYTE ** )&pCEKBCryptoDataWeakRef ) );

    ChkBOOL( cCEKBCryptoDataWeakRef == sizeof(*pCEKBCryptoDataWeakRef), DRM_E_TEE_ILWALID_KEY_DATA );

    ChkDR( DRM_TEE_BASE_IMPL_AllocateKeyArray( f_pContext, DRM_NO_OF( rgeAesTypes ), &pKeys ) );

    ChkDR( _CryptoData_UnwrapKeys(
        f_pContext,
        FALSE,
        NULL,
        DRM_TEE_XB_KB_TYPE_CEKB,
        0,
        NULL,
        0,
        NULL,
        NULL,
        DRM_NO_OF( rgeAesTypes ),
        pCEKBCryptoDataWeakRef->rgCICK,
        DRM_NO_OF( rgeAesTypes ),
        rgeAesTypes,
        pKeys ) );

    if( f_pfRoot != NULL )
    {
        *f_pfRoot = pCEKBCryptoDataWeakRef->fRoot;
    }
    if( f_pdwEncryptionMode != NULL )
    {
        *f_pdwEncryptionMode = pCEKBCryptoDataWeakRef->dwEncryptionMode;
    }
    if( f_pdwSelwrityLevel != NULL )
    {
        *f_pdwSelwrityLevel = pCEKBCryptoDataWeakRef->dwSelwrityLevel;
    }

    *f_pcKeys = DRM_NO_OF( rgeAesTypes );
    *f_ppKeys = pKeys;
    pKeys     = NULL;

ErrorExit:
    ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, DRM_NO_OF( rgeAesTypes ), &pKeys ) );
    return dr;
}
#endif
#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_BuildNKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in_ecount( 1 )                                const DRM_ID                     *f_pNonce,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_pNKB )
{
    DRM_RESULT dr = DRM_SUCCESS;

    AssertChkArg( f_pContext != NULL );
    AssertChkArg( f_pNonce   != NULL );
    AssertChkArg( f_pNKB     != NULL );

    ChkDR( DRM_TEE_KB_Build(
        f_pContext,
        DRM_TEE_XB_KB_TYPE_NKB,
        sizeof(DRM_ID),
        ( DRM_BYTE * )f_pNonce,
        f_pNKB ) );

ErrorExit:
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_ParseAndVerifyNKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pNKB,
    __out_ecount(1)                                       DRM_ID                     *f_pNonce )
{
    DRM_RESULT          dr                     = DRM_SUCCESS;
    DRM_DWORD           cbNKBCryptoDataWeakRef = 0;
    DRM_BYTE           *pNKBCryptoDataWeakRef  = NULL;

    AssertChkArg( f_pContext  != NULL );
    AssertChkArg( f_pNKB      != NULL );
    AssertChkArg( f_pNonce    != NULL );

    /* Parse the given NKB and verify its signature. */
    ChkDR( DRM_TEE_KB_ParseAndVerify(
        f_pContext,
        f_pNKB,
        DRM_TEE_XB_KB_TYPE_NKB,
        &cbNKBCryptoDataWeakRef,
        ( DRM_BYTE ** )&pNKBCryptoDataWeakRef ) );
    ChkBOOL( cbNKBCryptoDataWeakRef == sizeof(DRM_ID), DRM_E_TEE_ILWALID_KEY_DATA );

    ChkVOID( OEM_TEE_MEMCPY( f_pNonce, pNKBCryptoDataWeakRef, sizeof(DRM_ID) ) );

ErrorExit:
    return dr;
}
#endif
#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_FinalizePPKB(
    __inout                     DRM_TEE_CONTEXT                     *f_pContext,
    __in                        DRM_DWORD                            f_cKeys,
    __in                  const DRM_TEE_KEY                         *f_poKeys,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB                   *f_poPrevPPKB,
    __out                       DRM_TEE_BYTE_BLOB                   *f_poPPKB )
{
    DRM_RESULT                 dr              = DRM_SUCCESS;
    DRM_DWORD                  cPrevKeys       = 0;
    DRM_TEE_KEY               *pPrevKeys       = NULL;
    DRM_TEE_KEY                oPTK            = DRM_TEE_KEY_EMPTY;
    DRM_TEE_KEY                oPTKD           = DRM_TEE_KEY_EMPTY;
    DRM_TEE_KEY                oSSTKey         = DRM_TEE_KEY_EMPTY;
    OEM_TEE_CONTEXT           *pOemTeeCtx      = NULL;

    DRMASSERT( f_pContext != NULL );
    pOemTeeCtx = &f_pContext->oContext;

    ChkDR( DRM_TEE_BASE_IMPL_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_PR_SST, &oSSTKey ) );

    if( IsBlobAssigned( f_poPrevPPKB ) )
    {
        DRM_BOOL                fPersist            = TRUE;
        DRM_TEE_KB_PPKBData     oPreviousPPKB       = {0};
        DRM_TEE_KB_PPKBData    *pPrevPPKB           = NULL;
        DRM_DWORD               cbWrappedKeyBuffer  = sizeof( oPreviousPPKB.oSSTKey );
        DRM_DWORD               ibWrapperKeyBuffer  = 0;

        pPrevPPKB = &oPreviousPPKB;
        ChkDR( DRM_TEE_KB_ParseAndVerifyPPKB( f_pContext, f_poPrevPPKB, &cPrevKeys, &pPrevKeys, pPrevPPKB ) );

        oPTK.dwidTK = pPrevPPKB->dwidTK;

        ChkDR( DRM_TEE_BASE_IMPL_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_TK, &oPTK ) );
        ChkDR( OEM_TEE_BASE_GetTKByID( pOemTeeCtx, pPrevPPKB->dwidTK, &oPTK.oKey ) );
        ChkDR( DRM_TEE_BASE_IMPL_DeriveTKDFromTK( f_pContext, &oPTK, DRM_TEE_XB_KB_TYPE_PPKB, DRM_TEE_XB_KB_OPERATION_ENCRYPT, &fPersist, &oPTKD ) );
        ChkDR( OEM_TEE_BASE_UnwrapAES128KeyFromPersistedStorage(
            pOemTeeCtx,
            &oPTKD.oKey,
            &cbWrappedKeyBuffer,
            &ibWrapperKeyBuffer,
            ( const DRM_BYTE * )&pPrevPPKB->oSSTKey,
            &oSSTKey.oKey ) );
    }
    else
    {
        /* Only create a new SST key if we don't have a PPKB (which will already have the one we previously created). */
        ChkDR( OEM_TEE_BASE_GenerateRandomAES128Key( pOemTeeCtx, &oSSTKey.oKey ) );
    }

    /* create PPKB */
    ChkDR( DRM_TEE_KB_BuildPPKB( f_pContext, f_cKeys, f_poKeys, &oSSTKey, f_poPPKB ) );

ErrorExit:
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &oPTKD.oKey   ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &oPTK.oKey    ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &oSSTKey.oKey ) );
    ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, cPrevKeys, &pPrevKeys ) );

    return dr;
}
#endif
#ifdef NONE
/*
** special usage of the key blob infrastructure: decrypt with aes cbc and validate provisioning oem context
** No other key blob does bulk encrypt of payload
** Note: f_pRProvContextKB->pb gets corrupted.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_UnpackRemoteProvisioningBlob(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pRProvContextKB,
    __out_ecount( 1 )                                     DRM_DWORD                  *f_pcbBlob,
    __deref_out_ecount( *f_pcbBlob )                      DRM_BYTE                  **f_ppbBlob )
{
    DRM_RESULT                  dr                    = DRM_SUCCESS;
    DRM_BYTE                   *pbCryptoDataWeakRef   = NULL;
    DRM_DWORD                   cbCryptoDataWeakRef   = 0;
    DRM_BYTE                   *pbCryptoDataDecrypted = NULL;
    DRM_DWORD                   cbCryptoDataDecrypted = 0;
    OEM_TEE_CONTEXT            *pOemTeeCtx            = NULL;
    DRM_TEE_KEY                 oTKD                  = DRM_TEE_KEY_EMPTY;
    DRM_DWORD                   dwPadding; /* Initialized later in this function */

    pOemTeeCtx = &f_pContext->oContext;

    ChkDR( DRM_TEE_KB_ParseAndVerify(
        f_pContext,
        f_pRProvContextKB,
        DRM_TEE_XB_KB_TYPE_RPCKB,
        &cbCryptoDataWeakRef,
        &pbCryptoDataWeakRef ) );

    DRMASSERT( pbCryptoDataWeakRef != NULL );
    ChkDR( DRM_TEE_BASE_IMPL_DeriveTKDFromTK( f_pContext, NULL, DRM_TEE_XB_KB_TYPE_RPCKB, DRM_TEE_XB_KB_OPERATION_ENCRYPT, NULL, &oTKD ) );

    /* there has to be some content */
    ChkArg( cbCryptoDataWeakRef >= 2*DRM_AES_BLOCKLEN );

    /* decrypt pbCryptoDataWeakRef, validate it and return. */
    ChkDR( OEM_TEE_BASE_AES128CBC_DecryptData(
        pOemTeeCtx,
        &oTKD.oKey,
        pbCryptoDataWeakRef,
        cbCryptoDataWeakRef - DRM_AES_BLOCKLEN,
        pbCryptoDataWeakRef + DRM_AES_BLOCKLEN ) );

    dwPadding = pbCryptoDataWeakRef[cbCryptoDataWeakRef-1];
    ChkArg( dwPadding > 0 && dwPadding <= DRM_AES_BLOCKLEN );

    cbCryptoDataDecrypted = cbCryptoDataWeakRef - DRM_AES_BLOCKLEN - dwPadding;
    ChkDR( DRM_TEE_BASE_MemAlloc( f_pContext, cbCryptoDataDecrypted, ( DRM_VOID ** )&pbCryptoDataDecrypted ) );
    ChkVOID( OEM_TEE_MEMCPY( pbCryptoDataDecrypted, pbCryptoDataWeakRef + DRM_AES_BLOCKLEN, cbCryptoDataDecrypted ) );

    *f_ppbBlob = pbCryptoDataDecrypted;
    *f_pcbBlob = cbCryptoDataDecrypted;
    pbCryptoDataDecrypted = NULL;
ErrorExit:
    /* Clean up decrypted crypto data */
    if ( pbCryptoDataWeakRef != NULL )
    {
        ChkVOID( OEM_SELWRE_MEMSET( pbCryptoDataWeakRef, 0, cbCryptoDataWeakRef ) );
    }
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &oTKD.oKey ) );
    ChkVOID( DRM_TEE_BASE_MemFree( f_pContext, ( DRM_VOID ** )&pbCryptoDataDecrypted ) );

    return dr;
}

/* special usage of the key blob infrastructure: encrypt with aes cbc and sign provisioning oem context */
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_PackRemoteProvisioningBlob(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                                  DRM_DWORD                   f_cbBlob,
    __in_bcount(f_cbBlob)                           const DRM_BYTE                   *f_pbBlob,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_pRProvContextKB )
{
    DRM_RESULT                  dr              = DRM_SUCCESS;
    OEM_TEE_CONTEXT             *pOemTeeCtx     = NULL;
    DRM_DWORD                   cbCryptoData;   /* Initialized later */
    DRM_BYTE                    *pbCryptoData   = NULL;
    DRM_TEE_KEY                 oTKD            = DRM_TEE_KEY_EMPTY;

    ChkDR( DRM_TEE_BASE_IMPL_DeriveTKDFromTK( f_pContext, NULL, DRM_TEE_XB_KB_TYPE_RPCKB, DRM_TEE_XB_KB_OPERATION_ENCRYPT, NULL, &oTKD ) );

    /* the crypto data is guid, IV, encrypted blocks */
    cbCryptoData = 2 * DRM_AES_BLOCKLEN + f_cbBlob;
    pOemTeeCtx = &f_pContext->oContext;

    /* allocate IV, f_cbBlob and 16 bytes more for padding purposes */
    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, cbCryptoData, (DRM_VOID**)&pbCryptoData ) );

    ChkDR( DRM_TEE_BASE_IMPL_AES128CBC(
        f_pContext,
        f_cbBlob,
        f_pbBlob,
        &oTKD,
        &cbCryptoData,
        pbCryptoData ) );

    ChkDR( DRM_TEE_KB_Build( f_pContext, DRM_TEE_XB_KB_TYPE_RPCKB, cbCryptoData, pbCryptoData, f_pRProvContextKB ) );

ErrorExit:
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &oTKD.oKey ) );
    ChkVOID( DRM_TEE_BASE_MemFree( f_pContext, (DRM_VOID**)&pbCryptoData ) );

    return dr;
}
#endif

#if !defined (SEC_USE_CERT_TEMPLATE) && defined (SEC_COMPILE)
/*
** Note: No corresponding Build function.  Only the generatedevicecert.exe tool creates the LPKB.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_ParseAndVerifyLPKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pLPKB,
    __out                                                 DRM_DWORD                  *f_pcECCKeysPrivate,
    __out_ecount( *f_pcECCKeysPrivate )                   DRM_TEE_KEY               **f_ppECCKeysPrivate )
{
    DRM_RESULT                       dr                         = DRM_SUCCESS;
    DRM_TEE_KB_LPKBData             *pLPKBCryptoDataWeakRef     = NULL;
    DRM_DWORD                        cbLPKBCryptoDataWeakRef    = 0;
    DRM_TEE_KEY                     *pECCKeysPrivate            = NULL;
    DRM_DWORD                        cECCKeysPrivate            = 0;
    static const DRM_TEE_KEY_TYPE    rgeEccTypes[ 3 ]           = { DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_SIGN, DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_ENCRYPT, DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_PRND };

    AssertChkArg( f_pContext != NULL );
    AssertChkArg( f_pLPKB != NULL );
    AssertChkArg( f_pcECCKeysPrivate != NULL );
    AssertChkArg( f_ppECCKeysPrivate != NULL );

    ChkDR( DRM_TEE_KB_ParseAndVerify(
        f_pContext,
        f_pLPKB,
        DRM_TEE_XB_KB_TYPE_LPKB,
        &cbLPKBCryptoDataWeakRef,
        (DRM_BYTE **)&pLPKBCryptoDataWeakRef ) );

    ChkBOOL( cbLPKBCryptoDataWeakRef == sizeof( *pLPKBCryptoDataWeakRef ), DRM_E_TEE_ILWALID_KEY_DATA );
    ChkBOOL( pLPKBCryptoDataWeakRef->cKeys == 2 || pLPKBCryptoDataWeakRef->cKeys == 3, DRM_E_TEE_ILWALID_KEY_DATA );
    cECCKeysPrivate = pLPKBCryptoDataWeakRef->cKeys;

    ChkDR( DRM_TEE_BASE_IMPL_AllocateKeyArray( f_pContext, cECCKeysPrivate, &pECCKeysPrivate ) );

    ChkDR( _CryptoData_UnwrapKeys(
        f_pContext,
        TRUE,
        NULL,
        DRM_TEE_XB_KB_TYPE_LPKB,
        cECCKeysPrivate,
        pLPKBCryptoDataWeakRef->rgKeys,
        cECCKeysPrivate,
        rgeEccTypes,
        pECCKeysPrivate,
        0,
        NULL,
        0,
        NULL,
        NULL ) );

    *f_ppECCKeysPrivate = pECCKeysPrivate;
    pECCKeysPrivate = NULL;
    *f_pcECCKeysPrivate = cECCKeysPrivate;

ErrorExit:
    if( pECCKeysPrivate != NULL )
    {
        ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, cECCKeysPrivate, &pECCKeysPrivate ) );
    }
    return dr;
}

#endif
EXIT_PK_NAMESPACE_CODE;

