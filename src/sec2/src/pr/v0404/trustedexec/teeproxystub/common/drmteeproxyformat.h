/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __DRMTEEPROXYFORMAT_H__
#define __DRMTEEPROXYFORMAT_H__ 1

#include <drmteetypes.h>
#include <oemteetypes.h>
#include <drmxbbuilder.h>
#include <drmteeproxyformat_generated.h>

ENTER_PK_NAMESPACE;

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_PROXYSTUB_AddMethodInfo(
    __inout                                     DRM_XB_BUILDER_CONTEXT          *f_pXBContext,
    __in                                        DRM_METHOD_ID_DRM_TEE            f_eMethodID,
    __in                                        DRM_RESULT                       f_drResult,
    __inout                                     XB_DRM_TEE_PROXY_METHOD_REQ     *f_pXBMethodReq );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_PROXYSTUB_GetDwordParamByIndex(
    __in                                  const XB_DRM_TEE_PROXY_METHOD_REQ     *f_pXBMethodReq,
    __in                                        DRM_DWORD                        f_dwParamIndex,
    __out                                       DRM_DWORD                       *f_pdwParam );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_PROXYSTUB_GetQwordParamByIndex(
    __in                                  const XB_DRM_TEE_PROXY_METHOD_REQ     *f_pXBMethodReq,
    __in                                        DRM_DWORD                        f_dwParamIndex,
    __out                                       DRM_UINT64                      *f_pqwParam );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_PROXYSTUB_GetIDParamByIndex(
    __in                                  const XB_DRM_TEE_PROXY_METHOD_REQ     *f_pXBMethodReq,
    __in                                        DRM_DWORD                        f_dwParamIndex,
    __out                                       DRM_ID                          *f_pidParam );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_PROXYSTUB_AddParameter(
    __inout                                     DRM_XB_BUILDER_CONTEXT          *f_pXBContext,
    __in                                        DRM_TEE_PROXY_PARAMETER_TYPE     f_eParamType,
    __in                                        DRM_DWORD                        f_dwParamIndex,
    __in                                  const DRM_VOID                        *f_pvParam,
    __inout                                     DRM_VOID                        *f_pvXBParam,
    __inout                                     XB_DRM_TEE_PROXY_METHOD_REQ     *f_pXBMethodReq );

EXIT_PK_NAMESPACE;

#endif /* __DRMTEEPROXYFORMAT_H__ */
