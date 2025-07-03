/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#define DRM_BUILDING_OEMECCP160IMPL_C
#include <drmprofile.h>
#include <oemsha1.h>
#include <oem.h>
#include <oembroker.h>
#include <oemeccp256impl.h>
#include <oemcryptoctx.h>
#include <oembyteorder.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;
#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_ECC_InitializeBignumStackOEMCtxImpl(
    __inout DRM_VOID* f_pContext,
    __in DRM_DWORD f_cbContextSize,
    __in_opt DRM_VOID* f_pOEMContext )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_DWORD  dwStackSize;

    DRMBIGNUM_CONTEXT_STRUCT *pContext = (DRMBIGNUM_CONTEXT_STRUCT*) f_pContext;
    DRMCASSERT( sizeof( DRMBIGNUM_CONTEXT_STRUCT ) <= DRM_PKCRYPTO_CONTEXT_BUFFER_SIZE );

    ChkArg( f_pContext != NULL );
    ChkArg( f_cbContextSize > sizeof( DRMBIGNUM_CONTEXT_STRUCT ) );

    /* stack size is the size of the full context minus the part of context that is not rgbHeap */
    DRMCASSERT( sizeof(pContext->rgbHeap) == 1);
    dwStackSize = f_cbContextSize - (sizeof(*pContext) - sizeof(pContext->rgbHeap));

    ChkDR( DRM_STK_Init( &pContext->oHeap, pContext->rgbHeap, dwStackSize, TRUE ) );

    if ( f_pOEMContext != NULL )
    {
        pContext->oContext.pOEMContext = f_pOEMContext;
    }

ErrorExit:
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_ECC_InitializeBignumStackImpl(
    __inout DRM_VOID* f_pContext,
    __in DRM_DWORD f_cbContextSize )
{
    return OEM_ECC_InitializeBignumStackOEMCtxImpl( f_pContext, f_cbContextSize, NULL );
}

DRM_NO_INLINE DRM_API_VOID DRM_VOID* DRM_CALL bignum_alloc(__in const DRM_DWORD cblen, __in struct bigctx_t *f_pBigCtx )
{
    DRM_VOID *pbRet = NULL;

    DRMASSERT(  f_pBigCtx != NULL );

    if( DRM_FAILED( DRM_STK_Alloc( DRM_REINTERPRET_CAST( DRM_STACK_ALLOCATOR_CONTEXT, f_pBigCtx ), (DRM_DWORD)cblen, &pbRet ) ) )
    {
        return NULL;
    }

    return pbRet;
}

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL bignum_free( __in DRM_VOID *pvMem, __inout struct bigctx_t *f_pBigCtx )
{
    if( pvMem != NULL &&  f_pBigCtx != NULL )
    {
        (DRM_VOID)DRM_STK_Free( DRM_REINTERPRET_CAST( DRM_STACK_ALLOCATOR_CONTEXT, f_pBigCtx ), pvMem );
    }
}

/*
**  random_bytes is a function bignum code calls when it needs random bytes.
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL random_bytes(
    __out_bcount( nbyte )         DRM_BYTE          *byte_array,
    __in                    const DRM_DWORD          nbyte,
    __inout                       struct bigctx_t   *f_pBigCtx )
{
    /*
    ** BIGNUM context is passed all around BIGNUM code.
    ** Convert to proper structure and get pOEMContext.
    ** pOEMContext can be NULL on certain cases, for example in tools. It is set in Drm_Initialize
    ** If Drm_Initialize is called, then pBigNumCtx is present and pOEMContext also present.
    */

    DRMBIGNUM_CONTEXT_STRUCT *pBigNumCtx     = DRM_REINTERPRET_CAST( DRMBIGNUM_CONTEXT_STRUCT, f_pBigCtx );
    DRM_VOID                 *pOEMContext    = NULL;
    DRM_VOID                 *pOEMTEEContext = NULL;

    DRMASSERT( pBigNumCtx != NULL );

    if ( pBigNumCtx != NULL )
    {
        /*
        ** The BIGNUM context has mutually exclusive pointers to both an OEM context and an OEM TEE context.
        ** To increase code readability elsewhere, these pointers are represented in a union in the BIGNUM struct
        ** to avoid a single multi-purpose void ptr. In this particular case where we are calling the Oem_Broker
        ** we pass the two named members of the union to Oem_Broker_Random_GetBytes with the expectation that
        ** the normal world OEM or the TEE version of the underlying function will be invoked based on linkage.
        **
        ** In the call to Oem_Broker_Random_GetBytes one of the context pointers will necesarily be invalid with
        ** respect to the type that its name implies. This is acceptable because the Oem_Broker knows which
        ** pointer to use based on linkage of the normal world vs. tee broker implmentation.
        */
        pOEMContext = pBigNumCtx->oContext.pOEMContext;
        pOEMTEEContext = pBigNumCtx->oContext.pOEMTEEContext;
    }

    if( DRM_SUCCEEDED( Oem_Broker_Random_GetBytes( pOEMTEEContext, pOEMContext, byte_array, (DRM_DWORD)nbyte ) ) )
    {
        return TRUE;
    }

    return FALSE;
}
#endif

EXIT_PK_NAMESPACE_CODE;

