/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef _DRMTEECACHE_H_
#define _DRMTEECACHE_H_ 1

#include <drmteetypes.h>
#include <oemsha256types.h>
#include <oemeccp256.h>

ENTER_PK_NAMESPACE;

// LWE (nkuo) - following defines and type are referenced in OEM_TEE_InitSharedDataStruct() so we need to expose them to public
#define DRM_TEE_CACHE_HEAD_ENTRY  ((DRM_BYTE)(OEM_TEE_CACHE_MAX_ENTRIES + 1))
#define DRM_TEE_CACHE_TAIL_ENTRY  ((DRM_BYTE)(OEM_TEE_CACHE_MAX_ENTRIES + 2))
#define DRM_TEE_CACHE_FREE_ENTRY  ((DRM_BYTE)(OEM_TEE_CACHE_MAX_ENTRIES + 3))

#define TEE_CACHE_INITIALIZED     ((DRM_BYTE)1)
#define TEE_CACHE_UNINITIALIZED   ((DRM_BYTE)0)

typedef struct __tagDRM_TEE_CACHE_ENTRY
{
    DRM_ID    idSession;
    DRM_BYTE  iPrev;
    DRM_BYTE  iNext;
} DRM_TEE_CACHE_ENTRY;

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_CACHE_Initialize( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_CACHE_AddContext(
    __in            const DRM_TEE_CONTEXT              *f_pContext );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_CACHE_ReferenceContext(
    __in            const DRM_TEE_CONTEXT              *f_pContext );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_CACHE_RemoveContext(
    __in            const DRM_TEE_CONTEXT              *f_pContext );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_CACHE_CheckHash(
    __in            const OEM_SHA256_DIGEST            *f_pHash,
    __out                 DRM_BOOL                     *f_pfHashFound );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_CACHE_AddHash(
    __in            const OEM_SHA256_DIGEST            *f_pHash );

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_IMPL_CACHE_Clear( DRM_VOID );

EXIT_PK_NAMESPACE;

#endif /* _DRMTEECACHE_H_ */

