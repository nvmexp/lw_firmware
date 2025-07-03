/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __OEMSHA256_H__
#define __OEMSHA256_H__ 1

#include <oemsha256types.h>

ENTER_PK_NAMESPACE;

DRM_API DRM_RESULT DRM_CALL OEM_SHA256_Init(
    __out_ecount( 1 ) OEM_SHA256_CONTEXT *f_pShaContext );

DRM_API DRM_RESULT DRM_CALL OEM_SHA256_Update(
    __inout_ecount( 1 )             OEM_SHA256_CONTEXT *f_pShaContext,
    __in_ecount( f_cbBuffer ) const DRM_BYTE            f_rgbBuffer[],
    __in                            DRM_DWORD           f_cbBuffer );

DRM_API DRM_RESULT DRM_CALL OEM_SHA256_UpdateOffset(
    __inout_ecount( 1 )                                         OEM_SHA256_CONTEXT *f_pShaContext,
    __in_ecount( f_cbBufferRemaining + f_ibBufferOffset ) const DRM_BYTE            f_rgbBuffer[],
    __in                                                        DRM_DWORD           f_cbBufferRemaining,
    __in                                                        DRM_DWORD           f_ibBufferOffset )
GCC_ATTRIB_NO_STACK_PROTECT();

DRM_API DRM_RESULT DRM_CALL OEM_SHA256_Finalize(
    __inout_ecount( 1 ) OEM_SHA256_CONTEXT *f_pContext,
    __out_ecount( 1 )   OEM_SHA256_DIGEST  *f_pDigest )
GCC_ATTRIB_NO_STACK_PROTECT();

EXIT_PK_NAMESPACE;

#endif /* #ifndef __OEMSHA256_H__ */

