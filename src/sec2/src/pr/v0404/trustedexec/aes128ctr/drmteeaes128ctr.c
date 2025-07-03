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
#include <drmteecache.h>
#include <drmerr.h>
#include <oemtee.h>
#include <drmrivcrlparser.h>
#include <oembyteorder.h>
#include <drmxmrformatparser.h>
#include <drmderivedkey.h>
#include <drmbcertconstants.h>
#include <drmteedecryptinternal.h>
#include <drmteedebug.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_TEE_IMPL_AES128CTR_IsAES128CTRSupported( DRM_VOID )
{
    return TRUE;
}
#endif

#ifdef NONE
/*
** Synopsis:
**
** This function decrypts content using the decryption mode that was returned from
** DRM_TEE_DECRYPT_PrepareToDecrypt.
**
** This function is called inside:
**  Drm_Reader_DecryptLegacy
**  Drm_Reader_DecryptOpaque
**
** Operations Performed:
**
**  1. Verify the signature on the given DRM_TEE_CONTEXT.
**  2. Retrieve the data required for decryption as follows.
**     a. If you have provided the OEM key information via implementing the
**        OEM_TEE_DECRYPT_BuildExternalPolicyCryptoInfo, we will first attempt
**        to retrieve the decryption data from the OEM key info by calling
**        OEM_TEE_DECRYPT_ParseAndVerifyExternalPolicyCryptoInfo.  If you
**        implement this function and successfully return the keys, session ID, and
**        policy, the CDKB will not be parsed and used.  This option can be used
**        if either the OEM key information can be stored in the TEE or the
**        decryption data can be cryptographically protected with better performance
**        by your own implementation.  If you choose this option and the data will
**        not remain in the TEE, you MUST make sure the keys are cryptographically
**        protected and the policy can be validated not to have changed (via
**        cryptographic signature).
**     b. If OEM_TEE_DECRYPT_ParseAndVerifyExternalPolicyCryptoInfo returns
**        DRM_E_NOTIMPL or the key count returned is zero, the given Content
**        Decryption Key Blob (CDKB) will be parsed and its signature verified.
**  3. If the decryption mode returned from DRM_TEE_DECRYPT_PrepareToDecrypt
**     was equal to OEM_TEE_DECRYPTION_MODE_NOT_SELWRE,
**     decrypt the given content (using the given region mapping and IV)
**     into a clear buffer and return it via the Clear Content Data (CCD).
**     Otherwise, decrypt the given content (using the given region mapping and IV)
**     into an opaque buffer and return an it via the CCD.
**     The OEM-defined opaque buffer format is specified by the specific
**     OEM-defined decryption mode that was returned
**     (i.e. the OEM may support multiple decryption modes).
**
** Arguments:
**
** f_pContext:                 (in/out) The TEE context returned from
**                                      DRM_TEE_BASE_AllocTEEContext.
**                                      If this parameter is NULL, the CDKB must be
**                                      self-contained, i.e. it must include the
**                                      data required to reconstitute the DRM_TEE_CONTEXT.
** f_pCDKB:                    (in)     The Content Decryption Key Blob (CDKB) returned from
**                                      DRM_TEE_DECRYPT_PrepareToDecrypt.
** f_pOEMKeyInfo               (in)     The Oem Key Info returned from
**                                      DRM_TEE_PROXY_DECRYPT_CreateOEMBlobFromCDKB.
** f_pEncryptedRegionMapping:  (in)     The encrypted-region mapping.
**
**                                      The number of DRM_DWORDs must be a multiple
**                                      of two.  Each DRM_DWORD pair represents
**                                      values indicating how many clear bytes
**                                      followed by how many encrypted bytes are
**                                      present in the data on output.
**                                      The sum of all of these DRM_DWORD pairs
**                                      must be equal to bytecount(clear content),
**                                      i.e. equal to f_pEncrypted->cb.
**
**                                      If the entire f_pEncrypted is encrypted, there will be
**                                      two DRM_DWORDs with values 0, bytecount(encrypted content).
**                                      If the entire f_pEncrypted is clear, there will be
**                                      two DRM_DWORDs with values bytecount(clear content), 0.
**
**                                      The caller must already be aware of what information
**                                      to pass to this function, as this information is
**                                      format-/codec-specific.
**
** f_ui64Initializatiolwector: (in)     The AES128CTR initialization vector.
** f_pEncrypted:               (in)     The encrypted bytes being decrypted.
** f_pCCD:                     (out)    The Clear Content Data (CCD) which represents the
**                                      clear data that was decrypted by this function.
**                                      This CCD is opaque unless the decryption mode
**                                      returned by DRM_TEE_DECRYPT_PrepareToDecrypt
**                                      was OEM_TEE_DECRYPTION_MODE_NOT_SELWRE.
**                                      This should be freed with DRM_TEE_IMPL_BASE_FreeBlob.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_AES128CTR_DecryptContent(
    __inout_opt                 DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pCDKB,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pOEMKeyInfo,
    __in                  const DRM_TEE_DWORDLIST            *f_pEncryptedRegionMapping,
    __in                        DRM_UINT64                    f_ui64Initializatiolwector,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pEncrypted,
    __out                       DRM_TEE_BYTE_BLOB            *f_pCCD )
{
    DRM_RESULT               dr                     = DRM_SUCCESS;
    DRM_RESULT               drTmp                  = DRM_SUCCESS;
    DRM_TEE_BYTE_BLOB        oCCD                   = DRM_TEE_BYTE_BLOB_EMPTY;
    DRM_DWORD                cCDKeys                = 0;
    DRM_TEE_KEY             *pCDKeys                = NULL;
    DRM_TEE_CONTEXT          oTeeCtxReconstituted   = DRM_TEE_CONTEXT_EMPTY;
    DRM_BOOL                 fReconstituted         = FALSE;
    OEM_TEE_CONTEXT         *pOemTeeCtx             = NULL;
    OEM_TEE_MEMORY_HANDLE    hClear                 = OEM_TEE_MEMORY_HANDLE_ILWALID;
    DRM_TEE_POLICY_INFO     *pPolicy                = NULL;

    /* Arguments are checked in the DRM TEE proxy layer */
    DRMASSERT( f_pEncryptedRegionMapping != NULL );
    DRMASSERT( f_pEncrypted != NULL );
    DRMASSERT( f_pCCD != NULL );

    DRMASSERT( IsBlobAssigned( f_pEncrypted ) );

    /* f_pEncryptedRegionMapping must have a multiple of 2 DWORDs, each DWORD pair is a clear length followed by an encrypted length. */
    ChkArg( ( f_pEncryptedRegionMapping->cdwData & 1 ) == 0 );     /* Power of two: use & instead of % for better perf */

    ChkVOID( OEM_TEE_ZERO_MEMORY( f_pCCD, sizeof( *f_pCCD ) ) );

    ChkDR( DRM_TEE_IMPL_DECRYPT_DecryptContentPolicyHelper(
        f_pContext,
        &f_pContext,
        &oTeeCtxReconstituted,
        &fReconstituted,
        DRM_TEE_POLICY_INFO_CALLING_API_AES128CTR_DECRYPTCONTENT,
        f_pCDKB,
        f_pOEMKeyInfo,
        &cCDKeys,
        &pCDKeys,
        &pPolicy ) );

    pOemTeeCtx = &f_pContext->oContext;

    if( OEM_TEE_DECRYPTION_MODE_IS_HANDLE_TYPE( pPolicy->dwDecryptionMode ) )
    {
        DRM_DWORD cbhClear = 0;
        ChkDR( OEM_TEE_BASE_SelwreMemHandleAlloc( pOemTeeCtx, pPolicy->dwDecryptionMode, f_pEncrypted->cb, &cbhClear, &hClear ) );
        ChkDR( OEM_TEE_AES128CTR_DecryptContentIntoHandle(
            pOemTeeCtx,
            DRM_TEE_POLICY_INFO_CALLING_API_AES128CTR_DECRYPTCONTENT,
            pPolicy,
            &pCDKeys[ 1 ].oKey,
            f_pEncryptedRegionMapping->cdwData,
            f_pEncryptedRegionMapping->pdwData,
            f_ui64Initializatiolwector,
            DRM_UI64( 0 ),
            0,
            NULL,
            f_pEncrypted->cb,
            f_pEncrypted->pb,
            hClear,
            0,
            NULL ) );

        /* Transfer handle into blob */
        ChkDR( DRM_TEE_IMPL_BASE_AllocBlobAndTakeOwnership( f_pContext, cbhClear, (DRM_BYTE**)&hClear, &oCCD ) );
        oCCD.eType = DRM_TEE_BYTE_BLOB_TYPE_CCD;
        oCCD.dwSubType = OEM_TEE_DECRYPTION_MODE_HANDLE;
        hClear = OEM_TEE_MEMORY_HANDLE_ILWALID;
    }
    else
    {
        ChkDR( DRM_TEE_IMPL_BASE_AllocBlob( f_pContext, DRM_TEE_BLOB_ALLOC_BEHAVIOR_COPY, f_pEncrypted->cb, f_pEncrypted->pb, &oCCD ) );
        ChkDR( OEM_TEE_AES128CTR_DecryptContentIntoClear(
            pOemTeeCtx,
            DRM_TEE_POLICY_INFO_CALLING_API_AES128CTR_DECRYPTCONTENT,
            pPolicy,
            &pCDKeys[ 1 ].oKey,
            f_pEncryptedRegionMapping->cdwData,
            f_pEncryptedRegionMapping->pdwData,
            f_ui64Initializatiolwector,
            DRM_UI64( 0 ),
            0,
            NULL,
            oCCD.cb,
            (DRM_BYTE*)oCCD.pb,
            0,
            NULL ) );
    }

    if( pPolicy->dwDecryptionMode == OEM_TEE_DECRYPTION_MODE_SAMPLE_PROTECTION )
    {
        /*
        ** in case of sample protection, all input content must be encrypted; no sub sample mapping
        ** is supported/necessary
        */
        ChkArg( f_pEncryptedRegionMapping->cdwData == 2
             && f_pEncryptedRegionMapping->pdwData[0] == 0
             && f_pEncryptedRegionMapping->pdwData[1] == oCCD.cb );

        ChkDR( DRM_TEE_IMPL_SAMPLEPROT_ApplySampleProtection(
            pOemTeeCtx,
            &pCDKeys[ 2 ].oKey,
            f_ui64Initializatiolwector,
            oCCD.cb,
            (DRM_BYTE*)oCCD.pb ) );
    }

    /* Transfer ownership */
    ChkVOID( DRM_TEE_IMPL_BASE_TransferBlobOwnership( f_pCCD, &oCCD ) );

ErrorExit:
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( NULL, (DRM_VOID **)&pPolicy ) );

    //
    // LWE (kwilson) - Can't use ChkDR within ErrorExit. Cache the return value in drTmp and check value
    // of dr, before overwriting to be sure we are not overwriting a previous failure with success.
    //
    if( hClear != OEM_TEE_MEMORY_HANDLE_ILWALID )
    {
        drTmp = OEM_TEE_BASE_SelwreMemHandleFree( pOemTeeCtx, &hClear );
        if (dr == DRM_SUCCESS)
        {
            dr = drTmp;
        }
    }

    drTmp =  DRM_TEE_BASE_FreeBlob( f_pContext, &oCCD );
    if (dr == DRM_SUCCESS)
    {
        dr = drTmp;
    }

    if( pCDKeys != NULL )
    {
        ChkVOID( DRM_TEE_IMPL_BASE_FreeKeyArray( f_pContext, cCDKeys, &pCDKeys ) );
    }

    if( fReconstituted )
    {
        ChkVOID( OEM_TEE_BASE_SelwreMemFree( NULL, (DRM_VOID **)&oTeeCtxReconstituted.oContext.pbUnprotectedOEMData ) );
    }

    return dr;
}

/*
** Synopsis:
**
** This function decrypts multiple sets of audio content using the decryption mode
** that was returned from DRM_TEE_DECRYPT_PrepareToDecrypt.
**
** This function is not called from any top-level PK functions.
**
** Operations Performed:
**
**  1. Verify the signature on the given DRM_TEE_CONTEXT.
**  2. Retrieve the data required for decryption as in DRM_TEE_AES128CTR_DecryptContent.
**  3. If the decryption mode returned from DRM_TEE_DECRYPT_PrepareToDecrypt
**     was equal to OEM_TEE_DECRYPTION_MODE_NOT_SELWRE,
**     decrypt the given content (using the given IVs)
**     into a clear buffer and return it via the Clear Content Data (CCD).
**     Otherwise, decrypt the given content (using the given IVs)
**     into an opaque buffer and return an it via the CCD.
**     The OEM-defined opaque buffer format is specified by the specific
**     OEM-defined decryption mode that was returned
**     (i.e. the OEM may support multiple decryption modes).
**     Note that multiple decryptions are performed with different IVs
**     on different subsets of the encrypted passed in.
**
** Arguments:
**
** f_pContext:                  (in/out) The TEE context returned from
**                                       DRM_TEE_BASE_AllocTEEContext.
**                                       If this parameter is NULL, the CDKB must be
**                                       self-contained, i.e. it must include the
**                                       data required to reconstitute the DRM_TEE_CONTEXT.
** f_pCDKB:                         (in) The Content Decryption Key Blob (CDKB) returned from
**                                       DRM_TEE_DECRYPT_PrepareToDecrypt.
** f_pOEMKeyInfo                    (in) The Oem Key Info returned from
**                                       DRM_TEE_PROXY_DECRYPT_CreateOEMBlobFromCDKB.
** f_pInitializatiolwectors:        (in) The AES128CTR initialization vectors for each
**                                       subset of content indicated by f_pInitializatiolwectorSizes.
** f_pInitializatiolwectorSizes:    (in) Each DWORD in this list represents the number of bytes
**                                       that are encrypted with the corresponding IV in
**                                       f_pInitializatiolwectors.
**                                       The IV in f_pInitializatiolwectors[0] is used to
**                                       decrypt the first f_pInitializatiolwectorSizes[0] bytes,
**                                       The IV in f_pInitializatiolwectors[1] is used to
**                                       decrypt the *next* f_pInitializatiolwectorSizes[1] bytes,
**                                       and so forth.
** f_pEncrypted:                    (in) The encrypted bytes being decrypted.
**                                       Note that this represents the concatenation of multiple sets
**                                       of content with different IVs.
**                                       The number of entries in this array MUST match the
**                                       number of bytes in the sum over f_pInitializatiolwectorSizes.
** f_pCCD:                         (out) The Clear Content Data (CCD) which represents the
**                                       clear data that was decrypted by this function.
**                                       This CCD is opaque unless the decryption mode
**                                       returned by DRM_TEE_DECRYPT_PrepareToDecrypt
**                                       was OEM_TEE_DECRYPTION_MODE_NOT_SELWRE.
**                                       This should be freed with DRM_TEE_IMPL_BASE_FreeBlob.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_AES128CTR_DecryptAudioContentMultiple(
    __inout_opt                 DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pCDKB,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pOEMKeyInfo,
    __in                  const DRM_TEE_QWORDLIST            *f_pInitializatiolwectors,
    __in                  const DRM_TEE_DWORDLIST            *f_pInitializatiolwectorSizes,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pEncrypted,
    __out                       DRM_TEE_BYTE_BLOB            *f_pCCD )
{
    DRM_RESULT               dr                     = DRM_SUCCESS;
    DRM_TEE_BYTE_BLOB        oCCD                   = DRM_TEE_BYTE_BLOB_EMPTY;
    DRM_DWORD                cCDKeys                = 0;
    DRM_TEE_KEY             *pCDKeys                = NULL;
    DRM_TEE_CONTEXT          oTeeCtxReconstituted   = DRM_TEE_CONTEXT_EMPTY;
    DRM_BOOL                 fReconstituted         = FALSE;
    OEM_TEE_CONTEXT         *pOemTeeCtx             = NULL;
    DRM_TEE_POLICY_INFO     *pPolicy                = NULL;
    DRM_DWORD                iIV;                   /* Loop variable */
    DRM_BYTE                *pbNextEncrypted        = NULL;

    /* Arguments are checked in the DRM TEE proxy layer */
    DRMASSERT( f_pInitializatiolwectors != NULL );
    DRMASSERT( f_pInitializatiolwectorSizes != NULL );
    DRMASSERT( f_pEncrypted != NULL );
    DRMASSERT( f_pCCD != NULL );

    DRMASSERT( IsBlobAssigned( f_pEncrypted ) );

    ChkVOID( OEM_TEE_ZERO_MEMORY( f_pCCD, sizeof( *f_pCCD ) ) );

    /* Each vector must have a size, i.e. there's a strict 1:1 correspondence. */
    ChkArg( f_pInitializatiolwectors->cqwData == f_pInitializatiolwectorSizes->cdwData );

    /* All vector sizes must sum to the size of the encrypted buffer */
    {
        DRM_DWORD cbIVs = 0;

        for( iIV = 0; iIV < f_pInitializatiolwectorSizes->cdwData; iIV++ )
        {
            ChkArg( f_pInitializatiolwectorSizes->pdwData[ iIV ] > 0 );
            ChkDR( DRM_DWordAddSame( &cbIVs, f_pInitializatiolwectorSizes->pdwData[ iIV ] ) );
        }
        ChkArg( cbIVs == f_pEncrypted->cb );
    }

    ChkDR( DRM_TEE_IMPL_DECRYPT_DecryptContentPolicyHelper(
        f_pContext,
        &f_pContext,
        &oTeeCtxReconstituted,
        &fReconstituted,
        DRM_TEE_POLICY_INFO_CALLING_API_AES128CTR_DECRYPTAUDIOCONTENTMULTIPLE,
        f_pCDKB,
        f_pOEMKeyInfo,
        &cCDKeys,
        &pCDKeys,
        &pPolicy ) );

    pOemTeeCtx = &f_pContext->oContext;

    /* Allocate the output buffer based on type */
    if( OEM_TEE_DECRYPTION_MODE_IS_HANDLE_TYPE( pPolicy->dwDecryptionMode ) )
    {
        ChkDR( DRM_E_NOTIMPL );
    }
    else
    {
        ChkDR( DRM_TEE_IMPL_BASE_AllocBlob( f_pContext, DRM_TEE_BLOB_ALLOC_BEHAVIOR_COPY, f_pEncrypted->cb, f_pEncrypted->pb, &oCCD ) );
        pbNextEncrypted = (DRM_BYTE*)oCCD.pb;   /* cast away const */
    }

    for( iIV = 0; iIV < f_pInitializatiolwectors->cqwData; iIV++ )
    {
        DRM_UINT64   ui64NextIV             = f_pInitializatiolwectors->pqwData[ iIV ];
        DRM_DWORD    cbNextEncrypted        = f_pInitializatiolwectorSizes->pdwData[ iIV ];

        /* All audio bytes are always encrypted (i.e. clear byte count == 0 and encrypted byte count == all bytes) */
        DRM_DWORD    rgdwNextMapping[ 2 ];
        rgdwNextMapping[ 0 ] = 0;
        rgdwNextMapping[ 1 ] = f_pInitializatiolwectorSizes->pdwData[ iIV ];

        /* DRM_TEE_IMPL_BASE_AllocBlob would fail if oCCD.pb == NULL */
        DRMASSERT( pbNextEncrypted != NULL );
        __analysis_assume( pbNextEncrypted != NULL );

        ChkDR( OEM_TEE_AES128CTR_DecryptContentIntoClear(
            pOemTeeCtx,
            DRM_TEE_POLICY_INFO_CALLING_API_AES128CTR_DECRYPTAUDIOCONTENTMULTIPLE,
            pPolicy,
            &pCDKeys[ 1 ].oKey,
            DRM_NO_OF( rgdwNextMapping ),
            rgdwNextMapping,
            ui64NextIV,
            DRM_UI64( 0 ),
            0,
            NULL,
            cbNextEncrypted,
            pbNextEncrypted,
            0,
            NULL ) );

        if( pPolicy->dwDecryptionMode == OEM_TEE_DECRYPTION_MODE_SAMPLE_PROTECTION )
        {
            ChkDR( DRM_TEE_IMPL_SAMPLEPROT_ApplySampleProtection(
                pOemTeeCtx,
                &pCDKeys[ 2 ].oKey,
                ui64NextIV,
                cbNextEncrypted,
                pbNextEncrypted ) );
        }

        /* Move on to the next set of encrypted bytes */
        pbNextEncrypted += cbNextEncrypted;     /* Can't overflow: already verified that sub-sample mapping total size == total sample size */
    }

    /* Transfer ownership */
    ChkVOID( DRM_TEE_IMPL_BASE_TransferBlobOwnership( f_pCCD, &oCCD ) );

ErrorExit:
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( NULL, (DRM_VOID **)&pPolicy ) );

    //
    // LWE (kwilson) - Can't use ChkDR within ErrorExit. Cache the return value in drTmp and check value
    // of dr, before overwriting to be sure we are not overwriting a previous failure with success.
    //
    drTmp =  DRM_TEE_BASE_FreeBlob( f_pContext, &oCCD );
    if (dr == DRM_SUCCESS)
    {
        dr = drTmp;
    }

    if( pCDKeys != NULL )
    {
        ChkVOID( DRM_TEE_IMPL_BASE_FreeKeyArray( f_pContext, cCDKeys, &pCDKeys ) );
    }

    if( fReconstituted )
    {
        ChkVOID( OEM_TEE_BASE_SelwreMemFree( NULL, (DRM_VOID **)&oTeeCtxReconstituted.oContext.pbUnprotectedOEMData ) );
    }

    return dr;
}

/*
** Synopsis:
**
** This function decrypts content using the decryption mode that was returned from
** DRM_TEE_DECRYPT_PrepareToDecrypt and allows multiple encrypted regions
** (like DRM_TEE_AES128CTR_DecryptAudioContentMultiple), multiple IVs,
** and encrypted region striping (i.e. 'cens' if specified, 'cenc' if not).
**
** This function has a strict superset of the union of the functionality in
** DRM_TEE_AES128CTR_DecryptContent and DRM_TEE_AES128CTR_DecryptAudioContentMultiple.
**
** This function is called inside:
**  No top-level drmmanager functions.
**
** Operations Performed:
**
**  1. Verify the given DRM_TEE_CONTEXT.
**  2. Retrieve the data required for decryption as follows.
**     a. If you have provided the OEM key information via implementing the
**        OEM_TEE_DECRYPT_BuildExternalPolicyCryptoInfo, we will first attempt
**        to retrieve the decryption data from the OEM key info by calling
**        OEM_TEE_DECRYPT_ParseAndVerifyExternalPolicyCryptoInfo.  If you
**        implement this function and successfully return the keys, session ID, and
**        policy, the CDKB will not be parsed and used.  This option can be used
**        if either the OEM key information can be stored in the TEE or the
**        decryption data can be cryptographically protected with better performance
**        by your own implementation.  If you choose this option and the data will
**        not remain in the TEE, you MUST make sure the keys are cryptographically
**        protected and the policy can be validated not to have changed (via
**        cryptographic signature).
**     b. If OEM_TEE_DECRYPT_ParseAndVerifyExternalPolicyCryptoInfo returns
**        DRM_E_NOTIMPL or the key count returned is zero, the given Content
**        Decryption Key Blob (CDKB) will be parsed and its signature verified.
**  3. If the decryption mode returned from DRM_TEE_DECRYPT_PrepareToDecrypt
**     was equal to OEM_TEE_DECRYPTION_MODE_NOT_SELWRE,
**     decrypt the given content (using the given auxiliary data)
**     into a clear buffer and return it via the Clear Content Data (CCD).
**     Otherwise, decrypt the given content (using the given auxiliary data)
**     into an opaque buffer and return an it via the CCD.
**     The OEM-defined opaque buffer format is specified by the specific
**     OEM-defined decryption mode that was returned
**     (i.e. the OEM may support multiple decryption modes).
**
** Arguments:
**
** f_pContext:                             (in/out) The TEE context returned from
**                                                  DRM_TEE_BASE_AllocTEEContext.
**                                                  If this parameter is NULL, the CDKB must be
**                                                  self-contained, i.e. it must include the
**                                                  data required to reconstitute the
**                                                  DRM_TEE_CONTEXT.
** f_pCDKB:                                    (in) The Content Decryption Key Blob (CDKB)
**                                                  returned from DRM_TEE_DECRYPT_PrepareToDecrypt.
** f_pOEMKeyInfo                               (in) The Oem Key Info returned from
**                                                  DRM_TEE_PROXY_DECRYPT_CreateOEMBlobFromCDKB.
** f_pEncryptedRegionInitializatiolwectorsHigh (in) Each entry represents the high 8 bytes of the
**                                                  AES128CTR initialization vector for the next
**                                                  encrypted region.
** f_pEncryptedRegionInitializatiolwectorsLow  (in) Each entry represents the low 8 bytes of the
**                                                  AES128CTR initialization vector for the next
**                                                  encrypted region.  If an 8 byte IV is being used,
**                                                  f_pEncryptedRegionInitializatiolwectorsLow->cqwData
**                                                  should be set to 0.
** f_pEncryptedRegionCounts                    (in) Each entry represents the number of DWORD pairs
**                                                  in f_pEncryptedRegionMappings which represent
**                                                  the next encrypted region which uses a single
**                                                  AES128CTR initialization vector.
**
**                                                  The number of entries in this list MUST
**                                                  be equal to the number of entries in
**                                                  f_pEncryptedRegionInitializatiolwectorsHigh.
**
**                                                  Two times the sum of all entries in this
**                                                  list MUST be equal to the number of entries
**                                                  in f_pEncryptedRegionMappings.
**
**                                                  Typically, this value will have one entry
**                                                  per sample inside the f_pEncrypted buffer.
** f_pEncryptedRegionMappings                  (in) The encrypted-region mappings for all
**                                                  encrypted regions.
**
**                                                  The number of DRM_DWORDs must be a multiple
**                                                  of two.  Each DRM_DWORD pair represents
**                                                  values indicating how many clear bytes
**                                                  followed by how many encrypted bytes are
**                                                  present in the data on input.
**
**                                                  A given AES128CTR Initialization Vector from
**                                                  f_pEncryptedRegionInitializatiolwectorsHigh
**                                                  will be used for two times the number
**                                                  of encrypted region mappings in
**                                                  f_pEncryptedRegionMappings (as will
**                                                  f_pEncryptedRegionInitializatiolwectorsLow
**                                                  if present).
**
**                                                  The sum of all of these DRM_DWORD pairs
**                                                  must be equal to bytecount(clear content),
**                                                  i.e. equal to f_pEncrypted->cb.
**
**                                                  If an entire region is encrypted, there will be
**                                                  two DRM_DWORDs with values 0, bytecount(encrypted content).
**                                                  If an entire region is clear, there will be
**                                                  two DRM_DWORDs with values bytecount(clear content), 0.
**
**                                                  The caller must already be aware of what information
**                                                  to pass to this function, as this information is
**                                                  format-/codec-specific.
** f_pEncryptedRegionSkip                      (in) If non-empty, the encrypted regions are encrypted
**                                                  with striping (i.e. 'cens' content).  In this case:
**                                                  f_pEncryptedRegionSkip->cdwData MUST be 2.
**                                                  f_pEncryptedRegionSkip->pdwData[0] is the number of
**                                                  encrypted BLOCKS (16 bytes each) in each encrypted stripe.
**                                                  f_pEncryptedRegionSkip->pdwData[1] is the number of
**                                                  clear BLOCKS (16 bytes each) in each clear stripe.
**                                                  For non-striped encrypted regions (i.e. 'cenc' content),
**                                                  f_pEncryptedRegionSkip->cdwData should be set to 0
**                                                  OR f_pEncryptedRegionSkip->cdwData set to 2 with
**                                                  both entries set to 0.
** f_pEncrypted:                               (in) The encrypted bytes being decrypted.
** f_pCCD:                                    (out) The Clear Content Data (CCD) which represents the
**                                                  clear data that was decrypted by this function.
**                                                  This CCD is opaque unless the decryption mode
**                                                  returned by DRM_TEE_DECRYPT_PrepareToDecrypt
**                                                  was OEM_TEE_DECRYPTION_MODE_NOT_SELWRE.
**                                                  This should be freed with DRM_TEE_IMPL_BASE_FreeBlob.
**
**  Example input parameters:
**  Assume that if f_pEncryptedRegionCounts has two entries with values 2, 1.
**  Assume that f_pEncryptedRegionSkip is empty.
**  Assume that 8 byte IVs are being used, (i.e. f_pEncryptedRegionInitializatiolwectorsLow is empty).
**
**  -) f_pEncryptedRegionInitializatiolwectorsHigh
**     will have two entries.
**
**  -) f_pEncryptedRegionMappings
**     will have 6 entries i.e. 2*(2+1) entries.
**
**  -) Since f_pEncryptedRegionCounts->pdwData[0]==2
**
**     -) The first f_pEncryptedRegionMappings->pdwData[0]
**        bytes in f_pEncrypted are clear.
**     -) The next f_pEncryptedRegionMappings->pdwData[1]
**        bytes in f_pEncrypted are encrypted with
**        f_pEncryptedRegionInitializatiolwectorsHigh->pqwData[0]
**     -) The next f_pEncryptedRegionMappings->pdwData[2]
**        bytes in f_pEncrypted are clear.
**     -) The next f_pEncryptedRegionMappings->pdwData[3]
**        bytes in f_pEncrypted are (also) encrypted with
**        f_pEncryptedRegionInitializatiolwectorsHigh->pqwData[0]
**
**  -) Since f_pEncryptedRegionCounts->pdwData[1]==1
**
**     -) The next f_pEncryptedRegionMappings->pdwData[4]
**        bytes in f_pEncrypted are clear.
**     -) The next f_pEncryptedRegionMappings->pdwData[5]
**        bytes in f_pEncrypted are (also) encrypted with
**        f_pEncryptedRegionInitializatiolwectorsHigh->pqwData[1]
**
**  -) Sum of the six values in f_pEncryptedRegionInitializatiolwectorsHigh
**     will equal f_pEncrypted->cb.
**
**  If f_pEncryptedRegionSkip is non-empty, then each encrypted
**  region above will be partially encrypted.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_AES128CTR_DecryptContentMultiple(
    __inout_opt                 DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pCDKB,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pOEMKeyInfo,
    __in                  const DRM_TEE_QWORDLIST            *f_pEncryptedRegionInitializatiolwectorsHigh,
    __in_tee_opt          const DRM_TEE_QWORDLIST            *f_pEncryptedRegionInitializatiolwectorsLow,
    __in                  const DRM_TEE_DWORDLIST            *f_pEncryptedRegionCounts,
    __in                  const DRM_TEE_DWORDLIST            *f_pEncryptedRegionMappings,
    __in_tee_opt          const DRM_TEE_DWORDLIST            *f_pEncryptedRegionSkip,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pEncrypted,
    __out                       DRM_TEE_BYTE_BLOB            *f_pCCD )
{
    DRM_RESULT  dr = DRM_SUCCESS;

    ChkDR( DRM_TEE_AES128_DecryptContentHelper(
        f_pContext,
        DRM_TEE_POLICY_INFO_CALLING_API_AES128CTR_DECRYPTCONTENTMULTIPLE,
        f_pCDKB,
        f_pOEMKeyInfo,
        f_pEncryptedRegionInitializatiolwectorsHigh,
        f_pEncryptedRegionInitializatiolwectorsLow,
        f_pEncryptedRegionCounts,
        f_pEncryptedRegionMappings,
        f_pEncryptedRegionSkip,
        f_pEncrypted,
        f_pCCD ) );

ErrorExit:
    return dr;
}
#endif

EXIT_PK_NAMESPACE_CODE;
