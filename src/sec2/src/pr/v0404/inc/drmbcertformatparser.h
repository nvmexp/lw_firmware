/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __DRMBCERTFORMATPARSER_H__
#define __DRMBCERTFORMATPARSER_H__

#include <drmxbparser.h>
#include <drmbcertconstants.h>
#include <drmbcertformat_generated.h>
#include <oemtee.h>
#include <drmrevocationtypes.h>

ENTER_PK_NAMESPACE;

PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_POOR_DATA_ALIGNMENT_25021,"Ignore poor data alignment" );

typedef struct __tagDRM_BCERTFORMAT_VERIFICATIONRESULT {
    /*
    ** Number of the cert in the chain, zero-based with leaf cert as 0
    */
    DRM_DWORD   cCertNumber;
    /*
    ** Error code
    */
    DRM_RESULT  drResult;
} DRM_BCERTFORMAT_VERIFICATIONRESULT;

typedef enum __tagDRM_BCERTFORMAT_PARSER_CONTEXT_TYPE
{
    DRM_BCERTFORMAT_CONTEXT_TYPE_ILWALID = 0,
    DRM_BCERTFORMAT_CONTEXT_TYPE_FULL    = 1,
    DRM_BCERTFORMAT_CONTEXT_TYPE_BASE    = 2,
} DRM_BCERTFORMAT_PARSER_CONTEXT_TYPE;

typedef struct __tagDRM_BCERTFORMAT_PARSER_CONTEXT_BASE
{
    DRM_BCERTFORMAT_PARSER_CONTEXT_TYPE eContextType;
    DRM_BOOL                            fInitialized;
    DRMFILETIME                         ftLwrrentTime;
    DRM_DWORD                           dwExpectedLeafCertType;
    DRM_STACK_ALLOCATOR_CONTEXT        *pStack;
    OEM_TEE_CONTEXT                    *pOemTeeCtx;
} DRM_BCERTFORMAT_PARSER_CONTEXT_BASE;

#define DRM_BCERTFORMAT_PARSER_CONTEXT_BASE_EMPTY { DRM_BCERTFORMAT_CONTEXT_TYPE_ILWALID, 0, { 0, 0 }, 0, NULL, NULL }

typedef struct __tagDRM_BCERTFORMAT_PARSER_CONTEXT_FULL
{
    DRM_BCERTFORMAT_PARSER_CONTEXT_BASE oBaseCtx;
    DRM_BOOL                            fCheckSignature;
    DRM_DWORD                           iLwrrentCert;
    DRM_BOOL                            fRootPubKeySet;
    PUBKEY_P256                         oRootPubKey;
    DRM_BOOL                            fDontFailOnMissingExtData;
    DRM_DWORD                           dwRequiredKeyUsageMask;
    DRM_DWORD                           iResult;
    DRM_DWORD                           cResults;
    DRM_BCERTFORMAT_CERT                rgoCertCache[DRM_BCERT_MAX_CERTS_PER_CHAIN];
    DRM_BCERTFORMAT_VERIFICATIONRESULT *pResults;
} DRM_BCERTFORMAT_PARSER_CONTEXT_FULL;

#define DRM_BCERTFORMAT_PARSER_CONTEXT_BUFFER_SIZE sizeof( DRM_BCERTFORMAT_PARSER_CONTEXT_FULL )

typedef struct __tagDRM_BCERTFORMAT_PARSER_CONTEXT
{
    /*
    ** This data is Opaque.  Do not set any value in it.
    */
    DRM_BYTE rgbOpaqueBuffer[ DRM_BCERTFORMAT_PARSER_CONTEXT_BUFFER_SIZE ];
} DRM_BCERTFORMAT_PARSER_CONTEXT;

#define DRM_BCERTFORMAT_PARSER_CONTEXT_INTERNAL DRM_VOID

#define DRM_BCERTFORMAT_PARSER_CTX_IS_BASE( pCtx )          ((pCtx)->eContextType == DRM_BCERTFORMAT_CONTEXT_TYPE_BASE)
#define DRM_BCERTFORMAT_PARSER_CTX_IS_FULL( pCtx )          ((pCtx)->eContextType == DRM_BCERTFORMAT_CONTEXT_TYPE_FULL)
#define DRM_BCERTFORMAT_PARSER_CTX_TO_FULL( pCtx )          DRM_REINTERPRET_CAST( DRM_BCERTFORMAT_PARSER_CONTEXT_FULL, (pCtx) )
#define DRM_BCERTFORMAT_PARSER_CTX_TO_CONST_FULL( pCtx )    DRM_REINTERPRET_CONST_CAST( const DRM_BCERTFORMAT_PARSER_CONTEXT_FULL, (pCtx) )
#define DRM_BCERTFORMAT_PARSER_CTX_SIZE( pCtx )             (DRM_BCERTFORMAT_PARSER_CTX_IS_FULL( pCtx ) ? sizeof(DRM_BCERTFORMAT_PARSER_CONTEXT_FULL) : sizeof(DRM_BCERTFORMAT_PARSER_CONTEXT_BASE))


PREFAST_POP; /* __WARNING_POOR_DATA_ALIGNMENT_25021 */


typedef struct __tagDRM_BCERTFORMAT_MFG_STRING
{
    DRM_DWORD cbString;
    DRM_BYTE *pbString;
} DRM_BCERTFORMAT_MFG_STRING;

typedef enum __tagDRM_TEE_BCERTFORMAT_CHAIN_DATA_KEY
{
    DRM_BCERTFORMAT_CHAIN_DATA_KEY__SIGN                    = 0,
    DRM_BCERTFORMAT_CHAIN_DATA_KEY__ENCRYPT                 = 1,
    DRM_BCERTFORMAT_CHAIN_DATA_KEY__PRND_ENCRYPT_DEPRECATED = 2,
    DRM_BCERTFORMAT_CHAIN_DATA_KEY__SAMPLEPROT              = 3,
    DRM_BCERTFORMAT_CHAIN_DATA_KEY__SIGN_CRL                = 4,

    /* This must be the last item */
    DRM_BCERTFORMAT_CHAIN_DATA_KEY__COUNT                   = 5,
} DRM_TEE_BCERTFORMAT_CHAIN_DATA_KEY;

typedef struct __tagDRM_BCERTFORMAT_PARSER_CHAIN_DATA
{
    DRM_DWORD                  dwLeafSelwrityLevel;
    DRM_DWORD                  dwLeafFeatureSet;
    DRM_DWORD                  dwSelwrityVersion;
    DRM_DWORD                  dwPlatformID;
    DRM_DWORD                  cDigests;
    DRM_DWORD                  dwValidKeyMask;
    OEM_SHA256_DIGEST          rgoDigests[DRM_BCERT_MAX_CERTS_PER_CHAIN];
    PUBKEY_P256                rgoLeafPubKeys[DRM_BCERTFORMAT_CHAIN_DATA_KEY__COUNT];
    DRM_BCERTFORMAT_MFG_STRING oManufacturingName;
    DRM_BCERTFORMAT_MFG_STRING oManufacturingModel;
    DRM_BCERTFORMAT_MFG_STRING oManufacturingNumber;
} DRM_BCERTFORMAT_PARSER_CHAIN_DATA;

#define DRM_BCERTFORMAT_PARSER_CHAIN_DATA_KEY_BIT(dwKey)                 (1u << (dwKey))
#define DRM_BCERTFORMAT_PARSER_CHAIN_DATA_IS_KEY_VALID(dwKeyMask, dwKey) ( DRM_BCERTFORMAT_PARSER_CHAIN_DATA_KEY_BIT( dwKey ) & (dwKeyMask) )


#define DRM_BCERTFORMAT_CHKVERIFICATIONERR( pVerificationCtx, fCondition, dwErr ) DRM_DO {          \
    if( !(fCondition) )                                                                             \
    {                                                                                               \
        DRM_BCERTFORMAT_PARSER_CONTEXT_BASE *_pVCtx = (pVerificationCtx);                           \
        DRM_RESULT _drErr = (dwErr);                                                                \
        if( !DRM_BCERTFORMAT_PARSER_CTX_IS_FULL(_pVCtx ) )                                          \
        {                                                                                           \
            ChkDR( _drErr );                                                                        \
        }                                                                                           \
        else                                                                                        \
        {                                                                                           \
            DRM_BCERTFORMAT_PARSER_CONTEXT_FULL *_pVCtx2 =                                          \
                DRM_BCERTFORMAT_PARSER_CTX_TO_FULL(_pVCtx);                                         \
            ChkBOOL( _pVCtx2->iResult < _pVCtx2->cResults, _drErr );                                \
            _pVCtx2->pResults[_pVCtx2->iResult].cCertNumber = _pVCtx2->iLwrrentCert;                \
            _pVCtx2->pResults[_pVCtx2->iResult].drResult    = _drErr;                               \
            _pVCtx2->iResult = _pVCtx2->iResult + 1; /* Can't overflow: limited number of calls */  \
        }                                                                                           \
    }                                                                                               \
} DRM_WHILE_FALSE

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_OverrideRootPublicKey(
    __inout_ecount( 1 )                     DRM_BCERTFORMAT_PARSER_CONTEXT        *f_pParserCtx,
    __in_ecount_opt( 1 )              const PUBKEY_P256                           *f_pRootPubKey );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_InitializeParserContext(
    __in                                    DRM_BOOL                               f_fCheckSignature,
    __in                                    DRM_BOOL                               f_fDontFailOnMissingExtData,
    __in                                    DRM_DWORD                              f_cRequiredKeyUsages,
    __in_ecount_opt( f_cRequiredKeyUsages )
                                      const DRM_DWORD                             *f_pdwRequiredKeyUsages,
    __in                                    DRM_DWORD                              f_cResults,
    __in_ecount_opt( f_cResults )           DRM_BCERTFORMAT_VERIFICATIONRESULT    *f_pResults,
    __inout_ecount( 1 )                     DRM_BCERTFORMAT_PARSER_CONTEXT        *f_pParserCtx );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_GetCertificate(
    __inout_ecount( 1 )                     DRM_BCERTFORMAT_PARSER_CONTEXT        *f_pParserCtx,
    __in                              const DRM_BCERTFORMAT_CERT_HEADER           *f_pCertificateHeader,
    __out_ecount_opt( 1 )                   DRM_DWORD                             *f_pcbParsed,
    __out_ecount_opt( 1 )                   DRM_BCERTFORMAT_CERT                  *f_pCert );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_ParseCertificateChain(
    __inout_ecount( 1 )                     DRM_BCERTFORMAT_PARSER_CONTEXT        *f_pParserCtx,
    __inout_opt                             OEM_TEE_CONTEXT                       *f_pOemTeeContext,
    __in_ecount_opt( 1 )              const DRMFILETIME                           *f_pftLwrrentTime,
    __in                                    DRM_DWORD                              f_dwExpectedLeafCertType,
    __inout                                 DRM_STACK_ALLOCATOR_CONTEXT           *f_pStack,
    __in                                    DRM_DWORD                              f_cbCertData,
    __in_bcount( f_cbCertData )       const DRM_BYTE                              *f_pbCertData,
    __inout_ecount_opt( 1 )                 DRM_DWORD                             *f_cbParsed,
    __out_ecount( 1 )                       DRM_BCERTFORMAT_CHAIN                 *f_pChain,
    __out_ecount_opt( 1 )                   DRM_BCERTFORMAT_CERT                  *f_pLeafMostCert );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_VerifyCertificateChain(
    __inout_opt                             OEM_TEE_CONTEXT                       *f_pOemTeeContext,
    __inout_opt                             DRM_BCERTFORMAT_PARSER_CONTEXT        *f_pParserCtx,
    __in_ecount_opt( 1 )              const DRMFILETIME                           *f_pftLwrrentTime,
    __in                                    DRM_DWORD                              f_dwCertType,
    __in                                    DRM_DWORD                              f_dwMinSelwrityLevel,
    __in                                    DRM_DWORD                              f_cbCertData,
    __in_bcount( f_cbCertData )       const DRM_BYTE                              *f_pbCertData );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_GetPublicKeyByUsageFromChain(
    __inout_ecount( 1 )                     DRM_BCERTFORMAT_PARSER_CONTEXT        *f_pParserCtx,
    __in                              const DRM_BCERTFORMAT_CHAIN                 *f_pChain,
    __in                                    DRM_DWORD                              f_dwKeyUsage,
    __inout_ecount( 1 )                     PUBKEY_P256                           *f_pPubkey,
    __out_ecount_opt( 1 )                   DRM_DWORD                             *f_pdwKeyUsageSet,
    __inout_ecount_opt( 1 )                 DRM_BCERTFORMAT_CERT                  *f_pCert,
    __out_ecount_opt( 1 )                   DRM_DWORD                             *f_pdwCertKeyIndex );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_GetPublicKeyFromCert(
    __in_ecount( 1 )                  const DRM_BCERTFORMAT_CERT                  *f_pCert,
    __inout_ecount( 1 )                     PUBKEY_P256                           *f_pPubkey );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_GetPublicKeyByUsage(
    __inout_opt                             DRM_BCERTFORMAT_PARSER_CONTEXT        *f_pParserCtx,
    __inout_opt                             OEM_TEE_CONTEXT                       *f_pOemTeeContext,
    __in                              const DRM_DWORD                              f_cbCertData,
    __in_bcount(f_cbCertData)         const DRM_BYTE                              *f_pbCertData,
    __in                              const DRM_DWORD                              f_dwCertIndex,
    __in                              const DRM_DWORD                              f_dwKeyUsage,
    __out_ecount(1)                         PUBKEY_P256                           *f_pPubkey,
    __out_opt                               DRM_DWORD                             *f_pdwKeyUsageSet,
    __out_ecount_opt(1)                     DRM_DWORD                             *f_pdwCertKeyIndex );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_GetPublicKey(
    __inout_opt                             DRM_BCERTFORMAT_PARSER_CONTEXT        *f_pParserCtx,
    __inout_opt                             OEM_TEE_CONTEXT                       *f_pOemTeeContext,
    __in                              const DRM_DWORD                              f_cbCertData,
    __in_bcount(f_cbCertData)         const DRM_BYTE                              *f_pbCertData,
    __out_ecount(1)                         PUBKEY_P256                           *f_pPubkey );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_GetPublicKeyAndDomainRevision(
    _Inout_opt_                             DRM_BCERTFORMAT_PARSER_CONTEXT        *f_pParserCtx,
    _Inout_opt_                             OEM_TEE_CONTEXT                       *f_pOemTeeContext,
    _In_                              const DRM_DWORD                              f_cbCertData,
    _In_reads_( f_cbCertData )        const DRM_BYTE                              *f_pbCertData,
    _Out_writes_( 1 )                       PUBKEY_P256                           *f_pPubkey,
    _Out_writes_opt_( 1 )                   DRM_DWORD                             *f_pdwRevision );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_GetSelwrityLevel(
    __inout_opt                             DRM_BCERTFORMAT_PARSER_CONTEXT        *f_pParserCtx,
    __inout_opt                             OEM_TEE_CONTEXT                       *f_pOemTeeContext,
    __in                              const DRM_DWORD                              f_cbCertData,
    __in_bcount( f_cbCertData )       const DRM_BYTE                              *f_pbCertData,
    __out_ecount( 1 )                       DRM_DWORD                             *f_pdwSelwrityLevel );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_VerifyChildUsage(
    __in                              const DRM_DWORD                              f_dwChildKeyUsageMask,
    __in                              const DRM_DWORD                              f_dwParentKeyUsageMask );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_ParseCertificateChainData(
    __inout_opt                             OEM_TEE_CONTEXT                       *f_pOemTeeContext,
    __in_opt                          const DRMFILETIME                           *f_pftLwrrentTime,
    __in                                    DRM_DWORD                              f_dwExpectedLeafCertType,
    __inout                                 DRM_STACK_ALLOCATOR_CONTEXT           *f_pStack,
    __in                                    DRM_DWORD                              f_cbCertData,
    __in_bcount( f_cbCertData )       const DRM_BYTE                              *f_pbCertData,
    __out                                   DRM_BCERTFORMAT_PARSER_CHAIN_DATA     *f_poChainData,
    __in                                    DRM_BOOL                               f_fForceSkipSignatureCheck );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_VerifySignature(
    __in                              const DRM_BCERTFORMAT_CERT                  *f_pCertificate,
    __inout_opt                             OEM_TEE_CONTEXT                       *f_pOemTeeContext );

DRM_NO_INLINE DRM_API_VOID DRM_DWORD DRM_BCERTFORMAT_GetResultCount( __in DRM_BCERTFORMAT_PARSER_CONTEXT *f_pCertParserCtx );

EXIT_PK_NAMESPACE;

#endif /* __DRMBCERTFORMATPARSER_H__ */
