/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef DRMTEEBASE_H
#define DRMTEEBASE_H 1

#include <drmteetypes.h>
#include <oemteetypes.h>
#include <drmxmrformatparser.h>
#include <drmxmrconstants.h>
#include <oemeccp256.h>
#include <drmbcertformatparser.h>

ENTER_PK_NAMESPACE;

extern const PUBKEY_P256 g_ECC256MSPlayReadyRootIssuerPubKeyTEE;

typedef enum
{
    DRM_TEE_KEY_TYPE_ILWALID                 = 0,
    DRM_TEE_KEY_TYPE_TK                      = 1,
    DRM_TEE_KEY_TYPE_TKD                     = 2,
    DRM_TEE_KEY_TYPE_PR_SST                  = 3,
    DRM_TEE_KEY_TYPE_PR_CI                   = 4,
    DRM_TEE_KEY_TYPE_PR_CK                   = 5,
    DRM_TEE_KEY_TYPE_PR_MI                   = 6,
    DRM_TEE_KEY_TYPE_PR_MK                   = 7,
    DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_SIGN     = 8,
    DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_ENCRYPT  = 9,
    DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_PRND     = 10,
    DRM_TEE_KEY_TYPE_RPROV_SESSION_KEY       = 11,
    DRM_TEE_KEY_TYPE_RPROV_SESSION_KEY_CONF  = 12,
    DRM_TEE_KEY_TYPE_RPROV_SESSION_KEY_WRAP  = 13,
    DRM_TEE_KEY_TYPE_RPROV_SESSION_KEY_SIGN  = 14,
    DRM_TEE_KEY_TYPE_SAMPLEPROT              = 15,
    DRM_TEE_KEY_TYPE_DOMAIN_SESSION          = 16,
    DRM_TEE_KEY_TYPE_AES128_DERIVATION       = 17,
} DRM_TEE_KEY_TYPE;

typedef enum
{
    DRM_TEE_XB_KB_OPERATION_SIGN    = 0x1,
    DRM_TEE_XB_KB_OPERATION_ENCRYPT = 0x2,
} DRM_TEE_XB_KB_OPERATION;

typedef struct __tagDRM_TEE_KEY
{
    DRM_TEE_KEY_TYPE     eType;
    DRM_DWORD            dwidTK;    /* Only set if eType == DRM_TEE_KEY_TYPE_TK */
    OEM_TEE_KEY          oKey;
} DRM_TEE_KEY;

typedef struct __tagDRM_TEE_XMR_LICENSE
{
    DRM_BOOL                     fValid;
    DRM_XMRFORMAT                oLicense;
    DRM_DWORD                    cbStack;
    DRM_BYTE                    *pbStack;
    DRM_STACK_ALLOCATOR_CONTEXT  oStack;
} DRM_TEE_XMR_LICENSE;

#define DRM_TEE_KEY_EMPTY   { DRM_TEE_KEY_TYPE_ILWALID, 0, OEM_TEE_KEY_EMPTY }

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_ValidateLicenseExpiration(
    __inout               DRM_TEE_CONTEXT              *f_pContext,
    __in                  DRM_XMRFORMAT                *f_pLicense,
    __in                  DRM_BOOL                      f_fValidateBeginDate,
    __in                  DRM_BOOL                      f_fValidateEndDate );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_CheckLicenseSignature(
    __inout                DRM_TEE_CONTEXT              *f_pContext,
    __in             const DRM_XMRFORMAT                *f_pXMRLicense,
    __in_ecount( 2 ) const DRM_TEE_KEY                  *f_pKeys );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_AllocateKeyArray(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                                  DRM_DWORD                   f_cKeys,
    __deref_out                                           DRM_TEE_KEY               **f_ppKeys )
GCC_ATTRIB_NO_STACK_PROTECT();

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_BASE_IMPL_FreeKeyArray(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                                  DRM_DWORD                   f_cKeys,
    __deref_inout_ecount( f_cKeys )                       DRM_TEE_KEY               **f_ppKeys )
GCC_ATTRIB_NO_STACK_PROTECT();

DRM_NO_INLINE DRM_API DRM_ID DRM_CALL DRM_TEE_BASE_IMPL_GetKeyDerivationIDSign(
    __in                                                  DRM_TEE_XB_KB_TYPE          f_eKBType )
GCC_ATTRIB_NO_STACK_PROTECT();

DRM_NO_INLINE DRM_API DRM_ID DRM_CALL DRM_TEE_BASE_IMPL_GetKeyDerivationIDEncrypt(
    __in                                                  DRM_TEE_XB_KB_TYPE          f_eKBType )
GCC_ATTRIB_NO_STACK_PROTECT();

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_TEE_BASE_IMPL_IsKBTypePersistedToDisk(
    __in                                                  DRM_TEE_XB_KB_TYPE          f_eKBType );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_DeriveTKDFromTK(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in_opt                                        const DRM_TEE_KEY                *f_pTK,
    __in                                                  DRM_TEE_XB_KB_TYPE          f_eType,
    __in                                                  DRM_TEE_XB_KB_OPERATION     f_eOperation,
    __in_opt                                        const DRM_BOOL                   *f_pfPersist,
    __out                                                 DRM_TEE_KEY                *f_pTKD )
GCC_ATTRIB_NO_STACK_PROTECT();

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_AllocBlobAndTakeOwnership(
    __inout_opt                                           DRM_TEE_CONTEXT            *f_pContext,
    __in                                                  DRM_DWORD                   f_cb,
    __inout_bcount(f_cb)                                  DRM_BYTE                  **f_ppb,
    __inout                                               DRM_TEE_BYTE_BLOB          *f_pBlob );

DRM_NO_INLINE DRM_API DRM_TEE_KEY* DRM_CALL DRM_TEE_BASE_IMPL_LocateKeyInPPKBWeakRef(
    __in_opt                 const DRM_TEE_KEY_TYPE    *f_peType,
    __in                           DRM_DWORD            f_cPPKBKeys,
    __in_ecount( f_cPPKBKeys )     DRM_TEE_KEY         *f_pPPKBKeys );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_GetPKVersion(
    __out   DRM_DWORD    *f_pdwPKMajorVersion,
    __out   DRM_DWORD    *f_pdwPKMinorVersion,
    __out   DRM_DWORD    *f_pdwPKBuildVersion,
    __out   DRM_DWORD    *f_pdwPKQFEVersion );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_CopyDwordPairsToAlignedBufferAndSwapEndianness(
    __inout    DRM_TEE_CONTEXT      *f_pContext,
    __out      DRM_TEE_BYTE_BLOB    *f_pDest,
    __in const DRM_TEE_BYTE_BLOB    *f_pSource );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_AllocKeyAES128(
    __inout_opt                                     OEM_TEE_CONTEXT            *f_pContextAllowNULL,
    __in                                            DRM_TEE_KEY_TYPE            f_eType,
    __out                                           DRM_TEE_KEY                *f_pKey );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_AllocKeyECC256(
    __inout                                         OEM_TEE_CONTEXT            *f_pContext,
    __in                                            DRM_TEE_KEY_TYPE            f_eType,
    __out                                           DRM_TEE_KEY                *f_pKey );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_TEE_SAMPLEPROT_IMPL_IsDecryptionModeSupported(
    __in                        DRM_DWORD                     f_dwMode );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_SAMPLEPROT_IMPL_ApplySampleProtection(
    __inout                                         OEM_TEE_CONTEXT        *f_pContext,
    __in                                      const OEM_TEE_KEY            *f_pSPK,
    __in                                            DRM_UINT64              f_ui64Initializatiolwector,
    __in                                            DRM_DWORD               f_cbClearToSampleProtected,
    __inout_bcount( f_cbClearToSampleProtected )    DRM_BYTE               *f_pbClearToSampleProtected );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_GetAppSpecificHWID(
    __inout                                         OEM_TEE_CONTEXT            *f_pOemTeeCtx,
    __in                                      const DRM_ID                     *f_pidApplication,
    __out                                           DRM_ID                     *f_pidHWID )
GCC_ATTRIB_NO_STACK_PROTECT();

#define DRM_AESCBC_ENCRYPTED_SIZE_W_IV(cb) (cb+2*DRM_AES_BLOCKLEN-cb%DRM_AES_BLOCKLEN)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_AES128CBC(
    __inout                                                DRM_TEE_CONTEXT            *f_pContext,
    __in                                                   DRM_DWORD                   f_cbBlob,
    __in_bcount(f_cbBlob)                            const DRM_BYTE                   *f_pbBlob,
    __in                                             const DRM_TEE_KEY                *f_poKey,
    __inout                                                DRM_DWORD                  *f_pcbEncryptedBlob,
    __out_bcount(DRM_AESCBC_ENCRYPTED_SIZE_W_IV(f_cbBlob)) DRM_BYTE                   *f_pbEncryptedBlob );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_ParseCertificateChain(
    __inout_opt                           OEM_TEE_CONTEXT                       *f_pOemTeeContext,
    __in_ecount_opt( 1 )            const DRMFILETIME                           *f_pftLwrrentTime,
    __in                                  DRM_DWORD                              f_dwExpectedLeafCertType,
    __in                                  DRM_DWORD                              f_cbCertData,
    __in_bcount( f_cbCertData )     const DRM_BYTE                              *f_pbCertData,
    __out                                 DRM_BCERTFORMAT_PARSER_CHAIN_DATA     *f_pChainData );


DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_ParseLicense(
    __inout                           DRM_TEE_CONTEXT              *f_pContext,
    __in                              DRM_DWORD                     f_cbLicense,
    __in_ecount( f_cbLicense )  const DRM_BYTE                     *f_pbLicense,
    __inout                           DRM_TEE_XMR_LICENSE          *f_poLicense );

EXIT_PK_NAMESPACE;

#endif // DRMTEEBASE_H

