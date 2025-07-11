/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __DRM_STACK_ALLOCATOR__
#define __DRM_STACK_ALLOCATOR__

#include <drmstkalloctypes.h>

ENTER_PK_NAMESPACE;

/*********************************************************************
**
**  Function:  DRM_STK_Free
**
**  Synopsis:  Free a buffer that was allocated using DRM_STK_Alloc.
**
**  Arguments:
**     [pContext] -- Stack allcator context that was used to allocate the pbBuffer parameter
**     [pbBuffer] -- Pointer returned from a call to DRM_STK_Alloc
**
**  Notes:  Pointers must be freed in LIFO order ( just like a stack ).
**
*********************************************************************/
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_COUNT_REQUIRED_FOR_VOIDPTR_BUFFER, "pbBuffer points into internal the internal buffer and its size is defined by *(pbBuffer-sizeof(DWORD))" )
DRM_API DRM_RESULT DRM_CALL DRM_STK_Free(
    __in DRM_STACK_ALLOCATOR_CONTEXT *pContext,
    __in DRM_VOID                    *pbBuffer );
PREFAST_POP

#define SAFE_STK_FREE(pdsac,pv)              \
    if (pv != NULL)                          \
    {                                        \
        (DRM_VOID)DRM_STK_Free (pdsac, pv);  \
        pv = NULL;                           \
    }

DRM_API DRM_RESULT DRM_CALL DRM_STK_PreAlloc(
    __in                           DRM_STACK_ALLOCATOR_CONTEXT   *pContext,
    __out                          DRM_DWORD                     *pcbSize,
    __deref_out_bcount( *pcbSize ) DRM_VOID                     **ppbBuffer );

/*********************************************************************
**
**  Function:  DRM_STK_Alloc
**
**  Synopsis:  Allocates a buffer from a give stack context
**
**  Arguments:
**     [pContext] -- Stack allocator context to allocate from
**     [cbSize] -- Size of the buffer needed
**     [ppbBuffer] -- Pointer to pointer to hold the new memory offset.
**
**  Notes:  Pointers must be freed in LIFO order ( just like a stack ).
**
*********************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_STK_Alloc(
    __inout                      DRM_STACK_ALLOCATOR_CONTEXT *pContext,
    __in                         DRM_DWORD                    cbSize,
    __deref_out_bcount( cbSize ) DRM_VOID                   **ppbBuffer )
GCC_ATTRIB_NO_STACK_PROTECT();

/*********************************************************************
**
**  Function:  DRM_STK_Init
**
**  Synopsis:  Initializes stack with supplied buffer and size.
**             It is kind of trivial, but frees user from
**             knowledge about stack structure.
**             Unless fForceInit is set, does not overwrite initialized
**             stack.
**  Arguments:
**     [pContext] -- Stack allocator context to allocate from
**     [pbBuffer] -- Pointer for the buffer for the stack
**     [cbSize] --  Size of the buffer supplied
**     [fForceInit] -- Ignores values in stack, always initializes.
**
**  Notes:  Pointers must be freed in LIFO order ( just like a stack ).
**
*********************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_STK_Init(
    __inout                 DRM_STACK_ALLOCATOR_CONTEXT *pContext,
    __in_bcount( cbSize)    DRM_BYTE                    *pbBuffer,
    __in                    DRM_DWORD                    cbSize,
    __in                    DRM_BOOL                     fForceInit );

EXIT_PK_NAMESPACE;

#endif /* __DRM_STACK_ALLOCATOR__ */

