/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef DRMTEESTUBCOMMON_H
#define DRMTEESTUBCOMMON_H

#include <drmerr.h>
#include <drmxbbuilder.h>
#include <drmteeproxy_generated.h>
#include <drmteetypes.h>

ENTER_PK_NAMESPACE;

typedef struct __tagDRM_TEE_PROXY_PARAMETERS
{
    DRM_TEE_PROXY_PARAMETER_TYPE  eParameterType;
    DRM_DWORD                     idxParameter;
    DRM_BOOL                      fIsOutputParameter;
    union parameterValue
    {
        DRM_DWORD           dwValue;
        DRM_UINT64          qwValue;
        DRM_ID              idValue;
        DRM_TEE_BYTE_BLOB   blobValue;
        DRM_TEE_DWORDLIST   dwlValue;
        DRM_TEE_QWORDLIST   qwlValue;
    } uParameterValue;

} DRM_TEE_PROXY_PARAMETERS;


/* PRITEE helper functions */

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_STUB_InitParameters(
    __inout_ecount(MAX_DRM_TEE_PROXY_PARAMETER_COUNT)      DRM_TEE_PROXY_PARAMETERS              *f_pParameters );

//
// LWE (kwilson)  In Microsoft PK code, DRM_TEE_STUB_FreeParameters returns DRM_VOID.
// Lwpu had to change the return value to DRM_RESULT in order to support a return
// code from acquiring/releasing the critical section, which may not always succeed.
//
DRM_NO_INLINE DRM_RESULT DRM_CALL DRM_TEE_STUB_FreeParameters(
    __inout_opt                                            DRM_TEE_CONTEXT                       *f_pTEEContext,
    __inout_ecount(MAX_DRM_TEE_PROXY_PARAMETER_COUNT)      DRM_TEE_PROXY_PARAMETERS              *f_pParameters );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_STUB_GenerateOutput(
    __in_opt                                               DRM_TEE_CONTEXT                       *f_pTeeCtx,
    __in_ecount(MAX_DRM_TEE_PROXY_PARAMETER_COUNT)   const DRM_TEE_PROXY_PARAMETERS              *f_pParameters,
    __in                                                   DRM_RESULT                             f_drResult,
    __in                                                   DRM_DWORD                              f_cbStack,
    __inout_bcount( f_cbStack )                            DRM_BYTE                              *f_pbStack,
    __in                                             const XB_DRM_TEE_PROXY_METHOD_REQ           *f_pReq,
    __inout                                                XB_DRM_TEE_PROXY_METHOD_REQ           *f_pResp,
    __inout                                                DRM_DWORD                             *f_pcbResponseMessage,
    __inout_bcount( *f_pcbResponseMessage )                DRM_BYTE                              *f_pbResponseMessage );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_STUB_ReadParameters(
    __in_opt                                               DRM_TEE_CONTEXT                       *f_pTEEContext,
    __in                                             const XB_DRM_TEE_PROXY_METHOD_REQ           *f_pXBMethodReq,
    __inout_ecount( MAX_DRM_TEE_PROXY_PARAMETER_COUNT)     DRM_TEE_PROXY_PARAMETERS              *f_pParameters );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_STUB_XB_AddEntryTEEContext(
    __inout                                     DRM_XB_BUILDER_CONTEXT          *f_pXBContext,
    __in                                        DRM_WORD                         f_wXBType,
    __in                                        DRM_TEE_CONTEXT                 *f_pTEEContext,
    __inout                                     XB_DRM_TEE_CONTEXT              *f_pXBTEEContext );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_STUB_XB_TEECtxFromXBTEECtx(
    __in                                  const XB_DRM_TEE_CONTEXT              *f_pXBTEECtx,
    __inout                                     DRM_TEE_CONTEXT                 *f_pTEECtx );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_STUB_XB_TEEBlobFromXBBlob(
    __in_opt                                    DRM_TEE_CONTEXT                 *f_pTEEContext,
    __in                                  const XB_DRM_TEE_BYTE_BLOB_PARAMS     *f_pXBBlob,
    __inout                                     DRM_TEE_BYTE_BLOB               *f_pBlob,
    __in                                        DRM_BOOL                         f_fMakeCopy );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_STUB_XB_TEEDwordListFromXBDwordList(
    __in_opt                                    DRM_TEE_CONTEXT                 *f_pTEEContext,
    __in                                  const XB_DRM_DWORDLIST_PARAMS         *f_pXBDwordList,
    __inout                                     DRM_TEE_DWORDLIST               *f_pTEEDwordList,
    __in                                        DRM_BOOL                         f_fMakeCopy );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_STUB_XB_TEEQwordListFromXBQwordList(
    __in_opt                                    DRM_TEE_CONTEXT                 *f_pTEEContext,
    __in                                  const XB_DRM_QWORDLIST_PARAMS         *f_pXBQwordList,
    __inout                                     DRM_TEE_QWORDLIST               *f_pTEEQwordList,
    __in                                        DRM_BOOL                         f_fMakeCopy );

EXIT_PK_NAMESPACE;

#endif // DRMTEESTUBCOMMON_H

