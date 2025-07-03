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
#include <drmresults.h>
#include <drmerr.h>
#include <oemtee.h>
#include <oemteelprovcerttemplate.h>
#include <oemteecrypto.h>
#include <oemteecryptointernaltypes.h>
#include <drmlastinclude.h>
#include "sec2_hs.h"
#include "pr/pr_lassahs.h"
#include "pr/pr_lassahs_hs.h"
#include "mem_hs.h"
#include "lwosselwreovly.h"

ENTER_PK_NAMESPACE_CODE;
#if defined (SEC_USE_CERT_TEMPLATE) && defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_TEE_LPROV_IMPL_IsLPROVSupported( DRM_VOID )
{
    return TRUE;
}

PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_USING_FAILED_RESULT_6102, "Ignore Using Failed Result warning since out parameters are all initialized." );

static const DRM_ID g_idKeyDerivationCertificatePrivateKeysWrap PR_ATTR_DATA_OVLY(_g_idKeyDerivationCertificatePrivateKeysWrap) = {{ 0x9c, 0xe9, 0x34, 0x32, 0xc7, 0xd7, 0x40, 0x16, 0xba, 0x68, 0x47, 0x63, 0xf8, 0x01, 0xe1, 0x36 }};

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
    static const DRM_TEE_KEY_TYPE    rgeKeyTypes[] PR_ATTR_DATA_OVLY(_rgeKeyTypes) = { DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_SIGN, DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_ENCRYPT, DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_PRND };

    AssertChkArg( f_cECCKeys <= DRM_NO_OF( rgeKeyTypes ) );

    DRMASSERT( f_pContext != NULL );
    DRMASSERT( f_pECCKeys != NULL );
    DRMASSERT( f_pPubKeys != NULL );

    pOemTeeCtx = &f_pContext->oContext;

    for( iKey = 0; iKey < f_cECCKeys; iKey++ )
    {
        ChkDR( DRM_TEE_BASE_IMPL_AllocKeyECC256( pOemTeeCtx, rgeKeyTypes[ iKey ], &f_pECCKeys[ iKey ] ) );
        ChkDR( OEM_TEE_PROV_GenerateRandomECC256KeyPair( pOemTeeCtx, &f_pECCKeys[iKey].oKey, &f_pPubKeys[iKey] ) );
    }

ErrorExit:
    /* f_pECCKeys is caller-allocated and the caller will clean them up on failure */
    return dr;
}

/*
** Update the certificate digest with a SHA256 hashing the public key.
*/
static DRM_RESULT DRM_CALL _GenerateCertHash(
    __inout_opt                                 OEM_TEE_CONTEXT   *f_pOemTeeContext,
    __in                                        DRM_DWORD          f_cbData,
    __in_bcount( f_cbData )               const DRM_BYTE          *f_pbData,
    __inout_bcount( sizeof(DRM_SHA256_Digest) ) DRM_BYTE          *f_pDigest )
{
    DRM_RESULT         dr           = DRM_SUCCESS;
    DRM_SHA256_CONTEXT SHAContext;  /* Initialized by OEM_TEE_BASE_SHA256_Init     */
    DRM_SHA256_Digest  oDigest;     /* Initialized by OEM_TEE_BASE_SHA256_Finalize */

    ChkDR( OEM_TEE_BASE_SHA256_Init    ( f_pOemTeeContext, &SHAContext                     ) );
    ChkDR( OEM_TEE_BASE_SHA256_Update  ( f_pOemTeeContext, &SHAContext, f_cbData, f_pbData ) );
    ChkDR( OEM_TEE_BASE_SHA256_Finalize( f_pOemTeeContext, &SHAContext, &oDigest           ) );

    /* We cannot use f_pDigest in OEM_TEE_BASE_SHA256_Finalize because it may not be aligned */
    ChkVOID( OEM_TEE_MEMCPY( f_pDigest, &oDigest, sizeof(oDigest) ) );

ErrorExit:
    return dr;
}

static DRM_RESULT DRM_CALL _SignCertTemplate(
    __inout_opt                                 OEM_TEE_CONTEXT   *f_pOemTeeContext,
    __in_tee_opt                          const DRM_TEE_BYTE_BLOB *f_pOEMProtectedModelCertPrivateKey,
    __in                                        DRM_DWORD          f_cbDataToSign,
    __in_bcount( f_cbDataToSign )         const DRM_BYTE          *f_pbDataToSign,
    __inout_bcount( sizeof(SIGNATURE_P256) )    DRM_BYTE          *f_pSignature )
{
    DRM_RESULT     dr         = DRM_SUCCESS;
    SIGNATURE_P256 oSignature = {{0}};  // LWE (nkuo) - changed due to compile error "missing braces around initializer"
    OEM_TEE_KEY    oPrivKey   = {0};

    /* Allocate and unprotect model cert private key. */
    ChkDR( OEM_TEE_BASE_AllocKeyECC256( f_pOemTeeContext, &oPrivKey ) );

    ChkDR( OEM_TEE_LPROV_GetTemplateModelCertificatePrivateSigningKey(
        f_pOemTeeContext,
        &oPrivKey ) );

    ChkDR( OEM_TEE_LPROV_ECDSA_Sign(
        f_pOemTeeContext,
        f_cbDataToSign,
        f_pbDataToSign,
        &oPrivKey,
        &oSignature ) );

    /* We cannot use f_pSignature in OEM_TEE_LPROV_ECDSA_Sign because it may not be aligned */
    ChkVOID( OEM_TEE_MEMCPY( f_pSignature, &oSignature, sizeof(oSignature) ) );

ErrorExit:
    ChkVOID( OEM_TEE_BASE_FreeKeyECC256( f_pOemTeeContext, &oPrivKey ) );
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
**  3. Generate two new ECC keypairs for signing and encryption.
**  4. If the given model certificate chain supports being a PRND receiver,
**     generate a new ECC keypair for PRND encryption.
**  5. If a previous PPKB was not given, generate a new Secure Store (SST) AES key.
**  6. Generate a new PlayReady Device Certificate based on data inside the
**     inside the given model certificate chain, and data
**     provided by OEM functions.  Include the newly generated public keys for
**     signing, encryption, and (optionally) PRND encryption.
**  7. Append the new certificate to the given model certificate chain.
**  8. Via an OEM API, sign the new PlayReady Device Certificate using the
**     data provided in the given protected model private signing key.
**     Note that this data is treated as opaque by non-OEM code, and this
**     data may be ignored by the OEM function if the OEM signing function
**     already has access to the model private signing key.  This key
**     must be protected per the PlayReady Compliance and Robustness Rules.
**  9. Build and sign a PPKB.  Include the SST key and all Previous TEE Keys
**     (PTKs) from the previous PPKB (if specified), the current most-recent
**     Previous TEE Key (PTK) (if any), and all newly generated keys inside this PPKB.
** 10. Return the new PPKB and (now complete) PlayReady Device Certificate Chain.
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
** f_pPrevPPKB:                         (in)     The previous PlayReady Private Key Blob (PPKB),
**                                               if any.
** f_pModelCertificate:                 (in)     The model (group) certificate including
**                                               the complete chain by which it is signed.
**                                               Caller will get this from Oem_Device_GetCert.
** f_pOEMProtectedModelCertPrivateKey:  (in)     The model (group) certificate private key
**                                               returned from
**                                               Oem_Device_GetOEMProtectedModelPrivateSigningKey.
**                                               This parameter may be ignored if the TEE
**                                               natively has access to the private key.
**                                               This key must be protected per the
**                                               PlayReady Compliance and Robustness Rules.
** f_pidApplication:                    (in)     An application id to be used as part of the HWID.
** f_pPPKB:                             (out)    The new PlayReady Private Key Blob (PPKB).
**                                               This should be freed with DRM_TEE_BASE_FreeBlob.
** f_pNewCertificate:                   (out)    The new PlayReady client certificate (complete chain).
**                                               This should be freed with DRM_TEE_BASE_FreeBlob.
*/
extern PR_LASSAHS_DATA g_prLassahsData;
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_LPROV_GenerateDeviceKeys(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pPrevPPKB,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pModelCertificate,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pOEMProtectedModelCertPrivateKey,
    __in                  const DRM_ID                       *f_pidApplication,
    __out                       DRM_TEE_BYTE_BLOB            *f_pPPKB,
    __out                       DRM_TEE_BYTE_BLOB            *f_pNewCertificate )
{
    DRM_RESULT                         dr                  = DRM_SUCCESS;
    DRM_RESULT                         drTmp               = DRM_SUCCESS;
    DRM_DWORD                          cKeys               = 0;
    DRM_TEE_KEY                       *pKeys               = NULL;
    PUBKEY_P256                       *pPubKeys            = NULL;
    DRM_TEE_LPROV_CERT_INDEXES         oCertIndexes        = {{0}};  // LWE (nkuo) - changed due to compile error "missing braces around initializer"
    DRM_TEE_BYTE_BLOB                  oDevCertTemplate    = DRM_TEE_BYTE_BLOB_EMPTY;
    DRM_BYTE                          *pbCertTemp          = NULL;
    DRM_DWORD                          cbCertTemp          = 0;
    OEM_TEE_CONTEXT                   *pOemTeeCtx          = NULL;

    /* Arguments are checked in the PRITEE proxy layer */
    DRMASSERT( f_pContext                         != NULL );
    DRMASSERT( f_pPrevPPKB                        != NULL );
    DRMASSERT( f_pModelCertificate                != NULL );
    DRMASSERT( f_pOEMProtectedModelCertPrivateKey != NULL );
    DRMASSERT( f_pPPKB                            != NULL );
    DRMASSERT( f_pNewCertificate                  != NULL );

    g_prLassahsData.bActive = LW_TRUE;

    //
    // LWE (kwilson) - Preentry to GDK:  Nothing special needs to be done for
    // this code.  We can depend on autoloading to load the correct overlays,
    // as these are not signed.
    //
    ChkFlcnStatusToDr(prPreAllocMemory(PR_LASSAHS_PREALLOC_MEMORY_SIZE), DRM_E_OEM_PREENTRY_GDK_OUT_OF_MEMORY);

    //
    // LWE (kwilson) - The pre memory alloc is just to verify we have enough memory
    // for secure GDK.  It has to be freed before entering secure GDK.
    //
    prFreePreAllocMemory();

    // LWE (kwilson) - Load overlays (IMEM and DMEM).
    prLoadLassahsImemOverlays();
    prLoadLassahsDmemOverlays();

    //
    // LWE (kwilson) - HS Entry
    // 1. Enter wrapper critical section for LASSAHS
    //    (This is required because we ilwalidate imem blocks behind RTOS's back.
    //     RTOS does not deal well if a context switch interrupt happens during LASSAHS.)
    // 2. OEM_TEE_HsCommonEntryWrapper will handle the following:
    //    a. Setup entry into HS mode
    //    b. Call the LASSAHS Entry function
    //       (Please refer to the function description of prSbHsEntry for
    //        detailed steps.)
    //    c. Cleanup after returning from HS mode
    //
    lwrtosENTER_CRITICAL();
    {
        ChkDR(OEM_TEE_HsCommonEntryWrapper(PR_SB_HS_ENTRY, OVL_INDEX_ILWALID, NULL));

        pOemTeeCtx = &f_pContext->oContext;

        DRMASSERT( IsBlobConsistent( f_pModelCertificate ) );
        DRMASSERT( IsBlobConsistent( f_pPrevPPKB         ) );

        ChkVOID( OEM_TEE_ZERO_MEMORY( f_pPPKB, sizeof(*f_pPPKB) ) );
        ChkVOID( OEM_TEE_ZERO_MEMORY( f_pNewCertificate, sizeof(*f_pNewCertificate) ) );

        ChkDR( OEM_TEE_LPROV_GetDeviceCertTemplate( pOemTeeCtx, &cbCertTemp, &pbCertTemp, &oCertIndexes ) );
        ChkDR( DRM_TEE_BASE_AllocBlob( f_pContext, DRM_TEE_BLOB_ALLOC_BEHAVIOR_WEAKREF, cbCertTemp, pbCertTemp, &oDevCertTemplate ) );

        /* Make sure the certificate size is greater than the largest field we will update.  This prevents underflows from checks later. */
        AssertChkArg( oDevCertTemplate.cb > sizeof(SIGNATURE_P256) );
        cKeys = (DRM_DWORD)(oCertIndexes.rgibPubKeys[DRM_TEE_LPROV_PUBKEY_PRND_ENCRYPT_IDX] != 0 ? 3 : 2);

        ChkDR( DRM_TEE_BASE_IMPL_AllocateKeyArray( f_pContext, cKeys, &pKeys ) );

        /* No overflow check needed because cKeys is either 2 or 3 */
        ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, cKeys * sizeof(PUBKEY_P256), (DRM_VOID**)&pPubKeys ) );
        ChkDR( _GenerateNewECCKeys( f_pContext, cKeys, pKeys, pPubKeys ) );

        /* Generate a unique Identifier for this certificate. */
        AssertChkArg( oCertIndexes.ibCertID <= (oDevCertTemplate.cb - sizeof(DRM_ID)) );
        ChkDR( OEM_TEE_BASE_GenerateRandomBytes( pOemTeeCtx, sizeof(DRM_ID), (DRM_BYTE *)&oDevCertTemplate.pb[oCertIndexes.ibCertID] ) );
        {
            DRM_ID  oCertClientID;  /* Initialized later, gets copied into pBuilderCtx */

            ChkDR( DRM_TEE_BASE_IMPL_GetAppSpecificHWID( pOemTeeCtx, f_pidApplication, &oCertClientID ) );
            AssertChkArg( oCertIndexes.ibCertClientID <= (oDevCertTemplate.cb - sizeof(oCertClientID)) );
            ChkVOID( OEM_TEE_MEMCPY_IDX( oDevCertTemplate.pb, oCertIndexes.ibCertClientID, &oCertClientID, 0, sizeof(oCertClientID) ) );
        }

        AssertChkArg( oCertIndexes.rgibPubKeys[DRM_TEE_LPROV_PUBKEY_SIGN_IDX] > 0
            && oCertIndexes.rgibPubKeys[DRM_TEE_LPROV_PUBKEY_SIGN_IDX] <= (oDevCertTemplate.cb - sizeof(*pPubKeys)) );
        ChkVOID( OEM_TEE_MEMCPY_IDX( oDevCertTemplate.pb, oCertIndexes.rgibPubKeys[DRM_TEE_LPROV_PUBKEY_SIGN_IDX]   , &pPubKeys[0], 0, sizeof(*pPubKeys) ) );

        AssertChkArg( oCertIndexes.rgibPubKeys[DRM_TEE_LPROV_PUBKEY_ENCRYPT_IDX] > 0
            && oCertIndexes.rgibPubKeys[DRM_TEE_LPROV_PUBKEY_ENCRYPT_IDX] <= (oDevCertTemplate.cb - sizeof(*pPubKeys)) );
        ChkVOID( OEM_TEE_MEMCPY_IDX( oDevCertTemplate.pb, oCertIndexes.rgibPubKeys[DRM_TEE_LPROV_PUBKEY_ENCRYPT_IDX], &pPubKeys[1], 0, sizeof(*pPubKeys) ) );

        if( cKeys == 3 )
        {
            AssertChkArg( oCertIndexes.rgibPubKeys[DRM_TEE_LPROV_PUBKEY_ENCRYPT_IDX] <= (oDevCertTemplate.cb - sizeof(*pPubKeys)) );
            ChkVOID( OEM_TEE_MEMCPY_IDX( oDevCertTemplate.pb, oCertIndexes.rgibPubKeys[DRM_TEE_LPROV_PUBKEY_PRND_ENCRYPT_IDX], &pPubKeys[2], 0, sizeof(*pPubKeys) ) );
        }

        /*
        ** Update the certificate digest with a SHA256 hashing the public key.
        */
        AssertChkArg( oCertIndexes.ibCertDigest <= (oDevCertTemplate.cb - sizeof(DRM_SHA256_Digest)) );
        ChkDR( _GenerateCertHash(
            pOemTeeCtx,
            sizeof(PUBKEY_P256),
            &oDevCertTemplate.pb[ oCertIndexes.rgibPubKeys[DRM_TEE_LPROV_PUBKEY_SIGN_IDX] ],
            ( DRM_BYTE * )&oDevCertTemplate.pb[ oCertIndexes.ibCertDigest ] ) );

        /* Validate the indexes are not out of bounds in reference to the device cert chain template. */
        AssertChkArg( oCertIndexes.ibSignData <= oDevCertTemplate.cb );
        AssertChkArg( oCertIndexes.cbSignData <= (oDevCertTemplate.cb - sizeof(SIGNATURE_P256)) );
        AssertChkArg( oCertIndexes.ibSignature <= (oDevCertTemplate.cb - sizeof(SIGNATURE_P256)) );
        ChkDR( _SignCertTemplate(
            pOemTeeCtx,
            f_pOEMProtectedModelCertPrivateKey,
            oCertIndexes.cbSignData,
            &oDevCertTemplate.pb[oCertIndexes.ibSignData],
            ( DRM_BYTE * )&oDevCertTemplate.pb[oCertIndexes.ibSignature] ) );

    /* Uncomment the following line to verify the local provisioning cert chain. */
    /* #define _TEST_LPROV_CERT_GEN_ 1 */
    #if _TEST_LPROV_CERT_GEN_
        ChkDR( DRM_BCERTFORMAT_VerifyCertificateChain(
            pOemTeeCtx,
            NULL,
            NULL,
            DRM_BCERT_CERTTYPE_DEVICE,
            0,
            oDevCertTemplate.cb,
            oDevCertTemplate.pb ) );
    #endif

        ChkDR( DRM_TEE_KB_FinalizePPKB( f_pContext, cKeys, pKeys, f_pPrevPPKB, f_pPPKB ) );
        ChkVOID( DRM_TEE_BASE_TransferBlobOwnership( f_pNewCertificate, &oDevCertTemplate ) );

    ErrorExit:
        ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, cKeys, &pKeys ) );
        ChkVOID( OEM_TEE_BASE_SelwreMemFree(  pOemTeeCtx, ( DRM_VOID ** )&pPubKeys ) );

        //
        // LWE (kwilson) - Can't use ChkDR within ErrorExit. Cache the return value in drTmp and check value
        // of dr, before overwriting to be sure we are not overwriting a previous failure with success.
        //
        drTmp = DRM_TEE_BASE_FreeBlob( f_pContext, &oDevCertTemplate );
        if (dr == DRM_SUCCESS)
        {
            dr = drTmp;
        }

        ChkVOID( OEM_TEE_LPROV_FreeDeviceCertTemplate( pOemTeeCtx, &pbCertTemp ) );

        //
        // LWE (kwilson) - HS Exit
        // 1. OEM_TEE_HsCommonEntryWrapper will handle the following:
        //    a. Setup entry into HS mode
        //    b. Call the LASSAHS Exit function
        //       (Please refer to the function description of prSbHsExit for
        //        detailed steps.)
        //    c. Cleanup after returning from HS mode
        // 2. Exit wrapper critical section for LASSAHS
        //

        // LWE (kwilson) Can't use SEC2 status macro within "ErrorExit"
        drTmp = OEM_TEE_HsCommonEntryWrapper(PR_SB_HS_EXIT, OVL_INDEX_ILWALID, NULL);
        if (dr == DRM_SUCCESS)
        {
            dr = drTmp;
        }
    }
    lwrtosEXIT_CRITICAL();

    //
    // Need to revalidate and unload imem blocks ilwalidated during LASSAHS
    // This is required because we have ilwalidated imem blocks behind RTOS during LASSAHS
    //
    if (FLCN_OK != prRevalidateImemBlocks())
    {
        if (dr == DRM_SUCCESS)
        {
            dr = DRM_E_OEM_GDK_IMEM_BLOCK_REVALIDATION_FAILED;
        }
    }

    prUnloadOverlaysIlwalidatedDuringLassahs();

    g_prLassahsData.bActive = LW_FALSE;

    return dr;
}

PREFAST_POP;
#endif
EXIT_PK_NAMESPACE_CODE;
