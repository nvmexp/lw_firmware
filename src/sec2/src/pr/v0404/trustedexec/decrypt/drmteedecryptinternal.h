/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef _DRMTEEDECRYPTINTERNAL_H_
#define _DRMTEEDECRYPTINTERNAL_H_ 1

#include <drmteetypes.h>
#include <oemteetypes.h>
#include <drmteebase.h>

ENTER_PK_NAMESPACE;

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_DECRYPT_ParseAndVerifyExternalPolicyCryptoInfo(
    __inout            DRM_TEE_CONTEXT          *f_pContext,
    __in_opt     const DRM_TEE_BYTE_BLOB        *f_pCDKB,
    __in         const DRM_TEE_BYTE_BLOB        *f_pOEMKeyInfo,
    __in               DRM_BOOL                  f_fIsReconstituted,
    __out              DRM_DWORD                *f_pcKeys,
    __deref_out        DRM_TEE_KEY             **f_ppKeys,
    __deref_out        DRM_TEE_POLICY_INFO     **f_ppPolicyInfo,
    __out_opt          DRM_ID                   *f_pSessionID );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_DECRYPT_ParseAndVerifyCDKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pCDKB,
    __in                                                  DRM_BOOL                    f_fSelfContained,
    __out                                                 DRM_DWORD                  *f_pcKeys,
    __deref_out_ecount( *f_pcKeys )                       DRM_TEE_KEY               **f_ppKeys,
    __deref_opt_out_ecount_opt( 1 )                       DRM_TEE_KEY               **f_ppSelwreStop2Key,
    __out_opt                                             DRM_ID                     *f_pidSelwreStopSession,
    __deref_out                                           DRM_TEE_POLICY_INFO       **f_ppPolicy );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_DECRYPT_DecryptContentPolicyHelper(
    __inout_opt                 DRM_TEE_CONTEXT                      *f_pContext,
    __deref_out                 DRM_TEE_CONTEXT                     **f_ppContext,
    __inout                     DRM_TEE_CONTEXT                      *f_pTeeCtxReconstituted,
    __out                       DRM_BOOL                             *f_pfReconstituted,
    __in                        DRM_TEE_POLICY_INFO_CALLING_API       f_ePolicyCallingAPI,
    __in                  const DRM_TEE_BYTE_BLOB                    *f_pCDKB,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB                    *f_pOEMKeyInfo,
    __out                       DRM_DWORD                            *f_pcKeys,
    __deref_out                 DRM_TEE_KEY                         **f_ppKeys,
    __deref_out                 DRM_TEE_POLICY_INFO                 **f_ppPolicy );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_DECRYPT_EnforcePolicy(
    __inout                     OEM_TEE_CONTEXT                      *f_pContext,
    __in                        DRM_TEE_POLICY_INFO_CALLING_API       f_ePolicyCallingAPI,
    __in                  const DRM_TEE_POLICY_INFO                  *f_pPolicy );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_AES128_DecryptContentHelper(
    __inout_opt                 DRM_TEE_CONTEXT                 *f_pContext,
    __in                        DRM_TEE_POLICY_INFO_CALLING_API  f_ePolicyCallingAPI,
    __in                  const DRM_TEE_BYTE_BLOB               *f_pCDKB,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB               *f_pOEMKeyInfo,
    __in                  const DRM_TEE_QWORDLIST               *f_pEncryptedRegionInitializatiolwectorsHigh,
    __in_tee_opt          const DRM_TEE_QWORDLIST               *f_pEncryptedRegionInitializatiolwectorsLow,
    __in                  const DRM_TEE_DWORDLIST               *f_pEncryptedRegionCounts,
    __in                  const DRM_TEE_DWORDLIST               *f_pEncryptedRegionMappings,
    __in_tee_opt          const DRM_TEE_DWORDLIST               *f_pEncryptedRegionSkip,
    __in                  const DRM_TEE_BYTE_BLOB               *f_pEncrypted,
    __out                       DRM_TEE_BYTE_BLOB               *f_pCCD );

EXIT_PK_NAMESPACE;

#endif /* _DRMTEEDECRYPTINTERNAL_H_ */

