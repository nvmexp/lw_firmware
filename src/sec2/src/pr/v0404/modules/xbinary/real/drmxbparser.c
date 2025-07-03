/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#define DRM_BUILDING_DRMXBPARSER_C
#include <drmxbparser.h>
#include <drmbytemanip.h>
#include <drmmathsafe.h>
#include <drmstkalloc.h>
#include <oemcommon.h>
#include <drmlastinclude.h>

PRAGMA_STRICT_GS_PUSH_ON;

ENTER_PK_NAMESPACE_CODE;
#if defined(SEC_COMPILE)  

#define DRM_XB_PARSER_ROOT_OBJECT_INDEX            1
#define DRM_XB_PARSER_MIN_CONTAINER_DEPTH          2
#define DRM_XB_PARSER_MAX_CONTAINER_DEPTH         32

GCC_ATTRIB_NO_STACK_PROTECT()
static DRM_NO_INLINE DRM_RESULT DRM_CALL _DRM_XB_Parse_Object(
    __in                                                                                                      DRM_STACK_ALLOCATOR_CONTEXT *f_pStack,
    __in                                                                                                      DRM_XB_ALIGNMENT             f_eAlign,
    __in_ecount( f_cElements )                                                                          const DRM_XB_ELEMENT_DESCRIPTION  *f_pElements,
    __in                                                                                                      DRM_DWORD                    f_cElements,
    __in_bcount( f_iObject + f_cbObject )                                                               const DRM_BYTE                    *f_pbBinaryFormat,
    __in                                                                                                      DRM_DWORD                    f_iObject,
    __in                                                                                                      DRM_DWORD                    f_cbObject,
    __inout_bcount( DRM_MIN( f_cbObject, DRM_OFFSET_OF( DRM_XB_EMPTY, fValid ) + sizeof( DRM_BOOL ) ) )       DRM_VOID                    *f_pvObject,
    __out_opt                                                                                                 DRM_DWORD                   *f_pcbRead );

static DRM_NO_INLINE DRM_RESULT DRM_CALL _XB_CreateNewListItem(
    __in        DRM_STACK_ALLOCATOR_CONTEXT  *f_pStack,
    __in        DRM_VOID                     *f_pvObject,
    __in        DRM_DWORD                     f_dwOffsetInObject,
    __in        DRM_DWORD                     f_dwSizeOfItem,
    __deref_out DRM_XB_BASELIST             **f_ppItem )
{
    DRM_RESULT dr = DRM_SUCCESS;

    DRM_XB_BASELIST  *pNewObj = NULL;
    DRM_XB_BASELIST **ppHead  = NULL;

    ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)f_pvObject,
                            f_dwOffsetInObject,
                            (DRM_SIZE_T*)&ppHead ) );

    ChkDR( DRM_STK_Alloc( f_pStack,
                          f_dwSizeOfItem,
                         (DRM_VOID**)&pNewObj ) );

    pNewObj->fValid = FALSE;
    pNewObj->pNext  = *ppHead;
    *ppHead = pNewObj;
    *f_ppItem = pNewObj;

ErrorExit:
    return dr;
}

static DRM_NO_INLINE DRM_RESULT DRM_CALL _XB_CreateNewListItem_UnknownObject(
    __in        DRM_STACK_ALLOCATOR_CONTEXT  *f_pStack,
    __in        DRM_VOID                     *f_pvObject,
    __in        DRM_DWORD                     f_dwOffsetInObject,
    __in        DRM_DWORD                     f_dwSizeOfItem,
    __deref_out DRM_XB_UNKNOWN_OBJECT       **f_ppItem )
{
    DRM_RESULT dr = DRM_SUCCESS;

    DRM_XB_UNKNOWN_OBJECT  *pNewObj = NULL;
    DRM_XB_UNKNOWN_OBJECT **ppHead  = NULL;

    ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)f_pvObject,
                            f_dwOffsetInObject,
                            (DRM_SIZE_T*)&ppHead ) );

    ChkDR( DRM_STK_Alloc( f_pStack,
                          f_dwSizeOfItem,
                          (DRM_VOID**)&pNewObj ) );

    pNewObj->fValid = FALSE;
    pNewObj->pNext  = *ppHead;
    *ppHead = pNewObj;
    *f_ppItem = pNewObj;

ErrorExit:
    return dr;
}

static DRM_RESULT DRM_CALL _XB_CreateNewListItem_UnknownContainer(
    __in        DRM_STACK_ALLOCATOR_CONTEXT  *f_pStack,
    __in        DRM_VOID                     *f_pvObject,
    __in        DRM_DWORD                     f_dwOffsetInObject,
    __in        DRM_DWORD                     f_dwSizeOfItem,
    __deref_out DRM_XB_UNKNOWN_CONTAINER    **f_ppItem )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_XB_UNKNOWN_CONTAINER *pTail   = NULL;
    DRM_XB_UNKNOWN_CONTAINER *pHead   = NULL;

    ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)f_pvObject,
                            f_dwOffsetInObject,
                            (DRM_SIZE_T*)&pHead ) );

    pTail = pHead;

    while( pTail->pNext != NULL )
    {
        pTail = pTail->pNext;
    }

    if( pTail->fValid )
    {
        DRM_XB_UNKNOWN_CONTAINER *pNewObj = NULL;

        ChkDR( DRM_STK_Alloc( f_pStack,
                              f_dwSizeOfItem,
                             (DRM_VOID**)&pNewObj ) );

        /* Copy the head unknown container to the new object and update the head to maintain proper order */
        *pNewObj = *pTail;

        pTail->pNext = pNewObj;
        pTail = pNewObj;
    }

    pTail->fValid = FALSE;
    *f_ppItem     = pTail;

ErrorExit:
    return dr;
}

/*********************************************************************
**
** Function: _DRM_XB_Parse_WORD
**
** Synopsis: Internal API that deserializes a WORD value from a XB stream
**
** Arguments:
**
** [f_pbBuffer]         -- Pointer to the buffer to read from
** [f_piObject]         -- Index in the buffer to start reading from
** [f_pcbObject]        -- Number of bytes reamaining in the buffer (updated on exit)
** [f_pvObject]         -- WORD to store the deserialized result
**
** Returns:            DRM_SUCESS on success.
**                     DRM_E_XB_ILWALID_OBJECT is there is an issue with the object size.
**
**********************************************************************/
static DRM_NO_INLINE DRM_RESULT DRM_CALL _DRM_XB_Parse_WORD(
    __in_bcount( *f_piObject + *f_pcbObject)    const DRM_BYTE    *f_pbBuffer,
    __inout                                           DRM_DWORD   *f_piObject,
    __inout                                           DRM_DWORD   *f_pcbObject,
    __inout_bcount( sizeof( DRM_WORD ) )              DRM_VOID    *f_pvObject )
{
    DRM_RESULT    dr    = DRM_SUCCESS;
    DRM_WORD     *pword = ( DRM_WORD * )f_pvObject;

    ChkArg( f_pvObject  != NULL );
    ChkArg( f_pbBuffer  != NULL );
    ChkArg( f_piObject  != NULL );
    ChkArg( f_pcbObject != NULL );

    ChkDR( DRM_DWordSubSame( f_pcbObject, sizeof( DRM_WORD ) ) );

    NETWORKBYTES_TO_WORD( *pword,
                          f_pbBuffer,
                          *f_piObject );

    ChkDR( DRM_DWordAddSame( f_piObject, sizeof( DRM_WORD ) ) );

ErrorExit:
    return dr;
}

/*********************************************************************
**
** Function: _DRM_XB_Parse_DWORD
**
** Synopsis: Internal API that deserializes a DWORD value from a XB stream
**
** Arguments:
**
** [f_pbBuffer]         -- Pointer to the buffer to read from
** [f_piObject]         -- Index in the buffer to start reading from
** [f_pcbObject]        -- Number of bytes reamaining in the buffer (updated on exit)
** [f_pvObject]         -- DWORD to store the deserialized result
**
** Returns:            DRM_SUCESS on success.
**                     DRM_E_XB_ILWALID_OBJECT is there is an issue with the object size.
**
**********************************************************************/
static DRM_NO_INLINE DRM_RESULT DRM_CALL _DRM_XB_Parse_DWORD(
    __in_bcount( *f_piObject + *f_pcbObject)  const DRM_BYTE    *f_pbBuffer,
    __inout                                         DRM_DWORD   *f_piObject,
    __inout                                         DRM_DWORD   *f_pcbObject,
    __inout_bcount( sizeof( DRM_DWORD ) )           DRM_VOID    *f_pvObject )
{
    DRM_RESULT     dr     = DRM_SUCCESS;
    DRM_DWORD     *pdword = ( DRM_DWORD * )f_pvObject;

    ChkArg( f_pvObject  != NULL );
    ChkArg( f_pbBuffer  != NULL );
    ChkArg( f_piObject  != NULL );
    ChkArg( f_pcbObject != NULL );

    ChkDR( DRM_DWordSubSame( f_pcbObject, sizeof( DRM_DWORD ) ) );

    NETWORKBYTES_TO_DWORD( *pdword,
                           f_pbBuffer,
                           *f_piObject);

    ChkDR( DRM_DWordAddSame( f_piObject, sizeof( DRM_DWORD ) ) );

ErrorExit:
    return dr;
}

/*********************************************************************
**
** Function: _DRM_XB_Parse_QWORD
**
** Synopsis: Internal API that deserializes a QWORD value from a XB stream
**
** Arguments:
**
** [f_pbBuffer]         -- Pointer to the buffer to read from
** [f_piObject]         -- Index in the buffer to start reading from
** [f_pcbObject]        -- Number of bytes reamaining in the buffer (updated on exit)
** [f_pvObject]         -- QWORD to store the deserialized result
**
** Returns:            DRM_SUCESS on success.
**                     DRM_E_XB_ILWALID_OBJECT is there is an issue with the object size.
**
**********************************************************************/
static DRM_NO_INLINE DRM_RESULT DRM_CALL _DRM_XB_Parse_QWORD(
    __in_bcount( *f_piObject + *f_pcbObject) const DRM_BYTE    *f_pbBuffer,
    __inout                                        DRM_DWORD   *f_piObject,
    __inout                                        DRM_DWORD   *f_pcbObject,
    __inout_bcount( sizeof( DRM_UINT64 ) )         DRM_VOID    *f_pvObject )
{
    DRM_RESULT  dr        = DRM_SUCCESS;
    DRM_UINT64 *pqword    = DRM_REINTERPRET_CAST( DRM_UINT64, f_pvObject );

    ChkArg( f_pvObject  != NULL );
    ChkArg( f_pbBuffer  != NULL );
    ChkArg( f_piObject  != NULL );
    ChkArg( f_pcbObject != NULL );

    ChkDR( DRM_DWordSubSame( f_pcbObject, DRM_SIZEOFQWORD ) );

    NETWORKBYTES_TO_QWORD( *pqword,
                            f_pbBuffer,
                           *f_piObject);

    ChkDR( DRM_DWordAddSame( f_piObject, DRM_SIZEOFQWORD ) );

ErrorExit:
    return dr;
}

/*********************************************************************
**
** Function: _DRM_XB_Parse_GUID
**
** Synopsis: Internal API that deserializes a GUID value from a XB stream
**
** Arguments:
**
** [f_pbBuffer]         -- Pointer to the buffer to read from
** [f_piObject]         -- Index in the buffer to start reading from
** [f_pcbObject]        -- Number of bytes reamaining in the buffer (updated on exit)
** [f_pvObject]         -- GUID to store the deserialized result
**
** Returns:            DRM_SUCESS on success.
**                     DRM_E_XB_ILWALID_OBJECT is there is an issue with the object size.
**
**********************************************************************/
static DRM_NO_INLINE DRM_RESULT DRM_CALL _DRM_XB_Parse_GUID(
    __in_bcount( *f_piObject + *f_pcbObject) const DRM_BYTE    *f_pbBuffer,
    __inout                                        DRM_DWORD   *f_piObject,
    __inout                                        DRM_DWORD   *f_pcbObject,
    __inout_bcount( sizeof( DRM_GUID ) )           DRM_VOID    *f_pvObject )
{
    DRM_RESULT    dr    = DRM_SUCCESS;
    DRM_GUID     *pguid = DRM_REINTERPRET_CAST( DRM_GUID, f_pvObject );

    ChkArg( f_pvObject != NULL );
    ChkArg( f_pbBuffer != NULL );
    ChkArg( f_piObject  != NULL );
    ChkArg( f_pcbObject != NULL );

    ChkDR( DRM_DWordSubSame( f_pcbObject, sizeof( DRM_GUID ) ) );

    DRM_BYT_CopyBytes( pguid, 0, f_pbBuffer, *f_piObject, sizeof(DRM_GUID) );

    ChkDR( DRM_DWordAddSame( f_piObject, sizeof( DRM_GUID ) ) );

ErrorExit:
    return dr;
}

/*********************************************************************
**
** Function: _DRM_XB_Parse_GUIDLIST
**
** Synopsis: Internal API that deserializes a GUID list sequense from a XB stream
**
** Arguments:
**
** [f_pxbSize ]         -- The element metadata for the GUID list which indicates
**                         how the object is to be serialized.
** [f_pbBuffer]         -- Pointer to the buffer to read from
** [f_piObject]         -- Index in the buffer to start reading from
** [f_pcbObject]        -- Number of bytes reamaining in the buffer (updated on exit)
** [f_pvObject]         -- DRM_XB_GUIDLIST to store the deserialized result
**
** Returns:            DRM_SUCESS on success.
**                     DRM_E_XB_ILWALID_OBJECT is there is an issue with the object size.
**
**********************************************************************/
static DRM_NO_INLINE DRM_RESULT DRM_CALL _DRM_XB_Parse_GUIDLIST(
    __in_ecount( 1 )                         const DRM_XB_SIZE *f_pxbSize,
    __in_bcount( *f_piObject + *f_pcbObject) const DRM_BYTE    *f_pbBuffer,
    __inout                                        DRM_DWORD   *f_piObject,
    __inout                                        DRM_DWORD   *f_pcbObject,
    __inout_bcount( sizeof( DRM_XB_GUIDLIST ) )    DRM_VOID    *f_pvObject )
{
    DRM_RESULT        dr        = DRM_SUCCESS;
    DRM_DWORD         iObject   = *f_piObject;
    DRM_XB_GUIDLIST  *pguidlist = DRM_REINTERPRET_CAST( DRM_XB_GUIDLIST, f_pvObject );
    DRM_DWORD         dwResult  = 0;

    ChkArg( f_pvObject != NULL );
    ChkArg( f_pbBuffer != NULL );

    ChkBOOL( *f_pcbObject >= sizeof( DRM_DWORD ), DRM_E_XB_ILWALID_OBJECT );

    NETWORKBYTES_TO_DWORD( pguidlist->cGUIDs, f_pbBuffer, iObject );

    ChkDR( DRM_DWordAddSame( &iObject, sizeof( DRM_DWORD ) ) );

    ChkBOOL( pguidlist->cGUIDs >= f_pxbSize->wMinSize, DRM_E_XB_OBJECT_OUT_OF_RANGE );
    ChkBOOL( (f_pxbSize->wMaxSize == 0) || (pguidlist->cGUIDs <= f_pxbSize->wMaxSize), DRM_E_XB_OBJECT_OUT_OF_RANGE );

    ChkDR( DRM_DWordMult( pguidlist->cGUIDs, sizeof( DRM_GUID ), &dwResult ) );
    ChkDR( DRM_DWordAddSame( &dwResult, sizeof( DRM_DWORD ) ) );

    ChkBOOL( *f_pcbObject >= dwResult, DRM_E_XB_ILWALID_OBJECT );

    pguidlist->pguidBuffer = ( DRM_BYTE * )f_pbBuffer;
    pguidlist->iGuids      = iObject;
    pguidlist->fValid      = TRUE;

    ChkDR( DRM_DWordAddSame( f_piObject, sizeof( DRM_DWORD ) ) );

    ChkDR( DRM_DWordMult( pguidlist->cGUIDs, sizeof( DRM_GUID ), &dwResult ) );
    ChkDR( DRM_DWordAddSame( f_piObject, dwResult ) );

    ChkDR( DRM_DWordAddSame( &dwResult, sizeof( DRM_DWORD ) ) );
    ChkDR( DRM_DWordSubSame( f_pcbObject, dwResult ) );

ErrorExit:
    return dr;
}

/*********************************************************************
**
** Function: _DRM_XB_Parse_WORDLIST
**
** Synopsis: Internal API that deserializes a WORD list sequense from a XB stream
**
** Arguments:
**
** [f_pxbSize ]         -- The element metadata for the WORD list which indicates
**                         how the object is to be serialized.
** [f_pStack]           -- Pointer to the XBinary stack
** [f_pbBuffer]         -- Pointer to the buffer to read from
** [f_piObject]         -- Index in the buffer to start reading from
** [f_pcbObject]        -- Number of bytes reamaining in the buffer (updated on exit)
** [f_pvObject]         -- DRM_XB_WORDLIST to store the deserialized result
**
** Returns:            DRM_SUCESS on success.
**                     DRM_E_XB_ILWALID_OBJECT is there is an issue with the object size.
**
**********************************************************************/
static DRM_NO_INLINE DRM_RESULT DRM_CALL _DRM_XB_Parse_WORDLIST(
    __in_ecount( 1 )                         const DRM_XB_SIZE                 *f_pxbSize,
    __inout                                        DRM_STACK_ALLOCATOR_CONTEXT *f_pStack,
    __in_bcount( *f_piObject + *f_pcbObject) const DRM_BYTE                    *f_pbBuffer,
    __inout                                        DRM_DWORD                   *f_piObject,
    __inout                                        DRM_DWORD                   *f_pcbObject,
    __inout_bcount( sizeof( DRM_XB_WORDLIST ) )    DRM_VOID                    *f_pvObject )
{
    DRM_RESULT        dr        = DRM_SUCCESS;
    DRM_DWORD         iObject   = *f_piObject;
    DRM_XB_WORDLIST  *pwordlist = DRM_REINTERPRET_CAST( DRM_XB_WORDLIST, f_pvObject );
    DRM_DWORD         i         = 0;
    DRM_DWORD         dwResult  = 0;

    ChkArg( f_pvObject != NULL );
    ChkArg( f_pbBuffer != NULL );

    ChkBOOL( *f_pcbObject >= sizeof( DRM_DWORD ), DRM_E_XB_ILWALID_OBJECT );

    NETWORKBYTES_TO_DWORD( pwordlist->cWORDs, f_pbBuffer, iObject );

    ChkDR( DRM_DWordAddSame( &iObject, sizeof( DRM_DWORD ) ) );

    ChkBOOL( pwordlist->cWORDs >= f_pxbSize->wMinSize, DRM_E_XB_OBJECT_OUT_OF_RANGE );
    ChkBOOL( (f_pxbSize->wMaxSize == 0) || (pwordlist->cWORDs <= f_pxbSize->wMaxSize), DRM_E_XB_OBJECT_OUT_OF_RANGE );

    if( pwordlist->cWORDs > 0 )
    {
        ChkDR( DRM_DWordMult( pwordlist->cWORDs, sizeof( DRM_WORD ), &dwResult ) );

        ChkDR( DRM_STK_Alloc( f_pStack,
                              dwResult,
                             (DRM_VOID**)&pwordlist->pwordBuffer ) );

        ChkDR( DRM_DWordAddSame( &dwResult, sizeof( DRM_DWORD ) ) );
        ChkBOOL( *f_pcbObject >= dwResult, DRM_E_XB_ILWALID_OBJECT );

    }
    else
    {
        pwordlist->pwordBuffer = NULL;
    }

    pwordlist->iWords = 0;
    pwordlist->fValid = TRUE;

    ChkDR( DRM_DWordAddSame( f_piObject, sizeof( DRM_DWORD ) ) );
    ChkDR( DRM_DWordSubSame( f_pcbObject, sizeof( DRM_DWORD ) ) );

    if( pwordlist->cWORDs > 0 )
    {
        DRM_VOID *pvResult = NULL;

        ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)pwordlist->pwordBuffer, pwordlist->iWords, (DRM_SIZE_T*)&pvResult ) );

        for( i = 0; i < pwordlist->cWORDs; i++ )
        {
            ChkDR( _DRM_XB_Parse_WORD( f_pbBuffer,
                                       f_piObject,
                                       f_pcbObject,
                                       pvResult ) );

            ChkDR( DRM_DWordPtrAddSame( (DRM_SIZE_T*)&pvResult, sizeof(DRM_WORD) ) );
        }
    }

ErrorExit:
    return dr;
}

/*********************************************************************
**
** Function: _DRM_XB_Parse_DWORDLIST
**
** Synopsis: Internal API that deserializes a DWORD list sequense from a XB stream
**
** Arguments:
**
** [f_pxbSize ]         -- The element metadata for the DWORD list which indicates
**                         how the object is to be serialized.
** [f_pStack]           -- Pointer to the XBinary stack
** [f_cDwords]          -- The count of DWORDs in the list
** [f_pbBuffer]         -- Pointer to the buffer to read from
** [f_piObject]         -- Index in the buffer to start reading from
** [f_pcbObject]        -- Number of bytes reamaining in the buffer (updated on exit)
** [f_pvObject]         -- DRM_XB_DWORDLIST to store the deserialized result
**
** Returns:            DRM_SUCESS on success.
**                     DRM_E_XB_ILWALID_OBJECT is there is an issue with the object size.
**
**********************************************************************/
static DRM_NO_INLINE DRM_RESULT DRM_CALL _DRM_XB_Parse_DWORDLIST(
    __in_ecount( 1 )                         const DRM_XB_SIZE                 *f_pxbSize,
    __inout                                        DRM_STACK_ALLOCATOR_CONTEXT *f_pStack,
    __in                                     const DRM_DWORD                    f_cDwords,
    __in_bcount( *f_piObject + *f_pcbObject) const DRM_BYTE                    *f_pbBuffer,
    __inout                                        DRM_DWORD                   *f_piObject,
    __inout                                        DRM_DWORD                   *f_pcbObject,
    __inout_bcount( sizeof( DRM_XB_DWORDLIST ) )   DRM_VOID                    *f_pvObject )
{
    DRM_RESULT        dr          = DRM_SUCCESS;
    DRM_XB_DWORDLIST *pdwordlist  = DRM_REINTERPRET_CAST( DRM_XB_DWORDLIST, f_pvObject );

    DRM_DWORD         iDWORD      = 0;
    DRM_DWORD         dwResult    = 0;
    DRM_VOID         *pvTarget    = NULL;

    ChkArg( f_pvObject != NULL );
    ChkArg( f_pbBuffer != NULL );

    pdwordlist->cDWORDs = f_cDwords;

    ChkBOOL( f_cDwords >= f_pxbSize->wMinSize, DRM_E_XB_OBJECT_OUT_OF_RANGE );
    ChkBOOL( (f_pxbSize->wMaxSize == 0) || (f_cDwords <= f_pxbSize->wMaxSize), DRM_E_XB_OBJECT_OUT_OF_RANGE );

    if( pdwordlist->cDWORDs > 0 )
    {
        ChkDR( DRM_DWordMult( pdwordlist->cDWORDs, sizeof( DRM_DWORD ), &dwResult ) );
        ChkBOOL( *f_pcbObject >= dwResult, DRM_E_XB_ILWALID_OBJECT );

        ChkDR( DRM_STK_Alloc( f_pStack,
                              dwResult,
                              (DRM_VOID**)&pdwordlist->pdwordBuffer ) );
    }
    else
    {
        pdwordlist->pdwordBuffer = NULL;
    }

    pdwordlist->iDwords = 0;
    pdwordlist->fValid  = TRUE;

    if( pdwordlist->cDWORDs > 0 )
    {
        ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)pdwordlist->pdwordBuffer, pdwordlist->iDwords, (DRM_SIZE_T*)&pvTarget ) );

        for( iDWORD = 0; iDWORD < pdwordlist->cDWORDs; iDWORD++ )
        {
            ChkDR( _DRM_XB_Parse_DWORD( f_pbBuffer, f_piObject, f_pcbObject, pvTarget ) );
            ChkDR( DRM_DWordPtrAddSame( (DRM_SIZE_T*)&pvTarget, sizeof( DRM_DWORD ) ) );
        }
    }

ErrorExit:
    return dr;
}

/*********************************************************************
**
** Function: _DRM_XB_Parse_QWORDLIST
**
** Synopsis: Internal API that deserializes a QWORD list sequense from a XB stream
**
** Arguments:
**
** [f_pxbSize ]         -- The element metadata for the QWORD list which indicates
**                         how the object is to be serialized.
** [f_pStack]           -- Stack allocator contxt
** [f_pbBuffer]         -- Pointer to the buffer to read from
** [f_piObject]         -- Index in the buffer to start reading from
** [f_pcbObject]        -- Number of bytes reamaining in the buffer (updated on exit)
** [f_pvObject]         -- DRM_XB_QWORDLIST to store the deserialized result
**
** Returns:            DRM_SUCESS on success.
**                     DRM_E_XB_ILWALID_OBJECT is there is an issue with the object size.
**
**********************************************************************/
static DRM_NO_INLINE DRM_RESULT DRM_CALL _DRM_XB_Parse_QWORDLIST(
    __in_ecount( 1 )                         const DRM_XB_SIZE                 *f_pxbSize,
    __inout                                        DRM_STACK_ALLOCATOR_CONTEXT *f_pStack,
    __in_bcount( *f_piObject + *f_pcbObject) const DRM_BYTE                    *f_pbBuffer,
    __inout                                        DRM_DWORD                   *f_piObject,
    __inout                                        DRM_DWORD                   *f_pcbObject,
    __inout_bcount( sizeof( DRM_XB_QWORDLIST ) )   DRM_VOID                    *f_pvObject )
{
    DRM_RESULT        dr         = DRM_SUCCESS;
    DRM_DWORD         iObject    = *f_piObject;
    DRM_XB_QWORDLIST *pqwordlist = DRM_REINTERPRET_CAST( DRM_XB_QWORDLIST, f_pvObject );

    DRM_DWORD         iQWORD     = 0;
    DRM_DWORD         dwResult   = 0;
    DRM_SIZE_T        pvResult;

    ChkArg( f_pvObject != NULL );
    ChkArg( f_pbBuffer != NULL );

    ChkBOOL( *f_pcbObject >= sizeof( DRM_DWORD ), DRM_E_XB_ILWALID_OBJECT );

    NETWORKBYTES_TO_DWORD( pqwordlist->cQWORDs, f_pbBuffer, iObject );

    ChkDR( DRM_DWordAddSame( &iObject, DRM_SIZEOFQWORD ) );

    ChkBOOL( pqwordlist->cQWORDs >= f_pxbSize->wMinSize, DRM_E_XB_OBJECT_OUT_OF_RANGE );
    ChkBOOL( (f_pxbSize->wMaxSize == 0) || (pqwordlist->cQWORDs <= f_pxbSize->wMaxSize), DRM_E_XB_OBJECT_OUT_OF_RANGE );

    if( pqwordlist->cQWORDs > 0 )
    {
        ChkDR( DRM_DWordMult( pqwordlist->cQWORDs, DRM_SIZEOFQWORD, &dwResult ) );
        ChkDR( DRM_STK_Alloc( f_pStack,
                              dwResult,
                              (DRM_VOID**)&pqwordlist->pqwordBuffer ) );

        ChkDR( DRM_DWordAddSame( &dwResult, sizeof( DRM_DWORD ) ) );
        ChkBOOL( *f_pcbObject >= dwResult, DRM_E_XB_ILWALID_OBJECT );
    }
    else
    {
        pqwordlist->pqwordBuffer = NULL;
    }

    pqwordlist->iQwords = 0;
    pqwordlist->fValid  = TRUE;

    ChkDR( DRM_DWordAddSame( f_piObject, sizeof( DRM_DWORD ) ) );
    ChkDR( DRM_DWordSubSame( f_pcbObject, sizeof( DRM_DWORD ) ) );

    if( pqwordlist->cQWORDs > 0 )
    {
        ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)pqwordlist->pqwordBuffer, pqwordlist->iQwords, (DRM_SIZE_T*)&pvResult ) );
        for( iQWORD = 0; iQWORD < pqwordlist->cQWORDs; iQWORD++ )
        {
            ChkDR( _DRM_XB_Parse_QWORD( f_pbBuffer,
                                        f_piObject,
                                        f_pcbObject,
                                        (DRM_VOID*)pvResult ) );

            ChkDR( DRM_DWordPtrAddSame( (DRM_SIZE_T*)&pvResult, DRM_SIZEOFQWORD ) );
        }
    }

ErrorExit:
    return dr;
}

/*********************************************************************
**
** Function: _DRM_XB_Parse_STRUCTLIST
**
** Synopsis: Internal API that deserializes a STRUCT list sequence from a XB stream
**
** Arguments:
**
** [f_pStack]               -- Pointer to the XBinary stack
** [f_eAlign]               -- The aligment for the struct data.
** [f_cStructs]             -- The count of structs in the list
** [f_pXBSize]              -- Metadata that describes the serialization characteristics
**                             of the struct list.
** [f_cElementDescription]  -- The count of element descriptions
** [f_pElementDescription]  -- Pointer to the element descriptions
** [f_pbBuffer]             -- Pointer to the buffer to read from
** [f_piObject]             -- Index in the buffer to start reading from
** [f_pcbObject]            -- Number of bytes reamaining in the buffer (updated on exit)
** [f_pvObject]             -- pointer to the offset of where the linked list pointer resides
**                             in the struct
**
** Returns:            DRM_SUCESS on success.
**                     DRM_E_XB_ILWALID_OBJECT is there is an issue with the object size.
**
**********************************************************************/
static DRM_FRE_INLINE DRM_RESULT DRM_CALL _DRM_XB_Parse_STRUCTLIST(
    __inout                                        DRM_STACK_ALLOCATOR_CONTEXT *f_pStack,
    __in                                           DRM_XB_ALIGNMENT             f_eAlign,
    __in                                           DRM_DWORD                    f_cStructs,
    __in                                     const DRM_XB_SIZE                 *f_pXBSize,
    __in                                           DRM_DWORD                    f_cElementDescription,
    __in                                     const DRM_XB_ELEMENT_DESCRIPTION  *f_pElementDescription,
    __in_bcount( *f_piObject + *f_pcbObject) const DRM_BYTE                    *f_pbBuffer,
    __inout                                        DRM_DWORD                   *f_piObject,
    __inout                                        DRM_DWORD                   *f_pcbObject,
    __out                                          DRM_VOID                    *f_pvObject )
{
    DRM_RESULT        dr          = DRM_SUCCESS;
    DRM_BYTE         *pbNewObject = NULL;
    DRM_DWORD         cbNewObject = 0;
    DRM_DWORD         iLwrObject  = 0;

    ChkArg( f_pvObject != NULL );
    ChkArg( f_pbBuffer != NULL );
    ChkArg( f_pXBSize  != NULL );

    DRMASSERT( f_pXBSize->wMinSize == f_pXBSize->wMaxSize );

    ChkDR( DRM_DWordMult( f_pXBSize->wMinSize, f_cStructs, &cbNewObject ) );

    if( f_pElementDescription != NULL )
    {
        if (f_cStructs > 0)
        {
            ChkDR(DRM_STK_Alloc( f_pStack,
                                 cbNewObject,
                                 (DRM_VOID**)&pbNewObject));

            ZEROMEM(pbNewObject, cbNewObject);
        }

        for (iLwrObject = 0; iLwrObject < f_cStructs; iLwrObject++)
        {
            DRM_DWORD cbRead = 0;

            ChkDR( _DRM_XB_Parse_Object(
                f_pStack,
                f_eAlign,
                f_pElementDescription,
                f_cElementDescription,
                f_pbBuffer,
                *f_piObject,
                *f_pcbObject,
                &pbNewObject[ iLwrObject * f_pXBSize->wMinSize ],   /* Overflow check not needed because of size of buffer is f_cStructs * f_pXBSize->wMinSize */
                &cbRead ) );

            ChkDR( DRM_DWordAddSame( f_piObject, cbRead ) );
            ChkDR( DRM_DWordSubSame( f_pcbObject, cbRead ) );
        }
    }
    else
    {
        DRM_DWORD cbPadding = 0;

        iLwrObject = *f_piObject;

        ChkDR( _DRM_XB_AlignData( f_eAlign, *f_pcbObject + iLwrObject, &iLwrObject, &cbPadding ) );

        pbNewObject = ( DRM_BYTE * )&f_pbBuffer[iLwrObject];

        *f_piObject  = iLwrObject + cbNewObject;    /* No overflow check needed because of _DRM_XB_AlignData */
        *f_pcbObject = *f_pcbObject - cbNewObject;  /* No underflow check needed because of _DRM_XB_AlignData */
    }

    MEMCPY( f_pvObject, &pbNewObject, sizeof(DRM_VOID*) );

ErrorExit:
    return dr;
}

/*********************************************************************
**
** Function: _DRM_XB_Parse_BYTEARRAY
**
** Synopsis: Internal API that deserializes an array of bytes from a XB stream
**
** Arguments:
**
** [f_eAlign]           -- The alignment of the byte array data.
** [f_pXBSize]          -- Metadata that describes the serialization characteristics
**                         of the byte array.
** [f_pbBuffer]         -- Pointer to the buffer to read from
** [f_piObject]         -- Index in the buffer to start reading from
** [f_pcbObject]        -- Number of bytes remaining in the buffer (updated on exit)
** [f_cbDataToRead]     -- Number of bytes to read from the buffer to parse the byte array
** [f_pvObject]         -- DRM_XB_BYTEARRAY to store the deserialized result
**
** Returns:            DRM_SUCESS on success.
**                     DRM_E_XB_ILWALID_OBJECT is there is an issue with the object size.
**
**********************************************************************/
static DRM_NO_INLINE DRM_RESULT DRM_CALL _DRM_XB_Parse_BYTEARRAY(
    __in                                               DRM_XB_ALIGNMENT     f_eAlign,
    __in                                         const DRM_XB_SIZE         *f_pXBSize,
    __in_bcount( *f_piObject + *f_pcbObject )    const DRM_BYTE            *f_pbBuffer,
    __in                                               DRM_DWORD           *f_piObject,
    __in                                               DRM_DWORD           *f_pcbObject,
    __in                                               DRM_DWORD            f_cbDataToRead,
    __inout_bcount( sizeof( DRM_XB_BYTEARRAY ) )       DRM_VOID            *f_pvObject )
{
    DRM_RESULT         dr         = DRM_SUCCESS;
    DRM_XB_BYTEARRAY  *pbytearray = DRM_REINTERPRET_CAST( DRM_XB_BYTEARRAY, f_pvObject );

    DRM_DWORD          ibObject   = 0;
    DRM_DWORD          cbPadding  = 0;
    DRM_DWORD          cbMaxSize  = 0;

    ChkArg( f_pvObject  != NULL );
    ChkArg( f_pbBuffer  != NULL );
    ChkArg( f_piObject  != NULL );
    ChkArg( f_pcbObject != NULL );
    ChkArg( f_pXBSize   != NULL );

    if( f_cbDataToRead == 0 )
    {
        ChkBOOL( f_pXBSize->wMinSize == 0, DRM_E_XB_OBJECT_OUT_OF_RANGE );

        /* Empty byte array */
        pbytearray->fValid       = TRUE;
        pbytearray->cbData       = 0;
        pbytearray->pbDataBuffer = NULL;
        pbytearray->iData        = 0;

        goto ErrorExit;
    }

    cbMaxSize = *f_pcbObject;
    ibObject  = *f_piObject;

    ChkDR( DRM_DWordAddSame( &cbMaxSize, ibObject ) );
    ChkDR( _DRM_XB_AlignData( f_eAlign, cbMaxSize, &ibObject, &cbPadding ) );

    ChkBOOL( f_cbDataToRead <= *f_pcbObject, DRM_E_XB_ILWALID_OBJECT );

    pbytearray->cbData        = f_pXBSize->eSizeType == XB_SIZE_IS_ABSOLUTE ? f_cbDataToRead : f_cbDataToRead - cbPadding; /* No underflow check required because _DRM_XB_AlignData ensures f_cbDataToRead > cbPadding */
    pbytearray->pbDataBuffer  = ( DRM_BYTE * )f_pbBuffer;
    pbytearray->iData         = ibObject;

    ChkBOOL( pbytearray->cbData >= f_pXBSize->wMinSize, DRM_E_XB_OBJECT_OUT_OF_RANGE );
    ChkBOOL( (f_pXBSize->wMaxSize == 0) || (pbytearray->cbData <= f_pXBSize->wMaxSize), DRM_E_XB_OBJECT_OUT_OF_RANGE );

    ChkDR( DRM_DWordAddSame( f_piObject, f_cbDataToRead ) );
    ChkDR( DRM_DWordSubSame( f_pcbObject, f_cbDataToRead ) );

    if( f_pXBSize->wPadding != XB_PADDING_NONE && (f_cbDataToRead % f_pXBSize->wPadding != 0) )
    {
        DRM_WORD wPaddingAmount = f_pXBSize->wPadding - (f_cbDataToRead % f_pXBSize->wPadding);
        ChkDR( DRM_DWordAddSame( f_piObject, wPaddingAmount ) );
        ChkDR( DRM_DWordSubSame( f_pcbObject, wPaddingAmount ) );
    }

    pbytearray->fValid = TRUE;

ErrorExit:
    return dr;
}

/*********************************************************************
**
** Function: _DRM_XB_Parse_UNKNOWN_OBJECT
**
** Synopsis: Internal API that deserializes an unknown object from an XB stream
**
** Arguments:
**
** [f_pFormat]          -- The XBinary format object.
** [f_pbBuffer]         -- Pointer to the buffer to read from
** [f_wObjectType]      -- Unknown object type
** [f_wObjectFlags]     -- Unknown object flags
** [f_iObject]          -- Index in the buffer to start reading from
** [f_cbObject]         -- Number of bytes reamaining in the buffer (updated on exit)
** [f_pUnknown]         -- DRM_XB_UNKNOWN_OBJECT to store the deserialized result
**
** Returns:            DRM_SUCESS on success.
**                     DRM_E_XB_ILWALID_OBJECT is there is an issue with the object size.
**
**********************************************************************/
GCC_ATTRIB_NO_STACK_PROTECT()
static DRM_NO_INLINE DRM_RESULT DRM_CALL _DRM_XB_Parse_UNKNOWN_OBJECT(
    __in                                    const DRM_XB_FORMAT_DESCRIPTION   *f_pFormat,
    __in_bcount( f_iObject + f_cbObject )   const DRM_BYTE                    *f_pbBuffer,
    __in                                          DRM_WORD                     f_wObjectType,
    __in                                          DRM_WORD                     f_wObjectFlags,
    __in                                          DRM_DWORD                    f_iObject,
    __in                                          DRM_DWORD                    f_cbObject,
    __inout_ecount(1)                             DRM_XB_UNKNOWN_OBJECT       *f_pUnknown )
{
    DRM_RESULT       dr         = DRM_SUCCESS;
    DRM_XB_ALIGNMENT eAlign     = XB_ALIGN_1_BYTE;
    DRM_DWORD        cbPadding  = 0;

    ChkArg( f_pUnknown != NULL );
    ChkArg( f_pbBuffer != NULL );
    ChkArg( f_pFormat  != NULL );

    if( f_pFormat->pHeaderDescription != NULL )
    {
        eAlign = f_pFormat->pHeaderDescription->eAlign;
    }

    ChkDR( _DRM_XB_AlignData( eAlign, f_cbObject + f_iObject, &f_iObject, &cbPadding ) );


    f_pUnknown->wType    = f_wObjectType;
    f_pUnknown->wFlags   = f_wObjectFlags;
    f_pUnknown->pbBuffer = ( DRM_BYTE * )f_pbBuffer;
    f_pUnknown->ibData   = f_iObject;
    f_pUnknown->cbData   = f_cbObject - cbPadding;

    f_pUnknown->fValid = TRUE;

ErrorExit:
    return dr;
}

/*********************************************************************
**
** Function: _DRM_XB_Parse_Object
**
** Synopsis: API that initiates the parsing of a XBinary oject
**
** Arguments:
**
** [f_pStack]         -- Pointer to a stack context for dynamic allocations
** [f_eAlign]         -- The aligment for any raw data objects.
** [f_pEntry]         -- object entry description
** [f_pbBinaryFormat] -- binary stream to deserialized the object from
** [f_iObject]        -- index into f_pbBinaryFormat
** [f_cbObject]       -- count of bytes in f_pbBinaryFormat
** [f_pvObject]       -- VOID pointer of a struct to deserialized the object into
** [f_pcbRead]        -- out value of the number of bytes read from the stream
**
**********************************************************************/
static DRM_NO_INLINE DRM_RESULT DRM_CALL _DRM_XB_Parse_Object(
    __in                                                                                                      DRM_STACK_ALLOCATOR_CONTEXT *f_pStack,
    __in                                                                                                      DRM_XB_ALIGNMENT             f_eAlign,
    __in_ecount( f_cElements )                                                                          const DRM_XB_ELEMENT_DESCRIPTION  *f_pElements,
    __in                                                                                                      DRM_DWORD                    f_cElements,
    __in_bcount( f_iObject + f_cbObject )                                                               const DRM_BYTE                    *f_pbBinaryFormat,
    __in                                                                                                      DRM_DWORD                    f_iObject,
    __in                                                                                                      DRM_DWORD                    f_cbObject,
    __inout_bcount( DRM_MIN( f_cbObject, DRM_OFFSET_OF( DRM_XB_EMPTY, fValid ) + sizeof( DRM_BOOL ) ) )       DRM_VOID                    *f_pvObject,
    __out_opt                                                                                                 DRM_DWORD                   *f_pcbRead )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT    dr           = DRM_SUCCESS;
    DRM_DWORD     iLwrrent     = f_iObject;
    DRM_DWORD     cbLengthLeft = f_cbObject;
    DRM_XB_EMPTY *pContainer   = ( DRM_XB_EMPTY* )f_pvObject;
    DRM_DWORD     iElement     = 0;

    ChkArg( f_pbBinaryFormat != NULL );
    ChkArg( f_pvObject  != NULL );

    for( iElement = 0; iElement < f_cElements; iElement++ )
    {
        DRM_VOID *pobject       = NULL;
        DRM_DWORD cbDataToRead  = 0;
        DRM_DWORD cElementCount = 0;

        const DRM_XB_ELEMENT_DESCRIPTION *pElement = &f_pElements[iElement];

        ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)f_pvObject, pElement->wOffsetInLwrrentStruct, (DRM_SIZE_T*)&pobject ) );

        switch ( pElement->eElementType )
        {
        case XB_ELEMENT_TYPE_EMPTY:           /* No data */ break;
        case XB_ELEMENT_TYPE_WORD:            ChkDR( _DRM_XB_Parse_WORD      ( f_pbBinaryFormat, &iLwrrent, &cbLengthLeft, pobject ) ); break;
        case XB_ELEMENT_TYPE_DWORD:           ChkDR( _DRM_XB_Parse_DWORD     ( f_pbBinaryFormat, &iLwrrent, &cbLengthLeft, pobject ) ); break;
        case XB_ELEMENT_TYPE_QWORD:           ChkDR( _DRM_XB_Parse_QWORD     ( f_pbBinaryFormat, &iLwrrent, &cbLengthLeft, pobject ) ); break;
        case XB_ELEMENT_TYPE_GUID:            ChkDR( _DRM_XB_Parse_GUID      ( f_pbBinaryFormat, &iLwrrent, &cbLengthLeft, pobject ) ); break;
        case XB_ELEMENT_TYPE_GUIDLIST:        ChkDR( _DRM_XB_Parse_GUIDLIST  ( &pElement->xbSize, f_pbBinaryFormat, &iLwrrent, &cbLengthLeft, pobject ) ); break;
        case XB_ELEMENT_TYPE_WORDLIST:        ChkDR( _DRM_XB_Parse_WORDLIST  ( &pElement->xbSize, f_pStack, f_pbBinaryFormat, &iLwrrent, &cbLengthLeft, pobject ) ); break;
        case XB_ELEMENT_TYPE_DWORDLIST:
            switch( pElement->xbSize.eSizeType )
            {
                case XB_SIZE_IS_WORD_IN_ANOTHER_STRUCT_MEMBER:
                    {
                        DRM_SIZE_T *pwptr = NULL;
                        DRM_WORD wElementCount = 0;

                        /*
                        ** The wOffset value represents the offset from the containing structure where the count value is located.
                        ** This offset must be less than the current element's offset or the count value will not be initialized yet.
                        */
                        ChkArg((DRM_DWORD)pElement->xbSize.wOffset + sizeof(DRM_WORD) <= (DRM_DWORD)pElement->wOffsetInLwrrentStruct);

                        ChkDR(DRM_DWordPtrAdd((DRM_SIZE_T)f_pvObject, pElement->xbSize.wOffset, (DRM_SIZE_T*)&pwptr));
                        /* Read the WORD sized element count */
                        MEMCPY( &wElementCount, pwptr, sizeof(DRM_WORD) );
                        cElementCount = wElementCount;
                    }
                    break;

                case XB_SIZE_IS_RELATIVE_WORD:
                    {
                        DRM_WORD wElementCount = 0;
                        ChkDR( DRM_DWordSubSame( &cbLengthLeft, sizeof( DRM_WORD ) ) );
                        NETWORKBYTES_TO_WORD( wElementCount, f_pbBinaryFormat, iLwrrent );
                        ChkDR( DRM_DWordAddSame( &iLwrrent, sizeof( DRM_WORD ) ) );

                        cElementCount = wElementCount;
                        /* DRM_XB_DWORDLIST structure uses a DWORD for count regardless of the serialized size */
                        MEMCPY( pobject, &cElementCount, sizeof(DRM_DWORD) );
                    }
                    break;

                case XB_SIZE_IS_RELATIVE_DWORD:
                case XB_SIZE_IS_IGNORED:
                    ChkDR( _DRM_XB_Parse_DWORD( f_pbBinaryFormat, &iLwrrent, &cbLengthLeft, pobject ) );
                    MEMCPY( &cElementCount, pobject, sizeof(DRM_DWORD) );
                    break;

                default:
                    DRMASSERT( !"Element count should have been one of the known values" );
                    ChkDR( DRM_E_LOGICERR );
            }

            ChkDR( _DRM_XB_Parse_DWORDLIST ( &pElement->xbSize, f_pStack, cElementCount, f_pbBinaryFormat, &iLwrrent, &cbLengthLeft, pobject ) );
            break;
        case XB_ELEMENT_TYPE_QWORDLIST:
            ChkDR( _DRM_XB_Parse_QWORDLIST ( &pElement->xbSize, f_pStack, f_pbBinaryFormat, &iLwrrent, &cbLengthLeft, pobject ) );
            break;
        case XB_ELEMENT_TYPE_STRUCTLIST:
            switch( pElement->xbSize.eSizeType )
            {
                /*
                ** Special case for XMR parsing.  The element count was read in a previous element.  Use the
                ** xbSize.wOffset value as the offset to the prior value.
                */
                case XB_SIZE_IS_WORD_IN_ANOTHER_STRUCT_MEMBER:
                    {
                        DRM_VOID *pvElement = NULL;
                        DRM_WORD  wElementCount = 0;

                        /*
                        ** The size value must be somewhere before the current element.  The wOffset value indicates the size element's
                        ** offset from the start of the containing structure.
                        */
                        ChkArg( (DRM_DWORD)pElement->xbSize.wOffset + sizeof(DRM_WORD) <= (DRM_DWORD)pElement->wOffsetInLwrrentStruct );
                        ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)f_pvObject, pElement->xbSize.wOffset, (DRM_SIZE_T*)&pvElement ) );
                        /* Read the WORD sized element count value */
                        MEMCPY( &wElementCount, pvElement, sizeof(DRM_WORD) );
                        cElementCount = wElementCount;
                    }
                    break;

                /*
                ** All
                ** xbSize.wOffset value as the offset to the prior value.
                */
                case XB_SIZE_IS_DWORD_IN_ANOTHER_STRUCT_MEMBER:
                    {
                        DRM_VOID *pvElement = NULL;

                        /*
                        ** The wOffset value represents the offset from the containing structure where the count value is located.
                        ** The offset must be less than the current elements offset otherwise it will not have been initialized.
                        */
                        ChkArg( (DRM_DWORD)pElement->xbSize.wOffset + sizeof(DRM_DWORD) <= (DRM_DWORD)pElement->wOffsetInLwrrentStruct );
                        ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)f_pvObject, pElement->xbSize.wOffset, (DRM_SIZE_T*)&pvElement ) );
                        /* Read the DWORD sized element count value */
                        MEMCPY( &cElementCount, pvElement, sizeof(DRM_DWORD) );
                    }
                    break;

                /*
                ** For STRUCTLIST a cEntries count variable is generated by XBGen so we are expecting either
                ** XB_SIZE_IS_WORD_IN_ANOTHER_STRUCT_MEMBER or XB_SIZE_IS_DWORD_IN_ANOTHER_STRUCT_MEMBER
                */
                default:
                    DRMASSERT( !"Element count should have been one of the known values" );
                    ChkDR( DRM_E_LOGICERR );
                    break;
            }

            ChkDR( _DRM_XB_Parse_STRUCTLIST(    f_pStack,
                                                f_eAlign,
                                                cElementCount,
                                                &pElement->xbSize,
                                                pElement->cElementDescription,
            (const DRM_XB_ELEMENT_DESCRIPTION *)pElement->pElementDescription,
                                                f_pbBinaryFormat,
                                               &iLwrrent,
                                               &cbLengthLeft,
                                                pobject ) );
            break;
        case XB_ELEMENT_TYPE_BYTEARRAY:
            switch( pElement->xbSize.eSizeType )
            {
                case XB_SIZE_IS_RELATIVE_WORD:
                    {
                        DRM_WORD wDataToRead = 0;

                        /* If it's a relative size the WORD needs to be serialized as well */
                        ChkDR( DRM_DWordSubSame( &cbLengthLeft, sizeof( DRM_WORD ) ) );
                        NETWORKBYTES_TO_WORD( wDataToRead, f_pbBinaryFormat, iLwrrent );
                        ChkDR( DRM_DWordAddSame( &iLwrrent, sizeof( DRM_WORD ) ) );

                        cbDataToRead = wDataToRead;
                        /* DRM_XB_DWORDLIST structure uses a DWORD for count regardless of the serialized size */
                        MEMCPY( pobject, &cbDataToRead, sizeof(DRM_DWORD) );
                    }
                    break;

                case XB_SIZE_IS_RELATIVE_DWORD:
                    /* If it's a relative size the WORD needs to be serialized as well */
                    ChkDR( _DRM_XB_Parse_DWORD     ( f_pbBinaryFormat, &iLwrrent, &cbLengthLeft, pobject ) );
                    MEMCPY( &cbDataToRead, pobject, sizeof(DRM_DWORD) );
                    break;

                case XB_SIZE_IS_RELATIVE_DWORD_IN_BITS:
                    /* If it's a relative size the WORD needs to be serialized as well */
                    ChkDR( _DRM_XB_Parse_DWORD     ( f_pbBinaryFormat, &iLwrrent, &cbLengthLeft, pobject ) );
                    MEMCPY( &cbDataToRead, pobject, sizeof(DRM_DWORD) );

                    if( cbDataToRead % 8 != 0 )
                    {
                        ChkDR( DRM_E_XB_ILWALID_OBJECT );
                    }

                    cbDataToRead /= 8; /* Colwert bits to bytes */
                    break;

                case XB_SIZE_IS_WORD_IN_BITS_ANOTHER_STRUCT_MEMBER:
                    {
                        DRM_VOID  *pvElement   = NULL;
                        DRM_WORD   wSizeInBits = 0;

                        /*
                        ** The wOffset value represents the offset from the containing structure where the count value is located.
                        ** This offset must be less than the current elements offset, otherwise, the count will be uninitialized.
                        */
                        ChkArg( (DRM_DWORD)pElement->xbSize.wOffset + sizeof(DRM_WORD) <= (DRM_DWORD)pElement->wOffsetInLwrrentStruct );
                        ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)f_pvObject, pElement->xbSize.wOffset, (DRM_SIZE_T*)&pvElement ) );
                        MEMCPY( &wSizeInBits, pvElement, sizeof(DRM_WORD) );

                        if( wSizeInBits % 8 != 0 )
                        {
                            ChkDR( DRM_E_XB_ILWALID_OBJECT );
                        }

                        cbDataToRead = ( DRM_DWORD )(wSizeInBits / 8); /* Colwert bits to bytes */
                    }
                    break;

                case XB_SIZE_IS_ABSOLUTE:
                    DRMASSERT( pElement->xbSize.wMinSize == pElement->xbSize.wMaxSize );
                    cbDataToRead = pElement->xbSize.wMinSize;
                    break;

                case XB_SIZE_IS_REMAINING_DATA_IN_OBJECT:
                    DRMASSERT( iElement == (f_cElements - 1) );
                    cbDataToRead = cbLengthLeft;
                    break;

                case XB_SIZE_IS_DATA_IN_OBJECT_FOR_WEAKREF:
                    /*
                    ** Containers and objects can have a RawData element that allows a weak reference pointer to the objects raw data.
                    ** The RawData XBinary byte array does not cause the serialization offsets to be changed.  It simply creates a reference
                    ** object to the start of the objects raw data.
                    */
                    {
                        DRM_XB_BYTEARRAY *pxbbaRawData = NULL;

                        ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)f_pvObject, pElement->wOffsetInLwrrentStruct, (DRM_SIZE_T*)&pxbbaRawData ) );

                        pxbbaRawData->fValid       = TRUE;
                        pxbbaRawData->cbData       = f_cbObject;
                        pxbbaRawData->iData        = f_iObject;
                        pxbbaRawData->pbDataBuffer = ( DRM_BYTE * )f_pbBinaryFormat;
                    }
                    /* Move to the next element since we are done with the XBinary byte array */
                    continue;

                default:
                    DRMASSERT( !"Element size should have been one of the known values" );
                    ChkDR( DRM_E_LOGICERR );
                    break;
            }

            ChkDR( _DRM_XB_Parse_BYTEARRAY(
                f_eAlign,
                &pElement->xbSize,
                f_pbBinaryFormat,
                &iLwrrent,
                &cbLengthLeft,
                cbDataToRead,
                pobject ) );

            break;

        case XB_ELEMENT_TYPE_ILWALID:
        default:
            DRMASSERT( !"Unsupported object type.  Most likely there is an error in the format description tables." );
            ChkDR( DRM_E_XB_UNKNOWN_ELEMENT_TYPE );
        }
    }

    if( f_pcbRead != NULL )
    {
        ChkDR( DRM_DWordSub( f_cbObject, cbLengthLeft, f_pcbRead ) );
    }

    /* PREFAST uses the SAL correctly at call sites but not when checking for overflow using pContainer->fValid */
    __analysis_assume( f_cbObject >= DRM_OFFSET_OF( DRM_XB_EMPTY, fValid ) + sizeof( pContainer->fValid ) );
    pContainer->fValid = TRUE;

ErrorExit:
    return dr;
}

GCC_ATTRIB_NO_STACK_PROTECT()
static DRM_NO_INLINE DRM_RESULT DRM_CALL _DRM_XB_Parse_UnknownContainer(
    __in                                          DRM_STACK_ALLOCATOR_CONTEXT *f_pStack,
    __in                                          DRM_DWORD                    f_dwMaxRelwrsionDepth,
    __in                                          DRM_WORD                     f_wLwrrentContainer,
    __in                                          DRM_WORD                     f_wFlags,
    __in                                  const   DRM_XB_FORMAT_DESCRIPTION   *f_pFormat,
    __in_bcount( f_iObject + f_cbObject ) const   DRM_BYTE                    *f_pbBinaryFormat,
    __in                                          DRM_DWORD                    f_iObject,
    __in                                          DRM_DWORD                    f_cbObject,
    __inout                                       DRM_VOID                    *f_pvObject )
{
    CLAW_AUTO_RANDOM_CIPHER

    DRM_RESULT                dr = DRM_SUCCESS;
    DRM_DWORD                 iLwrrent = 0;
    DRM_WORD                  wFlags = 0;
    DRM_WORD                  wType = 0;
    DRM_DWORD                 cbChild    = 0;
    DRM_XB_UNKNOWN_CONTAINER *pContainer = ( DRM_XB_UNKNOWN_CONTAINER* )f_pvObject;

    ChkArg( f_pbBinaryFormat != NULL );
    ChkArg( f_pFormat        != NULL );
    ChkArg( f_pvObject       != NULL );

    ChkBOOL( f_dwMaxRelwrsionDepth != 0, DRM_E_XB_ILWALID_OBJECT );

    /* Initialize unknown container */
    pContainer->fValid             = TRUE;
    pContainer->pObject            = NULL;
    pContainer->pUnkChildcontainer = NULL;
    pContainer->wFlags             = f_wFlags;
    pContainer->wType              = f_wLwrrentContainer;

    for( iLwrrent = f_iObject;
         iLwrrent < f_iObject + f_cbObject;
         iLwrrent += cbChild )
    {
        DRM_DWORD  dwResult  = 0;
        DRM_DWORD  dwLwrrent = 0;
        DRM_DWORD  dwCbChild = 0;

        /* Verify that there's at least enough space for a valid empty object */
        DRMCASSERT( XB_BASE_OBJECT_LENGTH >= ( sizeof( DRM_WORD ) * 2 + sizeof( DRM_DWORD ) ) );
        ChkDR( DRM_DWordAdd( f_iObject, f_cbObject, &dwResult ) );
        ChkDR( DRM_DWordSubSame( &dwResult, iLwrrent ) );
        ChkBOOL( dwResult >= XB_BASE_OBJECT_LENGTH, DRM_E_XB_ILWALID_OBJECT );

        /* Parse flags, type, and size (base object length) */
        NETWORKBYTES_TO_WORD( wFlags, f_pbBinaryFormat, iLwrrent );

        ChkDR( DRM_DWordAdd( iLwrrent, sizeof( DRM_WORD ), &dwResult ) );
        NETWORKBYTES_TO_WORD( wType, f_pbBinaryFormat, dwResult );

        ChkDR( DRM_DWordAddSame( &dwResult, sizeof( DRM_WORD ) ) );

        NETWORKBYTES_TO_DWORD( cbChild, f_pbBinaryFormat, dwResult );

        /* Verify that the claimed object size has at least enough space for a valid empty object */
        ChkBOOL( cbChild >= XB_BASE_OBJECT_LENGTH, DRM_E_XB_ILWALID_OBJECT );

        /* Verify that there's at least enough space for the claimed object size */
        ChkDR( DRM_DWordAdd( f_iObject, f_cbObject, &dwResult ) );
        ChkDR( DRM_DWordSubSame( &dwResult, iLwrrent ) );
        ChkBOOL( cbChild <= dwResult, DRM_E_XB_ILWALID_OBJECT );

        /* Only unknown containers and unknown objects are allowed under an unknown container */
        if( wType != XB_OBJECT_TYPE_UNKNOWN && wType != XB_OBJECT_TYPE_UNKNOWN_CONTAINER )
        {
            ChkBOOL( !_XB_IsKnownObjectType( f_pFormat, wType ), DRM_E_XB_ILWALID_OBJECT );
        }

        ChkDR( DRM_DWordAdd(iLwrrent, XB_BASE_OBJECT_LENGTH, &dwLwrrent) );
        ChkDR( DRM_DWordSub(cbChild, XB_BASE_OBJECT_LENGTH, &dwCbChild) );

        if (wFlags & XB_FLAGS_CONTAINER)
        {
            DRM_XB_UNKNOWN_CONTAINER *pNewObj = NULL;

            ChkDR( DRM_STK_Alloc( f_pStack,
                                  sizeof(DRM_XB_UNKNOWN_CONTAINER),
                                 (DRM_VOID**)&pNewObj ) );

            /* Initialize linked lists and object pointer */
            pNewObj->fValid                = FALSE;
            pNewObj->pNext                 = pContainer->pUnkChildcontainer;
            pNewObj->pUnkChildcontainer    = NULL;
            pNewObj->pObject               = NULL;
            pContainer->pUnkChildcontainer = pNewObj;

            /* Found a container: relwrse to parse the container */
            ChkDR( _DRM_XB_Parse_UnknownContainer(
                f_pStack,
                f_dwMaxRelwrsionDepth - 1,
                wType,
                wFlags,
                f_pFormat,
                f_pbBinaryFormat,
                dwLwrrent,
                dwCbChild,
                ( DRM_VOID * )pNewObj ) );
        }
        else
        {
            DRM_XB_UNKNOWN_OBJECT *pNewObj = NULL;

            ChkDR( DRM_STK_Alloc( f_pStack,
                                  sizeof(DRM_XB_UNKNOWN_OBJECT),
                                 (DRM_VOID**)&pNewObj ) );

            /* Initialize linked list */
            pNewObj->fValid     = FALSE;
            pNewObj->pNext      = pContainer->pObject;
            pContainer->pObject = pNewObj;

            ChkDR( _DRM_XB_Parse_UNKNOWN_OBJECT(
                f_pFormat,
                f_pbBinaryFormat,
                wType,
                wFlags,
                dwLwrrent,
                dwCbChild,
                pNewObj ) );
        }
    }

ErrorExit:
    return dr;
}

static DRM_NO_INLINE DRM_RESULT DRM_CALL _DRM_XB_Parse_Unknown(
    __in                                          DRM_STACK_ALLOCATOR_CONTEXT *f_pStack,
    __in                                          DRM_DWORD                    f_dwMaxRelwrsionDepth,
    __in                                          DRM_WORD                     f_wLwrrentContainer,
    __in                                          DRM_WORD                     f_wFlags,
    __in                                          DRM_WORD                     f_wType,
    __in                                  const   DRM_XB_FORMAT_DESCRIPTION   *f_pFormat,
    __in_bcount( f_iObject + f_cbObject ) const   DRM_BYTE                    *f_pbBinaryFormat,
    __in                                          DRM_DWORD                    f_iObject,
    __in                                          DRM_DWORD                    f_cbObject,
    __inout                                       DRM_VOID                    *f_pvObject )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_VOID  *pobject   = NULL;
    DRM_DWORD  dwLwrrent = 0;
    DRM_DWORD  dwCbChild = 0;

    if (f_wFlags & XB_FLAGS_CONTAINER)
    {
        DRM_XB_ENTRY_DESCRIPTION *pEntry = NULL;

        if( _XB_IsObjectTypeAChildOf( f_pFormat, XB_OBJECT_TYPE_UNKNOWN_CONTAINER, f_wLwrrentContainer, &pEntry ) )
        {
            ChkBOOL( pEntry != NULL, DRM_E_XB_ILWALID_OBJECT );

            if( pEntry->fDuplicateAllowed )
            {
                /*
                ** If duplicates are allowed this means that the offset pobject points to needs to be
                ** the tail of a linked list.  We always allocate an object: it will be placed at the tail
                ** if the list is NULL
                */

                ChkDR( _XB_CreateNewListItem_UnknownContainer( f_pStack,
                                                               f_pvObject,
                                                               pEntry->wOffsetInLwrrentStruct,
                                                               pEntry->dwStructureSize,
                               ( DRM_XB_UNKNOWN_CONTAINER ** )&pobject ) );
            }
            else
            {
                ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)f_pvObject,
                                        pEntry->wOffsetInLwrrentStruct,
                                        (DRM_SIZE_T*)&pobject ) );

            }

            /* Found a container: relwrse to parse the container */
            ChkDR( DRM_DWordAdd( f_iObject, XB_BASE_OBJECT_LENGTH, &dwLwrrent ) );
            ChkDR( DRM_DWordSub( f_cbObject, XB_BASE_OBJECT_LENGTH, &dwCbChild ) );

            ChkDR( _DRM_XB_Parse_UnknownContainer(
                f_pStack,
                f_dwMaxRelwrsionDepth - 1,
                f_wType,
                f_wFlags,
                f_pFormat,
                f_pbBinaryFormat,
                dwLwrrent,
                dwCbChild,
                pobject ) );
        }
        else
        {
            /* Fail if the container is must understand and the format object does not support unknown containers */
            ChkBOOL( !(f_wFlags & XB_FLAGS_MUST_UNDERSTAND), DRM_E_XB_ILWALID_OBJECT );
        }
    }
    else
    {
        DRM_XB_ENTRY_DESCRIPTION *pEntry = NULL;

        if( _XB_IsObjectTypeAChildOf( f_pFormat, XB_OBJECT_TYPE_UNKNOWN, f_wLwrrentContainer, &pEntry ) )
        {
            ChkBOOL( pEntry != NULL, DRM_E_XB_ILWALID_OBJECT );

            if( pEntry->fDuplicateAllowed )
            {
                /*
                ** If duplicates are allowed this means that the offset pobject points to needs to be
                ** the head of a linked list.  We always allocate an object: it will be placed at the head
                ** if the list is NULL
                */

                ChkDR( _XB_CreateNewListItem_UnknownObject( f_pStack,
                                                            f_pvObject,
                                                            pEntry->wOffsetInLwrrentStruct,
                                                            pEntry->dwStructureSize,
                               ( DRM_XB_UNKNOWN_OBJECT ** )&pobject ) );
            }
            else
            {
                ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)f_pvObject,
                                        pEntry->wOffsetInLwrrentStruct,
                                        (DRM_SIZE_T*)&pobject ) );
            }

            ChkDR( DRM_DWordAdd( f_iObject, XB_BASE_OBJECT_LENGTH, &dwLwrrent ) );
            ChkDR( DRM_DWordSub( f_cbObject, XB_BASE_OBJECT_LENGTH, &dwCbChild ) );

            ChkDR( _DRM_XB_Parse_UNKNOWN_OBJECT(
                f_pFormat,
                f_pbBinaryFormat,
                f_wType,
                f_wFlags,
                dwLwrrent,
                dwCbChild,
                (DRM_XB_UNKNOWN_OBJECT*) pobject ) );
        }
        else
        {
            /* Fail if the object is must understand and the format object does not support unknown objects */
            ChkBOOL( !(f_wFlags & XB_FLAGS_MUST_UNDERSTAND), DRM_E_XB_ILWALID_OBJECT );
        }
    }

ErrorExit:
    return dr;
}

/*********************************************************************
**
** Function: _DRM_XB_GetMaxContainerDepth
**
** Synopsis: Internal API that callwlates the maximum container relwrsion depth
**           for a given XBinary format descriptor.
**
** Arguments:
**
** [f_pFormat] -- Descriptor for the entire format
**
**********************************************************************/
static DRM_NO_INLINE DRM_DWORD DRM_CALL _DRM_XB_GetMaxContainerDepth(
    __in    const   DRM_XB_FORMAT_DESCRIPTION   *f_pFormat )
{
    DRM_DWORD dwResult             = DRM_XB_PARSER_MIN_CONTAINER_DEPTH;
    DRM_BOOL  fHasUnknownContainer = FALSE;
    DRM_DWORD idx;

    DRMASSERT( f_pFormat != NULL );

    for( idx = 0; idx < f_pFormat->cEntryDescriptions; idx++ )
    {
        if( XB_OBJECT_TYPE_UNKNOWN_CONTAINER == f_pFormat->pEntryDescriptions[idx].wType )
        {
            fHasUnknownContainer = TRUE;
            break;
        }
    }

    if( fHasUnknownContainer )
    {
        /*
        ** Cannot overflow because it is just adding two small constant values
        ** (DRM_XB_PARSER_MAX_CONTAINER_DEPTH and DRM_XB_PARSER_MAX_UNKNOWN_CONTAINER_DEPTH).
        */
        dwResult += DRM_XB_PARSER_MAX_UNKNOWN_CONTAINER_DEPTH;
    }

    if( DRM_FAILED( DRM_DWordAddSame( &dwResult, f_pFormat->cContainerDepth ) ) )
    {
        DRMASSERT(FALSE);
        dwResult = DRM_XB_PARSER_MAX_CONTAINER_DEPTH;
    }

    return dwResult;
}


static DRM_NO_INLINE DRM_RESULT DRM_CALL _DRM_XB_CheckForContainerFillPatern(
    __in                      const DRM_XB_FORMAT_DESCRIPTION   *f_pFormat,
    __in                            DRM_WORD                     f_wLwrrentContainer,
    __in                            DRM_DWORD                    f_cbData,
    __in_bcount( f_cbData )   const DRM_BYTE                    *f_pbData,
    __out                           DRM_BOOL                    *f_pfIsFillPattern
    )
{
    DRM_RESULT                        dr          = DRM_SUCCESS;
    DRM_DWORD                         idxEntry    = 0;
    const DRM_XB_ELEMENT_DESCRIPTION *pElems      = NULL;
    const DRM_XB_ENTRY_DESCRIPTION   *pEntry      = NULL;
    DRM_WORD                          wContainer  = 0;

    ChkArg( f_pfIsFillPattern != NULL );
    ChkArg( f_pFormat         != NULL );
    ChkArg( f_pbData          != NULL );
    ChkArg( f_cbData           > 0    );

    *f_pfIsFillPattern = FALSE;

    /*
    ** The global header does not have any entry descriptions, therefore we use the root object which
    ** is added if the outer container has reserved memory.
    */
    wContainer = f_wLwrrentContainer == XB_OBJECT_GLOBAL_HEADER ? ( DRM_WORD )XB_OBJECT_TYPE_ROOT : f_wLwrrentContainer;
    idxEntry   = _XB_MapObjectTypeToEntryDescriptionIndex(f_pFormat, wContainer);
    ChkBOOL( idxEntry < f_pFormat->cEntryDescriptions, DRM_E_XB_ILWALID_OBJECT );

    pEntry = &f_pFormat->pEntryDescriptions[ idxEntry ];

    if( 0 == (pEntry->wFlags & XB_FLAGS_CONTAINER_HAS_RESERVE_BUFFER) )
    {
        goto ErrorExit;
    }

    pElems = DRM_REINTERPRET_CONST_CAST( const DRM_XB_ELEMENT_DESCRIPTION, pEntry->pContainerOrObjectDescriptions );

    if( pElems != NULL )
    {
        DRM_DWORD idx;

        for( idx = 0; idx < pEntry->cContainerOrObjectDescriptions; idx++ )
        {
            if( pElems[idx].xbSize.eSizeType == XB_SIZE_IS_CONTAINER_FILL_PATTERN )
            {
                DRM_BOOL        fIsMatch     = TRUE;
                const DRM_BYTE  bFillPattern = ( DRM_BYTE )( pElems[idx].xbSize.wOffset & 0xFF );
                const DRM_BYTE *pbData       = f_pbData;
                DRM_DWORD       cbData       = f_cbData;

                while( cbData > 0 )
                {
                    if( *pbData != bFillPattern )
                    {
                        fIsMatch = FALSE;
                        break;
                    }

                    pbData++;
                    cbData--;
                }

                *f_pfIsFillPattern = fIsMatch;
                break;
            }
        }
    }

ErrorExit:
    return dr;
}

/*********************************************************************
**
** Function: _DRM_XB_Parse_Container
**
** Synopsis: API that initiates the parsing of a XBinary container
**
** Arguments:
**
** [f_pStack]             -- Pointer to a stack context for dynamic allocations
** [f_dwMaxRelwrsionDepth]-- The maximum relwrsion depth for nested containers
** [f_wLwrrentContainer]  -- The container that we are lwrrently parsing
** [f_pFormat]            -- Descriptor for the entire format
** [f_pbBinaryFormat]     -- binary stream to deserialized the object from
** [f_wElementCountType]  -- The element type to track the instance count.
** [f_pcElementCount]     -- The pointer of the cEntries element that will
**                           hold the count value.
** [f_iObject]            -- index into f_pbBinaryFormat
** [f_cbObject]           -- count of bytes in f_pbBinaryFormat
** [f_pvObject]           -- VOID pointer of a struct to deserialized the object into
**
**********************************************************************/
GCC_ATTRIB_NO_STACK_PROTECT()
static DRM_NO_INLINE DRM_RESULT DRM_CALL _DRM_XB_Parse_Container(
    __in                                          DRM_STACK_ALLOCATOR_CONTEXT *f_pStack,
    __in                                          DRM_DWORD                    f_dwMaxRelwrsionDepth,
    __in                                          DRM_WORD                     f_wLwrrentContainer,
    __in                                  const   DRM_XB_FORMAT_DESCRIPTION   *f_pFormat,
    __in_bcount( f_iObject + f_cbObject ) const   DRM_BYTE                    *f_pbBinaryFormat,
    __in                                          DRM_WORD                     f_wElementCountType,
    __inout_opt                                   DRM_WORD                    *f_pcElementCount,
    __in                                          DRM_DWORD                    f_iObject,
    __in                                          DRM_DWORD                    f_cbObject,
    __inout                                       DRM_VOID                    *f_pvObject )
{
    CLAW_AUTO_RANDOM_CIPHER

    DRM_RESULT    dr                = DRM_SUCCESS;
    DRM_DWORD     iLwrrent          = 0;
    DRM_WORD      wFlags            = 0;
    DRM_WORD      wType             = 0;
    DRM_DWORD     cbChild           = 0;
    DRM_XB_EMPTY *pContainer        = ( DRM_XB_EMPTY* )f_pvObject;
    DRM_WORD      cElementTypeCount = 0;

    ChkArg( f_pbBinaryFormat != NULL );
    ChkArg( f_pFormat   != NULL );
    ChkArg( f_pvObject  != NULL );

    ChkBOOL( f_dwMaxRelwrsionDepth != 0, DRM_E_XB_ILWALID_OBJECT );

    pContainer->fValid = TRUE;

    for( iLwrrent = f_iObject;
         iLwrrent < f_iObject + f_cbObject;
         iLwrrent += cbChild )
    {
        DRM_VOID  *pobject   = NULL;
        DRM_DWORD  dwResult  = 0;
        DRM_DWORD  dwLwrrent = 0;
        DRM_DWORD  dwCbChild = 0;

        /* Verify that there's at least enough space for a valid empty object */
        DRMCASSERT( XB_BASE_OBJECT_LENGTH >= ( sizeof( DRM_WORD ) * 2 + sizeof( DRM_DWORD ) ) );
        ChkDR( DRM_DWordAdd( f_iObject, f_cbObject, &dwResult ) );
        ChkDR( DRM_DWordSubSame( &dwResult, iLwrrent ) );
        ChkBOOL( dwResult >= XB_BASE_OBJECT_LENGTH, DRM_E_XB_ILWALID_OBJECT );

        /* Parse flags, type, and size (base object length) */
        NETWORKBYTES_TO_WORD( wFlags,  f_pbBinaryFormat, iLwrrent );

        ChkDR( DRM_DWordAdd( iLwrrent, sizeof( DRM_WORD ), &dwResult ) );
        NETWORKBYTES_TO_WORD( wType,   f_pbBinaryFormat, dwResult );

        if( wType == f_wElementCountType )
        {
            cElementTypeCount++;
        }

        ChkDR( DRM_DWordAdd( iLwrrent, sizeof( DRM_DWORD ), &dwResult ) );
        NETWORKBYTES_TO_DWORD( cbChild, f_pbBinaryFormat, dwResult );

        /* Verify that the claimed object size has at least enough space for a valid empty object */
        ChkBOOL( cbChild >= XB_BASE_OBJECT_LENGTH, DRM_E_XB_ILWALID_OBJECT );

        /* Verify that there's at least enough space for the claimed object size */
        ChkDR( DRM_DWordAdd( f_iObject, f_cbObject, &dwResult ) );
        ChkDR( DRM_DWordSubSame( &dwResult, iLwrrent ) );
        ChkBOOL( cbChild <= dwResult, DRM_E_XB_ILWALID_OBJECT );

        if( !_XB_IsKnownObjectType( f_pFormat, wType ) )
        {
            ChkDR( _DRM_XB_Parse_Unknown(
                f_pStack,
                f_dwMaxRelwrsionDepth - 1,
                f_wLwrrentContainer,
                wFlags,
                wType,
                f_pFormat,
                f_pbBinaryFormat,
                iLwrrent,
                cbChild,
                f_pvObject ) );
        }
        else
        {
            ChkBOOL( f_wLwrrentContainer == f_pFormat->pEntryDescriptions[_XB_MapObjectTypeToEntryDescriptionIndex(f_pFormat, wType)].wParent, DRM_E_XB_ILWALID_OBJECT );

            if( f_pFormat->pEntryDescriptions[_XB_MapObjectTypeToEntryDescriptionIndex(f_pFormat, wType)].fDuplicateAllowed )
            {
                /*
                ** If duplicates are allowed this means that the offset pobject points to needs to be
                ** the head of a linked list.  We always allocate an object: it will be placed at the head
                ** if the list is NULL
                */

                ChkDR( _XB_CreateNewListItem( f_pStack,
                                              f_pvObject,
                                              f_pFormat->pEntryDescriptions[_XB_MapObjectTypeToEntryDescriptionIndex(f_pFormat, wType)].wOffsetInLwrrentStruct,
                                              f_pFormat->pEntryDescriptions[_XB_MapObjectTypeToEntryDescriptionIndex(f_pFormat, wType)].dwStructureSize,
                         (DRM_XB_BASELIST **)&pobject ) );
            }
            else
            {
                ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)f_pvObject,
                                        f_pFormat->pEntryDescriptions[_XB_MapObjectTypeToEntryDescriptionIndex(f_pFormat, wType)].wOffsetInLwrrentStruct,
                                        (DRM_SIZE_T*)&pobject ) );
            }

            if( wFlags & XB_FLAGS_CONTAINER )
            {
                DRM_WORD                          cbExtraSkip      = 0;
                const DRM_XB_ELEMENT_DESCRIPTION *pEntryDescr      = NULL;
                DRM_WORD                          cEntryDescr      = 0;
                DRM_WORD                          wSpecificElement = 0;
                DRM_WORD                         *pcElementCount   = NULL;

                cEntryDescr =                                       f_pFormat->pEntryDescriptions[_XB_MapObjectTypeToEntryDescriptionIndex(f_pFormat, wType)].cContainerOrObjectDescriptions;
                pEntryDescr = ( const DRM_XB_ELEMENT_DESCRIPTION * )f_pFormat->pEntryDescriptions[_XB_MapObjectTypeToEntryDescriptionIndex(f_pFormat, wType)].pContainerOrObjectDescriptions;

                if( pEntryDescr != NULL )
                {
                    DRM_DWORD idx = 0;

                    for( idx = 0;
                         idx < cEntryDescr;
                         idx ++ )
                     {
                        /* Only containers can have XB_ELEMENT_TYPE_ELEMENT_COUNT items.  These are used to add a count variable to the XB container object. */
                        if( pEntryDescr[idx].eElementType     == XB_ELEMENT_TYPE_ELEMENT_COUNT
                         && pEntryDescr[idx].xbSize.eSizeType == XB_SIZE_IS_ELEMENT_COUNT )
                        {
                            wSpecificElement = pEntryDescr[idx].xbSize.wMinSize;
                            ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)pobject, pEntryDescr[idx].wOffsetInLwrrentStruct, (DRM_SIZE_T*)&pcElementCount ) );
                        }

                        /*
                        ** Containers and objects can have a RawData element that allows a weak reference pointer to the objects raw data.
                        ** The RawData XBinary byte array does not cause the serialization offsets to be changed.  It simply creates a reference
                        ** object to the start of the objects raw data.
                        */
                        if(    pEntryDescr[idx].eElementType     == XB_ELEMENT_TYPE_BYTEARRAY
                            && pEntryDescr[idx].xbSize.eSizeType == XB_SIZE_IS_DATA_IN_OBJECT_FOR_WEAKREF )
                        {
                            DRM_XB_BYTEARRAY *pxbbaRawData = NULL;

                            ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)pobject, pEntryDescr[idx].wOffsetInLwrrentStruct, (DRM_SIZE_T*)&pxbbaRawData ) );

                            pxbbaRawData->fValid       = TRUE;
                            pxbbaRawData->cbData       = cbChild;
                            pxbbaRawData->iData        = iLwrrent;
                            pxbbaRawData->pbDataBuffer = ( DRM_BYTE * )f_pbBinaryFormat;
                        }
                     }
                }

                if( pEntryDescr == NULL )
                {
                    /*
                    ** Special case for non-std XB formats.
                    ** If the container had extra space in it we skip that space
                    ** as it's listed in cContainerOrObjectDescriptions
                    ** V1 KeyFile format uses this
                    */
                    cbExtraSkip = cEntryDescr;
                }

                /* Found a container: relwrse to parse the container */
                ChkDR( DRM_DWordAdd( XB_BASE_OBJECT_LENGTH, cbExtraSkip, &dwResult ) );
                ChkDR( DRM_DWordAdd( iLwrrent, dwResult, &dwLwrrent ) );
                ChkDR( DRM_DWordSub( cbChild, dwResult, &dwCbChild ) );

                ChkDR( _DRM_XB_Parse_Container(
                    f_pStack,
                    f_dwMaxRelwrsionDepth - 1,
                    wType,
                    f_pFormat,
                    f_pbBinaryFormat,
                    wSpecificElement,
                    pcElementCount,
                    dwLwrrent,
                    dwCbChild,
                    pobject ) );
            }
            else
            {
                ChkDR( DRM_DWordAdd( iLwrrent, XB_BASE_OBJECT_LENGTH, &dwLwrrent ) );
                ChkDR( DRM_DWordSub( cbChild, XB_BASE_OBJECT_LENGTH, &dwCbChild ) );

                ChkDR( _DRM_XB_Parse_Object(
                    f_pStack,
                    f_pFormat->pHeaderDescription != NULL ? f_pFormat->pHeaderDescription->eAlign : XB_ALIGN_1_BYTE,
                    (const DRM_XB_ELEMENT_DESCRIPTION*)(f_pFormat->pEntryDescriptions[_XB_MapObjectTypeToEntryDescriptionIndex(f_pFormat, wType)].pContainerOrObjectDescriptions),
                    f_pFormat->pEntryDescriptions[_XB_MapObjectTypeToEntryDescriptionIndex(f_pFormat, wType)].cContainerOrObjectDescriptions,
                    f_pbBinaryFormat,
                    dwLwrrent,
                    dwCbChild,
                    pobject,
                    NULL ) );
            }
        }
    }

    if( f_pcElementCount != NULL )
    {
        *f_pcElementCount = cElementTypeCount;
    }

ErrorExit:

    if( DRM_FAILED(dr) )
    {
        DRM_BOOL  fIsFillPattern = FALSE;
        DRM_DWORD dwLength       = 0;

        /*
        ** Some XBinary formats (e.g. bincert) can pad containers with reserved memory and a given byte fill pattern.  The following code
        ** looks to see if we tried to parse this padding buffer.  The callwlations below generate the length remaining in the container
        ** object.
        */
        if( DRM_SUCCEEDED( DRM_DWordAdd( f_iObject, f_cbObject, &dwLength ) ) &&
            DRM_SUCCEEDED( DRM_DWordSubSame( &dwLength, iLwrrent ) ) &&
            DRM_SUCCEEDED( _DRM_XB_CheckForContainerFillPatern( f_pFormat, f_wLwrrentContainer, dwLength, &f_pbBinaryFormat[iLwrrent], &fIsFillPattern ) ) &&
            fIsFillPattern )
        {
            dr = DRM_SUCCESS;
        }
    }

    return dr;
}

/*********************************************************************
**
** Function: DRM_XB_UnpackHeader
**
** Synopsis: API that initiates the parsing of the header information
** for an XBinary format.
**
** Arguments:
**
** [f_pb]              -- binary stream to deserialized
** [f_cb]              -- count of bytes in f_pb
** [f_pStack]          -- Pointer to a stack context for dynamic allocations
** [f_pformat]         -- Array of descriptors for the entire format (multiple versions)
** [f_cformat]         -- Count of descriptors in f_pformat
** [f_pdwVersionFound] -- Optional. Once successfully deserialized the version of
**                        the format found is placed in this out value.
** [f_pcbParsed]       -- Optional. The size in bytes that were processed from f_pb to
**                        parse the format header information.
** [f_pcbContainer]    -- Optional. The size in bytes of the format's outer container.
** [f_piFormat]        -- Optional. The index of the format version processed.
** [f_pStruct]         -- VOID pointer of a struct to deserialized the object into
**
**********************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_XB_UnpackHeader(
    __in_bcount( f_cb )       const DRM_BYTE                    *f_pb,
    __in                            DRM_DWORD                    f_cb,
    __inout                         DRM_STACK_ALLOCATOR_CONTEXT *f_pStack,
    __in_ecount( f_cformat )  const DRM_XB_FORMAT_DESCRIPTION   *f_pformat,
    __in                            DRM_DWORD                    f_cformat,
    __out_opt                       DRM_DWORD                   *f_pdwVersionFound,
    __out_opt                       DRM_DWORD                   *f_pcbParsed,
    __out_opt                       DRM_DWORD                   *f_pcbContainer,
    __out_opt                       DRM_DWORD                   *f_piFormat,
    __inout                         DRM_VOID                    *f_pStruct )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT          dr               = DRM_SUCCESS;
    DRM_UINT64          qwMagicConstant  = DRM_UI64LITERAL( 0, 0 );
    DRM_DWORD           dwVersion        = 0;
    DRM_DWORD           cbOuterContainer = 0;
    DRM_DWORD           iFormat          = 0;
    DRM_DWORD           icb              = 0;
    DRM_BOOL            fFound           = FALSE;
    DRM_BOOL            fFoundHeader     = FALSE;
    DRM_DWORD           dwResult         = 0;

    ChkArg( f_pb           != NULL );
    ChkArg( f_cb           != 0    );
    ChkArg( f_pStruct      != NULL );
    ChkArg( f_pformat      != NULL );
    ChkArg( f_pformat->pHeaderDescription != NULL );

    ChkBOOL( f_cb > XB_HEADER_LENGTH( f_pformat->pHeaderDescription->eFormatIdLength ), DRM_E_XB_ILWALID_OBJECT );

    switch( f_pformat->pHeaderDescription->eFormatIdLength )
    {
    case XB_FORMAT_ID_LENGTH_DWORD:
        {
            DRM_DWORD dwMagicConstant; /* initialized in the next line */
            NETWORKBYTES_TO_DWORD( dwMagicConstant, f_pb, icb );
            qwMagicConstant = DRM_UI64HL( 0, dwMagicConstant );
        }
        icb += XB_FORMAT_ID_LENGTH_DWORD;
        break;
    case XB_FORMAT_ID_LENGTH_QWORD:
        NETWORKBYTES_TO_QWORD( qwMagicConstant, f_pb, icb );
        icb += XB_FORMAT_ID_LENGTH_QWORD;
        break;
    default:
        ChkDR( DRM_E_XB_ILWALID_OBJECT );
        break;
    }

    NETWORKBYTES_TO_DWORD( dwVersion, f_pb, icb );
    if( f_pdwVersionFound != NULL )
    {
        *f_pdwVersionFound = dwVersion;
    }

    DRMASSERT( icb <= XB_FORMAT_ID_LENGTH_QWORD );
    /* icb is initialized to small value, no need for safe math */
    icb += sizeof( DRM_DWORD );

    for( iFormat = 0; !fFound && iFormat < f_cformat; iFormat++ )
    {
        if( DRM_UI64Eql( f_pformat[iFormat].pHeaderDescription->qwFormatIdentifier, qwMagicConstant ) )
        {
            fFoundHeader = TRUE;

            if( f_pformat[iFormat].pHeaderDescription->dwFormatVersion == dwVersion )
            {
                fFound = TRUE;
            }
        }
    }
    iFormat--;

    if( !fFound )
    {
        ChkDR( fFoundHeader ? DRM_E_XB_ILWALID_VERSION : DRM_E_XB_ILWALID_OBJECT );
    }

    ChkBOOL( f_pformat[iFormat].pHeaderDescription != NULL, DRM_E_XB_ILWALID_OBJECT );

    if( f_pformat[iFormat].pHeaderDescription->eAlign > XB_ALIGN_1_BYTE )
    {
        DRMASSERT( f_pformat[iFormat].pHeaderDescription->eAlign == XB_ALIGN_8_BYTE ||
                   f_pformat[iFormat].pHeaderDescription->eAlign == XB_ALIGN_16_BYTE );
        /* We can only guarantee alignment if the start of the buffer is aligned properly. */
        ChkBOOL( ((DRM_SIZE_T)f_pb & ( f_pformat[iFormat].pHeaderDescription->eAlign - 1)) == 0, DRM_E_XB_ILWALID_ALIGNMENT );
    }

    if( f_pformat[iFormat].pHeaderDescription->pEntryDescription == NULL )
    {
        NETWORKBYTES_TO_DWORD( cbOuterContainer, f_pb, icb );
        DRMASSERT( icb <= XB_FORMAT_ID_LENGTH_QWORD + sizeof(DRM_DWORD) );
        /* icb is always small here */
        icb += sizeof( DRM_DWORD );
    }
    else
    {
        DRM_DWORD  cbRead    = 0;
        DRM_DWORD *pdwStruct = NULL;

        /*
        ** Extended header data must include the dwVersion element as the first item
        ** after fValid.  This will contain the XBinary format version.  This is used
        ** by the XBinary XMR parser to get the XMR license version.
        */
        DRMASSERT( icb >= sizeof( DRM_DWORD ) );
        icb -= sizeof( DRM_DWORD ); /* icb is at least sizeof(DRM_DWORD) before this line */


        /*
        ** Special case.  This format has extended header data, which means that it's possible
        ** the length may not be where we expect
        */
        ChkDR( DRM_DWordSub( f_cb, icb, &dwResult ) );

        /* Offset of header extra data */
        ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)f_pStruct,
                                f_pformat[iFormat].pHeaderDescription->pEntryDescription->wOffsetInLwrrentStruct,
                                (DRM_SIZE_T*)&pdwStruct ) );

        ChkDR( _DRM_XB_Parse_Object(
                f_pStack,
                f_pformat->pHeaderDescription != NULL ? f_pformat->pHeaderDescription->eAlign : XB_ALIGN_1_BYTE,
                (const DRM_XB_ELEMENT_DESCRIPTION*)f_pformat[iFormat].pHeaderDescription->pEntryDescription->pContainerOrObjectDescriptions,
                f_pformat[iFormat].pHeaderDescription->pEntryDescription->cContainerOrObjectDescriptions,
                f_pb,
                icb,
                dwResult,
                pdwStruct,
                &cbRead) );

        ChkDR( DRM_DWordAddSame( &icb, cbRead ) );

        /* Offset in extra data */
        if( f_pformat[iFormat].pHeaderDescription->wOffsetOfSizeInHeaderStruct != DRM_XB_EXTENDED_HEADER_SIZE_OFFSET_ILWALID )
        {
            /* The header includes an explicit size.  Use it. */
            ChkDR( DRM_DWordPtrAddSame( (DRM_SIZE_T*)&pdwStruct,
                                    f_pformat[iFormat].pHeaderDescription->wOffsetOfSizeInHeaderStruct ) );
            cbOuterContainer = *pdwStruct;
        }
        else
        {
            /* The header does not include an explicit size.  The size is implied by the total size. */
            cbOuterContainer = f_cb;
        }
    }

    ChkBOOL( cbOuterContainer <= f_cb, DRM_E_XB_ILWALID_OBJECT );

    if( f_pcbParsed != NULL )
    {
        *f_pcbParsed = icb;
    }

    if( f_pcbContainer != NULL )
    {
        ChkDR( DRM_DWordSub( cbOuterContainer, icb, f_pcbContainer ) );
    }

    if( f_piFormat != NULL )
    {
        *f_piFormat = iFormat;
    }

    /*
    ** Special case for the RawData XBinary byte array for the entire xbinary
    ** format object.  This is lwrrently used by the XBinary XMR parser for
    ** signature validation.  It is always placed in the second slot for the
    ** entry descriptions.
    */
    if (f_pformat[iFormat].pEntryDescriptions != NULL &&
        f_pformat[iFormat].cEntryDescriptions > DRM_XB_PARSER_ROOT_OBJECT_INDEX &&
        f_pformat[iFormat].pEntryDescriptions[DRM_XB_PARSER_ROOT_OBJECT_INDEX].wType == XB_OBJECT_TYPE_ROOT)
    {
        const DRM_XB_ELEMENT_DESCRIPTION *pContainerEntry = NULL;
        DRM_WORD                          cContainerEntry = 0;

        cContainerEntry = f_pformat[iFormat].pEntryDescriptions[DRM_XB_PARSER_ROOT_OBJECT_INDEX].cContainerOrObjectDescriptions;
        pContainerEntry = (const DRM_XB_ELEMENT_DESCRIPTION *)f_pformat[iFormat].pEntryDescriptions[DRM_XB_PARSER_ROOT_OBJECT_INDEX].pContainerOrObjectDescriptions;

        if (cContainerEntry > 0 && pContainerEntry != NULL)
        {
            DRM_XB_BYTEARRAY *pxbbaRawData = NULL;

            ChkDR(DRM_DWordPtrAdd((DRM_SIZE_T)f_pStruct,
                pContainerEntry->wOffsetInLwrrentStruct,
                (DRM_SIZE_T *)&pxbbaRawData));

            pxbbaRawData->fValid       = TRUE;
            pxbbaRawData->cbData       = cbOuterContainer;
            pxbbaRawData->iData        = 0;
            pxbbaRawData->pbDataBuffer = (DRM_BYTE *)f_pb;
        }
    }

    /* Set the header object as valid */
    (( DRM_XB_EMPTY * )f_pStruct)->fValid = TRUE;

ErrorExit:
    return dr;
}

/*********************************************************************
**
** Function: DRM_XB_UnpackBinary
**
** Synopsis: API that initiates the parsing of a XBinary container
**
** Arguments:
**
** [f_pb]              -- binary stream to deserialized
** [f_cb]              -- count of bytes in f_pb
** [f_pStack]          -- Pointer to a stack context for dynamic allocations
** [f_pformat]         -- Array of descriptors for the entire format (multiple versions)
** [f_cformat]         -- Count of descriptors in f_pformat
** [f_pdwVersionFound] -- Optional. Once successfully deserialized the version of
**                        the format found is placed in this out value.
** [f_pcbParsed]       -- Optional. The size in bytes of the data from f_pb that
**                        was processed during deserialization.
** [f_pStruct]         -- VOID pointer of a struct to deserialized the object into
**
**********************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_XB_UnpackBinary(
    __in_bcount( f_cb )       const DRM_BYTE                    *f_pb,
    __in                            DRM_DWORD                    f_cb,
    __inout                         DRM_STACK_ALLOCATOR_CONTEXT *f_pStack,
    __in_ecount( f_cformat )  const DRM_XB_FORMAT_DESCRIPTION   *f_pformat,
    __in                            DRM_DWORD                    f_cformat,
    __out_opt                       DRM_DWORD                   *f_pdwVersionFound,
    __out_opt                       DRM_DWORD                   *f_pcbParsed,
    __inout                         DRM_VOID                    *f_pStruct )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT          dr               = DRM_SUCCESS;
    DRM_DWORD           iFormat          = 0;
    DRM_DWORD           cbParsed         = 0;
    DRM_DWORD           cbContainer      = 0;

    ChkArg( f_pb     != NULL );
    ChkArg( f_cb     != 0 );
    ChkArg( f_pStruct!= NULL );
    ChkArg( f_pformat!= NULL );
    ChkArg( f_pformat->pHeaderDescription != NULL );

    ChkDR( DRM_XB_UnpackHeader(
        f_pb,
        f_cb,
        f_pStack,
        f_pformat,
        f_cformat,
        f_pdwVersionFound,
        &cbParsed,
        &cbContainer,
        &iFormat,
        f_pStruct ) );

    ChkDR( _DRM_XB_Parse_Container(
        f_pStack,
        _DRM_XB_GetMaxContainerDepth( f_pformat ),
        XB_OBJECT_GLOBAL_HEADER,
        &(f_pformat[iFormat]),
        f_pb,
        0,
        NULL,
        cbParsed,
        cbContainer,
        f_pStruct ) );

    if( f_pcbParsed != NULL )
    {
        ChkDR( DRM_DWordAdd( cbParsed, cbContainer, f_pcbParsed ) );
    }

ErrorExit:
    return dr;
}

/*********************************************************************
**
** Function: _XB_FindChild
**
** Synopsis: API that given the start of an object will find a child object
**           with the type specified in f_wType
**
** Arguments:
**
** [f_wType]              -- Type of child object to locate
** [f_pbBuffer]           -- XBinary format to search
** [f_iLwrrentNode]       -- Index in f_pBuffer to start from
** [f_cbLwrrentNode]      -- Count of bytes in the current object
** [f_piChild]            -- Index of the child object found
** [f_pcbChild]           -- Count of bytes in the child object
**********************************************************************/
static DRM_NO_INLINE DRM_RESULT DRM_CALL _XB_FindChild(
    __in                          DRM_WORD         f_wType,
    __in_bcount(f_cbBuffer) const DRM_BYTE        *f_pbBuffer,
    __in                    const DRM_DWORD        f_cbBuffer,
    __in                          DRM_DWORD        f_iLwrrentNode,
    __in                          DRM_DWORD        f_cbLwrrentNode,
    __out                         DRM_DWORD       *f_piChild,
    __out                         DRM_DWORD       *f_pcbChild )
{
    DRM_RESULT  dr          = DRM_SUCCESS;
    DRM_DWORD   iNext       = f_iLwrrentNode;
    DRM_DWORD   cbNext      = 0;
    DRM_WORD    wNextType   = 0;
    DRM_DWORD   dwResult    = 0;
    DRM_DWORD   dwNextNode  = 0;
    DRM_DWORD   iNextMax    = 0;

    ChkArg( f_pbBuffer  != NULL );
    ChkArg( f_piChild   != NULL );
    ChkArg( f_pcbChild  != NULL );
    ChkArg( f_cbBuffer  >= sizeof(DRM_DWORD) + sizeof(DRM_WORD) );

    ChkBOOL( iNext >= f_iLwrrentNode, DRM_E_ARITHMETIC_OVERFLOW );

    ChkDR( DRM_DWordAdd( f_iLwrrentNode, f_cbLwrrentNode, &dwNextNode ) );
    /* make sure we are inside the buffer */
    ChkArg( dwNextNode <= f_cbBuffer );

    ChkDR( DRM_DWordSub( dwNextNode, XB_BASE_OBJECT_LENGTH, &iNextMax ) );

    /* iNextMax is at least XB_BASE_OBJECT_LENGTH away from end of buffer */
    DRMCASSERT( XB_BASE_OBJECT_LENGTH >= sizeof(DRM_WORD)+sizeof(DRM_WORD)+sizeof(DRM_DWORD) );
    /*
    ** because of that, if iNext < iNextMax, then we can read at least
    ** that amount from the buffer w/o overflowing
    */
    while( wNextType != f_wType
        && iNext < iNextMax )
    {
        ChkDR( DRM_DWordAdd( iNext, sizeof( DRM_WORD ), &dwResult ) );

        NETWORKBYTES_TO_WORD( wNextType, f_pbBuffer, dwResult );

        ChkDR( DRM_DWordAddSame( &dwResult, sizeof( DRM_WORD ) ) );

        NETWORKBYTES_TO_DWORD( cbNext, f_pbBuffer, dwResult );

        /* make sure we are moving ahead. */
        ChkArg( cbNext >= XB_BASE_OBJECT_LENGTH );

        ChkDR( DRM_DWordAddSame( &iNext, cbNext ) );
    }
    iNext -= cbNext; /* Can't underflow: cbNext is either 0 or cbNext += iNext was just exelwted */

    ChkBOOL( wNextType == f_wType, DRM_E_XB_OBJECT_NOTFOUND );

    ChkDR( DRM_DWordSub( dwNextNode, iNext, &dwResult ) );
    ChkBOOL( cbNext <= dwResult, DRM_E_XB_ILWALID_OBJECT );

    *f_piChild = iNext;
    *f_pcbChild = cbNext;

ErrorExit:
    return dr;
}

/*********************************************************************
**
** Function: DRM_XB_FindObject
**
** Synopsis: API that given the start of an object will find a child object
**           with the type specified in f_wObjectType
**
** Arguments:
**
** [f_wObjectType]    -- Type of object to locate
** [f_pformat]        -- format descriptor
** [f_pb]             -- Index in f_pBuffer to start from
** [f_cb]             -- Count of bytes in the format
** [f_piObject]       -- Index of the object found
** [f_pcbObject]      -- Count of bytes in the object
**********************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_XB_FindObject(
    __in                       DRM_WORD                     f_wObjectType,
    __in_ecount( 1 )     const DRM_XB_FORMAT_DESCRIPTION   *f_pformat,
    __in_bcount( f_cb )  const DRM_BYTE                    *f_pb,
    __in                       DRM_DWORD                    f_cb,
    __out                      DRM_DWORD                   *f_piObject,
    __out                      DRM_DWORD                   *f_pcbObject )
{
    CLAW_AUTO_RANDOM_CIPHER

    DRM_RESULT  dr          = DRM_SUCCESS;
    DRM_WORD    wParent     = f_wObjectType;
    DRM_WORD    wLevel      = 1;
    DRM_DWORD   iLwrrent    = 0;
    DRM_DWORD   iNext       = 0;
    DRM_DWORD   cbLwrrent   = f_cb;
    DRM_DWORD   cbNext      = 0;
    DRM_WORD    wHierarchy[XB_MAXIMUM_OBJECT_DEPTH] = { 0 };

    ChkArg( f_pb        != NULL );
    ChkArg( f_piObject  != NULL );
    ChkArg( f_pcbObject != NULL );
    ChkArg( f_pformat->pHeaderDescription != NULL );

    iLwrrent = XB_HEADER_LENGTH( f_pformat->pHeaderDescription->eFormatIdLength );
    iNext    = XB_HEADER_LENGTH( f_pformat->pHeaderDescription->eFormatIdLength );

    /* Substract the XB header as it won't be taken into account during the FindChild calls */
    ChkDR( DRM_DWordSubSame( &cbLwrrent, XB_HEADER_LENGTH( f_pformat->pHeaderDescription->eFormatIdLength ) ) );

    if( f_pformat->pHeaderDescription->pEntryDescription != NULL )
    {
        if( f_pformat->pHeaderDescription->pEntryDescription->wOffsetInLwrrentStruct == 0 )
        {
            /*
            ** Special case for extra header data.
            ** Given the special nature only word, dword, and qword types are supported
            */
            const DRM_XB_ELEMENT_DESCRIPTION *pElements  = (const DRM_XB_ELEMENT_DESCRIPTION*)f_pformat->pHeaderDescription->pEntryDescription->pContainerOrObjectDescriptions;
            DRM_DWORD cbDelta = 0;
            switch( pElements->eElementType )
            {
            case XB_ELEMENT_TYPE_WORD:
                cbDelta = sizeof( DRM_WORD );
                break;
            case XB_ELEMENT_TYPE_DWORD:
                cbDelta = sizeof( DRM_DWORD );
                break;
            case XB_ELEMENT_TYPE_QWORD:
                cbDelta = DRM_SIZEOFQWORD;
                break;
            default:
                ChkDR( DRM_E_XB_ILWALID_OBJECT );
                break;
            }

            ChkDR( DRM_DWordMult( f_pformat->pHeaderDescription->pEntryDescription->cContainerOrObjectDescriptions, cbDelta, &cbDelta ) );
            ChkDR( DRM_DWordAddSame( &iLwrrent, cbDelta ) );
            ChkDR( DRM_DWordSubSame( &cbLwrrent, cbDelta ) );
        }
        else
        {
            ChkDR( DRM_DWordAddSame( &iLwrrent, sizeof(DRM_DWORD) ) );
            ChkDR( DRM_DWordSubSame( &cbLwrrent, sizeof(DRM_DWORD) ) );
        }
    }

    for( ; ( wParent != XB_OBJECT_GLOBAL_HEADER
         && wLevel   < XB_MAXIMUM_OBJECT_DEPTH
         && _XB_IsKnownObjectType(f_pformat, wParent) ); wLevel++ )
    {
        wHierarchy[wLevel] = wParent;
        wParent = f_pformat->pEntryDescriptions[_XB_MapObjectTypeToEntryDescriptionIndex(f_pformat, wParent)].wParent;
    }
    wLevel--;

    /*
    **  First find node wHierarchy[wLevel], followed by wHierarchy[wLevel-1],
    **  etc till wHierarchy[1]
    */
    while( wLevel > 0 && wLevel < XB_MAXIMUM_OBJECT_DEPTH )
    {
        ChkDR( _XB_FindChild( wHierarchy[wLevel],
                              f_pb,
                              f_cb,
                              iLwrrent,
                              cbLwrrent,
                             &iNext,
                             &cbNext ) );
        iLwrrent = iNext;
        cbLwrrent = cbNext;
        wLevel--;
    }
    *f_piObject  = iLwrrent;
    *f_pcbObject = cbLwrrent;

ErrorExit:
    return dr;
}
#endif


EXIT_PK_NAMESPACE_CODE;

PRAGMA_STRICT_GS_POP;

