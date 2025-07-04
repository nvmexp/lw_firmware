/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef DRMTEE_H
#define DRMTEE_H 1

#include <drmteetypes.h>
#include <oemteetypes.h>

ENTER_PK_NAMESPACE;

PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_PARAM_25004, "Out params can't be const")
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_BUFFER_PARAM_25033, "Out params can't be const")

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_AllocTEEContext(
    __inout                     DRM_TEE_CONTEXT             **f_ppContext,
    __in_opt                    DRM_TEE_BYTE_BLOB            *f_pApplicationInfo,
    __out_opt                   DRM_TEE_BYTE_BLOB            *f_pSystemInfo );

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_BASE_FreeTEEContext(
    __inout                     DRM_TEE_CONTEXT             **f_ppContext );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_MemAlloc(
    __inout_opt                 DRM_TEE_CONTEXT              *f_pContext,
    __in                        DRM_DWORD                     f_cb,
    __deref_out_bcount(f_cb)    DRM_VOID                    **f_ppv );

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_BASE_MemFree(
    __inout_opt                 DRM_TEE_CONTEXT              *f_pContext,
    __inout                     DRM_VOID                    **f_ppv );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_AllocBlob(
    __inout_opt                 DRM_TEE_CONTEXT              *f_pContext,
    __in                        DRM_TEE_BLOB_ALLOC_BEHAVIOR   f_eBehavior,
    __in                        DRM_DWORD                     f_cb,
    __in_bcount_opt(f_cb) const DRM_BYTE                     *f_pb,
    __inout                     DRM_TEE_BYTE_BLOB            *f_pBlob );

//
// LWE (kwilson)  In Microsoft PK code, DRM_TEE_BASE_FreeBlob returns DRM_VOID.
// Lwpu had to change the return value to DRM_RESULT in order to support a return
// code from acquiring/releasing the critical section, which may not always succeed.
//
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_FreeBlob(
    __inout_opt                 DRM_TEE_CONTEXT              *f_pContext,
    __inout                     DRM_TEE_BYTE_BLOB            *f_pBlob );

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_BASE_TransferBlobOwnership(
    __inout                     DRM_TEE_BYTE_BLOB            *f_pDest,
    __inout                     DRM_TEE_BYTE_BLOB            *f_pSource );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_SignDataWithSelwreStoreKey(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pPPKB,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pDataToSign,
    __out                       DRM_TEE_BYTE_BLOB            *f_pSignature );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_CheckDeviceKeys(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pPPKB );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_GetDebugInformation(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __out                       DRM_DWORD                    *f_pdwLastHR,
    __out                       DRM_TEE_BYTE_BLOB            *f_pLog );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_GenerateNonce(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __out                       DRM_TEE_BYTE_BLOB            *f_pNKB,
    __out                       DRM_ID                       *f_pNonce );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_GetSystemTime(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __out                       DRM_UINT64                   *f_pui64SystemTime );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_LPROV_GenerateDeviceKeys(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pPrevPPKB,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pCertificate,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pOEMProtectedCertificatePrivateKeys,
    __in                  const DRM_ID                       *f_pidApplication,
    __out                       DRM_TEE_BYTE_BLOB            *f_pPPKB,
    __out                       DRM_TEE_BYTE_BLOB            *f_pNewCertificate )
GCC_ATTRIB_NO_STACK_PROTECT();

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_RPROV_GenerateBootstrapChallenge(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __inout_tee_opt             DRM_TEE_BYTE_BLOB            *f_pRProvContext,
    __out                       DRM_DWORD                    *f_pdwType,
    __out                       DRM_DWORD                    *f_pdwStep,
    __out                       DRM_DWORD                    *f_pdwAdditionalInfoNeeded,
    __out                       DRM_TEE_BYTE_BLOB            *f_pChallenge );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_RPROV_ProcessBootstrapResponse(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __inout_tee_opt             DRM_TEE_BYTE_BLOB            *f_pRProvContext,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pResponse,
    __out                       DRM_TEE_BYTE_BLOB            *f_pTPKBOut );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_RPROV_GenerateProvisioningRequest(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pTPKB,
    __in                  const DRM_ID                       *f_pidApplicationID,
    __out                       DRM_TEE_BYTE_BLOB            *f_pProvRequest );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_RPROV_ProcessProvisioningResponse(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pPrevPPKB,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pResponse,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pTPKB,
    __out                       DRM_TEE_BYTE_BLOB            *f_pPPKB,
    __out                       DRM_TEE_BYTE_BLOB            *f_pCert );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_LICPREP_PackageKey(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pPPKB,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pLicense,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pDKB,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pNKB,
    __out                       DRM_TEE_BYTE_BLOB            *f_pLKB );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_SAMPLEPROT_PrepareSampleProtectionKey(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pCertificate,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pRKB,
    __out                       DRM_TEE_BYTE_BLOB            *f_pSPKB,
    __out                       DRM_TEE_BYTE_BLOB            *f_pEncryptedKey );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_SELWRESTOP_GetGenerationID(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_ID                       *f_pidElwironment,
    __out                       DRM_DWORD                    *f_pdwGenerationID );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_AES128CTR_PreparePolicyInfo(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pLicenses,
    __in                        DRM_DWORD                     f_dwDecryptionMode,
    __out                       DRM_TEE_BYTE_BLOB            *f_pOEMPolicyInfo );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_AES128CTR_PrepareToDecrypt(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pLKB,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pLicenses,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pRKB,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pSPKB,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pChecksum,
    __inout                     DRM_DWORD                    *f_pdwDecryptionMode,
    __out                       DRM_TEE_BYTE_BLOB            *f_pCDKB );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_AES128CTR_CreateOEMBlobFromCDKB(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                        DRM_TEE_BYTE_BLOB            *f_pCDKB,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pOEMInitData,
    __out                       DRM_TEE_BYTE_BLOB            *f_pOEMKeyInfo );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_AES128CTR_DecryptContent(
    __inout_opt                 DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pCDKB,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pOEMKeyInfo,
    __in                  const DRM_TEE_DWORDLIST            *f_pEncryptedRegionMapping,
    __in                        DRM_UINT64                    f_ui64Initializatiolwector,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pEncrypted,
    __out                       DRM_TEE_BYTE_BLOB            *f_pCCD );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_AES128CTR_DecryptAudioContentMultiple(
    __inout_opt                 DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pCDKB,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pOEMKeyInfo,
    __in                  const DRM_TEE_QWORDLIST            *f_pInitializatiolwectors,
    __in                  const DRM_TEE_DWORDLIST            *f_pInitializatiolwectorSizes,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pEncrypted,
    __out                       DRM_TEE_BYTE_BLOB            *f_pCCD );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_SIGN_SignHash(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pPPKB,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pDataToSign,
    __out                       DRM_TEE_BYTE_BLOB            *f_pSignature );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_DOM_PackageKeys(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pPPKB,
    __in                        DRM_DWORD                     f_dwProtocolVersion,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pDomainSessionKey,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pDomainPrivateKeys,
    __out                       DRM_TEE_DWORDLIST            *f_pRevisions,
    __out                       DRM_TEE_BYTE_BLOB            *f_pDKBs );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_PRNDRX_ProcessRegistrationResponseMessage(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pPPKB,
    __inout_tee_opt             DRM_TEE_BYTE_BLOB            *f_pMRKB,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pResponseMessage );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_PRNDRX_GenerateProximityResponseNonce(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pMRKB,
    __inout                     DRM_ID                       *f_pNonce );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_PRNDRX_CompleteLicenseRequestMessage(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pMRKB,
    __inout                     DRM_TEE_BYTE_BLOB            *f_pRequestMessage );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_PRNDRX_ProcessLicenseTransmitMessage(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pMRKB,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pTransmitMessage );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_REVOCATION_IngestRevocationInfo(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pRuntimeCRL,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pRevInfo,
    __out                       DRM_TEE_BYTE_BLOB            *f_pRKB );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_LICGEN_CompleteLicense(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pRKB,
    __in                        DRM_TEE_LICGEN_OP             f_eOp,
    __in                        DRM_DWORD                     f_dwEncryptionMode,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pLicense,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pCertificate,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pRootLicense,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pRootCEKB,
    __out                       DRM_TEE_BYTE_BLOB            *f_pLKB,
    __out                       DRM_TEE_BYTE_BLOB            *f_pCEKB,
    __out                       DRM_TEE_BYTE_BLOB            *f_pCompletedLicense );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_LICGEN_AES128CTR_EncryptContent(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pCEKB,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pCCD,
    __in                  const DRM_TEE_DWORDLIST            *f_pEncryptedRegionMapping,
    __out                       DRM_UINT64                   *f_pui64Initializatiolwector,
    __out                       DRM_TEE_BYTE_BLOB            *f_pEncrypted );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_PRNDTX_ProcessRegistrationRequestMessage(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __inout_tee_opt             DRM_TEE_BYTE_BLOB            *f_pMTKB,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pRKB,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pRequestMessage,
    __out                       DRM_ID                       *f_pSessionID );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_PRNDTX_CompleteRegistrationResponseMessage(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pPPKB,
    __inout                     DRM_TEE_BYTE_BLOB            *f_pMTKB,
    __inout                     DRM_TEE_BYTE_BLOB            *f_pResponseMessage );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_PRNDTX_GenerateProximityDetectionChallengeNonce(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __inout                     DRM_TEE_BYTE_BLOB            *f_pMTKB,
    __out                       DRM_ID                       *f_pNonce );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_PRNDTX_VerifyProximityDetectionResponseNonce(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __inout                     DRM_TEE_BYTE_BLOB            *f_pMTKB,
    __in                  const DRM_ID                       *f_pEncryptedNonce );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_PRNDTX_ProcessLicenseRequestMessage(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __inout                     DRM_TEE_BYTE_BLOB            *f_pMTKB,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pRequestMessage );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_PRNDTX_CompleteLicenseTransmitMessage(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __inout                     DRM_TEE_BYTE_BLOB            *f_pMTKB,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pRKB,
    __inout                     DRM_TEE_BYTE_BLOB            *f_pTransmitMessage );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_PRNDTX_RebindLicenseToReceiver(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __inout                     DRM_TEE_BYTE_BLOB            *f_pMTKB,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pRKB,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pLicense,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pLKB,
    __out                       DRM_TEE_BYTE_BLOB            *f_pCompletedLicense );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_H264_PreProcessEncryptedData(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pCDKB,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pOEMKeyInfo,
    __inout                     DRM_UINT64                   *f_pui64Initializatiolwector,
    __in                  const DRM_TEE_DWORDLIST            *f_pEncryptedRegionMapping,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pEncryptedPartialFrame,
    __in_tee_opt          const DRM_TEE_DWORDLIST            *f_pOpaqueSliceHeaderOffsetData,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pOpaqueSliceHeaderState,
    __out                       DRM_TEE_BYTE_BLOB            *f_pOpaqueSliceHeaderStateUpdated,
    __out                       DRM_TEE_BYTE_BLOB            *f_pSliceHeaders,
    __out                       DRM_TEE_BYTE_BLOB            *f_pOpaqueFrameData,
    __inout_tee_opt             DRM_TEE_BYTE_BLOB            *f_pEncryptedTranscryptedFullFrame );

PREFAST_POP /* __WARNING_NONCONST_BUFFER_PARAM_25033 */
PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */

EXIT_PK_NAMESPACE;

#endif // DRMTEE_H

