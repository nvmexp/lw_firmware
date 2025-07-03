/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <oemtee.h>
#include <oemteelprovcerttemplate.h>
#include <oemteecryptointernaltypes.h>
#include <oemteeinternal.h>
#include <drmlastinclude.h>

/*
 * LW specific includes and extern variables
 */
#include "sec2sw.h"
#include "sec2_objpr.h"
#include "lwosselwreovly.h"
#include "config/g_pr_hal.h"
#include "sec2_hs.h"
#include "pr/pr_mpk.h"
#include "pr/pr_lassahs_hs.h"

ENTER_PK_NAMESPACE_CODE;

#if defined (SEC_USE_CERT_TEMPLATE) && defined (SEC_COMPILE)

/*
** OEM_MANDATORY_CONDITIONALLY:
** You MUST replace this function implementation with your own implementation
** that is specific to your PlayReady port if your PlayReady port if the device
** supports Local Provisioning (DRM_SUPPORT_LOCAL_PROVISIONING=2) and you use
** a device certificate template in the TEE. Under these cirlwmstances, this
** function MUST return the device certificate chain template.
**
** This function is only used if the client supports Local Provisioning with
** a devicer certificate template.
**
** Synopsis:
**
** This function returns the device certificate chain template, and the appropriate
** indexes within the template for the various fields that need to be updated during
** local provisioning. These fields include:
**      Index of each Public Key
**      Index of the Certificate Client ID
**      Index of the Certificate ID
**      Index of the Certificate Digest
**      Index of the Start of the Signed Portion of the Certificate
**      Size of the Signed Portion of the Certificate
**      Index of the Certificate's Signature
**
** Operations Performed:
**
**  1. Return the device certificate chain template.
**  2. Return the indexes into the cert template of fields required to be updated during
**     local provisioning.
**
** Arguments:
**
** f_pContext:                         (in/out) The OEM TEE context returned from
**                                              OEM_TEE_BASE_AllocTEEContext.
** f_pcbDeviceCertTemplate:            (out)    The size (in bytes) of the device certificate chain
**                                              template.
** f_ppbDeviceCertTemplate:            (out)    On success, a pointer to the device certificate
**                                              template.  This must be freed by calling
**                                              OEM_TEE_LPROV_FreeDeviceCertTemplate.
** f_pDeviceCertIndexes:               (out)    The set field indexes into f_pbDeviceCertTemplate
**                                              required by the local provisioning mechanism to
**                                              create a valid device certificate chain.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_LPROV_GetDeviceCertTemplate(
    __inout                                         OEM_TEE_CONTEXT             *f_pContext,
    __out                                           DRM_DWORD                   *f_pcbDeviceCertTemplate,
    __out_bcount( *f_pcbDeviceCertTemplate )        DRM_BYTE                   **f_ppbDeviceCertTemplate,
    __out                                           DRM_TEE_LPROV_CERT_INDEXES  *f_pDeviceCertIndexes )
{
    DRM_RESULT dr = DRM_SUCCESS;

    UNREFERENCED_PARAMETER( f_pContext );
    ChkArg( f_pcbDeviceCertTemplate != NULL );
    ChkArg( f_ppbDeviceCertTemplate != NULL );
    ChkArg( f_pDeviceCertIndexes    != NULL );

    // LWE (nkuo) - retrieve the cert template according to the board configuration
    prGetDeviceCertTemplate(f_ppbDeviceCertTemplate, f_pcbDeviceCertTemplate, (DRM_VOID *)f_pDeviceCertIndexes);

ErrorExit:
    return dr;
}

/*
** OEM_MANDATORY_CONDITIONALLY:
** You MUST replace this function implementation with your own implementation
** that is specific to your PlayReady port if your PlayReady port if the device
** supports Local Provisioning (DRM_SUPPORT_LOCAL_PROVISIONING=2) and you use
** a device certificate template in the TEE. Under these cirlwmstances, this
** function MUST return the device certificate chain template.
**
** This function is only used if the client supports Local Provisioning with
** a devicer certificate template.
**
** Synopsis:
**
** This function frees the device certificate template returned in the function
** OEM_TEE_LPROV_GetDeviceCertTemplate.  For this implementation, the certificate
** template returned was a weak reference to the global certifcate template object,
** so no memory needs to be freed.
**
** Operations Performed:
**
**  1. Free the device certificate template object (if neccessary).
**
** Arguments:
**
** f_pContext:                         (in/out) The OEM TEE context returned from
**                                              OEM_TEE_BASE_AllocTEEContext.
** f_pcbDeviceCertTemplate:            (out)    The size (in bytes) of the device certificate chain
**                                              template.
** f_ppbDeviceCertTemplate:            (out)    On success, a pointer to the device certificate
**                                              template.  This must be freed by calling
**                                              OEM_TEE_LPROV_FreeDeviceCertTemplate.
** f_pDeviceCertIndexes:               (out)    The set field indexes into f_pbDeviceCertTemplate
**                                              required by the local provisioning mechanism to
**                                              create a valid device certificate chain.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL OEM_TEE_LPROV_FreeDeviceCertTemplate(
    __inout                                         OEM_TEE_CONTEXT             *f_pContext,
    __inout                                         DRM_BYTE                   **f_ppbDeviceCertTemplate )
{
    UNREFERENCED_PARAMETER( f_pContext );

    if( f_ppbDeviceCertTemplate != NULL )
    {
        /* For this implementation there is nothing to free since the certificate is a weak reference to a global object. */
        *f_ppbDeviceCertTemplate = NULL;
    }
}


/*
** The sample model certificate private key zgpriv.dat.
*/
static const DRM_BYTE s_rgbModelCertPrivKey[] PR_ATTR_DATA_OVLY(_s_rgbModelCertPrivKey) =
{
    0x8b, 0x23, 0x5b, 0x80, 0xd9, 0x64, 0x47, 0x82, 0xea, 0xe3,
    0x61, 0x0d, 0xb4, 0xc8, 0xbd, 0xa0, 0x6a, 0x89, 0xf6, 0xbc,
    0x41, 0xc6, 0x1c, 0x27, 0x66, 0x17, 0xff, 0xb1, 0x62, 0x90,
    0xd7, 0xae,
};

/*
** Refer to function comments in oemteelprovcert.c.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_LPROV_GetTemplateModelCertificatePrivateSigningKey(
    __inout                                         OEM_TEE_CONTEXT   *f_pContext,
    __inout                                         OEM_TEE_KEY       *f_pModelCertPrivateKey )
{
    DRM_RESULT dr = DRM_SUCCESS;

    AssertChkArg( f_pModelCertPrivateKey != NULL );

    DRMSIZEASSERT( sizeof(PRIVKEY_P256), sizeof(s_rgbModelCertPrivKey) );

    DRM_DWORD rgbModelPrivKey[sizeof(PRIVKEY_P256)/sizeof(DRM_DWORD)];

    //
    // LWE (kwilson) - MPK Decrypt
    // OEM_TEE_HsCommonEntryWrapper will handle the following:
    //    1. Setup entry into HS mode
    //    2. Call the HS MPK Decrypt function
    //       (Please refer to the function description of prSbHsDecryptMPK for
    //        detailed steps.)
    //    3. Cleanup after returning from HS mode
    //
    ChkDR(OEM_TEE_HsCommonEntryWrapper(PR_SB_HS_DECRYPT_MPK, OVL_INDEX_ILWALID, rgbModelPrivKey));

    ChkVOID( OEM_TEE_MEMCPY(
        OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_ECC256_TO_PRIVKEY_P256( f_pModelCertPrivateKey ),
        (DRM_BYTE *)rgbModelPrivKey,
        sizeof(PRIVKEY_P256) ) );

ErrorExit:
    return dr;
}
#endif

EXIT_PK_NAMESPACE_CODE;

