/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <drmtee.h>
#include <drmteesupported.h>
#include <drmh264.h>
#include <drmteebase.h>
#include <drmteexb.h>
#include <drmteekbcryptodata.h>
#include <drmerr.h>
#include <drmmathsafe.h>
#include <oemtee.h>
#include <oembyteorder.h>
#include <drmteedecryptinternal.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_TEE_IMPL_H264_IsH264Supported( DRM_VOID )
{
    return TRUE;
}
#endif

#ifdef NONE
/*
** These three functions are required because the callback passed to DRM_H264_LocateAndVerifySliceHeaders
** take DRM_VOID* for their context but the function implementations we actually want to use take OEM_TEE_CONTEXT.
*/
static DRM_NO_INLINE DRM_RESULT DRM_CALL _SelwreMemAllocWrapper(
    __inout_opt                                     DRM_VOID                   *f_pCallbackContextAllowNULL,
    __in                                            DRM_DWORD                   f_cbSize,
    __deref_out_bcount( f_cbSize )                  DRM_VOID                  **f_ppv )
{
    return OEM_TEE_BASE_SelwreMemAlloc( (OEM_TEE_CONTEXT*)f_pCallbackContextAllowNULL, f_cbSize, f_ppv );
}

static DRM_NO_INLINE DRM_VOID DRM_CALL _SelwreMemFreeWrapper(
    __inout_opt                                     DRM_VOID                   *f_pCallbackContextAllowNULL,
    __inout                                         DRM_VOID                  **f_ppv )
{
    OEM_TEE_BASE_SelwreMemFree( (OEM_TEE_CONTEXT*)f_pCallbackContextAllowNULL, f_ppv );
}

static DRM_NO_INLINE DRM_RESULT DRM_CALL _GetSystemTimeAsFileTimeNotSetWrapper(
    __inout_opt                                     DRM_VOID                   *f_pCallbackContextAllowNULL,
    __out                                           DRMFILETIME                *f_pftSystemTime )
{
    DRMASSERT( f_pCallbackContextAllowNULL != NULL );
    __analysis_assume( f_pCallbackContextAllowNULL != NULL );
    return OEM_TEE_CLOCK_GetTimestamp( DRM_REINTERPRET_CAST( OEM_TEE_CONTEXT, f_pCallbackContextAllowNULL ), f_pftSystemTime );
}

/*
** Synopsis:
**
** This function preprocesses a frame of h.264 content.
**
** This function is not called by any Drm_* APIs.
**
** This function is only called if
** DRM_TEE_PROPERTY_SUPPORTS_PRE_PROCESS_ENCRYPTED_DATA  and/or
** DRM_TEE_PROPERTY_REQUIRES_PRE_PROCESS_ENCRYPTED_DATA_WITH_FULL_FRAMES
** are included in the bitmask returned from OEM_TEE_BASE_GetVersionInformation
** in the f_pdwProperties output parameter.
**
** Operations Performed:
**
**  1. Verify the signature on the given DRM_TEE_CONTEXT.
**  2. Retrieve the data required for decryption from one of the following
**     procedures.
**     a. If you have provided the OEM key information via implementing the
**        OEM_TEE_DECRYPT_BuildExternalPolicyCryptoInfo, we will first attempt
**        to retrieve the decryption data from the OEM key info by calling
**        OEM_TEE_DECRYPT_ParseAndVerifyExternalPolicyCryptoInfo.  If you
**        implement this function and successfully return the keys, session, and
**        policy, the CDKB will not be parsed and used.  This option can be used
**        to either if the OEM key information can be stored in the TEE or the
**        decryption data can be cryptographically protected with better performance
**        by your own implementation.  If you choose this option and the data will
**        not remain in the TEE, you MUST make sure the keys are cryptographically
**        protected and the policy can be validated not to have changed (via
**        cryptographic signature).
**     b. If OEM_TEE_DECRYPT_ParseAndVerifyExternalPolicyCryptoInfo returns
**        DRM_E_NOTIMPL, or the key count returned is zero, the given Content
**        Decryption Key Blob (CDKB) will be parsed and its signature verified.
**  3. Decrypt the given encrypted partial content frame.
**     (using the given region mapping and IV) into a clear buffer.
**  4. Parse the partial frame to locate the Slice Header(s).
**     If the slice header cannot be found or there is an error while parsing it,
**     fail with DRM_E_H264_PARSING_FAILED.
**  5. If transcryption is required, decrypt and encrypt the full frame.
**  6. Return any updated opaque state and the slice header(s) (in the clear).
**     If transcryption is required, also return the transcrypted frame
**     and any opaque frame data required by the codec.
**
** Arguments:
**
** f_pContext:                          (in/out) The TEE context returned from
**                                               DRM_TEE_BASE_AllocTEEContext.
** f_pCDKB:                             (in)     The Content Decryption Key Blob (CDKB) returned
**                                               from DRM_TEE_DECRYPT_PrepareToDecrypt.
** f_pOEMKeyInfo:                       (in)     The OEM specific key data blob returned from
**                                               DRM_TEE_DECRYPT_PrepareToDecrypt.
** f_pui64Initializatiolwector:         (in/out) On input, the AES128CTR initialization vector
**                                               used to decrypt the content using PlayReady.
**                                               On output, the updated value used for OEM
**                                               decryption of the transcrypted content
**                                               (if transcryption was performed).
**                                               If transcryption is performed, the value
**                                               of this initialization vector MUST change.
**                                               You MUST NOT use the same initialization vector
**                                               for the transcrypted version of the content.
** f_pEncryptedRegionMapping:           (in)     The encrypted-region mapping.
**                                               f_pEncryptedRegionMapping->pdwData points to an array of
**                                               DRM_DWORDs.
**
**                                               The number of DRM_DWORDs must be a multiple
**                                               of two.  Each DRM_DWORD pair represents
**                                               values indicating how many clear bytes
**                                               followed by how many encrypted bytes are
**                                               present in the data on output.
**                                               The sum of all of these DRM_DWORD pairs
**                                               must be equal to bytecount(clear content),
**                                               i.e. equal to f_pEncryptedTranscryptedFullFrame->cb
**                                               (if specified).
**
**                                               f_pEncryptedRegionMapping->cdwData is set to the number
**                                               of DRM_DWORDs and will always be a multiple of two.
**
**                                               If the entire frame is encrypted, there will be
**                                               two DRM_DWORDs with values:
**                                               0, bytecount(encrypted content).
**                                               If the entire frame is clear, there will be
**                                               two DRM_DWORDs with values:
**                                               bytecount(clear content), 0.
** f_pEncryptedPartialFrame:            (in)     The PlayReady-encrypted partial frame used for
**                                               slice header parsing.
** f_pOpaqueSliceHeaderOffsetData:      (in)     The offset data returned from
**                                               DRM_H264_FindSliceHeaderData.
**                                               This opaque data indicates which portions of the frame
**                                               were passed to this function to enable proper
**                                               decryption.
** f_pOpaqueSliceHeaderState:           (in)     Opaque state from a previous call to this function.
**                                               f_pOpaqueSliceHeaderState->cb will be zero on input
**                                               on the first call.
** f_pOpaqueSliceHeaderStateUpdated:    (out)    Opaque state for the next call to this function.
**                                               If f_pOpaqueSliceHeaderStateUpdated->cb is zero on
**                                               output, then the opaque state from the previous call
**                                               to this function (if any) should be used for the next
**                                               call to this function.
** f_pSliceHeaders:                     (out)    The slice header(s) to be passed to the h.264 codec.
** f_pOpaqueFrameData:                  (out)    Opaque frame data to be sent to the codec.
**                                               This data is treated as opaque by PlayReady.
**                                               This may be returned as DRM_TEE_BYTE_BLOB_EMPTY
**                                               if no such data is required.
** f_pEncryptedTranscryptedFullFrame:   (in/out) On input, the PlayReady-encrypted complete frame.
**                                               On output, either the data will be unchanged or will
**                                               be the OEM-encrypted (transcrypted) complete frame.
**                                               This parameter will contain data if (and only if)
**                                DRM_TEE_PROPERTY_REQUIRES_PRE_PROCESS_ENCRYPTED_DATA_WITH_FULL_FRAMES
**                                               is included in the property set returned by
**                                               OEM_TEE_BASE_GetVersionInformation.
**                                               If transcryption is being performed but it fails, this
**                                               buffer will be set to all zeroes on exit to ensure
**                                               that clear content is not leaked out of the TEE.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_H264_PreProcessEncryptedData(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pCDKB,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pOEMKeyInfo,
    __inout_ecount( 1 )         DRM_UINT64                   *f_pui64Initializatiolwector,
    __in                  const DRM_TEE_DWORDLIST            *f_pEncryptedRegionMapping,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pEncryptedPartialFrame,
    __in_tee_opt          const DRM_TEE_DWORDLIST            *f_pOpaqueSliceHeaderOffsetData,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pOpaqueSliceHeaderState,
    __out                       DRM_TEE_BYTE_BLOB            *f_pOpaqueSliceHeaderStateUpdated,
    __out                       DRM_TEE_BYTE_BLOB            *f_pSliceHeaders,
    __out                       DRM_TEE_BYTE_BLOB            *f_pOpaqueFrameData,
    __inout_tee_opt             DRM_TEE_BYTE_BLOB            *f_pEncryptedTranscryptedFullFrame )
{
    DRM_RESULT                   dr                             = DRM_SUCCESS;
    DRM_DWORD                    cKeys                          = 0;
    DRM_TEE_KEY                 *pKeys                          = NULL;
    DRM_TEE_BYTE_BLOB            oCCD                           = DRM_TEE_BYTE_BLOB_EMPTY;
    DRM_DWORD                    cbOpaqueStateUpdated           = 0;
    DRM_BYTE                    *pbOpaqueStateUpdated           = NULL;
    DRM_DWORD                    cbSliceHeaders                 = 0;
    DRM_BYTE                    *pbSliceHeaders                 = NULL;
    DRM_TEE_BYTE_BLOB            oOpaqueStateUpdated            = DRM_TEE_BYTE_BLOB_EMPTY;
    DRM_TEE_BYTE_BLOB            oSliceHeaders                  = DRM_TEE_BYTE_BLOB_EMPTY;
    OEM_TEE_CONTEXT             *pOemTeeCtx                     = NULL;
    DRM_TEE_POLICY_INFO         *pPolicy                        = NULL;
    DRM_DWORD                    cERMs                          = 0;
    const DRM_DWORD             *pcbERMs                        = NULL;
    DRM_DWORD                    cbOpaqueFrameData              = 0;
    DRM_BYTE                    *pbOpaqueFrameData              = NULL;
    DRM_TEE_BYTE_BLOB            oOpaqueFrameData               = DRM_TEE_BYTE_BLOB_EMPTY;

    /* Arguments are checked in the PRITEE proxy layer */
    DRMASSERT( f_pContext != NULL );
    DRMASSERT( f_pui64Initializatiolwector != NULL );
    DRMASSERT( f_pEncryptedRegionMapping != NULL );
    DRMASSERT( f_pOpaqueSliceHeaderOffsetData != NULL );
    DRMASSERT( f_pEncryptedPartialFrame != NULL );
    DRMASSERT( IsDwordListAssigned( f_pEncryptedRegionMapping ) );
    DRMASSERT( IsBlobAssigned( f_pEncryptedPartialFrame ) );

    pOemTeeCtx = &f_pContext->oContext;

    /* f_pEncryptedRegionMapping must have a multiple of 2 DWORDs, each DWORD pair is a clear length followed by an encrypted length. */
    ChkArg( f_pEncryptedRegionMapping->cdwData > 0 );
    ChkArg( ( f_pEncryptedRegionMapping->cdwData & 1 ) == 0 );     /* Power of two: use & instead of % for better perf */

    /* f_pOpaqueSliceHeaderOffsetData must have a multiple of 2 DWORDs, each DWORD pair is a care length followed by an don't-care length. */
    ChkArg( ( f_pOpaqueSliceHeaderOffsetData->cdwData & 1 ) == 0 );     /* Power of two: use & instead of % for better perf */

    ChkDRAllowENOTIMPL( DRM_TEE_IMPL_DECRYPT_ParseAndVerifyExternalPolicyCryptoInfo(
        f_pContext,
        f_pCDKB,
        f_pOEMKeyInfo,
        FALSE,
        &cKeys,
        &pKeys,
        &pPolicy,
        NULL ) );

    if( cKeys == 0 )
    {
        AssertChkBOOL( pKeys == NULL );
        AssertChkBOOL( pPolicy == NULL );
        ChkDR( DRM_TEE_IMPL_DECRYPT_ParseAndVerifyCDKB(
            f_pContext,
            f_pCDKB,
            FALSE,
            &cKeys,
            &pKeys,
            NULL,           /* No need for SelwreStop2Key here */
            NULL,           /* No need for idSelwreStopSession here */
            &pPolicy ) );
    }

    AssertChkBOOL( cKeys == 2 || cKeys == 3 );
    ChkDR( DRM_TEE_IMPL_DECRYPT_EnforcePolicy( &f_pContext->oContext, DRM_TEE_POLICY_INFO_CALLING_API_H264_PREPROCESSENCRYPTEDDATA, pPolicy ) );

    /* Number of encrypted region mappings and encrypted region mappings. */
    cERMs   = f_pEncryptedRegionMapping->cdwData;
    pcbERMs = f_pEncryptedRegionMapping->pdwData;

    {
        /*
        ** The caller never gives us the whole frame.  Instead, they give us a buffer containing only the NALUs we care about
        ** (which were returned by DRM_H264_FindSliceHeaderData).  While this vastly decreases the amount of data crossing
        ** the inselwre-secure boundary into the PRITEE, it also increases complexity here as we have to figure out which
        ** portions to decrypt and decrypt them one-by-one (instead of simply using the encrypted region data by itself).
        ** In the process, we need to form a single contiguous clear buffer containing the given partial frame so that
        ** we can pass it down to DRM_H264_LocateAndVerifySliceHeader and retrieve its slice header data.
        */

        /* Which ERM we are acting upon. */
        DRM_DWORD  iERM                           = 0;

        /* Whether the ERM we are acting upon is clear or encrypted. */
        DRM_BOOL   fERMClear                      = TRUE;     /* First item in an encrypted region mapping pair is clear */

        /* The number of remaining bytes in the ERM we are acting upon. */
        DRM_DWORD  cbERM                          = 0;

        /* The number of total care/don't-care offset data entries we have. */
        DRM_DWORD  cCares                         = f_pOpaqueSliceHeaderOffsetData->cdwData;

        /* The care data entries.  We know this is aligned because we just allocated it ourselves. */
        const DRM_DWORD *pcbCares                 = f_pOpaqueSliceHeaderOffsetData->pdwData;

        /* Which care entry we are acting upon. */
        DRM_DWORD  iCare                          = 0;

        /* Whether we care about the current offset data. */
        DRM_BOOL   fCare                          = TRUE;     /* First item in an offset data pair is "care" */

        /* The number of remaining bytes in the Care we are acting upon. */
        DRM_DWORD  cbCare                         = 0;

        /* Where we are in the partial frame that was passed in. */
        DRM_DWORD  ibPartialFrame                 = 0;

        /*
        ** The original frame has encrypted/clear portions.
        ** The encrypted portions are treated as if they were a single contiguous AES-encrypted buffer.
        ** Variable ibOriginalDecrypt represents our current offset in that AES-encrypted buffer.
        */
        DRM_DWORD  ibOriginalDecrypt               = 0;

        /*
        ** We verify that the sum of all care offset data entries we have matches the size of the given encrypted partial frame
        ** since the given encrypted partial frame should include exactly the bytes we care about - no more, no less.
        ** (We can use cbCare for this loop because it will be overwritten below.)
        */
        for( iCare = 0; iCare < cCares; iCare += 2 )    /* Can't overflow: cCares % 2 == 0 */
        {
            ChkDR( DRM_DWordAddSame( &cbCare, pcbCares[ iCare ] ) );
        }
        ChkArg( cbCare == f_pEncryptedPartialFrame->cb );

        /*
        ** We verify that the sum of all ERM entries we have contain sufficient bytes to encompass the entire passed in buffer.
        ** Otherwise, the ERMs could not possibly have mapped over the entire sample because the passed in buffer is a subset
        ** of the sample.
        ** (We can use cbERM for this loop because it will be overwritten below.)
        */
        for( iERM = 0; iERM < cERMs; iERM++ )
        {
            ChkDR( DRM_DWordAddSame( &cbERM, pcbERMs[ iERM ] ) );
        }
        ChkArg( cbERM >= f_pEncryptedPartialFrame->cb );

        /*
        ** When we're done decrypting, oCCD will have the entire partial frame we were passed in but with every byte in the clear.
        */
        ChkDR( DRM_TEE_IMPL_BASE_AllocBlob( f_pContext, DRM_TEE_BLOB_ALLOC_BEHAVIOR_COPY, f_pEncryptedPartialFrame->cb, f_pEncryptedPartialFrame->pb, &oCCD ) );
        __analysis_assume( oCCD.pb != NULL );

        iERM   = 0;
        cbERM  = pcbERMs[ 0 ];
        iCare  = 0;
        cbCare = pcbCares[ 0 ];

        /* Now, we walk through all the entries in both arrays. */
        while( iCare < cCares )
        {
            AssertChkBOOL( iERM < cERMs );
            if( cbCare == 0 )
            {
                /* We've completed the current care entry.  Move onto the next (which will have the opposite care/don't-care state as the current one). */
                iCare++;
                if( iCare < cCares )
                {
                    cbCare = pcbCares[ iCare ];
                }

                fCare = !fCare;
            }
            else if( cbERM == 0 )
            {
                /* We've completed the current ERM entry.  Move onto the next (which will have the opposite encrypted/clear state as the current one). */
                iERM++;
                AssertChkBOOL( iERM < cERMs );
                cbERM = pcbERMs[ iERM ];
                fERMClear = !fERMClear;
            }
            else
            {
                /*
                ** We are going to consume the smaller of the following:
                **  The number of bytes remaining in the current care entry.
                **  The number of bytes remaining in the current ERM.
                ** In either case, cbAdvance represents:
                **  The next number of bytes in the original frame which were clear OR encrypted [NOT both] (based on the current value of fERMClear).
                **  AND
                **  The next number of bytes in the original frame which we cared about OR did not care about [NOT both] (based on the current value of fCare).
                **  (If fCare, they were copied into the partial frame we were given.  Otherwise, the caller did not give them to us.)
                ** If we care about them, we are going to leave them as-is
                **  (because they were already in the clear) or decrypt them in-place
                **  (because they were encrypted).
                ** If we don't care about them, we ignore them except for the fact that
                **  if they were encrypted we need to advance within the original frame's
                **  AES-encrypted buffer mentioned previously.
                */
                DRM_DWORD cbAdvance = DRM_MIN( cbCare, cbERM );
                if( fERMClear )
                {
                    /*
                    ** This segment in the original content buffer is clear.
                    */
                    if( fCare )
                    {
                        /*
                        ** We care about it, so it exists in the passed in partial frame in the clear.
                        ** Therefore, we simply need to advance within the given partial frame
                        **  (because oCCD represents a copy of the original partial frame).
                        */
                        ibPartialFrame += cbAdvance;    /* Can't overflow: guaranteed by checks above */
                    }
                    else
                    {
                        /*
                        ** We didn't care about it, so all we need to do is discard it.
                        ** This block is a no-op because we have no additional processing to do.
                        */
                    }
                }
                else
                {
                    if( fCare )
                    {
                        /*
                        ** We care about it, so it exists in the passed in partial frame encrypted.
                        ** Therefore, we need to first decrypt it in-place
                        **  (because oCCD represents a copy of the original partial frame in the clear).
                        ** Every time we decrypt part of oCCD, we do so at offset ibPartialFrame,
                        **  i.e. how far we were into the original partial frame.
                        */

                        /* Required for the next call to work with >> */
                        DRMCASSERT( DRM_AES_BLOCKLEN == 16 );

                        /*
                        ** The original frame has encrypted/clear portions.
                        ** The encrypted portions are treated as if they were a single contiguous AES-encrypted buffer.
                        ** Variable ibOriginalDecrypt represents our current offset in that AES-encrypted buffer,
                        **  and here we colwert it into a block offset (ibOriginalDecrypt / DRM_AES_BLOCKLEN)
                        **  and a byte offset within that block (ibOriginalDecrypt % DRM_AES_BLOCKLEN).
                        ** We decrypt it in-place in oCCD.
                        ** The number of bytes we are decrypting is equal to the number of bytes
                        **  we are consuming from the original frame, i.e. cbAdvance.
                        */
                        ChkDR( OEM_TEE_H264_DecryptContentFragmentIntoClear(
                            pOemTeeCtx,
                            DRM_TEE_POLICY_INFO_CALLING_API_H264_PREPROCESSENCRYPTEDDATA,
                            pPolicy,
                            &pKeys[ 1 ].oKey,
                            *f_pui64Initializatiolwector,
                            DRM_UI64( ibOriginalDecrypt >> (DRM_DWORD)4 ), /* Power of two: use >> instead of / for better perf */
                            ibOriginalDecrypt & ( DRM_AES_BLOCKLEN - 1 ),   /* Power of two: use  & instead of % for better perf */
                            cbAdvance,
                            (DRM_BYTE*)&oCCD.pb[ ibPartialFrame ] ) );

                        /* We then advance within the given partial frame. */
                        ibPartialFrame += cbAdvance;    /* Can't overflow: guaranteed by checks above */
                    }

                    /*
                    ** Regardless of whether we cared about it or not, an encrypted portion
                    **  means we need to advance our offset into the original frame's
                    **  AES-encrypted buffer mentioned previously.
                    */
                    ibOriginalDecrypt += cbAdvance;     /* Can't overflow: guaranteed by checks above */
                }

                /* Finally, we update the number of bytes remaining in the current entry in each array. */
                cbCare -= cbAdvance;    /* Can't underflow: guaranteed by checks above */
                cbERM  -= cbAdvance;    /* Can't underflow: guaranteed by checks above */
            }
        }
    }

    {
        LPFN_H264_MEM_ALLOCATE  pfnMemAllocate = _SelwreMemAllocWrapper;
        LPFN_H264_MEM_FREE      pfnMemFree = _SelwreMemFreeWrapper;
        LPFN_H264_MEM_GET_TIME  pfnGetTime = _GetSystemTimeAsFileTimeNotSetWrapper;
        ChkDR( DRM_H264_LocateAndVerifySliceHeaders(
            pfnMemAllocate,
            pfnMemFree,
            pfnGetTime,
            pOemTeeCtx,
            oCCD.cb,
            oCCD.pb,
            f_pOpaqueSliceHeaderState->cb,
            f_pOpaqueSliceHeaderState->pb,
            &cbOpaqueStateUpdated,
            &pbOpaqueStateUpdated,
            &cbSliceHeaders,
            &pbSliceHeaders ) );
    }

    if( IsBlobAssigned( f_pEncryptedTranscryptedFullFrame ) )
    {
        dr = OEM_TEE_H264_PreProcessEncryptedData(
            pOemTeeCtx,
            &pKeys[ 1 ].oKey,
            f_pOEMKeyInfo->cb,
            f_pOEMKeyInfo->pb,
            f_pui64Initializatiolwector,
            cERMs,
            pcbERMs,
            &cbOpaqueFrameData,
            &pbOpaqueFrameData,
            f_pEncryptedTranscryptedFullFrame->cb,
            (DRM_BYTE *)f_pEncryptedTranscryptedFullFrame->pb );
        if( dr != DRM_E_NOTIMPL )
        {
            if( DRM_FAILED( dr ) )
            {
                /* Failed transcryption may have left some or all of the content in the clear.  Zero it out to ensure it is not leaked. */
                ChkVOID( OEM_TEE_H264_ZERO_MEMORY( NULL, f_pEncryptedTranscryptedFullFrame->cb, (DRM_BYTE *)f_pEncryptedTranscryptedFullFrame->pb ) );
                if( pbOpaqueFrameData != NULL && cbOpaqueFrameData > 0 )
                {
                    ChkVOID( OEM_TEE_ZERO_MEMORY( pbOpaqueFrameData, cbOpaqueFrameData ) );
                }
                ChkDR( dr );
            }
        }
        else
        {
            dr = DRM_SUCCESS;
        }
    }

    if( pbOpaqueFrameData != NULL )
    {
        ChkDR( DRM_TEE_IMPL_BASE_AllocBlob( f_pContext, DRM_TEE_BLOB_ALLOC_BEHAVIOR_COPY, cbOpaqueFrameData, pbOpaqueFrameData, &oOpaqueFrameData ) );
    }
    if( pbOpaqueStateUpdated != NULL )
    {
        ChkDR( DRM_TEE_IMPL_BASE_AllocBlobAndTakeOwnership( f_pContext, cbOpaqueStateUpdated, &pbOpaqueStateUpdated, &oOpaqueStateUpdated ) );
    }
    ChkDR( DRM_TEE_IMPL_BASE_AllocBlobAndTakeOwnership( f_pContext, cbSliceHeaders, &pbSliceHeaders, &oSliceHeaders ) );

    ChkVOID( DRM_TEE_IMPL_BASE_TransferBlobOwnership( f_pOpaqueSliceHeaderStateUpdated, &oOpaqueStateUpdated ) );
    ChkVOID( DRM_TEE_IMPL_BASE_TransferBlobOwnership( f_pSliceHeaders, &oSliceHeaders ) );
    ChkVOID( DRM_TEE_IMPL_BASE_TransferBlobOwnership( f_pOpaqueFrameData, &oOpaqueFrameData ) );

ErrorExit:
    ChkVOID( DRM_TEE_IMPL_BASE_FreeKeyArray( f_pContext, cKeys, &pKeys ) );
    ChkVOID( DRM_TEE_IMPL_BASE_FreeBlob( f_pContext, &oCCD ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&pbOpaqueStateUpdated ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&pbSliceHeaders ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&pbOpaqueFrameData ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&pPolicy ) );
    ChkVOID( DRM_TEE_IMPL_BASE_FreeBlob( f_pContext, &oOpaqueStateUpdated ) );
    ChkVOID( DRM_TEE_IMPL_BASE_FreeBlob( f_pContext, &oSliceHeaders ) );
    ChkVOID( DRM_TEE_IMPL_BASE_FreeBlob( f_pContext, &oOpaqueFrameData ) );
    return dr;
}
#endif

EXIT_PK_NAMESPACE_CODE;

