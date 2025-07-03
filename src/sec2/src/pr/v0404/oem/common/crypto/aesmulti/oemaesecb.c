/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#define DRM_BUILDING_OEMAESMULTI_C
#include <drmtypes.h>
#include <drmmathsafe.h>
#include <oembyteorder.h>
#include <oem.h>
#include <oemaesmulti.h>
#include <oembroker.h>
#include <drmprofile.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

#if defined (SEC_COMPILE)
/*********************************************************************************************
**
** Function:  Oem_Aes_EcbEncryptData
**
** Synopsis:  Does AES ECB-Mode encryption on a buffer of data
**
** Arguments: [f_pKey]      : The AES secret key used to encrypt  buffer
**                            - If this API is called inside the TEE. Then OEM_AES_KEY_CONTEXT will
**                              be pointing to an OEM_TEE_KEY_AES.
**                              OEM can colwert it to a DRM_AES_KEY using
**                              OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_AES_KEY.
**                            - If this API is called from the normal world, then OEM_AES_KEY_CONTEXT
**                              will be pointing to a DRM_AES_KEY. A simple cast to a DRM_AES_KEY*
**                              will be sufficient.
**            [f_pbData]    : The buffer to encrypt ( in place )
**            [f_cbData]    : The number of bytes to encrypt
**
** Returns:   DRM_SUCCESS
**               Success
**            DRM_E_ILWALIDARG
**               One of the pointers was NULL, or the byte count is 0
**               or not a multiple of DRM_AES_BLOCKLEN
**********************************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Aes_EcbEncryptData(
    __in_ecount( 1 )                const OEM_AES_KEY_CONTEXT   *f_pKey,
    __inout_bcount( f_cbData )            DRM_BYTE              *f_pbData,
    __in                                  DRM_DWORD              f_cbData )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT dr     = DRM_SUCCESS;
    DRM_DWORD  ibData = 0;

    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRMAES, PERF_FUNC_Oem_Aes_EcbEncryptData );

    ChkArg( f_pbData != NULL );
    ChkArg( f_cbData != 0 );
    ChkArg( f_pKey   != NULL );

    ChkArg( f_cbData % DRM_AES_BLOCKLEN == 0 );

    for ( ; ibData < f_cbData; ibData += DRM_AES_BLOCKLEN )
    {
        ChkDR( Oem_Broker_Aes_EncryptOneBlock( f_pKey, &( f_pbData[ibData] ) ) );
    }

ErrorExit:
    DRM_PROFILING_LEAVE_SCOPE;
    return dr;
}

/*********************************************************************************************
**
** Function:  Oem_Aes_EcbDecryptData
**
** Synopsis:  Does AES ECB-Mode decryption on a buffer of data
**
** Arguments: [f_pKey]      : The AES secret key used to decrypt buffer
**                            - If this API is called inside the TEE. Then OEM_AES_KEY_CONTEXT will
**                              be pointing to an OEM_TEE_KEY_AES.
**                              OEM can colwert it to a DRM_AES_KEY using
**                              OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_AES_KEY.
**                            - If this API is called from the normal world, then OEM_AES_KEY_CONTEXT
**                              will be pointing to a DRM_AES_KEY. A simple cast to a DRM_AES_KEY*
**                              will be sufficient.
**            [f_pbData]    : The buffer to decrypt ( in place )
**            [f_cbData]    : The number of bytes to decrypt
**
** Returns:   DRM_SUCCESS
**               Success
**            DRM_E_ILWALIDARG
**               One of the pointers was NULL, or the byte count is 0
**               or not a multiple of DRM_AES_BLOCKLEN
**********************************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Aes_EcbDecryptData(
    __in_ecount( 1 )                const OEM_AES_KEY_CONTEXT   *f_pKey,
    __inout_bcount( f_cbData )            DRM_BYTE              *f_pbData,
    __in                                  DRM_DWORD              f_cbData )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT dr     = DRM_SUCCESS;
    DRM_DWORD  ibData = 0;

    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRMAES, PERF_FUNC_Oem_Aes_EcbDecryptData );

    ChkArg( f_pbData != NULL );
    ChkArg( f_cbData != 0 );
    ChkArg( f_pKey   != NULL );

    ChkArg( f_cbData % DRM_AES_BLOCKLEN == 0 );

    for ( ; ibData < f_cbData; ibData += DRM_AES_BLOCKLEN )
    {
        ChkDR( Oem_Broker_Aes_DecryptOneBlock( f_pKey, &( f_pbData[ibData] ) ) );
    }

ErrorExit:
    DRM_PROFILING_LEAVE_SCOPE;
    return dr;
}
#endif

EXIT_PK_NAMESPACE_CODE;

