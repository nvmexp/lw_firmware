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

#if !defined (SEC_USE_CERT_TEMPLATE) && defined (SEC_COMPILE)
PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_PARAM_25004, "Ignore Nonconst Param - f_pOemTeeContext and f_pBigContext are mutually exclusive and used separately in different implementations of the broker functions." );
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_DANGEROUS_POINTERCAST_25024,"Ignore Dangerous Pointercast for colwersion from PRIVKEY_P256* to OEM_TEE_KEY*." );

/*
** Synopsis:
**
** This function brokers a call to the TEE implementation of
** OEM_ECDSA_Sign_P256.
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
** f_pBigContext:             (in)     This parameter is required to be NULL.
** f_rgbMessage:              (in)     This is the message to verify.
** f_cbMessageLen:            (in)     The length of the message to verify.
** f_pPrivateKey:             (in)     The private key that will be used to sign f_rgbMessage.
** f_pSignature:              (out)    The buffer that will hold the signature.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Broker_ECDSA_P256_Sign(
    __inout_opt                                DRM_VOID               *f_pOemTeeContextAllowNULL,
    __inout_opt                                struct bigctx_t        *f_pBigContext,
    __in_bcount( f_cbMessageLen )        const DRM_BYTE                f_rgbMessage[],
    __in                                       DRM_DWORD               f_cbMessageLen,
    __in                                 const PRIVKEY_P256           *f_pPrivateKey,
    __out                                      SIGNATURE_P256         *f_pSignature )
{
    DRM_RESULT dr = DRM_SUCCESS;

    UNREFERENCED_PARAMETER( f_pBigContext );

    ChkArg( f_rgbMessage     != NULL );
    ChkArg( f_cbMessageLen    > 0    );
    ChkArg( f_pPrivateKey    != NULL );
    ChkArg( f_pSignature     != NULL );

    DRMASSERT( DRM_IS_DWORD_ALIGNED( f_pSignature ) );
    DRMASSERT( DRM_IS_DWORD_ALIGNED( f_pPrivateKey ) );

    /*
    ** NOTE!!! Lwrrently we only sign local provisioning certificates inside the TEE.
    ** If that changes, this TEE broker call will need to be updated.
    */
    ChkDR( OEM_TEE_LPROV_ECDSA_Sign(
        ( OEM_TEE_CONTEXT * )f_pOemTeeContextAllowNULL,
        f_cbMessageLen,
        f_rgbMessage,
        ( const OEM_TEE_KEY * )f_pPrivateKey,
        f_pSignature ) );

ErrorExit:
    return dr;
}

PREFAST_POP;
PREFAST_POP;
#endif

EXIT_PK_NAMESPACE_CODE;

