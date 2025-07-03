/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef DRMTEEBASEKB_H
#define DRMTEEBASEKB_H 1

#include <drmteetypes.h>
#include <drmteebase.h>
#include <oemteetypes.h>
#include <oemaeskeywraptypes.h>
#include <drmrevocationtypes.h>
#include <drmbcertconstants.h>

ENTER_PK_NAMESPACE;

/*
** Some of the following KB crypto data structures are fixed size and some are variable length.
** For the variable length objects, there are two different variety: first, there are the type
** that simply have one field that is variable; second, there are some types that have multiple
** members that are varaible.  For the former, we include the variable sized member at the end
** of the structure as an array of 1 object. When building, if there is more than one object we
** allocated the appropriate size and use the array to access the additional objects. For the
** latter (mulitple variable sized objects), we callwlate and allocate the size required and use
** cb/ib members to reference where in the object the data exists. In addition, all variable
** size structures start with a cb member that declares the full size of the object.  This is
** used to validate the ib references.
*/

typedef struct __tagDRM_TEE_KB_PPKBData
{
    DRM_DWORD dwidTK;
    DRM_DWORD cKeys;
    OEM_WRAPPED_KEY_ECC_256 rgKeys[3]; /* PPKB has at most 3 keys: encrypt, sign, PRND */
    OEM_WRAPPED_KEY_AES_128 oSSTKey;
} DRM_TEE_KB_PPKBData;

typedef struct __tagDRM_TEE_KB_LKBData
{
    DRM_BOOL  fPersist;
    DRM_DWORD dwidTK;
    OEM_WRAPPED_KEY_AES_128 rgKeys[2]; /* LKB has at most 2 keys: CI and CK  */
} DRM_TEE_KB_LKBData;

typedef struct __tagDRM_TEE_KB_DKBData
{
    DRM_DWORD dwidTK;
    DRM_DWORD dwRevision;
    OEM_WRAPPED_KEY_ECC_256 oKey;
} DRM_TEE_KB_DKBData;

typedef struct __tagDRM_TEE_KB_MRKBData
{
    OEM_WRAPPED_KEY_AES_128 rgKeys[2]; /* MI/MK */
} DRM_TEE_KB_MRKBData;

typedef struct __tagDRM_TEE_KB_MTKB_Context
{
    DRM_ID                  idPrndSessionID;
    PUBKEY_P256             oCertEncryptionPubKey;
    DRM_DWORD               cReceiverCertDigests;
    DRM_RevocationEntry     rgbReceiverCertDigests[DRM_BCERT_MAX_CERTS_PER_CHAIN];
    DRM_ID                  idProximityNonce;
    DRM_ID                  idLmID;
    DRM_DWORD               dwFlags;
    DRM_DWORD               dwTimeDiffInSeconds;
    DRM_UINT64              qwMtkbTimeStampInSeconds;
    DRM_UINT64              qwRttTimeStampInMilliseconds;
    DRM_DWORD               dwCertSelwrityLevel;
    PUBKEY_P256             oRxPRNDEncryptionKey;
} DRM_TEE_KB_MTKB_Context;

#define MTKB_IS_FLAGSET(pctx, flag)         (flag) == ((pctx)->dwFlags & (flag))
#define MTKB_SET_FLAG(pctx, flag)           (pctx)->dwFlags |=  (flag)
#define MTKB_CLEAR_FLAG(pctx, flag)         (pctx)->dwFlags &= ~(flag)

typedef struct __tagDRM_TEE_KB_MTKBData
{
    DRM_TEE_KB_MTKB_Context oContext;
    OEM_WRAPPED_KEY_AES_128 rgMIMK[2];
} DRM_TEE_KB_MTKBData;

typedef enum __tagDRM_TEE_KB_MTKB_Flags
{
    MTKB_FLAG__RECEIVER_IN_FUTURE           = 0x01,
    MTKB_FLAG__SELWRE_OR_ANTIROLLBACK_CLOCK = 0x02,
    MTKB_FLAG__PROXIMITY_COMPLETED          = 0x04,
} DRM_TEE_KB_MTKB_Flags;

typedef struct __tagDRM_TEE_RKB_CONTEXT
{
    DRM_DWORD  dwRIV;
    DRM_DWORD  dwAppCrlVersion;
    DRM_DWORD  dwRuntimeCrlVersion;
    DRM_UINT64 uiRevInfoExpirationDate;
} DRM_TEE_RKB_CONTEXT;

typedef struct __tagDRM_TEE_KB_LPKBData
{
    DRM_DWORD                   cKeys;
    OEM_WRAPPED_KEY_ECC_256     rgKeys[ 3 ];    /* signing, encryption, and optionally PRND encryption */
} DRM_TEE_KB_LPKBData;

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_BuildPPKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                                  DRM_DWORD                   f_cKeys,
    __in_ecount( f_cKeys )                          const DRM_TEE_KEY                *f_pKeys,
    __in_ecount( 1 )                                const DRM_TEE_KEY                *f_pSSTKey,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_pPPKB )
GCC_ATTRIB_NO_STACK_PROTECT();

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_ParseAndVerifyPPKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pPPKB,
    __inout                                               DRM_DWORD                  *f_pcKeys,
    __deref_out_ecount( *f_pcKeys )                       DRM_TEE_KEY               **f_ppKeys,
    __out_ecount_opt( 1 )                                 DRM_TEE_KB_PPKBData        *f_pPPKBData )
GCC_ATTRIB_NO_STACK_PROTECT();

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_BuildLKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                                  DRM_BOOL                    f_fPersist,
    __in_ecount( 2 )                                const DRM_TEE_KEY                *f_pKeys,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_pLKB );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_ParseAndVerifyLKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pLKB,
    __out                                                 DRM_DWORD                  *f_pcKeys,
    __deref_out_ecount( *f_pcKeys )                       DRM_TEE_KEY               **f_ppKeys );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_BuildSPKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in_ecount( 1 )                                const DRM_TEE_KEY                *f_pKey,
    __in                                                  DRM_DWORD                   f_dwSelwrityLevel,
    __in                                                  DRM_DWORD                   f_cCertDigests,
    __in_ecount(f_cCertDigests)                     const DRM_RevocationEntry        *f_pCertDigests,
    __in                                                  DRM_DWORD                   f_dwRIV,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_pSPKB );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_ParseAndVerifySPKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pSPKB,
    __out                                                 DRM_DWORD                  *f_pdwSelwrityLevel,
    __out                                                 DRM_DWORD                  *f_pcCertDigests,
    __out_ecount( *f_pcCertDigests )                      DRM_RevocationEntry        *f_pCertDigests,
    __out                                                 DRM_DWORD                  *f_pdwRIV,
    __out                                                 DRM_DWORD                  *f_pcKeys,
    __deref_out_ecount( *f_pcKeys )                       DRM_TEE_KEY               **f_ppKeys );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_BuildTPKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in_ecount_opt( 1 )                            const DRM_TEE_KEY                *f_pKeySession,
    __in_ecount( RPROV_KEYPAIR_COUNT )              const DRM_TEE_KEY                *f_pECCKeysPrivate,
    __in_ecount( RPROV_KEYPAIR_COUNT )              const PUBKEY_P256                *f_pECCKeysPublic,
    __in_ecount( OEM_PROVISIONING_NONCE_LENGTH )    const DRM_BYTE                   *f_pbNonce,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_pTPKB );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_ParseAndVerifyTPKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pTPKB,
    __deref_out_ecount_opt( *f_pcoKSession )              DRM_TEE_KEY               **f_ppoKSession,
    __out_ecount( 1 )                                     DRM_DWORD                  *f_pcoKSession,
    __deref_opt_out_ecount( RPROV_KEYPAIR_COUNT )         DRM_TEE_KEY               **f_ppoECCKeysPrivate,
    __out_ecount( RPROV_KEYPAIR_COUNT )                   PUBKEY_P256                *f_poECCKeysPublic,
    __out_ecount_opt( OEM_PROVISIONING_NONCE_LENGTH )     DRM_BYTE                   *f_pbNonce );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_BuildDKB(
    __inout_opt                                           DRM_TEE_CONTEXT            *f_pContext,
    __in_ecount( 1 )                                      DRM_TEE_KEY                *f_pKey,
    __in                                                  DRM_DWORD                   f_dwRevision,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_pDKB );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_ParseAndVerifyDKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pDKB,
    __in                                                  DRM_DWORD                   f_dwRevision,
    __deref_out_ecount( 1 )                               DRM_TEE_KEY               **f_ppKey );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_BuildCDKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                                  DRM_BOOL                    f_fSelfContained,
    __in                                                  DRM_DWORD                   f_cKeys,
    __in_ecount( f_cKeys )                          const DRM_TEE_KEY                *f_pKeys,
    __in_ecount( 1 )                                      DRM_TEE_POLICY_INFO        *f_pPolicy,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_pCDKB );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_ParseAndVerifyCDKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pCDKB,
    __in                                                  DRM_BOOL                    f_fSelfContained,
    __out                                                 DRM_DWORD                  *f_pcKeys,
    __deref_out_ecount( *f_pcKeys )                       DRM_TEE_KEY               **f_ppKeys,
    __out_ecount( 1 )                                     DRM_TEE_POLICY_INFO        *f_pPolicy );

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_KB_FreeCDKBCryptoDataRestrictionArray(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                                  DRM_DWORD                   f_cRestrictions,
    __inout_opt                                           DRM_TEE_POLICY_RESTRICTION *f_pRestrictions );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_BuildMRKB(
    __inout_opt                                           DRM_TEE_CONTEXT            *f_pContext,
    __in_ecount( 2 )                                      DRM_TEE_KEY                *f_pMIMK,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_pMRKB );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_ParseAndVerifyMRKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pMRKB,
    __deref_out_ecount( 2 )                               DRM_TEE_KEY               **f_ppKeys );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_BuildMTKB(
    __inout_opt                                           DRM_TEE_CONTEXT            *f_pContext,
    __in_ecount( 1 )                                const DRM_TEE_KB_MTKB_Context    *f_pMTKBContext,
    __in_ecount( 2 )                                      DRM_TEE_KEY                *f_pMIMK,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_pMTKB );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_ParseAndVerifyMTKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pMTKB,
    __out_ecount_opt( 1 )                                 DRM_TEE_KB_MTKB_Context    *f_pMTKBContext,
    __deref_out_ecount( 2 )                               DRM_TEE_KEY               **f_ppKeys );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_BuildRKB(
    __inout_opt                                           DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_RKB_CONTEXT        *f_pRKBContext,
    __in                                                  DRM_DWORD                   f_cCrlDigestEntries,
    __in_ecount_opt( f_cCrlDigestEntries )          const DRM_RevocationEntry        *f_pCrlDigestEntries,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_pRKB );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_ParseAndVerifyRKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pRKB,
    __out                                                 DRM_TEE_RKB_CONTEXT        *f_pRkbCtx,
    __in                                                  DRM_DWORD                   f_cDigests,
    __in_ecount_opt(f_cDigests)                     const DRM_RevocationEntry        *f_pDigests );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_BuildCEKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                                  DRM_BOOL                    f_fRoot,
    __in                                                  DRM_DWORD                   f_dwEncryptionMode,
    __in                                                  DRM_DWORD                   f_dwSelwrityLevel,
    __in_ecount( 2 )                                const DRM_TEE_KEY                *f_pKeys,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_pCEKB );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_ParseAndVerifyCEKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pCEKB,
    __out_opt                                             DRM_BOOL                   *f_pfRoot,
    __out_opt                                             DRM_DWORD                  *f_pdwSelwrityLevel,
    __out_opt                                             DRM_DWORD                  *f_pdwEncryptionMode,
    __out                                                 DRM_DWORD                  *f_pcKeys,
    __deref_out_ecount( *f_pcKeys )                       DRM_TEE_KEY               **f_ppKeys );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_BuildNKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in_ecount( 1 )                                const DRM_ID                     *f_pNonce,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_pNKB );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_ParseAndVerifyNKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pNKB,
    __out_ecount(1)                                       DRM_ID                     *f_pNonce );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_FinalizePPKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                                  DRM_DWORD                   f_cKeys,
    __in                                            const DRM_TEE_KEY                *f_poKeys,
    __in_tee_opt                                    const DRM_TEE_BYTE_BLOB          *f_poPrevPPKB,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_poPPKB )
GCC_ATTRIB_NO_STACK_PROTECT();

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_PackRemoteProvisioningBlob(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                                  DRM_DWORD                   f_cbBlob,
    __in_bcount(f_cbBlob)                           const DRM_BYTE                   *f_pbBlob,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_pRProvContextKB );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_UnpackRemoteProvisioningBlob(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pRProvContextKB,
    __out_ecount( 1 )                                     DRM_DWORD                  *f_pcbBlob,
    __deref_out_ecount( *f_pcbBlob )                      DRM_BYTE                  **f_ppbBlob );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_ParseAndVerifyLPKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pLPKB,
    __out                                                 DRM_DWORD                  *f_pcECCKeysPrivate,
    __out_ecount( *f_pcECCKeysPrivate )                   DRM_TEE_KEY               **f_ppECCKeysPrivate );

EXIT_PK_NAMESPACE;

#endif // DRMTEEBASEKB_H

