/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <drmerr.h>
#include <oemtee.h>
#include <oembroker.h>
#include <drmteecache.h>

ENTER_PK_NAMESPACE_CODE;

PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_PARAM_25004, "Ignore Nonconst Param - f_pOemTeeContext and f_pBigContext are mutually exclusive and used separately in different implementations of the broker functions." );
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_DANGEROUS_POINTERCAST_25024,"Ignore Dangerous Pointercast for colwersion from PRIVKEY_P256* to OEM_TEE_KEY*." );

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
    DRM_SHA256_CONTEXT oShaCtx;
    DRM_SHA256_Digest  oHash        = {{ 0 }};  // LWE (nkuo) - changed due to compile error "missing braces around initializer"
    DRM_BOOL           fFound       = FALSE;   

    UNREFERENCED_PARAMETER( f_pBigContext );

    DRMASSERT( DRM_IS_DWORD_ALIGNED( f_pSignature ) );
    DRMASSERT( DRM_IS_DWORD_ALIGNED( f_pPubkey ) );

    // LWE (nkuo) - Initialize this parameter here instead at declaration time to avoid compile error "missing braces around initializer"
    ChkVOID( OEM_TEE_ZERO_MEMORY( &oShaCtx, sizeof(DRM_SHA256_CONTEXT) ) );

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

    // LWE (kwilson) - Due to critsec,  must check return type of CheckHash
    ChkDR( DRM_TEE_CACHE_CheckHash( &oHash, &fFound ) );
    if ( !fFound )
    {
        ChkDR( OEM_TEE_BASE_ECDSA_P256_Verify(
            DRM_REINTERPRET_CAST( OEM_TEE_CONTEXT, f_pOemTeeContextAllowNULL ),
            f_cbMessageLen,
            f_rgbMessage,
            f_pPubkey,
            f_pSignature ) );
        ChkDR( DRM_TEE_CACHE_AddHash( &oHash ) );
    }

ErrorExit:
    return dr;
}

/*
** Synopsis:
**
** This function brokers a call to the TEE implementation of
** Oem_Random_GetBytes.
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
** f_pOEMContext:             (in)     This parameter is requried to be null
** f_pbData:                  (out)    Buffer to hold the random bytes
** f_cbData:                  (in)     Count of bytes to generate and fill in pbData
*/
DRM_API DRM_RESULT DRM_CALL Oem_Broker_Random_GetBytes(
    __inout_opt                                DRM_VOID               *f_pOemTeeContextAllowNULL,
    __in_opt                                   DRM_VOID               *f_pOEMContext,
    __out_bcount(f_cbData)                     DRM_BYTE               *f_pbData,
    __in                                       DRM_DWORD               f_cbData )
{
    DRM_RESULT dr = DRM_SUCCESS;

    UNREFERENCED_PARAMETER( f_pOEMContext );

    ChkDR( OEM_TEE_BASE_GenerateRandomBytes( DRM_REINTERPRET_CAST( OEM_TEE_CONTEXT, f_pOemTeeContextAllowNULL ), f_cbData, f_pbData ) );

ErrorExit:
    return dr;
}
#endif

#ifdef NONE
/*
** Synopsis:
**
** This function brokers a call to the TEE implementation of
** OEM_TEE_BASE_CRITSEC_Initialize.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Broker_CRITSEC_Initialize( DRM_VOID )
{
    return OEM_TEE_BASE_CRITSEC_Initialize();
}

/*
** Synopsis:
**
** This function brokers a call to the TEE implementation of
** OEM_TEE_BASE_CRITSEC_Uninitialize.
*/
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL Oem_Broker_CRITSEC_Uninitialize( DRM_VOID )
{
    OEM_TEE_BASE_CRITSEC_Uninitialize();
}

/*
** Synopsis:
**
** This function brokers a call to the TEE implementation of
** OEM_TEE_BASE_CRITSEC_Enter.
**
** LWE (kwilson) Change the return type to DRM_RESULT because we can fail to acquire
** the mutex
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Broker_CRITSEC_Enter(DRM_VOID)
{
    return(OEM_TEE_BASE_CRITSEC_Enter());
}

/*
** Synopsis:
**
** This function brokers a call to the TEE implementation of
** OEM_TEE_BASE_CRITSEC_Leave.
*/
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL Oem_Broker_CRITSEC_Leave( DRM_VOID )
{
    OEM_TEE_BASE_CRITSEC_Leave();
}
#endif

/*
** Synopsis:
**
** This function brokers a call to the TEE implementation of
** the heap memory allocation function.
**
** Arguments:
**
** f_cbSize:    (in)    The amount of memory to allocate.
** f_ppv:       (out)   The allocated memory.
**                      This will be freed by Oem_Broker_MemFree.
*/
#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Broker_MemAlloc(
    __in                                     DRM_DWORD                 f_cbSize,
    __out                                    DRM_VOID                **f_ppv )
{
    return OEM_TEE_BASE_SelwreMemAlloc( NULL, f_cbSize, f_ppv );
}

/*
** Synopsis:
**
** This function brokers a call to the TEE implementation of
** the heap memory free function.
**
** Arguments:
**
** f_ppv:   (in/out)    On input, *f_ppv is the memory to free.
**                      On output, *f_ppv is NULL.
*/
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL Oem_Broker_MemFree(
    __inout                                  DRM_VOID                **f_ppv )
{
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( NULL, f_ppv ) );
}

/*
** Synopsis:
**
** This function brokers a call to the TEE implementation of
** the Oem_Clock_GetSystemTimeAsFileTime function.
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
** f_pFileTime:               (out)    Pointer to DRMFILETIME structure to get the time
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Broker_Clock_GetSystemTimeAsFileTime(
    __inout_opt                                DRM_VOID               *f_pOemTeeContextAllowNULL,
    __out                                      DRMFILETIME            *f_pFileTime )
{
    return OEM_TEE_CLOCK_GetSystemTime( DRM_REINTERPRET_CAST( OEM_TEE_CONTEXT, f_pOemTeeContextAllowNULL ), f_pFileTime );
}

/*
** Synopsis:
**
** This function brokers a call to the TEE implementation of the SHA256
** functions to create a digest from the provided data.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Broker_SHA256_CreateDigest(
    __inout_opt                                     DRM_VOID                   *f_pOemTeeContextAllowNULL,
    __in                                            DRM_DWORD                   f_cbBuffer,
    __in_ecount( f_cbBuffer )                 const DRM_BYTE                    f_rgbBuffer[],    
    __out                                           DRM_SHA256_Digest          *f_pDigest )
{
    DRM_RESULT         dr           = DRM_SUCCESS;
    DRM_SHA256_CONTEXT oSHAContext; /* Initialized by DRM_SHA256_Init */
    OEM_TEE_CONTEXT   *pOemTeeCtx   = DRM_REINTERPRET_CAST( OEM_TEE_CONTEXT, f_pOemTeeContextAllowNULL );

    ChkDR( OEM_TEE_BASE_SHA256_Init    ( pOemTeeCtx, &oSHAContext ) );
    ChkDR( OEM_TEE_BASE_SHA256_Update  ( pOemTeeCtx, &oSHAContext, f_cbBuffer, f_rgbBuffer ) );
    ChkDR( OEM_TEE_BASE_SHA256_Finalize( pOemTeeCtx, &oSHAContext, f_pDigest ) );

ErrorExit:
    return dr;
}
#endif

PREFAST_POP;
PREFAST_POP;

EXIT_PK_NAMESPACE_CODE;

