/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#define DRM_BUILDING_DRMSTKALLOCIMPLREAL_C
#include <drmstkalloc.h>
#include <drmbytemanip.h>
#include <drmmathsafe.h>
#include <drmerr.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

#if defined(SEC_COMPILE)
/*
** Push the stack to allocate the requested size of buffer
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_STK_Alloc(
    __inout                      DRM_STACK_ALLOCATOR_CONTEXT *pContext,
    __in                         DRM_DWORD                    cbSize,
    __deref_out_bcount( cbSize ) DRM_VOID                   **ppbBuffer )
{
    DRM_RESULT dr     = DRM_SUCCESS;
    DRM_DWORD  dwSize;
    DRM_DWORD  dwNewStackTop; /* Initialized in DRM_DwordAdd later */

    DRMCASSERT( DRM_IS_INTEGER_POWER_OF_TW0( sizeof(DRM_SIZE_T) ) );

    ChkArg( pContext  != NULL );
    ChkArg( ppbBuffer != NULL );
    ChkArg( cbSize     > 0 );

    *ppbBuffer = NULL;

    dwSize = cbSize;
    /* adjust cbSize for alignment of DRM_DWORD */
    /* Power of two: use & instead of % for better perf */
    if( ( cbSize & ( sizeof(DRM_SIZE_T) - 1 ) ) != 0 )
    {
        ChkDR( DRM_DWordAddSame( &dwSize, sizeof(DRM_SIZE_T) ) );
        dwSize -= ( dwSize & ( sizeof(DRM_SIZE_T) - 1 ) );
    }

    ChkDR( DRM_DWordAdd( dwSize, sizeof(DRM_SIZE_T), &dwNewStackTop ) );
    ChkDR( DRM_DWordAddSame( &dwNewStackTop, pContext->nStackTop ) );

    if( dwNewStackTop > pContext->cbStack )    /* checks that will be enough space*/
    {
        ChkDR( DRM_E_OUTOFMEMORY );
    }

    pContext->pdwStack[pContext->nStackTop>>2] = dwSize;

    /* *ppbBuffer = (DRM_BYTE*) pContext->pdwStack + pContext->nStackTop + sizeof(DRM_SIZE_T) */
    ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)pContext->pdwStack, pContext->nStackTop, (DRM_SIZE_T*)ppbBuffer ) );
    ChkDR( DRM_DWordPtrAddSame( (DRM_SIZE_T*)ppbBuffer, sizeof(DRM_SIZE_T) ) );

    pContext->nStackTop = dwNewStackTop;
    /*
    DRM_DBG_TRACE(("Alloc: top=0x%08X  pb=0x%08X  cb=%d\n", pContext->nStackTop, *ppbBuffer, dwSize));
    */
#if DRM_DBG
    if( !pContext->fWasPreAlloc )
    {
        /* clear the buffer */
        MEMSET((DRM_BYTE*)*ppbBuffer, 0xa, dwSize);
    }
#endif  /* DRM_DBG */
    pContext->fWasPreAlloc = FALSE;

ErrorExit:
    return dr;
}


/*
** Pop the stack to free the allocated buffer
*/
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_COUNT_REQUIRED_FOR_VOIDPTR_BUFFER, "pvBuffer points into internal the internal buffer and its size is defined by *(pvBuffer-sizeof(DRM_SIZE_T))" )
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_STK_Free(
    __in DRM_STACK_ALLOCATOR_CONTEXT *pContext,
    __in DRM_VOID                    *pvBuffer )
PREFAST_POP  /* __WARNING_COUNT_REQUIRED_FOR_VOIDPTR_BUFFER */
{
    DRM_RESULT      dr      = DRM_SUCCESS;
    DRM_DWORD       dwSize;

    if( pvBuffer == NULL )
    {
        /* pvBuffer is NULL, simply return */
        ChkDR( DRM_E_LOGICERR );
    }

    ChkArg( pContext != NULL );

    if( (((DRM_SIZE_T)pvBuffer) & (sizeof(DRM_SIZE_T)-1)) != 0 )
    {
        /* stack allocated pointers are DWORD aligned */
        ChkDR( DRM_E_LOGICERR );
    }

PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_BUFFER_UNDERFLOW_26001, "pvBuffer points into internal the internal buffer and its size is defined by *(pvBuffer-sizeof(DRM_SIZE_T))" )
    dwSize = *(const DRM_DWORD*)(((const DRM_BYTE*)pvBuffer)-sizeof(DRM_SIZE_T));
PREFAST_POP  /* __WARNING_BUFFER_UNDERFLOW_26001 */

    /* verify the buffer with the stack */
    /*
    DRM_DBG_TRACE((" Free: top=0x%08X  pb=0x%08X  cb=%d\n", pContext->nStackTop, pvBuffer, dwSize));
    */
    if( ((DRM_BYTE*)pvBuffer + (dwSize)) != (DRM_BYTE*)(pContext->pdwStack) + pContext->nStackTop )
    {
        DRM_DBG_TRACE(("\n\n***  DRM_STK_Free(): heap corrupted ***\n\n"));
        DRMASSERT( FALSE );
        ChkDR( DRM_E_STACK_CORRUPT ); /* internal stack corrupted */
    }

#if DRM_DBG
    /* clear the buffer */
    MEMSET((DRM_BYTE*)pvBuffer-sizeof(DRM_SIZE_T), 0xb, dwSize+sizeof(DRM_SIZE_T));
#endif  /* DRM_DBG */

    pContext->nStackTop -= (DRM_DWORD)(dwSize+sizeof(DRM_SIZE_T));

ErrorExit:
    return dr;
}

/* Init the stack.
*/

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_STK_Init(
    __inout                 DRM_STACK_ALLOCATOR_CONTEXT *pContext,
    __in_bcount( cbSize)    DRM_BYTE                    *pbBuffer,
    __in                    DRM_DWORD                    cbSize,
    __in                    DRM_BOOL                     fForceInit )

{   DRM_RESULT dr = DRM_SUCCESS;
    DRM_SIZE_T   pdwTemp;
    ChkArg( pContext != NULL && pbBuffer != NULL  && cbSize != 0  );

    /*  Check that stack was not initilized. */
    if( !fForceInit && pContext->pdwStack != NULL )
    {
        ChkDR( DRM_E_STACK_ALREADY_INITIALIZED );
    }

#if DRM_DBG
    /* Set all memory to 0xcd, so we can see un-allocated memory. */
    MEMSET( pbBuffer, 0xCD, cbSize );
#endif  /* DRM_DBG */

    pdwTemp = (DRM_SIZE_T)pbBuffer;
    if( (pdwTemp&(sizeof(DRM_SIZE_T)-1)) != 0 )
    {
        DRM_DWORD dwDifference = sizeof(DRM_SIZE_T)-(pdwTemp&(sizeof(DRM_SIZE_T)-1));

        ChkBOOL( dwDifference < cbSize, DRM_E_OUTOFMEMORY );
        cbSize -= dwDifference;
        pdwTemp += dwDifference;
    }

    pContext->pdwStack = (DRM_DWORD*)pdwTemp;
    pContext->cbStack = cbSize;
    pContext->nStackTop = 0;
    pContext->fWasPreAlloc = FALSE;

ErrorExit:
    return dr;
}
#endif

EXIT_PK_NAMESPACE_CODE;
