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

#ifdef NONE
/*
** Returns a pointer to where the next call to Alloc will allocate memory from, but doesn't
** actually allocate yet. Useful if the size of the buffer desired is not known in advance.
*/
DRM_API DRM_RESULT DRM_CALL DRM_STK_PreAlloc(
    __in                           DRM_STACK_ALLOCATOR_CONTEXT   *pContext,
    __out                          DRM_DWORD                     *pcbSize,
    __deref_out_bcount( *pcbSize ) DRM_VOID                     **ppbBuffer )
{
    DRM_RESULT      dr              = DRM_SUCCESS;
    DRM_DWORD       dwOffset        = 0;

    ChkArg( pContext  != NULL
         && ppbBuffer != NULL
         && pcbSize   != NULL );

    /*
    ** The following mathsafe function is equivalent to:
    ** dwOffset = pContext->nStackTop + sizeof(DRM_DWORD_PTR)
    ** *ppbBuffer = (DRM_BYTE*)pContext->pdwStack + dwOffset
    */
    ChkDR( DRM_DWordAdd( pContext->nStackTop, sizeof(DRM_DWORD_PTR), &dwOffset ) );
    ChkDR( DRM_DWordPtrAdd( (DRM_DWORD_PTR)pContext->pdwStack, dwOffset, (DRM_DWORD_PTR*)ppbBuffer ) );

    /*
    ** The following mathsafe function is equivalent to:
    ** *pcbSize = pContext->cbStack - (pContext->nStackTop + sizeof(DRM_DWORD_PTR));
    */
    ChkDR( DRM_DWordSub( pContext->cbStack, dwOffset, pcbSize ) );

    pContext->fWasPreAlloc = TRUE;

#if DRM_DBG
    /* clear the buffer */
    MEMSET((DRM_BYTE*)*ppbBuffer, 0xa, *pcbSize);
#endif  /* DRM_DBG */

ErrorExit:
    return dr;
}
#endif
#if defined(SEC_COMPILE)
/*
** Push the stack to allocate the requested size of buffer
*/
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_ILWALID_PARAM_VALUE_1, "*ppbBuffer should not yield this warning given __checkReturn and declaration of DRM_RESULT, but it still does." )
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_STK_Alloc(
    __inout                      DRM_STACK_ALLOCATOR_CONTEXT *pContext,
    __in                         DRM_DWORD                    cbSize,
    __deref_out_bcount( cbSize ) DRM_VOID                   **ppbBuffer )
PREFAST_POP /* __WARNING_ILWALID_PARAM_VALUE_1 */
{
    DRM_RESULT dr     = DRM_SUCCESS;
    DRM_DWORD  dwSize;
    DRM_DWORD  dwNewStackTop; /* Initialized in DRM_DwordAdd later */

    DRMCASSERT( DRM_IS_INTEGER_POWER_OF_TW0( sizeof(DRM_DWORD_PTR) ) );

    ChkArg( pContext  != NULL );
    ChkArg( ppbBuffer != NULL );
    ChkArg( cbSize     > 0 );

    *ppbBuffer = NULL;

    dwSize = cbSize;
    /* adjust cbSize for alignment of DRM_DWORD */
    /* Power of two: use & instead of % for better perf */
    if( ( cbSize & ( sizeof(DRM_DWORD_PTR) - 1 ) ) != 0 )
    {
        ChkDR( DRM_DWordAddSame( &dwSize, sizeof(DRM_DWORD_PTR) ) );
        dwSize -= ( dwSize & ( sizeof(DRM_DWORD_PTR) - 1 ) );
    }

    ChkDR( DRM_DWordAdd( dwSize, sizeof(DRM_DWORD_PTR), &dwNewStackTop ) );
    ChkDR( DRM_DWordAddSame( &dwNewStackTop, pContext->nStackTop ) );

    if( dwNewStackTop > pContext->cbStack )    /* checks that will be enough space*/
    {
        ChkDR( DRM_E_OUTOFMEMORY );
    }

    pContext->pdwStack[pContext->nStackTop>>2] = dwSize;

    /* *ppbBuffer = (DRM_BYTE*) pContext->pdwStack + pContext->nStackTop + sizeof(DRM_DWORD_PTR) */
    ChkDR( DRM_DWordPtrAdd( (DRM_DWORD_PTR)pContext->pdwStack, pContext->nStackTop, (DRM_DWORD_PTR*)ppbBuffer ) );
    ChkDR( DRM_DWordPtrAddSame( (DRM_DWORD_PTR*)ppbBuffer, sizeof(DRM_DWORD_PTR) ) );

    pContext->nStackTop = dwNewStackTop;
    /*
    TRACE(("Alloc: top=0x%08X  pb=0x%08X  cb=%d\n", pContext->nStackTop, *ppbBuffer, dwSize));
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
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_COUNT_REQUIRED_FOR_VOIDPTR_BUFFER, "pvBuffer points into internal the internal buffer and its size is defined by *(pvBuffer-sizeof(DWORD))" )
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_STK_Free(
    __in DRM_STACK_ALLOCATOR_CONTEXT *pContext,
    __in DRM_VOID                    *pvBuffer )
PREFAST_POP
{
    DRM_RESULT      dr      = DRM_SUCCESS;
    DRM_DWORD       dwSize;

    if( pvBuffer == NULL )
    {
        /* pvBuffer is NULL, simply return */
        ChkDR( DRM_E_LOGICERR );
    }

    ChkArg( pContext != NULL );

    if( (((DRM_DWORD_PTR)pvBuffer) & (sizeof(DRM_DWORD_PTR)-1)) != 0 )
    {
        /* stack allocated pointers are DWORD aligned */
        ChkDR( DRM_E_LOGICERR );
    }

    dwSize = *(DRM_DWORD*)(((DRM_BYTE*)pvBuffer)-sizeof(DRM_DWORD_PTR));

    /* verify the buffer with the stack */
    /*
    TRACE((" Free: top=0x%08X  pb=0x%08X  cb=%d\n", pContext->nStackTop, pvBuffer, dwSize));
    */
    if( ((DRM_BYTE*)pvBuffer + (dwSize)) != (DRM_BYTE*)(pContext->pdwStack) + pContext->nStackTop )
    {
        TRACE(("\n\n***  DRM_STK_Free(): heap corrupted ***\n\n"));
        DRMASSERT( FALSE );
        ChkDR( DRM_E_STACK_CORRUPT ); /* internal stack corrupted */
    }

#if DRM_DBG
    /* clear the buffer */
    MEMSET((DRM_BYTE*)pvBuffer-sizeof(DRM_DWORD_PTR), 0xb, dwSize+sizeof(DRM_DWORD_PTR));
#endif  /* DRM_DBG */

    pContext->nStackTop -= (DRM_DWORD)(dwSize+sizeof(DRM_DWORD_PTR));

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
    DRM_DWORD_PTR   pdwTemp;
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

    pdwTemp = (DRM_DWORD_PTR)pbBuffer;
    if( (pdwTemp&(sizeof(DRM_DWORD_PTR)-1)) != 0 )
    {
        DRM_DWORD dwDifference = sizeof(DRM_DWORD_PTR)-(pdwTemp&(sizeof(DRM_DWORD_PTR)-1));

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
