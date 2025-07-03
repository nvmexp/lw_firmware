/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __DRMHASHCACHE_H__
#define __DRMHASHCACHE_H__ 1

#include <drmtypes.h>
#include <oemsha256types.h>
#include <oemeccp256.h>

ENTER_PK_NAMESPACE;

typedef struct __tagDRM_CACHED_VERIFIED_HASH
{
    DRM_BOOL            fValid;
    OEM_SHA256_DIGEST   oHash;
} DRM_CACHED_VERIFIED_HASH;

typedef struct __tagDRM_HASHCACHE_CONTEXT_INTERNAL
{
    DRM_BOOL                  fValid;
    DRM_DWORD                 idxNextFree;
    DRM_DWORD                 cPreVerifiedHashes;
    DRM_DWORD                 cVerifiedHashes;
    DRM_CACHED_VERIFIED_HASH *pVerifiedHashes;
} DRM_HASHCACHE_CONTEXT_INTERNAL;


typedef struct __tagDRM_HASHCACHE_CONTEXT
{
    DRM_BYTE rgbContext[sizeof(DRM_HASHCACHE_CONTEXT_INTERNAL)];
} DRM_HASHCACHE_CONTEXT;

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_HASHCACHE_Initialize(
    __in                                    DRM_DWORD                    f_cPreVerifiedHashes,
    __in_ecount(f_cPreVerifiedHashes) const OEM_SHA256_DIGEST           *f_pPreVerifiedHashes,
    __in                                    DRM_DWORD                    f_cCacheBuffer,
    __out_ecount(f_cCacheBuffer)            DRM_CACHED_VERIFIED_HASH    *f_pCacheBuffer,
    __out                                   DRM_HASHCACHE_CONTEXT       *f_pCacheCtx );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_HASHCACHE_CheckHash(
    __in                              const DRM_HASHCACHE_CONTEXT       *f_pCacheCtx,
    __in                              const OEM_SHA256_DIGEST           *f_pHash );

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_HASHCACHE_AddHash(
    __inout                                 DRM_HASHCACHE_CONTEXT       *f_pCacheCtx,
    __in                              const OEM_SHA256_DIGEST           *f_pHash );

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_HASHCACHE_Clear(
    __inout                                 DRM_HASHCACHE_CONTEXT       *f_pCacheCtx );

EXIT_PK_NAMESPACE;

#endif /* __DRMHASHCACHE_H__ */

