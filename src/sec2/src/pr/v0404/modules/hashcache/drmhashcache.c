/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <drmhashcache.h>
#include <drmmathsafe.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;
#if defined (SEC_COMPILE)

/*
** Synopsis:
**
** This function initializes the cache used to maintain the set of valid
** DRM_CONTEXT sessions.
**
** Operations Performed:
**
**  1. Build the initial cache layout.
**  2. Set the initial cache parameter values.
**
** Arguments:
**
** f_cPreVerifiedHashes:    (in)    The count of pre-verified hashes.
** f_pPreVerifiedHashes:    (in)    The pre-verified hash values. This
**                                  parameter is used to skip signature
**                                  validation for known good signed
**                                  data.
** f_cCacheBuffer:          (in)    The count of elements in the cache
**                                  buffer (f_pCacheBuffer).
** f_pCacheBuffer:          (out)   The cache buffer where cached values
**                                  will be stored.  The ownership of
**                                  this data is maintained by the caller
**                                  and must not be released while the
**                                  cache is in use.
** f_pCacheCtx:             (out)   The cache context used to maintain
**                                  the internal cache state.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_HASHCACHE_Initialize(
    __in                                    DRM_DWORD                    f_cPreVerifiedHashes,
    __in_ecount(f_cPreVerifiedHashes) const OEM_SHA256_DIGEST           *f_pPreVerifiedHashes,
    __in                                    DRM_DWORD                    f_cCacheBuffer,
    __out_ecount(f_cCacheBuffer)            DRM_CACHED_VERIFIED_HASH    *f_pCacheBuffer,
    __out                                   DRM_HASHCACHE_CONTEXT       *f_pCacheCtx )
{
    DRM_RESULT                      dr      = DRM_SUCCESS;
    DRM_HASHCACHE_CONTEXT_INTERNAL *pCache  = DRM_REINTERPRET_CAST( DRM_HASHCACHE_CONTEXT_INTERNAL, f_pCacheCtx );
    DRM_DWORD                       idx;    /* Loop variable */

    ChkArg( f_cCacheBuffer >  0    );
    ChkArg( f_pCacheCtx     != NULL );
    ChkArg( f_cPreVerifiedHashes < f_cCacheBuffer );

    ChkVOID( OEM_SELWRE_ZERO_MEMORY( f_pCacheBuffer, f_cCacheBuffer ) );
    ChkVOID( OEM_SELWRE_ZERO_MEMORY( f_pCacheCtx, sizeof(*f_pCacheCtx) ) );

    pCache->pVerifiedHashes = f_pCacheBuffer;

    for( idx=0; idx<f_cPreVerifiedHashes; idx++ )
    {
        pCache->pVerifiedHashes[ idx ].oHash = f_pPreVerifiedHashes[ idx ];
        pCache->pVerifiedHashes[ idx ].fValid = TRUE;
    }

    pCache->idxNextFree        = f_cPreVerifiedHashes;
    pCache->cVerifiedHashes    = f_cCacheBuffer;
    pCache->cPreVerifiedHashes = f_cPreVerifiedHashes;
    pCache->fValid             = TRUE;

ErrorExit:
    return dr;
}


/*
** Synopsis:
**
** This function checks whether the given hash of data
** which has already been ECDSA signature-verified is contained within
** the cache.
** If so, it returns TRUE.
**
** Operations Performed:
**
**  1. Search the cache for the given hash.
**  2. If found, return TRUE.
**  3. Otherwise, return FALSE.
**
** Arguments:
**
** f_pCacheCtx: (in)        The cache context used to maintain the internal
**                          cache state.
** f_pHash:     (in)        The hash value to check.
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_HASHCACHE_CheckHash(
    __in            const DRM_HASHCACHE_CONTEXT       *f_pCacheCtx,
    __in            const OEM_SHA256_DIGEST           *f_pHash )
{
    DRM_RESULT                            dr     = DRM_SUCCESS;
    DRM_DWORD                             idx;   /* loop variable */
    DRM_BOOL                              fFound = FALSE;
    const DRM_HASHCACHE_CONTEXT_INTERNAL *pCache = DRM_REINTERPRET_CONST_CAST( const DRM_HASHCACHE_CONTEXT_INTERNAL, f_pCacheCtx );

    ChkArg( f_pCacheCtx != NULL );
    ChkArg( f_pHash     != NULL );
    ChkArg( pCache->fValid );

    DRMCASSERT( sizeof( *f_pHash ) == sizeof( pCache->pVerifiedHashes[ 0 ].oHash ) );

    for( idx = 0; idx < pCache->cVerifiedHashes; idx++ )
    {
        if( pCache->pVerifiedHashes[ idx ].fValid &&
            OEM_SELWRE_ARE_EQUAL( &pCache->pVerifiedHashes[ idx ].oHash, f_pHash, sizeof( *f_pHash ) ) )
        {
            fFound = TRUE;
            break;
        }
    }

ErrorExit:
    return DRM_SUCCEEDED(dr) && fFound;
}


/*
** Synopsis:
**
** This function adds the given hash of data
** which has already been ECDSA signature-verified to
** the cache.
**
** Operations Performed:
**
**  1. Add the hash to the cache.
**
** Arguments:
**
** f_pCacheCtx: (in/out)    The cache context used to maintain the internal
**                          cache state.
** f_pHash:     (in)        The hash value to add.
*/
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_HASHCACHE_AddHash(
    __inout               DRM_HASHCACHE_CONTEXT       *f_pCacheCtx,
    __in            const OEM_SHA256_DIGEST           *f_pHash )
{
    DRM_RESULT                      dr        = DRM_SUCCESS;
    DRM_DWORD                      *pIdxFree; /* initialized by assignment inside critsec */
    DRM_HASHCACHE_CONTEXT_INTERNAL *pCache    = DRM_REINTERPRET_CAST( DRM_HASHCACHE_CONTEXT_INTERNAL, f_pCacheCtx );

    ChkArg( f_pCacheCtx != NULL );
    ChkArg( f_pHash     != NULL );
    ChkArg( pCache->fValid );

    pIdxFree = &pCache->idxNextFree;

    /* Add the hash to the cache so we don't have to verify it again. */
    pCache->pVerifiedHashes[ *pIdxFree ].oHash = *f_pHash;
    pCache->pVerifiedHashes[ *pIdxFree ].fValid = TRUE;

    /*
    ** Determine which slot in the cache is the next one we should use.
    ** Simple cirlwlar cache: if the last entry is full, we restart with the first entry.
    ** The "first entry" is actually the first entry after any pre-verified hashes we have.
    */
    *pIdxFree = ( *pIdxFree + 1 );
    if( *pIdxFree >= pCache->cVerifiedHashes )
    {
        *pIdxFree = pCache->cPreVerifiedHashes;
    }

ErrorExit:
    DRMASSERT(DRM_SUCCEEDED(dr));
    (DRM_VOID)dr;
    return;
}
#endif
#ifdef NONE
/*
** Synopsis:
**
** This function clears the caches for verified hashes and
** for a verified RevInfo plus runtime CRL.  This function
** does not remove the pre-verified hash values.
**
** Operations Performed:
**
**  1. Clear the cache (except for pre-verified cache entries).
**
** Arguments:
**
** f_pCacheCtx: (in/out)    The cache context used to maintain the internal
**                          cache state.
*/
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_HASHCACHE_Clear(
    __inout               DRM_HASHCACHE_CONTEXT     *f_pCacheCtx )
{
    DRM_HASHCACHE_CONTEXT_INTERNAL *pCache = DRM_REINTERPRET_CAST( DRM_HASHCACHE_CONTEXT_INTERNAL, f_pCacheCtx );

    if( f_pCacheCtx != NULL && pCache->fValid )
    {
        DRM_DWORD cbCache = pCache->cVerifiedHashes;

        if( pCache->cPreVerifiedHashes <= cbCache )
        {
            cbCache -= pCache->cPreVerifiedHashes;      /* Can't underflow: Already verified that pCache->cPreVerifiedHashes <= cbCache */
        }
        else
        {
            /*
            ** We should never get here becauase we verify the count of pre-verified hashes
            ** is less than the max cache size in DRM_HASHCACHE_Initialize.
            */
            DRMASSERT(FALSE);
            pCache->cPreVerifiedHashes = 0;
        }

        if( DRM_SUCCEEDED( DRM_DWordMult( cbCache, sizeof(*pCache->pVerifiedHashes), &cbCache ) ) )
        {
            ChkVOID( OEM_SELWRE_ZERO_MEMORY( &pCache->pVerifiedHashes[pCache->cPreVerifiedHashes], cbCache ) );
        }
        else
        {
            DRMASSERT(FALSE);
        }
    }
}
#endif
EXIT_PK_NAMESPACE_CODE;

