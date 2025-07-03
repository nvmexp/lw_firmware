/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef DRMTEECACHE_H
#define DRMTEECACHE_H 1

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

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_CACHE_Initialize( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_CACHE_AddContext(
    __inout               DRM_TEE_CONTEXT              *f_pContext );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_CACHE_ReferenceContext(
    __inout               DRM_TEE_CONTEXT              *f_pContext );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_CACHE_RemoveContext(
    __inout               DRM_TEE_CONTEXT              *f_pContext );

//
// LWE (kwilson)  In Microsoft PK code, DRM_TEE_CACHE_CheckHash returns DRM_BOOL.
// Lwpu had to change the return value to DRM_RESULT in order to support a return
// code from acquiring/releasing the critical section, which may not always
// succeed. The boolean return value has been moved to the added param f_pfFound.
//
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_CACHE_CheckHash(
    __in            const DRM_SHA256_Digest            *f_pHash,
    __out                 DRM_BOOL                     *f_pfFound );

//
// LWE (kwilson)  In Microsoft PK code, DRM_TEE_CACHE_AddHash returns DRM_VOID.
// Lwpu had to change the return value to DRM_RESULT in order to support a return
// code from acquiring/releasing the critical section, which may not always succeed.
//
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_CACHE_AddHash(
    __in            const DRM_SHA256_Digest            *f_pHash );

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_CACHE_Clear( DRM_VOID );

EXIT_PK_NAMESPACE;

#endif // DRMTEECACHE_H

