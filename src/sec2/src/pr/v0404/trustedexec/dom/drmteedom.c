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
#include <drmdomainkeyxmrparsertypes.h>
#include <drmerr.h>
#include <oemtee.h>
#include <oembyteorder.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_TEE_IMPL_DOM_IsDOMSupported( DRM_VOID )
{
    return TRUE;
}

static DRM_FRE_INLINE DRM_RESULT DRM_CALL _ParseDomainPrivateKey(
   __in                                                       DRM_DWORD    f_cbData,
   __in_bcount( f_cbData )                              const DRM_BYTE    *f_pbData,
   __inout_ecount( 1 )                                        DRM_DWORD   *f_pibData,
   __out_ecount( 1 )                                          DRM_DWORD   *f_pdwRevision,
   __deref_out_bcount( ECC_P256_PRIVKEY_SIZE_IN_BYTES ) const DRM_BYTE   **f_ppbEncryptedDomainKey )
{
    DRM_RESULT            dr       = DRM_SUCCESS;
    DRM_DWORD             ibData   = 0;
    DRM_DOMKEYXMR_PRIVKEY oPrivKey = {0};

    DRMASSERT( f_pbData   != NULL );
    DRMASSERT( f_pibData  != NULL );
    DRMASSERT( f_cbData >= *f_pibData + DRM_DOMKEYXMR_PRIVKEYOBJ_MIN_LENGTH + ECC_P256_PRIVKEY_SIZE_IN_BYTES );

    ibData = *f_pibData;

    NETWORKBYTES_FROMBUFFER_TO_WORD ( oPrivKey.wFlags,          f_pbData, ibData, f_cbData );
    NETWORKBYTES_FROMBUFFER_TO_WORD ( oPrivKey.wType,           f_pbData, ibData, f_cbData );
    NETWORKBYTES_FROMBUFFER_TO_DWORD( oPrivKey.dwLength,        f_pbData, ibData, f_cbData );
    NETWORKBYTES_FROMBUFFER_TO_DWORD( oPrivKey.dwRevision,      f_pbData, ibData, f_cbData );
    NETWORKBYTES_FROMBUFFER_TO_WORD ( oPrivKey.wKeyType,        f_pbData, ibData, f_cbData );
    NETWORKBYTES_FROMBUFFER_TO_WORD ( oPrivKey.wEncryptionType, f_pbData, ibData, f_cbData );
    NETWORKBYTES_FROMBUFFER_TO_DWORD( oPrivKey.dwKeyLength,     f_pbData, ibData, f_cbData );

    ChkBOOL( oPrivKey.wType           == DRM_DOMKEYXMR_OBJTYPE_PRIVKEY,              DRM_E_DOMAIN_ILWALID_DOMKEYXMR_DATA );
    ChkBOOL( oPrivKey.wKeyType        == DRM_DOMKEYXMR_PRIVKEY_TYPE_ECCP256,         DRM_E_DOMAIN_ILWALID_DOMKEYXMR_DATA );
    ChkBOOL( oPrivKey.dwKeyLength     == ECC_P256_PRIVKEY_SIZE_IN_BYTES,             DRM_E_DOMAIN_ILWALID_DOMKEYXMR_DATA );
    ChkBOOL( oPrivKey.wEncryptionType == DRM_DOMKEYXMR_PRIVKEY_ENCTYPE_MIXED_AESECB, DRM_E_DOMAIN_ILWALID_DOMKEYXMR_DATA );

    *f_ppbEncryptedDomainKey = f_pbData + ibData;               /* Can't overflow: Guaranteed by macros called above */
    *f_pibData               = ibData + oPrivKey.dwKeyLength;   /* Can't overflow: Guaranteed by macros called above */
    *f_pdwRevision           = oPrivKey.dwRevision;

ErrorExit:
    return dr;
}

/*
** Synopsis:
**
** This function takes various data from the domain join response
** which includes the session key and domain certificate private keys.
** It symmetrically rebinds the domain certificate private keys
** to a TEE-specific key and returns a set of blobs to be persisted
** to disk so that the domain certificate private keys can later be
** used to decrypt root/simple license keys encrypted with their
** corresponding domain public keys.
**
** This function is called inside Drm_JoinDomain_ProcessResponse.
**
** Operations Performed:
**
**  1. Verify the signature on the given DRM_TEE_CONTEXT.
**  2. Parse the given PlayReady Private Key Blob (PPKB) and verify its signature.
**  3. Verify that the version of the domain protocol used is supported.
**     Return DRM_E_NOTIMPL if not.
**  4. Decrypt the symmetric session key with the private encryption key.
**  5. For each domain private key, decrypt it with the symmetric session key.
**  6. Build a set of Domain Key Blobs (DKBs), one for each domain private key.
**  7. Concatenate the set of DKBs together.
**  8. Return the set of DKBs.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                DRM_TEE_BASE_AllocTEEContext.
** f_pPPKB               (in)     The current PlayReady Private Key Blob (PPKB).
** f_dwProtocolVersion   (in)     The version of the domain join protocol used.
** f_pDomainSessionKey:  (in)     The raw encrypted session key used to encrypt the domain
**                                keys.  The key type and how it is encrypted are both
**                                implied by the protocol version.
** f_pDomainPrivateKeys: (in)     The raw encrypted domain keys.
**                                These come from the domain join response.  Based on
**                                the protocol version, the callee can determine the
**                                expected size of each encrypted domain key and thus
**                                can also determine the number of domain keys
**                                (which is also equal to <revision-count>).
** f_pRevisions:         (out)    The domain revision associated with each domain key.
**                                (f_pRevisions->cdwData == <revision-count>),
**                                and f_pRevisions->pdwData points to an array of DRM_DWORD
**                                items, where the number of revisions is f_pRevisions->cdwData.
**                                This should be freed with DRM_TEE_IMPL_BASE_FreeBlob.
** f_pDKBs:              (out)    Each DKB ready for storage to disk.  On output,
**                                (f_pDKBs->cb == (<revision-count>)*(<size of each DKB>))
**                                and the caller will fail if f_pDKBs->cb is
**                                not a multiple of <revision-count> as computed by
**                                evaluating f_pRevisions->cb / sizeof(DRM_DWORD).
**                                On output, f_pDKBs->pb points to an array of
**                                DKBs which the caller will directly write to persisted
**                                storage without modification along with the associated
**                                revision.  A single one of these DKBs will be passed
**                                to DRM_TEE_LICPREP_PackageKey when packaging the keys for
**                                a domain-bound license bound to the associated domain
**                                and revision.
**                                This should be freed with DRM_TEE_IMPL_BASE_FreeBlob.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_DOM_PackageKeys(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pPPKB,
    __in                        DRM_DWORD                     f_dwProtocolVersion,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pDomainSessionKey,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pDomainPrivateKeys,
    __out                       DRM_TEE_DWORDLIST            *f_pRevisions,
    __out                       DRM_TEE_BYTE_BLOB            *f_pDKBs )
{
    DRM_RESULT          dr                            = DRM_SUCCESS;
    DRM_RESULT          drTmp                         = DRM_SUCCESS;
    DRM_DWORD           cKeys                         = 0;
    DRM_TEE_KEY        *pKeys                         = NULL;
    DRM_TEE_KEY         rgoSessionKeys[2]             = { DRM_TEE_KEY_EMPTY, DRM_TEE_KEY_EMPTY };
    DRM_DWORD           cDomains                      = 0;
    CIPHERTEXT_P256     oCipherText;                  /* Initialized by memcpy before usage */
    DRM_TEE_KEY         oDomainPrivKey                = DRM_TEE_KEY_EMPTY;
    const DRM_TEE_KEY  *pPrivateEncryptionKeyWeakRef  = NULL;
    DRM_TEE_KEY_TYPE    eType                         = DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_ENCRYPT;
    DRM_TEE_BYTE_BLOB   oDKB                          = DRM_TEE_BYTE_BLOB_EMPTY;
    DRM_TEE_BYTE_BLOB   oDKBs                         = DRM_TEE_BYTE_BLOB_EMPTY;
    DRM_TEE_DWORDLIST   oRevisions                    = DRM_TEE_DWORDLIST_EMPTY;
    DRM_DWORD           iDomain                       = 0;
    OEM_TEE_CONTEXT    *pOemTeeCtx                    = NULL;
    const DRM_BYTE     *pbDomainKeysData              = NULL;
    DRM_DWORD           cbDomainKeysData              = 0;
    DRM_DWORD           ibDomainKeysData              = 0;

    /* Arguments are checked in the PRITEE proxy layer */
    DRMASSERT( f_pContext           != NULL );
    DRMASSERT( f_pPPKB              != NULL );
    DRMASSERT( f_pDomainSessionKey  != NULL );
    DRMASSERT( f_pDomainPrivateKeys != NULL );
    DRMASSERT( f_pRevisions         != NULL );
    DRMASSERT( f_pDKBs              != NULL );

    pOemTeeCtx = &f_pContext->oContext;

    /* Ensure that the Secure clock did not go out of sync */
    if( OEM_TEE_CLOCK_SelwreClockNeedsReSync( pOemTeeCtx ) )
    {
        ChkDR( DRM_E_TEE_CLOCK_DRIFTED );
    }

    ChkDR( DRM_TEE_IMPL_KB_ParseAndVerifyPPKB( f_pContext, f_pPPKB, &cKeys, &pKeys, NULL ) );
    ChkBOOL( f_dwProtocolVersion == DRM_DOMAIN_JOIN_PROTOCOL_VERSION, DRM_E_NOTIMPL );              /* Only one domain join protocol version is lwrrently supported (version 2) */

    ChkArg( sizeof( oCipherText ) == f_pDomainSessionKey->cb );
    ChkVOID( OEM_TEE_MEMCPY( &oCipherText, f_pDomainSessionKey->pb, sizeof(oCipherText) ) );

    pPrivateEncryptionKeyWeakRef = DRM_TEE_IMPL_BASE_LocateKeyInPPKBWeakRef( &eType, cKeys, pKeys );

    ChkDR( DRM_TEE_IMPL_BASE_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_DOMAIN_SESSION, &rgoSessionKeys[ 0 ] ) );
    ChkDR( DRM_TEE_IMPL_BASE_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_DOMAIN_SESSION, &rgoSessionKeys[ 1 ] ) );

    ChkDR( OEM_TEE_BASE_ECC256_Decrypt_AES128Keys( pOemTeeCtx, &pPrivateEncryptionKeyWeakRef->oKey, &oCipherText, &rgoSessionKeys[ 0 ].oKey, &rgoSessionKeys[ 1 ].oKey ) );

    pbDomainKeysData = f_pDomainPrivateKeys->pb;
    cbDomainKeysData = f_pDomainPrivateKeys->cb;

    ChkBOOL( cbDomainKeysData % ( DRM_DOMKEYXMR_PRIVKEYOBJ_MIN_LENGTH + ECC_P256_PRIVKEY_SIZE_IN_BYTES ) == 0, DRM_E_DOMAIN_ILWALID_DOMKEYXMR_DATA );
    cDomains = cbDomainKeysData / ( DRM_DOMKEYXMR_PRIVKEYOBJ_MIN_LENGTH + ECC_P256_PRIVKEY_SIZE_IN_BYTES );
    ChkBOOL( cDomains <= DRM_MAX_DOMAIN_JOIN_KEYS, DRM_E_DOMAIN_JOIN_TOO_MANY_KEYS );
    ChkBOOL( cDomains > 0, DRM_E_DOMAIN_ILWALID_DOMKEYXMR_DATA );

    oRevisions.cdwData = cDomains;
    ChkDR( DRM_TEE_IMPL_BASE_MemAlloc( f_pContext, cDomains * sizeof(DRM_DWORD), ( DRM_VOID ** )&oRevisions.pdwData ) );

    while( DRM_DOMKEYXMR_PRIVKEYOBJ_MIN_LENGTH < cbDomainKeysData && ibDomainKeysData < cbDomainKeysData - DRM_DOMKEYXMR_PRIVKEYOBJ_MIN_LENGTH )
    {
        DRM_DWORD       dwRevision           = 0;
        const DRM_BYTE *pbEncryptedDomainKey = NULL;
        DRM_DWORD      *pdwRevision          = ( DRM_DWORD * )&oRevisions.pdwData[iDomain];

        ChkDR( _ParseDomainPrivateKey( cbDomainKeysData, pbDomainKeysData, &ibDomainKeysData, &dwRevision, &pbEncryptedDomainKey ) );
        DRMASSERT( ibDomainKeysData <= cbDomainKeysData );

        *pdwRevision = dwRevision;

        ChkDR( DRM_TEE_IMPL_BASE_AllocKeyECC256( pOemTeeCtx, DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_ENCRYPT, &oDomainPrivKey ) );

        ChkDR( OEM_TEE_DOM_DecryptDomainKeyWithSessionKeys(
            pOemTeeCtx,
            &rgoSessionKeys[ 0 ].oKey,
            &rgoSessionKeys[ 1 ].oKey,
            pbEncryptedDomainKey,
            &oDomainPrivKey.oKey ) );

        ChkDR( DRM_TEE_IMPL_BASE_FreeBlob( f_pContext, &oDKB ) );

        ChkDR( DRM_TEE_IMPL_KB_BuildDKB(
            f_pContext,
            &oDomainPrivKey,
            dwRevision,
            &oDKB ) );

        ChkVOID( OEM_TEE_BASE_FreeKeyECC256( pOemTeeCtx, &oDomainPrivKey.oKey ) );

        if( oDKBs.pb == NULL )
        {
            /*
            ** Allocate the DKBs blob once we know the size of the first KB
            ** The following KB should have the same size
            */
            DRM_DWORD cbDKBs = 0;
            ChkDR( DRM_DWordMult( cDomains, oDKB.cb, &cbDKBs ) );
            ChkDR( DRM_TEE_IMPL_BASE_AllocBlob( f_pContext, DRM_TEE_BLOB_ALLOC_BEHAVIOR_NEW, cbDKBs, NULL, &oDKBs ) );
        }

        DRMASSERT( IsBlobAssigned( &oDKBs ) );
        __analysis_assume( IsBlobAssigned( &oDKBs ) );
        ChkVOID( OEM_TEE_MEMCPY( (DRM_VOID*)( oDKBs.pb + ( iDomain * oDKB.cb ) ), oDKB.pb, oDKB.cb ) );     /* Can't overflow: Guaranteed by previous checks */

        iDomain++;
    }

    DRMASSERT( ibDomainKeysData == cbDomainKeysData );
    DRMASSERT( iDomain          == cDomains         );

    ChkVOID( DRM_TEE_DWORDLIST_TRANSFER_OWNERSHIP( f_pRevisions, &oRevisions ) );
    ChkVOID( DRM_TEE_IMPL_BASE_TransferBlobOwnership( f_pDKBs, &oDKBs ) );

ErrorExit:
    ChkVOID( DRM_TEE_IMPL_BASE_FreeKeyArray( f_pContext, cKeys, &pKeys ) );
   
    //
    // LWE (kwilson) - Can't use ChkDR within ErrorExit. Cache the return value in drTmp and check value
    // of dr, before overwriting to be sure we are not overwriting a previous failure with success.
    //
    drTmp = DRM_TEE_IMPL_BASE_FreeBlob( f_pContext, &oDKBs );
    if (dr == DRM_SUCCESS)
    {
        dr = drTmp;
    }
    drTmp = DRM_TEE_IMPL_BASE_FreeBlob( f_pContext, &oDKB );
    if (dr == DRM_SUCCESS)
    {
        dr = drTmp;
    }

    ChkVOID( DRM_TEE_IMPL_BASE_MemFree( f_pContext, ( DRM_VOID ** )&oRevisions.pdwData ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyECC256( pOemTeeCtx, &oDomainPrivKey.oKey ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &rgoSessionKeys[ 0 ].oKey ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &rgoSessionKeys[ 1 ].oKey ) );

    return dr;
}
#endif

EXIT_PK_NAMESPACE_CODE;

