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
#include <drmerr.h>
#include <drmteebase.h>
#include <oemtee.h>
#include <drmrivcrlparser.h>
#include <oembyteorder.h>
#include <drmlicgentypes.h>
#include <drmmathsafe.h>
#include <drmxmrformatparser.h>
#include <drmbcertformatparser.h>
#include <drmteecache.h>
#include <drmlastinclude.h>

DRM_DEFINE_LOCAL_GUID( s_guidPlayReadyRuntimeCrl, 0x4E9D8C8A, 0xB652, 0x45A7, 0x97, 0x91, 0x69, 0x25, 0xA6, 0xB4, 0x79, 0x1F );

ENTER_PK_NAMESPACE_CODE;

#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_TEE_IMPL_REVOCATION_IsREVOCATIONSupported( DRM_VOID )
{
    return TRUE;
}

PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_USING_FAILED_RESULT_6102, "Ignore Using Failed Result warning since out parameters are all initialized." );

static DRM_GLOBAL_CONST PUBKEY_P256 s_rgRevocationDataPubKeyAllowList[] PR_ATTR_DATA_OVLY(_s_rgRevocationDataPubKeyWhitelist) =
{ {
    {
        0x3F, 0x3C, 0x09, 0x41, 0xB3, 0xE2, 0x45, 0xC4,
        0xF0, 0x55, 0x32, 0xF1, 0x00, 0x40, 0xAA, 0x48,
        0xFD, 0x2A, 0xC8, 0x44, 0x23, 0x68, 0x2D, 0xBF,
        0x45, 0xFE, 0x2A, 0x65, 0xFF, 0x4E, 0xFF, 0x3A,
        0x60, 0xC4, 0x2A, 0x71, 0x38, 0x61, 0xA3, 0xA7,
        0xBC, 0x89, 0xB3, 0xE7, 0xB9, 0xA4, 0xF4, 0xAA,
        0xA2, 0x8B, 0xA8, 0xCE, 0xE6, 0x89, 0xBA, 0x8D,
        0xF7, 0xB0, 0x1B, 0x6A, 0x79, 0xC7, 0xDC, 0x93,
    },
} };

static DRM_NO_INLINE DRM_RESULT DRM_CALL _VerifyRevocationDataSigningPubKeyAgainstAllowList( __in const PUBKEY_P256 *f_pPubKey )
{
    DRM_DWORD idx;
    for( idx = 0; idx < DRM_NO_OF( s_rgRevocationDataPubKeyAllowList ); idx++ )
    {
        if( OEM_SELWRE_ARE_EQUAL( f_pPubKey->m_rgbPubkey, s_rgRevocationDataPubKeyAllowList[ idx ].m_rgbPubkey, sizeof( *f_pPubKey ) ) )
        {
            return DRM_SUCCESS;
        }
    }
    return DRM_E_ILWALID_REVOCATION_LIST;
}

static DRM_NO_INLINE DRM_RESULT DRM_CALL _ParseAndVerifyCRL(
    __inout                         DRM_TEE_CONTEXT       *f_pContext,
    __in_bcount( f_cbCrl )    const DRM_BYTE              *f_pbCrl,
    __in                            DRM_DWORD              f_cbCrl,
    __out                           DRM_BCRL              *f_pDRM_BCRL );
static DRM_NO_INLINE DRM_RESULT DRM_CALL _ParseAndVerifyCRL(
    __inout                         DRM_TEE_CONTEXT       *f_pContext,
    __in_bcount( f_cbCrl )    const DRM_BYTE              *f_pbCrl,
    __in                            DRM_DWORD              f_cbCrl,
    __out                           DRM_BCRL              *f_pDRM_BCRL )
{
    DRM_RESULT                         dr                    = 0;
    DRM_DWORD                          cbSignedMessageLength = 0;
    DRM_BCRL_Signed                    oSignedCRL;           /* Initialized in DRM_BCrl_ParseSignedCrl */
    OEM_TEE_CONTEXT                   *pOemTeeCtx            = NULL;
    DRM_BCERTFORMAT_PARSER_CHAIN_DATA *pChainData            = NULL;
    PUBKEY_P256                        oPubKey;              /* Initialized by OEM_TEE_MEMCPY */

    DRMASSERT( f_pbCrl != NULL );
    DRMASSERT( f_cbCrl >  0    );

    pOemTeeCtx = &f_pContext->oContext;

    ChkDR( DRM_BCrl_ParseSignedCrl( f_pbCrl, f_cbCrl, &cbSignedMessageLength, &oSignedCRL ) );

    if( oSignedCRL.cbCertificateChain == sizeof( oPubKey ) )
    {
        /*
        ** The certificate chain was replaced with just the pubkey which signs the CRL.
        ** We will confirm that the pubkey is in our allow list of valid pubkeys.
        ** If it is, we'll verify the signature using that pubkey.  Otherwise, we'll fail.
        ** This codepath eliminates both the need to pass the (large) certificate chain
        ** that signs the CRL into the TEE and the need to do full certificate chain
        ** parsing/validation for that chain.
        */

        ChkVOID( OEM_TEE_MEMCPY( &oPubKey, oSignedCRL.pbCertificateChain, oSignedCRL.cbCertificateChain ) );
        ChkDR( _VerifyRevocationDataSigningPubKeyAgainstAllowList( &oPubKey ) );
    }
    else
    {
        ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, sizeof( *pChainData ), (DRM_VOID **)&pChainData ) );
        ChkDR( DRM_TEE_IMPL_BASE_ParseCertificateChain(
            pOemTeeCtx,
            NULL,                                       /* f_pftLwrrentTime                 */
            DRM_BCERT_CERTTYPE_CRL_SIGNER,              /* f_dwExpectedLeafCertType         */
            oSignedCRL.cbCertificateChain,
            oSignedCRL.pbCertificateChain,
            pChainData ) );
        ChkBOOL( DRM_BCERTFORMAT_PARSER_CHAIN_DATA_IS_KEY_VALID( pChainData->dwValidKeyMask, DRM_BCERTFORMAT_CHAIN_DATA_KEY__SIGN_CRL ), DRM_E_BCERT_NO_PUBKEY_WITH_REQUESTED_KEYUSAGE );
        ChkVOID( OEM_TEE_MEMCPY( &oPubKey, &pChainData->rgoLeafPubKeys[ DRM_BCERTFORMAT_CHAIN_DATA_KEY__SIGN_CRL ], sizeof( oPubKey ) ) );
    }

    {
        SIGNATURE_P256 oSignatureData; /* Initialized by OEM_TEE_MEMCPY */

        ChkVOID( OEM_TEE_MEMCPY( oSignatureData.m_rgbSignature, oSignedCRL.Signature.rgb, ECDSA_P256_SIGNATURE_SIZE_IN_BYTES ) );

        ChkDR( Oem_Broker_ECDSA_P256_Verify(
            pOemTeeCtx,
            NULL,
            f_pbCrl,
            cbSignedMessageLength,
            &oPubKey,
            &oSignatureData ) );
    }

    *f_pDRM_BCRL = oSignedCRL.Crl;

ErrorExit:
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, ( DRM_VOID ** )&pChainData ) );
    return dr;
}

static DRM_NO_INLINE DRM_RESULT DRM_CALL _ParseAndVerifyRevInfo(
    __inout                             OEM_TEE_CONTEXT       *f_pOemTeeCtx,
    __in_bcount( f_cbRevInfo )    const DRM_BYTE              *f_pbRevInfo,
    __in                                DRM_DWORD              f_cbRevInfo,
    __out                               DRM_RLVI              *f_poRLVI )
{
    DRM_RESULT  dr        = DRM_SUCCESS;
    DRM_DWORD   cbSigned  = 0;
    PUBKEY_P256 oPubKey;  /* Initialized by OEM_TEE_MEMCPY */

    ChkDR( DRM_RVK_ParseRevocationInfo( f_pbRevInfo, f_cbRevInfo, f_poRLVI, &cbSigned ) );

    if( f_poRLVI->certchain.cbCertChain == sizeof( oPubKey ) )
    {
        /*
        ** The certificate chain was replaced with just the pubkey which signs the RevInfo.
        ** We will confirm that the pubkey is in our allow list of valid pubkeys.
        ** If it is, we'll verify the signature using that pubkey.  Otherwise, we'll fail.
        ** This codepath eliminates both the need to pass the (large) certificate chain
        ** that signs the RevInfo into the TEE and the need to do full certificate chain
        ** parsing/validation for that chain.
        */

        SIGNATURE_P256 oSignature;   /* Initialized by OEM_TEE_MEMCPY */

        ChkVOID( OEM_TEE_MEMCPY( &oPubKey, &f_pbRevInfo[ f_poRLVI->certchain.ibCertChain ], sizeof( oPubKey ) ) );
        ChkDR( _VerifyRevocationDataSigningPubKeyAgainstAllowList( &oPubKey ) );

        ChkBOOL( sizeof( oSignature ) == f_poRLVI->signature.cbSignature, DRM_E_ILWALID_REVOCATION_LIST );
        ChkVOID( OEM_TEE_MEMCPY( &oSignature, &f_pbRevInfo[ f_poRLVI->signature.ibSignature ], sizeof( oSignature ) ) );

        ChkDR( Oem_Broker_ECDSA_P256_Verify(
            f_pOemTeeCtx,
            NULL,
            f_pbRevInfo,
            cbSigned,
            &oPubKey,
            &oSignature ) );
    }
    else
    {
        ChkDR( DRM_RVK_VerifyRevocationInfoV2Only( f_pOemTeeCtx, NULL, f_pbRevInfo, f_cbRevInfo, f_poRLVI ) );
    }

ErrorExit:
    return dr;
}

/*
** Synopsis:
**
** This function takes a a Revocation Information Blob (including CRLs)
** and returns a subset of the blob symmetrically rebound to a TEE-specific
** key thus allowing it to be used without further asymmetric crypto operations.
**
** Operations Performed:
**
**  1. Verify the signature on the given DRM_TEE_CONTEXT.
**  2. Parse the given Revocation information Blob and verify all associated signatures.
**  3. Build and sign a RKB.
**  4. Return the new RKB.
**
** Arguments:
**
** f_pContext:      (in/out) The TEE context returned from
**                           DRM_TEE_BASE_AllocTEEContext.
** f_pRuntimeCRL:   (in)     The Runtime CRL.
** f_pRevInfo:      (in)     Revocation Info.
** f_pRKB:          (out)    RKB for the given RevInfo.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_REVOCATION_IngestRevocationInfo(
    __inout               DRM_TEE_CONTEXT              *f_pContext,
    __in_tee_opt    const DRM_TEE_BYTE_BLOB            *f_pRuntimeCRL,
    __in_tee_opt    const DRM_TEE_BYTE_BLOB            *f_pRevInfo,
    __out                 DRM_TEE_BYTE_BLOB            *f_pRKB )
{
    DRM_RESULT           dr         = DRM_SUCCESS;
    DRM_RESULT           drTmp      = DRM_SUCCESS;
    OEM_TEE_CONTEXT     *pOemTeeCtx = NULL;
    DRM_TEE_RKB_CONTEXT  oRKBCtx    = { 0 };
    DRM_TEE_BYTE_BLOB    oNewRKB    = DRM_TEE_BYTE_BLOB_EMPTY;

    DRMASSERT( f_pContext    != NULL );
    DRMASSERT( f_pRuntimeCRL != NULL );
    DRMASSERT( f_pRevInfo    != NULL );
    DRMASSERT( f_pRKB        != NULL );

    pOemTeeCtx = &f_pContext->oContext;

    if( IsBlobAssigned( f_pRevInfo ) && IsBlobAssigned( f_pRuntimeCRL ) )
    {
        DRM_UINT64                 uiExpirationDurationInTicks    = DRM_UI64LITERAL( 0, 0 );
        DRM_BCRL                   oCRL;                          /* OEM_TEE_ZERO_MEMORY */
        DRM_BCRL                  *pCRL                           = NULL;
        DRM_UINT64                 uiRLVIIssuedTime               = DRM_UI64LITERAL( 0, 0 );
        DRM_DWORD                  dwRuntimeCRLVersionFromRevInfo = 0;
        const DRM_RevocationEntry *pCrlList                       = NULL;
        DRM_RevocationEntry       *pRuntimeCrlList                = NULL;
        DRM_RLVI                   oRLVI;                         /* OEM_TEE_ZERO_MEMORY */

        ChkVOID( OEM_TEE_ZERO_MEMORY( &oCRL, sizeof( oCRL ) ) );
        ChkVOID( OEM_TEE_ZERO_MEMORY( &oRLVI, sizeof( oRLVI ) ) );

        ChkDR( _ParseAndVerifyRevInfo( pOemTeeCtx, f_pRevInfo->pb, f_pRevInfo->cb, &oRLVI ) );

        ChkDR( DRM_RVK_FindEntryInRevInfo( &oRLVI, f_pRevInfo->pb, f_pRevInfo->cb, &s_guidPlayReadyRuntimeCrl, &dwRuntimeCRLVersionFromRevInfo ) );

        uiExpirationDurationInTicks = MAX_REVOCATION_EXPIRE_TICS;

        FILETIME_TO_UI64( oRLVI.head.ftIssuedTime, uiRLVIIssuedTime );
        ChkDR( DRM_UInt64Add( uiRLVIIssuedTime, uiExpirationDurationInTicks, &oRKBCtx.uiRevInfoExpirationDate ) );

        ChkDR( _ParseAndVerifyCRL( f_pContext, f_pRuntimeCRL->pb, f_pRuntimeCRL->cb, &oCRL ) );
        pCRL = &oCRL;

        ChkBOOL( OEM_SELWRE_ARE_EQUAL( &s_guidPlayReadyRuntimeCrl, pCRL->Identifier, sizeof( DRM_GUID ) ), DRM_E_ILWALID_REV_INFO );
        ChkBOOL( pCRL->dwVersion >= dwRuntimeCRLVersionFromRevInfo, DRM_E_ILWALID_REV_INFO );

        oRKBCtx.dwRIV               = oRLVI.head.dwRIV;
        oRKBCtx.dwRuntimeCrlVersion = pCRL->dwVersion;
        pRuntimeCrlList             = pCRL->Entries;

        if( pCRL->cRevocationEntries > 0 )
        {
            DRM_DWORD cbTotalCrl;   /* Initialized by DRM_DWordMult */
            ChkDR( DRM_DWordMult( pCRL->cRevocationEntries, sizeof( DRM_RevocationEntry ), &cbTotalCrl ) );
            pCrlList = DRM_REINTERPRET_CONST_CAST( const DRM_RevocationEntry, pRuntimeCrlList );
        }

        /*
        ** Note: The RKB is session-bound.
        ** Thus, our global-across-all-sessions cache can't hold the RKB itself.
        ** Fortunately, building an RKB is cheap.
        */
        ChkDR( DRM_TEE_IMPL_KB_BuildRKB(
            f_pContext,
            &oRKBCtx,
            pCRL->cRevocationEntries,
            pCrlList,
            &oNewRKB ) );
    }
    else
    {
        /*
        ** Create an empty RKB in the case we were fed empty byte blobs for RevInfo or RuntimeCRL information
        */
        ChkDR( DRM_TEE_IMPL_KB_BuildRKB(
            f_pContext,
            &oRKBCtx,
            0,
            NULL,
            &oNewRKB ) );
    }

    ChkVOID( DRM_TEE_IMPL_BASE_TransferBlobOwnership( f_pRKB, &oNewRKB ) );

ErrorExit:
    //
    // LWE (kwilson) - Can't use ChkDR within ErrorExit. Cache the return value in drTmp and check value
    // of dr, before overwriting to be sure we are not overwriting a previous failure with success.
    //
    drTmp = DRM_TEE_IMPL_BASE_FreeBlob( f_pContext, &oNewRKB );
    if (dr == DRM_SUCCESS)
    {
        dr = drTmp;
    }

    return dr;
}
#endif

PREFAST_POP;

EXIT_PK_NAMESPACE_CODE;

