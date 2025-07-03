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

#if defined(SEC_COMPILE)
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
** This function signs the given data with an AES key using AES OMAC1.
**
** Operations Performed:
**
**  1. AES OMAC1 sign the provided data.
**
** Arguments:
**
** f_pContextAllowNULL:     (in/out) The TEE context returned from
**                                   OEM_TEE_BASE_AllocTEEContext.
**                                   This function may receive NULL.
**                                   This function may receive an
**                                   OEM_TEE_CONTEXT where
**                                   cbUnprotectedOEMData == 0 and
**                                   pbUnprotectedOEMData == NULL.
** f_pKey:                      (in) AES Key to sign the data.
** f_pbData:                    (in) Buffer holding the data to sign.
** f_ibData:                    (in) Offset into buffer where the data to sign resides.
** f_cbData:                    (in) Number of bytes to sign.
** f_rgbTag:                   (out) Signature.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_OMAC1_Sign(
PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_PARAM_25004, "OEM_TEE_CONTEXT should never be const." )
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pKey,
    __in_bcount( f_ibData + f_cbData )              const DRM_BYTE                     *f_pbData,
    __in                                                  DRM_DWORD                     f_ibData,
    __in                                                  DRM_DWORD                     f_cbData,
    __out_bcount( DRM_AES_BLOCKLEN )                      DRM_BYTE                      f_rgbTag[ DRM_AES_BLOCKLEN ] )
{
    return Oem_Omac1_Sign( OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_BROKER( f_pKey ), f_pbData, f_ibData, f_cbData, f_rgbTag );
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
** This function verifies the given data with an AES key using AES OMAC1.
**
** Operations Performed:
**
**  1. AES OMAC1 verify the provided data.
**
** Arguments:
**
** f_pContextAllowNULL:     (in/out) The TEE context returned from
**                                   OEM_TEE_BASE_AllocTEEContext.
**                                   This function may receive NULL.
**                                   This function may receive an
**                                   OEM_TEE_CONTEXT where
**                                   cbUnprotectedOEMData == 0 and
**                                   pbUnprotectedOEMData == NULL.
** f_pKey:                      (in) AES Key to verify the data.
** f_pbData:                    (in) Buffer holding the data to verify.
** f_ibData:                    (in) Offset into buffer where the signed data resides.
** f_cbData:                    (in) Number of bytes to verify.
** f_pbSignature:               (in) Buffer holding the signature.
** f_ibSignature:               (in) Offset into buffer where the signature resides.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_OMAC1_Verify(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pKey,
    __in_bcount( f_ibData + f_cbData )              const DRM_BYTE                     *f_pbData,
    __in                                                  DRM_DWORD                     f_ibData,
    __in                                                  DRM_DWORD                     f_cbData,
    __in_bcount( f_ibSignature + DRM_AES_BLOCKLEN ) const DRM_BYTE                     *f_pbSignature,
    __in                                                  DRM_DWORD                     f_ibSignature )
{
    return Oem_Omac1_Verify( OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_BROKER( f_pKey ), f_pbData, f_ibData, f_cbData, f_pbSignature, f_ibSignature );
}
#endif
EXIT_PK_NAMESPACE_CODE;

