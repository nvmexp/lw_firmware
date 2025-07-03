/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <oemtee.h>
#include <oemteecrypto.h>
#include <oemteecryptointernaltypes.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;
#if defined (SEC_COMPILE)
/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST
** have a valid implementation for your PlayReady port if this function is still called
** by other OEM_TEE functions once your port is complete.
**
** Any reimplementation MUST exactly match the behavior of the default PK implementation.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function encrypts the given data with an AES key using AES ECB.
**
** Operations Performed:
**
**  1. AES ECB encrypt the data using the provided Key.
**
** Arguments:
**
** f_pContext:              (in/out) The TEE context returned from
**                                   OEM_TEE_BASE_AllocTEEContext.
** f_pKey:                      (in) AES Key to encrypt the data.
** f_pbData:                (in/out) Buffer holding the clear data and receive the encrypted data.
** f_cbData:                    (in) Length of the data to encrypt.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_EcbEncryptData(
PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_PARAM_25004, "OEM_TEE_CONTEXT should never be const." )
    __inout                                               OEM_TEE_CONTEXT              *f_pContext,
PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pKey,
    __inout_bcount( f_cbData )                            DRM_BYTE                     *f_pbData,
    __in                                                  DRM_DWORD                     f_cbData )
{
    return Oem_Aes_EcbEncryptData( OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_BROKER( f_pKey ), f_pbData, f_cbData );
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST
** have a valid implementation for your PlayReady port if this function is still called
** by other OEM_TEE functions once your port is complete.
**
** Any reimplementation MUST exactly match the behavior of the default PK implementation.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function decrypts the given data with an AES key using AES ECB.
**
** Operations Performed:
**
**  1. AES ECB decrypt the data using the provided Key.
**
** Arguments:
**
** f_pContext:              (in/out) The TEE context returned from
**                                   OEM_TEE_BASE_AllocTEEContext.
** f_pKey:                      (in) AES Key to decrypt the data.
** f_pbData:                (in/out) Buffer holding the encrypted data and receive the decrypted data.
** f_cbData:                    (in) Length of the data to decrypt.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_EcbDecryptData(
PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_PARAM_25004, "OEM_TEE_CONTEXT should never be const." )
    __inout                                               OEM_TEE_CONTEXT              *f_pContext,
PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pKey,
    __inout_bcount( f_cbData )                            DRM_BYTE                     *f_pbData,
    __in                                                  DRM_DWORD                     f_cbData )
{
    return Oem_Aes_EcbDecryptData( OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_BROKER( f_pKey ), f_pbData, f_cbData );
}
#endif
EXIT_PK_NAMESPACE_CODE;

