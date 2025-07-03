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
#include <drmteekbcryptodata.h>
#include <drmteexb.h>
#include <drmxmrformatparser.h>
#include <drmmathsafe.h>
#include <drmnonceverify.h>
#include <oemtee.h>
#include <oemparsers.h>
#include <oemaeskeywrap.h>
#include <drmnoncestore.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;
#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_TEE_LICPREP_IMPL_IsLICPREPSupported( DRM_VOID )
{
    return TRUE;
}

/*
** Maximum expected number of Licenses to be received by a PRND receiver during a single PRND session
** assuming key rotation.
*/
#define DRM_MAX_LICENSE_COUNT_FOR_NONCES (1024)

static DRM_RESULT DRM_CALL _VerifyInMemoryLicenseNonce(
    __inout               DRM_TEE_CONTEXT              *f_pContext,
    __in            const DRM_TEE_BYTE_BLOB            *f_pNKB,
    __in            const DRM_ID                       *f_pLID )
{
    DRM_RESULT  dr       = DRM_SUCCESS;
    DRM_ID      idNonce; /* Initialized later using DRM_TEE_KB_ParseNKBCryptoData */

    DRMASSERT( f_pContext != NULL );
    DRMASSERT( f_pNKB     != NULL );
    DRMASSERT( f_pLID     != NULL );

    ChkBOOL( IsBlobAssigned( f_pNKB ), DRM_E_NONCE_STORE_TOKEN_NOT_FOUND );

    /* Parse the given NKB and verify its signature. */
    ChkDR( DRM_TEE_KB_ParseAndVerifyNKB(
        f_pContext,
        f_pNKB,
        &idNonce ) );

    /* Check that the license is properly bound to the nonce */
    ChkDR( DRM_NONCE_VerifyNonce( &idNonce, f_pLID, DRM_MAX_LICENSE_COUNT_FOR_NONCES ) );

ErrorExit:
    return dr;
}

/*
** Synopsis:
**
** This function takes a license which is bound to the private device
** encryption key from the PlayReady leaf certificate Or is bound to
** the private domain encryption key from a domain certificate
** (which, in turn, is bound to the private device encryption
** key from the PlayReady leaf certificate) and symmetrically
** re-encrypts it to a TEE-specific key thus allowing the license to
** be used in the future without any asymmetric crypto operations.
**
** This function is called inside:
**  Drm_LicenseAcq_ProcessResponse
**  Drm_Prnd_Receiver_LicenseTransmit_Process
**
** Operations Performed:
**
**  1. Verify the signature on the given DRM_TEE_CONTEXT.
**  2. Parse the given PlayReady Private Key Blob (PPKB) and verify its signature.
**  3. Parse the given XMR license while ignoring any XMR objects which are
**     unnecessary for the remainder of this function.
**  4. If the OEM supports a clock inside the TEE, validate that the license
**     has not passed its end date.
**  5. If the license is a leaf license, halt and return an empty LKB.
**  6. If a Domain key Blob (DKB) was given, verify the license is domain-bound.
**  7. If the license is in-memory only, verify that the license ID is bound to
**     a Nonce from the provided NKB.
**  8. If a DKB was given, parse the given DKB and verify its signature.
**  9. Decrypt the license content keys (CI/CK) with the private key
**     associated with the public key to which the license is bound
**     (either the domain private key in the given DKB or the private
**     encryption key in the given PPKB).
** 10. Verify the license signature using CI.
** 11. Build and sign a License Key Blob (LKB).
** 12. Return the new LKB.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                DRM_TEE_BASE_AllocTEEContext.
** f_pPPKB               (in)     The current PlayReady Private Key Blob (PPKB).
** f_pLicense:           (in)     The raw binary XMR license.
**                                If the license is domain-bound and domains are not
**                                supported, this function will return DRM_E_NOTIMPL.
** f_pDKB:               (in)     The Domain Key Blob (DKB) to which the XMR license
**                                is bound, if any
**                                (i.e. one of the output values from
**                                DRM_TEE_DOM_PackageKeys).
**                                This parameter must have cb==0 for a license
**                                which is not domain-bound.
** f_pNKB:               (in/out) The Nonces Key Blob (NKB) which holds a list of valid
**                                nonces bound to the current session. To be used to verify
**                                the acquired in-memory only license.
** f_ppLKB:              (out)    The License Key Blob (LKB) for the given license
**                                ready for storage to disk OR ready to be held
**                                in memory for decryption for licenses that
**                                cannot be persisted.
**                                This should be freed with DRM_TEE_BASE_FreeBlob.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_LICPREP_PackageKey(
    __inout               DRM_TEE_CONTEXT              *f_pContext,
    __in_tee_opt    const DRM_TEE_BYTE_BLOB            *f_pPPKB,
    __in            const DRM_TEE_BYTE_BLOB            *f_pLicense,
    __in_tee_opt    const DRM_TEE_BYTE_BLOB            *f_pDKB,
    __in_tee_opt    const DRM_TEE_BYTE_BLOB            *f_pNKB,
    __out                 DRM_TEE_BYTE_BLOB            *f_pLKB )
{
    DRM_RESULT                   dr                     = DRM_SUCCESS;
    DRM_RESULT                   drTmp                  = DRM_SUCCESS;
    DRM_DWORD                    cKeys                  = 0;
    DRM_TEE_KEY                 *pKeys                  = NULL;
    DRM_TEE_XMR_LICENSE          oXmrLicense            = {0};
    DRM_BOOL                     fPersist               = FALSE;
    DRM_BOOL                     fScalableRoot          = FALSE;
    DRM_TEE_KEY                  rgoContentKeys[2]      = { DRM_TEE_KEY_EMPTY, DRM_TEE_KEY_EMPTY };
    DRM_TEE_KEY                 *pContentKeys           = NULL;
    DRM_DWORD                    cContentKeys           = 0;
    DRM_TEE_BYTE_BLOB            oLKB                   = DRM_TEE_BYTE_BLOB_EMPTY;
    OEM_TEE_CONTEXT             *pOemTeeCtx             = NULL;
    DRM_TEE_KEY                  oTKD                   = DRM_TEE_KEY_EMPTY;
    DRM_TEE_KEY                 *pDomainKey             = NULL;

    /* Arguments are checked in the PRITEE proxy layer */
    DRMASSERT( f_pContext != NULL );
    DRMASSERT( f_pPPKB    != NULL );
    DRMASSERT( f_pLicense != NULL );
    DRMASSERT( f_pDKB     != NULL );
    DRMASSERT( f_pLKB     != NULL );

    pOemTeeCtx = &f_pContext->oContext;

    /* Ensure that the Secure clock did not go out of sync */
    if( OEM_TEE_CLOCK_SelwreClockNeedsReSync( pOemTeeCtx ) )
    {
        ChkDR( DRM_E_TEE_CLOCK_DRIFTED );
    }

    ChkVOID( OEM_TEE_ZERO_MEMORY( f_pLKB, sizeof(*f_pLKB) ) );

    ChkDR( DRM_TEE_BASE_IMPL_ParseLicense( f_pContext, f_pLicense->cb, f_pLicense->pb, &oXmrLicense ) );

    ChkBOOL( oXmrLicense.oLicense.fValid, DRM_E_ILWALID_LICENSE );

    ChkArg( XMRFORMAT_IS_DOMAIN_ID_VALID( &oXmrLicense.oLicense ) == IsBlobAssigned( f_pDKB ) );

    /*
    ** Note: We allow storage of a license with a begin date in the future as it will become valid at a later point in time.
    ** Thus, we explicitly only validate the license end date in this function.
    ** During Bind, we will validate both begin an end date inside the TEE.
    */
    ChkDR( DRM_TEE_BASE_IMPL_ValidateLicenseExpiration( f_pContext, &oXmrLicense.oLicense, FALSE, TRUE ) );

    /* There is no need to generate an LKB for leaf licenses */
    if( !XMRFORMAT_IS_LEAF_LICENSE( &oXmrLicense.oLicense ) )
    {
        const DRM_BYTE                *pbCipherText                   = NULL;
        DRM_DWORD                      cbCipherText                   = 0;
        DRM_TEE_KEY_TYPE               eType                          = DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_ENCRYPT;
        DRM_TEE_KEY                   *pPrivateEncryptionKeyWeakRef   = NULL;
        const DRM_XMRFORMAT_DOMAIN_ID *pdomainID                      = &oXmrLicense.oLicense.OuterContainer.GlobalPolicyContainer.DomainID;

        fPersist = !XMRFORMAT_IS_CANNOT_PERSIST_LICENSE( &oXmrLicense.oLicense );
        fScalableRoot = XMRFORMAT_IS_AUX_KEY_VALID( &oXmrLicense.oLicense );

        ChkBOOL( XMRFORMAT_IS_CONTENT_KEY_VALID( &oXmrLicense.oLicense ), DRM_E_ILWALID_LICENSE );

        pbCipherText = XBBA_TO_PB( oXmrLicense.oLicense.OuterContainer.KeyMaterialContainer.ContentKey.xbbaEncryptedKey );
        AssertChkArg( pbCipherText != NULL );
        cbCipherText = XBBA_TO_CB( oXmrLicense.oLicense.OuterContainer.KeyMaterialContainer.ContentKey.xbbaEncryptedKey );
        AssertChkArg( cbCipherText > 0 );

        if ( !fPersist )
        {
            ChkDR( _VerifyInMemoryLicenseNonce( f_pContext, f_pNKB, &oXmrLicense.oLicense.HeaderData.LID ) );
        }

        if( oXmrLicense.oLicense.OuterContainer.KeyMaterialContainer.ContentKey.wKeyEncryptionCipherType == XMR_ASYMMETRIC_ENCRYPTION_TYPE_TEE_TRANSIENT )
        {
            ChkBOOL( DRM_TEE_LICGEN_IMPL_IsLICGENSupported(), DRM_E_ILWALID_LICENSE );

            ChkDR( DRM_TEE_BASE_AllocBlob( f_pContext, DRM_TEE_BLOB_ALLOC_BEHAVIOR_COPY, cbCipherText, pbCipherText, &oLKB ) );

            ChkDR( DRM_TEE_KB_ParseAndVerifyLKB( f_pContext, &oLKB, &cContentKeys, &pContentKeys ) );
            ChkBOOL( cContentKeys == 2, DRM_E_TEE_ILWALID_KEY_DATA );  /* CI/CK */

            ChkDR( DRM_TEE_BASE_IMPL_CheckLicenseSignature( f_pContext, &oXmrLicense.oLicense, pContentKeys ) );
        }
        else
        {
            CIPHERTEXT_P256  oCipherText; /* Initialized later on copy */

            ChkBOOL( cbCipherText == sizeof(CIPHERTEXT_P256) + ( fScalableRoot ? DRM_AES_KEYSIZE_128 : 0 ), DRM_E_ILWALID_LICENSE );

            if( IsBlobAssigned( f_pDKB ) )
            {
                /*
                ** If a DKB was given, parse the given DKB and verify its signature.
                ** The PrivateEncryptionKey in this case should be the domain key
                */
                ChkDR( DRM_TEE_KB_ParseAndVerifyDKB(
                   f_pContext,
                   f_pDKB,
                   pdomainID->dwRevision,
                   &pDomainKey ) );

                pPrivateEncryptionKeyWeakRef = pDomainKey;

            }
            else
            {
                ChkArg( IsBlobAssigned( f_pPPKB ) );
                ChkDR( DRM_TEE_KB_ParseAndVerifyPPKB( f_pContext, f_pPPKB, &cKeys, &pKeys, NULL ) );
                pPrivateEncryptionKeyWeakRef = DRM_TEE_BASE_IMPL_LocateKeyInPPKBWeakRef( &eType, cKeys, pKeys );
            }

            ChkVOID( OEM_TEE_MEMCPY( &oCipherText, pbCipherText, ECC_P256_CIPHERTEXT_SIZE_IN_BYTES ) );

            ChkDR( DRM_TEE_BASE_IMPL_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_PR_CI, &rgoContentKeys[ 0 ] ) );
            ChkDR( DRM_TEE_BASE_IMPL_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_PR_CK, &rgoContentKeys[ 1 ] ) );

            ChkDR( OEM_TEE_BASE_ECC256_Decrypt_AES128Keys(
                pOemTeeCtx,
                &pPrivateEncryptionKeyWeakRef->oKey,
                &oCipherText,
                &rgoContentKeys[0].oKey,
                &rgoContentKeys[1].oKey ) );

            if( fScalableRoot )
            {
                ChkDR( OEM_TEE_AES128CTR_UnshuffleScalableContentKeys( pOemTeeCtx, &rgoContentKeys[ 0 ].oKey, &rgoContentKeys[ 1 ].oKey ) );
            }

            ChkDR( DRM_TEE_BASE_IMPL_CheckLicenseSignature( f_pContext, &oXmrLicense.oLicense, rgoContentKeys ) );

            ChkDR( DRM_TEE_KB_BuildLKB( f_pContext, fPersist, rgoContentKeys, &oLKB ) );
        }

        ChkVOID( DRM_TEE_BASE_TransferBlobOwnership( f_pLKB, &oLKB ) );
    }

ErrorExit:
    ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, cContentKeys, &pContentKeys ) );
    ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, 1, &pDomainKey ) );
    
    //
    // LWE (kwilson) - Can't use ChkDR within ErrorExit. Cache the return value in drTmp and check value
    // of dr, before overwriting to be sure we are not overwriting a previous failure with success
    //
    drTmp = DRM_TEE_BASE_FreeBlob( f_pContext, &oLKB );
    if (dr == DRM_SUCCESS)
    {
        dr = drTmp;
    }

    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&oXmrLicense.pbStack ) );
    ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, cKeys, &pKeys ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &oTKD.oKey ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &rgoContentKeys[0].oKey ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &rgoContentKeys[1].oKey ) );
    return dr;
}
#endif
EXIT_PK_NAMESPACE_CODE;

