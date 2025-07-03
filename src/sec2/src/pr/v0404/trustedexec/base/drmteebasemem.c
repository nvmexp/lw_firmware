/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <drmtee.h>
#include <drmerr.h>
#include <oemtee.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

#if defined(SEC_COMPILE)
/*
** Synopsis:
**
** The secure-world version of this function allocates (or weak-references)
** memory in secure-world space.
** The inselwre-world version of this function allocates (or weak-references)
** memory in inselwre-world space.
** This function should never cross the secure/inselwre boundary.
**
** Operations Performed:
**
**  1. Allocate the given number of bytes.
**
** Arguments:
**
** f_pContext:      (in/out) The TEE context returned from
**                           DRM_TEE_BASE_AllocTEEContext.
** f_cb:            (in)     The number of bytes to allocate.
** f_ppv:           (out)    A pointer to the memory allocated
**                           from this call.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_MemAlloc(
    __inout_opt                 DRM_TEE_CONTEXT              *f_pContext,
    __in                        DRM_DWORD                     f_cb,
    __deref_out_bcount(f_cb)    DRM_VOID                    **f_ppv )
{
    DRM_RESULT       dr         = DRM_SUCCESS;
    OEM_TEE_CONTEXT *pOemTeeCtx = NULL;

    ChkArg( f_ppv != NULL );
    ChkArg( f_cb   > 0    );

    if( f_pContext != NULL )
    {
        pOemTeeCtx = &f_pContext->oContext;
    }

    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, f_cb, f_ppv ) );

ErrorExit:
    return dr;
}

/*
** Synopsis:
**
** This function frees the given memory.
** This function should never cross the secure/inselwre boundary.
** The proxy for the normal world side will implement the corresponding
** non-secure memory allocation method.
**
** Operations Performed:
**
**  1. Free the given memory.
**
** Arguments:
**
** f_pContext:      (in/out) The TEE context returned from
**                           DRM_TEE_BASE_AllocTEEContext.
** f_ppv:           (in/out) The memory to free.
**
*/
DRM_NO_INLINE DRM_API_VOID DRM_VOID   DRM_CALL DRM_TEE_IMPL_BASE_MemFree(
    __inout_opt                 DRM_TEE_CONTEXT              *f_pContext,
    __inout                     DRM_VOID                    **f_ppv )
{
    DRMASSERT( f_ppv != NULL );

    if( (f_ppv != NULL) )
    {
        OEM_TEE_CONTEXT *pOemTeeCtx = NULL;

        if( f_pContext != NULL )
        {
            pOemTeeCtx = &f_pContext->oContext;
        }

        ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, f_ppv ) );
    }
}

/*
** Synopsis:
**
** The secure-world version of this function allocates (or weak-references)
** memory in secure-world space.
** The inselwre-world version of this function allocates (or weak-references)
** memory in inselwre-world space.
** This function should never cross the secure/inselwre boundary.
**
** Operations Performed:
**
**  1. Verify the signature on the given DRM_TEE_CONTEXT.
**  2. Allocate a blob using the given behavior.
**
** Arguments:
**
** f_pContext:      (in/out) The TEE context returned from
**                           DRM_TEE_BASE_AllocTEEContext.
** f_eBehavior:     (in)     The style of allocation to use.
** f_cb:            (in)     The number of bytes to allocate
**                           and/or the number of bytes in f_pb.
** f_pb:            (in)     Pointer to bytes to copy or take
**                           a weak reference.  Must be NULL
**                           if f_eBehavior == DRM_TEE_BLOB_ALLOC_BEHAVIOR_NEW.
**                           Must be non-NULL otherwise.
** f_pBlob:         (in/out) On output, eType is set to
**                           DRM_TEE_BYTE_BLOB_TYPE_USER_MODE or
**                           DRM_TEE_BYTE_BLOB_TYPE_WEAK_REF
**                           (depending on f_eBehavior),
**                           dwSubType is set to 0, and pb is set
**                           to memory allocated in user mode or
**                           a direct pointer to f_pb.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_AllocBlob(
    __inout_opt                 DRM_TEE_CONTEXT              *f_pContext,
    __in                        DRM_TEE_BLOB_ALLOC_BEHAVIOR   f_eBehavior,
    __in                        DRM_DWORD                     f_cb,
    __in_bcount_opt(f_cb) const DRM_BYTE                     *f_pb,
    __inout                     DRM_TEE_BYTE_BLOB            *f_pBlob )
{
    DRM_RESULT       dr         = DRM_SUCCESS;
    OEM_TEE_CONTEXT *pOemTeeCtx = NULL;

    /*
    ** This function should NEVER cross the secure boundary. The proxy in the normal world will
    ** will duplicate this method with normal world allocations.
    */
    ChkArg( f_pBlob    != NULL );

    ChkArg( IsBlobEmpty( f_pBlob ) );

    if( f_pContext != NULL )
    {
        pOemTeeCtx = &f_pContext->oContext;
    }

    switch( f_eBehavior )
    {
    case DRM_TEE_BLOB_ALLOC_BEHAVIOR_NEW:
        ChkArg( f_cb  > 0 );
        ChkArg( f_pb == NULL );
        ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, f_cb, ( DRM_VOID ** )&f_pBlob->pb ) );
        f_pBlob->cb        = f_cb;
        f_pBlob->eType     = DRM_TEE_BYTE_BLOB_TYPE_USER_MODE;
        f_pBlob->dwSubType = 0;
        break;
    case DRM_TEE_BLOB_ALLOC_BEHAVIOR_COPY:
        ChkArg( f_cb  > 0 );
        ChkArg( f_pb != NULL );
        ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, f_cb, ( DRM_VOID ** )&f_pBlob->pb ) );
        ChkVOID( OEM_TEE_MEMCPY( (DRM_BYTE*)f_pBlob->pb, f_pb, f_cb ) );
        f_pBlob->cb        = f_cb;
        f_pBlob->eType     = DRM_TEE_BYTE_BLOB_TYPE_USER_MODE;
        f_pBlob->dwSubType = 0;
        break;
    case DRM_TEE_BLOB_ALLOC_BEHAVIOR_WEAKREF:
        ChkArg( ( f_pb == NULL ) == ( f_cb == 0 ) );
        f_pBlob->pb        = f_pb;
        f_pBlob->cb        = f_cb;
        f_pBlob->eType     = DRM_TEE_BYTE_BLOB_TYPE_WEAK_REF;
        f_pBlob->dwSubType = 0;
        break;
    default:
        ChkArgError();
        break;
    }

ErrorExit:
    return dr;
}

/*
** Synopsis:
**
** This function frees the given blob.
**
** Operations Performed:
**
**  1. Free the given blob.
**
** Arguments:
**
** f_pContext:      (in/out) The TEE context returned from
**                           DRM_TEE_BASE_AllocTEEContext.
** f_pBlob:         (in/out) The Blob to free.
**                           On output, cb is set to 0 and pb is set to NULL.
**                           Note that this oclwrs even if f_pBlob->eType
**                           is DRM_TEE_BYTE_BLOB_TYPE_WEAK_REF, i.e.
**                           the caller is expected to have their own pointer
**                           f_pBlob->pb which they can free
**                           (if they have not already freed it).
**                           f_pBlob->eType must be one of the following values:
**                           DRM_TEE_BYTE_BLOB_TYPE_ILWALID
**                            (No operation is performed)
**                           DRM_TEE_BYTE_BLOB_TYPE_WEAK_REF
**                            (No operation is performed)
**                           DRM_TEE_BYTE_BLOB_TYPE_USER_MODE
**                            (Memory is freed)
**                           DRM_TEE_BYTE_BLOB_TYPE_CCD
**                            (Freed using OEM_TEE_BASE_FreeBlob)
**
** LWE (kwilson) - In Microsoft PK code, DRM_TEE_BASE_FreeBlob returns DRM_VOID.
**                 Lwpu had to change the return value to DRM_RESULT in order
**                 to support a return code from acquiring/releasing the critical
**                 section, which may not always succeed.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_FreeBlob(
    __inout_opt           DRM_TEE_CONTEXT              *f_pContext,
    __inout               DRM_TEE_BYTE_BLOB            *f_pBlob )
{
    OEM_TEE_CONTEXT *pOemTeeCtx = NULL;
    DRM_RESULT       dr = DRM_SUCCESS;

    /*
    ** This function should NEVER cross the secure boundary. The proxy in the normal world will
    ** will duplicate this method.
    */
    DRMASSERT( f_pBlob != NULL );

    if( f_pContext != NULL )
    {
        pOemTeeCtx = &f_pContext->oContext;
    }

    if( f_pBlob != NULL )
    {
        if( f_pBlob->eType == DRM_TEE_BYTE_BLOB_TYPE_ILWALID )
        {
            /* Free allows invalid because memory might never have been allocated, e.g. on failure. */
            DRMASSERT( f_pBlob->pb          == NULL );
            DRMASSERT( f_pBlob->cb          == 0    );
            DRMASSERT( f_pBlob->dwSubType   == 0    );
        }
        else
        {
            DRMASSERT( IsBlobConsistent( f_pBlob ) );
            if( f_pBlob->pb != NULL && f_pBlob->cb != 0 )
            {
                switch( f_pBlob->eType )
                {
                case DRM_TEE_BYTE_BLOB_TYPE_WEAK_REF:
                    /* no-op */
                    break;
                case DRM_TEE_BYTE_BLOB_TYPE_USER_MODE:
                    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, ( DRM_VOID ** )&f_pBlob->pb ) );
                    break;
                case DRM_TEE_BYTE_BLOB_TYPE_CCD:
                    ChkDR( OEM_TEE_BASE_SelwreMemHandleFree( pOemTeeCtx, (OEM_TEE_MEMORY_HANDLE*)&f_pBlob->pb ) );
                    break;
                case DRM_TEE_BYTE_BLOB_TYPE_SELWRED_MODE:   __fallthrough;      /* Caller should never have seen this type! */
                default:
                    DRMASSERT( FALSE );
                    break;
                }
            }
        }
        ChkVOID( OEM_TEE_ZERO_MEMORY( f_pBlob, sizeof(*f_pBlob) ) );
    }

ErrorExit:
    return dr;
}

/*
** Synopsis:
**
** This function transfers the ownership of one blob to another.
** No memory is allocated by this function since the transfer is
** just a reassignment of a pointer. This function lives ONLY inside the TEE.
**
** Operations Performed:
**
**  1. Copy source DRM_TEE_BYTE_BLOB structure to destination blob.
**  2. Zero the source blob object so that it doesn't reference
**     the blob data.
**
** Arguments:
**
** f_pDest:          (out) The destination TEE blob object
** f_pSource:        (in/out) The source TEE blob object
**
*/
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_IMPL_BASE_TransferBlobOwnership(
    __inout                            DRM_TEE_BYTE_BLOB          *f_pDest,
    __inout                            DRM_TEE_BYTE_BLOB          *f_pSource )
{
    DRMASSERT( f_pDest     != NULL );
    DRMASSERT( f_pSource   != NULL );

    if( f_pDest != NULL && f_pSource != NULL )
    {
        DRMASSERT( f_pDest->pb == NULL ); /* Catch memory leaks */

        ChkVOID( OEM_TEE_MEMCPY( f_pDest, f_pSource, sizeof(*f_pDest) ) );
        ChkVOID( OEM_TEE_ZERO_MEMORY( f_pSource, sizeof(*f_pSource) ) );
    }
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_AllocBlobAndTakeOwnership(
    __inout_opt                                           DRM_TEE_CONTEXT              *f_pContext,
    __in                                                  DRM_DWORD                     f_cb,
    __deref_inout_bcount(f_cb)                            DRM_BYTE                    **f_ppb,
    __inout                                               DRM_TEE_BYTE_BLOB            *f_pBlob )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_ppb  != NULL );
    ChkArg( *f_ppb != NULL );
    ChkArg( f_cb   > 0 );

    ChkDR( DRM_TEE_IMPL_BASE_AllocBlob(
        f_pContext,
        DRM_TEE_BLOB_ALLOC_BEHAVIOR_WEAKREF,
        f_cb,
       *f_ppb,
        f_pBlob ) );
    f_pBlob->eType     = DRM_TEE_BYTE_BLOB_TYPE_USER_MODE;
    f_pBlob->dwSubType = 0;

    *f_ppb = NULL; /* Clear the original pointer */
ErrorExit:
    return dr;
}
#endif

EXIT_PK_NAMESPACE_CODE;

