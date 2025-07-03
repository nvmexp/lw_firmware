/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef _DRMTEEBASE_H_
#define _DRMTEEBASE_H_ 1

#include <drmteetypes.h>
#include <oemteetypes.h>
#include <drmxmrformatparser.h>
#include <drmxmrconstants.h>
#include <oemeccp256.h>
#include <drmbcertformatparser.h>

ENTER_PK_NAMESPACE;

extern DRM_GLOBAL_CONST PUBKEY_P256 g_ECC256MSPlayReadyRootIssuerPubKeyTEE;
extern DRM_GLOBAL_CONST DRM_ANSI_CONST_STRING g_rgdstrDrmTeeProperties[DRM_TEE_PROPERTY_MAX + 1];
extern DRM_GLOBAL_CONST DRM_ANSI_CONST_STRING g_rgstrDrmTeeAPIs[DRM_TEE_METHOD_FUNCTION_MAP_COUNT];
extern DRM_GLOBAL_CONST DRM_ANSI_CONST_STRING g_dstrTeeOpenTag;
extern DRM_GLOBAL_CONST DRM_ANSI_CONST_STRING g_dstrTeeCloseTag;
extern DRM_GLOBAL_CONST DRM_ANSI_CONST_STRING g_dstrPropertyOpenTag;
extern DRM_GLOBAL_CONST DRM_ANSI_CONST_STRING g_dstrPropertyCloseTag;
extern DRM_GLOBAL_CONST DRM_ANSI_CONST_STRING g_dstrAPIOpenTag;
extern DRM_GLOBAL_CONST DRM_ANSI_CONST_STRING g_dstrAPICloseTag;
extern DRM_GLOBAL_CONST DRM_ANSI_CONST_STRING g_dstrHardCodedAPIValue;
extern DRM_GLOBAL_CONST DRM_ANSI_CONST_STRING g_dstrSignatureOpenTag;
extern DRM_GLOBAL_CONST DRM_ANSI_CONST_STRING g_dstrSignatureCloseBrace;
extern DRM_GLOBAL_CONST DRM_ANSI_CONST_STRING g_dstrSignatureCloseTag;


typedef enum
{
    DRM_TEE_KEY_TYPE_ILWALID                            = 0,
    DRM_TEE_KEY_TYPE_TK                                 = 1,
    DRM_TEE_KEY_TYPE_TKD                                = 2,
    DRM_TEE_KEY_TYPE_PR_SST                             = 3,
    DRM_TEE_KEY_TYPE_PR_CI                              = 4,
    DRM_TEE_KEY_TYPE_PR_CK                              = 5,
    DRM_TEE_KEY_TYPE_PR_MI                              = 6,
    DRM_TEE_KEY_TYPE_PR_MK                              = 7,
    DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_SIGN                = 8,
    DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_ENCRYPT             = 9,
    DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_PRND_DEPRECATED     = 10,
    DRM_TEE_KEY_TYPE_RPROV_SESSION_KEY                  = 11,
    DRM_TEE_KEY_TYPE_RPROV_SESSION_KEY_CONF             = 12,
    DRM_TEE_KEY_TYPE_RPROV_SESSION_KEY_WRAP             = 13,
    DRM_TEE_KEY_TYPE_RPROV_SESSION_KEY_SIGN             = 14,
    DRM_TEE_KEY_TYPE_SAMPLEPROT                         = 15,
    DRM_TEE_KEY_TYPE_DOMAIN_SESSION                     = 16,
    DRM_TEE_KEY_TYPE_AES128_DERIVATION                  = 17,
    DRM_TEE_KEY_TYPE_AES128_SELWRESTOP2                 = 18,
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

PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_POOR_DATA_ALIGNMENT_25021, "Since PlayReady ships as source code, it is more important for types to be human-readable for member sequencing than minimum sized." )
typedef struct __tagDRM_TEE_XMR_LICENSE
{
    DRM_BOOL                     fValid;
    DRM_XMRFORMAT                oLicense;
    DRM_DWORD                    cbStack;
    DRM_BYTE                    *pbStack;
    DRM_STACK_ALLOCATOR_CONTEXT  oStack;
} DRM_TEE_XMR_LICENSE;
PREFAST_POP /* __WARNING_POOR_DATA_ALIGNMENT_25021 */

#define DRM_TEE_KEY_EMPTY            { DRM_TEE_KEY_TYPE_ILWALID, 0, OEM_TEE_KEY_EMPTY }
#define TEE_SIGNATURE_VERSION        "1"
#define TEE_SIGNATURE_VERSION_LENGTH 1

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_MemAlloc(
    __inout_opt                   DRM_TEE_CONTEXT              *f_pContext,
    __in                          DRM_DWORD                     f_cb,
    __deref_out_bcount( f_cb )    DRM_VOID                    **f_ppv );

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_IMPL_BASE_MemFree(
    __inout_opt                   DRM_TEE_CONTEXT              *f_pContext,
    __inout                       DRM_VOID                    **f_ppv );

PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_PARAM_25004, "f_pContext Parameter is modified in some implementations." )
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_AllocBlob(
    __inout_opt                   DRM_TEE_CONTEXT              *f_pContext,
    __in                          DRM_TEE_BLOB_ALLOC_BEHAVIOR   f_eBehavior,
    __in                          DRM_DWORD                     f_cb,
    __in_bcount_opt( f_cb ) const DRM_BYTE                     *f_pb,
    __inout                       DRM_TEE_BYTE_BLOB            *f_pBlob );
PREFAST_POP     /* __WARNING_NONCONST_PARAM_25004 */

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_FreeBlob(
    __inout_opt                   DRM_TEE_CONTEXT              *f_pContext,
    __inout                       DRM_TEE_BYTE_BLOB            *f_pBlob );

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_IMPL_BASE_TransferBlobOwnership(
    __inout                       DRM_TEE_BYTE_BLOB            *f_pDest,
    __inout                       DRM_TEE_BYTE_BLOB            *f_pSource );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_ValidateLicenseExpiration(
    __inout               DRM_TEE_CONTEXT              *f_pContext,
    __in            const DRM_XMRFORMAT                *f_pLicense,
    __in                  DRM_BOOL                      f_fValidateBeginDate,
    __in                  DRM_BOOL                      f_fValidateEndDate );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_CheckLicenseSignature(
    __inout                DRM_TEE_CONTEXT              *f_pContext,
    __in             const DRM_XMRFORMAT                *f_pXMRLicense,
    __in_ecount( 2 ) const DRM_TEE_KEY                  *f_pKeys );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_AllocateKeyArray(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                                  DRM_DWORD                   f_cKeys,
    __deref_out                                           DRM_TEE_KEY               **f_ppKeys )
GCC_ATTRIB_NO_STACK_PROTECT();

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_IMPL_BASE_FreeKeyArray(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                                  DRM_DWORD                   f_cKeys,
    __deref_inout_ecount( f_cKeys )                       DRM_TEE_KEY               **f_ppKeys )
GCC_ATTRIB_NO_STACK_PROTECT();

DRM_NO_INLINE DRM_API DRM_ID DRM_CALL DRM_TEE_IMPL_BASE_GetKeyDerivationIDSign(
    __in                                                  DRM_TEE_XB_KB_TYPE          f_eKBType )
GCC_ATTRIB_NO_STACK_PROTECT();

DRM_NO_INLINE DRM_API DRM_ID DRM_CALL DRM_TEE_IMPL_BASE_GetKeyDerivationIDEncrypt(
    __in                                                  DRM_TEE_XB_KB_TYPE          f_eKBType )
GCC_ATTRIB_NO_STACK_PROTECT();

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_TEE_IMPL_BASE_IsKBTypePersistedToDisk(
    __in                                                  DRM_TEE_XB_KB_TYPE          f_eKBType );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_DeriveTKDFromTK(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in_opt                                        const DRM_TEE_KEY                *f_pTK,
    __in                                                  DRM_TEE_XB_KB_TYPE          f_eType,
    __in                                                  DRM_TEE_XB_KB_OPERATION     f_eOperation,
    __in_opt                                        const DRM_BOOL                   *f_pfPersist,
    __out                                                 DRM_TEE_KEY                *f_pTKD )
GCC_ATTRIB_NO_STACK_PROTECT();

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_AllocBlobAndTakeOwnership(
    __inout_opt                                           DRM_TEE_CONTEXT            *f_pContext,
    __in                                                  DRM_DWORD                   f_cb,
    __deref_inout_bcount(f_cb)                            DRM_BYTE                  **f_ppb,
    __inout                                               DRM_TEE_BYTE_BLOB          *f_pBlob );

DRM_NO_INLINE DRM_API DRM_TEE_KEY* DRM_CALL DRM_TEE_IMPL_BASE_LocateKeyInPPKBWeakRef(
    __in_opt                 const DRM_TEE_KEY_TYPE    *f_peType,
    __in                           DRM_DWORD            f_cPPKBKeys,
    __in_ecount( f_cPPKBKeys )     DRM_TEE_KEY         *f_pPPKBKeys );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_GetPKVersion(
    __out   DRM_DWORD    *f_pdwPKMajorVersion,
    __out   DRM_DWORD    *f_pdwPKMinorVersion,
    __out   DRM_DWORD    *f_pdwPKBuildVersion,
    __out   DRM_DWORD    *f_pdwPKQFEVersion );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_CopyDwordPairsToAlignedBufferAndSwapEndianness(
    __inout    DRM_TEE_CONTEXT      *f_pContext,
    __out      DRM_TEE_BYTE_BLOB    *f_pDest,
    __in const DRM_TEE_BYTE_BLOB    *f_pSource );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_AllocKeyAES128(
    __inout_opt                                     OEM_TEE_CONTEXT            *f_pContextAllowNULL,
    __in                                            DRM_TEE_KEY_TYPE            f_eType,
    __out                                           DRM_TEE_KEY                *f_pKey );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_AllocKeyECC256(
    __inout                                         OEM_TEE_CONTEXT            *f_pContext,
    __in                                            DRM_TEE_KEY_TYPE            f_eType,
    __out                                           DRM_TEE_KEY                *f_pKey );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_TEE_IMPL_SAMPLEPROT_IsDecryptionModeSupported(
    __inout                     OEM_TEE_CONTEXT              *f_pOemTeeCtx,
    __in                        DRM_DWORD                     f_dwMode );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_SAMPLEPROT_ApplySampleProtection(
    __inout                                         OEM_TEE_CONTEXT        *f_pContext,
    __in                                      const OEM_TEE_KEY            *f_pSPK,
    __in                                            DRM_UINT64              f_ui64Initializatiolwector,
    __in                                            DRM_DWORD               f_cbClearToSampleProtected,
    __inout_bcount( f_cbClearToSampleProtected )    DRM_BYTE               *f_pbClearToSampleProtected );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_GetAppSpecificHWID(
    __inout                                         OEM_TEE_CONTEXT            *f_pOemTeeCtx,
    __in                                      const DRM_ID                     *f_pidApplication,
    __out                                           DRM_ID                     *f_pidHWID )
GCC_ATTRIB_NO_STACK_PROTECT();

/* Unsafe for potential math overflow unless input is pre-verified */
#define DRM_AESCBC_ENCRYPTED_SIZE_W_IV_NOT_OVERFLOW_SAFE(cb) ( cb + 2 * DRM_AES_BLOCKLEN - cb % DRM_AES_BLOCKLEN )
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_AES128CBC(
    __inout                                                                      DRM_TEE_CONTEXT            *f_pContext,
    __in                                                                         DRM_DWORD                   f_cbBlob,
    __in_bcount( f_cbBlob )                                                const DRM_BYTE                   *f_pbBlob,
    __in                                                                   const DRM_TEE_KEY                *f_poKey,
    __inout                                                                      DRM_DWORD                  *f_pcbEncryptedBlob,
    __out_bcount( DRM_AESCBC_ENCRYPTED_SIZE_W_IV_NOT_OVERFLOW_SAFE( f_cbBlob ) ) DRM_BYTE                   *f_pbEncryptedBlob );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_ParseCertificateChain(
    __inout_opt                           OEM_TEE_CONTEXT                       *f_pOemTeeContext,
    __in_ecount_opt( 1 )            const DRMFILETIME                           *f_pftLwrrentTime,
    __in                                  DRM_DWORD                              f_dwExpectedLeafCertType,
    __in                                  DRM_DWORD                              f_cbCertData,
    __in_bcount( f_cbCertData )     const DRM_BYTE                              *f_pbCertData,
    __out                                 DRM_BCERTFORMAT_PARSER_CHAIN_DATA     *f_pChainData );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_ParseLicenseAlloc(
    __inout                           DRM_TEE_CONTEXT              *f_pContext,
    __in                              DRM_DWORD                     f_cbLicense,
    __in_ecount( f_cbLicense )  const DRM_BYTE                     *f_pbLicense,
    __inout                           DRM_TEE_XMR_LICENSE          *f_poLicense );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_GetVersionInformation(
    __inout                                               OEM_TEE_CONTEXT          *f_pContext,
    __out                                                 DRM_DWORD                *f_pchManufacturerName,
    __deref_out_ecount( *f_pchManufacturerName )          DRM_WCHAR               **f_ppwszManufacturerName,
    __out                                                 DRM_DWORD                *f_pchModelName,
    __deref_out_ecount( *f_pchModelName )                 DRM_WCHAR               **f_ppwszModelName,
    __out                                                 DRM_DWORD                *f_pchModelNumber,
    __deref_out_ecount( *f_pchModelNumber )               DRM_WCHAR               **f_ppwszModelNumber,
    __out_ecount_opt( DRM_TEE_METHOD_FUNCTION_MAP_COUNT ) DRM_DWORD                *f_pdwFunctionMap,
    __out_opt                                             DRM_DWORD                *f_pcbProperties,
    __deref_opt_out_bcount( *f_pcbProperties )            DRM_BYTE                **f_ppbProperties );

#define DRM_TEE_IMPL_BASE_ParsedLicenseFree( __pOemTeeCtx, __poLicense ) DRM_DO {                       \
    if( (__poLicense) != NULL )                                                                         \
    {                                                                                                   \
        ChkVOID( OEM_TEE_BASE_SelwreMemFree( (__pOemTeeCtx), (DRM_VOID**)&(__poLicense)->pbStack ) );   \
    }                                                                                                   \
} DRM_WHILE_FALSE

EXIT_PK_NAMESPACE;

#endif /* _DRMTEEBASE_H_ */

