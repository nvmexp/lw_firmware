/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <drmteestubcommon.h>
#include <oemtee.h>
#include <drmtee.h>
#include <drmxbparser.h>
#include <drmteecache.h>
#include <drmteestub.h>
#include <drmteeproxystubcommon.h>
#include <drmteestubcommon_generated.h>
#include <drmmathsafe.h>
#include <drmteeproxyformat.h>

ENTER_PK_NAMESPACE_CODE;
#if defined (SEC_COMPILE)

// LWE (nkuo) - changed due to compile error "missing braces around initializer"
static const DRM_ID g_idKeyDerivationSignContext    = {{ 0x45, 0x42, 0x58, 0xc0, 0x2e, 0x93, 0x42, 0x30, 0x95, 0x59, 0x0e, 0xe6, 0xbb, 0xd7, 0x2b, 0x84 }};
static const DRM_ID g_idKeyDerivationEncryptContext = {{ 0xf6, 0xc9, 0x3b, 0x65, 0x9c, 0x0d, 0x48, 0xd7, 0xac, 0x3f, 0x5b, 0x6e, 0x01, 0x66, 0x6f, 0x81 }};

PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_READ_OVERRUN_6385, "Prefast Noise: Warning triggered on a Union, the type is checked before usage")
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_STUB_InitParameters(
    __inout_ecount( MAX_DRM_TEE_PROXY_PARAMETER_COUNT )     DRM_TEE_PROXY_PARAMETERS        *f_pParameters )
{
    DRM_RESULT dr  = DRM_SUCCESS;
    DRM_DWORD  idx = 0;

    AssertChkArg( f_pParameters != NULL );

    ChkVOID( OEM_TEE_ZERO_MEMORY( f_pParameters, sizeof( DRM_TEE_PROXY_PARAMETERS ) * MAX_DRM_TEE_PROXY_PARAMETER_COUNT ) );

    for( ; idx < MAX_DRM_TEE_PROXY_PARAMETER_COUNT; idx++ )
    {
        f_pParameters[idx].eParameterType =  DRM_TEE_PROXY_PARAMETER_TYPE__ILWALID;
    }

ErrorExit:
    return dr;
}

//
// LWE (kwilson)  In Microsoft PK code, DRM_TEE_STUB_FreeParameters returns DRM_VOID.
// Lwpu had to change the return value to DRM_RESULT in order to support a return
// code from acquiring/releasing the critical section, which may not always succeed.
//
DRM_NO_INLINE DRM_RESULT DRM_CALL DRM_TEE_STUB_FreeParameters(
    __inout_opt                                       DRM_TEE_CONTEXT                 *f_pTEEContext,
    __inout_ecount(MAX_DRM_TEE_PROXY_PARAMETER_COUNT) DRM_TEE_PROXY_PARAMETERS        *f_pParameters )
{
    DRM_DWORD idx = 0;
    DRM_RESULT dr = DRM_SUCCESS;

    DRMASSERT( f_pParameters != NULL );

    for( ; idx < MAX_DRM_TEE_PROXY_PARAMETER_COUNT; idx++ )
    {
        if( f_pParameters[idx].fIsOutputParameter )
        {
            if( f_pParameters[idx].eParameterType == DRM_TEE_PROXY_PARAMETER_TYPE__DRM_TEE_BYTE_BLOB )
            {
                /*
                ** For handle backed memory, we only want to free the handle after serializing the response.
                ** The actual memory backed by the handle should stay active until it is freed specifically
                ** with a call to OEM_TEE_BASE_SelwreMemHandleFree or DRM_TEE_BASE_FreeBlob.  This should only
                ** occur when the handled backed memory will no longer be used.
                */
                if(    f_pParameters[ idx ].uParameterValue.blobValue.eType     == DRM_TEE_BYTE_BLOB_TYPE_CCD
                    && f_pParameters[ idx ].uParameterValue.blobValue.dwSubType == OEM_TEE_AES128CTR_DECRYPTION_MODE_HANDLE )
                {
                    f_pParameters[ idx ].uParameterValue.blobValue.eType = DRM_TEE_BYTE_BLOB_TYPE_USER_MODE;
                }
                ChkDR( DRM_TEE_BASE_FreeBlob( f_pTEEContext, &f_pParameters[idx].uParameterValue.blobValue ) );
            }
            else if( f_pParameters[idx].eParameterType == DRM_TEE_PROXY_PARAMETER_TYPE__DRM_TEE_DWORDLIST )
            {
                ChkVOID( DRM_TEE_BASE_MemFree( f_pTEEContext, ( DRM_VOID ** )&f_pParameters[idx].uParameterValue.dwlValue.pdwData ) );
            }
            else if( f_pParameters[ idx ].eParameterType == DRM_TEE_PROXY_PARAMETER_TYPE__DRM_TEE_QWORDLIST )
            {
                ChkVOID( DRM_TEE_BASE_MemFree( f_pTEEContext, (DRM_VOID **)&f_pParameters[ idx ].uParameterValue.qwlValue.pqwData ) );
            }
        }
    }
ErrorExit:
    return dr;
}
PREFAST_POP /* __WARNING_READ_OVERRUN_6385 */

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_STUB_GenerateOutput(
    __in_opt                                             DRM_TEE_CONTEXT                            *f_pTeeCtx,
    __in_ecount( MAX_DRM_TEE_PROXY_PARAMETER_COUNT ) const DRM_TEE_PROXY_PARAMETERS                   *f_pParameters,
    __in                                                 DRM_RESULT                                  f_drResult,
    __in                                                 DRM_DWORD                                   f_cbStack,
    __inout_bcount( f_cbStack )                          DRM_BYTE                                   *f_pbStack,
    __in                                           const XB_DRM_TEE_PROXY_METHOD_REQ                *f_pReq,
    __inout                                              XB_DRM_TEE_PROXY_METHOD_REQ                *f_pResp,
    __inout                                              DRM_DWORD                                  *f_pcbResponseMessage,
    __inout_bcount( *f_pcbResponseMessage )              DRM_BYTE                                   *f_pbResponseMessage )
{
    DRM_RESULT                    dr                     = DRM_SUCCESS;
    DRM_XB_BUILDER_CONTEXT        xbContext              = {{ 0 }};  // LWE (nkuo) - changed due to compile error "missing braces around initializer"
    DRM_DWORD                     idx                    = 0;
    XB_DRM_DWORDLIST_PARAMS       rgXBDWLArgs[ MAX_DRM_TEE_PROXY_DWORDLIST ] = {{ 0 }};
    XB_DRM_QWORDLIST_PARAMS       rgXBQWLArgs[ MAX_DRM_TEE_PROXY_QWORDLIST ] = {{ 0 }};
    XB_DRM_TEE_BYTE_BLOB_PARAMS   rgXBlobArgs[ MAX_DRM_TEE_PROXY_BLOBS ]     = {{ 0 }};
    XB_DRM_ID_PARAMS              rgXBIdArgs[ MAX_DRM_TEE_PROXY_IDS ]       = {{ 0 }};
    XB_DRM_DWORD_PARAMS           rgXBDwordArgs[ MAX_DRM_TEE_PROXY_DWORDS ]    = {{ 0 }};
    XB_DRM_QWORD_PARAMS           rgXBQwordArgs[ MAX_DRM_TEE_PROXY_QWORDS ]    = {{ 0 }};

    ChkArg( f_pParameters != NULL );
    ChkArg( f_pbStack != NULL );
    ChkArg( f_cbStack > 0 );
    ChkArg( f_pReq != NULL );
    ChkArg( f_pResp != NULL );
    ChkArg( f_pcbResponseMessage != NULL );
    ChkArg( f_pbResponseMessage != NULL );

    if( f_pReq->MethodInfo.dwMethodID == DRM_TEE_PROXY_METHOD_ID( DRM_TEE_BASE_AllocTEEContext )
        && DRM_SUCCEEDED( f_drResult ) )
    {
        ChkDR( DRM_TEE_CACHE_Initialize() );
        ChkDR( DRM_TEE_CACHE_AddContext( f_pTeeCtx ) );
    }

    /* Start the XBinary response message */
    ChkDR( DRM_XB_StartFormat( f_pbStack, f_cbStack, DRM_TEE_PROXY_LWRRENT_VERSION, &xbContext, s_XB_DRM_TEE_PROXY_METHOD_REQ_FormatDescription ) );

    if( DRM_SUCCEEDED( f_drResult ) )
    {
        DRM_DWORD idxBlob      = 0;
        DRM_DWORD idxDword     = 0;
        DRM_DWORD idxQword     = 0;
        DRM_DWORD idxID        = 0;
        DRM_DWORD idxDwordList = 0;
        DRM_DWORD idxQwordList = 0;

        for( ; idx < MAX_DRM_TEE_PROXY_PARAMETER_COUNT; idx++ )
        {
            if( f_pParameters[ idx ].fIsOutputParameter )
            {
                DRM_VOID *pvParameter   = (DRM_VOID*)&f_pParameters[ idx ].uParameterValue;
                DRM_VOID *pvXBParam     = NULL;

                switch( f_pParameters[ idx ].eParameterType )
                {
                case DRM_TEE_PROXY_PARAMETER_TYPE__DRM_DWORD:
                    AssertChkArg( idxDword < DRM_NO_OF( rgXBDwordArgs ) );
                    pvXBParam   = (DRM_VOID*)&rgXBDwordArgs[ idxDword++ ];
                    break;
                case DRM_TEE_PROXY_PARAMETER_TYPE__DRM_QWORD:
                    AssertChkArg( idxQword < DRM_NO_OF( rgXBQwordArgs ) );
                    pvXBParam   = (DRM_VOID*)&rgXBQwordArgs[ idxQword++ ];
                    break;
                case DRM_TEE_PROXY_PARAMETER_TYPE__DRM_ID:
                    AssertChkArg( idxID < DRM_NO_OF( rgXBIdArgs ) );
                    pvXBParam   = (DRM_VOID*)&rgXBIdArgs[ idxID++ ];
                    break;
                case DRM_TEE_PROXY_PARAMETER_TYPE__DRM_TEE_BYTE_BLOB:
                    AssertChkArg( idxBlob < DRM_NO_OF( rgXBlobArgs ) );
                    pvXBParam   = (DRM_VOID*)&rgXBlobArgs[ idxBlob++ ];
                    break;
                case DRM_TEE_PROXY_PARAMETER_TYPE__DRM_TEE_DWORDLIST:
                    AssertChkArg( idxDwordList < DRM_NO_OF( rgXBDWLArgs ) );
                    pvXBParam   = (DRM_VOID*)&rgXBDWLArgs[ idxDwordList++ ];
                    break;
                case DRM_TEE_PROXY_PARAMETER_TYPE__DRM_TEE_QWORDLIST:
                    AssertChkArg( idxQwordList < DRM_NO_OF( rgXBQWLArgs ) );
                    pvXBParam   = (DRM_VOID*)&rgXBQWLArgs[ idxQwordList++ ];
                    break;
                default:
                    AssertChkArg( FALSE );
                }

                ChkDR( DRM_TEE_PROXYSTUB_AddParameter(
                    &xbContext,
                    f_pParameters[ idx ].eParameterType,
                    f_pParameters[ idx ].idxParameter,
                    pvParameter,
                    pvXBParam,
                    f_pResp ) );
            }
        }
    }

    if( f_pReq->MethodInfo.dwMethodID == DRM_TEE_PROXY_METHOD_ID( DRM_TEE_BASE_AllocTEEContext ) )
    {
        /* If the PRITEE method invocation failed during an AllocTEEContext call then we will not have a valid TEE context */
        if( DRM_SUCCEEDED( f_drResult ) )
        {
            ChkDR( DRM_TEE_STUB_XB_AddEntryTEEContext( &xbContext, XB_DRM_TEE_CONTEXT_ENTRY_TYPE, f_pTeeCtx, &f_pResp->MethodContext ) );
        }
        else
        {
            f_pResp->MethodContext.fValid                     = TRUE;
            f_pResp->MethodContext.xbbaContext.fValid         = TRUE;
            f_pResp->MethodContext.xbbaContext.iData          = 0;
            f_pResp->MethodContext.xbbaContext.cbData         = 0;
            f_pResp->MethodContext.xbbaContext.pbDataBuffer   = NULL;

            ChkDR( DRM_XB_AddEntry( &xbContext, XB_DRM_TEE_CONTEXT_ENTRY_TYPE, &f_pResp->MethodContext ) );
        }
    }
    else
    {
        ChkDR( DRM_TEE_STUB_XB_AddEntryTEEContext( &xbContext, XB_DRM_TEE_CONTEXT_ENTRY_TYPE, f_pTeeCtx, &f_pResp->MethodContext ) );
    }

    /* Add the method information and serialize the response */
    ChkDR( DRM_TEE_PROXYSTUB_AddMethodInfo( &xbContext, (DRM_METHOD_ID_DRM_TEE)f_pReq->MethodInfo.dwMethodID, f_drResult, f_pResp ) );
    ChkDR( DRM_XB_FinishFormat( &xbContext, f_pbResponseMessage, f_pcbResponseMessage ) );

ErrorExit:
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_STUB_ReadParameters(
    __in_opt                                               DRM_TEE_CONTEXT                 *f_pTEEContext,
    __in                                             const XB_DRM_TEE_PROXY_METHOD_REQ     *f_pXBMethodReq,
    __inout_ecount( MAX_DRM_TEE_PROXY_PARAMETER_COUNT)     DRM_TEE_PROXY_PARAMETERS        *f_pParameters )
{
    DRM_RESULT                         dr               = DRM_SUCCESS;
    const XB_DRM_ID_PARAMS            *pLwrId           = NULL;
    const XB_DRM_TEE_BYTE_BLOB_PARAMS *pLwrBlob         = NULL;
    const XB_DRM_DWORD_PARAMS         *pLwrDword        = NULL;
    const XB_DRM_QWORD_PARAMS         *pLwrQword        = NULL;
    const XB_DRM_DWORDLIST_PARAMS     *pLwrDwl          = NULL;
    const XB_DRM_QWORDLIST_PARAMS     *pLwrQwl          = NULL;
    DRM_DWORD                          idx              = 0;

    ChkArg( f_pXBMethodReq      != NULL );
    ChkArg( f_pParameters       != NULL );

    /* Dwords */
    pLwrDword = f_pXBMethodReq->DwordParams;
    while( pLwrDword != NULL )
    {
        ChkArg( pLwrDword->dwParamIndex < MAX_DRM_TEE_PROXY_PARAMETER_COUNT );
        ChkArg( f_pParameters[pLwrDword->dwParamIndex].eParameterType == DRM_TEE_PROXY_PARAMETER_TYPE__ILWALID );
        f_pParameters[pLwrDword->dwParamIndex].eParameterType          = DRM_TEE_PROXY_PARAMETER_TYPE__DRM_DWORD;
        f_pParameters[pLwrDword->dwParamIndex].uParameterValue.dwValue = pLwrDword->dwParamValue;
        pLwrDword = pLwrDword->pNext;
    }

    /* Blobs */
    pLwrBlob = f_pXBMethodReq->BlobParams;
    while( pLwrBlob != NULL )
    {
        ChkArg( pLwrBlob->dwParamIndex < MAX_DRM_TEE_PROXY_PARAMETER_COUNT );
        ChkArg( f_pParameters[pLwrBlob->dwParamIndex].eParameterType == DRM_TEE_PROXY_PARAMETER_TYPE__ILWALID );
        f_pParameters[pLwrBlob->dwParamIndex].eParameterType = DRM_TEE_PROXY_PARAMETER_TYPE__DRM_TEE_BYTE_BLOB;
        ChkDR( DRM_TEE_STUB_XB_TEEBlobFromXBBlob(
            f_pTEEContext,
            pLwrBlob,
            &f_pParameters[pLwrBlob->dwParamIndex].uParameterValue.blobValue,
            FALSE ) );
        pLwrBlob = pLwrBlob->pNext;
    }

    /* Qwords */
    pLwrQword = f_pXBMethodReq->QwordParams;
    while( pLwrQword != NULL )
    {
        ChkArg( pLwrQword->dwParamIndex < MAX_DRM_TEE_PROXY_PARAMETER_COUNT );
        ChkArg( f_pParameters[pLwrQword->dwParamIndex].eParameterType == DRM_TEE_PROXY_PARAMETER_TYPE__ILWALID );
        f_pParameters[pLwrQword->dwParamIndex].eParameterType          = DRM_TEE_PROXY_PARAMETER_TYPE__DRM_QWORD;
        f_pParameters[pLwrQword->dwParamIndex].uParameterValue.qwValue = pLwrQword->qwParamValue;
        pLwrQword = pLwrQword->pNext;
    }

    /* Dword Lists */
    pLwrDwl = f_pXBMethodReq->DwordListParams;
    while( pLwrDwl != NULL )
    {
        ChkArg( pLwrDwl->dwParamIndex < MAX_DRM_TEE_PROXY_PARAMETER_COUNT );
        ChkArg( f_pParameters[pLwrDwl->dwParamIndex].eParameterType == DRM_TEE_PROXY_PARAMETER_TYPE__ILWALID );
        f_pParameters[pLwrDwl->dwParamIndex].eParameterType = DRM_TEE_PROXY_PARAMETER_TYPE__DRM_TEE_DWORDLIST;
        ChkDR( DRM_TEE_STUB_XB_TEEDwordListFromXBDwordList(
            f_pTEEContext,
            pLwrDwl,
            &f_pParameters[pLwrDwl->dwParamIndex].uParameterValue.dwlValue,
            FALSE ) );
        pLwrDwl = pLwrDwl->pNext;
    }

    /* Qword Lists */
    pLwrQwl = f_pXBMethodReq->QwordListParams;
    while( pLwrQwl != NULL )
    {
        ChkArg( pLwrQwl->dwParamIndex < MAX_DRM_TEE_PROXY_PARAMETER_COUNT );
        ChkArg( f_pParameters[ pLwrQwl->dwParamIndex ].eParameterType == DRM_TEE_PROXY_PARAMETER_TYPE__ILWALID );
        f_pParameters[ pLwrQwl->dwParamIndex ].eParameterType = DRM_TEE_PROXY_PARAMETER_TYPE__DRM_TEE_QWORDLIST;
        ChkDR( DRM_TEE_STUB_XB_TEEQwordListFromXBQwordList(
            f_pTEEContext,
            pLwrQwl,
            &f_pParameters[ pLwrQwl->dwParamIndex ].uParameterValue.qwlValue,
            FALSE ) );
        pLwrQwl = pLwrQwl->pNext;
    }

    /* Drm_ID */
    pLwrId = f_pXBMethodReq->IdParams;
    while( pLwrId != NULL )
    {
        ChkArg( pLwrId->dwParamIndex < MAX_DRM_TEE_PROXY_PARAMETER_COUNT );
        ChkArg( f_pParameters[pLwrId->dwParamIndex].eParameterType == DRM_TEE_PROXY_PARAMETER_TYPE__ILWALID );
        f_pParameters[pLwrId->dwParamIndex].eParameterType          = DRM_TEE_PROXY_PARAMETER_TYPE__DRM_ID;
        f_pParameters[pLwrId->dwParamIndex].uParameterValue.idValue = pLwrId->idParamValue;
        pLwrId = pLwrId->pNext;
    }

    for( ; idx < MAX_DRM_TEE_PROXY_PARAMETER_COUNT; idx++ )
    {
        f_pParameters[idx].idxParameter = idx;
    }

ErrorExit:
    return dr;
}

/*
** Synopsis:
**
** This method encrypts, signs and returns a DRM_TEE_CONTEXT serialized to bytes.
** The DRM_TEE_CONTEXT context (along with the associated OEM_TEE_CONTEXT)
** is flattened into a single memory blob and then is encrypted using a key derived from the
** CTK key.  The encrypted blob is then signed by another key derived from the CTK key.
**
** Operations:
**   1. Callwlate the full size of the DRM_TEE_CONTEXT (OEM_TEE_CONTEXT included) plus the
**      the size of the signature.
**   2. Create and fill the flattened DRM_TEE_CONTEXT buffer.
**   3. Encrypt the context buffer with a key derived from CTK.
**   4. Sign the context buffer with another key dervied from CTK.
**
** Arguments:
**
** f_pTEEContext:       (in)     The DRM TEE context.
** f_pcbTEECtx:         (out)    The size of the serialized DRM TEE context in bytes.
** f_ppbTEECtx:         (out)    The the serialized DRM TEE context.
**                               This should be freed with DRM_TEE_BASE_MemFree.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_STUB_XB_TEECtxToBytes(
    __inout                                     DRM_TEE_CONTEXT                 *f_pTEEContext,
    __out                                       DRM_DWORD                       *f_pcbTEECtx,
    __deref_out_bcount( *f_pcbTEECtx )          DRM_BYTE                       **f_ppbTEECtx )
{
    DRM_RESULT               dr         = DRM_SUCCESS;
    DRM_DWORD                cbXBBData  = 0;
    DRM_BYTE                *pbXBBData  = NULL;
    DRM_BYTE                *pbLwrrent  = NULL;
    const OEM_TEE_CONTEXT   *pOemTeeCtx = NULL;

    ChkArg( f_pTEEContext != NULL );
    ChkArg( f_pcbTEECtx   != NULL );
    ChkArg( f_ppbTEECtx   != NULL );

    pOemTeeCtx = &f_pTEEContext->oContext;

    /* callwlate full size of flattened DRM_TEE_CONTEXT/OEM_TEE_CONTEXT, which is equal to the DRM_TEE_CONTEXT plus the unprotected OEM data (if any) */
    ChkDR( DRM_DWordAdd( sizeof( *f_pTEEContext ), pOemTeeCtx->cbUnprotectedOEMData, &cbXBBData ) );

    /* allocate data */
    ChkDR( DRM_TEE_BASE_MemAlloc( f_pTEEContext, cbXBBData, (DRM_VOID **)&pbXBBData ) );
    pbLwrrent = pbXBBData;

    /* Copy TEE context itself */
    ChkVOID( OEM_TEE_MEMCPY( pbXBBData, f_pTEEContext, sizeof( *f_pTEEContext ) ) );
    pbLwrrent += sizeof( *f_pTEEContext );

    /* Copy the unprotected OEM data (if any) */
    if( pOemTeeCtx->cbUnprotectedOEMData > 0 )
    {
        ChkVOID( OEM_TEE_MEMCPY( pbLwrrent, pOemTeeCtx->pbUnprotectedOEMData, pOemTeeCtx->cbUnprotectedOEMData ) );
    }

    /* Zero the pointer */
    ChkVOID( OEM_TEE_ZERO_MEMORY(
        &pbXBBData[ DRM_OFFSET_OF( DRM_TEE_CONTEXT, oContext ) + DRM_OFFSET_OF( OEM_TEE_CONTEXT, pbUnprotectedOEMData ) ],
        sizeof( pOemTeeCtx->pbUnprotectedOEMData ) ) );

    /* Transfer ownership */
    *f_pcbTEECtx = cbXBBData;
    *f_ppbTEECtx = pbXBBData;
    pbXBBData    = NULL;

ErrorExit:
    ChkVOID( DRM_TEE_BASE_MemFree( f_pTEEContext, ( DRM_VOID ** )&pbXBBData ) );
    return dr;
}


/*
** Synopsis:
**
** This method ilwokes DRM_TEE_STUB_XB_TEECtxToBytes on the given DRM_TEE_CONTEXT.
** Then the encrypted blob is added to the XBinary representation of the TEE context.
**
** Operations:
**   1. Ilwoke DRM_TEE_STUB_XB_TEECtxToBytes on the given DRM_TEE_CONTEXT.
**   2. Assign the encrypted and signed buffer to the XBinary TEE context structure
**   3. Add the XBinary object to the Binary builder
**
** Arguments:
**
** f_pXBContext:        (in/out) The XBinary builder context.
** f_wXBType:           (in)     The XBinary object type.
** f_pTEEContext:       (in)     The DRM TEE context.
** f_pXBTEEContext:     (in/out) The XBinary representation of the DRM TEE context object.  This
**                               argument is initialized with the argument f_pTEEContext.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_STUB_XB_AddEntryTEEContext(
    __inout                                     DRM_XB_BUILDER_CONTEXT          *f_pXBContext,
    __in                                        DRM_WORD                         f_wXBType,
    __in                                        DRM_TEE_CONTEXT                 *f_pTEEContext,
    __inout                                     XB_DRM_TEE_CONTEXT              *f_pXBTEEContext )
{
    DRM_RESULT  dr        = DRM_SUCCESS;
    DRM_DWORD   cbXBBData = 0;
    DRM_BYTE   *pbXBBData = NULL;

    ChkArg( f_wXBType       != 0 );
    ChkArg( f_pTEEContext   != NULL );
    ChkArg( f_pXBTEEContext != NULL );

    ChkDR( DRM_TEE_STUB_XB_TEECtxToBytes( f_pTEEContext, &cbXBBData, &pbXBBData ) );

    f_pXBTEEContext->fValid                     = TRUE;
    f_pXBTEEContext->xbbaContext.fValid         = TRUE;
    f_pXBTEEContext->xbbaContext.iData          = 0;
    f_pXBTEEContext->xbbaContext.cbData         = cbXBBData;
    f_pXBTEEContext->xbbaContext.pbDataBuffer   = pbXBBData;
    pbXBBData = NULL;

    ChkDR( DRM_XB_AddEntry( f_pXBContext, f_wXBType, f_pXBTEEContext ) );

ErrorExit:
    ChkVOID( DRM_TEE_BASE_MemFree( f_pTEEContext, ( DRM_VOID ** )&pbXBBData ) );
    return dr;
}

/*
** Synopsis:
**
** This method validates and decrypts DRM_TEE_CONTEXT object from the provided bytes.
** The decryption and signature checks are performed using separate keys
** derived from the CTK key.
**
** Operations:
**   1. Copy the bytes of the TEE context into a decryption buffer.
**   2. Verify the signature of the decryption buffer using a key derived from the CTK key.
**   3. Decrypt the buffer using a separate key derived from the CTK key.
**   4. Copy the decrypted TEE context data into the DRM_TEE_CONTEXT object.
**
** Arguments:
**
** f_cbTEECtx:     (in)     Number of bytes in the encrypted and signed TEE Ctx.
** f_pbTEECtx:     (in)     The encrypted and signed TEE Ctx.
** f_pTEEContext:  (in/out) A pointer to the DRM TEE context object that will be
**                          populated from the provided bytes.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_STUB_XB_TEECtxFromBytes(
    __in                                        DRM_DWORD                        f_cbTEECtx,
    __in_bcount( f_cbTEECtx )             const DRM_BYTE                        *f_pbTEECtx,
    __inout                                     DRM_TEE_CONTEXT                 *f_pTEEContext )
{
    DRM_RESULT       dr                         = DRM_SUCCESS;
    DRM_BYTE        *pbUnprotectedOEMDataCopy   = NULL;
    const DRM_BYTE  *pbLwrrent                  = NULL;
    OEM_TEE_CONTEXT *pOemTeeCtx                 = NULL;

    ChkArg( f_cbTEECtx > 0 );
    ChkArg( f_pbTEECtx != NULL );
    ChkArg( f_pTEEContext != NULL );

    pOemTeeCtx = &f_pTEEContext->oContext;

    /* Ensure that there's at least enough space for the context itself */
    ChkArg( f_cbTEECtx >= sizeof( *f_pTEEContext ) );
    pbLwrrent = f_pbTEECtx;

    /* Read the TEE context including the OEM data size but not including the OEM data itself */
    ChkVOID( OEM_TEE_MEMCPY( f_pTEEContext, pbLwrrent, sizeof( *f_pTEEContext ) ) );
    pbLwrrent += sizeof( *f_pTEEContext );

    /* Verify that the pointer was zero'd during serialization */
    ChkArg( pOemTeeCtx->pbUnprotectedOEMData == NULL );

    /* Read the OEM data, if any */
    if( pOemTeeCtx->cbUnprotectedOEMData > 0 )
    {
        ChkArg( f_cbTEECtx - sizeof( *f_pTEEContext ) == pOemTeeCtx->cbUnprotectedOEMData  );
        ChkDR( OEM_TEE_BASE_SelwreMemAlloc( NULL, pOemTeeCtx->cbUnprotectedOEMData, (DRM_VOID **)&pbUnprotectedOEMDataCopy ) );
        ChkVOID( OEM_TEE_MEMCPY( pbUnprotectedOEMDataCopy, pbLwrrent, pOemTeeCtx->cbUnprotectedOEMData ) );
    }
    else
    {
        ChkArg( f_cbTEECtx == sizeof( *f_pTEEContext ) );
    }

    /* Transfer ownership of the OEM data, if any */
    pOemTeeCtx->pbUnprotectedOEMData = pbUnprotectedOEMDataCopy;
    pbUnprotectedOEMDataCopy = NULL;

ErrorExit:
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( NULL, (DRM_VOID **)&pbUnprotectedOEMDataCopy ) );
    return dr;
}

/*
** Synopsis:
**
** This method ilwokes DRM_TEE_STUB_XB_TEECtxFromBytes on the bytes from the given XBinary TEE Context.
** Refer to that method for more details.
**
** Operations:
**   1. Ilwoke DRM_TEE_STUB_XB_TEECtxFromBytes on the bytes from the given XBinary TEE Context.
**
** Arguments:
**
** f_pXBTEECtx:    (in)     The XBinary repesentation of the DRM TEE context object.
** f_pTEECtx:      (in/out) A pointer to the DRM TEE context object that will be
**                          populated from the provided f_pXBTEECtx object.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_STUB_XB_TEECtxFromXBTEECtx(
    __in                              const XB_DRM_TEE_CONTEXT              *f_pXBTEECtx,
    __inout                                 DRM_TEE_CONTEXT                 *f_pTEEContext )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_pXBTEECtx != NULL );
    ChkDR( DRM_TEE_STUB_XB_TEECtxFromBytes(
        XBBA_TO_CB( f_pXBTEECtx->xbbaContext ),
        XBBA_TO_PB( f_pXBTEECtx->xbbaContext ),
        f_pTEEContext ) );

ErrorExit:
    return dr;
}

/*
** Synopsis:
**
** This method populates a DRM_TEE_BYTE_BLOB object from XBinary blob object.
**
** Arguments:
**
** f_pTEEContext:  (in)     The DRM TEE context.
** f_pXBBlob:      (in)     An XBinary blob object that will be used to populate the
**                          f_pBlob argument.
** f_pBlob:        (out)    A refrence to a TEE blob object that will be populated with
**                          the data in the argument (f_pXBBlob).
** f_fMakeCopy     (in)     Indicates whether a new memory allocation is required. If
**                          the value is TRUE, a new blob is created and the memory is
**                          copied.  If the value is FALSE, then the f_pBlob object will
**                          have a weak reference to the blob data.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_STUB_XB_TEEBlobFromXBBlob(
    __in_opt                                DRM_TEE_CONTEXT                 *f_pTEEContext,
    __in                              const XB_DRM_TEE_BYTE_BLOB_PARAMS     *f_pXBBlob,
    __inout                                 DRM_TEE_BYTE_BLOB               *f_pBlob,
    __in                                    DRM_BOOL                         f_fMakeCopy )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_pXBBlob   != NULL );
    ChkArg( f_pBlob     != NULL );
    ChkArg( f_pBlob->pb == NULL );
    ChkArg( f_pBlob->cb == 0     );

    if( f_pXBBlob->xbbaData.cbData > 0 )
    {
        ChkDR( DRM_TEE_BASE_AllocBlob(
            f_pTEEContext,
            f_fMakeCopy ? DRM_TEE_BLOB_ALLOC_BEHAVIOR_COPY : DRM_TEE_BLOB_ALLOC_BEHAVIOR_WEAKREF,
            f_pXBBlob->xbbaData.cbData,
            &f_pXBBlob->xbbaData.pbDataBuffer[f_pXBBlob->xbbaData.iData],
            f_pBlob ) );
    }
    else
    {
        f_pBlob->eType = ( DRM_TEE_BYTE_BLOB_TYPE )f_pXBBlob->eType;
        f_pBlob->pb    = NULL;
        f_pBlob->cb    = 0;
    }

    f_pBlob->dwSubType = f_pXBBlob->dwSubType;

ErrorExit:
    return dr;
}

/*
** Synopsis:
**
** This method populates a DRM_TEE_DWORDLIST object from XBinary DWORD list object.
**
** Arguments:
**
** f_pTEEContext:  (in)     The DRM TEE context.
** f_pXBDwordList: (in)     An XBinary DWORD list object that will be used to populate the
**                          f_pTEEDwordList argument.
** f_pTEEDwordList:(out)    A refrence to a TEE DWORD list object that will be populated with
**                          the data in the argument f_pXBDwordList.
** f_fMakeCopy     (in)     Indicates whether a new memory allocation is required. If
**                          the value is TRUE, memory will be allocated and the data is
**                          copied.  If the value is FALSE, then the f_pTEEDwordList object
**                          will have a weak reference to the DWORD list data.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_STUB_XB_TEEDwordListFromXBDwordList(
    __in_opt                                DRM_TEE_CONTEXT                 *f_pTEEContext,
    __in                              const XB_DRM_DWORDLIST_PARAMS         *f_pXBDwordList,
    __inout                                 DRM_TEE_DWORDLIST               *f_pTEEDwordList,
    __in                                    DRM_BOOL                         f_fMakeCopy )
{
    DRM_RESULT        dr   = DRM_SUCCESS;
    DRM_DWORD         cb   = 0;
    DRM_TEE_DWORDLIST oDWL = DRM_TEE_DWORDLIST_EMPTY;

    ChkArg( f_pXBDwordList           != NULL );
    ChkArg( f_pTEEDwordList          != NULL );
    ChkArg( f_pTEEDwordList->pdwData == NULL );
    ChkArg( f_pTEEDwordList->cdwData == 0    );

    cb = f_pXBDwordList->dwlParamValue.cDWORDs;

    if( cb > 0 )
    {
        if( f_fMakeCopy )
        {
            ChkDR( DRM_DWordMult( cb, sizeof(DRM_DWORD), &cb ) );
            ChkDR( DRM_TEE_BASE_MemAlloc( f_pTEEContext, cb, ( DRM_VOID ** )&oDWL.pdwData ) );
            ChkVOID( OEM_TEE_MEMCPY( ( DRM_BYTE * )oDWL.pdwData, XB_DWORD_LIST_TO_PDWORD( f_pXBDwordList->dwlParamValue ), cb ) );
            oDWL.cdwData = f_pXBDwordList->dwlParamValue.cDWORDs;
        }
        else
        {
            oDWL.cdwData = f_pXBDwordList->dwlParamValue.cDWORDs;
            oDWL.pdwData = XB_DWORD_LIST_TO_PDWORD( f_pXBDwordList->dwlParamValue );
        }
    }

    ChkVOID( DRM_TEE_DWORDLIST_TRANSFER_OWNERSHIP( f_pTEEDwordList, &oDWL ) );

ErrorExit:
    ChkVOID( DRM_TEE_BASE_MemFree( f_pTEEContext, ( DRM_VOID ** )&oDWL.pdwData ) );
    return dr;
}

/*
** Synopsis:
**
** This method populates a DRM_TEE_QWORDLIST object from XBinary QWORD list object.
**
** Arguments:
**
** f_pTEEContext:  (in)     The DRM TEE context.
** f_pXBQwordList: (in)     An XBinary QWORD list object that will be used to populate the
**                          f_pTEEQwordList argument.
** f_pTEEQwordList:(out)    A refrence to a TEE QWORD list object that will be populated with
**                          the data in the argument f_pXBQwordList.
** f_fMakeCopy     (in)     Indicates whether a new memory allocation is required. If
**                          the value is TRUE, memory will be allocated and the data is
**                          copied.  If the value is FALSE, then the f_pTEEQwordList object
**                          will have a weak reference to the QWORD list data.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_STUB_XB_TEEQwordListFromXBQwordList(
    __in_opt                                DRM_TEE_CONTEXT                 *f_pTEEContext,
    __in                              const XB_DRM_QWORDLIST_PARAMS         *f_pXBQwordList,
    __inout                                 DRM_TEE_QWORDLIST               *f_pTEEQwordList,
    __in                                    DRM_BOOL                         f_fMakeCopy )
{
    DRM_RESULT        dr   = DRM_SUCCESS;
    DRM_DWORD         cb   = 0;
    DRM_TEE_QWORDLIST oQWL = DRM_TEE_QWORDLIST_EMPTY;

    ChkArg( f_pXBQwordList != NULL );
    ChkArg( f_pTEEQwordList != NULL );
    ChkArg( f_pTEEQwordList->pqwData == NULL );
    ChkArg( f_pTEEQwordList->cqwData == 0 );

    cb = f_pXBQwordList->qwlParamValue.cQWORDs;

    if( cb > 0 )
    {
        if( f_fMakeCopy )
        {
            ChkDR( DRM_DWordMult( cb, sizeof( DRM_UINT64 ), &cb ) );
            ChkDR( DRM_TEE_BASE_MemAlloc( f_pTEEContext, cb, (DRM_VOID **)&oQWL.pqwData ) );
            ChkVOID( OEM_TEE_MEMCPY( (DRM_BYTE *)oQWL.pqwData, XB_QWORD_LIST_TO_PQWORD( f_pXBQwordList->qwlParamValue ), cb ) );
            oQWL.cqwData = f_pXBQwordList->qwlParamValue.cQWORDs;
        }
        else
        {
            oQWL.cqwData = f_pXBQwordList->qwlParamValue.cQWORDs;
            oQWL.pqwData = XB_QWORD_LIST_TO_PQWORD( f_pXBQwordList->qwlParamValue );
        }
    }

    ChkVOID( DRM_TEE_QWORDLIST_TRANSFER_OWNERSHIP( f_pTEEQwordList, &oQWL ) );

ErrorExit:
    ChkVOID( DRM_TEE_BASE_MemFree( f_pTEEContext, (DRM_VOID **)&oQWL.pqwData ) );
    return dr;
}
#endif
EXIT_PK_NAMESPACE_CODE;

