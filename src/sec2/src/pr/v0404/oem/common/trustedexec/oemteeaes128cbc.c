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
#ifdef SEC_COMPILE
/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** This function is only used if the client supports playback of AES CBC encrypted content.
** Certain PlayReady clients may not need to implement this function.
**
** Synopsis:
**
** This function in-place decrypts content into the clear using AES128CBC.
**
** Operations Performed:
**
**  1. In-place decrypt the given content into the clear using AES128CBC.
**
** Arguments:
**
** f_pContext:                      (in/out) The TEE context returned from
**                                           OEM_TEE_BASE_AllocTEEContext.
** f_ePolicyCallingAPI:             (in)     The calling API providing f_pPolicy.
** f_pPolicy:                       (in)     The current content protection policy.
** f_pCK:                           (in)     The content key used to decrypt the content.
** f_cEncryptedRegionMappings:      (in)     The number of encrypted-region mappings.
** f_pdwEncryptedRegionMappings:    (in)     The set of encrypted region mappings as pairs of
**                                           DRM_DWORDs.
**                                           The number of DRM_DWORDs must be a multiple
**                                           of two.  Each DRM_DWORD pair represents
**                                           values indicating how many clear bytes
**                                           followed by how many encrypted bytes are
**                                           present in the data on output.
**                                           If the entire f_pEncrypted is encrypted, there will be
**                                           two DRM_DWORDs with values 0, bytecount(encrypted content).
**                                           If the entire f_pEncrypted is clear, there will be
**                                           two DRM_DWORDs with values bytecount(clear content), 0.
** f_ui64InitializatiolwectorHigh:  (in)     The high 8 bytes of the AES128CBC initialization vector.
** f_ui64InitializatiolwectorLow:   (in)     The low 8 bytes of the AES128CBC initialization vector.
**                                           Will be zero if an 8 byte IV is being used.
** f_cEncryptedRegionSkip:          (in)     Number of entries in f_pcEncryptedRegionSkip.
**                                           Will always be zero or two.
** f_pcEncryptedRegionSkip:         (in)     Ignored if f_cEncryptedRegionSkip is zero.
**                                           Otherwise, defines the striping of the encrypted portions
**                                           of the given encrpted region.
**                                           f_pcEncryptedRegionSkip[0] defines the number of
**                                           encrypted BLOCKS in each stripe.
**                                           f_pcEncryptedRegionSkip[1] defines the number of
**                                           clear BLOCKS in each stripe.
** f_cbEncryptedToClear:            (in)     The number of bytes in f_pbEncryptedToClear.
** f_pbEncryptedToClear:            (in/out) On input, the encrypted content to decrypt.
**                                           On output, the clear content.
**                                           Only the bytes starting at offset f_ibProcessing
**                                           through the toal number of bytes in
**                                           f_pdwEncryptedRegionMappings will be processed.
** f_ibProcessing:                  (in)     Offset into f_pbEncryptedToClear to decrypt.
**                                           f_pdwEncryptedRegionMappings determines
**                                           the number of bytes to decrypt.
** f_pcbProcessed:                  (out)    If non-NULL, outputs the number of bytes processed,
**                                           i.e. the sum of all entries in
**                                           f_pdwEncryptedRegionMappings.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_AES128CBC_DecryptContentIntoClear(
    __inout                                          OEM_TEE_CONTEXT                 *f_pContext,
    __in                                             DRM_TEE_POLICY_INFO_CALLING_API  f_ePolicyCallingAPI,
    __in                                       const DRM_TEE_POLICY_INFO             *f_pPolicy,
    __in                                       const OEM_TEE_KEY                     *f_pCK,
    __in                                             DRM_DWORD                        f_cEncryptedRegionMappings,
    __in_ecount( f_cEncryptedRegionMappings )  const DRM_DWORD                       *f_pdwEncryptedRegionMappings,
    __in                                             DRM_UINT64                       f_ui64InitializatiolwectorHigh,
    __in                                             DRM_UINT64                       f_ui64InitializatiolwectorLow,
    __in                                             DRM_DWORD                        f_cEncryptedRegionSkip,
    __in_ecount_opt( f_cEncryptedRegionSkip )  const DRM_DWORD                       *f_pcEncryptedRegionSkip,
    __in                                             DRM_DWORD                        f_cbEncryptedToClear,
    __inout_bcount( f_cbEncryptedToClear )           DRM_BYTE                        *f_pbEncryptedToClear,
    __in                                             DRM_DWORD                        f_ibProcessing,
    __out_opt                                        DRM_DWORD                       *f_pcbProcessed )
{
    DRM_RESULT                     dr                               = DRM_SUCCESS;
    DRM_BYTE                       rgbIV[ DRM_AES_BLOCKLEN ]        = { 0 };
    DRM_BYTE                       rgbIVLwrrent[ DRM_AES_BLOCKLEN ] = { 0 };
    DRM_DWORD                      idx;                             /* loop variable */
    DRM_DWORD                      cbTotalEncrypted                 = 0;
    DRM_DWORD                      cbTotalClear                     = 0;
    DRM_DWORD                      cbTotalProcessed                 = 0;
    DRM_BYTE                      *pbLwrrent                        = NULL;
    DRM_DWORD                      cbEncryptedStripe                = 0;
    DRM_DWORD                      cbStripe                         = 0;

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );
    DRMASSERT( f_pCK != NULL );
    DRMASSERT( f_cEncryptedRegionMappings > 0 );
    DRMASSERT( ( f_cEncryptedRegionMappings & 1 ) == 0 );     /* Must be DWORD pairs.  Power of two: use & instead of % for better perf */
    DRMASSERT( f_pdwEncryptedRegionMappings != NULL );
    DRMASSERT( f_cbEncryptedToClear > 0 );
    DRMASSERT( f_pbEncryptedToClear != NULL );
    DRMASSERT( f_pPolicy->eSymmetricEncryptionType == XMR_SYMMETRIC_ENCRYPTION_TYPE_AES_128_CBC );
    DRMASSERT( f_ibProcessing < f_cbEncryptedToClear );
    DRMASSERT( f_cEncryptedRegionSkip == 0 || f_cEncryptedRegionSkip == 2 );
    __analysis_assume( f_ibProcessing < f_cbEncryptedToClear );

    pbLwrrent = &f_pbEncryptedToClear[ f_ibProcessing ];

    /* Underlying CBC implementation requires big endian IV */
    DRMCASSERT( sizeof( rgbIV ) == ( sizeof( f_ui64InitializatiolwectorHigh ) + sizeof( f_ui64InitializatiolwectorLow ) ) );
    ChkVOID( DRM_NATIVE_QWORD_TO_BIG_ENDIAN_BYTES( &rgbIV[ 0 ], f_ui64InitializatiolwectorHigh ) );
    ChkVOID( DRM_NATIVE_QWORD_TO_BIG_ENDIAN_BYTES( &rgbIV[ sizeof( f_ui64InitializatiolwectorHigh ) ], f_ui64InitializatiolwectorLow ) );
    ChkVOID( OEM_TEE_MEMCPY( rgbIVLwrrent, rgbIV, sizeof( rgbIVLwrrent ) ) );

    if( f_cEncryptedRegionSkip == 2 )
    {
        /* Shift to colwert BLOCKS to BYTES */
        DRMCASSERT( DRM_AES_BLOCKLEN == 16 );   /* Required for below to work without modification */
        cbEncryptedStripe = f_pcEncryptedRegionSkip[ 0 ] << 4;
        cbStripe = cbEncryptedStripe + ( f_pcEncryptedRegionSkip[ 1 ] << 4 );
    }

    for( idx = 0; idx < f_cEncryptedRegionMappings; )
    {
        DRM_DWORD    cbClear;
        DRM_DWORD    cbEncrypted;

        cbClear = f_pdwEncryptedRegionMappings[ idx++ ];

        /* We already confirmed this by ensuring that f_cEncryptedRegionMappings is not odd. */
        __analysis_assume( idx < f_cEncryptedRegionMappings );

        cbEncrypted = f_pdwEncryptedRegionMappings[ idx++ ];

        ChkArg( ( cbClear > 0 ) || ( cbEncrypted > 0 ) );

        ChkDR( DRM_DWordAddSame( &cbTotalClear, cbClear ) );
        ChkDR( DRM_DWordAddSame( &cbTotalEncrypted, cbEncrypted ) );
        ChkDR( DRM_DWordAdd( cbTotalClear, cbTotalEncrypted, &cbTotalProcessed ) );

        ChkArg( cbTotalProcessed <= f_cbEncryptedToClear - f_ibProcessing );

        if( cbClear > 0 )
        {
            ChkDR( DRM_DWordPtrAddSame( (DRM_SIZE_T*)&pbLwrrent, cbClear ) );
        }

        if( cbEncrypted > 0 )
        {
            /* We already confirmed this via ChkArg( cbTotalProcessed <= f_cbEncryptedToClear - f_ibProcessing ). */
            __analysis_assume( pbLwrrent + cbEncrypted + f_ibProcessing <= f_pbEncryptedToClear + f_cbEncryptedToClear );

            if( cbStripe == 0 )
            {
                /* No striping: handle the whole thing at once */
                DRM_BYTE rgbIVTemp[ DRM_AES_BLOCKLEN ] = { 0 };

                DRM_DWORD cbToDecrypt = cbEncrypted;

                /*
                ** Any final partial block is in the clear.  Don't attempt to decrypt it.
                */
                cbToDecrypt -= ( cbToDecrypt & (DRM_DWORD)15 );

                /*
                ** In 'cbc1', each encrypted region continues the CBC chain across regions
                ** from the last block of one region into the first block of the next region,
                ** i.e. the last block of *ciphertext* becomes the IV for the next block.
                */
                ChkVOID( OEM_TEE_MEMCPY( rgbIVTemp, &pbLwrrent[ cbToDecrypt - DRM_AES_BLOCKLEN ], DRM_AES_BLOCKLEN ) );

                ChkDR( OEM_TEE_CRYPTO_AES128_CbcDecryptData( f_pContext, f_pCK, pbLwrrent, cbToDecrypt, rgbIVLwrrent ) );

                ChkVOID( OEM_TEE_MEMCPY( rgbIVLwrrent, rgbIVTemp, DRM_AES_BLOCKLEN ) );
                ChkDR( DRM_DWordPtrAddSame( (DRM_SIZE_T*)&pbLwrrent, cbEncrypted ) );
            }
            else
            {
                /* Striping: handle it in pieces */
                DRM_DWORD cbRemaining;  /* loop variable */

                /*
                ** In 'cbcs', the IV resets at the start of each encrypted region.
                */
                ChkVOID( OEM_TEE_MEMCPY( rgbIVLwrrent, rgbIV, sizeof( rgbIVLwrrent ) ) );

                /* Handle all the complete stripes */
                for( cbRemaining = cbEncrypted; cbRemaining >= cbStripe; cbRemaining -= cbStripe )
                {
                    DRM_BYTE rgbIVTemp[ DRM_AES_BLOCKLEN ] = { 0 };

                    /*
                    ** In 'cbcs', each encrypted stripe continues the CBC chain across stripes
                    ** from the last block of one stripe into the first block of the next stripe,
                    ** i.e. the last block of *ciphertext* becomes the IV for the next block.
                    */
                    ChkVOID( OEM_TEE_MEMCPY( rgbIVTemp, &pbLwrrent[ cbEncryptedStripe - DRM_AES_BLOCKLEN ], DRM_AES_BLOCKLEN ) );

                    ChkDR( OEM_TEE_CRYPTO_AES128_CbcDecryptData( f_pContext, f_pCK, pbLwrrent, cbEncryptedStripe, rgbIVLwrrent ) );

                    ChkVOID( OEM_TEE_MEMCPY( rgbIVLwrrent, rgbIVTemp, DRM_AES_BLOCKLEN ) );
                    ChkDR( DRM_DWordPtrAddSame( (DRM_SIZE_T*)&pbLwrrent, cbStripe ) );
                }

                /* Handle a partial stripe, if any */
                if( cbRemaining > 0 )
                {
                    DRM_DWORD cbToDecrypt = DRM_MIN( cbRemaining, cbEncryptedStripe );

                    /*
                    ** Any final partial block is in the clear.  Don't attempt to decrypt it.
                    ** Note: Stripes are always sized in complete blocks, so we don't need to
                    ** do this subtraction in the "complete stripes" loop above.
                    */
                    cbToDecrypt -= ( cbToDecrypt & (DRM_DWORD)15 );

                    if( cbToDecrypt > 0 )
                    {
                        ChkDR( OEM_TEE_CRYPTO_AES128_CbcDecryptData( f_pContext, f_pCK, pbLwrrent, cbToDecrypt, rgbIVLwrrent ) );
                    }
                    ChkDR( DRM_DWordPtrAddSame( (DRM_SIZE_T*)&pbLwrrent, cbRemaining ) );

                    /* No need to update rgbIVLwrrent: it will reset at the start of the next encrypted region */
                }
            }
        }
    }

    if( f_pcbProcessed != NULL )
    {
        *f_pcbProcessed = cbTotalProcessed;
    }

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** This function is only used if the client supports playback of AES CBC encrypted content.
** Certain PlayReady clients may not need to implement this function.
** If you do not support handle-backed memory, this API should return DRM_E_NOTIMPL.
**
** Synopsis:
**
** This function in-place decrypts content into an opaque handle using AES128CBC.
**
** Operations Performed:
**
**  1. In-place decrypt the given content into an opaque handle using AES128CBC.
**
** Arguments:
**
** f_pContext:                      (in/out) The TEE context returned from
**                                           OEM_TEE_BASE_AllocTEEContext.
** f_ePolicyCallingAPI:             (in)     The calling API providing f_pPolicy.
** f_pPolicy:                       (in)     The current content protection policy.
** f_pCK:                           (in)     The content key used to decrypt the content.
** f_cEncryptedRegionMappings:      (in)     The number of encrypted-region mappings.
** f_pdwEncryptedRegionMappings:    (in)     The set of encrypted region mappings as pairs of
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
** f_ui64InitializatiolwectorHigh:  (in)     The high 8 bytes of the AES128CBC initialization vector.
** f_ui64InitializatiolwectorLow:   (in)     The low 8 bytes of the AES128CBC initialization vector.
**                                           Will be zero if an 8 byte IV is being used.
** f_cEncryptedRegionSkip:          (in)     Number of entries in f_pcEncryptedRegionSkip.
**                                           Will always be zero or two.
** f_pcEncryptedRegionSkip:         (in)     Ignored if f_cEncryptedRegionSkip is zero.
**                                           Otherwise, defines the striping of the encrypted
**                                           portions of the given encrpted region.
**                                           f_pcEncryptedRegionSkip[0] defines the number of
**                                           encrypted BLOCKS in each stripe.
**                                           f_pcEncryptedRegionSkip[1] defines the number of
**                                           clear BLOCKS in each stripe.
** f_cbEncrypted:                   (in)     The number of bytes in f_pbEncrypted.
** f_pbEncrypted:                   (in)     The bytes of content to decrypt.
**                                           Only the bytes starting at offset f_ibProcessing
**                                           through the toal number of bytes in
**                                           f_pdwEncryptedRegionMappings will be processed.
** f_hClear:                        (in/out) On input, a handle pre-allocated by
**                                           OEM_TEE_BASE_SelwreMemHandleAlloc with the number of bytes
**                                           in f_cbEncrypted.
**                                           On output, the portion of the bytes being processed
**                                           are decrypted (defined by f_pdwEncryptedRegionMappings,
**                                           f_ibProcessing, and f_pcEncryptedRegionSkip).
** f_ibProcessing:                  (in)     Offset into f_pbEncryptedToClear to decrypt.
**                                           f_pdwEncryptedRegionMappings determines
**                                           the number of bytes to decrypt.
** f_pcbProcessed:                  (out)    If non-NULL, outputs the number of bytes processed,
**                                           i.e. the sum of all entries in
**                                           f_pdwEncryptedRegionMappings.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_AES128CBC_DecryptContentIntoHandle(
    __inout                                          OEM_TEE_CONTEXT                 *f_pContext,
    __in                                             DRM_TEE_POLICY_INFO_CALLING_API  f_ePolicyCallingAPI,
    __in                                       const DRM_TEE_POLICY_INFO             *f_pPolicy,
    __in                                       const OEM_TEE_KEY                     *f_pCK,
    __in                                             DRM_DWORD                        f_cEncryptedRegionMappings,
    __in_ecount( f_cEncryptedRegionMappings )  const DRM_DWORD                       *f_pdwEncryptedRegionMappings,
    __in                                             DRM_UINT64                       f_ui64InitializatiolwectorHigh,
    __in                                             DRM_UINT64                       f_ui64InitializatiolwectorLow,
    __in                                             DRM_DWORD                        f_cEncryptedRegionSkip,
    __in_ecount_opt( f_cEncryptedRegionSkip )  const DRM_DWORD                       *f_pcEncryptedRegionSkip,
    __in                                             DRM_DWORD                        f_cbEncrypted,
    __in_bcount( f_cbEncrypted )               const DRM_BYTE                        *f_pbEncrypted,
    __inout                                          OEM_TEE_MEMORY_HANDLE            f_hClear,
    __in                                             DRM_DWORD                        f_ibProcessing,
    __out_opt                                        DRM_DWORD                       *f_pcbProcessed )
{
    DRM_RESULT  dr          = DRM_SUCCESS;
    DRM_BYTE   *pbClear     = NULL;
    DRM_DWORD   cbToProcess = 0;
    DRM_DWORD   cbProcessed = 0;
    DRM_DWORD   idx;        /* loop variable */

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );
    DRMASSERT( f_pCK != NULL );
    DRMASSERT( f_cEncryptedRegionMappings > 0 );
    DRMASSERT( ( f_cEncryptedRegionMappings & 1 ) == 0 );     /* Must be DWORD pairs.  Power of two: use & instead of % for better perf */
    DRMASSERT( f_pdwEncryptedRegionMappings != NULL );
    DRMASSERT( f_cbEncrypted > 0 );
    DRMASSERT( f_pbEncrypted != NULL );
    DRMASSERT( f_hClear != OEM_TEE_MEMORY_HANDLE_ILWALID );
    DRMASSERT( f_ibProcessing < f_cbEncrypted );
    DRMASSERT( f_pPolicy->eSymmetricEncryptionType == XMR_SYMMETRIC_ENCRYPTION_TYPE_AES_128_CBC );
    DRMASSERT( f_cEncryptedRegionSkip == 0 || f_cEncryptedRegionSkip == 2 );

    for( idx = 0; idx < f_cEncryptedRegionMappings; idx++ )
    {
        cbToProcess += f_pdwEncryptedRegionMappings[ idx ];
    }

    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( f_pContext, cbToProcess, (DRM_VOID**)&pbClear ) );
    ChkVOID( OEM_TEE_MEMCPY( pbClear, &f_pbEncrypted[ f_ibProcessing ], cbToProcess ) );

    ChkDR( OEM_TEE_AES128CBC_DecryptContentIntoClear(
        f_pContext,
        f_ePolicyCallingAPI,
        f_pPolicy,
        f_pCK,
        f_cEncryptedRegionMappings,
        f_pdwEncryptedRegionMappings,
        f_ui64InitializatiolwectorHigh,
        f_ui64InitializatiolwectorLow,
        f_cEncryptedRegionSkip,
        f_pcEncryptedRegionSkip,
        cbToProcess,
        pbClear,
        0,
        &cbProcessed ) );
    AssertChkArg( cbProcessed == cbToProcess );

    ChkDR( OEM_TEE_BASE_SelwreMemHandleWrite( f_pContext, f_ibProcessing, cbProcessed, pbClear, f_hClear ) );
    if( f_pcbProcessed != NULL )
    {
        *f_pcbProcessed = cbProcessed;
    }

ErrorExit:
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( f_pContext, (DRM_VOID**)&pbClear ) );
    return dr;
}
#endif
EXIT_PK_NAMESPACE_CODE;

PREFAST_POP; /* __WARNING_NONCONST_PARAM_25004 */

