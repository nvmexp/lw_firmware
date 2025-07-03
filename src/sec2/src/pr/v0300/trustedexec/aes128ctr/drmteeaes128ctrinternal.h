/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef DRMTEEAES128CTRINTERNAL_H
#define DRMTEEAES128CTRINTERNAL_H 1

#include <drmteetypes.h>
#include <oemteetypes.h>

ENTER_PK_NAMESPACE;

DRM_NO_INLINE DRM_API DRM_RESULT DRM_TEE_AES128CTR_IMPL_ParseAndVerifyExternalPolicyCryptoInfo(
    __inout            DRM_TEE_CONTEXT          *f_pContext,
    __in_opt     const DRM_TEE_BYTE_BLOB        *f_pCDKB,
    __in         const DRM_TEE_BYTE_BLOB        *f_pOEMKeyInfo,
    __in               DRM_BOOL                  f_fIsReconstituted,
    __out              DRM_DWORD                *f_pcKeys,
    __deref_out        DRM_TEE_KEY             **f_ppKeys,
    __deref_out        DRM_TEE_POLICY_INFO     **f_ppPolicyInfo,
    __out_opt          DRM_ID                   *f_pSessionID );

EXIT_PK_NAMESPACE;

#endif // DRMTEEAES128CTRINTERNAL_H

