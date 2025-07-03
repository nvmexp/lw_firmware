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
#include <oemaesmulti.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST
** have a valid implementation for your PlayReady port.
**
** This function is only used if the client supports Domains.
**
** Synopsis:
**
** This function decrypts the domain private key using the domain session keys.
**
** Operations Performed:
**
**  1. AES ECB Decrypt the given encrypted domain key in two parts using the given session keys.
**
** Arguments:
**
** f_pContext:              (in/out) The TEE context returned from
**                                   OEM_TEE_BASE_AllocTEEContext.
** f_pSessionKey1:          (in)     First Session Key.
** f_pSessionKey2:          (in)     Second Session Key.
** f_pbEncryptedDomainKey:  (in)     Encrypted domain private key.
** f_pDomainPrivKey:        (in/out) Domain private key.
**                                   The OEM_TEE_KEY must be pre-allocated
**                                   by the caller using OEM_TEE_BASE_AllocKeyECC256
**                                   and freed after use by the caller using
**                                   OEM_TEE_BASE_FreeKeyECC256.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_DOM_DecryptDomainKeyWithSessionKeys(
    __inout                                             OEM_TEE_CONTEXT       *f_pContext,
    __in                                          const OEM_TEE_KEY           *f_pSessionKey1,
    __in                                          const OEM_TEE_KEY           *f_pSessionKey2,
    __in_bcount( ECC_P256_PRIVKEY_SIZE_IN_BYTES ) const DRM_BYTE              *f_pbEncryptedDomainKey,
    __inout                                             OEM_TEE_KEY           *f_pDomainPrivKey )
{
    DRM_RESULT             dr                          = DRM_SUCCESS;
    DRM_BYTE              *pbDomainPrivKeyTempBuffer   = NULL;

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );

    ChkArg( f_pContext             != NULL );
    ChkArg( f_pDomainPrivKey       != NULL );
    ChkArg( f_pSessionKey1         != NULL );
    ChkArg( f_pSessionKey2         != NULL );
    ChkArg( f_pbEncryptedDomainKey != NULL );

    /* Allocate a temporary buffer to hold the encrypted domain key */
    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( f_pContext, ECC_P256_PRIVKEY_SIZE_IN_BYTES, (DRM_VOID**)&pbDomainPrivKeyTempBuffer ) );
    OEM_TEE_MEMCPY( (DRM_VOID*)pbDomainPrivKeyTempBuffer, (DRM_VOID*)f_pbEncryptedDomainKey, ECC_P256_PRIVKEY_SIZE_IN_BYTES );

    /* Decrypt the first part of the domain private key with Session key1 */
    ChkDR( OEM_TEE_CRYPTO_AES128_EcbDecryptData(
        f_pContext,
        f_pSessionKey1,
        pbDomainPrivKeyTempBuffer,
        DRM_AES_KEYSIZE_128 ) );

    /* Decrypt the Second part of the domain private key with Session key2 */
    ChkDR( OEM_TEE_CRYPTO_AES128_EcbDecryptData(
        f_pContext,
        f_pSessionKey2,
        &pbDomainPrivKeyTempBuffer[DRM_AES_KEYSIZE_128],
        DRM_AES_KEYSIZE_128 ) );

    ChkDR ( OEM_TEE_CRYPTO_ECC256_SetKey( f_pContext, f_pDomainPrivKey, pbDomainPrivKeyTempBuffer ) );

ErrorExit:
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( f_pContext, (DRM_VOID**)&pbDomainPrivKeyTempBuffer ) );
    return dr;
}

EXIT_PK_NAMESPACE_CODE;

