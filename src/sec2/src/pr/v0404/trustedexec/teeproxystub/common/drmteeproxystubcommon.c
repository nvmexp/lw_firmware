/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <drmerr.h>
#include <drmmathsafe.h>
#include <drmbytemanip.h>
#include <oemtee.h>
#include <oemcommon.h>
#include <oemteeproxy.h>
#include <drmxb.h>
#include <drmxbbuilder.h>
#include <drmxbparser.h>
#include <drmteeproxystubcommon.h>

#include <drmteeproxyformat.h>

ENTER_PK_NAMESPACE_CODE;

#include <drmteeproxyformat_generated.c>

#if defined(SEC_COMPILE)
/*
** Synopsis:
**
** Adds method information (method ID and result) to the XBinary builder.  When called from
** the proxy the parameter f_drResult should always be DRM_SUCCESS.
**
** Arguments:
**
** f_pXBContext:   (in)     The XBinary builder context.
** f_eMethodID:    (in)     The method ID for the TEE function being ilwoked.
** f_drResult:     (in)     The result of the ilwoked TEE function.  For proxy side use, this
**                          value should always be DRM_SUCCESS.
** f_pXBMethodReq: (in/out) The XBinary TEE method request structure.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_PROXYSTUB_AddMethodInfo(
    __inout                                     DRM_XB_BUILDER_CONTEXT          *f_pXBContext,
    __in                                        DRM_METHOD_ID_DRM_TEE            f_eMethodID,
    __in                                        DRM_RESULT                       f_drResult,
    __inout                                     XB_DRM_TEE_PROXY_METHOD_REQ     *f_pXBMethodReq )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_pXBContext   != NULL );
    ChkArg( f_pXBMethodReq != NULL );

    f_pXBMethodReq->fValid                = TRUE;
    f_pXBMethodReq->MethodInfo.fValid     = TRUE;
    f_pXBMethodReq->MethodInfo.dwResult   = ( DRM_DWORD )f_drResult;
    f_pXBMethodReq->MethodInfo.dwMethodID = f_eMethodID;

    ChkDR( DRM_XB_AddEntry( f_pXBContext, XB_DRM_TEE_REQ_METHOD_INFO_ENTRY_TYPE, &f_pXBMethodReq->MethodInfo ) );

ErrorExit:
    return dr;
}
#endif

#ifdef NONE
/*
** Synopsis:
**
** Gets a DWORD parameter from the XBinary TEE method request structure given its parameter index.
**
** Arguments:
**
** f_pXBMethodReq: (in)     The XBinary TEE method request structure.
** f_dwParamIndex: (in)     The index of the DWORD parameter to retrieve.
** f_pdwParam:     (in)     The DWORD value at the given parameter index.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_PROXYSTUB_GetDwordParamByIndex(
    __in                              const XB_DRM_TEE_PROXY_METHOD_REQ     *f_pXBMethodReq,
    __in                                    DRM_DWORD                        f_dwParamIndex,
    __out                                   DRM_DWORD                       *f_pdwParam )
{
    DRM_RESULT                 dr   = DRM_SUCCESS;
    const XB_DRM_DWORD_PARAMS *pLwr = NULL;

    ChkArg( f_pXBMethodReq != NULL );
    ChkArg( f_pdwParam     != NULL );

    pLwr = f_pXBMethodReq->DwordParams;

    while( pLwr != NULL )
    {
        if( pLwr->dwParamIndex == f_dwParamIndex )
        {
            *f_pdwParam = pLwr->dwParamValue;
            break;
        }

        pLwr = pLwr->pNext;
    }

    ChkBOOL( pLwr != NULL, DRM_E_NOT_FOUND );

ErrorExit:
    return dr;
}

/*
** Synopsis:
**
** Gets a QWORD parameter from the XBinary TEE method request structure given its parameter index.
**
** Arguments:
**
** f_pXBMethodReq: (in)     The XBinary TEE method request structure.
** f_qwParamIndex: (in)     The index of the QWORD parameter to retrieve.
** f_pqwParam:     (in)     The QWORD value at the given parameter index.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_PROXYSTUB_GetQwordParamByIndex(
    __in                              const XB_DRM_TEE_PROXY_METHOD_REQ     *f_pXBMethodReq,
    __in                                    DRM_DWORD                        f_dwParamIndex,
    __out                                   DRM_UINT64                      *f_pqwParam )
{
    DRM_RESULT                 dr   = DRM_SUCCESS;
    const XB_DRM_QWORD_PARAMS *pLwr = NULL;

    ChkArg( f_pXBMethodReq != NULL );
    ChkArg( f_pqwParam     != NULL );

    pLwr = f_pXBMethodReq->QwordParams;

    while( pLwr != NULL )
    {
        if( pLwr->dwParamIndex == f_dwParamIndex )
        {
            *f_pqwParam = pLwr->qwParamValue;
            break;
        }

        pLwr = pLwr->pNext;
    }

    ChkBOOL( pLwr != NULL, DRM_E_NOT_FOUND );

ErrorExit:
    return dr;
}

/*
** Synopsis:
**
** Gets a ID parameter from the XBinary TEE method request structure given its parameter index.
**
** Arguments:
**
** f_pXBMethodReq: (in)     The XBinary TEE method request structure.
** f_dwParamIndex: (in)     The index of the ID parameter to retrieve.
** f_pdwParam:     (in)     The ID value at the given parameter index.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_PROXYSTUB_GetIDParamByIndex(
    __in                              const XB_DRM_TEE_PROXY_METHOD_REQ     *f_pXBMethodReq,
    __in                                    DRM_DWORD                        f_dwParamIndex,
    __out                                   DRM_ID                          *f_pidParam )
{
    DRM_RESULT              dr   = DRM_SUCCESS;
    const XB_DRM_ID_PARAMS *pLwr = NULL;

    ChkArg( f_pXBMethodReq != NULL );
    ChkArg( f_pidParam     != NULL );

    pLwr = f_pXBMethodReq->IdParams;

    while( pLwr != NULL )
    {
        if( pLwr->dwParamIndex == f_dwParamIndex )
        {
            *f_pidParam = pLwr->idParamValue;
            break;
        }

        pLwr = pLwr->pNext;
    }

    ChkBOOL( pLwr != NULL, DRM_E_NOT_FOUND );

ErrorExit:
    return dr;
}
#endif

#if defined(SEC_COMPILE)
/*
** Synopsis:
**
** Adds a parameter to the XBinary TEE function call request/response message at
** the provided parameter index.
**
** Arguments:
**
** f_pXBContext:   (in)     The XBinary builder context.
** f_eParamType:   (in)     The parameter type.
** f_dwParamIndex: (in)     The index of the parameter in the function call.
** f_pvParam:      (in)     The parameter value.
** f_pvXBParam:    (in)     A reference to the XBinary parameter.
** f_pXBMethodReq: (in)     The XBinary TEE method request/response structure.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_PROXYSTUB_AddParameter(
    __inout                                     DRM_XB_BUILDER_CONTEXT          *f_pXBContext,
    __in                                        DRM_TEE_PROXY_PARAMETER_TYPE     f_eParamType,
    __in                                        DRM_DWORD                        f_dwParamIndex,
    __in                                  const DRM_VOID                        *f_pvParam,
    __inout                                     DRM_VOID                        *f_pvXBParam,
    __inout                                     XB_DRM_TEE_PROXY_METHOD_REQ     *f_pXBMethodReq )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_pXBContext   != NULL );
    ChkArg( f_pvParam      != NULL );
    ChkArg( f_pvXBParam    != NULL );
    ChkArg( f_pXBMethodReq != NULL );

    switch( f_eParamType )
    {
        case DRM_TEE_PROXY_PARAMETER_TYPE__DRM_DWORD:
            {
                XB_DRM_DWORD_PARAMS *pxbDword = ( XB_DRM_DWORD_PARAMS * )f_pvXBParam;
                const DRM_DWORD     *pdwParam = ( const DRM_DWORD     * )f_pvParam;

                pxbDword->fValid            = TRUE;
                pxbDword->dwParamIndex      = f_dwParamIndex;
                pxbDword->dwParamValue      = *pdwParam;
                pxbDword->pNext             = f_pXBMethodReq->DwordParams;
                f_pXBMethodReq->DwordParams = pxbDword;

                ChkDR( DRM_XB_AddEntry( f_pXBContext, XB_DRM_DWORD_PARAMS_ENTRY_TYPE, pxbDword ) );
            }
            break;
        case DRM_TEE_PROXY_PARAMETER_TYPE__DRM_QWORD:
            {
                XB_DRM_QWORD_PARAMS *pxbQword = ( XB_DRM_QWORD_PARAMS * )f_pvXBParam;
                const DRM_UINT64    *pqwParam = ( const DRM_UINT64    * )f_pvParam;

                pxbQword->fValid            = TRUE;
                pxbQword->dwParamIndex      = f_dwParamIndex;
                pxbQword->qwParamValue      = *pqwParam;
                pxbQword->pNext             = f_pXBMethodReq->QwordParams;
                f_pXBMethodReq->QwordParams = pxbQword;

                ChkDR( DRM_XB_AddEntry( f_pXBContext, XB_DRM_QWORD_PARAMS_ENTRY_TYPE, pxbQword ) );
            }
            break;
        case DRM_TEE_PROXY_PARAMETER_TYPE__DRM_ID:
            {
                XB_DRM_ID_PARAMS *pxbID    = ( XB_DRM_ID_PARAMS * )f_pvXBParam;
                const DRM_ID     *pidParam = ( const DRM_ID     * )f_pvParam;

                pxbID->fValid            = TRUE;
                pxbID->dwParamIndex      = f_dwParamIndex;
                pxbID->idParamValue      = *pidParam;
                pxbID->pNext             = f_pXBMethodReq->IdParams;
                f_pXBMethodReq->IdParams = pxbID;

                ChkDR( DRM_XB_AddEntry( f_pXBContext, XB_DRM_ID_PARAMS_ENTRY_TYPE, pxbID ) );
            }
            break;
        case DRM_TEE_PROXY_PARAMETER_TYPE__DRM_TEE_BYTE_BLOB:
            {
                XB_DRM_TEE_BYTE_BLOB_PARAMS *pxbBlob    = ( XB_DRM_TEE_BYTE_BLOB_PARAMS * )f_pvXBParam;
                const DRM_TEE_BYTE_BLOB     *pBlobParam = ( const DRM_TEE_BYTE_BLOB     * )f_pvParam;

                /* Verify that protected TEE blobs do not cross the TEE boundary */
                ChkBOOL( pBlobParam != NULL && pBlobParam->eType != DRM_TEE_BYTE_BLOB_TYPE_SELWRED_MODE, DRM_E_TEE_BLOB_ACCESS_DENIED );

                pxbBlob->fValid                   = TRUE;
                pxbBlob->dwParamIndex             = f_dwParamIndex;
                pxbBlob->eType                    = pBlobParam->eType;
                pxbBlob->dwSubType                = pBlobParam->dwSubType;
                pxbBlob->xbbaData.fValid          = TRUE;
                pxbBlob->xbbaData.iData           = 0;
                pxbBlob->xbbaData.pbDataBuffer    = pBlobParam->cb == 0 ? NULL : ( DRM_BYTE * )pBlobParam->pb;
                pxbBlob->xbbaData.cbData          = pBlobParam->cb;
                pxbBlob->pNext                    = f_pXBMethodReq->BlobParams;
                f_pXBMethodReq->BlobParams        = pxbBlob;

                ChkDR( DRM_XB_AddEntry( f_pXBContext, XB_DRM_TEE_BYTE_BLOB_PARAMS_ENTRY_TYPE, pxbBlob ) );
            }
            break;

        case DRM_TEE_PROXY_PARAMETER_TYPE__DRM_TEE_DWORDLIST:
            {
                XB_DRM_DWORDLIST_PARAMS *pXbParam   = ( XB_DRM_DWORDLIST_PARAMS * )f_pvXBParam;
                const DRM_TEE_DWORDLIST *pTeeParam  = ( const DRM_TEE_DWORDLIST * )f_pvParam;

                pXbParam->fValid                     = TRUE;
                pXbParam->dwParamIndex               = f_dwParamIndex;
                pXbParam->dwlParamValue.fValid       = TRUE;
                pXbParam->dwlParamValue.iDwords      = 0;
                pXbParam->dwlParamValue.pdwordBuffer = pTeeParam->cdwData == 0 ? NULL : ( DRM_BYTE * )pTeeParam->pdwData;
                pXbParam->dwlParamValue.cDWORDs      = pTeeParam->cdwData;
                pXbParam->pNext                      = f_pXBMethodReq->DwordListParams;
                f_pXBMethodReq->DwordListParams      = pXbParam;

                ChkDR( DRM_XB_AddEntry( f_pXBContext, XB_DRM_DWORDLIST_PARAMS_ENTRY_TYPE, pXbParam ) );
            }
            break;

        case DRM_TEE_PROXY_PARAMETER_TYPE__DRM_TEE_QWORDLIST:
            {
                XB_DRM_QWORDLIST_PARAMS *pXbParam   = ( XB_DRM_QWORDLIST_PARAMS * )f_pvXBParam;
                const DRM_TEE_QWORDLIST *pTeeParam  = ( const DRM_TEE_QWORDLIST * )f_pvParam;

                pXbParam->fValid                     = TRUE;
                pXbParam->dwParamIndex               = f_dwParamIndex;
                pXbParam->qwlParamValue.fValid       = TRUE;
                pXbParam->qwlParamValue.iQwords      = 0;
                pXbParam->qwlParamValue.pqwordBuffer = pTeeParam->cqwData == 0 ? NULL : ( DRM_BYTE * )pTeeParam->pqwData;
                pXbParam->qwlParamValue.cQWORDs      = pTeeParam->cqwData;
                pXbParam->pNext                      = f_pXBMethodReq->QwordListParams;
                f_pXBMethodReq->QwordListParams      = pXbParam;

                ChkDR( DRM_XB_AddEntry( f_pXBContext, XB_DRM_QWORDLIST_PARAMS_ENTRY_TYPE, pXbParam ) );
            }
            break;

        default:
            AssertLogicError();
            break;
    }

ErrorExit:
    return dr;
}
#endif
EXIT_PK_NAMESPACE_CODE;

