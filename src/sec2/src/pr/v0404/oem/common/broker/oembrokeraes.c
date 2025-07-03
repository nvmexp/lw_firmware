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
#include <oemaes.h>
#include <oemteecrypto.h>
#include <oemteecryptointernaltypes.h>
#include <aes128.h>

ENTER_PK_NAMESPACE_CODE;

#if DRM_COMPILE_FOR_NORMAL_WORLD
#ifdef NONE
/*
** Synopsis:
**
** This function brokers a call to the normal world implementation of
** Oem_Aes_EncryptOne.
**
** Arguments:
**
** f_pKey:       (in) Key used for encryption.
**                    For normal world implementation, the OEM_AES_KEY_CONTEXT is a pointer to an DRM_AES_KEY.
** f_rgbData:    (in) Data to encrypt in place.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Broker_Aes_EncryptOneBlock(
    __in_ecount( 1 )                   const OEM_AES_KEY_CONTEXT    *f_pKey,
    __inout_bcount( DRM_AES_BLOCKLEN )       DRM_BYTE                f_rgbData[ DRM_AES_BLOCKLEN ] )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkDR( Oem_Aes_EncryptOne( ( const DRM_AES_KEY *)f_pKey, f_rgbData ) );

ErrorExit:
    return dr;
}

/*
** Synopsis:
**
** This function brokers a call to the normal world implementation of
** Oem_Aes_DecryptOne.
**
** Arguments:
**
** f_pKey:       (in) Key used for decryption.
**                    For normal world implementation, the OEM_AES_KEY_CONTEXT is a pointer to an DRM_AES_KEY.
** f_rgbData:    (in) Data to decrypt in place.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Broker_Aes_DecryptOneBlock(
    __in_ecount( 1 )                   const OEM_AES_KEY_CONTEXT    *f_pKey,
    __inout_bcount( DRM_AES_BLOCKLEN )       DRM_BYTE                f_rgbData[ DRM_AES_BLOCKLEN ] )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkDR( Oem_Aes_DecryptOne( ( const DRM_AES_KEY *)f_pKey, f_rgbData ) );

ErrorExit:
    return dr;
}
#endif
#elif DRM_COMPILE_FOR_SELWRE_WORLD  /* DRM_COMPILE_FOR_NORMAL_WORLD */
#if defined(SEC_COMPILE)
/*
** Synopsis:
**
** This function brokers a call to the TEE implementation of
** OEM_TEE_CRYPTO_AES128_EncryptOneBlock.
**
** Arguments:
**
** f_pKey:       (in) Key used for encryption.
**                    For TEE implementation, the OEM_AES_KEY_CONTEXT is a pointer to an OEM_TEE_KEY.
** f_rgbData:    (in) Data to encrypt in place.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Broker_Aes_EncryptOneBlock(
    __in_ecount( 1 )                   const OEM_AES_KEY_CONTEXT    *f_pKey,
    __inout_bcount( DRM_AES_BLOCKLEN )       DRM_BYTE                f_rgbData[ DRM_AES_BLOCKLEN ] )
{
    DRM_RESULT   dr   = DRM_SUCCESS;

    // LWE (nkuo) - encrypt the data by using the SW secret in f_pKey
    DRM_DWORD    fCryptoMode = DRF_DEF(_PR_AES128, _OPERATION_LS, _ENC, _YES);

   ChkDR( OEM_TEE_CRYPTO_AES128_EncryptOneBlock( (const OEM_TEE_KEY*)f_pKey, f_rgbData, fCryptoMode ) );

ErrorExit:
    return dr;
}

/*
** Synopsis:
**
** This function brokers a call to the TEE implementation of
** OEM_TEE_CRYPTO_AES128_DecryptOneBlock.
**
** Arguments:
**
** f_pKey:       (in) Key used for decryption.
**                    For TEE implementation, the OEM_AES_KEY_CONTEXT is a pointer to an OEM_TEE_KEY.
** f_rgbData:    (in) Data to decrypt in place.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Broker_Aes_DecryptOneBlock(
    __in_ecount( 1 )                   const OEM_AES_KEY_CONTEXT    *f_pKey,
    __inout_bcount( DRM_AES_BLOCKLEN )       DRM_BYTE                f_rgbData[ DRM_AES_BLOCKLEN ] )
{
    DRM_RESULT   dr   = DRM_SUCCESS;

    // LWE (nkuo) - decrypt the data by using the SW secret in f_pKey
    DRM_DWORD    fCryptoMode = DRF_DEF(_PR_AES128, _OPERATION_LS, _ENC, _NO);

    ChkDR( OEM_TEE_CRYPTO_AES128_DecryptOneBlock( (const OEM_TEE_KEY*)f_pKey, f_rgbData, fCryptoMode ) );

ErrorExit:
    return dr;
}
#endif
#else

#error Both DRM_COMPILE_FOR_NORMAL_WORLD and DRM_COMPILE_FOR_SELWRE_WORLD are set to 0.

#endif /* DRM_COMPILE_FOR_SELWRE_WORLD || DRM_COMPILE_FOR_NORMAL_WORLD */

EXIT_PK_NAMESPACE_CODE;

