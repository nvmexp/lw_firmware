/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef OEMTEEPROXY_H
#define OEMTEEPROXY_H 1

#include <oemteetypes.h>
#include <oemeccp256.h>
#include <drmteetypes.h>
#include <drmxb.h>
#include <drmteeproxy_generated.h>

ENTER_PK_NAMESPACE;

PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_BUFFER_PARAM_25033, "Prefast Noise: inout params should not be const. Functions are implemented by OEM." )
PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_PARAM_25004, "Prefast Noise: inout params should not be const. Functions are implemented by OEM." )

/*
** The maximum size for any message crossing the TEE boundary. By default this value
** is 12KB.  However, if the TEE can handle larger messages, the OEM can change this
** value.
*/
#define OEM_TEE_PROXY_MAX_PRITEE_MESSAGE_SIZE (12 * 1024)
/*
** The default size for pre-allocated PRITEE reponse messages.  This value can be modified
** by the OEM if the default size is too small.
*/
#define OEM_TEE_PROXY_OUTPUT_LENGTH__DEFAULT (2 * 1024)
#define OEM_TEE_PROXY_OUTPUT_LENGTH__MAX     (OEM_TEE_PROXY_MAX_PRITEE_MESSAGE_SIZE - DRM_TEE_XB_PROXY_STACK_SIZE)
#define OEM_TEE_PROXY_SIZEOF_TEE_CONTEXT     (sizeof(OEM_TEE_CONTEXT_INTERNAL) + sizeof(DRM_TEE_CONTEXT))

/*
** TEE Transport Abstraction Layer Client ("normal world") operations
*/

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_PROXY_Initialize( DRM_VOID );
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL OEM_TEE_PROXY_Uninitialize( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_PROXY_MethodIlwoke(
    __in_opt                                DRM_VOID                    *f_pvUserCtx,
    __in                                    DRM_DWORD                    f_dwFunctionMapOEMValue,
    __in                                    DRM_DWORD                    f_cbRequestMessage,
    __inout_bcount( f_cbRequestMessage )    DRM_BYTE                    *f_pbRequestMessage,
    __inout_opt                             DRM_DWORD                   *f_pcbResponseMessage,
    __inout_bcount_part_opt( *f_pcbResponseMessage, *f_pcbResponseMessage )
                                            DRM_BYTE                    *f_pbResponseMessage ) ;

DRM_API DRM_NO_INLINE DRM_RESULT DRM_CALL OEM_TEE_PROXY_TraceIlwokeResults(
    __in_opt                                DRM_VOID                    *f_pvUserCtx,
    __in                                    DRM_DWORD                    f_dwFunctionMapOEMValue,
    __in                                    DRM_DWORD                    f_dwFunctionIdValue,
    __in                                    DRM_RESULT                   f_drMethodIlwoke,
    __in                                    DRM_RESULT                   f_drResponse,
    __in                              const DRM_TEE_BYTE_BLOB           *f_pLog );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_PROXY_GetSerializationRequirements(
    __in_opt                                DRM_VOID                                 *f_pvUserCtx,
    __out_ecount( 1 )                       DRM_TEE_PROXY_SERIALIZATION_REQUIREMENTS *f_pRequirements );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_PROXY_GetMaxResponseMessageSize(
    __in_opt                          const DRM_VOID                    *f_pvUserCtx,
    __in_opt                                DRM_DWORD                    f_dwMethodID,
    __inout_ecount( 1 )                     DRM_DWORD                   *f_pdwEstimatedResponseSize );

PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */
PREFAST_POP /* __WARNING_NONCONST_BUFFER_PARAM_25033 */

EXIT_PK_NAMESPACE;

#endif // OEMTEEPROXY_H
