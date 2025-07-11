/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __DRMLICEVAL_H__
#define __DRMLICEVAL_H__

#include <drmselwrestoretypes.h>
#include <drmxmrformatparser.h>
#include <drmcallbacks.h>
#include <drmlicevaltypes.h>

ENTER_PK_NAMESPACE;

DRM_API DRM_RESULT DRM_CALL DRM_LEVL_SearchQualifierGuidInEnablerContainer(
    __in        DRM_GUID const                    *f_pdguidActionQualifier,
    __in        DRM_XB_UNKNOWN_CONTAINER const   *f_pEnablerContainer,
    __in        DRM_WORD const                     f_wContainerType,
    __in        DRM_WORD const                     f_wObjectType,
    __inout     DRM_BOOL                          *f_pfFound,
    __inout_opt DRM_XB_UNKNOWN_CONTAINER const  **f_ppEnablerContainerFound );

DRM_API DRM_RESULT DRM_CALL DRM_LEVL_PerformOperations(
    __inout         DRM_LICEVAL_CONTEXT          *f_pContextLicEval,
    __in            eDRM_LICEVAL_OPERATIONS       f_eOperation,
    __in            eDRM_LICEVAL_OPERATION_STATE  f_eOperationState,
    __in_opt  const DRM_CONST_STRING             *f_pdstrAction,
    __out_opt       DRM_BOOL                     *f_pfPerform,
    __out_opt       DRM_BOOL                     *f_pfActionExisted,
    __in      const DRM_DST                      *f_pDatastore,
    __in_opt        DRMPFNPOLICYCALLBACK          f_pfnPolicyCallback,
    __in_opt  const DRM_VOID                     *f_pv );

DRM_API DRM_RESULT DRM_CALL DRM_LEVL_PerformOperationsXMR(
    __inout     DRM_LICEVAL_CONTEXT          *f_pContextLicEval,
    __in        eDRM_LICEVAL_OPERATIONS       f_eOperation,
    __in        eDRM_LICEVAL_OPERATION_STATE  f_eOperationState,
    __in_opt    DRM_GUID const               *f_pdguidAction,
    __in_opt    DRM_GUID const               *f_pdguidActionQualifier,
    __in        DRM_DWORD const               f_dwRightID,
    __out_opt   DRM_BOOL                     *f_pfPerform,
    __out_opt   DRM_BOOL                     *f_pfActionExisted,
    __in        DRM_DST const                *f_pDatastore,
    __in_opt    DRMPFNPOLICYCALLBACK          f_pfnPolicyCallback,
    __in_opt    DRM_VOID const               *f_pv );

DRM_API DRM_RESULT DRM_CALL DRM_LEVL_IsLicenseReadyForDeletion(
    __inout DRM_LICEVAL_CONTEXT *f_pContextLicEval,
    __out   DRM_BOOL            *f_pfDelete );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_LEVL_GetMachineDatetime(
    __inout     DRM_LICEVAL_CONTEXT *f_pContextLicEval,
    __out       DRM_UINT64          *f_pui64MachineDateTime );

EXIT_PK_NAMESPACE;

#endif /* __DRMLICEVAL_H__ */

