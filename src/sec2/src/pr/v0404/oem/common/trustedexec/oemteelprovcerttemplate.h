/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef _OEMTEELPROVCERTTEMPLATE_H_
#define _OEMTEELPROVCERTTEMPLATE_H_1

#if defined (SEC_COMPILE)

#include <oemteetypes.h>
#include <drmteetypes.h>
#include <drmteeproxystubcommon.h>

ENTER_PK_NAMESPACE;

typedef enum __tagDRM_TEE_LPROV_PUBKEY_INDEX
{
    DRM_TEE_LPROV_PUBKEY_SIGN_IDX         = 0,
    DRM_TEE_LPROV_PUBKEY_ENCRYPT_IDX      = 1,

    /* The following enum value MUST be last item in the enum. */
    DRM_TEE_LPROV_PUBKEY_IDX_SIZE         = 2,
} DRM_TEE_LPROV_PUBKEY_INDEX;

typedef struct __tagDRM_TEE_LPROV_CERT_INDEXES
{
    DRM_DWORD rgibPubKeys[DRM_TEE_LPROV_PUBKEY_IDX_SIZE];
    DRM_DWORD ibCertClientID;
    DRM_DWORD ibCertID;
    DRM_DWORD ibCertDigest;
    DRM_DWORD ibSignData;
    DRM_DWORD cbSignData;
    DRM_DWORD ibSignature;
} DRM_TEE_LPROV_CERT_INDEXES;

PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_PARAM_25004, "OEM Context should never be const." )

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_LPROV_GetDeviceCertTemplate(
    __inout                                         OEM_TEE_CONTEXT             *f_pContext,
    __out                                           DRM_DWORD                   *f_pcbDeviceCertTemplate,
    __out_bcount( *f_pcbDeviceCertTemplate )        DRM_BYTE                   **f_ppbDeviceCertTemplate,
    __out                                           DRM_TEE_LPROV_CERT_INDEXES  *f_pDeviceCertIndexes );

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL OEM_TEE_LPROV_FreeDeviceCertTemplate(
    __inout                                         OEM_TEE_CONTEXT             *f_pContext,
    __inout                                         DRM_BYTE                   **f_ppbDeviceCertTemplate );


DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_LPROV_GetTemplateModelCertificatePrivateSigningKey(
    __inout                                         OEM_TEE_CONTEXT   *f_pContext,
    __inout                                         OEM_TEE_KEY       *f_pModelCertPrivateKey )
GCC_ATTRIB_NO_STACK_PROTECT();
        
PREFAST_POP     /* __WARNING_NONCONST_PARAM_25004 */

EXIT_PK_NAMESPACE;

#endif
#endif /* _OEMTEELPROVCERTTEMPLATE_H_ */

