/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <drmteexb.h>
#include <drmteekbcryptodata.h>
#include <drmxbparser.h>
#include <drmxbbuilder.h>
#include <drmmathsafe.h>
#include <drmstkalloc.h>
#include <oemtee.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

#include <drmteexb_generated.c>

#define DRM_TEE_XB_LWRRENT_VERSION              1
#define DRM_TEE_XB_CRYPTO_BLOB_LWRRENT_VERSION  1
#if defined (SEC_COMPILE)
/* The signature buffer is offset some bytes in from the object */
                                                     /* Base offset          wSignatureType     xbbaSignature.cbData (with XB_SIZE_RELATIVE_WORD) */
static const DRM_DWORD c_iSignatureFromObjectStart = XB_BASE_OBJECT_LENGTH + sizeof(DRM_WORD) + sizeof(DRM_WORD);

static DRM_ALWAYS_INLINE DRM_RESULT DRM_CALL _RemapTEEFormatError( DRM_RESULT f_dr );
static DRM_ALWAYS_INLINE DRM_RESULT DRM_CALL _RemapTEEFormatError( DRM_RESULT f_dr )
{
    DRM_RESULT dr = DRM_SUCCESS;
    switch( f_dr )
    {
    case DRM_E_XB_OBJECT_NOTFOUND:
    case DRM_E_XB_ILWALID_OBJECT:
    case DRM_E_XB_OBJECT_ALREADY_EXISTS:
    case DRM_E_XB_REQUIRED_OBJECT_MISSING:
    case DRM_E_XB_UNKNOWN_ELEMENT_TYPE:
    case DRM_E_XB_ILWALID_VERSION:
        DRMASSERT( FALSE );
        dr = DRM_E_TEE_ILWALID_KEY_DATA;
        break;
    default:
        dr = f_dr;
        break;
    }
    return dr;
}

static DRM_FRE_INLINE DRM_RESULT DRM_CALL _KBWithSignature_Start(
    __inout                               DRM_TEE_CONTEXT                               *f_pContext,
    __inout                               DRM_STACK_ALLOCATOR_CONTEXT                   *f_pStack,
    __inout_ecount( 1 )                   DRM_XB_BUILDER_CONTEXT                        *f_pcontextBuilder,
    __in                                  DRM_WORD                                       f_wSignatureObjectType,
    __in                            const DRM_XB_FORMAT_DESCRIPTION                     *f_pFormat );
static DRM_FRE_INLINE DRM_RESULT DRM_CALL _KBWithSignature_Start(
    __inout                               DRM_TEE_CONTEXT                               *f_pContext,
    __inout                               DRM_STACK_ALLOCATOR_CONTEXT                   *f_pStack,
    __inout_ecount( 1 )                   DRM_XB_BUILDER_CONTEXT                        *f_pcontextBuilder,
    __in                                  DRM_WORD                                       f_wSignatureObjectType,
    __in                            const DRM_XB_FORMAT_DESCRIPTION                     *f_pFormat )
{
    DRM_RESULT               dr     = DRM_SUCCESS;
    DRM_TEE_XB_KB_SIGNATURE *pXBSig = NULL;
    DRM_BYTE                *pbSig  = NULL;

    DRMASSERT( f_pContext        != NULL );
    DRMASSERT( f_pStack          != NULL );
    DRMASSERT( f_pcontextBuilder != NULL );
    DRMASSERT( f_pFormat         != NULL );

    ChkDR( DRM_STK_Alloc( f_pStack, sizeof( DRM_TEE_XB_KB_SIGNATURE ), ( DRM_VOID ** )&pXBSig ) );
    ChkDR( DRM_STK_Alloc( f_pStack, DRM_AES128OMAC1_SIZE_IN_BYTES    , ( DRM_VOID ** )&pbSig  ) );
    ChkDR( DRM_XB_StartFormatFromStack( f_pStack, DRM_TEE_XB_LWRRENT_VERSION, f_pcontextBuilder, f_pFormat ) );

    pXBSig->wSignatureType             = OEM_TEE_XB_SIGNATURE_TYPE_AES_OMAC1_TKD;
    pXBSig->xbbaSignature.cbData       = DRM_AES128OMAC1_SIZE_IN_BYTES;
    pXBSig->xbbaSignature.pbDataBuffer = pbSig;
    pXBSig->xbbaSignature.iData        = 0;

    ChkDR( DRM_XB_AddEntry( f_pcontextBuilder, f_wSignatureObjectType, pXBSig ) );

ErrorExit:
    return dr;
}

static DRM_FRE_INLINE DRM_RESULT DRM_CALL _KBWithSignature_Finish(
    __inout                               DRM_TEE_CONTEXT                               *f_pContext,
    __in_ecount( 1 )                const DRM_XB_BUILDER_CONTEXT                        *f_pcontextBuilder,
    __in                                  DRM_WORD                                       f_wSignatureObjectType,
    __in                                  DRM_TEE_XB_KB_TYPE                             f_eKBType,
    __in                            const DRM_XB_FORMAT_DESCRIPTION                     *f_pFormat,
    __inout_bcount_opt( *f_pcbKB )        DRM_BYTE                                      *f_pbKB,
    __inout                               DRM_DWORD                                     *f_pcbKB,
    __in_opt                        const DRM_BOOL                                      *f_pfPersist )
GCC_ATTRIB_NO_STACK_PROTECT();
static DRM_FRE_INLINE DRM_RESULT DRM_CALL _KBWithSignature_Finish(
    __inout                               DRM_TEE_CONTEXT                               *f_pContext,
    __in_ecount( 1 )                const DRM_XB_BUILDER_CONTEXT                        *f_pcontextBuilder,
    __in                                  DRM_WORD                                       f_wSignatureObjectType,
    __in                                  DRM_TEE_XB_KB_TYPE                             f_eKBType,
    __in                            const DRM_XB_FORMAT_DESCRIPTION                     *f_pFormat,
    __inout_bcount_opt( *f_pcbKB )        DRM_BYTE                                      *f_pbKB,
    __inout                               DRM_DWORD                                     *f_pcbKB,
    __in_opt                        const DRM_BOOL                                      *f_pfPersist )
{
    DRM_RESULT       dr                    = DRM_SUCCESS;
    DRM_DWORD        iSignatureObject      = 0;
    DRM_DWORD        cbSignatureObject     = 0;
    DRM_DWORD        cbKBWithoutSignature  = 0;
    DRM_DWORD        iSignature            = 0;
    DRM_BYTE        *pbSig                 = NULL;
    DRM_TEE_KEY      oTKD                  = DRM_TEE_KEY_EMPTY;
    OEM_TEE_CONTEXT *pOemTeeCtx            = NULL;

    DRMASSERT( f_pContext        != NULL );
    DRMASSERT( f_pcontextBuilder != NULL );
    DRMASSERT( f_pFormat         != NULL );
    DRMASSERT( f_pcbKB           != NULL );

    pOemTeeCtx = &f_pContext->oContext;

    ChkDR( DRM_XB_FinishFormat( f_pcontextBuilder, f_pbKB, f_pcbKB ) );
    __analysis_assume( f_pbKB != NULL );    /* DRM_XB_FinishFormat fails if f_pbKB == NULL */

    ChkDR( DRM_XB_FindObject( f_wSignatureObjectType, f_pFormat, f_pbKB, *f_pcbKB, &iSignatureObject, &cbSignatureObject ) );

    ChkDR( DRM_DWordSub( *f_pcbKB, cbSignatureObject, &cbKBWithoutSignature ) );
    ChkDR( DRM_DWordAdd( iSignatureObject, c_iSignatureFromObjectStart, &iSignature ) );
    /* 8-byte align */
    ChkDR( DRM_DWordAddSame( &iSignature, 0x7 ) );
    iSignature &= ~(DRM_DWORD)0x7;

    ChkDR( DRM_DWordPtrAdd( (DRM_DWORD_PTR)f_pbKB, iSignature, (DRM_DWORD_PTR*)&pbSig ) );

    ChkDR( DRM_TEE_BASE_IMPL_DeriveTKDFromTK( f_pContext, NULL, f_eKBType, DRM_TEE_XB_KB_OPERATION_SIGN, f_pfPersist, &oTKD ) );
    ChkDR( OEM_TEE_BASE_SignWithOMAC1( pOemTeeCtx, &oTKD.oKey, cbKBWithoutSignature, f_pbKB, pbSig ) );

    DRMASSERT( iSignatureObject + cbSignatureObject == *f_pcbKB );

ErrorExit:
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &oTKD.oKey ) );
    return dr;
}

static DRM_FRE_INLINE DRM_RESULT DRM_CALL _KBWithSignature_Parse(
    __inout                                         DRM_TEE_CONTEXT                              *f_pContext,
    __in_bcount( f_cbKB )                     const DRM_BYTE                                     *f_pbKB,
    __in                                            DRM_DWORD                                     f_cbKB,
    __in                                            DRM_BOOL                                      f_fReconstituted,
    __in                                            DRM_WORD                                      f_wSignatureObjectType,
    __in                                            DRM_TEE_XB_KB_TYPE                            f_eKBType,
    __in                                      const DRM_XB_FORMAT_DESCRIPTION                    *f_pFormat,
    __out                                           DRM_DWORD                                    *f_pcbCryptoDataWeakRef,
    __deref_out_bcount( *f_pcbCryptoDataWeakRef )   DRM_BYTE                                    **f_ppbCryptoDataWeakRef,
    __out_opt                                       DRM_DWORD                                    *f_pdwidTK );
static DRM_FRE_INLINE DRM_RESULT DRM_CALL _KBWithSignature_Parse(
    __inout                                         DRM_TEE_CONTEXT                              *f_pContext,
    __in_bcount( f_cbKB )                     const DRM_BYTE                                     *f_pbKB,
    __in                                            DRM_DWORD                                     f_cbKB,
    __in                                            DRM_BOOL                                      f_fReconstituted,
    __in                                            DRM_WORD                                      f_wSignatureObjectType,
    __in                                            DRM_TEE_XB_KB_TYPE                            f_eKBType,
    __in                                      const DRM_XB_FORMAT_DESCRIPTION                    *f_pFormat,
    __out                                           DRM_DWORD                                    *f_pcbCryptoDataWeakRef,
    __deref_out_bcount( *f_pcbCryptoDataWeakRef )   DRM_BYTE                                    **f_ppbCryptoDataWeakRef,
    __out_opt                                       DRM_DWORD                                    *f_pdwidTK )
{
    DRM_RESULT                      dr                        = DRM_SUCCESS;
    DRM_TEE_XB_KB                   oxbKB                     = { 0 };
    DRM_STACK_ALLOCATOR_CONTEXT     stack;                    /* Initialized by DRM_STK_Init */
    DRM_DWORD                       dwVersionFound            = 0;
    DRM_DWORD                       iSignatureObject          = 0;
    DRM_DWORD                       cbSignatureObject         = 0;
    DRM_DWORD                       cbKBWithoutSignature      = 0;
    const DRM_DWORD                 c_cFormat                 = 1;  /* f_pFormat always points to a single format definition */
    DRM_DWORD                       cbCryptoDataWeakRef       = 0;
    DRM_BYTE                       *pbCryptoDataWeakRef       = NULL;
    DRM_TEE_KEY                     oTKD                      = DRM_TEE_KEY_EMPTY;
    DRM_DWORD                       dwidTK                    = 0;
    OEM_TEE_CONTEXT                *pOemTeeCtx                = NULL;
    DRM_TEE_KEY                     oTKToFree                 = DRM_TEE_KEY_EMPTY;
    DRM_BOOL                        fPersistentSignature      = FALSE;
    DRM_BYTE                        rgbUnused[ sizeof( DRM_DWORD_PTR ) ];

    DRMASSERT( f_pContext      != NULL );
    DRMASSERT( f_pbKB          != NULL );
    DRMASSERT( f_pFormat       != NULL );

    ChkArg( f_pcbCryptoDataWeakRef != NULL );
    ChkArg( f_ppbCryptoDataWeakRef != NULL );

    if( f_fReconstituted )
    {
        DRMASSERT( f_eKBType == DRM_TEE_XB_KB_TYPE_CDKB );
    }
    else
    {
        pOemTeeCtx = &f_pContext->oContext;
    }

    /*
    ** KBs have no data that requires allocation during parsing.
    ** Since KBs are opaque outside the TEE, a newer KB that could
    ** require allocation is invalid if passed into an older TEE.
    */
    ChkDR( DRM_STK_Init( &stack, rgbUnused, sizeof( rgbUnused ), TRUE ) );

    dr = DRM_XB_UnpackBinary( f_pbKB, f_cbKB, &stack, f_pFormat, c_cFormat, &dwVersionFound, NULL, &oxbKB );
    if( dr == DRM_E_OUTOFMEMORY )
    {
        /* Since allocation failed and no allocation is required, this must be invalid XBinary. */
        DRMASSERT( FALSE );
        dr = DRM_E_XB_ILWALID_OBJECT;
    }
    ChkDR( dr );

    ChkBOOL( dwVersionFound                 == DRM_TEE_XB_LWRRENT_VERSION,              DRM_E_TEE_ILWALID_KEY_DATA );
    ChkBOOL( oxbKB.CryptoData.wVersion      == DRM_TEE_XB_CRYPTO_BLOB_LWRRENT_VERSION,  DRM_E_TEE_ILWALID_KEY_DATA );
    ChkBOOL( oxbKB.CryptoData.wKeyBlobType  == f_eKBType,                               DRM_E_TEE_ILWALID_KEY_DATA );
    ChkBOOL( oxbKB.Signature.wSignatureType == OEM_TEE_XB_SIGNATURE_TYPE_AES_OMAC1_TKD, DRM_E_TEE_ILWALID_KEY_DATA );

    ChkDR( DRM_XB_FindObject( f_wSignatureObjectType, f_pFormat, f_pbKB, f_cbKB, &iSignatureObject, &cbSignatureObject ) );

    cbKBWithoutSignature = iSignatureObject;

    pbCryptoDataWeakRef = XBBA_TO_PB( oxbKB.CryptoData.xbbaPayload );
    cbCryptoDataWeakRef = XBBA_TO_CB( oxbKB.CryptoData.xbbaPayload );
    ChkArg( pbCryptoDataWeakRef != NULL );

    if( f_pdwidTK == NULL )
    {
        f_pdwidTK = &dwidTK;
    }

    switch( f_eKBType )
    {
        case DRM_TEE_XB_KB_TYPE_LKB:
            ChkArg( cbCryptoDataWeakRef == sizeof( DRM_TEE_KB_LKBData ) );
            if( ( ( const DRM_TEE_KB_LKBData * )pbCryptoDataWeakRef )->fPersist )
            {
                *f_pdwidTK = ( ( const DRM_TEE_KB_LKBData * )pbCryptoDataWeakRef)->dwidTK;
                fPersistentSignature = TRUE;
            }
            break;
        case DRM_TEE_XB_KB_TYPE_CDKB:
            /*
            ** CDKB is a special blob since it can be used w/ empty TEE context when it is self-contained.
            ** In such case the signature has to be persistable. The enforcement of transience happens
            ** through contents of crypto blob.
            */
            fPersistentSignature = TRUE;
            break;
        case DRM_TEE_XB_KB_TYPE_LPKB:
            /*
            ** LPKB contains the pre-generated device (leaf) certificate's ECC private keys
            ** which are constant until the device gets a TEE update
            ** (which would contain a new root TEE key and new ECC keys).
            */
            fPersistentSignature = TRUE;
            break;
        case DRM_TEE_XB_KB_TYPE_PPKB:
            fPersistentSignature = TRUE;
            ChkArg( cbCryptoDataWeakRef == sizeof( DRM_TEE_KB_PPKBData ) );
            *f_pdwidTK = ( ( const DRM_TEE_KB_PPKBData * )pbCryptoDataWeakRef )->dwidTK;
            break;
        case DRM_TEE_XB_KB_TYPE_DKB:
            fPersistentSignature = TRUE;
            ChkArg( cbCryptoDataWeakRef == sizeof( DRM_TEE_KB_DKBData ) );
            *f_pdwidTK = ( ( const DRM_TEE_KB_DKBData * )pbCryptoDataWeakRef )->dwidTK;
            break;
        case DRM_TEE_XB_KB_TYPE_TPKB : __fallthrough;
        case DRM_TEE_XB_KB_TYPE_MRKB : __fallthrough;
        case DRM_TEE_XB_KB_TYPE_MTKB : __fallthrough;
        case DRM_TEE_XB_KB_TYPE_CEKB : __fallthrough;
        case DRM_TEE_XB_KB_TYPE_RKB  : __fallthrough;
        case DRM_TEE_XB_KB_TYPE_SPKB : __fallthrough;
        case DRM_TEE_XB_KB_TYPE_RPCKB: __fallthrough;
        case DRM_TEE_XB_KB_TYPE_NKB  : __fallthrough;
            fPersistentSignature = FALSE;
            break;
        default:
            DRMASSERT( FALSE );
            break;
    }

    if ( !fPersistentSignature || f_eKBType == DRM_TEE_XB_KB_TYPE_CDKB )
    {
        ChkDR( OEM_TEE_BASE_GetCTKID( pOemTeeCtx, f_pdwidTK ) );
    }

    /* if type is strongly non_persistent, DRM_ID identifying TK is always the first 16 bytes of XBinary Crypto Data Object's Payload */
    ChkDR( DRM_TEE_BASE_IMPL_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_TK, &oTKToFree ) );

    ChkDR( OEM_TEE_BASE_GetTKByID( pOemTeeCtx, *f_pdwidTK, &oTKToFree.oKey ) );

    ChkDR( DRM_TEE_BASE_IMPL_DeriveTKDFromTK( f_pContext, &oTKToFree, f_eKBType, DRM_TEE_XB_KB_OPERATION_SIGN, &fPersistentSignature, &oTKD ) );
    dr = OEM_TEE_BASE_VerifyOMAC1Signature(
        pOemTeeCtx,
        &oTKD.oKey,
        cbKBWithoutSignature,
        f_pbKB,
        oxbKB.Signature.xbbaSignature.cbData,
        XBBA_TO_PB(oxbKB.Signature.xbbaSignature) );
    if( ( f_eKBType == DRM_TEE_XB_KB_TYPE_PPKB ) && DRM_FAILED( dr ) )
    {
        dr = DRM_E_TEE_ROOT_KEY_CHANGED;
    }
    ChkDR( dr );

    *f_pcbCryptoDataWeakRef = cbCryptoDataWeakRef;
    *f_ppbCryptoDataWeakRef = pbCryptoDataWeakRef;

ErrorExit:
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &oTKToFree.oKey ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &oTKD.oKey ) );
    return dr;
}

/* Populates f_pKB only on success */
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_Build(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                                  DRM_TEE_XB_KB_TYPE          f_eKBType,
    __in                                                  DRM_DWORD                   f_cbKBCryptoData,
    __in_bcount( f_cbKBCryptoData )                 const DRM_BYTE                   *f_pbKBCryptoData,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_pKB )
{
    DRM_RESULT                   dr                   = DRM_SUCCESS;
    DRM_BYTE                    *pbStack              = NULL;
    DRM_DWORD                    cbStack              = DRM_TEE_KB_STACK_SIZE;
    DRM_XB_BUILDER_CONTEXT       oXBBuilder           = {{0}};  // LWE (nkuo) - changed due to compile error "missing braces around initializer"
    DRM_TEE_XB_KB_CRYPTO_DATA    oXBKBCryptoData      = {0};
    DRM_BYTE                    *pbKB                 = NULL;
    DRM_DWORD                    cbKB                 = 0;
    OEM_TEE_CONTEXT             *pOemTeeCtx           = NULL;
    DRM_BOOL                     fPersistentSignature = FALSE;
    DRM_STACK_ALLOCATOR_CONTEXT  oStack;              /* Initialized by DRM_STK_Init */

    DRMASSERT( f_pContext != NULL );
    DRMASSERT( f_pKB      != NULL );
    ChkVOID( OEM_TEE_ZERO_MEMORY( f_pKB, sizeof( *f_pKB ) ) );

    pOemTeeCtx = &f_pContext->oContext;

    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, cbStack, (DRM_VOID**)&pbStack ) );
    ChkDR( DRM_STK_Init( &oStack, pbStack, cbStack, TRUE ) );

    ChkDR( _KBWithSignature_Start(
        f_pContext,
        &oStack,
        &oXBBuilder,
        DRM_TEE_XB_KB_SIGNATURE_ENTRY_TYPE,
        s_DRM_TEE_XB_KB_FormatDescription ) );

    oXBKBCryptoData.fValid                   = TRUE;
    oXBKBCryptoData.wVersion                 = DRM_TEE_XB_CRYPTO_BLOB_LWRRENT_VERSION;
    oXBKBCryptoData.wKeyBlobType             = (DRM_WORD)f_eKBType;
    oXBKBCryptoData.xbbaPayload.cbData       = f_cbKBCryptoData;
    oXBKBCryptoData.xbbaPayload.pbDataBuffer = ( DRM_BYTE * )f_pbKBCryptoData;
    oXBKBCryptoData.xbbaPayload.iData        = 0;
    ChkDR( DRM_XB_AddEntry( &oXBBuilder, DRM_TEE_XB_KB_CRYPTO_DATA_ENTRY_TYPE, &oXBKBCryptoData ) );

    if ( DRM_TEE_XB_KB_TYPE_LKB == f_eKBType )
    {
        ChkArg( f_cbKBCryptoData == sizeof( DRM_TEE_KB_LKBData ) );
        /* for blobs that can be transient or not the first byte of crypto data describes the transience */
        fPersistentSignature = ( ( DRM_TEE_KB_LKBData * )f_pbKBCryptoData )->fPersist;
    }
    else if ( DRM_TEE_XB_KB_TYPE_CDKB == f_eKBType )
    {
        /*
        ** for DRM_TEE_XB_KB_TYPE_CDKB the transience is enforced during parsing of the crypto blob and not during signature checking.
        ** this is because of the reconstitution scenario.
        */
        fPersistentSignature = TRUE;
    }
    else
    {
        fPersistentSignature = DRM_TEE_BASE_IMPL_IsKBTypePersistedToDisk( f_eKBType );
    }

    dr = _KBWithSignature_Finish(
        f_pContext,
        &oXBBuilder,
        DRM_TEE_XB_KB_SIGNATURE_ENTRY_TYPE,
        f_eKBType,
        s_DRM_TEE_XB_KB_FormatDescription,
        NULL,
        &cbKB,
        &fPersistentSignature );
    DRM_REQUIRE_BUFFER_TOO_SMALL( dr );

    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, cbKB, (DRM_VOID**)&pbKB ) );

    ChkDR( _KBWithSignature_Finish(
        f_pContext,
        &oXBBuilder,
        DRM_TEE_XB_KB_SIGNATURE_ENTRY_TYPE,
        f_eKBType,
        s_DRM_TEE_XB_KB_FormatDescription,
        pbKB,
        &cbKB,
        &fPersistentSignature ) );

    ChkDR( DRM_TEE_BASE_IMPL_AllocBlobAndTakeOwnership( f_pContext, cbKB, &pbKB, f_pKB ) );

ErrorExit:
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&pbStack ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&pbKB ) );
    return _RemapTEEFormatError( dr );
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_ParseAndVerifyWithReconstitution(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pKB,
    __in                                                  DRM_TEE_XB_KB_TYPE          f_eType,
    __in_opt                                        const DRM_BOOL                   *f_pfReconstituted,
    __out                                                 DRM_DWORD                  *f_pcbCryptoDataWeakRef,
    __deref_out_bcount( *f_pcbCryptoDataWeakRef )         DRM_BYTE                  **f_ppbCryptoDataWeakRef )
{
    DRM_RESULT         dr                  = DRM_SUCCESS;
    DRM_DWORD          cbCryptoDataWeakRef = 0;
    DRM_BYTE          *pbCryptoDataWeakRef = NULL;

    /* Arguments are checked in the PRITEE proxy layer */
    DRMASSERT( f_pContext             != NULL );
    DRMASSERT( f_pKB                  != NULL );
    DRMASSERT( f_pcbCryptoDataWeakRef != NULL );
    DRMASSERT( f_ppbCryptoDataWeakRef != NULL );

    ChkBOOL( f_pKB->cb > 0, DRM_E_TEE_ILWALID_KEY_DATA );

    ChkDR( _KBWithSignature_Parse(
        f_pContext,
        f_pKB->pb,
        f_pKB->cb,
        f_pfReconstituted != NULL ? *f_pfReconstituted : FALSE,
        DRM_TEE_XB_KB_SIGNATURE_ENTRY_TYPE,
        f_eType,
        s_DRM_TEE_XB_KB_FormatDescription,
        &cbCryptoDataWeakRef,
        &pbCryptoDataWeakRef,
        NULL ) );

    *f_pcbCryptoDataWeakRef = cbCryptoDataWeakRef;
    *f_ppbCryptoDataWeakRef = pbCryptoDataWeakRef;

ErrorExit:
    return _RemapTEEFormatError( dr );
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_KB_ParseAndVerify(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pKB,
    __in                                                  DRM_TEE_XB_KB_TYPE          f_eType,
    __out                                                 DRM_DWORD                  *f_pcbCryptoDataWeakRef,
    __deref_out_bcount( *f_pcbCryptoDataWeakRef )         DRM_BYTE                  **f_ppbCryptoDataWeakRef )
{
    return DRM_TEE_KB_ParseAndVerifyWithReconstitution(
        f_pContext,
        f_pKB,
        f_eType,
        NULL,
        f_pcbCryptoDataWeakRef,
        f_ppbCryptoDataWeakRef );
}
#endif
EXIT_PK_NAMESPACE_CODE;

