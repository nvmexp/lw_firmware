/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <drmerr.h>
#include <oembroker.h>
#include <oemcommon.h>
#include <oemtee.h>
#include <oemteecrypto.h>
#include <drmteecache.h>

ENTER_PK_NAMESPACE_CODE;

PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_PARAM_25004, "Ignore Nonconst Param - f_pOemTeeContext and f_pBigContext are mutually exclusive and used separately in different implementations of the broker functions." );

#if DRM_COMPILE_FOR_NORMAL_WORLD
#ifdef NONE
/*
** Synopsis:
**
** This function brokers a call to the normal world implementation of
** OEM_ECDSA_Verify_P256.
**
** Arguments:
**
** f_pOemTeeContext:       (in) This parameter should be NULL for normal world invocation.
** f_pBigContext:          (in) Optional. This is the crypto context used in normal world.
** f_rgbMessage:           (in) This is the message to verify.
** f_cbMessageLen:         (in) The length of the message to verify.
** f_pPubkey:              (in) The public key to use to verify the message.
** f_pSignature:           (in) The signature of the message being validated.
*/
DRM_API DRM_RESULT DRM_CALL Oem_Broker_ECDSA_P256_Verify(
    __inout_opt                                DRM_VOID               *f_pOemTeeContext,
    __inout_opt                                struct bigctx_t        *f_pBigContext,
    __in_ecount( f_cbMessageLen )       const  DRM_BYTE                f_rgbMessage[],
    __in                                const  DRM_DWORD               f_cbMessageLen,
    __in                                const  PUBKEY_P256            *f_pPubkey,
    __in                                const  SIGNATURE_P256         *f_pSignature )
{
    DRM_RESULT       dr          = DRM_SUCCESS;
    struct bigctx_t *pBigContext = NULL;

    UNREFERENCED_PARAMETER( f_pOemTeeContext );

    DRMASSERT( DRM_IS_DWORD_ALIGNED( f_pPubkey    ) );
    DRMASSERT( DRM_IS_DWORD_ALIGNED( f_pSignature ) );

    if( f_pBigContext == NULL )
    {
        ChkMem( pBigContext = ( struct bigctx_t * )Oem_MemAlloc( sizeof(DRM_CRYPTO_CONTEXT) ) );
        ChkVOID( OEM_SELWRE_ZERO_MEMORY( pBigContext, sizeof( DRM_CRYPTO_CONTEXT ) ) );
        f_pBigContext = pBigContext;
    }

    ChkDR( OEM_ECDSA_Verify_P256(
        f_rgbMessage,
        f_cbMessageLen,
        f_pPubkey,
        f_pSignature,
        f_pBigContext,
        sizeof(DRM_CRYPTO_CONTEXT) ) );

ErrorExit:
    ChkVOID( SAFE_OEM_FREE( pBigContext ) );
    return dr;
}
#endif
#elif DRM_COMPILE_FOR_SELWRE_WORLD  /* DRM_COMPILE_FOR_NORMAL_WORLD */
#if defined (SEC_COMPILE)
/*
** Synopsis:
**
** This function brokers a call to the TEE implementation of
** OEM_ECDSA_Verify_P256.
**
** Arguments:
**
** f_pOemTeeContextAllowNULL: (in/out) The TEE context returned from
**                                     OEM_TEE_BASE_AllocTEEContext.
**                                     This function may receive NULL.
**                                     This function may receive an
**                                     OEM_TEE_CONTEXT where
**                                     cbUnprotectedOEMData == 0 and
**                                     pbUnprotectedOEMData == NULL.
** f_rgbMessage:              (in)     This is the message to verify.
** f_cbMessageLen:            (in)     The length of the message to verify.
** f_pPubkey:                 (in)     The public key to use to verify the message.
** f_pSignature:              (in)     The signature of the message being validated.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Broker_ECDSA_P256_Verify(
    __inout_opt                                DRM_VOID               *f_pOemTeeContextAllowNULL,
    __inout_opt                                struct bigctx_t        *f_pBigContext,
    __in_ecount( f_cbMessageLen )       const  DRM_BYTE                f_rgbMessage[],
    __in                                const  DRM_DWORD               f_cbMessageLen,
    __in                                const  PUBKEY_P256            *f_pPubkey,
    __in                                const  SIGNATURE_P256         *f_pSignature )
{
    DRM_RESULT         dr           = DRM_SUCCESS;
    OEM_SHA256_DIGEST  oHash        = {{ 0 }};  // LWE (nkuo) - changed due to compile error "missing braces around initializer"
    DRM_BOOL           fHash        = FALSE;
    OEM_SHA256_CONTEXT oShaCtx;

    ChkVOID( OEM_SELWRE_ZERO_MEMORY( &oShaCtx, sizeof( oShaCtx ) ) );
    UNREFERENCED_PARAMETER( f_pBigContext );

    DRMASSERT( DRM_IS_DWORD_ALIGNED( f_pSignature ) );
    DRMASSERT( DRM_IS_DWORD_ALIGNED( f_pPubkey ) );

    /*
    ** It is secure to cache this hash because it guarantees we did this exact same operation previously.
    ** The types of data we ECDSA verify inside the TEE include:
    ** 1. Certificates (CRL signing, sample protection, Device)
    ** 2. CRLs.
    ** 3. Rev Infos.
    */
    ChkDR( OEM_TEE_BASE_SHA256_Init( DRM_REINTERPRET_CAST( OEM_TEE_CONTEXT, f_pOemTeeContextAllowNULL ), &oShaCtx ) );
    ChkDR( OEM_TEE_BASE_SHA256_Update( DRM_REINTERPRET_CAST( OEM_TEE_CONTEXT, f_pOemTeeContextAllowNULL ), &oShaCtx, f_cbMessageLen, f_rgbMessage ) );
    ChkDR( OEM_TEE_BASE_SHA256_Update( DRM_REINTERPRET_CAST( OEM_TEE_CONTEXT, f_pOemTeeContextAllowNULL ), &oShaCtx, sizeof( *f_pPubkey ), DRM_REINTERPRET_CONST_CAST( const DRM_BYTE, f_pPubkey ) ) );
    ChkDR( OEM_TEE_BASE_SHA256_Update( DRM_REINTERPRET_CAST( OEM_TEE_CONTEXT, f_pOemTeeContextAllowNULL ), &oShaCtx, sizeof( *f_pSignature ), DRM_REINTERPRET_CONST_CAST( const DRM_BYTE, f_pSignature ) ) );
    ChkDR( OEM_TEE_BASE_SHA256_Finalize( DRM_REINTERPRET_CAST( OEM_TEE_CONTEXT, f_pOemTeeContextAllowNULL ), &oShaCtx, &oHash ) );

    ChkDR( DRM_TEE_IMPL_CACHE_CheckHash( &oHash, &fHash ) );
    if( !fHash )
    {
        ChkDR( OEM_TEE_BASE_ECDSA_P256_Verify(
            DRM_REINTERPRET_CAST( OEM_TEE_CONTEXT, f_pOemTeeContextAllowNULL ),
            f_cbMessageLen,
            f_rgbMessage,
            f_pPubkey,
            f_pSignature ) );
        ChkDR( DRM_TEE_IMPL_CACHE_AddHash( &oHash ) );
    }

ErrorExit:
    return dr;
}
#endif

#else

#error Both DRM_COMPILE_FOR_NORMAL_WORLD and DRM_COMPILE_FOR_SELWRE_WORLD are set to 0.

#endif /* DRM_COMPILE_FOR_SELWRE_WORLD || DRM_COMPILE_FOR_NORMAL_WORLD */

PREFAST_POP;

EXIT_PK_NAMESPACE_CODE;
