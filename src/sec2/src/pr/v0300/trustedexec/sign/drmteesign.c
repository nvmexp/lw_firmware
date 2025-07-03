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
#include <drmerr.h>
#include <oemtee.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_TEE_SIGN_IMPL_IsSIGNSupported( DRM_VOID )
{
    return TRUE;
}

/*
** Synopsis:
**
** This function signs a hash of a message using the device private signing key
**
** This function is called inside:
**  Drm_LicenseAcq_GenerateChallenge
**  Drm_LicenseAcq_GenerateAck
**  Drm_JoinDomain_GenerateChallenge
**  Drm_LeaveDomain_GenerateChallenge
**  Drm_Metering_GenerateChallenge
**  Drm_Prnd_Receiver_RegistrationRequest_Generate
**
** Operations Performed:
**
**  1. Verify the signature on the given DRM_TEE_CONTEXT.
**  2. Parse the given PlayReady Private Key Blob (PPKB)
**     and verify its signature.
**  3  Verify that the size of the hash
**  4. Sign the given data with the PlayReady Private Signing Key
**  5. Return the signature.
**
** Arguments:
**
** f_pContext:     (in/out) The TEE context returned from
**                          DRM_TEE_BASE_AllocTEEContext.
** f_pPPKB         (in)     The current PlayReady Private Key Blob (PPKB).
** f_pDataToSign:  (in)     The hash data which is going to be signed.
** f_pSignature:   (out)    The signature over the Signed Info.
**                          This should be freed with DRM_TEE_BASE_FreeBlob.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_SIGN_SignHash(
    __inout               DRM_TEE_CONTEXT              *f_pContext,
    __in            const DRM_TEE_BYTE_BLOB            *f_pPPKB,
    __in            const DRM_TEE_BYTE_BLOB            *f_pDataToSign,
    __out                 DRM_TEE_BYTE_BLOB            *f_pSignature )
{
    DRM_RESULT          dr                  = DRM_SUCCESS;
    DRM_RESULT          drTmp               = DRM_SUCCESS;
    DRM_TEE_BYTE_BLOB   oSignatureBlob      = DRM_TEE_BYTE_BLOB_EMPTY;
    SIGNATURE_P256      oSignature          = {{0}};  // LWE (nkuo) - changed due to compile error "missing braces around initializer"
    DRM_DWORD           cKeys               = 0;
    DRM_TEE_KEY        *pKeys               = NULL;
    DRM_TEE_KEY_TYPE    eType               = DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_SIGN;
    DRM_TEE_KEY        *pSigningKeyWeakRef  = NULL;

    /* Arguments are checked in the PRITEE proxy layer */
    DRMASSERT( f_pContext    != NULL );
    DRMASSERT( f_pPPKB       != NULL );
    DRMASSERT( f_pDataToSign != NULL );
    DRMASSERT( f_pSignature  != NULL );
    DRMASSERT( sizeof(DRM_SHA256_Digest) == ECC_P256_INTEGER_SIZE_IN_BYTES );

    /* Ensure that the Secure clock did not go out of sync */
    if( OEM_TEE_CLOCK_SelwreClockNeedsReSync( &f_pContext->oContext ) )
    {
        ChkDR( DRM_E_TEE_CLOCK_DRIFTED );
    }

    ChkBOOL( f_pDataToSign->cb == sizeof(DRM_SHA256_Digest), DRM_E_XMLSIG_SHA_HASH_SIZE );

    ChkDR( DRM_TEE_KB_ParseAndVerifyPPKB( f_pContext, f_pPPKB, &cKeys, &pKeys, NULL ) );

    ChkVOID( OEM_TEE_ZERO_MEMORY( f_pSignature, sizeof(*f_pSignature) ) );

    pSigningKeyWeakRef = DRM_TEE_BASE_IMPL_LocateKeyInPPKBWeakRef( &eType, cKeys, pKeys );

    ChkDR( OEM_TEE_BASE_SignHashWithDeviceSigningKey(
        &f_pContext->oContext,
        &pSigningKeyWeakRef->oKey,
        f_pDataToSign->pb,
        &oSignature ) );

    ChkDR( DRM_TEE_BASE_AllocBlob( f_pContext, DRM_TEE_BLOB_ALLOC_BEHAVIOR_COPY, sizeof(oSignature), (const DRM_BYTE*)&oSignature, &oSignatureBlob ) );

    ChkVOID( DRM_TEE_BASE_TransferBlobOwnership( f_pSignature, &oSignatureBlob ) );

ErrorExit:

    //
    // LWE (kwilson) - Can't use ChkDR within ErrorExit. Cache the return value in drTmp and check value
    // of dr, before overwriting to be sure we are not overwriting a previous failure with success.
    //
    drTmp = DRM_TEE_BASE_FreeBlob( f_pContext, &oSignatureBlob );
    if (dr == DRM_SUCCESS)
    {
        dr = drTmp;
    }

    ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, cKeys, &pKeys ) );
    return dr;
}
#endif

EXIT_PK_NAMESPACE_CODE;

