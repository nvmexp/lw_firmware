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

PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_BUFFER_PARAM_25033, "Out params can't be const." );
PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_PARAM_25004, "Prefast Noise: OEM_TEE_CONTEXT and out parameter should not be const." );

ENTER_PK_NAMESPACE_CODE;
#if !defined (SEC_USE_CERT_TEMPLATE) && defined (SEC_COMPILE)
/*
** OEM_MANDATORY_CONDITIONALLY:
** You MUST replace this function implementation with your own implementation
** that is specific to your PlayReady port if your PlayReady port
** supports Local Provisioning (DRM_SUPPORT_LOCAL_PROVISIONING=1) and your model
** certificate's private signing key is either stored in a protected format in
** the application layer and needs to be unprotected for use or if the private
** key exists only in the TEE and needs to be returned by this function.
**
** For example, you may simply fetch the model certificate private key
** directly from hardware and return it.  In this case, this function can
** completely ignore the other parameters.
**
** This function is only called if OEM_TEE_LPROV_UnprotectCertificate
** returned a model certificate.
**
** The model certificate private signing key must be protected
** per the PlayReady Compliance and Robustness Rules.
**
** This function is only used if the client supports Local Provisioning.
** Certain PlayReady clients, such as PRND Transmitters, may not need to include
** this function.
**
** Synopsis:
**
** This function either extracts the model private signing key from the provided
** protected key or ignores its input and returns the private signing key (if the
** key is directly accessible in the TEE).
**
** If this function returns DRM_E_NOTIMPL, it must do one of the following.
**
** 1. Populate the f_pcbOEMDefaultProtectedModelCertificatePrivateKey and
**    f_ppbOEMDefaultProtectedModelCertificatePrivateKey output parameters.
**    In this case, the caller will IGNORE the input private key from
**    user mode (if any) and will unprotect that output from this function
**    using the default mechanism.
**    It will then use that as the model certificate private signing key.
** 2. or, alternatively, NOT populate those output parameters.
**    In this case, the caller will REQUIRE an input private key from
**    user mode and will unprotect it using the default mechanism.
**    It will then use that as the model certificate private signing key.
**
** If this function returns DRM_SUCCESS, it must populate the
** f_pModelCertPrivateKey output parameter with the model certificate
** private signing key in unprotected form.
** In this case, the caller will IGNORE the input private key from
** user mode (if any).  It will pay attention only to f_pModelCertPrivateKey.
** It will then use that as the model certificate private signing key.
**
** The default mechanism unwraps the given protected blob with a key derived
** from the current TK for the purpose of persisting the key to disk.  It uses the
** AES 128 Codebook for the KEK.  It uses the AES Key Wrap Specification given in
** the following NIST document (published 16 November 2001).
**
** http://csrc.nist.gov/groups/ST/toolkit/dolwments/kms/key-wrap.pdf
**
**
** Operations Performed:
**
**  If the TEE has access to the model certificate private signing key, then:
**
**      If the model certificate private signing key is available to the TEE
**       in protected form using the default mechanim:
**
**          Then populate the the appropriate output parameters with
**           that protected key and return DRM_E_NOTIMPL to signal
**           to the caller that the output from this function should
**           be unprotected via the default mechanism.  See remarks.
**
**          Else return the unprotected key via the output paramaeter
**           f_pModelCertPrivateKey after unprotecting it (if necessary).
**
**  Else the model certificate private signing key MUST have been passed
**   to the TEE from user mode in a protected form.
**
**  If that protected form is OEM-specific:
**
**      Return the unprotected key via the output paramaeter
**       f_pModelCertPrivateKey after unprotecting it (if necessary).
**       See remarks.
**
**      Else the key MUST have been protected via the default
**       mechanism.  Return DRM_E_NOTIMPL to signal to the caller
**       that the protected key from user mode should be unprotected
**       via the default mechanism.  (This is the default implementation.)
**
**
** Arguments:
**
** f_pContext:                               (in/out) The TEE context returned from
**                                                    OEM_TEE_BASE_AllocTEEContext.
** f_cbOEMProtectedModelCertificatePrivateKey:   (in) The size of the protected model cert private
**                                                    signing key.
**                                                    This parameter will be zero if no
**                                                    key was passed from user mode.
** f_pbOEMProtectedModelCertificatePrivateKey:   (in) The protected model cert private
**                                                    signing key.  For a DRM_E_NOTIMPL
**                                                    implemenation the protected data
**                                                    must be wrapped using the algorithm
**                                                    defined in the synopsis.
**                                                    This parameter will be NULL if no
**                                                    key was passed from user mode.
** f_pModelCertPrivateKey:                   (in/out) The corresponding ECC256 model
**                                                    cert private signing key.
**                                                    The OEM_TEE_KEY must be pre-allocated
**                                                    by the caller using OEM_TEE_BASE_AllocKeyECC256
**                                                    and freed after use by the caller using
**                                                    OEM_TEE_BASE_FreeKeyECC256.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_LPROV_UnprotectModelCertificatePrivateSigningKey(
    __inout                                                                           OEM_TEE_CONTEXT   *f_pContext,
    __in                                                                              DRM_DWORD          f_cbOEMProtectedModelCertificatePrivateKey,
    __in_bcount( f_cbOEMProtectedModelCertificatePrivateKey )                   const DRM_BYTE          *f_pbOEMProtectedModelCertificatePrivateKey,
    __out                                                                             DRM_DWORD         *f_pcbOEMDefaultProtectedModelCertificatePrivateKey,
    __deref_out_bcount_opt( *f_pcbOEMDefaultProtectedModelCertificatePrivateKey )     DRM_BYTE         **f_ppbOEMDefaultProtectedModelCertificatePrivateKey,
    __inout                                                                           OEM_TEE_KEY       *f_pModelCertPrivateKey )
{
    UNREFERENCED_PARAMETER( f_pContext );
    UNREFERENCED_PARAMETER( f_cbOEMProtectedModelCertificatePrivateKey );
    UNREFERENCED_PARAMETER( f_pbOEMProtectedModelCertificatePrivateKey );
    UNREFERENCED_PARAMETER( f_pcbOEMDefaultProtectedModelCertificatePrivateKey );
    UNREFERENCED_PARAMETER( f_ppbOEMDefaultProtectedModelCertificatePrivateKey );
    UNREFERENCED_PARAMETER( f_pModelCertPrivateKey );
    return DRM_E_NOTIMPL;
}

/*
** OEM_MANDATORY_CONDITIONALLY:
** You MUST replace this function implementation with your own implementation
** that is specific to your PlayReady port if your PlayReady port
** supports Local Provisioning (DRM_SUPPORT_LOCAL_PROVISIONING=1) and your leaf
** certificate's private keys are either stored in a protected format in
** the application layer and need to be unprotected for use or if the private
** keys exists only in the TEE and need to be returned by this function.
**
** For example, you may simply fetch the leaf certificate private keys
** directly from hardware and return them.  In this case, this function can
** completely ignore the other parameters.
**
** This function is only called if OEM_TEE_LPROV_UnprotectCertificate
** returned a leaf certificate.
**
** The certificate's private keys must be protected
** per the PlayReady Compliance and Robustness Rules.
**
** This function is only used if the client supports Local Provisioning.
** Certain PlayReady clients, such as PRND Transmitters, may not need to include
** this function.
**
** Synopsis:
**
** This function either extracts the leaf private keys from the provided
** protected keys or ignores its input and returns the private keys (if the
** keys are directly accessible in the TEE).
**
** If this function returns DRM_E_NOTIMPL, it must do one of the following.
**
** 1. Populate the f_pcbOEMDefaultProtectedLeafCertificatePrivateKeys and
**    f_ppbOEMDefaultProtectedLeafCertificatePrivateKeys output parameters.
**    In this case, the caller will IGNORE the input private keys from
**    user mode (if any) and will unprotect that output from this function
**    using the default mechanism.
**    It will then use them as the certificate private keys.
** 2. or, alternatively, NOT populate those output parameters.
**    In this case, the caller will REQUIRE input private keys from
**    user mode and will unprotect them using the default mechanism.
**    It will then use them as the certificate private keys.
**
** If this function returns DRM_SUCCESS, it must populate the
** f_pLeafCertificatePrivate*Key parameters with the private keys in unprotected form.
** In this case, the caller will IGNORE the input keys from
** user mode (if any).  It will pay attention only to f_pLeafCertificatePrivate*Key.
** It will then use those output parameters as the certificate private keys.
**
** The default mechanism unwraps 2 or 3 keys from the given protected blob with a key
** derived from the current TK for the purpose of persisting the key to disk.  It uses
** the AES 128 Codebook for the KEK.  It uses the AES Key Wrap Specification given in
** the following NIST document (published 16 November 2001).
**
** http://csrc.nist.gov/groups/ST/toolkit/dolwments/kms/key-wrap.pdf
**
**
** Operations Performed:
**
**  If the TEE has access to the leaf certificate private keys, then:
**
**      If the leaf certificate private keys are available to the TEE
**       in protected form using the default mechanim:
**
**          Then populate the the appropriate output parameters with
**           that protected keys and return DRM_E_NOTIMPL to signal
**           to the caller that the output from this function should
**           be unprotected via the default mechanism.  See remarks.
**
**          Else return the unprotected keys via the output parameters
**           f_pLeafCertificatePrivate*Key after unprotecting them (if necessary).
**
**  Else the leaf certificate private keys MUST have been passed
**   to the TEE from user mode in a protected form.
**
**  If that protected form is OEM-specific:
**
**      Return the unprotected keys via the output parameters
**       f_pLeafCertificatePrivate*Key after unprotecting them (if necessary).
**       See remarks.
**
**      Else the keys MUST have been protected via the default
**       mechanism.  Return DRM_E_NOTIMPL to signal to the caller
**       that the protected keys from user mode should be unprotected
**       via the default mechanism.  (This is the default implementation.)
**
**
** Arguments:
**
** f_pContext:                                 (in/out) The TEE context returned from
**                                                      OEM_TEE_BASE_AllocTEEContext.
** f_cbOEMProtectedLeafCertificatePrivateKeys:     (in) The size of the protected leaf cert private
**                                                      keys.
**                                                      This parameter will be zero if no
**                                                      keys were passed from user mode.
** f_pbOEMProtectedLeafCertificatePrivateKeys:     (in) The protected leaf cert private
**                                                      keys.  For a DRM_E_NOTIMPL
**                                                      implemenation the protected data
**                                                      must be wrapped using the algorithm
**                                                      defined in the synopsis.
**                                                      This parameter will be NULL if no
**                                                      keys were passed from user mode.
** f_pLeafCertificatePrivateSigningKey:        (in/out) The corresponding ECC256 leaf
**                                                      certificate private signing key.
**                                                      The OEM_TEE_KEY must be pre-allocated
**                                                      by the caller using OEM_TEE_BASE_AllocKeyECC256
**                                                      and freed after use by the caller using
**                                                      OEM_TEE_BASE_FreeKeyECC256.
** f_pLeafCertificatePrivateEncryptionKey:     (in/out) The corresponding ECC256 leaf
**                                                      certificate private encryption key.
**                                                      The OEM_TEE_KEY must be pre-allocated
**                                                      by the caller using OEM_TEE_BASE_AllocKeyECC256
**                                                      and freed after use by the caller using
**                                                      OEM_TEE_BASE_FreeKeyECC256.
** f_pLeafCertificatePrivatePRNDEncryptionKey: (in/out) The corresponding ECC256 leaf
**                                                      certificate private PRND encryption key.
**                                                      Parameter will be NULL if the certificate does
**                                                      not support PRND.
**                                                      The OEM_TEE_KEY must be pre-allocated
**                                                      by the caller using OEM_TEE_BASE_AllocKeyECC256
**                                                      and freed after use by the caller using
**                                                      OEM_TEE_BASE_FreeKeyECC256.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_LPROV_UnprotectLeafCertificatePrivateKeys(
    __inout                                                                           OEM_TEE_CONTEXT   *f_pContext,
    __in                                                                              DRM_DWORD          f_cbOEMProtectedLeafCertificatePrivateKeys,
    __in_bcount( f_cbOEMProtectedLeafCertificatePrivateKeys )                   const DRM_BYTE          *f_pbOEMProtectedLeafCertificatePrivateKeys,
    __out                                                                             DRM_DWORD         *f_pcbOEMDefaultProtectedLeafCertificatePrivateKeys,
    __deref_out_bcount_opt( *f_pcbOEMDefaultProtectedLeafCertificatePrivateKeys )     DRM_BYTE         **f_ppbOEMDefaultProtectedLeafCertificatePrivateKeys,
    __inout                                                                           OEM_TEE_KEY       *f_pLeafCertificatePrivateSigningKey,
    __inout                                                                           OEM_TEE_KEY       *f_pLeafCertificatePrivateEncryptionKey,
    __inout_opt                                                                       OEM_TEE_KEY       *f_pLeafCertificatePrivatePRNDEncryptionKey )
{
    UNREFERENCED_PARAMETER( f_pContext );
    UNREFERENCED_PARAMETER( f_cbOEMProtectedLeafCertificatePrivateKeys );
    UNREFERENCED_PARAMETER( f_pbOEMProtectedLeafCertificatePrivateKeys );
    UNREFERENCED_PARAMETER( f_pcbOEMDefaultProtectedLeafCertificatePrivateKeys );
    UNREFERENCED_PARAMETER( f_ppbOEMDefaultProtectedLeafCertificatePrivateKeys );
    UNREFERENCED_PARAMETER( f_pLeafCertificatePrivateSigningKey );
    UNREFERENCED_PARAMETER( f_pLeafCertificatePrivateEncryptionKey );
    UNREFERENCED_PARAMETER( f_pLeafCertificatePrivatePRNDEncryptionKey );
    return DRM_E_NOTIMPL;
}

/*
** OEM_MANDATORY_CONDITIONALLY:
** You MUST replace this function implementation with your own implementation
** that returns FALSE if (and only if) your PlayReady port
** supports Local Provisioning (DRM_SUPPORT_LOCAL_PROVISIONING=1) and your
** device expects the certificate passed to and/or returned by
** OEM_TEE_LPROV_UnprotectCertificate to be a leaf certificate instead
** of a model certificate.  Otherwise, this function can be left as-is.
**
** Returns:
**
** TRUE if a model certificate is available on the client for local provisioning and
** a leaf certificate / private keys are generated upon first device initialization.
** FALSE if a leaf certificate is already available on the client upon
** first device initialization thus not requiring local certificate building.
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL OEM_TEE_LPROV_IsModelCertificateExpected( DRM_VOID )
{
    return TRUE;
}

/*
** OEM_MANDATORY_CONDITIONALLY:
** You MUST replace this function implementation with your own implementation
** that is specific to your PlayReady port if your PlayReady port
** supports Local Provisioning (DRM_SUPPORT_LOCAL_PROVISIONING=1) and your model
** or leaf certificate is either stored in a protected format in the application
** layer and needs to be unprotected for use or if the model or leaf certificate
** exists only in the TEE and needs be returned by this function.
**
** For example, you may simply fetch the model or leaf certificate
** directly from hardware and return it and completely ignore the
** f_pInputCertificate parameter.
**
** The model or leaf certificate does not need to be protected, but you may
** decide to protect it anyway at your discretion.  If the model or leaf
** certificate is given from user mode in unprotected form, this function can
** simply return DRM_E_NOTIMPL.  This is the default implementation.
** Otherwise, if it is given from user mode in protected form,
** this function should unprotect it and return it.
**
** This function is only used if the client supports Local Provisioning.
** Certain PlayReady clients, such as PRND Transmitters, may not need to include
** this function.
**
** Synopsis:
**
** This function returns the model or leaf certificate.
** The caller will parse the certificate to determine which type of certificate
** was returned and use that information to decide whether to call
** OEM_TEE_LPROV_UnprotectModelCertificatePrivateSigningKey or
** OEM_TEE_LPROV_UnprotectLeafCertificatePrivateKeys
**
** If this function returns DRM_E_NOTIMPL, the default implementation is used.
** The default implementation simply uses the input model or leaf certificate unchanged.
**
** Operations Performed:
**
**  1. Return the model or leaf certificate.
**
** Arguments:
**
** f_pContext:                         (in/out) The TEE context returned from
**                                              OEM_TEE_BASE_AllocTEEContext.
** f_cbInputCertificate:                   (in) The size of the input model or leaf certificate.
** f_pbInputCertificate:                   (in) The model or leaf certificate returned from
**                                              Oem_Device_GetCert in inselwre world.
** f_pcbInputCertificate:                 (out) The size of the model or leaf certificate.
** f_ppbCertificate:                      (out) The model or leaf certificate.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_BUFFER_PARAM_25033, "Out params can't be const.");
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_PARAM_25004, "Prefast Noise: OEM_TEE_CONTEXT should not be const.");
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_LPROV_UnprotectCertificate(
    __inout                                            OEM_TEE_CONTEXT         *f_pContext,
    __in                                               DRM_DWORD                f_cbInputCertificate,
    __in_bcount( f_cbInputCertificate )          const DRM_BYTE                *f_pbInputCertificate,
    __out                                              DRM_DWORD               *f_pcbCertificate,
    __deref_out_bcount( *f_pcbCertificate )            DRM_BYTE               **f_ppbCertificate )
{
    UNREFERENCED_PARAMETER( f_pContext );
    UNREFERENCED_PARAMETER( f_cbInputCertificate );
    UNREFERENCED_PARAMETER( f_pbInputCertificate );
    UNREFERENCED_PARAMETER( f_pcbCertificate );
    UNREFERENCED_PARAMETER( f_ppbCertificate );
    return DRM_E_NOTIMPL;
}
#endif
EXIT_PK_NAMESPACE_CODE;

PREFAST_POP;  /* __WARNING_NONCONST_PARAM_25004 */
PREFAST_POP;  /* __WARNING_NONCONST_BUFFER_PARAM_25033 */

