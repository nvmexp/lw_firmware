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
#include <oemcommon.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;
#if defined (SEC_COMPILE)
/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST
** have a valid implementation for your PlayReady port.
**
** Any reimplementation MUST exactly match the behavior of the default PK implementation.
**
** This function is only used if the client supports Local Provisioning.
** Certain PlayReady clients, such as PRND Transmitters, may not need to implement this function.
**
** Synopsis:
**
** This function takes in data to sign using ECC256 ECDSA with SHA256 as the hashing algorithm.
**
** Operations Performed:
**
**  1. Sign the given data with the given private signing key.
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
** f_cbToSign:              (in)     The size of the data to be signed.
** f_pbToSign:              (in)     The data to be signed.
** f_pECCSigningKey:        (in)     The private ECC256 signing key.
** f_pSignature:            (out)    The signature over the given data with the given signing
**                                   key.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_LPROV_ECDSA_Sign(
    __inout_opt                         OEM_TEE_CONTEXT            *f_pContextAllowNULL,
    __in                                DRM_DWORD                   f_cbToSign,
    __in_bcount( f_cbToSign )     const DRM_BYTE                   *f_pbToSign,
    __in                          const OEM_TEE_KEY                *f_pECCSigningKey,
    __out                               SIGNATURE_P256             *f_pSignature )
{
    ASSERT_TEE_CONTEXT_ALLOW_NULL( f_pContextAllowNULL );
    return OEM_TEE_CRYPTO_ECC256_ECDSA_SHA256_Sign(
        f_pContextAllowNULL,
        f_pbToSign,
        f_cbToSign,
        f_pECCSigningKey,
        f_pSignature );
}
#endif

#if !defined (SEC_USE_CERT_TEMPLATE) && defined (SEC_COMPILE)
/*
** OEM_MANDATORY_CONDITIONALLY:
** You MUST replace this function implementation with your own implementation
** that is specific to your PlayReady port if your PlayReady port supports Local
** Provisioning (DRM_SUPPORT_LOCAL_PROVISIONING=1).
** You MUST provide model information for local provisioning per the PlayReady
** Compliance and Robustness Rules.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** Operations Performed:
**
**  1. Return the model information.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pcchModelInfo:      (in/out) On input, the size of the buffer specified.
**                                This may be zero if the size is being requested.
**                                On output, the size of the model info.
** f_pwchModelInfo:      (out)    The model info.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_PARAM_25004, "Prefast Noise: OEM_TEE_CONTEXT should not be const.");
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_LPROV_GetDeviceModelInfo(
    __inout                                              OEM_TEE_CONTEXT            *f_pContext,
    __inout                                              DRM_DWORD                  *f_pcchModelInfo,
    __out_ecount_opt( *f_pcchModelInfo )                 DRM_WCHAR                  *f_pwchModelInfo )
{
    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );
    return Oem_Device_GetModelInfo( NULL, f_pwchModelInfo, f_pcchModelInfo );
}
PREFAST_POP   /* __WARNING_NONCONST_PARAM_25004 */


/*
** OEM_MANDATORY_CONDITIONALLY:
** You MUST replace this function implementation with your own implementation that
** is specific to your PlayReady port if your PlayReady port supports Local
** Provisioning (DRM_SUPPORT_LOCAL_PROVISIONING=1).
** You MUST provide model information for local provisioning per the PlayReady
** Compliance and Robustness Rules.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** Operations Performed:
**
**  1. Return the security version and platform ID for the PlayReady TEE implementation.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pdwSelwrityVersion: (out)    The security version which uniquely defines
**                                the version of PlayReady TEE DRM implementation.
**                                The value OEM_TEE_LPROV_SELWRITY_VERSION_IGNORE
**                                can be used to ignore the security version check
**                                during local provisioning.
** f_pdwPlatformID:      (out)    The Platform ID with which the model
**                                certificate is associated.  Assigning the value
**                                OEM_TEE_LPROV_PLATFORM_ID_IGNORE to this parameter
**                                will indicate that the platform ID is not to
**                                be used for model certificate validation during
**                                local provisioning.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_PARAM_25004, "Prefast Noise: OEM_TEE_CONTEXT should not be const.");
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_LPROV_GetModelSelwrityVersion(
    __inout                                         OEM_TEE_CONTEXT            *f_pContext,
    __out                                           DRM_DWORD                  *f_pdwSelwrityVersion,
    __out                                           DRM_DWORD                  *f_pdwPlatformID )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );

    ChkArg( f_pdwSelwrityVersion != NULL );
    ChkArg( f_pdwPlatformID      != NULL );

    *f_pdwSelwrityVersion = OEM_TEE_LPROV_SELWRITY_VERSION_IGNORE;  /* Indicates the Security Version validation is ignored during local provisioning. */
    *f_pdwPlatformID      = OEM_TEE_LPROV_PLATFORM_ID_IGNORE;       /* Indicates the Platform ID validation is ignored during local provisioning. */

ErrorExit:
    return dr;
}
PREFAST_POP   /* __WARNING_NONCONST_PARAM_25004 */
#endif
EXIT_PK_NAMESPACE_CODE;

